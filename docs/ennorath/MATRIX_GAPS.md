# Failing matrix buttons and the RotWK functions their modules lack

Command: `tools/mod/matrix_gaps.py --base <ROTWK_INSTALL> --overlay <mod install> --matrix docs/ennorath/MATRIX.tsv --map docs/ennorath/RW_FUNCTION_MAP.tsv --out docs/ennorath/MATRIX_GAPS --inventory <rw_function_blocks.json> --game <game.dat>`

Failing matrix rows: 268; joined rows: 545; module classes reached: 16.

A link is a name match: the module's fields name the button's SpecialPower or Upgrade. The map's status is exact RW citation in engine/src (a port without a citation reads `none`). The join says where to look first, not which function fails: each row's cause is still traced.

`specific none`: a `none` function reached from at most 3 module classes of the map (the class's own code); the other `none` rows are shared helpers that lack a citation everywhere. Functions of at most 16 bytes (class-name getters, one-field setters) are left out of it.

Recognised by their code with --game and left out of it too (996 functions of the inventory): getModuleNameKey (a short call of NameKeyGenerator RW 0x5487EC) and xfer functions (first indirect call Xfer vslot 0x28, xferVersion), scalar deleting destructors (a short call of operator delete RW 0x42F6A0) and destructors (a first `mov [ecx], <vtable>` and a tail jump to the base destructor, at most 64 bytes).

## Main classes: each failing button once, under its most specific class

The main class of a button is the class it links to that the fewest failing buttons link to (then: more specific none functions). The shared links of an ability's chain (SpecialPowerModule, UnpauseSpecialPowerUpgrade, ...) stay in the next table; here a button counts where its own mechanism lives.

| class | buttons | results | specific none | first specific none functions | example buttons |
|---|---:|---|---:|---|---|
| WeaponFireSpecialAbilityUpdate | 42 | NO-EFFECT 24, NO-FIRE 7, NO-TARGET 7, ORDER 3, LEFT-WORLD 1 | 0 | - | AngmarAgandaur Command_SpecialAbilityAgandaurLighting, AngmarHwaldar_mod Command_HwaldarVerbrannteErde, AngmarMorgramir_mod Command_SpecialAbilityBetrayer |
| AutoAbilityBehavior | 23 | NO-EFFECT 11, NO-FIRE 7, ORDER 2, GREY-BUG 1, MOD-DATA 1, NOT-ON-BAR 1 | 5 | 0x0085D3ED@1 0x006289E7@5 0x0062AB74@5 0x00629BCA@6 0x006A95E6@6 | AngmarDarkRangerHorde Command_AngmarRangersIceArrowAbility, AngmarGulmataur Command_SpecialAbilityGulmatRunes, AngmarGulmataur Command_SpecialAbilityGulmatRunesPoison |
| OCLSpecialPower | 16 | NO-FIRE 6, ORDER 4, NO-EFFECT 3, NOT-ON-BAR 3 | 0 | - | DwarvenDain_mod Command_SpecialAbilityDrarSpeerfeuer, DwarvenNarin Command_ramridercaptain, GaladrielDarkMirror Command_GaladrielMirrorWardByMirror |
| ToggleMountedSpecialAbilityUpdate | 10 | NO-EFFECT 4, NO-FIRE 3, ORDER 2, STUCK 1 | 0 | - | GasthausKhamulSpell Command_SpecialAbilityKhamulGuldurEmpowered, GasthausKhamulSpellOnFoot Command_SpecialAbilityKhamulGuldurEmpowered, GasthausKhamulonFellBeastSpell Command_SpecialAbilityKhamulGuldurEmpowered |
| ModelConditionSpecialAbilityUpdate | 6 | NO-EFFECT 5, GREY-BUG 1 | 0 | - | GondorAragorn_SummonedHunters Command_SpecialAbilityElendilSummoned, GondorBoromir_OneRing Command_SpecialAbilityBoromirHornRing, GondorBoromir_mod Command_SpecialAbilityBoromirHorn |
| HeroModeSpecialAbilityUpdate | 5 | NO-VALUE 3, GREY-BUG 1, NO-EFFECT 1 | 0 | - | GondorBoromir_mod_ElrondsRat Command_SpecialAbilityBoromirTapfererWiderstand, GondorDenethor_forFortress Command_SpecialAbilityDenethorNotstandssituation, LothlorienLegolas Command_LegolasElbenauge |
| SpecialAbilityUpdate | 4 | NO-EFFECT 2, NO-SEND 1, ORDER 1 | 1 | 0x00852CA2@1 | CarthaenSpy Command_SpecialAbilityLoneRangerSummonFinal, DorwinionNaturmagier Command_SpecialAbilityNaturmagierErntesegen, ElvenThranduil_mod Command_ThranduilKnigDsterwalds |
| UnpauseSpecialPowerUpgrade | 3 | GREY-BUG 1, NO-EFFECT 1, NO-FIRE 1 | 2 | 0x00697B6E@1 0x008D280A@3 | GondorDenethor_forFortress Command_SpecialAbilityDenethorVerzweiflungsakt, IsengardSarumanVerfluchter Command_IndustriellerWahn, RavenPost Command_RavenPostSecondBonus |
| ToggleHiddenSpecialAbilityUpdate | 2 | NO-EFFECT 1, STUCK 1 | 0 | - | GasthausGhanBuriGhan Command_SpecialAbilityGeheimeWege, HaradSpy Command_SpecialAbilityHaradScoutCloak |
| WeaponModeSpecialPowerUpdate | 2 | NO-EFFECT 1, NO-SEND 1 | 0 | - | WildMarauderSwordHorde Command_HalftrollChargeAttack, WildMarauderSwords Command_AngmarTrollChargeAttack |
| ActivateModuleSpecialPower | 1 | NO-EFFECT 1 | 0 | - | DwarvenDurin Command_SpecialAbilityDurinMeisterGestein |
| GrantUpgradeCreate | 1 | GREY-BUG 1 | 0 | - | DwarvenGuardianEreborHorde_Summoned Command_PurchaseUpgradeDwarvenBasicTraining |
| LevelGrantSpecialPower | 1 | ORDER 1 | 0 | - | RohanTheoden_mod Command_SpecialAbilityKingsFavorEdainTheo |
| ProductionSpeedBonus | 1 | NO-EFFECT 1 | 0 | - | IsengardWulfgar Command_SpecialAbilityWulfgarRufderPlnderer |
| SpecialPowerModule | 1 | NO-FIRE 1 | 3 | 0x00654F8E@0 0x00896A32@0 0x008981BE@0 | GondorAdvisor_forFortress Command_SpecialAbilityAdvisorDefendIniciative |

## Module classes by all the failing buttons they touch (shared links included)

| class | buttons | map none | specific none | map partial | first specific none functions |
|---|---:|---:|---:|---:|---|
| SpecialPowerModule | 84 | 290 | 3 | 3 | 0x00654F8E@0 0x00896A32@0 0x008981BE@0 |
| UnpauseSpecialPowerUpgrade | 73 | 17 | 2 | 0 | 0x00697B6E@1 0x008D280A@3 |
| WeaponFireSpecialAbilityUpdate | 65 | 162 | 0 | 11 | - |
| OCLSpecialPower | 29 | 252 | 0 | 0 | - |
| AutoAbilityBehavior | 28 | 306 | 5 | 14 | 0x0085D3ED@1 0x006289E7@5 0x0062AB74@5 0x00629BCA@6 0x006A95E6@6 |
| ToggleMountedSpecialAbilityUpdate | 12 | 337 | 0 | 16 | - |
| ModelConditionSpecialAbilityUpdate | 6 | 157 | 0 | 11 | - |
| HeroModeSpecialAbilityUpdate | 5 | 88 | 0 | 4 | - |
| SpecialAbilityUpdate | 4 | 334 | 1 | 16 | 0x00852CA2@1 |
| ToggleHiddenSpecialAbilityUpdate | 2 | 180 | 0 | 11 | - |
| WeaponModeSpecialPowerUpdate | 2 | 108 | 0 | 4 | - |
| ActivateModuleSpecialPower | 1 | 32 | 0 | 1 | - |
| GrantUpgradeCreate | 1 | 21 | 0 | 0 | - |
| LevelGrantSpecialPower | 1 | 32 | 0 | 1 | - |
| LevelUpUpgrade | 1 | 20 | 0 | 1 | - |
| ProductionSpeedBonus | 1 | 26 | 0 | 0 | - |

## Failing rows without a module link, by result

| result | rows |
|---|---:|
| MEMBER | 70 |
| MOD-DATA | 49 |
| FAIL | 28 |
| TEMPORARY | 18 |
| NO-SEND | 6 |
| NOT-ON-BAR | 3 |
| LEFT-WORLD | 2 |
| NO-EFFECT | 2 |
| NO-FIRE | 2 |
| NO-STANCE | 2 |
| STUCK | 2 |
| GREY-BUG | 1 |
| NO-RULE | 1 |
