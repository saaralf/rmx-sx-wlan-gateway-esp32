#!/usr/bin/env python3
"""send_test_frame.py — send a single raw frame to an interface (diagnostics).

ONLY send frames whose format is documented/safe. This tool never invents
protocol bytes; it transmits exactly the bytes you give it as hex.

Usage:
    python tools/send_test_frame.py --device /dev/serial/by-id/... \
        --baud 57600 --hex "A5 01 6E 12 ..."

Use --echo-only to just print what WOULD be sent without touching hardware.
"""
import argparse
import binascii
import sys

try:
    import serial
except ImportError:
    print("pyserial required", file=sys.stderr)
    sys.exit(1)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--device", default="/dev/ttyUSB0")
    ap.add_argument("--baud", type=int, default=57600)
    ap.add_argument("--hex", required=True, help="space-separated hex bytes")
    ap.add_argument("--echo-only", action="store_true")
    args = ap.parse_args()

    raw = binascii.unhexlify("".join(args.hex.split()))
    if args.echo_only:
        print(f"Would send ({len(raw)} bytes): {raw.hex(' ').upper()}")
        return 0
    try:
        ser = serial.Serial(args.device, args.baud, timeout=0.1)
    except Exception as exc:
        print(f"cannot open {args.device}: {exc}", file=sys.stderr)
        return 1
    try:
        ser.write(raw)
        print(f"Sent {len(raw)} bytes: {raw.hex(' ').upper()}")
        # optional: read a short response
        resp = ser.read(64)
        if resp:
            print(f"RX: {resp.hex(' ').upper()}")
    finally:
        ser.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
