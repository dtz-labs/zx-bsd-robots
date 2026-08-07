#!/usr/bin/env python3
"""Fail when a linked 48K machine image leaves too little stack room."""

from __future__ import annotations

import re
import sys
from pathlib import Path


MIN_STACK_RESERVE = 2048
REQUIRED = ("__BSS_END_tail", "__register_sp", "__crt_stack_size")


def parse_symbols(path: Path) -> dict[str, int]:
    symbols: dict[str, int] = {}
    pattern = re.compile(r"^([A-Za-z0-9_]+)\s*=\s*\$([0-9A-Fa-f]+)\b")
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        match = pattern.match(line)
        if match:
            symbols[match.group(1)] = int(match.group(2), 16)
    return symbols


def main(argv: list[str]) -> int:
    if len(argv) != 2:
        print(
            f"usage: {Path(argv[0]).name} build/zx-bsd-robots-48k.map",
            file=sys.stderr,
        )
        return 2

    map_path = Path(argv[1])
    if not map_path.is_file():
        print(f"error: map file not found: {map_path}", file=sys.stderr)
        return 2

    symbols = parse_symbols(map_path)
    missing = [name for name in REQUIRED if name not in symbols]
    if missing:
        print(f"error: missing map symbols: {', '.join(missing)}", file=sys.stderr)
        return 2

    bss_end = symbols["__BSS_END_tail"]
    stack_top = symbols["__register_sp"]
    stack_reserve = symbols["__crt_stack_size"]
    gap = stack_top - bss_end
    required = max(MIN_STACK_RESERVE, stack_reserve)
    if gap < required:
        print("48K RAM layout: NOT SAFE")
        print(f"  BSS ends at ${bss_end:04X}")
        print(f"  stack starts at ${stack_top:04X}")
        print(f"  only {gap} bytes remain; require at least {required}")
        return 1

    print("48K RAM layout: safe")
    print(f"  BSS end:   ${bss_end:04X}")
    print(f"  stack top: ${stack_top:04X}")
    print(f"  reserved:  {stack_reserve} bytes")
    print(f"  free gap:  {gap} bytes")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
