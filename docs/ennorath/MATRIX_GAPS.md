# Failing matrix buttons and the RotWK functions their modules lack

Command: `tools/mod/matrix_gaps.py --base <ROTWK_INSTALL> --overlay <mod install> --matrix docs/ennorath/MATRIX.tsv --map docs/ennorath/RW_FUNCTION_MAP.tsv --out docs/ennorath/MATRIX_GAPS --inventory <rw_function_blocks.json> --game <game.dat>`

Failing matrix rows: 200; joined rows: 354; module classes reached: 16.

A link is a name match: the module's fields name the button's SpecialPower or Upgrade. The map's status is exact RW citation in engine/src (a port without a citation reads `none`). The join says where to look first, not which function fails: each row's cause is still traced.

`specific none`: a `none` function reached from at most 3 module classes of the map (the class's own code); the other `none` rows are shared helpers that lack a citation everywhere. Functions of at most 16 bytes (class-name getters, one-field setters) are left out of it.

Recognised by their code with --game and left out of it too (996 functions of the inventory): getModuleNameKey (a short call of NameKeyGenerator RW 0x5487EC) and xfer functions (first indirect call Xfer vslot 0x28, xferVersion), scalar deleting destructors (a short call of operator delete RW 0x42F6A0) and destructors (a first `mov [ecx], <vtable>` and a tail jump to the base destructor, at most 64 bytes).

## Main classes: each failing button once, under its most specific class

The main class of a button is the class it links to that the fewest failing buttons link to (then: more specific none functions). The shared links of an ability's chain (SpecialPowerModule, UnpauseSpecialPowerUpgrade, ...) stay in the next table; here a button counts where its own mechanism lives.

| class | buttons | results | specific none | first specific none functions | example buttons |
|---|---:|---|---:|---|---|
| OCLSpecialPower | 9 | NOT-ON-BAR 3, ORDER 3, NO-EFFECT 2, NO-FIRE 1 | 0 | - | DwarvenDain_mod Command_SpecialAbilityDrarSpeerfeuer, DwarvenNarin Command_ramridercaptain, GaladrielDarkMirror Command_GaladrielMirrorWardByMirror |
| AutoAbilityBehavior | 7 | NO-EFFECT 3, GREY-BUG 1, MOD-DATA 1, NO-FIRE 1, ORDER 1 | 4 | 0x006289E7@5 0x0062AB74@5 0x00629BCA@6 0x006A95E6@6 | AngmarDarkRangerHorde Command_AngmarRangersIceArrowAbility, DwarvenAxtschwingerHorde Command_AxtschwingerAxeThrow, GondorDenethor_forFortress Command_SpecialAbilityDenethorSteuererlass |
| ToggleMountedSpecialAbilityUpdate | 7 | NO-EFFECT 4, ORDER 2, STUCK 1 | 0 | - | GondorBeregond Command_BeregondMountNew, HaradBigScorpion Command_BigScorpionUnderground, MordorTribute_forHarad Command_SpecialAbilityTributeGainTwo |
| WeaponFireSpecialAbilityUpdate | 7 | NO-FIRE 3, NO-EFFECT 2, ORDER 2 | 0 | - | AngmarNecroAcolyteBook Command_NecromancerDeathBargain, AngmarNecroAcolyteBook Command_SpecialAbilityGulzarMOrgulSeucheAcolyte, AngmarNecronomicon Command_SpecialAbilityGulzarMOrgulSeucheAcolyte |
| ModelConditionSpecialAbilityUpdate | 6 | NO-EFFECT 5, GREY-BUG 1 | 0 | - | GondorAragorn_SummonedHunters Command_SpecialAbilityElendilSummoned, GondorBoromir_OneRing Command_SpecialAbilityBoromirHornRing, GondorBoromir_mod Command_SpecialAbilityBoromirHorn |
| SpecialAbilityUpdate | 4 | NO-EFFECT 2, NO-SEND 1, ORDER 1 | 1 | 0x00852CA2@1 | CarthaenSpy Command_SpecialAbilityLoneRangerSummonFinal, DorwinionNaturmagier Command_SpecialAbilityNaturmagierErntesegen, ElvenThranduil_mod Command_ThranduilKnigDsterwalds |
| HeroModeSpecialAbilityUpdate | 2 | GREY-BUG 1, NO-EFFECT 1 | 0 | - | GondorBoromir_mod_ElrondsRat Command_SpecialAbilityBoromirTapfererWiderstand, GondorDenethor_forFortress Command_SpecialAbilityDenethorNotstandssituation |
| LevelGrantSpecialPower | 2 | ORDER 2 | 0 | - | RohanTheoden_mod Command_SpecialAbilityKingsFavorEdainTheo, Theoden_OneRing Command_SpecialAbilityKingsFavorEdainTheoRing |
| SpecialPowerModule | 2 | GREY-BUG 1, NO-EFFECT 1 | 3 | 0x00654F8E@0 0x00896A32@0 0x008981BE@0 | GondorDenethor_forFortress Command_SpecialAbilityDenethorVerzweiflungsakt, RavenPost Command_RavenPostSecondBonus |
| ToggleHiddenSpecialAbilityUpdate | 2 | NO-EFFECT 1, STUCK 1 | 0 | - | GasthausGhanBuriGhan Command_SpecialAbilityGeheimeWege, HaradSpy Command_SpecialAbilityHaradScoutCloak |
| WeaponModeSpecialPowerUpdate | 2 | NO-EFFECT 1, NO-SEND 1 | 0 | - | WildMarauderSwordHorde Command_HalftrollChargeAttack, WildMarauderSwords Command_AngmarTrollChargeAttack |
| ActivateModuleSpecialPower | 1 | NO-EFFECT 1 | 0 | - | DwarvenDurin Command_SpecialAbilityDurinMeisterGestein |
| GrantUpgradeCreate | 1 | GREY-BUG 1 | 0 | - | DwarvenGuardianEreborHorde_Summoned Command_PurchaseUpgradeDwarvenBasicTraining |
| ProductionSpeedBonus | 1 | NO-EFFECT 1 | 0 | - | IsengardWulfgar Command_SpecialAbilityWulfgarRufderPlnderer |

## Module classes by all the failing buttons they touch (shared links included)

| class | buttons | map none | specific none | map partial | first specific none functions |
|---|---:|---:|---:|---:|---|
| UnpauseSpecialPowerUpgrade | 40 | 13 | 2 | 0 | 0x00697B6E@1 0x008D280A@3 |
| SpecialPowerModule | 38 | 247 | 3 | 3 | 0x00654F8E@0 0x00896A32@0 0x008981BE@0 |
| WeaponFireSpecialAbilityUpdate | 16 | 113 | 0 | 11 | - |
| AutoAbilityBehavior | 12 | 240 | 4 | 14 | 0x006289E7@5 0x0062AB74@5 0x00629BCA@6 0x006A95E6@6 |
| OCLSpecialPower | 12 | 222 | 0 | 0 | - |
| ToggleMountedSpecialAbilityUpdate | 10 | 256 | 0 | 16 | - |
| ModelConditionSpecialAbilityUpdate | 6 | 111 | 0 | 11 | - |
| SpecialAbilityUpdate | 4 | 258 | 1 | 16 | 0x00852CA2@1 |
| HeroModeSpecialAbilityUpdate | 2 | 62 | 0 | 4 | - |
| LevelGrantSpecialPower | 2 | 18 | 0 | 1 | - |
| ToggleHiddenSpecialAbilityUpdate | 2 | 129 | 0 | 11 | - |
| WeaponModeSpecialPowerUpdate | 2 | 76 | 0 | 4 | - |
| ActivateModuleSpecialPower | 1 | 18 | 0 | 1 | - |
| GrantUpgradeCreate | 1 | 11 | 0 | 0 | - |
| LevelUpUpgrade | 1 | 6 | 0 | 1 | - |
| ProductionSpeedBonus | 1 | 16 | 0 | 0 | - |

## Failing rows without a module link, by result

| result | rows |
|---|---:|
| MEMBER | 70 |
| MOD-DATA | 49 |
| FAIL | 27 |
| TEMPORARY | 18 |
| NO-SEND | 6 |
| NOT-ON-BAR | 3 |
| LEFT-WORLD | 2 |
| NO-EFFECT | 2 |
| NO-STANCE | 2 |
| STUCK | 2 |
| GREY-BUG | 1 |
| NO-RULE | 1 |
