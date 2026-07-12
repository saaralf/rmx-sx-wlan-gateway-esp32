"""Structured logging configuration.

Supports three targets, all combinable:
  * stderr (default, useful for foreground/container runs)
  * rotating log files on disk
  * systemd journal (via systemd.journal_handler if installed)

The JSON formatter emits one structured line per record with the fields:
level, logger, message, timestamp, module, plus any extra attributes that
were passed via `logger.info("msg", extra={...})`.
"""
from __future__ import annotations

import json
import logging
import logging.handlers
import sys
from typing import Any, Optional


class JsonFormatter(logging.Formatter):
    def format(self, record: logging.LogRecord) -> str:
        payload: dict[str, Any] = {
            "ts": self.formatTime(record, "%Y-%m-%dT%H:%M:%SZ"),
            "level": record.levelname,
            "logger": record.name,
            "msg": record.getMessage(),
        }
        if record.exc_info:
            payload["exc"] = self.formatException(record.exc_info)
        # surface structured extras
        for key, value in record.__dict__.items():
            if key not in (
                "args", "asctime", "created", "exc_info", "exc_text", "filename",
                "funcName", "levelname", "levelno", "lineno", "module", "msecs",
                "message", "msg", "name", "pathname", "process", "processName",
                "relativeCreated", "stack_info", "thread", "threadName", "taskName",
            ):
                payload[key] = value
        return json.dumps(payload, default=str)


def setup_logging(
    level: str = "INFO",
    log_dir: Optional[str] = None,
    to_journald: bool = False,
    to_stderr: bool = True,
) -> logging.Logger:
    root = logging.getLogger()
    root.setLevel(getattr(logging, level.upper(), logging.INFO))

    # Clear any pre-existing handlers to keep idempotent.
    for h in list(root.handlers):
        root.removeHandler(h)
        h.close()

    fmt = "%(asctime)s %(levelname)s %(name)s %(message)s"
    plain = logging.Formatter(fmt, "%Y-%m-%dT%H:%M:%SZ")

    if to_stderr:
        sh = logging.StreamHandler(sys.stderr)
        sh.setFormatter(plain)
        root.addHandler(sh)

    if to_journald:
        try:
            from systemd.journal import JournalHandler  # type: ignore
            jh = JournalHandler()
            jh.setFormatter(logging.Formatter("%(levelname)s %(name)s %(message)s"))
            root.addHandler(jh)
        except Exception as exc:  # pragma: no cover
            logging.getLogger(__name__).warning(
                "journald logging requested but unavailable: %s", exc
            )

    if log_dir:
        from pathlib import Path
        Path(log_dir).mkdir(parents=True, exist_ok=True)
        fh = logging.handlers.RotatingFileHandler(
            f"{log_dir}/rmx-sx-gateway.log",
            maxBytes=5_000_000,
            backupCount=5,
            encoding="utf-8",
        )
        fh.setFormatter(JsonFormatter())
        root.addHandler(fh)

    logging.getLogger("aiohttp").setLevel(logging.WARNING)
    logging.getLogger("asyncio").setLevel(logging.WARNING)
    return logging.getLogger("rmx_sx_gateway")
