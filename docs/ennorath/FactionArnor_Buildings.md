# Арнор: первоначальные строительные меню и улучшения

[Рабочий реестр](FactionArnor.md). Ennorath 1.9.8, исходный архив `____enrth_data.big`; наборы найдены в объявлениях объектов и командах `ArnorPorterStoneMakerCommandSet`. Это предварительный список: наследование, доступность, все состояния и итоговые overrides ещё не доказаны. Команда/цель в таблице — ID из исходных определений, не подтверждение поведения. У каждой строки данные **INVENTORIED**, логика / UI / визуал **TODO**.

Начальные строительные команды: ферма → `ArnorFarmNew`, каменотёс → `ArnorStoneworkerMod`, казарма → `ArnorBarracksNew`, конюшня → `ArnorStableNew`, мастерская → `ArnorWorkshopNew`, башня → `ArnorBattleTowerNew`, колодец → `ArnorWellNew`, статуя → `ArnorStatueMod`, кузница → `ArnorForgeNew`, стена → `ArnorWallHubSmall`, крепость → `ArnorFortress`. Башня наследует `ArnorKeepNew`: её наследуемые команды ещё предстоит раскрыть.

## `ArnorFarmNew`

- [ ] **ПРИОСТАНОВЛЕНО:** проверить строительство, все уровни, переключения, производство и улучшения по циклу E–F.

### `ArnorFarmNewCommandSet`

| Слот | Команда | Тип | Объект / улучшение / способность | Статус |
|---:|---|---|---|---|
| 6 | `Command_Sell` | `SELL` | — | INVENTORIED; TODO |

## `ArnorStoneworkerMod`

- [ ] **ПРИОСТАНОВЛЕНО:** проверить строительство, все уровни, переключения, производство и улучшения по циклу E–F.

### `ArnorStoneWorkerCommandSetMod`

| Слот | Команда | Тип | Объект / улучшение / способность | Статус |
|---:|---|---|---|---|
| 1 | `Command_BuildArnorHarvester` | `UNIT_BUILD` | `ArnorHarvester` | INVENTORIED; TODO |
| 6 | `Command_Sell` | `SELL` | — | INVENTORIED; TODO |

## `ArnorBarracksNew`

- [ ] **ПРИОСТАНОВЛЕНО:** проверить строительство, все уровни, переключения, производство и улучшения по циклу E–F.

### `ArnorBarracksCommandSet`

| Слот | Команда | Тип | Объект / улучшение / способность | Статус |
|---:|---|---|---|---|
| 1 | `Command_ConstructArnorFighterHordeMod` | `UNIT_BUILD` | `ArnorFighterHordeNew` | INVENTORIED; TODO |
| 2 | `Command_ConstructArnorArcherHordeMod` | `UNIT_BUILD` | `ArnorArcherHordeNew` | INVENTORIED; TODO |
| 3 | `Command_ConstructArnorTowerShieldGuardHordeMod` | `UNIT_BUILD` | `ArnorTowerShieldGuardHordeNew` | INVENTORIED; TODO |
| 4 | `Command_PurchaseTechnologyArnorModBasicTraining` | `PLAYER_UPGRADE` | `Upgrade_TechnologyArnorBasicTraining` | INVENTORIED; TODO |
| 5 | `Command_PurchaseUpgradeArnorBarracksLevel2` | `OBJECT_UPGRADE` | `Upgrade_ArnorBarracksLevel2` | INVENTORIED; TODO |
| 6 | `Command_Sell` | `SELL` | — | INVENTORIED; TODO |

### `ArnorBarracksCommandSetLevel2`

| Слот | Команда | Тип | Объект / улучшение / способность | Статус |
|---:|---|---|---|---|
| 1 | `Command_ConstructArnorFighterHordeMod` | `UNIT_BUILD` | `ArnorFighterHordeNew` | INVENTORIED; TODO |
| 2 | `Command_ConstructArnorArcherHordeMod` | `UNIT_BUILD` | `ArnorArcherHordeNew` | INVENTORIED; TODO |
| 3 | `Command_ConstructArnorTowerShieldGuardHordeMod` | `UNIT_BUILD` | `ArnorTowerShieldGuardHordeNew` | INVENTORIED; TODO |
| 4 | `Command_PurchaseTechnologyArnorModBasicTraining` | `PLAYER_UPGRADE` | `Upgrade_TechnologyArnorBasicTraining` | INVENTORIED; TODO |
| 5 | `Command_PurchaseUpgradeArnorBarracksLevel3` | `OBJECT_UPGRADE` | `Upgrade_ArnorBarracksLevel3` | INVENTORIED; TODO |
| 6 | `Command_Sell` | `SELL` | — | INVENTORIED; TODO |

### `ArnorBarracksCommandSetLevel3`

| Слот | Команда | Тип | Объект / улучшение / способность | Статус |
|---:|---|---|---|---|
| 1 | `Command_ConstructArnorFighterHordeMod` | `UNIT_BUILD` | `ArnorFighterHordeNew` | INVENTORIED; TODO |
| 2 | `Command_ConstructArnorArcherHordeMod` | `UNIT_BUILD` | `ArnorArcherHordeNew` | INVENTORIED; TODO |
| 3 | `Command_ConstructArnorTowerShieldGuardHordeMod` | `UNIT_BUILD` | `ArnorTowerShieldGuardHordeNew` | INVENTORIED; TODO |
| 4 | `Command_PurchaseTechnologyArnorModBasicTraining` | `PLAYER_UPGRADE` | `Upgrade_TechnologyArnorBasicTraining` | INVENTORIED; TODO |
| 6 | `Command_Sell` | `SELL` | — | INVENTORIED; TODO |

### `ArnorBarracksCommandSetBanner400`

| Слот | Команда | Тип | Объект / улучшение / способность | Статус |
|---:|---|---|---|---|
| 1 | `Command_ConstructArnorFighterHordeMod` | `UNIT_BUILD` | `ArnorFighterHordeNew` | INVENTORIED; TODO |
| 2 | `Command_ConstructArnorArcherHordeMod` | `UNIT_BUILD` | `ArnorArcherHordeNew` | INVENTORIED; TODO |
| 3 | `Command_ConstructArnorTowerShieldGuardHordeMod` | `UNIT_BUILD` | `ArnorTowerShieldGuardHordeNew` | INVENTORIED; TODO |
| 4 | `Command_PurchaseTechnologyArnorModBasicTraining400` | `PLAYER_UPGRADE` | `Upgrade_TechnologyArthedainBasicTraining` | INVENTORIED; TODO |
| 5 | `Command_PurchaseUpgradeArnorBarracksLevel2` | `OBJECT_UPGRADE` | `Upgrade_ArnorBarracksLevel2` | INVENTORIED; TODO |
| 6 | `Command_Sell` | `SELL` | — | INVENTORIED; TODO |

### `ArnorBarracksCommandSetLevel2Banner400`

| Слот | Команда | Тип | Объект / улучшение / способность | Статус |
|---:|---|---|---|---|
| 1 | `Command_ConstructArnorFighterHordeMod` | `UNIT_BUILD` | `ArnorFighterHordeNew` | INVENTORIED; TODO |
| 2 | `Command_ConstructArnorArcherHordeMod` | `UNIT_BUILD` | `ArnorArcherHordeNew` | INVENTORIED; TODO |
| 3 | `Command_ConstructArnorTowerShieldGuardHordeMod` | `UNIT_BUILD` | `ArnorTowerShieldGuardHordeNew` | INVENTORIED; TODO |
| 4 | `Command_PurchaseTechnologyArnorModBasicTraining400` | `PLAYER_UPGRADE` | `Upgrade_TechnologyArthedainBasicTraining` | INVENTORIED; TODO |
| 5 | `Command_PurchaseUpgradeArnorBarracksLevel3` | `OBJECT_UPGRADE` | `Upgrade_ArnorBarracksLevel3` | INVENTORIED; TODO |
| 6 | `Command_Sell` | `SELL` | — | INVENTORIED; TODO |

### `ArnorBarracksCommandSetLevel3Banner400`

| Слот | Команда | Тип | Объект / улучшение / способность | Статус |
|---:|---|---|---|---|
| 1 | `Command_ConstructArnorFighterHordeMod` | `UNIT_BUILD` | `ArnorFighterHordeNew` | INVENTORIED; TODO |
| 2 | `Command_ConstructArnorArcherHordeMod` | `UNIT_BUILD` | `ArnorArcherHordeNew` | INVENTORIED; TODO |
| 3 | `Command_ConstructArnorTowerShieldGuardHordeMod` | `UNIT_BUILD` | `ArnorTowerShieldGuardHordeNew` | INVENTORIED; TODO |
| 4 | `Command_PurchaseTechnologyArnorModBasicTraining400` | `PLAYER_UPGRADE` | `Upgrade_TechnologyArthedainBasicTraining` | INVENTORIED; TODO |
| 6 | `Command_Sell` | `SELL` | — | INVENTORIED; TODO |

## `ArnorStableNew`

- [ ] **ПРИОСТАНОВЛЕНО:** проверить строительство, все уровни, переключения, производство и улучшения по циклу E–F.

### `ArnorStablesCommandSet`

| Слот | Команда | Тип | Объект / улучшение / способность | Статус |
|---:|---|---|---|---|
| 1 | `Command_ConstructArnorKnightHordeMod` | `UNIT_BUILD` | `ArnorKnightHordeNew` | INVENTORIED; TODO |
| 2 | `Command_PurchaseTechnologyArnorModKnightShield` | `PLAYER_UPGRADE` | `Upgrade_TechnologyGondorKnightShield` | INVENTORIED; TODO |
| 5 | `Command_PurchaseUpgradeArnorStableLevel2` | `OBJECT_UPGRADE` | `Upgrade_ArnorStableLevel2` | INVENTORIED; TODO |
| 6 | `Command_Sell` | `SELL` | — | INVENTORIED; TODO |

### `ArnorStablesCommandSetLevel2`

| Слот | Команда | Тип | Объект / улучшение / способность | Статус |
|---:|---|---|---|---|
| 1 | `Command_ConstructArnorKnightHordeMod` | `UNIT_BUILD` | `ArnorKnightHordeNew` | INVENTORIED; TODO |
| 2 | `Command_PurchaseTechnologyArnorModKnightShield` | `PLAYER_UPGRADE` | `Upgrade_TechnologyGondorKnightShield` | INVENTORIED; TODO |
| 5 | `Command_PurchaseUpgradeArnorStableLevel3` | `OBJECT_UPGRADE` | `Upgrade_ArnorStableLevel3` | INVENTORIED; TODO |
| 6 | `Command_Sell` | `SELL` | — | INVENTORIED; TODO |

### `ArnorStablesCommandSetLevel3`

| Слот | Команда | Тип | Объект / улучшение / способность | Статус |
|---:|---|---|---|---|
| 1 | `Command_ConstructArnorKnightHordeMod` | `UNIT_BUILD` | `ArnorKnightHordeNew` | INVENTORIED; TODO |
| 2 | `Command_PurchaseTechnologyArnorModKnightShield` | `PLAYER_UPGRADE` | `Upgrade_TechnologyGondorKnightShield` | INVENTORIED; TODO |
| 6 | `Command_Sell` | `SELL` | — | INVENTORIED; TODO |

## `ArnorWorkshopNew`

- [ ] **ПРИОСТАНОВЛЕНО:** проверить строительство, все уровни, переключения, производство и улучшения по циклу E–F.

### `ArnorWorkshopCommandSet`

| Слот | Команда | Тип | Объект / улучшение / способность | Статус |
|---:|---|---|---|---|
| 1 | `Command_ConstructArnorBatteringRam` | `UNIT_BUILD` | `ArnorBatteringRamNew` | INVENTORIED; TODO |
| 2 | `Command_ConstructArnorTrebuchet` | `UNIT_BUILD` | `ArnorTrebuchetNew` | INVENTORIED; TODO |
| 3 | `Command_PurchaseTechnologyArnorFireStones` | `PLAYER_UPGRADE` | `Upgrade_TechnologyArnorFireStones` | INVENTORIED; TODO |
| 4 | `Command_PurchaseUpgradeArnorWorkshopLevel2` | `OBJECT_UPGRADE` | `Upgrade_ArnorWorkshopLevel2` | INVENTORIED; TODO |
| 6 | `Command_Sell` | `SELL` | — | INVENTORIED; TODO |

### `ArnorWorkshopCommandSetLevel2`

| Слот | Команда | Тип | Объект / улучшение / способность | Статус |
|---:|---|---|---|---|
| 1 | `Command_ConstructArnorBatteringRam` | `UNIT_BUILD` | `ArnorBatteringRamNew` | INVENTORIED; TODO |
| 2 | `Command_ConstructArnorTrebuchet` | `UNIT_BUILD` | `ArnorTrebuchetNew` | INVENTORIED; TODO |
| 3 | `Command_PurchaseTechnologyArnorFireStones` | `PLAYER_UPGRADE` | `Upgrade_TechnologyArnorFireStones` | INVENTORIED; TODO |
| 4 | `Command_PurchaseUpgradeArnorWorkshopLevel3` | `OBJECT_UPGRADE` | `Upgrade_ArnorWorkshopLevel3` | INVENTORIED; TODO |
| 6 | `Command_Sell` | `SELL` | — | INVENTORIED; TODO |

### `ArnorWorkshopCommandSetLevel3`

| Слот | Команда | Тип | Объект / улучшение / способность | Статус |
|---:|---|---|---|---|
| 1 | `Command_ConstructArnorBatteringRam` | `UNIT_BUILD` | `ArnorBatteringRamNew` | INVENTORIED; TODO |
| 2 | `Command_ConstructArnorTrebuchet` | `UNIT_BUILD` | `ArnorTrebuchetNew` | INVENTORIED; TODO |
| 3 | `Command_PurchaseTechnologyArnorFireStones` | `PLAYER_UPGRADE` | `Upgrade_TechnologyArnorFireStones` | INVENTORIED; TODO |
| 6 | `Command_Sell` | `SELL` | — | INVENTORIED; TODO |

## `ArnorForgeNew`

- [ ] **ПРИОСТАНОВЛЕНО:** проверить строительство, все уровни, переключения, производство и улучшения по циклу E–F.

### `ArnorForgeCommandSetMod`

| Слот | Команда | Тип | Объект / улучшение / способность | Статус |
|---:|---|---|---|---|
| 1 | `Command_PurchaseTechnologyArnorForgedBladesMod` | `PLAYER_UPGRADE` | `Upgrade_TechnologyGondorForgedBlades` | INVENTORIED; TODO |
| 2 | `Command_PurchaseTechnologyArnorFireArrowsForge` | `PLAYER_UPGRADE` | `Upgrade_TechnologyGondorFireArrows` | INVENTORIED; TODO |
| 3 | `Command_PurchaseTechnologyArnorModHeavyArmor` | `PLAYER_UPGRADE` | `Upgrade_TechnologyGondorHeavyArmor` | INVENTORIED; TODO |
| 4 | `Command_PurchaseUpgradeNumenorSteel` | `PLAYER_UPGRADE` | `Upgrade_SchwerterderWesternis` | INVENTORIED; TODO |
| 5 | `Command_PurchaseUpgradeGondorForgeLevel2` | `OBJECT_UPGRADE` | `Upgrade_GondorForgeLevel2` | INVENTORIED; TODO |
| 6 | `Command_Sell` | `SELL` | — | INVENTORIED; TODO |

### `ArnorForgeCommandSetModLevel2`

| Слот | Команда | Тип | Объект / улучшение / способность | Статус |
|---:|---|---|---|---|
| 1 | `Command_PurchaseTechnologyArnorForgedBladesMod` | `PLAYER_UPGRADE` | `Upgrade_TechnologyGondorForgedBlades` | INVENTORIED; TODO |
| 2 | `Command_PurchaseTechnologyArnorFireArrowsForge` | `PLAYER_UPGRADE` | `Upgrade_TechnologyGondorFireArrows` | INVENTORIED; TODO |
| 3 | `Command_PurchaseTechnologyArnorModHeavyArmor` | `PLAYER_UPGRADE` | `Upgrade_TechnologyGondorHeavyArmor` | INVENTORIED; TODO |
| 4 | `Command_PurchaseUpgradeNumenorSteel` | `PLAYER_UPGRADE` | `Upgrade_SchwerterderWesternis` | INVENTORIED; TODO |
| 5 | `Command_PurchaseUpgradeGondorForgeLevel3` | `OBJECT_UPGRADE` | `Upgrade_GondorForgeLevel3` | INVENTORIED; TODO |
| 6 | `Command_Sell` | `SELL` | — | INVENTORIED; TODO |

### `ArnorForgeCommandSetModLevel3`

| Слот | Команда | Тип | Объект / улучшение / способность | Статус |
|---:|---|---|---|---|
| 1 | `Command_PurchaseTechnologyArnorForgedBladesMod` | `PLAYER_UPGRADE` | `Upgrade_TechnologyGondorForgedBlades` | INVENTORIED; TODO |
| 2 | `Command_PurchaseTechnologyArnorFireArrowsForge` | `PLAYER_UPGRADE` | `Upgrade_TechnologyGondorFireArrows` | INVENTORIED; TODO |
| 3 | `Command_PurchaseTechnologyArnorModHeavyArmor` | `PLAYER_UPGRADE` | `Upgrade_TechnologyGondorHeavyArmor` | INVENTORIED; TODO |
| 4 | `Command_PurchaseUpgradeNumenorSteel` | `PLAYER_UPGRADE` | `Upgrade_SchwerterderWesternis` | INVENTORIED; TODO |
| 6 | `Command_Sell` | `SELL` | — | INVENTORIED; TODO |

## `ArnorWellNew / ArnorStatueMod`

- [ ] **ПРИОСТАНОВЛЕНО:** проверить строительство, все уровни, переключения, производство и улучшения по циклу E–F.

### `SellableCommandSet`

| Слот | Команда | Тип | Объект / улучшение / способность | Статус |
|---:|---|---|---|---|
| 6 | `Command_Sell` | `SELL` | — | INVENTORIED; TODO |

## `ArnorWallHubSmall`

- [ ] **ПРИОСТАНОВЛЕНО:** проверить строительство, все уровни, переключения, производство и улучшения по циклу E–F.

### `ArnorCastleWallHubSmallCommandSet`

| Слот | Команда | Тип | Объект / улучшение / способность | Статус |
|---:|---|---|---|---|
| 1 | `Command_ArnorCastleWallHub_BeginWallSpan` | `DOZER_CONSTRUCT` | `ArnorWallHubSmall` | INVENTORIED; TODO |
| 4 | `Command_CancelWallSpan` | `CANCEL_NEIGHBORHOOD` | — | INVENTORIED; TODO |
| 6 | `Command_Sell` | `SELL` | — | INVENTORIED; TODO |

## `ArnorFortress`

- [ ] **ПРИОСТАНОВЛЕНО:** проверить строительство, все уровни, переключения, производство и улучшения по циклу E–F.

### `ArnorFortressCommandSet`

| Слот | Команда | Тип | Объект / улучшение / способность | Статус |
|---:|---|---|---|---|
| 1 | `Command_RingMechanikGondor_forFornost` | `SPECIAL_POWER` | `SpecialAbilityFakeLeadership` | INVENTORIED; TODO |
| 2 | `Command_PurchaseUpgradeImladrisAlly` | `PLAYER_UPGRADE` | `Upgrade_ArnorFortressImladrisAlly` | INVENTORIED; TODO |
| 3 | `Command_PurchaseUpgradeGondorAlly` | `PLAYER_UPGRADE` | `Upgrade_ArnorFortressGondorAlly` | INVENTORIED; TODO |
| 4 | `Command_SelectUpgradesArnorFaction` | `PUSH_VISIBLE_COMMAND_RANGE` | — | INVENTORIED; TODO |
| 5 | `Command_SelectRevivablesArnorFortress` | `PUSH_VISIBLE_COMMAND_RANGE` | — | INVENTORIED; TODO |
| 6 | `Command_Sell` | `SELL` | — | INVENTORIED; TODO |
| 7 | `Command_SpecialAbilityArnorPalantirEye` | `SPECIAL_POWER` | `SpecialAbilityArnorFortressIvoryTowerVision` | INVENTORIED; TODO |
| 8 | `Command_ConstructArnorPorter` | `UNIT_BUILD` | `ArnorPorter` | INVENTORIED; TODO |
| 9 | `Command_PurchaseUpgradeArnorFortressBanners` | `OBJECT_UPGRADE` | `Upgrade_ArnorFortressBanners` | INVENTORIED; TODO |
| 10 | `Command_PurchaseUpgradeArnorFortressFlamingMunitions` | `OBJECT_UPGRADE` | `Upgrade_GoodFortressFlamingMunitionsTrigger` | INVENTORIED; TODO |
| 11 | `Command_PurchaseUpgradeArnorFortressCrystalMoat` | `OBJECT_UPGRADE` | `Upgrade_ArnorFortressCrystalMoat` | INVENTORIED; TODO |
| 12 | `Command_PurchaseUpgradeArnorFortressNumenorStonework` | `OBJECT_UPGRADE` | `Upgrade_ArnorFortressNumenorStoneworkTrigger` | INVENTORIED; TODO |
| 13 | `Command_PurchaseUpgradeArnorOldKingStatues` | `OBJECT_UPGRADE` | `Upgrade_ArnorFortressHouseOfHealing` | INVENTORIED; TODO |
| 14 | `Command_PurchaseUpgradeArnorFortressPalantirEye` | `OBJECT_UPGRADE` | `Upgrade_ArnorFortressPalantirEye` | INVENTORIED; TODO |
| 15 | `Command_RadialBack` | `POP_VISIBLE_COMMAND_RANGE` | — | INVENTORIED; TODO |
| 16 | `Command_FakeRingHeroReviveSlot` | `REVIVE` | — | INVENTORIED; TODO |
| 17 | `Command_FakeCreateAHeroReviveSlot` | `REVIVE` | — | INVENTORIED; TODO |
| 18 | `Command_GenericReviveSlot1` | `REVIVE` | — | INVENTORIED; TODO |
| 19 | `Command_GenericReviveSlot2` | `REVIVE` | — | INVENTORIED; TODO |
| 20 | `Command_GenericReviveSlot3` | `REVIVE` | — | INVENTORIED; TODO |
| 21 | `Command_GenericReviveSlot4` | `REVIVE` | — | INVENTORIED; TODO |
| 22 | `Command_GenericReviveSlot5` | `REVIVE` | — | INVENTORIED; TODO |
| 23 | `Command_FakeHeroReviveSlot7Imrahill` | `REVIVE` | — | INVENTORIED; TODO |
| 24 | `Command_FakeHeroReviveSlot8Hobbit` | `REVIVE` | — | INVENTORIED; TODO |
| 25 | `Command_RadialBack` | `POP_VISIBLE_COMMAND_RANGE` | — | INVENTORIED; TODO |

## Следующие проверки

- Установить точные условия переходов каждого уровня, цены и время, эффекты на характеристики и разблокировку войск; отдельно проверить шесть состояний казармы и три состояния кузницы, конюшни и мастерской.
- Раскрыть производство до объектов/орд и их альтернативных форм, апгрейды до носителей и эффектов, все крепостные пристройки и захватываемые здания.
- Сверить меню с действующим профилем и оригинальной игрой; условия доступности и запуск каждой команды проверить в игре. Ветка башни требует обхода родительского объекта.

## Дополнительные меню строителя

Эти четыре набора упомянуты в `arnorporter.ini`; условие переключения ещё нужно проследить. Содержимое указано по локальному архиву, применимость к человеку/ИИ и реальная доступность не проверены.

### `ArnorPorterArthedainStoneCommandSet`

| Слот | Команда | Тип | Объект / улучшение / способность | Статус |
|---:|---|---|---|---|
| 1 | `Command_Stop` | `STOP` | — | INVENTORIED; TODO |
| 2 | `Command_PorterExtinguishFire` | `SPECIAL_POWER` | `SpecialAbilityPorterExtinguishFire` | INVENTORIED; TODO |
| 3 | `Command_Repair` | `SPECIAL_POWER` | `SpecialRepairStructure` | INVENTORIED; TODO |
| 4 | `Command_PorterConstructArnorFarm` | `DOZER_CONSTRUCT` | `ArnorFarmNew` | INVENTORIED; TODO |
| 5 | `Command_PorterConstructArnorStoneWorker` | `DOZER_CONSTRUCT` | `ArnorStoneworkerMod` | INVENTORIED; TODO |
| 6 | `Command_PorterConstructArnorBarracks` | `DOZER_CONSTRUCT` | `ArnorBarracksNew` | INVENTORIED; TODO |
| 7 | `Command_PorterConstructArthedainCamp` | `DOZER_CONSTRUCT` | `ArnorArthedainCamp` | INVENTORIED; TODO |
| 8 | `Command_PorterConstructArnorStable` | `DOZER_CONSTRUCT` | `ArnorStableNew` | INVENTORIED; TODO |
| 9 | `Command_PorterConstructArnorWorkshop` | `DOZER_CONSTRUCT` | `ArnorWorkshopNew` | INVENTORIED; TODO |
| 10 | `Command_PorterConstructArnorSentryTower` | `DOZER_CONSTRUCT` | `ArnorBattleTowerNew` | INVENTORIED; TODO |
| 11 | `Command_PorterConstructArnorWell` | `DOZER_CONSTRUCT` | `ArnorWellNew` | INVENTORIED; TODO |
| 12 | `Command_PorterConstructArnorStatue` | `DOZER_CONSTRUCT` | `ArnorStatueMod` | INVENTORIED; TODO |
| 13 | `Command_PorterConstructArnorBlacksmithMod` | `DOZER_CONSTRUCT` | `ArnorForgeNew` | INVENTORIED; TODO |
| 14 | `Command_PorterConstructArnorWallHub` | `DOZER_CONSTRUCT` | `ArnorWallHubSmall` | INVENTORIED; TODO |
| 15 | `Command_PorterConstructArnorFortress` | `DOZER_CONSTRUCT` | `ArnorFortress` | INVENTORIED; TODO |

### `ArnorPorterCardolanStoneCommandSet`

| Слот | Команда | Тип | Объект / улучшение / способность | Статус |
|---:|---|---|---|---|
| 1 | `Command_Stop` | `STOP` | — | INVENTORIED; TODO |
| 2 | `Command_PorterExtinguishFire` | `SPECIAL_POWER` | `SpecialAbilityPorterExtinguishFire` | INVENTORIED; TODO |
| 3 | `Command_Repair` | `SPECIAL_POWER` | `SpecialRepairStructure` | INVENTORIED; TODO |
| 4 | `Command_PorterConstructArnorFarm` | `DOZER_CONSTRUCT` | `ArnorFarmNew` | INVENTORIED; TODO |
| 5 | `Command_PorterConstructArnorStoneWorker` | `DOZER_CONSTRUCT` | `ArnorStoneworkerMod` | INVENTORIED; TODO |
| 6 | `Command_PorterConstructArnorBarracks` | `DOZER_CONSTRUCT` | `ArnorBarracksNew` | INVENTORIED; TODO |
| 7 | `Command_PorterConstructCardolanCamp` | `DOZER_CONSTRUCT` | `ArnorCardolanCamp` | INVENTORIED; TODO |
| 8 | `Command_PorterConstructArnorStable` | `DOZER_CONSTRUCT` | `ArnorStableNew` | INVENTORIED; TODO |
| 9 | `Command_PorterConstructArnorWorkshop` | `DOZER_CONSTRUCT` | `ArnorWorkshopNew` | INVENTORIED; TODO |
| 10 | `Command_PorterConstructArnorSentryTower` | `DOZER_CONSTRUCT` | `ArnorBattleTowerNew` | INVENTORIED; TODO |
| 11 | `Command_PorterConstructArnorWell` | `DOZER_CONSTRUCT` | `ArnorWellNew` | INVENTORIED; TODO |
| 12 | `Command_PorterConstructArnorStatue` | `DOZER_CONSTRUCT` | `ArnorStatueMod` | INVENTORIED; TODO |
| 13 | `Command_PorterConstructArnorBlacksmithMod` | `DOZER_CONSTRUCT` | `ArnorForgeNew` | INVENTORIED; TODO |
| 14 | `Command_PorterConstructArnorWallHub` | `DOZER_CONSTRUCT` | `ArnorWallHubSmall` | INVENTORIED; TODO |
| 15 | `Command_PorterConstructArnorFortress` | `DOZER_CONSTRUCT` | `ArnorFortress` | INVENTORIED; TODO |

### `ArnorPorterRingStoneCommandSet`

| Слот | Команда | Тип | Объект / улучшение / способность | Статус |
|---:|---|---|---|---|
| 1 | `Command_Stop` | `STOP` | — | INVENTORIED; TODO |
| 2 | `Command_PorterExtinguishFire` | `SPECIAL_POWER` | `SpecialAbilityPorterExtinguishFire` | INVENTORIED; TODO |
| 3 | `Command_Repair` | `SPECIAL_POWER` | `SpecialRepairStructure` | INVENTORIED; TODO |
| 4 | `Command_PorterConstructArnorWallHubRing` | `DOZER_CONSTRUCT` | `ArnorWallHubSmall` | INVENTORIED; TODO |
| 5 | `Command_PorterConstructArnorFarm` | `DOZER_CONSTRUCT` | `ArnorFarmNew` | INVENTORIED; TODO |
| 6 | `Command_PorterConstructArnorStoneWorker` | `DOZER_CONSTRUCT` | `ArnorStoneworkerMod` | INVENTORIED; TODO |
| 7 | `Command_PorterConstructArnorBarracks` | `DOZER_CONSTRUCT` | `ArnorBarracksNew` | INVENTORIED; TODO |
| 8 | `Command_PorterConstructCardolanCamp` | `DOZER_CONSTRUCT` | `ArnorCardolanCamp` | INVENTORIED; TODO |
| 9 | `Command_PorterConstructArthedainCamp` | `DOZER_CONSTRUCT` | `ArnorArthedainCamp` | INVENTORIED; TODO |
| 10 | `Command_PorterConstructArnorStable` | `DOZER_CONSTRUCT` | `ArnorStableNew` | INVENTORIED; TODO |
| 11 | `Command_PorterConstructArnorWorkshop` | `DOZER_CONSTRUCT` | `ArnorWorkshopNew` | INVENTORIED; TODO |
| 12 | `Command_PorterConstructArnorSentryTower` | `DOZER_CONSTRUCT` | `ArnorBattleTowerNew` | INVENTORIED; TODO |
| 13 | `Command_PorterConstructArnorWell` | `DOZER_CONSTRUCT` | `ArnorWellNew` | INVENTORIED; TODO |
| 14 | `Command_PorterConstructArnorStatue` | `DOZER_CONSTRUCT` | `ArnorStatueMod` | INVENTORIED; TODO |
| 15 | `Command_PorterConstructArnorBlacksmithMod` | `DOZER_CONSTRUCT` | `ArnorForgeNew` | INVENTORIED; TODO |
| 16 | `Command_PorterConstructArnorFortress` | `DOZER_CONSTRUCT` | `ArnorFortress` | INVENTORIED; TODO |

### `ArnorPorterModAICommandSet`

| Слот | Команда | Тип | Объект / улучшение / способность | Статус |
|---:|---|---|---|---|
| 1 | `Command_Stop` | `STOP` | — | INVENTORIED; TODO |
| 2 | `Command_PorterExtinguishFire` | `SPECIAL_POWER` | `SpecialAbilityPorterExtinguishFire` | INVENTORIED; TODO |
| 3 | `Command_Repair` | `SPECIAL_POWER` | `SpecialRepairStructure` | INVENTORIED; TODO |
| 4 | `Command_PorterConstructArnorWallHubAI` | `DOZER_CONSTRUCT` | `ArnorWallHubSmallAI` | INVENTORIED; TODO |
| 5 | `Command_PorterConstructArnorFarm` | `DOZER_CONSTRUCT` | `ArnorFarmNew` | INVENTORIED; TODO |
| 6 | `Command_PorterConstructArnorStoneWorker` | `DOZER_CONSTRUCT` | `ArnorStoneworkerMod` | INVENTORIED; TODO |
| 7 | `Command_PorterConstructArnorBarracks` | `DOZER_CONSTRUCT` | `ArnorBarracksNew` | INVENTORIED; TODO |
| 8 | `Command_PorterConstructCardolanCamp` | `DOZER_CONSTRUCT` | `ArnorCardolanCamp` | INVENTORIED; TODO |
| 9 | `Command_PorterConstructArthedainCamp` | `DOZER_CONSTRUCT` | `ArnorArthedainCamp` | INVENTORIED; TODO |
| 10 | `Command_PorterConstructArnorStable` | `DOZER_CONSTRUCT` | `ArnorStableNew` | INVENTORIED; TODO |
| 11 | `Command_PorterConstructArnorWorkshop` | `DOZER_CONSTRUCT` | `ArnorWorkshopNew` | INVENTORIED; TODO |
| 12 | `Command_PorterConstructArnorSentryTower` | `DOZER_CONSTRUCT` | `ArnorBattleTowerNew` | INVENTORIED; TODO |
| 13 | `Command_PorterConstructArnorWell` | `DOZER_CONSTRUCT` | `ArnorWellNew` | INVENTORIED; TODO |
| 14 | `Command_PorterConstructArnorStatue` | `DOZER_CONSTRUCT` | `ArnorStatueMod` | INVENTORIED; TODO |
| 15 | `Command_PorterConstructArnorBlacksmithMod` | `DOZER_CONSTRUCT` | `ArnorForgeNew` | INVENTORIED; TODO |
| 16 | `Command_PorterConstructMenFortress` | `DOZER_CONSTRUCT` | `MenFortress` | INVENTORIED; TODO |
| 17 | `Command_PorterConstructArnorFortressAI` | `DOZER_CONSTRUCT` | `ArnorFort` | INVENTORIED; TODO |


## Дополнительные лагеря и наследуемое меню башни

Строительные команды дополнительных меню указывают на лагеря Артэдайна и Кардолана. Ниже — наборы, найденные в определениях этих объектов и родителя башни. Условия выбора лагеря, доступность каждого набора и эффект апгрейдов требуют проверки. `ArnorFort` из меню ИИ пока не раскрыт.

### `ArnorArthedainCamp`

- [ ] **ПРИОСТАНОВЛЕНО:** раскрыть условия меню и проверить доступность и все команды.

#### `ArthedainCampCommandSet`

| Слот | Команда | Тип | Объект / улучшение / способность | Статус |
|---:|---|---|---|---|
| 1 | `Command_ConstructArthedainWarrior` | `UNIT_BUILD` | `ArthedainWarriorHorde` | INVENTORIED; TODO |
| 2 | `Command_ConstructBreeTownGuard` | `UNIT_BUILD` | `BreeTownGuardHorde` | INVENTORIED; TODO |
| 3 | `Command_ConstructArthedainCrossbowHorde` | `UNIT_BUILD` | `ArthedainCrossbowHorde` | INVENTORIED; TODO |
| 4 | `Command_PurchaseUpgradeArthedainCaptainsUnlock` | `PLAYER_UPGRADE` | `Upgrade_ArthedainCaptainsUnlock` | INVENTORIED; TODO |
| 5 | `Command_ArthedainBarraksPassive` | `SPECIAL_POWER` | `SpecialAbilityFakeLeadership2` | INVENTORIED; TODO |
| 6 | `Command_Sell` | `SELL` | — | INVENTORIED; TODO |
| 7 | `Command_PurchaseUpgradeArthCampLevel2` | `OBJECT_UPGRADE` | `Upgrade_ArthedainCampLevel2` | INVENTORIED; TODO |

#### `ArthedainCampCommandSetArmr`

| Слот | Команда | Тип | Объект / улучшение / способность | Статус |
|---:|---|---|---|---|
| 1 | `Command_ConstructArthedainWarriorArmr` | `UNIT_BUILD` | `ArthedainWarriorHorde_Armr` | INVENTORIED; TODO |
| 2 | `Command_ConstructBreeTownGuardArmr` | `UNIT_BUILD` | `BreeTownGuardHorde_Armr` | INVENTORIED; TODO |
| 3 | `Command_ConstructArthedainCrossbowHordeArmr` | `UNIT_BUILD` | `ArthedainCrossbowHorde_armr` | INVENTORIED; TODO |
| 4 | `Command_PurchaseUpgradeArthedainCaptainsUnlock` | `PLAYER_UPGRADE` | `Upgrade_ArthedainCaptainsUnlock` | INVENTORIED; TODO |
| 5 | `Command_ArthedainBarraksPassive` | `SPECIAL_POWER` | `SpecialAbilityFakeLeadership2` | INVENTORIED; TODO |
| 6 | `Command_Sell` | `SELL` | — | INVENTORIED; TODO |

#### `ArthedainCampCommandSetUpThree`

| Слот | Команда | Тип | Объект / улучшение / способность | Статус |
|---:|---|---|---|---|
| 1 | `Command_ConstructArthedainWarriorN2` | `UNIT_BUILD` | `ArthedainWarriorHorde_n2` | INVENTORIED; TODO |
| 2 | `Command_ConstructBreeTownGuardN2` | `UNIT_BUILD` | `BreeTownGuardHorde_n2` | INVENTORIED; TODO |
| 3 | `Command_ConstructArthedainCrossbowHorde` | `UNIT_BUILD` | `ArthedainCrossbowHorde` | INVENTORIED; TODO |
| 4 | `Command_PurchaseUpgradeArthedainCaptainsUnlock` | `PLAYER_UPGRADE` | `Upgrade_ArthedainCaptainsUnlock` | INVENTORIED; TODO |
| 5 | `Command_ArthedainBarraksPassive` | `SPECIAL_POWER` | `SpecialAbilityFakeLeadership2` | INVENTORIED; TODO |
| 6 | `Command_Sell` | `SELL` | — | INVENTORIED; TODO |

#### `ArthedainCampCommandSetUpThreeArmor`

| Слот | Команда | Тип | Объект / улучшение / способность | Статус |
|---:|---|---|---|---|
| 1 | `Command_ConstructArthedainWarriorArmr` | `UNIT_BUILD` | `ArthedainWarriorHorde_Armr` | INVENTORIED; TODO |
| 2 | `Command_ConstructBreeTownGuardArmr` | `UNIT_BUILD` | `BreeTownGuardHorde_Armr` | INVENTORIED; TODO |
| 3 | `Command_ConstructArthedainCrossbowHordeArmr` | `UNIT_BUILD` | `ArthedainCrossbowHorde_armr` | INVENTORIED; TODO |
| 4 | `Command_PurchaseUpgradeArthedainCaptainsUnlock` | `PLAYER_UPGRADE` | `Upgrade_ArthedainCaptainsUnlock` | INVENTORIED; TODO |
| 5 | `Command_ArthedainBarraksPassive` | `SPECIAL_POWER` | `SpecialAbilityFakeLeadership2` | INVENTORIED; TODO |
| 6 | `Command_Sell` | `SELL` | — | INVENTORIED; TODO |

#### `ArthedainCampCommandSetUpThreeSword`

| Слот | Команда | Тип | Объект / улучшение / способность | Статус |
|---:|---|---|---|---|
| 1 | `Command_ConstructArthedainWarriorlvl2` | `UNIT_BUILD` | `ArthedainWarriorHorde_lvl2` | INVENTORIED; TODO |
| 2 | `Command_ConstructBreeTownGuardlvl2` | `UNIT_BUILD` | `BreeTownGuardHorde_lvl2` | INVENTORIED; TODO |
| 3 | `Command_ConstructArthedainCrossbowHorde` | `UNIT_BUILD` | `ArthedainCrossbowHorde` | INVENTORIED; TODO |
| 4 | `Command_PurchaseUpgradeArthedainCaptainsUnlock` | `PLAYER_UPGRADE` | `Upgrade_ArthedainCaptainsUnlock` | INVENTORIED; TODO |
| 5 | `Command_ArthedainBarraksPassive` | `SPECIAL_POWER` | `SpecialAbilityFakeLeadership2` | INVENTORIED; TODO |
| 6 | `Command_Sell` | `SELL` | — | INVENTORIED; TODO |

#### `ArthedainCampCommandSetUpTwo`

| Слот | Команда | Тип | Объект / улучшение / способность | Статус |
|---:|---|---|---|---|
| 1 | `Command_ConstructArthedainWarriorN2` | `UNIT_BUILD` | `ArthedainWarriorHorde_n2` | INVENTORIED; TODO |
| 2 | `Command_ConstructBreeTownGuardN2` | `UNIT_BUILD` | `BreeTownGuardHorde_n2` | INVENTORIED; TODO |
| 3 | `Command_ConstructArthedainCrossbowHorde` | `UNIT_BUILD` | `ArthedainCrossbowHorde` | INVENTORIED; TODO |
| 4 | `Command_PurchaseUpgradeArthedainCaptainsUnlock` | `PLAYER_UPGRADE` | `Upgrade_ArthedainCaptainsUnlock` | INVENTORIED; TODO |
| 5 | `Command_ArthedainBarraksPassive` | `SPECIAL_POWER` | `SpecialAbilityFakeLeadership2` | INVENTORIED; TODO |
| 6 | `Command_Sell` | `SELL` | — | INVENTORIED; TODO |
| 7 | `Command_PurchaseUpgradeArthCampLevel3` | `OBJECT_UPGRADE` | `Upgrade_ArthedainCampLevel3` | INVENTORIED; TODO |

#### `ArthedainCampCommandSetUpTwoSword`

| Слот | Команда | Тип | Объект / улучшение / способность | Статус |
|---:|---|---|---|---|
| 1 | `Command_ConstructArthedainWarriorlvl2` | `UNIT_BUILD` | `ArthedainWarriorHorde_lvl2` | INVENTORIED; TODO |
| 2 | `Command_ConstructBreeTownGuardlvl2` | `UNIT_BUILD` | `BreeTownGuardHorde_lvl2` | INVENTORIED; TODO |
| 3 | `Command_ConstructArthedainCrossbowHorde` | `UNIT_BUILD` | `ArthedainCrossbowHorde` | INVENTORIED; TODO |
| 4 | `Command_PurchaseUpgradeArthedainCaptainsUnlock` | `PLAYER_UPGRADE` | `Upgrade_ArthedainCaptainsUnlock` | INVENTORIED; TODO |
| 5 | `Command_ArthedainBarraksPassive` | `SPECIAL_POWER` | `SpecialAbilityFakeLeadership2` | INVENTORIED; TODO |
| 6 | `Command_Sell` | `SELL` | — | INVENTORIED; TODO |
| 7 | `Command_PurchaseUpgradeArthCampLevel3` | `OBJECT_UPGRADE` | `Upgrade_ArthedainCampLevel3` | INVENTORIED; TODO |

#### `ArthedainCampCommandSetlvl2`

| Слот | Команда | Тип | Объект / улучшение / способность | Статус |
|---:|---|---|---|---|
| 1 | `Command_ConstructArthedainWarriorlvl2` | `UNIT_BUILD` | `ArthedainWarriorHorde_lvl2` | INVENTORIED; TODO |
| 2 | `Command_ConstructBreeTownGuardlvl2` | `UNIT_BUILD` | `BreeTownGuardHorde_lvl2` | INVENTORIED; TODO |
| 3 | `Command_ConstructArthedainCrossbowHorde` | `UNIT_BUILD` | `ArthedainCrossbowHorde` | INVENTORIED; TODO |
| 4 | `Command_PurchaseUpgradeArthedainCaptainsUnlock` | `PLAYER_UPGRADE` | `Upgrade_ArthedainCaptainsUnlock` | INVENTORIED; TODO |
| 5 | `Command_ArthedainBarraksPassive` | `SPECIAL_POWER` | `SpecialAbilityFakeLeadership2` | INVENTORIED; TODO |
| 6 | `Command_Sell` | `SELL` | — | INVENTORIED; TODO |
| 7 | `Command_PurchaseUpgradeArthCampLevel2` | `OBJECT_UPGRADE` | `Upgrade_ArthedainCampLevel2` | INVENTORIED; TODO |

#### `ArthedainCampCommandSetlvl2AI`

| Слот | Команда | Тип | Объект / улучшение / способность | Статус |
|---:|---|---|---|---|
| 1 | `Command_ConstructArthedainWarrior` | `UNIT_BUILD` | `ArthedainWarriorHorde` | INVENTORIED; TODO |
| 2 | `Command_ConstructBreeTownGuard` | `UNIT_BUILD` | `BreeTownGuardHorde` | INVENTORIED; TODO |
| 3 | `Command_ConstructArthedainCrossbowHorde` | `UNIT_BUILD` | `ArthedainCrossbowHorde` | INVENTORIED; TODO |
| 5 | `Command_ArthedainBarraksPassive` | `SPECIAL_POWER` | `SpecialAbilityFakeLeadership2` | INVENTORIED; TODO |
| 6 | `Command_Sell` | `SELL` | — | INVENTORIED; TODO |

#### `ArthedainCampCommandSetlvl3`

| Слот | Команда | Тип | Объект / улучшение / способность | Статус |
|---:|---|---|---|---|
| 1 | `Command_ConstructArthedainWarriorlvl3` | `UNIT_BUILD` | `ArthedainWarriorHorde_lvl3` | INVENTORIED; TODO |
| 2 | `Command_ConstructBreeTownGuardlvl3` | `UNIT_BUILD` | `BreeTownGuardHorde_lvl3` | INVENTORIED; TODO |
| 3 | `Command_ConstructArthedainCrossbowHordeArmr` | `UNIT_BUILD` | `ArthedainCrossbowHorde_armr` | INVENTORIED; TODO |
| 4 | `Command_PurchaseUpgradeArthedainCaptainsUnlock` | `PLAYER_UPGRADE` | `Upgrade_ArthedainCaptainsUnlock` | INVENTORIED; TODO |
| 5 | `Command_ArthedainBarraksPassive` | `SPECIAL_POWER` | `SpecialAbilityFakeLeadership2` | INVENTORIED; TODO |
| 6 | `Command_Sell` | `SELL` | — | INVENTORIED; TODO |

#### `ArthedainCampCommandSetlvl3BothThree`

| Слот | Команда | Тип | Объект / улучшение / способность | Статус |
|---:|---|---|---|---|
| 1 | `Command_ConstructArthedainWarriorlvl3` | `UNIT_BUILD` | `ArthedainWarriorHorde_lvl3` | INVENTORIED; TODO |
| 2 | `Command_ConstructBreeTownGuardlvl3` | `UNIT_BUILD` | `BreeTownGuardHorde_lvl3` | INVENTORIED; TODO |
| 3 | `Command_ConstructArthedainCrossbowHordeArmr` | `UNIT_BUILD` | `ArthedainCrossbowHorde_armr` | INVENTORIED; TODO |
| 4 | `Command_PurchaseUpgradeArthedainCaptainsUnlock` | `PLAYER_UPGRADE` | `Upgrade_ArthedainCaptainsUnlock` | INVENTORIED; TODO |
| 5 | `Command_ArthedainBarraksPassive` | `SPECIAL_POWER` | `SpecialAbilityFakeLeadership2` | INVENTORIED; TODO |
| 6 | `Command_Sell` | `SELL` | — | INVENTORIED; TODO |

### `ArnorCardolanCamp`

- [ ] **ПРИОСТАНОВЛЕНО:** раскрыть условия меню и проверить доступность и все команды.

#### `CardolanCampCommandSet`

| Слот | Команда | Тип | Объект / улучшение / способность | Статус |
|---:|---|---|---|---|
| 1 | `Command_ConstructDunedainPikeman` | `UNIT_BUILD` | `DunedainSpearmanHorde` | INVENTORIED; TODO |
| 2 | `Command_ConstructCardolanRanger` | `UNIT_BUILD` | `CardolanRangerHorde` | INVENTORIED; TODO |
| 3 | `Command_ConstructCardolanMountedRanger` | `UNIT_BUILD` | `CardolanMountedRangerHorde` | INVENTORIED; TODO |
| 6 | `Command_Sell` | `SELL` | — | INVENTORIED; TODO |

### `ArnorKeepNew (родитель ArnorBattleTowerNew)`

- [ ] **ПРИОСТАНОВЛЕНО:** раскрыть условия меню и проверить доступность и все команды.

#### `GondorSentryTowerCommandSet`

| Слот | Команда | Тип | Объект / улучшение / способность | Статус |
|---:|---|---|---|---|
| 1 | `Command_ExitGarrison` | `EXIT_CONTAINER` | — | INVENTORIED; TODO |
| 2 | `Command_PurchaseGondorSentryTowerFireArrowsUpgrade` | `OBJECT_UPGRADE` | `Upgrade_SentryTowerFireArrows` | INVENTORIED; TODO |
| 6 | `Command_Sell` | `SELL` | — | INVENTORIED; TODO |

### `ArnorWallHubSmallAI`

- [ ] **ПРИОСТАНОВЛЕНО:** раскрыть условия меню и проверить доступность и все команды.

#### `ArnorCastleWallHubSmallCommandSetAI`

| Слот | Команда | Тип | Объект / улучшение / способность | Статус |
|---:|---|---|---|---|
| 1 | `Command_ArnorCastleWallHub_BeginWallSpanAI` | `DOZER_CONSTRUCT` | `ArnorWallHubSmallAI` | INVENTORIED; TODO |
| 4 | `Command_CancelWallSpan` | `CANCEL_NEIGHBORHOOD` | — | INVENTORIED; TODO |
| 6 | `Command_Sell` | `SELL` | — | INVENTORIED; TODO |

