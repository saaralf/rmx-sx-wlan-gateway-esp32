"""WebSocket server: the ESP32 throttle <-> gateway bridge.

Exposes ws://<host>:<port>/ws. Handles:
  * hello / hello_ack handshake (protocol version negotiation)
  * ping / pong heartbeat
  * strict JSON message validation (rejects oversized / malformed / unknown)
  * routing of drive / function / loco_stop / emergency_stop commands into the
    central state and the appropriate interface driver
  * broadcasting loco_status and command_ack to the right clients
  * re-sync via request_state / state_snapshot after reconnect

Safety: commands for a loco whose interface is offline are rejected with a
clear error when safety.reject_commands_when_interface_offline is set.
Commands with non-increasing sequence numbers are dropped (replay/stale).
"""
from __future__ import annotations

import logging
import time
from typing import Dict, Optional

from aiohttp import web, WSMsgType

from .client_manager import Client, ClientManager
from .config import GatewayConfig, SafetyConfig
from .drivers.base import InterfaceDriver
from .models import BusType, Direction, LocoRef
from .protocol import (
    PROTOCOL_VERSION,
    CommandAck,
    DriveCommand,
    EmergencyStop,
    ErrorMessage,
    FunctionCommand,
    HelloAck,
    HelloMessage,
    LocoStatusMessage,
    MsgType,
    PingMessage,
    PongMessage,
    RequestState,
    SelectLoco,
    StateSnapshot,
    LocoStatePayload,
    parse_message,
    encode,
    ProtocolError,
)
from .state import StateStore

logger = logging.getLogger(__name__)


class WebSocketServer:
    def __init__(
        self, cfg: GatewayConfig, state: StateStore,
        clients: ClientManager, drivers: Dict[str, InterfaceDriver],
    ) -> None:
        self.cfg = cfg
        self.state = state
        self.clients = clients
        self.drivers = drivers
        self.app = web.Application()
        self.app.router.add_get("/ws", self._handle_ws)
        self.app.router.add_get("/health", self._handle_health)
        self._runner: Optional[web.AppRunner] = None
        self._pending_acks: Dict[int, float] = {}

    async def start(self) -> None:
        self._runner = web.AppRunner(self.app, access_log=None)
        await self._runner.setup()
        site = web.TCPSite(self._runner, self.cfg.server.host, self.cfg.server.port)
        await site.start()
        logger.info("websocket server listening",
                    extra={"host": self.cfg.server.host, "port": self.cfg.server.port})

    async def stop(self) -> None:
        if self._runner:
            await self._runner.cleanup()

    # -- health ----------------------------------------------------------
    async def _handle_health(self, request: web.Request) -> web.Response:
        online = sum(1 for d in self.drivers.values() if d.is_connected())
        return web.json_response({
            "status": "ok",
            "clients": self.clients.count(),
            "interfaces_online": online,
            "interfaces_total": len(self.drivers),
        })

    # -- websocket -------------------------------------------------------
    async def _handle_ws(self, request: web.Request) -> web.WebSocketResponse:
        ws = web.WebSocketResponse(
            max_msg_size=self.cfg.server.max_message_size,
            heartbeat=20,        # aiohttp application-level ping
            autoping=True,
        )
        await ws.prepare(request)
        client: Optional[Client] = None
        client_id = f"anon-{id(ws)}"

        try:
            async for msg in ws:
                if msg.type == WSMsgType.TEXT:
                    try:
                        parsed = parse_message(msg.data)
                    except ProtocolError as exc:
                        await ws.send_str(encode(ErrorMessage(
                            code="protocol_error", message=str(exc))))
                        logger.warning("protocol error",
                                       extra={"client": client_id, "err": str(exc)})
                        continue

                    # route
                    if isinstance(parsed, HelloMessage):
                        client_id = parsed.client_id
                        client = Client(client_id, ws.send_str)
                        self.clients.register(client)
                        await ws.send_str(encode(HelloAck(
                            protocol_version=PROTOCOL_VERSION,
                            status="ready",
                        )))
                        logger.info("hello", extra={"client": client_id,
                                                    "fw": parsed.firmware_version})
                        continue

                    if client is None:
                        # any other message requires a prior hello
                        await ws.send_str(encode(ErrorMessage(
                            code="not_helloed",
                            message="send hello first")))
                        continue

                    client.touch()

                    if isinstance(parsed, PingMessage):
                        await ws.send_str(encode(PongMessage(sequence=parsed.sequence)))
                        continue

                    if isinstance(parsed, SelectLoco):
                        ref = LocoRef(interface_id=parsed.interface, bus=parsed.bus,
                                      address=parsed.address)
                        self.state.subscribe(client_id, ref)
                        # immediately push current snapshot if known
                        st = self.state.get(ref)
                        if st is not None:
                            await ws.send_str(encode(self._snapshot(st)))
                        continue

                    if isinstance(parsed, DriveCommand):
                        await self._handle_drive(ws, client, parsed)
                        continue

                    if isinstance(parsed, FunctionCommand):
                        await self._handle_function(ws, client, parsed)
                        continue

                    if isinstance(parsed, RequestState):
                        ref = LocoRef(interface_id=parsed.interface, bus=parsed.bus,
                                      address=parsed.address)
                        st = self.state.get(ref)
                        if st is not None:
                            await ws.send_str(encode(self._snapshot(st)))
                        continue

                    if isinstance(parsed, EmergencyStop):
                        await self._handle_emergency(ws, client, parsed)
                        continue

                    # unknown / unhandled
                    logger.warning("unhandled message type",
                                   extra={"client": client_id,
                                          "type": getattr(parsed, "type", None)})

                elif msg.type == WSMsgType.ERROR:
                    logger.error("ws connection error",
                                extra={"client": client_id,
                                       "err": str(ws.exception())})
                    break
                else:
                    # binary / close handled by loop
                    if msg.type in (WSMsgType.CLOSE, WSMsgType.CLOSED):
                        break
        finally:
            if client is not None:
                self.clients.unregister(client.client_id)
        return ws

    # -- command handlers ------------------------------------------------
    async def _handle_drive(self, ws, client: Client, cmd: DriveCommand) -> None:
        await self._require_interface(ws, cmd.interface, cmd.sequence)
        ref = LocoRef(interface_id=cmd.interface, bus=cmd.bus, address=cmd.address)
        # a client commanding a loco automatically becomes a viewer of it
        self.state.subscribe(client.client_id, ref)
        # offline guard
        if self._interface_offline(cmd.interface):
            await self._reject(ws, cmd.sequence, "interface_offline")
            return
        # apply to state (replay protection inside)
        st, changed = self.state.apply_command(
            ref, speed=cmd.speed, direction=cmd.direction,
            sequence=cmd.sequence, source="command", client_id=client.client_id)
        await self._ack(ws, cmd.sequence, ok=changed)
        if changed:
            await self._forward_drive(ref, st.speed, st.direction)
            await self._broadcast_status(st)

    async def _handle_function(self, ws, client: Client, cmd: FunctionCommand) -> None:
        await self._require_interface(ws, cmd.interface, cmd.sequence)
        ref = LocoRef(interface_id=cmd.interface, bus=cmd.bus, address=cmd.address)
        self.state.subscribe(client.client_id, ref)
        if self._interface_offline(cmd.interface):
            await self._reject(ws, cmd.sequence, "interface_offline")
            return
        st, changed = self.state.apply_command(
            ref, function=cmd.function, function_state=cmd.state,
            sequence=cmd.sequence, source="command", client_id=client.client_id)
        await self._ack(ws, cmd.sequence, ok=changed)
        if changed:
            await self._forward_function(ref, cmd.function, cmd.state)
            await self._broadcast_status(st)

    async def _handle_emergency(self, ws, client: Client, cmd: EmergencyStop) -> None:
        await self._require_interface(ws, cmd.interface, cmd.sequence)
        driver = self.drivers.get(cmd.interface)
        if driver is None or not driver.is_connected():
            await self._reject(ws, cmd.sequence, "interface_offline")
            return
        try:
            await driver.emergency_stop(cmd.bus)
        except Exception as exc:  # noqa: BLE001
            logger.error("emergency_stop failed",
                        extra={"interface": cmd.interface, "err": str(exc)})
            await self._reject(ws, cmd.sequence, "driver_error")
            return
        # mark all affected locos stopped in state
        bus_filter = cmd.bus
        for s in self.state.all():
            if s.interface_id == cmd.interface and (bus_filter is None or s.bus == bus_filter):
                self.state.emergency_stop_loco(
                    LocoRef(interface_id=s.interface_id, bus=s.bus, address=s.address),
                    source="emergency")
                await self._broadcast_status(s)
        await self._ack(ws, cmd.sequence, ok=True)

    # -- helpers ---------------------------------------------------------
    async def _require_interface(self, ws, interface_id: str, sequence: int) -> None:
        if interface_id not in self.drivers:
            await self._reject(ws, sequence, "unknown_interface")

    def _interface_offline(self, interface_id: str) -> bool:
        if not self.cfg.safety.reject_commands_when_interface_offline:
            return False
        d = self.drivers.get(interface_id)
        return d is None or not d.is_connected()

    async def _ack(self, ws, sequence: int, ok: bool) -> None:
        await ws.send_str(encode(CommandAck(
            sequence=sequence, status="accepted" if ok else "rejected",
            error=None if ok else "stale_or_duplicate")))

    async def _reject(self, ws, sequence: int, error: str) -> None:
        await ws.send_str(encode(CommandAck(
            sequence=sequence, status="rejected", error=error)))

    async def _forward_drive(self, ref: LocoRef, speed: int, direction: Direction) -> None:
        d = self.drivers.get(ref.interface_id)
        if d and d.is_connected():
            try:
                await d.send_drive(ref, speed, direction)
            except Exception as exc:  # noqa: BLE001
                logger.error("forward drive failed",
                            extra={"loco": ref.key, "err": str(exc)})

    async def _forward_function(self, ref: LocoRef, function: int, state: bool) -> None:
        d = self.drivers.get(ref.interface_id)
        if d and d.is_connected():
            try:
                await d.set_function(ref, function, state)
            except Exception as exc:  # noqa: BLE001
                logger.error("forward function failed",
                            extra={"loco": ref.key, "fn": function, "err": str(exc)})

    async def _broadcast_status(self, st) -> None:
        ref = LocoRef(interface_id=st.interface_id, bus=st.bus, address=st.address)
        payload = LocoStatusMessage(
            interface=st.interface_id, bus=st.bus, address=st.address,
            speed=st.speed, direction=st.direction,
            functions={int(k): bool(v) for k, v in st.functions.items()},
            source=st.source, timestamp=st.last_update,
        )
        await self.clients.broadcast(encode(payload), only_viewers_of=ref)

    def _snapshot(self, st) -> StateSnapshot:
        return StateSnapshot(locomotive=LocoStatePayload(
            interface=st.interface_id, bus=st.bus, address=st.address,
            speed=st.speed, direction=st.direction,
            functions={int(k): bool(v) for k, v in st.functions.items()},
            last_update=st.last_update, source=st.source,
            connected=st.connected, owner=st.owner, version=st.version,
        ))
