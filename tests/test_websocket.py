"""Tests: WebSocket server integration (aiohttp TestServer + simulator)."""
import asyncio
import json
import pytest
from aiohttp import ClientSession, WSMsgType

from rmx_sx_gateway.config import GatewayConfig, load_config
from rmx_sx_gateway.state import StateStore
from rmx_sx_gateway.client_manager import ClientManager
from rmx_sx_gateway.interface_manager import InterfaceManager, set_client_manager
from rmx_sx_gateway.websocket_server import WebSocketServer
from rmx_sx_gateway.models import LocoRef, BusType, Direction


@pytest.fixture
def cfg():
    return GatewayConfig(
        server={"port": 8080, "max_clients": 5, "heartbeat_timeout_seconds": 5},
        interfaces=[{"id": "sim1", "type": "Simulator", "enabled": True,
                    "buses": ["RMX0", "SX0"], "speed_steps": 32}],
        locomotives=[],
    )


async def _start_server(cfg):
    state = StateStore()
    clients = ClientManager(state)
    iface = InterfaceManager(cfg, state)
    await iface.start()
    set_client_manager(clients)
    ws = WebSocketServer(cfg, state, clients, iface.drivers)
    runner = await _setup(ws)
    return ws, iface, state, clients, runner


async def _setup(ws):
    from aiohttp import web
    runner = web.AppRunner(ws.app, access_log=None)
    await runner.setup()
    # dynamic port
    import socket
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    port = s.getsockname()[1]
    s.close()
    site = web.TCPSite(runner, "127.0.0.1", port)
    await site.start()
    ws._port = port
    return runner


@pytest.mark.asyncio
async def test_hello_and_ping(cfg):
    ws, iface, state, clients, runner = await _start_server(cfg)
    try:
        async with ClientSession() as s:
            async with s.ws_connect(f"http://127.0.0.1:{ws._port}/ws") as c:
                await c.send_json({"type": "hello", "client_id": "t1",
                                    "protocol_version": 1})
                msg = await c.receive_json()
                assert msg["type"] == "hello_ack"
                await c.send_json({"type": "ping", "sequence": 3})
                msg = await c.receive_json()
                assert msg["type"] == "pong" and msg["sequence"] == 3
    finally:
        await runner.cleanup()
        await iface.stop()


@pytest.mark.asyncio
async def test_drive_end_to_end(cfg):
    ws, iface, state, clients, runner = await _start_server(cfg)
    try:
        async with ClientSession() as s:
            async with s.ws_connect(f"http://127.0.0.1:{ws._port}/ws") as c:
                await c.send_json({"type": "hello", "client_id": "t1",
                                    "protocol_version": 1})
                await c.receive_json()  # hello_ack
                await c.send_json({"type": "drive", "interface": "sim1",
                                   "bus": "RMX0", "address": 110, "speed": 22,
                                   "direction": "forward", "sequence": 1})
                ack = await c.receive_json()
                assert ack["type"] == "command_ack"
                assert ack["status"] == "accepted"
                # simulator should echo a loco_status
                got = False
                for _ in range(5):
                    m = await c.receive()
                    if m.type == WSMsgType.TEXT:
                        data = json.loads(m.data)
                        if data.get("type") == "loco_status":
                            assert data["speed"] == 22
                            got = True
                            break
                assert got, "expected loco_status feedback from simulator"
    finally:
        await runner.cleanup()
        await iface.stop()


@pytest.mark.asyncio
async def test_emergency_broadcast(cfg):
    ws, iface, state, clients, runner = await _start_server(cfg)
    try:
        async with ClientSession() as s:
            async with s.ws_connect(f"http://127.0.0.1:{ws._port}/ws") as c:
                await c.send_json({"type": "hello", "client_id": "t1",
                                    "protocol_version": 1})
                await c.receive_json()
                await c.send_json({"type": "select_loco", "interface": "sim1",
                                   "bus": "RMX0", "address": 110})
                await c.send_json({"type": "drive", "interface": "sim1",
                                   "bus": "RMX0", "address": 110, "speed": 22,
                                   "direction": "forward", "sequence": 1})
                await c.receive_json()  # ack
                await c.send_json({"type": "emergency_stop", "interface": "sim1",
                                   "bus": "RMX0", "sequence": 2})
                # skip any loco_status broadcasts, wait for the command_ack
                ack = None
                for _ in range(5):
                    m = await c.receive()
                    if m.type == WSMsgType.TEXT:
                        data = json.loads(m.data)
                        if data.get("type") == "command_ack":
                            ack = data
                            break
                assert ack is not None
                assert ack["status"] == "accepted"
                # expect a loco_status with speed 0 as viewer
                got = False
                for _ in range(5):
                    m = await c.receive()
                    if m.type == WSMsgType.TEXT:
                        data = json.loads(m.data)
                        if data.get("type") == "loco_status":
                            assert data["speed"] == 0
                            got = True
                            break
                assert got
    finally:
        await runner.cleanup()
        await iface.stop()
