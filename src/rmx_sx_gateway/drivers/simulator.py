"""Simulator interface driver.

Implements the full InterfaceDriver contract without any hardware. It keeps an
internal loco state, echoes feedback events, and can be scripted to inject
faults (disconnect, delays, errors, unknown telegrams) for testing.

Speed scale: the simulator clamps to [0, speed_steps-1] per loco where
speed_steps=32 by default (meaningful range 0..31, ignoring bit 7 direction).
This mirrors classic Selectrix 31-step encoding without inventing bytes.
"""
from __future__ import annotations

import asyncio
from typing import Dict, List, Optional

from ..models import BusType, Direction, LocoRef
from .base import (
    Capabilities,
    InterfaceDriver,
    InterfaceEvent,
    InterfaceEventType,
)


class SimulatorDriver(InterfaceDriver):
    def __init__(
        self, interface_id: str, device: Optional[str] = None,
        speed_steps: int = 32,
    ) -> None:
        super().__init__(interface_id, device or "simulator://internal")
        self._speed_steps = speed_steps
        self._connected = False
        self._state: Dict[str, dict] = {}            # key -> {speed,dir,fns,estop}
        self._event_queue: asyncio.Queue[InterfaceEvent] = asyncio.Queue()
        self._fault_disconnect = False
        self._fault_delay = 0.0
        self._capabilities = Capabilities(
            buses=[BusType.RMX0, BusType.RMX1, BusType.SX0, BusType.SX1],
            emergency_stop_per_bus=True,
            feedback=True,
            max_function=31,
        )

    # -- lifecycle -------------------------------------------------------
    async def connect(self) -> None:
        await asyncio.sleep(0)  # yield
        self._connected = True
        await self._emit(InterfaceEvent(
            type=InterfaceEventType.INTERFACE_STATUS,
            interface_id=self.interface_id,
            connected=True, device=self.device, type_name="Simulator",
        ))

    async def disconnect(self) -> None:
        self._connected = False
        await self._emit(InterfaceEvent(
            type=InterfaceEventType.INTERFACE_STATUS,
            interface_id=self.interface_id, connected=False,
        ))

    def is_connected(self) -> bool:
        return self._connected and not self._fault_disconnect

    # -- commands --------------------------------------------------------
    async def send_drive(self, ref: LocoRef, speed: int, direction: Direction) -> None:
        self._ensure(ref)
        if self._fault_delay:
            await asyncio.sleep(self._fault_delay)
        if not self.is_connected():
            raise RuntimeError("simulator offline")
        speed = max(0, min(speed, self._speed_steps - 1))
        st = self._state[ref.key]
        st["speed"] = 0 if st["estop"] else speed
        st["direction"] = direction.value
        await self._emit_status(ref)

    async def set_function(self, ref: LocoRef, function: int, state: bool) -> None:
        self._ensure(ref)
        if not self.is_connected():
            raise RuntimeError("simulator offline")
        self._state[ref.key]["functions"][function] = state
        await self._emit_status(ref)

    async def loco_stop(self, ref: LocoRef) -> None:
        self._ensure(ref)
        self._state[ref.key]["speed"] = 0
        await self._emit_status(ref)

    async def emergency_stop(self, bus: Optional[BusType] = None) -> None:
        for key, st in self._state.items():
            if bus is None or st["bus"] == bus.value:
                st["speed"] = 0
                st["estop"] = True
        # emit for all affected (coarse: emit all)
        for key, st in self._state.items():
            ref = LocoRef(
                interface_id=self.interface_id,
                bus=BusType(st["bus"]),
                address=st["address"],
            )
            await self._emit(InterfaceEvent(
                type=InterfaceEventType.LOCO_STATUS,
                interface_id=self.interface_id, bus=ref.bus,
                address=ref.address, speed=0,
                direction=Direction(st["direction"]),
                functions=dict(st["functions"]),
                connected=True, source="simulator",
            ))

    async def request_status(self, ref: LocoRef) -> None:
        self._ensure(ref)
        await self._emit_status(ref)

    # -- events ----------------------------------------------------------
    async def read_events(self):
        while True:
            ev = await self._event_queue.get()
            yield ev

    # -- diagnostics -----------------------------------------------------
    async def health_check(self) -> bool:
        return self.is_connected()

    def get_capabilities(self) -> Capabilities:
        return self._capabilities

    # -- test helpers ----------------------------------------------------
    def inject_disconnect(self, value: bool) -> None:
        self._fault_disconnect = value
        if value:
            # mark all states offline
            for st in self._state.values():
                st["connected"] = False

    def inject_delay(self, seconds: float) -> None:
        self._fault_delay = max(0.0, seconds)

    def inject_unknown_telegram(self, raw_hex: str) -> None:
        import asyncio as _a
        _a.get_event_loop().call_soon_threadsafe(
            lambda: self._event_queue.put_nowait(InterfaceEvent(
                type=InterfaceEventType.UNKNOWN_TELEGRAM,
                interface_id=self.interface_id,
                raw=raw_hex,
            ))
        )

    def get_state(self, ref: LocoRef) -> Optional[dict]:
        return self._state.get(ref.key)

    # -- internals -------------------------------------------------------
    def _ensure(self, ref: LocoRef) -> None:
        if ref.key not in self._state:
            self._state[ref.key] = {
                "speed": 0, "direction": Direction.FORWARD.value,
                "functions": {}, "estop": False, "connected": True,
                "bus": ref.bus.value, "address": ref.address,
            }

    async def _emit_status(self, ref: LocoRef) -> None:
        st = self._state[ref.key]
        await self._emit(InterfaceEvent(
            type=InterfaceEventType.LOCO_STATUS,
            interface_id=self.interface_id, bus=ref.bus,
            address=ref.address, speed=st["speed"],
            direction=Direction(st["direction"]),
            functions=dict(st["functions"]),
            connected=self.is_connected(), source="simulator",
        ))

    async def _emit(self, ev: InterfaceEvent) -> None:
        await self._event_queue.put(ev)
