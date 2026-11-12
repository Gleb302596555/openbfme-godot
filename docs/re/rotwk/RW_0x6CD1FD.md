# RW 0x6CD1FD — Weapon/ProjectileStreamName: the object a weapon makes when it fires (issue #99, lane PORT-27)

RotWK 2.01 (`RotWK/game.dat`, md5 `daed3668006cd90f01c34e5a7da1901f`, the S-001 provenance caveat
applies). Every instruction below was read in that image with `tools/mod/rw.py dis` in this
session. Port: `engine/src/GameLogic/Object/WeaponState.cpp` (`Weapon::registerProjectileStream`,
`Weapon::destroyProjectileStream`), `engine/src/GameLogic/Combat/ObjectWeapons.cpp`
(`ObjectWeapons::Host`), `engine/src/GameLogic/Combat/ProjectileLauncher.cpp`. No data of the mod
is reproduced here; the one template name in the tests is the mod's own field value.

## The field

| What | Address |
|---|---|
| field-table row | RW 0xC173F8 (`ProjectileStreamName`), in the WeaponTemplate table at RW 0xC16DD8 |
| parser | RW 0x42EE5E (ascii / GameAsciiString) |
| member | `WeaponTemplate + 0x178` |
| reader | RW 0x6CD209 (`lea ecx, [eax + 0x178]`, `eax` = the Weapon's template, `[weapon + 4]`) |

The port parsed this field from the start (`WeaponTemplate::m_projectileStreamName`,
`GameLogic/Object/Weapon.cpp:506`) and never read it: the audit row said `parsed-unused`. A
second field of the same name exists on `ProjectileNugget` (RW 0xC7B4E8, nugget `+ 0x158`); it is
a different row and does not feed this path (the path reads the WEAPON's `+ 0x5C`, which only
RW 0x6CD1FD writes).

## The state: Weapon + 0x5C

`Weapon` is the small inline object every weapon slot, crush weapon and temporary weapon is; its
layout comes from the constructor RW 0x6CCF5D and the copy constructor RW 0x6CD01C:

| offset | writer | meaning |
|---|---|---|
| + 0x04 | RW 0x6CCF60 | the WeaponTemplate |
| + 0x08 | RW 0x6CCF64 | the owner ObjectID |
| + 0x0C | RW 0x6CCF68 | the slot |
| + 0x30 | copy ctor RW 0x6CD0B4 | the last-shot frame (copied) |
| + 0x3C | assign RW 0x6CD0AA | from `template + 0x108` |
| **+ 0x5C** | **ctor RW 0x6CCF5D stores 0**; RW 0x6CD291 writes the stream id; RW 0x6CD233 / 0x6CD246 clear it | **the ProjectileStreamName object** |

`Weapon`'s vtable is RW 0xC18404; slot 0 is RW 0x6CDB57 (the deleting wrapper, which calls the
destructor RW 0x6CD0BE), slot 2 is RW 0x6CD016 (`getObject`, answering RW 0xBF71F0), slot 3 is
RW 0x6CEA67. That vtable is what ties the three functions used here to the class — they were not
taken from a "code region".

The copy constructor (RW 0x6CD01C, stores at RW 0x6CD03A) and `Weapon::operator=` (RW 0x6CA19F)
do **not** copy `+ 0x5C`: a copied weapon holds no stream. The port follows that (the copy
constructor leaves `m_projectileStreamID` at its 0 default; the assignment operator does not
touch it either).

## The creation — RW 0x6CD1FD, body RW 0x6CD206 .. RW 0x6CD297

Called from exactly one place, `privateFireWeapon` (RW 0x6CEF6D) at RW 0x6CF012, after its
status write-back (RW 0x6CDCE7) and before the nuggets are delivered. It runs once per shot.

| address | instruction | what the branch does |
|---|---|---|
| RW 0x6CD209 | `lea ecx, [eax + 0x178]` | the WEAPON's template's field (`eax` = `[esi + 4]` from RW 0x6CD206) |
| RW 0x6CD212 | `call RW 0x401E64` (`test al`), `jne RW 0x6CD295` | the string is empty -> the whole block is skipped, `+ 0x5C` untouched |
| RW 0x6CD222 / RW 0x6CD227 | `push [esi + 8]`, `call RW 0x449681` | GameLogic::findObjectByID(owner). No owner -> RW 0x6CD233 `and [esi + 0x5C], eax` (eax 0): the id is cleared |
| RW 0x6CD238 / RW 0x6CD23D | `findObjectByID([esi + 0x5C])`, `jne RW 0x6CD294` | the cached stream is still in the object list -> nothing to do, this shot reuses it |
| RW 0x6CD246 | `and [esi + 0x5C], eax` | the id was stale: cleared before a new object is made |
| RW 0x6CD249 .. RW 0x6CD252 | `mov ecx, [RW 0xDE4A40]`, `call RW 0x6D1305` | ThingFactory::findTemplate(name). A name that is not a template answers null ("DEBUG: Unable to find template" at RW 0x6E57A3) and the block ends at RW 0x6CD295 |
| RW 0x6CD257 .. RW 0x6CD261 | empty `std::string` (ctor RW 0x9ED6E5, dtor RW 0x401E78) | the fourth `newObject` argument |
| RW 0x6CD26C / RW 0x6CD271 | `call RW 0x68B678`, `mov eax, [eax + 0x30C]` | **INFERENCE**: the owner's controlling player's default team (`Player + 0x30C`, getter RW 0x79FD6F), the same team the rest of the port passes to `newObject` |
| RW 0x6CD277 | `mov [esp + 0x14], 0` | `ObjectStatusMaskType` 0 (three zero dwords, RW 0x6CD27C .. RW 0x6CD284) |
| RW 0x6CD285 / RW 0x6CD28A | `call RW 0x6D165E` | GameLogic::newObject(template, team, status 0, &zero) — the vector comes from RW 0x6CD25B .. RW 0x6CD268 whose three floats RW 0xA3CF28 sets to 0: **the stream object starts at the origin** |
| RW 0x6CD28C / RW 0x6CD291 | `test eax`, `mov eax, [eax + 0x74]`, `mov [esi + 0x5C], eax` | the new object's id (Thing + 0x74) is kept in the weapon |

So: one object per weapon, made on the first shot that reaches step 6, reused by every later shot
while the object is in the list, replaced after it is gone, never re-made while it lives.

## The destruction — RW 0x6CD0BE (destructor) 

| address | instruction | what it does |
|---|---|---|
| RW 0x6CD0BE | SEH prologue (`mov eax, ...; call RW 0xA3CEF0`) | the function's entry |
| RW 0x6CD0C8 | `push ecx; push esi; ...` | the destructor body begins here |
| RW 0x6CD0D0 | `mov [esi], RW 0xC18404` | the vtable store: this is a Weapon |
| RW 0x6CD0DC / RW 0x6CD0E8 | `push [esi + 0x5C]`, `call RW 0x449681` | findObjectByID(stream) |
| RW 0x6CD0ED / RW 0x6CD0F4 | `test eax`, `call RW 0x62BBAB` | GameLogic::destroyObject of it; `je RW 0x6CD0F9` when it is gone (id 0 included) |
| RW 0x6CD0F9 .. RW 0x6CD10A | `[esi + 0x40]` freed | the scatter list, unrelated |

The id is not cleared: the weapon is going. The port does the same from both places it frees a
Weapon instance — `ObjectWeapons::rebuildWeapons` (a weapon set change) and
`ObjectWeapons::~ObjectWeapons` (the object or its weapons go) — because both are where retail's
`Weapon` destructor runs. The port's stream object leaves through `GameLogic::destroyObject`, so
it dies in the same phase (phase 5, `processDestroyList`, RW 0x62EBB1) as every other destroyed
object, not immediately.

## What each shot does with the stream — RW 0x90FB7A .. RW 0x90FC5A

Inside `ProjectileNugget::deliver`'s object-target branch (the function RW 0x90FA48; the
position-target branch RW 0x90FC70 has no such block at all):

| address | instruction | what it does |
|---|---|---|
| RW 0x90FB80 | `push [esi + 0x5C]` (`esi` = the firing Weapon, RW 0x90F9F5) | the stream object |
| RW 0x90FB85 | `call RW 0x449681` | findObjectByID; `jz RW 0x90FC5F` on the pointer alone — a dying object that is still in the list is used |
| RW 0x90FB94 / RW 0x90FB99 | `push [esi + 8]`, `call RW 0x449681` | the owner; missing -> the record ends (RW 0x90FBA2) |
| RW 0x90FBC7 / RW 0x90FBDD | `call RW 0x68BDA5` with the key `[RW 0xDE9FCC]` (the string `ProjectileStreamUpdate` at RW 0xBE40E8) | the stream object's module; null -> the record ends (RW 0x90FBE6) |
| RW 0x90FBEE | `call RW 0x74094C` | the stream object's `setPosition` (a two-instruction wrapper: `mov ecx, [ecx + 8]; jmp RW 0x70C201`) with the owner's geometry centre (`lea eax, [esi + 0x38]` at RW 0x90FBE8) — **INFERENCE**: + 0x38 is the position the rest of the port calls the object's position |
| RW 0x90FC4A .. RW 0x90FC5A | `call RW 0x7408A8` | ProjectileStreamUpdate::addProjectile(owner id, victim or null, victim id or 0, projectile id, &owner geometry centre) |

`ProjectileStreamUpdate::addProjectile` (RW 0x7408A8) keeps the owner id at `+ 0x78` (set once),
clears the current slot when the victim id (`+ 0x7C`) or the position (`+ 0x80`) changes, and
puts the projectile id in a 20-slot ring (`+ 0x20 + idx * 4`, `idx = (idx + 1) % 0x14`). The port
already had that module; this lane connects the two calls to it.

## The port's mapping

| RW | port |
|---|---|
| RW 0x6CD1FD | `Weapon::registerProjectileStream` (`Object/WeaponState.cpp`), called as `privateFireWeapon`'s step 6 (RW 0x6CF012) |
| RW 0x6CD291 / + 0x5C | `Weapon::m_projectileStreamID`, `xfer`ed and hashed with the weapon's other state |
| RW 0x6CD23D / 0x6CD246 | `WeaponHost::projectileStreamExists` (ObjectWeapons::Host) |
| RW 0x6CD285 | `WeaponHost::createProjectileStream` (`logic().newObject(t, team, ObjectStatusMaskType{})`) |
| RW 0x6CD0BE | `Weapon::destroyProjectileStream` from `ObjectWeapons::~ObjectWeapons` |
| RW 0x90FB7A .. 0x90FC5A | `ProjectileLauncher::launch`, after the projectile exists: `ProjectileStreamUpdate::addProjectile` plus the `setPosition` of the stream object |

`WeaponTemplate + 0x178` had no other reader in the image, and the nugget's own field of the same
name is not read by this path.
