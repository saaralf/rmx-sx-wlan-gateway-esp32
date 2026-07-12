"""Domain models shared across the gateway.

This module intentionally contains NO protocol bytes for RMX/SX. It only
defines neutral data structures (loco identity, state, enums) that are
independent of the underlying interface type.
"""
from __future__ import annotations

from datetime import datetime, timezone
from enum import Enum
from typing import Dict, Optional

from pydantic import BaseModel, Field, field_validator

# ---------------------------------------------------------------------------
# Hard limits (safety boundaries). The effective speed range is further
# constrained per locomotive by `speed_steps` from configuration.
# ---------------------------------------------------------------------------
MIN_SPEED = 0
MAX_SPEED_HARD = 9999          # absolute hard cap, never exceeded
MIN_FUNCTION = 0
MAX_FUNCTION = 31             # Selectrix supports F0..F28; 31 is a hard cap

# Selectrix address space: 112 channels (0..111) per bus.
SX_ADDRESS_MAX = 111
# RMX address space: OFFEN — assumed 0..103 (104 addresses) until the RMX
# documentation (RMX-Doku_V5.pdf) is analysed in phase 4. Treat as preliminary.
RMX_ADDRESS_MAX = 103


class BusType(str, Enum):
    """A bus is always owned by exactly one interface and is never global.

    The same numeric address on RMX0 is NOT the same loco as the same
    address on SX0.
    """
    RMX0 = "RMX0"
    RMX1 = "RMX1"
    SX0 = "SX0"
    SX1 = "SX1"

    @property
    def family(self) -> str:
        return "RMX" if self.value.startswith("RMX") else "SX"


class Direction(str, Enum):
    FORWARD = "forward"
    REVERSE = "reverse"


def _utcnow_iso() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def loco_key(interface_id: str, bus: str, address: int) -> str:
    """Canonical, unique loco key: 'interface_id:bus:address'."""
    return f"{interface_id}:{bus}:{address}"


class LocoRef(BaseModel):
    """A globally unique loco reference: interface + bus + address."""
    interface_id: str = Field(..., min_length=1)
    bus: BusType
    address: int

    @field_validator("address")
    @classmethod
    def _check_address(cls, v: int, info) -> int:
        # We cannot easily access `bus` here in isolation; full check in config.
        if v < 0 or v > max(SX_ADDRESS_MAX, RMX_ADDRESS_MAX):
            raise ValueError(f"address {v} out of supported range")
        return v

    @property
    def key(self) -> str:
        return loco_key(self.interface_id, self.bus.value, self.address)

    def with_key(self) -> str:
        return self.key


class LocoState(BaseModel):
    """The authoritative, server-held state of one locomotive.

    The Raspberry Pi is the single source of truth. Values originate from
    interface feedback where available, otherwise from validated commands.
    """
    interface_id: str
    bus: BusType
    address: int
    speed: int = 0
    direction: Direction = Direction.FORWARD
    functions: Dict[int, bool] = Field(default_factory=dict)
    last_update: str = Field(default_factory=_utcnow_iso)
    source: str = "unknown"            # "interface" | "command" | "simulator" | ...
    connected: bool = True
    emergency_stop: bool = False
    owner: Optional[str] = None        # client_id holding an exclusive claim
    version: int = 0                   # monotonically increasing state version

    @property
    def key(self) -> str:
        return loco_key(self.interface_id, self.bus.value, self.address)

    def is_function_active(self, fn: int) -> bool:
        return bool(self.functions.get(fn, False))
