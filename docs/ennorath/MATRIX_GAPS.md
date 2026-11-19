# Failing matrix buttons and the RotWK functions their modules lack

Command: `tools/mod/matrix_gaps.py --base <ROTWK_INSTALL> --overlay <mod install> --matrix docs/ennorath/MATRIX.tsv --map docs/ennorath/RW_FUNCTION_MAP.tsv --out docs/ennorath/MATRIX_GAPS --inventory <rw_function_blocks.json> --game <game.dat>`

Failing matrix rows: 262; joined rows: 614; module classes reached: 21.

A link is a name match: the module's fields name the button's SpecialPower or Upgrade. The map's status is exact RW citation in engine/src (a port without a citation reads `none`). The join says where to look first, not which function fails: each row's cause is still traced.

`specific none`: a `none` function reached from at most 3 module classes of the map (the class's own code); the other `none` rows are shared helpers that lack a citation everywhere. Functions of at most 16 bytes (class-name getters, one-field setters) are left out of it.

Recognised by their code with --game and left out of it too (996 functions of the inventory): getModuleNameKey (a short call of NameKeyGenerator RW 0x5487EC) and xfer functions (first indirect call Xfer vslot 0x28, xferVersion), scalar deleting destructors (a short call of operator delete RW 0x42F6A0) and destructors (a first `mov [ecx], <vtable>` and a tail jump to the base destructor, at most 64 bytes).

## Main classes: each failing button once, under its most specific class

The main class of a button is the class it links to that the fewest failing buttons link to (then: more specific none functions). The shared links of an ability's chain (SpecialPowerModule, UnpauseSpecialPowerUpgrade, ...) stay in the next table; here a button counts where its own mechanism lives.

| class | buttons | results | specific none | first specific none functions | example buttons |
|---|---:|---|---:|---|---|
| WeaponFireSpecialAbilityUpdate | 47 | NO-EFFECT 30, NO-TARGET 7, NO-FIRE 5, ORDER 4, LEFT-WORLD 1 | 3 | 0x00896156@0 0x00896370@0 0x006CA701@2 | AngmarAgandaur Command_SpecialAbilityAgandaurLighting, AngmarHwaldar_mod Command_HwaldarVerbrannteErde, AngmarMorgramir_mod Command_SpecialAbilityBetrayer |
| OCLSpecialPower | 26 | NO-FIRE 9, NO-EFFECT 7, ORDER 6, NOT-ON-BAR 3, GREY-BUG 1 | 2 | 0x008C7A33@1 0x00896A70@2 | AngmarGulmataur Command_SpecialAbilityGulmatRunes, AngmarGulmataur Command_SpecialAbilityGulmatRunesPoison, DwarvenDain_mod Command_SpecialAbilityDrarSpeerfeuer |
| RepairSpecialPower | 12 | NO-TARGET 12 | 0 | - | ArnorPorter Command_Repair, DwarvenMinerMod_Dozer Command_Repair, EvilmenPorter Command_Repair |
| AutoAbilityBehavior | 11 | NO-EFFECT 8, MOD-DATA 1, NO-FIRE 1, NOT-ON-BAR 1 | 5 | 0x0085D3ED@1 0x006289E7@5 0x0062AB74@5 0x00629BCA@6 0x006A95E6@6 | AngmarDarkRangerHorde Command_AngmarRangersIceArrowAbility, DwarvenAxtschwingerHorde Command_AxtschwingerAxeThrow, GondorGandalfGeprfte Command_SpecialAbilityWizardBlastGeprfte |
| ToggleMountedSpecialAbilityUpdate | 11 | NO-EFFECT 6, NO-FIRE 3, ORDER 1, STUCK 1 | 0 | - | GasthausKhamulSpell Command_SpecialAbilityKhamulGuldurEmpowered, GasthausKhamulSpellOnFoot Command_SpecialAbilityKhamulGuldurEmpowered, GasthausKhamulonFellBeastSpell Command_SpecialAbilityKhamulBlackGuardsSummon |
| LevelGrantSpecialPower | 9 | NO-FIRE 6, ORDER 2, NO-EFFECT 1 | 0 | - | AngmarGolfimbul Command_SpecialAbilityGolfimbulRaid, ArnorMalbethSeer Command_SpecialAbilityMalbethOnFarm, DwarvenNarin Command_SpecialAbilityNarinExp |
| ModelConditionSpecialAbilityUpdate | 6 | NO-EFFECT 5, GREY-BUG 1 | 0 | - | GondorAragorn_SummonedHunters Command_SpecialAbilityElendilSummoned, GondorBoromir_OneRing Command_SpecialAbilityBoromirHornRing, GondorBoromir_mod Command_SpecialAbilityBoromirHorn |
| ArrowStormUpdate | 5 | NO-EFFECT 5 | 7 | 0x00893D61@0 0x00893E4C@1 0x006A057C@2 0x006A1D75@2 0x006A253A@2 0x00852CA2@2 0x006A05C1@3 | GondorGandalfGeprfte Command_GondorGandalfLightningSwordGeprfte, IsengardSarumanKorrumpierte Command_SarumanPower, IsengardSaruman_Mod Command_SarumanPower |
| SpecialAbilityUpdate | 4 | NO-EFFECT 2, NO-SEND 1, ORDER 1 | 2 | 0x00851E83@0 0x00852CA2@1 | CarthaenSpy Command_SpecialAbilityLoneRangerSummonFinal, DorwinionNaturmagier Command_SpecialAbilityNaturmagierErntesegen, ElvenThranduil_mod Command_ThranduilKnigDsterwalds |
| SummonReplacementSpecialAbilityUpdate | 4 | LEFT-WORLD 4 | 0 | - | ImladrisLoremaster Command_SpecialAbilityLoremasterAlueChoice, ImladrisLoremaster Command_SpecialAbilityLoremasterManveChoice, ImladrisLoremaster Command_SpecialAbilityLoremasterUlmoChoice |
| PlayerHealSpecialPower | 3 | NO-EFFECT 3 | 0 | - | DwarvenBalin Command_BalinBaukunstMorias, DwarvenBalin Command_BalinBaukunstderZwerge, RohanTheodred Command_TheodredReparaturarbeiten |
| StoreObjectsSpecialPower | 3 | NO-FIRE 3 | 1 | 0x008D1E1E@1 | DwarvenBalin Command_SpecialAbilityBalinExpidition, HaradAlatar_mod Command_SpecialAbilityAlatarTeleportAlly, HaradAlatar_modRing Command_SpecialAbilityAlatarTeleportRing |
| UnpauseSpecialPowerUpgrade | 3 | GREY-BUG 1, NO-EFFECT 1, NO-FIRE 1 | 2 | 0x00697B6E@1 0x008D280A@3 | GondorDenethor_forFortress Command_SpecialAbilityDenethorVerzweiflungsakt, IsengardSarumanVerfluchter Command_IndustriellerWahn, RavenPost Command_RavenPostSecondBonus |
| ActivateModuleSpecialPower | 2 | NO-EFFECT 2 | 0 | - | DwarvenDurin Command_SpecialAbilityDurinMeisterGestein, MordorMouthOfSauronNew Command_SpecialAbilityWortedesElends |
| HeroModeSpecialAbilityUpdate | 2 | GREY-BUG 1, NO-EFFECT 1 | 0 | - | GondorBoromir_mod_ElrondsRat Command_SpecialAbilityBoromirTapfererWiderstand, GondorDenethor_forFortress Command_SpecialAbilityDenethorNotstandssituation |
| ToggleHiddenSpecialAbilityUpdate | 2 | NO-EFFECT 1, STUCK 1 | 0 | - | GasthausGhanBuriGhan Command_SpecialAbilityGeheimeWege, HaradSpy Command_SpecialAbilityHaradScoutCloak |
| WeaponModeSpecialPowerUpdate | 2 | NO-EFFECT 1, NO-SEND 1 | 0 | - | WildMarauderSwordHorde Command_HalftrollChargeAttack, WildMarauderSwords Command_AngmarTrollChargeAttack |
| GrantUpgradeCreate | 1 | GREY-BUG 1 | 0 | - | DwarvenGuardianEreborHorde_Summoned Command_PurchaseUpgradeDwarvenBasicTraining |
| ProductionSpeedBonus | 1 | NO-EFFECT 1 | 6 | 0x008C726B@1 0x005FD6A5@2 0x006AEFBE@2 0x005FD431@3 0x006AE82A@3 0x0060D23A@4 | IsengardWulfgar Command_SpecialAbilityWulfgarRufderPlnderer |
| SpecialPowerModule | 1 | NO-FIRE 1 | 4 | 0x00654F8E@0 0x00896A32@0 0x008981BE@0 0x00896A70@1 | GondorAdvisor_forFortress Command_SpecialAbilityAdvisorDefendIniciative |

## Module classes by all the failing buttons they touch (shared links included)

| class | buttons | map none | specific none | map partial | first specific none functions |
|---|---:|---:|---:|---:|---|
| SpecialPowerModule | 104 | 295 | 4 | 3 | 0x00654F8E@0 0x00896A32@0 0x008981BE@0 0x00896A70@1 |
| UnpauseSpecialPowerUpgrade | 96 | 18 | 2 | 0 | 0x00697B6E@1 0x008D280A@3 |
| WeaponFireSpecialAbilityUpdate | 67 | 174 | 3 | 12 | 0x00896156@0 0x00896370@0 0x006CA701@2 |
| AutoAbilityBehavior | 36 | 317 | 5 | 14 | 0x0085D3ED@1 0x006289E7@5 0x0062AB74@5 0x00629BCA@6 0x006A95E6@6 |
| OCLSpecialPower | 31 | 259 | 2 | 0 | 0x008C7A33@1 0x00896A70@2 |
| ToggleMountedSpecialAbilityUpdate | 15 | 348 | 0 | 16 | - |
| RepairSpecialPower | 12 | 10 | 0 | 0 | - |
| LevelGrantSpecialPower | 9 | 36 | 0 | 1 | - |
| ModelConditionSpecialAbilityUpdate | 6 | 169 | 0 | 12 | - |
| ArrowStormUpdate | 5 | 181 | 7 | 12 | 0x00893D61@0 0x00893E4C@1 0x006A057C@2 0x006A1D75@2 0x006A253A@2 0x00852CA2@2 0x006A05C1@3 |
| SpecialAbilityUpdate | 4 | 349 | 2 | 16 | 0x00851E83@0 0x00852CA2@1 |
| SummonReplacementSpecialAbilityUpdate | 4 | 324 | 0 | 14 | - |
| PlayerHealSpecialPower | 3 | 219 | 0 | 2 | - |
| StoreObjectsSpecialPower | 3 | 37 | 1 | 2 | 0x008D1E1E@1 |
| ActivateModuleSpecialPower | 2 | 36 | 0 | 1 | - |
| HeroModeSpecialAbilityUpdate | 2 | 95 | 0 | 4 | - |
| ToggleHiddenSpecialAbilityUpdate | 2 | 190 | 0 | 12 | - |
| WeaponModeSpecialPowerUpdate | 2 | 111 | 0 | 4 | - |
| GrantUpgradeCreate | 1 | 21 | 0 | 0 | - |
| LevelUpUpgrade | 1 | 20 | 0 | 1 | - |
| ProductionSpeedBonus | 1 | 41 | 6 | 1 | 0x008C726B@1 0x005FD6A5@2 0x006AEFBE@2 0x005FD431@3 0x006AE82A@3 0x0060D23A@4 |

## Failing rows without a module link, by result

| result | rows |
|---|---:|
| MEMBER | 68 |
| MOD-DATA | 26 |
| TEMPORARY | 18 |
| NO-SEND | 6 |
| NO-FIRE | 5 |
| NO-EFFECT | 4 |
| FAIL | 3 |
| NOT-ON-BAR | 3 |
| LEFT-WORLD | 2 |
| ORDER | 2 |
| STUCK | 2 |
| GREY-BUG | 1 |
| NO-TARGET | 1 |
