# RotWK whole-image map (tools/rw_object_model/binary_map.py, class_map.py)

game.dat SHA256 `5481de75b63e2248…`. Addresses, counts and statuses only: no game bytes.

## Functions

72135 functions (evidence: call 32053, code-imm 2817, data 26094, extra-block 56, vtable-slot 11115). Status by `RW 0x…` citations in engine/src: ported 11416, partial 325, library 2851, none 57543.
`library`: the proven list (tools/mod/rw_library_functions.tsv) and the functions match_rotwk.py labels `library` (see below).
`none` means no `RW 0x...` citation: the client side (draw, audio, UI, network) is rebuilt on Godot rather than ported line by line, so there `none` is not a to-do count.

Exact by construction: an entry has code evidence (the PE entry, a direct call, a slot of a table code installs) or is a data /
code pointer that decodes cleanly and owns no other function's instruction; a switch is followed only when its `cmp` / `ja` guard
proves the bound; every function is decoded again with the final entry set, so the result does not depend on discovery order.
Unresolved indirect jumps: 759; rejected candidates: 7746; call targets that do not decode: 8.

## Classes

3835 installed vtables, 2595 classes (keyed by the table their constructors / destructors install on
this + 0), 1614 with a proven base (a base constructor / destructor called on the same this, or on this + k for a
secondary base whose table the class also installs).

## Virtual calls

30615 `call [R + d]` sites: object type unproven 19512, receiver not a vtable load 6053, resolved 4711, table / slot mismatch 339.
A call is resolved only when its receiver's type is proven: a method's this (the class and its descendants), an object a
constructor or `new` in the same function built, a global / field whose every store holds a proven type, a return value or an
argument when all the function's call sites are known. A store through an unproven pointer is not counted (no alias analysis).

## By subsystem

The label of tools/re/match_rotwk.py (RotWK paired with BFME2 1.06 and its decomp) over this inventory, `?` labels folded in;
`library` there is code the port replaces wholesale.

| subsystem | functions | ported | partial | library | none |
|---|---:|---:|---:|---:|---:|
| unclassified | 12126 | 943 | 31 | 19 | 11133 |
| engine core / system | 11980 | 1748 | 45 | 114 | 10073 |
| GameClient / draw | 10151 | 1018 | 30 | 69 | 9034 |
| objects / modules | 6296 | 2127 | 60 | 59 | 4050 |
| APT / UI / shell | 6289 | 740 | 34 | 82 | 5433 |
| AI / pathfinding | 3821 | 1570 | 47 | 16 | 2188 |
| network / online | 3797 | 132 | 4 | 4 | 3657 |
| War of the Ring / LivingWorld | 2756 | 340 | 8 | 10 | 2398 |
| library | 2722 | 67 | 4 | 2295 | 356 |
| math / support libs | 2177 | 113 | 5 | 69 | 1990 |
| script engine | 2132 | 493 | 15 | 14 | 1610 |
| combat / weapons | 1985 | 853 | 11 | 20 | 1101 |
| audio | 1656 | 201 | 3 | 11 | 1441 |
| horde / locomotion | 1416 | 518 | 12 | 26 | 860 |
| INI / templates | 1002 | 255 | 11 | 23 | 713 |
| castles / structures | 791 | 232 | 4 | 5 | 550 |
| map / terrain logic | 443 | 43 | 1 | 9 | 390 |
| other | 371 | 9 | 0 | 1 | 361 |
| EH/compiler glue | 221 | 13 | 0 | 5 | 203 |
| Create-a-Hero | 3 | 1 | 0 | 0 | 2 |

Tables: RW_BINARY_MAP.tsv (one row per function), RW_CLASSES.tsv, RW_VCALLS.tsv.
