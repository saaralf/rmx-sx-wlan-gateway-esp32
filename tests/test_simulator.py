"""Tests: simulator driver (stand-in for real interfaces)."""
import asyncio
import pytest
from rmx_sx_gateway.models import LocoRef, BusType, Direction
from rmx_sx_gateway.drivers.simulator import SimulatorDriver
from rmx_sx_gateway.drivers.base import InterfaceEventType


def ref():
    return LocoRef(interface_id="sim1", bus=BusType.RMX0, address=110)


@pytest.mark.asyncio
async def test_connect_emits_status():
    d = SimulatorDriver("sim1")
    await d.connect()
    assert d.is_connected()
    events = await _collect_for(d, 0.2)
    from rmx_sx_gateway.drivers.base import InterfaceEventType
    assert any(e.type == InterfaceEventType.INTERFACE_STATUS for e in events)
    await d.disconnect()


@pytest.mark.asyncio
async def test_drive_and_feedback():
    d = SimulatorDriver("sim1")
    await d.connect()
    events = await _collect_for(d, 0.2)
    await d.send_drive(ref(), 25, Direction.FORWARD)
    events += await _collect_for(d, 0.2)
    statuses = [e for e in events if e.type == InterfaceEventType.LOCO_STATUS]
    assert statuses, "expected a loco_status feedback event"
    assert statuses[0].speed == 25
    assert statuses[0].direction == Direction.FORWARD
    await d.disconnect()


@pytest.mark.asyncio
async def test_function_feedback():
    d = SimulatorDriver("sim1")
    await d.connect()
    await d.set_function(ref(), 0, True)
    events = await _collect_for(d, 0.2)
    statuses = [e for e in events if e.type == InterfaceEventType.LOCO_STATUS]
    assert any(s.functions.get(0) is True for s in statuses)
    await d.disconnect()


@pytest.mark.asyncio
async def test_emergency_stop():
    d = SimulatorDriver("sim1")
    await d.connect()
    await d.send_drive(ref(), 25, Direction.FORWARD)
    await d.emergency_stop(BusType.RMX0)
    events = await _collect_for(d, 0.2)
    estops = [e for e in events if e.type == InterfaceEventType.LOCO_STATUS and e.speed == 0]
    assert estops, "emergency stop should zero the speed"
    await d.disconnect()


@pytest.mark.asyncio
async def test_disconnect_fault():
    d = SimulatorDriver("sim1")
    await d.connect()
    d.inject_disconnect(True)
    assert d.is_connected() is False
    with pytest.raises(RuntimeError):
        await d.send_drive(ref(), 10, Direction.FORWARD)
    await d.disconnect()


async def _collect_for(driver, seconds: float):
    """Collect events emitted within `seconds` without hanging forever."""
    collected = []

    async def _pump():
        async for e in driver.read_events():
            collected.append(e)

    task = asyncio.create_task(_pump())
    await asyncio.sleep(seconds)
    task.cancel()
    try:
        await task
    except asyncio.CancelledError:
        pass
    return collected

