# Matrix review (tools/mod/matrix_review.py)

Layer 2 (lint) flags verdicts the mod's own data contradicts; layer 3 lists every button whose verdict moved since the previous run. A flagged red is likely the sweep's; an unflagged red that moved is the one to check against RotWK.

## Lint: 69 flagged, 0 of them red

By rule: L1 66, L4 3


## Changes since d220f2dc: 86 buttons moved, 3 turned red

- Angmar `AngmarNecroAcolyteBook` `Command_NecromancerDeathBargain`: PASS -> NO-FIRE — SpecialAbilityDeathBargainAcolyte (target none) did not fire in 300 frames (sent: MSG_DESTROY_SELECTED_GROUP b1 | MSG_DESTROY_SELECTED_GROUP
- Arnor `spell` `SpellBookStarOfTheNorth`: PASS -> NO-RULE — cast, but no effect seen beyond the control run: no effect of CloudBreakSpecialPower is asserted;  (enemies lost 2161 with the cast, 1742 wi
- Evilmen `HaradmuhadMounted` `Command_SpecialAbilityMuhadSummonOliph`: PASS -> NO-FIRE — SpecialAbilityMuhadSummonOliphant (target the ground) did not fire in 300 frames (sent: MSG_CREATE_SELECTED_GROUP b1 o31192 | MSG_DESTROY_SE
- Angmar `AngmarAgandaur` `Command_SpecialAbilityAgandaurLighting`: NO-EFFECT -> PASS
- Angmar `AngmarGulmataur` `Command_SpecialAbilityGulmatRunes`: NO-FIRE -> PASS
- Angmar `AngmarGulmataur` `Command_SpecialAbilityGulmatRunesPoison`: NO-FIRE -> PASS
- Angmar `AngmarHwaldar_mod` `Command_HwaldarVerbrannteErde`: NO-EFFECT -> PASS
- Angmar `AngmarMorgramir_mod` `Command_SpecialAbilityBetrayer`: NO-EFFECT -> PASS
- Angmar `AngmarMorgramir_mod` `Command_SpecialAbilitySoulLink`: NO-EFFECT -> PASS
- Angmar `AngmarNecroAcolyteBook` `Command_NecromancerFrostArmorStructure`: ORDER -> PASS
- Angmar `AngmarNecroAcolyteBook` `Command_SpecialAbilityGulzarMOrgulSeucheAcolyte`: LEFT-WORLD -> NO-FIRE — SpecialAbilityGulzarMorgulSeucheAcolyte (target GondorFighterHorde at 1850,2330,50, owner Player_2 [pixel 512,384, collision 0x14, model pic
- Angmar `AngmarNecromancerHordePlague` `Command_NecromancerDevastation`: NO-FIRE -> PASS
- Angmar `AngmarNecronomicon` `Command_NecromancerFrostArmorStructure`: NO-EFFECT -> PASS
- Angmar `AngmarPorter` `Command_PorterExtinguishFire`: NO-EFFECT -> PASS
- Angmar `AngmarTrollSling` `Command_SpecialAbilityAngmarCatapultPoisonBarrels`: NO-FIRE -> PASS
- Angmar `AngmarTrollSlingFortress` `Command_SpecialAbilityAngmarCatapultPoisonBarrels`: NO-FIRE -> PASS
- Angmar `AngmarTrollSlingWall` `Command_SpecialAbilityAngmarCatapultPoisonBarrels`: NO-FIRE -> PASS
- Angmar `spell` `SpellBookSummonStoneGiants`: FAIL -> PASS
- Arnor `ArnorPorter` `Command_PorterExtinguishFire`: NO-EFFECT -> PASS
- Dwarves `DwarvenCatapult` `Command_SpecialAbilityDwarvenIronHillsCatapultOilBarrels`: NO-FIRE -> PASS
- Dwarves `DwarvenCatapult_EredLuin` `Command_SpecialAbilityDwarvenIronHillsCatapultOilBarrels`: NO-FIRE -> PASS
- Dwarves `DwarvenMinerMod_Dozer` `Command_PorterExtinguishFire`: NO-EFFECT -> PASS
- Dwarves `DwarvenThorinIII_Eisenberge` `Command_SpecialAbilitySchmiedAlle`: NO-FIRE -> PASS
- Dwarves `ImladrisGwaihir_Summoned` `Command_GwaihirBlast`: ORDER -> PASS
- Elves `DorwinionNaturmagier` `Command_SpecialAbilityNaturmagierHeilung`: NO-EFFECT -> PASS
- Elves `ImladrisCitadel` `Command_SpecialAbilitySaatdesgoldenenWaldes`: NO-FIRE -> ORDER
- Elves `LothlorienAloneWorker` `Command_PorterExtinguishFire`: NO-EFFECT -> PASS
- Elves `LothlorienGaladrielRingablehnen` `Command_SpecialAbilityErinnerunganTelperion`: NO-EFFECT -> PASS
- Elves `LothlorienLegolas` `Command_LegolasElbenauge`: NO-VALUE -> PASS
- Elves `MirkwoodAvariWarrior` `Command_SpecialAbilityAvariSpionage`: NO-FIRE -> PASS
- Evilmen `EvilmenPorter` `Command_PorterExtinguishFire`: NO-EFFECT -> PASS
- Evilmen `HaradAlatar_modRing` `Command_SpecialAbilityAlatarSummonOliphs`: ORDER -> PASS
- Evilmen `HaradArnorSlaveAxeHorde` `Command_SpecialAbilityHaradSlavesBuildBoost`: NO-TARGET -> PASS
- Evilmen `HaradDwarvenSlaveHorde` `Command_SpecialAbilityHaradSlavesBuildBoost`: NO-TARGET -> PASS
- Evilmen `HaradElfSlaveHorde` `Command_SpecialAbilityHaradSlavesBuildBoost`: NO-TARGET -> PASS
- Evilmen `HaradFortressCitadel` `Command_SpecialAbilityFortressHaradSpyTechAI`: NOT-ON-BAR -> - — 
- Evilmen `HaradGondorSlaveHorde` `Command_SpecialAbilityHaradSlavesBuildBoost`: NO-TARGET -> PASS
- Evilmen `HaradRohanSlaveHorde` `Command_SpecialAbilityHaradSlavesBuildBoost`: NO-TARGET -> PASS
- Evilmen `HaradSlaveOrcHorde` `Command_SpecialAbilityHaradSlavesBuildBoost`: NO-TARGET -> PASS
- Evilmen `HaradrimArcherHordeNewOnMumakil` `Command_HaradrimArcherBarbedArrowAbility`: ORDER -> PASS
- Evilmen `HaradrrimSlaveAxeHorde` `Command_SpecialAbilityHaradSlavesBuildBoost`: NO-TARGET -> PASS
- Evilmen `MordorBlackUrukHorde` `Command_MordorBloodThirstySacrificeUruk`: NO-FIRE -> PASS
- Evilmen `MordorBlackUrukHordeBlock` `Command_MordorBloodThirstySacrificeUruk`: NO-FIRE -> PASS
- Evilmen `SalemeBlindTrap` `Command_SalemeBlendfalleDetonate`: NO-EFFECT -> PASS
- Evilmen `SalemeExplodeTrap` `Command_SalemeExplosionsfalleDetonate`: NO-EFFECT -> PASS
- Evilmen `SalemePoisonTrap` `Command_SalemeGiftfalleDetonate`: NO-EFFECT -> PASS
- Evilmen `SalemePoisonTrapStrong` `Command_SalemeGiftfalleDetonate`: NO-EFFECT -> PASS
- Imladris `AngmarTrollSlingWall` `Command_SpecialAbilityAngmarCatapultPoisonBarrels`: NO-FIRE -> PASS
- Imladris `ImladrisGwaihirMod` `Command_SpecialAbilityGwahirScreechManve`: NO-EFFECT -> PASS
- Imladris `ImladrisGwaihirMod` `Command_SpecialAbilityGwahirScreechNew`: NO-EFFECT -> PASS
- Imladris `ImladrisPorter` `Command_PorterExtinguishFire`: NO-EFFECT -> PASS
- Imladris `IsengardSarumanVerfluchter` `Command_IndustriellerWahn`: NO-FIRE -> PASS
- Imladris `LothlorienLegolas_ElrondsRat` `Command_LegolasElbenauge`: NO-VALUE -> PASS
- Isengard `IsengardDeployedExplosiveMine` `Command_IsengardMineDetonate`: NO-EFFECT -> PASS
- Isengard `IsengardDeployedExplosiveMineAI` `Command_IsengardMineDetonateAI`: DISABLED -> PASS
- Isengard `IsengardPorter` `Command_PorterExtinguishFire`: NO-EFFECT -> PASS
- Isengard `IsengardSarumanVerfluchter` `Command_IndustriellerWahn`: NO-FIRE -> PASS
- Isengard `MordorGrishnak_mod` `Command_SpecialAbilityGrishnakGiftwolke`: NO-EFFECT -> PASS
- Isengard `spell` `SpellBookDevastation`: NO-RULE -> PASS
- Men `GondorAdvisor_forFortress` `Command_SpecialAbilityAdvisorDefendIniciative`: NO-FIRE -> PASS
- Men `GondorAdvisor_forFortress` `Command_SpecialAbilityAdvisorMarketTrade`: NO-FIRE -> PASS
- Men `MenPorter` `Command_PorterExtinguishFire`: NO-EFFECT -> PASS
- Men `MenPorterRepair` `Command_PorterExtinguishFire`: NO-EFFECT -> PASS
- Men `MordorBalrog` `Command_SpecialAbilityBalrogWings`: NO-EFFECT -> PASS
- Mordor `GasthausKhamulSpell` `Command_SpecialAbilityKhamulGuldurEmpowered`: NO-FIRE -> PASS
- Mordor `GasthausKhamulSpellOnFoot` `Command_SpecialAbilityKhamulGuldurEmpowered`: NO-FIRE -> PASS
- Mordor `GasthausKhamulonFellBeastSpell` `Command_SpecialAbilityKhamulGuldurEmpowered`: NO-FIRE -> PASS
- Mordor `GorbagShagratUnited` `Command_SpecialAbilityGorbagUngolEmpowered`: NO-FIRE -> PASS
- Mordor `MordorBlackUrukHorde` `Command_MordorBloodThirstySacrificeUruk`: NO-FIRE -> PASS
- Mordor `MordorBlackUrukHordeBlock` `Command_MordorBloodThirstySacrificeUruk`: NO-FIRE -> PASS
- Mordor `MordorDrummerTroll` `Command_SpecialAbilityMordorDrummerRoar`: NO-EFFECT -> PASS
- Mordor `MordorGothmog_mod` `Command_SpecialAbilityGothmogMorgulEmpowered`: NO-FIRE -> PASS
- Mordor `MordorNekromantReborn` `Command_SpecialAbilityGuldurEmpowered`: NO-FIRE -> PASS
- Mordor `MordorNekromantReborn` `Command_SpecialAbilityMorgulEmpowered`: NO-FIRE -> PASS
- Mordor `MordorNekromantReborn` `Command_SpecialAbilityUngolEmpowered`: NO-FIRE -> PASS
- Mordor `MordorPorter` `Command_PorterExtinguishFire`: NO-EFFECT -> PASS
- Rohan `LothlorienLegolas_SummonedHunters` `Command_LegolasElbenauge_Spell`: NO-VALUE -> PASS
- Rohan `MordorBalrog` `Command_SpecialAbilityBalrogWings`: NO-EFFECT -> PASS
- Rohan `RohanPorter` `Command_PorterExtinguishFire`: NO-EFFECT -> PASS
- Rohan `RohanTheoden_mod` `Command_SpecialAbilityKingsFavorEdainTheo`: PASS -> ORDER
- Rohan `Theoden_OneRing` `Command_SpecialAbilityKingsFavorEdainTheoRing`: PASS -> ORDER
- Wild `MordorBalrog` `Command_SpecialAbilityBalrogWings`: NO-EFFECT -> PASS
- Wild `MordorBalrogAI` `Command_SpecialAbilityBalrogWingsAI`: NO-EFFECT -> PASS
- Wild `WildFortressCitadel` `Command_SpecialAbilityOpferannehmen`: NO-FIRE -> PASS
- Wild `WildPorter` `Command_PorterExtinguishFire`: NO-EFFECT -> PASS
- Wild `WildPorterSpecAI` `Command_PorterExtinguishFire`: NO-EFFECT -> PASS
