"""Tests: central state store, replay/stale protection, emergency stop."""
import pytest
from rmx_sx_gateway.models import LocoRef, BusType, Direction
from rmx_sx_gateway.state import StateStore


def ref():
    return LocoRef(interface_id="sim1", bus=BusType.RMX0, address=110)


def test_apply_command_updates_state():
    st = StateStore()
    s, changed = st.apply_command(ref(), speed=20, direction=Direction.FORWARD,
                                 sequence=1, source="command")
    assert changed
    assert s.speed == 20 and s.direction == Direction.FORWARD
    assert s.source == "command"


def test_replay_same_sequence_ignored():
    st = StateStore()
    st.apply_command(ref(), speed=20, direction=Direction.FORWARD, sequence=1)
    s, changed = st.apply_command(ref(), speed=99, direction=Direction.REVERSE, sequence=1)
    assert changed is False
    assert s.speed == 20  # unchanged


def test_stale_lower_sequence_ignored():
    st = StateStore()
    st.apply_command(ref(), speed=20, sequence=5)
    s, changed = st.apply_command(ref(), speed=50, sequence=3)
    assert changed is False
    assert s.speed == 20


def test_higher_sequence_applied():
    st = StateStore()
    st.apply_command(ref(), speed=20, sequence=5)
    s, changed = st.apply_command(ref(), speed=50, sequence=6)
    assert changed and s.speed == 50


def test_function_toggle():
    st = StateStore()
    st.apply_command(ref(), function=0, function_state=True, sequence=1)
    st.apply_command(ref(), function=3, function_state=True, sequence=2)
    st.apply_command(ref(), function=0, function_state=False, sequence=3)
    s = st.get(ref())
    assert s.is_function_active(0) is False
    assert s.is_function_active(3) is True


def test_stop_and_emergency():
    st = StateStore()
    st.apply_command(ref(), speed=30, sequence=1)
    st.stop_loco(ref())
    assert st.get(ref()).speed == 0
    st.apply_command(ref(), speed=30, sequence=2)
    st.emergency_stop_loco(ref())
    s = st.get(ref())
    assert s.speed == 0 and s.emergency_stop is True


def test_subscribers_and_viewers():
    st = StateStore()
    st.subscribe("c1", ref())
    st.subscribe("c2", ref())
    assert st.viewers_of(ref()) == {"c1", "c2"}
    st.unsubscribe("c1", ref())
    assert st.viewers_of(ref()) == {"c2"}
