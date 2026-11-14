# PORT-162 / issue #380 — `AIData/SkirmishBuildList/Structure` in RotWK 2.01

Date: 2026-11-13. Source: `workspace/RotWK/game.dat`, 11,346,944 bytes,
MD5 `daed3668006cd90f01c34e5a7da1901f`; static disassembly with
`tools/mod/rw.py` (caveat S-001).

## Verdict

**SkirmishBuildList Location/Angle/InitiallyBuilt/Rebuilds/AutomaticallyBuild: parsed, no reachable reader в RotWK 2.01 (единственное тело RW 0x8F42BD..0x8F4556 без ссылок: ноль rel32 и ноль абсолютных указателей в образе).**

The only body found touching any of these records is RW `0x8F42BD–0x8F4556`.
The loaded image contains no rel32 `call` / `jmp` to `0x8F42BD` in `.text`
and no absolute pointer value `0x008F42BD` in `.text`, `.rdata`, or `.data`;
the body is also absent from the `AISkirmishPlayer` vtable. Its accesses to
`Location` and `InitiallyBuilt` therefore belong to unreachable code. The
other three fields are not accessed by that body, and no other reader was
found. The owner approved the no-reader resolution: documentation only, with
no runtime change, PORT-162 tests, or Ennorath in-game check. Do not port the
donor helper or upstream a behavior that RotWK does not reach.

## Main and donor checks

`origin/main` has no runtime `SkirmishBuildList` implementation. Its
`GameLogicSettings.cpp` catches the nested block and records
`AIData.SkirmishBuildList` in `unappliedFields`; `test_logic_settings.cpp`
pins parsing / nested-`End` handling only. `AIBaseBuilder` builds from the
separate `TheBaseTemplateLibrary` / `AIBase` data path. Routing this list into
that subsystem would create a parallel mechanism, not port an existing
consumer.

The BFME2 donor was checked first:
`/tmp/open-bfme-2/Code/GameEngine/Source/GameLogic/AI/AISkirmishPlayer.cpp`,
`AISkirmishPlayer::adjustBuildList` (around lines 820–920) checks for an
existing command centre, marks a matching `BuildListInfo` as initially built,
and shifts/rotates list locations relative to the start position. It ends with
`cur->setAngle(cur->getAngle())`, which preserves the angle rather than
transforming it. This is donor behavior only; it cannot override the RotWK
reachability result. The donor search did not establish a rebuild-count
consumer for these records.

## RotWK data creation and storage

The data descriptors and object chain are:

- `AIData/SkirmishBuildList`: row `0xC1CB20`, callback `0x7001DB`.
- Its `Structure` list row is `0xC1D878`, callback `0x72C3AB`; the
  `Structure` field table is `0xC23FF8`.
- `0x7001DB` parses a named list record and calls `0x7000AB`, which stores
  or replaces the list on the `AIData` object at `+0xF8`.
- `0x72C3AB` allocates an `0x80`-byte `BuildListInfo`, calls constructor
  `0x72BB37`, parses the `Structure` fields, and appends the record through
  `0x6FEBED`. The intrusive next link is `+0x2C`.
- Constructor `0x72BB37` installs vtable `0xC23F5C`. Its first four slots
  are `0x72BF24`, `0x63F3BF`, `0x72B94C`, and `0x72B645`; slot 3 is the
  fieldwise Xfer routine. Parsing, construction, cloning, and Xfer prove that
  the values are represented and preserved, not that gameplay consumes them.

## Field-by-field audit

| Field | Structure row / parser / object offset | Constructor default | Reader result |
|---|---|---|---|
| `Location` | `0xC24008` / `0x42F298` / `+0x0C` | zero vector (`0x72BBB8–0x72BBC2`) | The unreachable body accesses the coordinate at `0x8F4372–0x8F437A` and rewrites it at `0x8F44E1–0x8F453C`; no reachable reader. |
| `Angle` | `0xC24028` / `0x42EE15` / `+0x20` | `0.0f` (`0x72BB71`) | No access to this record field in the candidate body; no reachable reader. |
| `InitiallyBuilt` | `0xC24038` / `0x42E558` / `+0x24` | `false` (`0x72BB76`) | The unreachable body writes `true` at `0x8F437B`; no reachable reader. |
| `Rebuilds` | `0xC24018` / `0x42EC5E` / `+0x28` | `0` (`0x72BB79`) | No gameplay read found; parse/default/copy/Xfer only. |
| `AutomaticallyBuild` | `0xC24058` / `0x42E558` / `+0x3B` | `true` (`0x72BBB4`) | No gameplay read found. The `+0x3B` copy at `0x72B84F` and Xfer are not readers. |

The body `0x8F42BD–0x8F4556` traverses a linked list through `+0x2C`,
performs the `Location` / `InitiallyBuilt` operations above, and uses a
separately calculated map rotation to transform locations. It does not load
`BuildListInfo +0x20`, `+0x28`, or `+0x3B`.

## Reachability evidence

The class name adjacent to `AISkirmishPlayer` vtable `0xC79788` and the
constructor at `0x8F3DF3` (called from `0x6AA4E4`, after base constructor
`0x8F7F2B`) identify the surrounding object. The constructor installs
`0xC79788` at `0x8F3E1D`. The target begins at the function boundary with the
SEH prologue `mov eax, 0xBA7335; call 0xA3CEF0`, after the preceding
function's `ret 4`. However:

- `rw.py xref 0x8F42BD` and `rw.py xref 0x8F4556` return no direct references.
- The raw `.text` branch scan finds zero rel32 `call` / `jmp` targets at
  `0x8F42BD`; the absolute-address scan finds zero `0x008F42BD` values in the
  loaded image's `.text`, `.rdata`, and `.data` sections.
- The body is absent from the `0x6C` slots of vtable `0xC79788`.
- The `AIData +0xF8` parse/storage chain does not connect to this body. The
  independent RotWK batch-13 review also found no consumer of that list.

The `creation → constructor → vtable → slot → reader` chain therefore stops
at list storage. The no-reader verdict applies to all five fields; the two
accesses in the sole candidate body are unreachable. The approved disposition
is “no reachable reader,” not a port of the donor's behavior.

## Scope and resolution

- No `AIData` / `BuildListInfo` runtime member, alternate AI path, or behavior
  was introduced. No `*.hashes` file was changed.
- No synthetic PORT-162 test was added, per the approved no-reader resolution.
  Existing parser tests cover parsing only; there is no list-driven behavior
  change to test.
- No Ennorath in-game check is required for this documentation-only result;
  no runtime or vanilla gameplay behavior is claimed as ported.
- Stop S-7343 records the approved resolution. The PR closes #380 with these
  documentation changes only.
