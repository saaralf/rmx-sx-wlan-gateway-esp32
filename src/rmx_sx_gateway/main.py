"""Application entrypoint and orchestration.

Wires together: configuration, logging, state store, client manager,
interface manager (drivers) and the WebSocket server. Handles SIGTERM/SIGINT
gracefully and runs the whole stack under a single asyncio event loop.
"""
from __future__ import annotations

import argparse
import asyncio
import logging
import os
import signal
import sys

from . import __version__
from .config import load_config
from .client_manager import ClientManager
from .interface_manager import InterfaceManager, set_client_manager
from .logging_config import setup_logging
from .state import StateStore
from .websocket_server import WebSocketServer

logger = logging.getLogger(__name__)

DEFAULT_CONFIG = os.environ.get(
    "RMX_SX_CONFIG", "/etc/rmx-sx-gateway/gateway.yaml")


async def run(config_path: str) -> int:
    try:
        cfg = load_config(config_path)
    except Exception as exc:  # noqa: BLE001
        logging.getLogger("rmx_sx_gateway").error("config load failed: %s", exc)
        return 2

    log = setup_logging(
        level=cfg.server.log_level,
        log_dir=cfg.server.log_dir,
        to_journald=os.path.exists("/run/systemd/system"),
        to_stderr=True,
    )

    logger.info("starting rmx-sx-wlan-gateway",
                extra={"version": __version__, "config": config_path})
    logger.info("configured interfaces",
                extra={"interfaces": [i.id for i in cfg.interfaces]})

    state = StateStore()
    clients = ClientManager(state, heartbeat_timeout=cfg.server.heartbeat_timeout_seconds)

    # Build interface manager first so we can inject the client manager.
    iface_mgr = InterfaceManager(cfg, state)
    try:
        await iface_mgr.start()
    except NotImplementedError as exc:
        logger.error("interface driver not available: %s", exc)
        await iface_mgr.stop()
        return 3

    set_client_manager(clients)

    ws = WebSocketServer(cfg, state, clients, iface_mgr.drivers)
    await ws.start()

    stop = asyncio.Event()

    def _handler(_sig):
        logger.info("signal received, shutting down", extra={"signal": _sig})
        stop.set()

    loop = asyncio.get_running_loop()
    for s in (signal.SIGTERM, signal.SIGINT):
        try:
            loop.add_signal_handler(s, _handler, s)
        except NotImplementedError:
            pass

    try:
        await stop.wait()
    finally:
        logger.info("shutting down services")
        await ws.stop()
        await iface_mgr.stop()
        logger.info("shutdown complete")
    return 0


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(prog="rmx-sx-gateway",
                                     description="WLAN gateway for ESP32 throttles")
    parser.add_argument("-c", "--config", default=DEFAULT_CONFIG,
                        help="path to gateway.yaml")
    parser.add_argument("--version", action="store_true",
                        help="print version and exit")
    args = parser.parse_args(argv)

    if args.version:
        print(__version__)
        return 0

    if not os.path.exists(args.config):
        print(f"ERROR: config file not found: {args.config}", file=sys.stderr)
        return 2

    try:
        return asyncio.run(run(args.config))
    except KeyboardInterrupt:
        return 0


if __name__ == "__main__":
    raise SystemExit(main())
