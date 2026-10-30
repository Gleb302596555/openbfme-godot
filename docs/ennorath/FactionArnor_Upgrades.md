# Арнор: первоначальные улучшения зданий

[Рабочий реестр](FactionArnor.md) · [меню зданий](FactionArnor_Buildings.md). Здесь только улучшения `PLAYER_UPGRADE` / `OBJECT_UPGRADE` из уже найденных меню. Другие варианты команд и добавочные/призываемые здания ещё не раскрыты. Данные взяты из локального архива Ennorath 1.9.8; в случае нескольких определений указываются все найденные, итоговый приоритет следует установить через runtime.

- [ ] **ПРИОСТАНОВЛЕНО:** проверить все условия исследования, эффект, цены после разрешения макросов и переходы уровня; отдельно пройти локальные и глобальные улучшения в игре.

| Улучшение | Тип команды | Здание / кнопка | Объявления в архиве (стоимость, время) | Статус |
|---|---|---|---|---|
| `Upgrade_ArnorBarracksLevel2` | `OBJECT_UPGRADE` | `ArnorBarracksNew` / `Command_PurchaseUpgradeArnorBarracksLevel2` | `data\ini\upgrade.ini`:1654 cost `GONDOR_BARRACKS_LEVEL2_UPGRADE_COST`, time `GONDOR_BARRACKS_LEVEL2_UPGRADE_BUILDTIME` | INVENTORIED; поведение TODO |
| `Upgrade_ArnorBarracksLevel3` | `OBJECT_UPGRADE` | `ArnorBarracksNew` / `Command_PurchaseUpgradeArnorBarracksLevel3` | `data\ini\upgrade.ini`:1662 cost `GONDOR_BARRACKS_LEVEL3_UPGRADE_COST`, time `GONDOR_BARRACKS_LEVEL3_UPGRADE_BUILDTIME` | INVENTORIED; поведение TODO |
| `Upgrade_ArnorFortressBanners` | `OBJECT_UPGRADE` | `ArnorFortress` / `Command_PurchaseUpgradeArnorFortressBanners` | `data\ini\includes\upgrade.inc`:1269 cost `500`, time `30` | INVENTORIED; поведение TODO |
| `Upgrade_ArnorFortressCrystalMoat` | `OBJECT_UPGRADE` | `ArnorFortress` / `Command_PurchaseUpgradeArnorFortressCrystalMoat` | `data\ini\upgrade.ini`:1267 cost `800`, time `ELVEN_CRYSTALMOAT_BUILDTIME` | INVENTORIED; поведение TODO |
| `Upgrade_ArnorFortressGondorAlly` | `PLAYER_UPGRADE` | `ArnorFortress` / `Command_PurchaseUpgradeGondorAlly` | `data\ini\default\upgrade.ini`:25 cost `0`, time `3` | INVENTORIED; поведение TODO |
| `Upgrade_ArnorFortressHouseOfHealing` | `OBJECT_UPGRADE` | `ArnorFortress` / `Command_PurchaseUpgradeArnorOldKingStatues` | `data\ini\includes\upgrade.inc`:1278 cost `500`, time `30` | INVENTORIED; поведение TODO |
| `Upgrade_ArnorFortressImladrisAlly` | `PLAYER_UPGRADE` | `ArnorFortress` / `Command_PurchaseUpgradeImladrisAlly` | `data\ini\default\upgrade.ini`:18 cost `0`, time `3` | INVENTORIED; поведение TODO |
| `Upgrade_ArnorFortressNumenorStoneworkTrigger` | `OBJECT_UPGRADE` | `ArnorFortress` / `Command_PurchaseUpgradeArnorFortressNumenorStonework` | `data\ini\upgrade.ini`:1245 cost `GONDOR_NEMENORSTONEWORK_BUILDCOST`, time `GONDOR_NEMENORSTONEWORK_BUILDTIME` | INVENTORIED; поведение TODO |
| `Upgrade_ArnorFortressPalantirEye` | `OBJECT_UPGRADE` | `ArnorFortress` / `Command_PurchaseUpgradeArnorFortressPalantirEye` | `data\ini\upgrade.ini`:1257 cost `800`, time `ELVEN_EAGLENEST_BUILDTIME` | INVENTORIED; поведение TODO |
| `Upgrade_ArnorStableLevel2` | `OBJECT_UPGRADE` | `ArnorStableNew` / `Command_PurchaseUpgradeArnorStableLevel2` | `data\ini\upgrade.ini`:1670 cost `GONDOR_STABLE_LEVEL2_UPGRADE_COST`, time `GONDOR_STABLE_LEVEL2_UPGRADE_BUILDTIME` | INVENTORIED; поведение TODO |
| `Upgrade_ArnorStableLevel3` | `OBJECT_UPGRADE` | `ArnorStableNew` / `Command_PurchaseUpgradeArnorStableLevel3` | `data\ini\upgrade.ini`:1678 cost `GONDOR_STABLE_LEVEL3_UPGRADE_COST`, time `GONDOR_STABLE_LEVEL3_UPGRADE_BUILDTIME` | INVENTORIED; поведение TODO |
| `Upgrade_ArnorWorkshopLevel2` | `OBJECT_UPGRADE` | `ArnorWorkshopNew` / `Command_PurchaseUpgradeArnorWorkshopLevel2` | `data\ini\upgrade.ini`:1686 cost `GONDOR_WORKSHOP_LEVEL2_UPGRADE_COST`, time `GONDOR_WORKSHOP_LEVEL2_UPGRADE_BUILDTIME` | INVENTORIED; поведение TODO |
| `Upgrade_ArnorWorkshopLevel3` | `OBJECT_UPGRADE` | `ArnorWorkshopNew` / `Command_PurchaseUpgradeArnorWorkshopLevel3` | `data\ini\upgrade.ini`:1694 cost `GONDOR_WORKSHOP_LEVEL3_UPGRADE_COST`, time `GONDOR_WORKSHOP_LEVEL3_UPGRADE_BUILDTIME` | INVENTORIED; поведение TODO |
| `Upgrade_GondorForgeLevel2` | `OBJECT_UPGRADE` | `ArnorForgeNew` / `Command_PurchaseUpgradeGondorForgeLevel2` | `data\ini\upgrade.ini`:1565 cost `500`, time `GONDOR_FORGE_LEVEL2_UPGRADE_BUILDTIME` | INVENTORIED; поведение TODO |
| `Upgrade_GondorForgeLevel3` | `OBJECT_UPGRADE` | `ArnorForgeNew` / `Command_PurchaseUpgradeGondorForgeLevel3` | `data\ini\upgrade.ini`:1573 cost `250`, time `GONDOR_FORGE_LEVEL3_UPGRADE_BUILDTIME` | INVENTORIED; поведение TODO |
| `Upgrade_GoodFortressFlamingMunitionsTrigger` | `OBJECT_UPGRADE` | `ArnorFortress` / `Command_PurchaseUpgradeArnorFortressFlamingMunitions` | `data\ini\upgrade.ini`:1133 cost `1500`, time `30` | INVENTORIED; поведение TODO |
| `Upgrade_SchwerterderWesternis` | `PLAYER_UPGRADE` | `ArnorForgeNew` / `Command_PurchaseUpgradeNumenorSteel` | `data\ini\includes\upgrade.inc`:2894 cost `1500`, time `60` | INVENTORIED; поведение TODO |
| `Upgrade_TechnologyArnorBasicTraining` | `PLAYER_UPGRADE` | `ArnorBarracksNew` / `Command_PurchaseTechnologyArnorModBasicTraining` | `data\ini\upgrade.ini`:1872 cost `GONDOR_TECH_BASIC_TRAINING_BUILDCOST`, time `GONDOR_TECH_BASIC_TRAINING_BUILDTIME` | INVENTORIED; поведение TODO |
| `Upgrade_TechnologyArnorFireStones` | `PLAYER_UPGRADE` | `ArnorWorkshopNew` / `Command_PurchaseTechnologyArnorFireStones` | `data\ini\upgrade.ini`:1982 cost `GONDOR_TECH_FIRE_STONES_BUILDCOST`, time `GONDOR_TECH_FIRE_STONES_BUILDTIME` | INVENTORIED; поведение TODO |
| `Upgrade_TechnologyArthedainBasicTraining` | `PLAYER_UPGRADE` | `ArnorBarracksNew` / `Command_PurchaseTechnologyArnorModBasicTraining400` | `data\ini\upgrade.ini`:1880 cost `400`, time `GONDOR_TECH_BASIC_TRAINING_BUILDTIME` | INVENTORIED; поведение TODO |
| `Upgrade_TechnologyGondorFireArrows` | `PLAYER_UPGRADE` | `ArnorForgeNew` / `Command_PurchaseTechnologyArnorFireArrowsForge` | `data\ini\upgrade.ini`:1766 cost `GONDOR_TECH_FIRE_ARROWS_BUILDCOST`, time `GONDOR_TECH_FIRE_ARROWS_BUILDTIME` | INVENTORIED; поведение TODO |
| `Upgrade_TechnologyGondorForgedBlades` | `PLAYER_UPGRADE` | `ArnorForgeNew` / `Command_PurchaseTechnologyArnorForgedBladesMod` | `data\ini\upgrade.ini`:1787 cost `750`, time `GONDOR_TECH_FORGED_BLADES_BUILDTIME` | INVENTORIED; поведение TODO |
| `Upgrade_TechnologyGondorHeavyArmor` | `PLAYER_UPGRADE` | `ArnorForgeNew` / `Command_PurchaseTechnologyArnorModHeavyArmor` | `data\ini\upgrade.ini`:1809 cost `GONDOR_TECH_HEAVY_ARMOR_BUILDCOST`, time `GONDOR_TECH_HEAVY_ARMOR_BUILDTIME` | INVENTORIED; поведение TODO |
| `Upgrade_TechnologyGondorKnightShield` | `PLAYER_UPGRADE` | `ArnorStableNew` / `Command_PurchaseTechnologyArnorModKnightShield` | `data\ini\upgrade.ini`:1830 cost `GONDOR_TECH_KNIGHT_SHIELD_BUILDCOST`, time `GONDOR_TECH_KNIGHT_SHIELD_BUILDTIME` | INVENTORIED; поведение TODO |

## Дополнительные улучшения из меню лагерей

Источник: [меню лагерей](FactionArnor_Buildings.md). Достижимость и работа ещё не проверены; сначала раскрыть каждое состояние и связанные определения.

| Объект / улучшение | Тип команды | Здание / кнопка | Меню | Статус |
|---|---|---|---|---|
| `Upgrade_ArthedainCampLevel2` | `OBJECT_UPGRADE` | `ArnorArthedainCamp` / `Command_PurchaseUpgradeArthCampLevel2` | `ArthedainCampCommandSet`, `ArthedainCampCommandSetlvl2` | INVENTORIED; TODO |
| `Upgrade_ArthedainCampLevel3` | `OBJECT_UPGRADE` | `ArnorArthedainCamp` / `Command_PurchaseUpgradeArthCampLevel3` | `ArthedainCampCommandSetUpTwo`, `ArthedainCampCommandSetUpTwoSword` | INVENTORIED; TODO |
| `Upgrade_ArthedainCaptainsUnlock` | `PLAYER_UPGRADE` | `ArnorArthedainCamp` / `Command_PurchaseUpgradeArthedainCaptainsUnlock` | `ArthedainCampCommandSet`, `ArthedainCampCommandSetArmr`, `ArthedainCampCommandSetUpThree`, `ArthedainCampCommandSetUpThreeArmor`, `ArthedainCampCommandSetUpThreeSword`, `ArthedainCampCommandSetUpTwo`, `ArthedainCampCommandSetUpTwoSword`, `ArthedainCampCommandSetlvl2`, `ArthedainCampCommandSetlvl3`, `ArthedainCampCommandSetlvl3BothThree` | INVENTORIED; TODO |
| `Upgrade_SentryTowerFireArrows` | `OBJECT_UPGRADE` | `ArnorKeepNew (родитель ArnorBattleTowerNew)` / `Command_PurchaseGondorSentryTowerFireArrowsUpgrade` | `GondorSentryTowerCommandSet` | INVENTORIED; TODO |
