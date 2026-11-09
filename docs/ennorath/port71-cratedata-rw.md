# Port 71 / issue #185 — CrateData and CreateCrateDie, RW reading and data reachability stop

Date: 2026-11-09. Source: RotWK 2.01 `game.dat` (MD5 `daed3668006cd90f01c34e5a7da1901f`, caveat S-001), read with `tools/mod/rw.py dis|xref|str` and full-image pointer/branch scans, cross-checked against the BFME2 1.06 donor (`Open-BFME-2` `GameLogic/System/CrateSystem*.cpp`, `CrateTemplate*.cpp`, `GameLogic/Object/Die/CreateCrateDie*.cpp`). No game or mod bytes are committed.

## 1. Block keyword, subsystem store and `CrateTemplate` field table

Global INI block table entry at `RW 0xD9DD68`:

| VA | value | meaning |
|---|---|---|
| `0xD9DD68` | `0xBF1D38` | `"CrateData"` keyword |
| `0xD9DD6C` | `0x75EC76` | `CrateSystem::parseCrateTemplateDefinition` (DONOR BFME2 RVA `0x0035CFEA`, `CrateSystem.cpp`) |

Subsystem global `TheCrateSystem` (`CrateSystem *`) at `RW 0xDE78A0`:

- Initialised in `GameLogic` subsystem startup (`RW 0x63C085..0x63C0C6`): allocates `0x18` bytes (`0x63C085`), runs `CrateSystem::CrateSystem` (`RW 0x75E762`, vtable `RW 0xC2C9E4`; DONOR BFME2 RVA `0x0035CAD6`), and registers `"TheCrateSystem"` (`0xBFECB8`) into `0xDE78A0` via `initSubsystem` (`0x63C0B3..0x63C0C6 -> 0x637F7B`).
- `CrateSystem` holds `m_crateTemplateVector` (`std::vector<CrateTemplate *>`) at `+0x0C..+0x17`. Slot 0 of `0xC2C9E4` is deleting destructor `RW 0x75E86E` (`~CrateSystem` `RW 0x75E793`); slot 9 (`+0x24`) is `CrateSystem::reset` (`RW 0x75E600`, DONOR BFME2 RVA `0x0035C9CF`), which calls `Overridable::deleteOverrides` (`0x5E2E2E`) on each entry and erases `NULL` slots (`0x5FF944`).

`CrateSystem::parseCrateTemplateDefinition` (`RW 0x75EC76..0x75ED22`):

1. `0x75EC8E..0x75EC9B`: reads block name token (`INI::getNextToken` `0x42DC9F`).
2. `0x75ECAF..0x75ECB5`: looks up `TheCrateSystem->friend_findCrateTemplate(name)` (`RW 0x75E660`, DONOR BFME2 RVA `0x0035CA2F`), which scans `+0x0C`, compares `CrateTemplate + 0x10` (`m_name`) via `0x4065AA`, and walks the `Overridable + 0x04` chain via `0x465AF4`.
3. `0x75ECBC..0x75ECE2` (not found): calls `TheCrateSystem->newCrateTemplate(name)` (`RW 0x75EBB8`, DONOR BFME2 RVA `0x0035CF2C`) — allocates `0x44` bytes, runs `CrateTemplate::CrateTemplate` (`RW 0x75E8C2`), looks up `"DefaultCrate"` (`0xC2CA50`) via `0x75E660`, copies it via `CrateTemplate::operator=` (`RW 0x75EADE`, guarded by byte `0xDE78F0`) when present, sets `m_name` (`RW 0x75E5CC`), and appends to `+0x0C` (`0x90BE00`). If `[ini + 8] == 2` (`INI_LOAD_CREATE_OVERRIDES`, `0x75ECD8`), sets `isOverride` (`[eax + 8] = 1`, `0x75ECDE`).
4. `0x75ECE4..0x75ECFC` (found): loads `[ini + 8]`; if load type is `2` (`INI_LOAD_CREATE_OVERRIDES`) or `4`, chains an override via `TheCrateSystem->newCrateTemplateOverride` (`RW 0x75EB32`, DONOR BFME2 RVA `0x0035CEA6`).
5. `0x75ECFD..0x75ED05`: calls `INI::initFromINI` (`0x42DB80`) with `CrateTemplate`'s field table `RW 0xC2C928` (`CrateTemplate::getFieldParse` `RW 0x75E5C6`).

`CrateTemplate` (`0x44` bytes, ctor `RW 0x75E8C2`, dtor `RW 0x75E956`, vtable `RW 0xC2CA1C`; DONOR BFME2 RVA `0x0035CC36`, `CrateTemplateCtor.cpp`) and field table `RW 0xC2C928`:

| row VA | field name | parser | userData | offset | ctor default (`RW 0x75E8C2`) |
|---|---|---|---|---|---|
| `0xC2C928` | `CreationChance` (`0xC2C9D4`) | `0x42ED00` (`INI::parseReal`) | `0` | `+0x14` | `0.0f` (`0x75E922`) |
| `0xC2C938` | `VeterancyLevel` (`0xBF74A0`) | `0x42E956` (`INI::parseIndexList`) | `0xD9F5E4` (`TheVeterancyNames`) | `+0x18` | `5` (`LEVEL_INVALID`, `0x75E936`) |
| `0xC2C948` | `KilledByType` (`0xC2C9C4`) | `0x6564E7` (`KindOfMaskType::parseFromINI`) | `0` | `+0x1C` | `0x1C` bytes zeroed (`0x75E91B..0x75E927`) |
| `0xC2C958` | `CrateObject` (`0xC2C9B8`) | `0x75E9AF` (`CrateTemplate::parseCrateCreationEntry`) | `0` | `+0x00` (`+0x3C` list) | empty `std::list` (`0x75E93D`) |
| `0xC2C968` | `KillerScience` (`0xC2C9A8`) | `0x73AFE3` (`INI::parseScience`) | `0` | `+0x38` | `-1` (`SCIENCE_INVALID`, `0x75E92C`) |
| `0xC2C978` | `OwnedByMaker` (`0xC2C998`) | `0x42E558` (`INI::parseBool`) | `0` | `+0x40` | `0` (`false`, `0x75E945`) |
| `0xC2C988` | `NULL` | `NULL` | `0` | `0` | terminator |

`CrateTemplate::parseCrateCreationEntry` (`RW 0x75E9AF..0x75EA6A`, DONOR BFME2 RVA `0x0035CD23`) reads two tokens (`crateName`, `crateValue` scanned via `sscanf(..., "%f", ...)` at `0x75E9E4..0x75E9EA`, throwing INI exception `0xC2CA20` on failure) and appends `(AsciiString crateName, float crateChance)` to the list at `this + 0x3C` (`0x75EA39..0x75EA41`).

## 2. Exhaustive reader scan for `TheCrateSystem` and `CrateTemplate`

All sections of `game.dat` were scanned for 4-byte references to `TheCrateSystem` (`0xDE78A0`) and relative `E8/E9/0F8x` branches to `CrateSystem` methods:

- `0xDE78A0` is referenced at five instructions only:
  1. `0x63C0C2`: `initSubsystem` (`"TheCrateSystem"`).
  2. `0x75ECB1`, `0x75ECCF`, `0x75ECF3`: `CrateSystem::parseCrateTemplateDefinition`.
  3. `0x888A91` (`0x888A8F mov ecx, dword ptr [0xDE78A0]; 0x888A95 call 0x75E660`): `CreateCrateDie::onDie` (`RW 0x888A24`).
- `CrateSystem::friend_findCrateTemplate` (`RW 0x75E660`) has three callers only: `0x75EC17` (`newCrateTemplate`), `0x75ECB5` (`parseCrateTemplateDefinition`), and `0x888A95` (`CreateCrateDie::onDie`).

`CreateCrateDie` is therefore the **only** consumer of `TheCrateSystem` / `CrateData` in RotWK 2.01.

## 3. `CreateCrateDie` class proof and `onDie` branches

Module registration (`RW 0x6588D7..0x6588FF`, `engine/data/rotwk-201/module-registry.json`):

- Name `"CreateCrateDie"` (`0xC0B950`), type `0` (`MODULETYPE_BEHAVIOR`), interface mask `2` (`MODULEINTERFACE_DIE`).
- `friend_newModuleData`: `RW 0x6570AD` (`0x3C` bytes) -> `CreateCrateDieModuleData` ctor `RW 0x656FFE` (vtable `RW 0xC0A2B8`, dtor `RW 0x657065`, `DieMuxData` at `+0x08` via `0x653257`, `std::list<AsciiString> m_crateNameList` at `+0x38`; DONOR BFME2 RVA `0x002572EE`). `buildFieldParse` `RW 0x6536F4` adds `DieMuxData::getFieldParse` (`0x8D296A`, `+8`) and table `RW 0xC069D8` (one row: `"CrateData"` `0xBF1D38` -> `CreateCrateDieModuleData::parseCrateData` `RW 0x888B65`, DONOR BFME2 RVA `0x004858F4`, pushing tokens onto `data + 0x38`).
- `friend_newModuleInstance`: `RW 0x64C53A` (`0x14` bytes) -> `CreateCrateDie` ctor `RW 0x888761` (dtor `RW 0x888696`): installs main vtable `RW 0xC60F3C` (`getModuleNameKey` `RW 0x88871C`), `BehaviorModuleInterface` `RW 0xC61430` (`+0x0C`), and `DieModuleInterface` `RW 0xC60F38` (`+0x10`, slot 0 `onDie` = `RW 0x888A24`).
- Full `.text`/`.rdata` scan confirms **no derived class** inherits `CreateCrateDie` or `CreateCrateDieModuleData` (`0x888761` called only from `0x64C561`; `0x656FFE` called only from `0x6570CE`; `0x6536F4` referenced only from `0x6570E5`; `0x888A24` referenced only in `0xC60F38`).

`CreateCrateDie::onDie` (`RW 0x888A24..0x888B62`; DONOR BFME2 `CreateCrateDie.cpp`, `CreateCrateDieTestKillerType.cpp`):

1. `0x888A38`: `DieModule::isDieApplicable(damageInfo)` (`0x85FED5`); returns on mismatch.
2. `0x888A45..0x888A6A`: `killer = TheGameLogic->findObjectByID(damageInfo->in.m_sourceID)` (`[0xDE412C]` -> `0x449681`); if `killer != NULL` and `killer->getRelationship(me) == 2` (`ALLIES`, `0x68D7AB`), returns (`0x888B5E`).
3. `0x888A70..0x888B58`: iterates `m_crateNameList` (`data + 0x38`) and resolves `currentCrateData = TheCrateSystem->friend_findCrateTemplate(*iter)` (`0x888A8F..0x888A95`):
   - `0x888AAB`: `testCreationChance` (`RW 0x8886B5`): draws `GetGameLogicRandomValueReal(0.0f, 1.0f, "...CreateCrateDie.cpp", 0x70)` (`0x6D332C`), requires `m_creationChance (+0x14) > draw` (`0x8886E0..0x8886E4` `fcompi` / `jbe`).
   - **`VeterancyLevel` (`CrateTemplate + 0x18`) is unread**: neither `CreateCrateDie::onDie` nor any helper in `game.dat` (nor BFME2 `CreateCrateDie.cpp`) reads `CrateTemplate + 0x18`; only ctor `0x75E936` and `operator=` `0x75EB00` touch `+0x18` (victim veterancy gating on death is `DieMuxData`'s own `VeterancyLevels` inside `0x85FED5`).
   - `0x888AB8..0x888AD3`: if `m_killedByTypeKindof` (`+0x1C`) `any()` (`0x6C824C`), calls `testKillerType` (`RW 0x8887A7`, DONOR BFME2 RVA `0x00485536`): requires `killer != NULL` and `killer->getTemplate()->isKindOfMulti(m_killedByTypeKindof, KINDOFMASK_NONE)` (`template + 0x108`, `0xDE49E4`, `0x70B8C7`).
   - `0x888AD5..0x888AEA`: if `m_killerScience` (`+0x38`) `!= -1`, calls `testKillerScience` (`RW 0x8886F1`): requires `killer != NULL`, `killer->getControllingPlayer()` (`0x68B678`), and `player->hasScience(m_killerScience)` (`0x6AC207`).
   - `0x888AF3`: `createCrate` (`RW 0x8887D6..0x888A21`): draws `r = GetGameLogicRandomValueReal(0.0f, 1.0f, line 0xA7)` (`0x88880B`), accumulates `crateChance` (`node + 0x0C`) across `m_possibleCrates` (`+0x3C`) until `sum > r` (`comiss` / `ja` at `0x888844..0x88884D`), resolves `ThingFactory::findTemplate` (`[0xDE4A40]` -> `0x6D1305`), finds ground placement via `PartitionManager::findPositionAround` (`0x68592D`: `maxRadius = 5.0f` `0xBDAE58` with `FPF_IGNORE_ALLY_OR_NEUTRAL_UNITS = 8` on `LAYER_GROUND == 1`, fallback `maxRadius = 125.0f` `0xC60FD0` with `0`), spawns via `ThingFactory::newObject` (`0x6D165E`), sets position (`0x70C201`), random orientation `[0, 2*pi]` (line `0xE2`, `0x888986` -> `0x70C31E`), layer (`0x68BB9D`), and crate terrain decal on drawable (`0x8889AF..0x8889FC`).
   - `0x888AFF..0x888B16`: if `m_isOwnedByMaker` (`+0x40`) `!= 0`, calls `crate->setTeam(me->getControllingPlayer()->getDefaultTeam())` (`player + 0x30C` -> `0x69954A`).
   - `0x888B1B..0x888B47`: if `killer != NULL`, `killer->getControllingPlayer()->getPlayerType() == 1` (`PLAYER_COMPUTER`, `player + 0x5C`), and `killer->getAIUpdateInterface()` (`killer + 0x260`) `!= NULL`, stores `ai->m_crateCreated = crate->getID()` (`ai + 0x238 = crate + 0x74`).

## 4. Data reachability in RotWK 2.01 and Ennorath 1.9.8.2

Per issue #185 ("Check first which Ennorath objects reference a CrateData (`CreateCrateDie` or similar): if none in RotWK / Ennorath data reaches it, stop with that verdict"):

1. **Ennorath 1.9.8.2 merged INI dataset** (vanilla RotWK 2.01 overridden by Ennorath 1.9.8.2):
   - **Zero** objects or child objects use `CreateCrateDie` (the string occurs only in a commented-out example in `data/ini/crate.ini`).
   - **Zero** fields anywhere in `data/ini/**` or `bases/**` reference any of the nine `CrateData` definitions (`DefaultCrate` in `data/ini/default/crate.ini`, and `SalvageCrateData`, `EliteTankCrateData`, `HeroicTankCrateData`, `GLA02_Always100DollarCrate`, `GLA02_Always200DollarCrate`, `GLA02_Always1000DollarCrate`, `GLA02_Always1500DollarCrate`, `GLA02_Always2500DollarCrate` in `data/ini/crate.ini`) outside their own `CrateData <Name>` headers.
   - Seven of the eight `CrateData` blocks in `data/ini/crate.ini` (`EliteTankCrateData`, `HeroicTankCrateData`, `GLA02_Always*DollarCrate`) are unedited *Generals* leftovers whose `CrateObject` templates (`1000DollarCrate`, `SmallLevelUpCrate`, `2500DollarCrate`, `MediumLevelUpCrate`, `2FreeCrusadersCrate`, `100DollarCrate`, `200DollarCrate`, `1500DollarCrate`) are commented out in `data/ini/crate.ini` and do not exist as `Object` templates; `SalvageCrateData` is likewise an unreferenced *Generals* leftover.
   - `field-audit-v6/module-coverage.tsv` confirms `CreateCrateDie` has `0` definitions in Ennorath (absent from `module-queue.tsv`).
2. **Vanilla RotWK 2.01 and BFME1 censuses**:
   - `engine/tests/data/object_model/rotwk201_module_histogram.tsv` and `rotwk201_template_modules.tsv` (4,657 retail templates): **0** `CreateCrateDie` modules.
   - `engine/tests/data/object_model/census_module_histogram.tsv` (BFME1): **0** `CreateCrateDie` modules.
   - `engine/tests/data/map-survey.json` (181 retail maps): **0** occurrences of `CreateCrateDie` or `CrateData`.

## 5. Stop

No object in RotWK 2.01 or Ennorath 1.9.8.2 reaches `CrateData` or instantiates `CreateCrateDie` (the sole reader of `TheCrateSystem` in `game.dat`). Porting `TheCrateSystem` and `CreateCrateDie` now would add dead subsystem/module code and touch shared `AIUpdateInterface + 0x238` (`m_crateCreated`) simulation state with zero live callers in retail or Ennorath data. Registered as `S-7211`.

INFERENCE: Ennorath per-map `map.ini` files are not part of the `data/ini` + `bases` audit archive (`S-116` also keeps non-`Object` `map.ini` blocks as recording stubs); runtime-generated pointers outside the static `game.dat` image were not excluded.
Owner decision needed: close #185 as unreachable in RotWK 2.01 / Ennorath 1.9.8.2 data, or approve porting `TheCrateSystem` + `CreateCrateDie` (including `AIUpdateInterface::m_crateCreated`) for synthetic/third-party mod coverage.
