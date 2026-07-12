"""Driver package.

Concrete drivers:
  * simulator  -> SimulatorDriver   (no hardware, fully implemented)
  * RMX952     -> RMX952Driver      (implemented after RMX docs analysis)
  * SX825      -> SX825Driver       (implemented after SX reference analysis)
  * SX852      -> SX852Driver       (implemented after SX reference analysis)

The Pi NEVER drives the RMX/SX bus over GPIO itself. It only talks to a
ready-made PC interface over USB/serial.
"""
from .base import (
    Capabilities,
    InterfaceDriver,
    InterfaceEvent,
    InterfaceEventType,
)
from .simulator import SimulatorDriver

__all__ = [
    "InterfaceDriver",
    "InterfaceEvent",
    "InterfaceEventType",
    "Capabilities",
    "SimulatorDriver",
]
