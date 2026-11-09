# RW 0x86AA4B — TransportContain's GrabWeapon fire and FireGrabWeaponOnVictim (issue #172, lane PORT-62)

RotWK 2.01 (`workspace/RotWK/game.dat`, md5 `daed3668006cd90f01c34e5a7da1901f`, the S-001
provenance caveat applies). Port: `engine/src/GameLogic/Object/Contain/TransportContainRuntime.cpp`
(`TransportContain::onContaining`, lane GARRISON-2) and the tests
`engine/tests/test_port_62.cpp`. No data of the mod is reproduced here; the object and weapon names
quoted in the issue and in the PR are the mod's own.

Every instruction quoted below was decoded from the image with `tools/mod/rw.py dis` (capstone 5,
image base `0x400000`); the readers of status `0x3C` were enumerated from the image's `.text` with a
linear capstone scan of every call site of `Object::testStatus` (RW 0x44DDEC).

## Where the fields and the slots are

| What | Address |
|---|---|
| TransportContain's main field table | RW 0xC5AC78 (the first row is `ExitDelay`) |
| field-table row `GrabWeapon` | RW 0xC5ACC8 -> data `+ 0x144` (a `WeaponTemplate *`, null when the INI names none or a name the store does not know: RW 0x73AE79 -> RW 0x6CC5DF, no error) |
| field-table row `FireGrabWeaponOnVictim` | RW 0xC5AD08 -> data `+ 0x148` (byte), parser RW 0x42E558 (`parseBool`), default true |
| field-table row `ShouldThrowOutPassengers` / its delay | data `+ 0x150` / `+ 0x154` (the block's neighbour, RW 0x86AAA3) |
| the function | `TransportContain::onContaining` RW 0x86A943, the contain interface's slot `+ 0x58` (interface RW 0xC5A690) |
| its caller | `OpenContain::addToContain` RW 0x8674C2 at RW 0x867530 |
| the fire | `WeaponStore::createAndFireTempWeapon` RW 0x6CF530, TheWeaponStore RW 0xDE4A1C |
| the relationship | `Object::getRelationship` RW 0x68D7AB (2 = ALLIES) |
| the status | `Object::setStatus` RW 0x62684D -> RW 0x6267C7 -> RW 0x68D440, the mask at Object `+ 0x94`; `Object::testStatus` RW 0x44DDEC |
| the status bit | `0x3C` = 60 = `UNATTACKABLE` (the port's `BitFlagNames.cpp`, RW's `TheObjectStatusNames[60]`) |
| the reader the bracket works through | `ActiveBody::attemptDamage`'s acceptance, RW 0x8C3FFB .. 0x8C400C (the port's `Module/ActiveBody.cpp`, "UNATTACKABLE only takes UNRESISTABLE") |

## The block, instruction by instruction

`ebx` is the `TransportContainModuleData`, `esi` the contain subobject (the owning `Object *` sits
at `esi - 0x18`), `edi` the rider, `edx` the contain's vtable. RW 0x86AA46 .. 0x86AA48 is the
previous step (`onRiderPlaced`, contain slot `0x6C`); RW 0x86AAA3 is the next one
(`ShouldThrowOutPassengers`).

```
86AA46  push   eax
86AA47  push   edi
86AA48  call   dword ptr [edx + 0x6c]        ; onRiderPlaced (contain slot 0x6C)
86AA4B  cmp    dword ptr [ebx + 0x144], 0    ; GrabWeapon: a WeaponTemplate pointer
86AA52  je     0x86aaa3                      ; none -> the whole block is skipped
86AA54  mov    ecx, dword ptr [esi - 0x18]   ; the container's Object
86AA57  push   edi                           ; the rider
86AA58  call   0x68d7ab                      ; Object::getRelationship(rider)
86AA5D  cmp    eax, 2                        ; ALLIES
86AA60  je     0x86aaa3                      ; an allied rider -> skipped, no fire
86AA62  cmp    byte ptr [ebx + 0x148], 0     ; FireGrabWeaponOnVictim, the first read
86AA69  jne    0x86aa76                      ; true -> straight to the fire
86AA6B  push   1
86AA6D  push   0x3c                          ; UNATTACKABLE
86AA6F  mov    ecx, edi                      ; the rider
86AA71  call   0x62684d                      ; Object::setStatus(0x3C, true)
86AA76  mov    eax, dword ptr [esi - 0x18]   ; the container's Object
86AA79  lea    ecx, dword ptr [eax + 0x38]   ; its position
86AA7C  push   ecx                           ; argument 3: where the shot is aimed
86AA7D  mov    ecx, dword ptr [0xde4a1c]     ; TheWeaponStore (thiscall)
86AA83  push   eax                           ; argument 2: the container as the source
86AA84  push   dword ptr [ebx + 0x144]       ; argument 1: the GrabWeapon template
86AA8A  call   0x6cf530                      ; createAndFireTempWeapon  <-- reached from BOTH branches
86AA8F  cmp    byte ptr [ebx + 0x148], 0     ; FireGrabWeaponOnVictim, the second read
86AA96  jne    0x86aaa3                      ; true -> done
86AA98  push   0
86AA9A  push   0x3c
86AA9C  mov    ecx, edi
86AA9E  call   0x62684d                      ; Object::setStatus(0x3C, false)
86AAA3  cmp    byte ptr [ebx + 0x150], 0     ; ShouldThrowOutPassengers (already ported)
```

So `FireGrabWeaponOnVictim` **does not gate the fire**: RW 0x86AA8A runs whether the byte is set or
clear. What the field decides is whether the rider carries `UNATTACKABLE` for the duration of that
one call — set at RW 0x86AA71, taken off again at RW 0x86AA9E before `onContaining` returns. The
issue's wording ("with `No` the transport grabs without firing") is not what the image does: the
transport fires either way, and with `No` the victim it grabbed is immune to that fire.

## What the bracket does: the body's damage acceptance reads the status

`UNATTACKABLE` as a *status* (Object `+ 0x94`, bit 60) has five readers in the whole image, found by
enumerating all 575 call sites of `Object::testStatus` (RW 0x44DDEC) and keeping those that push
`0x3C`. One of them is on the fire's own path — the victims' damage acceptance:

| reader | function | on the fire's path? |
|---|---|---|
| **RW 0x8C3FFF** | **`ActiveBody::attemptDamage`'s acceptance** (RW 0x8C3FDA `[esi + 0xB7]` indestructible, RW 0x8C3FE7 `push 0x14A; call 0x46E918` model condition INVULNERABLE, then RW 0x8C3FFB `push 0x3C; call 0x44DDEC`; RW 0x8C4008 `cmp dword [ebx + 0x10], 8` — only damage type 8, UNRESISTABLE, gets through) | **yes**: every victim of the nugget goes through it |
| RW 0x6C920B, RW 0x6C93E4 | `WeaponSet::getAbleToAttackSpecificObject` (head RW 0x6C9147; its only caller is `Object::getAbleToAttackSpecificObject` RW 0x68D6A6 at RW 0x68D79F, 15 callers, none of them the fire) | no |
| RW 0x75120E | an AI goal-object test | no |
| RW 0x76DF7C | `AIGiantBirdSwoopState::update` (head RW 0x76DF1D) | no |

The rest of the fire was walked too and reads no status: `privateFireWeapon` RW 0x6CEF6D,
`createAndFireTempWeapon` RW 0x6CF530, the area damage RW 0x90DEF0 (its range query RW 0xA39340 at
RW 0x90DFB9), the nugget's applicability RW 0x90E855 and the shockwave RW 0x9108EE call none of
RW 0x44DDEC / 0x68D6A6 / 0x6C9147, and a linear capstone scan of RW 0x6CE000 .. 0x6CF600 and
RW 0x90C000 .. 0x912000 finds no read of Object `+ 0x94` .. `+ 0xA3` (the six hits are a nugget's
`BitFlags` members at RW 0x90D6F5 / RW 0x90D924, a vtable slot `[eax + 0x98]` and a block copy).
What the area *query* excludes is the **KindOf** bit `UNATTACKABLE` (54) — RW 0x90DF78 pushes `0x36`
into the `PartitionFilter` (see `docs/re/rotwk/RW_0x90DEF0.md`); that is a template bit, not the
status, so the shielded rider is still a candidate and still walks into RW 0x8C3FFB, which then
refuses the damage.

`Object::setStatus` (RW 0x62684D -> RW 0x6267C7 -> RW 0x68D440) keeps the mask at Object `+ 0x94`
and has side effects for two bits only: bit 8 (`REPULSOR`, RW 0x68D485 .. 0x68D4AD -> RW 0x8E3B71)
and bit 2 (`UNDER_CONSTRUCTION`, RW 0x68C213). Bit 60 has none: the bracket changes nothing but the
bit.

**Net behaviour.** With `FireGrabWeaponOnVictim = No` the grab weapon still fires: everything in its
area takes the damage *except the rider it just grabbed*, which the status spares for that one call
(unless the nugget's damage type is UNRESISTABLE). With `Yes` (the default, and no 2.01 or mod
template that sets it) the grabbed victim takes its own grab's splash like any other enemy in the
area. The fell beast has no `ENCLOSED` in `ObjectStatusOfContained`, so the grabbed victim is still
in the world (RW 0x865D3D takes an object out of the world only for an enclosing container) and is a
candidate of the area query in both cases — the difference is exactly the acceptance at RW 0x8C3FFB.

## The order inside `addToContain`, and why the bracket cannot outlive the fire

`OpenContain::addToContain` RW 0x8674C2 runs, in this order:

```
867530  call   dword ptr [eax + 0x58]        ; onContaining (the grab fire, above)
86753B  call   dword ptr [eax + 0x48]        ; redeployOccupants
867543  call   0x6901ae                      ; Object::objectOnContainedBy: the ObjectStatusOfContained mask
86754C  call   dword ptr [eax + 0x54]        ; recalculate the contain's state
```

`objectOnContainedBy` (RW 0x6901AE) applies the container's `ObjectStatusOfContained` mask to the
rider *after* the fire, so the bracket is never what leaves a rider `UNATTACKABLE` once the entry is
over. For a container whose mask already holds `UNATTACKABLE` — the mod's fell beast is one — the bit
is set again a moment later and stays set for as long as the victim is held; for a container without
it the rider is not `UNATTACKABLE` once `addToContain` returns. Either way the bracket's own clear at
RW 0x86AA9E has run before that, and the fire it bracketed is over.

## Donor

DONOR BFME2 0x0046781F (`TransportContainOnContaining.cpp`, the same block at the same data offsets
`+ 0x144` / `+ 0x148` / `+ 0x150`): the shape is identical, which is how the block was found; every
address above was then confirmed in the RotWK image itself.

## What the port does now

`TransportContain::onContaining` (`TransportContainRuntime.cpp`, RW 0x86A943) resolves
`m_grabWeapon.name` through `TheWeaponStore` (null when the name is absent or unknown, as RW's
`+ 0x144` pointer is), skips the block for an `ALLIES` rider, brackets the fire with
`setStatus(UNATTACKABLE, true/false)` when `m_fireGrabWeaponOnVictim` is false, and fires
`ObjectWeapons::createAndFireTempWeapon(grab, container, *container->getPosition())` — the port's
RW 0x6CF530 — from the container at the container's own position. The bracket needs no new state:
the port's `ActiveBody::attemptDamage` already reads the status (`Module/ActiveBody.cpp`, RW
0x8C3FFB), so the shielded victim is refused there exactly as in retail. Before lane PORT-62 the
port counted the whole block as unported (`++m_stats.unported`, stop S-1103 / S-1104) and never
fired; the stop text keeps `CanGrabStructure` (the trolls' `GrabObject` body path), whose entry test
RW 0x86A07D was already ported.

## Open

* The client half of the grab is not ported and is not part of this lane: the mod's grab weapon
  carries a `MetaImpactNugget` (shockwave, taper) and a weapon field the port has no reader for
  (`CanSwoop`); the shockwave is a client effect in the port.
* `ShouldThrowOutPassengers`' throw (RW 0x86A530, the physics shock RW 0x792A69) is still unported
  and still counted by `Stats::unported` (`TransportContainRuntime.cpp`, RW 0x86B885).
* **INFERENCE**: the KindOf base `+ 0x108` used to read `BIG_MONSTER` / `TRANSPORT` in
  `docs/re/rotwk/RW_0x90DEF0.md` is not re-derived here.
* The identification of RW 0x8C3FFF's function as the body's `attemptDamage` is the port's own
  reading of it (lane INVUL-1 / ATTEMPTDMG-2 cite RW 0x8C3FDA and RW 0x8C3FE7 for the two tests
  immediately before); the instruction sequence above was re-read from the image for this note.
