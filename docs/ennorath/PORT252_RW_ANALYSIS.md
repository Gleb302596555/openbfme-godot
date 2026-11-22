# PORT-252 (Issue #600): `SpecialAbilityUpdate` & Subclass Parity Analysis (RotWK 2.01)

Scope: the 5 special-ability update classes covered by issue #600 (`SpecialAbilityUpdate`, `HeroModeSpecialAbilityUpdate`, `ModelConditionSpecialAbilityUpdate`, `TeleportSpecialAbilityUpdate`, `FlingPassengerSpecialAbilityUpdate`) and their 29 uncited depth-0 RotWK 2.01 vtable/interface functions, plus depth-1 method audit, Ennorath 1.9.8.3 field audit, and failing matrix-button triage.

## 1. Classification of All 29 Depth-0 Uncited Functions (RotWK 2.01 `game.dat`)

| Class | Vtable | Slot / Role | RW 2.01 Address | Verdict | Disposition |
|---|---|---|---|---|---|
| `SpecialAbilityUpdate` | `0x00C61844` | `getModuleNameString` | `0x00851E7D` | Ported elsewhere | Cited on `SpecialAbilityUpdate` (`SpecialAbilityModules.h`) |
| `SpecialAbilityUpdate` | `0x00C61844` | `getModuleNameKey` | `0x00851E38` | Ported elsewhere | Cited on `SpecialAbilityUpdate` (`SpecialAbilityModules.h`) |
| `SpecialAbilityUpdate` | `0x00C61844` | `xfer` | `0x00852782` | Ported elsewhere | Cited on `SpecialAbilityUpdate::xfer` (`SpecialAbilityModules.cpp`) |
| `SpecialAbilityUpdate` | `0x00C61844` | deleting dtor -> `~SpecialAbilityUpdate` (`0x00854D9A`) | `0x0085536C` | **Port now** | `SpecialAbilityUpdate::~SpecialAbilityUpdate()` calls `SpecialAbilityUpdate::finishAbility(true, true)` (`RW 0x008530BA`) |
| `SpecialAbilityUpdate` | `+0x20` (`0x00C618A4`) | `drivesSpecialPower` (slot 7) | `0x00851E83` | Ported elsewhere | Cited on `SpecialAbilityUpdate::drivesSpecialPower` (`SpecialAbilityModules.h`) |
| `SpecialAbilityUpdate` | `+0x20` (`0x00C618A4`) | `isActive` (slot 2) | `0x009D4099` | Ported elsewhere | Cited on `SpecialAbilityUpdate::isActive` (`SpecialAbilityModules.h`) |
| `HeroModeSpecialAbilityUpdate` | `0x00C64D14` | `getModuleNameString` | `0x00895836` | Ported elsewhere | Cited on `HeroModeSpecialAbilityUpdate` (`SpecialAbilityModules.h`) |
| `HeroModeSpecialAbilityUpdate` | `0x00C64D14` | `getModuleNameKey` | `0x0089585C` | Ported elsewhere | Cited on `HeroModeSpecialAbilityUpdate` (`SpecialAbilityModules.h`) |
| `HeroModeSpecialAbilityUpdate` | `0x00C64D14` | s1 no-op thunk (`0x8B88B0 -> 0x4D0EAF -> 0x63F3BF ret`) | `0x008E38D8` | Ported elsewhere | Cited on class (`SpecialAbilityModules.h`) |
| `HeroModeSpecialAbilityUpdate` | `0x00C64D14` | `xfer` (v1 -> base `0x00852782`) | `0x008C6E67` | Ported elsewhere | Cited on `HeroModeSpecialAbilityUpdate::xfer` (`SpecialAbilityModules.cpp`) |
| `HeroModeSpecialAbilityUpdate` | `0x00C64D14` | deleting dtor (`0x89583C -> ~SAU 0x854D9A`) | `0x008958A1` | **Port now** | Inherited via `~SpecialAbilityUpdate()` |
| `ModelConditionSpecialAbilityUpdate` | `0x00C64C54` | `getModuleNameString` | `0x008943FC` | Ported elsewhere | Cited on `ModelConditionSpecialAbilityUpdate` (`SpecialAbilityModules.h`) |
| `ModelConditionSpecialAbilityUpdate` | `0x00C64C54` | `getModuleNameKey` | `0x00894422` | Ported elsewhere | Cited on `ModelConditionSpecialAbilityUpdate` (`SpecialAbilityModules.h`) |
| `ModelConditionSpecialAbilityUpdate` | `0x00C64C54` | s1 no-op thunk | `0x008E38D8` | Ported elsewhere | Cited on class (`SpecialAbilityModules.h`) |
| `ModelConditionSpecialAbilityUpdate` | `0x00C64C54` | `xfer` (v1 -> base `0x00852782`) | `0x008C6E67` | Ported elsewhere | Cited on `ModelConditionSpecialAbilityUpdate::xfer` (`SpecialAbilityModules.cpp`) |
| `ModelConditionSpecialAbilityUpdate` | `0x00C64C54` | deleting dtor (`0x894402 -> ~SAU 0x854D9A`) | `0x0089453F` | **Port now** | Inherited via `~SpecialAbilityUpdate()` |
| `TeleportSpecialAbilityUpdate` | `0x00C64E8C` | `getModuleNameString` | `0x0089640F` | Ported elsewhere | Cited on `TeleportSpecialAbilityUpdate` (`HeroAbilityModules.h`) |
| `TeleportSpecialAbilityUpdate` | `0x00C64E8C` | `getModuleNameKey` | `0x0089643F` | Ported elsewhere | Cited on `TeleportSpecialAbilityUpdate` (`HeroAbilityModules.h`) |
| `TeleportSpecialAbilityUpdate` | `0x00C64E8C` | s1 no-op thunk | `0x008E38D8` | Ported elsewhere | Cited on class (`HeroAbilityModules.h`) |
| `TeleportSpecialAbilityUpdate` | `0x00C64E8C` | `xfer` (v1 -> base `0x00852782`) | `0x0089659F` | Ported elsewhere | Cited on `TeleportSpecialAbilityUpdate::xfer` (`HeroAbilityModules.cpp`) |
| `TeleportSpecialAbilityUpdate` | `0x00C64E8C` | deleting dtor (`0x896415 -> ~SAU 0x854D9A`) | `0x00896484` | **Port now** | Inherited via `~SpecialAbilityUpdate()` |
| `TeleportSpecialAbilityUpdate` | `0x00C64E8C` | `finishAbility` tail-call (`jmp 0x8530BA`) | `0x00896435` | Ported elsewhere | Cited (`HeroAbilityModules.cpp`) |
| `TeleportSpecialAbilityUpdate` | `0x00C64E8C` | `update` tail-call (`jmp 0x854DF7`) | `0x0089643A` | Ported elsewhere | Cited (`HeroAbilityModules.cpp`) |
| `TeleportSpecialAbilityUpdate` | `+0x20` (`0x00C64EEC`) | `allowsSpecialPowerUse` (`MaxDistance` `+0xDC`) | `0x00896608` | **Overlap (PR #584)** | Left untouched — ported in PR #584 (Port 247, issue #571) |
| `FlingPassengerSpecialAbilityUpdate` | `0x00C652D4` | `getModuleNameString` | `0x008986A3` | Ported elsewhere | Cited on `FlingPassengerSpecialAbilityUpdate` (`FlingPassengerSpecialAbilityUpdate.h`) |
| `FlingPassengerSpecialAbilityUpdate` | `0x00C652D4` | `getModuleNameKey` | `0x008986C9` | Ported elsewhere | Cited on `FlingPassengerSpecialAbilityUpdate` (`FlingPassengerSpecialAbilityUpdate.h`) |
| `FlingPassengerSpecialAbilityUpdate` | `0x00C652D4` | s1 no-op thunk | `0x008E38D8` | Ported elsewhere | Cited on class (`FlingPassengerSpecialAbilityUpdate.h`) |
| `FlingPassengerSpecialAbilityUpdate` | `0x00C652D4` | `xfer` (v1 -> base `0x00852782`) | `0x008C6E67` | Ported elsewhere | Cited on `FlingPassengerSpecialAbilityUpdate::xfer` (`FlingPassengerSpecialAbilityUpdate.cpp`) |
| `FlingPassengerSpecialAbilityUpdate` | `0x00C652D4` | deleting dtor (`0x8986A9 -> ~SAU 0x854D9A`) | `0x0089873E` | **Port now** | Inherited via `~SpecialAbilityUpdate()` |

## 2. Ten Verified RotWK 2.01 Behavior Fixes Ported in PORT-252

All 10 items add zero new simulation state (no `crc` or `xfer` layout change) and zero overlap with open PRs (#584, #587, #595) or issue #574:

1. **`~SpecialAbilityUpdate()` (`RW 0x00854D9A`) + `Object::~Object()` client detach**:
   - All 5 deleting destructors (`0x0085536C`, `0x008958A1`, `0x0089453F`, `0x00896484`, `0x0089873E`) chain to `~SpecialAbilityUpdate()` (`0x00854D9A`), which calls `SpecialAbilityUpdate::finishAbility(true, true)` (`0x008530BA`) non-virtually so dying/destroyed casters release active captures, stop active sound loops, clear `m_updateDisabled`, and clean up grab state.
   - `Object::~Object()` now clears `m_client = nullptr` right after `hooks->objectDestroyed(*this)` (matching retail `Object::~Object` detaching the drawable before deleting modules) so module destructors cannot emit `ClientEvent::MODEL_CONDITION` after `ClientEvent::DESTROYED`, clears cached module interface pointers (`m_ai`, `m_body`, `m_contain`, `m_attributePool`, `m_physics`, `m_slaved`, `m_moduleCachesBuilt`, `m_collideModules`, matching `RW 0x0069A8D7..0x0069A8E9`) before deleting modules, and guards `m_modules` scans (`Object.cpp`, `SpecialPowerModules.cpp`) against already-reset null entries during teardown.
2. **`finishAbility` paralysis (`RW 0x00853369..0x00853393`)**:
   - Replaces `++m_unported` for `ParalyzeDurationWhenCompleted` (`+0xB4`) / `ParalyzeDurationWhenAborted` (`+0xB8`) with `ai->aiBusyFor(paralyze, CMD_FROM_AI)` (`RW 0x00852E2A`, `AICMD_BUSY` `0x31`).
3. **`finishAbility` power module notice (`RW 0x0085313E..0x0085314F`)**:
   - Calls `SpecialPowerModuleInterface::clearUpdateDisabled()` (vslot `+0x4C` on `0x00C64FF0`, `RW 0x00896751` clearing `SpecialPowerModule::m_updateDisabled` `+0x28`).
4. **`handlePackingProcessing` `0x1D` early finish (`RW 0x00851F39..0x00851F65`, `0x00852027`)**:
   - For `SPECIAL_INFANTRY_CAPTURE_BUILDING` (`0x1D`), if the target is missing or already claimed by another object (`t->getCapturerID() != INVALID_ID && t->getCapturerID() != obj->getID()`), calls `finishAfterPacking()` (slot `+0x4C`) and returns `true`.
5. **`update` command-source abort guard (`RW 0x00854F05..0x00854F1E`)**:
   - Skips non-AI command abort when `obj->testStatus(0x66)` (`OBJECT_STATUS_SUMMONING_REPLACEMENT`, `Object + 0xA0 bit 6`) is set.
6. **`startUnpacking` `SPECIAL_GRAB_CHUNK` (`0x28`) (`RW 0x0085384C..0x00853882`)**:
   - Replaces `++m_unported`: when `target` is `KindOf = CHUNK_VENDOR` (`132`, template `+0x118 bit 4`), sets model condition `176` (`Object + 0x120 |= 0x10000`, cleared by `grabFinish` `RW 0x008532B7`).
7. **`startPreparation` contain rider weapon state (`RW 0x00853C1A..0x00853C42`)**:
   - Calls `ContainModuleInterface::setWeaponStateOfRider(*t)` (contain vslot `+0xEC`: verified across all 17 contain vtables of RotWK 2.01 to be either OpenContain no-op `0x009F3A3C` or `TransportContain::setWeaponStateOfRider` `0x0086A43E`).
   - Also verified RotWK 2.01 instructions `0x00853E1B..0x00853E34` (`EVA_BuildingBeingStolen`, index `0x10`) and `0x008545FA..0x00854618` (`EVA_BuildingStolen`, index `0x11`) through `0x005DD9EE`, narrowing `S-7360`.
8. **`restartPersistent` `SPECIAL_GRAB_PASSENGER` (`0x27`) gate (`RW 0x008524EA..0x00852512`)**:
   - Sets `m_prepFrames = 0` unless `target` exists and has `KindOf = SHRUBBERY` or `GRAB_AND_KILL`; prevents spurious persistent loops on regular grabbed infantry.
9. **`ModelConditionSpecialAbilityUpdate` emotion pulse `PartitionFilterSameMapStatus` (`RW 0x00894701 -> 0x00C0F374 -> 0x00660E93`)**:
   - Rejects targets with `o.isOffMap() != obj.isOffMap()`.
10. **`SpecialAbilityUpdate::onEffectExpired` (`RW 0x00851E9A`)**:
    - Clears `m_effectExpiry = 0` (`+0x2C`) in the base implementation.

## 3. Ennorath 1.9.8.3 INI Census, Field Audit, and Matrix Audit

- **Field audit (`/tmp/ennorath-audit/field-audit.tsv`)**: 80 `used`, 3 `parsed-unused` (`TeleportSpecialAbilityUpdate/MaxDistance` and `FlingPassengerSpecialAbilityUpdate/FlingPassengerVelocity` handled in PR #584; `ModelConditionSpecialAbilityUpdate/PreTriggerUnstealthTime` handled in PR #595). Zero Ennorath occurrences of `SpecialObject*`, `ContactPointOverride`, `FleeRangeAfterCompletion`, or `NeedCollisionBeforeTrigger`.
- **Ennorath INI template census (`/tmp/ennorath-ini`)**:
  - `SPECIAL_GRAB_PASSENGER` (`0x27`): 11 templates (e.g., `GoblinCaveTroll`, `MordorCaveTroll`, `MordorMountainTroll`, `AngmarRogash_New`, `ArnorCaptain`, `MordorMorgothsDmon`, `Ogre`, `WatcherHead`).
  - `SPECIAL_INFANTRY_CAPTURE_BUILDING` (`0x1D`): 10 templates (`ArthedainCapitan`, `DwarvenNar_AxeThrower`, `GoblinSpearHorde`, 4 Isengard combo hordes, `LothlorienGilGalad`, `MordorNazgul_forRingseeker`, `Ruber1`).
  - `HeroModeSpecialAbilityUpdate`: 135 templates; `ModelConditionSpecialAbilityUpdate` emotion pulse: 15 templates (`GondorAragorn_mod`, `GondorAragorn_king`, `GondorEarnur`, `GondorGandalf_WhiteCouncil`, `GondorIsildur_OneRing`, `RohanCitadel`, etc.); `TeleportSpecialAbilityUpdate`: 34 templates; `FlingPassengerSpecialAbilityUpdate`: 14 templates.
- **25 failing buttons in `docs/ennorath/MATRIX.tsv`**: 15 `NO-RULE` (covered by PR #587's `SweepEffectProbe.h`), 5 `NOT-ON-BAR`/`NO-SELECT`, 1 `PASS`, 2 mod INI defects (`GondorBoromir_mod_ElrondsRat` missing `AttributeModifierUpgrade`, `WildMarauderSwordBanner` missing `SpecialPowerModule`), 2 `AttributeModifierAffectsSelf=No` (target/ally-only powers with no self effect). None indicate a broken `SpecialAbilityUpdate` code path.
