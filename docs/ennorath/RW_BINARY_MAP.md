# RotWK whole-image map (tools/rw_object_model/binary_map.py, class_map.py)

game.dat SHA256 `5481de75b63e2248…`. Addresses, counts and statuses only: no game bytes.

## Functions

72135 functions (evidence: call 32053, code-imm 2817, data 26094, extra-block 56, vtable-slot 11115). Status by `RW 0x…` citations in engine/src: ported 11416, partial 325, library 2887, none 57507.
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
| engine core / system | 14353 | 1754 | 46 | 114 | 12439 |
| GameClient / draw | 10365 | 1018 | 30 | 69 | 9248 |
| APT / UI / shell | 9443 | 741 | 34 | 82 | 8586 |
| objects / modules | 6343 | 2127 | 60 | 59 | 4097 |
| unclassified | 4643 | 936 | 30 | 19 | 3658 |
| network / online | 4445 | 132 | 4 | 4 | 4305 |
| AI / pathfinding | 4111 | 1570 | 47 | 16 | 2478 |
| War of the Ring / LivingWorld | 3115 | 340 | 8 | 10 | 2757 |
| library | 2758 | 67 | 4 | 2331 | 356 |
| math / support libs | 2277 | 113 | 5 | 69 | 2090 |
| script engine | 2133 | 493 | 15 | 14 | 1611 |
| combat / weapons | 1986 | 853 | 11 | 20 | 1102 |
| audio | 1677 | 201 | 3 | 11 | 1462 |
| horde / locomotion | 1425 | 518 | 12 | 26 | 869 |
| INI / templates | 1064 | 255 | 11 | 23 | 775 |
| castles / structures | 823 | 232 | 4 | 5 | 582 |
| map / terrain logic | 451 | 43 | 1 | 9 | 398 |
| other | 394 | 9 | 0 | 1 | 384 |
| EH/compiler glue | 325 | 13 | 0 | 5 | 307 |
| Create-a-Hero | 4 | 1 | 0 | 0 | 3 |

Tables: RW_BINARY_MAP.tsv (one row per function), RW_CLASSES.tsv, RW_VCALLS.tsv.
