# План переноса Ennorath и недостающих систем RotWK 2.01 в upstream (Open-BFME)

Срез от **2026-11-25** (`main` форка `c2ebd6eaf` против `upstream/main` `2eb79285d`).
База мода: **Ennorath 1.9.8.7** (`game.dat` MD5 `f222b925edf2a9553e574ad8445f2841`) и **1.9.8.5** (`fc32e4c4142b5c607b90972eaf448400`).

Документ фиксирует, что уже готово в форке и в каком порядке это переносить в `Open-BFME/openbfme-godot`, чтобы upstream со временем смог запускать Ennorath нативно.

---

## Как устроен перенос и как upstream запустит Ennorath

1. **Не делать весь форк профилем.** В профиль (`ModEngineFeatures`, см. upstream PR #409 / #423 / #425) идут **только** правки пропатченного pySAGE `game.dat` мода — то, что отличается от ванильного `game.dat` RotWK 2.01.
2. **Весь ванильный код RotWK 2.01** (загрузчики, модули, ИИ, Draw-модули, эффекты, парсинг INI) идёт в `Open-BFME/openbfme-godot` обычными PR **без флагов профиля** — это общий движок для всех модов и ванили.
3. **Запуск в upstream:** файлы и архивы Ennorath в Git и PR **никогда не отправляются**. Когда нужные PR будут приняты, любой пользователь или мейнтейнер upstream запускает локально установленный Ennorath обычной командой:
   ```text
   Godot ... --path godot -- --mod-install=<путь_к_установке_Ennorath>
   ```
   Движок сам считает MD5 `game.dat`, находит профиль в `ModEngineFeatures` и включает нужные pySAGE-возможности. Все тесты в upstream PR — синтетические, на мини-INI без данных мода.

---

## Часть 1. Профиль Ennorath (`ModEngineFeatures`, pySAGE-правки `game.dat`)

Исходники в форке: `engine/src/Common/ModEngineFeatures.h`, `engine/src/Common/System/ModEngineFeatures.cpp`, карта секций — [ENNORATH_EXE.md](ENNORATH_EXE.md).

### Уже покрыто в upstream (не дублировать)

У Ennorath 1.9.8.5 и Patch 2.02 9.8.5 / 9.8.6 **один и тот же MD5 `game.dat`** (`fc32e4c4142b5c607b90972eaf448400`). В upstream уже открыты PR от `LuisTNH`:
- **PR #409**: регистрация `fc32e4c4…`, `.cmdext` (`edain-unchained.commandset-limit`), `.obscmd` (`edain-unchained.observer-command-range`), `leftOutArchives`.
- **PR #423**: `.stancem` (`patch-202.hold-ground-keeps-move` = наш `ennorath.stance-keeps-moving`).
- **PR #425**: `.bldcap` (`patch-202.building-limit-counts-phantoms` = наш `ennorath.building-limit-fix`).

После мёрджа `#409` профиль Ennorath 1.9.8.7 (`f222b925…`) должен переиспользовать те же общие имена фич, а не заводить дубликаты.

### 1А. Маленькие и автономные pySAGE-фичи (12 пунктов, удобно слать по 1 PR)

| Статус | Фича / задача | Секция pySAGE | Основные файлы в форке |
|---|---|---|---|
| [ ] | Профиль `Ennorath 1.9.8.7 game.dat` (`f222b925…`) | MD5 `f222b925edf2a9553e574ad8445f2841` | `ModEngineFeatures.cpp` |
| [ ] | `ennorath.evacuate-containers` | `.keydef` `0xF39580` | `GarrisonContain.cpp` (`orderAllPassengersToExit`) |
| [ ] | `ennorath.middle-click-camera` | `.camfix` | `LookAtXlat.cpp` |
| [ ] | `ennorath.spell-store-delay` | `.spstore` | `AptSpellStore.cpp` (задержка 500 мс) |
| [ ] | `ennorath.chat-clipboard` | `.txtclip` | `GadgetTextEntry.cpp`, `GodotAptPlayer.cpp` |
| [ ] | `ennorath.upgrades-5120` | `.upg5k` | `Upgrade.{h,cpp}` (маска 5120 бит) |
| [ ] | `ennorath.toggle-mounted-zero-speed` (1.9.8.7) | `.wmfeat` `0xF3A940` | `SpecialAbilityModules.cpp` |
| [ ] | `ennorath.garrison-hero-survive` (1.9.8.7) | `.wmfeat` `0xF3A95F` | `GarrisonContain.cpp`, `OpenContainRuntime.cpp` |
| [ ] | `ennorath.cant-earn-powerpoints` (1.9.8.7) | `.wmfeat` `0xF3A34C` | `KindOfTokens.cpp`, `ExperienceWorld.{h,cpp}`, `SpecialAbilityModules.cpp`, `LivingWorldAutoResolveBattle.cpp` |
| [ ] | `ennorath.portrait-upgrade` (1.9.8.7) | `.wmfeat` `0xF3A000` | `UpgradeModuleClasses.{h,cpp}`, `ModuleFactory.cpp`, `PalantirCommandUI.cpp`, `InGameHeroSelect.cpp` |
| [ ] | `ennorath.filtered-horde-respawn` (1.9.8.7) | `.wmfeat` `0xF3A20D` | `AreaScanModules.{h,cpp}`, `ModuleFactory.cpp` |
| [ ] | `ennorath.builder-fixes` (1.9.8.7) | `.wmfeat` `0xF3AA56` / `.hotkeys` | `InGameHeroSelect.cpp` |

### 1Б. Средние и крупные pySAGE-фичи (UI, лобби, реплеи, Война Кольца — 8 пунктов)

| Статус | Фича | Секции pySAGE | Основные файлы в форке |
|---|---|---|---|
| [ ] | `ennorath.online-delay-fix` | `.dlyfix` | `EnnorathOptions.cpp`, `AptSimpleScreens.cpp` |
| [ ] | `ennorath.lobby-map-search` | `.mapfind`, `.mapsvar` | `AptSkirmish`, `AptLanLobby`, `AptOnlineShell`, `GadgetTextEntry` |
| [ ] | `ennorath.chat-mute` | `.chatmut` | `AptSimpleScreens.cpp`, `GodotGameWorld.cpp`, `Network::fileChat` |
| [ ] | `ennorath.options-pages` | `.optui`, `.optfix` | `EnnorathOptions.{h,cpp}`, `AptSimpleScreens.cpp` |
| [ ] | `ennorath.option-hotkeys` + `ennorath.option-hotkeys-1987` | `.hotkeys`, `.keydef`, `.keycmd`, `.wmkeys` | `EnnorathOptions.{h,cpp}`, `AptSimpleScreens.cpp`, `CommandXlat.cpp`, `InGameHeroSelect.cpp`, `GodotInGameHud.cpp` |
| [ ] | `ennorath.select-all-buildings` | `.bldgrp`, `.padsel`, `.bldext`, `.bldcan`, `.bldlvl` | `InGameUI.cpp`, `ControlBar.cpp`, `ControlBarCommandProcessing.cpp` |
| [ ] | `ennorath.replay-tabs-chat-pause` | `.rpchat`, `.rppause`, `.rppvar` | `Recorder.{h,cpp}`, `ReplaySeek.h`, `AptPalantir`, `AptMessenger`, `GodotGameWorld`, `game.gd` |
| [ ] | `ennorath.wotr-8-players` | `.wotrui`, `.wotrobs`, `.wtrview`, `.wtrdisp`, `.lobfac` | `AptSkirmish`, `SkirmishGameSetup`, `LivingWorldSetupText`, `LivingWorldLogic`, `GodotGameWorld`, `NetGameSession` |

*(Лимиты 30/60 FPS `.fxrate`, все разрешения `.ressort`, >14 фракций и >2 строителей в OpenBFME отсутствуют изначально и в профиль не переносятся — см. [ENNORATH_EXE.md](ENNORATH_EXE.md).)*

---

## Часть 2. Ванильные модули и подсистемы RotWK 2.01 (без профиля)

На 2026-11-25 в `engine/src/` форка есть **92 новых `.cpp`-файла**, которых нет в `upstream/main`.
Из них **4 уже находятся в ветках `up/*`**: `ControlBarScheme` (`up/local-12`, PR #428), `ObjectShadow` (`up/local-14`, PR #435), `FadeAndDieOrnamentUpdate` (`up/local-105`, PR #362), `AITeamTempo` (`up/port38`).
Ниже — оставшиеся **88 файлов** (не считая `EnnorathOptions.cpp`, который относится к Части 1Б).

### 2.1. Загрузчики, парсинг и рендер-база (7 файлов — приоритет №1) — **ОТПРАВЛЕНО**

Нужны, чтобы upstream корректно загружал текстуры, карты и эффекты мода:
- [x] `GameClient/DdsDecode.cpp` + `GodotDevice/GodotDds.cpp` (нестандартные DDS и короткие mip-цепочки) — **upstream PR `#448`** (`up/dds-decode`)
- [x] `GameClient/FXListParseServices.cpp` (валидация звуков/EVA в `FXList`, закрывает `S-190`) — **upstream PR `#449`** (`up/port-330`)
- [x] W3D `textureloader`: пропуск расширений без точки (`RW 0x530D59`), подавление ложных `texture not found` — **upstream PR `#450`** (`up/port-430`)
- [x] Усечённые W3D-файлы (`ChunkLoadClass::allowTruncatedEnd`) и отсутствующие сабобъекты (`AbsentFromData`) — **upstream PR `#451`** (`up/w3d-truncated-and-absent`)
- [x] `SubsystemLoadReport::macrosBeforeFX` → `FXPlayback::loadRetailData` (макросы `#define` из `Water.ini`/ранних файлов до `FXList.ini`) — **upstream PR `#452`** (`up/fx-macros-before-fx`)
- [x] `Libraries/WWVegas/WW3D2/lightenvironment.cpp` (`LightEnvironmentClass`, per-render-object освещение, `S-687`) — **upstream PR `#453`** (`up/local-38`)
- [x] `Common/LightPointSystem.cpp` (система light points Войны Кольца, скрипты 351–354 / 405 / условие 124, `LightPointsUpSound`, `Player::xfer` v3) — **upstream PR `#454`** (`up/port-113`)
- [x] `GameClient/GUI/AptScreens/AptMapPreview.cpp` (превью карты и стартовые точки на экране загрузки, `ShowRandomStartPos`, `isSlotLocalAlly`/`apparentStartPos`) — **upstream PR `#455`** (`up/local-56`)
- [ ] `Common/BootSplash.cpp` — *пропущено: в коде нет ни одного вызывающего (upstream отклоняет мёртвый код)*
- [x] `Common/FunctionCurve.cpp` — **upstream PR `#490`** (`up/part-the-heavens-update`, вместе с `PartTheHeavensUpdate` и `ProjectedShadowManager`)
- *(Проверено: `Win32BIGFileSystem.cpp` не имеет отличий от `upstream/main`.)*

### 2.2. Логические модули (`GameLogic/Module/*` + `AODHordeContain`, 44 класса) — **ОТПРАВЛЕНО**

Автономные ванильные классы — идеально уходят по 1 PR на модуль:

- **Апгрейды (6):**
  - [x] `BaseUpgrade` — **upstream PR `#459`** (`up/base-upgrade`)
  - [x] `GiveUpgradeUpdate` *(проверен сценарием мода)* — **upstream PR `#493`** (`up/give-upgrade-update`)
  - [x] `LocomotorSetUpgrade` — **upstream PR `#458`** (`up/locomotor-set-upgrade`)
  - [x] `TooltipUpgrade` — **upstream PR `#460`** (`up/tooltip-upgrade`)
  - [x] `WallUpgradeUpdate` — **upstream PR `#462`** (`up/wall-upgrade-update`)
  - [x] `WeaponBonusUpgrade` — **upstream PR `#457`** (`up/weapon-bonus-upgrade`)
- **Спецспособности, контейнеры и орды (11):**
  - [x] `AODCrushCollide` + `GameLogic/Object/Contain/AODHordeContain` (**upstream PR `#494`**, `up/aod-horde-and-crush`) + `OathbreakersFadeAwayBehavior` (**upstream PR `#489`**, `up/oathbreakers-fade-away`)
  - [x] `DeflectSpecialPower` — **upstream PR `#467`** (`up/deflect-special-power`)
  - [x] `DetachableRiderUpdate` (+ `DetachableRiderBody`) — **upstream PR `#488`** (`up/detachable-rider`)
  - [x] `FlingPassengerSpecialAbilityUpdate` — **upstream PR `#470`** (`up/fling-passenger`)
  - [x] `HordeTransportContainDamage` — **upstream PR `#468`** (`up/horde-transport-contain-damage`)
  - [x] `HordeWorkerAIUpdate` — **upstream PR `#486`** (`up/horde-worker-ai-update`)
  - [x] `SplitHordeSpecialPower` — **upstream PR `#491`** (`up/split-horde-special-power`)
  - [x] `StoreObjectsSpecialPower` — **upstream PR `#465`** (`up/store-and-teleport-powers`)
  - [x] `TeleportToCasterSpecialPower` — **upstream PR `#465`** (`up/store-and-teleport-powers`)
- **Урон, смерть и эффекты на поле (13):**
  - [x] `ClearanceTestingSlowDeathGeometry` — **upstream PR `#483`** (`up/clearance-death-geometry`)
  - [x] `DamageFieldUpdate` — **upstream PR `#481`** (`up/damage-field-update`)
  - [x] `DestroyEnvironmentUpdate` — **upstream PR `#492`** (`up/destroy-environment-and-rain-of-fire`)
  - [x] `EvacuateDamage` — *уже открыт в upstream как PR `#456`*
  - [x] `FreeLifeBody` — **upstream PR `#464`** (`up/free-life-body`)
  - [x] `GiantBirdSlowDeathBehavior` — **upstream PR `#487`** (`up/giant-bird-slow-death`); 2026-11-26: superseded by LuisTNH's **#518** (MOVING cleared after the die modules RW 0x699044, paralysed physics RW 0x792C6A, xfer v2); S-5606 moves into #518 with credit, then #487 is closed. On the next upstream merge take #518's version.
  - [x] `OilSpillUpdate` — **upstream PR `#472`** (`up/oil-spill-update`)
  - [x] `PoisonedBehavior` — **upstream PR `#466`** (`up/poisoned-behavior`)
  - [x] `ProjectileStreamUpdate` — **upstream PR `#484`** (`up/projectile-stream-update`)
  - [x] `RainOfFireUpdate` — **upstream PR `#492`** (`up/destroy-environment-and-rain-of-fire`)
  - [x] `ReflectDamage` — **upstream PR `#463`** (`up/reflect-damage`)
  - [x] `ShipSlowDeathBehavior` — **upstream PR `#475`** (`up/ship-slow-death`)
  - [x] `StrafeAreaUpdate` — **upstream PR `#485`** (`up/strafe-area-update`)
- **Поведения, спавн и прочее (14):**
  - [x] `AimWeaponBehavior` — *уже открыт в upstream как PR `#447`*
  - [x] `AnimalAIUpdate` — *уже реализован в `upstream/main` через `AmbientAIUpdate` (`S-5910`)*
  - [x] `AnnounceBirthAndDeathBehavior` — **upstream PR `#478`** (`up/announce-birth-and-death`)
  - [x] `CivilianSpawnCollide` + `CivilianSpawnUpdate` — **upstream PR `#477`** (`up/civilian-spawn`)
  - [x] `DelayedLuaEventUpdate` — **upstream PR `#474`** (`up/delayed-lua-event-update`)
  - [x] `DynamicShroudClearingRangeUpdate` — **upstream PR `#476`** (`up/dynamic-shroud-clearing-range`)
  - [x] `EntEnragedUpdate` — **upstream PR `#479`** (`up/ent-enraged-update`)
  - [x] `OCLUpdate` — **upstream PR `#480`** (`up/ocl-update`)
  - [x] `OneRingPenaltyUpdate` — **upstream PR `#482`** (`up/one-ring-penalty-update`)
  - [x] `PartTheHeavensUpdate` — **upstream PR `#490`** (`up/part-the-heavens-update`)
  - [x] `ReplaceObjectUpdate` — **upstream PR `#473`** (`up/replace-object-update`)
  - [x] `RunOffMapBehavior` — **upstream PR `#469`** (`up/run-off-map-behavior`)
  - [x] `SpecialEnemySenseUpdate` — **upstream PR `#471`** (`up/special-enemy-sense`)

### 2.3. Крупные системы логики и ИИ (10 файлов) — **ВЫПОЛНЕНО**

- [x] `GameLogic/Map/FireLogicSystem.cpp` (**upstream PR `#510`**, `up/fire-logic-system`) + `GameLogic/AI/AIBurningDeathState.cpp` (**upstream PR `#509`**, `up/ai-burning-death-state`)
- [x] `GameLogic/AI/AIRampageState.cpp` — **upstream PR `#501`** (`up/ai-rampage-state`)
- [x] `GameLogic/AI/AIAttackCharge.cpp` — **upstream PR `#496`** (`up/ai-attack-charge`)
- [x] `GameLogic/AI/AIAttackHordePosition.cpp` — **upstream PR `#498`** (`up/ai-attack-horde-position`)
- [x] `GameLogic/AI/AIGroupOrders.cpp` — **upstream PR `#508`** (`up/ai-group-orders`); *(клиентский `GameClient/OrderLines.cpp` зависит от мёрджа `#508` и `#504` `SegLineGeometry`)*
- [x] `GameLogic/AI/AITNGuard.cpp` — **upstream PR `#500`** (`up/ai-tn-guard`)
- [x] `GameLogic/Map/TerrainTaintClear.cpp` *(уже в `upstream/main` через `TAINTAREA-1` `GameLogic::clearTaintArea`/`clearTaintObjects`)* + `W3DDevice/GameClient/W3DTaintMask.cpp` (**upstream PR `#503`**, `up/w3d-taint-mask`)
- [x] `GameLogic/System/CrateSystem.cpp` — **upstream PR `#495`** (`up/crate-and-victory-systems`)
- [x] `GameLogic/System/VictorySystem.cpp` — **upstream PR `#495`** (`up/crate-and-victory-systems`)

### 2.4. Клиентские Draw-модули, декали, эффекты и звук (21 файл)

*(Визуальные PR отправляем сначала без скриншотов; если мейнтейнеры upstream запросят — снимаем «до/после».)*

- **W3D Draw-модули (8):**
  - [x] `W3DDebrisDraw` — **upstream PR `#497`** (`up/w3d-debris-draw`)
  - [x] `W3DLaserDraw` + `GameClient/LaserUpdate` — **upstream PR `#499`** (`up/laser-draw-update`)
  - [ ] `W3DProjectileStreamDraw` — *ждёт мёрджа `#484` (`ProjectileStreamUpdate`, из него читаются точки потока)*
  - [x] `W3DQuadrupedDraw` — **upstream PR `#502`** (`up/w3d-quadruped-draw`; без `calcPhysicsXformHugeFourLegs`: в upstream нет семейства `calcPhysicsXform`)
  - [x] `W3DSailModelDraw` — **upstream PR `#505`** (`up/w3d-sail-model-draw`, вместе с размещением AttachToBoneInAnotherModule, `DrawableAttach.cpp`)
  - [x] `W3DSupplyDraw` — **upstream PR `#517`** (`up/w3d-supply-draw`; только сторона отрисовки, источник `SupplyWarehouseDockUpdate` не в upstream, S-112)
  - [x] `W3DTornadoDraw` — **upstream PR `#513`** (`up/w3d-tornado-draw`; без огибающей тона: в upstream нет tint envelope Drawable, S-113)
- **Декали на земле (4):**
  - [x] `TerrainScorches` — **upstream PR `#507`** (`up/terrain-scorches`, только ядро `TerrainScorchBuffer`; `GodotTerrainScorches` и проводка ждут решения мейнтейнеров о замене пути `ScorchBuffer` из PR `#246`)
  - [ ] `TerrainTracks` + `GodotTerrainTracks` — *ждёт `#507` и Godot-части выжженной земли (материал слоя; ещё поля снимка логики и `MakeTrackMarks`/`MaxTerrainTracks`, ~30 файлов)*
- **Клиентские поведения, звук и тени (9):**
  - [ ] `ContainedPose` — *заблокирован: нужен `Drawable::getPresentationPose` (физика представления `applyPhysicsXform`/`calcPitchRoll`, в upstream нет; см. открытый `#430`)*
  - [x] `EvaAnnounceClientCreate` *(проверен сценарием мода)* — **upstream PR `#511`** (`up/eva-announce-client-create`, с установкой Eva-поиска в `RetailObjectWorld`)
  - [ ] `EvaSighting` + `GameLogic/Object/EvaSightingNotice` — *заблокирован: читает живую `GameLogic` из `DrawableManager::syncTransforms`, а в upstream клиент не держит логику (поток логики, S-810); нужна переделка на снимок или решение мейнтейнеров*
  - [x] `Common/Audio/ScoredKillEvaAnnouncer` — **upstream PR `#516`** (`up/scored-kill-eva-announcer`; инертен, пока никто не ставит контроллер)
  - [x] `ModelConditionAudioLoopClientBehavior` *(проверен сценарием мода)* — **upstream PR `#512`** (`up/model-condition-audio-loop`; S-6024)
  - [x] `ParticleBoneFollow` — **upstream PR `#506`** (`up/particle-bone-follow`)
  - [x] `ProjectedShadowManager` — **upstream PR `#490`** (`up/part-the-heavens-update`, вместе с `PartTheHeavensUpdate` и `FunctionCurve`)
  - [x] `RadarMarkerClientUpdate` — **upstream PR `#514`** (`up/radar-marker-client-update`; `clientUpdate` пока никто не вызывает, S-2451)
  - [x] `SegLineGeometry` — **upstream PR `#504`** (`up/seg-line-geometry`)
  - [x] `SwayClientUpdate` — **upstream PR `#515`** (`up/sway-client-update`)

---

## Часть 3. Порядок выполнения и текущий статус (handoff 2026-11-25)

### Что уже сделано
1. **Этап 0 — ЗАВЕРШЁН (починены 4 красных открытых PR в upstream):**
   - [x] PR `#437` (`up/local-132`, `1949b316f`) — убрана лишняя `)` в `engine/CMakeLists.txt`; Windows/Linux/TSan/Cross/Commit gate зелёные.
   - [x] PR `#390` (`up/local-75`, `33acd1d82`) — влит `upstream/main`, обновлён `Enum` в `test_hero_facing_execution.cpp`; Windows/Linux/macOS/TSan/Cross зелёные.
   - [x] PR `#361` (`up/local-104`, `4f990e328`) — тест `test_stealth_disguise_load.cpp` переведён на полный `GameState::saveSnapshot`/`loadSnapshot`; все проверки зелёные.
   - [x] PR `#387` (`up/local-30`, `88841d91e` + `95ff604c9`) — влит `upstream/main`, `ActiveBody::xfer` поднят до v4 (поверх upstream v3 `BLDGDEBRIS-1`), обновлены `test_fx4_transition.cpp` и `test_bldgdebris1.cpp` (25 байт = 24 байта v3 + 1 байт v4); CI перезапущен после `95ff604c9`.
2. **Этап 1 — ЗАВЕРШЁН (открыты 8 новых PR `#448`–`#455`, см. раздел 2.1):**
   - `#448` (`up/dds-decode`) — CI полностью зелёный (доходит только ASan).
   - `#449` (`up/port-330`), `#450` (`up/port-430`), `#451` (`up/w3d-truncated-and-absent`), `#452` (`up/fx-macros-before-fx`) — Commit gate зелёный, кросс-платформенные сборки/санитайзеры завершаются.
   - `#453` (`up/local-38`), `#454` (`up/port-113`), `#455` (`up/local-56`) — открыты, локальный commit gate пройден, поставлены в очередь CI.
3. **Этап 2.2 — ЗАВЕРШЁН (открыты 37 новых PR `#457`–`#494`, см. раздел 2.2 + `FunctionCurve`/`ProjectedShadowManager`):**
   - Все 44 класса раздела 2.2 покрыты (`#457`–`#494`, плюс `AimWeaponBehavior` `#447`, `EvacuateDamage` `#456`, `AnimalAIUpdate` уже в `upstream/main`).
   - Починены CI-проверки: лексикографическая сортировка `docs/STOPS.md`, BFME1-поле `RunToLocation` (`#469`), `tools/save/closed_sets.json` (`#474`, `#476`, `#480`).
4. **Этап 2.3 — ЗАВЕРШЁН (открыты 9 новых PR `#495`, `#496`, `#498`, `#500`, `#501`, `#503`, `#508`, `#509`, `#510`, см. раздел 2.3):**
   - Покрыты все 9 пунктов раздела 2.3 (`CrateSystem` + `VictorySystem`, `AIAttackCharge`, `AIAttackHordePosition`, `AITNGuard`, `AIRampageState`, `W3DTaintMask` при готовом `TerrainTaintClear` в `upstream/main`, `AIGroupOrders`, `AIBurningDeathState`, `FireLogicSystem`).
5. **Этап 2.4 — ОТПРАВЛЕН (открыты 14 PR `#497`, `#499`, `#502`, `#504`–`#507`, `#511`–`#517`, см. раздел 2.4; 2026-11-26, два параллельных исполнителя: ПК `workspace/wt/upstream-pr` и песочница 8 `wt/up-pr`):**
   - Не отправлены 4 пункта: `W3DProjectileStreamDraw` (ждёт `#484`), `TerrainTracks` (ждёт `#507`), `ContainedPose` (нужна физика представления Drawable), `EvaSighting` (клиент не держит логику в upstream). Черновик `EvaSighting` не собирался; патч лежал в scratchpad сессии.
   - Конфликты при мёрдже: все PR правят `engine/CMakeLists.txt`, `test_module_coverage.cpp` (183/126 → 184/125, как и `#490`/`#482`), `tools/sim/sim_policy.json`, `registerAll`; после первого мёрджа остальным — rebase и пересчёт счётчиков.
   - Ловушка `sim_audit`: включение `AudioEventRTS.h` или `RadiusCursor.h` из симуляционного исходника (через `Drawable.h`/`Drawable.cpp`) роняет аудит — такие части вынесены в `DrawableTornado.cpp` / `DrawableAudioLoop.cpp`.
6. **Профиль Ennorath (`ModEngineFeatures`, Часть 1А и 1Б) — ЕЩЁ НЕ ОТПРАВЛЕН.**
   - Ждёт Этапа 3 (и мёрджа upstream PR `#409` от `LuisTNH` с регистрацией `fc32e4c4…`, чтобы не конфликтовать и переиспользовать общие имена фич).

### Следующие шаги (для следующего агента)
0. **Пауза upstream (2026-11-26, мейнтейнер Ancalgonn в upstream #465):** новых PR не открывать, пока очередь (~10 обзоров в день) не сократится; бот `upstream_loop.sh` на песочнице 4 остановлен. Порядок обзора, который мы назвали: #496, #498, #466, #481, #467, #465, #491, #493, #480, #488, остальное по номерам. #471 закрыт в пользу #461 (LuisTNH), #487 уступает #518 (LuisTNH).
   - **Обещано мейнтейнеру:** до их обзора пройти топ-10 по чек-листу сохранений (Часть 4, последний пункт) и запушить исправления в их ветки `up/*`.
1. **Проверить финальный CI** по `#387` и `#449`–`#455` (если где-то будет реальное падение — починить в `workspace/wt/upstream-pr`; известные флаки раннеров: `test_mp3_impairment` на macOS, `test_platformicon1` порт-флак на Windows и `Greptile Review` rate-limit).
2. **Этап 2 — быстрые автономные ванильные модули (раздел 2.2):**
   - По 1 классу на PR, начиная с 6 апгрейдов (`BaseUpgrade`, `GiveUpgradeUpdate`, `LocomotorSetUpgrade`, `TooltipUpgrade`, `WallUpgradeUpdate`, `WeaponBonusUpgrade`), затем `FreeLifeBody`, `EvacuateDamage`, `SplitHordeSpecialPower`, `PoisonedBehavior`, `ReflectDamage`, `StoreObjectsSpecialPower`, `TeleportToCasterSpecialPower`, а также `PartTheHeavensUpdate` + `FunctionCurve`.
3. **Этап 3 — профиль Ennorath в `ModEngineFeatures` (Часть 1):**
   - Проверить статус upstream PR `#409` / `#423` / `#425`. Как только `#409` влит (или если владелец решит отправить профиль раньше поверх `upstream/main`), отправить пачку маленьких pySAGE-фич из раздела **1А** (начиная с профиля `Ennorath 1.9.8.7` `f222b925…` и 1-файловых фич `evacuate-containers`, `middle-click-camera`, `spell-store-delay`, `toggle-mounted-zero-speed`, `garrison-hero-survive`, `cant-earn-powerpoints`, `portrait-upgrade`, `filtered-horde-respawn`).
   - Затем по одному перенести крупные UI/реплей/WotR фичи из **1Б**.
4. **Этап 4 — крупные системы и рендер (разделы 2.3 и 2.4):**
   - `FireLogicSystem` + `AIBurningDeathState`, `AIRampageState`, `CrateSystem`, `VictorySystem`, W3D Draw-модули и декали.

---

## Часть 4. Чек-лист правил для каждого upstream PR

Полные правила: [PROCESS.md](PROCESS.md) §12 и `AGENTS.md`.
- Работать только в `workspace/wt/upstream-pr` на ветке `up/<ключ>` от `upstream/main`; основное дерево остаётся чистым.
- Указывать затронутые редакции (`RotWK only`, `both`, `BFME2 only`); при отличиях цитировать и `RW 0x…`, и `BFME2 0x…`.
- Не тянуть файлы форка (`docs/ennorath/*`, `workspace/*`, чужие строки `STOPS.md` / `CMakeLists`), не писать `#N` (только `fork issue N`).
- Всегда добавлять синтетический тест, который падает без правки; для новых клиентских `.cpp` обновлять `tools/sim/sim_policy.json`, при изменении `STOPS.md` — сортировать строки и прогонять `tools/stops_count.py --write`.
- Отмечать отправленные пункты в этом файле и в [PR_CANDIDATES.md](PR_CANDIDATES.md).
- **PR, меняющий сохранение (xfer)** (мейнтейнеры upstream 2026-11-26: повторяющиеся блокеры, без них PR получает NEEDS AUTHOR):
  1. версия снапшота поднята, и сохранения до правки грузятся — с тестом, который грузит настоящий старый payload (для замены `UnportedUpdateModule` — старое состояние v1 базы + счётчик вызовов);
  2. у каждого нового сохраняемого поля есть запись в закрытом наборе `tools/save`;
  3. в `engine/tests/test_module_coverage.cpp` обновлены счётчики ported / missing.
