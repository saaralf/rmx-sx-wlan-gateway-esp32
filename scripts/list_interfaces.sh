#!/usr/bin/env bash
# list_interfaces.sh — enumerate serial interfaces and USB descriptors.
# Useful to find the correct /dev/serial/by-id path for RMX952/SX825/SX852.
set -euo pipefail

echo "=== /dev/serial/by-id ==="
if ls /dev/serial/by-id/* >/dev/null 2>&1; then
  for d in /dev/serial/by-id/*; do
    echo "$d -> $(readlink -f "$d")"
  done
else
  echo "none"
fi

echo
echo "=== USB devices (vid:pid, vendor, product) ==="
if command -v lsusb >/dev/null 2>&1; then
  lsusb
else
  # fall back to sysfs
  for f in /sys/bus/usb/devices/*/idVendor; do
    dev=$(dirname "$f")
    vid=$(cat "$dev/idVendor" 2>/dev/null)
    pid=$(cat "$dev/idProduct" 2>/dev/null)
    vendor=$(cat "$dev/manufacturer" 2>/dev/null)
    product=$(cat "$dev/product" 2>/dev/null)
    serial=$(cat "$dev/serial" 2>/dev/null)
    echo "vid=$vid pid=$pid vendor='$vendor' product='$product' serial='$serial'"
  done
fi

echo
echo "=== Kernel driver binding (ttyUSB/ttyACM) ==="
for t in /sys/class/tty/ttyUSB* /sys/class/tty/ttyACM*; do
  [[ -e "$t" ]] || continue
  name=$(basename "$t")
  driver=$(readlink "$t/device/driver" 2>/dev/null | xargs basename 2>/dev/null || echo "?")
  echo "$name driver=$driver"
done
