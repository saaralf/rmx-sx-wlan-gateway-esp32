"""Tests: JSON protocol parsing/validation/encoding."""
import pytest
from rmx_sx_gateway import protocol as P
from rmx_sx_gateway.protocol import (
    parse_message, encode, ProtocolError, DriveCommand, MsgType, HelloMessage,
)


def test_hello_roundtrip():
    raw = ('{"type":"hello","client_id":"t1","protocol_version":1,'
           '"device":"ESP32-2432S028R","firmware_version":"0.1.0"}')
    m = parse_message(raw)
    assert isinstance(m, HelloMessage)
    assert m.client_id == "t1"
    assert m.protocol_version == 1
    out = encode(m)
    assert '"type":"hello"' in out


def test_drive_valid():
    m = parse_message('{"type":"drive","interface":"sim1","bus":"RMX0",'
                      '"address":110,"speed":18,"direction":"forward","sequence":5}')
    assert m.speed == 18 and m.direction.value == "forward"
    assert m.sequence == 5


def test_drive_invalid_speed():
    with pytest.raises(ProtocolError):
        parse_message('{"type":"drive","interface":"sim1","bus":"RMX0",'
                      '"address":110,"speed":99999,"direction":"forward","sequence":5}')


def test_drive_invalid_bus():
    with pytest.raises(ProtocolError):
        parse_message('{"type":"drive","interface":"sim1","bus":"RMX9",'
                      '"address":110,"speed":5,"direction":"forward","sequence":5}')


def test_drive_invalid_direction():
    with pytest.raises(ProtocolError):
        parse_message('{"type":"drive","interface":"sim1","bus":"RMX0",'
                      '"address":110,"speed":5,"direction":"sideways","sequence":5}')


def test_function_number_out_of_range():
    with pytest.raises(ProtocolError):
        parse_message('{"type":"function","interface":"sim1","bus":"SX0",'
                      '"address":42,"function":32,"state":true,"sequence":7}')


def test_unknown_type_rejected():
    with pytest.raises(ProtocolError):
        parse_message('{"type":"bogus"}')


def test_missing_type_rejected():
    with pytest.raises(ProtocolError):
        parse_message('{"address":42}')


def test_invalid_json_rejected():
    with pytest.raises(ProtocolError):
        parse_message('{not json')


def test_extra_field_rejected():
    # pydantic forbids extra fields by default
    with pytest.raises(ProtocolError):
        parse_message('{"type":"ping","sequence":1,"extra":"x"}')


def test_ping_pong():
    m = parse_message('{"type":"ping","sequence":9}')
    assert m.sequence == 9
    out = encode(P.PongMessage(sequence=9))
    assert '"type":"pong"' in out and '"sequence":9' in out
