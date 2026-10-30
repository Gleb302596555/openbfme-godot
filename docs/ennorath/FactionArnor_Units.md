# Арнор: производимые объекты из первоначальных меню

[Рабочий реестр](FactionArnor.md) · [строительные меню](FactionArnor_Buildings.md). Это только объекты, на которые непосредственно ссылаются команды `UNIT_BUILD` в 20 уже найденных наборах зданий. Призывы, скрытые ветви, орды/бойцы и альтернативные меню ещё предстоит добавить. Исходный архив Ennorath 1.9.8; доступность и соответствие исходной игре не проверены.

- [ ] **ПРИОСТАНОВЛЕНО:** раскрыть каждую орду до бойцов, все формы, команды, уровни, улучшения, здоровье/оружие/броню, источники найма и призыва; проверить в оригинале и порте.

| Объект | Начальное здание | Команда | Меню | Статус |
|---|---|---|---|---|
| `ArnorArcherHordeNew` | `ArnorBarracksNew` | `Command_ConstructArnorArcherHordeMod` | `ArnorBarracksCommandSet`, `ArnorBarracksCommandSetBanner400`, `ArnorBarracksCommandSetLevel2`, `ArnorBarracksCommandSetLevel2Banner400`, `ArnorBarracksCommandSetLevel3`, `ArnorBarracksCommandSetLevel3Banner400` | INVENTORIED; поведение TODO |
| `ArnorBatteringRamNew` | `ArnorWorkshopNew` | `Command_ConstructArnorBatteringRam` | `ArnorWorkshopCommandSet`, `ArnorWorkshopCommandSetLevel2`, `ArnorWorkshopCommandSetLevel3` | INVENTORIED; поведение TODO |
| `ArnorFighterHordeNew` | `ArnorBarracksNew` | `Command_ConstructArnorFighterHordeMod` | `ArnorBarracksCommandSet`, `ArnorBarracksCommandSetBanner400`, `ArnorBarracksCommandSetLevel2`, `ArnorBarracksCommandSetLevel2Banner400`, `ArnorBarracksCommandSetLevel3`, `ArnorBarracksCommandSetLevel3Banner400` | INVENTORIED; поведение TODO |
| `ArnorHarvester` | `ArnorStoneworkerMod` | `Command_BuildArnorHarvester` | `ArnorStoneWorkerCommandSetMod` | INVENTORIED; поведение TODO |
| `ArnorKnightHordeNew` | `ArnorStableNew` | `Command_ConstructArnorKnightHordeMod` | `ArnorStablesCommandSet`, `ArnorStablesCommandSetLevel2`, `ArnorStablesCommandSetLevel3` | INVENTORIED; поведение TODO |
| `ArnorPorter` | `ArnorFortress` | `Command_ConstructArnorPorter` | `ArnorFortressCommandSet` | INVENTORIED; поведение TODO |
| `ArnorTowerShieldGuardHordeNew` | `ArnorBarracksNew` | `Command_ConstructArnorTowerShieldGuardHordeMod` | `ArnorBarracksCommandSet`, `ArnorBarracksCommandSetBanner400`, `ArnorBarracksCommandSetLevel2`, `ArnorBarracksCommandSetLevel2Banner400`, `ArnorBarracksCommandSetLevel3`, `ArnorBarracksCommandSetLevel3Banner400` | INVENTORIED; поведение TODO |
| `ArnorTrebuchetNew` | `ArnorWorkshopNew` | `Command_ConstructArnorTrebuchet` | `ArnorWorkshopCommandSet`, `ArnorWorkshopCommandSetLevel2`, `ArnorWorkshopCommandSetLevel3` | INVENTORIED; поведение TODO |

## Состав найденных отрядов и начальные меню

Данные из соответствующих файлов `data/ini/object/goodfaction/units/arnor/new/` исходного архива. Число бойцов ниже — исходный `InitialPayload`, а не проверка живого отряда. Необходимы отдельные проверки пополнения, знамени, потерь и изменений состава.

| Отряд | Начальный боец и количество | Начальное меню | Статус |
|---|---|---|---|
| `ArnorArcherHordeNew` | `ArnorArcherNew` × 10 | `ArnorArcherHordeModCommandSet` | INVENTORIED; логика / UI TODO |
| `ArnorFighterHordeNew` | `ArnorFighterNew` × 10 | `ArnorFighterHordeModCommandSet` | INVENTORIED; логика / UI TODO |
| `ArnorKnightHordeNew` | `ArnorCavalryNew` × 10 | `ArnorKnightHordeCommandSetMod` | INVENTORIED; логика / UI TODO |
| `ArnorTowerShieldGuardHordeNew` | `ArnorTowerShieldGuardNew` × 10 | `ArnorPikeManHordeCommandSet` | INVENTORIED; логика / UI TODO |

Строитель `ArnorPorter` также содержит ссылки на `ArnorPorterArthedainStoneCommandSet`, `ArnorPorterCardolanStoneCommandSet`, `ArnorPorterRingStoneCommandSet` и `ArnorPorterModAICommandSet`. Условия переключения ещё не исследованы; содержимое записано в реестре зданий; поэтому перечень строительных объектов из первого меню неполон.

## Исходные меню прямого производства

Определения из `commandset.ini` / `includes/commandset.inc` и кнопок. Команды каждого набора, а также дополнительные формы и их переходы ещё нужно проверить в эффективном профиле и оригинале.

### `ArnorArcherHordeModCommandSet`

| Слот | Команда | Тип | Способность / улучшение | Статус |
|---:|---|---|---|---|
| 1 | `Command_ToggleStance` | `TOGGLE_STANCE` | — | INVENTORIED; TODO |
| 2 | `Command_ToggleFormationArnorArcherNew` | `HORDE_TOGGLE_FORMATION` | — | INVENTORIED; TODO |
| 3 | `Command_ArcherBombard` | `FIRE_WEAPON` | — | INVENTORIED; TODO |
| 4 | `Command_PurchaseUpgradeArnorFireArrowsMod` | `OBJECT_UPGRADE` | `Upgrade_GondorFireArrows` | INVENTORIED; TODO |
| 5 | `Command_PurchaseUpgradeArnorHeavyArmorMod` | `OBJECT_UPGRADE` | `Upgrade_GondorHeavyArmor` | INVENTORIED; TODO |
| 6 | `Command_PurchaseUpgradeArnorModBasicTraining` | `OBJECT_UPGRADE` | `Upgrade_GondorBasicTraining` | INVENTORIED; TODO |
| 12 | `Command_CaptureBuilding` | `SPECIAL_POWER` | `SpecialAbilityCaptureBuilding` | INVENTORIED; TODO |
| 13 | `Command_AttackMove` | `ATTACK_MOVE` | — | INVENTORIED; TODO |
| 14 | `Command_Stop` | `STOP` | — | INVENTORIED; TODO |
| 16 | `Command_SetStanceBattle` | `SET_STANCE` | — | INVENTORIED; TODO |
| 17 | `Command_SetStanceAggressive` | `SET_STANCE` | — | INVENTORIED; TODO |
| 18 | `Command_SetStanceHoldGround` | `SET_STANCE` | — | INVENTORIED; TODO |

### `ArnorFighterHordeModCommandSet`

| Слот | Команда | Тип | Способность / улучшение | Статус |
|---:|---|---|---|---|
| 1 | `Command_ToggleStance` | `TOGGLE_STANCE` | — | INVENTORIED; TODO |
| 2 | `Command_ArnorRevengeBonus` | `SPECIAL_POWER` | `SpecialAbilityFakeLeadership` | INVENTORIED; TODO |
| 4 | `Command_PurchaseUpgradeArnorForgedBladesMod` | `OBJECT_UPGRADE` | `Upgrade_GondorForgedBlades` | INVENTORIED; TODO |
| 5 | `Command_PurchaseUpgradeArnorHeavyArmorMod` | `OBJECT_UPGRADE` | `Upgrade_GondorHeavyArmor` | INVENTORIED; TODO |
| 6 | `Command_PurchaseUpgradeArnorModBasicTraining` | `OBJECT_UPGRADE` | `Upgrade_GondorBasicTraining` | INVENTORIED; TODO |
| 12 | `Command_CaptureBuilding` | `SPECIAL_POWER` | `SpecialAbilityCaptureBuilding` | INVENTORIED; TODO |
| 13 | `Command_AttackMove` | `ATTACK_MOVE` | — | INVENTORIED; TODO |
| 14 | `Command_Stop` | `STOP` | — | INVENTORIED; TODO |
| 16 | `Command_SetStanceBattle` | `SET_STANCE` | — | INVENTORIED; TODO |
| 17 | `Command_SetStanceAggressive` | `SET_STANCE` | — | INVENTORIED; TODO |
| 18 | `Command_SetStanceHoldGround` | `SET_STANCE` | — | INVENTORIED; TODO |

### `ArnorKnightHordeCommandSetMod`

| Слот | Команда | Тип | Способность / улучшение | Статус |
|---:|---|---|---|---|
| 1 | `Command_ToggleStance` | `TOGGLE_STANCE` | — | INVENTORIED; TODO |
| 2 | `Command_ToggleFormationArnKnightBlock` | `HORDE_TOGGLE_FORMATION` | — | INVENTORIED; TODO |
| 3 | `Command_PurchaseUpgradeArnorModKnightShield` | `OBJECT_UPGRADE` | `Upgrade_GondorKnightShield` | INVENTORIED; TODO |
| 4 | `Command_PurchaseUpgradeArnorForgedBladesMod` | `OBJECT_UPGRADE` | `Upgrade_GondorForgedBlades` | INVENTORIED; TODO |
| 5 | `Command_PurchaseUpgradeArnorHeavyArmorMod` | `OBJECT_UPGRADE` | `Upgrade_GondorHeavyArmor` | INVENTORIED; TODO |
| 6 | `Command_PurchaseUpgradeArnorModBasicTraining` | `OBJECT_UPGRADE` | `Upgrade_GondorBasicTraining` | INVENTORIED; TODO |
| 12 | `Command_CaptureBuilding` | `SPECIAL_POWER` | `SpecialAbilityCaptureBuilding` | INVENTORIED; TODO |
| 13 | `Command_AttackMove` | `ATTACK_MOVE` | — | INVENTORIED; TODO |
| 14 | `Command_Stop` | `STOP` | — | INVENTORIED; TODO |
| 16 | `Command_SetStanceBattle` | `SET_STANCE` | — | INVENTORIED; TODO |
| 17 | `Command_SetStanceAggressive` | `SET_STANCE` | — | INVENTORIED; TODO |
| 18 | `Command_SetStanceHoldGround` | `SET_STANCE` | — | INVENTORIED; TODO |

### `ArnorPikeManHordeCommandSet`

| Слот | Команда | Тип | Способность / улучшение | Статус |
|---:|---|---|---|---|
| 1 | `Command_ToggleStance` | `TOGGLE_STANCE` | — | INVENTORIED; TODO |
| 2 | `Command_ToggleFormationArnorPikeWall` | `HORDE_TOGGLE_FORMATION` | — | INVENTORIED; TODO |
| 4 | `Command_PurchaseUpgradeArnorForgedBladesMod` | `OBJECT_UPGRADE` | `Upgrade_GondorForgedBlades` | INVENTORIED; TODO |
| 5 | `Command_PurchaseUpgradeArnorHeavyArmorMod` | `OBJECT_UPGRADE` | `Upgrade_GondorHeavyArmor` | INVENTORIED; TODO |
| 6 | `Command_PurchaseUpgradeArnorModBasicTraining` | `OBJECT_UPGRADE` | `Upgrade_GondorBasicTraining` | INVENTORIED; TODO |
| 12 | `Command_CaptureBuilding` | `SPECIAL_POWER` | `SpecialAbilityCaptureBuilding` | INVENTORIED; TODO |
| 13 | `Command_AttackMove` | `ATTACK_MOVE` | — | INVENTORIED; TODO |
| 14 | `Command_Stop` | `STOP` | — | INVENTORIED; TODO |
| 16 | `Command_SetStanceBattle` | `SET_STANCE` | — | INVENTORIED; TODO |
| 17 | `Command_SetStanceAggressive` | `SET_STANCE` | — | INVENTORIED; TODO |
| 18 | `Command_SetStanceHoldGround` | `SET_STANCE` | — | INVENTORIED; TODO |

### `ArnorHarvesterCommandSet`

| Слот | Команда | Тип | Способность / улучшение | Статус |
|---:|---|---|---|---|
| 13 | `Command_AttackMove` | `ATTACK_MOVE` | — | INVENTORIED; TODO |
| 14 | `Command_Stop` | `STOP` | — | INVENTORIED; TODO |

### `ArnorBatteringRamCommandSet`

| Слот | Команда | Тип | Способность / улучшение | Статус |
|---:|---|---|---|---|
| 1 | `Command_ToggleStance` | `TOGGLE_STANCE` | — | INVENTORIED; TODO |
| 2 | `Command_PurchaseUpgradeArnorTaranArmor` | `OBJECT_UPGRADE` | `Upgrade_TaranArmor` | INVENTORIED; TODO |
| 12 | `Command_CaptureBuilding` | `SPECIAL_POWER` | `SpecialAbilityCaptureBuilding` | INVENTORIED; TODO |
| 13 | `Command_AttackMove` | `ATTACK_MOVE` | — | INVENTORIED; TODO |
| 14 | `Command_Stop` | `STOP` | — | INVENTORIED; TODO |
| 16 | `Command_SetStanceBattle` | `SET_STANCE` | — | INVENTORIED; TODO |
| 17 | `Command_SetStanceAggressive` | `SET_STANCE` | — | INVENTORIED; TODO |
| 18 | `Command_SetStanceHoldGround` | `SET_STANCE` | — | INVENTORIED; TODO |

### `ArnorTrebuchetCommandSet`

| Слот | Команда | Тип | Способность / улучшение | Статус |
|---:|---|---|---|---|
| 1 | `Command_ToggleStance` | `TOGGLE_STANCE` | — | INVENTORIED; TODO |
| 2 | `Command_ToggleTrebAutoAcquire` | `TOGGLE_NO_AUTO_ACQUIRE` | — | INVENTORIED; TODO |
| 3 | `Command_Bombard` | `FIRE_WEAPON` | — | INVENTORIED; TODO |
| 4 | `Command_PurchaseUpgradeArnorFireStones` | `OBJECT_UPGRADE` | `Upgrade_ArnorFireStones` | INVENTORIED; TODO |
| 13 | `Command_AttackMove` | `ATTACK_MOVE` | — | INVENTORIED; TODO |
| 14 | `Command_Stop` | `STOP` | — | INVENTORIED; TODO |
| 16 | `Command_SetStanceBattle` | `SET_STANCE` | — | INVENTORIED; TODO |
| 17 | `Command_SetStanceAggressive` | `SET_STANCE` | — | INVENTORIED; TODO |
| 18 | `Command_SetStanceHoldGround` | `SET_STANCE` | — | INVENTORIED; TODO |


## Дополнительные команды производства из меню лагерей

Источник: [меню лагерей](FactionArnor_Buildings.md). Достижимость и работа ещё не проверены; сначала раскрыть каждое состояние и связанные определения.

| Объект / улучшение | Тип команды | Здание / кнопка | Меню | Статус |
|---|---|---|---|---|
| `ArthedainCrossbowHorde` | `UNIT_BUILD` | `ArnorArthedainCamp` / `Command_ConstructArthedainCrossbowHorde` | `ArthedainCampCommandSet`, `ArthedainCampCommandSetUpThree`, `ArthedainCampCommandSetUpThreeSword`, `ArthedainCampCommandSetUpTwo`, `ArthedainCampCommandSetUpTwoSword`, `ArthedainCampCommandSetlvl2`, `ArthedainCampCommandSetlvl2AI` | INVENTORIED; TODO |
| `ArthedainCrossbowHorde_armr` | `UNIT_BUILD` | `ArnorArthedainCamp` / `Command_ConstructArthedainCrossbowHordeArmr` | `ArthedainCampCommandSetArmr`, `ArthedainCampCommandSetUpThreeArmor`, `ArthedainCampCommandSetlvl3`, `ArthedainCampCommandSetlvl3BothThree` | INVENTORIED; TODO |
| `ArthedainWarriorHorde` | `UNIT_BUILD` | `ArnorArthedainCamp` / `Command_ConstructArthedainWarrior` | `ArthedainCampCommandSet`, `ArthedainCampCommandSetlvl2AI` | INVENTORIED; TODO |
| `ArthedainWarriorHorde_Armr` | `UNIT_BUILD` | `ArnorArthedainCamp` / `Command_ConstructArthedainWarriorArmr` | `ArthedainCampCommandSetArmr`, `ArthedainCampCommandSetUpThreeArmor` | INVENTORIED; TODO |
| `ArthedainWarriorHorde_lvl2` | `UNIT_BUILD` | `ArnorArthedainCamp` / `Command_ConstructArthedainWarriorlvl2` | `ArthedainCampCommandSetUpThreeSword`, `ArthedainCampCommandSetUpTwoSword`, `ArthedainCampCommandSetlvl2` | INVENTORIED; TODO |
| `ArthedainWarriorHorde_lvl3` | `UNIT_BUILD` | `ArnorArthedainCamp` / `Command_ConstructArthedainWarriorlvl3` | `ArthedainCampCommandSetlvl3`, `ArthedainCampCommandSetlvl3BothThree` | INVENTORIED; TODO |
| `ArthedainWarriorHorde_n2` | `UNIT_BUILD` | `ArnorArthedainCamp` / `Command_ConstructArthedainWarriorN2` | `ArthedainCampCommandSetUpThree`, `ArthedainCampCommandSetUpTwo` | INVENTORIED; TODO |
| `BreeTownGuardHorde` | `UNIT_BUILD` | `ArnorArthedainCamp` / `Command_ConstructBreeTownGuard` | `ArthedainCampCommandSet`, `ArthedainCampCommandSetlvl2AI` | INVENTORIED; TODO |
| `BreeTownGuardHorde_Armr` | `UNIT_BUILD` | `ArnorArthedainCamp` / `Command_ConstructBreeTownGuardArmr` | `ArthedainCampCommandSetArmr`, `ArthedainCampCommandSetUpThreeArmor` | INVENTORIED; TODO |
| `BreeTownGuardHorde_lvl2` | `UNIT_BUILD` | `ArnorArthedainCamp` / `Command_ConstructBreeTownGuardlvl2` | `ArthedainCampCommandSetUpThreeSword`, `ArthedainCampCommandSetUpTwoSword`, `ArthedainCampCommandSetlvl2` | INVENTORIED; TODO |
| `BreeTownGuardHorde_lvl3` | `UNIT_BUILD` | `ArnorArthedainCamp` / `Command_ConstructBreeTownGuardlvl3` | `ArthedainCampCommandSetlvl3`, `ArthedainCampCommandSetlvl3BothThree` | INVENTORIED; TODO |
| `BreeTownGuardHorde_n2` | `UNIT_BUILD` | `ArnorArthedainCamp` / `Command_ConstructBreeTownGuardN2` | `ArthedainCampCommandSetUpThree`, `ArthedainCampCommandSetUpTwo` | INVENTORIED; TODO |
| `CardolanMountedRangerHorde` | `UNIT_BUILD` | `ArnorCardolanCamp` / `Command_ConstructCardolanMountedRanger` | `CardolanCampCommandSet` | INVENTORIED; TODO |
| `CardolanRangerHorde` | `UNIT_BUILD` | `ArnorCardolanCamp` / `Command_ConstructCardolanRanger` | `CardolanCampCommandSet` | INVENTORIED; TODO |
| `DunedainSpearmanHorde` | `UNIT_BUILD` | `ArnorCardolanCamp` / `Command_ConstructDunedainPikeman` | `CardolanCampCommandSet` | INVENTORIED; TODO |
