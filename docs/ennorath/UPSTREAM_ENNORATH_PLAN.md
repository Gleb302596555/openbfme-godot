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

### 2.1. Загрузчики, парсинг и рендер-база (7 файлов — приоритет №1)

Нужны, чтобы upstream корректно загружал текстуры, карты и эффекты мода:
- [ ] `GameClient/DdsDecode.cpp` + `GodotDevice/GodotDds.cpp` (нестандартные DDS и короткие mip-цепочки)
- [ ] `Common/BootSplash.cpp`
- [ ] `GameClient/GUI/AptScreens/AptMapPreview.cpp`
- [ ] `GameClient/FXListParseServices.cpp`
- [ ] `Common/LightPointSystem.cpp`
- [ ] `Libraries/WWVegas/WW3D2/lightenvironment.cpp`
- [ ] `Common/FunctionCurve.cpp`

*(Дополнительно проверить правки в уже существующих файлах загрузчиков: порядок `lang\*.big` / `russianpatch201.big`, усечённые W3D, RLE TGA, `RandomTexture` / `EnableHouseColor`.)*

### 2.2. Логические модули (`GameLogic/Module/*` + `AODHordeContain`, 44 класса)

Автономные ванильные классы — идеально уходят по 1 PR на модуль:

- **Апгрейды (6):**
  - [ ] `BaseUpgrade`
  - [ ] `GiveUpgradeUpdate` *(проверен сценарием мода)*
  - [ ] `LocomotorSetUpgrade`
  - [ ] `TooltipUpgrade`
  - [ ] `WallUpgradeUpdate`
  - [ ] `WeaponBonusUpgrade`
- **Спецспособности, контейнеры и орды (11):**
  - [ ] `AODCrushCollide` + `GameLogic/Object/Contain/AODHordeContain` + `OathbreakersFadeAwayBehavior`
  - [ ] `DeflectSpecialPower`
  - [ ] `DetachableRiderUpdate`
  - [ ] `FlingPassengerSpecialAbilityUpdate`
  - [ ] `HordeTransportContainDamage`
  - [ ] `HordeWorkerAIUpdate`
  - [ ] `SplitHordeSpecialPower`
  - [ ] `StoreObjectsSpecialPower`
  - [ ] `TeleportToCasterSpecialPower`
- **Урон, смерть и эффекты на поле (13):**
  - [ ] `ClearanceTestingSlowDeathGeometry`
  - [ ] `DamageFieldUpdate`
  - [ ] `DestroyEnvironmentUpdate`
  - [ ] `EvacuateDamage`
  - [ ] `FreeLifeBody`
  - [ ] `GiantBirdSlowDeathBehavior`
  - [ ] `OilSpillUpdate`
  - [ ] `PoisonedBehavior`
  - [ ] `ProjectileStreamUpdate`
  - [ ] `RainOfFireUpdate`
  - [ ] `ReflectDamage`
  - [ ] `ShipSlowDeathBehavior`
  - [ ] `StrafeAreaUpdate`
- **Поведения, спавн и прочее (14):**
  - [ ] `AimWeaponBehavior`
  - [ ] `AnimalAIUpdate`
  - [ ] `AnnounceBirthAndDeathBehavior`
  - [ ] `CivilianSpawnCollide` + `CivilianSpawnUpdate`
  - [ ] `DelayedLuaEventUpdate`
  - [ ] `DynamicShroudClearingRangeUpdate`
  - [ ] `EntEnragedUpdate`
  - [ ] `OCLUpdate`
  - [ ] `OneRingPenaltyUpdate`
  - [ ] `PartTheHeavensUpdate`
  - [ ] `ReplaceObjectUpdate`
  - [ ] `RunOffMapBehavior`
  - [ ] `SpecialEnemySenseUpdate`

### 2.3. Крупные системы логики и ИИ (10 файлов)

- [ ] `GameLogic/Map/FireLogicSystem.cpp` + `GameLogic/AI/AIBurningDeathState.cpp` (огонь на земле и горящая смерть)
- [ ] `GameLogic/AI/AIRampageState.cpp` (буйство)
- [ ] `GameLogic/AI/AIAttackCharge.cpp`
- [ ] `GameLogic/AI/AIAttackHordePosition.cpp`
- [ ] `GameLogic/AI/AIGroupOrders.cpp` + `GameClient/OrderLines.cpp` (очередь приказов и линии по Alt)
- [ ] `GameLogic/AI/AITNGuard.cpp`
- [ ] `GameLogic/Map/TerrainTaintClear.cpp` + `W3DDevice/GameClient/W3DTaintMask.cpp`
- [ ] `GameLogic/System/CrateSystem.cpp`
- [ ] `GameLogic/System/VictorySystem.cpp`

### 2.4. Клиентские Draw-модули, декали, эффекты и звук (21 файл)

*(Визуальные PR отправляем сначала без скриншотов; если мейнтейнеры upstream запросят — снимаем «до/после».)*

- **W3D Draw-модули (8):**
  - [ ] `W3DDebrisDraw`
  - [ ] `W3DLaserDraw` + `GameClient/LaserUpdate`
  - [ ] `W3DProjectileStreamDraw`
  - [ ] `W3DQuadrupedDraw`
  - [ ] `W3DSailModelDraw`
  - [ ] `W3DSupplyDraw`
  - [ ] `W3DTornadoDraw`
- **Декали на земле (4):**
  - [ ] `TerrainScorches` + `GodotTerrainScorches`
  - [ ] `TerrainTracks` + `GodotTerrainTracks`
- **Клиентские поведения, звук и тени (9):**
  - [ ] `ContainedPose`
  - [ ] `EvaAnnounceClientCreate` *(проверен сценарием мода)*
  - [ ] `EvaSighting` + `GameLogic/Object/EvaSightingNotice`
  - [ ] `Common/Audio/ScoredKillEvaAnnouncer`
  - [ ] `ModelConditionAudioLoopClientBehavior` *(проверен сценарием мода)*
  - [ ] `ParticleBoneFollow`
  - [ ] `ProjectedShadowManager`
  - [ ] `RadarMarkerClientUpdate`
  - [ ] `SegLineGeometry`
  - [ ] `SwayClientUpdate`

---

## Часть 3. Порядок выполнения (дорожная карта)

1. **Этап 0 — добить оставшийся красный CI в уже открытых PR:**
   - [ ] PR `#390` (`up/local-75` — падения `test_hero_facing_execution.cpp`)
   - [ ] PR `#437` (`up/local-132` — диагностировать и починить сборку на `upstream/main`)
2. **Этап 1 — загрузка ассетов мода (раздел 2.1):**
   - `DdsDecode` + `GodotDds` и базовые фиксы загрузчиков, чтобы upstream загружал текстуры/карты Ennorath без ошибок.
3. **Этап 2 — быстрые автономные ванильные модули (раздел 2.2):**
   - По 1 классу на PR, начиная с самых компактных и проверенных (`GiveUpgradeUpdate`, `BaseUpgrade`, `LocomotorSetUpgrade`, `WeaponBonusUpgrade`, `TooltipUpgrade`, `FreeLifeBody`, `EvacuateDamage`, `SplitHordeSpecialPower`, `PoisonedBehavior`, `ReflectDamage`, `StoreObjectsSpecialPower` + `TeleportToCasterSpecialPower`).
4. **Этап 3 — профиль Ennorath в `ModEngineFeatures` (раздел 1):**
   - Дождаться мёрджа upstream PR `#409` (чтобы не конфликтовать с регистрацией `fc32e4c4…` и переиспользовать общие ключи).
   - Отправить пачку маленьких pySAGE-фич из раздела **1А** (начиная с профиля `1.9.8.7` и 1-файловых фич `evacuate-containers`, `middle-click-camera`, `spell-store-delay`, `toggle-mounted-zero-speed`, `garrison-hero-survive`, `cant-earn-powerpoints`, `portrait-upgrade`, `filtered-horde-respawn`).
   - Затем по одному перенести крупные UI/реплей/WotR фичи из **1Б**.
5. **Этап 4 — крупные системы и рендер (разделы 2.3 и 2.4):**
   - `FireLogicSystem` + `AIBurningDeathState`, `AIRampageState`, `CrateSystem`, `VictorySystem`, W3D Draw-модули и декали.

---

## Часть 4. Чек-лист правил для каждого upstream PR

Полные правила: [PROCESS.md](PROCESS.md) §12 и `AGENTS.md`.
- Работать только в `workspace/wt/upstream-pr` на ветке `up/<ключ>` от `upstream/main`; основное дерево остаётся чистым.
- Указывать затронутые редакции (`RotWK only`, `both`, `BFME2 only`); при отличиях цитировать и `RW 0x…`, и `BFME2 0x…`.
- Не тянуть файлы форка (`docs/ennorath/*`, `workspace/*`, чужие строки `STOPS.md` / `CMakeLists`), не писать `#N` (только `fork issue N`).
- Всегда добавлять синтетический тест, который падает без правки; для новых клиентских `.cpp` обновлять `tools/sim/sim_policy.json`, при изменении `STOPS.md` — сортировать строки и прогонять `tools/stops_count.py --write`.
- Отмечать отправленные пункты в этом файле и в [PR_CANDIDATES.md](PR_CANDIDATES.md).
