# Failing matrix buttons and the RotWK functions their modules lack

Command: `tools/mod/matrix_gaps.py --base <ROTWK_INSTALL> --overlay <mod install> --matrix docs/ennorath/MATRIX.tsv --map docs/ennorath/RW_FUNCTION_MAP.tsv --out docs/ennorath/MATRIX_GAPS --inventory <rw_function_blocks.json> --game <game.dat>`

Failing matrix rows: 249; joined rows: 582; module classes reached: 19.

A link is a name match: the module's fields name the button's SpecialPower or Upgrade. The map's status is exact RW citation in engine/src (a port without a citation reads `none`). The join says where to look first, not which function fails: each row's cause is still traced.

`specific none`: a `none` function reached from at most 3 module classes of the map (the class's own code); the other `none` rows are shared helpers that lack a citation everywhere. Functions of at most 16 bytes (class-name getters, one-field setters) are left out of it.

Recognised by their code with --game and left out of it too (996 functions of the inventory): getModuleNameKey (a short call of NameKeyGenerator RW 0x5487EC) and xfer functions (first indirect call Xfer vslot 0x28, xferVersion), scalar deleting destructors (a short call of operator delete RW 0x42F6A0) and destructors (a first `mov [ecx], <vtable>` and a tail jump to the base destructor, at most 64 bytes).

## Main classes: each failing button once, under its most specific class

The main class of a button is the class it links to that the fewest failing buttons link to (then: more specific none functions). The shared links of an ability's chain (SpecialPowerModule, UnpauseSpecialPowerUpgrade, ...) stay in the next table; here a button counts where its own mechanism lives.

| class | buttons | results | specific none | first specific none functions | example buttons |
|---|---:|---|---:|---|---|
| WeaponFireSpecialAbilityUpdate | 50 | NO-EFFECT 30, NO-SEND 7, NO-FIRE 6, NOT-ON-BAR 5, LEFT-WORLD 2 | 3 | 0x00896156@0 0x00896370@0 0x006CA701@2 | AngmarAgandaur Command_SpecialAbilityAgandaurLighting, AngmarFortressCitadel Command_SpecialAbilityAngmarGiftFortress, AngmarHwaldar_mod Command_HwaldarVerbrannteErde |
| OCLSpecialPower | 25 | NO-FIRE 13, NO-EFFECT 7, NOT-ON-BAR 4, FAIL 1 | 5 | 0x008C744B@0 0x008C72C3@1 0x008C7A33@1 0x005EFB95@2 0x00896A70@2 | AngmarGulmataur Command_SpecialAbilityGulmatRunes, AngmarGulmataur Command_SpecialAbilityGulmatRunesPoison, ArnorFortressCitadel Command_SpecialAbilityArnorPalantirEye |
| RepairSpecialPower | 12 | NO-SEND 12 | 0 | - | ArnorPorter Command_Repair, DwarvenMinerMod_Dozer Command_Repair, EvilmenPorter Command_Repair |
| AutoAbilityBehavior | 11 | NO-EFFECT 7, NOT-ON-BAR 4 | 5 | 0x0085D3ED@1 0x006289E7@5 0x0062AB74@5 0x00629BCA@6 0x006A95E6@6 | AngmarDarkRangerHorde Command_AngmarRangersIceArrowAbility, AngmarFortressCitadel Command_FireWeaponAngmarFortressSanctumFrostball, DwarvenAxtschwingerHorde Command_AxtschwingerAxeThrow |
| ModelConditionSpecialAbilityUpdate | 9 | NO-EFFECT 5, NOT-ON-BAR 4 | 0 | - | GondorAragorn_SummonedHunters Command_SpecialAbilityElendilSummoned, GondorBoromir_OneRing Command_SpecialAbilityBoromirHornRing, GondorBoromir_mod Command_SpecialAbilityBoromirHorn |
| ToggleMountedSpecialAbilityUpdate | 9 | NO-EFFECT 5, NO-FIRE 3, STUCK 1 | 0 | - | GasthausKhamulSpell Command_SpecialAbilityKhamulGuldurEmpowered, GasthausKhamulSpellOnFoot Command_SpecialAbilityKhamulGuldurEmpowered, GasthausKhamulonFellBeastSpell Command_SpecialAbilityKhamulGuldurEmpowered |
| LevelGrantSpecialPower | 7 | NO-FIRE 6, NO-EFFECT 1 | 0 | - | AngmarGolfimbul Command_SpecialAbilityGolfimbulRaid, ArnorMalbethSeer Command_SpecialAbilityMalbethOnFarm, DwarvenNarin Command_SpecialAbilityNarinExp |
| ArrowStormUpdate | 5 | NO-EFFECT 5 | 7 | 0x00893D61@0 0x00893E4C@1 0x006A057C@2 0x006A1D75@2 0x006A253A@2 0x00852CA2@2 0x006A05C1@3 | GondorGandalfGeprfte Command_GondorGandalfLightningSwordGeprfte, IsengardSarumanKorrumpierte Command_SarumanPower, IsengardSaruman_Mod Command_SarumanPower |
| SpecialAbilityUpdate | 4 | NO-EFFECT 3, NO-SEND 1 | 2 | 0x00851E83@0 0x00852CA2@1 | CarthaenSpy Command_SpecialAbilityLoneRangerSummonFinal, DorwinionNaturmagier Command_SpecialAbilityNaturmagierErntesegen, ElvenThranduil_mod Command_ThranduilKnigDsterwalds |
| SummonReplacementSpecialAbilityUpdate | 4 | LEFT-WORLD 4 | 0 | - | ImladrisLoremaster Command_SpecialAbilityLoremasterAlueChoice, ImladrisLoremaster Command_SpecialAbilityLoremasterManveChoice, ImladrisLoremaster Command_SpecialAbilityLoremasterUlmoChoice |
| PlayerHealSpecialPower | 3 | NO-EFFECT 3 | 0 | - | DwarvenBalin Command_BalinBaukunstMorias, DwarvenBalin Command_BalinBaukunstderZwerge, RohanTheodred Command_TheodredReparaturarbeiten |
| StoreObjectsSpecialPower | 3 | NO-FIRE 3 | 1 | 0x008D1E1E@1 | DwarvenBalin Command_SpecialAbilityBalinExpidition, HaradAlatar_mod Command_SpecialAbilityAlatarTeleportAlly, HaradAlatar_modRing Command_SpecialAbilityAlatarTeleportRing |
| ActivateModuleSpecialPower | 2 | NO-EFFECT 2 | 0 | - | DwarvenDurin Command_SpecialAbilityDurinMeisterGestein, MordorMouthOfSauronNew Command_SpecialAbilityWortedesElends |
| ToggleHiddenSpecialAbilityUpdate | 2 | NO-EFFECT 1, STUCK 1 | 0 | - | GasthausGhanBuriGhan Command_SpecialAbilityGeheimeWege, HaradSpy Command_SpecialAbilityHaradScoutCloak |
| UnpauseSpecialPowerUpgrade | 2 | NO-EFFECT 1, NO-FIRE 1 | 2 | 0x00697B6E@1 0x008D280A@3 | IsengardSarumanVerfluchter Command_IndustriellerWahn, RavenPost Command_RavenPostSecondBonus |
| WeaponModeSpecialPowerUpdate | 2 | NO-EFFECT 1, NO-SEND 1 | 0 | - | WildMarauderSwordHorde Command_HalftrollChargeAttack, WildMarauderSwords Command_AngmarTrollChargeAttack |
| HeroModeSpecialAbilityUpdate | 1 | NO-EFFECT 1 | 0 | - | GondorBoromir_mod_ElrondsRat Command_SpecialAbilityBoromirTapfererWiderstand |
| ProductionSpeedBonus | 1 | NO-EFFECT 1 | 6 | 0x008C726B@1 0x005FD6A5@2 0x006AEFBE@2 0x005FD431@3 0x006AE82A@3 0x0060D23A@4 | IsengardWulfgar Command_SpecialAbilityWulfgarRufderPlnderer |
| SpecialPowerModule | 1 | NO-FIRE 1 | 4 | 0x00654F8E@0 0x00896A32@0 0x008981BE@0 0x00896A70@1 | GondorAdvisor_forFortress Command_SpecialAbilityAdvisorDefendIniciative |

## Module classes by all the failing buttons they touch (shared links included)

| class | buttons | map none | specific none | map partial | first specific none functions |
|---|---:|---:|---:|---:|---|
| SpecialPowerModule | 106 | 298 | 4 | 3 | 0x00654F8E@0 0x00896A32@0 0x008981BE@0 0x00896A70@1 |
| UnpauseSpecialPowerUpgrade | 93 | 21 | 2 | 0 | 0x00697B6E@1 0x008D280A@3 |
| WeaponFireSpecialAbilityUpdate | 71 | 178 | 3 | 12 | 0x00896156@0 0x00896370@0 0x006CA701@2 |
| AutoAbilityBehavior | 35 | 320 | 5 | 14 | 0x0085D3ED@1 0x006289E7@5 0x0062AB74@5 0x00629BCA@6 0x006A95E6@6 |
| OCLSpecialPower | 29 | 266 | 5 | 0 | 0x008C744B@0 0x008C72C3@1 0x008C7A33@1 0x005EFB95@2 0x00896A70@2 |
| RepairSpecialPower | 12 | 13 | 0 | 0 | - |
| ToggleMountedSpecialAbilityUpdate | 12 | 352 | 0 | 16 | - |
| ModelConditionSpecialAbilityUpdate | 9 | 174 | 0 | 12 | - |
| LevelGrantSpecialPower | 7 | 39 | 0 | 1 | - |
| ArrowStormUpdate | 5 | 186 | 7 | 12 | 0x00893D61@0 0x00893E4C@1 0x006A057C@2 0x006A1D75@2 0x006A253A@2 0x00852CA2@2 0x006A05C1@3 |
| SpecialAbilityUpdate | 4 | 353 | 2 | 16 | 0x00851E83@0 0x00852CA2@1 |
| SummonReplacementSpecialAbilityUpdate | 4 | 328 | 0 | 14 | - |
| PlayerHealSpecialPower | 3 | 222 | 0 | 2 | - |
| StoreObjectsSpecialPower | 3 | 40 | 1 | 2 | 0x008D1E1E@1 |
| ActivateModuleSpecialPower | 2 | 39 | 0 | 1 | - |
| ToggleHiddenSpecialAbilityUpdate | 2 | 194 | 0 | 12 | - |
| WeaponModeSpecialPowerUpdate | 2 | 117 | 0 | 4 | - |
| HeroModeSpecialAbilityUpdate | 1 | 98 | 0 | 4 | - |
| ProductionSpeedBonus | 1 | 45 | 6 | 1 | 0x008C726B@1 0x005FD6A5@2 0x006AEFBE@2 0x005FD431@3 0x006AE82A@3 0x0060D23A@4 |

## Failing rows without a module link, by result

| result | rows |
|---|---:|
| NO-MODULE | 62 |
| FORMATION | 24 |
| LEFT-WORLD | 20 |
| NO-SEND | 7 |
| NO-FIRE | 5 |
| NO-EFFECT | 4 |
| NOT-ON-BAR | 3 |
| STUCK | 2 |
