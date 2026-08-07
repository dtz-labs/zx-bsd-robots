#!/usr/bin/env python3
"""Validate checksums, loader autostart, and 48K CODE bounds in a TAP."""

from __future__ import annotations

import argparse
import struct
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class Header:
    kind: int
    filename: str
    length: int
    parameter_1: int
    parameter_2: int


def xor_is_zero(block: bytes) -> bool:
    value = 0
    for byte in block:
        value ^= byte
    return value == 0


def parse_blocks(raw: bytes) -> list[bytes]:
    blocks: list[bytes] = []
    offset = 0
    while offset < len(raw):
        if offset + 2 > len(raw):
            raise ValueError("truncated TAP block length")
        length = struct.unpack_from("<H", raw, offset)[0]
        offset += 2
        end = offset + length
        if length < 2 or end > len(raw):
            raise ValueError(f"invalid TAP block {len(blocks)} length {length}")
        block = raw[offset:end]
        if not xor_is_zero(block):
            raise ValueError(f"invalid checksum in TAP block {len(blocks)}")
        blocks.append(block)
        offset = end
    return blocks


def parse_header(block: bytes) -> Header:
    if len(block) != 19 or block[0] != 0:
        raise ValueError("expected a standard 19-byte TAP header")
    return Header(
        kind=block[1],
        filename=block[2:12].decode("ascii", "replace").rstrip(),
        length=struct.unpack_from("<H", block, 12)[0],
        parameter_1=struct.unpack_from("<H", block, 14)[0],
        parameter_2=struct.unpack_from("<H", block, 16)[0],
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("tap", type=Path)
    args = parser.parse_args()

    blocks = parse_blocks(args.tap.read_bytes())
    if len(blocks) != 4:
        raise SystemExit(f"invalid TAP: expected 4 blocks, found {len(blocks)}")

    loader_header = parse_header(blocks[0])
    code_header = parse_header(blocks[2])
    loader = blocks[1][1:-1]
    code = blocks[3][1:-1]

    if blocks[1][0] != 0xFF or blocks[3][0] != 0xFF:
        raise SystemExit("invalid TAP: a header is not followed by a data block")
    if loader_header.kind != 0 or code_header.kind != 3:
        raise SystemExit("invalid TAP: expected BASIC loader then CODE")
    if len(loader) != loader_header.length or len(code) != code_header.length:
        raise SystemExit("invalid TAP: header/data lengths disagree")
    if loader_header.parameter_1 == 0x8000:
        raise SystemExit("invalid TAP: BASIC loader has no autostart line")
    code_end = code_header.parameter_1 + len(code)
    if code_end > 0x10000:
        raise SystemExit("invalid TAP: CODE extends beyond 64K address space")

    print(
        "TAP OK: "
        f"loader {loader_header.filename!r} autostarts at "
        f"{loader_header.parameter_1}; CODE {code_header.filename!r} "
        f"loads ${code_header.parameter_1:04X}-${code_end - 1:04X} "
        f"({len(code)} bytes)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
