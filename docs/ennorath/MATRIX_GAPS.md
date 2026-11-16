# Failing matrix buttons and the RotWK functions their modules lack

Command: `tools/mod/matrix_gaps.py --base <ROTWK_INSTALL> --overlay <mod install> --matrix docs/ennorath/MATRIX.tsv --map docs/ennorath/RW_FUNCTION_MAP.tsv --out docs/ennorath/MATRIX_GAPS --inventory <rw_function_blocks.json> --game <game.dat>`

Failing matrix rows: 420; joined rows: 1351; module classes reached: 33.

A link is a name match: the module's fields name the button's SpecialPower or Upgrade. The map's status is exact RW citation in engine/src (a port without a citation reads `none`). The join says where to look first, not which function fails: each row's cause is still traced.

`specific none`: a `none` function reached from at most 3 module classes of the map (the class's own code); the other `none` rows are shared helpers that lack a citation everywhere. Functions of at most 16 bytes (class-name getters, one-field setters) are left out of it.

Recognised by their code with --game and left out of it too (996 functions of the inventory): getModuleNameKey (a short call of NameKeyGenerator RW 0x5487EC) and xfer functions (first indirect call Xfer vslot 0x28, xferVersion), scalar deleting destructors (a short call of operator delete RW 0x42F6A0) and destructors (a first `mov [ecx], <vtable>` and a tail jump to the base destructor, at most 64 bytes).

## Main classes: each failing button once, under its most specific class

The main class of a button is the class it links to that the fewest failing buttons link to (then: more specific none functions). The shared links of an ability's chain (SpecialPowerModule, UnpauseSpecialPowerUpgrade, ...) stay in the next table; here a button counts where its own mechanism lives.

| class | buttons | results | specific none | first specific none functions | example buttons |
|---|---:|---|---:|---|---|
| WeaponFireSpecialAbilityUpdate | 101 | NO-RULE 47, NO-EFFECT 38, NO-FIRE 8, NOT-ON-BAR 5, LEFT-WORLD 2, FAIL 1 | 3 | 0x00896156@0 0x00896370@0 0x006CA701@2 | AngmarAgandaur Command_SpecialAbilityAgandaurLighting, AngmarFortressCitadel Command_SpecialAbilityAngmarGiftFortress, AngmarGolfimbul Command_SpecialAbilityGolfimbulSummonKeep |
| ReplaceSelfUpgrade | 37 | NOT-APPLIED 34, CRASH 3 | 2 | 0x008BB21F@0 0x008BBAA9@1 | AngmarWallSegmentSmall Command_AngmarWallUpgradeToCatapult, AngmarWallSegmentSmall Command_AngmarWallUpgradeToHub, AngmarWallSegmentSmall Command_AngmarWallUpgradeToPosternGate |
| OCLSpecialPower | 27 | NO-FIRE 14, NO-EFFECT 6, NOT-ON-BAR 4, NO-RULE 2, FAIL 1 | 5 | 0x008C744B@0 0x008C72C3@1 0x008C7A33@1 0x005EFB95@2 0x00896A70@2 | AngmarGulmataur Command_SpecialAbilityGulmatRunes, AngmarGulmataur Command_SpecialAbilityGulmatRunesPoison, ArnorFortressCitadel Command_SpecialAbilityArnorPalantirEye |
| AutoAbilityBehavior | 24 | NO-RULE 16, NO-EFFECT 4, NOT-ON-BAR 4 | 5 | 0x0085D3ED@1 0x006289E7@5 0x0062AB74@5 0x00629BCA@6 0x006A95E6@6 | AngmarDarkRangerHorde Command_AngmarRangersIceArrowAbility, AngmarFortressCitadel Command_FireWeaponAngmarFortressSanctumFrostball, CardolanRangerHorde Command_ArnorRangerDebuffAbility |
| ModelConditionSpecialAbilityUpdate | 17 | NO-RULE 13, NOT-ON-BAR 4 | 0 | - | DwarvenCaptainofDale_mod Command_BrandsFalkeSummon, GasthausGrimbeorn Command_BienenzuchtSummon, GasthausGrimbeornLorien Command_BienenzuchtSummon |
| ActivateModuleSpecialPower | 15 | NO-EFFECT 14, NO-FIRE 1 | 0 | - | AngmarHassall Command_SpecialAbilityHassalMeat, AngmarHassallWerwolf Command_SpecialAbilityHasalMoonlight, DwarvenDurin Command_SpecialAbilityDurinMeisterGestein |
| ToggleMountedSpecialAbilityUpdate | 15 | NO-EFFECT 7, NO-RULE 7, STUCK 1 | 0 | - | AngmarWitchkingNew Command_SpecialAbilityAngmarWKTerribleFuryNew, AngmarWitchkingNewRing Command_SpecialAbilityAngmarWKTerribleFuryRing, GondorBeregond Command_BeregondMountNew |
| RepairSpecialPower | 12 | NO-FIRE 12 | 0 | - | ArnorPorter Command_Repair, DwarvenMinerMod_Dozer Command_Repair, EvilmenPorter Command_Repair |
| LevelGrantSpecialPower | 7 | NO-FIRE 6, NO-EFFECT 1 | 0 | - | AngmarGolfimbul Command_SpecialAbilityGolfimbulRaid, ArnorMalbethSeer Command_SpecialAbilityMalbethOnFarm, DwarvenNarin Command_SpecialAbilityNarinExp |
| SpecialAbilityUpdate | 6 | NO-EFFECT 4, NO-RULE 2 | 2 | 0x00851E83@0 0x00852CA2@1 | BruchtalLichtbringerWasser Command_SpecialAbilityWasserdesBruinen, CarthaenSpy Command_SpecialAbilityLoneRangerSummonFinal, DorwinionNaturmagier Command_SpecialAbilityNaturmagierErntesegen |
| ArrowStormUpdate | 5 | NO-EFFECT 5 | 7 | 0x00893D61@0 0x00893E4C@1 0x006A057C@2 0x006A1D75@2 0x006A253A@2 0x00852CA2@2 0x006A05C1@3 | GondorGandalfGeprfte Command_GondorGandalfLightningSwordGeprfte, IsengardSarumanKorrumpierte Command_SarumanPower, IsengardSaruman_Mod Command_SarumanPower |
| ToggleDeploySpecialAbilityUpdate | 5 | NO-RULE 5 | 0 | - | DwarvenBatteringRam Command_SpecialAbilityDwarvenDemolisherDeploy, DwarvenDemolisher Command_SpecialAbilityDwarvenDemolisherDeploy, DwarvenDemolisher_Eisenberge Command_SpecialAbilityDwarvenDemolisherDeploy |
| DoCommandUpgrade | 4 | LEFT-WORLD 2, CRASH 1, NOT-APPLIED 1 | 0 | - | ImladrisLoremaster Command_UpgradeLoremasterAlueChoice, ImladrisLoremaster Command_UpgradeLoremasterManveChoice, ImladrisLoremaster Command_UpgradeLoremasterUlmoChoice |
| ReplaceObjectUpdate | 4 | NO-RULE 4 | 0 | - | RohanRadagast Command_SpecialAbilityRadagastConvertEnemy, RohanRadagast_RingHero Command_SpecialAbilityRadagastConvertEnemyRing, RohanRadagastaufdemAdler Command_SpecialAbilityRadagastConvertEnemyAdler |
| PlayerHealSpecialPower | 3 | NO-EFFECT 3 | 0 | - | DwarvenBalin Command_BalinBaukunstMorias, DwarvenBalin Command_BalinBaukunstderZwerge, RohanTheodred Command_TheodredReparaturarbeiten |
| StoreObjectsSpecialPower | 3 | NO-FIRE 3 | 1 | 0x008D1E1E@1 | DwarvenBalin Command_SpecialAbilityBalinExpidition, HaradAlatar_mod Command_SpecialAbilityAlatarTeleportAlly, HaradAlatar_modRing Command_SpecialAbilityAlatarTeleportRing |
| InvisibilitySpecialPower | 2 | NO-EFFECT 1, NO-RULE 1 | 3 | 0x008C66B3@1 0x0081A857@3 0x007766C4@4 | HaradSalemeMod Command_SpecialAbilityMoveUnseenSaleme, LothlorienHaldirRohan_Summoned Command_SpecialAbilityMoveUnseenHaldirRohan |
| ToggleHiddenSpecialAbilityUpdate | 2 | NO-EFFECT 1, STUCK 1 | 0 | - | GasthausGhanBuriGhan Command_SpecialAbilityGeheimeWege, HaradSpy Command_SpecialAbilityHaradScoutCloak |
| UnpauseSpecialPowerUpgrade | 2 | NO-EFFECT 1, NO-FIRE 1 | 2 | 0x00697B6E@1 0x008D280A@3 | IsengardSarumanVerfluchter Command_IndustriellerWahn, RavenPost Command_RavenPostSecondBonus |
| CostModifierUpgrade | 1 | NOT-APPLIED 1 | 12 | 0x006AB681@2 0x006AFCE3@2 0x006B0060@2 0x006B2028@2 0x006AAD50@3 0x006B013B@3 0x006B1F55@3 0x006AFF6F@4 | ImladrisCitadel Command_PurchaseUpgradeBndnisWaldelben_New |
| DominateEnemySpecialPower | 1 | NO-EFFECT 1 | 0 | - | AngmarHassall Command_HassallDominateWarg |
| HeroModeSpecialAbilityUpdate | 1 | NO-EFFECT 1 | 0 | - | GondorBoromir_mod_ElrondsRat Command_SpecialAbilityBoromirTapfererWiderstand |
| ProductionSpeedBonus | 1 | NO-EFFECT 1 | 6 | 0x008C726B@1 0x005FD6A5@2 0x006AEFBE@2 0x005FD431@3 0x006AE82A@3 0x0060D23A@4 | IsengardWulfgar Command_SpecialAbilityWulfgarRufderPlnderer |
| SpecialDisguiseUpdate | 1 | NO-RULE 1 | 0 | - | RohanEowyn_mod Command_DisguiseEowynEdain |
| SpecialPowerModule | 1 | NO-FIRE 1 | 4 | 0x00654F8E@0 0x00896A32@0 0x008981BE@0 0x00896A70@1 | GondorAdvisor_forFortress Command_SpecialAbilityAdvisorDefendIniciative |
| SummonReplacementSpecialAbilityUpdate | 1 | LEFT-WORLD 1 | 0 | - | ImladrisLoremaster Command_SpecialAbilityLoremasterRandomChoice |
| TransportContain | 1 | NO-RULE 1 | 1 | 0x0086A3DB@0 | FangornFlinkbaum Command_SpecialAbilityFlinkbaumEntmootHaste |
| WeaponModeSpecialPowerUpdate | 1 | NO-EFFECT 1 | 0 | - | WildMarauderSwordHorde Command_HalftrollChargeAttack |

## Module classes by all the failing buttons they touch (shared links included)

| class | buttons | map none | specific none | map partial | first specific none functions |
|---|---:|---:|---:|---:|---|
| SpecialPowerModule | 209 | 298 | 4 | 3 | 0x00654F8E@0 0x00896A32@0 0x008981BE@0 0x00896A70@1 |
| UnpauseSpecialPowerUpgrade | 164 | 21 | 2 | 0 | 0x00697B6E@1 0x008D280A@3 |
| WeaponFireSpecialAbilityUpdate | 144 | 178 | 3 | 12 | 0x00896156@0 0x00896370@0 0x006CA701@2 |
| AutoAbilityBehavior | 57 | 320 | 5 | 14 | 0x0085D3ED@1 0x006289E7@5 0x0062AB74@5 0x00629BCA@6 0x006A95E6@6 |
| ReplaceSelfUpgrade | 37 | 225 | 2 | 7 | 0x008BB21F@0 0x008BBAA9@1 |
| ToggleMountedSpecialAbilityUpdate | 30 | 352 | 0 | 16 | - |
| OCLSpecialPower | 28 | 266 | 5 | 0 | 0x008C744B@0 0x008C72C3@1 0x008C7A33@1 0x005EFB95@2 0x00896A70@2 |
| ModelConditionSpecialAbilityUpdate | 17 | 174 | 0 | 12 | - |
| ActivateModuleSpecialPower | 15 | 39 | 0 | 1 | - |
| RepairSpecialPower | 12 | 13 | 0 | 0 | - |
| LevelGrantSpecialPower | 7 | 39 | 0 | 1 | - |
| SpecialAbilityUpdate | 6 | 353 | 2 | 16 | 0x00851E83@0 0x00852CA2@1 |
| ArrowStormUpdate | 5 | 186 | 7 | 12 | 0x00893D61@0 0x00893E4C@1 0x006A057C@2 0x006A1D75@2 0x006A253A@2 0x00852CA2@2 0x006A05C1@3 |
| ToggleDeploySpecialAbilityUpdate | 5 | 166 | 0 | 12 | - |
| DoCommandUpgrade | 4 | 311 | 0 | 13 | - |
| ReplaceObjectUpdate | 4 | 40 | 0 | 1 | - |
| StatusBitsUpgrade | 4 | 37 | 0 | 3 | - |
| PlayerHealSpecialPower | 3 | 222 | 0 | 2 | - |
| StoreObjectsSpecialPower | 3 | 40 | 1 | 2 | 0x008D1E1E@1 |
| InvisibilitySpecialPower | 2 | 287 | 3 | 4 | 0x008C66B3@1 0x0081A857@3 0x007766C4@4 |
| ToggleHiddenSpecialAbilityUpdate | 2 | 194 | 0 | 12 | - |
| AudioLoopUpgrade | 1 | 39 | 3 | 1 | 0x008554C5@0 0x0086232E@1 0x008BBD6A@1 |
| CastleUpgrade | 1 | 58 | 1 | 0 | 0x007997E7@2 |
| CommandSetUpgrade | 1 | 80 | 1 | 3 | 0x008E3D1E@2 |
| CostModifierUpgrade | 1 | 44 | 12 | 0 | 0x006AB681@2 0x006AFCE3@2 0x006B0060@2 0x006B2028@2 0x006AAD50@3 0x006B013B@3 0x006B1F55@3 0x006AFF6F@4 |
| DominateEnemySpecialPower | 1 | 39 | 0 | 1 | - |
| HeroModeSpecialAbilityUpdate | 1 | 98 | 0 | 4 | - |
| ModelConditionUpgrade | 1 | 33 | 0 | 3 | - |
| ProductionSpeedBonus | 1 | 45 | 6 | 1 | 0x008C726B@1 0x005FD6A5@2 0x006AEFBE@2 0x005FD431@3 0x006AE82A@3 0x0060D23A@4 |
| SpecialDisguiseUpdate | 1 | 196 | 0 | 12 | - |
| SummonReplacementSpecialAbilityUpdate | 1 | 328 | 0 | 14 | - |
| TransportContain | 1 | 239 | 1 | 10 | 0x0086A3DB@0 |
| WeaponModeSpecialPowerUpdate | 1 | 117 | 0 | 4 | - |

## Failing rows without a module link, by result

| result | rows |
|---|---:|
| NO-MODULE | 62 |
| LEFT-WORLD | 27 |
| FORMATION | 24 |
| NO-RULE | 19 |
| NO-EFFECT | 7 |
| NO-FIRE | 7 |
| NO-SEND | 7 |
| NOT-ON-BAR | 3 |
| STUCK | 2 |
