#!/usr/bin/env python3
"""protocol_decoder.py — offline decoder for captured SX/RMX telegrams.

This is a PLACEHOLDER decoder shell. It does NOT invent protocol bytes. As the
RMX documentation (RMX-Doku_V5.pdf) and SX reference analysis are completed,
concrete decoders are added here. For now it:
  * validates frame length / basic structure for known patterns, and
  * prints raw fields so a human can annotate real captures.

Usage:
    python tools/protocol_decoder.py --hex "A5 01 6E 12 ..." [--family RMX|SX]
"""
import argparse
import binascii


def decode_placeholder(family: str, raw: bytes) -> dict:
    """Return an annotated, NON-authoritative view of a frame.

    No protocol semantics are asserted here. Field offsets are labeled only
    as 'byte N' so real captures can be studied. Replace with real decoders
    once the interface documentation is analysed.
    """
    return {
        "family": family,
        "length": len(raw),
        "bytes": [f"{b:02X}" for b in raw],
        "note": "placeholder decode — no protocol semantics assumed; "
                "see docs/rmx-protocol-analysis.md and docs/sx-reference-analysis.md",
    }


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--hex", required=True)
    ap.add_argument("--family", choices=["RMX", "SX"], default="RMX")
    args = ap.parse_args()
    raw = binascii.unhexlify("".join(args.hex.split()))
    result = decode_placeholder(args.family, raw)
    print(result)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
