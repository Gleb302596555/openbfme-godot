# Арнор: первоначальные наборы команд героев

[Рабочий реестр](FactionArnor.md) · Ennorath 1.9.8, архив `____enrth_data.big`. Список ниже охватывает семь героев из `BuildableHeroesMP` и найденные в их определениях наборы команд. Это частичная инвентаризация исходного архива: смена форм, призванные герои, наследование и точный результат монтирования ещё не исследованы. Наличие кнопки не означает работоспособность.

На каждую строку нужны условия появления и уровня, тип цели и отказа, проверка действия, расхода и перезарядки, эффектов, интерфейса и визуала в оригинале и порте. Статус всех строк — **INVENTORIED**, логика / UI / визуал — **TODO**. Общие команды движения и строя тоже включены, чтобы не потерять слот.

## Начальные стоимости героев

Значения из объявлений объектов в `data/ini/object/goodfaction/units/arnor/new/`; это исходные данные до проверки модификаторов и фактического найма. Время — исходное значение INI, единицу и преобразование сверить с парсером.

| Герой | Стоимость | Время | Статус |
|---|---:|---:|---|
| `ArnorCarthaen_mod` | 1100 | 25 | INVENTORIED; найм TODO |
| `ArnorAraphantNew` | 1900 | 30 | INVENTORIED; найм TODO |
| `ArnorAranarthNewest` | 2000 | 30 | INVENTORIED; найм TODO |
| `ArnorArveduiNew` | 3500 | 45 | INVENTORIED; найм TODO |
| `ArnorMalbethSeer` | 500 | 20 | INVENTORIED; найм TODO |
| `LossothHero` | 1300 | 30 | INVENTORIED; найм TODO |
| `BuccaMarish` | 250 | 15 | INVENTORIED; найм TODO |

## `ArnorCarthaen_mod`

- [ ] **ПРИОСТАНОВЛЕНО:** изучить все формы, уровни, способности и доступность; проверить через интерфейс и против оригинала.

### `ArnorCartheanNewCommandSet`

| Слот | Команда | Тип / SpecialPower | Статус |
|---:|---|---|---|
| 1 | `Command_ToggleStance` | `TOGGLE_STANCE` | INVENTORIED; TODO |
| 2 | `Command_CarthaenWeaponToggle` | `SPECIAL_POWER` / `SpecialAbilityBerserkerLightTorch` | INVENTORIED; TODO |
| 3 | `Command_SpawnOneSpyRanger` | `SPECIAL_POWER` / `SpawnLoneRangerSpyCarthaen` | INVENTORIED; TODO |
| 4 | `Command_SpecialAbilityCarthaenDeadlySniper` | `SPECIAL_POWER` / `SpecialAbilityDamrodHeroMode` | INVENTORIED; TODO |
| 5 | `Command_SpecialAbilityCarthaenEscape` | `SPECIAL_POWER` / `SpecialAbilityWormtongueEscape` | INVENTORIED; TODO |
| 6 | `Command_CarthaenFireArrow` | `SPECIAL_POWER` / `SpecialAbilityFakeLeadership` | INVENTORIED; TODO |
| 12 | `Command_CaptureBuilding` | `SPECIAL_POWER` / `SpecialAbilityCaptureBuilding` | INVENTORIED; TODO |
| 13 | `Command_AttackMove` | `ATTACK_MOVE` | INVENTORIED; TODO |
| 14 | `Command_Stop` | `STOP` | INVENTORIED; TODO |
| 16 | `Command_SetStanceBattle` | `SET_STANCE` | INVENTORIED; TODO |
| 17 | `Command_SetStanceAggressive` | `SET_STANCE` | INVENTORIED; TODO |
| 18 | `Command_SetStanceHoldGround` | `SET_STANCE` | INVENTORIED; TODO |

## `ArnorAraphantNew`

- [ ] **ПРИОСТАНОВЛЕНО:** изучить все формы, уровни, способности и доступность; проверить через интерфейс и против оригинала.

### `ArnorAraphantCommandSetNew`

| Слот | Команда | Тип / SpecialPower | Статус |
|---:|---|---|---|
| 1 | `Command_ToggleStance` | `TOGGLE_STANCE` | INVENTORIED; TODO |
| 2 | `Command_TogglePalantir1_Araphant` | `TOGGLE_WEAPONSET` | INVENTORIED; TODO |
| 3 | `Command_AraphantToggleMounted` | `SPECIAL_POWER` / `SpecialAbilityToggleMounted` | INVENTORIED; TODO |
| 4 | `Command_SpecialAbilityAraphantGunstdesKnigs` | `SPECIAL_POWER` / `SpecialAbilityKingsFavor` | INVENTORIED; TODO |
| 5 | `Command_SpawnArafantGuardiansPike` | `SPECIAL_POWER` / `SpawnArafantGuardians` | INVENTORIED; TODO |
| 6 | `Command_SpecialAbilityAraphantSegenNew` | `SPECIAL_POWER` / `SpecialAbilityAraphantSegenNew` | INVENTORIED; TODO |
| 7 | `Command_SpecialPowerAraphantStatSoldierAI` | `SPECIAL_POWER` / `SpecialPowerAraphantStatSoldier` | INVENTORIED; TODO |
| 8 | `Command_SpecialPowerAraphantStatPikemanAI` | `SPECIAL_POWER` / `SpecialAbilityFakeLeadership2` | INVENTORIED; TODO |
| 9 | `Command_SpecialAbilityAraphantDefenceAI` | `SPECIAL_POWER` / `SpecialAbilityAraphantDefenceAI` | INVENTORIED; TODO |
| 10 | `Command_SpecialPowerAraphantStatArcherAI` | `SPECIAL_POWER` / `SpecialPowerAraphantStatArcher` | INVENTORIED; TODO |
| 12 | `Command_CaptureBuilding` | `SPECIAL_POWER` / `SpecialAbilityCaptureBuilding` | INVENTORIED; TODO |
| 13 | `Command_AttackMove` | `ATTACK_MOVE` | INVENTORIED; TODO |
| 14 | `Command_Stop` | `STOP` | INVENTORIED; TODO |
| 16 | `Command_SetStanceBattle` | `SET_STANCE` | INVENTORIED; TODO |
| 17 | `Command_SetStanceAggressive` | `SET_STANCE` | INVENTORIED; TODO |
| 18 | `Command_SetStanceHoldGround` | `SET_STANCE` | INVENTORIED; TODO |

## `ArnorAranarthNewest`

- [ ] **ПРИОСТАНОВЛЕНО:** изучить все формы, уровни, способности и доступность; проверить через интерфейс и против оригинала.

### `ArnorAranarthNewCommandSetArcher`

| Слот | Команда | Тип / SpecialPower | Статус |
|---:|---|---|---|
| 1 | `Command_ToggleStance` | `TOGGLE_STANCE` | INVENTORIED; TODO |
| 2 | `Command_AranarthToggleMounted` | `SPECIAL_POWER` / `SpecialAbilityToggleMounted` | INVENTORIED; TODO |
| 3 | `Command_SpecialAbilityWoundArrowAranarth` | `SPECIAL_POWER` / `SpecialAbilityGamlingFireArrow` | INVENTORIED; TODO |
| 4 | `Command_AranarthLongShotAbility` | `SPECIAL_POWER` / `SpecialAbilityAranarthLongShot` | INVENTORIED; TODO |
| 5 | `Command_SpawnAranarthGuardiansArch` | `SPECIAL_POWER` / `SpawnAranarthGuardians` | INVENTORIED; TODO |
| 6 | `Command_SpecialArveduiGuardians` | `SPECIAL_POWER` / `SpecialAbilityAranartGuards` | INVENTORIED; TODO |
| 12 | `Command_CaptureBuilding` | `SPECIAL_POWER` / `SpecialAbilityCaptureBuilding` | INVENTORIED; TODO |
| 13 | `Command_AttackMove` | `ATTACK_MOVE` | INVENTORIED; TODO |
| 14 | `Command_Stop` | `STOP` | INVENTORIED; TODO |
| 16 | `Command_SetStanceBattle` | `SET_STANCE` | INVENTORIED; TODO |
| 17 | `Command_SetStanceAggressive` | `SET_STANCE` | INVENTORIED; TODO |
| 18 | `Command_SetStanceHoldGround` | `SET_STANCE` | INVENTORIED; TODO |

### `ArnorAranarthNewCommandSetMalbeth`

| Слот | Команда | Тип / SpecialPower | Статус |
|---:|---|---|---|
| 1 | `Command_ToggleStance` | `TOGGLE_STANCE` | INVENTORIED; TODO |
| 2 | `Command_AranarthToggleMounted` | `SPECIAL_POWER` / `SpecialAbilityToggleMounted` | INVENTORIED; TODO |
| 3 | `Command_SpecialAbilityWoundArrowAranarth` | `SPECIAL_POWER` / `SpecialAbilityGamlingFireArrow` | INVENTORIED; TODO |
| 4 | `Command_AranarthLongShotAbilityImp` | `SPECIAL_POWER` / `SpecialAbilityAranarthLongShotImp` | INVENTORIED; TODO |
| 5 | `Command_SpawnAranarthGuardiansArch` | `SPECIAL_POWER` / `SpawnAranarthGuardians` | INVENTORIED; TODO |
| 6 | `Command_SpecialArveduiGuardians` | `SPECIAL_POWER` / `SpecialAbilityAranartGuards` | INVENTORIED; TODO |
| 12 | `Command_CaptureBuilding` | `SPECIAL_POWER` / `SpecialAbilityCaptureBuilding` | INVENTORIED; TODO |
| 13 | `Command_AttackMove` | `ATTACK_MOVE` | INVENTORIED; TODO |
| 14 | `Command_Stop` | `STOP` | INVENTORIED; TODO |
| 16 | `Command_SetStanceBattle` | `SET_STANCE` | INVENTORIED; TODO |
| 17 | `Command_SetStanceAggressive` | `SET_STANCE` | INVENTORIED; TODO |
| 18 | `Command_SetStanceHoldGround` | `SET_STANCE` | INVENTORIED; TODO |

## `ArnorArveduiNew`

- [ ] **ПРИОСТАНОВЛЕНО:** изучить все формы, уровни, способности и доступность; проверить через интерфейс и против оригинала.

### `ArnorArveduiModCommandSetNew`

| Слот | Команда | Тип / SpecialPower | Статус |
|---:|---|---|---|
| 1 | `Command_ToggleStance` | `TOGGLE_STANCE` | INVENTORIED; TODO |
| 2 | `Command_ArveduiLeadership` | `SPECIAL_POWER` / `SpecialAbilityFakeLeadership` | INVENTORIED; TODO |
| 3 | `Command_SpecialAbilityArveduiSlamNew` | `SPECIAL_POWER` / `SpecialAbilityArveduiSlamNew` | INVENTORIED; TODO |
| 4 | `Command_SpecialAbilityArveduiSpyPalantirNew` | `SPECIAL_POWER` / `SpecialAbilityArveduiPalantirNew` | INVENTORIED; TODO |
| 5 | `Command_SpecialAbilityPalantirBlast` | `SPECIAL_POWER` / `SpecialAbilityMuhadHorn` | INVENTORIED; TODO |
| 6 | `Command_SpecialAbilityArveduiExplosion` | `SPECIAL_POWER` / `SpecialAbilityArveduiRockFiring` | INVENTORIED; TODO |
| 8 | `Command_ToggleNekromantGestaltenSauron` | `SPECIAL_POWER` / `SpecialAbilityToggleSauronGestaltenSauron` | INVENTORIED; TODO |
| 9 | `Command_ToggleArveduiGoodRing` | `SPECIAL_POWER` / `SpecialAbilityToggleSarumanRing` | INVENTORIED; TODO |
| 12 | `Command_CaptureBuilding` | `SPECIAL_POWER` / `SpecialAbilityCaptureBuilding` | INVENTORIED; TODO |
| 13 | `Command_AttackMove` | `ATTACK_MOVE` | INVENTORIED; TODO |
| 14 | `Command_Stop` | `STOP` | INVENTORIED; TODO |
| 16 | `Command_SetStanceBattle` | `SET_STANCE` | INVENTORIED; TODO |
| 17 | `Command_SetStanceAggressive` | `SET_STANCE` | INVENTORIED; TODO |
| 18 | `Command_SetStanceHoldGround` | `SET_STANCE` | INVENTORIED; TODO |

### `ArnorArveduiModCommandSetNewMal`

| Слот | Команда | Тип / SpecialPower | Статус |
|---:|---|---|---|
| 1 | `Command_ToggleStance` | `TOGGLE_STANCE` | INVENTORIED; TODO |
| 2 | `Command_ArveduiToggleMounted` | `SPECIAL_POWER` / `SpecialAbilityToggleMounted` | INVENTORIED; TODO |
| 3 | `Command_SpecialAbilityArveduiSlamNew` | `SPECIAL_POWER` / `SpecialAbilityArveduiSlamNew` | INVENTORIED; TODO |
| 4 | `Command_SpecialAbilityArveduiSpyPalantirNew` | `SPECIAL_POWER` / `SpecialAbilityArveduiPalantirNew` | INVENTORIED; TODO |
| 5 | `Command_SpecialAbilityPalantirBlast` | `SPECIAL_POWER` / `SpecialAbilityMuhadHorn` | INVENTORIED; TODO |
| 6 | `Command_SpecialAbilityArveduiExplosion` | `SPECIAL_POWER` / `SpecialAbilityArveduiRockFiring` | INVENTORIED; TODO |
| 8 | `Command_ToggleNekromantGestaltenSauron` | `SPECIAL_POWER` / `SpecialAbilityToggleSauronGestaltenSauron` | INVENTORIED; TODO |
| 9 | `Command_ToggleArveduiGoodRing` | `SPECIAL_POWER` / `SpecialAbilityToggleSarumanRing` | INVENTORIED; TODO |
| 12 | `Command_CaptureBuilding` | `SPECIAL_POWER` / `SpecialAbilityCaptureBuilding` | INVENTORIED; TODO |
| 13 | `Command_AttackMove` | `ATTACK_MOVE` | INVENTORIED; TODO |
| 14 | `Command_Stop` | `STOP` | INVENTORIED; TODO |
| 16 | `Command_SetStanceBattle` | `SET_STANCE` | INVENTORIED; TODO |
| 17 | `Command_SetStanceAggressive` | `SET_STANCE` | INVENTORIED; TODO |
| 18 | `Command_SetStanceHoldGround` | `SET_STANCE` | INVENTORIED; TODO |

## `ArnorMalbethSeer`

- [ ] **ПРИОСТАНОВЛЕНО:** изучить все формы, уровни, способности и доступность; проверить через интерфейс и против оригинала.

### `ArnorMalbethCommandSetHero`

| Слот | Команда | Тип / SpecialPower | Статус |
|---:|---|---|---|
| 1 | `Command_ToggleStance` | `TOGGLE_STANCE` | INVENTORIED; TODO |
| 2 | `Command_MalbethSystem` | `SPECIAL_POWER` / `SpecialAbilityFakeLeadership2` | INVENTORIED; TODO |
| 3 | `Command_SpecialAbilityMalbethPalantir` | `SPECIAL_POWER` / `SpecialAbilityMalbethPalantir` | INVENTORIED; TODO |
| 4 | `Command_SpecialAbilityMalbethOnFarm` | `SPECIAL_POWER` / `SpecialAbilityMalbethFarmBuff` | INVENTORIED; TODO |
| 5 | `Command_SpecialAbilityMalbethProphecy` | `SPECIAL_POWER` / `SpecialAbilityMalbethProphecy` | INVENTORIED; TODO |
| 6 | `Command_SpecialAbilityMalbethTactics` | `SPECIAL_POWER` / `MalbethTacticsSpecialPower` | INVENTORIED; TODO |
| 8 | `Command_SpecialAbilityMalbethProphecyAI` | `SPECIAL_POWER` / `SpecialAbilityMalbethProphecyAI` | INVENTORIED; TODO |
| 12 | `Command_CaptureBuilding` | `SPECIAL_POWER` / `SpecialAbilityCaptureBuilding` | INVENTORIED; TODO |
| 13 | `Command_AttackMove` | `ATTACK_MOVE` | INVENTORIED; TODO |
| 14 | `Command_Stop` | `STOP` | INVENTORIED; TODO |
| 16 | `Command_SetStanceBattle` | `SET_STANCE` | INVENTORIED; TODO |
| 17 | `Command_SetStanceAggressive` | `SET_STANCE` | INVENTORIED; TODO |
| 18 | `Command_SetStanceHoldGround` | `SET_STANCE` | INVENTORIED; TODO |

## `LossothHero`

- [ ] **ПРИОСТАНОВЛЕНО:** изучить все формы, уровни, способности и доступность; проверить через интерфейс и против оригинала.

### `ArnorLossothHeroCommandSet`

| Слот | Команда | Тип / SpecialPower | Статус |
|---:|---|---|---|
| 1 | `Command_ToggleStance` | `TOGGLE_STANCE` | INVENTORIED; TODO |
| 2 | `Command_LossothLeadership` | `SPECIAL_POWER` / `SpecialAbilityFakeLeadership2` | INVENTORIED; TODO |
| 3 | `Command_SpawnLossothFrostTrap` | `SPECIAL_POWER` / `SpawnUndulosPoisonTrap` | INVENTORIED; TODO |
| 4 | `Command_LossothSurviveAbilityHealNew` | `SPECIAL_POWER` / `SpecialAbilityUndulusAthelas` | INVENTORIED; TODO |
| 5 | `Command_SpawnLossothBearTrap` | `SPECIAL_POWER` / `SpawnLossothBearTrap` | INVENTORIED; TODO |
| 6 | `Command_SpecialAbilitySummonLossothCamp` | `SPECIAL_POWER` / `SpecialAbilityLossothSummonCamp` | INVENTORIED; TODO |
| 12 | `Command_CaptureBuilding` | `SPECIAL_POWER` / `SpecialAbilityCaptureBuilding` | INVENTORIED; TODO |
| 13 | `Command_AttackMove` | `ATTACK_MOVE` | INVENTORIED; TODO |
| 15 | `Command_Stop` | `STOP` | INVENTORIED; TODO |
| 16 | `Command_SetStanceBattle` | `SET_STANCE` | INVENTORIED; TODO |
| 17 | `Command_SetStanceAggressive` | `SET_STANCE` | INVENTORIED; TODO |
| 18 | `Command_SetStanceHoldGround` | `SET_STANCE` | INVENTORIED; TODO |

## `BuccaMarish`

- [ ] **ПРИОСТАНОВЛЕНО:** изучить все формы, уровни, способности и доступность; проверить через интерфейс и против оригинала.

### `ArnorHobbitHeroCommandSet`

| Слот | Команда | Тип / SpecialPower | Статус |
|---:|---|---|---|
| 1 | `Command_ToggleStance` | `TOGGLE_STANCE` | INVENTORIED; TODO |
| 2 | `Command_ToggleHobbitRockThrowSkokka` | `TOGGLE_WEAPONSET` | INVENTORIED; TODO |
| 3 | `Command_SpecialAbilityElfCloakSkokka` | `SPECIAL_POWER` / `SpecialAbilityElfCloak` | INVENTORIED; TODO |
| 4 | `Command_GiveMeTheMoneyBitch` | `NONE` | INVENTORIED; TODO |
| 5 | `Command_SkokkaLeadership` | `SPECIAL_POWER` / `SpecialAbilityFakeLeadership2` | INVENTORIED; TODO |
| 6 | `Command_HobbitSongs` | `SPECIAL_POWER` / `SpecialAbilityArnorHobbitSong` | INVENTORIED; TODO |
| 7 | `Command_HobbitSongsAI` | `SPECIAL_POWER` / `SpecialAbilityArnorHobbitSongAI` | INVENTORIED; TODO |
| 12 | `Command_CaptureBuilding` | `SPECIAL_POWER` / `SpecialAbilityCaptureBuilding` | INVENTORIED; TODO |
| 13 | `Command_AttackMove` | `ATTACK_MOVE` | INVENTORIED; TODO |
| 14 | `Command_Stop` | `STOP` | INVENTORIED; TODO |
| 16 | `Command_SetStanceBattle` | `SET_STANCE` | INVENTORIED; TODO |
| 17 | `Command_SetStanceAggressive` | `SET_STANCE` | INVENTORIED; TODO |
| 18 | `Command_SetStanceHoldGround` | `SET_STANCE` | INVENTORIED; TODO |

## Дополнительные определения форм в тех же файлах

Найдены в файлах `arnoraranarthnew.ini` и `arnorarveduinew.ini`; условия перехода из нанимаемого героя и достижимость этих форм пока не установлены. Смена формы должна быть проверена в игре, а не предполагаться по имени. Все строки **INVENTORIED**, поведение **TODO**.

### `ArnorAranarthNew` → `ArnorAranarthNewCommandSet`

| Слот | Команда | Тип / SpecialPower | Статус |
|---:|---|---|---|
| 1 | `Command_ToggleStance` | `TOGGLE_STANCE` | INVENTORIED; TODO |
| 2 | `Command_ToggleAranarthWeaponReal` | `TOGGLE_WEAPONSET` | INVENTORIED; TODO |
| 3 | `Command_AranarthToggleMounted` | `SPECIAL_POWER` / `SpecialAbilityToggleMounted` | INVENTORIED; TODO |
| 4 | `Command_SpecialAbilityAranarthBackstab` | `SPECIAL_POWER` / `SpecialAbilityGorbagBackstab` | INVENTORIED; TODO |
| 5 | `Command_AranarthLongShotAbility` | `SPECIAL_POWER` / `SpecialAbilityAranarthLongShot` | INVENTORIED; TODO |
| 6 | `Command_SpecialArveduiGuardians` | `SPECIAL_POWER` / `SpecialAbilityAranartGuards` | INVENTORIED; TODO |
| 7 | `Command_AranarthLongShotAbilityImpAI` | `SPECIAL_POWER` / `SpecialAbilityAranarthLongShotImp` | INVENTORIED; TODO |
| 8 | `Command_SpecialAbilityGrishnackBackstabAI` | `SPECIAL_POWER` / `SpecialAbilityGorbagBackstabAI` | INVENTORIED; TODO |
| 9 | `Command_AranarthToggleMountedAI` | `SPECIAL_POWER` / `SpecialAbilityToggleMountedTwo` | INVENTORIED; TODO |
| 12 | `Command_CaptureBuilding` | `SPECIAL_POWER` / `SpecialAbilityCaptureBuilding` | INVENTORIED; TODO |
| 13 | `Command_AttackMove` | `ATTACK_MOVE` | INVENTORIED; TODO |
| 14 | `Command_Stop` | `STOP` | INVENTORIED; TODO |
| 16 | `Command_SetStanceBattle` | `SET_STANCE` | INVENTORIED; TODO |
| 17 | `Command_SetStanceAggressive` | `SET_STANCE` | INVENTORIED; TODO |
| 18 | `Command_SetStanceHoldGround` | `SET_STANCE` | INVENTORIED; TODO |

### `ArnorAranarthNewMounted` → `ArnorAranarthNewCommandSet_Mounted`

| Слот | Команда | Тип / SpecialPower | Статус |
|---:|---|---|---|
| 1 | `Command_ToggleStance` | `TOGGLE_STANCE` | INVENTORIED; TODO |
| 2 | `Command_ToggleAranarthWeaponReal` | `TOGGLE_WEAPONSET` | INVENTORIED; TODO |
| 3 | `Command_AranarthDisMounted` | `SPECIAL_POWER` / `SpecialAbilityToggleMounted` | INVENTORIED; TODO |
| 4 | `Command_SpecialAbilityAranarthBackstab` | `SPECIAL_POWER` / `SpecialAbilityGorbagBackstab` | INVENTORIED; TODO |
| 5 | `Command_AranarthLongShotAbility` | `SPECIAL_POWER` / `SpecialAbilityAranarthLongShot` | INVENTORIED; TODO |
| 6 | `Command_SpecialArveduiGuardians` | `SPECIAL_POWER` / `SpecialAbilityAranartGuards` | INVENTORIED; TODO |
| 7 | `Command_AranarthLongShotAbilityImpAI` | `SPECIAL_POWER` / `SpecialAbilityAranarthLongShotImp` | INVENTORIED; TODO |
| 9 | `Command_ToggleAranarthWeaponRealAI` | `TOGGLE_WEAPONSET` | INVENTORIED; TODO |
| 10 | `Command_SpecialAbilityWoundArrowAranarthAI` | `SPECIAL_POWER` / `SpecialAbilityGamlingFireArrowAI` | INVENTORIED; TODO |
| 12 | `Command_CaptureBuilding` | `SPECIAL_POWER` / `SpecialAbilityCaptureBuilding` | INVENTORIED; TODO |
| 13 | `Command_AttackMove` | `ATTACK_MOVE` | INVENTORIED; TODO |
| 14 | `Command_Stop` | `STOP` | INVENTORIED; TODO |
| 16 | `Command_SetStanceBattle` | `SET_STANCE` | INVENTORIED; TODO |
| 17 | `Command_SetStanceAggressive` | `SET_STANCE` | INVENTORIED; TODO |
| 18 | `Command_SetStanceHoldGround` | `SET_STANCE` | INVENTORIED; TODO |

### `ArnorArveduiEvilRing` → `ArnorArveduiModCommandSetEvil`

| Слот | Команда | Тип / SpecialPower | Статус |
|---:|---|---|---|
| 1 | `Command_ToggleStance` | `TOGGLE_STANCE` | INVENTORIED; TODO |
| 2 | `Command_ArveduiLeadershipEvil` | `SPECIAL_POWER` / `SpecialAbilityFakeLeadership` | INVENTORIED; TODO |
| 3 | `Command_SpecialAbilityArveduiSlamEvil` | `SPECIAL_POWER` / `SpecialAbilityElendil2` | INVENTORIED; TODO |
| 4 | `Command_SpecialAbilityArveduiEscape` | `SPECIAL_POWER` / `SpecialAbilityArveduiEscape` | INVENTORIED; TODO |
| 5 | `Command_SpecialAbilityPalantirBlastEvil` | `SPECIAL_POWER` / `SpecialAbilityMuhadHorn` | INVENTORIED; TODO |
| 6 | `Command_SpecialArveduiNumenorGuard` | `SPECIAL_POWER` / `SpecialAbilityElendil5` | INVENTORIED; TODO |
| 12 | `Command_CaptureBuilding` | `SPECIAL_POWER` / `SpecialAbilityCaptureBuilding` | INVENTORIED; TODO |
| 13 | `Command_AttackMove` | `ATTACK_MOVE` | INVENTORIED; TODO |
| 14 | `Command_Stop` | `STOP` | INVENTORIED; TODO |
| 16 | `Command_SetStanceBattle` | `SET_STANCE` | INVENTORIED; TODO |
| 17 | `Command_SetStanceAggressive` | `SET_STANCE` | INVENTORIED; TODO |
| 18 | `Command_SetStanceHoldGround` | `SET_STANCE` | INVENTORIED; TODO |

### `ArnorArveduiGoodRing` → `ArnorArveduiModCommandSetGood`

| Слот | Команда | Тип / SpecialPower | Статус |
|---:|---|---|---|
| 1 | `Command_ToggleStance` | `TOGGLE_STANCE` | INVENTORIED; TODO |
| 2 | `Command_ArveduiToggleMountedRing` | `SPECIAL_POWER` / `SpecialAbilityToggleMounted` | INVENTORIED; TODO |
| 3 | `Command_ArveduiRingBladeMaster` | `SPECIAL_POWER` / `SpecialAbilityAragornBladeMaster` | INVENTORIED; TODO |
| 4 | `Command_ArveduiLeadershipGoodRing` | `SPECIAL_POWER` / `SpecialAbilityFakeLeadership` | INVENTORIED; TODO |
| 5 | `Command_SpecialAbilityPalantirBlastGood` | `SPECIAL_POWER` / `SpecialAbilityMuhadHorn` | INVENTORIED; TODO |
| 6 | `Command_SpecialAbilityArveduiDeathArmy` | `SPECIAL_POWER` / `SpecialAbilityArveduiPersonalFlood` | INVENTORIED; TODO |
| 12 | `Command_CaptureBuilding` | `SPECIAL_POWER` / `SpecialAbilityCaptureBuilding` | INVENTORIED; TODO |
| 13 | `Command_AttackMove` | `ATTACK_MOVE` | INVENTORIED; TODO |
| 14 | `Command_Stop` | `STOP` | INVENTORIED; TODO |
| 16 | `Command_SetStanceBattle` | `SET_STANCE` | INVENTORIED; TODO |
| 17 | `Command_SetStanceAggressive` | `SET_STANCE` | INVENTORIED; TODO |
| 18 | `Command_SetStanceHoldGround` | `SET_STANCE` | INVENTORIED; TODO |

## Следующие проверки

- Развернуть все альтернативные формы и условия смены наборов команд по наследованию, апгрейдам, опыту и скриптам; найденные наборы не являются полным перечнем.
- Сопоставить SpecialPower с модулем выполнения в конкретной форме героя и проверить каждое действие, включая пассивные `FakeLeadership` и кнопки `NONE`. Некоторые команды имеют суффикс `AI`; выяснить, доступны ли они игроку.
- Сверить стоимость, время найма и максимальные лимиты по эффективному профилю; пройти все уровни, смерть и возвращение через обычный игровой путь.
