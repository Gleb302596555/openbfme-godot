# Port 238: candidate command-set references (unfinished)

Metadata-only lexical extraction from Ennorath 1.9.8.3, `data/ini/includes/commandset.inc` and `commandbutton.inc`. These are direct, literal references from ten candidate sets in `PORT238_CANDIDATES.md` and two Frodo sets; they are **not** an exhaustive power inventory, do not resolve object inheritance / dynamic substitutions, and assign no ported verdict. Standard controls remain listed because their slots also matter. Duplicate declarations and included/override files are not resolved.

## Sets and slots

- `ArnorArveduiModCommandSet_OneRing` (commandset.inc:9287): 1 `Command_ToggleStance`; 2 `Command_ArveduiOneRingLeadership`; 3 `Command_SpecialAbilityArveduiUnantastbar`; 4 `Command_SpecialAbilityArveduiEinzugserlass`; 5 `Command_SpecialAbilityArveduiAussortierung`; 6 `Command_SpecialAbilityMalbethWeissagung`; 12 `Command_CaptureBuilding`; 13 `Command_AttackMove`; 14 `Command_Stop`; 16 `Command_SetStanceBattle`; 17 `Command_SetStanceAggressive`; 18 `Command_SetStanceHoldGround`.
- `BoromirModCommandSet_OneRing` (commandset.inc:14282): 1 `Command_ToggleStance`; 2 `Command_BoromirRingLeadership`; 3 `Command_SpecialAbilityBoromirHornRing`; 4 `Command_SpecialAbilityBoromirSchildRing`; 5 `Command_SpecialAbilityBoromirVerteidigungderweienStadt`; 12 `Command_CaptureBuilding`; 13 `Command_AttackMove`; 14 `Command_Stop`; 16 `Command_SetStanceBattle`; 17 `Command_SetStanceAggressive`; 18 `Command_SetStanceHoldGround`.
- `ElrondMODRingCommandSet` (commandset.inc:15335): 1 `Command_ToggleStance`; 2 `Command_SpecialAbilityElrondFloodRing`; 3 `Command_ElrondToggleMounted`; 4 `Command_SpecialAbilityElrondNoldorKingPassive`; 5 `Command_SpecialAbilityElrondRestoration`; 6 `Command_ElrondRingTornado`; 12 `Command_CaptureBuilding`; 13 `Command_AttackMove`; 14 `Command_Stop`; 16 `Command_SetStanceBattle`; 17 `Command_SetStanceAggressive`; 18 `Command_SetStanceHoldGround`.
- `ElrondMODRingCommandSetUp` (commandset.inc:15350): 1 `Command_ToggleStance`; 2 `Command_SpecialAbilityElrondFloodRing`; 3 `Command_ElrondToggleMounted`; 4 `Command_SpecialAbilityElrondNoldorKingActive`; 5 `Command_SpecialAbilityElrondRestoration`; 6 `Command_ElrondRingTornado`; 12 `Command_CaptureBuilding`; 13 `Command_AttackMove`; 14 `Command_Stop`; 16 `Command_SetStanceBattle`; 17 `Command_SetStanceAggressive`; 18 `Command_SetStanceHoldGround`.
- `GondorIsildurCommandSet_OneRing` (commandset.inc:11688): 1 `Command_ToggleStance`; 2 `Command_IsildurOneRingLeadership`; 3 `Command_IsildurKritischerAnstrum`; 4 `Command_SpecialAbilityIsildur5Ring`; 5 `Command_SpecialAbilityArnorIsildurNarsil`; 12 `Command_CaptureBuilding`; 13 `Command_AttackMove`; 14 `Command_Stop`; 16 `Command_SetStanceBattle`; 17 `Command_SetStanceAggressive`; 18 `Command_SetStanceHoldGround`.
- `LothlorienGaladrielCommandSet_Ring` (commandset.inc:11584): 1 `Command_ToggleStance`; 2 `Command_GaladrieldunkleHerrin`; 3 `Command_SpecialAbilityGaladrielGrundfeste`; 4 `Command_SpecialAbilityGaladrielAntlitz`; 5 `Command_SpecialPowerGaladrielSpawnTornado`; 6 `Command_SpecialAbilityGaladrielMachtdesRinges`; 12 `Command_CaptureBuilding`; 13 `Command_AttackMove`; 14 `Command_Stop`; 16 `Command_SetStanceBattle`; 17 `Command_SetStanceAggressive`; 18 `Command_SetStanceHoldGround`.
- `LothlorienGaladrielCommandSet_Valar` (commandset.inc:11568): 1 `Command_ToggleStance`; 2 `Command_GaladrielGesegneteAura`; 3 `Command_SpecialAbilityWordNenyaValar`; 4 `Command_SpecialAbilityErinnerunganTelperion`; 5 `Command_SpecialAbilityGaladrielLichtEarendilsBonus`; 6 `Command_SpecialAbilityMachtdesgoldenenWaldes`; 7 `Command_SpecialAbilityFakeUpdateGaladrielRingAnnehmenFX`; 12 `Command_CaptureBuilding`; 13 `Command_AttackMove`; 14 `Command_Stop`; 16 `Command_SetStanceBattle`; 17 `Command_SetStanceAggressive`; 18 `Command_SetStanceHoldGround`.
- `RohanFrodoModRingCommandSet` (commandset.inc:9961): 1 `Command_ToggleStance`; 2 `Command_ToggleFrodoWeapon`; 3 `Command_OneRing`; 4 `Command_SpecialAbilityPhialOfGaladrielEdain`; 5 `Command_FrodoMithrilhemd`; 12 `Command_CaptureBuilding`; 13 `Command_AttackMove`; 14 `Command_Stop`; 16 `Command_SetStanceBattle`; 17 `Command_SetStanceAggressive`; 18 `Command_SetStanceHoldGround`.
- `RohanFrodoModRingCommandSet_Galadriel` (commandset.inc:9975): 1 `Command_ToggleStance`; 2 `Command_ToggleFrodoWeapon`; 3 `Command_OneRing`; 4 `Command_SamLeadership`; 5 `Command_SpecialAbilityPhialOfGaladrielGeschenke`; 6 `Command_FrodoMithrilhemd`; 12 `Command_CaptureBuilding`; 13 `Command_AttackMove`; 14 `Command_Stop`; 16 `Command_SetStanceBattle`; 17 `Command_SetStanceAggressive`; 18 `Command_SetStanceHoldGround`.
- `RohanRadagastRingCommandSet` (commandset.inc:13712): 1 `Command_ToggleStance`; 2 `Command_SpecialAbilityNatursanktum`; 3 `Command_RadagastKraenRufRing`; 4 `Command_RadagastAdleraufsteigenRing`; 5 `Command_SpecialAbilityRadagastFreunde`; 6 `Command_SpecialAbilityRadagastConvertEnemyRing`; 12 `Command_CaptureBuilding`; 13 `Command_AttackMove`; 14 `Command_Stop`; 16 `Command_SetStanceBattle`; 17 `Command_SetStanceAggressive`; 18 `Command_SetStanceHoldGround`.
- `RohanRadagastaufAdlerRingCommandSet` (commandset.inc:13742): 1 `Command_ToggleStance`; 2 `Command_SpecialAbilityNatursanktum`; 3 `Command_RadagastKraenRufRing`; 4 `Command_RadagastAdlerabsteigen`; 5 `Command_SpecialAbilityRadagastFreunde`; 6 `Command_SpecialAbilityRadagastConvertEnemyAdlerRing`; 12 `Command_CaptureBuilding`; 13 `Command_AttackMove`; 14 `Command_Stop`; 16 `Command_SetStanceBattle`; 17 `Command_SetStanceAggressive`; 18 `Command_SetStanceHoldGround`.
- `RohanTheodenringCommandSetEnd` (commandset.inc:13549): 1 `Command_ToggleStance`; 2 `Command_TheodenToggleMountedModEdain`; 3 `Command_SpecialAbilityTheodenLeadershipRing`; 4 `Command_SummonEdoras`; 5 `Command_SpecialAbilityKingsFavorEdainTheoRing`; 6 `Command_SpecialAbilityTheodenGloriousChargeEdain`; 12 `Command_CaptureBuilding`; 13 `Command_AttackMove`; 14 `Command_Stop`; 16 `Command_SetStanceBattle`; 17 `Command_SetStanceAggressive`; 18 `Command_SetStanceHoldGround`.

## Direct button fields

The dash means no field found in this file, not proof that a reader or behavior is absent. `Command` is the button dispatcher kind, not a complete ability implementation.

| Button | Command | SpecialPower | NeededUpgrade | Definition |
| --- | --- | --- | --- | --- |
| `Command_ArveduiOneRingLeadership` | `SPECIAL_POWER` | `SpecialAbilityFakeLeadership` | — | `commandbutton.inc:35079` |
| `Command_AttackMove` | — | — | — | not found in this file |
| `Command_BoromirRingLeadership` | `SPECIAL_POWER` | `SpecialAbilityFakeLeadership2` | — | `commandbutton.inc:32680` |
| `Command_CaptureBuilding` | — | — | — | not found in this file |
| `Command_ElrondRingTornado` | `SPECIAL_POWER` | `SpecialPowerSpawnTornado2` | — | `commandbutton.inc:4077` |
| `Command_ElrondToggleMounted` | `SPECIAL_POWER` | `SpecialAbilityToggleMounted` | — | `commandbutton.inc:51244` |
| `Command_FrodoMithrilhemd` | `SPECIAL_POWER` | `SpecialAbilityFakeLeadership2` | — | `commandbutton.inc:6133` |
| `Command_GaladrielGesegneteAura` | `SPECIAL_POWER` | `SpecialAbilityFakeLeadership` | — | `commandbutton.inc:628` |
| `Command_GaladrieldunkleHerrin` | `SPECIAL_POWER` | `SpecialAbilityGaladrielDarkMirror` | — | `commandbutton.inc:653` |
| `Command_IsildurKritischerAnstrum` | `SPECIAL_POWER` | `SpecialAbilityHeroModeNarsilEdain` | — | `commandbutton.inc:4228` |
| `Command_IsildurOneRingLeadership` | `SPECIAL_POWER` | `SpecialAbilityFakeLeadership` | — | `commandbutton.inc:34926` |
| `Command_OneRing` | `ONE_RING` | — | — | `commandbutton.inc:20613` |
| `Command_RadagastAdlerabsteigen` | `SPECIAL_POWER` | `SpecialAbilityAdlerAbsteigen` | — | `commandbutton.inc:2043` |
| `Command_RadagastAdleraufsteigenRing` | `SPECIAL_POWER` | `SpecialAbilityAdlerAufsteigen` | — | `commandbutton.inc:2471` |
| `Command_RadagastKraenRufRing` | `SPECIAL_POWER` | `SpecialAbilityRadagastRavens` | — | `commandbutton.inc:2457` |
| `Command_SamLeadership` | `SPECIAL_POWER` | `SpecialAbilityFakeLeadership` | — | `commandbutton.inc:6144` |
| `Command_SetStanceAggressive` | — | — | — | not found in this file |
| `Command_SetStanceBattle` | — | — | — | not found in this file |
| `Command_SetStanceHoldGround` | — | — | — | not found in this file |
| `Command_SpecialAbilityArnorIsildurNarsil` | `SPECIAL_POWER` | `SpecialAbilityArnorIsildurNarsil` | — | `commandbutton.inc:34435` |
| `Command_SpecialAbilityArveduiAussortierung` | `SPECIAL_POWER` | `SpecialAbilityKlingentanz` | — | `commandbutton.inc:35119` |
| `Command_SpecialAbilityArveduiEinzugserlass` | `SPECIAL_POWER` | `SpecialAbilityArveduiEinzugserlass` | — | `commandbutton.inc:35105` |
| `Command_SpecialAbilityArveduiUnantastbar` | `SPECIAL_POWER` | `SpecialAbilitySalemeGiftfalleauslegen` | — | `commandbutton.inc:35091` |
| `Command_SpecialAbilityBoromirHornRing` | `SPECIAL_POWER` | `SpecialAbilityHornOfGondor02` | — | `commandbutton.inc:6557` |
| `Command_SpecialAbilityBoromirSchildRing` | `SPECIAL_POWER` | `SpecialAbilityAragornBladeMaster` | — | `commandbutton.inc:32755` |
| `Command_SpecialAbilityBoromirVerteidigungderweienStadt` | `SPECIAL_POWER` | `SpecialAbilityBoromirOsgiliathRing` | — | `commandbutton.inc:32769` |
| `Command_SpecialAbilityElrondFloodRing` | `SPECIAL_POWER` | `SpecialAbilityPersonalFlood` | `Upgrade_Level_2` | `commandbutton.inc:36338` |
| `Command_SpecialAbilityElrondNoldorKingActive` | `SPECIAL_POWER` | `SpecialAbilityElrondsLehrenRing` | — | `commandbutton.inc:51853` |
| `Command_SpecialAbilityElrondNoldorKingPassive` | `SPECIAL_POWER` | `SpecialAbilityFakeLeadership2` | — | `commandbutton.inc:51841` |
| `Command_SpecialAbilityElrondRestoration` | — | — | — | not found in this file |
| `Command_SpecialAbilityErinnerunganTelperion` | `SPECIAL_POWER` | `SpecialAbilityErinnerunganTelperion` | — | `commandbutton.inc:639` |
| `Command_SpecialAbilityFakeUpdateGaladrielRingAnnehmenFX` | `SPECIAL_POWER` | `SpecialAbilityTheodenGloriousCharge` | — | `commandbutton.inc:27321` |
| `Command_SpecialAbilityGaladrielAntlitz` | `SPECIAL_POWER` | `SpecialAbilityGaladrielAntlitz` | — | `commandbutton.inc:689` |
| `Command_SpecialAbilityGaladrielGrundfeste` | `SPECIAL_POWER` | `SpecialAbilityGaladrielGrundfeste` | — | `commandbutton.inc:677` |
| `Command_SpecialAbilityGaladrielLichtEarendilsBonus` | `SPECIAL_POWER` | `SpecialAbilityGaladrielLichtEarendils` | — | `commandbutton.inc:6313` |
| `Command_SpecialAbilityGaladrielMachtdesRinges` | `SPECIAL_POWER` | `SpecialAbilityGaladrielWordOfPower` | — | `commandbutton.inc:702` |
| `Command_SpecialAbilityIsildur5Ring` | — | — | — | not found in this file |
| `Command_SpecialAbilityKingsFavorEdainTheoRing` | `SPECIAL_POWER` | `SpecialAbilityKingsFavor` | — | `commandbutton.inc:15465` |
| `Command_SpecialAbilityMachtdesgoldenenWaldes` | `SPECIAL_POWER` | `SpecialAbilityMagiedesgoldenenWaldes` | — | `commandbutton.inc:27272` |
| `Command_SpecialAbilityMalbethWeissagung` | `SPECIAL_POWER` | `SpecialAbilityMalbethWeissagung` | — | `commandbutton.inc:35133` |
| `Command_SpecialAbilityNatursanktum` | `SPECIAL_POWER` | `SpecialAbilityNatursanktum` | — | `commandbutton.inc:34181` |
| `Command_SpecialAbilityPhialOfGaladrielEdain` | `SPECIAL_POWER` | `SpecialAbilityPhialOfGaladriel` | — | `commandbutton.inc:17846` |
| `Command_SpecialAbilityPhialOfGaladrielGeschenke` | `SPECIAL_POWER` | `SpecialAbilityPhialOfGaladriel` | — | `commandbutton.inc:17860` |
| `Command_SpecialAbilityRadagastConvertEnemyAdlerRing` | `SPECIAL_POWER` | `SpecialAbilityRadagastWordOfPoultry` | — | `commandbutton.inc:2498` |
| `Command_SpecialAbilityRadagastConvertEnemyRing` | `SPECIAL_POWER` | `SpecialAbilityRadagastWordOfPoultry` | — | `commandbutton.inc:2484` |
| `Command_SpecialAbilityRadagastFreunde` | `SPECIAL_POWER` | `SpecialAbilityRadagastFreunde` | — | `commandbutton.inc:2550` |
| `Command_SpecialAbilityTheodenGloriousChargeEdain` | `SPECIAL_POWER` | `SpecialAbilityTheodenGloriousCharge` | `Upgrade_Level_10` | `commandbutton.inc:15434` |
| `Command_SpecialAbilityTheodenLeadershipRing` | `SPECIAL_POWER` | `SpecialAbilityFakeLeadership` | — | `commandbutton.inc:15362` |
| `Command_SpecialAbilityWordNenyaValar` | `SPECIAL_POWER` | `SpecialAbilityDerRingNenya` | — | `commandbutton.inc:16949` |
| `Command_SpecialPowerGaladrielSpawnTornado` | `SPECIAL_POWER` | `SpecialPowerSpawnTornado` | — | `commandbutton.inc:665` |
| `Command_Stop` | — | — | — | not found in this file |
| `Command_SummonEdoras` | `SPECIAL_POWER` | `SpecialAbilityTheoderOsgiliathring` | — | `commandbutton.inc:38575` |
| `Command_TheodenToggleMountedModEdain` | `SPECIAL_POWER` | `SpecialAbilityToggleMounted` | — | `commandbutton.inc:15323` |
| `Command_ToggleFrodoWeapon` | — | — | — | not found in this file |
| `Command_ToggleStance` | — | — | — | not found in this file |

Next: resolve all command-set swaps and all button fields, SpecialPower module tags and upgrades through actual object inheritance, then compare each behavior and appearance to RW with a test or an explicit stop. Only `Command_OneRing` has a partial independent RW analysis in `PORT238_RW_ANALYSIS.md`; even that remains limited by S-7445 and pending test execution.
