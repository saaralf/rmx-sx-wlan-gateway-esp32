"""SX825 / SLX825 driver (Rautenhaus-Format), eigenständig neu implementiert.

Reference for the *serial PC protocol* (NOT the SX bus itself):
  github.com/michael71/SX3-PC  (GPL-3.0 — used for understanding only;
  this driver is an independent re-implementation under MIT).

Protocol facts taken from that reference (SXInterface.java /
GenericSXInterface.java):
  * 2-byte frames, 8N1, baudrate configurable (default 57600).
  * Write:  byte0 = addr | 0x80,  byte1 = data
  * Read :  byte0 = addr,        byte1 = 0x00  (Trix format: single data echo)
  * Rautenhaus mode enable:  write addr 126 (0xFE) with 0xA0 (monitor+feedback on)
  * Power: addr 127 (0xFF): 0x80 = on, 0x00 = off, read = 127,0x00
  * Loco always on SX0. Loco data byte layout:
        bit0..4 = speed (0..31)
        bit5    = 1 -> reverse
        bit6    = 1 -> light
        bit7    = 1 -> horn
  * After monitor mode, interface auto-sends [addr, data] on any bus change;
    addr bit7 selects bus (0=SX0, 1=SX1; SLX825 single-bus => always 0).

FUNCTION MAPPING (F0..F16) is decoder-specific and NOT defined in the reference
code (which only used raw SX bits 1..8). The gateway therefore offers a
configurable per-loco bit mapping. Until that mapping is configured we only
support the documented loco byte (light = F0, horn = extra) and raw accessory
bits. This is marked OPEN in docs/open-questions.md.
"""
from __future__ import annotations

import asyncio
import logging
from typing import AsyncIterator, Dict, List, Optional

from .base import InterfaceDriver, InterfaceEvent, InterfaceEventType
from ..models import BusType, Direction, LocoRef, LocoState

logger = logging.getLogger(__name__)

# SX constants observed in the reference implementation
SX0_MAX = 112          # addresses 0..111
SX_POWER_ADDR = 127    # 0x7F
SX_RAUTENHAUS_MODE_ADDR = 126  # 0xFE
RAUTENHAUS_ENABLE = 0xA0  # monitor on (bit7) + feedback on (bit5)


class SX825Driver(InterfaceDriver):
    """Driver for SLX825 / SX825 (Rautenhaus 2-byte serial protocol)."""

    def __init__(self, interface_id: str, device: str, baudrate: int = 57600,
                 timeout: float = 0.2):
        super().__init__(interface_id)
        self.device = device
        self.baudrate = baudrate
        self.timeout = timeout
        self._reader: Optional[asyncio.StreamReader] = None
        self._writer: Optional[asyncio.StreamWriter] = None
        self._lock = asyncio.Lock()
        self._buf: List[int] = []  # reassembly buffer for [addr,data] pairs
        self._rx = 0
        self._tx = 0
        self._errors = 0

    async def connect(self) -> None:
        import serial_asyncio  # local import keeps deps optional
        self._reader, self._writer = await serial_asyncio.open_serial_connection(
            url=self.device, baudrate=self.baudrate, bytesize=8,
            parity="N", stopbits=1, timeout=self.timeout)
        self._connected = True
        # Enter Rautenhaus monitor+feedback mode
        await self._write_frame(SX_RAUTENHAUS_MODE_ADDR, RAUTENHAUS_ENABLE)
        logger.info("[%s] SX825 connected on %s @ %d, monitor mode set",
                    self.interface_id, self.device, self.baudrate)
        self._push_event(InterfaceEventType.INTERFACE_STATUS, connected=True)

    async def disconnect(self) -> None:
        self._connected = False
        if self._writer is not None:
            try:
                self._writer.close()
            except Exception:
                pass
        self._reader = None
        self._writer = None
        self._push_event(InterfaceEventType.INTERFACE_STATUS, connected=False)

    async def send_drive(self, ref: LocoRef, speed: int, direction: Direction) -> None:
        """Encode a loco command on SX0 (per reference: locos always SX0)."""
        if ref.bus != BusType.SX0:
            # SX1 is for accessories/feedback in the reference; refuse loco on SX1
            raise ValueError(f"SX825 loco drive only supported on SX0, got {ref.bus}")
        speed = max(0, min(31, int(speed)))
        data = speed & 0x1F
        data |= 0x20 if direction == Direction.REVERSE else 0x00
        # light (F0) is stored in current state; merged here if known
        cur = self._state and self._state.get(ref)
        if cur is not None and cur.functions.get("0", False):
            data |= 0x40
        await self._write_frame(ref.address, data)
        # after write, monitor mode echoes [addr,data] back -> parsed in pump

    async def set_function(self, ref: LocoRef, function: int, state: bool) -> None:
        """Set a function. F0 (light) is mapped onto the loco data byte bit6.
        Other functions require a decoder-specific mapping (OPEN, see docs)."""
        if function == 0:
            # toggle light bit in the current loco byte
            cur = self._state and self._state.get(ref)
            speed = cur.speed if cur else 0
            direction = cur.direction if cur else Direction.FORWARD
            data = (speed & 0x1F) | (0x20 if direction == Direction.REVERSE else 0)
            if state:
                data |= 0x40
            await self._write_frame(ref.address, data)
        else:
            logger.warning("[%s] SX825 function F%d needs decoder mapping (OPEN)",
                           self.interface_id, function)
            # TODO: configurable bit mapping per loco

    async def emergency_stop(self, bus=None) -> None:
        # Stop all known locos on this interface (best effort)
        if self._state is None:
            return
        for st in self._state.all():
            if st.interface_id != self.interface_id:
                continue
            if bus is not None and st.bus != bus:
                continue
            await self._write_frame(st.address, 0x00)  # speed 0, forward, no light

    async def loco_stop(self, ref: LocoRef) -> None:
        await self._write_frame(ref.address, 0x00)

    async def request_status(self, ref: LocoRef) -> Optional[LocoState]:
        # In monitor mode the interface pushes updates; no explicit request.
        return self._state.get(ref) if self._state else None

    async def read_events(self) -> AsyncIterator[InterfaceEvent]:
        while self._connected and self._reader is not None:
            try:
                chunk = await self._reader.read(64)
            except asyncio.CancelledError:
                break
            except Exception as exc:  # pragma: no cover
                self._errors += 1
                logger.error("[%s] serial read error: %s", self.interface_id, exc)
                await asyncio.sleep(0.1)
                continue
            if not chunk:
                await asyncio.sleep(0.01)
                continue
            self._rx += len(chunk)
            for b in chunk:
                self._buf.append(b)
                if len(self._buf) >= 2:
                    addr, data = self._buf[0], self._buf[1]
                    self._buf = self._buf[2:]
                    await self._on_pair(addr, data)

    async def health_check(self) -> bool:
        return self._connected

    def get_capabilities(self) -> Dict[str, object]:
        return {
            "type": "SX825",
            "protocol": "rautenhaus-2byte",
            "buses": ["SX0"],
            "baudrate": self.baudrate,
            "loco_bus": "SX0",
            "function_mapping": "F0=light-bit6; F1..F16 OPEN/decoder-specific",
            "address_range": [0, SX0_MAX - 1],
        }

    # --- internal ---
    async def _write_frame(self, addr: int, data: int) -> None:
        if self._writer is None:
            raise RuntimeError("not connected")
        async with self._lock:
            self._writer.write(bytes([addr | 0x80, data & 0xFF]))
            await self._writer.drain()
        self._tx += 2

    async def _on_pair(self, addr: int, data: int) -> None:
        # monitor echo: addr (bit7 = bus), data
        bus = BusType.SX1 if (addr & 0x80) else BusType.SX0
        a = addr & 0x7F
        if a == SX_POWER_ADDR:
            logger.info("[%s] power state=%s", self.interface_id, "ON" if data else "OFF")
            return
        if self._state is None:
            return
        ref = LocoRef(interface_id=self.interface_id, bus=bus, address=a)
        st = self._state.get(ref)
        if st is None:
            return
        # decode loco byte
        speed = data & 0x1F
        direction = Direction.REVERSE if (data & 0x20) else Direction.FORWARD
        functions = dict(st.functions)
        functions["0"] = bool(data & 0x40)
        self._state.apply_interface_update(ref, speed=speed, direction=direction,
                                           functions=functions, source="interface")
