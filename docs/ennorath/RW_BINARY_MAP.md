# RotWK whole-image map (tools/rw_object_model/binary_map.py, class_map.py)

game.dat SHA256 `5481de75b63e2248…`. Addresses, counts and statuses only: no game bytes.

## Functions

72135 functions (evidence: call 32053, code-imm 2817, data 26094, extra-block 56, vtable-slot 11115). Status by `RW 0x…` citations in engine/src: ported 13388, partial 310, library 2876, eh-glue 19547, compiler-glue 1603, non-code 385, not-reached 2376, none 31650.
`library`: the proven list (tools/mod/rw_library_functions.tsv) and the functions match_rotwk.py labels `library` (see below).
`eh-glue`: proven MSVC C++ EH compiler glue (__ehhandler$ stubs and FuncInfo::pUnwindMap funclets with 0 callers and no vtable slot).
`compiler-glue`: proven MSVC scalar deleting destructor wrappers whose wrapped ~Class() destructor is tracked separately.
`non-code`: proven phantom `evidence=data` entries with 0 callers, no vtable slot, and impossible user-mode x86 opcodes or initial `int3` padding.
`not-reached`: proven unreachable in `docs/ennorath/rw_verdicts.d/*.tsv` (with evidence; `too big` and `unchecked` remain `none`).
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

| subsystem | functions | ported | partial | library | glue/non-code | not-reached | none |
|---|---:|---:|---:|---:|---:|---:|---:|
| engine core / system | 14353 | 1797 | 44 | 114 | 4747 | 0 | 7651 |
| GameClient / draw | 10365 | 1060 | 30 | 67 | 3102 | 0 | 6106 |
| APT / UI / shell | 9443 | 758 | 29 | 82 | 3958 | 0 | 4616 |
| objects / modules | 6343 | 2681 | 51 | 58 | 1968 | 1269 | 316 |
| unclassified | 4643 | 963 | 29 | 19 | 220 | 0 | 3412 |
| network / online | 4445 | 134 | 6 | 4 | 1499 | 0 | 2802 |
| AI / pathfinding | 4111 | 1875 | 47 | 16 | 1184 | 143 | 846 |
| War of the Ring / LivingWorld | 3115 | 629 | 4 | 10 | 1229 | 282 | 961 |
| library | 2758 | 73 | 3 | 2326 | 20 | 0 | 336 |
| math / support libs | 2277 | 123 | 6 | 68 | 463 | 0 | 1617 |
| script engine | 2133 | 734 | 19 | 14 | 733 | 245 | 388 |
| combat / weapons | 1986 | 1074 | 9 | 18 | 530 | 205 | 150 |
| audio | 1677 | 204 | 3 | 11 | 514 | 0 | 945 |
| horde / locomotion | 1425 | 673 | 10 | 26 | 300 | 46 | 370 |
| INI / templates | 1064 | 262 | 14 | 23 | 283 | 0 | 482 |
| castles / structures | 823 | 249 | 5 | 5 | 332 | 121 | 111 |
| map / terrain logic | 451 | 77 | 1 | 9 | 191 | 65 | 108 |
| other | 394 | 8 | 0 | 1 | 24 | 0 | 361 |
| EH/compiler glue | 325 | 13 | 0 | 5 | 238 | 0 | 69 |
| Create-a-Hero | 4 | 1 | 0 | 0 | 0 | 0 | 3 |

Tables: RW_BINARY_MAP.tsv (one row per function), RW_CLASSES.tsv, RW_VCALLS.tsv.

## Limits (what the map does not know yet)

- Virtual calls: 19512 of 30615 have a receiver whose type is not proven, most often an argument of a
  virtual method (its callers are not all known) or a global written by code the inventory does not cover;
  6053 call through a pointer that is not a table load; 339 name a table or slot
  the model does not have at that subobject. `resolved virtual callers` is therefore a lower bound: 0 does not mean nobody calls it.
- Subsystem: 4643 functions have no subsystem (the BFME2 pairing found no partner or no label);
  the `?` labels are the pairing's guesses.
- Status is by citation in comments only (string literals such as stop descriptions are ignored): a function ported without a
  `RW 0x...` citation is `none`, a citation inside a body does not count.
- Refreshed by every maps run (tools/colab/nightly_maps.sh, binary_map()): the inventory and facts are cached on the Drive by
  game.dat's SHA256 and the tools' blob ids; the port status and the BFME2 pairing are recomputed each run.
- Users: the task dossier (tools/ci/port_dossier.py, section «Карта бинарника») and its disassembly (refs/arena/inventory holds this
  inventory's exact bounds since 2026-11-24).
