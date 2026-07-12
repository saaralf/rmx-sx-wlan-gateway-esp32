"""Configuration model and loader.

Validates every value at startup. A misconfigured file must produce a clear,
actionable error rather than a cryptic crash mid-run.
"""
from __future__ import annotations

import os
from typing import Dict, List, Optional

import yaml
from pydantic import BaseModel, Field, field_validator, ValidationError

from .models import BusType


class ServerConfig(BaseModel):
    host: str = "0.0.0.0"
    port: int = Field(8080, ge=1, le=65535)
    log_level: str = "INFO"
    max_clients: int = Field(10, ge=1, le=1000)
    max_message_size: int = Field(16384, ge=1024, le=1048576)
    heartbeat_timeout_seconds: float = Field(30.0, gt=1.0)
    log_dir: Optional[str] = "/var/log/rmx-sx-gateway"


class WifiConfig(BaseModel):
    ssid: str = "Modellbahn-Fahrregler"
    password: str = "bitte-aendern"
    network: str = "192.168.50.0/24"
    gateway: str = "192.168.50.1"
    dhcp_start: str = "192.168.50.100"
    dhcp_end: str = "192.168.50.200"
    country: str = "DE"
    channel: int = Field(6, ge=1, le=14)
    interface: str = "wlan0"


class SafetyConfig(BaseModel):
    stop_on_client_disconnect: bool = False
    client_disconnect_timeout_seconds: float = Field(30.0, ge=0.0)
    reject_commands_when_interface_offline: bool = True


class InterfaceConfig(BaseModel):
    id: str = Field(..., min_length=1)
    type: str = Field(..., pattern="^(RMX952|SX825|SX852|Simulator)$")
    device: Optional[str] = None
    enabled: bool = True
    baudrate: int = 57600
    buses: List[BusType] = Field(default_factory=list)
    speed_steps: int = Field(32, ge=2, le=128)

    @field_validator("buses")
    @classmethod
    def _nonempty(cls, v):
        if not v:
            raise ValueError("at least one bus must be listed")
        return v


class LocomotiveConfig(BaseModel):
    id: str = Field(..., min_length=1)
    name: str = ""
    interface: str = Field(..., min_length=1)
    bus: BusType
    address: int = Field(..., ge=0, le=111)
    speed_steps: int = Field(31, ge=2, le=128)
    visible_functions: List[int] = Field(default_factory=list)

    @field_validator("visible_functions")
    @classmethod
    def _fns(cls, v):
        if len(v) > 8:
            raise ValueError("at most 8 visible functions on the throttle UI")
        for f in v:
            if f < 0 or f > 31:
                raise ValueError("function index must be 0..31")
        return v


class GatewayConfig(BaseModel):
    server: ServerConfig = Field(default_factory=ServerConfig)
    wifi: WifiConfig = Field(default_factory=WifiConfig)
    safety: SafetyConfig = Field(default_factory=SafetyConfig)
    interfaces: List[InterfaceConfig] = Field(default_factory=list)
    locomotives: List[LocomotiveConfig] = Field(default_factory=list)

    @field_validator("interfaces")
    @classmethod
    def _unique_interface_ids(cls, v):
        ids = [i.id for i in v]
        if len(ids) != len(set(ids)):
            raise ValueError("interface ids must be unique")
        return v

    def interface_by_id(self, iid: str) -> Optional[InterfaceConfig]:
        for i in self.interfaces:
            if i.id == iid:
                return i
        return None

    def loco_by_id(self, lid: str) -> Optional[LocomotiveConfig]:
        for l in self.locomotives:
            if l.id == lid:
                return l
        return None


def load_config(path: str) -> GatewayConfig:
    if not os.path.exists(path):
        raise FileNotFoundError(f"config file not found: {path}")
    with open(path, "r", encoding="utf-8") as fh:
        raw = yaml.safe_load(fh)
    if raw is None:
        raw = {}
    try:
        return GatewayConfig(**raw)
    except ValidationError as exc:
        # Re-raise with a clearer message but keep the structured detail.
        raise ConfigError(f"Invalid configuration in {path}:\n{exc}") from exc


class ConfigError(Exception):
    pass
