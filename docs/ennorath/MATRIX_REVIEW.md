# Matrix review (tools/mod/matrix_review.py)

Layer 2 (lint) flags verdicts the mod's own data contradicts; layer 3 lists every button whose verdict moved since the previous run. A flagged red is likely the sweep's; an unflagged red that moved is the one to check against RotWK.

## Lint: 71 flagged, 0 of them red

By rule: L1 66, L4 5


## Changes since 3f939131: 45 buttons moved, 2 turned red

- Elves `ElvenThranduil_mod` `Command_ThranduilKnigDsterwalds`: ORDER -> NO-EFFECT — SpecialAbilityTheodenGloriousCharge (target none) fired but SpecialPowerModule AttributeModifier KnigDsterwaldsModifier: no result; 
- Isengard `IsengardDeployedExplosiveMine` `Command_IsengardMineDetonate`: ORDER -> NO-EFFECT — SpecialAbilityDetonate (target none) fired but WeaponFireSpecialAbilityUpdate SpecialWeapon IsegardDetonateMine: no result (the caster's big
- Angmar `AngmarGolfimbul` `Command_SpecialAbilityGolfimbulRaid`: NO-FIRE -> PASS
- Angmar `AngmarRogashEnnorath` `Command_AngmarRogashLeap`: NO-EFFECT -> PASS
- Arnor `ArnorMalbethSeer` `Command_SpecialAbilityMalbethOnFarm`: NO-FIRE -> PASS
- Arnor `FornostHammer` `Command_SpecialAbilityFornostHammerQuake`: PASS -> DISABLED
- Arnor `LothlorienCirdan_summ` `Command_SpecialAbilityKirdanKingsFavor`: NO-FIRE -> PASS
- Arnor `spell` `SpellBookStarOfTheNorth`: NO-RULE -> PASS
- Dwarves `DwarvenDwalin` `Command_SpecialAbilityDwalinErdenhammer`: NO-EFFECT -> PASS
- Dwarves `DwarvenGimliMod` `Command_RohanGimliLeap`: NO-EFFECT -> PASS
- Dwarves `DwarvenGloin_mod` `Command_SpecialAbilityGloinSmash`: NO-EFFECT -> PASS
- Dwarves `DwarvenNarin` `Command_SpecialAbilityNarinExp`: NO-FIRE -> PASS
- Elves `LothlorienGaladriel` `Command_SpecialAbilityGaladrielVilya`: NO-EFFECT -> PASS
- Elves `LothlorienLegolas` `Command_SpecialAbilityArrowStorm`: NO-EFFECT -> PASS
- Elves `LothlorienLegolas` `Command_SpecialAbilityTrainArchers`: NO-EFFECT -> PASS
- Elves `MirkwoodElkrider` `Command_SpecialAbilityElkriderNaturBuff`: NO-VALUE -> PASS
- Elves `MirkwoodElkriderHorde` `Command_SpecialAbilityElkriderNaturBuff`: NO-VALUE -> PASS
- Elves `MirkwoodElkriderHordeWedgeFormation` `Command_SpecialAbilityElkriderNaturBuff`: NO-VALUE -> PASS
- Evilmen `HaradAlatar_modRing` `Command_SpecialAbilityAlatarSummonOliphs`: PASS -> ORDER
- Evilmen `HaradmuhadMounted` `Command_SpecialAbilityMuhadSummonOliph`: ORDER -> PASS
- Evilmen `HaradrimArcherHordeNewOnMumakil` `Command_HaradrimArcherBarbedArrowAbility`: PASS -> ORDER
- Evilmen `MordorOutpost` `Command_OutpostOrcSpawn`: PASS -> ORDER
- Imladris `GondorGandalfGeprfte` `Command_GondorGandalfLightningSwordGeprfte`: NO-EFFECT -> PASS
- Imladris `GondorGandalfGeprfte` `Command_SpecialAbilityWordOfPowerEdainGeprfte`: NO-EFFECT -> PASS
- Imladris `GondorGandalfVerfhrte` `Command_SpecialAbilityDunklesWortderMacht`: NO-EFFECT -> PASS
- Imladris `ImladrisElrond_RingHero` `Command_SpecialAbilityElrondNoldorKingActive`: NO-FIRE -> PASS
- Imladris `ImladrisZwillingeEdain` `Command_SpecialAbilityHeldenkillerElladan`: NO-EFFECT -> PASS
- Imladris `IsengardSarumanKorrumpierte` `Command_SarumanPower`: NO-EFFECT -> PASS
- Imladris `IsengardSaruman_Mod` `Command_SarumanPower`: NO-EFFECT -> PASS
- Imladris `LothlorienCirdan` `Command_SpecialAbilityKirdanKingsFavor`: NO-FIRE -> PASS
- Isengard `IsengardSarumanKorrumpierte` `Command_SarumanPower`: NO-EFFECT -> PASS
- Isengard `IsengardSaruman_Mod` `Command_SarumanPower`: NO-EFFECT -> PASS
- Isengard `IsengardSharku_mod` `Command_SpecialAbilityTameTheBeastNew`: NO-FIRE -> PASS
- Men `GondorGandalfGeprfte` `Command_GondorGandalfLightningSwordGeprfte`: NO-EFFECT -> PASS
- Men `GondorGandalfGeprfte` `Command_SpecialAbilityWizardBlastGeprfte`: NO-EFFECT -> PASS
- Men `GondorGandalfGeprfte` `Command_SpecialAbilityWordOfPowerEdainGeprfte`: NO-EFFECT -> PASS
- Men `GondorGandalfVerfhrte` `Command_SpecialAbilityDunklesWortderMacht`: NO-EFFECT -> PASS
- Men `ImladrisZwillingeEdain` `Command_SpecialAbilityHeldenkillerElladan`: NO-EFFECT -> PASS
- Men `RohanPippin_mod` `Command_PippinSongs`: NO-FIRE -> PASS
- Mordor `MordorLorgan` `Command_SpecialAbilityTrainSoldersLorgan`: NO-FIRE -> PASS
- Mordor `MordorMouthOfSauronNew` `Command_SpecialAbilityWortedesElends`: NO-EFFECT -> PASS
- Wild `WildDuhrbur` `Command_DuhrburLeap`: NO-EFFECT -> PASS
- Wild `WildGoblinKing_mod` `Command_GoblinKingGiftangriff`: NO-EFFECT -> PASS
- Wild `WildGoblinKing_mod` `Command_GorkilZuchtspinnenSummon`: PASS -> ORDER
- Wild `WildSmaugNewRingFly` `Command_SmaugIncinerateNew`: PASS -> ORDER
