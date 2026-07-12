#!/usr/bin/env python3
"""serial_sniffer.py — capture raw RX/TX hex dumps from an SX/RMX interface.

Usage:
    python tools/serial_sniffer.py --device /dev/serial/by-id/... --baud 57600

Options:
    --device      serial path (default /dev/ttyUSB0)
    --baud        baud rate (default 57600)
    --hexdump     print a classic hex+ascii dump (default: compact hex)
    --timeout     read timeout seconds (default 0.05)
    --duration    stop after N seconds (default: run until Ctrl-C)
    --out         write binary capture to this file

The raw-data mode is OFF by default in the gateway; this standalone tool is the
safe way to capture traffic for protocol analysis without touching the bus.
"""
import argparse
import binascii
import sys
import time

try:
    import serial
except ImportError:
    print("pyserial required: pip install pyserial", file=sys.stderr)
    sys.exit(1)


def hexdump(data: bytes) -> str:
    lines = []
    for i in range(0, len(data), 16):
        chunk = data[i:i + 16]
        hexs = " ".join(f"{b:02X}" for b in chunk)
        ascii_ = "".join(chr(b) if 32 <= b < 127 else "." for b in chunk)
        lines.append(f"{i:04X}  {hexs:<48}  {ascii_}")
    return "\n".join(lines)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--device", default="/dev/ttyUSB0")
    ap.add_argument("--baud", type=int, default=57600)
    ap.add_argument("--hexdump", action="store_true")
    ap.add_argument("--timeout", type=float, default=0.05)
    ap.add_argument("--duration", type=float, default=0.0)
    ap.add_argument("--out", default=None)
    args = ap.parse_args()

    try:
        ser = serial.Serial(args.device, args.baud, timeout=args.timeout)
    except Exception as exc:
        print(f"cannot open {args.device}: {exc}", file=sys.stderr)
        return 1

    print(f"Sniffing {args.device} @ {args.baud} (Ctrl-C to stop)")
    binout = open(args.out, "wb") if args.out else None
    start = time.time()
    try:
        while True:
            if args.duration and (time.time() - start) > args.duration:
                break
            data = ser.read(64)
            if not data:
                continue
            ts = time.strftime("%H:%M:%S")
            if args.hexdump:
                print(f"[{ts}] RX\n{hexdump(data)}")
            else:
                print(f"[{ts}] RX: {binascii.hexlify(data).decode('ascii').upper()}")
            if binout:
                binout.write(data)
    except KeyboardInterrupt:
        print("\nstopped")
    finally:
        ser.close()
        if binout:
            binout.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
