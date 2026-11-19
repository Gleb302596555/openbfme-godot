# Production and economy modules: the 64 uncited own functions (Local 91, #717)

The function map (`RW_FUNCTION_MAP.tsv`) listed 64 depth-0 functions with `status = none` across 13 production and economy module classes (63 unique addresses; `0x004B612F` is shared by `ProductionUpdate` and `BaseUpgrade`). Every address was disassembled from RotWK 2.01 `game.dat` (`md5 daed3668…`) with `tools/mod/rw.py dis` and cross-checked against the BFME2 1.06 decompilation (`/content/reference/open-bfme-2`).

Status:
- **a** = ported; `RW 0x…` citation added at the port's function (no behavior change, no test needed).
- **b** = ported differently (or handled by C++ / module registry / native snapshot); difference noted and `RW 0x…` citation added at the counterpart.
- **c** = missing; ported in this task with synthetic `LOCAL-91` tests.

## Classification (64 entries, 63 unique addresses)

| Class | RW | Function / Behavior | Status | Port / Action |
|---|---|---|---|---|
| `ProductionUpdate` | `0x004B612F` | `ModuleData::rva002115D0` (returns `AsciiString::TheEmptyString` `0xDC62B8`) | b | Default empty-string virtual on `ModuleData` (shared with `BaseUpgrade`); no port counterpart needed |
| `ProductionUpdate` | `0x008A03C1` | `ProductionUpdateInterface` slot 9 (`isUnitInQueue(ProductionID)`): walks `m_productionQueue`, returns true if any unit/hero entry (`RW 0x729661`: type 1 or 3) has `productionID == id` | **c** | **Ported** (`ProductionUpdate::isUnitInQueue`) |
| `ProductionUpdate` | `0x008A05A7` | `ProductionUpdate::onDie` (`DieModuleInterface` thunk: `this -= 4`, calls slot 16 `cancelAndReleaseAllProduction` `RW 0x8A1666`) | b | Ported in `Object::onDie` (`Object.cpp`) calling `production->cancelAndReleaseAllProduction()` directly; cited |
| `ProductionUpdate` | `0x008A0625` | `ProductionUpdateInterface` slot 19 (`countValue30InQueue(int)`): walks `m_productionQueue`, counts entries with `value30 == arg` (`+0x30`) | **c** | **Ported** (`ProductionUpdate::countValue30InQueue`) |
| `ProductionUpdate` | `0x008A18DA` | `GetSnapshotName()` -> `"ProductionUpdate"` (`0xC0B1A8`) | b | Module registry (`ProductionUpdate::registerClass`); cited |
| `ProductionUpdate` | `0x008A18ED` | `BehaviorModule::getDie()` (`this ? this + 0x24 : nullptr`) | b | `Object::onDie` reaches `ProductionUpdate` via `getProductionUpdateInterface()`; cited |
| `ProductionUpdate` | `0x008A1904` | `ProductionUpdateInterface` slot 22 (`nextProduction(const ProductionEntry*)`) | a | Ported (`ProductionUpdate::nextProduction`); cited |
| `ProductionUpdate` | `0x008A1916` | `ProductionUpdateInterface` slot 24 (`setFactoryDisabled(bool)`, `+0x13C`) | a | Ported (`ProductionUpdate::setFactoryDisabled`); cited |
| `ProductionUpdate` | `0x008A1923` | `ProductionUpdateInterface` slot 25 (`isFactoryDisabled()`, `+0x13C`) | a | Ported (`ProductionUpdate::isFactoryDisabled`); cited |
| `ProductionUpdate` | `0x008A192A` | `getModuleNameKey()` (cached `NameKey` of `"ProductionUpdate"`) | b | Module registry (`ProductionUpdate::registerClass`); cited |
| `ProductionUpdate` | `0x008A196F` | `getDisabledTypesToProcess()` (copies `m_moduleData + 0x2C`) | a | Ported (`ProductionUpdate::getDisabledTypesToProcess`); cited |
| `ProductionUpdate` | `0x008A1B36` | `ProductionUpdate::scalar deleting destructor` (calls `0x8A198B`) | b | C++ `~ProductionUpdate` owns its members; cited |
| `ProductionUpdate` | `0x008A3084` | `ProductionEntry::scalar deleting destructor` (calls `0x8A1A9D`) | b | `std::unique_ptr<ProductionEntry>` in `m_queue`; cited |
| `ProductionUpdate` | `0x008A30A0` | `ProductionUpdateInterface` slot 13 (`cancelUnitCreateByTemplate(tt, immediate)`): `!immediate` appends `tt->getName()` to `m_pendingCancelTemplates` (`+0x130`), drained at top of `update()` (`RW 0x8A1B52`); `immediate` cancels all queued unit/hero entries with `percentComplete < 100.0f` (`0xBD88D8`) and `isEquivalentTo(tt, entry->objectToProduce)` (`RW 0x73D5C2`) | **c** | **Ported** (`ProductionUpdate::cancelUnitCreateByTemplate`, `RW 0x8A1B52` drain in `update()`, `crc` + `xfer` v2) |
| `ProductionUpdate` | `0x008A3111` | `ProductionUpdate::xfer(Xfer*)` (including `m_pendingCancelTemplates` at `+0x130`, version >= 6) | b | OpenBFME native snapshot (`ProductionUpdate::xfer`, bumped to v2 with `m_pendingCancelTemplates`); cited |
| `ProductionUpdate` | `0x009F9F37` | `ProductionUpdateInterface` slot 17 (`getProductionCount()`, `+0x34`) | a | Ported (`ProductionUpdate::getProductionCount`); cited |
| `QueueProductionExitUpdate` | `0x008A3851` | `getModuleNameKey()` (`"QueueProductionExitUpdate"`) | b | Module registry; cited |
| `QueueProductionExitUpdate` | `0x008A3896` | `GetSnapshotName()` (`"QueueProductionExitUpdate"`) | b | Module registry; cited |
| `QueueProductionExitUpdate` | `0x008A389C` | `getRallyPoint()` (`m_rallyPointExists ? &m_rallyPoint : nullptr`) | a | Ported (`QueueProductionExitUpdate::getRallyPoint`); cited |
| `QueueProductionExitUpdate` | `0x008A38D6` | `QueueProductionExitUpdate::xfer(Xfer*)` | a | Ported (`QueueProductionExitUpdate::xfer`); cited |
| `QueueProductionExitUpdate` | `0x008A3941` | `useSpawnRallyPoint()` (`m_data->m_canRallyToSlaughter`, `data + 0x31`) | a | Ported (`QueueProductionExitUpdate::useSpawnRallyPoint`); cited |
| `QueueProductionExitUpdate` | `0x008A39B2` | `QueueProductionExitUpdate::scalar deleting destructor` | b | C++ destructor; cited |
| `SpawnPointProductionExitUpdate` | `0x008A7208` | `GetSnapshotName()` (`"SpawnPointProductionExitUpdate"`) | b | Module registry; cited |
| `SpawnPointProductionExitUpdate` | `0x008A720E` | `SpawnPointProductionExitUpdate::xfer(Xfer*)` (base + 10 occupier IDs) | b | OpenBFME native snapshot also saves bone coords/angles/count (`SpawnPointProductionExitUpdate::xfer`); cited |
| `SpawnPointProductionExitUpdate` | `0x008A724F` | `getModuleNameKey()` (`"SpawnPointProductionExitUpdate"`) | b | Module registry; cited |
| `SpawnPointProductionExitUpdate` | `0x008A7347` | `SpawnPointProductionExitUpdate::scalar deleting destructor` | b | C++ destructor; cited |
| `SupplyCenterDockUpdate` | `0x008A44D2` | `GetSnapshotName()` (`"SupplyCenterDockUpdate"`) | b | Module registry; cited |
| `SupplyCenterDockUpdate` | `0x008A4513` | `getModuleNameKey()` (`"SupplyCenterDockUpdate"`) | b | Module registry; cited |
| `SupplyCenterDockUpdate` | `0x008A4558` | `SupplyCenterDockUpdate::scalar deleting destructor` | b | C++ destructor; cited |
| `SupplyCenterDockUpdate` | `0x008A4ABF` | `loadPostProcess()` (jumps to `DockUpdate::loadPostProcess` `RW 0x99198A`: reloads dock bone positions if `m_positionsLoaded`) | b | `DockUpdate::xferDockUpdate` saves positions and `m_numberApproachPositionBones` directly (`DockUpdate.cpp`); cited |
| `SupplyCenterProductionExitUpdate` | `0x008A9BBB` | `GetSnapshotName()` (`"SupplyCenterProductionExitUpdate"`) | b | Module registry; cited |
| `SupplyCenterProductionExitUpdate` | `0x008A9BE3` | `getModuleNameKey()` (`"SupplyCenterProductionExitUpdate"`) | b | Module registry; cited |
| `SupplyCenterProductionExitUpdate` | `0x008A9CA0` | `SupplyCenterProductionExitUpdate::scalar deleting destructor` | b | C++ destructor; cited |
| `SupplyCenterProductionExitUpdate` | `0x008A9CBC` | Primary vtable slot 13 (`getUnitCreatePoint(Coord3D*)`, shared with `DefaultProductionExitUpdate`): transforms `m_data->m_unitCreatePoint` by object's transform matrix and returns true (false if no object). Note: `ExitInterface` slot 10 (`+0x28`) is still `0x008655DC` (returns false). | **c** | **Ported** (`SimpleProductionExitUpdate::getUnitCreatePoint`) |
| `SupplyCenterCreate` | `0x008BCEEC` | `GetSnapshotName()` (`"SupplyCenterCreate"`) | b | Module registry; cited |
| `SupplyCenterCreate` | `0x008BCF0B` | `getModuleNameKey()` (`"SupplyCenterCreate"`) | b | Module registry; cited |
| `SupplyCenterCreate` | `0x008BCF50` | `SupplyCenterCreate::scalar deleting destructor` | b | C++ destructor; cited |
| `SupplyWarehouseCreate` | `0x008BCFE3` | `GetSnapshotName()` (`"SupplyWarehouseCreate"`) | b | Module registry; cited |
| `SupplyWarehouseCreate` | `0x008BD002` | `getModuleNameKey()` (`"SupplyWarehouseCreate"`) | b | Module registry; cited |
| `SupplyWarehouseCreate` | `0x008BD047` | `SupplyWarehouseCreate::scalar deleting destructor` | b | C++ destructor; cited |
| `AutoDepositUpdate` | `0x0089D973` | `GetSnapshotName()` (`"AutoDepositUpdate"`) | b | Module registry; cited |
| `AutoDepositUpdate` | `0x0089D979` | `getModuleNameKey()` (`"AutoDepositUpdate"`) | b | Module registry; cited |
| `AutoDepositUpdate` | `0x0089DA02` | `AutoDepositUpdate::scalar deleting destructor` | b | C++ destructor; cited |
| `AutoDepositUpdate` | `0x008A438F` | `AutoDepositUpdate::xfer(Xfer*)` (`m_depositOnFrame`, `m_awardInitialCaptureBonus`, `m_started`) | a | Ported (`AutoDepositUpdate::xfer`); cited |
| `BuildableHeroListUpgrade` | `0x008BC40C` | `GetSnapshotName()` (`"BuildableHeroListUpgrade"`) | b | Module registry; cited |
| `BuildableHeroListUpgrade` | `0x008BC412` | `getModuleNameKey()` (`"BuildableHeroListUpgrade"`) | b | Module registry; cited |
| `BuildableHeroListUpgrade` | `0x008BC4AA` | `BuildableHeroListUpgrade::scalar deleting destructor` | b | C++ destructor; cited |
| `BuildableHeroListUpgrade` | `0x008BC5CB` | `BuildableHeroListUpgrade::processUpgradeRemoval()` (`UpgradeMux` slot 8): for each `BuildableRingHeroesMP` template, calls `RW 0x8BC519` (schedules cancellation on every player `FS_FACTORY` with `getProductionCount() > 0` via `ProductionUpdateInterface` slot 13 `RW 0x8A30A0(tt, false)`), removes the template from `PlayerHeroList` (`RW 0x78138B`), marks control bar dirty, and clears custom anim (`RW 0x8D28F9`) | **c** | **Ported** (`BuildableHeroListUpgrade::processUpgradeRemoval`, `PlayerHeroList::removePurchase`) |
| `BaseUpgrade` | `0x004B612F` | Duplicate of `ProductionUpdate` `0x004B612F` (`ModuleData::rva002115D0`) | b | See `ProductionUpdate` |
| `BaseUpgrade` | `0x008B77B6` | `GetSnapshotName()` (`"BaseUpgrade"`) | b | Module registry; cited |
| `BaseUpgrade` | `0x008B77BC` | `getModuleNameKey()` (`"BaseUpgrade"`) | b | Module registry; cited |
| `BaseUpgrade` | `0x008B78B6` | `BaseUpgradeModuleData::scalar deleting destructor` | b | C++ destructor; cited |
| `BaseUpgrade` | `0x008B78D2` | `BaseUpgrade::scalar deleting destructor` | b | C++ destructor; cited |
| `RebuildHoleBehavior` | `0x00886554` | `getModuleNameKey()` (`"RebuildHoleBehavior"`) | b | Module registry; cited |
| `RebuildHoleBehavior` | `0x00886599` | `GetSnapshotName()` (`"RebuildHoleBehavior"`) | b | Module registry; cited |
| `RebuildHoleBehavior` | `0x0088659F` | `getRebuildHoleInterface()` (`this ? this + 0x24 : nullptr`) | b | `RebuildHoleBehavior::getRebuildHoleInterfaceFromObject` (`RebuildHoleModules.h`); cited |
| `RebuildHoleBehavior` | `0x008868A6` | `RebuildHoleBehavior::scalar deleting destructor` (calls `RW 0x8867BB` which destroys worker) | b | C++ destructor / `onDelete`; cited |
| `RebuildHoleExposeDie` | `0x00889A43` | `getModuleNameKey()` (`"RebuildHoleExposeDie"`) | b | Module registry; cited |
| `RebuildHoleExposeDie` | `0x00889A88` | `GetSnapshotName()` (`"RebuildHoleExposeDie"`) | b | Module registry; cited |
| `RebuildHoleExposeDie` | `0x00889A8E` | `RebuildHoleExposeDie::scalar deleting destructor` | b | C++ destructor; cited |
| `PillageModule` | `0x008881C7` | `GetSnapshotName()` (`"PillageModule"`) | b | Module registry; cited |
| `PillageModule` | `0x008881CD` | `PillageModule::scalar deleting destructor` | b | C++ destructor; cited |
| `PillageModule` | `0x00888202` | `getModuleNameKey()` (`"PillageModule"`) | b | Module registry; cited |
| `PillageModule` | `0x00888247` | `PillageModule::xfer(Xfer*)` (`BehaviorModule::xfer` + `m_counter`) | a | Ported (`PillageModule::xfer`); cited |

Summary: **10 (a) already ported**, **49 (b) ported differently / registry / C++ dtors** (48 unique + 1 shared `0x004B612F`), **5 (c) missing -> all 5 ported in this task**, **0 stops needed**.
