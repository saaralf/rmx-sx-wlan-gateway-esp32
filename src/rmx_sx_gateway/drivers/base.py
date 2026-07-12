"""Abstract interface driver.

Every physical or simulated interface (RMX952, SX825, SX852, Simulator)
implements this contract. The gateway core only depends on this abstraction,
never on concrete protocol bytes.

IMPORTANT (safety): a driver MUST NOT send repeated old drive commands after a
reconnect, and MUST report its real connection state via health_check() and
InterfaceEvent(status). No commanded state may be reported as "success" while
the interface is offline.
"""
from __future__ import annotations

import abc
import enum
from dataclasses import dataclass, field
from typing import AsyncIterator, Dict, List, Optional

from ..models import BusType, Direction, LocoRef, LocoState


class InterfaceEventType(str, enum.Enum):
    LOCO_STATUS = "loco_status"
    INTERFACE_STATUS = "interface_status"
    ERROR = "error"
    UNKNOWN_TELEGRAM = "unknown_telegram"


@dataclass
class InterfaceEvent:
    """An event emitted by a driver upward to the gateway core."""
    type: InterfaceEventType
    interface_id: str
    # optional payload fields
    bus: Optional[BusType] = None
    address: Optional[int] = None
    speed: Optional[int] = None
    direction: Optional[Direction] = None
    functions: Optional[Dict[int, bool]] = None
    connected: Optional[bool] = None
    device: Optional[str] = None
    type_name: Optional[str] = None
    source: Optional[str] = None
    raw: Optional[str] = None          # hex dump for unknown telegrams
    error: Optional[str] = None
    details: Dict = field(default_factory=dict)


@dataclass
class Capabilities:
    buses: List[BusType]
    emergency_stop_per_bus: bool = True
    feedback: bool = True
    max_function: int = 31


class InterfaceDriver(abc.ABC):
    def __init__(self, interface_id: str, device: Optional[str] = None) -> None:
        self.interface_id = interface_id
        self.device = device
        self._connected = False
        self._listeners = []  # type: ignore[var-annotated]
        self._state = None  # set by InterfaceManager (StateStore)

    def set_state_store(self, store) -> None:
        """Called by InterfaceManager to wire the driver to the central store."""
        self._state = store

    def _push_event(self, etype: InterfaceEventType, **kwargs) -> None:
        """Helper used by drivers to enqueue an event for the core."""
        ev = InterfaceEvent(type=etype, interface_id=self.interface_id, **kwargs)
        for cb in self._listeners:
            try:
                cb(ev)
            except Exception:  # pragma: no cover
                pass

    def add_event_listener(self, cb) -> None:
        self._listeners.append(cb)

    # -- lifecycle -------------------------------------------------------
    @abc.abstractmethod
    async def connect(self) -> None:
        ...

    @abc.abstractmethod
    async def disconnect(self) -> None:
        ...

    def is_connected(self) -> bool:
        return self._connected

    # -- commands (from client -> interface) -----------------------------
    @abc.abstractmethod
    async def send_drive(self, ref: LocoRef, speed: int, direction: Direction) -> None:
        ...

    @abc.abstractmethod
    async def set_function(self, ref: LocoRef, function: int, state: bool) -> None:
        ...

    @abc.abstractmethod
    async def loco_stop(self, ref: LocoRef) -> None:
        ...

    @abc.abstractmethod
    async def emergency_stop(self, bus: Optional[BusType] = None) -> None:
        ...

    @abc.abstractmethod
    async def request_status(self, ref: LocoRef) -> Optional[LocoState]:
        ...

    # -- events (interface -> gateway) ------------------------------------
    @abc.abstractmethod
    async def read_events(self) -> AsyncIterator[InterfaceEvent]:
        """Async iterator yielding InterfaceEvent objects."""
        ...

    # -- diagnostics -----------------------------------------------------
    @abc.abstractmethod
    async def health_check(self) -> bool:
        ...

    @abc.abstractmethod
    def get_capabilities(self) -> Capabilities:
        ...
