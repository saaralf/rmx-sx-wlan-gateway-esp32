"""Central locomotive state store.

The Raspberry Pi is the authoritative source of truth for each loco's state.
This store:
  * keeps the last accepted (sequence, command) per loco to reject
    duplicates and stale commands (sequence must strictly increase per loco),
  * applies interface feedback events into the state,
  * tracks an optional exclusive owner (claim) per loco,
  * bumps a monotonic state version on every change,
  * records the source of the last update.
"""
from __future__ import annotations

import logging
from datetime import datetime, timezone
from typing import Dict, List, Optional, Tuple

from .models import BusType, Direction, LocoRef, LocoState
from .drivers.base import InterfaceEvent

logger = logging.getLogger(__name__)


class StateStore:
    def __init__(self) -> None:
        self._states: Dict[str, LocoState] = {}
        # last accepted sequence per loco key (for replay/stale protection)
        self._last_seq: Dict[str, int] = {}
        # loco key -> set of client_ids currently displaying it
        self._subscribers: Dict[str, set] = {}

    # -- query -----------------------------------------------------------
    def get(self, ref: LocoRef) -> Optional[LocoState]:
        return self._states.get(ref.key)

    def all(self) -> List[LocoState]:
        return list(self._states.values())

    def states_for_bus(self, interface_id: str, bus: BusType) -> List[LocoState]:
        return [
            s for s in self._states.values()
            if s.interface_id == interface_id and s.bus == bus
        ]

    # -- command intake (client -> state) --------------------------------
    def apply_command(
        self, ref: LocoRef, *, speed: Optional[int] = None,
        direction: Optional[Direction] = None, function: Optional[int] = None,
        function_state: Optional[bool] = None, sequence: Optional[int] = None,
        source: str = "command", client_id: Optional[str] = None,
    ) -> Tuple[LocoState, bool]:
        """Apply a validated command. Returns (state, changed).

        Replay protection: if `sequence` is provided and <= last accepted
        sequence for this loco, the command is ignored (changed=False) unless
        it is an emergency/stop override.
        """
        if sequence is not None:
            last = self._last_seq.get(ref.key)
            if last is not None and sequence <= last:
                st = self._states.get(ref.key)
                logger.info("stale/duplicate command ignored",
                            extra={"loco": ref.key, "seq": sequence, "last": last})
                if st is None:
                    st = self._new(ref)
                return st, False

        st = self._states.get(ref.key) or self._new(ref)
        st.version += 1
        st.source = source
        st.last_update = _utcnow()

        if speed is not None:
            st.speed = max(0, min(speed, 9999))
            st.emergency_stop = False
        if direction is not None:
            st.direction = direction
        if function is not None:
            st.functions[function] = bool(function_state)

        if sequence is not None:
            self._last_seq[ref.key] = sequence

        if client_id:
            st.owner = client_id  # soft owner for display
        self._states[ref.key] = st
        logger.info("state updated",
                    extra={"loco": ref.key, "speed": st.speed,
                           "dir": st.direction.value, "seq": sequence,
                           "src": source})
        return st, True

    def stop_loco(self, ref: LocoRef, source: str = "command") -> LocoState:
        st = self._states.get(ref.key) or self._new(ref)
        st.version += 1
        st.speed = 0
        st.emergency_stop = False
        st.source = source
        st.last_update = _utcnow()
        self._states[ref.key] = st
        return st

    def emergency_stop_loco(self, ref: LocoRef, source: str = "command") -> LocoState:
        st = self._states.get(ref.key) or self._new(ref)
        st.version += 1
        st.speed = 0
        st.emergency_stop = True
        st.source = source
        st.last_update = _utcnow()
        self._states[ref.key] = st
        return st

    # -- interface feedback intake (interface -> state) ------------------
    def apply_interface_event(self, ev: InterfaceEvent) -> Optional[LocoState]:
        if ev.type.value != "loco_status" or ev.bus is None or ev.address is None:
            return None
        ref = LocoRef(interface_id=ev.interface_id, bus=ev.bus, address=ev.address)
        st = self._states.get(ref.key) or self._new(ref)
        st.version += 1
        st.source = ev.source or "interface"
        st.last_update = _utcnow()
        if ev.speed is not None:
            st.speed = ev.speed
        if ev.direction is not None:
            st.direction = ev.direction
        if ev.functions is not None:
            for fn, val in ev.functions.items():
                st.functions[fn] = bool(val)
        if ev.connected is not None:
            st.connected = ev.connected
        self._states[ref.key] = st
        return st

    def mark_interface_offline(self, interface_id: str) -> List[str]:
        affected = [k for k, s in self._states.items() if s.interface_id == interface_id]
        for k in affected:
            self._states[k].connected = False
            self._states[k].source = "interface_offline"
        return affected

    # -- subscriptions (which clients view which loco) -------------------
    def subscribe(self, client_id: str, ref: LocoRef) -> None:
        self._subscribers.setdefault(ref.key, set()).add(client_id)

    def unsubscribe(self, client_id: str, ref: LocoRef) -> None:
        s = self._subscribers.get(ref.key)
        if s and client_id in s:
            s.discard(client_id)

    def viewers_of(self, ref: LocoRef) -> set:
        return set(self._subscribers.get(ref.key, set()))

    # -- helpers ---------------------------------------------------------
    def _new(self, ref: LocoRef) -> LocoState:
        return LocoState(
            interface_id=ref.interface_id, bus=ref.bus, address=ref.address,
        )


def _utcnow() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
