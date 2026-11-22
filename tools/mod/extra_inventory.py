#!/usr/bin/env python3
"""Recover additional verified RotWK function roots for field_gaps.py.

The output is a separate ignored CFG inventory, never binary bytes. Use only
addresses independently established as function starts in RW disassembly.
For example RW 0x4944B7 loads TheGlobalWeatherSystem and tail-jumps to the
HasLightning reader RW 0x719BE4. The module-root inventory can omit both.
"""
import argparse
from pathlib import Path
import rw_function_map as rm


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--game', type=Path, required=True)
    ap.add_argument('--start', action='append', type=lambda value: int(value, 0), required=True,
                    help='independently verified function entry VA (repeatable)')
    ap.add_argument('--out', type=Path, required=True, help='separate ignored JSON inventory')
    args = ap.parse_args(argv)
    if args.out.exists():
        ap.error('output already exists; choose a new path (avoid stale roots and overwriting existing inventories)')
    blocks, analyzer, _ = rm.load_function_blocks(args.game, args.out, [], set(args.start))
    print(f'{len(blocks)} functions recovered with {len(set(args.start))} extra roots ({analyzer}) -> {args.out}')


if __name__ == '__main__':
    main()
