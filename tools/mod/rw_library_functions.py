#!/usr/bin/env python3
"""The RotWK functions the port never ports one by one: library, compiler and debug code (owner 2026-11-21).

The function map (tools/mod/rw_function_map.py, docs/ennorath/RW_FUNCTION_MAP.tsv) walks every module's call graph; a function
counts as ported only when engine/src cites its first instruction. The walk also reaches code the port replaces wholesale:
the C++ standard library the game linked (STLport containers, strings), the C runtime, compiler-generated helpers (deleting
destructors, SEH prologs), the game's debug hooks, and the engine infrastructure the port has its own of (the INI reader,
Xfer / CRC, the allocator: rw_function_map.HAND_STOPS). Such a function stays uncited forever; counting it as open work
overstates what is left. This tool writes their list, tools/mod/rw_library_functions.tsv (addresses and names only, no
game bytes); the map marks them `library` and the counts (progress_snapshot, MATRIX_GAPS, the task dossiers) leave them out.

    python tools/mod/rw_library_functions.py [--map docs/ennorath/RW_FUNCTION_MAP.tsv] [--out tools/mod/rw_library_functions.tsv]

The rule works on the map's `decomp` column (the BFME2 decomp's name of the matched function, Port 240) and is strict on
purpose: a container or string class of the standard library, a C runtime name (no class, leading underscore), a
compiler-generated destructor, the game's debug hooks, and the hand stop list. A game template (e.g.
`SparseMatchFinder<>`) is not library. A function with no decomp name is never listed here: it stays `none` until a
task classifies it. A wrong entry would hide a real gap: add by hand only with the reason, and review the diff.
"""
from __future__ import annotations

import argparse
import csv
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

STL = re.compile(r"(^|::|\b)(std::|_STL::|stlp_std::|_Tree|_Rb_tree|_Hashtable|_Ht_iterator<>|hashtable<>|basic_string|allocator|"
                 r"(map|multimap|vector|list|set|multiset|hash_map|hash_set|hash_multimap|deque|pair|less|equal_to)<>)")
STRINGS = re.compile(r"(^|::)(AsciiString|UnicodeString|StringBase<>)(::|$)")
CRT = re.compile(r"^_{1,2}(SEH_prolog|SEH_epilog|memset|memcpy|memmove|memcmp|strcmp|strncmp|strcpy|strncpy|strcat|strncat|strlen|stricmp|"
                 r"strnicmp|atexit|atoi|atof|ftol2?|tolower|toupper|isprint|isspace|isalpha|isdigit|isalnum|upper_bound|lower_bound|"
                 r"security_check_cookie|CxxThrowException|purecall|allmul|alldiv|aulldiv|allrem|aullrem|chkstk|alloca_probe)$")
# the Lua 4 virtual machine the game links (its C API `_lua_*` and the internals around it): the port runs its own Lua
LUA_API = re.compile(r"^_lua[A-Za-z_]")
DEBUG = re.compile(r"^_bfme_debug|^DebugLog|^DEBUG_")
COMPILER = re.compile(r"`(scalar|vector) deleting destructor'")


def classify(name: str, address: int = 0, lua_block: tuple[int, int] = (0, 0)) -> str | None:
    n = re.sub(r"\s*\(tier [A-Z]\)\s*$", "", name or "").strip()
    if not n:
        return None
    if DEBUG.search(n):
        return "debug hook of the game"
    if COMPILER.search(n):
        return "compiler-generated deleting destructor"
    if STL.search(n):
        return "C++ standard library container (STLport)"
    if STRINGS.search(n):
        return "string class (AsciiString / UnicodeString)"
    if "::" not in n and (CRT.match(n) or n.startswith("operator new") or n.startswith("operator delete")):
        return "C runtime / compiler helper"
    if "::" not in n and (LUA_API.match(n) or (0 < lua_block[0] <= address <= lua_block[1] and n.startswith("_") and not n.startswith("_bfme"))):
        return "Lua virtual machine (the port runs its own Lua)"
    return None


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--map", type=Path, default=Path("docs/ennorath/RW_FUNCTION_MAP.tsv"))
    ap.add_argument("--out", type=Path, default=Path("tools/mod/rw_library_functions.tsv"))
    a = ap.parse_args(argv)
    from rw_function_map import HAND_STOPS  # here, not at import: load() is used without capstone
    rows = {}
    for address, name in HAND_STOPS.items():
        rows[address] = (name, "port infrastructure or library (rw_function_map.HAND_STOPS)")
    with a.map.open(encoding="utf-8") as f:
        table = list(csv.DictReader(f, delimiter="\t"))
    bare = lambda r: re.sub(r"\s*\(tier [A-Z]\)\s*$", "", r.get("decomp") or "").strip()
    lua = [int(r["function"], 16) for r in table if LUA_API.match(bare(r))]
    lua_block = (min(lua), max(lua)) if lua else (0, 0)  # the span of the `_lua*` functions: the VM's own code
    for r in table:
        address = int(r["function"], 16)
        why = classify(r.get("decomp", ""), address, lua_block)
        if why and address not in rows:
            rows[address] = (bare(r), why)
    with a.out.open("w", encoding="utf-8", newline="") as f:
        f.write("# RotWK functions the port replaces wholesale (tools/mod/rw_library_functions.py): the function map marks them `library`.\n")
        f.write("address\tname\treason\n")
        for address in sorted(rows):
            name, why = rows[address]
            f.write(f"0x{address:08X}\t{name}\t{why}\n")
    print(f"{a.out}: {len(rows)} functions")
    return 0


def load(path: Path = Path(__file__).resolve().parent / "rw_library_functions.tsv") -> dict[int, str]:
    """address -> reason, for the map and the counts; empty when the list is missing."""
    if not path.exists():
        return {}
    out = {}
    with path.open(encoding="utf-8") as f:
        for line in f:
            if line.startswith("#") or line.startswith("address\t") or not line.strip():
                continue
            address, _name, why = line.rstrip("\n").split("\t")
            out[int(address, 16)] = why
    return out


if __name__ == "__main__":
    sys.exit(main())
