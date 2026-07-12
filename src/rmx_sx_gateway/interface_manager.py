"""Interface manager: owns drivers, auto-reconnect, event pump.

Responsibilities:
  * instantiate drivers from configuration
  * connect / auto-reconnect on failure (isolated per interface)
  * pump driver events into the central state store and broadcast to clients
  * expose driver handles to the WebSocket server
"""
from __future__ import annotations

import asyncio
import logging
from typing import Dict

from .config import GatewayConfig, InterfaceConfig
from .drivers.base import InterfaceDriver, InterfaceEvent, InterfaceEventType
from .drivers.simulator import SimulatorDriver
from .state import StateStore

logger = logging.getLogger(__name__)


class InterfaceManager:
    def __init__(self, cfg: GatewayConfig, state: StateStore) -> None:
        self.cfg = cfg
        self.state = state
        self.drivers: Dict[str, InterfaceDriver] = {}
        self._tasks: list[asyncio.Task] = []

    async def start(self) -> None:
        for icfg in self.cfg.interfaces:
            if not icfg.enabled:
                logger.info("interface disabled", extra={"interface": icfg.id})
                continue
            driver = self._build(icfg)
            self.drivers[icfg.id] = driver
            self._tasks.append(asyncio.create_task(self._lifecycle(icfg, driver)))
            self._tasks.append(asyncio.create_task(self._pump(icfg, driver)))

    async def stop(self) -> None:
        for t in self._tasks:
            t.cancel()
        for driver in self.drivers.values():
            try:
                await driver.disconnect()
            except Exception:  # noqa: BLE001
                pass

    # -- driver construction --------------------------------------------
    def _build(self, icfg: InterfaceConfig) -> InterfaceDriver:
        # RMX952 / SX825 / SX852 drivers are implemented in later phases.
        # Until then, only Simulator is wired; others raise so config errors
        # surface clearly rather than silently pretending to work.
        if icfg.type == "Simulator":
            return SimulatorDriver(
                icfg.id, device=icfg.device, speed_steps=icfg.speed_steps)
        # Placeholder: real drivers added in phase 4/5. Raise to avoid
        # pretending we can talk to hardware we haven't implemented.
        raise NotImplementedError(
            f"Driver for type '{icfg.type}' not implemented yet "
            f"(arrives in a later phase). Use type: Simulator for now.")

    # -- lifecycle -------------------------------------------------------
    async def _lifecycle(self, icfg: InterfaceConfig, driver: InterfaceDriver) -> None:
        while True:
            try:
                if not driver.is_connected():
                    logger.info("connecting interface",
                                extra={"interface": icfg.id, "type": icfg.type})
                    await driver.connect()
                    logger.info("interface connected",
                                extra={"interface": icfg.id})
                await asyncio.sleep(5)
                ok = await driver.health_check()
                if not ok:
                    logger.warning("interface health check failed",
                                   extra={"interface": icfg.id})
                    await driver.disconnect()
            except asyncio.CancelledError:
                break
            except Exception as exc:  # noqa: BLE001
                logger.error("interface lifecycle error",
                            extra={"interface": icfg.id, "err": str(exc)})
                try:
                    await driver.disconnect()
                except Exception:
                    pass
                await asyncio.sleep(3)

    # -- event pump ------------------------------------------------------
    async def _pump(self, icfg: InterfaceConfig, driver: InterfaceDriver) -> None:
        try:
            async for ev in driver.read_events():
                await self._on_event(ev)
        except asyncio.CancelledError:
            return
        except Exception as exc:  # noqa: BLE001
            logger.error("event pump crashed",
                        extra={"interface": icfg.id, "err": str(exc)})

    async def _on_event(self, ev: InterfaceEvent) -> None:
        if ev.type == InterfaceEventType.LOCO_STATUS:
            st = self.state.apply_interface_event(ev)
            if st is not None:
                from .websocket_server import LocoStatusMessage, encode
                from .client_manager import ClientManager
                # direct broadcast via global client manager (set in main)
                if _CLIENTS is not None:
                    payload = LocoStatusMessage(
                        interface=st.interface_id, bus=st.bus, address=st.address,
                        speed=st.speed, direction=st.direction,
                        functions={int(k): bool(v) for k, v in st.functions.items()},
                        source=st.source, timestamp=st.last_update,
                    )
                    from .models import LocoRef
                    ref = LocoRef(interface_id=st.interface_id, bus=st.bus, address=st.address)
                    await _CLIENTS.broadcast(encode(payload), only_viewers_of=ref)
        elif ev.type == InterfaceEventType.INTERFACE_STATUS:
            if ev.connected is False:
                affected = self.state.mark_interface_offline(ev.interface_id)
                logger.warning("interface offline; marking locos unsafe",
                               extra={"interface": ev.interface_id,
                                      "locos": len(affected)})
        elif ev.type == InterfaceEventType.UNKNOWN_TELEGRAM:
            logger.warning("unknown telegram",
                           extra={"interface": ev.interface_id, "raw": ev.raw})


_CLIENTS = None  # injected by main.py


def set_client_manager(cm) -> None:
    global _CLIENTS
    _CLIENTS = cm
