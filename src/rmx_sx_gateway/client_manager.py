"""Client manager: tracks connected throttles and broadcasts messages.

Responsibilities:
  * register/unregister WebSocket clients with a stable client_id
  * per-client heartbeat tracking (ping/pong) and stale detection
  * broadcast a model/message to all clients or a filtered subset
  * resolve which clients view a given loco (for targeted broadcasts)
"""
from __future__ import annotations

import asyncio
import logging
import time
from typing import Callable, Dict, Optional, Set

from pydantic import BaseModel
from .state import StateStore
from .models import LocoRef

logger = logging.getLogger(__name__)


class Client:
    def __init__(self, client_id: str, send_coro: Callable[[str], "asyncio.Future"]):
        self.client_id = client_id
        self._send = send_coro
        self.last_seen = time.monotonic()
        self.hello_done = False

    async def send(self, raw: str) -> None:
        await self._send(raw)

    def touch(self) -> None:
        self.last_seen = time.monotonic()


class ClientManager:
    def __init__(self, state: StateStore, heartbeat_timeout: float = 30.0) -> None:
        self._clients: Dict[str, Client] = {}
        self._state = state
        self.heartbeat_timeout = heartbeat_timeout

    def register(self, client: Client) -> None:
        self._clients[client.client_id] = client
        logger.info("client registered", extra={"client": client.client_id,
                                                "total": len(self._clients)})

    def unregister(self, client_id: str) -> None:
        self._clients.pop(client_id, None)
        logger.info("client unregistered", extra={"client": client_id,
                                                  "total": len(self._clients)})

    def get(self, client_id: str) -> Optional[Client]:
        return self._clients.get(client_id)

    def all_client_ids(self) -> Set[str]:
        return set(self._clients.keys())

    def touch(self, client_id: str) -> None:
        c = self._clients.get(client_id)
        if c:
            c.touch()

    def is_stale(self, client: Client) -> bool:
        return (time.monotonic() - client.last_seen) > self.heartbeat_timeout

    async def broadcast(self, raw: str, only_viewers_of: Optional[LocoRef] = None) -> int:
        """Send `raw` to all clients, or only those viewing `only_viewers_of`.

        Returns the number of clients the message was sent to.
        """
        targets: list[Client]
        if only_viewers_of is None:
            targets = list(self._clients.values())
        else:
            viewer_ids = self._state.viewers_of(only_viewers_of)
            targets = [c for cid, c in self._clients.items() if cid in viewer_ids]
        count = 0
        for client in targets:
            try:
                await client.send(raw)
                count += 1
            except Exception as exc:  # noqa: BLE001
                logger.warning("broadcast send failed",
                               extra={"client": client.client_id, "err": str(exc)})
        return count

    def count(self) -> int:
        return len(self._clients)
