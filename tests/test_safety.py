"""Tests: safety rules — offline rejection, limits, no stale repeats."""
import pytest
from rmx_sx_gateway.config import GatewayConfig, SafetyConfig
from rmx_sx_gateway.models import LocoRef, BusType, Direction
from rmx_sx_gateway.state import StateStore
from rmx_sx_gateway.client_manager import ClientManager
from rmx_sx_gateway.interface_manager import InterfaceManager, set_client_manager
from rmx_sx_gateway.websocket_server import WebSocketServer
from aiohttp import ClientSession, WSMsgType
import asyncio, json, socket


def _cfg(offline_reject=True):
    return GatewayConfig(
        server={"port": 8080, "heartbeat_timeout_seconds": 5},
        safety={"reject_commands_when_interface_offline": offline_reject},
        interfaces=[{"id": "sim1", "type": "Simulator", "enabled": True,
                    "buses": ["RMX0"], "speed_steps": 32}],
    )


async def _start(cfg):
    state = StateStore()
    clients = ClientManager(state)
    iface = InterfaceManager(cfg, state)
    await iface.start()
    set_client_manager(clients)
    ws = WebSocketServer(cfg, state, clients, iface.drivers)
    from aiohttp import web
    runner = web.AppRunner(ws.app, access_log=None)
    await runner.setup()
    s = socket.socket(); s.bind(("127.0.0.1", 0)); port = s.getsockname()[1]; s.close()
    await web.TCPSite(runner, "127.0.0.1", port).start()
    ws._port = port
    return ws, iface, runner


@pytest.mark.asyncio
async def test_command_rejected_when_interface_offline():
    cfg = _cfg(offline_reject=True)
    ws, iface, runner = await _start(cfg)
    # disconnect the simulator to force offline
    await iface.drivers["sim1"].disconnect()
    try:
        async with ClientSession() as s:
            async with s.ws_connect(f"http://127.0.0.1:{ws._port}/ws") as c:
                await c.send_json({"type": "hello", "client_id": "t", "protocol_version": 1})
                await c.receive_json()
                await c.send_json({"type": "drive", "interface": "sim1",
                                   "bus": "RMX0", "address": 110, "speed": 10,
                                   "direction": "forward", "sequence": 1})
                ack = await c.receive_json()
                assert ack["status"] == "rejected"
                assert ack["error"] == "interface_offline"
    finally:
        await runner.cleanup()
        await iface.stop()


@pytest.mark.asyncio
async def test_command_accepted_when_offline_guard_disabled():
    cfg = _cfg(offline_reject=False)
    ws, iface, runner = await _start(cfg)
    await iface.drivers["sim1"].disconnect()
    try:
        async with ClientSession() as s:
            async with s.ws_connect(f"http://127.0.0.1:{ws._port}/ws") as c:
                await c.send_json({"type": "hello", "client_id": "t", "protocol_version": 1})
                await c.receive_json()
                await c.send_json({"type": "drive", "interface": "sim1",
                                   "bus": "RMX0", "address": 110, "speed": 10,
                                   "direction": "forward", "sequence": 1})
                ack = await c.receive_json()
                assert ack["status"] == "accepted"
    finally:
        await runner.cleanup()
        await iface.stop()


@pytest.mark.asyncio
async def test_unknown_interface_rejected():
    cfg = _cfg()
    ws, iface, runner = await _start(cfg)
    try:
        async with ClientSession() as s:
            async with s.ws_connect(f"http://127.0.0.1:{ws._port}/ws") as c:
                await c.send_json({"type": "hello", "client_id": "t", "protocol_version": 1})
                await c.receive_json()
                await c.send_json({"type": "drive", "interface": "nope",
                                   "bus": "RMX0", "address": 110, "speed": 10,
                                   "direction": "forward", "sequence": 1})
                ack = await c.receive_json()
                assert ack["status"] == "rejected"
                assert ack["error"] == "unknown_interface"
    finally:
        await runner.cleanup()
        await iface.stop()
