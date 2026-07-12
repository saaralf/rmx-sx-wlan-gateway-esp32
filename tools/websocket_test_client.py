#!/usr/bin/env python3
"""websocket_test_client.py — minimal ESP32-throttle simulator for the gateway.

Connects to ws://<host>:<port>/ws, performs the hello handshake, and can be
driven interactively or in a scripted scenario to validate the protocol,
reconnection, and state sync.

Usage:
    python tools/websocket_test_client.py --host 192.168.50.1 --port 8080 \
        --client-id test-01

Interactive commands (type and enter):
    drive <speed> <forward|reverse>
    fn <n> <0|1>
    stop
    estop
    select <iface> <bus> <addr>
    state <iface> <bus> <addr>
    ping
    quit

Reconnection is automatic on drop.
"""
import argparse
import asyncio
import json
import sys

try:
    import aiohttp
except ImportError:
    print("aiohttp required", file=sys.stderr)
    sys.exit(1)


class TestClient:
    def __init__(self, host, port, client_id):
        self.url = f"http://{host}:{port}/ws" if False else f"ws://{host}:{port}/ws"
        self.client_id = client_id
        self.seq = 0
        self.ws = None

    async def _send(self, msg: dict):
        self.seq += 1
        if "sequence" in msg and msg["sequence"] is None:
            msg["sequence"] = self.seq
        await self.ws.send_str(json.dumps(msg))

    async def hello(self):
        await self._send({
            "type": "hello", "client_id": self.client_id,
            "protocol_version": 1, "device": "TestClient",
            "firmware_version": "0.0.1",
        })

    async def run(self):
        while True:
            try:
                async with aiohttp.ClientSession() as session:
                    async with session.ws_connect(self.url) as ws:
                        self.ws = ws
                        print("connected")
                        await self.hello()
                        async for msg in ws:
                            if msg.type == aiohttp.WSMsgType.TEXT:
                                data = json.loads(msg.data)
                                print("<-", data.get("type"), data)
                            elif msg.type in (aiohttp.WSMsgType.CLOSED,
                                              aiohttp.WSMsgType.ERROR):
                                break
            except Exception as exc:
                print("connection lost:", exc)
            print("reconnecting in 2s...")
            await asyncio.sleep(2)

    async def cmd(self, line: str):
        parts = line.split()
        if not parts:
            return
        op = parts[0]
        if op == "drive" and len(parts) >= 3:
            await self._send({"type": "drive", "interface": "sim1", "bus": "RMX0",
                              "address": 110, "speed": int(parts[1]),
                              "direction": parts[2], "sequence": None})
        elif op == "fn" and len(parts) >= 3:
            await self._send({"type": "function", "interface": "sim1", "bus": "RMX0",
                              "address": 110, "function": int(parts[1]),
                              "state": bool(int(parts[2])), "sequence": None})
        elif op == "stop":
            await self._send({"type": "loco_stop", "interface": "sim1", "bus": "RMX0",
                              "address": 110, "sequence": None})
        elif op == "estop":
            await self._send({"type": "emergency_stop", "interface": "sim1",
                              "bus": "RMX0", "sequence": None})
        elif op == "select" and len(parts) >= 4:
            await self._send({"type": "select_loco", "interface": parts[1],
                              "bus": parts[2], "address": int(parts[3])})
        elif op == "state" and len(parts) >= 4:
            await self._send({"type": "request_state", "interface": parts[1],
                              "bus": parts[2], "address": int(parts[3])})
        elif op == "ping":
            await self._send({"type": "ping", "sequence": None})
        else:
            print("unknown command")


async def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=8080)
    ap.add_argument("--client-id", default="test-01")
    args = ap.parse_args()

    tc = TestClient(args.host, args.port, args.client_id)
    task = asyncio.create_task(tc.run())
    print("Interactive test client. Commands: drive/ fn/ stop/ estop/ select/ state/ ping/ quit")
    loop = asyncio.get_event_loop()
    while True:
        line = await loop.run_in_executor(None, sys.stdin.readline)
        line = line.strip()
        if line in ("quit", "exit"):
            break
        await tc.cmd(line)
    task.cancel()


if __name__ == "__main__":
    asyncio.run(main())
