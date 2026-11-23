# Matrix review (tools/mod/matrix_review.py)

Layer 2 (lint) flags verdicts the mod's own data contradicts; layer 3 lists every button whose verdict moved since the previous run. A flagged red is likely the sweep's; an unflagged red that moved is the one to check against RotWK.

## Lint: 69 flagged, 0 of them red

By rule: L1 66, L4 3


## Changes since 8acb6ea1: 56 buttons moved, 22 turned red

- Angmar `spell` `SpellBookSummonStoneGiants`: PASS -> FAIL — the HUD refused the scene's spot and every spot of the ring search (RotWK's location test, RW 0x82DFB7) (the spot's cell status 0) [AngmarDa
- Arnor `ArthedainCrossbowHorde_armr` `Command_PurchaseUpgradeArnorHeavyArmorMod`: DISABLED -> FAIL — DISABLED: already purchased (RW 0x9430C8 / RW 0x942FF9: availability 8)
- Arnor `ArthedainWarriorHorde_Armr` `Command_PurchaseUpgradeArnorHeavyArmorMod`: DISABLED -> FAIL — DISABLED: already purchased (RW 0x9430C8 / RW 0x942FF9: availability 8)
- Arnor `ArthedainWarriorHorde_lvl2` `Command_PurchaseUpgradeArnorForgedBladesMod`: DISABLED -> FAIL — DISABLED: already purchased (RW 0x9430C8 / RW 0x942FF9: availability 8)
- Arnor `ArthedainWarriorHorde_lvl3` `Command_PurchaseUpgradeArnorForgedBladesMod`: DISABLED -> FAIL — DISABLED: already purchased (RW 0x9430C8 / RW 0x942FF9: availability 8)
- Arnor `ArthedainWarriorHorde_lvl3` `Command_PurchaseUpgradeArnorHeavyArmorMod`: DISABLED -> FAIL — DISABLED: already purchased (RW 0x9430C8 / RW 0x942FF9: availability 8)
- Arnor `BreeTownGuardHordePorcupine_Armr` `Command_PurchaseUpgradeArnorHeavyArmorMod`: DISABLED -> FAIL — DISABLED: already purchased (RW 0x9430C8 / RW 0x942FF9: availability 8)
- Arnor `BreeTownGuardHordePorcupine_lvl2` `Command_PurchaseUpgradeArnorForgedBladesMod`: DISABLED -> FAIL — DISABLED: already purchased (RW 0x9430C8 / RW 0x942FF9: availability 8)
- Arnor `BreeTownGuardHordePorcupine_lvl3` `Command_PurchaseUpgradeArnorForgedBladesMod`: DISABLED -> FAIL — DISABLED: already purchased (RW 0x9430C8 / RW 0x942FF9: availability 8)
- Arnor `BreeTownGuardHordePorcupine_lvl3` `Command_PurchaseUpgradeArnorHeavyArmorMod`: DISABLED -> FAIL — DISABLED: already purchased (RW 0x9430C8 / RW 0x942FF9: availability 8)
- Arnor `BreeTownGuardHorde_Armr` `Command_PurchaseUpgradeArnorHeavyArmorMod`: DISABLED -> FAIL — DISABLED: already purchased (RW 0x9430C8 / RW 0x942FF9: availability 8)
- Arnor `BreeTownGuardHorde_lvl2` `Command_PurchaseUpgradeArnorForgedBladesMod`: DISABLED -> FAIL — DISABLED: already purchased (RW 0x9430C8 / RW 0x942FF9: availability 8)
- Arnor `BreeTownGuardHorde_lvl3` `Command_PurchaseUpgradeArnorForgedBladesMod`: DISABLED -> FAIL — DISABLED: already purchased (RW 0x9430C8 / RW 0x942FF9: availability 8)
- Arnor `BreeTownGuardHorde_lvl3` `Command_PurchaseUpgradeArnorHeavyArmorMod`: DISABLED -> FAIL — DISABLED: already purchased (RW 0x9430C8 / RW 0x942FF9: availability 8)
- Dwarves `DwarvenBattleWagon` `Command_UpgradeBattleWagonMenOfDale`: DISABLED -> FAIL — DISABLED: object not affected by Upgrade_BattleWagonMenOfDale (RW 0x9430F2 -> RW 0x694914: no UpgradeMux module triggered or RequiredObjectF
- Elves `ImladrisCitadel` `Command_PurchaseUpgradeBndnisWaldelben_Done`: DISABLED -> FAIL — DISABLED: object not affected by Upgrade_BndnisWaldelben (RW 0x9430F2 -> RW 0x694914: no UpgradeMux module triggered or RequiredObjectFilter
- Imladris `ElvenRivendellLancerHordeMod_up` `Command_PurchaseUpgradeBruchtalBasicTraining`: DISABLED -> FAIL — DISABLED: already purchased (RW 0x9430C8 / RW 0x942FF9: availability 8)
- Imladris `ElvenRivendellLancerHordeMod_up` `Command_PurchaseUpgradeBruchtalForgedBlades`: DISABLED -> FAIL — DISABLED: already purchased (RW 0x9430C8 / RW 0x942FF9: availability 8)
- Imladris `ElvenRivendellLancerHordeMod_up` `Command_PurchaseUpgradeBruchtalHeavyArmor`: DISABLED -> FAIL — DISABLED: already purchased (RW 0x9430C8 / RW 0x942FF9: availability 8)
- Imladris `ElvenRivendellLancerHordeMod_up` `Command_PurchaseUpgradeBruchtalKnightSchild`: DISABLED -> FAIL — DISABLED: already purchased (RW 0x9430C8 / RW 0x942FF9: availability 8)
- Mordor `MordorNekromantReborn` `Command_TogglePalantir2Necromant`: DISABLED -> FAIL — DISABLED: already purchased (RW 0x9430C8 / RW 0x942FF9: availability 8)
- Rohan `IsengardVerraeterCavalryHorde` `Command_PurchaseRohanHorses`: DISABLED -> FAIL — DISABLED: object not affected by Upgrade_PurchaseRohanHorses (RW 0x9430F2 -> RW 0x694914: no UpgradeMux module triggered or RequiredObjectFi
- Angmar `AngmarBarrowWightLair_Player_Independant` `Command_SpecialAbilityKarshSummonLvl10`: DISABLED -> PASS
- Angmar `AngmarBarrowWightLair_Player_Independant` `Command_SpecialAbilityKarshSummonLvl3`: DISABLED -> PASS
- Angmar `AngmarBarrowWightLair_Player_Independant` `Command_SpecialAbilityKarshSummonLvl5`: DISABLED -> PASS
- Angmar `AngmarBarrowWightLair_Player_Independant` `Command_SpecialAbilityKarshSummonLvl7`: DISABLED -> PASS
- Arnor `MordorWitchKingOnFellBeastSpell` `Command_KhamulFellBeastToggleMounted`: DISABLED -> PASS
- Arnor `MordorWitchKingPferdSpell` `Command_WitchKingToggleMountedonFellbeastNew`: DISABLED -> PASS
- Arnor `MordorWitchKingSpell` `Command_WitchKingToggleMountedonFellbeastNew`: DISABLED -> PASS
- Dwarves `IronHillsBallista` `Command_Bombard`: DISABLED -> PASS
- Elves `LothlorienOuterFlet` `Command_PurchaseLothlorienSentryTowerFireArrowsUpgrade`: GREY-BUG -> FAIL — DISABLED: object not affected by Upgrade_SentryTowerFireArrows (RW 0x9430F2 -> RW 0x694914: no UpgradeMux module triggered or RequiredObject
- Elves `RohanSam_mod` `Command_SamBratpfanne`: NO-SEND -> PASS
- Evilmen `EvilMenCorsairBallista` `Command_BombardEvil`: DISABLED -> PASS
- Evilmen `EvilMenCorsairBallistaBuildable` `Command_BombardEvil`: DISABLED -> PASS
- Evilmen `HaradBigScorpion_Underground` `Command_BigScorpionRelease`: DISABLED -> PASS
- Evilmen `HaradFortressBallista` `Command_BombardEvil`: DISABLED -> PASS
- Evilmen `HaradmuhadMounted` `Command_UnmountMuhadMumakil`: DISABLED -> PASS
- Imladris `DwarvenGuardianEreborHorde_Summoned` `Command_PurchaseUpgradeDwarvenBasicTraining`: GREY-BUG -> FAIL — DISABLED: object not affected by Upgrade_DwarvenBasicTraining (RW 0x9430F2 -> RW 0x694914: no UpgradeMux module triggered or RequiredObjectF
- Imladris `MordorWitchKingOnFellBeastSpell` `Command_KhamulFellBeastToggleMounted`: DISABLED -> PASS
- Imladris `MordorWitchKingPferdSpell` `Command_WitchKingToggleMountedonFellbeastNew`: DISABLED -> PASS
- Imladris `MordorWitchKingSpell` `Command_WitchKingToggleMountedonFellbeastNew`: DISABLED -> PASS
- Isengard `IsengardBallista` `Command_BombardEvil`: DISABLED -> PASS
- Isengard `IsengardBallista_Summoned` `Command_BombardEvil`: DISABLED -> PASS
- Isengard `IsengardFortressBallista` `Command_BombardEvil`: DISABLED -> PASS
- Men `MordorWitchKingOnFellBeastSpell` `Command_KhamulFellBeastToggleMounted`: DISABLED -> PASS
- Men `MordorWitchKingPferdSpell` `Command_WitchKingToggleMountedonFellbeastNew`: DISABLED -> PASS
- Men `MordorWitchKingSpell` `Command_WitchKingToggleMountedonFellbeastNew`: DISABLED -> PASS
- Men `RohanTheoden_mod` `Command_SpecialAbilityKingsFavorEdainTheoRing`: PASS -> ORDER
- Mordor `GasthausKhamulonFellBeastSpell` `Command_KhamulFellBeastToggleMounted`: DISABLED -> PASS
- Mordor `MordorGrond` `(aura) ModuleTag_DreadVisageAura`: NO-AURA -> PASS
- Mordor `MordorWitchKingOnFellBeastSpell` `Command_KhamulFellBeastToggleMounted`: DISABLED -> PASS
- Mordor `MordorWitchKingPferdSpell` `Command_WitchKingToggleMountedonFellbeastNew`: DISABLED -> PASS
- Mordor `MordorWitchKingSpell` `Command_WitchKingToggleMountedonFellbeastNew`: DISABLED -> PASS
- Rohan `MordorWitchKingOnFellBeastSpell` `Command_KhamulFellBeastToggleMounted`: DISABLED -> PASS
- Rohan `MordorWitchKingPferdSpell` `Command_WitchKingToggleMountedonFellbeastNew`: DISABLED -> PASS
- Rohan `MordorWitchKingSpell` `Command_WitchKingToggleMountedonFellbeastNew`: DISABLED -> PASS
