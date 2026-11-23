# Failing matrix buttons and the RotWK functions their modules lack

Command: `tools/mod/matrix_gaps.py --base <ROTWK_INSTALL> --overlay <mod install> --matrix docs/ennorath/MATRIX.tsv --map docs/ennorath/RW_FUNCTION_MAP.tsv --out docs/ennorath/MATRIX_GAPS --inventory <rw_function_blocks.json> --game <game.dat>`

Failing matrix rows: 152; joined rows: 262; module classes reached: 15.

A link is a name match: the module's fields name the button's SpecialPower or Upgrade. The map's status is exact RW citation in engine/src (a port without a citation reads `none`). The join says where to look first, not which function fails: each row's cause is still traced.

`specific none`: a `none` function reached from at most 3 module classes of the map (the class's own code); the other `none` rows are shared helpers that lack a citation everywhere. Functions of at most 16 bytes (class-name getters, one-field setters) are left out of it.

Recognised by their code with --game and left out of it too (996 functions of the inventory): getModuleNameKey (a short call of NameKeyGenerator RW 0x5487EC) and xfer functions (first indirect call Xfer vslot 0x28, xferVersion), scalar deleting destructors (a short call of operator delete RW 0x42F6A0) and destructors (a first `mov [ecx], <vtable>` and a tail jump to the base destructor, at most 64 bytes).

## Main classes: each failing button once, under its most specific class

The main class of a button is the class it links to that the fewest failing buttons link to (then: more specific none functions). The shared links of an ability's chain (SpecialPowerModule, UnpauseSpecialPowerUpgrade, ...) stay in the next table; here a button counts where its own mechanism lives.

| class | buttons | results | specific none | first specific none functions | example buttons |
|---|---:|---|---:|---|---|
| AutoAbilityBehavior | 6 | NO-EFFECT 3, MOD-DATA 1, NO-FIRE 1, ORDER 1 | 3 | 0x006289E7@5 0x00629BCA@6 0x006A95E6@6 | AngmarDarkRangerHorde Command_AngmarRangersIceArrowAbility, DwarvenAxtschwingerHorde Command_AxtschwingerAxeThrow, HaradrimArcherHordeNewOnMumakil Command_HaradrimArcherBarbedArrowAbility |
| OCLSpecialPower | 6 | ORDER 3, NO-EFFECT 2, FAIL 1 | 0 | - | DwarvenDain_mod Command_SpecialAbilityDrarSpeerfeuer, DwarvenNarin Command_ramridercaptain, GondorDenethor_forFortress Command_SpecialAbilityDenethorSteuererlass |
| ModelConditionSpecialAbilityUpdate | 5 | NO-EFFECT 4, FAIL 1 | 0 | - | GondorAragorn_SummonedHunters Command_SpecialAbilityElendilSummoned, GondorBoromir_OneRing Command_SpecialAbilityBoromirHornRing, GondorBoromir_mod_ElrondsRat Command_SpecialAbilityBoromirHorn |
| WeaponFireSpecialAbilityUpdate | 5 | NO-FIRE 3, NO-EFFECT 1, ORDER 1 | 0 | - | AngmarNecroAcolyteBook Command_NecromancerDeathBargain, AngmarNecroAcolyteBook Command_SpecialAbilityGulzarMOrgulSeucheAcolyte, AngmarNecronomicon Command_SpecialAbilityGulzarMOrgulSeucheAcolyte |
| SpecialAbilityUpdate | 3 | MEMBER 1, NO-EFFECT 1, ORDER 1 | 1 | 0x00852CA2@1 | CarthaenSpy Command_SpecialAbilityLoneRangerSummonFinal, DorwinionNaturmagier Command_SpecialAbilityNaturmagierErntesegen, WildMarauderSwordBanner Command_AngmarTrollChargeAttack |
| HeroModeSpecialAbilityUpdate | 2 | FAIL 1, NO-EFFECT 1 | 0 | - | GondorBoromir_mod_ElrondsRat Command_SpecialAbilityBoromirTapfererWiderstand, GondorDenethor_forFortress Command_SpecialAbilityDenethorNotstandssituation |
| GrantUpgradeCreate | 1 | GREY-BUG 1 | 0 | - | DwarvenGuardianEreborHorde_Summoned Command_PurchaseUpgradeDwarvenBasicTraining |
| LevelGrantSpecialPower | 1 | ORDER 1 | 0 | - | RohanTheoden_mod Command_SpecialAbilityKingsFavorEdainTheo |
| ObjectCreationUpgrade | 1 | FAIL 1 | 1 | 0x008B810B@2 | LindonTower Command_PurchaseUpgradeLindonReinforcement |
| SpecialPowerModule | 1 | FAIL 1 | 3 | 0x00654F8E@0 0x00896A32@0 0x008981BE@0 | GondorDenethor_forFortress Command_SpecialAbilityDenethorVerzweiflungsakt |
| ToggleMountedSpecialAbilityUpdate | 1 | NO-EFFECT 1 | 0 | - | GondorBoromir_mod Command_SpecialAbilityBoromirHorn |
| WeaponModeSpecialPowerUpdate | 1 | MEMBER 1 | 0 | - | WildMarauderSwords Command_AngmarTrollChargeAttack |

## Module classes by all the failing buttons they touch (shared links included)

| class | buttons | map none | specific none | map partial | first specific none functions |
|---|---:|---:|---:|---:|---|
| UnpauseSpecialPowerUpgrade | 25 | 6 | 2 | 0 | 0x00697B6E@1 0x008D280A@3 |
| SpecialPowerModule | 23 | 58 | 3 | 2 | 0x00654F8E@0 0x00896A32@0 0x008981BE@0 |
| AutoAbilityBehavior | 12 | 55 | 3 | 11 | 0x006289E7@5 0x00629BCA@6 0x006A95E6@6 |
| WeaponFireSpecialAbilityUpdate | 12 | 13 | 0 | 8 | - |
| OCLSpecialPower | 7 | 51 | 0 | 0 | - |
| ModelConditionSpecialAbilityUpdate | 6 | 12 | 0 | 8 | - |
| SpecialAbilityUpdate | 3 | 66 | 1 | 12 | 0x00852CA2@1 |
| HeroModeSpecialAbilityUpdate | 2 | 9 | 0 | 4 | - |
| ToggleMountedSpecialAbilityUpdate | 2 | 64 | 0 | 12 | - |
| GrantUpgradeCreate | 1 | 0 | 0 | 0 | - |
| LevelGrantSpecialPower | 1 | 1 | 0 | 1 | - |
| LevelUpUpgrade | 1 | 0 | 0 | 1 | - |
| ObjectCreationUpgrade | 1 | 1 | 1 | 1 | 0x008B810B@2 |
| RemoveUpgradeUpgrade | 1 | 0 | 0 | 3 | - |
| WeaponModeSpecialPowerUpdate | 1 | 9 | 0 | 3 | - |

## Failing rows without a module link, by result

| result | rows |
|---|---:|
| MEMBER | 70 |
| MOD-DATA | 51 |
| TEMPORARY | 18 |
| NO-SEND | 6 |
| NO-EFFECT | 2 |
| GREY-BUG | 1 |
| LEFT-WORLD | 1 |
| NO-AURA | 1 |
| NO-RULE | 1 |
