"""WebSocket JSON protocol: message types, validation, encode/decode.

This module is the single source of truth for the wire protocol between
ESP32 throttles and the Raspberry Pi gateway. Every message is a JSON object
with a `type` field. Unknown/extra fields are rejected to keep the protocol
strict and versionable.

Protocol version: 1
"""
from __future__ import annotations

import json
from datetime import datetime, timezone
from enum import Enum
from typing import Any, Dict, List, Optional

from pydantic import BaseModel, ConfigDict, Field, ValidationError, field_validator

from .models import BusType, Direction, LocoRef

PROTOCOL_VERSION = 1
SERVER_NAME = "rmx-sx-gateway"
SERVER_VERSION = "0.1.0"
MAX_MESSAGE_SIZE = 16384  # hard cap, also enforced by the ws server


class MsgType(str, Enum):
    HELLO = "hello"
    HELLO_ACK = "hello_ack"
    PING = "ping"
    PONG = "pong"
    SELECT_LOCO = "select_loco"
    DRIVE = "drive"
    FUNCTION = "function"
    LOCO_STOP = "loco_stop"
    EMERGENCY_STOP = "emergency_stop"
    LOCO_STATUS = "loco_status"
    COMMAND_ACK = "command_ack"
    INTERFACE_STATUS = "interface_status"
    REQUEST_STATE = "request_state"
    STATE_SNAPSHOT = "state_snapshot"
    CLAIM_LOCO = "claim_loco"
    RELEASE_LOCO = "release_loco"
    ERROR = "error"


# All wire messages strictly forbid unknown/extra fields so the protocol
# stays closed and versionable.
_FORBID_EXTRA = ConfigDict(extra="forbid")


# ---------------------------------------------------------------------------
# Helper validators
# ---------------------------------------------------------------------------
def _validate_speed(v: int) -> int:
    if not isinstance(v, int) or isinstance(v, bool):
        raise ValueError("speed must be an integer")
    if v < 0 or v > 9999:
        raise ValueError("speed out of range")
    return v


def _validate_function_number(v: int) -> int:
    if not isinstance(v, int) or isinstance(v, bool):
        raise ValueError("function must be an integer")
    if v < 0 or v > 31:
        raise ValueError("function out of range 0..31")
    return v


def _utcnow_iso() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


# ---------------------------------------------------------------------------
# Request / response models (clients -> server and server -> clients)
# ---------------------------------------------------------------------------
class HelloMessage(BaseModel):
    type: MsgType = MsgType.HELLO
    model_config = _FORBID_EXTRA
    client_id: str = Field(..., min_length=1, max_length=64)
    protocol_version: int
    device: str = "ESP32-2432S028R"
    firmware_version: str = "0.1.0"

    @field_validator("protocol_version")
    @classmethod
    def _pv(cls, v):
        if v != PROTOCOL_VERSION:
            # We still accept lower versions best-effort; but record mismatch.
            pass
        return v


class HelloAck(BaseModel):
    type: MsgType = MsgType.HELLO_ACK
    model_config = _FORBID_EXTRA
    protocol_version: int = PROTOCOL_VERSION
    server: str = SERVER_NAME
    server_version: str = SERVER_VERSION
    status: str = "ready"


class PingMessage(BaseModel):
    type: MsgType = MsgType.PING
    model_config = _FORBID_EXTRA
    sequence: int


class PongMessage(BaseModel):
    type: MsgType = MsgType.PONG
    model_config = _FORBID_EXTRA
    sequence: int


class SelectLoco(BaseModel):
    type: MsgType = MsgType.SELECT_LOCO
    model_config = _FORBID_EXTRA
    interface: str = Field(..., min_length=1)
    bus: BusType
    address: int = Field(..., ge=0, le=111)


class DriveCommand(BaseModel):
    type: MsgType = MsgType.DRIVE
    model_config = _FORBID_EXTRA
    interface: str = Field(..., min_length=1)
    bus: BusType
    address: int = Field(..., ge=0, le=111)
    speed: int
    direction: Direction
    sequence: int

    @field_validator("speed")
    @classmethod
    def _spd(cls, v):
        return _validate_speed(v)


class FunctionCommand(BaseModel):
    type: MsgType = MsgType.FUNCTION
    model_config = _FORBID_EXTRA
    interface: str = Field(..., min_length=1)
    bus: BusType
    address: int = Field(..., ge=0, le=111)
    function: int
    state: bool
    sequence: int

    @field_validator("function")
    @classmethod
    def _fn(cls, v):
        return _validate_function_number(v)


class LocoStop(BaseModel):
    type: MsgType = MsgType.LOCO_STOP
    model_config = _FORBID_EXTRA
    interface: str = Field(..., min_length=1)
    bus: BusType
    address: int = Field(..., ge=0, le=111)
    sequence: int


class EmergencyStop(BaseModel):
    type: MsgType = MsgType.EMERGENCY_STOP
    model_config = _FORBID_EXTRA
    interface: str = Field(..., min_length=1)
    bus: Optional[BusType] = None
    sequence: int


class CommandAck(BaseModel):
    type: MsgType = MsgType.COMMAND_ACK
    model_config = _FORBID_EXTRA
    sequence: int
    status: str            # "accepted" | "rejected"
    error: Optional[str] = None


class LocoStatusMessage(BaseModel):
    type: MsgType = MsgType.LOCO_STATUS
    model_config = _FORBID_EXTRA
    interface: str
    bus: BusType
    address: int
    speed: int
    direction: Direction
    functions: Dict[int, bool] = Field(default_factory=dict)
    source: str = "interface"
    timestamp: str = Field(default_factory=_utcnow_iso)


class InterfaceStatusMessage(BaseModel):
    type: MsgType = MsgType.INTERFACE_STATUS
    model_config = _FORBID_EXTRA
    interface: str
    connected: bool
    device: Optional[str] = None
    type_name: Optional[str] = None


class RequestState(BaseModel):
    type: MsgType = MsgType.REQUEST_STATE
    model_config = _FORBID_EXTRA
    interface: str = Field(..., min_length=1)
    bus: BusType
    address: int = Field(..., ge=0, le=111)


class LocoStatePayload(BaseModel):
    interface: str
    bus: BusType
    address: int
    speed: int
    direction: Direction
    functions: Dict[int, bool] = Field(default_factory=dict)
    last_update: Optional[str] = None
    source: Optional[str] = None
    connected: Optional[bool] = None
    owner: Optional[str] = None
    version: Optional[int] = None


class StateSnapshot(BaseModel):
    type: MsgType = MsgType.STATE_SNAPSHOT
    model_config = _FORBID_EXTRA
    locomotive: LocoStatePayload


class ClaimLoco(BaseModel):
    type: MsgType = MsgType.CLAIM_LOCO
    model_config = _FORBID_EXTRA
    interface: str = Field(..., min_length=1)
    bus: BusType
    address: int = Field(..., ge=0, le=111)
    client_id: Optional[str] = None


class ReleaseLoco(BaseModel):
    type: MsgType = MsgType.RELEASE_LOCO
    model_config = _FORBID_EXTRA
    interface: str = Field(..., min_length=1)
    bus: BusType
    address: int = Field(..., ge=0, le=111)
    client_id: Optional[str] = None


class ErrorMessage(BaseModel):
    type: MsgType = MsgType.ERROR
    model_config = _FORBID_EXTRA
    code: str
    message: str


# Map type string -> model class
MODEL_BY_TYPE = {
    MsgType.HELLO: HelloMessage,
    MsgType.PING: PingMessage,
    MsgType.SELECT_LOCO: SelectLoco,
    MsgType.DRIVE: DriveCommand,
    MsgType.FUNCTION: FunctionCommand,
    MsgType.LOCO_STOP: LocoStop,
    MsgType.EMERGENCY_STOP: EmergencyStop,
    MsgType.REQUEST_STATE: RequestState,
    MsgType.CLAIM_LOCO: ClaimLoco,
    MsgType.RELEASE_LOCO: ReleaseLoco,
    # server -> client types are only used for encoding
    MsgType.HELLO_ACK: HelloAck,
    MsgType.PONG: PongMessage,
    MsgType.LOCO_STATUS: LocoStatusMessage,
    MsgType.COMMAND_ACK: CommandAck,
    MsgType.INTERFACE_STATUS: InterfaceStatusMessage,
    MsgType.STATE_SNAPSHOT: StateSnapshot,
    MsgType.ERROR: ErrorMessage,
}


def parse_message(raw: str) -> Any:
    """Parse and validate a raw JSON string into a typed message model.

    Raises ProtocolError on malformed JSON, unknown type, or validation error.
    """
    try:
        data = json.loads(raw)
    except (json.JSONDecodeError, UnicodeDecodeError) as exc:
        raise ProtocolError(f"invalid JSON: {exc}") from exc
    if not isinstance(data, dict):
        raise ProtocolError("message must be a JSON object")
    type_str = data.get("type")
    if type_str is None:
        raise ProtocolError("missing 'type' field")
    try:
        msg_type = MsgType(type_str)
    except ValueError:
        raise ProtocolError(f"unknown message type: {type_str!r}")
    model = MODEL_BY_TYPE.get(msg_type)
    if model is None:
        raise ProtocolError(f"message type {msg_type} is not client->server")
    try:
        return model(**data)
    except ValidationError as exc:
        raise ProtocolError(f"validation failed for {msg_type.value}: {exc}") from exc


def encode(model: BaseModel) -> str:
    """Serialize a response model to a JSON string."""
    return model.model_dump_json(exclude_none=True)


class ProtocolError(Exception):
    """Raised when a message fails to parse or validate."""

