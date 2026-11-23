# Matrix review (tools/mod/matrix_review.py)

Layer 2 (lint) flags verdicts the mod's own data contradicts; layer 3 lists every button whose verdict moved since the previous run. A flagged red is likely the sweep's; an unflagged red that moved is the one to check against RotWK.

## Lint: 69 flagged, 0 of them red

By rule: L1 66, L4 3


## Changes since c14cef83: 75 buttons moved, 13 turned red

- Isengard `spell` `SpellBookBndnissderzweiTime`: PASS -> FAIL — the spell bar's button is not castable (owned 1, usable 0, ready 1)
- Isengard `spell` `SpellBookCrebain`: PASS -> FAIL — the spell bar's button is not castable (owned 1, usable 0, ready 1)
- Isengard `spell` `SpellBookDevastation`: PASS -> FAIL — the spell bar's button is not castable (owned 1, usable 0, ready 1)
- Isengard `spell` `SpellBookFreezingRain`: PASS -> FAIL — the spell bar's button is not castable (owned 1, usable 0, ready 1)
- Isengard `spell` `SpellBookFueltheFires`: PASS -> FAIL — the spell bar's button is not castable (owned 1, usable 0, ready 1)
- Isengard `spell` `SpellBookIsengardCapitans`: PASS -> FAIL — the spell bar's button is not castable (owned 1, usable 0, ready 1)
- Isengard `spell` `SpellBookIsengardMinenRegen`: PASS -> FAIL — the spell bar's button is not castable (owned 1, usable 0, ready 1)
- Isengard `spell` `SpellBookPalantirVision`: PASS -> FAIL — the spell bar's button is not castable (owned 1, usable 0, ready 1)
- Isengard `spell` `SpellBookSarumanVielfarbig`: PASS -> FAIL — the spell bar's button is not castable (owned 1, usable 0, ready 1)
- Isengard `spell` `SpellBookWarChantIsengard`: PASS -> FAIL — the spell bar's button is not castable (owned 1, usable 0, ready 1)
- Isengard `spell` `SpellBookWargBow`: PASS -> FAIL — the spell bar's button is not castable (owned 1, usable 0, ready 1)
- Isengard `spell` `SpellBookWildMenAllies`: PASS -> FAIL — the spell bar's button is not castable (owned 1, usable 0, ready 1)
- Mordor `MordorGrond` `(aura) ModuleTag_DreadVisageAura`: PASS -> NO-AURA — NO-AURA: GrondAntiFearModiffer on Azog_CINE (enemy) at 40 of Range 150 never arrived in 20 frames (pulses 5; the target is dead)
- Arnor `GondorTowerShieldGuardHordeShieldWall` `Command_ToggleFormationSchieldwallTurmwachenNo`: FAIL -> DISABLED
- Arnor `spell` `SpellBookStarOfTheNorth`: NO-RULE -> PASS
- Dwarves `DwarvenDurin` `Command_SpecialAbilityDurinMeisterGestein`: NO-EFFECT -> PASS
- Dwarves `spell` `SpecialAbilityDurinstag`: FAIL -> PASS
- Dwarves `spell` `SpellBookBombard`: FAIL -> PASS
- Dwarves `spell` `SpellBookDwarvenRiches`: FAIL -> PASS
- Dwarves `spell` `SpellBookEarthquake`: FAIL -> PASS
- Dwarves `spell` `SpellBookGaydalf`: FAIL -> PASS
- Dwarves `spell` `SpellBookMithril`: FAIL -> PASS
- Dwarves `spell` `SpellBookSpawnLoneTowerZwerge`: FAIL -> PASS
- Dwarves `spell` `SpellBookTrotzderZwerge`: FAIL -> PASS
- Dwarves `spell` `SpellBookUndermine`: FAIL -> PASS
- Dwarves `spell` `SpellBookZwergeAdler`: FAIL -> PASS
- Dwarves `spell` `SpellBookZwergenbier`: FAIL -> PASS
- Dwarves `spell` `SpellBookZwergenhorn`: FAIL -> PASS
- Elves `ElvenThranduil_mod` `Command_ThranduilKnigDsterwalds`: NO-EFFECT -> PASS
- Elves `GaladrielDarkMirror` `Command_GaladrielMirrorDissentLocked`: NOT-ON-BAR -> DISABLED
- Elves `GaladrielDarkMirror` `Command_GaladrielMirrorWardByMirror`: NOT-ON-BAR -> PASS
- Elves `GaladrielDarkMirror8` `Command_GaladrielMirrorDissent`: NOT-ON-BAR -> PASS
- Elves `GaladrielDarkMirror8` `Command_GaladrielMirrorWardByMirror`: NOT-ON-BAR -> PASS
- Elves `ImladrisCitadel` `Command_SpecialAbilitySaatdesgoldenenWaldes`: ORDER -> PASS
- Elves `TomBombadil_Summoned` `(aura) ModuleTag_Leadership`: LEFT-WORLD -> DISABLED
- Evilmen `HaradAlatar_modRing` `Command_SpecialAbilityAlatarSummonOliphs`: PASS -> ORDER
- Evilmen `HaradBigScorpion` `Command_BigScorpionUnderground`: NO-EFFECT -> PASS
- Evilmen `HaradSpy` `Command_SpecialAbilityHaradScoutCloak`: STUCK -> PASS
- Evilmen `HaradmuhadMounted` `Command_SpecialAbilityMuhadSummonOliph`: NO-FIRE -> PASS
- Evilmen `HaradrimArcherHordeNewOnMumakil` `Command_HaradrimArcherBarbedArrowAbility`: PASS -> ORDER
- Evilmen `MordorOutpost` `Command_OutpostOrcSpawn`: ORDER -> PASS
- Evilmen `MordorTribute_forHarad` `Command_SpecialAbilityTributeGainTwo`: NO-EFFECT -> PASS
- Evilmen `MordorTribute_forHaradSmall` `Command_SpecialAbilityTributeGainTwo`: NO-EFFECT -> PASS
- Evilmen `spell` `SpellBookHaradEnchArmor`: FAIL -> PASS
- Imladris `ImladrisGildor` `Command_SpecialAbilityElfCloakGildor`: STUCK -> PASS
- Imladris `spell` `SpellBookErestorWisdom`: FAIL -> PASS
- Imladris `spell` `SpellBookManveWisdom`: FAIL -> PASS
- Imladris `spell` `SpellBookSpecialPowerElrondsRat`: FAIL -> PASS
- Isengard `IsengardWulfgar` `Command_SpecialAbilityWulfgarRufderPlnderer`: NO-EFFECT -> PASS
- Men `GondorBeregond` `Command_BeregondMountNew`: STUCK -> PASS
- Men `GondorFaramir_mod` `Command_HeroFaramirWeaponToggle`: STUCK -> PASS
- Men `GondorTowerShieldGuardHordeShieldWall` `Command_ToggleFormationSchieldwallTurmwachenNo`: FAIL -> DISABLED
- Men `MenPorterRepair` `Command_RepairAI`: NO-SEND -> PASS
- Men `spell` `SpellBookRebuild`: FAIL -> PASS
- Mordor `spell` `SpellBookCalltheHordeMod`: FAIL -> PASS
- Mordor `spell` `SpellBookEvilmenAllies`: FAIL -> PASS
- Mordor `spell` `SpellBookHammerderUnterwelt`: FAIL -> PASS
- Rohan `GasthausGhanBuriGhan` `Command_SpecialAbilityGeheimeWege`: NO-EFFECT -> PASS
- Rohan `GondorTowerShieldGuardHordeShieldWall` `Command_ToggleFormationSchieldwallTurmwachenNo`: FAIL -> DISABLED
- Rohan `RohanPeasantHorde` `Command_PurchaseUpgradeRohanForgedBladesForPeasants`: NOT-ON-BAR -> MOD-DATA
- Rohan `RohanPeasantHorde` `Command_PurchaseUpgradeRohanHeavyArmorForPeasants`: NOT-ON-BAR -> MOD-DATA
- Rohan `spell` `SpellBookDraft`: FAIL -> PASS
- Wild `MordorBalrogAI` `Command_SpecialAbilityBalrogBreathAI`: NO-EFFECT -> PASS
- Wild `MordorBalrogAI` `Command_SpecialAbilityMordorBalrogWhipAI`: NO-EFFECT -> PASS
- Wild `RavenPost` `Command_RavenPostSecondBonus`: NO-EFFECT -> PASS
- Wild `WildGoblinKing_mod` `Command_GorkilBombThrow`: NO-EFFECT -> PASS
- Wild `WildGoblinKing_mod` `Command_GorkilZuchtspinnenSummon`: ORDER -> PASS
- Wild `WildMarauderSwordBanner` `Command_AngmarTrollChargeAttack`: NO-SEND -> MEMBER
- Wild `WildMarauderSwordHorde` `Command_HalftrollChargeAttack`: NO-EFFECT -> PASS
- Wild `WildMarauderSwords` `Command_AngmarTrollChargeAttack`: NO-SEND -> MEMBER
- Wild `WildPorterSpecAI` `Command_RepairAI`: ORDER -> PASS
- Wild `WildSmaugNewRingFly` `Command_SmaugIncinerateNew`: ORDER -> PASS
- Wild `WildZuchtdrache` `Command_ToggleStance`: NO-STANCE -> PASS
- Wild `WildZuchtdracheBaby` `Command_ToggleStance`: NO-STANCE -> PASS
- Wild `spell` `SpellBookPaktdesHasses`: FAIL -> PASS
