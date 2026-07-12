"""Tests: configuration validation and loading."""
import pytest
from rmx_sx_gateway.config import load_config, ConfigError, GatewayConfig
from rmx_sx_gateway.models import BusType

GOOD_YAML = """
server:
  port: 8080
interfaces:
  - id: sim1
    type: Simulator
    enabled: true
    buses: [RMX0, SX0]
locomotives:
  - id: br110
    name: BR 110
    interface: sim1
    bus: RMX0
    address: 110
    visible_functions: [1, 2, 3, 4, 9, 10, 11, 12]
"""


def test_valid_config_parses():
    import tempfile, os
    with tempfile.NamedTemporaryFile("w", suffix=".yaml", delete=False) as f:
        f.write(GOOD_YAML)
        path = f.name
    try:
        cfg = load_config(path)
        assert cfg.interfaces[0].id == "sim1"
        assert cfg.locomotives[0].address == 110
    finally:
        os.unlink(path)


def test_duplicate_interface_ids_rejected():
    import tempfile, os
    yaml = """
interfaces:
  - id: dup
    type: Simulator
    enabled: true
    buses: [RMX0]
  - id: dup
    type: Simulator
    enabled: true
    buses: [RMX0]
"""
    with tempfile.NamedTemporaryFile("w", suffix=".yaml", delete=False) as f:
        f.write(yaml); path = f.name
    try:
        with pytest.raises(ConfigError):
            load_config(path)
    finally:
        os.unlink(path)


def test_invalid_interface_type_rejected():
    import tempfile, os
    yaml = """
interfaces:
  - id: x
    type: NotARealType
    enabled: true
    buses: [RMX0]
"""
    with tempfile.NamedTemporaryFile("w", suffix=".yaml", delete=False) as f:
        f.write(yaml); path = f.name
    try:
        with pytest.raises(ConfigError):
            load_config(path)
    finally:
        os.unlink(path)


def test_too_many_visible_functions_rejected():
    import tempfile, os
    yaml = """
locomotives:
  - id: l
    interface: sim1
    bus: RMX0
    address: 1
    visible_functions: [1,2,3,4,5,6,7,8,9]
"""
    with tempfile.NamedTemporaryFile("w", suffix=".yaml", delete=False) as f:
        f.write(yaml); path = f.name
    try:
        with pytest.raises(ConfigError):
            load_config(path)
    finally:
        os.unlink(path)


def test_address_out_of_range_rejected():
    import tempfile, os
    yaml = """
locomotives:
  - id: l
    interface: sim1
    bus: RMX0
    address: 999
"""
    with tempfile.NamedTemporaryFile("w", suffix=".yaml", delete=False) as f:
        f.write(yaml); path = f.name
    try:
        with pytest.raises(ConfigError):
            load_config(path)
    finally:
        os.unlink(path)
