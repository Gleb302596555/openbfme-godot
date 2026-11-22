# Open stop mechanism candidates

Regenerate: `python tools/mod/stop_groups.py --game workspace/RotWK/game.dat --out docs/ennorath/STOP_GROUPS`

For --game install Python packages `capstone` and `pefile`; issue matching uses `gh` or an explicit --issues JSON snapshot. A discovered or explicitly supplied whole-function inventory takes precedence over the game heuristic; without either, only exact map starts are resolved.

Candidates are heuristic, not proof. Class merges use the area or the first named class of the gap; literal port and map-decomp name matches remain listed separately in TSV; generic names do not place a stop. Cross-group references remain visible after splitting. Common utility functions, the manually reviewed broad draw dispatcher RW 0x004BE587, and citations in explicitly historical-fix sentences are not merge pivots; every citation is retained. Check ownership, dependencies and issue overlap before implementation. Large transitive components are split by function; links can cross groups. Unknown function status is reported separately and is not counted as none, but is included as unverified work in the ranking denominator. Unavailable issue lookup is not evidence of no coverage.

990 open stops; 564 groups; 57 unplaced.

## Ranked groups

| Rank | Mechanism | Stops | none/partial functions | Red buttons | Unused fields | Open task |
|---:|---|---:|---:|---:|---:|---|
| 1 | RW 0x005D893C | 3 | 0 | 0 | 5 | #567 Port 243: Body damage states: ActiveBody EnteringDamagedTransitionTime and the Damaged / ReallyDamaged attribute modifiers (ActiveBody, RespawnBody) |
| 2 | ToggleHiddenSpecialAbilityUpdate | 2 | 0 | 2 | 0 | none matched |
| 3 | DynamicPortalBehaviour | 2 | 1 | 0 | 1 | #688 Port 296: Castles вЂ” CastleBehavior, CastleMemberBehavior and FoundationAIUpdate: the uncited own functions (27) and their five unread fields |
| 4 | linked stops S-017, S-123 | 2 | 0 | 0 | 0 | none matched |
| 5 | linked stops S-041, S-120 | 2 | 0 | 0 | 0 | none matched |
| 6 | linked stops S-081, S-233 | 2 | 0 | 0 | 0 | none matched |
| 7 | linked stops S-1122, S-723 | 2 | 0 | 0 | 0 | none matched |
| 8 | RW 0x0097C819 | 2 | 0 | 0 | 0 | none matched |
| 9 | ObjectCreationUpgrade | 3 | 0 | 0 | 3 | #766 Local 100: upgrade modules: CommandSetUpgrade (515 modules), CostModifierUpgrade, CastleUpgrade, GiveUpgradeUpdate, AudioLoopUpgrade, WallUpgradeUpdate, LocomotorSetUpgrade: 27 uncited own functions, #607 Port 259: Horde-level powers: SplitHordeSpecialPower, HordeDispatchSpecialPower, SiegeDeployHordeSpecialPower, SiegeDeploySpecialPower |
| 10 | linked stops S-080, S-093 | 3 | 0 | 0 | 0 | none matched |
| 11 | linked stops S-2006 | 1 | 0 | 33 | 0 | #599 Port 251: The special-power chain every Ennorath ability runs through: SpecialPowerModule and UnpauseSpecialPowerUpgrade's uncited RotWK functions |
| 12 | RW 0x00853468 | 2 | 0 | 4 | 0 | none matched |
| 13 | RW 0x0073F684 | 1 | 0 | 4 | 0 | none matched |
| 14 | RW 0x00797A96 | 1 | 0 | 0 | 3 | #766 Local 100: upgrade modules: CommandSetUpgrade (515 modules), CostModifierUpgrade, CastleUpgrade, GiveUpgradeUpdate, AudioLoopUpgrade, WallUpgradeUpdate, LocomotorSetUpgrade: 27 uncited own functions, #688 Port 296: Castles вЂ” CastleBehavior, CastleMemberBehavior and FoundationAIUpdate: the uncited own functions (27) and their five unread fields |
| 15 | RespawnUpdate | 1 | 0 | 0 | 3 | #690 Port 298: RespawnUpdate вЂ” hero respawn: its unread DeathAnimationTime and 4 uncited own functions, #567 Port 243: Body damage states: ActiveBody EnteringDamagedTransitionTime and the Damaged / ReallyDamaged attribute modifiers (ActiveBody, RespawnBody) |
| 16 | HordeContain | 5 | 1 | 0 | 2 | #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate, #607 Port 259: Horde-level powers: SplitHordeSpecialPower, HordeDispatchSpecialPower, SiegeDeployHordeSpecialPower, SiegeDeploySpecialPower |
| 17 | GiantBirdSlowDeathBehavior | 1 | 0 | 0 | 2 | none matched |
| 18 | linked stops S-564, S-7100 | 2 | 0 | 0 | 2 | #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate |
| 19 | RW 0x0068B6F5 | 1 | 1 | 0 | 1 | none matched |
| 20 | SubObjectsUpgrade | 1 | 1 | 0 | 1 | #570 Port 246: Client fades: W3DTreeDraw FadeDistance / FadeTarget, W3DFloorDraw FloorFadeRateOnObjectDeath, SubObjectsUpgrade WaitBeforeFadeInSeconds |
| 21 | FlingPassengerSpecialAbilityUpdate | 1 | 0 | 0 | 1 | none matched |
| 22 | RW 0x00646F9C | 1 | 0 | 0 | 0 | none matched |
| 23 | linked stops S-023, S-390 | 2 | 0 | 0 | 0 | none matched |
| 24 | RW 0x0073F24D | 2 | 0 | 0 | 0 | none matched |
| 25 | RW 0x0042D400 | 1 | 0 | 0 | 0 | none matched |
| 26 | RW 0x00436A90 | 1 | 0 | 0 | 0 | none matched |
| 27 | RW 0x004B33FB | 3 | 0 | 0 | 0 | none matched |
| 28 | RW 0x004B67D4 | 1 | 0 | 0 | 0 | none matched |
| 29 | RW 0x004B52C8 | 1 | 0 | 0 | 0 | none matched |
| 30 | RW 0x008E168A | 2 | 0 | 0 | 0 | none matched |
| 31 | RW 0x005E5B42 | 1 | 0 | 0 | 0 | none matched |
| 32 | RW 0x00531C77 | 1 | 0 | 0 | 0 | none matched |
| 33 | RW 0x0077AEAA | 1 | 0 | 0 | 0 | none matched |
| 34 | SpecialDisguiseUpdate | 1 | 0 | 0 | 0 | none matched |
| 35 | FellBeastSwoopPower | 1 | 0 | 0 | 0 | none matched |
| 36 | RW 0x008895D8 | 1 | 0 | 0 | 0 | none matched |
| 37 | RW 0x007FC8E1 | 1 | 0 | 0 | 0 | none matched |
| 38 | RW 0x0062017F | 1 | 0 | 0 | 0 | none matched |
| 39 | linked stops S-145, S-521 | 2 | 0 | 0 | 0 | none matched |
| 40 | RW 0x0068BF75 | 1 | 0 | 0 | 0 | none matched |
| 41 | RW 0x004BF2D8 | 2 | 0 | 0 | 0 | none matched |
| 42 | linked stops S-1651, S-1652 | 2 | 0 | 0 | 0 | none matched |
| 43 | linked stops S-178 | 1 | 0 | 0 | 0 | none matched |
| 44 | RW 0x0068F430 | 1 | 0 | 0 | 0 | none matched |
| 45 | RW 0x0090DEF0 | 1 | 0 | 0 | 0 | none matched |
| 46 | RW 0x00968E59 | 1 | 0 | 0 | 0 | none matched |
| 47 | RW 0x005A07A0 | 1 | 0 | 0 | 0 | none matched |
| 48 | RW 0x00871536 | 1 | 0 | 0 | 0 | none matched |
| 49 | RW 0x00794F65 | 1 | 0 | 0 | 0 | none matched |
| 50 | RW 0x006A7C86 | 2 | 0 | 0 | 0 | none matched |
| 51 | RW 0x0068D524 | 1 | 0 | 0 | 0 | none matched |
| 52 | linked stops S-221, S-7183 | 2 | 0 | 0 | 0 | none matched |
| 53 | RW 0x00452FC2 | 1 | 0 | 0 | 0 | none matched |
| 54 | RW 0x0044DFBA | 1 | 0 | 0 | 0 | #748 Port 312: Radar: the hidden flag and its script actions, the stealth blink's two tests, the attack notice's control flow (S-2453, S-2454, S-2457) |
| 55 | RW 0x005DBF35 | 1 | 0 | 0 | 0 | none matched |
| 56 | RW 0x006A8F42 | 1 | 0 | 0 | 0 | none matched |
| 57 | RW 0x00AE4360 | 1 | 0 | 0 | 0 | none matched |
| 58 | linked stops S-294 | 1 | 0 | 0 | 0 | none matched |
| 59 | RW 0x0091FC9C | 1 | 0 | 0 | 0 | none matched |
| 60 | RW 0x0066352C | 1 | 0 | 0 | 0 | none matched |
| 61 | RW 0x006DCDE9 | 1 | 0 | 0 | 0 | none matched |
| 62 | RW 0x009C8F23 | 1 | 0 | 0 | 0 | none matched |
| 63 | linked stops S-4076, S-4552 | 2 | 0 | 0 | 0 | none matched |
| 64 | RW 0x006A936A | 1 | 0 | 0 | 0 | none matched |
| 65 | RW 0x0060D899 | 1 | 0 | 0 | 0 | none matched |
| 66 | RW 0x0075C7AE | 1 | 0 | 0 | 0 | none matched |
| 67 | RW 0x00646771 | 1 | 0 | 0 | 0 | none matched |
| 68 | linked stops S-4870, S-4873 | 2 | 0 | 0 | 0 | none matched |
| 69 | RW 0x007B212F | 1 | 0 | 0 | 0 | none matched |
| 70 | RW 0x00A14419 | 1 | 0 | 0 | 0 | none matched |
| 71 | RW 0x00991027 | 2 | 0 | 0 | 0 | none matched |
| 72 | DeflectSpecialPower | 1 | 0 | 0 | 0 | none matched |
| 73 | TeleportToCasterSpecialPower | 1 | 0 | 0 | 0 | none matched |
| 74 | RW 0x0059972C | 1 | 0 | 0 | 0 | none matched |
| 75 | RW 0x00537904 | 1 | 0 | 0 | 0 | none matched |
| 76 | RW 0x00857818 | 1 | 0 | 0 | 0 | none matched |
| 77 | RW 0x005DCC88 | 1 | 0 | 0 | 0 | none matched |
| 78 | RW 0x0079E175 | 1 | 0 | 0 | 0 | none matched |
| 79 | RW 0x0053B5C0 | 1 | 0 | 0 | 0 | none matched |
| 80 | RW 0x00470176 | 1 | 0 | 0 | 0 | none matched |
| 81 | RW 0x004B4FFD | 1 | 0 | 0 | 0 | none matched |
| 82 | RW 0x0073B22F | 1 | 0 | 0 | 0 | none matched |
| 83 | W3DFloorDraw | 1 | 0 | 0 | 0 | #570 Port 246: Client fades: W3DTreeDraw FadeDistance / FadeTarget, W3DFloorDraw FloorFadeRateOnObjectDeath, SubObjectsUpgrade WaitBeforeFadeInSeconds |
| 84 | ShareExperienceBehavior | 1 | 0 | 0 | 0 | none matched |
| 85 | WallHubBehavior | 1 | 0 | 0 | 0 | #787 Local 109: builders and walls, second pass: WorkerAIUpdate, DozerAIUpdate, WallHubBehavior, ManTheWallsSpecialPower вЂ” 34 functions |
| 86 | RW 0x006D5C0F | 1 | 0 | 0 | 0 | none matched |
| 87 | RW 0x00736FCD | 1 | 0 | 0 | 0 | none matched |
| 88 | linked stops S-248, S-707 | 3 | 0 | 0 | 2 | #687 Port 295: GettingBuiltBehavior вЂ” the uncited own functions of a building under construction (19), #567 Port 243: Body damage states: ActiveBody EnteringDamagedTransitionTime and the Damaged / ReallyDamaged attribute modifiers (ActiveBody, RespawnBody) |
| 89 | linked stops S-104, S-380 | 3 | 0 | 0 | 0 | none matched |
| 90 | RW 0x008B3744 | 2 | 0 | 0 | 5 | #690 Port 298: RespawnUpdate вЂ” hero respawn: its unread DeathAnimationTime and 4 uncited own functions, #688 Port 296: Castles вЂ” CastleBehavior, CastleMemberBehavior and FoundationAIUpdate: the uncited own functions (27) and their five unread fields, #567 Port 243: Body damage states: ActiveBody EnteringDamagedTransitionTime and the Damaged / ReallyDamaged attribute modifiers (ActiveBody, RespawnBody) |
| 91 | RW 0x005DE588 | 4 | 1 | 0 | 2 | #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate |
| 92 | linked stops S-031, S-1650 | 2 | 0 | 0 | 0 | none matched |
| 93 | linked stops S-1125, S-722 | 2 | 0 | 0 | 0 | none matched |
| 94 | RW 0x0062D7EA | 2 | 0 | 0 | 0 | none matched |
| 95 | linked stops S-1409, S-177 | 2 | 0 | 0 | 0 | none matched |
| 96 | linked stops S-1930, S-245 | 2 | 0 | 0 | 0 | none matched |
| 97 | RW 0x008D861A | 2 | 0 | 0 | 0 | none matched |
| 98 | RW 0x00674FFE | 2 | 0 | 0 | 0 | none matched |
| 99 | GateOpenAndCloseBehavior | 2 | 0 | 0 | 0 | none matched |
| 100 | linked stops S-482, S-5202 | 3 | 0 | 0 | 0 | #787 Local 109: builders and walls, second pass: WorkerAIUpdate, DozerAIUpdate, WallHubBehavior, ManTheWallsSpecialPower вЂ” 34 functions |
| 101 | RW 0x00628882 | 8 | 0 | 4 | 1 | #570 Port 246: Client fades: W3DTreeDraw FadeDistance / FadeTarget, W3DFloorDraw FloorFadeRateOnObjectDeath, SubObjectsUpgrade WaitBeforeFadeInSeconds |
| 102 | RW 0x00853EDF | 1 | 0 | 5 | 0 | none matched |
| 103 | RW 0x00851E9F | 3 | 0 | 4 | 1 | none matched |
| 104 | SpecialAbilityUpdate | 1 | 2 | 4 | 0 | none matched |
| 105 | W3DHordeModelDraw | 1 | 2 | 0 | 5 | #569 Port 245: W3DScriptedModelDraw: MinLODRequired, HighDetailOnly, UseProducerTexture, UseDefaultAnimation, BirthFadeAdditive |
| 106 | LargeGroupBonusUpdate | 2 | 2 | 0 | 3 | #700 Port 308: Small module gaps with candidate readers вЂ” LargeGroupBonusUpdate.AlliesOnly, W3DLightDraw.Radius, EvaAnnounceClientCreate.CreateFakeRadarEvent and their classes' uncited functions, #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate |
| 107 | RW 0x00731010 | 1 | 0 | 0 | 3 | #688 Port 296: Castles вЂ” CastleBehavior, CastleMemberBehavior and FoundationAIUpdate: the uncited own functions (27) and their five unread fields |
| 108 | RW 0x006955BC | 3 | 0 | 0 | 2 | #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate |
| 109 | linked stops S-1426, S-167 | 3 | 0 | 0 | 2 | #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate |
| 110 | ClearanceTestingSlowDeathBehavior | 1 | 1 | 0 | 1 | none matched |
| 111 | RW 0x0067C5F3 | 1 | 2 | 0 | 1 | #688 Port 296: Castles вЂ” CastleBehavior, CastleMemberBehavior and FoundationAIUpdate: the uncited own functions (27) and their five unread fields |
| 112 | RW 0x004D6FFA | 1 | 0 | 0 | 0 | none matched |
| 113 | RW 0x007405B1 | 1 | 0 | 0 | 0 | none matched |
| 114 | RW 0x0042DB44 | 1 | 0 | 0 | 0 | none matched |
| 115 | RW 0x0063CEF1 | 2 | 0 | 0 | 0 | none matched |
| 116 | RW 0x008D7FD2 | 1 | 0 | 0 | 0 | none matched |
| 117 | RW 0x006011F3 | 1 | 0 | 0 | 0 | none matched |
| 118 | RW 0x00AD3520 | 2 | 1 | 0 | 0 | #766 Local 100: upgrade modules: CommandSetUpgrade (515 modules), CostModifierUpgrade, CastleUpgrade, GiveUpgradeUpdate, AudioLoopUpgrade, WallUpgradeUpdate, LocomotorSetUpgrade: 27 uncited own functions |
| 119 | RW 0x00734E94 | 3 | 0 | 0 | 0 | #687 Port 295: GettingBuiltBehavior вЂ” the uncited own functions of a building under construction (19), #570 Port 246: Client fades: W3DTreeDraw FadeDistance / FadeTarget, W3DFloorDraw FloorFadeRateOnObjectDeath, SubObjectsUpgrade WaitBeforeFadeInSeconds |
| 120 | RW 0x00AF4890 | 1 | 0 | 0 | 0 | none matched |
| 121 | RW 0x007F2A14 | 1 | 0 | 0 | 0 | none matched |
| 122 | RW 0x007B970F | 1 | 0 | 0 | 0 | none matched |
| 123 | RW 0x006BE6A6 | 1 | 0 | 0 | 0 | none matched |
| 124 | RW 0x00625886 | 1 | 0 | 0 | 0 | none matched |
| 125 | RW 0x0079E4D8 | 1 | 0 | 0 | 0 | none matched |
| 126 | RW 0x009C293D | 1 | 0 | 0 | 0 | none matched |
| 127 | RW 0x005FE852 | 1 | 0 | 0 | 0 | none matched |
| 128 | RW 0x00477D1C | 1 | 0 | 0 | 0 | none matched |
| 129 | RW 0x005F2EBC | 1 | 0 | 0 | 0 | none matched |
| 130 | RW 0x0064DFA5 | 1 | 0 | 0 | 0 | none matched |
| 131 | RW 0x007836C0 | 1 | 0 | 0 | 0 | none matched |
| 132 | RW 0x00590C90 | 1 | 0 | 0 | 0 | none matched |
| 133 | RW 0x005E821A | 1 | 0 | 0 | 0 | none matched |
| 134 | RW 0x009A4C70 | 1 | 0 | 0 | 0 | none matched |
| 135 | RW 0x0075C02D | 1 | 0 | 0 | 0 | none matched |
| 136 | linked stops S-282, S-450 | 2 | 0 | 0 | 0 | none matched |
| 137 | StructureToppleUpdate | 1 | 0 | 0 | 0 | none matched |
| 138 | RW 0x0048B7B1 | 1 | 0 | 0 | 0 | none matched |
| 139 | RW 0x005E0A39 | 1 | 1 | 0 | 0 | none matched |
| 140 | RW 0x0080731E | 1 | 0 | 0 | 0 | none matched |
| 141 | RW 0x0061B083 | 1 | 0 | 0 | 0 | none matched |
| 142 | RW 0x008D3AF1 | 1 | 0 | 0 | 0 | none matched |
| 143 | RW 0x009BEA20 | 1 | 0 | 0 | 0 | none matched |
| 144 | RW 0x00403111 | 1 | 0 | 0 | 0 | none matched |
| 145 | RW 0x0099187C | 1 | 0 | 0 | 0 | none matched |
| 146 | RW 0x005DFBEC | 1 | 0 | 0 | 0 | none matched |
| 147 | RW 0x006898E3 | 1 | 0 | 0 | 0 | none matched |
| 148 | RW 0x00696FD2 | 1 | 0 | 0 | 0 | none matched |
| 149 | RW 0x00691602 | 1 | 0 | 0 | 0 | #750 Port 314: Ghost objects: a destroyed building stays in the fog for players who saw it (S-561, S-4970) |
| 150 | GiveUpgradeUpdate | 1 | 0 | 0 | 0 | #766 Local 100: upgrade modules: CommandSetUpgrade (515 modules), CostModifierUpgrade, CastleUpgrade, GiveUpgradeUpdate, AudioLoopUpgrade, WallUpgradeUpdate, LocomotorSetUpgrade: 27 uncited own functions |
| 151 | ModelConditionAudioLoopClientBehavior | 1 | 0 | 0 | 0 | none matched |
| 152 | StrafeAreaUpdate | 1 | 0 | 0 | 0 | none matched |
| 153 | RW 0x006897B9 | 1 | 0 | 0 | 0 | none matched |
| 154 | RW 0x004BCE68 | 1 | 0 | 0 | 0 | none matched |
| 155 | RW 0x004C6514 | 1 | 0 | 0 | 0 | none matched |
| 156 | RW 0x00671ED8 | 1 | 0 | 0 | 0 | none matched |
| 157 | RW 0x005F11A8 | 1 | 0 | 0 | 0 | none matched |
| 158 | RW 0x006FF412 | 1 | 0 | 0 | 0 | none matched |
| 159 | RW 0x0074A4EE | 1 | 0 | 0 | 0 | none matched |
| 160 | RW 0x006FEEF7 | 1 | 0 | 0 | 0 | none matched |
| 161 | RW 0x0063256F | 1 | 0 | 0 | 0 | none matched |
| 162 | RW 0x0088D993 | 5 | 0 | 0 | 0 | #787 Local 109: builders and walls, second pass: WorkerAIUpdate, DozerAIUpdate, WallHubBehavior, ManTheWallsSpecialPower вЂ” 34 functions, #687 Port 295: GettingBuiltBehavior вЂ” the uncited own functions of a building under construction (19) |
| 163 | RW 0x004CF884 | 3 | 0 | 0 | 0 | none matched |
| 164 | RW 0x0044C84A | 3 | 0 | 0 | 0 | none matched |
| 165 | RW 0x0083C29E | 3 | 0 | 0 | 0 | none matched |
| 166 | RW 0x0066C0B9 | 3 | 1 | 0 | 0 | #533 Port 223: the attack machine's Pursue state (a fleeing victim) as RotWK вЂ” mounted heroes stop short of a running single target (P-150, S-325), #515 Port 218: Object / WeaponSet getAbleToAttackSpecificObject (RW 0x68D6A6 / 0x6C9147) as RotWK, for every consumer: the attack machine, closest enemy, guard, garrison, turret, orders (S-325) |
| 167 | RW 0x00881FF3 | 3 | 0 | 0 | 0 | none matched |
| 168 | RW 0x00693919 | 2 | 0 | 6 | 2 | #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate |
| 169 | RW 0x008558C0 | 2 | 0 | 4 | 0 | #766 Local 100: upgrade modules: CommandSetUpgrade (515 modules), CostModifierUpgrade, CastleUpgrade, GiveUpgradeUpdate, AudioLoopUpgrade, WallUpgradeUpdate, LocomotorSetUpgrade: 27 uncited own functions |
| 170 | RW 0x0054BDE0 | 4 | 0 | 0 | 6 | #570 Port 246: Client fades: W3DTreeDraw FadeDistance / FadeTarget, W3DFloorDraw FloorFadeRateOnObjectDeath, SubObjectsUpgrade WaitBeforeFadeInSeconds, #569 Port 245: W3DScriptedModelDraw: MinLODRequired, HighDetailOnly, UseProducerTexture, UseDefaultAnimation, BirthFadeAdditive |
| 171 | RW 0x00535C5A | 2 | 5 | 0 | 5 | #569 Port 245: W3DScriptedModelDraw: MinLODRequired, HighDetailOnly, UseProducerTexture, UseDefaultAnimation, BirthFadeAdditive |
| 172 | RW 0x004B686D | 4 | 0 | 0 | 1 | #687 Port 295: GettingBuiltBehavior вЂ” the uncited own functions of a building under construction (19), #570 Port 246: Client fades: W3DTreeDraw FadeDistance / FadeTarget, W3DFloorDraw FloorFadeRateOnObjectDeath, SubObjectsUpgrade WaitBeforeFadeInSeconds |
| 173 | RW 0x006AA688 | 2 | 1 | 0 | 0 | #787 Local 109: builders and walls, second pass: WorkerAIUpdate, DozerAIUpdate, WallHubBehavior, ManTheWallsSpecialPower вЂ” 34 functions, #465 Port 198: SpawnBehavior: the spawn interface's attack / idle orders and Player::onUnitCreated (S-1425) |
| 174 | RW 0x00626A87 | 2 | 0 | 0 | 0 | none matched |
| 175 | RW 0x0046DFBA | 2 | 0 | 0 | 0 | none matched |
| 176 | RW 0x00693D0C | 3 | 0 | 0 | 5 | #567 Port 243: Body damage states: ActiveBody EnteringDamagedTransitionTime and the Damaged / ReallyDamaged attribute modifiers (ActiveBody, RespawnBody) |
| 177 | ActiveBody | 3 | 0 | 0 | 4 | #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate, #567 Port 243: Body damage states: ActiveBody EnteringDamagedTransitionTime and the Damaged / ReallyDamaged attribute modifiers (ActiveBody, RespawnBody) |
| 178 | RW 0x00814BEC | 4 | 0 | 0 | 0 | none matched |
| 179 | RW 0x0082D7F2 | 3 | 1 | 8 | 1 | none matched |
| 180 | HordeAIUpdate | 1 | 0 | 0 | 2 | #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate, #650 Port 283: HorseHordeContain and HordeAIUpdate: the cavalry horde's uncited own functions (classify, then port the missing ones) |
| 181 | RW 0x0073B723 | 1 | 0 | 0 | 1 | #700 Port 308: Small module gaps with candidate readers вЂ” LargeGroupBonusUpdate.AlliesOnly, W3DLightDraw.Radius, EvaAnnounceClientCreate.CreateFakeRadarEvent and their classes' uncited functions |
| 182 | RW 0x007B68F9 | 1 | 0 | 0 | 0 | none matched |
| 183 | RW 0x00AF4DE0 | 1 | 0 | 0 | 0 | none matched |
| 184 | RW 0x00602FFE | 3 | 0 | 0 | 0 | none matched |
| 185 | RW 0x004DD1F4 | 1 | 2 | 0 | 0 | none matched |
| 186 | RW 0x005DC46C | 1 | 0 | 0 | 0 | none matched |
| 187 | RW 0x006789B4 | 1 | 0 | 0 | 0 | none matched |
| 188 | RW 0x007396E8 | 1 | 0 | 0 | 0 | none matched |
| 189 | RW 0x006625E8 | 1 | 2 | 0 | 0 | none matched |
| 190 | RW 0x004052F9 | 1 | 0 | 0 | 0 | none matched |
| 191 | RW 0x006E1A40 | 1 | 0 | 0 | 0 | none matched |
| 192 | RW 0x0062605D | 1 | 0 | 0 | 0 | none matched |
| 193 | RW 0x004533A9 | 1 | 0 | 0 | 0 | none matched |
| 194 | RW 0x006EF591 | 1 | 0 | 0 | 0 | none matched |
| 195 | RW 0x00442D32 | 1 | 0 | 0 | 0 | none matched |
| 196 | RW 0x005DCB28 | 1 | 0 | 0 | 0 | none matched |
| 197 | RW 0x006235E2 | 4 | 0 | 0 | 0 | none matched |
| 198 | RW 0x00662D70 | 1 | 0 | 0 | 0 | none matched |
| 199 | RW 0x00812C89 | 1 | 0 | 0 | 0 | none matched |
| 200 | RW 0x0044CD2D | 1 | 0 | 0 | 0 | none matched |
| 201 | RW 0x004500B9 | 1 | 0 | 0 | 0 | none matched |
| 202 | RW 0x006429AD | 1 | 0 | 0 | 0 | none matched |
| 203 | RW 0x006AAA2E | 1 | 0 | 0 | 0 | #787 Local 109: builders and walls, second pass: WorkerAIUpdate, DozerAIUpdate, WallHubBehavior, ManTheWallsSpecialPower вЂ” 34 functions |
| 204 | RW 0x0074697A | 1 | 0 | 0 | 0 | none matched |
| 205 | RW 0x005E2E2E | 1 | 0 | 0 | 0 | none matched |
| 206 | RW 0x00615871 | 1 | 0 | 0 | 0 | none matched |
| 207 | RW 0x0071A1A7 | 1 | 0 | 0 | 0 | none matched |
| 208 | RW 0x0068BB14 | 1 | 0 | 0 | 0 | none matched |
| 209 | RW 0x005EAB0A | 1 | 0 | 0 | 0 | none matched |
| 210 | RW 0x004D12FA | 1 | 0 | 0 | 0 | none matched |
| 211 | RW 0x00441E23 | 1 | 0 | 0 | 0 | none matched |
| 212 | RW 0x004E5D2C | 1 | 0 | 0 | 0 | none matched |
| 213 | linked stops S-5151 | 1 | 0 | 0 | 0 | none matched |
| 214 | linked stops S-526, S-710 | 3 | 0 | 0 | 0 | none matched |
| 215 | AODHordeContain | 1 | 0 | 0 | 0 | none matched |
| 216 | RW 0x00B501A0 | 1 | 0 | 0 | 0 | none matched |
| 217 | RW 0x008C1025 | 1 | 0 | 0 | 0 | none matched |
| 218 | RW 0x006AAFC3 | 1 | 0 | 0 | 0 | none matched |
| 219 | RW 0x0068C3A3 | 1 | 0 | 0 | 0 | none matched |
| 220 | RW 0x005F3664 | 1 | 0 | 0 | 0 | none matched |
| 221 | AIUpdateInterface | 3 | 9 | 0 | 0 | none matched |
| 222 | SupplyWarehouseDockUpdate | 2 | 0 | 0 | 0 | none matched |
| 223 | RW 0x0068EF58 | 1 | 0 | 0 | 0 | none matched |
| 224 | RW 0x004A8F95 | 4 | 0 | 0 | 0 | none matched |
| 225 | RW 0x007510EC | 4 | 0 | 0 | 0 | none matched |
| 226 | RW 0x00851E97 | 2 | 3 | 0 | 8 | #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate, #650 Port 283: HorseHordeContain and HordeAIUpdate: the cavalry horde's uncited own functions (classify, then port the missing ones), #567 Port 243: Body damage states: ActiveBody EnteringDamagedTransitionTime and the Damaged / ReallyDamaged attribute modifiers (ActiveBody, RespawnBody) |
| 227 | RW 0x0087F7B4 | 4 | 1 | 0 | 6 | #766 Local 100: upgrade modules: CommandSetUpgrade (515 modules), CostModifierUpgrade, CastleUpgrade, GiveUpgradeUpdate, AudioLoopUpgrade, WallUpgradeUpdate, LocomotorSetUpgrade: 27 uncited own functions, #688 Port 296: Castles вЂ” CastleBehavior, CastleMemberBehavior and FoundationAIUpdate: the uncited own functions (27) and their five unread fields, #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate, #607 Port 259: Horde-level powers: SplitHordeSpecialPower, HordeDispatchSpecialPower, SiegeDeployHordeSpecialPower, SiegeDeploySpecialPower, #296 [deferred] Local 40: bridge logic as retail: towers, damage stages, pathfinding layer, radar (BridgeBehavior, S-7223) |
| 228 | RW 0x004516DE | 2 | 0 | 0 | 0 | none matched |
| 229 | RW 0x0091BE64 | 2 | 0 | 0 | 0 | none matched |
| 230 | RW 0x009D2B49 | 2 | 0 | 0 | 0 | none matched |
| 231 | RW 0x004C451D | 5 | 1 | 0 | 5 | #569 Port 245: W3DScriptedModelDraw: MinLODRequired, HighDetailOnly, UseProducerTexture, UseDefaultAnimation, BirthFadeAdditive |
| 232 | OCLSpecialPower | 1 | 2 | 11 | 0 | none matched |
| 233 | RW 0x005F00CA | 1 | 0 | 11 | 0 | none matched |
| 234 | SalvageCrateCollide | 1 | 0 | 0 | 4 | #695 Port 303: SalvageCrateCollide вЂ” crates: its four unread chances and radius and 3 uncited functions |
| 235 | RW 0x00875FB2 | 3 | 1 | 0 | 2 | #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate |
| 236 | RW 0x0068BBE0 | 1 | 0 | 0 | 2 | #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate |
| 237 | RW 0x0092F778 | 1 | 0 | 0 | 1 | none matched |
| 238 | RW 0x00467B4D | 1 | 3 | 0 | 0 | #570 Port 246: Client fades: W3DTreeDraw FadeDistance / FadeTarget, W3DFloorDraw FloorFadeRateOnObjectDeath, SubObjectsUpgrade WaitBeforeFadeInSeconds |
| 239 | RW 0x0045CB3B | 1 | 0 | 0 | 0 | none matched |
| 240 | RW 0x006B5F78 | 1 | 0 | 0 | 0 | none matched |
| 241 | RW 0x006E189C | 1 | 0 | 0 | 0 | none matched |
| 242 | RW 0x0064148E | 2 | 0 | 0 | 0 | none matched |
| 243 | RW 0x006729A3 | 1 | 2 | 0 | 0 | none matched |
| 244 | RW 0x006ED071 | 1 | 0 | 0 | 0 | none matched |
| 245 | RW 0x0082DFB7 | 4 | 0 | 0 | 0 | #766 Local 100: upgrade modules: CommandSetUpgrade (515 modules), CostModifierUpgrade, CastleUpgrade, GiveUpgradeUpdate, AudioLoopUpgrade, WallUpgradeUpdate, LocomotorSetUpgrade: 27 uncited own functions |
| 246 | RW 0x0091EC52 | 3 | 0 | 0 | 0 | none matched |
| 247 | RW 0x006D1305 | 1 | 0 | 0 | 0 | none matched |
| 248 | RW 0x0077208A | 1 | 0 | 0 | 0 | none matched |
| 249 | RW 0x005E3F49 | 1 | 1 | 0 | 0 | none matched |
| 250 | RW 0x00808F53 | 1 | 0 | 0 | 0 | none matched |
| 251 | RW 0x0044F3AB | 2 | 1 | 0 | 0 | none matched |
| 252 | RW 0x00795221 | 1 | 0 | 0 | 0 | #787 Local 109: builders and walls, second pass: WorkerAIUpdate, DozerAIUpdate, WallHubBehavior, ManTheWallsSpecialPower вЂ” 34 functions |
| 253 | RW 0x00441E4A | 1 | 0 | 0 | 0 | none matched |
| 254 | RW 0x006308F3 | 2 | 0 | 0 | 0 | none matched |
| 255 | RW 0x0066110A | 1 | 0 | 0 | 0 | none matched |
| 256 | RW 0x00683D53 | 1 | 0 | 0 | 0 | none matched |
| 257 | RW 0x0060331E | 1 | 0 | 0 | 0 | none matched |
| 258 | RW 0x009C4EBD | 1 | 0 | 0 | 0 | none matched |
| 259 | RW 0x00582070 | 1 | 0 | 0 | 0 | none matched |
| 260 | OilSpillUpdate | 1 | 0 | 0 | 0 | none matched |
| 261 | RW 0x00676A8A | 1 | 0 | 0 | 0 | none matched |
| 262 | RW 0x0042E59E | 1 | 0 | 0 | 0 | none matched |
| 263 | RW 0x006CD0BE | 1 | 0 | 0 | 0 | none matched |
| 264 | RW 0x00670570 | 1 | 0 | 0 | 0 | none matched |
| 265 | RW 0x006886E0 | 1 | 1 | 0 | 0 | none matched |
| 266 | AIUpdateInterface | 4 | 16 | 0 | 0 | #787 Local 109: builders and walls, second pass: WorkerAIUpdate, DozerAIUpdate, WallHubBehavior, ManTheWallsSpecialPower вЂ” 34 functions, #650 Port 283: HorseHordeContain and HordeAIUpdate: the cavalry horde's uncited own functions (classify, then port the missing ones) |
| 267 | AISpecialPowerUpdate | 1 | 0 | 0 | 0 | none matched |
| 268 | RW 0x0066E233 | 1 | 0 | 0 | 0 | none matched |
| 269 | RW 0x0081E60B | 8 | 1 | 0 | 7 | #787 Local 109: builders and walls, second pass: WorkerAIUpdate, DozerAIUpdate, WallHubBehavior, ManTheWallsSpecialPower вЂ” 34 functions, #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate, #569 Port 245: W3DScriptedModelDraw: MinLODRequired, HighDetailOnly, UseProducerTexture, UseDefaultAnimation, BirthFadeAdditive, #296 [deferred] Local 40: bridge logic as retail: towers, damage stages, pathfinding layer, radar (BridgeBehavior, S-7223) |
| 270 | SymbioticStructuresBody | 3 | 11 | 0 | 3 | #567 Port 243: Body damage states: ActiveBody EnteringDamagedTransitionTime and the Damaged / ReallyDamaged attribute modifiers (ActiveBody, RespawnBody) |
| 271 | RW 0x00989449 | 6 | 0 | 0 | 0 | none matched |
| 272 | RW 0x00B5470F | 2 | 0 | 0 | 0 | none matched |
| 273 | RW 0x0062215B | 2 | 0 | 0 | 0 | none matched |
| 274 | linked stops S-179, S-3504 | 2 | 0 | 0 | 0 | none matched |
| 275 | linked stops S-1892, S-1920 | 2 | 0 | 0 | 0 | none matched |
| 276 | RW 0x0045C675 | 2 | 0 | 0 | 0 | none matched |
| 277 | RW 0x0044D25B | 2 | 0 | 0 | 0 | none matched |
| 278 | RW 0x00988BB5 | 2 | 0 | 0 | 0 | none matched |
| 279 | RW 0x006E9B92 | 2 | 0 | 0 | 0 | none matched |
| 280 | RW 0x00942733 | 5 | 1 | 0 | 3 | #688 Port 296: Castles вЂ” CastleBehavior, CastleMemberBehavior and FoundationAIUpdate: the uncited own functions (27) and their five unread fields, #567 Port 243: Body damage states: ActiveBody EnteringDamagedTransitionTime and the Damaged / ReallyDamaged attribute modifiers (ActiveBody, RespawnBody) |
| 281 | RW 0x0088FF6F | 3 | 1 | 0 | 6 | #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate, #567 Port 243: Body damage states: ActiveBody EnteringDamagedTransitionTime and the Damaged / ReallyDamaged attribute modifiers (ActiveBody, RespawnBody) |
| 282 | RW 0x0087468B | 7 | 1 | 0 | 2 | #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate, #492 Port 210: Horde wall scaling: connect the collide entry so goblins and spiderlings climb walls (S-4461) |
| 283 | W3DScriptedModelDraw | 1 | 5 | 0 | 5 | #569 Port 245: W3DScriptedModelDraw: MinLODRequired, HighDetailOnly, UseProducerTexture, UseDefaultAnimation, BirthFadeAdditive |
| 284 | RW 0x0070E911 | 2 | 0 | 0 | 0 | none matched |
| 285 | RW 0x006311ED | 4 | 0 | 0 | 0 | none matched |
| 286 | RW 0x00694CCC | 1 | 0 | 0 | 0 | none matched |
| 287 | RW 0x006BBD55 | 1 | 0 | 0 | 0 | none matched |
| 288 | RW 0x0061D664 | 1 | 0 | 0 | 0 | none matched |
| 289 | RW 0x0080CDA2 | 1 | 0 | 0 | 0 | none matched |
| 290 | RW 0x00490EE6 | 2 | 0 | 0 | 0 | none matched |
| 291 | RW 0x00693C2D | 1 | 0 | 0 | 0 | #749 Port 313: Hold fire / auto-acquire buttons: the button image toggle and the related object's flag (S-1983, S-1992) |
| 292 | RW 0x0064CEBF | 1 | 0 | 0 | 0 | none matched |
| 293 | RW 0x006230B6 | 1 | 0 | 0 | 0 | none matched |
| 294 | RW 0x00443D5F | 1 | 0 | 0 | 0 | none matched |
| 295 | RW 0x00472480 | 1 | 0 | 0 | 0 | none matched |
| 296 | PhysicsBehavior | 1 | 0 | 0 | 0 | none matched |
| 297 | RW 0x008431B8 | 4 | 0 | 0 | 0 | none matched |
| 298 | RW 0x0066139C | 3 | 1 | 0 | 1 | #754 Port 318: AerialPathfindNoFlyZone: RotWK's block parser (TriggerArea, Height) and the no-fly zone test of flyers (S-7338, S-4041) |
| 299 | RW 0x00940435 | 8 | 0 | 12 | 1 | #749 Port 313: Hold fire / auto-acquire buttons: the button image toggle and the related object's flag (S-1983, S-1992) |
| 300 | RW 0x00763D11 | 2 | 1 | 15 | 0 | none matched |
| 301 | RW 0x00696FD2 | 2 | 0 | 12 | 1 | none matched |
| 302 | RW 0x0042EEFA | 2 | 2 | 0 | 6 | #695 Port 303: SalvageCrateCollide вЂ” crates: its four unread chances and radius and 3 uncited functions, #567 Port 243: Body damage states: ActiveBody EnteringDamagedTransitionTime and the Damaged / ReallyDamaged attribute modifiers (ActiveBody, RespawnBody) |
| 303 | RW 0x00668AF8 | 2 | 0 | 0 | 2 | #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate |
| 304 | linked stops S-283, S-6010 | 2 | 0 | 0 | 0 | none matched |
| 305 | RW 0x006A2AE5 | 3 | 0 | 0 | 0 | #757 Port 321: Wall line build from the HUD: the release's restricted-area test and the line ghost (S-1770) |
| 306 | RW 0x006732DC | 3 | 1 | 0 | 0 | none matched |
| 307 | RW 0x0046F48E | 4 | 1 | 0 | 2 | #750 Port 314: Ghost objects: a destroyed building stays in the fog for players who saw it (S-561, S-4970), #700 Port 308: Small module gaps with candidate readers вЂ” LargeGroupBonusUpdate.AlliesOnly, W3DLightDraw.Radius, EvaAnnounceClientCreate.CreateFakeRadarEvent and their classes' uncited functions |
| 308 | RW 0x0042EC5E | 4 | 1 | 0 | 1 | none matched |
| 309 | RW 0x0068FA3D | 5 | 1 | 7 | 2 | none matched |
| 310 | RW 0x00683D89 | 3 | 0 | 11 | 4 | #625 Port 266: Tree fading: the tree buffer's per-tree target opacity (W3DTreeDraw FadeDistance / FadeTarget, GameData AllowTreeFading / TreeFadeObjectFilter), split from Port 246 #570, #570 Port 246: Client fades: W3DTreeDraw FadeDistance / FadeTarget, W3DFloorDraw FloorFadeRateOnObjectDeath, SubObjectsUpgrade WaitBeforeFadeInSeconds |
| 311 | RW 0x008705A6 | 2 | 0 | 4 | 0 | none matched |
| 312 | RW 0x0073FB7A | 3 | 1 | 0 | 2 | #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate |
| 313 | RW 0x00478289 | 1 | 0 | 0 | 0 | none matched |
| 314 | RW 0x0081F8D8 | 7 | 0 | 0 | 0 | none matched |
| 315 | RW 0x00680B70 | 2 | 0 | 0 | 0 | none matched |
| 316 | RW 0x00674727 | 2 | 5 | 0 | 0 | none matched |
| 317 | RW 0x005E2F24 | 1 | 0 | 0 | 0 | none matched |
| 318 | RW 0x007052BE | 1 | 0 | 0 | 0 | none matched |
| 319 | RW 0x004A225B | 1 | 0 | 0 | 0 | none matched |
| 320 | RW 0x0044FDF3 | 1 | 1 | 0 | 0 | #748 Port 312: Radar: the hidden flag and its script actions, the stealth blink's two tests, the attack notice's control flow (S-2453, S-2454, S-2457) |
| 321 | RW 0x0073D0BA | 1 | 0 | 0 | 0 | none matched |
| 322 | RW 0x006A3755 | 2 | 0 | 0 | 0 | none matched |
| 323 | linked stops S-4120, S-5010 | 2 | 0 | 0 | 0 | none matched |
| 324 | RW 0x008F069A | 1 | 0 | 0 | 0 | none matched |
| 325 | RW 0x0062550D | 1 | 0 | 0 | 0 | none matched |
| 326 | RW 0x00664D7E | 1 | 0 | 0 | 0 | none matched |
| 327 | RW 0x0044DE58 | 1 | 0 | 0 | 0 | none matched |
| 328 | RW 0x00504019 | 1 | 0 | 0 | 0 | none matched |
| 329 | RW 0x0068B3BD | 1 | 0 | 0 | 0 | #650 Port 283: HorseHordeContain and HordeAIUpdate: the cavalry horde's uncited own functions (classify, then port the missing ones) |
| 330 | RW 0x0062E4E8 | 9 | 1 | 0 | 2 | #756 Port 320: Victory: computer players die when no human is left, the shared AI branch, the defeat frames and the defeated player's map reveal (S-1062), #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate |
| 331 | RW 0x0073193D | 3 | 0 | 0 | 0 | none matched |
| 332 | RW 0x0081C5D3 | 2 | 0 | 0 | 0 | none matched |
| 333 | RW 0x0083B471 | 2 | 0 | 0 | 0 | none matched |
| 334 | RW 0x007486FD | 2 | 0 | 0 | 0 | none matched |
| 335 | RW 0x00697F28 | 3 | 0 | 0 | 2 | #787 Local 109: builders and walls, second pass: WorkerAIUpdate, DozerAIUpdate, WallHubBehavior, ManTheWallsSpecialPower вЂ” 34 functions, #567 Port 243: Body damage states: ActiveBody EnteringDamagedTransitionTime and the Damaged / ReallyDamaged attribute modifiers (ActiveBody, RespawnBody) |
| 336 | RW 0x00779A3D | 3 | 0 | 0 | 0 | none matched |
| 337 | RW 0x0071EBDA | 3 | 0 | 0 | 0 | #687 Port 295: GettingBuiltBehavior вЂ” the uncited own functions of a building under construction (19) |
| 338 | RW 0x00A149A2 | 3 | 0 | 0 | 0 | none matched |
| 339 | RW 0x0062F91A | 5 | 0 | 0 | 3 | #688 Port 296: Castles вЂ” CastleBehavior, CastleMemberBehavior and FoundationAIUpdate: the uncited own functions (27) and their five unread fields |
| 340 | RW 0x0068C7E9 | 3 | 1 | 0 | 2 | #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate |
| 341 | RW 0x006E8707 | 2 | 1 | 0 | 2 | #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate |
| 342 | RW 0x0068C053 | 2 | 0 | 0 | 2 | #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate |
| 343 | RW 0x00816655 | 3 | 0 | 0 | 0 | none matched |
| 344 | RW 0x00454186 | 1 | 0 | 0 | 0 | none matched |
| 345 | RW 0x00696266 | 1 | 0 | 0 | 0 | #465 Port 198: SpawnBehavior: the spawn interface's attack / idle orders and Player::onUnitCreated (S-1425) |
| 346 | RW 0x008208D4 | 1 | 0 | 0 | 0 | none matched |
| 347 | RW 0x0092F082 | 3 | 1 | 0 | 0 | none matched |
| 348 | RW 0x007846EE | 1 | 0 | 0 | 0 | none matched |
| 349 | RW 0x009EDA17 | 1 | 0 | 0 | 0 | none matched |
| 350 | RW 0x0068E43B | 1 | 0 | 0 | 0 | none matched |
| 351 | RW 0x005E31DF | 1 | 1 | 0 | 0 | none matched |
| 352 | RW 0x006AF269 | 2 | 1 | 33 | 0 | #755 Port 319: Team state: the members' trigger flags (+ 0x5C), the team events list and the attacker records' expiry (S-1365), #599 Port 251: The special-power chain every Ennorath ability runs through: SpecialPowerModule and UnpauseSpecialPowerUpgrade's uncited RotWK functions |
| 353 | RW 0x0080ACE3 | 2 | 0 | 0 | 0 | none matched |
| 354 | RW 0x00669932 | 5 | 0 | 0 | 3 | #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate, #296 [deferred] Local 40: bridge logic as retail: towers, damage stages, pathfinding layer, radar (BridgeBehavior, S-7223) |
| 355 | RW 0x00701443 | 8 | 1 | 40 | 1 | #753 Port 317: AI guard: object and team guard targets with the script guard commands, the turret target and the outer re-entry (S-3802), #751 Port 315: AI hunt: the crate pick-up state and the attack priority info (S-1188), #687 Port 295: GettingBuiltBehavior вЂ” the uncited own functions of a building under construction (19), #599 Port 251: The special-power chain every Ennorath ability runs through: SpecialPowerModule and UnpauseSpecialPowerUpgrade's uncited RotWK functions |
| 356 | RW 0x006AA91D | 1 | 0 | 44 | 0 | #599 Port 251: The special-power chain every Ennorath ability runs through: SpecialPowerModule and UnpauseSpecialPowerUpgrade's uncited RotWK functions |
| 357 | AutoAbilityBehavior | 1 | 5 | 12 | 1 | none matched |
| 358 | RW 0x006CAB85 | 4 | 0 | 5 | 2 | #567 Port 243: Body damage states: ActiveBody EnteringDamagedTransitionTime and the Damaged / ReallyDamaged attribute modifiers (ActiveBody, RespawnBody), #467 Port 200 (parked: after Port 218): TurretAI: the mood timer and matrix, isAbleToAttack / the continued-target test, slot 0x1C4 (S-1106) |
| 359 | RW 0x0068B244 | 1 | 0 | 0 | 0 | none matched |
| 360 | RW 0x00824448 | 1 | 0 | 0 | 0 | none matched |
| 361 | RW 0x0069990F | 2 | 1 | 0 | 0 | none matched |
| 362 | RW 0x005E9BEF | 2 | 0 | 0 | 0 | none matched |
| 363 | DynamicPortalBehaviour | 1 | 0 | 0 | 0 | #688 Port 296: Castles вЂ” CastleBehavior, CastleMemberBehavior and FoundationAIUpdate: the uncited own functions (27) and their five unread fields |
| 364 | RW 0x008F3EFE | 1 | 0 | 0 | 0 | none matched |
| 365 | RW 0x00885CD5 | 2 | 0 | 0 | 4 | #766 Local 100: upgrade modules: CommandSetUpgrade (515 modules), CostModifierUpgrade, CastleUpgrade, GiveUpgradeUpdate, AudioLoopUpgrade, WallUpgradeUpdate, LocomotorSetUpgrade: 27 uncited own functions, #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate, #567 Port 243: Body damage states: ActiveBody EnteringDamagedTransitionTime and the Damaged / ReallyDamaged attribute modifiers (ActiveBody, RespawnBody) |
| 366 | RW 0x0046448B | 2 | 0 | 0 | 1 | #700 Port 308: Small module gaps with candidate readers вЂ” LargeGroupBonusUpdate.AlliesOnly, W3DLightDraw.Radius, EvaAnnounceClientCreate.CreateFakeRadarEvent and their classes' uncited functions |
| 367 | RW 0x00753755 | 2 | 0 | 0 | 0 | none matched |
| 368 | RW 0x009288D0 | 2 | 0 | 0 | 0 | none matched |
| 369 | RW 0x0082FCAB | 2 | 0 | 0 | 0 | none matched |
| 370 | RW 0x006AA5B3 | 2 | 1 | 0 | 0 | #766 Local 100: upgrade modules: CommandSetUpgrade (515 modules), CostModifierUpgrade, CastleUpgrade, GiveUpgradeUpdate, AudioLoopUpgrade, WallUpgradeUpdate, LocomotorSetUpgrade: 27 uncited own functions |
| 371 | linked stops S-1922, S-1953 | 2 | 0 | 0 | 0 | none matched |
| 372 | linked stops S-4751 | 1 | 0 | 0 | 0 | none matched |
| 373 | RW 0x006634BF | 1 | 0 | 0 | 0 | none matched |
| 374 | RW 0x00643374 | 5 | 1 | 0 | 0 | #766 Local 100: upgrade modules: CommandSetUpgrade (515 modules), CostModifierUpgrade, CastleUpgrade, GiveUpgradeUpdate, AudioLoopUpgrade, WallUpgradeUpdate, LocomotorSetUpgrade: 27 uncited own functions |
| 375 | RW 0x00470F44 | 2 | 0 | 0 | 0 | none matched |
| 376 | RW 0x0048AB28 | 2 | 2 | 0 | 0 | none matched |
| 377 | RW 0x004874A7 | 2 | 0 | 0 | 0 | none matched |
| 378 | RW 0x0042E558 | 5 | 0 | 0 | 0 | none matched |
| 379 | RW 0x00695A06 | 4 | 0 | 0 | 2 | #752 Port 316: Model condition flushes on the creation paths and before bone queries (S-1583), #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate |
| 380 | RW 0x00814EFA | 4 | 0 | 0 | 0 | none matched |
| 381 | RW 0x008A1B9F | 9 | 1 | 1 | 10 | #690 Port 298: RespawnUpdate вЂ” hero respawn: its unread DeathAnimationTime and 4 uncited own functions, #688 Port 296: Castles вЂ” CastleBehavior, CastleMemberBehavior and FoundationAIUpdate: the uncited own functions (27) and their five unread fields, #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate, #567 Port 243: Body damage states: ActiveBody EnteringDamagedTransitionTime and the Damaged / ReallyDamaged attribute modifiers (ActiveBody, RespawnBody) |
| 382 | RW 0x00667290 | 1 | 0 | 0 | 1 | #607 Port 259: Horde-level powers: SplitHordeSpecialPower, HordeDispatchSpecialPower, SiegeDeployHordeSpecialPower, SiegeDeploySpecialPower |
| 383 | RW 0x00648CF1 | 2 | 0 | 0 | 0 | none matched |
| 384 | RW 0x00807A81 | 3 | 1 | 0 | 0 | #687 Port 295: GettingBuiltBehavior вЂ” the uncited own functions of a building under construction (19) |
| 385 | RW 0x00667174 | 3 | 1 | 0 | 0 | #460 Port 193: Fear: the AI states 32 AVOID_SCARER and 57 are not ported (S-1027) |
| 386 | RW 0x005E39CE | 3 | 0 | 0 | 0 | none matched |
| 387 | RW 0x0096F53C | 2 | 0 | 0 | 0 | none matched |
| 388 | RW 0x006CC915 | 4 | 1 | 0 | 2 | #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate |
| 389 | RW 0x0091988F | 3 | 0 | 0 | 0 | none matched |
| 390 | RW 0x004A4CD3 | 1 | 0 | 0 | 0 | none matched |
| 391 | RW 0x0074E083 | 1 | 1 | 0 | 0 | none matched |
| 392 | RW 0x00492C35 | 1 | 0 | 0 | 0 | none matched |
| 393 | RW 0x0078419D | 1 | 0 | 0 | 0 | none matched |
| 394 | RW 0x004E70F5 | 2 | 0 | 0 | 0 | none matched |
| 395 | RW 0x00450C12 | 1 | 0 | 0 | 0 | none matched |
| 396 | RW 0x006639CF | 1 | 0 | 0 | 0 | none matched |
| 397 | RW 0x00601C62 | 4 | 2 | 0 | 2 | #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate |
| 398 | RW 0x006FC18E | 2 | 0 | 4 | 1 | none matched |
| 399 | RW 0x006C6743 | 2 | 0 | 0 | 0 | #766 Local 100: upgrade modules: CommandSetUpgrade (515 modules), CostModifierUpgrade, CastleUpgrade, GiveUpgradeUpdate, AudioLoopUpgrade, WallUpgradeUpdate, LocomotorSetUpgrade: 27 uncited own functions |
| 400 | RW 0x007592E1 | 1 | 0 | 0 | 0 | none matched |
| 401 | W3DLaserDraw | 1 | 5 | 0 | 0 | none matched |
| 402 | RW 0x00698F06 | 4 | 0 | 0 | 1 | #688 Port 296: Castles вЂ” CastleBehavior, CastleMemberBehavior and FoundationAIUpdate: the uncited own functions (27) and their five unread fields |
| 403 | RW 0x005EBAEE | 3 | 1 | 0 | 0 | none matched |
| 404 | RW 0x0067CAC6 | 1 | 0 | 0 | 0 | none matched |
| 405 | RW 0x007B274C | 1 | 0 | 0 | 0 | none matched |
| 406 | RW 0x00437120 | 1 | 0 | 0 | 0 | none matched |
| 407 | RW 0x00466289 | 2 | 1 | 0 | 0 | none matched |
| 408 | RW 0x006FFBEF | 4 | 1 | 0 | 3 | #567 Port 243: Body damage states: ActiveBody EnteringDamagedTransitionTime and the Damaged / ReallyDamaged attribute modifiers (ActiveBody, RespawnBody) |
| 409 | RW 0x004A25AA | 1 | 0 | 0 | 0 | none matched |
| 410 | RW 0x006EA857 | 2 | 1 | 0 | 0 | none matched |
| 411 | BridgeBehavior | 1 | 1 | 0 | 0 | #296 [deferred] Local 40: bridge logic as retail: towers, damage stages, pathfinding layer, radar (BridgeBehavior, S-7223) |
| 412 | RW 0x006B3025 | 1 | 0 | 0 | 0 | none matched |
| 413 | RW 0x0068B36F | 2 | 1 | 34 | 2 | #688 Port 296: Castles вЂ” CastleBehavior, CastleMemberBehavior and FoundationAIUpdate: the uncited own functions (27) and their five unread fields, #599 Port 251: The special-power chain every Ennorath ability runs through: SpecialPowerModule and UnpauseSpecialPowerUpgrade's uncited RotWK functions |
| 414 | RW 0x006B47DC | 1 | 0 | 0 | 0 | none matched |
| 415 | RW 0x0090C0EE | 3 | 0 | 0 | 0 | none matched |
| 416 | RW 0x008F0EB4 | 3 | 0 | 0 | 0 | none matched |
| 417 | RW 0x006AA5F5 | 2 | 0 | 4 | 0 | none matched |
| 418 | RW 0x006B694D | 2 | 0 | 0 | 0 | none matched |
| 419 | HordeContain | 2 | 50 | 0 | 2 | #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate |
| 420 | RW 0x006B5F1C | 1 | 0 | 1 | 0 | none matched |
| 421 | RW 0x006F5BB0 | 4 | 1 | 0 | 2 | none matched |
| 422 | linked stops S-480 | 1 | 0 | 37 | 1 | #766 Local 100: upgrade modules: CommandSetUpgrade (515 modules), CostModifierUpgrade, CastleUpgrade, GiveUpgradeUpdate, AudioLoopUpgrade, WallUpgradeUpdate, LocomotorSetUpgrade: 27 uncited own functions, #599 Port 251: The special-power chain every Ennorath ability runs through: SpecialPowerModule and UnpauseSpecialPowerUpgrade's uncited RotWK functions, #570 Port 246: Client fades: W3DTreeDraw FadeDistance / FadeTarget, W3DFloorDraw FloorFadeRateOnObjectDeath, SubObjectsUpgrade WaitBeforeFadeInSeconds |
| 423 | linked stops S-7728 | 1 | 0 | 29 | 1 | none matched |
| 424 | WeaponFireSpecialAbilityUpdate | 2 | 0 | 15 | 0 | none matched |
| 425 | RW 0x008B1690 | 1 | 0 | 13 | 1 | none matched |
| 426 | linked stops S-1987 | 1 | 0 | 11 | 0 | none matched |
| 427 | RW 0x005DD9EE | 1 | 0 | 4 | 4 | #766 Local 100: upgrade modules: CommandSetUpgrade (515 modules), CostModifierUpgrade, CastleUpgrade, GiveUpgradeUpdate, AudioLoopUpgrade, WallUpgradeUpdate, LocomotorSetUpgrade: 27 uncited own functions, #688 Port 296: Castles вЂ” CastleBehavior, CastleMemberBehavior and FoundationAIUpdate: the uncited own functions (27) and their five unread fields |
| 428 | PickupStuffUpdate | 1 | 0 | 4 | 0 | none matched |
| 429 | RW 0x0068C89B | 1 | 0 | 4 | 0 | none matched |
| 430 | LevelGrantSpecialPower | 1 | 0 | 1 | 0 | none matched |
| 431 | ActivateModuleSpecialPower | 1 | 0 | 1 | 0 | none matched |
| 432 | RW 0x004BD9A7 | 1 | 0 | 0 | 5 | #569 Port 245: W3DScriptedModelDraw: MinLODRequired, HighDetailOnly, UseProducerTexture, UseDefaultAnimation, BirthFadeAdditive |
| 433 | RW 0x00873F30 | 1 | 0 | 0 | 2 | #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate |
| 434 | linked stops S-1712 | 1 | 0 | 0 | 2 | #567 Port 243: Body damage states: ActiveBody EnteringDamagedTransitionTime and the Damaged / ReallyDamaged attribute modifiers (ActiveBody, RespawnBody) |
| 435 | RW 0x008A3DD5 | 2 | 0 | 0 | 2 | #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate |
| 436 | RubbleRiseUpdate | 1 | 0 | 0 | 2 | none matched |
| 437 | ActiveBody | 1 | 0 | 0 | 2 | #567 Port 243: Body damage states: ActiveBody EnteringDamagedTransitionTime and the Damaged / ReallyDamaged attribute modifiers (ActiveBody, RespawnBody) |
| 438 | SlowDeathBehavior | 1 | 0 | 0 | 1 | none matched |
| 439 | linked stops S-4192, S-5011 | 2 | 0 | 0 | 1 | #700 Port 308: Small module gaps with candidate readers вЂ” LargeGroupBonusUpdate.AlliesOnly, W3DLightDraw.Radius, EvaAnnounceClientCreate.CreateFakeRadarEvent and their classes' uncited functions |
| 440 | CreateObjectDie | 1 | 0 | 0 | 1 | none matched |
| 441 | linked stops S-003 | 1 | 0 | 0 | 0 | none matched |
| 442 | linked stops S-004 | 1 | 0 | 0 | 0 | none matched |
| 443 | linked stops S-006 | 1 | 0 | 0 | 0 | none matched |
| 444 | linked stops S-007 | 1 | 0 | 0 | 0 | none matched |
| 445 | linked stops S-010 | 1 | 0 | 0 | 0 | none matched |
| 446 | linked stops S-011 | 1 | 0 | 0 | 0 | none matched |
| 447 | linked stops S-012 | 1 | 0 | 0 | 0 | none matched |
| 448 | linked stops S-015 | 1 | 0 | 0 | 0 | none matched |
| 449 | linked stops S-016, S-042 | 2 | 0 | 0 | 0 | none matched |
| 450 | linked stops S-020, S-021 | 3 | 0 | 0 | 0 | none matched |
| 451 | linked stops S-027 | 1 | 0 | 0 | 0 | none matched |
| 452 | linked stops S-028, S-460 | 2 | 0 | 0 | 0 | none matched |
| 453 | linked stops S-030 | 1 | 0 | 0 | 0 | none matched |
| 454 | linked stops S-038 | 1 | 0 | 0 | 0 | none matched |
| 455 | linked stops S-039 | 1 | 0 | 0 | 0 | none matched |
| 456 | linked stops S-060 | 1 | 0 | 0 | 0 | none matched |
| 457 | linked stops S-070 | 1 | 0 | 0 | 0 | none matched |
| 458 | linked stops S-083 | 1 | 0 | 0 | 0 | none matched |
| 459 | linked stops S-100, S-109 | 3 | 0 | 0 | 0 | none matched |
| 460 | linked stops S-102 | 1 | 0 | 0 | 0 | none matched |
| 461 | linked stops S-108, S-130 | 3 | 0 | 0 | 0 | none matched |
| 462 | linked stops S-114 | 1 | 0 | 0 | 0 | none matched |
| 463 | RW 0x00608D0B | 1 | 0 | 0 | 0 | none matched |
| 464 | linked stops S-133 | 1 | 0 | 0 | 0 | none matched |
| 465 | linked stops S-1335 | 1 | 0 | 0 | 0 | none matched |
| 466 | linked stops S-134 | 1 | 0 | 0 | 0 | none matched |
| 467 | linked stops S-138 | 1 | 0 | 0 | 0 | none matched |
| 468 | linked stops S-146 | 1 | 0 | 0 | 0 | none matched |
| 469 | RW 0x00674B1F | 1 | 0 | 0 | 0 | none matched |
| 470 | linked stops S-1540 | 1 | 0 | 0 | 0 | none matched |
| 471 | linked stops S-1541, S-232 | 2 | 0 | 0 | 0 | none matched |
| 472 | linked stops S-1561 | 1 | 0 | 0 | 0 | none matched |
| 473 | RW 0x004BE587 | 1 | 0 | 0 | 0 | none matched |
| 474 | linked stops S-170, S-175 | 2 | 0 | 0 | 0 | none matched |
| 475 | linked stops S-171, S-5140 | 2 | 0 | 0 | 0 | none matched |
| 476 | linked stops S-1730 | 1 | 0 | 0 | 0 | none matched |
| 477 | linked stops S-174 | 1 | 0 | 0 | 0 | none matched |
| 478 | linked stops S-176 | 1 | 0 | 0 | 0 | none matched |
| 479 | W3DTruckDraw | 1 | 0 | 0 | 0 | none matched |
| 480 | linked stops S-181 | 1 | 0 | 0 | 0 | none matched |
| 481 | RW 0x0063F3BF | 1 | 0 | 0 | 0 | none matched |
| 482 | linked stops S-184 | 1 | 0 | 0 | 0 | none matched |
| 483 | linked stops S-187 | 1 | 0 | 0 | 0 | none matched |
| 484 | RW 0x006628CD | 1 | 0 | 0 | 0 | none matched |
| 485 | linked stops S-188 | 1 | 0 | 0 | 0 | none matched |
| 486 | linked stops S-1890 | 1 | 0 | 0 | 0 | none matched |
| 487 | linked stops S-1891 | 1 | 0 | 0 | 0 | none matched |
| 488 | RW 0x0089C1A4 | 1 | 0 | 0 | 0 | none matched |
| 489 | RW 0x0067055A | 1 | 0 | 0 | 0 | none matched |
| 490 | ReplaceSelfUpgrade | 1 | 0 | 0 | 0 | none matched |
| 491 | linked stops S-1997 | 1 | 0 | 0 | 0 | none matched |
| 492 | linked stops S-2004 | 1 | 0 | 0 | 0 | none matched |
| 493 | linked stops S-2005 | 1 | 0 | 0 | 0 | none matched |
| 494 | linked stops S-204 | 1 | 0 | 0 | 0 | #766 Local 100: upgrade modules: CommandSetUpgrade (515 modules), CostModifierUpgrade, CastleUpgrade, GiveUpgradeUpdate, AudioLoopUpgrade, WallUpgradeUpdate, LocomotorSetUpgrade: 27 uncited own functions |
| 495 | RW 0x006AAB85 | 1 | 0 | 0 | 0 | none matched |
| 496 | RW 0x0070BCE7 | 1 | 0 | 0 | 0 | none matched |
| 497 | linked stops S-231 | 1 | 0 | 0 | 0 | none matched |
| 498 | linked stops S-234 | 1 | 0 | 0 | 0 | none matched |
| 499 | RW 0x0067B4B7 | 1 | 0 | 0 | 0 | #748 Port 312: Radar: the hidden flag and its script actions, the stealth blink's two tests, the attack notice's control flow (S-2453, S-2454, S-2457) |
| 500 | linked stops S-249 | 1 | 0 | 0 | 0 | none matched |
| 501 | RW 0x0089DB5D | 2 | 0 | 0 | 0 | #766 Local 100: upgrade modules: CommandSetUpgrade (515 modules), CostModifierUpgrade, CastleUpgrade, GiveUpgradeUpdate, AudioLoopUpgrade, WallUpgradeUpdate, LocomotorSetUpgrade: 27 uncited own functions |
| 502 | RW 0x00642002 | 1 | 0 | 0 | 0 | none matched |
| 503 | RW 0x006A8630 | 1 | 0 | 0 | 0 | none matched |
| 504 | linked stops S-2704 | 1 | 0 | 0 | 0 | none matched |
| 505 | linked stops S-274 | 1 | 0 | 0 | 0 | none matched |
| 506 | linked stops S-284 | 1 | 0 | 0 | 0 | none matched |
| 507 | linked stops S-290 | 1 | 0 | 0 | 0 | none matched |
| 508 | RW 0x0068F430 | 1 | 0 | 0 | 0 | none matched |
| 509 | StructureCollapseUpdate | 1 | 0 | 0 | 0 | none matched |
| 510 | linked stops S-3403 | 1 | 0 | 0 | 0 | none matched |
| 511 | RW 0x00936B7D | 1 | 0 | 0 | 0 | none matched |
| 512 | linked stops S-3507 | 1 | 0 | 0 | 0 | none matched |
| 513 | RW 0x00441C56 | 1 | 0 | 0 | 0 | none matched |
| 514 | RW 0x00690BD2 | 1 | 0 | 0 | 0 | none matched |
| 515 | linked stops S-3711 | 1 | 0 | 0 | 0 | none matched |
| 516 | RW 0x00862A88 | 1 | 0 | 0 | 0 | #465 Port 198: SpawnBehavior: the spawn interface's attack / idle orders and Player::onUnitCreated (S-1425) |
| 517 | linked stops S-4000 | 1 | 0 | 0 | 0 | none matched |
| 518 | linked stops S-4001 | 1 | 0 | 0 | 0 | none matched |
| 519 | linked stops S-4002 | 1 | 0 | 0 | 0 | none matched |
| 520 | RW 0x00667416 | 1 | 0 | 0 | 0 | #787 Local 109: builders and walls, second pass: WorkerAIUpdate, DozerAIUpdate, WallHubBehavior, ManTheWallsSpecialPower вЂ” 34 functions, #650 Port 283: HorseHordeContain and HordeAIUpdate: the cavalry horde's uncited own functions (classify, then port the missing ones) |
| 521 | linked stops S-4222, S-5013 | 2 | 0 | 0 | 0 | none matched |
| 522 | linked stops S-4280, S-810 | 2 | 0 | 0 | 0 | none matched |
| 523 | RW 0x00441C56 | 1 | 0 | 0 | 0 | none matched |
| 524 | linked stops S-4550 | 1 | 0 | 0 | 0 | none matched |
| 525 | linked stops S-4752 | 1 | 0 | 0 | 0 | none matched |
| 526 | linked stops S-4753 | 1 | 0 | 0 | 0 | none matched |
| 527 | linked stops S-4850 | 1 | 0 | 0 | 0 | none matched |
| 528 | linked stops S-4860 | 1 | 0 | 0 | 0 | none matched |
| 529 | linked stops S-4874 | 1 | 0 | 0 | 0 | none matched |
| 530 | linked stops S-4876 | 1 | 0 | 0 | 0 | none matched |
| 531 | linked stops S-4878 | 1 | 0 | 0 | 0 | none matched |
| 532 | linked stops S-4931 | 1 | 0 | 0 | 0 | none matched |
| 533 | linked stops S-4974 | 1 | 0 | 0 | 0 | #570 Port 246: Client fades: W3DTreeDraw FadeDistance / FadeTarget, W3DFloorDraw FloorFadeRateOnObjectDeath, SubObjectsUpgrade WaitBeforeFadeInSeconds |
| 534 | linked stops S-4981 | 1 | 0 | 0 | 0 | none matched |
| 535 | linked stops S-5014 | 1 | 0 | 0 | 0 | none matched |
| 536 | linked stops S-5016 | 1 | 0 | 0 | 0 | none matched |
| 537 | linked stops S-5017 | 1 | 0 | 0 | 0 | none matched |
| 538 | linked stops S-5141 | 1 | 0 | 0 | 0 | none matched |
| 539 | linked stops S-5143 | 1 | 0 | 0 | 0 | none matched |
| 540 | DoCommandUpgrade | 1 | 0 | 0 | 0 | none matched |
| 541 | linked stops S-5204 | 1 | 0 | 0 | 0 | none matched |
| 542 | linked stops S-5306 | 1 | 0 | 0 | 0 | none matched |
| 543 | AimWeaponBehavior | 1 | 0 | 0 | 0 | none matched |
| 544 | RW 0x0082C167 | 1 | 0 | 0 | 0 | none matched |
| 545 | linked stops S-566 | 1 | 0 | 0 | 0 | none matched |
| 546 | RW 0x004BE587 | 1 | 0 | 0 | 0 | none matched |
| 547 | BoredUpdate | 1 | 0 | 0 | 0 | none matched |
| 548 | AODCrushCollide | 1 | 0 | 0 | 0 | none matched |
| 549 | linked stops S-6009 | 1 | 0 | 0 | 0 | none matched |
| 550 | AnnounceBirthAndDeathBehavior | 1 | 0 | 0 | 0 | none matched |
| 551 | TooltipUpgrade | 1 | 0 | 0 | 0 | none matched |
| 552 | linked stops S-6101 | 1 | 0 | 0 | 0 | none matched |
| 553 | RW 0x008DE522 | 1 | 0 | 0 | 0 | none matched |
| 554 | W3DDebrisDraw | 1 | 0 | 0 | 0 | none matched |
| 555 | linked stops S-7202 | 1 | 0 | 0 | 0 | none matched |
| 556 | linked stops S-725 | 1 | 0 | 0 | 0 | none matched |
| 557 | RW 0x004C72BE | 1 | 0 | 0 | 0 | none matched |
| 558 | linked stops S-7364 | 1 | 0 | 0 | 0 | none matched |
| 559 | linked stops S-7385 | 1 | 0 | 0 | 0 | none matched |
| 560 | InactiveBody | 1 | 0 | 0 | 0 | none matched |
| 561 | HordeGarrisonContain | 1 | 0 | 0 | 0 | none matched |
| 562 | HordeSiegeEngineContain | 1 | 0 | 0 | 0 | none matched |
| 563 | linked stops S-815 | 1 | 0 | 0 | 0 | none matched |
| 564 | linked stops S-831 | 1 | 0 | 0 | 0 | none matched |

## Group details

### G001: RW 0x005D893C

Stops: S-185, S-322, S-636. Kinds: code.
Functions: RW 0x005D893C (unknown), RW 0x0068FB63 (ported), RW 0x008C3FA3 (ported). Classes: AIBurningDeathState, ActiveBody, DamageHost, DelayedDeathBody, GlobalData, HighlanderBody, ImmortalBody, InactiveBody, RespawnBody, StructureBody, SymbioticStructuresBody.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: ActiveBody.EnteringDamagedTransitionTime, ActiveBody.ReallyDamagedAttributeModifier, RespawnBody.DamagedAttributeModifier, RespawnBody.ReallyDamagedAttributeModifier, SymbioticStructuresBody.Symbiote.
Existing task matches: #567 Port 243: Body damage states: ActiveBody EnteringDamagedTransitionTime and the Damaged / ReallyDamaged attribute modifiers (ActiveBody, RespawnBody).

### G002: ToggleHiddenSpecialAbilityUpdate

Stops: S-1043, S-7605. Kinds: code.
Functions: RW 0x00691059 (ported), RW 0x00691106 (ported), RW 0x007760B7 (ported), RW 0x007A1C0D (unknown), RW 0x0081B376 (ported), RW 0x00851B6E (ported), RW 0x008B1909 (ported), RW 0x008B1962 (ported), RW 0x008B19AC (ported), RW 0x008B1A70 (ported), RW 0x008C65D2 (ported), RW 0x008C66EB (ported). Classes: InvisibilitySpecialPower, ObjectWeapons, ToggleHiddenSpecialAbilityUpdate.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: Command_SpecialAbilityGeheimeWege, Command_SpecialAbilityHaradScoutCloak.
Unused fields: none linked.
Existing task matches: none.

### G003: DynamicPortalBehaviour

Stops: S-4854, S-4855. Kinds: code.
Functions: RW 0x00672F68 (partial), RW 0x006E85E9 (ported), RW 0x006E85FB (ported), RW 0x008582DE (ported), RW 0x00863D3A (ported), RW 0x00864357 (ported). Classes: CastleMemberBehavior, DynamicPortalBehaviour, FoundationAIUpdate.
Candidates: 1 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: CastleMemberBehavior.StoreUpgradePrice.
Existing task matches: #688 Port 296: Castles вЂ” CastleBehavior, CastleMemberBehavior and FoundationAIUpdate: the uncited own functions (27) and their five unread fields.

### G004: linked stops S-017, S-123

Stops: S-017, S-123. Kinds: code.
Functions: RW 0x00440809 (ported), RW 0x00B5F9F0 (unknown). Classes: none.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G005: linked stops S-041, S-120

Stops: S-041, S-120. Kinds: code.
Functions: RW 0x00B5B320 (unknown). Classes: none.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G006: linked stops S-081, S-233

Stops: S-081, S-233. Kinds: code.
Functions: RW 0x0042F4D0 (ported), RW 0x0042F4E0 (ported), RW 0x005E586F (unknown). Classes: GameLogicRandom.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G007: linked stops S-1122, S-723

Stops: S-1122, S-723. Kinds: code.
Functions: RW 0x0069954A (ported), RW 0x0069ABA7 (ported), RW 0x006AF598 (unknown), RW 0x006D876A (ported), RW 0x006D9042 (ported), RW 0x00870667 (ported). Classes: DisconnectManager, LockstepDriver.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G008: RW 0x0097C819

Stops: S-4071, S-4750. Kinds: code.
Functions: RW 0x0097C819 (unknown). Classes: AptOnlineShell.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G009: ObjectCreationUpgrade

Stops: S-1985, S-1995, S-2007. Kinds: code.
Functions: RW 0x0068B581 (ported), RW 0x00698EC3 (ported), RW 0x00887857 (unknown), RW 0x00887A35 (ported), RW 0x00887AF5 (unknown), RW 0x008A1B9F (ported), RW 0x008B8281 (ported), RW 0x008B8464 (ported), RW 0x008B87C5 (ported). Classes: AimWeaponBehavior, AnnounceBirthAndDeathBehavior, AudioLoopUpgrade, AutoPickUpUpdate, BaseUpgrade, BoredUpdate, ClickReactionBehavior, EvacuateDamage, FlingPassengerSpecialAbilityUpdate, GeometryUpgrade, HordeTransportContainDamage, ObjectCreationUpgrade, PickupStuffUpdate, ProductionUpdate, RunOffMapBehavior, SiegeDeployHordeSpecialPower, SlaughterHordeContain, SlaveWatcherBehavior, SlavedUpdate, TransitionDamageFX.
Candidates: 0 none/partial functions; 2 unknown status.
Red buttons: none linked.
Unused fields: FlingPassengerSpecialAbilityUpdate.FlingPassengerVelocity, RunOffMapBehavior.DieOnMap, RunOffMapBehavior.RunOffMapWaypointName.
Existing task matches: #766 Local 100: upgrade modules: CommandSetUpgrade (515 modules), CostModifierUpgrade, CastleUpgrade, GiveUpgradeUpdate, AudioLoopUpgrade, WallUpgradeUpdate, LocomotorSetUpgrade: 27 uncited own functions; #607 Port 259: Horde-level powers: SplitHordeSpecialPower, HordeDispatchSpecialPower, SiegeDeployHordeSpecialPower, SiegeDeploySpecialPower.

### G010: linked stops S-080, S-093

Stops: S-080, S-093, S-189. Kinds: code.
Functions: RW 0x006D315D (ported), RW 0x006D31D4 (unknown), RW 0x006D3261 (unknown). Classes: RandomAlgorithm, W3DClientRandom.
Candidates: 0 none/partial functions; 2 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G011: linked stops S-2006

Stops: S-2006. Kinds: code.
Functions: RW 0x0086C4EA (unknown). Classes: SpecialPowerModule.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: Command_AngmarRangersIceArrowAbility, Command_AngmarTrollChargeAttack, Command_AxtschwingerAxeThrow, Command_BeregondMountNew, Command_BigScorpionUnderground, Command_BlackRiderMorgulBladeForHorde, Command_FireDrakeBroodInferno, Command_GorkilBombThrow, Command_HalftrollChargeAttack, Command_IsenfurtWchterSmite, Command_NecromancerDeathBargain, Command_RavenPostSecondBonus, Command_SmaugIncinerateNew, Command_SpecialAbilityBalrogBreathAI, Command_SpecialAbilityBoromirHorn, Command_SpecialAbilityBoromirHornRing, Command_SpecialAbilityBoromirTapfererWiderstand, Command_SpecialAbilityBuhrdurFressen, Command_SpecialAbilityDenethorNotstandssituation, Command_SpecialAbilityDenethorSehenderStein1, Command_SpecialAbilityDenethorVerzweiflungsakt, Command_SpecialAbilityDurinMeisterGestein, Command_SpecialAbilityElendilSummoned, Command_SpecialAbilityGeheimeWege, Command_SpecialAbilityGulzarMOrgulSeucheAcolyte, Command_SpecialAbilityHaradScoutCloak, Command_SpecialAbilityKingsFavorEdainTheo, Command_SpecialAbilityMordorBalrogWhipAI, Command_SpecialAbilityNaturmagierErntesegen, Command_SpecialAbilitySaatdesgoldenenWaldes, Command_SpecialAbilityScreechDunklerMarschall, Command_SpecialAbilityTributeGainTwo, Command_ThranduilKnigDsterwalds.
Unused fields: none linked.
Existing task matches: #599 Port 251: The special-power chain every Ennorath ability runs through: SpecialPowerModule and UnpauseSpecialPowerUpgrade's uncited RotWK functions.

### G012: RW 0x00853468

Stops: S-703, S-7224. Kinds: code.
Functions: RW 0x007546DD (unknown), RW 0x0085260A (unknown), RW 0x008530BA (ported), RW 0x00853468 (ported), RW 0x00853BEC (ported), RW 0x00853EDF (ported), RW 0x008DEDBB (ported), RW 0x008E3174 (ported). Classes: SpecialAbilityUpdate.
Candidates: 0 none/partial functions; 2 unknown status.
Red buttons: Command_AngmarTrollChargeAttack, Command_SpecialAbilityLoneRangerSummonFinal, Command_SpecialAbilityNaturmagierErntesegen, Command_ThranduilKnigDsterwalds.
Unused fields: none linked.
Existing task matches: none.

### G013: RW 0x0073F684

Stops: S-7226. Kinds: code.
Functions: RW 0x0073F684 (unknown), RW 0x00852DC5 (ported), RW 0x00853EDF (ported), RW 0x008DDB2A (ported), RW 0x008DEDBB (ported). Classes: AIAttackState, AIChargeTargetState, SpecialAbilityUpdate, UnitVoiceResponse.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: Command_AngmarTrollChargeAttack, Command_SpecialAbilityLoneRangerSummonFinal, Command_SpecialAbilityNaturmagierErntesegen, Command_ThranduilKnigDsterwalds.
Unused fields: none linked.
Existing task matches: none.

### G014: RW 0x00797A96

Stops: S-303. Kinds: code.
Functions: RW 0x00797A96 (ported), RW 0x007987EE (ported), RW 0x0079A518 (ported), RW 0x0079A5EC (ported), RW 0x0079AC19 (ported), RW 0x0079BE6A (ported), RW 0x0079C265 (ported), RW 0x00858701 (ported), RW 0x00858C41 (ported), RW 0x00858CFC (unknown), RW 0x00859198 (ported). Classes: CastleBehavior, CastleUpgrade, WallUpgradeUpdate.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: CastleBehavior.CrewPrepareFX, CastleBehavior.CrewPrepareInterval, CastleBehavior.CrewReleaseFX.
Existing task matches: #766 Local 100: upgrade modules: CommandSetUpgrade (515 modules), CostModifierUpgrade, CastleUpgrade, GiveUpgradeUpdate, AudioLoopUpgrade, WallUpgradeUpdate, LocomotorSetUpgrade: 27 uncited own functions; #688 Port 296: Castles вЂ” CastleBehavior, CastleMemberBehavior and FoundationAIUpdate: the uncited own functions (27) and their five unread fields.

### G015: RespawnUpdate

Stops: S-854. Kinds: code.
Functions: RW 0x008B38A2 (unknown), RW 0x008B3A35 (ported), RW 0x008C47DE (ported). Classes: ActiveBody, RespawnUpdate.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: ActiveBody.EnteringDamagedTransitionTime, ActiveBody.ReallyDamagedAttributeModifier, RespawnUpdate.DeathAnimationTime.
Existing task matches: #690 Port 298: RespawnUpdate вЂ” hero respawn: its unread DeathAnimationTime and 4 uncited own functions; #567 Port 243: Body damage states: ActiveBody EnteringDamagedTransitionTime and the Damaged / ReallyDamaged attribute modifiers (ActiveBody, RespawnBody).

### G016: HordeContain

Stops: S-082, S-1102, S-6004, S-6027, S-7293. Kinds: code.
Functions: RW 0x0046E918 (ported), RW 0x00667144 (ported), RW 0x0068B678 (ported), RW 0x0068C866 (ported), RW 0x006930C9 (unknown), RW 0x00693A1A (ported), RW 0x007448DE (unknown), RW 0x00751716 (unknown), RW 0x00855830 (ported), RW 0x0086C7F6 (ported), RW 0x0086C962 (ported), RW 0x0086D755 (ported), RW 0x0086EBF4 (ported), RW 0x0086EE1B (ported), RW 0x0087043B (ported), RW 0x00871D41 (ported), RW 0x008722AE (unknown), RW 0x00872ADF (ported), RW 0x00872B76 (none), RW 0x00872B91 (ported), RW 0x00872EFC (ported), RW 0x00873FE8 (ported), RW 0x008757FC (ported), RW 0x00875C93 (ported), RW 0x00875DC3 (ported), RW 0x00875EFD (ported), RW 0x00876481 (ported), RW 0x00876B25 (ported), RW 0x00876FC4 (ported), RW 0x008791C5 (ported), RW 0x008CCA45 (ported), RW 0x008CCCB4 (ported), RW 0x009325B4 (ported). Classes: AICombineState, CombineHordeSpecialPower, HordeContain, RankInfo, SplitHordeSpecialPower.
Candidates: 1 none/partial functions; 4 unknown status.
Red buttons: none linked.
Unused fields: HordeContain.LivingWorldOverloadTemplate, HordeContain.RanksToJustFreeWhenAttacking.
Existing task matches: #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate; #607 Port 259: Horde-level powers: SplitHordeSpecialPower, HordeDispatchSpecialPower, SiegeDeployHordeSpecialPower, SiegeDeploySpecialPower.

### G017: GiantBirdSlowDeathBehavior

Stops: S-5606. Kinds: code.
Functions: RW 0x00865076 (unknown), RW 0x0086511C (ported). Classes: GiantBirdSlowDeathBehavior.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: GiantBirdSlowDeathBehavior.CrashAvoidStrength, GiantBirdSlowDeathBehavior.NeedToMaintainFlailingHeight.
Existing task matches: none.

### G018: linked stops S-564, S-7100

Stops: S-564, S-7100. Kinds: code.
Functions: RW 0x0068C234 (ported), RW 0x0068E4E2 (ported), RW 0x0069264D (unknown), RW 0x00694A45 (unknown), RW 0x00804F39 (ported), RW 0x00872B9E (ported), RW 0x00872BAB (ported), RW 0x0088E6AF (ported), RW 0x0088E87F (ported), RW 0x0088E8C7 (ported), RW 0x0088E9B0 (ported). Classes: DynamicShroudClearingRangeUpdate, HordeContain, RadiusDecal.
Candidates: 0 none/partial functions; 2 unknown status.
Red buttons: none linked.
Unused fields: HordeContain.LivingWorldOverloadTemplate, HordeContain.RanksToJustFreeWhenAttacking.
Existing task matches: #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate.

### G019: RW 0x0068B6F5

Stops: S-2456. Kinds: code.
Functions: RW 0x0068B6F5 (ported), RW 0x006D8C53 (ported), RW 0x006D9004 (none), RW 0x006D9042 (ported). Classes: StealthUpdate.
Candidates: 1 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: StealthUpdate.HintDetectableConditions.
Existing task matches: none.

### G020: SubObjectsUpgrade

Stops: S-484. Kinds: code.
Functions: RW 0x0067273A (ported), RW 0x008B8F40 (partial), RW 0x008B911E (ported). Classes: SubObjectsUpgrade.
Candidates: 1 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: SubObjectsUpgrade.WaitBeforeFadeInSeconds.
Existing task matches: #570 Port 246: Client fades: W3DTreeDraw FadeDistance / FadeTarget, W3DFloorDraw FloorFadeRateOnObjectDeath, SubObjectsUpgrade WaitBeforeFadeInSeconds.

### G021: FlingPassengerSpecialAbilityUpdate

Stops: S-7758. Kinds: project.
Functions: RW 0x008986A3 (ported), RW 0x008986C9 (ported), RW 0x0089870E (unknown), RW 0x0089873E (ported), RW 0x0089875A (ported), RW 0x008C6E67 (ported). Classes: FlingPassengerSpecialAbilityUpdate, ModuleData.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: FlingPassengerSpecialAbilityUpdate.FlingPassengerVelocity.
Existing task matches: none.

### G022: RW 0x00646F9C

Stops: S-002. Kinds: code.
Functions: RW 0x00646F9C (unknown). Classes: AptButtonTooltipMap, ArmySummaryDescription.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G023: linked stops S-023, S-390

Stops: S-023, S-390. Kinds: code.
Functions: RW 0x004AC4ED (unknown), RW 0x0054F406 (unknown). Classes: none.
Candidates: 0 none/partial functions; 2 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G024: RW 0x0073F24D

Stops: S-073, S-075. Kinds: code.
Functions: RW 0x00656EF1 (unknown), RW 0x0073F24D (unknown). Classes: INIException, InactiveBody, ModuleData, PillageModule.
Candidates: 0 none/partial functions; 2 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G025: RW 0x0042D400

Stops: S-076. Kinds: code.
Functions: RW 0x0042D400 (unknown). Classes: none.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G026: RW 0x00436A90

Stops: S-077. Kinds: code.
Functions: RW 0x00436A90 (library), RW 0x0073CE7E (unknown). Classes: ThingTemplate.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G027: RW 0x004B33FB

Stops: S-092, S-682, S-7194. Kinds: code.
Functions: RW 0x004B33FB (unknown), RW 0x004B55D5 (ported), RW 0x004B6CDD (ported), RW 0x004B6D5F (ported), RW 0x004BF560 (ported), RW 0x004C6514 (ported), RW 0x004C72BE (ported), RW 0x005F2F55 (ported), RW 0x005F32A7 (ported), RW 0x005F530A (ported), RW 0x00672B5B (ported), RW 0x0067DAE6 (unknown), RW 0x0068BBE0 (ported), RW 0x00694C0D (ported), RW 0x006CCB4F (unknown), RW 0x006EACB9 (ported). Classes: FXEvent, W3DDrawFrame, W3DDrawPoseView.
Candidates: 0 none/partial functions; 3 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G028: RW 0x004B67D4

Stops: S-095. Kinds: code.
Functions: RW 0x004B67D4 (unknown), RW 0x0068B34C (ported). Classes: FXEvent.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G029: RW 0x004B52C8

Stops: S-096. Kinds: code.
Functions: RW 0x004B52C8 (ported), RW 0x004B5497 (unknown). Classes: DrawServices, W3DDrawFrame.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G030: RW 0x008E168A

Stops: S-1021, S-7507. Kinds: code.
Functions: RW 0x005FF9D3 (ported), RW 0x00662FC8 (ported), RW 0x00663053 (ported), RW 0x0066309C (ported), RW 0x00735F53 (ported), RW 0x00835B0A (ported), RW 0x00835D55 (ported), RW 0x008B5427 (ported), RW 0x008B56CF (ported), RW 0x008B56EB (ported), RW 0x008B62BE (unknown), RW 0x008E0D59 (unknown), RW 0x008E0ED7 (ported), RW 0x008E116B (ported), RW 0x008E11A1 (ported), RW 0x008E12F5 (ported), RW 0x008E168A (ported). Classes: AIState, EmotionNugget, EmotionSystem, EmotionTrackerUpdate.
Candidates: 0 none/partial functions; 2 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G031: RW 0x005E5B42

Stops: S-1140. Kinds: code.
Functions: RW 0x005E5B42 (unknown), RW 0x006765B9 (ported). Classes: LiveGame.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G032: RW 0x00531C77

Stops: S-119. Kinds: code.
Functions: RW 0x00531C77 (unknown). Classes: PlayerTemplate, SidesList.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G033: RW 0x0077AEAA

Stops: S-1201. Kinds: code.
Functions: RW 0x0077AEAA (unknown). Classes: ActionManager, CommandTranslator.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G034: SpecialDisguiseUpdate

Stops: S-1221. Kinds: code.
Functions: RW 0x008B4440 (unknown), RW 0x008B45BF (ported), RW 0x008B4702 (ported), RW 0x008B4760 (ported). Classes: SpecialDisguiseUpdate.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G035: FellBeastSwoopPower

Stops: S-1223. Kinds: code.
Functions: RW 0x00662B28 (ported), RW 0x0076A139 (unknown), RW 0x0076D995 (ported), RW 0x008BD372 (ported), RW 0x008CB167 (ported), RW 0x008CB194 (ported), RW 0x008CB1BA (ported), RW 0x008CB21F (ported), RW 0x008CB28B (ported), RW 0x008CB2CE (ported). Classes: AIGiantBirdFollowThruState, AIUpdateInterface, FellBeastSwoopPower, GiantBirdAIUpdate.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G036: RW 0x008895D8

Stops: S-1242. Kinds: code.
Functions: RW 0x008895D8 (unknown). Classes: AnimationSoundClientBehavior.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G037: RW 0x007FC8E1

Stops: S-1320. Kinds: code.
Functions: RW 0x007FC8E1 (unknown). Classes: none.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G038: RW 0x0062017F

Stops: S-1410. Kinds: code.
Functions: RW 0x0062017F (unknown). Classes: none.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G039: linked stops S-145, S-521

Stops: S-145, S-521. Kinds: code.
Functions: RW 0x006AED4B (unknown), RW 0x0073B4A0 (unknown). Classes: PlayerTemplate.
Candidates: 0 none/partial functions; 2 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G040: RW 0x0068BF75

Stops: S-148. Kinds: code.
Functions: RW 0x0068BF75 (ported), RW 0x006907F1 (ported), RW 0x00690A42 (unknown), RW 0x00692443 (ported). Classes: ObjectTypes.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G041: RW 0x004BF2D8

Stops: S-1601, S-1791. Kinds: code.
Functions: RW 0x004BF2D8 (ported), RW 0x004C3731 (ported), RW 0x0067651E (unknown), RW 0x00868E5C (unknown), RW 0x008B3313 (ported). Classes: DrawableManager, SMCHelper, W3DModelDraw.
Candidates: 0 none/partial functions; 2 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G042: linked stops S-1651, S-1652

Stops: S-1651, S-1652. Kinds: code, project.
Functions: RW 0x00537CE0 (unknown), RW 0x00961DFC (unknown). Classes: FXPlayer, MapTerrainBuilder, MeshModelClass, StructureCollapseUpdate.
Candidates: 0 none/partial functions; 2 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G043: linked stops S-178

Stops: S-178. Kinds: code.
Functions: RW 0x007254A7 (unknown). Classes: LiveGame, NewGameSink, PlayerList, SkirmishGameInfo, WindowManager.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G044: RW 0x0068F430

Stops: S-183. Kinds: code.
Functions: RW 0x0068F430 (ported), RW 0x006FF412 (ported), RW 0x00AD2620 (unknown). Classes: WeaponRangeHost.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G045: RW 0x0090DEF0

Stops: S-186. Kinds: code.
Functions: RW 0x0090DEF0 (unknown). Classes: none.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G046: RW 0x00968E59

Stops: S-192. Kinds: code.
Functions: RW 0x00968E59 (unknown). Classes: none.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G047: RW 0x005A07A0

Stops: S-196. Kinds: code.
Functions: RW 0x005A07A0 (unknown). Classes: W3dShaderStruct.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G048: RW 0x00871536

Stops: S-2008. Kinds: code.
Functions: RW 0x006ED843 (ported), RW 0x0086C4EA (unknown), RW 0x00871536 (ported). Classes: none.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G049: RW 0x00794F65

Stops: S-2009. Kinds: code.
Functions: RW 0x00794F65 (unknown), RW 0x008BC992 (ported). Classes: ObjectCreationUpgrade, SlaveWatcherBehavior, SpawnUnitBehavior.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G050: RW 0x006A7C86

Stops: S-206, S-254. Kinds: code.
Functions: RW 0x00602E64 (ported), RW 0x00625456 (ported), RW 0x006A7B9F (ported), RW 0x006A7C86 (unknown), RW 0x006A7F79 (ported), RW 0x006AA858 (ported), RW 0x006E1F2F (ported), RW 0x00779C9C (unknown). Classes: none.
Candidates: 0 none/partial functions; 2 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G051: RW 0x0068D524

Stops: S-220. Kinds: code.
Functions: RW 0x0068D524 (ported), RW 0x0069A760 (unknown). Classes: AIWorld, LocomotorSet, ThingTemplate.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G052: linked stops S-221, S-7183

Stops: S-221, S-7183. Kinds: code, project.
Functions: RW 0x0042EC5E (unknown), RW 0x0064BB2C (unknown), RW 0x00882058 (ported). Classes: AIIdleState, AIStateMachine, AIUpdateInterface, AmbientAIUpdate, AnimalAIUpdate, AnimalAIUpdateModuleData.
Candidates: 0 none/partial functions; 2 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G053: RW 0x00452FC2

Stops: S-242. Kinds: code.
Functions: RW 0x00452FC2 (unknown). Classes: none.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G054: RW 0x0044DFBA

Stops: S-2454. Kinds: code.
Functions: RW 0x0044DFBA (unknown), RW 0x00693BF2 (ported), RW 0x00694C0D (ported). Classes: InvisibilityManager.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: #748 Port 312: Radar: the hidden flag and its script actions, the stealth blink's two tests, the attack notice's control flow (S-2453, S-2454, S-2457).

### G055: RW 0x005DBF35

Stops: S-247. Kinds: code.
Functions: RW 0x005DBF35 (unknown). Classes: none.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G056: RW 0x006A8F42

Stops: S-275. Kinds: code.
Functions: RW 0x006A8F42 (unknown). Classes: LiveGame.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G057: RW 0x00AE4360

Stops: S-292. Kinds: code.
Functions: RW 0x00AE4360 (unknown). Classes: none.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G058: linked stops S-294

Stops: S-294. Kinds: code.
Functions: RW 0x006D5C0F (unknown). Classes: none.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G059: RW 0x0091FC9C

Stops: S-3401. Kinds: code.
Functions: RW 0x0091FC9C (unknown). Classes: ShellEnvironment.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G060: RW 0x0066352C

Stops: S-3806. Kinds: code.
Functions: RW 0x0066352C (ported), RW 0x00696057 (unknown), RW 0x006CC07C (ported), RW 0x006ED843 (ported), RW 0x00863983 (ported). Classes: DynamicPortalBehaviour.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G061: RW 0x006DCDE9

Stops: S-4005. Kinds: code.
Functions: RW 0x006DCDE9 (unknown). Classes: none.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G062: RW 0x009C8F23

Stops: S-4072. Kinds: code.
Functions: RW 0x009C8F23 (unknown). Classes: none.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G063: linked stops S-4076, S-4552

Stops: S-4076, S-4552. Kinds: code.
Functions: RW 0x0084211D (unknown), RW 0x00843D43 (unknown). Classes: CreateAHeroSystem, SkirmishGameSlot.
Candidates: 0 none/partial functions; 2 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G064: RW 0x006A936A

Stops: S-411. Kinds: code.
Functions: RW 0x006A936A (unknown). Classes: none.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G065: RW 0x0060D899

Stops: S-4250. Kinds: code.
Functions: RW 0x0060D899 (unknown). Classes: none.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G066: RW 0x0075C7AE

Stops: S-4253. Kinds: code.
Functions: RW 0x0075C7AE (unknown). Classes: Region3D, TerrainResourceManager.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G067: RW 0x00646771

Stops: S-4464. Kinds: code.
Functions: RW 0x00646771 (unknown). Classes: FormationAssistant.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G068: linked stops S-4870, S-4873

Stops: S-4870, S-4873. Kinds: code.
Functions: RW 0x009C9DDD (unknown), RW 0x009C9F30 (unknown). Classes: none.
Candidates: 0 none/partial functions; 2 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G069: RW 0x007B212F

Stops: S-522. Kinds: code.
Functions: RW 0x007B212F (unknown). Classes: INIException.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G070: RW 0x00A14419

Stops: S-5302. Kinds: code.
Functions: RW 0x00A14419 (unknown). Classes: none.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G071: RW 0x00991027

Stops: S-5304, S-7715. Kinds: code.
Functions: RW 0x00867D6A (unknown), RW 0x0087CE8F (ported), RW 0x00941B9F (unknown), RW 0x00991027 (ported). Classes: CommandSet, GarrisonContain, HordeGarrisonContain, ModEngineFeatures, OpenContain, TunnelContain.
Candidates: 0 none/partial functions; 2 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G072: DeflectSpecialPower

Stops: S-5601. Kinds: code.
Functions: RW 0x0068BE97 (ported), RW 0x0068E31F (ported), RW 0x00691526 (ported), RW 0x008C98EB (unknown). Classes: CombatState, DeflectSpecialPower.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G073: TeleportToCasterSpecialPower

Stops: S-6003. Kinds: code.
Functions: RW 0x00494615 (ported), RW 0x008D193A (unknown), RW 0x008D1AEB (ported). Classes: TeleportToCasterSpecialPower.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G074: RW 0x0059972C

Stops: S-6021. Kinds: code.
Functions: RW 0x0059972C (unknown). Classes: none.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G075: RW 0x00537904

Stops: S-6023. Kinds: code.
Functions: RW 0x00537904 (unknown). Classes: W3DShaderKey.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G076: RW 0x00857818

Stops: S-654. Kinds: code.
Functions: RW 0x00857818 (unknown). Classes: ObjectFilter.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G077: RW 0x005DCC88

Stops: S-7090. Kinds: code.
Functions: RW 0x005DCC88 (ported), RW 0x005DCCEB (ported), RW 0x00676711 (ported), RW 0x0067A544 (unknown), RW 0x0067B4B7 (ported), RW 0x00694CCC (ported). Classes: EvaDamageNotice.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G078: RW 0x0079E175

Stops: S-7186. Kinds: project.
Functions: RW 0x0079E175 (unknown). Classes: ScoreKeeper.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G079: RW 0x0053B5C0

Stops: S-7334. Kinds: code.
Functions: RW 0x0053B5C0 (unknown). Classes: none.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G080: RW 0x00470176

Stops: S-7340. Kinds: code.
Functions: RW 0x00470176 (unknown), RW 0x0049A2EB (ported), RW 0x004F24A8 (ported). Classes: W3DShadowManager.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G081: RW 0x004B4FFD

Stops: S-7428. Kinds: code.
Functions: RW 0x004B4FFD (ported), RW 0x0076B668 (unknown), RW 0x00853EDF (ported). Classes: GiantBirdAIUpdate.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G082: RW 0x0073B22F

Stops: S-7472. Kinds: project.
Functions: RW 0x0073B22F (unknown). Classes: none.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G083: W3DFloorDraw

Stops: S-7506. Kinds: code.
Functions: RW 0x004E504B (unknown). Classes: W3DFloorDraw.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: #570 Port 246: Client fades: W3DTreeDraw FadeDistance / FadeTarget, W3DFloorDraw FloorFadeRateOnObjectDeath, SubObjectsUpgrade WaitBeforeFadeInSeconds.

### G084: ShareExperienceBehavior

Stops: S-7545. Kinds: code.
Functions: RW 0x00850C32 (ported), RW 0x0088314A (unknown), RW 0x008831C8 (ported). Classes: ShareExperienceBehavior.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G085: WallHubBehavior

Stops: S-7564. Kinds: code.
Functions: RW 0x00693B55 (unknown), RW 0x00855E26 (ported), RW 0x00855E2C (ported), RW 0x00855E38 (ported). Classes: WallHubBehavior.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: #787 Local 109: builders and walls, second pass: WorkerAIUpdate, DozerAIUpdate, WallHubBehavior, ManTheWallsSpecialPower вЂ” 34 functions.

### G086: RW 0x006D5C0F

Stops: S-764. Kinds: code.
Functions: RW 0x006D5C0F (unknown). Classes: none.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G087: RW 0x00736FCD

Stops: S-814. Kinds: code.
Functions: RW 0x00736FCD (unknown). Classes: GameWorld, LiveGameAudio, PlayerView.
Candidates: 0 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G088: linked stops S-248, S-707

Stops: S-248, S-707, S-7512. Kinds: code.
Functions: RW 0x0042C22B (ported), RW 0x0069320D (ported), RW 0x006968BC (ported), RW 0x00857BDA (unknown), RW 0x008C3FA3 (ported), RW 0x008E326D (ported), RW 0x009709DA (unknown), RW 0x009715BA (unknown), RW 0x009D7BC8 (unknown). Classes: ActiveBody, CrowdResponse, GettingBuiltBehavior.
Candidates: 0 none/partial functions; 4 unknown status.
Red buttons: none linked.
Unused fields: ActiveBody.EnteringDamagedTransitionTime, ActiveBody.ReallyDamagedAttributeModifier.
Existing task matches: #687 Port 295: GettingBuiltBehavior вЂ” the uncited own functions of a building under construction (19); #567 Port 243: Body damage states: ActiveBody EnteringDamagedTransitionTime and the Damaged / ReallyDamaged attribute modifiers (ActiveBody, RespawnBody).

### G089: linked stops S-104, S-380

Stops: S-104, S-380, S-5142. Kinds: code.
Functions: RW 0x009C8694 (unknown), RW 0x009C881D (unknown), RW 0x00AE76B0 (unknown), RW 0x00B016D0 (unknown). Classes: none.
Candidates: 0 none/partial functions; 4 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G090: RW 0x008B3744

Stops: S-343, S-851. Kinds: code.
Functions: RW 0x007817A3 (unknown), RW 0x0079B374 (ported), RW 0x007A7F8B (ported), RW 0x008B307E (ported), RW 0x008B310D (ported), RW 0x008B3313 (ported), RW 0x008B3349 (unknown), RW 0x008B3744 (unknown), RW 0x008B3A35 (ported), RW 0x008C5828 (ported). Classes: CastleMemberBehavior, DelayedDeathBody, LifetimeUpdate, RespawnBody, RespawnUpdate, SymbioticStructuresBody.
Candidates: 0 none/partial functions; 3 unknown status.
Red buttons: none linked.
Unused fields: CastleMemberBehavior.StoreUpgradePrice, RespawnBody.DamagedAttributeModifier, RespawnBody.ReallyDamagedAttributeModifier, RespawnUpdate.DeathAnimationTime, SymbioticStructuresBody.Symbiote.
Existing task matches: #690 Port 298: RespawnUpdate вЂ” hero respawn: its unread DeathAnimationTime and 4 uncited own functions; #688 Port 296: Castles вЂ” CastleBehavior, CastleMemberBehavior and FoundationAIUpdate: the uncited own functions (27) and their five unread fields; #567 Port 243: Body damage states: ActiveBody EnteringDamagedTransitionTime and the Damaged / ReallyDamaged attribute modifiers (ActiveBody, RespawnBody).

### G091: RW 0x005DE588

Stops: S-1322, S-1361, S-190, S-520. Kinds: code.
Functions: RW 0x005DE588 (unknown), RW 0x006939DF (ported), RW 0x00696E63 (ported), RW 0x006AAC66 (ported), RW 0x0073AA94 (unknown), RW 0x0073B192 (unknown), RW 0x0073B217 (unknown), RW 0x008713F6 (partial), RW 0x0089509B (unknown), RW 0x0089515C (ported), RW 0x00895310 (ported), RW 0x008953BB (ported). Classes: AttachUpdate, FXListParseServices, HordeContain, LightPointLevel, PlayerTemplate.
Candidates: 1 none/partial functions; 5 unknown status.
Red buttons: none linked.
Unused fields: HordeContain.LivingWorldOverloadTemplate, HordeContain.RanksToJustFreeWhenAttacking.
Existing task matches: #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate.

### G092: linked stops S-031, S-1650

Stops: S-031, S-1650. Kinds: code.
Functions: RW 0x004876C6 (unknown), RW 0x004AF7A9 (unknown), RW 0x00511763 (unknown). Classes: LookupTablePostEffect, MapPostEffectsHost, MapWeather.
Candidates: 0 none/partial functions; 3 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G093: linked stops S-1125, S-722

Stops: S-1125, S-722. Kinds: code.
Functions: RW 0x0077DEE3 (unknown), RW 0x0077DEFD (unknown), RW 0x0077E29A (unknown). Classes: GameWorld, ReplayPlayback.
Candidates: 0 none/partial functions; 3 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G094: RW 0x0062D7EA

Stops: S-1342, S-270. Kinds: code.
Functions: RW 0x0062D7EA (unknown), RW 0x006497EC (unknown), RW 0x007F104D (unknown). Classes: LivingWorldLogic.
Candidates: 0 none/partial functions; 3 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G095: linked stops S-1409, S-177

Stops: S-1409, S-177. Kinds: code.
Functions: RW 0x0072260B (unknown), RW 0x0075E4DF (unknown), RW 0x009C3F2A (unknown). Classes: EntryData.
Candidates: 0 none/partial functions; 3 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G096: linked stops S-1930, S-245

Stops: S-1930, S-245. Kinds: code.
Functions: RW 0x00450DC2 (unknown), RW 0x00452BD3 (unknown), RW 0x0045FBB9 (unknown). Classes: none.
Candidates: 0 none/partial functions; 3 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G097: RW 0x008D861A

Stops: S-4810, S-4812. Kinds: code.
Functions: RW 0x008D836B (unknown), RW 0x008D861A (unknown), RW 0x008D89D7 (unknown). Classes: none.
Candidates: 0 none/partial functions; 3 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G098: RW 0x00674FFE

Stops: S-4945, S-701. Kinds: code.
Functions: RW 0x004C9323 (ported), RW 0x004CE8CA (ported), RW 0x0066844A (ported), RW 0x006726B9 (unknown), RW 0x00674FFE (ported), RW 0x00675095 (ported), RW 0x00676A8A (ported), RW 0x0067A0E8 (unknown), RW 0x00851E97 (ported), RW 0x0089209F (ported), RW 0x008CF01B (ported), RW 0x008CF769 (ported), RW 0x008D008C (unknown), RW 0x008D0138 (ported), RW 0x008D0277 (ported). Classes: AIUpdateInterface, DeployStyleAIUpdate, ModelConditionSoundSelectorClientBehavior, RandomSoundSelectorClientBehavior, SoundSelectorClientBehavior, SoundSelectors, SoundState, UpgradeSoundSelectorClientBehavior.
Candidates: 0 none/partial functions; 3 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G099: GateOpenAndCloseBehavior

Stops: S-7563, S-7565. Kinds: code.
Functions: RW 0x00707887 (unknown), RW 0x0089C04B (unknown), RW 0x0089C37A (ported), RW 0x0089CC29 (ported), RW 0x009A5E04 (ported), RW 0x009E35EF (ported), RW 0x009E35F3 (unknown). Classes: GateOpenAndCloseBehavior, ModuleData, UpdateModule.
Candidates: 0 none/partial functions; 3 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G100: linked stops S-482, S-5202

Stops: S-482, S-5202, S-5203. Kinds: code.
Functions: RW 0x0068D7AB (ported), RW 0x006ABABD (ported), RW 0x006ABBB7 (unknown), RW 0x0082C5A6 (unknown), RW 0x0082C65C (unknown), RW 0x008ADAA0 (unknown), RW 0x008AE798 (ported), RW 0x008FABC8 (unknown). Classes: ActionManager, DozerAIUpdate, PlayerEconomyHost, ResourceGatheringManager, SupplyWarehouseCreate, SupplyWarehouseDockUpdate, WorkerAIUpdate.
Candidates: 0 none/partial functions; 5 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: #787 Local 109: builders and walls, second pass: WorkerAIUpdate, DozerAIUpdate, WallHubBehavior, ManTheWallsSpecialPower вЂ” 34 functions.

### G101: RW 0x00628882

Stops: S-1024, S-110, S-129, S-147, S-4191, S-5015, S-532, S-920. Kinds: code.
Functions: RW 0x004C238A (ported), RW 0x004C451D (ported), RW 0x004C7C56 (unknown), RW 0x00628882 (ported), RW 0x00663E32 (unknown), RW 0x006695EF (unknown), RW 0x0067449C (ported), RW 0x006789B4 (ported), RW 0x00679512 (ported), RW 0x0068B53C (ported), RW 0x0068C234 (ported), RW 0x0068C409 (unknown), RW 0x0068C7E9 (unknown), RW 0x0068D30E (ported), RW 0x0068E31F (ported), RW 0x00693D0C (ported), RW 0x00695A06 (unknown), RW 0x00697F28 (unknown), RW 0x006B183D (unknown), RW 0x006D165E (ported), RW 0x006D328E (ported), RW 0x006F74D0 (unknown), RW 0x00733FAF (unknown), RW 0x00735941 (ported), RW 0x00735F53 (ported), RW 0x00804FCC (ported), RW 0x00853EDF (ported), RW 0x0085C30F (ported), RW 0x0085C8C9 (ported), RW 0x0088B737 (unknown), RW 0x0088B830 (unknown), RW 0x00896FD9 (unknown), RW 0x0089713F (ported), RW 0x0089763B (ported), RW 0x008977BF (unknown), RW 0x00897987 (ported), RW 0x008980A3 (ported), RW 0x008980EF (ported), RW 0x0089816C (ported), RW 0x00A39090 (ported), RW 0x00B4F410 (unknown). Classes: AIUpdateInterface, ClickReactionBehavior, CreationScriptData, DelayedLuaEventUpdate, DeletionUpdate, EventList, GlobalData, HitReactionBehavior, LiveGame, LiveScripting, LuaEventNugget, LuaGameHost, LuaScriptEngine, MapObjectReport, MapObjectRuntime, MapRuntimeReport, SidesList, SpecialAbilityUpdate, SpecialDisguiseUpdate, SubObjectsUpgrade, UpgradeModule, W3DFloorDraw.
Candidates: 0 none/partial functions; 15 unknown status.
Red buttons: Command_AngmarTrollChargeAttack, Command_SpecialAbilityLoneRangerSummonFinal, Command_SpecialAbilityNaturmagierErntesegen, Command_ThranduilKnigDsterwalds.
Unused fields: SubObjectsUpgrade.WaitBeforeFadeInSeconds.
Existing task matches: #570 Port 246: Client fades: W3DTreeDraw FadeDistance / FadeTarget, W3DFloorDraw FloorFadeRateOnObjectDeath, SubObjectsUpgrade WaitBeforeFadeInSeconds.

### G102: RW 0x00853EDF

Stops: S-7430. Kinds: code.
Functions: RW 0x00853EDF (ported), RW 0x00873A02 (ported), RW 0x008C6D43 (unknown), RW 0x008C6E82 (unknown). Classes: LevelGrantSpecialPower, SpecialAbilityUpdate.
Candidates: 0 none/partial functions; 2 unknown status.
Red buttons: Command_AngmarTrollChargeAttack, Command_SpecialAbilityKingsFavorEdainTheo, Command_SpecialAbilityLoneRangerSummonFinal, Command_SpecialAbilityNaturmagierErntesegen, Command_ThranduilKnigDsterwalds.
Unused fields: none linked.
Existing task matches: none.

### G103: RW 0x00851E9F

Stops: S-1225, S-5608, S-859. Kinds: code.
Functions: RW 0x006613D3 (unknown), RW 0x006616AC (unknown), RW 0x0068B303 (ported), RW 0x0068E083 (ported), RW 0x006996DC (unknown), RW 0x0079F0E1 (ported), RW 0x007B18B8 (ported), RW 0x00851607 (ported), RW 0x00851E9F (ported), RW 0x00852A3E (ported), RW 0x00852E8F (ported), RW 0x008530BA (ported), RW 0x0085393B (ported), RW 0x00853BEC (ported), RW 0x00853EDF (ported), RW 0x00854DF7 (ported), RW 0x008A05B2 (ported), RW 0x008A1666 (ported), RW 0x008B202A (ported), RW 0x008B208A (ported), RW 0x008B21DA (unknown), RW 0x008B4455 (ported), RW 0x008D2688 (unknown), RW 0x008E3ABB (unknown). Classes: ObjectFilter, SpecialAbilityUpdate, SpecialDisguiseUpdate, SummonReplacementSpecialAbilityUpdate.
Candidates: 0 none/partial functions; 6 unknown status.
Red buttons: Command_AngmarTrollChargeAttack, Command_SpecialAbilityLoneRangerSummonFinal, Command_SpecialAbilityNaturmagierErntesegen, Command_ThranduilKnigDsterwalds.
Unused fields: SummonReplacementSpecialAbilityUpdate.OpacityTarget.
Existing task matches: none.

### G104: SpecialAbilityUpdate

Stops: S-7493. Kinds: code.
Functions: RW 0x008530BA (ported), RW 0x00854D9A (ported), RW 0x00854DF7 (ported), RW 0x008D0E1D (none), RW 0x008D0F50 (ported), RW 0x008D1214 (none), RW 0x008D128E (ported). Classes: CurseSpecialPower, DominateEnemySpecialPower, SpecialAbilityUpdate.
Candidates: 2 none/partial functions; 0 unknown status.
Red buttons: Command_AngmarTrollChargeAttack, Command_SpecialAbilityLoneRangerSummonFinal, Command_SpecialAbilityNaturmagierErntesegen, Command_ThranduilKnigDsterwalds.
Unused fields: none linked.
Existing task matches: none.

### G105: W3DHordeModelDraw

Stops: S-7615. Kinds: code.
Functions: RW 0x00478461 (none), RW 0x004B2D5E (ported), RW 0x004B32FD (none), RW 0x004C52FE (ported), RW 0x004CA6F0 (ported). Classes: LogicSnapshot, W3DHordeModelDraw, W3DScriptedModelDraw.
Candidates: 2 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: W3DScriptedModelDraw.BirthFadeAdditive, W3DScriptedModelDraw.HighDetailOnly, W3DScriptedModelDraw.MinLODRequired, W3DScriptedModelDraw.UseDefaultAnimation, W3DScriptedModelDraw.UseProducerTexture.
Existing task matches: #569 Port 245: W3DScriptedModelDraw: MinLODRequired, HighDetailOnly, UseProducerTexture, UseDefaultAnimation, BirthFadeAdditive.

### G106: LargeGroupBonusUpdate

Stops: S-1023, S-7291. Kinds: code, project.
Functions: RW 0x00660AFE (unknown), RW 0x0068C3E6 (ported), RW 0x00804FCC (ported), RW 0x00887B72 (ported), RW 0x00887DF7 (ported), RW 0x00887F17 (ported), RW 0x008936C9 (partial), RW 0x00893810 (none), RW 0x00893871 (unknown), RW 0x008938D9 (ported). Classes: HordeContain, LargeGroupBonusUpdate, PassiveAreaEffectBehavior.
Candidates: 2 none/partial functions; 2 unknown status.
Red buttons: none linked.
Unused fields: HordeContain.LivingWorldOverloadTemplate, HordeContain.RanksToJustFreeWhenAttacking, LargeGroupBonusUpdate.AlliesOnly.
Existing task matches: #700 Port 308: Small module gaps with candidate readers вЂ” LargeGroupBonusUpdate.AlliesOnly, W3DLightDraw.Radius, EvaAnnounceClientCreate.CreateFakeRadarEvent and their classes' uncited functions; #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate.

### G107: RW 0x00731010

Stops: S-300. Kinds: code.
Functions: RW 0x00731010 (unknown), RW 0x007311FB (unknown). Classes: CastleBehavior.
Candidates: 0 none/partial functions; 2 unknown status.
Red buttons: none linked.
Unused fields: CastleBehavior.CrewPrepareFX, CastleBehavior.CrewPrepareInterval, CastleBehavior.CrewReleaseFX.
Existing task matches: #688 Port 296: Castles вЂ” CastleBehavior, CastleMemberBehavior and FoundationAIUpdate: the uncited own functions (27) and their five unread fields.

### G108: RW 0x006955BC

Stops: S-1403, S-323, S-634. Kinds: code.
Functions: RW 0x005F0126 (ported), RW 0x00672B5B (ported), RW 0x0068F37F (ported), RW 0x006955BC (ported), RW 0x00809FFB (unknown), RW 0x0080B1D4 (ported), RW 0x0080B339 (ported), RW 0x008211EC (unknown), RW 0x00821319 (unknown), RW 0x008719E4 (ported), RW 0x0089A468 (unknown), RW 0x009C513E (unknown), RW 0x009C523F (unknown). Classes: BannerCarrierUpdate, CreateAHeroSystem, EndGameView, FXEventLog, HordeContain, LevelUpFx.
Candidates: 0 none/partial functions; 6 unknown status.
Red buttons: none linked.
Unused fields: HordeContain.LivingWorldOverloadTemplate, HordeContain.RanksToJustFreeWhenAttacking.
Existing task matches: #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate.

### G109: linked stops S-1426, S-167

Stops: S-1426, S-167, S-328. Kinds: code.
Functions: RW 0x00655085 (unknown), RW 0x00668303 (ported), RW 0x0068B58C (ported), RW 0x006996DC (unknown), RW 0x006CA525 (ported), RW 0x0079F486 (ported), RW 0x008A510B (ported), RW 0x008A51B9 (ported), RW 0x008A5319 (unknown), RW 0x008A567E (unknown), RW 0x008A58FA (unknown), RW 0x008A5B75 (unknown), RW 0x008A5D00 (ported). Classes: AIUpdateInterface, HordeContain, ObjectPathfindAdapter, SlavedUpdate.
Candidates: 0 none/partial functions; 6 unknown status.
Red buttons: none linked.
Unused fields: HordeContain.LivingWorldOverloadTemplate, HordeContain.RanksToJustFreeWhenAttacking.
Existing task matches: #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate.

### G110: ClearanceTestingSlowDeathBehavior

Stops: S-7557. Kinds: code.
Functions: RW 0x00886EE6 (ported), RW 0x00886EF2 (ported), RW 0x00886FB1 (unknown), RW 0x00887013 (ported), RW 0x0088701C (ported), RW 0x00887070 (partial), RW 0x00887191 (ported). Classes: ClearanceTestingSlowDeathBehavior, GeometryInfo.
Candidates: 1 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: ClearanceTestingSlowDeathBehavior.ClearanceGeometryIsSmall.
Existing task matches: none.

### G111: RW 0x0067C5F3

Stops: S-952. Kinds: code.
Functions: RW 0x0067C5F3 (ported), RW 0x00683955 (ported), RW 0x0068E083 (ported), RW 0x007990D9 (partial), RW 0x0079A09D (ported), RW 0x0079A518 (ported), RW 0x0079B3C4 (ported), RW 0x0079B881 (ported), RW 0x0079C78C (ported), RW 0x008582DE (ported), RW 0x008584DB (partial), RW 0x00AD2CE0 (ported). Classes: CastleMemberBehavior, FoundationAIUpdate.
Candidates: 2 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: CastleMemberBehavior.StoreUpgradePrice.
Existing task matches: #688 Port 296: Castles вЂ” CastleBehavior, CastleMemberBehavior and FoundationAIUpdate: the uncited own functions (27) and their five unread fields.

### G112: RW 0x004D6FFA

Stops: S-036. Kinds: code.
Functions: RW 0x004D6FFA (unknown), RW 0x004D7C45 (unknown). Classes: GlobalData.
Candidates: 0 none/partial functions; 2 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G113: RW 0x007405B1

Stops: S-071. Kinds: code.
Functions: RW 0x0073FAC4 (unknown), RW 0x007405B1 (unknown). Classes: ArmorTemplateSet, LocomotorSet, ProductionPrerequisite, RawBlock, WeaponSet, WeaponTemplateSet.
Candidates: 0 none/partial functions; 2 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G114: RW 0x0042DB44

Stops: S-091. Kinds: code.
Functions: RW 0x0042D400 (unknown), RW 0x0042DB44 (unknown), RW 0x004BE587 (ported). Classes: MapObjectBuilder, MapObjectRuntime, W3DDrawScriptApi, W3DDrawScriptHost, W3DLuaDrawScriptHost.
Candidates: 0 none/partial functions; 2 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G115: RW 0x0063CEF1

Stops: S-1001, S-4892. Kinds: code.
Functions: RW 0x0063CEF1 (unknown), RW 0x0064849E (unknown), RW 0x00670A50 (ported), RW 0x00670AA2 (ported), RW 0x0067309D (ported), RW 0x00675996 (unknown), RW 0x0068FC06 (ported), RW 0x00719DEE (unknown), RW 0x0085EBEE (ported), RW 0x0085F28A (ported). Classes: GlobalWeatherSystem, LogicSnapshot, W3DInstancer.
Candidates: 0 none/partial functions; 4 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G116: RW 0x008D7FD2

Stops: S-1127. Kinds: code.
Functions: RW 0x008D7FD2 (unknown), RW 0x008D826F (unknown). Classes: DisconnectManager.
Candidates: 0 none/partial functions; 2 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G117: RW 0x006011F3

Stops: S-116. Kinds: code.
Functions: RW 0x006011F3 (unknown), RW 0x0062710D (unknown). Classes: CloudEffect, ParticleSystem, WeatherSettingStore.
Candidates: 0 none/partial functions; 2 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G118: RW 0x00AD3520

Stops: S-121, S-1423. Kinds: code.
Functions: RW 0x006503A4 (unknown), RW 0x00660D2C (ported), RW 0x00688354 (ported), RW 0x0068836C (ported), RW 0x0068B244 (ported), RW 0x0068F37F (ported), RW 0x0079FD6F (ported), RW 0x008AEC79 (unknown), RW 0x008BAB72 (ported), RW 0x008BABCB (unknown), RW 0x008BAC96 (ported), RW 0x008BADC0 (ported), RW 0x00AD3520 (ported), RW 0x00B6C700 (partial). Classes: GeometryInfo, GeometryUpgrade, WallUpgradeUpdate.
Candidates: 1 none/partial functions; 3 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: #766 Local 100: upgrade modules: CommandSetUpgrade (515 modules), CostModifierUpgrade, CastleUpgrade, GiveUpgradeUpdate, AudioLoopUpgrade, WallUpgradeUpdate, LocomotorSetUpgrade: 27 uncited own functions.

### G119: RW 0x00734E94

Stops: S-126, S-1520, S-830. Kinds: code.
Functions: RW 0x004C3B25 (ported), RW 0x00670AA2 (ported), RW 0x00672823 (ported), RW 0x006789B4 (ported), RW 0x00734E94 (unknown), RW 0x0073501F (unknown), RW 0x00737654 (unknown), RW 0x007953DB (unknown), RW 0x00795468 (unknown), RW 0x00795496 (unknown). Classes: BuildAssistant, GettingBuiltBehavior, TextFile, W3DDrawScriptApi, W3DFloorDraw, W3DLuaDrawScriptHost.
Candidates: 0 none/partial functions; 6 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: #687 Port 295: GettingBuiltBehavior вЂ” the uncited own functions of a building under construction (19); #570 Port 246: Client fades: W3DTreeDraw FadeDistance / FadeTarget, W3DFloorDraw FloorFadeRateOnObjectDeath, SubObjectsUpgrade WaitBeforeFadeInSeconds.

### G120: RW 0x00AF4890

Stops: S-1262. Kinds: code.
Functions: RW 0x00AF4890 (unknown), RW 0x00AF4B20 (unknown). Classes: none.
Candidates: 0 none/partial functions; 2 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G121: RW 0x007F2A14

Stops: S-1321. Kinds: code.
Functions: RW 0x007F2A14 (unknown), RW 0x009015CE (unknown). Classes: INIException, MapInfo, OwnershipSet, StartingRestriction.
Candidates: 0 none/partial functions; 2 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G122: RW 0x007B970F

Stops: S-1325. Kinds: code.
Functions: RW 0x007B970F (unknown), RW 0x00976877 (unknown). Classes: OwnershipSet, SpawnArmies, SpawnBuildings, StartingRestriction.
Candidates: 0 none/partial functions; 2 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G123: RW 0x006BE6A6

Stops: S-1336. Kinds: code.
Functions: RW 0x006BE6A6 (unknown), RW 0x007111F8 (unknown). Classes: GameMessage, LivingWorldLogic.
Candidates: 0 none/partial functions; 2 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G124: RW 0x00625886

Stops: S-1337. Kinds: code.
Functions: RW 0x00625886 (unknown), RW 0x006BE50E (unknown). Classes: LivingWorldLogic, StrategicSession.
Candidates: 0 none/partial functions; 2 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G125: RW 0x0079E4D8

Stops: S-1363. Kinds: code.
Functions: RW 0x0079E4D8 (unknown), RW 0x007C38F6 (unknown). Classes: ScoreKeeper.
Candidates: 0 none/partial functions; 2 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G126: RW 0x009C293D

Stops: S-1404. Kinds: code.
Functions: RW 0x009C293D (unknown), RW 0x009C38EB (unknown). Classes: none.
Candidates: 0 none/partial functions; 2 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G127: RW 0x005FE852

Stops: S-144. Kinds: code.
Functions: RW 0x005FE852 (unknown), RW 0x005FE9AA (unknown). Classes: GlobalData, PlayerTemplate.
Candidates: 0 none/partial functions; 2 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G128: RW 0x00477D1C

Stops: S-1482. Kinds: code.
Functions: RW 0x00477D1C (unknown), RW 0x00530D29 (unknown). Classes: none.
Candidates: 0 none/partial functions; 2 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G129: RW 0x005F2EBC

Stops: S-193. Kinds: code.
Functions: RW 0x005F2EBC (unknown), RW 0x00676C7D (unknown). Classes: none.
Candidates: 0 none/partial functions; 2 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G130: RW 0x0064DFA5

Stops: S-1941. Kinds: code.
Functions: RW 0x0064DFA5 (unknown), RW 0x00656693 (ported), RW 0x00696FD2 (ported), RW 0x0089CB19 (ported), RW 0x00911516 (unknown). Classes: GateOpenAndCloseBehavior, GateProxyBehavior, OpenGateNugget.
Candidates: 0 none/partial functions; 2 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G131: RW 0x007836C0

Stops: S-1952. Kinds: code.
Functions: RW 0x007836C0 (unknown), RW 0x00801211 (unknown). Classes: PlayerStatusInput.
Candidates: 0 none/partial functions; 2 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G132: RW 0x00590C90

Stops: S-197. Kinds: code.
Functions: RW 0x00590C90 (unknown), RW 0x00B574D0 (unknown). Classes: none.
Candidates: 0 none/partial functions; 2 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G133: RW 0x005E821A

Stops: S-201. Kinds: code.
Functions: RW 0x005E821A (ported), RW 0x0066C4CA (ported), RW 0x00770EDE (unknown), RW 0x0087C9B8 (unknown). Classes: AIWorld, CommandSet, LiveGame.
Candidates: 0 none/partial functions; 2 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G134: RW 0x009A4C70

Stops: S-2340. Kinds: code.
Functions: RW 0x009A4C70 (unknown), RW 0x009BD46C (unknown). Classes: VP6Decoder.
Candidates: 0 none/partial functions; 2 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G135: RW 0x0075C02D

Stops: S-250. Kinds: code.
Functions: RW 0x0075B8CC (ported), RW 0x0075C02D (unknown), RW 0x0075C1A9 (unknown). Classes: PathfinderResourceTerrain, TerrainLogic, TerrainResourceManager.
Candidates: 0 none/partial functions; 2 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G136: linked stops S-282, S-450

Stops: S-282, S-450. Kinds: code.
Functions: RW 0x00486D86 (unknown), RW 0x0048C06C (unknown), RW 0x00502858 (unknown), RW 0x005DFE90 (unknown). Classes: HudInput, PinholeView, TacticalCamera, ViewShake.
Candidates: 0 none/partial functions; 4 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G137: StructureToppleUpdate

Stops: S-345. Kinds: code.
Functions: RW 0x008A8F1B (ported), RW 0x008A8FA5 (unknown), RW 0x008A92EA (ported), RW 0x008A950A (unknown), RW 0x008A96A0 (ported). Classes: ScriptEngine, StructureToppleUpdate.
Candidates: 0 none/partial functions; 2 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G138: RW 0x0048B7B1

Stops: S-3501. Kinds: code.
Functions: RW 0x0048B7B1 (unknown), RW 0x0048CBFE (unknown). Classes: none.
Candidates: 0 none/partial functions; 2 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G139: RW 0x005E0A39

Stops: S-4122. Kinds: code.
Functions: RW 0x005E0A39 (partial), RW 0x006BFEFD (library), RW 0x00A201CA (unknown). Classes: none.
Candidates: 1 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G140: RW 0x0080731E

Stops: S-4254. Kinds: code.
Functions: RW 0x0080731E (unknown), RW 0x00808D3C (unknown). Classes: VictoryConditions.
Candidates: 0 none/partial functions; 2 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G141: RW 0x0061B083

Stops: S-4255. Kinds: code.
Functions: RW 0x0061B083 (unknown), RW 0x0080B4F5 (unknown). Classes: none.
Candidates: 0 none/partial functions; 2 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G142: RW 0x008D3AF1

Stops: S-4530. Kinds: code.
Functions: RW 0x008D3AF1 (unknown), RW 0x008D4301 (unknown). Classes: NetworkConfig, ProfileIdentity.
Candidates: 0 none/partial functions; 2 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G143: RW 0x009BEA20

Stops: S-4551. Kinds: code.
Functions: RW 0x009BEA20 (unknown), RW 0x009BECCB (unknown). Classes: none.
Candidates: 0 none/partial functions; 2 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G144: RW 0x00403111

Stops: S-4940. Kinds: code.
Functions: RW 0x00403111 (ported), RW 0x0070BFD1 (ported), RW 0x008639EF (ported), RW 0x00863D77 (unknown), RW 0x00863EB3 (unknown), RW 0x0086404D (ported), RW 0x008CA30F (ported). Classes: DynamicPortalBehaviour, SiegePortalInterface.
Candidates: 0 none/partial functions; 2 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G145: RW 0x0099187C

Stops: S-5201. Kinds: code.
Functions: RW 0x0099187C (ported), RW 0x00991BFE (unknown), RW 0x0099230C (unknown). Classes: DockUpdate.
Candidates: 0 none/partial functions; 2 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G146: RW 0x005DFBEC

Stops: S-5205. Kinds: code.
Functions: RW 0x005DFBEC (unknown), RW 0x005E05AB (unknown). Classes: FXDecalDesc, FXPlaybackWorld.
Candidates: 0 none/partial functions; 2 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G147: RW 0x006898E3

Stops: S-524. Kinds: code.
Functions: RW 0x006898E3 (unknown), RW 0x00689920 (unknown), RW 0x00782AA4 (ported). Classes: ExperienceScalarTable.
Candidates: 0 none/partial functions; 2 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G148: RW 0x00696FD2

Stops: S-5307. Kinds: code.
Functions: RW 0x00696FD2 (ported), RW 0x0077BECA (unknown), RW 0x0077BEDC (unknown). Classes: DoCommandUpgrade, GameLogicDispatch.
Candidates: 0 none/partial functions; 2 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G149: RW 0x00691602

Stops: S-561. Kinds: code.
Functions: RW 0x00691602 (unknown), RW 0x00B4E190 (unknown). Classes: GlobalData, W3DDefaultDraw.
Candidates: 0 none/partial functions; 2 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: #750 Port 314: Ghost objects: a destroyed building stays in the fog for players who saw it (S-561, S-4970).

### G150: GiveUpgradeUpdate

Stops: S-6011. Kinds: code.
Functions: RW 0x0089FE01 (unknown), RW 0x0089FE64 (unknown), RW 0x008A00C2 (ported), RW 0x008A01B2 (ported). Classes: GiveUpgradeUpdate.
Candidates: 0 none/partial functions; 2 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: #766 Local 100: upgrade modules: CommandSetUpgrade (515 modules), CostModifierUpgrade, CastleUpgrade, GiveUpgradeUpdate, AudioLoopUpgrade, WallUpgradeUpdate, LocomotorSetUpgrade: 27 uncited own functions.

### G151: ModelConditionAudioLoopClientBehavior

Stops: S-6024. Kinds: code.
Functions: RW 0x0063F3BF (ported), RW 0x00674727 (unknown), RW 0x0067896F (ported), RW 0x008D02D9 (ported), RW 0x008D03F8 (ported), RW 0x008D0425 (ported), RW 0x008D04D9 (ported), RW 0x008D04EB (ported), RW 0x008D07CF (unknown). Classes: ModelConditionAudioLoopClientBehavior.
Candidates: 0 none/partial functions; 2 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G152: StrafeAreaUpdate

Stops: S-6026. Kinds: code.
Functions: RW 0x0064D3D8 (unknown), RW 0x008A3948 (unknown). Classes: ObjectWeapons, StrafeAreaUpdate.
Candidates: 0 none/partial functions; 2 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G153: RW 0x006897B9

Stops: S-632. Kinds: code.
Functions: RW 0x006897B9 (unknown), RW 0x00689BB3 (ported), RW 0x0068A928 (unknown). Classes: INIException.
Candidates: 0 none/partial functions; 2 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G154: RW 0x004BCE68

Stops: S-683. Kinds: code.
Functions: RW 0x004BABBA (unknown), RW 0x004BCE68 (unknown), RW 0x008A1B9F (ported). Classes: FXEvent.
Candidates: 0 none/partial functions; 2 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G155: RW 0x004C6514

Stops: S-684. Kinds: code.
Functions: RW 0x004C6514 (ported), RW 0x005F889B (ported), RW 0x005FC7DB (unknown), RW 0x0073AECB (unknown). Classes: FXParticleSystem, ParticleSystem.
Candidates: 0 none/partial functions; 2 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G156: RW 0x00671ED8

Stops: S-704. Kinds: code.
Functions: RW 0x00671ED8 (unknown), RW 0x008DDF3B (unknown), RW 0x008DE838 (ported). Classes: InGameUI.
Candidates: 0 none/partial functions; 2 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G157: RW 0x005F11A8

Stops: S-7219. Kinds: code.
Functions: RW 0x005F11A8 (unknown), RW 0x005F1B5A (unknown), RW 0x005F32D7 (ported), RW 0x005F526A (ported), RW 0x0067449C (ported), RW 0x0068BBE0 (ported), RW 0x0070E013 (ported). Classes: ClientEvent, FXParticleSystem, ObjectCreationList, ParticleSystem.
Candidates: 0 none/partial functions; 2 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G158: RW 0x006FF412

Stops: S-7330. Kinds: project.
Functions: RW 0x006FF412 (ported), RW 0x00949567 (unknown), RW 0x0094A29F (unknown). Classes: GuardOuterState.
Candidates: 0 none/partial functions; 2 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G159: RW 0x0074A4EE

Stops: S-7433. Kinds: code.
Functions: RW 0x0074A4EE (unknown), RW 0x0074E9DD (unknown). Classes: none.
Candidates: 0 none/partial functions; 2 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G160: RW 0x006FEEF7

Stops: S-7627. Kinds: project.
Functions: RW 0x006FEEF7 (unknown), RW 0x006FF49C (unknown). Classes: GameLogicSettings.
Candidates: 0 none/partial functions; 2 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G161: RW 0x0063256F

Stops: S-811. Kinds: code.
Functions: RW 0x0063256F (unknown), RW 0x00674B1F (ported), RW 0x006765B9 (ported), RW 0x00B27C80 (unknown). Classes: none.
Candidates: 0 none/partial functions; 2 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G162: RW 0x0088D993

Stops: S-1240, S-302, S-304, S-650, S-656. Kinds: code.
Functions: RW 0x00684BCA (ported), RW 0x00684CBA (unknown), RW 0x00690584 (unknown), RW 0x006AAF3B (unknown), RW 0x0079A09D (ported), RW 0x0079AC19 (ported), RW 0x008561C0 (ported), RW 0x00856227 (unknown), RW 0x008566DF (ported), RW 0x00857818 (unknown), RW 0x00857BDA (unknown), RW 0x00857E77 (ported), RW 0x00858701 (ported), RW 0x0088BFA9 (ported), RW 0x0088C06B (unknown), RW 0x0088C217 (ported), RW 0x0088C2FE (unknown), RW 0x0088C4A3 (ported), RW 0x0088C5C7 (unknown), RW 0x0088CCFC (ported), RW 0x0088CFFE (ported), RW 0x0088D0D8 (ported), RW 0x0088D272 (ported), RW 0x0088D44F (ported), RW 0x0088D7D2 (unknown), RW 0x0088D993 (unknown). Classes: DozerAIUpdate, GettingBuiltBehavior, TerrainLogic, WorkerAIUpdate.
Candidates: 0 none/partial functions; 11 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: #787 Local 109: builders and walls, second pass: WorkerAIUpdate, DozerAIUpdate, WallHubBehavior, ManTheWallsSpecialPower вЂ” 34 functions; #687 Port 295: GettingBuiltBehavior вЂ” the uncited own functions of a building under construction (19).

### G163: RW 0x004CF884

Stops: S-1002, S-365, S-391. Kinds: code.
Functions: RW 0x004CF884 (ported), RW 0x004CFA9A (unknown), RW 0x004CFD13 (unknown), RW 0x00525E1F (unknown), RW 0x00526016 (unknown), RW 0x0052746F (unknown), RW 0x00527B7C (unknown), RW 0x005677B2 (unknown), RW 0x00672FC4 (ported). Classes: GameWorld, W3DStreakDraw.
Candidates: 0 none/partial functions; 7 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G164: RW 0x0044C84A

Stops: S-1440, S-194, S-198. Kinds: code.
Functions: RW 0x0044C84A (unknown), RW 0x0044CBDA (unknown), RW 0x005370FE (unknown), RW 0x00577BA0 (unknown), RW 0x0057DAB0 (unknown), RW 0x00961DFC (unknown), RW 0x0096AF99 (unknown). Classes: FXPlayer.
Candidates: 0 none/partial functions; 7 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G165: RW 0x0083C29E

Stops: S-288, S-3301, S-4360. Kinds: code.
Functions: RW 0x0048ECF4 (unknown), RW 0x0063F135 (unknown), RW 0x0069CBBE (unknown), RW 0x006A091A (unknown), RW 0x006A0A5E (unknown), RW 0x006A2314 (unknown), RW 0x0083C29E (unknown). Classes: InGameUI, SelectionInfo, SelectionTranslator.
Candidates: 0 none/partial functions; 7 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G166: RW 0x0066C0B9

Stops: S-325, S-7329, S-7341. Kinds: code.
Functions: RW 0x00441B59 (ported), RW 0x0066B2F5 (unknown), RW 0x0066B3FD (ported), RW 0x0066BD33 (ported), RW 0x0066BEE5 (none), RW 0x0066BF4C (ported), RW 0x0066C03D (ported), RW 0x0066C0B9 (ported), RW 0x0066C148 (ported), RW 0x0066C1C9 (ported), RW 0x0066DE02 (ported), RW 0x0068D6A6 (ported), RW 0x00741798 (unknown), RW 0x00949994 (unknown), RW 0x0094BA8C (unknown), RW 0x0094BCAC (unknown), RW 0x0094C144 (unknown). Classes: AIAttackState, AIGuardMachine, AIIdleState, AIUpdateInterface, GuardAttackAggressorState, GuardExitConditions.
Candidates: 1 none/partial functions; 6 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: #533 Port 223: the attack machine's Pursue state (a fleeing victim) as RotWK вЂ” mounted heroes stop short of a running single target (P-150, S-325); #515 Port 218: Object / WeaponSet getAbleToAttackSpecificObject (RW 0x68D6A6 / 0x6C9147) as RotWK, for every consumer: the attack machine, closest enemy, guard, garrison, turret, orders (S-325).

### G167: RW 0x00881FF3

Stops: S-5602, S-5910, S-6002. Kinds: code.
Functions: RW 0x004035AC (unknown), RW 0x0046E918 (ported), RW 0x00660AC1 (ported), RW 0x0066491A (ported), RW 0x00869F22 (unknown), RW 0x00881FCC (unknown), RW 0x00881FF3 (unknown), RW 0x00882058 (ported), RW 0x008ACAFA (unknown), RW 0x008B6D10 (unknown), RW 0x008BF076 (unknown), RW 0x008BF12E (ported). Classes: AnimalAIUpdate, Coord3D, EvacuateDamage, ReplaceObjectUpdate.
Candidates: 0 none/partial functions; 7 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G168: RW 0x00693919

Stops: S-1222, S-1224. Kinds: code.
Functions: RW 0x006630A9 (ported), RW 0x006630D7 (ported), RW 0x00663128 (ported), RW 0x00663171 (ported), RW 0x0068C3A3 (ported), RW 0x00693919 (ported), RW 0x00699368 (ported), RW 0x00699513 (ported), RW 0x0069954A (ported), RW 0x0069AB22 (ported), RW 0x0069ABA7 (ported), RW 0x007A7483 (ported), RW 0x008513E4 (unknown), RW 0x00851C92 (ported), RW 0x0086ED25 (ported), RW 0x0086EDB0 (ported), RW 0x00896C72 (ported), RW 0x008A4ED8 (ported), RW 0x008B0485 (ported), RW 0x008B068D (ported), RW 0x008B07EA (unknown), RW 0x008B0A0D (ported), RW 0x008D0BF3 (ported), RW 0x008D0CE0 (ported), RW 0x008D0F6C (unknown), RW 0x008D103D (ported), RW 0x008D222A (ported), RW 0x008DB749 (unknown), RW 0x008DB758 (unknown). Classes: ActivateModuleSpecialPower, DominateEnemySpecialPower, HordeContain, LevelUp, PickupStuffUpdate, RousingSpeechUpdate, SpecialAbilityUpdate, TemporarilyDefectUpdate, WeaponModeSpecialPowerUpdate.
Candidates: 0 none/partial functions; 5 unknown status.
Red buttons: Command_AngmarTrollChargeAttack, Command_HalftrollChargeAttack, Command_SpecialAbilityDurinMeisterGestein, Command_SpecialAbilityLoneRangerSummonFinal, Command_SpecialAbilityNaturmagierErntesegen, Command_ThranduilKnigDsterwalds.
Unused fields: HordeContain.LivingWorldOverloadTemplate, HordeContain.RanksToJustFreeWhenAttacking.
Existing task matches: #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate.

### G169: RW 0x008558C0

Stops: S-856, S-858. Kinds: code.
Functions: RW 0x005DC48C (ported), RW 0x00654B36 (unknown), RW 0x00804FCC (ported), RW 0x00855533 (unknown), RW 0x00855706 (ported), RW 0x00855761 (unknown), RW 0x008558C0 (ported), RW 0x0086EB60 (ported), RW 0x0089F114 (unknown), RW 0x0089F42D (ported), RW 0x008C2FC1 (ported), RW 0x00AD49B0 (unknown). Classes: AttributeModifierAuraUpdate, AutoHealBehavior, CostModifierUpgrade, SpecialAbilityUpdate.
Candidates: 0 none/partial functions; 5 unknown status.
Red buttons: Command_AngmarTrollChargeAttack, Command_SpecialAbilityLoneRangerSummonFinal, Command_SpecialAbilityNaturmagierErntesegen, Command_ThranduilKnigDsterwalds.
Unused fields: none linked.
Existing task matches: #766 Local 100: upgrade modules: CommandSetUpgrade (515 modules), CostModifierUpgrade, CastleUpgrade, GiveUpgradeUpdate, AudioLoopUpgrade, WallUpgradeUpdate, LocomotorSetUpgrade: 27 uncited own functions.

### G170: RW 0x0054BDE0

Stops: S-022, S-1408, S-6022, S-7190. Kinds: code, project.
Functions: RW 0x004A897D (unknown), RW 0x004A8983 (unknown), RW 0x004B877D (ported), RW 0x00531C77 (unknown), RW 0x00535BCF (ported), RW 0x00536A40 (unknown), RW 0x0054BDE0 (unknown), RW 0x006727B0 (unknown), RW 0x0080959A (unknown), RW 0x008B91AD (ported), RW 0x00B55146 (unknown), RW 0x00B552BF (unknown), RW 0x00B5570B (unknown). Classes: ClientEvent, ObjectClientHooks, SubObjectsUpgrade, W3DInstancer, W3DModelDraw, W3DScriptedModelDraw.
Candidates: 0 none/partial functions; 10 unknown status.
Red buttons: none linked.
Unused fields: SubObjectsUpgrade.WaitBeforeFadeInSeconds, W3DScriptedModelDraw.BirthFadeAdditive, W3DScriptedModelDraw.HighDetailOnly, W3DScriptedModelDraw.MinLODRequired, W3DScriptedModelDraw.UseDefaultAnimation, W3DScriptedModelDraw.UseProducerTexture.
Existing task matches: #570 Port 246: Client fades: W3DTreeDraw FadeDistance / FadeTarget, W3DFloorDraw FloorFadeRateOnObjectDeath, SubObjectsUpgrade WaitBeforeFadeInSeconds; #569 Port 245: W3DScriptedModelDraw: MinLODRequired, HighDetailOnly, UseProducerTexture, UseDefaultAnimation, BirthFadeAdditive.

### G171: RW 0x00535C5A

Stops: S-7700, S-7701. Kinds: code.
Functions: RW 0x004B7384 (none), RW 0x004C3A02 (none), RW 0x004C3D1F (none), RW 0x004C436D (none), RW 0x00535C5A (none). Classes: GlobalData, W3DModelDraw, W3DModelDrawModuleData, W3DScriptedModelDraw.
Candidates: 5 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: W3DScriptedModelDraw.BirthFadeAdditive, W3DScriptedModelDraw.HighDetailOnly, W3DScriptedModelDraw.MinLODRequired, W3DScriptedModelDraw.UseDefaultAnimation, W3DScriptedModelDraw.UseProducerTexture.
Existing task matches: #569 Port 245: W3DScriptedModelDraw: MinLODRequired, HighDetailOnly, UseProducerTexture, UseDefaultAnimation, BirthFadeAdditive.

### G172: RW 0x004B686D

Stops: S-1051, S-112, S-461, S-7232. Kinds: code.
Functions: RW 0x004B4379 (ported), RW 0x004B4443 (ported), RW 0x004B516C (ported), RW 0x004B51B5 (ported), RW 0x004B686D (ported), RW 0x004C9A39 (ported), RW 0x004CA6F5 (unknown), RW 0x004CA784 (ported), RW 0x004CA9B6 (unknown), RW 0x004CBFFB (ported), RW 0x004CDEDA (unknown), RW 0x004CFF2A (unknown), RW 0x004D0009 (ported), RW 0x004D00CD (ported), RW 0x0063252F (ported), RW 0x0063256F (unknown), RW 0x006719CD (unknown), RW 0x00672B5B (ported), RW 0x006730E1 (unknown), RW 0x0067476B (unknown), RW 0x00675996 (unknown), RW 0x0067B7AA (unknown), RW 0x0068BD71 (ported), RW 0x0086ABDC (ported), RW 0x00AD1920 (ported). Classes: GettingBuiltBehavior, MapObjectRuntime, W3DFloorDraw, W3DQuadrupedDraw, W3DSailModelDraw, W3DSupplyDraw, W3DTankDraw, W3DTruckDraw.
Candidates: 0 none/partial functions; 10 unknown status.
Red buttons: none linked.
Unused fields: W3DSailModelDraw.BlowingThresholdDegrees.
Existing task matches: #687 Port 295: GettingBuiltBehavior вЂ” the uncited own functions of a building under construction (19); #570 Port 246: Client fades: W3DTreeDraw FadeDistance / FadeTarget, W3DFloorDraw FloorFadeRateOnObjectDeath, SubObjectsUpgrade WaitBeforeFadeInSeconds.

### G173: RW 0x006AA688

Stops: S-1425, S-890. Kinds: code.
Functions: RW 0x0066362D (ported), RW 0x00670AA2 (ported), RW 0x00678F54 (unknown), RW 0x0068AE08 (partial), RW 0x006938BD (ported), RW 0x006A99CB (ported), RW 0x006A9D9C (ported), RW 0x006A9E48 (unknown), RW 0x006AA688 (unknown), RW 0x006ABABD (ported), RW 0x00862456 (ported), RW 0x0086255C (ported), RW 0x0086280D (ported), RW 0x00862865 (ported), RW 0x00862A88 (ported), RW 0x00862B07 (ported), RW 0x00862DAB (ported), RW 0x00862EA3 (ported), RW 0x00862F4A (ported), RW 0x00863225 (ported), RW 0x008633A4 (ported), RW 0x00863585 (ported), RW 0x0088BF4E (ported), RW 0x0088D0D8 (ported), RW 0x0088D123 (ported), RW 0x0088DBB3 (unknown), RW 0x008F0660 (ported), RW 0x008F06FF (ported). Classes: DozerAIUpdate, SpawnBehavior.
Candidates: 1 none/partial functions; 4 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: #787 Local 109: builders and walls, second pass: WorkerAIUpdate, DozerAIUpdate, WallHubBehavior, ManTheWallsSpecialPower вЂ” 34 functions; #465 Port 198: SpawnBehavior: the spawn interface's attack / idle orders and Player::onUnitCreated (S-1425).

### G174: RW 0x00626A87

Stops: S-3505, S-5012. Kinds: code.
Functions: RW 0x00626362 (unknown), RW 0x00626A87 (unknown), RW 0x00757CA0 (unknown), RW 0x007583CC (unknown), RW 0x0081D356 (unknown). Classes: AudioSettings.
Candidates: 0 none/partial functions; 5 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G175: RW 0x0046DFBA

Stops: S-4900, S-7192. Kinds: code, project.
Functions: RW 0x0042ED00 (unknown), RW 0x0046DFBA (unknown), RW 0x00679AE8 (unknown), RW 0x006D1D80 (unknown), RW 0x00740079 (unknown). Classes: ThingTemplate, WindowManager.
Candidates: 0 none/partial functions; 5 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G176: RW 0x00693D0C

Stops: S-142, S-5031, S-580. Kinds: code.
Functions: RW 0x00470D34 (unknown), RW 0x00627C1F (unknown), RW 0x0068D4D0 (ported), RW 0x0068D524 (ported), RW 0x00692223 (ported), RW 0x0069320D (ported), RW 0x00693D0C (ported), RW 0x00695070 (ported), RW 0x00696800 (ported), RW 0x0069990F (unknown), RW 0x006AD2B5 (ported), RW 0x006B099B (unknown), RW 0x006CA3DA (unknown), RW 0x006CF328 (ported), RW 0x006D9042 (ported), RW 0x00840485 (unknown), RW 0x00840B9D (unknown), RW 0x008476E6 (unknown), RW 0x008BFB5E (ported), RW 0x008BFB86 (ported), RW 0x008BFBAE (ported), RW 0x008C6244 (ported), RW 0x008C62BB (ported), RW 0x008C63C4 (ported), RW 0x008DBACE (ported), RW 0x00AD2CE0 (ported). Classes: ActionManager, ActiveBody, AptMpGameSetup, DamageInfo, GameInfo, GameSlot, GiantBirdAIUpdate, HijackerUpdate, ObjectWeapons, PartitionManager, PhysicsBehavior, PorcupineFormationBodyModule, SquishCollide.
Candidates: 0 none/partial functions; 8 unknown status.
Red buttons: none linked.
Unused fields: ActiveBody.EnteringDamagedTransitionTime, ActiveBody.ReallyDamagedAttributeModifier, PorcupineFormationBodyModule.CrushDamageWeaponTemplate, PorcupineFormationBodyModule.CrusherLevelResisted, PorcupineFormationBodyModule.DamageWeaponTemplate.
Existing task matches: #567 Port 243: Body damage states: ActiveBody EnteringDamagedTransitionTime and the Damaged / ReallyDamaged attribute modifiers (ActiveBody, RespawnBody).

### G177: ActiveBody

Stops: S-149, S-342, S-686. Kinds: code.
Functions: RW 0x0042ED00 (unknown), RW 0x005EFFE7 (ported), RW 0x005F0126 (ported), RW 0x0068B2CB (ported), RW 0x0073A368 (unknown), RW 0x0073A5C9 (unknown), RW 0x0079DA66 (ported), RW 0x00871B9B (ported), RW 0x008727F9 (unknown), RW 0x008BDA75 (unknown), RW 0x008BDCF7 (ported), RW 0x008BDDAC (unknown), RW 0x008BDF08 (ported), RW 0x008BE077 (ported), RW 0x008C1BB7 (ported), RW 0x008C2A5D (ported), RW 0x008C2F02 (ported), RW 0x008C2FC1 (ported), RW 0x008C31A5 (ported), RW 0x008C3841 (unknown), RW 0x008C3F1F (unknown), RW 0x008C3FA3 (ported). Classes: ActiveBody, CreateModule, ExperienceTracker, GlobalData, HordeContain, TransitionDamageFX.
Candidates: 0 none/partial functions; 8 unknown status.
Red buttons: none linked.
Unused fields: ActiveBody.EnteringDamagedTransitionTime, ActiveBody.ReallyDamagedAttributeModifier, HordeContain.LivingWorldOverloadTemplate, HordeContain.RanksToJustFreeWhenAttacking.
Existing task matches: #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate; #567 Port 243: Body damage states: ActiveBody EnteringDamagedTransitionTime and the Damaged / ReallyDamaged attribute modifiers (ActiveBody, RespawnBody).

### G178: RW 0x00814BEC

Stops: S-172, S-3502, S-3503, S-3600. Kinds: code.
Functions: RW 0x006C033A (ported), RW 0x006C1B52 (unknown), RW 0x00727081 (unknown), RW 0x00812C89 (unknown), RW 0x00813395 (unknown), RW 0x008142D2 (unknown), RW 0x00814BEC (unknown), RW 0x008421E0 (unknown), RW 0x00986624 (unknown), RW 0x00986776 (unknown), RW 0x00986810 (unknown), RW 0x00986B1C (unknown). Classes: AptGadgetLayer, AptMapPreview, AptSkirmish, GameWindow, GameWindowManager, WindowManager.
Candidates: 0 none/partial functions; 11 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G179: RW 0x0082D7F2

Stops: S-1282, S-7140, S-7509. Kinds: code, project.
Functions: RW 0x00664127 (unknown), RW 0x0068DF46 (ported), RW 0x00756C87 (unknown), RW 0x007640C1 (ported), RW 0x0082C167 (ported), RW 0x0082C366 (ported), RW 0x0082C84C (partial), RW 0x0082D0A1 (ported), RW 0x0082D7F2 (ported), RW 0x0082D925 (ported), RW 0x0082DB5D (unknown), RW 0x0088C51B (unknown), RW 0x0088CC08 (unknown), RW 0x0088DBB3 (unknown), RW 0x0088E17F (unknown), RW 0x00897E01 (ported), RW 0x00898C17 (unknown), RW 0x008B1142 (ported). Classes: AIMover, ActionManager, CommandButtonHuntUpdate, HudInput, ObjectFilter, ToggleMountedSpecialAbilityUpdate.
Candidates: 1 none/partial functions; 8 unknown status.
Red buttons: Command_BeregondMountNew, Command_BigScorpionUnderground, Command_GorkilBombThrow, Command_GorkilZuchtspinnenSummon, Command_SmaugIncinerateNew, Command_SpecialAbilityBoromirHorn, Command_SpecialAbilityKingsFavorEdainTheo, Command_SpecialAbilityTributeGainTwo.
Unused fields: ToggleMountedSpecialAbilityUpdate.OpacityTarget.
Existing task matches: none.

### G180: HordeAIUpdate

Stops: S-1501. Kinds: code.
Functions: RW 0x0070C0AD (unknown), RW 0x0086BDD3 (ported), RW 0x0087594C (ported), RW 0x008759FF (ported), RW 0x0089DE81 (unknown), RW 0x0089DF11 (unknown), RW 0x0089E169 (ported). Classes: HordeAIUpdate, HordeContain.
Candidates: 0 none/partial functions; 3 unknown status.
Red buttons: none linked.
Unused fields: HordeContain.LivingWorldOverloadTemplate, HordeContain.RanksToJustFreeWhenAttacking.
Existing task matches: #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate; #650 Port 283: HorseHordeContain and HordeAIUpdate: the cavalry horde's uncited own functions (classify, then port the missing ones).

### G181: RW 0x0073B723

Stops: S-074. Kinds: code.
Functions: RW 0x0073B723 (unknown), RW 0x0086C30A (unknown), RW 0x008B618C (unknown). Classes: W3DLightDraw.
Candidates: 0 none/partial functions; 3 unknown status.
Red buttons: none linked.
Unused fields: W3DLightDraw.Radius.
Existing task matches: #700 Port 308: Small module gaps with candidate readers вЂ” LargeGroupBonusUpdate.AlliesOnly, W3DLightDraw.Radius, EvaAnnounceClientCreate.CreateFakeRadarEvent and their classes' uncited functions.

### G182: RW 0x007B68F9

Stops: S-037. Kinds: code.
Functions: RW 0x007B68F9 (unknown), RW 0x007D01C0 (unknown), RW 0x007D5270 (unknown). Classes: none.
Candidates: 0 none/partial functions; 3 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G183: RW 0x00AF4DE0

Stops: S-107. Kinds: code.
Functions: RW 0x00AF4DE0 (unknown), RW 0x00B0E620 (unknown), RW 0x00B0F320 (unknown). Classes: none.
Candidates: 0 none/partial functions; 3 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G184: RW 0x00602FFE

Stops: S-1124, S-1343, S-1921. Kinds: code.
Functions: RW 0x00602E64 (ported), RW 0x00602FFE (unknown), RW 0x00603533 (unknown), RW 0x0060CC67 (unknown), RW 0x006290C7 (unknown), RW 0x0077D251 (unknown), RW 0x0077E29A (unknown), RW 0x007BF15E (unknown), RW 0x007C45E0 (unknown), RW 0x0081A375 (unknown). Classes: EndGameController, NetGameSession, ScriptEngine, VictoryConditions.
Candidates: 0 none/partial functions; 9 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G185: RW 0x004DD1F4

Stops: S-1183. Kinds: code.
Functions: RW 0x004DD1F4 (partial), RW 0x006099DC (partial), RW 0x0060C58E (unknown), RW 0x00759FDA (ported). Classes: none.
Candidates: 2 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G186: RW 0x005DC46C

Stops: S-1185. Kinds: code.
Functions: RW 0x005DC46C (ported), RW 0x00606FDA (unknown), RW 0x00609C3A (ported), RW 0x0060C441 (unknown), RW 0x006A8541 (unknown). Classes: none.
Candidates: 0 none/partial functions; 3 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G187: RW 0x006789B4

Stops: S-124. Kinds: code.
Functions: RW 0x006789B4 (ported), RW 0x0073511D (unknown), RW 0x0073529F (unknown), RW 0x00736DF5 (unknown). Classes: LiveScripting, LuaGameHost, MapObjectRuntime, W3DLuaDrawScriptHost.
Candidates: 0 none/partial functions; 3 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G188: RW 0x007396E8

Stops: S-127. Kinds: code.
Functions: RW 0x007396E8 (unknown), RW 0x007397D7 (unknown), RW 0x00949147 (unknown). Classes: AIUpdateInterface, EventList, LuaEventRegistry.
Candidates: 0 none/partial functions; 3 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G189: RW 0x006625E8

Stops: S-128. Kinds: code.
Functions: RW 0x006625E8 (ported), RW 0x007337FE (none), RW 0x007358B2 (unknown), RW 0x0073617F (partial). Classes: none.
Candidates: 2 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G190: RW 0x004052F9

Stops: S-1281. Kinds: code.
Functions: RW 0x004052F9 (ported), RW 0x00406585 (ported), RW 0x00667C76 (unknown), RW 0x00690BD2 (ported), RW 0x006EA0EC (unknown), RW 0x00AD2CE0 (ported), RW 0x00AD30E0 (ported), RW 0x00AD3D30 (unknown). Classes: AIUpdateInterface, GeometryInfo, GeometryUpgrade.
Candidates: 0 none/partial functions; 3 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G191: RW 0x006E1A40

Stops: S-1327. Kinds: code.
Functions: RW 0x006E1A40 (unknown), RW 0x006E1F6C (unknown), RW 0x006E1FD8 (unknown). Classes: none.
Candidates: 0 none/partial functions; 3 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G192: RW 0x0062605D

Stops: S-1344. Kinds: code.
Functions: RW 0x0062605D (unknown), RW 0x00629750 (unknown), RW 0x008D5273 (unknown). Classes: LivingWorldLogic.
Candidates: 0 none/partial functions; 3 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G193: RW 0x004533A9

Stops: S-1462. Kinds: code.
Functions: RW 0x004533A9 (unknown), RW 0x006B3557 (unknown), RW 0x006DADCC (unknown). Classes: AudioEventRTS.
Candidates: 0 none/partial functions; 3 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G194: RW 0x006EF591

Stops: S-165. Kinds: code.
Functions: RW 0x006EF591 (unknown), RW 0x007661A3 (unknown), RW 0x00767B0E (unknown). Classes: none.
Candidates: 0 none/partial functions; 3 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G195: RW 0x00442D32

Stops: S-1653. Kinds: project.
Functions: RW 0x00442D32 (unknown), RW 0x00449CF8 (unknown), RW 0x0065D189 (unknown). Classes: GlobalData, SkyboxSettings.
Candidates: 0 none/partial functions; 3 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G196: RW 0x005DCB28

Stops: S-1674. Kinds: code.
Functions: RW 0x005DCB28 (unknown), RW 0x005DD1F3 (unknown), RW 0x0075991F (unknown). Classes: none.
Candidates: 0 none/partial functions; 3 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G197: RW 0x006235E2

Stops: S-173, S-295, S-4610, S-7759. Kinds: code.
Functions: RW 0x00441506 (unknown), RW 0x00498B1A (unknown), RW 0x00498CBC (unknown), RW 0x004993DA (unknown), RW 0x0049957B (unknown), RW 0x005EE1A6 (ported), RW 0x005EE3D0 (unknown), RW 0x005EE7A8 (unknown), RW 0x006235E2 (unknown), RW 0x00624C68 (unknown), RW 0x00782CA0 (unknown), RW 0x00782CDC (unknown), RW 0x00782DD9 (unknown). Classes: AptButtonTooltipMap, AptHoverInventory, AptPlayer, InGameUI, ScriptEngine, WindowManager.
Candidates: 0 none/partial functions; 12 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G198: RW 0x00662D70

Stops: S-1790. Kinds: code.
Functions: RW 0x00662D70 (ported), RW 0x006968BC (ported), RW 0x008CEF91 (ported), RW 0x00910025 (unknown), RW 0x0091062F (unknown), RW 0x00910A29 (unknown). Classes: AttributeModifierPoolUpdate, MetaImpactNugget.
Candidates: 0 none/partial functions; 3 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G199: RW 0x00812C89

Stops: S-1914. Kinds: code.
Functions: RW 0x0046DFBA (unknown), RW 0x00812C89 (unknown), RW 0x00953861 (unknown). Classes: AptMainMenu.
Candidates: 0 none/partial functions; 3 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G200: RW 0x0044CD2D

Stops: S-195. Kinds: code.
Functions: RW 0x0044CD2D (unknown), RW 0x005FBBC4 (unknown), RW 0x00964C00 (unknown). Classes: ParticleDrawBuilder, W3DInstancer.
Candidates: 0 none/partial functions; 3 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G201: RW 0x004500B9

Stops: S-2450. Kinds: code.
Functions: RW 0x004500B9 (unknown), RW 0x0068BCBD (unknown), RW 0x006AEAEB (unknown), RW 0x006D9042 (ported), RW 0x00776F03 (ported). Classes: none.
Candidates: 0 none/partial functions; 3 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G202: RW 0x006429AD

Stops: S-285. Kinds: code.
Functions: RW 0x006429AD (unknown), RW 0x006E61D4 (unknown), RW 0x009205C4 (unknown). Classes: GlobalData, SelectionTranslator.
Candidates: 0 none/partial functions; 3 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G203: RW 0x006AAA2E

Stops: S-305. Kinds: code.
Functions: RW 0x006AAA2E (unknown), RW 0x0077A91B (unknown), RW 0x0077B011 (unknown), RW 0x0088C217 (ported). Classes: BuildAssistant, DozerAIUpdate.
Candidates: 0 none/partial functions; 3 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: #787 Local 109: builders and walls, second pass: WorkerAIUpdate, DozerAIUpdate, WallHubBehavior, ManTheWallsSpecialPower вЂ” 34 functions.

### G204: RW 0x0074697A

Stops: S-327. Kinds: code.
Functions: RW 0x0074697A (unknown), RW 0x00746C2A (unknown), RW 0x0074ACAD (unknown). Classes: none.
Candidates: 0 none/partial functions; 3 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G205: RW 0x005E2E2E

Stops: S-3808. Kinds: code.
Functions: RW 0x005E2E2E (unknown), RW 0x005FEA89 (unknown), RW 0x0062D232 (unknown), RW 0x0063F3BF (ported). Classes: ArmyDefinition, CreateAHeroSystem, LiveGame, PlayerAIType, PlayerTemplate, PlayerTemplateStore, RetailObjectWorld, SkirmishAIData.
Candidates: 0 none/partial functions; 3 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G206: RW 0x00615871

Stops: S-410. Kinds: code.
Functions: RW 0x00615871 (unknown), RW 0x0082F676 (unknown), RW 0x0082FD6C (unknown). Classes: ArmyDefinition, ArmyMemberDefinition, PlayerAIType, SkirmishAIData.
Candidates: 0 none/partial functions; 3 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G207: RW 0x0071A1A7

Stops: S-4251. Kinds: code.
Functions: RW 0x0071A1A7 (unknown), RW 0x0071A2E3 (unknown), RW 0x00763EDF (unknown). Classes: BitFlags, GlobalWeatherSystem, ObjectFilter.
Candidates: 0 none/partial functions; 3 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G208: RW 0x0068BB14

Stops: S-4320. Kinds: code.
Functions: RW 0x0068BB14 (ported), RW 0x008655DC (ported), RW 0x00865679 (unknown), RW 0x008666F4 (unknown), RW 0x00868759 (unknown), RW 0x0087CF57 (ported). Classes: ActionManager, GarrisonContain, HordeGarrisonContain, OpenContain, ProductionUpdate.
Candidates: 0 none/partial functions; 3 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G209: RW 0x005EAB0A

Stops: S-4901. Kinds: code.
Functions: RW 0x005EAB0A (unknown), RW 0x005EAE37 (unknown), RW 0x00927EC6 (unknown). Classes: none.
Candidates: 0 none/partial functions; 3 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G210: RW 0x004D12FA

Stops: S-4971. Kinds: code.
Functions: RW 0x004D12FA (unknown), RW 0x005E21D8 (ported), RW 0x005E2282 (ported), RW 0x005FA2CE (unknown), RW 0x005FC39A (unknown). Classes: LiveGameAudio, ParticleSystem.
Candidates: 0 none/partial functions; 3 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G211: RW 0x00441E23

Stops: S-4972. Kinds: code.
Functions: RW 0x00441E23 (unknown), RW 0x006031A9 (ported), RW 0x00603418 (unknown), RW 0x00609685 (unknown). Classes: ScriptEngine.
Candidates: 0 none/partial functions; 3 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G212: RW 0x004E5D2C

Stops: S-4973. Kinds: code.
Functions: RW 0x004E5D2C (unknown), RW 0x004E5EDA (unknown), RW 0x004EC93A (unknown), RW 0x008CD675 (ported). Classes: SwayClientUpdate.
Candidates: 0 none/partial functions; 3 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G213: linked stops S-5151

Stops: S-5151. Kinds: code.
Functions: RW 0x0067DFCE (unknown), RW 0x0081F8D8 (unknown), RW 0x0083D7AD (unknown), RW 0x008CDCE9 (ported). Classes: BeaconClientUpdate.
Candidates: 0 none/partial functions; 3 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G214: linked stops S-526, S-710

Stops: S-526, S-710, S-711. Kinds: code.
Functions: RW 0x004A813E (unknown), RW 0x006AA9C2 (unknown), RW 0x006AE186 (unknown), RW 0x006AE2E5 (unknown), RW 0x0075950A (unknown), RW 0x00759646 (unknown), RW 0x00759A4E (unknown), RW 0x007E4FE0 (unknown), RW 0x007E50F6 (unknown). Classes: AcquiredScienceQueue, PlayerScience, ScriptConditions, ScriptEngine.
Candidates: 0 none/partial functions; 9 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G215: AODHordeContain

Stops: S-5310. Kinds: code.
Functions: RW 0x0064B845 (unknown), RW 0x0079D7AD (ported), RW 0x0087D55F (unknown), RW 0x0087D649 (ported), RW 0x0087D691 (unknown), RW 0x0087D9F6 (ported), RW 0x0087DB01 (ported), RW 0x0087DEFB (ported), RW 0x0087E894 (ported), RW 0x0087E8FC (ported). Classes: AODHordeContain, ExperienceTracker, GameLogicRandom.
Candidates: 0 none/partial functions; 3 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G216: RW 0x00B501A0

Stops: S-563. Kinds: code.
Functions: RW 0x00B501A0 (unknown), RW 0x00B50C90 (unknown), RW 0x00B50E60 (unknown). Classes: none.
Candidates: 0 none/partial functions; 3 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G217: RW 0x008C1025

Stops: S-5901. Kinds: code.
Functions: RW 0x008C1025 (unknown), RW 0x008C1039 (unknown), RW 0x008C10DB (unknown). Classes: CrateCollide.
Candidates: 0 none/partial functions; 3 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G218: RW 0x006AAFC3

Stops: S-631. Kinds: code.
Functions: RW 0x006AAFC3 (unknown), RW 0x006AB0D0 (ported), RW 0x00782AA4 (ported), RW 0x0079DB50 (unknown), RW 0x0079DB6B (unknown), RW 0x0079DBA1 (ported). Classes: ExperienceAwardSink, ExperienceWorld, PlayerExperienceAwardSink, PlayerScience, ScoreKeeper.
Candidates: 0 none/partial functions; 3 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G219: RW 0x0068C3A3

Stops: S-653. Kinds: code.
Functions: RW 0x0068C3A3 (ported), RW 0x0070BA38 (unknown), RW 0x007940E8 (unknown), RW 0x00796576 (ported), RW 0x00797465 (ported), RW 0x007977B3 (unknown), RW 0x0079F0E1 (ported), RW 0x008566DF (ported), RW 0x00856992 (ported), RW 0x00857E77 (ported), RW 0x008C1D53 (ported), RW 0x00A3C6F0 (ported). Classes: BuildAssistant.
Candidates: 0 none/partial functions; 3 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G220: RW 0x005F3664

Stops: S-7290. Kinds: project.
Functions: RW 0x005F3664 (unknown), RW 0x005F44F4 (ported), RW 0x005F461D (unknown), RW 0x005F4F97 (unknown), RW 0x005FBE28 (ported). Classes: FXParticleSystem.
Candidates: 0 none/partial functions; 3 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G221: AIUpdateInterface

Stops: S-7445, S-7601, S-7704. Kinds: code.
Functions: RW 0x00662B58 (none), RW 0x00662B6C (none), RW 0x00662CAC (none), RW 0x00662D0B (none), RW 0x00662EEE (none), RW 0x00663EF4 (ported), RW 0x00664533 (ported), RW 0x0066708E (ported), RW 0x00668D8E (none), RW 0x0066E078 (ported), RW 0x0066ECBF (none), RW 0x0066ECD0 (none), RW 0x0082C254 (ported), RW 0x0082C798 (ported), RW 0x008A37F3 (none). Classes: AIStateMachine, AIUpdateInterface, ProneUpdate.
Candidates: 9 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G222: SupplyWarehouseDockUpdate

Stops: S-7533, S-7534. Kinds: code.
Functions: RW 0x0064C16D (unknown), RW 0x00671948 (ported), RW 0x0075A243 (ported), RW 0x007C191A (unknown), RW 0x00886C76 (unknown), RW 0x00886E38 (unknown), RW 0x00886E4A (unknown), RW 0x008AB64E (unknown), RW 0x008AB6E3 (ported). Classes: ScriptActions, SupplyWarehouseDockUpdate.
Candidates: 0 none/partial functions; 6 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G223: RW 0x0068EF58

Stops: S-812. Kinds: code.
Functions: RW 0x005E5B42 (unknown), RW 0x006260E1 (unknown), RW 0x00626154 (unknown), RW 0x0068EF58 (ported). Classes: AIUpdateInterface.
Candidates: 0 none/partial functions; 3 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G224: RW 0x004A8F95

Stops: S-135, S-1910, S-3700, S-763. Kinds: code.
Functions: RW 0x004A2226 (unknown), RW 0x004A8AD5 (unknown), RW 0x004A8F95 (unknown), RW 0x004A999B (unknown), RW 0x004AA369 (unknown), RW 0x00553FD0 (unknown), RW 0x005563F0 (unknown), RW 0x0061829B (unknown), RW 0x006186E0 (unknown), RW 0x006D5C0F (unknown), RW 0x00822A4E (unknown), RW 0x00822D75 (unknown), RW 0x00B225B0 (unknown). Classes: AptCharacterInst, AptMenuPlayer.
Candidates: 0 none/partial functions; 13 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G225: RW 0x007510EC

Stops: S-7164, S-7185, S-7320, S-7633. Kinds: code, project.
Functions: RW 0x0063F3BF (ported), RW 0x0068B53C (ported), RW 0x00742D63 (unknown), RW 0x00748E46 (unknown), RW 0x0074CABC (unknown), RW 0x0074CED6 (unknown), RW 0x0074DADA (unknown), RW 0x0075066F (unknown), RW 0x007510EC (unknown), RW 0x0075D516 (unknown), RW 0x00769335 (unknown), RW 0x007695C2 (unknown), RW 0x00769C31 (unknown), RW 0x00769EA0 (ported), RW 0x007A3E36 (unknown), RW 0x00942733 (unknown). Classes: AIAttackState, AIHuntState, AIUpdateInterface, CommandButton, GiantBirdAIUpdate, GiantBirdAttackMoveToState.
Candidates: 0 none/partial functions; 13 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G226: RW 0x00851E97

Stops: S-7191, S-7229. Kinds: project.
Functions: RW 0x0042EB2A (unknown), RW 0x0046C6BE (ported), RW 0x00660D71 (unknown), RW 0x00694CCC (ported), RW 0x0073AE79 (unknown), RW 0x00851E97 (ported), RW 0x00872A6F (ported), RW 0x0087B392 (ported), RW 0x008C3388 (ported), RW 0x008C3FA3 (ported), RW 0x008C5524 (partial), RW 0x008C6244 (ported), RW 0x008C6269 (partial), RW 0x008C62BB (ported), RW 0x008C633D (partial), RW 0x008C63C4 (ported), RW 0x008ECF3E (unknown), RW 0x009B501B (ported). Classes: AODHordeContain, ActiveBody, CitadelSlaughterHordeContain, ContainModuleInterface, EvaDamageNotice, GarrisonContain, HordeContain, HordeGarrisonContain, HordeSiegeEngineContain, HordeTransportContain, HorseHordeContain, OpenContain, PorcupineFormationBodyModule, ProductionQueueHordeContain, SiegeEngineContain, SlaughterHordeContain, TransportContain, TunnelContain.
Candidates: 3 none/partial functions; 4 unknown status.
Red buttons: none linked.
Unused fields: ActiveBody.EnteringDamagedTransitionTime, ActiveBody.ReallyDamagedAttributeModifier, HordeContain.LivingWorldOverloadTemplate, HordeContain.RanksToJustFreeWhenAttacking, HorseHordeContain.LivingWorldOverloadTemplate, PorcupineFormationBodyModule.CrushDamageWeaponTemplate, PorcupineFormationBodyModule.CrusherLevelResisted, PorcupineFormationBodyModule.DamageWeaponTemplate.
Existing task matches: #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate; #650 Port 283: HorseHordeContain and HordeAIUpdate: the cavalry horde's uncited own functions (classify, then port the missing ones); #567 Port 243: Body damage states: ActiveBody EnteringDamagedTransitionTime and the Damaged / ReallyDamaged attribute modifiers (ActiveBody, RespawnBody).

### G227: RW 0x0087F7B4

Stops: S-1103, S-1105, S-4943, S-813. Kinds: code.
Functions: RW 0x005E821A (ported), RW 0x006316EF (unknown), RW 0x0066246B (ported), RW 0x0066276B (ported), RW 0x00674B1F (ported), RW 0x0068449B (ported), RW 0x0068B3AB (unknown), RW 0x0068BF11 (unknown), RW 0x0068DA67 (unknown), RW 0x00696E63 (ported), RW 0x00697F0F (ported), RW 0x006996DC (unknown), RW 0x006FF412 (ported), RW 0x0076F466 (unknown), RW 0x00781FAF (unknown), RW 0x00795221 (unknown), RW 0x0079ADE9 (partial), RW 0x007A4606 (unknown), RW 0x007C3FD1 (unknown), RW 0x00858701 (ported), RW 0x0086D881 (ported), RW 0x0086D8FE (ported), RW 0x00870A92 (ported), RW 0x00873AE3 (ported), RW 0x0087ECC7 (ported), RW 0x0087ED45 (ported), RW 0x0087ED55 (ported), RW 0x0087EE17 (ported), RW 0x0087EE53 (ported), RW 0x0087EFF8 (ported), RW 0x0087F341 (ported), RW 0x0087F5B4 (ported), RW 0x0087F7B4 (ported), RW 0x0087FD0F (ported), RW 0x0087FFC7 (ported), RW 0x008807CA (ported), RW 0x008A4D09 (unknown), RW 0x008A7BF9 (ported), RW 0x008AEC79 (unknown), RW 0x008B3313 (ported), RW 0x008BF2BF (ported), RW 0x008D1979 (unknown). Classes: AIGroup, AttachUpdate, BridgeBehavior, BuildAssistant, CastleBehavior, DynamicPortalBehaviour, FoundationAIUpdate, HordeContain, HordeSiegeEngineContain, HordeTransportContainDamage, ProductionUpdate, QueueProductionExitUpdate, ReplaceSelfUpgrade, SiegeAIUpdate, SiegeDeployHordeSpecialPower, SiegeDeploySpecialPower, SiegeDockingBehavior, SiegeEngineContain, SiegeEngineContainT, SlowDeathBehavior, StopSpecialPower, StructureCollapseUpdate, StructureToppleUpdate, TeleportToCasterSpecialPower, TerrainLogic, TunnelContain, TurretAI, WallUpgradeUpdate.
Candidates: 1 none/partial functions; 13 unknown status.
Red buttons: none linked.
Unused fields: CastleBehavior.CrewPrepareFX, CastleBehavior.CrewPrepareInterval, CastleBehavior.CrewReleaseFX, HordeContain.LivingWorldOverloadTemplate, HordeContain.RanksToJustFreeWhenAttacking, SiegeDeploySpecialPower.InitiateSound.
Existing task matches: #766 Local 100: upgrade modules: CommandSetUpgrade (515 modules), CostModifierUpgrade, CastleUpgrade, GiveUpgradeUpdate, AudioLoopUpgrade, WallUpgradeUpdate, LocomotorSetUpgrade: 27 uncited own functions; #688 Port 296: Castles вЂ” CastleBehavior, CastleMemberBehavior and FoundationAIUpdate: the uncited own functions (27) and their five unread fields; #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate; #607 Port 259: Horde-level powers: SplitHordeSpecialPower, HordeDispatchSpecialPower, SiegeDeployHordeSpecialPower, SiegeDeploySpecialPower; #296 [deferred] Local 40: bridge logic as retail: towers, damage stages, pathfinding layer, radar (BridgeBehavior, S-7223).

### G228: RW 0x004516DE

Stops: S-243, S-7386. Kinds: code.
Functions: RW 0x004516DE (unknown), RW 0x00451946 (unknown), RW 0x004519FC (unknown), RW 0x0045235B (unknown), RW 0x00458825 (unknown), RW 0x004591D4 (unknown), RW 0x006DB221 (unknown). Classes: AudioManager.
Candidates: 0 none/partial functions; 7 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G229: RW 0x0091BE64

Stops: S-3303, S-4902. Kinds: code.
Functions: RW 0x0075D9B1 (unknown), RW 0x0091B1D2 (unknown), RW 0x0091BE64 (unknown), RW 0x0091C108 (unknown), RW 0x0091C262 (unknown), RW 0x00925699 (unknown), RW 0x009275EC (unknown). Classes: AptMainMenu, GameInfo, GlobalData.
Candidates: 0 none/partial functions; 7 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G230: RW 0x009D2B49

Stops: S-4531, S-7457. Kinds: code.
Functions: RW 0x0069F54F (unknown), RW 0x008DEDBB (ported), RW 0x009D2814 (unknown), RW 0x009D285E (unknown), RW 0x009D2B49 (unknown), RW 0x009D7109 (unknown), RW 0x009D7154 (unknown), RW 0x00A00C0E (unknown). Classes: AptPalantir, ControlBar.
Candidates: 0 none/partial functions; 7 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G231: RW 0x004C451D

Stops: S-090, S-098, S-3304, S-5032, S-5231. Kinds: code.
Functions: RW 0x0046FE84 (unknown), RW 0x0047005F (unknown), RW 0x00476C3F (unknown), RW 0x00477969 (unknown), RW 0x0047A0AD (ported), RW 0x004876C6 (unknown), RW 0x004B1BC6 (ported), RW 0x004B2C18 (ported), RW 0x004B304E (ported), RW 0x004B67D4 (unknown), RW 0x004B92D3 (unknown), RW 0x004B931E (ported), RW 0x004BF15E (ported), RW 0x004BF2D8 (ported), RW 0x004BF4D9 (ported), RW 0x004BF501 (ported), RW 0x004BF560 (ported), RW 0x004BFAF4 (ported), RW 0x004C060B (unknown), RW 0x004C1068 (ported), RW 0x004C2FC0 (unknown), RW 0x004C451D (ported), RW 0x004C70C3 (none), RW 0x004C72BE (ported), RW 0x004FA526 (unknown), RW 0x004FA909 (unknown), RW 0x00508B2D (unknown), RW 0x0050977A (unknown), RW 0x0050B85D (unknown), RW 0x0050BF6A (ported), RW 0x0050D92C (unknown), RW 0x00601C62 (unknown), RW 0x00672D28 (ported), RW 0x00675655 (ported), RW 0x0073228B (ported), RW 0x007323CE (ported), RW 0x007BD74F (unknown), RW 0x00A3CFA4 (ported). Classes: GameEdition, GlobalData, GlowEffect, W3DInstancer, W3DModelDraw, W3DScriptedModelDraw, W3DScriptedModelDrawOptions.
Candidates: 1 none/partial functions; 17 unknown status.
Red buttons: none linked.
Unused fields: W3DScriptedModelDraw.BirthFadeAdditive, W3DScriptedModelDraw.HighDetailOnly, W3DScriptedModelDraw.MinLODRequired, W3DScriptedModelDraw.UseDefaultAnimation, W3DScriptedModelDraw.UseProducerTexture.
Existing task matches: #569 Port 245: W3DScriptedModelDraw: MinLODRequired, HighDetailOnly, UseProducerTexture, UseDefaultAnimation, BirthFadeAdditive.

### G232: OCLSpecialPower

Stops: S-7426. Kinds: code.
Functions: RW 0x005F08C2 (unknown), RW 0x005F0A7A (unknown), RW 0x0072BB37 (none), RW 0x0072BBEF (none), RW 0x0072D72F (ported), RW 0x00798F70 (ported). Classes: OCLSpecialPower.
Candidates: 2 none/partial functions; 2 unknown status.
Red buttons: Command_GaladrielMirrorDissent, Command_GaladrielMirrorWardByMirror, Command_GorkilZuchtspinnenSummon, Command_MenLongShotAbility, Command_OutpostOrcSpawn, Command_SpecialAbilityBeutekarrendesGroorks, Command_SpecialAbilityDenethorSteuererlass, Command_SpecialAbilityDrarSpeerfeuer, Command_SpecialAbilityLoneRangerSummonFinal, Command_SpecialAbilityMuhadSummonOliph, Command_ramridercaptain.
Unused fields: none linked.
Existing task matches: none.

### G233: RW 0x005F00CA

Stops: S-931. Kinds: code.
Functions: RW 0x005F00CA (ported), RW 0x005F275D (unknown), RW 0x006CF530 (ported), RW 0x006EF173 (unknown), RW 0x007A80B1 (unknown), RW 0x007A82AF (unknown), RW 0x007A8368 (ported). Classes: AIUpdateInterface, GiantBirdAIUpdate, OCLSpecialPower, StrafeAreaUpdate, TerrainLogic.
Candidates: 0 none/partial functions; 4 unknown status.
Red buttons: Command_GaladrielMirrorDissent, Command_GaladrielMirrorWardByMirror, Command_GorkilZuchtspinnenSummon, Command_MenLongShotAbility, Command_OutpostOrcSpawn, Command_SpecialAbilityBeutekarrendesGroorks, Command_SpecialAbilityDenethorSteuererlass, Command_SpecialAbilityDrarSpeerfeuer, Command_SpecialAbilityLoneRangerSummonFinal, Command_SpecialAbilityMuhadSummonOliph, Command_ramridercaptain.
Unused fields: none linked.
Existing task matches: none.

### G234: SalvageCrateCollide

Stops: S-260. Kinds: code.
Functions: RW 0x004B1B5A (ported), RW 0x0062BBAB (ported), RW 0x0065592C (unknown), RW 0x008C06B6 (unknown), RW 0x008C0760 (unknown), RW 0x008C07A0 (unknown), RW 0x008C080B (ported), RW 0x008C136C (ported), RW 0x008C1442 (ported), RW 0x008C145D (ported), RW 0x008C148B (ported), RW 0x008C158B (ported). Classes: AIWorld, CrateCollide, SalvageCrateCollide.
Candidates: 0 none/partial functions; 4 unknown status.
Red buttons: none linked.
Unused fields: SalvageCrateCollide.BannerChance, SalvageCrateCollide.LevelUpRadius, SalvageCrateCollide.PorterChance, SalvageCrateCollide.ResourceChance.
Existing task matches: #695 Port 303: SalvageCrateCollide вЂ” crates: its four unread chances and radius and 3 uncited functions.

### G235: RW 0x00875FB2

Stops: S-1600, S-589, S-7499. Kinds: code, project.
Functions: RW 0x004B3D8D (ported), RW 0x0066137C (ported), RW 0x00661673 (ported), RW 0x0066341C (ported), RW 0x0068C0C5 (unknown), RW 0x0068C213 (ported), RW 0x0068F1A8 (ported), RW 0x006962DB (ported), RW 0x006968BC (ported), RW 0x00697E50 (ported), RW 0x006CCB4F (unknown), RW 0x0073D5C2 (ported), RW 0x00756C87 (unknown), RW 0x00792997 (unknown), RW 0x00792AFF (ported), RW 0x0079D8EF (ported), RW 0x00861E6E (ported), RW 0x008620DD (ported), RW 0x0086D056 (partial), RW 0x008700AC (ported), RW 0x00871B9B (ported), RW 0x00875FB2 (ported), RW 0x00876481 (ported), RW 0x00876A08 (ported), RW 0x008791C5 (ported), RW 0x0089F114 (unknown), RW 0x0089F42D (ported), RW 0x008BFBAE (ported), RW 0x00910025 (unknown), RW 0x00910070 (unknown), RW 0x00910179 (unknown), RW 0x00910380 (unknown), RW 0x0091062F (unknown), RW 0x009108EE (unknown). Classes: AttributeModifierAuraUpdate, DamageInfo, HordeContain, MetaImpactNugget.
Candidates: 1 none/partial functions; 11 unknown status.
Red buttons: none linked.
Unused fields: HordeContain.LivingWorldOverloadTemplate, HordeContain.RanksToJustFreeWhenAttacking.
Existing task matches: #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate.

### G236: RW 0x0068BBE0

Stops: S-2610. Kinds: code.
Functions: RW 0x006616AC (unknown), RW 0x0068BBE0 (ported), RW 0x006961F1 (ported), RW 0x0069675C (unknown), RW 0x006CA935 (ported), RW 0x006FF7FA (ported), RW 0x00744AAA (unknown), RW 0x0086C6EB (ported), RW 0x0086FA87 (ported), RW 0x00870C29 (ported), RW 0x00875221 (ported), RW 0x00875550 (ported), RW 0x00911A58 (unknown). Classes: HordeAttackNugget, HordeContain, OpenContain.
Candidates: 0 none/partial functions; 4 unknown status.
Red buttons: none linked.
Unused fields: HordeContain.LivingWorldOverloadTemplate, HordeContain.RanksToJustFreeWhenAttacking.
Existing task matches: #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate.

### G237: RW 0x0092F778

Stops: S-1955. Kinds: code.
Functions: RW 0x0092F778 (unknown), RW 0x009304DB (unknown), RW 0x009305CE (unknown), RW 0x009D2437 (unknown). Classes: LifetimeUpdate, StealthUpdate.
Candidates: 0 none/partial functions; 4 unknown status.
Red buttons: none linked.
Unused fields: StealthUpdate.HintDetectableConditions.
Existing task matches: none.

### G238: RW 0x00467B4D

Stops: S-115. Kinds: code.
Functions: RW 0x00467B4D (none), RW 0x004CEA1C (ported), RW 0x004CEA6F (ported), RW 0x004CEADF (ported), RW 0x004E4000 (none), RW 0x004E49C1 (unknown), RW 0x004E592B (none). Classes: W3DFloorDraw, W3DPropDraw.
Candidates: 3 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: #570 Port 246: Client fades: W3DTreeDraw FadeDistance / FadeTarget, W3DFloorDraw FloorFadeRateOnObjectDeath, SubObjectsUpgrade WaitBeforeFadeInSeconds.

### G239: RW 0x0045CB3B

Stops: S-1241. Kinds: code.
Functions: RW 0x0045CB3B (unknown), RW 0x0045CC91 (unknown), RW 0x00461A83 (unknown), RW 0x004A7C71 (unknown). Classes: none.
Candidates: 0 none/partial functions; 4 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G240: RW 0x006B5F78

Stops: S-1324. Kinds: code.
Functions: RW 0x006B5F78 (unknown), RW 0x006BDD98 (unknown), RW 0x006BDEC7 (unknown), RW 0x006BDF30 (unknown). Classes: LivingWorldLogic.
Candidates: 0 none/partial functions; 4 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G241: RW 0x006E189C

Stops: S-1338. Kinds: code.
Functions: RW 0x006E189C (unknown), RW 0x009326A3 (unknown), RW 0x00932A8D (unknown), RW 0x00932EB8 (unknown). Classes: none.
Candidates: 0 none/partial functions; 4 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G242: RW 0x0064148E

Stops: S-1407, S-1560. Kinds: code.
Functions: RW 0x00638D22 (unknown), RW 0x00641160 (unknown), RW 0x0064126F (unknown), RW 0x0064148E (unknown), RW 0x00644148 (unknown), RW 0x006DD398 (ported), RW 0x009787C9 (unknown), RW 0x00AAA5A0 (unknown), RW 0x00AAA6C0 (unknown). Classes: none.
Candidates: 0 none/partial functions; 8 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G243: RW 0x006729A3

Stops: S-164. Kinds: code.
Functions: RW 0x006729A3 (ported), RW 0x006729E9 (none), RW 0x006E861E (ported), RW 0x006ECC06 (unknown), RW 0x008E1DAD (ported), RW 0x008E1E64 (partial), RW 0x00935051 (unknown), RW 0x00936B7D (ported). Classes: none.
Candidates: 2 none/partial functions; 2 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G244: RW 0x006ED071

Stops: S-1834. Kinds: code.
Functions: RW 0x006ED071 (ported), RW 0x006EE12D (unknown), RW 0x006F2FC3 (unknown), RW 0x006F9850 (unknown), RW 0x00767B0E (unknown). Classes: none.
Candidates: 0 none/partial functions; 4 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G245: RW 0x0082DFB7

Stops: S-1874, S-4640, S-4641, S-531. Kinds: code.
Functions: RW 0x004B0A98 (unknown), RW 0x0050C485 (unknown), RW 0x0067F52B (ported), RW 0x00680A75 (ported), RW 0x00693919 (ported), RW 0x006A077A (unknown), RW 0x006A3B77 (unknown), RW 0x006A4260 (unknown), RW 0x006A4E27 (unknown), RW 0x006AA8FB (unknown), RW 0x006AA91D (ported), RW 0x006B183D (unknown), RW 0x0073232B (ported), RW 0x00732888 (ported), RW 0x00732926 (ported), RW 0x00732DE7 (ported), RW 0x00763D11 (ported), RW 0x0076A247 (unknown), RW 0x0076F788 (unknown), RW 0x0076F7FB (unknown), RW 0x0081E7B2 (unknown), RW 0x0081EAD3 (unknown), RW 0x0082DFB7 (unknown), RW 0x0082E20F (unknown), RW 0x0082E8F3 (unknown), RW 0x00AD2CE0 (ported). Classes: AIGroup, ActionManager, CommandTranslator, CostModifierUpgrade, GeometryInfo, InGameUI, ObjectFilter, RadiusDecal, RadiusDecalTemplate, SpecialPowerTemplate, WorldHeightMap.
Candidates: 0 none/partial functions; 16 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: #766 Local 100: upgrade modules: CommandSetUpgrade (515 modules), CostModifierUpgrade, CastleUpgrade, GiveUpgradeUpdate, AudioLoopUpgrade, WallUpgradeUpdate, LocomotorSetUpgrade: 27 uncited own functions.

### G246: RW 0x0091EC52

Stops: S-1913, S-4510, S-4670. Kinds: code.
Functions: RW 0x004591D4 (unknown), RW 0x00641DED (unknown), RW 0x006DA95E (ported), RW 0x006E5FB3 (unknown), RW 0x007234EC (unknown), RW 0x007237F8 (unknown), RW 0x0091EB77 (unknown), RW 0x0091EBD1 (unknown), RW 0x0091EC52 (unknown), RW 0x0091ED91 (unknown), RW 0x0091F90D (unknown), RW 0x0091FC9C (unknown), RW 0x009205C4 (unknown). Classes: AptButtonTooltipMap, AptOptionsScreen, AudioManager, GameLODManager, GlobalData, MiscAudio, ShellEnvironment.
Candidates: 0 none/partial functions; 12 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G247: RW 0x006D1305

Stops: S-1980. Kinds: code.
Functions: RW 0x006D1305 (ported), RW 0x0079468B (unknown), RW 0x007966BB (unknown), RW 0x007967C1 (unknown), RW 0x0086561F (unknown). Classes: BuildAssistant, GarrisonContain, PlayerTemplate.
Candidates: 0 none/partial functions; 4 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G248: RW 0x0077208A

Stops: S-1982. Kinds: code.
Functions: RW 0x0077208A (unknown), RW 0x0077229A (unknown), RW 0x0077A348 (unknown), RW 0x0077A3C8 (unknown). Classes: GUICommandTranslator.
Candidates: 0 none/partial functions; 4 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G249: RW 0x005E3F49

Stops: S-222. Kinds: code.
Functions: RW 0x005E3F49 (ported), RW 0x00663D70 (ported), RW 0x006695EF (unknown), RW 0x0068C82D (ported), RW 0x006E8937 (partial), RW 0x006F1B3E (unknown), RW 0x006F74D0 (unknown). Classes: AIUpdateInterface, GiantBirdAIUpdate.
Candidates: 1 none/partial functions; 3 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G250: RW 0x00808F53

Stops: S-251. Kinds: code.
Functions: RW 0x0079DFFA (unknown), RW 0x0079E3A8 (unknown), RW 0x0079F0E1 (ported), RW 0x0079F303 (ported), RW 0x0079F486 (ported), RW 0x0079F704 (unknown), RW 0x00808F53 (unknown). Classes: ScoreKeeper.
Candidates: 0 none/partial functions; 4 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G251: RW 0x0044F3AB

Stops: S-291, S-761. Kinds: code.
Functions: RW 0x0044E3B6 (unknown), RW 0x0044EE19 (unknown), RW 0x0044EFBB (unknown), RW 0x0044F3AB (unknown), RW 0x0044F9DA (unknown), RW 0x0050434F (unknown), RW 0x00532875 (none), RW 0x006D89F2 (unknown). Classes: none.
Candidates: 1 none/partial functions; 7 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G252: RW 0x00795221

Stops: S-307. Kinds: code.
Functions: RW 0x00795221 (unknown), RW 0x0079586E (unknown), RW 0x00855EC7 (unknown), RW 0x008560C0 (unknown). Classes: BuildAssistant, WallHubBehavior.
Candidates: 0 none/partial functions; 4 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: #787 Local 109: builders and walls, second pass: WorkerAIUpdate, DozerAIUpdate, WallHubBehavior, ManTheWallsSpecialPower вЂ” 34 functions.

### G253: RW 0x00441E4A

Stops: S-344. Kinds: code.
Functions: RW 0x00441E4A (unknown), RW 0x00808A43 (unknown), RW 0x00809253 (unknown), RW 0x00809404 (unknown). Classes: VictoryConditions.
Candidates: 0 none/partial functions; 4 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G254: RW 0x006308F3

Stops: S-4021, S-4121. Kinds: code, project.
Functions: RW 0x006308F3 (unknown), RW 0x006AE9B7 (unknown), RW 0x006B205F (unknown), RW 0x006DCE1C (unknown), RW 0x0079ECCB (unknown), RW 0x007A56B6 (unknown), RW 0x007A634B (unknown), RW 0x007A7184 (unknown). Classes: GameState, ScoreKeeper, TeamFactory, TeamPrototype.
Candidates: 0 none/partial functions; 8 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G255: RW 0x0066110A

Stops: S-414. Kinds: code.
Functions: RW 0x0066110A (unknown), RW 0x006ADBEB (ported), RW 0x008EEC71 (unknown), RW 0x0097DAFC (unknown), RW 0x0097DFCD (unknown). Classes: FireLogicNugget.
Candidates: 0 none/partial functions; 4 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G256: RW 0x00683D53

Stops: S-4252. Kinds: code.
Functions: RW 0x00680753 (unknown), RW 0x00683D53 (unknown), RW 0x00683F8C (unknown), RW 0x00684167 (unknown). Classes: TerrainLogic, TerrainTrees.
Candidates: 0 none/partial functions; 4 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G257: RW 0x0060331E

Stops: S-4256. Kinds: code.
Functions: RW 0x0060331E (unknown), RW 0x0060A45B (unknown), RW 0x00667BEF (ported), RW 0x007B2F8A (unknown), RW 0x007B3063 (unknown). Classes: AIGateUpdate, LiveScripting, ScriptEngine, ScriptGroup, SequentialScript.
Candidates: 0 none/partial functions; 4 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G258: RW 0x009C4EBD

Stops: S-4755. Kinds: code.
Functions: RW 0x009C4EBD (unknown), RW 0x009C54C4 (unknown), RW 0x009C5A66 (unknown), RW 0x009C5EA4 (unknown). Classes: none.
Candidates: 0 none/partial functions; 4 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G259: RW 0x00582070

Stops: S-6020. Kinds: code.
Functions: RW 0x00582070 (unknown), RW 0x00586EE0 (unknown), RW 0x005870A0 (unknown), RW 0x00587320 (unknown). Classes: none.
Candidates: 0 none/partial functions; 4 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G260: OilSpillUpdate

Stops: S-6025. Kinds: code.
Functions: RW 0x0064CC80 (unknown), RW 0x0064CCB8 (unknown), RW 0x0088F7D8 (unknown), RW 0x0088F863 (unknown), RW 0x0088F97A (ported), RW 0x0088FA81 (ported). Classes: FireWeaponUpdate, FireWeaponWhenDeadBehavior, OilSpillUpdate.
Candidates: 0 none/partial functions; 4 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G261: RW 0x00676A8A

Stops: S-712. Kinds: code.
Functions: RW 0x00676A8A (ported), RW 0x00748C0B (unknown), RW 0x00748D8A (unknown), RW 0x0074DADA (unknown), RW 0x0074DF88 (unknown). Classes: FiringTracker.
Candidates: 0 none/partial functions; 4 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G262: RW 0x0042E59E

Stops: S-7220. Kinds: project.
Functions: RW 0x0042E59E (unknown), RW 0x0047E865 (unknown), RW 0x00500313 (unknown), RW 0x0060047C (unknown). Classes: none.
Candidates: 0 none/partial functions; 4 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G263: RW 0x006CD0BE

Stops: S-7322. Kinds: code.
Functions: RW 0x006CD0BE (unknown), RW 0x006CD1FD (ported), RW 0x006CEF6D (ported), RW 0x007408A8 (unknown), RW 0x0074094C (unknown), RW 0x0090FB7A (unknown). Classes: ProjectileStreamUpdate, W3DProjectileStreamDraw, WeaponTemplate.
Candidates: 0 none/partial functions; 4 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G264: RW 0x00670570

Stops: S-7325. Kinds: code.
Functions: RW 0x00670570 (unknown), RW 0x00677026 (unknown), RW 0x0067BE90 (unknown), RW 0x00868A70 (unknown). Classes: DrawableManager, OpenContain.
Candidates: 0 none/partial functions; 4 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G265: RW 0x006886E0

Stops: S-7332. Kinds: project.
Functions: RW 0x006886E0 (unknown), RW 0x0088BED2 (none), RW 0x0088C7DB (unknown), RW 0x0088E17F (unknown). Classes: none.
Candidates: 1 none/partial functions; 3 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G266: AIUpdateInterface

Stops: S-7342, S-7599, S-7600, S-7703. Kinds: code.
Functions: RW 0x00662D0B (none), RW 0x00662EEE (none), RW 0x00662F26 (none), RW 0x00662F61 (none), RW 0x00664533 (ported), RW 0x006646F5 (none), RW 0x0066472E (none), RW 0x006647A7 (none), RW 0x006647E0 (none), RW 0x00664819 (none), RW 0x006648E2 (none), RW 0x00664952 (ported), RW 0x00667323 (none), RW 0x006673C7 (none), RW 0x00668167 (none), RW 0x00668D8E (none), RW 0x0066B6B8 (ported), RW 0x0066B72D (ported), RW 0x0066B88C (ported), RW 0x0066BD33 (ported), RW 0x0066BEE5 (none), RW 0x0066E078 (ported), RW 0x0066E0AA (ported), RW 0x0066E166 (ported), RW 0x0082C4E9 (ported), RW 0x0082C798 (ported), RW 0x008A37F3 (none). Classes: AIAttackState, AICombineState, AIEnterAndAttackState, AIMoveAndEvacuateState, AIUpdateInterface, ActionManager, CommandSet, DozerAIUpdate, GiantBirdAIUpdate, HordeAIUpdate, ProneUpdate.
Candidates: 16 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: #787 Local 109: builders and walls, second pass: WorkerAIUpdate, DozerAIUpdate, WallHubBehavior, ManTheWallsSpecialPower вЂ” 34 functions; #650 Port 283: HorseHordeContain and HordeAIUpdate: the cavalry horde's uncited own functions (classify, then port the missing ones).

### G267: AISpecialPowerUpdate

Stops: S-7389. Kinds: code.
Functions: RW 0x00449681 (ported), RW 0x0068B678 (ported), RW 0x0079F922 (unknown), RW 0x00992724 (ported), RW 0x009CD79A (unknown), RW 0x009E85C6 (unknown), RW 0x00A0229F (unknown). Classes: AISpecialPowerUpdate.
Candidates: 0 none/partial functions; 4 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G268: RW 0x0066E233

Stops: S-781. Kinds: code.
Functions: RW 0x0066E233 (ported), RW 0x0068D7AB (ported), RW 0x006F503B (unknown), RW 0x00744B71 (unknown), RW 0x007463E8 (unknown), RW 0x0074A594 (unknown). Classes: PhysicsBehavior.
Candidates: 0 none/partial functions; 4 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G269: RW 0x0081E60B

Stops: S-1427, S-2000, S-2002, S-286, S-4840, S-4944, S-5902, S-7321. Kinds: code.
Functions: RW 0x0041B730 (unknown), RW 0x0044DDEC (ported), RW 0x004B7BA0 (unknown), RW 0x004B9696 (ported), RW 0x005F11A8 (unknown), RW 0x005F2078 (unknown), RW 0x0062DCE4 (unknown), RW 0x0063F3BF (ported), RW 0x00663EF4 (ported), RW 0x0066403D (unknown), RW 0x0066826E (unknown), RW 0x0066C609 (ported), RW 0x00672F44 (partial), RW 0x0067CAC6 (unknown), RW 0x0067F2D0 (ported), RW 0x0067FFA6 (unknown), RW 0x0068044E (unknown), RW 0x00680A75 (ported), RW 0x0068BBE0 (ported), RW 0x0068C02E (ported), RW 0x0068C05A (unknown), RW 0x0069029B (ported), RW 0x0069472D (unknown), RW 0x00698EC3 (ported), RW 0x00699349 (ported), RW 0x0069C860 (unknown), RW 0x0069D27E (unknown), RW 0x0069D674 (unknown), RW 0x006A4260 (unknown), RW 0x006C8652 (ported), RW 0x006C9147 (ported), RW 0x006CC07C (ported), RW 0x006E4D2C (ported), RW 0x006EA4E7 (unknown), RW 0x006F0741 (ported), RW 0x006F0B27 (unknown), RW 0x006F1B3E (unknown), RW 0x006F25FE (unknown), RW 0x006F5BB0 (ported), RW 0x006FF7FA (ported), RW 0x00740049 (unknown), RW 0x0079350E (ported), RW 0x0081DB0C (unknown), RW 0x0081E147 (unknown), RW 0x0081E60B (unknown), RW 0x0081E7B2 (unknown), RW 0x0082C5A6 (unknown), RW 0x0082CBD5 (ported), RW 0x0082D0A1 (ported), RW 0x0082D780 (unknown), RW 0x0082D925 (ported), RW 0x0082DDE1 (unknown), RW 0x0083BCB1 (unknown), RW 0x00871536 (ported), RW 0x00873FE8 (ported), RW 0x0088397F (ported), RW 0x00886831 (ported), RW 0x00886878 (ported), RW 0x008868C2 (ported), RW 0x00889AAF (ported), RW 0x0088C217 (ported), RW 0x008AD6A4 (ported), RW 0x0090D16A (ported), RW 0x00935051 (unknown), RW 0x0093527F (unknown), RW 0x009356DF (unknown), RW 0x00935BFF (unknown), RW 0x00935FAA (ported), RW 0x00936B7D (ported). Classes: AIWorld, ActionManager, BridgeBehavior, CitadelSlaughterHordeContain, CommandTranslator, DozerAIUpdate, GUICommandTranslator, GeometryInfo, HordeContain, InGameUI, ObjectFilter, RebuildHoleBehavior, RebuildHoleExposeDie, ResourceGatheringManager, SlaughterHordeContainBehaviorData, ThingTemplate, W3DModelDraw, W3DScriptedModelDraw, WeaponSet, WorkerAIUpdate.
Candidates: 1 none/partial functions; 33 unknown status.
Red buttons: none linked.
Unused fields: HordeContain.LivingWorldOverloadTemplate, HordeContain.RanksToJustFreeWhenAttacking, W3DScriptedModelDraw.BirthFadeAdditive, W3DScriptedModelDraw.HighDetailOnly, W3DScriptedModelDraw.MinLODRequired, W3DScriptedModelDraw.UseDefaultAnimation, W3DScriptedModelDraw.UseProducerTexture.
Existing task matches: #787 Local 109: builders and walls, second pass: WorkerAIUpdate, DozerAIUpdate, WallHubBehavior, ManTheWallsSpecialPower вЂ” 34 functions; #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate; #569 Port 245: W3DScriptedModelDraw: MinLODRequired, HighDetailOnly, UseProducerTexture, UseDefaultAnimation, BirthFadeAdditive; #296 [deferred] Local 40: bridge logic as retail: towers, damage stages, pathfinding layer, radar (BridgeBehavior, S-7223).

### G270: SymbioticStructuresBody

Stops: S-7195, S-7408, S-7618. Kinds: code.
Functions: RW 0x0064DFEB (ported), RW 0x006512BE (unknown), RW 0x008C4C82 (unknown), RW 0x008C4CF4 (none), RW 0x008C4DE3 (none), RW 0x008C4EA4 (none), RW 0x008C4EE0 (ported), RW 0x008C4F2D (ported), RW 0x008C4F84 (none), RW 0x008C501C (none), RW 0x008C5123 (none), RW 0x008C519A (none), RW 0x008C52B5 (none), RW 0x008C5304 (none), RW 0x008C53AF (none), RW 0x008C53D0 (none), RW 0x00AD1920 (ported). Classes: ActiveBody, BodyModuleInterface, GlobalData, ImmortalBody, ObjectSideSnapshot, SymbioticStructuresBody.
Candidates: 11 none/partial functions; 2 unknown status.
Red buttons: none linked.
Unused fields: ActiveBody.EnteringDamagedTransitionTime, ActiveBody.ReallyDamagedAttributeModifier, SymbioticStructuresBody.Symbiote.
Existing task matches: #567 Port 243: Body damage states: ActiveBody EnteringDamagedTransitionTime and the Damaged / ReallyDamaged attribute modifiers (ActiveBody, RespawnBody).

### G271: RW 0x00989449

Stops: S-1121, S-3900, S-3901, S-4075, S-720, S-721. Kinds: code.
Functions: RW 0x00441B7C (ported), RW 0x00610A21 (unknown), RW 0x00625AF1 (unknown), RW 0x0064322E (unknown), RW 0x0065DBFC (unknown), RW 0x0065DD0F (unknown), RW 0x006B5E3E (unknown), RW 0x008445C9 (unknown), RW 0x008D2EC6 (unknown), RW 0x008D3AF1 (unknown), RW 0x008D3FD9 (unknown), RW 0x008D4301 (unknown), RW 0x008D4761 (unknown), RW 0x008D4E89 (unknown), RW 0x008D57D8 (unknown), RW 0x008D75AC (unknown), RW 0x008D77B5 (unknown), RW 0x008D7C78 (unknown), RW 0x008D7F77 (unknown), RW 0x008D8233 (unknown), RW 0x008D826F (unknown), RW 0x008D836B (unknown), RW 0x008D8553 (unknown), RW 0x008D861A (unknown), RW 0x008D86D0 (unknown), RW 0x008D8735 (unknown), RW 0x008D8A90 (unknown), RW 0x00989449 (unknown). Classes: AptQuitMenu, DisconnectManager, GameWorld, GlobalData, LobbyStart, NetGameSession.
Candidates: 0 none/partial functions; 27 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G272: RW 0x00B5470F

Stops: S-1483, S-4555. Kinds: code.
Functions: RW 0x00533630 (unknown), RW 0x005424E0 (unknown), RW 0x00813514 (unknown), RW 0x008145DD (unknown), RW 0x00B5442B (unknown), RW 0x00B5470F (unknown), RW 0x00B54BFC (unknown), RW 0x00B54C1A (unknown), RW 0x00B54D33 (unknown). Classes: none.
Candidates: 0 none/partial functions; 9 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G273: RW 0x0062215B

Stops: S-1484, S-1771. Kinds: code.
Functions: RW 0x00602B76 (unknown), RW 0x0062215B (unknown), RW 0x00624EE1 (unknown), RW 0x006DA774 (unknown), RW 0x006E56F3 (unknown), RW 0x0075DB34 (unknown), RW 0x0091EA39 (unknown), RW 0x0091FC9C (unknown), RW 0x00925699 (unknown). Classes: AptTimeLine, OptionPreferences.
Candidates: 0 none/partial functions; 9 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G274: linked stops S-179, S-3504

Stops: S-179, S-3504. Kinds: code.
Functions: RW 0x00821AB4 (unknown), RW 0x00821EBD (unknown), RW 0x008220A3 (unknown), RW 0x008221F5 (unknown), RW 0x0082259F (unknown), RW 0x00822626 (unknown), RW 0x00928CDB (unknown), RW 0x0093BD82 (unknown), RW 0x0093BF28 (unknown). Classes: AptSkirmish, GameInfo, SkirmishPreferences.
Candidates: 0 none/partial functions; 9 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G275: linked stops S-1892, S-1920

Stops: S-1892, S-1920. Kinds: code.
Functions: RW 0x0048EDED (unknown), RW 0x0066403D (unknown), RW 0x00680A75 (ported), RW 0x006819D9 (unknown), RW 0x00690E97 (ported), RW 0x0069B76C (unknown), RW 0x0069E8D0 (unknown), RW 0x0069F54F (unknown), RW 0x006F5BB0 (ported), RW 0x0081E7B2 (unknown), RW 0x0081F743 (unknown), RW 0x0083816E (unknown). Classes: GlobalData, LockstepDriver, SmoothStats, TerrainLogic, UnitVoiceResponse.
Candidates: 0 none/partial functions; 9 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G276: RW 0x0045C675

Stops: S-240, S-241. Kinds: code.
Functions: RW 0x00440D63 (unknown), RW 0x00453227 (unknown), RW 0x00456F44 (unknown), RW 0x0045C675 (unknown), RW 0x00461615 (unknown), RW 0x00461771 (unknown), RW 0x005D90C3 (unknown), RW 0x006DB821 (unknown), RW 0x006DBA01 (unknown). Classes: AudioSettings.
Candidates: 0 none/partial functions; 9 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G277: RW 0x0044D25B

Stops: S-2455, S-3950. Kinds: code.
Functions: RW 0x0044D25B (unknown), RW 0x0044D41B (unknown), RW 0x0044D47A (unknown), RW 0x004626A3 (unknown), RW 0x00503C33 (unknown), RW 0x0050434F (unknown), RW 0x006D879A (unknown), RW 0x006D8EBE (unknown), RW 0x006D9242 (unknown). Classes: AptPalantir, TacticalView.
Candidates: 0 none/partial functions; 9 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G278: RW 0x00988BB5

Stops: S-4073, S-724. Kinds: code.
Functions: RW 0x00847AB2 (unknown), RW 0x00847ED0 (unknown), RW 0x008483E4 (unknown), RW 0x008490E0 (unknown), RW 0x00849677 (unknown), RW 0x008496C2 (unknown), RW 0x0084C257 (unknown), RW 0x0084C844 (unknown), RW 0x00988BB5 (unknown). Classes: AptLanLobby, GameSorter, NetByteReader.
Candidates: 0 none/partial functions; 9 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G279: RW 0x006E9B92

Stops: S-4160, S-4161. Kinds: code.
Functions: RW 0x00660AC1 (ported), RW 0x0066B564 (ported), RW 0x006E8190 (ported), RW 0x006E9B92 (unknown), RW 0x006EC931 (unknown), RW 0x006EFBB8 (ported), RW 0x006FE456 (unknown), RW 0x0074403F (unknown), RW 0x00751A8A (unknown), RW 0x00756452 (unknown), RW 0x00756651 (unknown), RW 0x0077122F (unknown), RW 0x008657D1 (unknown), RW 0x008682D9 (ported), RW 0x0086A2D0 (ported), RW 0x00AD1B20 (ported). Classes: AIMoveToPositionAndEnterState, OpenContain.
Candidates: 0 none/partial functions; 9 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G280: RW 0x00942733

Stops: S-1260, S-200, S-7427, S-850, S-951. Kinds: code.
Functions: RW 0x005E2DF4 (ported), RW 0x00625E0A (ported), RW 0x00661E50 (ported), RW 0x0066288A (ported), RW 0x0066491A (ported), RW 0x00664D12 (ported), RW 0x00667290 (ported), RW 0x0066F603 (unknown), RW 0x0071EBDA (unknown), RW 0x0073AF89 (unknown), RW 0x0073B22F (unknown), RW 0x0073B304 (unknown), RW 0x0073B4A0 (unknown), RW 0x0073BB7D (unknown), RW 0x0073D0BA (ported), RW 0x0075CAA9 (unknown), RW 0x00780D22 (unknown), RW 0x007810BC (unknown), RW 0x00797B06 (ported), RW 0x00797B63 (ported), RW 0x00797B6D (ported), RW 0x00798057 (unknown), RW 0x007983E3 (ported), RW 0x007985AA (ported), RW 0x00799039 (ported), RW 0x007995FB (ported), RW 0x0079969F (ported), RW 0x007999E2 (ported), RW 0x00799ACB (ported), RW 0x00799F0F (unknown), RW 0x0079A237 (ported), RW 0x0079A472 (ported), RW 0x0079AC19 (ported), RW 0x0079ADE9 (partial), RW 0x0079B0F1 (ported), RW 0x0079B374 (ported), RW 0x0079B3C4 (ported), RW 0x0079B98B (ported), RW 0x0079BD2F (unknown), RW 0x0079C17D (ported), RW 0x0079C543 (unknown), RW 0x0079CB47 (ported), RW 0x0079CCF2 (ported), RW 0x0079CF2A (ported), RW 0x0082D780 (unknown), RW 0x00851E97 (ported), RW 0x00858701 (ported), RW 0x008BD372 (ported), RW 0x00940AE9 (unknown), RW 0x00942490 (unknown), RW 0x0094251F (unknown), RW 0x00942733 (unknown), RW 0x009438C8 (unknown), RW 0x00943D6F (unknown), RW 0x0094518D (unknown). Classes: AIUpdateInterface, ActiveBody, CastleMemberBehavior, CommandButton, CommandSet, ControlBar, InGameUI, ScienceStore, SpecialPowerStore, TunnelContain, UpgradeCenter.
Candidates: 1 none/partial functions; 22 unknown status.
Red buttons: none linked.
Unused fields: ActiveBody.EnteringDamagedTransitionTime, ActiveBody.ReallyDamagedAttributeModifier, CastleMemberBehavior.StoreUpgradePrice.
Existing task matches: #688 Port 296: Castles вЂ” CastleBehavior, CastleMemberBehavior and FoundationAIUpdate: the uncited own functions (27) and their five unread fields; #567 Port 243: Body damage states: ActiveBody EnteringDamagedTransitionTime and the Damaged / ReallyDamaged attribute modifiers (ActiveBody, RespawnBody).

### G281: RW 0x0088FF6F

Stops: S-7213, S-7214, S-983. Kinds: code.
Functions: RW 0x00458DFF (unknown), RW 0x0046C1DA (library), RW 0x004728D3 (unknown), RW 0x00472980 (unknown), RW 0x00494615 (ported), RW 0x004B1B5A (ported), RW 0x00664485 (ported), RW 0x00672B5B (ported), RW 0x0068F2F1 (ported), RW 0x006B4E57 (ported), RW 0x0085FFC9 (unknown), RW 0x00865598 (ported), RW 0x0086DED1 (unknown), RW 0x00875221 (ported), RW 0x0087545C (none), RW 0x00875550 (ported), RW 0x00878B63 (unknown), RW 0x00878B7E (unknown), RW 0x00878EE5 (unknown), RW 0x00881863 (unknown), RW 0x0088EE6E (ported), RW 0x0088FBCA (ported), RW 0x0088FBEE (ported), RW 0x0088FDAD (ported), RW 0x0088FE48 (unknown), RW 0x0088FF6F (ported), RW 0x008901AB (ported), RW 0x008904B8 (ported), RW 0x00890738 (ported), RW 0x00890CD9 (unknown), RW 0x008A7D1D (ported), RW 0x008A85D0 (ported), RW 0x008A96A0 (ported), RW 0x008C1E4E (ported), RW 0x008C23EA (ported), RW 0x008C258F (ported), RW 0x009034EC (unknown), RW 0x00911407 (unknown). Classes: AIUpdateInterface, ActiveBody, BodyModuleInterface, EntEnragedUpdate, FXEvent, FireSpreadUpdate, FlammableUpdate, GameLogicSettings, HordeContain, RubbleRiseUpdate, StructureCollapseUpdate, StructureToppleUpdate.
Candidates: 1 none/partial functions; 13 unknown status.
Red buttons: none linked.
Unused fields: ActiveBody.EnteringDamagedTransitionTime, ActiveBody.ReallyDamagedAttributeModifier, HordeContain.LivingWorldOverloadTemplate, HordeContain.RanksToJustFreeWhenAttacking, RubbleRiseUpdate.MaxRubbleRiseDelay, RubbleRiseUpdate.MinRubbleRiseDelay.
Existing task matches: #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate; #567 Port 243: Body damage states: ActiveBody EnteringDamagedTransitionTime and the Damaged / ReallyDamaged attribute modifiers (ActiveBody, RespawnBody).

### G282: RW 0x0087468B

Stops: S-1502, S-1751, S-1830, S-224, S-4461, S-584, S-892. Kinds: code.
Functions: RW 0x00490AC4 (ported), RW 0x005E4C40 (ported), RW 0x005E72F7 (unknown), RW 0x005E737B (unknown), RW 0x005E8865 (ported), RW 0x0066288A (ported), RW 0x006631BF (ported), RW 0x0066352C (ported), RW 0x00663F48 (unknown), RW 0x00664485 (ported), RW 0x0066498A (ported), RW 0x006695EF (unknown), RW 0x00669932 (ported), RW 0x0066C748 (unknown), RW 0x0066D0B3 (unknown), RW 0x0066D16E (ported), RW 0x0066E233 (ported), RW 0x0066E58F (ported), RW 0x006729E9 (none), RW 0x00680A75 (ported), RW 0x0068B36F (ported), RW 0x0068BBF4 (ported), RW 0x006939DF (ported), RW 0x00694183 (ported), RW 0x00694233 (ported), RW 0x006EA4E7 (unknown), RW 0x006EA5B2 (unknown), RW 0x006EB792 (unknown), RW 0x006ED071 (ported), RW 0x006ED46C (unknown), RW 0x006EDFD7 (ported), RW 0x006EE12D (unknown), RW 0x006EF865 (unknown), RW 0x006F0741 (ported), RW 0x006F1B3E (unknown), RW 0x006F6286 (unknown), RW 0x006F74D0 (unknown), RW 0x006F7938 (unknown), RW 0x006F9850 (unknown), RW 0x006FCD08 (unknown), RW 0x006FDB3D (unknown), RW 0x0070C31E (ported), RW 0x00741724 (unknown), RW 0x00742DD0 (unknown), RW 0x00744375 (unknown), RW 0x0074A33C (unknown), RW 0x00765598 (unknown), RW 0x00765F31 (ported), RW 0x00767A66 (unknown), RW 0x007767A9 (ported), RW 0x00851E97 (ported), RW 0x00852E2A (ported), RW 0x0086BCF9 (ported), RW 0x0086BD54 (ported), RW 0x0086C61E (ported), RW 0x0086D170 (ported), RW 0x0086E214 (unknown), RW 0x00870A1B (unknown), RW 0x00871897 (unknown), RW 0x00872EFC (ported), RW 0x00873FE8 (ported), RW 0x0087468B (ported), RW 0x0087594C (ported), RW 0x00875FB2 (ported), RW 0x00877A7A (ported), RW 0x00877D89 (ported), RW 0x00877E12 (ported), RW 0x0089E169 (ported), RW 0x008C0518 (ported), RW 0x008E26B7 (ported), RW 0x0098B828 (unknown), RW 0x0098B874 (unknown), RW 0x0098F819 (unknown), RW 0x009902A1 (unknown), RW 0x009B501B (ported). Classes: AIMover, HordeContain, HordeMemberCollide, PhysicsBehavior, TransportContain.
Candidates: 1 none/partial functions; 32 unknown status.
Red buttons: none linked.
Unused fields: HordeContain.LivingWorldOverloadTemplate, HordeContain.RanksToJustFreeWhenAttacking.
Existing task matches: #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate; #492 Port 210: Horde wall scaling: connect the collide entry so goblins and spiderlings climb walls (S-4461).

### G283: W3DScriptedModelDraw

Stops: S-7616. Kinds: code.
Functions: RW 0x004B48E0 (none), RW 0x004B5A01 (none), RW 0x004B8803 (none), RW 0x004BACA2 (none), RW 0x004C6D93 (none). Classes: W3DScriptedModelDraw.
Candidates: 5 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: W3DScriptedModelDraw.BirthFadeAdditive, W3DScriptedModelDraw.HighDetailOnly, W3DScriptedModelDraw.MinLODRequired, W3DScriptedModelDraw.UseDefaultAnimation, W3DScriptedModelDraw.UseProducerTexture.
Existing task matches: #569 Port 245: W3DScriptedModelDraw: MinLODRequired, HighDetailOnly, UseProducerTexture, UseDefaultAnimation, BirthFadeAdditive.

### G284: RW 0x0070E911

Stops: S-035, S-160. Kinds: code.
Functions: RW 0x004628BF (unknown), RW 0x00482A0C (unknown), RW 0x004AEC41 (unknown), RW 0x004B055D (unknown), RW 0x0063F3BF (ported), RW 0x0067DAE6 (unknown), RW 0x00681F0A (unknown), RW 0x00709DD8 (unknown), RW 0x0070D6EE (unknown), RW 0x0070E0B8 (unknown), RW 0x0070E911 (unknown). Classes: StandingWaterArea, TerrainLogic.
Candidates: 0 none/partial functions; 10 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G285: RW 0x006311ED

Stops: S-1182, S-453, S-525, S-528. Kinds: code.
Functions: RW 0x00443939 (unknown), RW 0x00443C81 (unknown), RW 0x00489C32 (unknown), RW 0x00602E64 (ported), RW 0x006253BF (unknown), RW 0x00630078 (unknown), RW 0x006311A0 (unknown), RW 0x006311ED (unknown), RW 0x006B34EE (unknown), RW 0x006DA774 (unknown), RW 0x00779C9C (unknown), RW 0x00779D0E (unknown), RW 0x00779F20 (unknown), RW 0x007827C8 (ported), RW 0x007BB04E (unknown), RW 0x007BB540 (unknown), RW 0x007BCAAE (unknown), RW 0x007BE0EA (unknown), RW 0x007BE239 (unknown), RW 0x007BE2EC (unknown), RW 0x007C0D9B (unknown), RW 0x007E5AB2 (unknown). Classes: InGameHud, InGameUI, LiveGame, LiveGameAudio, MissionObjectiveTracker, ParabolicEase, ScriptCameraDirector, ScriptClientRequest, ScriptEngine, ScriptEngineHost, ScriptLocalView, TacticalCamera.
Candidates: 0 none/partial functions; 20 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G286: RW 0x00694CCC

Stops: S-1220. Kinds: code.
Functions: RW 0x00694CCC (ported), RW 0x007379CB (ported), RW 0x00737BF7 (unknown), RW 0x00737D2D (unknown), RW 0x00737F35 (unknown), RW 0x007380A2 (unknown), RW 0x00738246 (unknown). Classes: EmotionTrackerUpdate, LiveScripting.
Candidates: 0 none/partial functions; 5 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G287: RW 0x006BBD55

Stops: S-1332. Kinds: code.
Functions: RW 0x006BBD55 (unknown), RW 0x006BC634 (unknown), RW 0x006DCFDF (unknown), RW 0x006DEA25 (unknown), RW 0x006E04AD (unknown). Classes: GameState, LivingWorld, LivingWorldLogic, NewGameMessage.
Candidates: 0 none/partial functions; 5 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G288: RW 0x0061D664

Stops: S-1400. Kinds: code.
Functions: RW 0x0061D664 (unknown), RW 0x0061ED51 (unknown), RW 0x0061EE28 (unknown), RW 0x0080A352 (unknown), RW 0x0080CC7E (unknown). Classes: none.
Candidates: 0 none/partial functions; 5 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G289: RW 0x0080CDA2

Stops: S-1402. Kinds: code.
Functions: RW 0x0080CDA2 (unknown), RW 0x0081A375 (unknown), RW 0x0081A452 (unknown), RW 0x009C3DC7 (unknown), RW 0x009C5D6F (unknown). Classes: AptMessageBox.
Candidates: 0 none/partial functions; 5 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G290: RW 0x00490EE6

Stops: S-1710, S-2341. Kinds: code.
Functions: RW 0x00490D08 (unknown), RW 0x00490EE6 (unknown), RW 0x0049112C (unknown), RW 0x0063C5E2 (unknown), RW 0x0063CB3E (unknown), RW 0x006429AD (unknown), RW 0x0064838D (unknown), RW 0x0065C67E (unknown), RW 0x0065CBEF (unknown), RW 0x0065D3F5 (unknown). Classes: GlobalData, OptionPreferences, VideoPlayer.
Candidates: 0 none/partial functions; 10 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G291: RW 0x00693C2D

Stops: S-1983. Kinds: code.
Functions: RW 0x00693C2D (unknown), RW 0x0075D366 (unknown), RW 0x0077B6D6 (unknown), RW 0x0077B72B (unknown), RW 0x0077B81B (unknown). Classes: SlaveAttackNugget.
Candidates: 0 none/partial functions; 5 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: #749 Port 313: Hold fire / auto-acquire buttons: the button image toggle and the related object's flag (S-1983, S-1992).

### G292: RW 0x0064CEBF

Stops: S-1984. Kinds: code.
Functions: RW 0x0064CEBF (unknown), RW 0x00891CD3 (ported), RW 0x00891D3B (unknown), RW 0x00891D62 (unknown), RW 0x00891F9B (ported), RW 0x00892013 (unknown), RW 0x0089209F (ported), RW 0x008921E3 (ported), RW 0x0089251C (ported), RW 0x008B1B87 (unknown), RW 0x008B1C5F (ported), RW 0x008B1C94 (ported), RW 0x008B1D10 (ported), RW 0x008B1DB3 (ported). Classes: DeployStyleAIUpdate, ToggleDeploySpecialAbilityUpdate.
Candidates: 0 none/partial functions; 5 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G293: RW 0x006230B6

Stops: S-4070. Kinds: code.
Functions: RW 0x006230B6 (unknown), RW 0x00847152 (unknown), RW 0x0091D7A3 (unknown), RW 0x0091E133 (unknown), RW 0x009CCC06 (unknown). Classes: none.
Candidates: 0 none/partial functions; 5 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G294: RW 0x00443D5F

Stops: S-4893. Kinds: code.
Functions: RW 0x00443D5F (unknown), RW 0x00443E2F (unknown), RW 0x00443E66 (unknown), RW 0x00614AA7 (unknown), RW 0x007CAFA5 (unknown). Classes: none.
Candidates: 0 none/partial functions; 5 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G295: RW 0x00472480

Stops: S-567. Kinds: code.
Functions: RW 0x00472480 (unknown), RW 0x0047266B (unknown), RW 0x00472792 (unknown), RW 0x00472EF4 (unknown), RW 0x00B52E10 (unknown). Classes: none.
Candidates: 0 none/partial functions; 5 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G296: PhysicsBehavior

Stops: S-782. Kinds: code.
Functions: RW 0x00441C56 (ported), RW 0x005E3A1B (ported), RW 0x006260E1 (unknown), RW 0x006630D7 (ported), RW 0x00693919 (ported), RW 0x006CF590 (ported), RW 0x006EF8A8 (unknown), RW 0x006F0741 (ported), RW 0x0070BA76 (ported), RW 0x0070C201 (ported), RW 0x00792559 (unknown), RW 0x0079261A (unknown), RW 0x00792997 (unknown), RW 0x00792A41 (ported), RW 0x00792A69 (ported), RW 0x00792AFF (ported), RW 0x00792DBD (ported), RW 0x0079308D (ported), RW 0x00793224 (ported), RW 0x00793372 (ported), RW 0x0079350E (ported), RW 0x009188EB (ported), RW 0x00B26610 (ported). Classes: GlobalData, PhysicsBehavior.
Candidates: 0 none/partial functions; 5 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G297: RW 0x008431B8

Stops: S-1261, S-4074, S-4553, S-4557. Kinds: code.
Functions: RW 0x00648BA7 (unknown), RW 0x00649A2A (unknown), RW 0x00649E5C (unknown), RW 0x0078A594 (unknown), RW 0x0081A3FB (unknown), RW 0x0084211D (unknown), RW 0x008421E0 (unknown), RW 0x008431B8 (unknown), RW 0x008434A1 (unknown), RW 0x0084392C (unknown), RW 0x00843D43 (unknown), RW 0x00847508 (unknown), RW 0x008479EB (unknown), RW 0x00953243 (unknown), RW 0x009534DE (unknown), RW 0x00953861 (unknown), RW 0x009A8423 (unknown), RW 0x009A845E (unknown), RW 0x009AF431 (unknown), RW 0x009BECCB (unknown), RW 0x009BEDE0 (unknown). Classes: AptMessageBox, AptOnlineShell.
Candidates: 0 none/partial functions; 21 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G298: RW 0x0066139C

Stops: S-4041, S-7338, S-7409. Kinds: code, project.
Functions: RW 0x004923F0 (ported), RW 0x0066139C (unknown), RW 0x00674FFE (ported), RW 0x00675095 (ported), RW 0x0067C5F3 (ported), RW 0x00713242 (unknown), RW 0x0071343E (unknown), RW 0x007139E6 (unknown), RW 0x00777DAE (unknown), RW 0x00777E54 (unknown), RW 0x00777F2B (unknown), RW 0x00777F9F (unknown), RW 0x00778036 (unknown), RW 0x00778056 (unknown), RW 0x00778176 (unknown), RW 0x0077824E (unknown), RW 0x00778413 (unknown), RW 0x008603F9 (ported), RW 0x00886EE6 (ported), RW 0x00886EF2 (ported), RW 0x00886F1C (ported), RW 0x00886FB1 (unknown), RW 0x00887013 (ported), RW 0x00887070 (partial), RW 0x00887082 (ported), RW 0x00887191 (ported), RW 0x008E047D (ported), RW 0x00AD2B30 (unknown), RW 0x00AD2CE0 (ported). Classes: ClearanceTestingSlowDeathBehavior, GeometryInfo.
Candidates: 1 none/partial functions; 15 unknown status.
Red buttons: none linked.
Unused fields: ClearanceTestingSlowDeathBehavior.ClearanceGeometryIsSmall.
Existing task matches: #754 Port 318: AerialPathfindNoFlyZone: RotWK's block parser (TriggerArea, Height) and the no-fly zone test of flyers (S-7338, S-4041).

### G299: RW 0x00940435

Stops: S-1990, S-1992, S-3460, S-7285, S-7292, S-7337, S-7444, S-923. Kinds: code, project.
Functions: RW 0x00674FFE (ported), RW 0x0071F933 (unknown), RW 0x0073EDD0 (ported), RW 0x00748418 (unknown), RW 0x0075CDFC (unknown), RW 0x0075CE47 (unknown), RW 0x0075CECD (unknown), RW 0x0075D082 (unknown), RW 0x0075D0B9 (ported), RW 0x0075D0F8 (unknown), RW 0x0075D137 (unknown), RW 0x0075D22E (unknown), RW 0x0075D366 (unknown), RW 0x0075D3E9 (unknown), RW 0x0075D516 (unknown), RW 0x00779A3D (unknown), RW 0x0077A91B (unknown), RW 0x0077B011 (unknown), RW 0x0077B6AB (unknown), RW 0x007C3C88 (unknown), RW 0x007C3D61 (unknown), RW 0x008227BB (unknown), RW 0x00822A98 (unknown), RW 0x00822BE3 (unknown), RW 0x00822DD7 (unknown), RW 0x00822E43 (unknown), RW 0x008232ED (unknown), RW 0x0082379A (unknown), RW 0x0083EE14 (unknown), RW 0x0085D7B6 (unknown), RW 0x00872B69 (ported), RW 0x008DDB2A (ported), RW 0x008DEDBB (ported), RW 0x00930E6E (unknown), RW 0x009312B9 (unknown), RW 0x00931698 (unknown), RW 0x0093178C (unknown), RW 0x00940435 (unknown), RW 0x009407AE (unknown), RW 0x009412EB (unknown), RW 0x00941365 (unknown), RW 0x00941A22 (unknown), RW 0x00941ABB (unknown), RW 0x00941B9F (unknown), RW 0x00942733 (unknown), RW 0x00944534 (unknown), RW 0x009D28D4 (unknown), RW 0x009D2B49 (unknown), RW 0x009D6F9C (unknown). Classes: AIAttackState, AptPalantir, AptSpellStore, AutoAbilityBehavior, BuildAssistant, CommandButton, CommandSet, ControlBar, HordeContainInterface, MappedImageCollection, PlayerTemplate.
Candidates: 0 none/partial functions; 43 unknown status.
Red buttons: Command_AngmarRangersIceArrowAbility, Command_AxtschwingerAxeThrow, Command_BlackRiderMorgulBladeForHorde, Command_IsenfurtWchterSmite, Command_RepairAI, Command_SpecialAbilityBalrogBreathAI, Command_SpecialAbilityBoromirHorn, Command_SpecialAbilityBoromirHornRing, Command_SpecialAbilityBoromirTapfererWiderstand, Command_SpecialAbilityDenethorSteuererlass, Command_SpecialAbilityGwahirScreechManve, Command_SpecialAbilityNaturmagierErntesegen.
Unused fields: AutoAbilityBehavior.WorkingRadius.
Existing task matches: #749 Port 313: Hold fire / auto-acquire buttons: the button image toggle and the related object's flag (S-1983, S-1992).

### G300: RW 0x00763D11

Stops: S-7160, S-7161. Kinds: code.
Functions: RW 0x00435F30 (ported), RW 0x00464A09 (ported), RW 0x004B612F (ported), RW 0x005487EC (ported), RW 0x0064DD3A (unknown), RW 0x0064DDC3 (unknown), RW 0x0068449B (ported), RW 0x0068C8DE (ported), RW 0x0068E5A3 (ported), RW 0x0068E67A (ported), RW 0x0076379C (unknown), RW 0x00763D11 (ported), RW 0x0076FB3F (unknown), RW 0x0077B51B (unknown), RW 0x0082D5DA (ported), RW 0x0082D925 (ported), RW 0x00899405 (partial), RW 0x008994BF (unknown), RW 0x00899562 (ported), RW 0x0089958F (ported), RW 0x00899A96 (unknown), RW 0x00899C7D (unknown), RW 0x00899E41 (ported), RW 0x00899E47 (ported), RW 0x00899E8C (unknown), RW 0x00899F1F (unknown), RW 0x00899F81 (ported), RW 0x00899F9D (ported), RW 0x0089A129 (ported), RW 0x008B0125 (ported), RW 0x008B66B4 (ported). Classes: ActionManager, AutoPickUpUpdate, BoredUpdate, GrabPassengerSpecialPower, ModuleData, WeaponFireSpecialAbilityUpdate.
Candidates: 1 none/partial functions; 10 unknown status.
Red buttons: Command_AngmarRangersIceArrowAbility, Command_AxtschwingerAxeThrow, Command_BlackRiderMorgulBladeForHorde, Command_FireDrakeBroodInferno, Command_GorkilBombThrow, Command_IsenfurtWchterSmite, Command_MenLongShotAbility, Command_NecromancerDeathBargain, Command_SmaugIncinerateNew, Command_SpecialAbilityBalrogBreathAI, Command_SpecialAbilityBuhrdurFressen, Command_SpecialAbilityDrarSpeerfeuer, Command_SpecialAbilityGulzarMOrgulSeucheAcolyte, Command_SpecialAbilityMordorBalrogWhipAI, Command_SpecialAbilitySaatdesgoldenenWaldes.
Unused fields: none linked.
Existing task matches: none.

### G301: RW 0x00696FD2

Stops: S-1190, S-5305. Kinds: code.
Functions: RW 0x0068C91B (ported), RW 0x0068CB85 (ported), RW 0x0068E5A3 (ported), RW 0x0069121A (ported), RW 0x00695338 (ported), RW 0x00696266 (ported), RW 0x00696FD2 (ported), RW 0x00697890 (unknown), RW 0x006979D9 (unknown), RW 0x006CA232 (ported), RW 0x006CD142 (ported), RW 0x0075CD39 (ported), RW 0x0075D22E (unknown), RW 0x007C11EE (unknown), RW 0x007C1254 (unknown), RW 0x007C12DC (unknown), RW 0x007C6243 (unknown), RW 0x007C62BF (unknown), RW 0x007C6363 (unknown), RW 0x00943D6F (unknown), RW 0x00943F81 (unknown). Classes: AISpecialPowerUpdate, AutoAbilityBehavior, DoCommandUpgrade, GateProxyBehavior, ProductionUpdate.
Candidates: 0 none/partial functions; 11 unknown status.
Red buttons: Command_AngmarRangersIceArrowAbility, Command_AxtschwingerAxeThrow, Command_BlackRiderMorgulBladeForHorde, Command_IsenfurtWchterSmite, Command_RepairAI, Command_SpecialAbilityBalrogBreathAI, Command_SpecialAbilityBoromirHorn, Command_SpecialAbilityBoromirHornRing, Command_SpecialAbilityBoromirTapfererWiderstand, Command_SpecialAbilityDenethorSteuererlass, Command_SpecialAbilityGwahirScreechManve, Command_SpecialAbilityNaturmagierErntesegen.
Unused fields: AutoAbilityBehavior.WorkingRadius.
Existing task matches: none.

### G302: RW 0x0042EEFA

Stops: S-7234, S-7521. Kinds: code.
Functions: RW 0x0042EEFA (unknown), RW 0x0065592C (unknown), RW 0x0066F748 (unknown), RW 0x006957B9 (unknown), RW 0x006AEE22 (ported), RW 0x0079DA0A (ported), RW 0x0079DB50 (unknown), RW 0x0079DB6B (unknown), RW 0x008C0518 (ported), RW 0x008C0FAB (unknown), RW 0x008C1025 (unknown), RW 0x008C136C (ported), RW 0x008C1419 (none), RW 0x008C148B (ported), RW 0x008C158B (ported), RW 0x008C17DD (unknown), RW 0x008C1CD5 (ported), RW 0x008C261D (partial), RW 0x00A39340 (ported). Classes: ActiveBody, CrateCollide, ExperienceTracker, GameLogicSettings, SalvageCrateCollide, UpgradeCenter, VeterancyCrateCollide.
Candidates: 2 none/partial functions; 9 unknown status.
Red buttons: none linked.
Unused fields: ActiveBody.EnteringDamagedTransitionTime, ActiveBody.ReallyDamagedAttributeModifier, SalvageCrateCollide.BannerChance, SalvageCrateCollide.LevelUpRadius, SalvageCrateCollide.PorterChance, SalvageCrateCollide.ResourceChance.
Existing task matches: #695 Port 303: SalvageCrateCollide вЂ” crates: its four unread chances and radius and 3 uncited functions; #567 Port 243: Body damage states: ActiveBody EnteringDamagedTransitionTime and the Damaged / ReallyDamaged attribute modifiers (ActiveBody, RespawnBody).

### G303: RW 0x00668AF8

Stops: S-4852, S-4857. Kinds: code.
Functions: RW 0x00662A0E (ported), RW 0x00668AF8 (ported), RW 0x0066D8F5 (ported), RW 0x0066DA5F (ported), RW 0x0067262C (ported), RW 0x0068229C (unknown), RW 0x006829B0 (unknown), RW 0x006E8454 (ported), RW 0x006EA42D (ported), RW 0x006EA48A (ported), RW 0x006EF591 (unknown), RW 0x006F1F0D (unknown), RW 0x006F5547 (unknown), RW 0x006F6286 (unknown), RW 0x006F7938 (unknown), RW 0x006F8B6E (unknown), RW 0x006F91E8 (unknown), RW 0x0076573F (ported), RW 0x00765E44 (unknown), RW 0x00937A97 (unknown). Classes: DynamicPortalBehaviour, HordeContain.
Candidates: 0 none/partial functions; 11 unknown status.
Red buttons: none linked.
Unused fields: HordeContain.LivingWorldOverloadTemplate, HordeContain.RanksToJustFreeWhenAttacking.
Existing task matches: #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate.

### G304: linked stops S-283, S-6010

Stops: S-283, S-6010. Kinds: code.
Functions: RW 0x0068DDF1 (ported), RW 0x0079F430 (unknown), RW 0x0079F475 (unknown), RW 0x0079F60E (unknown), RW 0x008836D3 (unknown), RW 0x0088373F (unknown), RW 0x008837E6 (unknown), RW 0x008838C3 (ported), RW 0x00883935 (unknown), RW 0x00883A29 (ported), RW 0x00883B4A (unknown), RW 0x00883D88 (unknown), RW 0x00883F79 (unknown), RW 0x008840FC (ported), RW 0x00884244 (ported), RW 0x008842AF (unknown). Classes: CitadelSlaughterHordeContain, InGameUI, ScoreKeeper, SlaughterHordeContain.
Candidates: 0 none/partial functions; 11 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G305: RW 0x006A2AE5

Stops: S-1770, S-306, S-5030. Kinds: code.
Functions: RW 0x00405406 (ported), RW 0x00405482 (ported), RW 0x00492AC8 (unknown), RW 0x004AD985 (unknown), RW 0x0050C485 (unknown), RW 0x0068B6F5 (ported), RW 0x0069CCEC (unknown), RW 0x006A2AE5 (unknown), RW 0x006A3755 (unknown), RW 0x006A8695 (ported), RW 0x00707141 (unknown), RW 0x0073228B (ported), RW 0x0073232B (ported), RW 0x0075B561 (unknown), RW 0x0075B8CC (ported), RW 0x0075C02D (unknown), RW 0x0075C1A9 (unknown), RW 0x00793E33 (unknown), RW 0x00794C63 (unknown), RW 0x00796810 (unknown), RW 0x0081F8D8 (unknown), RW 0x0083E486 (unknown), RW 0x008E951F (ported), RW 0x008E9744 (ported), RW 0x008E99EE (ported), RW 0x008E9B34 (ported), RW 0x008E9B9A (ported), RW 0x008E9C35 (unknown), RW 0x008EC1D6 (ported), RW 0x0094089B (unknown). Classes: BuildAssistant, CommandTranslator, Coord2D, InGameUI, MiscAudio, PlaceEventTranslator, TerrainResourceBehavior, TerrainResourceClientBehavior, TerrainResourceManager.
Candidates: 0 none/partial functions; 17 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: #757 Port 321: Wall line build from the HUD: the release's restricted-area test and the line ghost (S-1770).

### G306: RW 0x006732DC

Stops: S-7222, S-7324, S-7387. Kinds: code, project.
Functions: RW 0x0042EE15 (unknown), RW 0x005E31DF (unknown), RW 0x005E4326 (unknown), RW 0x005E4653 (none), RW 0x005E4F44 (unknown), RW 0x005E5785 (unknown), RW 0x005E8865 (ported), RW 0x005EA197 (unknown), RW 0x00670570 (unknown), RW 0x00670B11 (unknown), RW 0x00670E09 (unknown), RW 0x0067309D (ported), RW 0x006732DC (unknown), RW 0x00675996 (unknown), RW 0x00676346 (unknown), RW 0x00677026 (unknown), RW 0x00677CF9 (unknown), RW 0x0067B7AA (unknown), RW 0x0067BDC0 (unknown), RW 0x008BD372 (ported), RW 0x008CEF91 (ported). Classes: DrawableLocoInfo, PhysicsXformInfo.
Candidates: 1 none/partial functions; 16 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G307: RW 0x0046F48E

Stops: S-1042, S-4970, S-687, S-7396. Kinds: code.
Functions: RW 0x00462B9B (unknown), RW 0x0046DFBA (unknown), RW 0x0046E93D (unknown), RW 0x0046F48E (unknown), RW 0x004CF121 (unknown), RW 0x00530A10 (unknown), RW 0x0053F640 (unknown), RW 0x00540250 (unknown), RW 0x0054F406 (unknown), RW 0x005DD9EE (ported), RW 0x005DF95D (unknown), RW 0x005DFAA3 (unknown), RW 0x005DFC2E (unknown), RW 0x005DFE90 (unknown), RW 0x005DFFA8 (unknown), RW 0x005E0045 (unknown), RW 0x005E0108 (unknown), RW 0x005E0441 (unknown), RW 0x005E05AB (unknown), RW 0x0064849E (unknown), RW 0x00671841 (ported), RW 0x00673114 (unknown), RW 0x006760F9 (ported), RW 0x00678F54 (unknown), RW 0x0068CC53 (ported), RW 0x0075CDC4 (ported), RW 0x00776759 (partial), RW 0x00777310 (ported), RW 0x007B1E5C (unknown). Classes: FXPlayback, InvisibilityManager, LightEnvironmentClass, LightPulsePool, StealthUpdate, StealthUpdateModuleData, W3DInstancer, W3DLightDraw.
Candidates: 1 none/partial functions; 22 unknown status.
Red buttons: none linked.
Unused fields: StealthUpdate.HintDetectableConditions, W3DLightDraw.Radius.
Existing task matches: #750 Port 314: Ghost objects: a destroyed building stays in the fog for players who saw it (S-561, S-4970); #700 Port 308: Small module gaps with candidate readers вЂ” LargeGroupBonusUpdate.AlliesOnly, W3DLightDraw.Radius, EvaAnnounceClientCreate.CreateFakeRadarEvent and their classes' uncited functions.

### G308: RW 0x0042EC5E

Stops: S-4460, S-700, S-7193, S-7235. Kinds: code, project.
Functions: RW 0x0042EC5E (unknown), RW 0x00645487 (unknown), RW 0x006717E0 (unknown), RW 0x00671A3F (unknown), RW 0x006A0A5E (unknown), RW 0x006D1D80 (unknown), RW 0x006FF265 (ported), RW 0x00740069 (unknown), RW 0x00740074 (unknown), RW 0x0076E90A (ported), RW 0x0076F466 (unknown), RW 0x0076FFF0 (ported), RW 0x00771E7D (unknown), RW 0x0077348C (ported), RW 0x007747DE (ported), RW 0x00825B52 (unknown), RW 0x00825C45 (none), RW 0x00825DC2 (unknown), RW 0x008264D2 (unknown), RW 0x00826674 (unknown), RW 0x0083C29E (unknown), RW 0x0089F7F6 (unknown), RW 0x0089F890 (unknown), RW 0x0089F91F (ported), RW 0x0089F9A8 (ported), RW 0x008D26BF (ported), RW 0x008D278A (ported), RW 0x008D286F (ported), RW 0x008DEDBB (ported), RW 0x0094F7C6 (unknown), RW 0x0094F90B (unknown), RW 0x0094FA6C (unknown), RW 0x0094FB5F (unknown), RW 0x0094FC3F (unknown), RW 0x0094FCBA (ported). Classes: AIGroup, CrowdResponse, DrawableManager, FormationAssistant, InGameUI, RadiateFearUpdate, ScoreKeeper, ThingTemplate, UpgradeMux.
Candidates: 1 none/partial functions; 22 unknown status.
Red buttons: none linked.
Unused fields: RadiateFearUpdate.WhichSpecialPower.
Existing task matches: none.

### G309: RW 0x0068FA3D

Stops: S-1022, S-1040, S-1041, S-7175, S-7326. Kinds: code.
Functions: RW 0x0042EE5E (unknown), RW 0x0049D446 (unknown), RW 0x004B3D8D (ported), RW 0x005487EC (ported), RW 0x005FC922 (unknown), RW 0x005FC948 (unknown), RW 0x005FC966 (unknown), RW 0x00644FD0 (ported), RW 0x00649279 (unknown), RW 0x0064A492 (ported), RW 0x00660D71 (unknown), RW 0x006612AC (unknown), RW 0x0066844A (ported), RW 0x006760F9 (ported), RW 0x0068BBE0 (ported), RW 0x0068D7AB (ported), RW 0x0068F383 (ported), RW 0x0068FA3D (ported), RW 0x00690BD2 (ported), RW 0x00694C0D (ported), RW 0x00694CCC (ported), RW 0x006AB148 (unknown), RW 0x006C9147 (ported), RW 0x006DA34C (ported), RW 0x006EA857 (ported), RW 0x00744B25 (unknown), RW 0x0075232F (unknown), RW 0x0075255F (unknown), RW 0x00753295 (unknown), RW 0x00753456 (unknown), RW 0x007729A6 (unknown), RW 0x00776117 (ported), RW 0x00776294 (ported), RW 0x007767A9 (ported), RW 0x00776B50 (ported), RW 0x00776F03 (ported), RW 0x00777310 (ported), RW 0x007779E3 (ported), RW 0x007BC7EC (unknown), RW 0x0081A6A1 (unknown), RW 0x0081AA03 (ported), RW 0x0081ABDF (ported), RW 0x0081ACD7 (ported), RW 0x0081AF68 (ported), RW 0x0081B376 (ported), RW 0x0081B64F (ported), RW 0x0081B948 (ported), RW 0x0081BB8A (ported), RW 0x0081BCBD (ported), RW 0x0081BE85 (unknown), RW 0x0081C217 (ported), RW 0x0081C32C (ported), RW 0x0081CB08 (unknown), RW 0x0081CFFA (unknown), RW 0x00822A98 (unknown), RW 0x00822E43 (unknown), RW 0x00826080 (unknown), RW 0x0084A760 (unknown), RW 0x0087159F (none), RW 0x0089F9A8 (ported), RW 0x008A641A (ported), RW 0x008A6F84 (ported), RW 0x008A7099 (ported), RW 0x008B4E75 (ported), RW 0x008B4FEA (ported), RW 0x008B50BE (ported), RW 0x008B50F0 (unknown), RW 0x008B5427 (ported), RW 0x008B5492 (ported), RW 0x008B5498 (ported), RW 0x008B54DD (ported), RW 0x008B56CF (ported), RW 0x008B56EB (ported), RW 0x008B5738 (ported), RW 0x008B5FD0 (ported), RW 0x008E3C6E (ported), RW 0x00903497 (unknown), RW 0x0091432D (unknown), RW 0x00914814 (unknown), RW 0x009188EB (ported), RW 0x00A39090 (ported), RW 0x00A39340 (ported). Classes: AIAttackState, EmotionTrackerUpdate, InvisibilityManager, InvisibilityUpdate, ModelConditionSpecialAbilityUpdate, ModuleFactory, PlayerTemplate, RadiateFearUpdate, SpecialDisguiseUpdate, StealthDetectorUpdate, StealthUpdate, ToggleHiddenSpecialAbilityUpdate.
Candidates: 1 none/partial functions; 28 unknown status.
Red buttons: Command_SpecialAbilityBoromirHorn, Command_SpecialAbilityBoromirHornRing, Command_SpecialAbilityDenethorSehenderStein1, Command_SpecialAbilityElendilSummoned, Command_SpecialAbilityGeheimeWege, Command_SpecialAbilityHaradScoutCloak, Command_SpecialAbilityScreechDunklerMarschall.
Unused fields: RadiateFearUpdate.WhichSpecialPower, StealthUpdate.HintDetectableConditions.
Existing task matches: none.

### G310: RW 0x00683D89

Stops: S-111, S-530, S-930. Kinds: code.
Functions: RW 0x004622AC (unknown), RW 0x004CE3A8 (unknown), RW 0x004E70F5 (unknown), RW 0x004E8B8F (unknown), RW 0x004EA068 (unknown), RW 0x004EA107 (unknown), RW 0x004EC93A (unknown), RW 0x004EEB15 (unknown), RW 0x005E07F7 (unknown), RW 0x005EF8D8 (unknown), RW 0x005EF96D (unknown), RW 0x005F0126 (ported), RW 0x005F0328 (unknown), RW 0x005F0462 (unknown), RW 0x005F0D68 (unknown), RW 0x005F1A60 (unknown), RW 0x0067D400 (ported), RW 0x0067D519 (unknown), RW 0x0067D6E1 (ported), RW 0x0067D742 (ported), RW 0x0067E078 (ported), RW 0x0067F52B (ported), RW 0x006808ED (ported), RW 0x00683D89 (ported), RW 0x006849F8 (ported), RW 0x0068C6C8 (unknown), RW 0x00776294 (ported), RW 0x0081ABDF (ported), RW 0x008CC711 (ported), RW 0x008E3ABB (unknown). Classes: DefectionHelper, DevastateSpecialPower, FadeAndDieOrnamentUpdate, GlobalData, OCLSpecialPower, ObjectCreationList, StealthUpdate, TerrainTrees, TransitionDamageFX, TreeTopple, W3DTreeDraw.
Candidates: 0 none/partial functions; 18 unknown status.
Red buttons: Command_GaladrielMirrorDissent, Command_GaladrielMirrorWardByMirror, Command_GorkilZuchtspinnenSummon, Command_MenLongShotAbility, Command_OutpostOrcSpawn, Command_SpecialAbilityBeutekarrendesGroorks, Command_SpecialAbilityDenethorSteuererlass, Command_SpecialAbilityDrarSpeerfeuer, Command_SpecialAbilityLoneRangerSummonFinal, Command_SpecialAbilityMuhadSummonOliph, Command_ramridercaptain.
Unused fields: StealthUpdate.HintDetectableConditions, W3DTreeDraw.DarkeningFactor, W3DTreeDraw.FadeDistance, W3DTreeDraw.FadeTarget.
Existing task matches: #625 Port 266: Tree fading: the tree buffer's per-tree target opacity (W3DTreeDraw FadeDistance / FadeTarget, GameData AllowTreeFading / TreeFadeObjectFilter), split from Port 246 #570; #570 Port 246: Client fades: W3DTreeDraw FadeDistance / FadeTarget, W3DFloorDraw FloorFadeRateOnObjectDeath, SubObjectsUpgrade WaitBeforeFadeInSeconds.

### G311: RW 0x008705A6

Stops: S-1582, S-860. Kinds: code.
Functions: RW 0x0066137C (ported), RW 0x00662398 (unknown), RW 0x006C85CA (ported), RW 0x006C8A4E (ported), RW 0x006CA3DA (unknown), RW 0x006CB779 (ported), RW 0x006CDBF3 (ported), RW 0x00742D63 (unknown), RW 0x00851B6E (ported), RW 0x0085207A (unknown), RW 0x0085241F (ported), RW 0x00852465 (unknown), RW 0x008530BA (ported), RW 0x008536EA (ported), RW 0x00853EDF (ported), RW 0x00854DF7 (ported), RW 0x00865853 (unknown), RW 0x008669B1 (unknown), RW 0x0086FEEC (ported), RW 0x008705A6 (ported), RW 0x0087B308 (ported), RW 0x008BD372 (ported), RW 0x0090E855 (unknown), RW 0x00911086 (unknown), RW 0x0091117C (unknown), RW 0x009119CB (unknown), RW 0x00AD2620 (unknown). Classes: AIAttackState, AIWaitUntilFinishedFiringState, DamageContainedNugget, DamageNugget, GarrisonContain, HordeAttackNugget, SpecialAbilityUpdate, TurretAI.
Candidates: 0 none/partial functions; 12 unknown status.
Red buttons: Command_AngmarTrollChargeAttack, Command_SpecialAbilityLoneRangerSummonFinal, Command_SpecialAbilityNaturmagierErntesegen, Command_ThranduilKnigDsterwalds.
Unused fields: none linked.
Existing task matches: none.

### G312: RW 0x0073FB7A

Stops: S-072, S-1300, S-7319. Kinds: code, project.
Functions: RW 0x0042ECB2 (unknown), RW 0x0042EE15 (unknown), RW 0x0068F0EC (ported), RW 0x006915AF (unknown), RW 0x00691A2F (unknown), RW 0x006C644C (unknown), RW 0x006C650B (unknown), RW 0x006C6623 (unknown), RW 0x006C6975 (unknown), RW 0x006D1D80 (unknown), RW 0x0073FB7A (unknown), RW 0x007ED963 (unknown), RW 0x007EDB72 (unknown), RW 0x007EDC2F (unknown), RW 0x007EE057 (ported), RW 0x007EE166 (ported), RW 0x0086CA78 (none), RW 0x008BFBAE (ported), RW 0x008C1D75 (ported), RW 0x008ED505 (unknown), RW 0x008ED575 (unknown), RW 0x008F98E1 (unknown), RW 0x00936B7D (ported), RW 0x00AD2860 (ported), RW 0x00AD4310 (unknown). Classes: Coord3D, GeometryInfo, HordeContain, HordeContainInterface, InactiveBody, RawTokens, SquishCollide, ThingTemplate.
Candidates: 1 none/partial functions; 17 unknown status.
Red buttons: none linked.
Unused fields: HordeContain.LivingWorldOverloadTemplate, HordeContain.RanksToJustFreeWhenAttacking.
Existing task matches: #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate.

### G313: RW 0x00478289

Stops: S-094. Kinds: code.
Functions: RW 0x00478289 (unknown), RW 0x004C30BD (unknown), RW 0x004C3321 (unknown), RW 0x004C42A8 (unknown), RW 0x004C7E42 (unknown), RW 0x004C85E9 (unknown). Classes: FXEvent.
Candidates: 0 none/partial functions; 6 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G314: RW 0x0081F8D8

Stops: S-1202, S-1673, S-1675, S-1954, S-280, S-287, S-4964. Kinds: code.
Functions: RW 0x004474A0 (unknown), RW 0x00485B79 (unknown), RW 0x0048AB28 (unknown), RW 0x004B583C (ported), RW 0x00646771 (unknown), RW 0x0068DE58 (ported), RW 0x00691F7F (unknown), RW 0x00691FCE (unknown), RW 0x0069B5ED (unknown), RW 0x0069B6A8 (unknown), RW 0x0069B9C8 (unknown), RW 0x0069BF39 (unknown), RW 0x0069D90C (unknown), RW 0x006A4109 (unknown), RW 0x006A43FE (unknown), RW 0x006A67EA (unknown), RW 0x006A7232 (unknown), RW 0x006AAE79 (unknown), RW 0x006ABABD (ported), RW 0x006AC722 (unknown), RW 0x006AD722 (unknown), RW 0x0071077B (unknown), RW 0x0071083F (unknown), RW 0x0071C6AF (unknown), RW 0x0071CF3E (unknown), RW 0x0071D041 (unknown), RW 0x0075B068 (unknown), RW 0x007BC586 (unknown), RW 0x007BC5E9 (unknown), RW 0x008076CD (unknown), RW 0x0081DABC (unknown), RW 0x0081DB0C (unknown), RW 0x0081E4B5 (unknown), RW 0x0081F791 (unknown), RW 0x0081F8D8 (unknown), RW 0x0082076C (unknown), RW 0x00838EBA (unknown), RW 0x0083AB5D (unknown), RW 0x0083AC4A (unknown), RW 0x0083C29E (unknown), RW 0x0083D353 (unknown), RW 0x0083D7AD (unknown), RW 0x0083EE14 (unknown), RW 0x008DB103 (ported), RW 0x009403ED (unknown), RW 0x0096F890 (unknown). Classes: CommandTranslator, ControlBar, HotKeyTranslator, InGameUI, ScriptActions, SelectionTranslator, W3DModelDraw, WeaponSet.
Candidates: 0 none/partial functions; 42 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G315: RW 0x00680B70

Stops: S-1280, S-301. Kinds: code.
Functions: RW 0x00405ED1 (unknown), RW 0x00661A02 (unknown), RW 0x00661C09 (unknown), RW 0x00680B70 (unknown), RW 0x006851F1 (unknown), RW 0x0068592D (ported), RW 0x00794A9C (unknown), RW 0x00794C34 (unknown), RW 0x00796810 (unknown), RW 0x00796912 (unknown), RW 0x007969C9 (unknown), RW 0x00796C5A (unknown), RW 0x00796D37 (unknown), RW 0x00AD2CE0 (ported). Classes: BuildAssistant, GeometryInfo, GeometryUpgrade, HordeGarrisonContain, QueueProductionExitUpdate, TerrainLogic.
Candidates: 0 none/partial functions; 12 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G316: RW 0x00674727

Stops: S-1463, S-1464. Kinds: code.
Functions: RW 0x0049F474 (ported), RW 0x004BFAF4 (ported), RW 0x004C0CBC (ported), RW 0x006717C4 (partial), RW 0x006746FD (partial), RW 0x00674727 (unknown), RW 0x00678721 (partial), RW 0x00678816 (partial), RW 0x0067896F (ported), RW 0x00690728 (unknown), RW 0x00690772 (ported), RW 0x00692313 (unknown), RW 0x006923B7 (ported), RW 0x0083F159 (ported), RW 0x0083F1BC (ported), RW 0x0083F21F (ported), RW 0x0083F279 (unknown), RW 0x0083F321 (unknown), RW 0x008AF005 (ported), RW 0x008AF09D (ported), RW 0x008CE235 (partial), RW 0x008CE75B (unknown), RW 0x008CEBCB (unknown). Classes: AnimationSoundClientBehavior, LargeGroupAudioUpdate.
Candidates: 5 none/partial functions; 7 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G317: RW 0x005E2F24

Stops: S-1500. Kinds: code.
Functions: RW 0x005E2F24 (unknown), RW 0x006E82B3 (ported), RW 0x006E8E19 (ported), RW 0x006E95B2 (unknown), RW 0x006EA5B2 (unknown), RW 0x006F0889 (unknown), RW 0x0076550F (ported), RW 0x0086E214 (unknown), RW 0x00871897 (unknown). Classes: none.
Candidates: 0 none/partial functions; 6 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G318: RW 0x007052BE

Stops: S-1915. Kinds: code.
Functions: RW 0x007052BE (unknown), RW 0x008404F2 (unknown), RW 0x00845830 (unknown), RW 0x00846A2C (unknown), RW 0x00975782 (unknown), RW 0x00975C3A (unknown). Classes: none.
Candidates: 0 none/partial functions; 6 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G319: RW 0x004A225B

Stops: S-1916. Kinds: code.
Functions: RW 0x004A225B (unknown), RW 0x00724495 (unknown), RW 0x00725182 (unknown), RW 0x007252B6 (unknown), RW 0x007258E1 (unknown), RW 0x00727081 (unknown). Classes: GameWindowManager, ListboxData.
Candidates: 0 none/partial functions; 6 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G320: RW 0x0044FDF3

Stops: S-2453. Kinds: code.
Functions: RW 0x0044FDF3 (unknown), RW 0x0069B0F8 (unknown), RW 0x006AAAAF (none), RW 0x006D8E0F (unknown), RW 0x006D8E2B (unknown), RW 0x007BCEE2 (unknown). Classes: none.
Candidates: 1 none/partial functions; 5 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: #748 Port 312: Radar: the hidden flag and its script actions, the stealth blink's two tests, the attack notice's control flow (S-2453, S-2454, S-2457).

### G321: RW 0x0073D0BA

Stops: S-2521. Kinds: code.
Functions: RW 0x0073D0BA (ported), RW 0x0092BA91 (unknown), RW 0x0092BF34 (unknown), RW 0x0092BFED (ported), RW 0x0092C428 (unknown), RW 0x0092C734 (ported), RW 0x0092C809 (ported), RW 0x0092C8C4 (unknown), RW 0x0092CD78 (ported), RW 0x0092CF64 (unknown), RW 0x0092D9F4 (unknown). Classes: CreateAHeroHero, DrawableManager, InGameHeroSelect, InGameHeroSelectInterface, InGameHudNode, InGameUI.
Candidates: 0 none/partial functions; 6 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G322: RW 0x006A3755

Stops: S-3302, S-7361. Kinds: code.
Functions: RW 0x0048EAD8 (unknown), RW 0x0049249D (unknown), RW 0x00670AF4 (unknown), RW 0x006718FB (ported), RW 0x00674504 (ported), RW 0x0069C5E6 (unknown), RW 0x0069CCEC (unknown), RW 0x006A2AE5 (unknown), RW 0x006A3457 (unknown), RW 0x006A3755 (unknown), RW 0x006A3ABE (unknown), RW 0x00707141 (unknown), RW 0x00796C5A (unknown), RW 0x00796D1F (unknown), RW 0x00797A96 (ported), RW 0x00798F70 (ported). Classes: BuildAssistant, GeometryInfo, PlaceEventTranslator, PlacementIcons, PortSnapView, W3DInstancer, W3DShaderKey.
Candidates: 0 none/partial functions; 12 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G323: linked stops S-4120, S-5010

Stops: S-4120, S-5010. Kinds: code.
Functions: RW 0x006DD65C (unknown), RW 0x006DDF2F (unknown), RW 0x006DE498 (unknown), RW 0x006DF4D1 (unknown), RW 0x006DF73E (unknown), RW 0x00A1F5B9 (unknown), RW 0x00A1FB84 (unknown), RW 0x00A203D6 (unknown), RW 0x00A206EF (unknown), RW 0x00A20DD0 (unknown), RW 0x00A2111A (unknown), RW 0x00A211DF (unknown). Classes: GameState, LiveGame, TeamFactory, XferCRC, XferLoad, XferSave.
Candidates: 0 none/partial functions; 12 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G324: RW 0x008F069A

Stops: S-4221. Kinds: code.
Functions: RW 0x006C7779 (ported), RW 0x006C778A (ported), RW 0x006C7946 (unknown), RW 0x008F069A (ported), RW 0x008F0C47 (ported), RW 0x009A051D (ported), RW 0x009A0705 (unknown), RW 0x009A0838 (ported), RW 0x00A39260 (unknown), RW 0x00A3B040 (unknown), RW 0x00B51A50 (unknown), RW 0x00B52D50 (unknown). Classes: none.
Candidates: 0 none/partial functions; 6 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G325: RW 0x0062550D

Stops: S-4811. Kinds: code.
Functions: RW 0x0062550D (unknown), RW 0x00625524 (unknown), RW 0x0062B385 (unknown), RW 0x0065DA20 (unknown), RW 0x008D335D (unknown), RW 0x008D8B51 (unknown). Classes: DisconnectManager.
Candidates: 0 none/partial functions; 6 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G326: RW 0x00664D7E

Stops: S-7162. Kinds: code.
Functions: RW 0x00664D7E (ported), RW 0x0066E8FF (unknown), RW 0x006FB754 (unknown), RW 0x007468D5 (ported), RW 0x00749D46 (unknown), RW 0x00749F93 (unknown), RW 0x007553AB (unknown), RW 0x00755798 (unknown). Classes: AIAttackApproachState, CombatSettings.
Candidates: 0 none/partial functions; 6 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G327: RW 0x0044DE58

Stops: S-7448. Kinds: code.
Functions: RW 0x0044DE58 (unknown), RW 0x0044EDAE (unknown), RW 0x0044F0FA (unknown), RW 0x0065287A (unknown), RW 0x00676711 (ported), RW 0x0068EBE9 (ported), RW 0x008CDFC9 (unknown), RW 0x008CE0C4 (unknown), RW 0x008CE116 (ported), RW 0x009F9E7B (ported). Classes: DrawableModule, RadarMarker, RadarMarkerClientUpdate.
Candidates: 0 none/partial functions; 6 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G328: RW 0x00504019

Stops: S-762. Kinds: code.
Functions: RW 0x00504019 (unknown), RW 0x0050414D (unknown), RW 0x00504230 (unknown), RW 0x005044B4 (unknown), RW 0x00518000 (unknown), RW 0x00576240 (unknown). Classes: AptPalantir.
Candidates: 0 none/partial functions; 6 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G329: RW 0x0068B3BD

Stops: S-783. Kinds: code.
Functions: RW 0x006668CA (ported), RW 0x0068B3BD (ported), RW 0x00693A1A (ported), RW 0x006EAF79 (ported), RW 0x006ED0A0 (unknown), RW 0x006F1B3E (unknown), RW 0x006F1C90 (unknown), RW 0x0098F371 (unknown), RW 0x009902A1 (unknown), RW 0x00990A8E (unknown). Classes: HordeAIUpdate.
Candidates: 0 none/partial functions; 6 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: #650 Port 283: HorseHordeContain and HordeAIUpdate: the cavalry horde's uncited own functions (classify, then port the missing ones).

### G330: RW 0x0062E4E8

Stops: S-1062, S-143, S-223, S-562, S-581, S-630, S-7216, S-780, S-922. Kinds: code, project.
Functions: RW 0x0042ED00 (unknown), RW 0x006159B1 (unknown), RW 0x00615A68 (unknown), RW 0x00625F7C (unknown), RW 0x0062E4E8 (unknown), RW 0x0062EE54 (unknown), RW 0x006311ED (unknown), RW 0x0063AD4F (unknown), RW 0x00670216 (ported), RW 0x00689E3C (ported), RW 0x0068A928 (unknown), RW 0x0068BA2A (unknown), RW 0x0068C18F (ported), RW 0x0068C7B2 (unknown), RW 0x0068D8F7 (ported), RW 0x0068E31F (ported), RW 0x00693D0C (ported), RW 0x00695475 (ported), RW 0x006955BC (ported), RW 0x00696266 (ported), RW 0x00698F06 (ported), RW 0x006ABE7C (unknown), RW 0x006ABF5E (unknown), RW 0x006AF598 (unknown), RW 0x006B5DE0 (ported), RW 0x006C7518 (unknown), RW 0x006DF73E (unknown), RW 0x006DF940 (unknown), RW 0x006FAA6D (unknown), RW 0x00719C69 (ported), RW 0x00719DEE (unknown), RW 0x00719EBE (unknown), RW 0x00719FDB (unknown), RW 0x0071A024 (unknown), RW 0x0071A04F (unknown), RW 0x0071A09E (unknown), RW 0x0071A13D (unknown), RW 0x007640C1 (ported), RW 0x0076ED16 (unknown), RW 0x0076F115 (ported), RW 0x0077348C (ported), RW 0x007747DE (ported), RW 0x00774897 (ported), RW 0x00774E2F (ported), RW 0x0077CA47 (unknown), RW 0x0079D3DA (unknown), RW 0x0079DA9D (unknown), RW 0x0079DB50 (unknown), RW 0x0079DB6B (unknown), RW 0x00800C8F (unknown), RW 0x00806909 (unknown), RW 0x00806926 (unknown), RW 0x00806CC5 (unknown), RW 0x00806D18 (none), RW 0x00806EA2 (unknown), RW 0x00806F5F (unknown), RW 0x00808D3C (unknown), RW 0x00808F53 (unknown), RW 0x00809253 (unknown), RW 0x00821567 (unknown), RW 0x008659D8 (unknown), RW 0x00867C40 (unknown), RW 0x00867E1B (unknown), RW 0x00873A02 (ported), RW 0x00897987 (ported), RW 0x0094FCBA (ported), RW 0x00973C81 (unknown), RW 0x00A39020 (unknown), RW 0x00A3B4E0 (unknown), RW 0x00AD2CE0 (ported), RW 0x00B4D940 (unknown), RW 0x00B4E890 (ported), RW 0x00B6C420 (unknown), RW 0x00B6C470 (ported), RW 0x00B6C480 (ported), RW 0x00B6CF90 (unknown), RW 0x00B6D060 (unknown), RW 0x00B6D5E0 (unknown), RW 0x00B6D730 (unknown), RW 0x00B6D8A0 (unknown), RW 0x00B6E6B0 (unknown). Classes: AICommands, AIGroup, AISkirmishPlayer, AIWorld, BezierProjectileBehavior, CellGrid, CombatState, CommandList, ExperienceTracker, ExperienceWorld, FactionVictoryData, GameInfo, GameLogicDispatch, GateOpenAndCloseBehavior, GeometryInfo, HordeContain, MoveToGroupOrder, OpenContain, ScoreKeeper, VictoryConditions, VictorySystem, WeatherClient.
Candidates: 1 none/partial functions; 55 unknown status.
Red buttons: none linked.
Unused fields: HordeContain.LivingWorldOverloadTemplate, HordeContain.RanksToJustFreeWhenAttacking.
Existing task matches: #756 Port 320: Victory: computer players die when no human is left, the shared AI branch, the defeat frames and the defeated player's map reveal (S-1062); #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate.

### G331: RW 0x0073193D

Stops: S-1060, S-1181, S-271. Kinds: code.
Functions: RW 0x0061632F (unknown), RW 0x006ADD80 (unknown), RW 0x006B07EF (unknown), RW 0x0072D943 (unknown), RW 0x0072FE23 (unknown), RW 0x007313B3 (unknown), RW 0x0073161C (unknown), RW 0x0073193D (unknown), RW 0x00779173 (unknown), RW 0x007B937D (unknown), RW 0x007BC586 (unknown), RW 0x007BF05C (unknown), RW 0x007BF15E (unknown), RW 0x007E526C (unknown), RW 0x00808A20 (unknown), RW 0x00808CCD (unknown), RW 0x00808E1F (unknown), RW 0x00808F53 (unknown), RW 0x00809253 (unknown). Classes: EconomyContext, LiveGame, PlayerAIType, ScriptEngine, SidesList, VictoryConditions.
Candidates: 0 none/partial functions; 19 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G332: RW 0x0081C5D3

Stops: S-273, S-3506. Kinds: code.
Functions: RW 0x0062648E (unknown), RW 0x0073BDA3 (unknown), RW 0x0081C5D3 (unknown), RW 0x0081CB08 (unknown), RW 0x00821A14 (unknown), RW 0x008D3702 (unknown), RW 0x0093C1F9 (unknown), RW 0x0093E6F2 (unknown), RW 0x0093E72F (unknown), RW 0x009604E7 (unknown), RW 0x0096054C (unknown), RW 0x00960621 (unknown), RW 0x009CE8F3 (unknown). Classes: AptLoadScreen, AptMapPreview, SkirmishPreferences.
Candidates: 0 none/partial functions; 13 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G333: RW 0x0083B471

Stops: S-454, S-4982. Kinds: code.
Functions: RW 0x00486F76 (unknown), RW 0x0048713E (unknown), RW 0x0048A72D (unknown), RW 0x0063F0E2 (unknown), RW 0x0069B507 (unknown), RW 0x006A7232 (unknown), RW 0x00838EBA (unknown), RW 0x008392A7 (unknown), RW 0x0083AC4A (unknown), RW 0x0083B471 (unknown), RW 0x0083B851 (unknown), RW 0x0083B94F (unknown), RW 0x0091ED91 (unknown). Classes: AptInput, AptMenuPlayer, HudInput, InGameHud, InGameUI, WindowManager.
Candidates: 0 none/partial functions; 13 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G334: RW 0x007486FD

Stops: S-7331, S-7365. Kinds: code, project.
Functions: RW 0x0069519A (ported), RW 0x006CA32D (unknown), RW 0x00741745 (unknown), RW 0x00744401 (unknown), RW 0x00744AAA (unknown), RW 0x007457DF (unknown), RW 0x007458EC (unknown), RW 0x00745901 (unknown), RW 0x007486FD (unknown), RW 0x00749A3E (unknown), RW 0x0074AD8E (unknown), RW 0x0074AF91 (unknown), RW 0x0074CBBD (unknown), RW 0x008DB860 (unknown), RW 0x009188EB (ported). Classes: AIAttackState, WeaponTemplate.
Candidates: 0 none/partial functions; 13 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G335: RW 0x00697F28

Stops: S-4190, S-4220, S-7189. Kinds: code.
Functions: RW 0x005E68A4 (unknown), RW 0x005E7F12 (unknown), RW 0x00608BCA (ported), RW 0x0064F96A (unknown), RW 0x00667416 (ported), RW 0x0069609F (ported), RW 0x00697B41 (ported), RW 0x00697F28 (unknown), RW 0x006CEA67 (unknown), RW 0x007082E2 (ported), RW 0x00755174 (unknown), RW 0x007A5B03 (ported), RW 0x007A6A20 (ported), RW 0x007C68AA (unknown), RW 0x007E5970 (unknown), RW 0x007E8A25 (unknown), RW 0x007EDDB6 (ported), RW 0x007EDECE (ported), RW 0x007EDF70 (ported), RW 0x007EE166 (ported), RW 0x007EE260 (ported), RW 0x00850C60 (ported), RW 0x00868FEF (ported), RW 0x0088927E (ported), RW 0x0088B863 (ported), RW 0x0088C966 (ported), RW 0x008C3388 (ported), RW 0x008CD4AA (ported), RW 0x008D27E8 (ported), RW 0x008D284A (ported), RW 0x008DBAEC (unknown), RW 0x008DC2C3 (unknown), RW 0x008F03FA (unknown), RW 0x008F3F6C (unknown), RW 0x008F5756 (unknown), RW 0x008FA569 (unknown), RW 0x0093AC0F (unknown), RW 0x00A39260 (unknown), RW 0x00A3B040 (unknown), RW 0x00B51A50 (unknown), RW 0x00B52D50 (unknown). Classes: AIBuilder, AISkirmishPlayer, AIStateMachine, AIUpdateInterface, AIWorld, ActiveBody, BehaviorModule, DamageFilteredCreateObjectDie, DeletionUpdate, DozerAIUpdate, LocomotorSet, ObjectModule, OpenContain, PathfindZoneManager, ScriptEngine, SkirmishAI, StateMachine, ThreatFinderUpdate, TunnelTracker, TurretAI, UnportedUpdateModule, UpdateModule.
Candidates: 0 none/partial functions; 20 unknown status.
Red buttons: none linked.
Unused fields: ActiveBody.EnteringDamagedTransitionTime, ActiveBody.ReallyDamagedAttributeModifier.
Existing task matches: #787 Local 109: builders and walls, second pass: WorkerAIUpdate, DozerAIUpdate, WallHubBehavior, ManTheWallsSpecialPower вЂ” 34 functions; #567 Port 243: Body damage states: ActiveBody EnteringDamagedTransitionTime and the Damaged / ReallyDamaged attribute modifiers (ActiveBody, RespawnBody).

### G336: RW 0x00779A3D

Stops: S-1364, S-1875, S-281. Kinds: code.
Functions: RW 0x005DA83D (unknown), RW 0x0063874B (unknown), RW 0x0063AD4F (unknown), RW 0x0063F024 (unknown), RW 0x006A950B (ported), RW 0x006AAC78 (unknown), RW 0x006AD555 (unknown), RW 0x006AD5CA (unknown), RW 0x006AD677 (unknown), RW 0x006AD722 (unknown), RW 0x006AE36F (unknown), RW 0x006E5E16 (unknown), RW 0x007572FA (unknown), RW 0x007575F4 (unknown), RW 0x00779A3D (unknown), RW 0x007BD210 (unknown), RW 0x008F7B3B (unknown), RW 0x008F833B (unknown), RW 0x008F9705 (unknown), RW 0x00927EC6 (unknown), RW 0x0099FB84 (unknown). Classes: AiOrdersManager, MetaMap, OptionPreferences.
Candidates: 0 none/partial functions; 20 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G337: RW 0x0071EBDA

Stops: S-3801, S-5040, S-7388. Kinds: code.
Functions: RW 0x0068B678 (ported), RW 0x0068B749 (ported), RW 0x0068C3E6 (ported), RW 0x0068C82D (ported), RW 0x006AA72B (unknown), RW 0x006AAC52 (ported), RW 0x006AAF3B (unknown), RW 0x006D4671 (unknown), RW 0x0071EBDA (unknown), RW 0x0071ED96 (unknown), RW 0x0071EF95 (unknown), RW 0x00793EEB (unknown), RW 0x00856434 (ported), RW 0x00856992 (ported), RW 0x0088D272 (ported), RW 0x0092FA83 (unknown), RW 0x0092FAA5 (unknown), RW 0x0092FE33 (unknown), RW 0x009305CE (unknown), RW 0x0094397F (unknown), RW 0x00943D6F (unknown), RW 0x009441FE (unknown), RW 0x00944534 (unknown), RW 0x0094475A (unknown), RW 0x00944911 (unknown), RW 0x00944EFB (unknown), RW 0x00945938 (unknown), RW 0x009D2B49 (unknown). Classes: AptPalantir, ControlBar, GUICommandTranslator, GettingBuiltBehavior, InGameHud, SelectionTranslator.
Candidates: 0 none/partial functions; 20 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: #687 Port 295: GettingBuiltBehavior вЂ” the uncited own functions of a building under construction (19).

### G338: RW 0x00A149A2

Stops: S-5160, S-5300, S-5301. Kinds: code.
Functions: RW 0x0042C33B (unknown), RW 0x00490D08 (unknown), RW 0x006417DC (unknown), RW 0x006427C8 (unknown), RW 0x0070254F (unknown), RW 0x00704BDE (unknown), RW 0x00706A71 (unknown), RW 0x00706F1E (unknown), RW 0x007B9EBE (unknown), RW 0x007BAA44 (unknown), RW 0x007BADB9 (unknown), RW 0x007BAE93 (unknown), RW 0x00A14275 (unknown), RW 0x00A14313 (unknown), RW 0x00A14419 (unknown), RW 0x00A149A2 (unknown), RW 0x00A14AEB (unknown), RW 0x00A14D2B (ported), RW 0x00A1766D (unknown), RW 0x00A17FB0 (unknown), RW 0x00A183AA (unknown). Classes: ArchiveFileSystem, AsciiString, GameState, GameWorld, GlobalData, MapObjectBuilder, MapTerrainBuilder, PathfindView.
Candidates: 0 none/partial functions; 20 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G339: RW 0x0062F91A

Stops: S-272, S-3902, S-4932, S-5261, S-610. Kinds: code.
Functions: RW 0x00441C56 (ported), RW 0x004449C4 (unknown), RW 0x0044E5F3 (unknown), RW 0x004500B9 (unknown), RW 0x0047321C (unknown), RW 0x004735AB (unknown), RW 0x00497F60 (unknown), RW 0x005F15CE (unknown), RW 0x006282F0 (unknown), RW 0x00629DD9 (unknown), RW 0x0062AC17 (unknown), RW 0x0062B074 (unknown), RW 0x0062C8D9 (unknown), RW 0x0062CE75 (unknown), RW 0x0062DCE4 (unknown), RW 0x0062F91A (unknown), RW 0x0067AFC3 (unknown), RW 0x0067F63F (unknown), RW 0x00682D4C (unknown), RW 0x00683393 (ported), RW 0x00683955 (ported), RW 0x00686459 (unknown), RW 0x0068B244 (ported), RW 0x0068BCBD (unknown), RW 0x006923B7 (ported), RW 0x0069472D (unknown), RW 0x006987DE (unknown), RW 0x006A87F5 (unknown), RW 0x006A8D2B (unknown), RW 0x006AA72B (unknown), RW 0x00797949 (unknown), RW 0x0079BE6A (ported), RW 0x008003F6 (unknown), RW 0x0089EE67 (ported), RW 0x0089F114 (unknown), RW 0x008C7DBA (ported), RW 0x008C8E67 (ported), RW 0x00911C46 (unknown), RW 0x00AD4B80 (unknown), RW 0x00AD4C10 (ported), RW 0x00AD4FA0 (unknown), RW 0x00AD4FD0 (unknown), RW 0x00AD52B0 (unknown), RW 0x00AD5740 (unknown), RW 0x00B4F440 (unknown). Classes: AIWorld, AttributeModifierAuraUpdate, CastleBehavior, LiveGame, PlayerList, TaintManager, TerrainLogic, TerrainTrees, VictoryConditions.
Candidates: 0 none/partial functions; 35 unknown status.
Red buttons: none linked.
Unused fields: CastleBehavior.CrewPrepareFX, CastleBehavior.CrewPrepareInterval, CastleBehavior.CrewReleaseFX.
Existing task matches: #688 Port 296: Castles вЂ” CastleBehavior, CastleMemberBehavior and FoundationAIUpdate: the uncited own functions (27) and their five unread fields.

### G340: RW 0x0068C7E9

Stops: S-1000, S-587, S-852. Kinds: code.
Functions: RW 0x00449681 (ported), RW 0x004B1B5A (ported), RW 0x00625E0A (ported), RW 0x00671913 (unknown), RW 0x0067449C (ported), RW 0x0068B645 (ported), RW 0x0068B934 (unknown), RW 0x0068BB14 (ported), RW 0x0068C7E9 (unknown), RW 0x0068C933 (ported), RW 0x0068D19A (unknown), RW 0x0068E197 (unknown), RW 0x00690053 (unknown), RW 0x0069036C (unknown), RW 0x006B16EC (unknown), RW 0x0070E013 (ported), RW 0x0073CE2A (ported), RW 0x0074C4A7 (unknown), RW 0x00780EF0 (unknown), RW 0x0079DC9E (ported), RW 0x00804F02 (unknown), RW 0x00809FFB (unknown), RW 0x0080FD9C (ported), RW 0x008311B1 (unknown), RW 0x00855830 (ported), RW 0x0085EBEE (ported), RW 0x0086C81E (ported), RW 0x0086EBF4 (ported), RW 0x0086EE52 (ported), RW 0x00870204 (ported), RW 0x008718ED (ported), RW 0x00871915 (unknown), RW 0x008719E4 (ported), RW 0x008720C2 (ported), RW 0x00872A8D (none), RW 0x0087370B (ported), RW 0x0087393E (ported), RW 0x00873A02 (ported), RW 0x00873AE3 (ported), RW 0x008759FF (ported), RW 0x00876406 (unknown), RW 0x0089A258 (unknown), RW 0x0089A308 (unknown), RW 0x0089A392 (ported), RW 0x0089A5C5 (unknown), RW 0x0089A90F (unknown), RW 0x0089AB8F (ported), RW 0x0089ACE4 (ported), RW 0x008B4F39 (ported), RW 0x008B50A5 (ported), RW 0x008E3C58 (unknown). Classes: AllowBannerSpawnUpgrade, ArmySummarySystem, BannerCarrierUpdate, EmotionTrackerUpdate, ExperienceTracker, HordeContain, WeaponStatusHelper.
Candidates: 1 none/partial functions; 20 unknown status.
Red buttons: none linked.
Unused fields: HordeContain.LivingWorldOverloadTemplate, HordeContain.RanksToJustFreeWhenAttacking.
Existing task matches: #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate.

### G341: RW 0x006E8707

Stops: S-1104, S-1620. Kinds: code.
Functions: RW 0x00665777 (unknown), RW 0x0066E12A (ported), RW 0x0068BB9D (ported), RW 0x0069029B (ported), RW 0x006E85E9 (ported), RW 0x006E8707 (ported), RW 0x006EFBB8 (ported), RW 0x006F3C87 (ported), RW 0x006FE456 (unknown), RW 0x00852E2A (ported), RW 0x00865679 (unknown), RW 0x00865694 (unknown), RW 0x008657B6 (unknown), RW 0x008657D1 (unknown), RW 0x00866B69 (unknown), RW 0x00867120 (unknown), RW 0x00867242 (unknown), RW 0x00867ACD (unknown), RW 0x00867AD4 (unknown), RW 0x008682D9 (ported), RW 0x00868A70 (unknown), RW 0x0086A002 (ported), RW 0x0086A104 (ported), RW 0x0086A1FA (ported), RW 0x0086A530 (partial), RW 0x0086A731 (ported), RW 0x0086A943 (ported), RW 0x0086ABDC (ported), RW 0x0086B169 (ported), RW 0x0086B86D (ported), RW 0x0086CF2A (ported), RW 0x0086D061 (unknown), RW 0x0086EBF4 (ported), RW 0x0086F053 (ported), RW 0x0086F18F (ported), RW 0x00872D0A (ported), RW 0x0087370B (ported), RW 0x00873D48 (ported), RW 0x00873F30 (ported), RW 0x008759FF (ported), RW 0x00877E12 (ported), RW 0x0087A48C (ported), RW 0x0087A6D3 (ported), RW 0x0087A89E (ported), RW 0x0087AD94 (ported), RW 0x0087AE0A (ported), RW 0x0087CA2B (ported), RW 0x0087D3CA (ported), RW 0x008C3FA3 (ported). Classes: AIUpdateInterface, GarrisonContain, HordeContain, HordeContainCore, HordeContainInterface, HordeTransportContain, OpenContain, SlaughterHordeContain, TransportContain.
Candidates: 1 none/partial functions; 13 unknown status.
Red buttons: none linked.
Unused fields: HordeContain.LivingWorldOverloadTemplate, HordeContain.RanksToJustFreeWhenAttacking.
Existing task matches: #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate.

### G342: RW 0x0068C053

Stops: S-163, S-4851. Kinds: code.
Functions: RW 0x0068229C (unknown), RW 0x0068B3CF (unknown), RW 0x0068B425 (ported), RW 0x0068C02E (ported), RW 0x0068C053 (ported), RW 0x0068C05A (unknown), RW 0x006A950B (ported), RW 0x006AAC66 (ported), RW 0x006ECF1D (unknown), RW 0x006EF3C3 (unknown), RW 0x006F3082 (unknown), RW 0x006F70D5 (unknown), RW 0x006FCE1A (unknown), RW 0x006FD06F (unknown), RW 0x006FD716 (unknown), RW 0x006FDB3D (unknown), RW 0x006FDC15 (unknown), RW 0x006FDDE5 (unknown), RW 0x00767B0E (unknown), RW 0x00871536 (ported). Classes: AIGroup, ActionManager, FakePathfindPortalBehaviour, HordeContain, PlayerTemplate, SkirmishAI, SkirmishAIManager.
Candidates: 0 none/partial functions; 14 unknown status.
Red buttons: none linked.
Unused fields: HordeContain.LivingWorldOverloadTemplate, HordeContain.RanksToJustFreeWhenAttacking.
Existing task matches: #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate.

### G343: RW 0x00816655

Stops: S-1126, S-4003, S-4980. Kinds: code.
Functions: RW 0x006221E0 (ported), RW 0x00622463 (ported), RW 0x006DD795 (unknown), RW 0x006DE38D (unknown), RW 0x006E04AD (unknown), RW 0x0075E3B7 (unknown), RW 0x00812913 (unknown), RW 0x00812A37 (unknown), RW 0x008162E5 (unknown), RW 0x008165A1 (unknown), RW 0x00816655 (unknown), RW 0x00816927 (unknown), RW 0x00816DBF (unknown), RW 0x00816F24 (unknown), RW 0x00816FA3 (unknown), RW 0x00817B5F (unknown), RW 0x008180A8 (unknown), RW 0x0081874E (unknown), RW 0x00818D41 (unknown), RW 0x00819548 (unknown), RW 0x0091C2FC (unknown), RW 0x0091ED91 (unknown), RW 0x00921B0F (unknown). Classes: AptPlayer, AptQuitMenu, AptSaveLoad, AptScreen, ShellAction, WindowManager.
Candidates: 0 none/partial functions; 21 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G344: RW 0x00454186

Stops: S-1187. Kinds: code.
Functions: RW 0x00454186 (unknown), RW 0x004541FF (unknown), RW 0x006DA80E (unknown), RW 0x006DB821 (unknown), RW 0x006DBA01 (unknown), RW 0x00759B84 (unknown), RW 0x007E4DF0 (unknown). Classes: LiveGameScriptHost, ScriptEngine.
Candidates: 0 none/partial functions; 7 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G345: RW 0x00696266

Stops: S-1301. Kinds: code.
Functions: RW 0x0068B411 (ported), RW 0x00691269 (ported), RW 0x00696266 (ported), RW 0x0076FCBF (unknown), RW 0x0076FD04 (unknown), RW 0x0077005F (unknown), RW 0x007728E8 (unknown), RW 0x00774F2B (unknown), RW 0x0077B00A (unknown), RW 0x00862A4E (ported), RW 0x008E1BD4 (ported), RW 0x008F144C (unknown). Classes: AIGroup, PathfinderPosGoalManager, SpawnBehavior.
Candidates: 0 none/partial functions; 7 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: #465 Port 198: SpawnBehavior: the spawn interface's attack / idle orders and Player::onUnitCreated (S-1425).

### G346: RW 0x008208D4

Stops: S-1959. Kinds: code.
Functions: RW 0x008208D4 (unknown), RW 0x00820A2B (unknown), RW 0x00820A57 (unknown), RW 0x00820EF0 (unknown), RW 0x00821073 (unknown), RW 0x0090E28C (unknown), RW 0x00911407 (unknown). Classes: DOTManager, DOTNugget.
Candidates: 0 none/partial functions; 7 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G347: RW 0x0092F082

Stops: S-2700, S-289, S-293. Kinds: code.
Functions: RW 0x0092E753 (none), RW 0x0092EE8B (unknown), RW 0x0092F015 (unknown), RW 0x0092F082 (unknown), RW 0x0092F27D (unknown), RW 0x0092FF5C (unknown), RW 0x00930035 (unknown), RW 0x00930088 (unknown), RW 0x009300F8 (unknown), RW 0x0093023D (unknown), RW 0x00943D6F (unknown), RW 0x0096F53C (unknown), RW 0x0096F85A (unknown), RW 0x0096FAC6 (unknown), RW 0x0097D718 (ported), RW 0x009D28D4 (unknown), RW 0x009D2AED (unknown), RW 0x009D3380 (unknown), RW 0x009D353B (unknown), RW 0x009D6EC7 (unknown), RW 0x009D72DB (unknown), RW 0x009DC1A1 (unknown). Classes: CommandButton, CommandSet, ControlBar.
Candidates: 1 none/partial functions; 20 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G348: RW 0x007846EE

Stops: S-3610. Kinds: code.
Functions: RW 0x007846EE (unknown), RW 0x007BA37A (unknown), RW 0x008431B8 (unknown), RW 0x008433D4 (unknown), RW 0x00847508 (unknown), RW 0x009758E3 (unknown), RW 0x0097597F (unknown). Classes: GlobalData, PlayerTemplate, SkirmishGameSetup.
Candidates: 0 none/partial functions; 7 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G349: RW 0x009EDA17

Stops: S-419. Kinds: code.
Functions: RW 0x008EE765 (unknown), RW 0x008EEBAC (unknown), RW 0x008EEC71 (unknown), RW 0x008EEF8F (unknown), RW 0x0099327C (unknown), RW 0x0099F313 (unknown), RW 0x009EDA17 (unknown). Classes: ArmyDefinition.
Candidates: 0 none/partial functions; 7 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G350: RW 0x0068E43B

Stops: S-560. Kinds: code.
Functions: RW 0x0068E43B (ported), RW 0x0068EC3B (unknown), RW 0x007C19D7 (unknown), RW 0x008DD6F3 (ported), RW 0x00B4EF50 (unknown), RW 0x00B509E0 (unknown), RW 0x00B51030 (unknown), RW 0x00B52740 (unknown), RW 0x00B52BF0 (unknown). Classes: AudioSettings, ScriptActions.
Candidates: 0 none/partial functions; 7 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G351: RW 0x005E31DF

Stops: S-7217. Kinds: project.
Functions: RW 0x0042E558 (unknown), RW 0x005E31DF (unknown), RW 0x005E357E (unknown), RW 0x005E4326 (unknown), RW 0x005E4653 (none), RW 0x006CA144 (unknown), RW 0x006CA14B (unknown), RW 0x0076CDFB (ported), RW 0x0076D25F (ported), RW 0x0086588C (ported). Classes: GiantBirdAIUpdate, LocomotorTemplate, OpenContain, WeaponTemplate.
Candidates: 1 none/partial functions; 6 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G352: RW 0x006AF269

Stops: S-1365, S-7390. Kinds: code.
Functions: RW 0x0060BD42 (ported), RW 0x00662597 (unknown), RW 0x0068DD46 (unknown), RW 0x00695003 (unknown), RW 0x006ADD22 (unknown), RW 0x006AF269 (unknown), RW 0x0071EFA2 (ported), RW 0x007396E8 (unknown), RW 0x00759FDA (ported), RW 0x0079FE29 (unknown), RW 0x007A09E4 (unknown), RW 0x007A12F4 (ported), RW 0x007A1FB1 (unknown), RW 0x007A2004 (unknown), RW 0x007A208C (unknown), RW 0x007A267D (unknown), RW 0x007A59CB (unknown), RW 0x0089679D (ported), RW 0x008969E5 (ported), RW 0x00896A32 (none), RW 0x008973EE (unknown). Classes: CommandSet, ScriptEngine, SpecialPowerModule, SpecialPowerModuleInterface.
Candidates: 1 none/partial functions; 14 unknown status.
Red buttons: Command_AngmarRangersIceArrowAbility, Command_AngmarTrollChargeAttack, Command_AxtschwingerAxeThrow, Command_BeregondMountNew, Command_BigScorpionUnderground, Command_BlackRiderMorgulBladeForHorde, Command_FireDrakeBroodInferno, Command_GorkilBombThrow, Command_HalftrollChargeAttack, Command_IsenfurtWchterSmite, Command_NecromancerDeathBargain, Command_RavenPostSecondBonus, Command_SmaugIncinerateNew, Command_SpecialAbilityBalrogBreathAI, Command_SpecialAbilityBoromirHorn, Command_SpecialAbilityBoromirHornRing, Command_SpecialAbilityBoromirTapfererWiderstand, Command_SpecialAbilityBuhrdurFressen, Command_SpecialAbilityDenethorNotstandssituation, Command_SpecialAbilityDenethorSehenderStein1, Command_SpecialAbilityDenethorVerzweiflungsakt, Command_SpecialAbilityDurinMeisterGestein, Command_SpecialAbilityElendilSummoned, Command_SpecialAbilityGeheimeWege, Command_SpecialAbilityGulzarMOrgulSeucheAcolyte, Command_SpecialAbilityHaradScoutCloak, Command_SpecialAbilityKingsFavorEdainTheo, Command_SpecialAbilityMordorBalrogWhipAI, Command_SpecialAbilityNaturmagierErntesegen, Command_SpecialAbilitySaatdesgoldenenWaldes, Command_SpecialAbilityScreechDunklerMarschall, Command_SpecialAbilityTributeGainTwo, Command_ThranduilKnigDsterwalds.
Unused fields: none linked.
Existing task matches: #755 Port 319: Team state: the members' trigger flags (+ 0x5C), the team events list and the attacker records' expiry (S-1365); #599 Port 251: The special-power chain every Ennorath ability runs through: SpecialPowerModule and UnpauseSpecialPowerUpgrade's uncited RotWK functions.

### G353: RW 0x0080ACE3

Stops: S-1226, S-1401. Kinds: code.
Functions: RW 0x0061A10F (unknown), RW 0x0061B103 (unknown), RW 0x0061B17D (ported), RW 0x0061E7EE (unknown), RW 0x0061E9CD (unknown), RW 0x0061EB52 (unknown), RW 0x0061F82B (unknown), RW 0x0061FFF2 (unknown), RW 0x006314CD (unknown), RW 0x0068AB07 (ported), RW 0x006D32E4 (ported), RW 0x0073C25F (ported), RW 0x0075CD7A (ported), RW 0x00780EF0 (unknown), RW 0x0080959A (unknown), RW 0x008096B0 (ported), RW 0x00809CA6 (ported), RW 0x00809FFB (unknown), RW 0x0080A190 (ported), RW 0x0080AA38 (ported), RW 0x0080ACE3 (unknown), RW 0x0080B339 (ported), RW 0x0080B4F5 (unknown), RW 0x0080C3AB (ported), RW 0x00A200A3 (unknown), RW 0x00A2D770 (unknown). Classes: AptCreateAHero, AptMyHero, CreateAHeroSystem, GameInfo, GameWorld, GroupOrder.
Candidates: 0 none/partial functions; 15 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G354: RW 0x00669932

Stops: S-084, S-161, S-166, S-1750, S-7384. Kinds: code.
Functions: RW 0x004B37A3 (ported), RW 0x004C72BE (ported), RW 0x005E2F24 (unknown), RW 0x005E3BA5 (ported), RW 0x005E4CA7 (ported), RW 0x005E4CD3 (ported), RW 0x005E4F44 (unknown), RW 0x005E5785 (unknown), RW 0x005E5D9A (unknown), RW 0x005E613F (unknown), RW 0x005E6D4F (unknown), RW 0x005E737B (unknown), RW 0x005E742E (unknown), RW 0x005E74C1 (unknown), RW 0x005E76AC (ported), RW 0x005E77E6 (unknown), RW 0x005E7ACC (unknown), RW 0x005E7CC7 (ported), RW 0x005E8865 (ported), RW 0x005E91E2 (unknown), RW 0x005E98D6 (ported), RW 0x00662A0E (ported), RW 0x00663E32 (unknown), RW 0x00664127 (unknown), RW 0x00664485 (ported), RW 0x00665186 (ported), RW 0x006654AD (unknown), RW 0x00665C33 (ported), RW 0x006668CA (ported), RW 0x00668E94 (ported), RW 0x006695EF (unknown), RW 0x00669932 (ported), RW 0x0066D16E (ported), RW 0x00680A75 (ported), RW 0x0068B3CF (unknown), RW 0x0068D7AB (ported), RW 0x00694BF8 (ported), RW 0x0069519A (ported), RW 0x006D328E (ported), RW 0x006EA42D (ported), RW 0x006EB792 (unknown), RW 0x006EE600 (unknown), RW 0x006EF3C3 (unknown), RW 0x006EF591 (unknown), RW 0x006F0741 (ported), RW 0x006F1B3E (unknown), RW 0x006F5547 (unknown), RW 0x006F5BB0 (ported), RW 0x006F6286 (unknown), RW 0x006F8B6E (unknown), RW 0x006FB869 (unknown), RW 0x006FC9DA (unknown), RW 0x0070BA76 (ported), RW 0x00765565 (unknown), RW 0x007681BB (unknown), RW 0x007687EB (unknown), RW 0x00768832 (unknown), RW 0x00768AA7 (unknown), RW 0x00768AE9 (unknown), RW 0x007767A9 (ported), RW 0x00872EFC (ported), RW 0x00934E34 (ported), RW 0x0093527F (unknown), RW 0x00937EC3 (unknown), RW 0x009389CE (unknown), RW 0x009A67E1 (unknown), RW 0x009BC1A0 (unknown). Classes: AIMover, BridgeBehavior, GiantBirdAIUpdate, HordeContain, PathfindLayer, PhysicsBehavior, StealthUpdate, ThingTemplate.
Candidates: 0 none/partial functions; 38 unknown status.
Red buttons: none linked.
Unused fields: HordeContain.LivingWorldOverloadTemplate, HordeContain.RanksToJustFreeWhenAttacking, StealthUpdate.HintDetectableConditions.
Existing task matches: #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate; #296 [deferred] Local 40: bridge logic as retail: towers, damage stages, pathfinding layer, radar (BridgeBehavior, S-7223).

### G355: RW 0x00701443

Stops: S-1188, S-1227, S-1228, S-3802, S-420, S-4462, S-583, S-702. Kinds: code.
Functions: RW 0x004035AC (unknown), RW 0x00405553 (ported), RW 0x004627C1 (unknown), RW 0x005E2DF4 (ported), RW 0x0065F4A2 (ported), RW 0x0065F4DC (ported), RW 0x00660CA4 (unknown), RW 0x0066276B (ported), RW 0x00662A21 (unknown), RW 0x00664485 (ported), RW 0x006644E6 (ported), RW 0x00667144 (ported), RW 0x0066B1D0 (unknown), RW 0x0066C4CA (ported), RW 0x0068BBE0 (ported), RW 0x0068D6A6 (ported), RW 0x0068E43B (ported), RW 0x0068E5A3 (ported), RW 0x0068E67A (ported), RW 0x0068E754 (unknown), RW 0x0068FA3D (ported), RW 0x00691269 (ported), RW 0x00691421 (ported), RW 0x006950F3 (ported), RW 0x0069519A (ported), RW 0x00696266 (ported), RW 0x00696FD2 (ported), RW 0x00697890 (unknown), RW 0x006979D9 (unknown), RW 0x006AF150 (unknown), RW 0x006AF4B1 (unknown), RW 0x006C8652 (ported), RW 0x006C9147 (ported), RW 0x006E8BBA (unknown), RW 0x006EEBC1 (unknown), RW 0x006EF387 (ported), RW 0x006F0AFE (unknown), RW 0x006F0DA3 (unknown), RW 0x006F2F8F (ported), RW 0x006F58C3 (unknown), RW 0x006F5BB0 (ported), RW 0x006FAA6D (unknown), RW 0x006FF412 (ported), RW 0x006FF7FA (ported), RW 0x006FFBEF (unknown), RW 0x007006D7 (ported), RW 0x00701443 (ported), RW 0x0070146A (unknown), RW 0x0073ACEF (unknown), RW 0x0074109B (unknown), RW 0x00741133 (unknown), RW 0x00741450 (unknown), RW 0x00742271 (unknown), RW 0x0074379D (unknown), RW 0x00743818 (unknown), RW 0x00743BD2 (unknown), RW 0x00743C14 (unknown), RW 0x00744D6E (unknown), RW 0x00747EF4 (unknown), RW 0x0074B1B7 (unknown), RW 0x0074D326 (unknown), RW 0x0074F21A (unknown), RW 0x0074F31F (unknown), RW 0x007510EC (unknown), RW 0x00752C4D (unknown), RW 0x0075CC3E (ported), RW 0x00765BF6 (unknown), RW 0x0076F037 (partial), RW 0x0076F115 (ported), RW 0x0076F426 (ported), RW 0x0076FFF0 (ported), RW 0x0077122F (unknown), RW 0x0077348C (ported), RW 0x007747DE (ported), RW 0x00774897 (ported), RW 0x00774E2F (ported), RW 0x00792973 (unknown), RW 0x0079FDEA (unknown), RW 0x007A3E36 (unknown), RW 0x007BBCF9 (unknown), RW 0x007C0347 (unknown), RW 0x007C9531 (unknown), RW 0x00855830 (ported), RW 0x0085730B (ported), RW 0x0085750D (ported), RW 0x0085D2FF (ported), RW 0x0085D4B5 (unknown), RW 0x0085D5EA (ported), RW 0x0085D6AD (unknown), RW 0x0085D76C (unknown), RW 0x0085D930 (unknown), RW 0x0085D9D1 (ported), RW 0x0085DF88 (ported), RW 0x008983B5 (ported), RW 0x0089841E (ported), RW 0x008C3A33 (ported), RW 0x008DBDFF (unknown), RW 0x008E302A (ported), RW 0x00949994 (unknown), RW 0x00949A10 (unknown), RW 0x00949BD3 (unknown), RW 0x00949CA7 (unknown), RW 0x0094A29F (unknown), RW 0x0094A407 (unknown), RW 0x0099116C (unknown), RW 0x0099135D (unknown), RW 0x009913BC (unknown), RW 0x00991500 (unknown), RW 0x00991626 (unknown), RW 0x00991660 (unknown). Classes: AIAttackState, AIGoingIdleState, AIGuardState, AutoAbilityBehavior, CommandButton, DualWeaponBehavior, FiringTracker, GettingBuiltBehavior, GiantBirdAIUpdate, PhysicsBehavior, StateMachine, TargetFinder, UnpauseSpecialPowerUpgrade, WeaponModeSpecialPowerUpdate, WeaponSet.
Candidates: 1 none/partial functions; 60 unknown status.
Red buttons: Command_AngmarRangersIceArrowAbility, Command_AngmarTrollChargeAttack, Command_AxtschwingerAxeThrow, Command_BeregondMountNew, Command_BigScorpionUnderground, Command_BlackRiderMorgulBladeForHorde, Command_FireDrakeBroodInferno, Command_GorkilBombThrow, Command_GorkilZuchtspinnenSummon, Command_HalftrollChargeAttack, Command_IsenfurtWchterSmite, Command_MenLongShotAbility, Command_NecromancerDeathBargain, Command_OutpostOrcSpawn, Command_RavenPostSecondBonus, Command_RepairAI, Command_SmaugIncinerateNew, Command_SpecialAbilityBalrogBreathAI, Command_SpecialAbilityBeutekarrendesGroorks, Command_SpecialAbilityBoromirHorn, Command_SpecialAbilityBoromirHornRing, Command_SpecialAbilityBoromirTapfererWiderstand, Command_SpecialAbilityDenethorSehenderStein1, Command_SpecialAbilityDenethorSteuererlass, Command_SpecialAbilityDenethorVerzweiflungsakt, Command_SpecialAbilityDrarSpeerfeuer, Command_SpecialAbilityDurinMeisterGestein, Command_SpecialAbilityElendilSummoned, Command_SpecialAbilityGulzarMOrgulSeucheAcolyte, Command_SpecialAbilityGwahirScreechManve, Command_SpecialAbilityKingsFavorEdainTheo, Command_SpecialAbilityLoneRangerSummonFinal, Command_SpecialAbilityMordorBalrogWhipAI, Command_SpecialAbilityMuhadSummonOliph, Command_SpecialAbilityNaturmagierErntesegen, Command_SpecialAbilityScreechDunklerMarschall, Command_SpecialAbilityTributeGainTwo, Command_SpecialAbilityWulfgarRufderPlnderer, Command_ThranduilKnigDsterwalds, Command_ramridercaptain.
Unused fields: AutoAbilityBehavior.WorkingRadius.
Existing task matches: #753 Port 317: AI guard: object and team guard targets with the script guard commands, the turret target and the outer re-entry (S-3802); #751 Port 315: AI hunt: the crate pick-up state and the attack priority info (S-1188); #687 Port 295: GettingBuiltBehavior вЂ” the uncited own functions of a building under construction (19); #599 Port 251: The special-power chain every Ennorath ability runs through: SpecialPowerModule and UnpauseSpecialPowerUpgrade's uncited RotWK functions.

### G356: RW 0x006AA91D

Stops: S-529. Kinds: code.
Functions: RW 0x006AA91D (ported), RW 0x006AD1B0 (ported), RW 0x006AD22B (unknown), RW 0x006AD26F (ported), RW 0x00758CBF (unknown), RW 0x00758D21 (unknown), RW 0x00758D83 (unknown), RW 0x00759986 (ported), RW 0x007599EA (unknown), RW 0x007B0F25 (ported), RW 0x007E4E08 (unknown), RW 0x00853EDF (ported), RW 0x00896756 (ported), RW 0x008969E5 (ported), RW 0x00896B56 (ported), RW 0x00896C72 (ported), RW 0x00896CF2 (ported), RW 0x00896E31 (ported), RW 0x0089713F (ported), RW 0x008973EE (unknown), RW 0x0089763B (ported), RW 0x00897987 (ported), RW 0x00897E87 (ported), RW 0x008A007F (ported), RW 0x008C75D8 (ported), RW 0x008CC4B4 (ported), RW 0x00991500 (unknown). Classes: OCLSpecialPower, PlayerHealSpecialPower, SpecialAbilityUpdate, SpecialPowerModule.
Candidates: 0 none/partial functions; 8 unknown status.
Red buttons: Command_AngmarRangersIceArrowAbility, Command_AngmarTrollChargeAttack, Command_AxtschwingerAxeThrow, Command_BeregondMountNew, Command_BigScorpionUnderground, Command_BlackRiderMorgulBladeForHorde, Command_FireDrakeBroodInferno, Command_GaladrielMirrorDissent, Command_GaladrielMirrorWardByMirror, Command_GorkilBombThrow, Command_GorkilZuchtspinnenSummon, Command_HalftrollChargeAttack, Command_IsenfurtWchterSmite, Command_MenLongShotAbility, Command_NecromancerDeathBargain, Command_OutpostOrcSpawn, Command_RavenPostSecondBonus, Command_SmaugIncinerateNew, Command_SpecialAbilityBalrogBreathAI, Command_SpecialAbilityBeutekarrendesGroorks, Command_SpecialAbilityBoromirHorn, Command_SpecialAbilityBoromirHornRing, Command_SpecialAbilityBoromirTapfererWiderstand, Command_SpecialAbilityBuhrdurFressen, Command_SpecialAbilityDenethorNotstandssituation, Command_SpecialAbilityDenethorSehenderStein1, Command_SpecialAbilityDenethorSteuererlass, Command_SpecialAbilityDenethorVerzweiflungsakt, Command_SpecialAbilityDrarSpeerfeuer, Command_SpecialAbilityDurinMeisterGestein, Command_SpecialAbilityElendilSummoned, Command_SpecialAbilityGeheimeWege, Command_SpecialAbilityGulzarMOrgulSeucheAcolyte, Command_SpecialAbilityHaradScoutCloak, Command_SpecialAbilityKingsFavorEdainTheo, Command_SpecialAbilityLoneRangerSummonFinal, Command_SpecialAbilityMordorBalrogWhipAI, Command_SpecialAbilityMuhadSummonOliph, Command_SpecialAbilityNaturmagierErntesegen, Command_SpecialAbilitySaatdesgoldenenWaldes, Command_SpecialAbilityScreechDunklerMarschall, Command_SpecialAbilityTributeGainTwo, Command_ThranduilKnigDsterwalds, Command_ramridercaptain.
Unused fields: none linked.
Existing task matches: #599 Port 251: The special-power chain every Ennorath ability runs through: SpecialPowerModule and UnpauseSpecialPowerUpgrade's uncited RotWK functions.

### G357: AutoAbilityBehavior

Stops: S-7722. Kinds: project.
Functions: RW 0x006289E7 (none), RW 0x00629BCA (none), RW 0x0062AB74 (none), RW 0x0062FF03 (partial), RW 0x0064ACD2 (unknown), RW 0x006A95E6 (none), RW 0x0085D2FF (ported), RW 0x0085D355 (unknown), RW 0x0085DEC9 (unknown). Classes: AsciiString, AutoAbilityBehavior, AutoAbilityBehaviorModuleData.
Candidates: 5 none/partial functions; 3 unknown status.
Red buttons: Command_AngmarRangersIceArrowAbility, Command_AxtschwingerAxeThrow, Command_BlackRiderMorgulBladeForHorde, Command_IsenfurtWchterSmite, Command_RepairAI, Command_SpecialAbilityBalrogBreathAI, Command_SpecialAbilityBoromirHorn, Command_SpecialAbilityBoromirHornRing, Command_SpecialAbilityBoromirTapfererWiderstand, Command_SpecialAbilityDenethorSteuererlass, Command_SpecialAbilityGwahirScreechManve, Command_SpecialAbilityNaturmagierErntesegen.
Unused fields: AutoAbilityBehavior.WorkingRadius.
Existing task matches: none.

### G358: RW 0x006CAB85

Stops: S-1100, S-1106, S-360, S-7296. Kinds: code.
Functions: RW 0x0042ED6F (unknown), RW 0x00441B59 (ported), RW 0x004C34A2 (ported), RW 0x0066246B (ported), RW 0x00664E18 (unknown), RW 0x006658D3 (unknown), RW 0x006756A1 (ported), RW 0x0068B58C (ported), RW 0x006901AE (ported), RW 0x0069024C (unknown), RW 0x006CAB85 (ported), RW 0x006CB490 (ported), RW 0x006CD11F (ported), RW 0x006CD1FD (ported), RW 0x006CD298 (ported), RW 0x006E8707 (ported), RW 0x006F5F65 (ported), RW 0x006FF412 (ported), RW 0x0070BCE7 (ported), RW 0x0074D25D (unknown), RW 0x0074E6CE (unknown), RW 0x0075232F (unknown), RW 0x00753295 (unknown), RW 0x00763543 (ported), RW 0x008530BA (ported), RW 0x00865CE3 (unknown), RW 0x00865D3D (unknown), RW 0x0086603B (ported), RW 0x00866367 (unknown), RW 0x00867120 (unknown), RW 0x008680F4 (ported), RW 0x0087B042 (unknown), RW 0x0087B3EC (unknown), RW 0x0087B6E1 (unknown), RW 0x0087B8C5 (ported), RW 0x0087B935 (ported), RW 0x0087BBE8 (ported), RW 0x0087BD77 (unknown), RW 0x0087BE7C (unknown), RW 0x0087BEC8 (ported), RW 0x0087BFA6 (ported), RW 0x0087C273 (unknown), RW 0x0087C2B7 (unknown), RW 0x0087C5D8 (ported), RW 0x0087C6CE (ported), RW 0x0087C8DF (ported), RW 0x0087CA2B (ported), RW 0x0087CD76 (ported), RW 0x0087CFEB (ported), RW 0x0087D0E0 (ported), RW 0x0087D252 (ported), RW 0x0087D3CA (ported), RW 0x0089958F (ported), RW 0x008A641A (ported), RW 0x008B5738 (ported), RW 0x008C1B14 (ported), RW 0x008C281E (ported), RW 0x008DBFFC (unknown), RW 0x008DC182 (unknown), RW 0x008DC6E7 (unknown), RW 0x008DC75E (unknown), RW 0x008DCBF0 (unknown), RW 0x008DCC8A (unknown), RW 0x008DCF1D (unknown), RW 0x008DD01F (unknown), RW 0x008DD079 (unknown), RW 0x008DD0E4 (unknown), RW 0x008DD1A6 (unknown), RW 0x0090FA48 (unknown), RW 0x0090FC70 (unknown), RW 0x00990DEA (ported), RW 0x00990E5F (ported). Classes: ActiveBody, AutoPickUpUpdate, BezierProjectileBehavior, GarrisonContain, HordeGarrisonContain, InactiveBody, LevelGrantSpecialPower, ObjectFilter, OpenContain, ProjectileNugget, SpecialAbilityUpdate, StealthDetectorUpdate, TargetFinder, TurretAI.
Candidates: 0 none/partial functions; 32 unknown status.
Red buttons: Command_AngmarTrollChargeAttack, Command_SpecialAbilityKingsFavorEdainTheo, Command_SpecialAbilityLoneRangerSummonFinal, Command_SpecialAbilityNaturmagierErntesegen, Command_ThranduilKnigDsterwalds.
Unused fields: ActiveBody.EnteringDamagedTransitionTime, ActiveBody.ReallyDamagedAttributeModifier.
Existing task matches: #567 Port 243: Body damage states: ActiveBody EnteringDamagedTransitionTime and the Damaged / ReallyDamaged attribute modifiers (ActiveBody, RespawnBody); #467 Port 200 (parked: after Port 218): TurretAI: the mood timer and matrix, isAbleToAttack / the continued-target test, slot 0x1C4 (S-1106).

### G359: RW 0x0068B244

Stops: S-1020. Kinds: code.
Functions: RW 0x0068B244 (ported), RW 0x0068E31F (ported), RW 0x0068EE53 (unknown), RW 0x00A39570 (ported), RW 0x00A3A6B0 (unknown), RW 0x00A3A700 (unknown), RW 0x00A3A860 (unknown), RW 0x00A3B450 (unknown), RW 0x00A3B4E0 (unknown), RW 0x00A3B580 (ported), RW 0x00A3BBB0 (unknown), RW 0x00A3BD20 (unknown), RW 0x00A3BDB0 (ported), RW 0x00A3C4E0 (ported), RW 0x00AD2CE0 (ported). Classes: GeometryInfo, PartitionManager.
Candidates: 0 none/partial functions; 8 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G360: RW 0x00824448

Stops: S-1323. Kinds: code.
Functions: RW 0x00824448 (unknown), RW 0x00824B8A (unknown), RW 0x008287E3 (unknown), RW 0x00828C70 (unknown), RW 0x008293C6 (unknown), RW 0x00829CE8 (unknown), RW 0x0082A2A2 (unknown), RW 0x00836B35 (unknown). Classes: AutoResolveArmor, AutoResolveBody, AutoResolveWeapon, ThingFactory, ThingTemplate.
Candidates: 0 none/partial functions; 8 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G361: RW 0x0069990F

Stops: S-141, S-7295. Kinds: code.
Functions: RW 0x00533860 (unknown), RW 0x00534864 (unknown), RW 0x00534C4C (unknown), RW 0x0063F3BF (ported), RW 0x0064BECC (unknown), RW 0x0064BF04 (unknown), RW 0x0064FF7C (unknown), RW 0x006957B9 (unknown), RW 0x0069990F (unknown), RW 0x006AD2B5 (ported), RW 0x006AD3E6 (partial), RW 0x006CA434 (ported), RW 0x006CA4B5 (ported), RW 0x006CA7AC (ported), RW 0x006CB559 (unknown), RW 0x0087BEC8 (ported), RW 0x00884CF1 (unknown), RW 0x008A5319 (unknown), RW 0x008B97C0 (unknown), RW 0x008B980D (unknown), RW 0x008CF957 (ported), RW 0x008CFB87 (unknown), RW 0x008CFC29 (unknown), RW 0x008E3174 (ported). Classes: DefectionHelper, GlobalData, ObjectHelperShell, SMCHelper, WeaponBonus, WeaponBonusUpgrade, WeaponSet, WeaponStatusHelper.
Candidates: 1 none/partial functions; 15 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G362: RW 0x005E9BEF

Stops: S-1711, S-2520. Kinds: code.
Functions: RW 0x0042D753 (unknown), RW 0x005E9BEF (unknown), RW 0x005E9D3C (unknown), RW 0x0069B7BE (unknown), RW 0x0069CE5F (unknown), RW 0x0069FF85 (unknown), RW 0x0091B5E9 (unknown), RW 0x0091B603 (unknown), RW 0x0091C2FC (unknown), RW 0x009C667F (unknown), RW 0x009C6737 (unknown), RW 0x009C6748 (unknown), RW 0x009C6765 (unknown), RW 0x009C6A7F (unknown), RW 0x009C6BB5 (unknown), RW 0x009C7113 (unknown). Classes: AptMainMenu, CommandButtonHelp, CreditsManager, GameFont, GlobalLanguage, InGameUI, MilitaryCaptionSettings, MiscAudio, UnicodeString.
Candidates: 0 none/partial functions; 16 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G363: DynamicPortalBehaviour

Stops: S-4853. Kinds: code.
Functions: RW 0x0062DCE4 (unknown), RW 0x0068C650 (ported), RW 0x00694692 (unknown), RW 0x00858701 (ported), RW 0x008639EF (ported), RW 0x00863B28 (ported), RW 0x00863D3A (ported), RW 0x00863EB3 (unknown), RW 0x0086404D (ported), RW 0x00864314 (ported), RW 0x00864341 (ported), RW 0x00864357 (ported), RW 0x008643EC (ported), RW 0x0086441E (unknown), RW 0x00864794 (unknown), RW 0x0086498E (unknown), RW 0x008649B7 (unknown), RW 0x008649ED (ported), RW 0x00864A00 (ported), RW 0x00864B29 (unknown), RW 0x00864D02 (ported), RW 0x00864EAA (ported), RW 0x00AD2040 (ported). Classes: DynamicPortalBehaviour, FakePathfindPortalBehaviour, FoundationAIUpdate, UpgradeModule.
Candidates: 0 none/partial functions; 8 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: #688 Port 296: Castles вЂ” CastleBehavior, CastleMemberBehavior and FoundationAIUpdate: the uncited own functions (27) and their five unread fields.

### G364: RW 0x008F3EFE

Stops: S-7188. Kinds: project.
Functions: RW 0x008F3EFE (unknown), RW 0x008F54BA (unknown), RW 0x008F6DE9 (unknown), RW 0x008F7F2B (unknown), RW 0x008F805D (unknown), RW 0x008F81BE (unknown), RW 0x008F9487 (unknown), RW 0x008F994F (unknown), RW 0x00A3CFA4 (ported). Classes: GameLogicSettings, SkirmishAI, TeamPrototype.
Candidates: 0 none/partial functions; 8 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G365: RW 0x00885CD5

Stops: S-140, S-924. Kinds: code.
Functions: RW 0x00625685 (unknown), RW 0x006533E5 (unknown), RW 0x006CEF6D (ported), RW 0x006CF3AE (unknown), RW 0x006CF3D2 (ported), RW 0x0077A0B2 (unknown), RW 0x007C46A0 (unknown), RW 0x0082D925 (ported), RW 0x008554D6 (ported), RW 0x0085C5DE (ported), RW 0x0085C655 (unknown), RW 0x0085C704 (ported), RW 0x0085C75E (ported), RW 0x0085C8C9 (ported), RW 0x00885A6F (unknown), RW 0x00885CD5 (ported), RW 0x00885D90 (ported), RW 0x0088B7B0 (unknown), RW 0x0088B84F (ported), RW 0x0088F0AA (ported), RW 0x0088F554 (ported), RW 0x0088F731 (unknown), RW 0x00898A2D (unknown), RW 0x00898A5D (unknown), RW 0x00898B7C (ported), RW 0x00898C17 (unknown), RW 0x00898EE8 (unknown), RW 0x0089903C (ported), RW 0x008BBCB6 (ported), RW 0x008BBD1E (unknown), RW 0x008BBE71 (ported), RW 0x008BBEFE (ported), RW 0x008BBF5A (ported), RW 0x008BBF93 (ported), RW 0x008C942D (unknown), RW 0x008C94D7 (ported), RW 0x008C9629 (ported), RW 0x008C98AA (unknown), RW 0x008CD1F6 (ported), RW 0x008CD256 (ported), RW 0x008CD297 (ported), RW 0x008CD313 (unknown), RW 0x00B6C480 (ported). Classes: ActiveBody, AudioLoopUpgrade, ClickReactionBehavior, CommandButtonHuntUpdate, CritterEmitterUpdate, DeletionUpdate, FireWeaponUpdate, FireWeaponWhenDamagedBehavior, GrabPassengerSpecialPower, HordeContain, ModEngineFeature, ObjectWeapons, WeaponSet.
Candidates: 0 none/partial functions; 17 unknown status.
Red buttons: none linked.
Unused fields: ActiveBody.EnteringDamagedTransitionTime, ActiveBody.ReallyDamagedAttributeModifier, HordeContain.LivingWorldOverloadTemplate, HordeContain.RanksToJustFreeWhenAttacking.
Existing task matches: #766 Local 100: upgrade modules: CommandSetUpgrade (515 modules), CostModifierUpgrade, CastleUpgrade, GiveUpgradeUpdate, AudioLoopUpgrade, WallUpgradeUpdate, LocomotorSetUpgrade: 27 uncited own functions; #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate; #567 Port 243: Body damage states: ActiveBody EnteringDamagedTransitionTime and the Damaged / ReallyDamaged attribute modifiers (ActiveBody, RespawnBody).

### G366: RW 0x0046448B

Stops: S-113, S-4642. Kinds: code.
Functions: RW 0x00464453 (unknown), RW 0x0046448B (unknown), RW 0x004645EE (unknown), RW 0x00464930 (unknown), RW 0x004B1B5A (ported), RW 0x004CE61A (unknown), RW 0x004CE74B (unknown), RW 0x004CF121 (unknown), RW 0x004CF246 (ported), RW 0x004CF2B5 (ported), RW 0x004CF5EE (ported), RW 0x004D054B (unknown), RW 0x004D12FA (unknown), RW 0x004D1417 (unknown), RW 0x004D1981 (unknown), RW 0x004D20C6 (unknown), RW 0x004D3376 (unknown), RW 0x00671E56 (unknown), RW 0x00676711 (ported), RW 0x00707FFF (unknown), RW 0x007641A2 (unknown), RW 0x00764483 (unknown), RW 0x008CEF91 (ported), RW 0x009F3A3C (ported). Classes: BuffInstance, BuffManager, ClientBuffs, FXPlayback, LightPulsePool, PlayerHealSpecialPower, W3DBuffDraw, W3DBuffDrawModuleData, W3DDebrisDraw, W3DDefaultDraw, W3DLaserDraw, W3DLightDraw, W3DLightDrawModuleData, W3DProjectileStreamDraw, W3DProjectileStreamDrawModuleData, W3DStreakDraw, W3DTornadoDraw, W3DTornadoDrawModuleData.
Candidates: 0 none/partial functions; 17 unknown status.
Red buttons: none linked.
Unused fields: W3DLightDraw.Radius.
Existing task matches: #700 Port 308: Small module gaps with candidate readers вЂ” LargeGroupBonusUpdate.AlliesOnly, W3DLightDraw.Radius, EvaAnnounceClientCreate.CreateFakeRadarEvent and their classes' uncited functions.

### G367: RW 0x00753755

Stops: S-1107, S-7581. Kinds: code.
Functions: RW 0x00660D2C (ported), RW 0x00664ABF (ported), RW 0x0068DF11 (ported), RW 0x006F5547 (unknown), RW 0x006F5F65 (ported), RW 0x006FF3A2 (unknown), RW 0x00743AA6 (unknown), RW 0x00743B40 (unknown), RW 0x007446FD (unknown), RW 0x00747EAA (unknown), RW 0x0074BB9A (unknown), RW 0x00753755 (unknown), RW 0x0077ACCE (unknown), RW 0x007C95FE (unknown), RW 0x007CA692 (unknown), RW 0x00865EB6 (ported), RW 0x00868759 (unknown), RW 0x0087A361 (ported), RW 0x0087A606 (ported), RW 0x0087CE3D (ported), RW 0x0087CE66 (ported), RW 0x00880E29 (ported), RW 0x008810C9 (ported), RW 0x008811DD (ported), RW 0x0088121A (ported), RW 0x008812B0 (ported), RW 0x00881367 (ported), RW 0x008FA451 (unknown), RW 0x008FA4C9 (unknown), RW 0x008FA4E8 (ported), RW 0x008FA569 (unknown), RW 0x008FA702 (unknown), RW 0x0094C8D0 (unknown), RW 0x00AD1AE0 (ported). Classes: AITNGuardMachine, AITunnelNetworkGuardState, AIWanderInPlaceState, AIWanderState, HordeGarrisonContain, HordeTransportContain, OpenContain, TunnelContain, TunnelTracker.
Candidates: 0 none/partial functions; 17 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G368: RW 0x009288D0

Stops: S-1406, S-3701. Kinds: code.
Functions: RW 0x005FC95B (unknown), RW 0x006199EB (unknown), RW 0x00619D3B (unknown), RW 0x0061A424 (unknown), RW 0x00800A25 (unknown), RW 0x00801289 (unknown), RW 0x0080204C (unknown), RW 0x008023C1 (unknown), RW 0x00802DBA (unknown), RW 0x00841A9E (unknown), RW 0x00842C93 (unknown), RW 0x00843B4C (unknown), RW 0x00927FEF (unknown), RW 0x009288D0 (unknown), RW 0x00928A29 (unknown), RW 0x009293F5 (unknown), RW 0x009609D9 (unknown). Classes: AptSkirmish, GameInfo, GameSlot, PlayerTemplate, SkirmishGameInfo.
Candidates: 0 none/partial functions; 17 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G369: RW 0x0082FCAB

Stops: S-415, S-416. Kinds: code.
Functions: RW 0x0082FCAB (unknown), RW 0x008E3FD6 (unknown), RW 0x008E4591 (ported), RW 0x008E480C (ported), RW 0x008F069A (ported), RW 0x008F0C47 (ported), RW 0x009A03A1 (unknown), RW 0x009A0426 (unknown), RW 0x009A051D (ported), RW 0x009A05DB (unknown), RW 0x009A0705 (unknown), RW 0x009A0838 (ported), RW 0x009A0993 (unknown), RW 0x009A0B26 (unknown), RW 0x009A0BC0 (unknown), RW 0x009A0D4F (unknown), RW 0x009A0E2B (unknown), RW 0x009A1051 (unknown), RW 0x009A1196 (unknown), RW 0x009ED764 (unknown), RW 0x009EDA17 (unknown), RW 0x009EDB44 (unknown), RW 0x009EDBC6 (unknown). Classes: AIUnitBuilder, ArmyDefinition, BuildAssistant, CommandPoints, SkirmishAI.
Candidates: 0 none/partial functions; 17 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G370: RW 0x006AA5B3

Stops: S-1189, S-982. Kinds: code.
Functions: RW 0x00662DD9 (unknown), RW 0x0066810B (unknown), RW 0x0066B907 (ported), RW 0x0066B9AF (ported), RW 0x0066C1C9 (ported), RW 0x00693B94 (ported), RW 0x006AA5B3 (ported), RW 0x0070042A (ported), RW 0x0074292D (unknown), RW 0x007489B6 (unknown), RW 0x0074BB7E (unknown), RW 0x0074E083 (unknown), RW 0x0075070F (unknown), RW 0x00750AE7 (unknown), RW 0x00750B58 (unknown), RW 0x00755A20 (unknown), RW 0x00755D78 (unknown), RW 0x0076E90A (ported), RW 0x0076EDA7 (ported), RW 0x00771E1E (unknown), RW 0x00771E7D (unknown), RW 0x007BFE32 (unknown), RW 0x007C0091 (unknown), RW 0x007C9196 (unknown), RW 0x007C928D (unknown), RW 0x00894DF8 (ported), RW 0x008E3D1E (partial), RW 0x0094FCBA (ported). Classes: AIGroup, AIMoveToState, CommandSetUpgrade, MonitorConditionUpdate.
Candidates: 1 none/partial functions; 17 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: #766 Local 100: upgrade modules: CommandSetUpgrade (515 modules), CostModifierUpgrade, CastleUpgrade, GiveUpgradeUpdate, AudioLoopUpgrade, WallUpgradeUpdate, LocomotorSetUpgrade: 27 uncited own functions.

### G371: linked stops S-1922, S-1953

Stops: S-1922, S-1953. Kinds: code.
Functions: RW 0x00625456 (ported), RW 0x00626087 (unknown), RW 0x006264E1 (unknown), RW 0x006A950B (ported), RW 0x006A9999 (ported), RW 0x00721FA9 (unknown), RW 0x0081560E (unknown), RW 0x0081606D (unknown), RW 0x008E873E (unknown), RW 0x008E8843 (unknown), RW 0x008E8B61 (unknown), RW 0x008E8BEB (unknown), RW 0x00914B54 (unknown), RW 0x00914C51 (unknown), RW 0x00914EF0 (unknown), RW 0x0091578C (unknown), RW 0x00915BF6 (unknown), RW 0x00915F5D (unknown), RW 0x009164EB (unknown), RW 0x009166AC (unknown), RW 0x0091741F (unknown). Classes: AptObjectivesMenu, AptPalantir, GameWorld, PlayerStatusInfo.
Candidates: 0 none/partial functions; 18 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G372: linked stops S-4751

Stops: S-4751. Kinds: code.
Functions: RW 0x00800ACE (unknown), RW 0x00801360 (unknown), RW 0x00802B04 (unknown), RW 0x008311B1 (unknown), RW 0x0084A8AD (unknown), RW 0x00851E97 (ported), RW 0x009881DE (unknown), RW 0x0098824E (unknown), RW 0x00988418 (unknown), RW 0x009888B7 (unknown). Classes: GameInfo, GameSorter.
Candidates: 0 none/partial functions; 9 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G373: RW 0x006634BF

Stops: S-588. Kinds: code.
Functions: RW 0x006634BF (ported), RW 0x006643FC (ported), RW 0x0066D658 (unknown), RW 0x006CA525 (ported), RW 0x006F2956 (unknown), RW 0x0086F18F (ported), RW 0x0098B7CC (unknown), RW 0x0098BA04 (unknown), RW 0x0098C6A8 (unknown), RW 0x0098F327 (unknown), RW 0x0098F3A3 (unknown), RW 0x0099006A (unknown), RW 0x009902A1 (unknown). Classes: none.
Candidates: 0 none/partial functions; 9 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G374: RW 0x00643374

Stops: S-1061, S-1831, S-451, S-657, S-7197. Kinds: code, project.
Functions: RW 0x004B3D8D (ported), RW 0x006429AD (unknown), RW 0x00643374 (unknown), RW 0x0066C4CA (ported), RW 0x0066E58F (ported), RW 0x0068B3CF (unknown), RW 0x0068F3F2 (unknown), RW 0x00696266 (ported), RW 0x006A9999 (ported), RW 0x006AF60A (unknown), RW 0x006CA525 (ported), RW 0x00754B18 (unknown), RW 0x00756E5C (unknown), RW 0x007571A7 (unknown), RW 0x007572FA (unknown), RW 0x0075748C (unknown), RW 0x0076CDFB (ported), RW 0x00770D27 (unknown), RW 0x00774897 (ported), RW 0x0077B0C8 (unknown), RW 0x0077B2A7 (unknown), RW 0x00793DB9 (unknown), RW 0x0079561F (unknown), RW 0x0079586E (unknown), RW 0x007958DD (unknown), RW 0x00796810 (unknown), RW 0x0079DBC6 (unknown), RW 0x0079DF0E (unknown), RW 0x0079DFC2 (ported), RW 0x0079DFFA (unknown), RW 0x0079E2A8 (unknown), RW 0x0079E404 (unknown), RW 0x0079ECCB (unknown), RW 0x0079F0E1 (ported), RW 0x0079F303 (ported), RW 0x0079F486 (ported), RW 0x0079F704 (unknown), RW 0x00853EDF (ported), RW 0x008566DF (ported), RW 0x00856C7E (none), RW 0x00856E1F (unknown), RW 0x00856E5C (unknown), RW 0x00858701 (ported), RW 0x00875FB2 (ported), RW 0x008840FC (ported), RW 0x0088C5C7 (unknown), RW 0x0088CFFE (ported), RW 0x0088D272 (ported), RW 0x008A11D2 (ported), RW 0x008A13EE (ported), RW 0x008AD6A4 (ported), RW 0x008B140D (unknown), RW 0x008B228C (unknown), RW 0x0094D4F5 (unknown), RW 0x0094D79D (unknown), RW 0x0094DA57 (unknown), RW 0x0094DB32 (unknown), RW 0x0094DC37 (unknown), RW 0x0094DF1B (unknown), RW 0x0094DF97 (unknown), RW 0x0094E184 (unknown), RW 0x0094E232 (unknown), RW 0x0094E857 (unknown), RW 0x0094ED4C (unknown), RW 0x00A28B60 (unknown), RW 0x00A28BC0 (unknown), RW 0x00A28D70 (unknown), RW 0x00AAD950 (unknown), RW 0x00AB9CA0 (unknown). Classes: AIOrdersManager, AIWorld, AttackObjectGroupOrder, BuildAssistant, CastleUpgrade, GarrisonObjectGroupOrder, GlobalData, MoveToFormationGroupOrder, MoveToGroupOrder, ObjectOrderQueue, PlayerList, ScoreKeeper, WallUpgradeUpdate.
Candidates: 1 none/partial functions; 46 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: #766 Local 100: upgrade modules: CommandSetUpgrade (515 modules), CostModifierUpgrade, CastleUpgrade, GiveUpgradeUpdate, AudioLoopUpgrade, WallUpgradeUpdate, LocomotorSetUpgrade: 27 uncited own functions.

### G375: RW 0x00470F44

Stops: S-033, S-4891. Kinds: code.
Functions: RW 0x00467073 (unknown), RW 0x00468CC0 (unknown), RW 0x0046DEA9 (unknown), RW 0x0046F52A (unknown), RW 0x0046F6DB (unknown), RW 0x00470F44 (unknown), RW 0x0048470D (unknown), RW 0x004948AB (unknown), RW 0x00494DBA (unknown), RW 0x00495228 (unknown), RW 0x00497D00 (unknown), RW 0x004AC4ED (unknown), RW 0x004E28F0 (unknown), RW 0x004E44FB (unknown), RW 0x004EDE13 (unknown), RW 0x00614AB6 (unknown), RW 0x00614C71 (unknown), RW 0x0061501A (unknown), RW 0x00615124 (unknown). Classes: CloudEffect, GameWorld, GlobalData, GodotWeather, MapObjectBuilder, MapTerrainBuilder, TerrainLogic, TerrainScorchBuffer.
Candidates: 0 none/partial functions; 19 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G376: RW 0x0048AB28

Stops: S-1200, S-2480. Kinds: code.
Functions: RW 0x00471A5B (unknown), RW 0x0048AB28 (unknown), RW 0x0053B5C0 (unknown), RW 0x00575220 (unknown), RW 0x0059C680 (unknown), RW 0x0062F397 (unknown), RW 0x00660E93 (unknown), RW 0x0066C536 (ported), RW 0x0066DE02 (ported), RW 0x006719FA (ported), RW 0x0068449B (ported), RW 0x0068B5FA (ported), RW 0x0068D6A6 (ported), RW 0x006AAB85 (ported), RW 0x006C8652 (ported), RW 0x00770917 (none), RW 0x0077131B (unknown), RW 0x00771380 (ported), RW 0x0077208A (unknown), RW 0x007721E4 (unknown), RW 0x007721FE (unknown), RW 0x007722AE (unknown), RW 0x00772350 (unknown), RW 0x00772362 (unknown), RW 0x0077AED6 (unknown), RW 0x0077C1C0 (unknown), RW 0x0082CFD4 (unknown), RW 0x00863807 (ported), RW 0x008DB249 (none). Classes: AIGroup, AIUpdateInterface, TerrainTrees.
Candidates: 2 none/partial functions; 17 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G377: RW 0x004874A7

Stops: S-1405, S-3702. Kinds: code.
Functions: RW 0x004474A0 (unknown), RW 0x0048688A (unknown), RW 0x004874A7 (unknown), RW 0x00489C32 (unknown), RW 0x00502858 (unknown), RW 0x00533590 (unknown), RW 0x00533630 (unknown), RW 0x006193B4 (unknown), RW 0x0065BE82 (unknown), RW 0x006DD0DE (unknown), RW 0x006DD1B2 (unknown), RW 0x00919C26 (unknown), RW 0x0091A20A (unknown), RW 0x0091A3A9 (unknown), RW 0x0092B12F (unknown), RW 0x009BF019 (unknown), RW 0x009BF0CD (unknown), RW 0x009BFE74 (unknown), RW 0x009C4076 (unknown). Classes: TacticalCamera.
Candidates: 0 none/partial functions; 19 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G378: RW 0x0042E558

Stops: S-7182, S-7211, S-7221, S-7228, S-7286. Kinds: project.
Functions: RW 0x0042E558 (unknown), RW 0x0042E956 (unknown), RW 0x0042ED00 (unknown), RW 0x0042EEFA (unknown), RW 0x004B2A9B (ported), RW 0x005AF2A0 (unknown), RW 0x005E4326 (unknown), RW 0x005E4F44 (unknown), RW 0x005E586F (unknown), RW 0x005EAB8F (unknown), RW 0x005EAE37 (unknown), RW 0x0064C53A (unknown), RW 0x006564E7 (unknown), RW 0x00656FFE (unknown), RW 0x006570AD (unknown), RW 0x00676346 (unknown), RW 0x006AA31E (unknown), RW 0x006AA450 (unknown), RW 0x006B0239 (unknown), RW 0x006B07EF (unknown), RW 0x006B099B (unknown), RW 0x006CDE89 (unknown), RW 0x006D1D80 (unknown), RW 0x006FFBEF (unknown), RW 0x0073193D (unknown), RW 0x00732BAD (ported), RW 0x0073AFE3 (unknown), RW 0x0073FB7A (unknown), RW 0x0074C4A7 (unknown), RW 0x0074C67C (unknown), RW 0x007510EC (unknown), RW 0x0075E600 (unknown), RW 0x0075E660 (unknown), RW 0x0075E762 (unknown), RW 0x0075E8C2 (unknown), RW 0x0075E9AF (unknown), RW 0x0075EB32 (unknown), RW 0x0075EBB8 (unknown), RW 0x0075EC76 (unknown), RW 0x008886B5 (unknown), RW 0x008886F1 (unknown), RW 0x00888761 (unknown), RW 0x008887A7 (unknown), RW 0x008887D6 (unknown), RW 0x00888A24 (unknown), RW 0x00888B65 (unknown), RW 0x00892F5E (unknown), RW 0x008DB8CB (unknown), RW 0x008F3DF3 (unknown), RW 0x008F7F2B (unknown). Classes: AIAttackFireState, AIAttackState, AISkirmishPlayer, AIUpdateInterface, CrateSystem, CrateTemplate, DrawableIconUI, GameLogicSettings, LinearCampaign, LiveGame, LocomotorTemplate, ModuleData, PlayerList, PlayerTemplate, ShellServices, SidesList, SkirmishAIData, SkirmishAIManager, ThingTemplate, WeaponTemplate.
Candidates: 0 none/partial functions; 48 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G379: RW 0x00695A06

Stops: S-1186, S-150, S-1583, S-585. Kinds: code.
Functions: RW 0x004B3609 (ported), RW 0x005F0EE6 (unknown), RW 0x00603939 (unknown), RW 0x0060396C (unknown), RW 0x00662A21 (unknown), RW 0x006649C6 (ported), RW 0x00664B93 (ported), RW 0x00664C11 (ported), RW 0x00664D7E (ported), RW 0x00664E18 (unknown), RW 0x0066844A (ported), RW 0x0066A7EC (ported), RW 0x0066BD33 (ported), RW 0x0066C03D (ported), RW 0x0066E12A (ported), RW 0x0067449C (ported), RW 0x00674673 (unknown), RW 0x00674B1F (ported), RW 0x006756A1 (ported), RW 0x00675996 (unknown), RW 0x0067651E (unknown), RW 0x0068F259 (ported), RW 0x0069213E (unknown), RW 0x00693A9A (unknown), RW 0x00695A06 (unknown), RW 0x00696E63 (ported), RW 0x00697C09 (ported), RW 0x0069990F (unknown), RW 0x006A8F42 (unknown), RW 0x006F2799 (unknown), RW 0x00700F52 (unknown), RW 0x00740E95 (unknown), RW 0x0074115E (unknown), RW 0x00743864 (unknown), RW 0x0075277F (unknown), RW 0x0076FF27 (unknown), RW 0x007713EE (unknown), RW 0x00771790 (unknown), RW 0x0077208A (unknown), RW 0x0077BC49 (unknown), RW 0x0078142F (unknown), RW 0x00793372 (ported), RW 0x0079350E (ported), RW 0x007987EE (ported), RW 0x00799F0F (unknown), RW 0x007A0ECD (ported), RW 0x007A7483 (ported), RW 0x007BFF62 (unknown), RW 0x007C1704 (unknown), RW 0x007C6C2B (unknown), RW 0x007C81B9 (unknown), RW 0x007C920F (unknown), RW 0x0083568D (unknown), RW 0x00835967 (unknown), RW 0x00861F97 (ported), RW 0x008620DD (ported), RW 0x008622E9 (ported), RW 0x0086C40D (ported), RW 0x00870E50 (ported), RW 0x00870F75 (ported), RW 0x00875FB2 (ported), RW 0x008D3167 (unknown), RW 0x00949BD3 (unknown), RW 0x0094D4BC (unknown), RW 0x0098F5A5 (unknown), RW 0x0098FE62 (unknown), RW 0x00990B5B (unknown). Classes: AIAttackAreaState, AIAttackSquadState, AIGoingIdleState, AIGroup, HordeContain, PlayerList, StanceTemplate, StancesBehavior, TeamFactory, ThingFactory.
Candidates: 0 none/partial functions; 39 unknown status.
Red buttons: none linked.
Unused fields: HordeContain.LivingWorldOverloadTemplate, HordeContain.RanksToJustFreeWhenAttacking.
Existing task matches: #752 Port 316: Model condition flushes on the creation paths and before bone queries (S-1583); #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate.

### G380: RW 0x00814EFA

Stops: S-1333, S-1480, S-1911, S-2342. Kinds: code.
Functions: RW 0x004A1130 (unknown), RW 0x0060E8DB (ported), RW 0x00630078 (unknown), RW 0x006DD947 (unknown), RW 0x007018E0 (unknown), RW 0x007019A0 (unknown), RW 0x00701F81 (unknown), RW 0x00702689 (unknown), RW 0x00702E2B (unknown), RW 0x007052BE (unknown), RW 0x00718D93 (unknown), RW 0x007190B4 (unknown), RW 0x0071928A (unknown), RW 0x007234EC (unknown), RW 0x007237F8 (unknown), RW 0x00779F20 (unknown), RW 0x007F2440 (unknown), RW 0x007F2A76 (unknown), RW 0x0081487B (unknown), RW 0x00814E3A (unknown), RW 0x00814EFA (unknown), RW 0x00845830 (unknown), RW 0x0097572C (unknown), RW 0x00975942 (unknown), RW 0x00975C3A (unknown), RW 0x00975D3F (unknown), RW 0x00975E04 (unknown), RW 0x00975F23 (unknown), RW 0x0097606B (unknown), RW 0x0097640C (unknown), RW 0x00976497 (unknown), RW 0x00976877 (unknown), RW 0x00976B49 (unknown), RW 0x00976C4D (unknown), RW 0x00976D64 (unknown), RW 0x00976F59 (unknown), RW 0x009770CF (unknown), RW 0x009772E6 (unknown), RW 0x00977542 (unknown), RW 0x0097759F (unknown). Classes: AptMapPreview, AptMenuPlayer, AptRenderList, GameWindow, LivingWorld, StartingRestriction, VP6MovieStream, VictoryType.
Candidates: 0 none/partial functions; 39 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G381: RW 0x008A1B9F

Stops: S-1180, S-202, S-203, S-2452, S-486, S-7294, S-7323, S-855, S-921. Kinds: code, project.
Functions: RW 0x0044DE58 (unknown), RW 0x00462637 (unknown), RW 0x0048B7B1 (unknown), RW 0x004E6FB6 (unknown), RW 0x004E7401 (unknown), RW 0x004E85EE (unknown), RW 0x004E9E4E (unknown), RW 0x004EA005 (unknown), RW 0x004EAAA7 (unknown), RW 0x004EACB0 (unknown), RW 0x004EC40E (unknown), RW 0x004EC7B6 (unknown), RW 0x004EC8EB (unknown), RW 0x004EDA0B (unknown), RW 0x005DD1F3 (unknown), RW 0x00608397 (unknown), RW 0x00608504 (unknown), RW 0x006092A9 (ported), RW 0x0060A1D5 (unknown), RW 0x0060C1C9 (ported), RW 0x00657A6E (unknown), RW 0x00659CE0 (unknown), RW 0x0066F5E5 (ported), RW 0x0067B4B7 (ported), RW 0x0067D08D (unknown), RW 0x0067D4CE (ported), RW 0x0067E879 (ported), RW 0x0067F6F0 (ported), RW 0x0067F738 (unknown), RW 0x00684412 (ported), RW 0x00687C4C (ported), RW 0x0068B749 (ported), RW 0x0068F383 (ported), RW 0x006917BB (unknown), RW 0x0069264D (unknown), RW 0x006938BD (ported), RW 0x006939DF (ported), RW 0x00693A1A (ported), RW 0x00694BF8 (ported), RW 0x00698F06 (ported), RW 0x00699368 (ported), RW 0x0069954A (ported), RW 0x006996DC (unknown), RW 0x006A950B (ported), RW 0x006AA450 (unknown), RW 0x006ABD0B (ported), RW 0x006ABDC2 (ported), RW 0x006AF269 (unknown), RW 0x006AF3C8 (ported), RW 0x006AF484 (ported), RW 0x006B0239 (unknown), RW 0x006B29ED (unknown), RW 0x006D5B23 (unknown), RW 0x006D5BD9 (unknown), RW 0x006D881F (ported), RW 0x006D8E2B (unknown), RW 0x006D93EB (ported), RW 0x006D98FA (ported), RW 0x006D9A71 (unknown), RW 0x006D9B4D (ported), RW 0x006D9BF3 (ported), RW 0x006E2D92 (unknown), RW 0x006E2E18 (unknown), RW 0x006E85E9 (ported), RW 0x006EF225 (unknown), RW 0x006F85A6 (unknown), RW 0x006FE456 (unknown), RW 0x00715C81 (unknown), RW 0x0071C7E4 (ported), RW 0x0071C81A (unknown), RW 0x0071E1E7 (unknown), RW 0x0071EBDA (unknown), RW 0x0071F207 (unknown), RW 0x0071FC08 (unknown), RW 0x0073A429 (unknown), RW 0x0073B217 (unknown), RW 0x0073C39E (ported), RW 0x00758DE5 (unknown), RW 0x00759A1C (ported), RW 0x0075A243 (ported), RW 0x007767A9 (ported), RW 0x00779544 (unknown), RW 0x00779A3D (unknown), RW 0x00780EB6 (unknown), RW 0x007812B2 (unknown), RW 0x007815E0 (unknown), RW 0x00782214 (unknown), RW 0x00792DBD (ported), RW 0x007996D3 (unknown), RW 0x0079A5EC (ported), RW 0x0079D322 (ported), RW 0x0079D833 (ported), RW 0x007A07D8 (unknown), RW 0x007A0ECD (ported), RW 0x007A7D7E (ported), RW 0x007BC221 (unknown), RW 0x007BC250 (unknown), RW 0x007BD6A8 (unknown), RW 0x007BED64 (unknown), RW 0x007BEE14 (unknown), RW 0x007C0B61 (unknown), RW 0x007C2AAC (unknown), RW 0x007C37C9 (unknown), RW 0x007C3F9B (unknown), RW 0x007C595A (unknown), RW 0x007C7D12 (unknown), RW 0x007CAFA5 (unknown), RW 0x007D01C0 (unknown), RW 0x007D5270 (unknown), RW 0x007E4866 (unknown), RW 0x007E4F6A (unknown), RW 0x007E5142 (unknown), RW 0x007E5BF4 (unknown), RW 0x007E64CD (unknown), RW 0x007E9356 (unknown), RW 0x007E9F44 (unknown), RW 0x007EB7CD (unknown), RW 0x0081ACD7 (ported), RW 0x0086EC2D (ported), RW 0x0087566B (ported), RW 0x0088459B (unknown), RW 0x0088D993 (unknown), RW 0x0089B7AE (unknown), RW 0x008A03A0 (ported), RW 0x008A07E0 (unknown), RW 0x008A0FDA (ported), RW 0x008A1666 (ported), RW 0x008A171F (unknown), RW 0x008A198B (ported), RW 0x008A1B9F (ported), RW 0x008A3111 (ported), RW 0x008A3AB4 (ported), RW 0x008A3DD5 (ported), RW 0x008B307E (ported), RW 0x008B3637 (unknown), RW 0x008B36B2 (unknown), RW 0x008B3744 (unknown), RW 0x008B38A2 (unknown), RW 0x008B3A35 (ported), RW 0x008B3C54 (unknown), RW 0x008B4271 (none), RW 0x008B4E75 (ported), RW 0x008C3FA3 (ported), RW 0x008C553F (ported), RW 0x008C7171 (ported), RW 0x008C7BF2 (ported), RW 0x008C7FA2 (ported), RW 0x008C88A0 (ported), RW 0x008C88CF (ported), RW 0x008C8ADA (ported), RW 0x008C8CE4 (ported), RW 0x008C8FF5 (ported), RW 0x008C918D (ported), RW 0x008C92CE (ported), RW 0x008C93A5 (ported), RW 0x008CBFCF (ported), RW 0x008CC0CC (ported), RW 0x008CDCE9 (ported), RW 0x008F7599 (unknown). Classes: ActiveBody, BuildAssistant, CastleBehavior, ControlBar, DevastateSpecialPower, GlobalData, HordeContain, InGameHud, InGameUI, LifetimeUpdate, MiscAudio, PlayerUpgradeSpecialPower, ProductionQueueHordeContain, ProductionSpeedBonus, ProductionUpdate, RespawnBody, RespawnUpdate, ScavengerSpecialPower, ScriptActions, ScriptConditions, ScriptEngine, TerrainLogic, TerrainTrees, ThingTemplate, UntamedAllegianceSpecialPower, UpgradeTemplate.
Candidates: 1 none/partial functions; 88 unknown status.
Red buttons: Command_SpecialAbilityWulfgarRufderPlnderer.
Unused fields: ActiveBody.EnteringDamagedTransitionTime, ActiveBody.ReallyDamagedAttributeModifier, CastleBehavior.CrewPrepareFX, CastleBehavior.CrewPrepareInterval, CastleBehavior.CrewReleaseFX, HordeContain.LivingWorldOverloadTemplate, HordeContain.RanksToJustFreeWhenAttacking, RespawnBody.DamagedAttributeModifier, RespawnBody.ReallyDamagedAttributeModifier, RespawnUpdate.DeathAnimationTime.
Existing task matches: #690 Port 298: RespawnUpdate вЂ” hero respawn: its unread DeathAnimationTime and 4 uncited own functions; #688 Port 296: Castles вЂ” CastleBehavior, CastleMemberBehavior and FoundationAIUpdate: the uncited own functions (27) and their five unread fields; #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate; #567 Port 243: Body damage states: ActiveBody EnteringDamagedTransitionTime and the Damaged / ReallyDamaged attribute modifiers (ActiveBody, RespawnBody).

### G382: RW 0x00667290

Stops: S-4942. Kinds: code.
Functions: RW 0x00667290 (ported), RW 0x0070E013 (ported), RW 0x007449D4 (unknown), RW 0x0075638B (unknown), RW 0x007BD7A7 (unknown), RW 0x007BD7FC (unknown), RW 0x007C74AA (unknown), RW 0x007C7617 (unknown), RW 0x007C77EB (unknown), RW 0x007C821E (unknown), RW 0x007E5760 (unknown), RW 0x0085D216 (ported), RW 0x00893C80 (ported), RW 0x008CA5C2 (ported), RW 0x008CAA12 (ported), RW 0x008CAC43 (ported), RW 0x008CACC6 (ported), RW 0x00991660 (unknown). Classes: SiegeAIUpdate, SiegeDeployHordeSpecialPower, SiegeDeploySpecialPower, SiegeDockingBehavior, StopSpecialPower.
Candidates: 0 none/partial functions; 10 unknown status.
Red buttons: none linked.
Unused fields: SiegeDeploySpecialPower.InitiateSound.
Existing task matches: #607 Port 259: Horde-level powers: SplitHordeSpecialPower, HordeDispatchSpecialPower, SiegeDeployHordeSpecialPower, SiegeDeploySpecialPower.

### G383: RW 0x00648CF1

Stops: S-1064, S-1339. Kinds: code.
Functions: RW 0x00625E36 (unknown), RW 0x00625FC2 (unknown), RW 0x00648CF1 (unknown), RW 0x006497EC (unknown), RW 0x006B23FB (unknown), RW 0x006B5E3E (unknown), RW 0x00779B67 (unknown), RW 0x008385A3 (unknown), RW 0x008445C9 (unknown), RW 0x009001FB (unknown), RW 0x0090313E (unknown), RW 0x00921841 (unknown), RW 0x00921904 (unknown), RW 0x00921D5B (unknown), RW 0x009220DE (unknown), RW 0x009222B2 (unknown), RW 0x0097E137 (unknown), RW 0x0097EFB2 (unknown), RW 0x0098430E (unknown), RW 0x00984640 (unknown). Classes: AptLanLobby, AptQuitMenu, GameWorld, LivingWorld.
Candidates: 0 none/partial functions; 20 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G384: RW 0x00807A81

Stops: S-207, S-2702, S-4612. Kinds: code.
Functions: RW 0x005FF323 (unknown), RW 0x005FF4D7 (unknown), RW 0x0068C3E6 (ported), RW 0x0068DAE6 (ported), RW 0x0069194E (unknown), RW 0x00691B15 (unknown), RW 0x006A8803 (unknown), RW 0x006AC927 (unknown), RW 0x0074040D (unknown), RW 0x00740483 (unknown), RW 0x007404E0 (unknown), RW 0x00793EEB (unknown), RW 0x00793F31 (unknown), RW 0x00794F65 (unknown), RW 0x0080774E (unknown), RW 0x00807A00 (unknown), RW 0x00807A81 (unknown), RW 0x00856501 (ported), RW 0x0085657E (ported), RW 0x00857074 (ported), RW 0x008BC992 (ported), RW 0x008EBA26 (unknown), RW 0x008EBC61 (unknown), RW 0x008F99B8 (none), RW 0x008F9ADD (unknown), RW 0x008F9BBF (unknown), RW 0x0092BF34 (unknown), RW 0x0092C265 (unknown), RW 0x0093099D (unknown), RW 0x00930DC4 (unknown), RW 0x0093FF0A (unknown), RW 0x00940159 (unknown), RW 0x009402E7 (unknown), RW 0x009744E4 (unknown), RW 0x00974795 (unknown), RW 0x00974EEB (unknown). Classes: CommandPoints, GettingBuiltBehavior, InGameUI, ProductionPrerequisite, ScienceStore, SpawnUnitBehavior, UnitHelpSource.
Candidates: 1 none/partial functions; 29 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: #687 Port 295: GettingBuiltBehavior вЂ” the uncited own functions of a building under construction (19).

### G385: RW 0x00667174

Stops: S-1027, S-363, S-7181. Kinds: code.
Functions: RW 0x005E821A (ported), RW 0x00625759 (ported), RW 0x00662EB1 (ported), RW 0x00662FC8 (ported), RW 0x00663C6B (unknown), RW 0x0066498A (ported), RW 0x00664D5D (unknown), RW 0x00664D7E (ported), RW 0x00667174 (ported), RW 0x0068E43B (ported), RW 0x0068F383 (ported), RW 0x00690B8C (partial), RW 0x00693919 (ported), RW 0x00699282 (ported), RW 0x006CB6EA (ported), RW 0x006CC79C (unknown), RW 0x006EF225 (unknown), RW 0x006FCE1A (unknown), RW 0x00738481 (unknown), RW 0x00740C97 (unknown), RW 0x00741675 (ported), RW 0x0074168E (ported), RW 0x00741D3E (unknown), RW 0x00743E6C (unknown), RW 0x00745FBB (unknown), RW 0x00748E46 (unknown), RW 0x00749693 (unknown), RW 0x0074983C (unknown), RW 0x0074D68C (unknown), RW 0x0074D6EA (unknown), RW 0x0074D7C7 (unknown), RW 0x0074D826 (unknown), RW 0x0074E3E7 (unknown), RW 0x0075187A (unknown), RW 0x00751D34 (unknown), RW 0x00751DA9 (ported), RW 0x00751DFC (unknown), RW 0x00751E3A (unknown), RW 0x0075307D (unknown), RW 0x0075464E (unknown), RW 0x007546DD (unknown), RW 0x007547D5 (unknown), RW 0x0075638B (unknown), RW 0x0075681A (unknown), RW 0x00852E2A (ported), RW 0x0085EA5F (ported), RW 0x0085EF6A (ported), RW 0x0085F28A (ported), RW 0x0085F85B (ported), RW 0x0085FDB7 (ported), RW 0x0087A321 (ported), RW 0x008AAF8E (unknown), RW 0x008B4FA1 (unknown), RW 0x008C57A1 (ported). Classes: AIRampageState, AIState, AIUpdateInterface, CombatState, DelayedDeathBody, GeometryInfo, HordeTransportContain.
Candidates: 1 none/partial functions; 30 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: #460 Port 193: Fear: the AI states 32 AVOID_SCARER and 57 are not ported (S-1027).

### G386: RW 0x005E39CE

Stops: S-1101, S-4321, S-785. Kinds: code.
Functions: RW 0x004B12E8 (ported), RW 0x005E39CE (ported), RW 0x005E83FD (unknown), RW 0x0066276B (ported), RW 0x00662B3C (ported), RW 0x0066434E (ported), RW 0x006643FC (ported), RW 0x00664579 (ported), RW 0x006645F3 (ported), RW 0x0066468F (ported), RW 0x00664767 (ported), RW 0x00665C33 (ported), RW 0x006668CA (ported), RW 0x0066831A (ported), RW 0x00668E94 (ported), RW 0x0066C66E (ported), RW 0x0066C6E3 (unknown), RW 0x0066D85F (ported), RW 0x0066DA5F (ported), RW 0x0067C81E (unknown), RW 0x0068D6A6 (ported), RW 0x00690E97 (ported), RW 0x006F503B (unknown), RW 0x006F53AF (unknown), RW 0x006FB231 (unknown), RW 0x006FE456 (unknown), RW 0x0070D41A (ported), RW 0x00741675 (ported), RW 0x0074168E (ported), RW 0x007433E3 (unknown), RW 0x00743437 (unknown), RW 0x0074352D (unknown), RW 0x0074356A (unknown), RW 0x00747C37 (unknown), RW 0x00747CD1 (unknown), RW 0x00747D04 (unknown), RW 0x00748E46 (unknown), RW 0x00749497 (unknown), RW 0x0074949C (unknown), RW 0x007515E1 (unknown), RW 0x00751A8A (unknown), RW 0x00752B0F (unknown), RW 0x00753755 (unknown), RW 0x00755572 (unknown), RW 0x00756067 (unknown), RW 0x00756452 (unknown), RW 0x0076FB64 (unknown), RW 0x007725DF (unknown), RW 0x00774EDF (unknown), RW 0x0077AC36 (unknown), RW 0x0077AD4D (unknown), RW 0x007FEAC1 (ported), RW 0x0082C8A7 (unknown), RW 0x0082CBD5 (ported), RW 0x0082CFD4 (unknown), RW 0x008BD372 (ported), RW 0x008C1D75 (ported). Classes: AIEnterState, AIExitState, AIHordeEnterState, AIMoveToPositionAndEnterState, AIMoveToState, AIWorld, AODCrushCollide, ActionManager, CivilianSpawnCollide, HordeMemberCollide, InactiveBody, ObjectWeapons, Rva001E4147, SquishCollide.
Candidates: 0 none/partial functions; 31 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G387: RW 0x0096F53C

Stops: S-3803, S-7335. Kinds: code, project.
Functions: RW 0x00449681 (ported), RW 0x0068BDA5 (ported), RW 0x00696FD2 (ported), RW 0x006A8803 (unknown), RW 0x006C106C (unknown), RW 0x0075CE47 (unknown), RW 0x0076FF27 (unknown), RW 0x0076FF4B (unknown), RW 0x00861E6E (ported), RW 0x0092FF5C (unknown), RW 0x0093023D (unknown), RW 0x00944534 (unknown), RW 0x00949CA7 (unknown), RW 0x0096F33E (unknown), RW 0x0096F53C (unknown), RW 0x0096F6E7 (unknown), RW 0x0096F7B7 (unknown), RW 0x0096F85A (unknown), RW 0x0096F890 (unknown), RW 0x0096FAC6 (unknown), RW 0x0096FCCB (unknown), RW 0x0096FDFD (unknown), RW 0x009D3380 (unknown), RW 0x009D353B (unknown), RW 0x009D72DB (unknown). Classes: AIGroup, ChangeStanceGroupOrder, CommandButton, ControlBar, InGameCommandButtonSubMenu, StancesBehavior.
Candidates: 0 none/partial functions; 21 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G388: RW 0x006CC915

Stops: S-364, S-582, S-680, S-787. Kinds: code.
Functions: RW 0x00405553 (ported), RW 0x005D893C (unknown), RW 0x00660D2C (ported), RW 0x00660EC6 (ported), RW 0x0066352C (ported), RW 0x006639CF (unknown), RW 0x00667C76 (unknown), RW 0x0067D7EC (partial), RW 0x0068B411 (ported), RW 0x0068C5AF (ported), RW 0x0068C9AC (unknown), RW 0x0068FB63 (ported), RW 0x00690BD2 (ported), RW 0x00694C0D (ported), RW 0x006CB7BD (unknown), RW 0x006CB85A (ported), RW 0x006CC07C (ported), RW 0x006CC653 (ported), RW 0x006CC79C (unknown), RW 0x006CC915 (unknown), RW 0x006CF3AE (unknown), RW 0x006CF4D6 (ported), RW 0x006CF590 (ported), RW 0x006E82B3 (ported), RW 0x006E8523 (unknown), RW 0x006E9070 (unknown), RW 0x006EA0EC (unknown), RW 0x006EAD32 (unknown), RW 0x006ED798 (unknown), RW 0x006EE6DD (unknown), RW 0x006EFE9F (unknown), RW 0x006F3082 (unknown), RW 0x006F37B3 (unknown), RW 0x006F5BB0 (ported), RW 0x006F61BD (unknown), RW 0x006F807D (unknown), RW 0x006FB754 (unknown), RW 0x006FF7FA (ported), RW 0x00740E39 (unknown), RW 0x00741450 (unknown), RW 0x00744B25 (unknown), RW 0x00744B71 (unknown), RW 0x007463E8 (unknown), RW 0x00746579 (unknown), RW 0x00746DB6 (unknown), RW 0x007471CF (unknown), RW 0x0074A594 (unknown), RW 0x0074B001 (unknown), RW 0x0074B1D6 (unknown), RW 0x0074C258 (unknown), RW 0x0074C4A7 (unknown), RW 0x0074CED6 (unknown), RW 0x0074D46D (unknown), RW 0x0074EF4B (unknown), RW 0x0074F599 (unknown), RW 0x00792973 (unknown), RW 0x0085F08A (ported), RW 0x00872EFC (ported), RW 0x00876FC4 (ported), RW 0x008C9D06 (unknown), RW 0x008DBDFF (unknown), RW 0x0090E28C (unknown), RW 0x00938939 (unknown). Classes: AIAttackMeleeApproachState, AIAttackMeleeEngageState, AIMeleeReAcquireState, DrawableLaunchBones, HordeContain, ObjectWeapons, TargetFinder, WeaponBonus.
Candidates: 1 none/partial functions; 42 unknown status.
Red buttons: none linked.
Unused fields: HordeContain.LivingWorldOverloadTemplate, HordeContain.RanksToJustFreeWhenAttacking.
Existing task matches: #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate.

### G389: RW 0x0091988F

Stops: S-1123, S-1912, S-2703. Kinds: code.
Functions: RW 0x005424E0 (unknown), RW 0x006224C5 (unknown), RW 0x00622C88 (unknown), RW 0x006230B6 (unknown), RW 0x0071FC08 (unknown), RW 0x0075DE01 (unknown), RW 0x0075E1D3 (unknown), RW 0x008075E4 (unknown), RW 0x00807848 (unknown), RW 0x00818AB8 (unknown), RW 0x0082289A (unknown), RW 0x008232ED (unknown), RW 0x0082379A (unknown), RW 0x00847152 (unknown), RW 0x008E8CED (unknown), RW 0x008E90DD (unknown), RW 0x00918FF8 (unknown), RW 0x009190C0 (unknown), RW 0x009190F0 (unknown), RW 0x00919158 (unknown), RW 0x0091934F (unknown), RW 0x009195E1 (unknown), RW 0x0091988F (unknown), RW 0x0091ED91 (unknown), RW 0x00921904 (unknown), RW 0x009222B2 (unknown), RW 0x00926BE3 (unknown), RW 0x009282E3 (unknown), RW 0x009286D7 (unknown), RW 0x0092E229 (unknown), RW 0x0092E462 (unknown), RW 0x0092E570 (unknown), RW 0x009F9F16 (ported), RW 0x00A0CB16 (ported), RW 0x00B5442B (unknown). Classes: AptDisconnectScreen, ControlBar, InGameUI, UnitHelpSource, WindowManager.
Candidates: 0 none/partial functions; 33 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G390: RW 0x004A4CD3

Stops: S-2701. Kinds: code.
Functions: RW 0x004A4CD3 (unknown), RW 0x004A5112 (unknown), RW 0x00715A84 (unknown), RW 0x00729D9A (unknown), RW 0x009457FE (unknown), RW 0x009459D1 (unknown), RW 0x009D5BDC (unknown), RW 0x009D5CCE (unknown), RW 0x009D6250 (unknown), RW 0x009D6320 (unknown), RW 0x009D6521 (unknown). Classes: ControlBar, GlobalData.
Candidates: 0 none/partial functions; 11 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G391: RW 0x0074E083

Stops: S-4463. Kinds: code.
Functions: RW 0x00668E94 (ported), RW 0x0066BB9D (ported), RW 0x0066BC68 (ported), RW 0x0068C933 (ported), RW 0x0074B85F (unknown), RW 0x0074DF88 (unknown), RW 0x0074E083 (unknown), RW 0x0074FC9B (unknown), RW 0x0074FD39 (unknown), RW 0x0074FF49 (unknown), RW 0x007500F0 (unknown), RW 0x007504E0 (unknown), RW 0x00752FAC (none), RW 0x00753755 (unknown), RW 0x0077122F (unknown). Classes: AIAttackState, AIFollowPathAsTeamState, AIMoveToState.
Candidates: 1 none/partial functions; 10 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G392: RW 0x00492C35

Stops: S-4890. Kinds: code.
Functions: RW 0x00492C35 (unknown), RW 0x00492CE9 (unknown), RW 0x00492D51 (unknown), RW 0x00492E5F (unknown), RW 0x00492F0F (unknown), RW 0x00493547 (unknown), RW 0x00493563 (unknown), RW 0x004936CB (unknown), RW 0x0049405A (unknown), RW 0x004943E1 (unknown), RW 0x00600ADE (unknown). Classes: GodotWeather, SnowManager, WeatherSetting, WorldHeightMap.
Candidates: 0 none/partial functions; 11 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G393: RW 0x0078419D

Stops: S-5150. Kinds: code.
Functions: RW 0x0069C4B3 (unknown), RW 0x006A0BC0 (unknown), RW 0x006A1F4D (unknown), RW 0x0078419D (unknown), RW 0x0078A594 (unknown), RW 0x0078ABD1 (unknown), RW 0x008D3AF1 (unknown), RW 0x008D54D1 (unknown), RW 0x008ECD8E (unknown), RW 0x008ECF3E (unknown), RW 0x008ED256 (unknown). Classes: AptInGameChat, InGameChat, InGameUI.
Candidates: 0 none/partial functions; 11 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G394: RW 0x004E70F5

Stops: S-5260, S-7215. Kinds: code.
Functions: RW 0x004E6C74 (unknown), RW 0x004E70F5 (unknown), RW 0x004E79C1 (unknown), RW 0x004E7D26 (unknown), RW 0x004E837E (unknown), RW 0x004E8B8F (unknown), RW 0x004E96D9 (unknown), RW 0x004E9E4E (unknown), RW 0x004EA005 (unknown), RW 0x004EA068 (unknown), RW 0x004EAD92 (unknown), RW 0x004EC40E (unknown), RW 0x004ED508 (unknown), RW 0x004EEB15 (unknown), RW 0x005F0EE6 (unknown), RW 0x0067F4C2 (unknown), RW 0x0067F6F0 (ported), RW 0x0068449B (ported), RW 0x00684BCA (ported), RW 0x006854FD (unknown), RW 0x006858BE (unknown), RW 0x006886E0 (unknown), RW 0x0069264D (unknown), RW 0x00695070 (ported), RW 0x00853EDF (ported), RW 0x0088D44F (ported), RW 0x0088EE6E (ported), RW 0x0089958F (ported), RW 0x008C9629 (ported), RW 0x009B90D6 (unknown), RW 0x009B92F6 (unknown), RW 0x00AD2CE0 (ported). Classes: AutoPickUpUpdate, FireSpreadUpdate, GlobalData, ObjectCreationList.
Candidates: 0 none/partial functions; 22 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G395: RW 0x00450C12

Stops: S-7760. Kinds: code.
Functions: RW 0x00450C12 (unknown), RW 0x004510B8 (unknown), RW 0x00453F01 (unknown), RW 0x00455552 (unknown), RW 0x00458825 (unknown), RW 0x0045E6FF (unknown), RW 0x00460D96 (unknown), RW 0x0046139D (unknown), RW 0x00461DA6 (unknown), RW 0x00660539 (unknown), RW 0x00660893 (unknown). Classes: AudioSettings.
Candidates: 0 none/partial functions; 11 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G396: RW 0x006639CF

Stops: S-786. Kinds: code.
Functions: RW 0x006639CF (unknown), RW 0x00668E94 (ported), RW 0x00680A75 (ported), RW 0x0068B3AB (unknown), RW 0x006E9070 (unknown), RW 0x006EBE89 (unknown), RW 0x006EEBC1 (unknown), RW 0x0074449D (unknown), RW 0x00744B71 (unknown), RW 0x00746DB6 (unknown), RW 0x008E28AC (unknown), RW 0x009344DE (unknown), RW 0x00938939 (unknown). Classes: AIAttackMeleeApproachState.
Candidates: 0 none/partial functions; 11 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G397: RW 0x00601C62

Stops: S-1950, S-2481, S-2522, S-3402. Kinds: code.
Functions: RW 0x00446330 (unknown), RW 0x004B2A9B (ported), RW 0x0050977A (unknown), RW 0x00509F7D (unknown), RW 0x0050B4EB (none), RW 0x0050C23A (unknown), RW 0x0050C485 (unknown), RW 0x0050C980 (unknown), RW 0x0050CAA8 (unknown), RW 0x0050CDD6 (unknown), RW 0x0050D46A (unknown), RW 0x0050D92C (unknown), RW 0x0050DCB3 (unknown), RW 0x00516A30 (unknown), RW 0x00516AB0 (unknown), RW 0x005372AB (unknown), RW 0x006019D1 (unknown), RW 0x00601BBD (unknown), RW 0x00601C62 (unknown), RW 0x006020B4 (unknown), RW 0x00602165 (unknown), RW 0x0060252D (unknown), RW 0x00602C03 (unknown), RW 0x00646771 (unknown), RW 0x00671FD5 (unknown), RW 0x006721CC (unknown), RW 0x006723C3 (unknown), RW 0x00672943 (unknown), RW 0x00672973 (unknown), RW 0x006729A3 (ported), RW 0x006729E9 (none), RW 0x00673F6D (unknown), RW 0x0067580E (unknown), RW 0x00676346 (unknown), RW 0x00677CF9 (unknown), RW 0x006780DF (unknown), RW 0x00679129 (unknown), RW 0x0067939F (unknown), RW 0x006799E3 (unknown), RW 0x0068B6F5 (ported), RW 0x0068B749 (ported), RW 0x0068DB35 (ported), RW 0x006916CD (ported), RW 0x0073228B (ported), RW 0x007326CC (ported), RW 0x0075C1A9 (unknown), RW 0x0079DA9D (unknown), RW 0x0086F4EE (ported), RW 0x0091F175 (unknown), RW 0x0091F2AB (unknown), RW 0x0091F3B3 (unknown), RW 0x00920000 (unknown), RW 0x00920E0F (unknown), RW 0x00920F34 (unknown). Classes: AptOptionsScreen, DrawGroupInfo, DrawableIconUI, ExperienceTracker, FXPlayer, GameLODManager, GameWorld, GlobalData, HordeContain, RadiusDecal, SelectionDecal, StaticLODState, W3DMaterialFactory, W3DShaderKey.
Candidates: 2 none/partial functions; 43 unknown status.
Red buttons: none linked.
Unused fields: HordeContain.LivingWorldOverloadTemplate, HordeContain.RanksToJustFreeWhenAttacking.
Existing task matches: #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate.

### G398: RW 0x006FC18E

Stops: S-4040, S-4858. Kinds: code.
Functions: RW 0x005E98D6 (ported), RW 0x005FF924 (ported), RW 0x00662E01 (unknown), RW 0x006668CA (ported), RW 0x0068D30E (ported), RW 0x006AA7B2 (ported), RW 0x006AAC66 (ported), RW 0x006F9829 (unknown), RW 0x006FC18E (unknown), RW 0x006FDE38 (unknown), RW 0x0074B77D (unknown), RW 0x00756DC6 (ported), RW 0x0076958A (unknown), RW 0x007695A7 (unknown), RW 0x007695C2 (unknown), RW 0x0076A0E6 (unknown), RW 0x0076A1F5 (unknown), RW 0x0076A2B8 (unknown), RW 0x0076A320 (unknown), RW 0x0076A451 (unknown), RW 0x0076A496 (unknown), RW 0x0076A6CC (unknown), RW 0x0076AB4E (ported), RW 0x0076ABC1 (unknown), RW 0x0076AC9F (unknown), RW 0x0076B14A (unknown), RW 0x0076B228 (unknown), RW 0x0076B491 (ported), RW 0x0076B543 (ported), RW 0x0076B559 (ported), RW 0x0076B56B (ported), RW 0x0076B668 (unknown), RW 0x0076CB71 (ported), RW 0x0076CDFB (ported), RW 0x0076D25F (ported), RW 0x0076D3B1 (ported), RW 0x0076D659 (ported), RW 0x0076D778 (ported), RW 0x0076D84B (unknown), RW 0x0076D995 (ported), RW 0x0077131B (unknown), RW 0x007FEAC1 (ported), RW 0x0085393B (ported), RW 0x0093AD93 (unknown). Classes: AIAttackState, AIGiantBirdFollowThruState, AIGiantBirdSwoopState, AIGoingIdleState, AIOrdersManager, AIUpdateInterface, AIWorld, CollideModuleInterface, CreateObjectDie, GiantBirdAIUpdate, GiantBirdAttackMoveToState, GiantBirdFollowPathState, GiantBirdFollowWaypointPathState, GiantBirdNormalFlightState, SpecialAbilityUpdate.
Candidates: 0 none/partial functions; 23 unknown status.
Red buttons: Command_AngmarTrollChargeAttack, Command_SpecialAbilityLoneRangerSummonFinal, Command_SpecialAbilityNaturmagierErntesegen, Command_ThranduilKnigDsterwalds.
Unused fields: CreateObjectDie.DebrisPortionOfSelf.
Existing task matches: none.

### G399: RW 0x006C6743

Stops: S-1302, S-418. Kinds: code.
Functions: RW 0x00697C09 (ported), RW 0x006A99CB (ported), RW 0x006A9D9C (ported), RW 0x006C6743 (unknown), RW 0x006C6F01 (unknown), RW 0x006C720A (unknown), RW 0x006C7344 (unknown), RW 0x006F5BB0 (ported), RW 0x006F5F65 (ported), RW 0x007A2C96 (unknown), RW 0x007A581D (unknown), RW 0x007A6295 (unknown), RW 0x007EDC2F (unknown), RW 0x007EE166 (ported), RW 0x00853EDF (ported), RW 0x008B7C68 (ported), RW 0x008E3CF3 (unknown), RW 0x008E3EF8 (ported), RW 0x008E4042 (unknown), RW 0x008E42C8 (ported), RW 0x008E4456 (ported), RW 0x008E4475 (ported), RW 0x008E4591 (ported), RW 0x008E46D9 (unknown), RW 0x008F0D54 (unknown), RW 0x0090B2A0 (unknown), RW 0x0090B342 (unknown), RW 0x0099E63B (unknown), RW 0x009A24A6 (unknown), RW 0x009A24FC (unknown), RW 0x009A26A1 (unknown), RW 0x009A2CA5 (unknown), RW 0x009A2E06 (unknown), RW 0x009A3040 (unknown), RW 0x009A3366 (unknown), RW 0x009B4EF0 (unknown), RW 0x009EF9AF (unknown). Classes: AIPositionSampler, AIStatCollector, CommandSetUpgrade.
Candidates: 0 none/partial functions; 24 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: #766 Local 100: upgrade modules: CommandSetUpgrade (515 modules), CostModifierUpgrade, CastleUpgrade, GiveUpgradeUpdate, AudioLoopUpgrade, WallUpgradeUpdate, LocomotorSetUpgrade: 27 uncited own functions.

### G400: RW 0x007592E1

Stops: S-6102. Kinds: code.
Functions: RW 0x007592E1 (unknown), RW 0x007593A4 (unknown), RW 0x0075985D (unknown), RW 0x0075A344 (unknown), RW 0x007BC3A0 (unknown), RW 0x007BD4C1 (unknown), RW 0x007BD4F3 (unknown), RW 0x007BD524 (unknown), RW 0x007C0BCF (unknown), RW 0x00B4DBE0 (unknown), RW 0x00B4DCA0 (unknown), RW 0x00B4FFF0 (unknown). Classes: none.
Candidates: 0 none/partial functions; 12 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G401: W3DLaserDraw

Stops: S-7218. Kinds: code.
Functions: RW 0x00464159 (unknown), RW 0x004C8A6C (ported), RW 0x004C917B (unknown), RW 0x004C9450 (unknown), RW 0x0055DB80 (none), RW 0x0055DBE0 (none), RW 0x0055DC20 (none), RW 0x0055DCE0 (none), RW 0x0055E9F0 (none), RW 0x0064D1FB (unknown), RW 0x0064D233 (unknown), RW 0x00764B43 (unknown), RW 0x00764C4D (ported), RW 0x00764E88 (ported), RW 0x007651B1 (ported), RW 0x007654A8 (unknown). Classes: LaserUpdate, SegmentedLineClass, W3DLaserDraw, W3DLaserDrawState.
Candidates: 5 none/partial functions; 7 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G402: RW 0x00698F06

Stops: S-1329, S-1940, S-709, S-7233. Kinds: code.
Functions: RW 0x00615C01 (unknown), RW 0x006164D7 (unknown), RW 0x00616DA6 (unknown), RW 0x006265D2 (unknown), RW 0x0062679C (unknown), RW 0x00629F34 (unknown), RW 0x0062AC17 (unknown), RW 0x0062BBAB (ported), RW 0x006709A2 (unknown), RW 0x00672DFB (unknown), RW 0x0067561E (ported), RW 0x00678C69 (unknown), RW 0x00678D8B (unknown), RW 0x00678F54 (unknown), RW 0x0067B4B7 (ported), RW 0x0068B581 (ported), RW 0x0068B678 (ported), RW 0x0068BE59 (ported), RW 0x0068C3E6 (ported), RW 0x0068E31F (ported), RW 0x00694CCC (ported), RW 0x00698F06 (ported), RW 0x006AAC4B (ported), RW 0x006ABABD (ported), RW 0x006B1146 (unknown), RW 0x006B16EC (unknown), RW 0x006B34EE (unknown), RW 0x006B851C (unknown), RW 0x006BC7F0 (unknown), RW 0x006BCC5C (unknown), RW 0x006D876A (ported), RW 0x006D9042 (ported), RW 0x006DCFDF (unknown), RW 0x006E301F (unknown), RW 0x006E42CF (unknown), RW 0x006E42DA (unknown), RW 0x006E4B21 (unknown), RW 0x006E4B3E (unknown), RW 0x006E861E (ported), RW 0x006F3082 (unknown), RW 0x006FB231 (unknown), RW 0x00780172 (unknown), RW 0x0078100E (unknown), RW 0x00781FAF (unknown), RW 0x00782214 (unknown), RW 0x007F32F4 (unknown), RW 0x00808A43 (unknown), RW 0x0080F4D3 (unknown), RW 0x0080F754 (unknown), RW 0x00810C95 (unknown), RW 0x00811D91 (unknown), RW 0x00811E1F (unknown), RW 0x008122C5 (unknown), RW 0x00812563 (unknown), RW 0x008125FC (unknown), RW 0x0085FED5 (ported), RW 0x00889FA7 (ported), RW 0x0088A0E2 (unknown), RW 0x0089C04B (unknown), RW 0x0089C12F (unknown), RW 0x0089C18E (unknown), RW 0x0089C35E (ported), RW 0x0089C6F5 (ported), RW 0x0089C759 (ported), RW 0x0089CA84 (ported), RW 0x0089CC29 (ported), RW 0x008B4D34 (ported), RW 0x008ED6DC (unknown), RW 0x008ED726 (unknown), RW 0x008ED772 (ported), RW 0x008EDA39 (unknown), RW 0x00936B7D (ported), RW 0x009A0426 (unknown), RW 0x00AD2CE0 (ported), RW 0x00B6D060 (unknown), RW 0x00B6D730 (unknown). Classes: AIGateUpdate, AIWorld, AptBannerUI, ArmySummarySystem, BannerUI, CastleMemberBehavior, FakePathfindPortalBehaviour, GateOpenAndCloseBehavior, KeepObjectDie, LivingWorldBattleTags.
Candidates: 0 none/partial functions; 50 unknown status.
Red buttons: none linked.
Unused fields: CastleMemberBehavior.StoreUpgradePrice.
Existing task matches: #688 Port 296: Castles вЂ” CastleBehavior, CastleMemberBehavior and FoundationAIUpdate: the uncited own functions (27) and their five unread fields.

### G403: RW 0x005EBAEE

Stops: S-1360, S-4903, S-4904. Kinds: code.
Functions: RW 0x005EAB0A (unknown), RW 0x005EAB8F (unknown), RW 0x005EADDE (unknown), RW 0x005EAE37 (unknown), RW 0x005EBAEE (unknown), RW 0x005EBD76 (unknown), RW 0x005EBF92 (unknown), RW 0x005EC4EC (unknown), RW 0x005EC5D1 (unknown), RW 0x006314CD (unknown), RW 0x006AE3CC (unknown), RW 0x006DE8F1 (unknown), RW 0x0073BFE8 (unknown), RW 0x00776294 (ported), RW 0x00780172 (unknown), RW 0x00780E30 (unknown), RW 0x007814A1 (unknown), RW 0x00781B17 (unknown), RW 0x00781FAF (unknown), RW 0x00782148 (unknown), RW 0x00782326 (unknown), RW 0x007BC7EC (unknown), RW 0x007BCF6C (unknown), RW 0x007BE0C0 (unknown), RW 0x007C45E0 (unknown), RW 0x007CAA30 (unknown), RW 0x007E6B50 (unknown), RW 0x007EB1C9 (unknown), RW 0x007EB69A (unknown), RW 0x0081B948 (ported), RW 0x0086D6FD (partial), RW 0x0091B825 (unknown), RW 0x0091BE05 (unknown), RW 0x0091C108 (unknown), RW 0x0091C1C8 (unknown), RW 0x0091C215 (unknown), RW 0x00926E0A (unknown), RW 0x00927898 (unknown), RW 0x00927B9F (unknown), RW 0x00927EC6 (unknown). Classes: AptCampaignMenu, AptMainMenu, CampaignProgress, ExtraRows, GameState, GameWorld, LinearCampaign, RebuildHoleBehavior, RebuildHoleExposeDie, SaveGameFile, SubsystemLegend, ThingTemplate, TutorialSettings.
Candidates: 1 none/partial functions; 37 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G404: RW 0x0067CAC6

Stops: S-4856. Kinds: code.
Functions: RW 0x0067CAC6 (unknown), RW 0x00680A75 (ported), RW 0x006EE600 (unknown), RW 0x006F0741 (ported), RW 0x006F25FE (unknown), RW 0x006F6286 (unknown), RW 0x006F8601 (unknown), RW 0x006F8B6E (unknown), RW 0x007682E1 (ported), RW 0x007684F8 (ported), RW 0x0076879E (unknown), RW 0x00768832 (unknown), RW 0x0093527F (unknown), RW 0x009356DF (unknown), RW 0x00935FAA (ported), RW 0x009389CE (unknown), RW 0x00939FA8 (unknown), RW 0x0093A530 (unknown). Classes: PathfindLayer.
Candidates: 0 none/partial functions; 13 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G405: RW 0x007B274C

Stops: S-5041. Kinds: code.
Functions: RW 0x007B274C (unknown), RW 0x007B2B7F (unknown), RW 0x0084010E (unknown), RW 0x008460B5 (unknown), RW 0x008465A4 (unknown), RW 0x009270AD (unknown), RW 0x00929745 (unknown), RW 0x0093DA51 (unknown), RW 0x0093DAC6 (unknown), RW 0x0093E669 (unknown), RW 0x0093E72F (unknown), RW 0x009CE6FE (unknown), RW 0x009CE8F3 (unknown). Classes: GameInfo, ScoreKeeper.
Candidates: 0 none/partial functions; 13 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G406: RW 0x00437120

Stops: S-760. Kinds: code.
Functions: RW 0x00437120 (unknown), RW 0x0068FBD3 (ported), RW 0x00694BF8 (ported), RW 0x006AD967 (unknown), RW 0x006D7647 (unknown), RW 0x008001E8 (unknown), RW 0x0092F712 (unknown), RW 0x0092FBAB (unknown), RW 0x0092FE33 (unknown), RW 0x00930809 (unknown), RW 0x0093089B (unknown), RW 0x009441FE (unknown), RW 0x0094475A (unknown), RW 0x00944EFB (unknown), RW 0x00ADF7E0 (unknown). Classes: ControlBar.
Candidates: 0 none/partial functions; 13 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G407: RW 0x00466289

Stops: S-326, S-784. Kinds: code.
Functions: RW 0x00441B59 (ported), RW 0x00466289 (unknown), RW 0x006616AC (unknown), RW 0x00662DD9 (unknown), RW 0x006630A9 (ported), RW 0x00663802 (unknown), RW 0x006639CF (unknown), RW 0x006668CA (ported), RW 0x0066831A (ported), RW 0x0066844A (ported), RW 0x00668E94 (ported), RW 0x0066BD33 (ported), RW 0x0067D7EC (partial), RW 0x0068B3AB (unknown), RW 0x0068C9AC (unknown), RW 0x0068D30E (ported), RW 0x00697C09 (ported), RW 0x006CA33E (unknown), RW 0x006CA974 (unknown), RW 0x006CB85A (ported), RW 0x006CC07C (ported), RW 0x006CC653 (ported), RW 0x006CC6DD (unknown), RW 0x006EFD35 (unknown), RW 0x006F2D31 (unknown), RW 0x006F37B3 (unknown), RW 0x006F3C87 (ported), RW 0x006F4557 (unknown), RW 0x006F4F9D (unknown), RW 0x006F9829 (unknown), RW 0x006F9850 (unknown), RW 0x006FB754 (unknown), RW 0x006FB869 (unknown), RW 0x006FC18E (unknown), RW 0x006FDE38 (unknown), RW 0x007006D7 (ported), RW 0x00701443 (ported), RW 0x0070146A (unknown), RW 0x0070C55D (ported), RW 0x00743670 (unknown), RW 0x00751F01 (unknown), RW 0x007553AB (unknown), RW 0x00758B68 (ported), RW 0x00771F50 (unknown), RW 0x0093AD93 (unknown), RW 0x00AD2CE0 (ported). Classes: AIAttackApproachState, AttackPriorityInfo, ScriptEngine, TargetFinder, TerrainLogic.
Candidates: 1 none/partial functions; 27 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G408: RW 0x006FFBEF

Stops: S-152, S-7230, S-7333, S-895. Kinds: code, project.
Functions: RW 0x004627C1 (unknown), RW 0x00660D71 (unknown), RW 0x00662E01 (unknown), RW 0x00680A75 (ported), RW 0x0068F3F2 (unknown), RW 0x00694CCC (ported), RW 0x00696266 (ported), RW 0x006AA61B (ported), RW 0x006AA6CD (unknown), RW 0x006AA6E1 (unknown), RW 0x006C6623 (unknown), RW 0x006F0B27 (unknown), RW 0x006FEEF7 (unknown), RW 0x006FFBEF (unknown), RW 0x007441E0 (ported), RW 0x0076C479 (unknown), RW 0x0076F115 (ported), RW 0x0076F426 (ported), RW 0x0076FFF0 (ported), RW 0x0077348C (ported), RW 0x007747DE (ported), RW 0x00774897 (ported), RW 0x00774E2F (ported), RW 0x00774F9C (partial), RW 0x007A05A0 (ported), RW 0x007A0F74 (ported), RW 0x007A5B03 (ported), RW 0x007A6A20 (ported), RW 0x007B7E23 (unknown), RW 0x007E5640 (unknown), RW 0x007E56C1 (unknown), RW 0x00831CB0 (unknown), RW 0x0083320B (unknown), RW 0x008345F4 (unknown), RW 0x00834CA4 (unknown), RW 0x008C3FA3 (ported), RW 0x008C4FD7 (ported), RW 0x008C5100 (ported), RW 0x008F1499 (unknown), RW 0x008F1536 (unknown), RW 0x008F1792 (unknown), RW 0x008F17EE (unknown), RW 0x008F1848 (unknown), RW 0x008F1D88 (ported), RW 0x008F42AC (unknown), RW 0x008F42BD (unknown), RW 0x008F5220 (unknown), RW 0x008F5756 (unknown), RW 0x008F5ABD (unknown), RW 0x008F6B5A (unknown), RW 0x008F7441 (unknown), RW 0x008F7573 (unknown), RW 0x0094FCBA (ported), RW 0x0098BDDE (unknown), RW 0x009B3184 (unknown), RW 0x009B31C5 (unknown), RW 0x009B31E6 (unknown), RW 0x009B3380 (unknown), RW 0x009B3453 (unknown), RW 0x009B3548 (unknown), RW 0x009B3646 (unknown), RW 0x009B36BD (unknown), RW 0x009B396A (unknown), RW 0x009B3BCB (unknown), RW 0x009B3C94 (unknown), RW 0x009B3D02 (unknown), RW 0x009B3EAB (unknown), RW 0x009B4041 (unknown), RW 0x009B42BE (unknown), RW 0x009B436B (unknown), RW 0x009B450C (unknown), RW 0x009B461D (unknown), RW 0x009B48F2 (unknown), RW 0x009B4AA8 (unknown), RW 0x009B4B0E (unknown), RW 0x009B4C32 (unknown), RW 0x009EF783 (unknown), RW 0x009EF829 (unknown), RW 0x009EF955 (unknown). Classes: AIGroup, AITactic, ActiveBody, DamageInfoOutput, GameLogicSettings, GenericCopy, ScriptConditions, SymbioticStructuresBody.
Candidates: 1 none/partial functions; 57 unknown status.
Red buttons: none linked.
Unused fields: ActiveBody.EnteringDamagedTransitionTime, ActiveBody.ReallyDamagedAttributeModifier, SymbioticStructuresBody.Symbiote.
Existing task matches: #567 Port 243: Body damage states: ActiveBody EnteringDamagedTransitionTime and the Damaged / ReallyDamaged attribute modifiers (ActiveBody, RespawnBody).

### G409: RW 0x004A25AA

Stops: S-1063. Kinds: code.
Functions: RW 0x004A25AA (unknown), RW 0x00725D96 (unknown), RW 0x00924DD1 (unknown), RW 0x00924F24 (unknown), RW 0x009257F2 (unknown), RW 0x0092632E (unknown), RW 0x009265BD (unknown), RW 0x009CDEC1 (unknown), RW 0x009CE51E (unknown), RW 0x009F07C7 (unknown), RW 0x009F082F (unknown), RW 0x009F0885 (unknown), RW 0x009F0B50 (unknown), RW 0x009F0E7A (unknown), RW 0x009F19DC (unknown). Classes: AptTimeLine.
Candidates: 0 none/partial functions; 15 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G410: RW 0x006EA857

Stops: S-321, S-5200. Kinds: code.
Functions: RW 0x0067055A (ported), RW 0x0067D734 (unknown), RW 0x006CC399 (unknown), RW 0x006CCB4F (unknown), RW 0x006CEF6D (ported), RW 0x006E89EE (ported), RW 0x006EA857 (ported), RW 0x00740F2C (unknown), RW 0x008663B6 (unknown), RW 0x0087A38E (partial), RW 0x0087CE79 (ported), RW 0x0090DEF0 (unknown), RW 0x0090EAF9 (unknown), RW 0x0090EF46 (unknown), RW 0x0090F177 (unknown), RW 0x0090F5F3 (unknown), RW 0x0090F858 (unknown), RW 0x0090F887 (unknown), RW 0x00910C8A (unknown), RW 0x00910DCF (unknown), RW 0x00910EA1 (unknown), RW 0x00910F78 (unknown), RW 0x0091117C (unknown), RW 0x009115BB (unknown), RW 0x0091161F (unknown), RW 0x00911770 (unknown), RW 0x009117A7 (unknown), RW 0x009118B1 (unknown), RW 0x00911C46 (unknown), RW 0x00911EDC (unknown), RW 0x00912241 (unknown), RW 0x0094B4B5 (unknown), RW 0x0094B727 (unknown), RW 0x0094B819 (unknown), RW 0x0094B926 (unknown), RW 0x00990D14 (ported). Classes: AIHarvestState, AttributeModifierNugget, DOTManager, DOTNugget, DamageNugget, FireLogicNugget, GateProxyBehavior, LinearTarget, MetaImpactNugget, ObjectCreationList, OpenContain, ParalyzeNugget, ProjectileLauncher, ProjectileNugget, WeaponOCLNugget.
Candidates: 1 none/partial functions; 29 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G411: BridgeBehavior

Stops: S-7223. Kinds: code.
Functions: RW 0x0044F709 (unknown), RW 0x004DD43F (unknown), RW 0x004DDEBD (unknown), RW 0x004DE105 (unknown), RW 0x004DEFEB (unknown), RW 0x0062F1A4 (unknown), RW 0x0063C5E2 (unknown), RW 0x006813E2 (unknown), RW 0x0068141D (unknown), RW 0x00681A58 (unknown), RW 0x006E8630 (unknown), RW 0x00768AE9 (unknown), RW 0x0085A7A9 (none), RW 0x008DA9C6 (unknown), RW 0x0096589F (unknown). Classes: BridgeBehavior, TerrainLogic.
Candidates: 1 none/partial functions; 14 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: #296 [deferred] Local 40: bridge logic as retail: towers, damage stages, pathfinding layer, radar (BridgeBehavior, S-7223).

### G412: RW 0x006B3025

Stops: S-1328. Kinds: code.
Functions: RW 0x006B3025 (unknown), RW 0x006BC2EC (unknown), RW 0x006BD3F3 (unknown), RW 0x006BD4D5 (unknown), RW 0x006E19A1 (unknown), RW 0x0079D4B9 (ported), RW 0x008FAFB8 (unknown), RW 0x008FB695 (unknown), RW 0x008FC8A9 (unknown), RW 0x008FE401 (unknown), RW 0x008FE6AF (unknown), RW 0x008FE7B0 (unknown), RW 0x008FEA4D (unknown), RW 0x008FEC1C (unknown), RW 0x008FEDF2 (unknown), RW 0x008FF0AC (unknown), RW 0x009A35DE (unknown). Classes: ConcurrentRegionBonus, SciencePurchasePointTracker.
Candidates: 0 none/partial functions; 16 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G413: RW 0x0068B36F

Stops: S-1876, S-861. Kinds: code.
Functions: RW 0x005EA74E (partial), RW 0x0060A1D5 (unknown), RW 0x006682B1 (ported), RW 0x0066D7C9 (unknown), RW 0x0068B36F (ported), RW 0x00695338 (ported), RW 0x007A3E9B (unknown), RW 0x007EDAED (unknown), RW 0x00897368 (unknown), RW 0x008B1142 (ported), RW 0x008B1219 (ported), RW 0x008B125F (ported), RW 0x008B12BF (unknown), RW 0x008B140D (unknown), RW 0x008B1690 (ported), RW 0x008B1E9A (unknown), RW 0x008B1F59 (unknown), RW 0x008B1FDA (ported), RW 0x008B228C (unknown), RW 0x008E3D4C (ported), RW 0x008E3D77 (ported), RW 0x008F02CC (unknown), RW 0x0099F7EF (unknown), RW 0x0099F83D (unknown), RW 0x0099F88B (unknown), RW 0x0099FA40 (unknown), RW 0x0099FAEF (unknown), RW 0x0099FC0C (unknown), RW 0x0099FCC4 (unknown), RW 0x0099FD0E (unknown), RW 0x0099FDDE (unknown), RW 0x0099FE74 (unknown), RW 0x009A0023 (unknown), RW 0x009A018B (unknown), RW 0x009A135B (unknown), RW 0x009A155C (unknown), RW 0x009A1696 (unknown), RW 0x009EC539 (unknown), RW 0x009EC8A1 (unknown), RW 0x009ECC0D (unknown), RW 0x009ECF7F (unknown), RW 0x009ED351 (unknown), RW 0x009ED374 (unknown), RW 0x009ED45E (unknown), RW 0x009ED4D2 (unknown), RW 0x009ED764 (unknown), RW 0x009ED8C4 (unknown). Classes: CastleMemberBehavior, SkirmishAI, SpecialPowerModule, ToggleMountedSpecialAbilityUpdate.
Candidates: 1 none/partial functions; 36 unknown status.
Red buttons: Command_AngmarRangersIceArrowAbility, Command_AngmarTrollChargeAttack, Command_AxtschwingerAxeThrow, Command_BeregondMountNew, Command_BigScorpionUnderground, Command_BlackRiderMorgulBladeForHorde, Command_FireDrakeBroodInferno, Command_GorkilBombThrow, Command_GorkilZuchtspinnenSummon, Command_HalftrollChargeAttack, Command_IsenfurtWchterSmite, Command_NecromancerDeathBargain, Command_RavenPostSecondBonus, Command_SmaugIncinerateNew, Command_SpecialAbilityBalrogBreathAI, Command_SpecialAbilityBoromirHorn, Command_SpecialAbilityBoromirHornRing, Command_SpecialAbilityBoromirTapfererWiderstand, Command_SpecialAbilityBuhrdurFressen, Command_SpecialAbilityDenethorNotstandssituation, Command_SpecialAbilityDenethorSehenderStein1, Command_SpecialAbilityDenethorVerzweiflungsakt, Command_SpecialAbilityDurinMeisterGestein, Command_SpecialAbilityElendilSummoned, Command_SpecialAbilityGeheimeWege, Command_SpecialAbilityGulzarMOrgulSeucheAcolyte, Command_SpecialAbilityHaradScoutCloak, Command_SpecialAbilityKingsFavorEdainTheo, Command_SpecialAbilityMordorBalrogWhipAI, Command_SpecialAbilityNaturmagierErntesegen, Command_SpecialAbilitySaatdesgoldenenWaldes, Command_SpecialAbilityScreechDunklerMarschall, Command_SpecialAbilityTributeGainTwo, Command_ThranduilKnigDsterwalds.
Unused fields: CastleMemberBehavior.StoreUpgradePrice, ToggleMountedSpecialAbilityUpdate.OpacityTarget.
Existing task matches: #688 Port 296: Castles вЂ” CastleBehavior, CastleMemberBehavior and FoundationAIUpdate: the uncited own functions (27) and their five unread fields; #599 Port 251: The special-power chain every Ennorath ability runs through: SpecialPowerModule and UnpauseSpecialPowerUpgrade's uncited RotWK functions.

### G414: RW 0x006B47DC

Stops: S-1330. Kinds: code.
Functions: RW 0x006B47DC (unknown), RW 0x006B546B (unknown), RW 0x006B9B65 (unknown), RW 0x006BABA8 (unknown), RW 0x006BE20A (unknown), RW 0x007F0C1A (unknown), RW 0x007F7C8D (unknown), RW 0x00811951 (unknown), RW 0x0090002D (unknown), RW 0x009001FB (unknown), RW 0x00900533 (unknown), RW 0x009006E5 (unknown), RW 0x00900AA7 (unknown), RW 0x00902B96 (unknown), RW 0x00908010 (unknown), RW 0x009095A7 (unknown), RW 0x009A4205 (unknown), RW 0x009A5807 (unknown), RW 0x009A63BD (unknown), RW 0x009A747A (unknown). Classes: ArmySummary, LivingWorldAI.
Candidates: 0 none/partial functions; 20 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G415: RW 0x0090C0EE

Stops: S-7187, S-7336, S-893. Kinds: code.
Functions: RW 0x00441E4A (unknown), RW 0x0068C82D (ported), RW 0x00696266 (ported), RW 0x006A9999 (ported), RW 0x006AA61B (ported), RW 0x006C7EDD (unknown), RW 0x006F5F65 (ported), RW 0x0073C25F (ported), RW 0x0082FF49 (unknown), RW 0x008DC9AB (ported), RW 0x008EE765 (unknown), RW 0x008F175A (unknown), RW 0x008F18DE (unknown), RW 0x008F232F (unknown), RW 0x008F2338 (unknown), RW 0x0090C0EE (unknown), RW 0x0090C5E2 (unknown), RW 0x0099E766 (unknown), RW 0x009A1696 (unknown), RW 0x009B58BF (unknown), RW 0x009B71B7 (unknown), RW 0x009B7260 (unknown), RW 0x009B737F (unknown), RW 0x009B7458 (unknown), RW 0x009B7503 (unknown), RW 0x009B7AE2 (unknown), RW 0x009B7E5F (unknown), RW 0x009B7EBD (unknown), RW 0x009B7F56 (unknown), RW 0x009B8047 (unknown), RW 0x009B80FA (unknown), RW 0x009B8160 (unknown), RW 0x009B83A6 (unknown), RW 0x009B8451 (unknown), RW 0x009B8556 (unknown), RW 0x009B8635 (unknown), RW 0x009B877C (unknown), RW 0x009B87DA (unknown), RW 0x009B88D8 (unknown), RW 0x009B8994 (unknown), RW 0x009B89CF (unknown), RW 0x009B8A06 (unknown), RW 0x009B8A47 (unknown), RW 0x009B8A7B (unknown), RW 0x009B8C32 (unknown), RW 0x009B8D81 (unknown), RW 0x009B8E1E (unknown), RW 0x009B9B9B (unknown), RW 0x009B9E06 (unknown), RW 0x009BA005 (unknown), RW 0x009BAA0E (unknown), RW 0x009BAE5F (unknown), RW 0x009BAF02 (unknown), RW 0x009BB1C4 (unknown), RW 0x009BB226 (unknown), RW 0x009BB2A7 (unknown), RW 0x009BB2EA (unknown), RW 0x009BB3CA (unknown), RW 0x009BB479 (unknown), RW 0x009BB5E4 (unknown), RW 0x009BBA51 (unknown), RW 0x009BBE48 (unknown), RW 0x009BBFDC (unknown), RW 0x009BC1A0 (unknown), RW 0x009BC213 (unknown), RW 0x009BC39F (unknown), RW 0x009BC3F4 (unknown), RW 0x009EFAD6 (unknown), RW 0x009EFAF0 (unknown), RW 0x009EFAFB (unknown). Classes: AIStatCollector, AITactic, AITacticsGenerator, ArmyDefinition, GateOpenAndCloseBehavior, VictoryConditions.
Candidates: 0 none/partial functions; 63 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G416: RW 0x008F0EB4

Stops: S-412, S-413, S-4194. Kinds: code.
Functions: RW 0x0042EC5E (unknown), RW 0x0064501B (unknown), RW 0x006A96A0 (unknown), RW 0x006A99CB (ported), RW 0x006A9D9C (ported), RW 0x006A9E48 (unknown), RW 0x006A9EAD (unknown), RW 0x006AA030 (unknown), RW 0x006C6DB1 (unknown), RW 0x006C7581 (unknown), RW 0x006C75B5 (unknown), RW 0x006C7677 (unknown), RW 0x006C78D9 (unknown), RW 0x006C7928 (unknown), RW 0x006C7946 (unknown), RW 0x0070011F (unknown), RW 0x007A69E1 (unknown), RW 0x0082F787 (unknown), RW 0x0082FA98 (unknown), RW 0x0089C04B (unknown), RW 0x0089C12F (unknown), RW 0x0089CC29 (ported), RW 0x008E3CF3 (unknown), RW 0x008ED6DC (unknown), RW 0x008ED726 (unknown), RW 0x008ED772 (ported), RW 0x008EDA39 (unknown), RW 0x008EDA5E (ported), RW 0x008EDB2C (unknown), RW 0x008EDBB0 (unknown), RW 0x008EDDF6 (unknown), RW 0x008F02CC (unknown), RW 0x008F0502 (unknown), RW 0x008F069A (ported), RW 0x008F06FF (ported), RW 0x008F0A53 (unknown), RW 0x008F0BBC (ported), RW 0x008F0C47 (ported), RW 0x008F0EB4 (unknown), RW 0x008F736E (unknown), RW 0x008F7D71 (unknown), RW 0x008F858E (unknown), RW 0x008F8654 (unknown), RW 0x008F89D2 (unknown), RW 0x008F8B4A (unknown), RW 0x0090B3CE (unknown), RW 0x0090CADA (unknown), RW 0x0090CE35 (unknown), RW 0x0090D16A (ported), RW 0x0090D3C7 (unknown), RW 0x0096190B (unknown), RW 0x00961975 (unknown), RW 0x0097DC1A (unknown), RW 0x0097DC99 (unknown), RW 0x0097DD6B (unknown), RW 0x0097DFCD (unknown), RW 0x0099E766 (unknown), RW 0x0099E773 (unknown), RW 0x0099E79A (unknown), RW 0x0099EEF4 (unknown), RW 0x009A0993 (unknown), RW 0x009A19E0 (unknown), RW 0x009A221E (unknown), RW 0x009A23A4 (unknown), RW 0x009B53C6 (unknown), RW 0x009B6F70 (unknown), RW 0x009B9E06 (unknown), RW 0x009BA3C2 (unknown), RW 0x009BC882 (unknown), RW 0x009BCA33 (unknown), RW 0x009BCA8E (unknown), RW 0x009ED8C4 (unknown), RW 0x009EDC36 (unknown), RW 0x009EFCC4 (unknown). Classes: AIBuilder, AISideInfo, AISkirmishPlayer, AIStatCollector, ArmyDefinition, GameLogicSettings, SkirmishAI, SkirmishAIManager.
Candidates: 0 none/partial functions; 64 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G417: RW 0x006AA5F5

Stops: S-1421, S-7227. Kinds: code, project.
Functions: RW 0x0063F3BF (ported), RW 0x00668303 (ported), RW 0x0066C4CA (ported), RW 0x0068AD89 (ported), RW 0x0068C26D (ported), RW 0x0068C461 (ported), RW 0x00696FD2 (ported), RW 0x006AA5F5 (unknown), RW 0x006ABD44 (unknown), RW 0x006ABDF7 (unknown), RW 0x006ABE2C (unknown), RW 0x0075CF21 (unknown), RW 0x007A07D8 (unknown), RW 0x007A0956 (unknown), RW 0x007A1129 (unknown), RW 0x007A3EF5 (unknown), RW 0x007A5582 (unknown), RW 0x00851E97 (ported), RW 0x008B708B (unknown), RW 0x008B714F (ported), RW 0x008B73B7 (ported), RW 0x008C74C4 (ported), RW 0x008E4042 (unknown), RW 0x008E4456 (ported), RW 0x008E46A5 (ported), RW 0x008F0284 (unknown), RW 0x008F457D (unknown), RW 0x008F47EB (unknown), RW 0x008F6186 (unknown), RW 0x008F6511 (unknown), RW 0x008F6B5A (unknown), RW 0x0090CD45 (unknown), RW 0x0099302D (unknown), RW 0x00993055 (unknown), RW 0x0099316A (unknown), RW 0x00993307 (unknown), RW 0x009E691D (unknown), RW 0x009E6CCB (unknown), RW 0x009E6EF4 (unknown), RW 0x009E70B6 (unknown), RW 0x009E726A (unknown), RW 0x009E74B7 (unknown), RW 0x009E7942 (unknown), RW 0x009E7A98 (unknown), RW 0x009E7F0F (unknown), RW 0x009E85C6 (unknown), RW 0x009E880B (unknown), RW 0x009E896E (unknown), RW 0x009E8A42 (unknown), RW 0x009E91EA (unknown), RW 0x009E9222 (unknown), RW 0x009E92CA (unknown), RW 0x009E9B37 (unknown), RW 0x009EB961 (unknown), RW 0x00A01C9C (unknown), RW 0x00A021D8 (unknown), RW 0x00A0229F (unknown), RW 0x00A0241D (unknown), RW 0x00A024DA (unknown), RW 0x00A0260A (unknown). Classes: AISkirmishPlayer, AISpecialPowerUpdate, ActionManager, ExperienceTracker, SkirmishAI, SlavedUpdate, SpecialAbilityUpdate, TeamPrototype, TerrainTrees, ThingTemplate.
Candidates: 0 none/partial functions; 47 unknown status.
Red buttons: Command_AngmarTrollChargeAttack, Command_SpecialAbilityLoneRangerSummonFinal, Command_SpecialAbilityNaturmagierErntesegen, Command_ThranduilKnigDsterwalds.
Unused fields: none linked.
Existing task matches: none.

### G418: RW 0x006B694D

Stops: S-1331, S-1334. Kinds: code.
Functions: RW 0x00620B6A (unknown), RW 0x006B6657 (unknown), RW 0x006B694D (unknown), RW 0x006B6ACC (unknown), RW 0x006B89D4 (unknown), RW 0x006B943A (unknown), RW 0x006B94C6 (unknown), RW 0x006B9527 (unknown), RW 0x006B955E (unknown), RW 0x006BA994 (unknown), RW 0x006BB70D (unknown), RW 0x006D7647 (unknown), RW 0x007F6443 (unknown), RW 0x00839D7F (unknown), RW 0x0083A0E6 (unknown), RW 0x0083A272 (unknown), RW 0x008F33A0 (unknown), RW 0x008F352E (unknown), RW 0x0092E689 (unknown), RW 0x00953007 (unknown), RW 0x0097E6DD (unknown), RW 0x0097F999 (unknown), RW 0x0097FBB7 (unknown), RW 0x00983517 (unknown), RW 0x00983B39 (unknown), RW 0x009845B4 (unknown), RW 0x0098471D (unknown), RW 0x00985DD6 (unknown), RW 0x009860EA (unknown), RW 0x00986404 (unknown), RW 0x009CF16C (unknown), RW 0x009DAD60 (unknown), RW 0x009E1532 (unknown), RW 0x009E21B5 (unknown), RW 0x009F4287 (unknown), RW 0x009F466E (unknown), RW 0x009F8007 (unknown), RW 0x009F9376 (unknown), RW 0x009F986A (unknown), RW 0x009FBFA0 (unknown), RW 0x009FC1A4 (unknown), RW 0x009FE832 (unknown), RW 0x00A07D22 (unknown), RW 0x00A08253 (unknown), RW 0x00A09009 (unknown), RW 0x00A092D3 (unknown), RW 0x00A09964 (unknown), RW 0x00A0DF80 (unknown). Classes: AptStrategicHUD, ArmySummaryDescription, GameSetup, ScoreKeeper.
Candidates: 0 none/partial functions; 48 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G419: HordeContain

Stops: S-7375, S-7505. Kinds: code, project.
Functions: RW 0x004065AA (ported), RW 0x00449681 (ported), RW 0x00645716 (none), RW 0x0068C866 (ported), RW 0x00865598 (ported), RW 0x0086BD7D (none), RW 0x0086BD98 (none), RW 0x0086BDC0 (none), RW 0x0086BE2E (none), RW 0x0086BE49 (none), RW 0x0086BE64 (none), RW 0x0086C0BC (none), RW 0x0086C192 (none), RW 0x0086C758 (none), RW 0x0086C81E (ported), RW 0x0086C8B9 (none), RW 0x0086C8F2 (none), RW 0x0086C9E7 (none), RW 0x0086C9F3 (none), RW 0x0086CA78 (none), RW 0x0086D438 (none), RW 0x0086D4CD (none), RW 0x0086D562 (ported), RW 0x0086D5BB (none), RW 0x0086DF44 (ported), RW 0x0086ECAB (ported), RW 0x0086F60A (none), RW 0x0086F6E1 (none), RW 0x0086F7A3 (none), RW 0x0086F93F (none), RW 0x0086F9CA (ported), RW 0x0086FE0F (none), RW 0x0086FEAD (none), RW 0x0086FEEC (ported), RW 0x00870655 (none), RW 0x008707A0 (none), RW 0x00870AEE (none), RW 0x00870D4E (ported), RW 0x00871110 (none), RW 0x00871184 (none), RW 0x0087159F (none), RW 0x00871844 (none), RW 0x00871A90 (ported), RW 0x00871F29 (ported), RW 0x00871F8B (none), RW 0x00872A69 (none), RW 0x00872A8D (none), RW 0x00872A9A (none), RW 0x00872AAE (none), RW 0x00872ACB (none), RW 0x00872AD2 (none), RW 0x00872AE7 (none), RW 0x00872AEE (none), RW 0x00872AF5 (none), RW 0x00872B02 (none), RW 0x00872B24 (none), RW 0x00872B76 (none), RW 0x00872EFC (ported), RW 0x00873262 (none), RW 0x008736EF (library), RW 0x0087545C (none), RW 0x00875F18 (none), RW 0x008762DD (none), RW 0x00876406 (unknown), RW 0x00876481 (ported), RW 0x0087737A (none), RW 0x008780E4 (none), RW 0x00879106 (ported), RW 0x008C72DF (ported). Classes: Coord3D, HordeContain, UnitVoiceResponse.
Candidates: 50 none/partial functions; 1 unknown status.
Red buttons: none linked.
Unused fields: HordeContain.LivingWorldOverloadTemplate, HordeContain.RanksToJustFreeWhenAttacking.
Existing task matches: #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate.

### G420: RW 0x006B5F1C

Stops: S-1326. Kinds: code.
Functions: RW 0x006B5F1C (unknown), RW 0x006B7229 (unknown), RW 0x006E0B4A (unknown), RW 0x006E1970 (unknown), RW 0x006E1986 (unknown), RW 0x006E24C0 (unknown), RW 0x006E2625 (unknown), RW 0x006E4123 (unknown), RW 0x00780172 (unknown), RW 0x007F13AC (unknown), RW 0x007F19D6 (unknown), RW 0x007F32F4 (unknown), RW 0x007F3F58 (unknown), RW 0x007F4DC8 (unknown), RW 0x0080ED8C (unknown), RW 0x0080EDB8 (unknown), RW 0x0080F018 (unknown), RW 0x0080F0D8 (unknown), RW 0x0080F237 (unknown), RW 0x008E4ACD (unknown), RW 0x008E5C5C (unknown), RW 0x008FE971 (unknown), RW 0x008FF364 (unknown), RW 0x008FF50E (unknown), RW 0x008FF6F8 (unknown), RW 0x008FF849 (unknown), RW 0x008FFC4A (unknown), RW 0x008FFF41 (unknown), RW 0x009D3829 (unknown), RW 0x009D3881 (unknown). Classes: LevelUpUpgrade, LivingWorldLogic, UnitRevivalTracker.
Candidates: 0 none/partial functions; 30 unknown status.
Red buttons: Command_PurchaseUpgradeDwarvenBasicTraining.
Unused fields: none linked.
Existing task matches: none.

### G421: RW 0x006F5BB0

Stops: S-162, S-417, S-5308, S-590. Kinds: code.
Functions: RW 0x004627C1 (unknown), RW 0x0063F3BF (ported), RW 0x00667ED1 (unknown), RW 0x0068B401 (ported), RW 0x0068BBE0 (ported), RW 0x006ADC43 (ported), RW 0x006C644C (unknown), RW 0x006C650B (unknown), RW 0x006C6975 (unknown), RW 0x006C6A24 (unknown), RW 0x006C7344 (unknown), RW 0x006C74C2 (unknown), RW 0x006C7946 (unknown), RW 0x006CA525 (ported), RW 0x006EB2EF (unknown), RW 0x006F1F0D (unknown), RW 0x006F2F8F (ported), RW 0x006F2FFF (unknown), RW 0x006F3C87 (ported), RW 0x006F49BC (unknown), RW 0x006F5BB0 (ported), RW 0x006F5EF3 (unknown), RW 0x006F5F65 (ported), RW 0x006F6454 (unknown), RW 0x006F6C18 (unknown), RW 0x006F8601 (unknown), RW 0x006F8B6E (unknown), RW 0x006F91E8 (unknown), RW 0x006F9829 (unknown), RW 0x006FA733 (unknown), RW 0x006FB67A (unknown), RW 0x006FB754 (unknown), RW 0x006FC18E (unknown), RW 0x006FE7CB (unknown), RW 0x006FF7FA (ported), RW 0x0070B9E0 (ported), RW 0x007443C0 (unknown), RW 0x007463E8 (unknown), RW 0x00746783 (unknown), RW 0x007468D5 (ported), RW 0x0074A594 (unknown), RW 0x0074A63F (unknown), RW 0x0074AA03 (unknown), RW 0x0074AC0C (unknown), RW 0x0074EB5C (unknown), RW 0x00774897 (ported), RW 0x00774E2F (ported), RW 0x00774F2B (unknown), RW 0x00774F9C (partial), RW 0x0077559F (unknown), RW 0x007A0F74 (ported), RW 0x007A12F4 (ported), RW 0x007A6295 (unknown), RW 0x007EDC2F (unknown), RW 0x0082EF7B (unknown), RW 0x00865321 (ported), RW 0x00865332 (unknown), RW 0x0086539E (unknown), RW 0x0086540A (ported), RW 0x0087631F (ported), RW 0x008E4591 (ported), RW 0x008F11DA (unknown), RW 0x008F121F (unknown), RW 0x008F135E (unknown), RW 0x008F144C (unknown), RW 0x008F1725 (unknown), RW 0x008F175A (unknown), RW 0x008F1792 (unknown), RW 0x008F17EE (unknown), RW 0x008F19E9 (unknown), RW 0x008F1D88 (ported), RW 0x008F2006 (ported), RW 0x008F21F7 (unknown), RW 0x008F243A (unknown), RW 0x008F2784 (unknown), RW 0x008F27AD (unknown), RW 0x008F28DD (unknown), RW 0x0090B1F4 (unknown), RW 0x0090B2A0 (unknown), RW 0x0090B342 (unknown), RW 0x0090B4E7 (unknown), RW 0x0090B5A9 (unknown), RW 0x0090BE31 (unknown), RW 0x0090C436 (unknown), RW 0x0090C50C (unknown), RW 0x0090C5E2 (unknown), RW 0x0090CCCB (unknown), RW 0x0090D3C7 (unknown), RW 0x0093485A (unknown), RW 0x0093491C (unknown), RW 0x00937F2D (unknown), RW 0x00937F7F (unknown), RW 0x00937FD2 (unknown), RW 0x00938240 (unknown), RW 0x00938939 (unknown), RW 0x0093894B (unknown), RW 0x009389A6 (unknown), RW 0x009389CE (unknown), RW 0x00938B3B (unknown), RW 0x00938D89 (unknown), RW 0x00938E42 (unknown), RW 0x0093A530 (unknown), RW 0x0093B3D8 (unknown), RW 0x0097CC29 (unknown), RW 0x0097CCBE (unknown), RW 0x0097CDA0 (unknown), RW 0x0097CE90 (unknown), RW 0x0097D035 (unknown), RW 0x0097D191 (unknown), RW 0x0097D425 (unknown), RW 0x0097D4E6 (unknown), RW 0x0097D74F (unknown), RW 0x0097DA21 (unknown), RW 0x0097DC99 (unknown), RW 0x00993373 (unknown), RW 0x009A2CA5 (unknown), RW 0x009A2E06 (unknown), RW 0x009A3366 (unknown), RW 0x009B2B04 (unknown), RW 0x009B2EB9 (unknown), RW 0x009B304A (unknown), RW 0x009B313C (unknown), RW 0x009B3184 (unknown), RW 0x009B31C5 (unknown), RW 0x009B31E6 (unknown), RW 0x009B333D (unknown), RW 0x009B3380 (unknown), RW 0x009B3453 (unknown), RW 0x009B3548 (unknown), RW 0x009B3734 (unknown), RW 0x009B37A7 (unknown), RW 0x009B3849 (unknown), RW 0x009B3D8B (unknown), RW 0x009B43E9 (unknown), RW 0x009B49BD (unknown), RW 0x009B4DE7 (unknown), RW 0x009B4E2B (unknown), RW 0x009B4EB7 (unknown), RW 0x009B4EF0 (unknown), RW 0x009B4F8F (unknown), RW 0x009B5020 (unknown), RW 0x009B5042 (unknown), RW 0x009B5190 (unknown), RW 0x009B52BD (unknown), RW 0x009EF783 (unknown), RW 0x009EF829 (unknown), RW 0x009EF955 (unknown), RW 0x009EF9AF (unknown), RW 0x009EFAF0 (unknown). Classes: AIGroup, AITactic, AITacticsGenerator, GarrisonContain, PathfindZoneManager, RunOffMapBehavior, TerrainLogic.
Candidates: 1 none/partial functions; 126 unknown status.
Red buttons: none linked.
Unused fields: RunOffMapBehavior.DieOnMap, RunOffMapBehavior.RunOffMapWaypointName.
Existing task matches: none.

### G422: linked stops S-480

Stops: S-480. Kinds: code.
Functions: none resolved. Classes: ArmorUpgrade, AttributeModifierUpgrade, CastleUpgrade, CommandPointsUpgrade, CommandSetUpgrade, CostModifierUpgrade, DoCommandUpgrade, ExperienceScalarUpgrade, GeometryUpgrade, GrantUpgradeCreate, InheritUpgradeCreate, LevelUpUpgrade, LocomotorSetUpgrade, ModelConditionUpgrade, ObjectCreationUpgrade, RemoveUpgradeUpgrade, ReplaceSelfUpgrade, StatusBitsUpgrade, SubObjectsUpgrade, UnpauseSpecialPowerUpgrade, UpgradeMux, WeaponSetUpgrade.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: Command_AngmarRangersIceArrowAbility, Command_AngmarTrollChargeAttack, Command_BeregondMountNew, Command_BigScorpionUnderground, Command_BlackRiderMorgulBladeForHorde, Command_FireDrakeBroodInferno, Command_GorkilBombThrow, Command_GorkilZuchtspinnenSummon, Command_HalftrollChargeAttack, Command_MenLongShotAbility, Command_NecromancerDeathBargain, Command_OutpostOrcSpawn, Command_PurchaseUpgradeDwarvenBasicTraining, Command_RavenPostSecondBonus, Command_SmaugIncinerateNew, Command_SpecialAbilityBalrogBreathAI, Command_SpecialAbilityBeutekarrendesGroorks, Command_SpecialAbilityBoromirHorn, Command_SpecialAbilityBoromirHornRing, Command_SpecialAbilityBoromirTapfererWiderstand, Command_SpecialAbilityDenethorSehenderStein1, Command_SpecialAbilityDenethorSteuererlass, Command_SpecialAbilityDenethorVerzweiflungsakt, Command_SpecialAbilityDrarSpeerfeuer, Command_SpecialAbilityDurinMeisterGestein, Command_SpecialAbilityElendilSummoned, Command_SpecialAbilityGulzarMOrgulSeucheAcolyte, Command_SpecialAbilityKingsFavorEdainTheo, Command_SpecialAbilityLoneRangerSummonFinal, Command_SpecialAbilityMordorBalrogWhipAI, Command_SpecialAbilityMuhadSummonOliph, Command_SpecialAbilityNaturmagierErntesegen, Command_SpecialAbilityScreechDunklerMarschall, Command_SpecialAbilityTributeGainTwo, Command_SpecialAbilityWulfgarRufderPlnderer, Command_ThranduilKnigDsterwalds, Command_ramridercaptain.
Unused fields: SubObjectsUpgrade.WaitBeforeFadeInSeconds.
Existing task matches: #766 Local 100: upgrade modules: CommandSetUpgrade (515 modules), CostModifierUpgrade, CastleUpgrade, GiveUpgradeUpdate, AudioLoopUpgrade, WallUpgradeUpdate, LocomotorSetUpgrade: 27 uncited own functions; #599 Port 251: The special-power chain every Ennorath ability runs through: SpecialPowerModule and UnpauseSpecialPowerUpgrade's uncited RotWK functions; #570 Port 246: Client fades: W3DTreeDraw FadeDistance / FadeTarget, W3DFloorDraw FloorFadeRateOnObjectDeath, SubObjectsUpgrade WaitBeforeFadeInSeconds.

### G423: linked stops S-7728

Stops: S-7728. Kinds: code.
Functions: none resolved. Classes: OCLSpecialPower, ToggleMountedSpecialAbilityUpdate, WeaponFireSpecialAbilityUpdate.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: Command_AngmarRangersIceArrowAbility, Command_AxtschwingerAxeThrow, Command_BeregondMountNew, Command_BigScorpionUnderground, Command_BlackRiderMorgulBladeForHorde, Command_FireDrakeBroodInferno, Command_GaladrielMirrorDissent, Command_GaladrielMirrorWardByMirror, Command_GorkilBombThrow, Command_GorkilZuchtspinnenSummon, Command_IsenfurtWchterSmite, Command_MenLongShotAbility, Command_NecromancerDeathBargain, Command_OutpostOrcSpawn, Command_SmaugIncinerateNew, Command_SpecialAbilityBalrogBreathAI, Command_SpecialAbilityBeutekarrendesGroorks, Command_SpecialAbilityBoromirHorn, Command_SpecialAbilityBuhrdurFressen, Command_SpecialAbilityDenethorSteuererlass, Command_SpecialAbilityDrarSpeerfeuer, Command_SpecialAbilityGulzarMOrgulSeucheAcolyte, Command_SpecialAbilityKingsFavorEdainTheo, Command_SpecialAbilityLoneRangerSummonFinal, Command_SpecialAbilityMordorBalrogWhipAI, Command_SpecialAbilityMuhadSummonOliph, Command_SpecialAbilitySaatdesgoldenenWaldes, Command_SpecialAbilityTributeGainTwo, Command_ramridercaptain.
Unused fields: ToggleMountedSpecialAbilityUpdate.OpacityTarget.
Existing task matches: none.

### G424: WeaponFireSpecialAbilityUpdate

Stops: S-7469, S-7508. Kinds: code.
Functions: RW 0x008961F7 (ported), RW 0x00897E01 (ported), RW 0x008E302A (ported). Classes: FiringTracker, WeaponFireSpecialAbilityUpdate.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: Command_AngmarRangersIceArrowAbility, Command_AxtschwingerAxeThrow, Command_BlackRiderMorgulBladeForHorde, Command_FireDrakeBroodInferno, Command_GorkilBombThrow, Command_IsenfurtWchterSmite, Command_MenLongShotAbility, Command_NecromancerDeathBargain, Command_SmaugIncinerateNew, Command_SpecialAbilityBalrogBreathAI, Command_SpecialAbilityBuhrdurFressen, Command_SpecialAbilityDrarSpeerfeuer, Command_SpecialAbilityGulzarMOrgulSeucheAcolyte, Command_SpecialAbilityMordorBalrogWhipAI, Command_SpecialAbilitySaatdesgoldenenWaldes.
Unused fields: none linked.
Existing task matches: none.

### G425: RW 0x008B1690

Stops: S-863. Kinds: code.
Functions: RW 0x008B1690 (ported). Classes: ActivateModuleSpecialPower, FlingPassengerSpecialAbilityUpdate, LevelGrantSpecialPower, OCLSpecialPower, TransportContain.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: Command_GaladrielMirrorDissent, Command_GaladrielMirrorWardByMirror, Command_GorkilZuchtspinnenSummon, Command_MenLongShotAbility, Command_OutpostOrcSpawn, Command_SpecialAbilityBeutekarrendesGroorks, Command_SpecialAbilityDenethorSteuererlass, Command_SpecialAbilityDrarSpeerfeuer, Command_SpecialAbilityDurinMeisterGestein, Command_SpecialAbilityKingsFavorEdainTheo, Command_SpecialAbilityLoneRangerSummonFinal, Command_SpecialAbilityMuhadSummonOliph, Command_ramridercaptain.
Unused fields: FlingPassengerSpecialAbilityUpdate.FlingPassengerVelocity.
Existing task matches: none.

### G426: linked stops S-1987

Stops: S-1987. Kinds: code.
Functions: none resolved. Classes: FloodUpdate, OCLSpecialPower.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: Command_GaladrielMirrorDissent, Command_GaladrielMirrorWardByMirror, Command_GorkilZuchtspinnenSummon, Command_MenLongShotAbility, Command_OutpostOrcSpawn, Command_SpecialAbilityBeutekarrendesGroorks, Command_SpecialAbilityDenethorSteuererlass, Command_SpecialAbilityDrarSpeerfeuer, Command_SpecialAbilityLoneRangerSummonFinal, Command_SpecialAbilityMuhadSummonOliph, Command_ramridercaptain.
Unused fields: none linked.
Existing task matches: none.

### G427: RW 0x005DD9EE

Stops: S-7360. Kinds: code.
Functions: RW 0x005DD9EE (ported), RW 0x00799440 (ported), RW 0x0079CB47 (ported), RW 0x00853BEC (ported), RW 0x00853EDF (ported). Classes: CastleBehavior, CastleMemberBehavior, CastleUpgrade, SpecialAbilityUpdate.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: Command_AngmarTrollChargeAttack, Command_SpecialAbilityLoneRangerSummonFinal, Command_SpecialAbilityNaturmagierErntesegen, Command_ThranduilKnigDsterwalds.
Unused fields: CastleBehavior.CrewPrepareFX, CastleBehavior.CrewPrepareInterval, CastleBehavior.CrewReleaseFX, CastleMemberBehavior.StoreUpgradePrice.
Existing task matches: #766 Local 100: upgrade modules: CommandSetUpgrade (515 modules), CostModifierUpgrade, CastleUpgrade, GiveUpgradeUpdate, AudioLoopUpgrade, WallUpgradeUpdate, LocomotorSetUpgrade: 27 uncited own functions; #688 Port 296: Castles вЂ” CastleBehavior, CastleMemberBehavior and FoundationAIUpdate: the uncited own functions (27) and their five unread fields.

### G428: PickupStuffUpdate

Stops: S-1422. Kinds: code.
Functions: none resolved. Classes: PickupStuffUpdate, SpecialAbilityUpdate, VeterancyCrateCollide.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: Command_AngmarTrollChargeAttack, Command_SpecialAbilityLoneRangerSummonFinal, Command_SpecialAbilityNaturmagierErntesegen, Command_ThranduilKnigDsterwalds.
Unused fields: none linked.
Existing task matches: none.

### G429: RW 0x0068C89B

Stops: S-1873. Kinds: code.
Functions: RW 0x0068C89B (ported), RW 0x00851B6E (ported), RW 0x008969E5 (ported), RW 0x00896DB4 (ported), RW 0x00897D2D (ported), RW 0x00897E01 (ported). Classes: SpecialAbilityUpdate.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: Command_AngmarTrollChargeAttack, Command_SpecialAbilityLoneRangerSummonFinal, Command_SpecialAbilityNaturmagierErntesegen, Command_ThranduilKnigDsterwalds.
Unused fields: none linked.
Existing task matches: none.

### G430: LevelGrantSpecialPower

Stops: S-7510. Kinds: project.
Functions: none resolved. Classes: LevelGrantSpecialPower, PlayerHealSpecialPower.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: Command_SpecialAbilityKingsFavorEdainTheo.
Unused fields: none linked.
Existing task matches: none.

### G431: ActivateModuleSpecialPower

Stops: S-7511. Kinds: project.
Functions: RW 0x008D222A (ported). Classes: ActivateModuleSpecialPower.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: Command_SpecialAbilityDurinMeisterGestein.
Unused fields: none linked.
Existing task matches: none.

### G432: RW 0x004BD9A7

Stops: S-097. Kinds: code.
Functions: RW 0x004BD9A7 (ported), RW 0x004BDED7 (ported). Classes: DrawableLaunchBones, W3DModelBones, W3DScriptedModelDraw.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: W3DScriptedModelDraw.BirthFadeAdditive, W3DScriptedModelDraw.HighDetailOnly, W3DScriptedModelDraw.MinLODRequired, W3DScriptedModelDraw.UseDefaultAnimation, W3DScriptedModelDraw.UseProducerTexture.
Existing task matches: #569 Port 245: W3DScriptedModelDraw: MinLODRequired, HighDetailOnly, UseProducerTexture, UseDefaultAnimation, BirthFadeAdditive.

### G433: RW 0x00873F30

Stops: S-118. Kinds: code.
Functions: RW 0x00873F30 (ported), RW 0x00875847 (ported), RW 0x00877751 (ported). Classes: AODHordeContain, HordeContain, HordeContainCore, ProductionQueueHordeContain.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: HordeContain.LivingWorldOverloadTemplate, HordeContain.RanksToJustFreeWhenAttacking.
Existing task matches: #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate.

### G434: linked stops S-1712

Stops: S-1712. Kinds: code.
Functions: RW 0x006AD2B5 (ported). Classes: ActiveBody, BodyModuleInterface.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: ActiveBody.EnteringDamagedTransitionTime, ActiveBody.ReallyDamagedAttributeModifier.
Existing task matches: #567 Port 243: Body damage states: ActiveBody EnteringDamagedTransitionTime and the Damaged / ReallyDamaged attribute modifiers (ActiveBody, RespawnBody).

### G435: RW 0x008A3DD5

Stops: S-2003, S-894. Kinds: code.
Functions: RW 0x008A3DD5 (ported). Classes: AIUpdateInterface, AIWorld, HordeContain, QueueProductionExitUpdate.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: HordeContain.LivingWorldOverloadTemplate, HordeContain.RanksToJustFreeWhenAttacking.
Existing task matches: #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate.

### G436: RubbleRiseUpdate

Stops: S-346. Kinds: code.
Functions: RW 0x006D332C (ported), RW 0x008A83CE (ported), RW 0x008A83F6 (ported), RW 0x008A847C (ported), RW 0x008A8547 (ported), RW 0x008A85D0 (ported). Classes: RubbleRiseUpdate.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: RubbleRiseUpdate.MaxRubbleRiseDelay, RubbleRiseUpdate.MinRubbleRiseDelay.
Existing task matches: none.

### G437: ActiveBody

Stops: S-681. Kinds: code.
Functions: RW 0x005F0126 (ported), RW 0x008C2F02 (ported). Classes: ActiveBody, DamageFX.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: ActiveBody.EnteringDamagedTransitionTime, ActiveBody.ReallyDamagedAttributeModifier.
Existing task matches: #567 Port 243: Body damage states: ActiveBody EnteringDamagedTransitionTime and the Damaged / ReallyDamaged attribute modifiers (ActiveBody, RespawnBody).

### G438: SlowDeathBehavior

Stops: S-324. Kinds: code.
Functions: RW 0x008609A8 (ported), RW 0x00860B39 (ported), RW 0x00860E93 (ported), RW 0x00861712 (ported). Classes: CreateObjectDie, DestroyDie, FXListDie, FireWeaponWhenDeadBehavior, HeroDie, KeepObjectDie, SlowDeathBehavior, UpgradeDie.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: CreateObjectDie.DebrisPortionOfSelf.
Existing task matches: none.

### G439: linked stops S-4192, S-5011

Stops: S-4192, S-5011. Kinds: code.
Functions: none resolved. Classes: AICommandSink, ClientEvent, LargeGroupBonusUpdate.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: LargeGroupBonusUpdate.AlliesOnly.
Existing task matches: #700 Port 308: Small module gaps with candidate readers вЂ” LargeGroupBonusUpdate.AlliesOnly, W3DLightDraw.Radius, EvaAnnounceClientCreate.CreateFakeRadarEvent and their classes' uncited functions.

### G440: CreateObjectDie

Stops: S-7236. Kinds: project.
Functions: RW 0x00888F04 (ported). Classes: CreateObjectDie, CreateObjectDieModuleData.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: CreateObjectDie.DebrisPortionOfSelf.
Existing task matches: none.

### G441: linked stops S-003

Stops: S-003. Kinds: code.
Functions: none resolved. Classes: none.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G442: linked stops S-004

Stops: S-004. Kinds: code.
Functions: none resolved. Classes: none.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G443: linked stops S-006

Stops: S-006. Kinds: code.
Functions: none resolved. Classes: AptNativeHash.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G444: linked stops S-007

Stops: S-007. Kinds: code.
Functions: none resolved. Classes: AptValue.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G445: linked stops S-010

Stops: S-010. Kinds: code.
Functions: none resolved. Classes: WindowManager.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G446: linked stops S-011

Stops: S-011. Kinds: code.
Functions: none resolved. Classes: none.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G447: linked stops S-012

Stops: S-012. Kinds: code.
Functions: none resolved. Classes: ArchiveHandle.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G448: linked stops S-015

Stops: S-015. Kinds: code.
Functions: none resolved. Classes: AptObject.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G449: linked stops S-016, S-042

Stops: S-016, S-042. Kinds: code.
Functions: none resolved. Classes: none.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G450: linked stops S-020, S-021

Stops: S-020, S-021, S-029. Kinds: code.
Functions: none resolved. Classes: none.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G451: linked stops S-027

Stops: S-027. Kinds: code.
Functions: none resolved. Classes: Random4Class.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G452: linked stops S-028, S-460

Stops: S-028, S-460. Kinds: code.
Functions: RW 0x004C34A2 (ported), RW 0x006756A1 (ported). Classes: DrawableLaunchBones, W3DSupplyDraw.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G453: linked stops S-030

Stops: S-030. Kinds: code.
Functions: none resolved. Classes: WorldHeightMap.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G454: linked stops S-038

Stops: S-038. Kinds: code.
Functions: none resolved. Classes: SidesList.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G455: linked stops S-039

Stops: S-039. Kinds: code.
Functions: none resolved. Classes: none.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G456: linked stops S-060

Stops: S-060. Kinds: code.
Functions: none resolved. Classes: BaseHeightMapRenderObjClass, TerrainLogic.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G457: linked stops S-070

Stops: S-070. Kinds: code.
Functions: none resolved. Classes: ModuleData, ModuleFactory, RawModuleData.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G458: linked stops S-083

Stops: S-083. Kinds: code.
Functions: none resolved. Classes: AudioIniState, ObjectFilter, RetailObjectWorld.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G459: linked stops S-100, S-109

Stops: S-100, S-109, S-136. Kinds: code.
Functions: none resolved. Classes: none.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G460: linked stops S-102

Stops: S-102. Kinds: code.
Functions: none resolved. Classes: AptSpriteInst.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G461: linked stops S-108, S-130

Stops: S-108, S-130, S-137. Kinds: code.
Functions: none resolved. Classes: AptMenuPlayer.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G462: linked stops S-114

Stops: S-114. Kinds: code.
Functions: none resolved. Classes: GlobalData.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G463: RW 0x00608D0B

Stops: S-1184. Kinds: code.
Functions: RW 0x00608D0B (ported), RW 0x006D32E4 (ported). Classes: none.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G464: linked stops S-133

Stops: S-133. Kinds: code.
Functions: none resolved. Classes: AptFile.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G465: linked stops S-1335

Stops: S-1335. Kinds: code.
Functions: none resolved. Classes: LivingWorld, MapInfo, W3DInstancer, W3DModelBuilder.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G466: linked stops S-134

Stops: S-134. Kinds: code.
Functions: none resolved. Classes: none.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G467: linked stops S-138

Stops: S-138. Kinds: code.
Functions: none resolved. Classes: AptMainMenu, AptMapPreview, WindowManager.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G468: linked stops S-146

Stops: S-146. Kinds: code.
Functions: none resolved. Classes: PlayerList.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G469: RW 0x00674B1F

Stops: S-151. Kinds: code.
Functions: RW 0x00674B1F (ported), RW 0x006765B9 (ported). Classes: LiveGame, LogicSnapshot.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G470: linked stops S-1540

Stops: S-1540. Kinds: code.
Functions: none resolved. Classes: none.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G471: linked stops S-1541, S-232

Stops: S-1541, S-232. Kinds: code, project.
Functions: none resolved. Classes: none.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G472: linked stops S-1561

Stops: S-1561. Kinds: code.
Functions: none resolved. Classes: RetailArchivePolicy.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G473: RW 0x004BE587

Stops: S-1580. Kinds: code.
Functions: RW 0x004BE587 (ported). Classes: none.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G474: linked stops S-170, S-175

Stops: S-170, S-175. Kinds: code.
Functions: none resolved. Classes: AptMainMenu, AptStats, PlayerList, PlayerTemplate, ShellRequest.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G475: linked stops S-171, S-5140

Stops: S-171, S-5140. Kinds: code.
Functions: none resolved. Classes: WindowManager.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G476: linked stops S-1730

Stops: S-1730. Kinds: code.
Functions: none resolved. Classes: W3DInstancer.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G477: linked stops S-174

Stops: S-174. Kinds: code.
Functions: none resolved. Classes: AptHost.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G478: linked stops S-176

Stops: S-176. Kinds: code.
Functions: none resolved. Classes: DefaultFontMetrics, FontMetricsSource, GameFont, GameWindowManager, GlobalLanguage.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G479: W3DTruckDraw

Stops: S-1792. Kinds: code.
Functions: RW 0x0046E918 (ported), RW 0x004CBFFB (ported), RW 0x0068B34C (ported). Classes: W3DTruckDraw.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G480: linked stops S-181

Stops: S-181. Kinds: code.
Functions: none resolved. Classes: DamageFX, ParticleSystem, WeaponReferenceHost, WeaponStore.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G481: RW 0x0063F3BF

Stops: S-182. Kinds: code.
Functions: RW 0x0063F3BF (ported). Classes: SubsystemInterface, WeaponBonus, WeaponBonusSet, WeaponStore.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G482: linked stops S-184

Stops: S-184. Kinds: code.
Functions: RW 0x008E326D (ported). Classes: FiringTracker.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G483: linked stops S-187

Stops: S-187. Kinds: code.
Functions: none resolved. Classes: GrabNugget.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G484: RW 0x006628CD

Stops: S-1872. Kinds: code.
Functions: RW 0x006628CD (ported), RW 0x006EACB9 (ported), RW 0x00765522 (ported), RW 0x0082C366 (ported). Classes: ActionManager.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G485: linked stops S-188

Stops: S-188. Kinds: code.
Functions: none resolved. Classes: WeaponSet.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G486: linked stops S-1890

Stops: S-1890. Kinds: code.
Functions: none resolved. Classes: NetImpairment.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G487: linked stops S-1891

Stops: S-1891. Kinds: code.
Functions: none resolved. Classes: none.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G488: RW 0x0089C1A4

Stops: S-1942. Kinds: code.
Functions: RW 0x0089C1A4 (ported), RW 0x0089C261 (ported). Classes: GateOpenAndCloseBehavior.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G489: RW 0x0067055A

Stops: S-1951. Kinds: code.
Functions: RW 0x0067055A (ported), RW 0x0068D8F7 (ported). Classes: HordeContainCore, InGameUI.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G490: ReplaceSelfUpgrade

Stops: S-1986. Kinds: code.
Functions: RW 0x0068BE59 (ported), RW 0x0068C3E6 (ported), RW 0x008BB23F (ported), RW 0x008BB5FA (ported), RW 0x00AD22F0 (ported). Classes: GateOpenAndCloseBehavior, ReplaceSelfUpgrade.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G491: linked stops S-1997

Stops: S-1997. Kinds: code.
Functions: none resolved. Classes: HordeAttackNugget.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G492: linked stops S-2004

Stops: S-2004. Kinds: code.
Functions: none resolved. Classes: AttributeModifierNugget, DOTNugget, DamageNugget.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G493: linked stops S-2005

Stops: S-2005. Kinds: code.
Functions: RW 0x006ED843 (ported). Classes: none.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G494: linked stops S-204

Stops: S-204. Kinds: code.
Functions: RW 0x006AD8A7 (ported). Classes: CostModifierUpgrade, ProductionModifier.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: #766 Local 100: upgrade modules: CommandSetUpgrade (515 modules), CostModifierUpgrade, CastleUpgrade, GiveUpgradeUpdate, AudioLoopUpgrade, WallUpgradeUpdate, LocomotorSetUpgrade: 27 uncited own functions.

### G495: RW 0x006AAB85

Stops: S-208. Kinds: code.
Functions: RW 0x006AAB85 (ported). Classes: AIGroup.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G496: RW 0x0070BCE7

Stops: S-209. Kinds: code.
Functions: RW 0x0070BCE7 (ported), RW 0x008A7363 (ported), RW 0x008A74B7 (ported), RW 0x008A9DBA (ported), RW 0x00B261F0 (ported). Classes: SpawnPointProductionExitUpdate, SupplyCenterProductionExitUpdate.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G497: linked stops S-231

Stops: S-231. Kinds: code.
Functions: none resolved. Classes: HordeContainCore, LiveGame, MapChunks, TerrainLogic, TerrainPathfindSource.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G498: linked stops S-234

Stops: S-234. Kinds: code.
Functions: none resolved. Classes: GameInfo, GameMessage.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G499: RW 0x0067B4B7

Stops: S-2457. Kinds: code.
Functions: RW 0x0067B4B7 (ported), RW 0x006968BC (ported). Classes: InGameHud.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: #748 Port 312: Radar: the hidden flag and its script actions, the stealth blink's two tests, the attack notice's control flow (S-2453, S-2454, S-2457).

### G500: linked stops S-249

Stops: S-249. Kinds: code.
Functions: none resolved. Classes: AudioSettings.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G501: RW 0x0089DB5D

Stops: S-253, S-256. Kinds: code.
Functions: RW 0x0046E918 (ported), RW 0x0068C818 (ported), RW 0x0068C82D (ported), RW 0x0068F1A8 (ported), RW 0x0068F259 (ported), RW 0x006A7C01 (ported), RW 0x006AC06F (ported), RW 0x006AD8A7 (ported), RW 0x0079D833 (ported), RW 0x007A075A (ported), RW 0x008854D3 (ported), RW 0x0089DB5D (ported), RW 0x008A4574 (ported). Classes: AutoDepositUpdate, CostModifierUpgrade, ExperienceTracker, ExperienceWorld.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: #766 Local 100: upgrade modules: CommandSetUpgrade (515 modules), CostModifierUpgrade, CastleUpgrade, GiveUpgradeUpdate, AudioLoopUpgrade, WallUpgradeUpdate, LocomotorSetUpgrade: 27 uncited own functions.

### G502: RW 0x00642002

Stops: S-255. Kinds: code.
Functions: RW 0x00642002 (ported). Classes: GlobalData.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G503: RW 0x006A8630

Stops: S-259. Kinds: code.
Functions: RW 0x006A8630 (ported). Classes: none.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G504: linked stops S-2704

Stops: S-2704. Kinds: code.
Functions: none resolved. Classes: AptLoader.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G505: linked stops S-274

Stops: S-274. Kinds: code.
Functions: none resolved. Classes: AptPalantir, AptScreenFactoryTable.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G506: linked stops S-284

Stops: S-284. Kinds: code.
Functions: none resolved. Classes: HudContext, HudInput, InGameHud.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G507: linked stops S-290

Stops: S-290. Kinds: code.
Functions: none resolved. Classes: ControlBar.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G508: RW 0x0068F430

Stops: S-320. Kinds: code.
Functions: RW 0x0068F430 (ported), RW 0x006ED843 (ported), RW 0x0073D89F (ported). Classes: WeaponSet, WeaponStatusHelper.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G509: StructureCollapseUpdate

Stops: S-340. Kinds: code.
Functions: RW 0x008A7D1D (ported). Classes: StructureCollapseUpdate, StructureToppleUpdate.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G510: linked stops S-3403

Stops: S-3403. Kinds: code.
Functions: none resolved. Classes: AptOptionsScreen, OpenBFMEOptionsPage.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G511: RW 0x00936B7D

Stops: S-341. Kinds: code.
Functions: RW 0x00936B7D (ported), RW 0x00AD14F0 (ported), RW 0x00AD1920 (ported), RW 0x00AD1D60 (ported), RW 0x00AD2700 (ported), RW 0x00AD2770 (ported), RW 0x00AD2860 (ported). Classes: GeometryUpgrade.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G512: linked stops S-3507

Stops: S-3507. Kinds: code.
Functions: none resolved. Classes: InGameHud.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G513: RW 0x00441C56

Stops: S-361. Kinds: code.
Functions: RW 0x00441C56 (ported), RW 0x00960E8D (ported), RW 0x009612A3 (ported), RW 0x009D6A35 (ported). Classes: BezierSegment.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G514: RW 0x00690BD2

Stops: S-362. Kinds: code.
Functions: RW 0x00690BD2 (ported), RW 0x006CB81A (ported), RW 0x006CB85A (ported), RW 0x00AD30E0 (ported). Classes: none.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G515: linked stops S-3711

Stops: S-3711. Kinds: code.
Functions: none resolved. Classes: NetAddress.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G516: RW 0x00862A88

Stops: S-3800. Kinds: code.
Functions: RW 0x00862A88 (ported), RW 0x00862B07 (ported), RW 0x008633A4 (ported). Classes: LiveGame, SpawnBehavior, SpawnPointProductionExitUpdate.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: #465 Port 198: SpawnBehavior: the spawn interface's attack / idle orders and Player::onUnitCreated (S-1425).

### G517: linked stops S-4000

Stops: S-4000. Kinds: code.
Functions: none resolved. Classes: CommandLogLoad, SaveGameFile.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G518: linked stops S-4001

Stops: S-4001. Kinds: code.
Functions: none resolved. Classes: FrameBatch, ReplayPlayback.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G519: linked stops S-4002

Stops: S-4002. Kinds: code.
Functions: none resolved. Classes: GameWorld, NewGameMessage.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G520: RW 0x00667416

Stops: S-4193. Kinds: code.
Functions: RW 0x00667416 (ported). Classes: AIUpdateInterface, AIWorld, DozerAIUpdate, GiantBirdAIUpdate, HordeAIUpdate, UpdateModule.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: #787 Local 109: builders and walls, second pass: WorkerAIUpdate, DozerAIUpdate, WallHubBehavior, ManTheWallsSpecialPower вЂ” 34 functions; #650 Port 283: HorseHordeContain and HordeAIUpdate: the cavalry horde's uncited own functions (classify, then port the missing ones).

### G521: linked stops S-4222, S-5013

Stops: S-4222, S-5013. Kinds: code, project.
Functions: none resolved. Classes: AIWorld.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G522: linked stops S-4280, S-810

Stops: S-4280, S-810. Kinds: code.
Functions: none resolved. Classes: GameWorld, InGameHudNode, LiveGame, LiveGameFrameDriver, LockstepDriver, LogicSnapshot, ModuleFactory, ReplayWriter, ShroudManager, TacticalCamera.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G523: RW 0x00441C56

Stops: S-452. Kinds: code.
Functions: RW 0x00441C56 (ported). Classes: none.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G524: linked stops S-4550

Stops: S-4550. Kinds: code.
Functions: none resolved. Classes: AptMessenger.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G525: linked stops S-4752

Stops: S-4752. Kinds: code.
Functions: none resolved. Classes: AptLanLobby.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G526: linked stops S-4753

Stops: S-4753. Kinds: code.
Functions: none resolved. Classes: AptLanLobby.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G527: linked stops S-4850

Stops: S-4850. Kinds: code.
Functions: none resolved. Classes: AptCanvasInputs, GameWindow, PlatformKind, TextBadge.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G528: linked stops S-4860

Stops: S-4860. Kinds: code.
Functions: none resolved. Classes: GameInfo, GameWorld, OnlineLobbyClient.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G529: linked stops S-4874

Stops: S-4874. Kinds: code.
Functions: none resolved. Classes: none.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G530: linked stops S-4876

Stops: S-4876. Kinds: code.
Functions: none resolved. Classes: PlayerTemplate.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G531: linked stops S-4878

Stops: S-4878. Kinds: code.
Functions: none resolved. Classes: GameWorld.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G532: linked stops S-4931

Stops: S-4931. Kinds: code.
Functions: none resolved. Classes: LiveGame.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G533: linked stops S-4974

Stops: S-4974. Kinds: code.
Functions: none resolved. Classes: W3DFloorDraw, W3DMaterialFactory.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: #570 Port 246: Client fades: W3DTreeDraw FadeDistance / FadeTarget, W3DFloorDraw FloorFadeRateOnObjectDeath, SubObjectsUpgrade WaitBeforeFadeInSeconds.

### G534: linked stops S-4981

Stops: S-4981. Kinds: code.
Functions: none resolved. Classes: MenuNavigator.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G535: linked stops S-5014

Stops: S-5014. Kinds: code.
Functions: none resolved. Classes: TeamFactory.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G536: linked stops S-5016

Stops: S-5016. Kinds: code.
Functions: none resolved. Classes: AptPlayer, MappedImageCollection, PlacementIcons, TerrainResourceBehavior, TerrainResourceFeedback.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G537: linked stops S-5017

Stops: S-5017. Kinds: code.
Functions: none resolved. Classes: InGameUI, LivingWorldStores, ThingTemplate.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G538: linked stops S-5141

Stops: S-5141. Kinds: code.
Functions: none resolved. Classes: GameInfo.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G539: linked stops S-5143

Stops: S-5143. Kinds: code.
Functions: none resolved. Classes: AptOnlineShell.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G540: DoCommandUpgrade

Stops: S-5145. Kinds: code.
Functions: RW 0x0068E5A3 (ported), RW 0x00695338 (ported), RW 0x00696FD2 (ported), RW 0x008B8DFC (ported), RW 0x008B8E2E (ported). Classes: DoCommandUpgrade.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G541: linked stops S-5204

Stops: S-5204. Kinds: code.
Functions: none resolved. Classes: DrawEntry, W3DModelDraw.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G542: linked stops S-5306

Stops: S-5306. Kinds: code.
Functions: none resolved. Classes: GameWorld.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G543: AimWeaponBehavior

Stops: S-5607. Kinds: code.
Functions: RW 0x0070E013 (ported), RW 0x0085E30B (ported). Classes: AimWeaponBehavior.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G544: RW 0x0082C167

Stops: S-565. Kinds: code.
Functions: RW 0x0082C167 (ported). Classes: ActionManager, ObjectWeapons.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G545: linked stops S-566

Stops: S-566. Kinds: code.
Functions: none resolved. Classes: none.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G546: RW 0x004BE587

Stops: S-5920. Kinds: code.
Functions: RW 0x004BE587 (ported). Classes: none.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G547: BoredUpdate

Stops: S-6000. Kinds: code.
Functions: RW 0x0082D5DA (ported). Classes: ActionManager, BoredUpdate.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G548: AODCrushCollide

Stops: S-6007. Kinds: code.
Functions: RW 0x0087DEFB (ported), RW 0x0087E894 (ported). Classes: AODCrushCollide, AODHordeContain, HordeContainInterface.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G549: linked stops S-6009

Stops: S-6009. Kinds: code.
Functions: none resolved. Classes: none.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G550: AnnounceBirthAndDeathBehavior

Stops: S-6012. Kinds: code.
Functions: RW 0x006A03A9 (ported), RW 0x00888439 (ported), RW 0x00888453 (ported). Classes: AnnounceBirthAndDeathBehavior, InGameUI.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G551: TooltipUpgrade

Stops: S-6013. Kinds: code.
Functions: RW 0x00674FAE (ported), RW 0x006796DA (ported), RW 0x008BBBCC (ported), RW 0x008BBC0A (ported). Classes: TooltipUpgrade, UnitHelpSource.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G552: linked stops S-6101

Stops: S-6101. Kinds: code.
Functions: none resolved. Classes: InGameUIMessages.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G553: RW 0x008DE522

Stops: S-705. Kinds: code.
Functions: RW 0x00688EEC (ported), RW 0x008DD663 (ported), RW 0x008DE522 (ported). Classes: ExperienceTracker.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G554: W3DDebrisDraw

Stops: S-7101. Kinds: code.
Functions: RW 0x004B15BC (ported). Classes: W3DDebrisDraw.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G555: linked stops S-7202

Stops: S-7202. Kinds: code.
Functions: none resolved. Classes: DrawableManager, W3DModelDraw.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G556: linked stops S-725

Stops: S-725. Kinds: code.
Functions: none resolved. Classes: none.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G557: RW 0x004C72BE

Stops: S-7287. Kinds: code.
Functions: RW 0x004C72BE (ported). Classes: GameWorld, HudDevice, W3DModelDraw.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G558: linked stops S-7364

Stops: S-7364. Kinds: code.
Functions: none resolved. Classes: ControlBar.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G559: linked stops S-7385

Stops: S-7385. Kinds: code.
Functions: none resolved. Classes: ParabolicEase, ScriptCameraDirector, TacticalCamera, TacticalView.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G560: InactiveBody

Stops: S-7548. Kinds: code.
Functions: RW 0x008C1A75 (ported), RW 0x008C1AEA (ported), RW 0x00993419 (ported). Classes: InactiveBody.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G561: HordeGarrisonContain

Stops: S-7606. Kinds: code.
Functions: RW 0x0087A3A4 (ported). Classes: HordeGarrisonContain, HordeSiegeEngineContain, HordeTransportContain, OpenContain, SlaughterHordeContain, TunnelContain.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G562: HordeSiegeEngineContain

Stops: S-7607. Kinds: code.
Functions: RW 0x0087F0CF (ported), RW 0x0087FF18 (ported). Classes: HordeSiegeEngineContain, SiegeEngineContain.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G563: linked stops S-815

Stops: S-815. Kinds: code.
Functions: none resolved. Classes: GameWorld, LevelUpFx.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

### G564: linked stops S-831

Stops: S-831. Kinds: code.
Functions: none resolved. Classes: GameWorld, GammaCompositeHost, MapTerrainBuilder.
Candidates: 0 none/partial functions; 0 unknown status.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.


## Unplaced

S-001, S-005, S-008, S-009, S-013, S-014, S-024, S-025, S-026, S-032, S-034, S-040, S-101, S-103, S-105, S-106, S-122, S-125, S-131, S-132, S-1542, S-180, S-1923, S-1931, S-199, S-230, S-244, S-246, S-3710, S-4004, S-4020, S-4077, S-4100, S-4101, S-4102, S-4103, S-4482, S-455, S-4554, S-456, S-4700, S-4701, S-4702, S-4830, S-4831, S-4871, S-4872, S-4875, S-4877, S-4879, S-4880, S-4930, S-5144, S-706, S-7363, S-7434, S-7502

## Top 30 task drafts

### Port N: close mechanism RW 0x005D893C: stops S-185, S-322, S-636

#### Problem
3 open stops may share a mechanism. Verify each link; do not assume the grouping proves ownership.
- S-185 (code): Armour stacking — The two ARMOR attribute-modifier reads in `AdjustDamage` (RW 0x5D8A64, 0x5D8AF1) call into `.danetta` in the installed image (stacking `1-(1-a)(1-b)`); the port implements the additive sum of the unpatched getter and BFME2 1.06. Identical for one modifier..
- S-322 (code): ActiveBody — armour is the ArmorSet of the object's armor flags looked up per hit (RW 0x5D893C); the flank test (RW 0x68FB63), the ARMOR / INVULNERABLE attribute modifiers, doDamageFX, the damage modules' onDamage are not ported; lane LOCAL-1 (issue #88): GameData AttributeModifierArmorMaxBonus (row RW 0xC00490, GlobalData + 0xAE8) is loaded by GameLogicSettingsLoader and passed to AdjustDamage via ActiveBody::DamageHost::armorMaxBonus(), replacing the hardco.
- S-636 (code): Armour attribute queries (XP-1) — AdjustDamage's INVULNERABLE and ARMOR queries (RW 0x5D8A64, 0x5D8AF1) call into the community-added `.danetta` section of the available binary; they are ported as the victim's pool sums with the damage type's name (Damage.h's contract), unverified against a clean 2.01.

#### Known
Kinds: code. Classes: AIBurningDeathState, ActiveBody, DamageHost, DelayedDeathBody, GlobalData, HighlanderBody, ImmortalBody, InactiveBody, RespawnBody, StructureBody, SymbioticStructuresBody.
Functions: RW 0x005D893C (unknown), RW 0x0068FB63 (ported), RW 0x008C3FA3 (ported).
Addresses: S-185: RW 0x005D8A64, RW 0x005D8AF1; S-322: RW 0x005D893C, RW 0x0068FB63, RW 0x008C3FE7, RW 0x008C4012, RW 0x008C4169, RW 0x008C4214, RW 0x008C476C, RW 0x008C47BD, RW 0x00C00490; S-636: RW 0x005D8A64, RW 0x005D8AF1.
Red buttons: none linked.
Unused fields: ActiveBody.EnteringDamagedTransitionTime, ActiveBody.ReallyDamagedAttributeModifier, RespawnBody.DamagedAttributeModifier, RespawnBody.ReallyDamagedAttributeModifier, SymbioticStructuresBody.Symbiote.
Existing task matches: #567 Port 243: Body damage states: ActiveBody EnteringDamagedTransitionTime and the Damaged / ReallyDamaged attribute modifiers (ActiveBody, RespawnBody).

#### Task
Trace the RotWK mechanism and callers, implement only verified missing behavior, and close or narrow the relevant stops.

#### Tests
Add synthetic regression tests for the verified behavior and adjacent branches; run the full test target.

#### In game
Choose a verified affected Ennorath object and document reproduction and expected retail behavior.

#### Do not touch
Unrelated stops, other open task areas, game files and generated source maps.

#### Done
RW instructions and tests justify each change; report any remaining stops and inference.

### Port N: close mechanism ToggleHiddenSpecialAbilityUpdate: stops S-1043, S-7605

#### Problem
2 open stops may share a mechanism. Verify each link; do not assume the grouping proves ownership.
- S-1043 (code): Stealth abilities (STEALTH-2) — ToggleHiddenSpecialAbilityUpdate (the elven cloak, the hobbits' and Wormtongue's escapes: RW 0x8B1909 trigger, RW 0x8B1A70 hide, RW 0x8B19AC unhide, RW 0x8B1962 EffectDuration, untoggled by RW 0x81B3D3 / 0x7760B7) and InvisibilitySpecialPower (Move Unseen: RW 0x8C66EB at a location, RW 0x8C65D2 at an object) run. Not ported: ShowPalantirTimer and the EffectDuration report to the player (the palantir timer, client); the hide's room test for the ga.
- S-7605 (code): ToggleHiddenSpecialAbilityUpdate: two small differences (Port 279, #646) — Found by Port 279's line-by-line check against RotWK 2.01 (the class otherwise matches: hide / unhide RW 0x8B1A70 / 0x8B19AC, update RW 0x8B1962, triggerAbilityEffect RW 0x8B1909): (1) `canHide()` answers false for the special enums 0x27 / 0x28 (S-1043) where RW checks the free slots of the object's Contain (RW 0x851B94 .. 0x851BC6); (2) hide / unhide set WEAPONSET_HIDDEN only through the object's weapons (`if (ObjectWeapons *w = obj->getWeapons(.

#### Known
Kinds: code. Classes: InvisibilitySpecialPower, ObjectWeapons, ToggleHiddenSpecialAbilityUpdate.
Functions: RW 0x00691059 (ported), RW 0x00691106 (ported), RW 0x007760B7 (ported), RW 0x007A1C0D (unknown), RW 0x0081B376 (ported), RW 0x00851B6E (ported), RW 0x008B1909 (ported), RW 0x008B1962 (ported), RW 0x008B19AC (ported), RW 0x008B1A70 (ported), RW 0x008C65D2 (ported), RW 0x008C66EB (ported).
Addresses: S-1043: RW 0x007760B7, RW 0x007A1C0D, RW 0x0081B3D3, RW 0x00851BA2, RW 0x008B1909, RW 0x008B1962, RW 0x008B19AC, RW 0x008B1A70, RW 0x008C65D2, RW 0x008C66EB; S-7605: RW 0x00691059, RW 0x00691106, RW 0x00851B94, RW 0x00851BC6, RW 0x008B1909, RW 0x008B1962, RW 0x008B19AC, RW 0x008B1A70.
Red buttons: Command_SpecialAbilityGeheimeWege, Command_SpecialAbilityHaradScoutCloak.
Unused fields: none linked.
Existing task matches: none.

#### Task
Trace the RotWK mechanism and callers, implement only verified missing behavior, and close or narrow the relevant stops.

#### Tests
Add synthetic regression tests for the verified behavior and adjacent branches; run the full test target.

#### In game
Choose a verified affected Ennorath object and document reproduction and expected retail behavior.

#### Do not touch
Unrelated stops, other open task areas, game files and generated source maps.

#### Done
RW instructions and tests justify each change; report any remaining stops and inference.

### Port N: close mechanism DynamicPortalBehaviour: stops S-4854, S-4855

#### Problem
2 open stops may share a mechanism. Verify each link; do not assume the grouping proves ownership.
- S-4854 (code): DynamicPortalBehaviour WallBoundsMesh (lane WALL-2) — upgradeImplementation's WallBoundsMesh branch (RW 0x86436B .. 0x8643BE: the drawable takes the named mesh, RW 0x672F68, and the object leaves and enters the pathfinder's map, RW 0x6E85FB / 0x6E85E9) is not ported: no retail DynamicPortalBehaviour sets WallBoundsMesh; a module that does is reported.
- S-4855 (code): DynamicPortalBehaviour and the castle upgrade plot (lane WALL-2) — With an AI, upgradeImplementation and removal hand the object's id (or 0) to its FoundationAIUpdate interface's slot 2 (RW 0x8643CA / 0x863D56 -> RW 0x8582DE: plot occupant, status and drawable effects); the real castle-upgrade path reaches it (GondorCastleUpgrade / RohanCastleUpgrade with Upgrade_PosternGate), and `DynamicPortalBehaviour::foundationOccupant` currently reports it (`PortalModules.cpp:455`). `Construction::constructOnPlot` (S-303) .

#### Known
Kinds: code. Classes: CastleMemberBehavior, DynamicPortalBehaviour, FoundationAIUpdate.
Functions: RW 0x00672F68 (partial), RW 0x006E85E9 (ported), RW 0x006E85FB (ported), RW 0x008582DE (ported), RW 0x00863D3A (ported), RW 0x00864357 (ported).
Addresses: S-4854: RW 0x00672F68, RW 0x006E85E9, RW 0x006E85FB, RW 0x0086436B, RW 0x008643BE; S-4855: RW 0x008582DE, RW 0x00863D56, RW 0x008643CA.
Red buttons: none linked.
Unused fields: CastleMemberBehavior.StoreUpgradePrice.
Existing task matches: #688 Port 296: Castles вЂ” CastleBehavior, CastleMemberBehavior and FoundationAIUpdate: the uncited own functions (27) and their five unread fields.

#### Task
Trace the RotWK mechanism and callers, implement only verified missing behavior, and close or narrow the relevant stops.

#### Tests
Add synthetic regression tests for the verified behavior and adjacent branches; run the full test target.

#### In game
Choose a verified affected Ennorath object and document reproduction and expected retail behavior.

#### Do not touch
Unrelated stops, other open task areas, game files and generated source maps.

#### Done
RW instructions and tests justify each change; report any remaining stops and inference.

### Port N: close mechanism linked stops S-017, S-123: stops S-017, S-123

#### Problem
2 open stops may share a mechanism. Verify each link; do not assume the grouping proves ownership.
- S-017 (code): Apt atof — Retail MSVCR71 atof differs from correct rounding for decimal texts whose exact value lies in [2e-324, DBL_MIN): it returns TWICE the value for exact powers of two there (and 0 for 2^-1023), the exact decimal of 2^-1075 is 0x1 in retail but 0 in the engine (Boolean true vs false), and 2.225073858507201259573821257020768020077017763406988739288377e-308 is 0x000FFFFFFFFFFFFF in retail but 0x0010000000000000 in the engine. The cause inside the CRT i.
- S-123 (code): Lua numerics — Script arithmetic (`+ - * /` and the `for` step) is rounded to a 24 bit significand like retail's x87 at precision control 24 (setFPMode RW 0x440809; one rounding of the exact result, checked against the hardware-verified NumericState). That the game runs at PC24 while scripts execute is assumed, not traced; `LUA_EA_NUM_DOUBLE` selects plain doubles. Number text is MSVCR71's %.16g (3 digit exponents, 1.#INF, 1.#QNAN, -1.#IND: CRT facts, the binar.

#### Known
Kinds: code. Classes: none.
Functions: RW 0x00440809 (ported), RW 0x00B5F9F0 (unknown).
Addresses: S-017: ; S-123: RW 0x00440809, RW 0x00B5FA89, RW 0x00B5FAE1, RW 0x00D0AC04.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

#### Task
Trace the RotWK mechanism and callers, implement only verified missing behavior, and close or narrow the relevant stops.

#### Tests
Add synthetic regression tests for the verified behavior and adjacent branches; run the full test target.

#### In game
Choose a verified affected Ennorath object and document reproduction and expected retail behavior.

#### Do not touch
Unrelated stops, other open task areas, game files and generated source maps.

#### Done
RW instructions and tests justify each change; report any remaining stops and inference.

### Port N: close mechanism linked stops S-041, S-120: stops S-041, S-120

#### Problem
2 open stops may share a mechanism. Verify each link; do not assume the grouping proves ownership.
- S-041 (code): Lua type() — `type(<boolean>)` in retail Lua faults: `lua_typename` (RW 0xB5B320) indexes the 6-entry table at 0xD0B298 with EA's boolean tag 6 and dereferences the bytes of the string "table" as a `char*` (read at 0x6C626174). There is no retail result to port.
- S-120 (code): Lua fork — EA's Lua 4.0.1 is rebuilt, not copied: stock lua.org 4.0.1 (md5 a31d963d..., vendored pristine) plus a patch layer (engine/src/Libraries/Lua/README.md) reconstructed from the RotWK disassembly and Open-BFME-1's reconstruction; no EA source exists. The ORACLE-1 recordings (16 boolean results, the function counts 37/19/11/23/5, the type(boolean) fault) are the only executed retail values; the rest is read from the disassembly (jump ops, OP_NOT, OP_.

#### Known
Kinds: code. Classes: none.
Functions: RW 0x00B5B320 (unknown).
Addresses: S-041: RW 0x00B5B320; S-120: RW 0x00B63759.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

#### Task
Trace the RotWK mechanism and callers, implement only verified missing behavior, and close or narrow the relevant stops.

#### Tests
Add synthetic regression tests for the verified behavior and adjacent branches; run the full test target.

#### In game
Choose a verified affected Ennorath object and document reproduction and expected retail behavior.

#### Do not touch
Unrelated stops, other open task areas, game files and generated source maps.

#### Done
RW instructions and tests justify each change; report any remaining stops and inference.

### Port N: close mechanism linked stops S-081, S-233: stops S-081, S-233

#### Problem
2 open stops may share a mechanism. Verify each link; do not assume the grouping proves ownership.
- S-081 (code): Locomotor numerics — The locomotor movers mix x87 and SSE (`moveForward` RW 0x5E586F, the getters, Wheels' 4/pi products) and call the CRT / x87 trigonometry (`fcos` / `fsin` RW 0x42F4E0 / 0x42F4D0, CRT sin / cos / atan2). The port reproduces operation order and PC24 precision through `NumericState` and plain float32 SSE arithmetic (built with `-ffp-contract=off`), but sin / cos / atan2 are not retail's: they were the platform libm (`GameLogic/SimMath.h`) and since l.
- S-233 (code): Numeric facade bounds — `pc24*W`, `sqrtPC24` and `fildU32` are binary64-bounded carriers, not x87 registers (`(2^600 * 2^600) / 2^600` is infinity here); exact only for the audited float-derived chains. (Resolved by lane WIN-1: `atan2d` / `cosd` / `sind` / `cosf32` / `sinf32` no longer call the platform libm, which glibc and the UCRT round differently, but `NumericState::sinDD` / `cosDD` / `atan2DD`, deterministic double-double, pinned bit for bit against 5,333 correctl.

#### Known
Kinds: code. Classes: GameLogicRandom.
Functions: RW 0x0042F4D0 (ported), RW 0x0042F4E0 (ported), RW 0x005E586F (unknown).
Addresses: S-081: RW 0x0042F4D0, RW 0x0042F4E0, RW 0x005E586F, RW 0x005E58BE; S-233: .
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

#### Task
Trace the RotWK mechanism and callers, implement only verified missing behavior, and close or narrow the relevant stops.

#### Tests
Add synthetic regression tests for the verified behavior and adjacent branches; run the full test target.

#### In game
Choose a verified affected Ennorath object and document reproduction and expected retail behavior.

#### Do not touch
Unrelated stops, other open task areas, game files and generated source maps.

#### Done
RW instructions and tests justify each change; report any remaining stops and inference.

### Port N: close mechanism linked stops S-1122, S-723: stops S-1122, S-723

#### Problem
2 open stops may share a mechanism. Verify each link; do not assume the grouping proves ownership.
- S-1122 (code): Self destruct (MP-2) — MSG_SELF_DESTRUCT (logicMessageDispatcher case RW 0x77CA3D: the first mutual living ally gets the assets, else the player is killed) and Player::transferAssetsFromThat (RW 0x6AF598: objects without KindOf INERT / MOVE_ONLY / IGNORED_IN_GUI change to the ally's default team with INHERITED_FROM_ALLY_TEAM and the ally's addObjectBuilt with its counting off, a horde member's container joins, the completed upgrades but RW 0xC14034 .. 0xC14170's fourte.
- S-723 (code): Network disconnects — lane MP-2 ports RotWK's DisconnectManager (GameNetwork/DisconnectManager.h: a peer silent for NetworkDisconnectTime brings up the disconnect screen, the timeout bars run on NetworkPlayerTimeoutTime, the remaining players vote, the next packet router drops the player at the frame every peer is on, DESTROYPLAYER becomes MSG_SELF_DESTRUCT { TRUE }); a stall is still reported (`LockstepDriver::longStalls`); the open points are S-1121 and S-1122.

#### Known
Kinds: code. Classes: DisconnectManager, LockstepDriver.
Functions: RW 0x0069954A (ported), RW 0x0069ABA7 (ported), RW 0x006AF598 (unknown), RW 0x006D876A (ported), RW 0x006D9042 (ported), RW 0x00870667 (ported).
Addresses: S-1122: RW 0x0069954A, RW 0x0069ABA7, RW 0x006AF598, RW 0x006D876A, RW 0x006D9042, RW 0x0077CA3D, RW 0x00870667, RW 0x00C14034, RW 0x00C14170, RW 0x00DE4AB8; S-723: .
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

#### Task
Trace the RotWK mechanism and callers, implement only verified missing behavior, and close or narrow the relevant stops.

#### Tests
Add synthetic regression tests for the verified behavior and adjacent branches; run the full test target.

#### In game
Choose a verified affected Ennorath object and document reproduction and expected retail behavior.

#### Do not touch
Unrelated stops, other open task areas, game files and generated source maps.

#### Done
RW instructions and tests justify each change; report any remaining stops and inference.

### Port N: close mechanism RW 0x0097C819: stops S-4071, S-4750

#### Problem
2 open stops may share a mechanism. Verify each link; do not assume the grouping proves ownership.
- S-4071 (code): Online sign-in (ONLINE-4) — OnlineLogin.apt (RW 0x97C819, BFME2 0x572885 tier A) signs in with GameSpy (e-mail, nickname, password, locale). OpenBFME's lobby has only guest sign-in (docs/ONLINE.md section 3): the Nickname combo is the guest name (the last one offered again, kept in Options.ini OpenBFMEOnlineName by the game); E-mail, Password and RememberInfo are disabled; Create, Delete nickname, Register and ServiceTerms are disabled through the movie's own DisableButton*.
- S-4750 (code): Online login: nickname only (MENUWORK-1) — The owner does not want accounts yet (2026-10-14): OnlineLogin.apt (RW 0x97C819, GameSpy's account sign-in) is shown with the nickname row only. Once the page's clips are placed the engine hides the movie's account parts every update (`AptOnlineShell::loginAccountParts()`: the password and remember-me labels, the e-mail and nickname frames (one shape, InputFields depth 2), the lower box of Service terms / Official web site (innerBorder depths 3, .

#### Known
Kinds: code. Classes: AptOnlineShell.
Functions: RW 0x0097C819 (unknown).
Addresses: S-4071: RW 0x0097C819; S-4750: RW 0x0097C819.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

#### Task
Trace the RotWK mechanism and callers, implement only verified missing behavior, and close or narrow the relevant stops.

#### Tests
Add synthetic regression tests for the verified behavior and adjacent branches; run the full test target.

#### In game
Choose a verified affected Ennorath object and document reproduction and expected retail behavior.

#### Do not touch
Unrelated stops, other open task areas, game files and generated source maps.

#### Done
RW instructions and tests justify each change; report any remaining stops and inference.

### Port N: close mechanism ObjectCreationUpgrade: stops S-1985, S-1995, S-2007

#### Problem
3 open stops may share a mechanism. Verify each link; do not assume the grouping proves ownership.
- S-1985 (code): Object creation upgrade (FACT-A; the module is FACT-C's port since merge q2, whose stop is S-2007) — ObjectCreationUpgrade (RW 0x8B8281 / 0x8B8464 / 0x8B87C5) runs: the delay, ThingToSpawn built at the offset (or queued in the building's production), its fade in, its SlavedUpdate enslaved, GrantUpgrade / RemoveUpgrade. CLOSED by lane MODCOMPAT-MOD1: the master's SlaveWatcherBehavior gets the new object's id (RW 0x887AF5) and DestroyWhenSold kills the watched slave (RW 0x698EC3, with DeathAnimAndDuration, RW 0x68B581).
- S-1995 (code): Other unported modules of Isengard and Mordor (FACT-B) — ObjectCreationUpgrade (the Lava Moat) is ported by FACT-A on merge q1 and left both pins; PickupStuffUpdate (every horde) is ported by lane MODCOMPAT-MOD1 (S-1422) and left both pins; AutoPickUpUpdate (the mountain troll, the Watcher) and BoredUpdate (the mountain troll) are ported by lane MODCOMPAT-MOD1 (S-7160, S-7161) and left the Mordor pin; TransitionDamageFX is ported by lane FX-4 (S-686) and left both pins; lane MODCOMPAT-MOD1 ported Audio.
- S-2007 (code): ObjectCreationUpgrade (FACT-C) — Ported from RW 0x8B8281 (upgradeImplementation), 0x8B8464 (update), 0x8B87C5 (processUpgradeRemoval), data RW 0xC6E2F8: after Delay, ThingToSpawn made at the owner's transform applied to Offset, turned by Angle, its create modules run; or, UseBuildingProduction, queued at the owner's ProductionUpdate; then GrantUpgrade given and RemoveUpgrade taken. The Angmar spike moat, the Sanctum's readiness (its Frostball), the expansions' free wall catapult.

#### Known
Kinds: code. Classes: AimWeaponBehavior, AnnounceBirthAndDeathBehavior, AudioLoopUpgrade, AutoPickUpUpdate, BaseUpgrade, BoredUpdate, ClickReactionBehavior, EvacuateDamage, FlingPassengerSpecialAbilityUpdate, GeometryUpgrade, HordeTransportContainDamage, ObjectCreationUpgrade, PickupStuffUpdate, ProductionUpdate, RunOffMapBehavior, SiegeDeployHordeSpecialPower, SlaughterHordeContain, SlaveWatcherBehavior, SlavedUpdate, TransitionDamageFX.
Functions: RW 0x0068B581 (ported), RW 0x00698EC3 (ported), RW 0x00887857 (unknown), RW 0x00887A35 (ported), RW 0x00887AF5 (unknown), RW 0x008A1B9F (ported), RW 0x008B8281 (ported), RW 0x008B8464 (ported), RW 0x008B87C5 (ported).
Addresses: S-1985: RW 0x0068B581, RW 0x00698EC3, RW 0x00887AF5, RW 0x008B8281, RW 0x008B8464, RW 0x008B87C5; S-1995: RW 0x00887857, RW 0x00887A35, RW 0x00887AF5; S-2007: RW 0x00887AF5, RW 0x008A270E, RW 0x008B8281, RW 0x008B87C5, RW 0x00C6E2F8.
Red buttons: none linked.
Unused fields: FlingPassengerSpecialAbilityUpdate.FlingPassengerVelocity, RunOffMapBehavior.DieOnMap, RunOffMapBehavior.RunOffMapWaypointName.
Existing task matches: #766 Local 100: upgrade modules: CommandSetUpgrade (515 modules), CostModifierUpgrade, CastleUpgrade, GiveUpgradeUpdate, AudioLoopUpgrade, WallUpgradeUpdate, LocomotorSetUpgrade: 27 uncited own functions; #607 Port 259: Horde-level powers: SplitHordeSpecialPower, HordeDispatchSpecialPower, SiegeDeployHordeSpecialPower, SiegeDeploySpecialPower.

#### Task
Trace the RotWK mechanism and callers, implement only verified missing behavior, and close or narrow the relevant stops.

#### Tests
Add synthetic regression tests for the verified behavior and adjacent branches; run the full test target.

#### In game
Choose a verified affected Ennorath object and document reproduction and expected retail behavior.

#### Do not touch
Unrelated stops, other open task areas, game files and generated source maps.

#### Done
RW instructions and tests justify each change; report any remaining stops and inference.

### Port N: close mechanism linked stops S-080, S-093: stops S-080, S-093, S-189

#### Problem
3 open stops may share a mechanism. Verify each link; do not assume the grouping proves ownership.
- S-080 (code): Game-logic RNG — The RotWK `game.dat` on this machine (community modified, S-001) holds a different generator from BFME2 1.06 and ZH: `randomValue` RW 0x6D315D is `seed[0]*0x08088405+1` returned xor the product's high word (21 bytes, then 98 NOPs filling the 119-byte slot of BFME2 0x633EC3's six-word carry chain), and `seedRandom` RW 0x6D31D4 multiplies by 0x7FFFFFED and fills all six words. Whether a clean 2.01 has the carry chain (patched here) or the LCG (chan.
- S-093 (code): Draw client random — The render-side random (`W3DClientRandom`) is an instance of the shared generators of `Common/RandomValue.h` with its OWN state (never the logic generator's), and the algorithm has NO default: the constructor names `RandomAlgorithm::ZH_CarryChain` or `RotWK_GameDat_LCG`. TARGET (patched binary, S-001): RotWK's client wrappers 0x6D32E4 / 0x6D33AB call the same 0x6D315D LCG replacement (21 bytes + 98 NOPs) as the logic RNG, one community patch site.
- S-189 (code): Weapon RNG — The logic RNG algorithm of the retail 2.01 is unproven (S-080), so every weapon draw (pre-attack jitter, delay, reload, scatter pick) inherits it. `RandomValue.h` calls RW 0xDE4364 TheGameLogic; it is TheGlobalData (the logic frame is [0xDE412C]+0x40)..

#### Known
Kinds: code. Classes: RandomAlgorithm, W3DClientRandom.
Functions: RW 0x006D315D (ported), RW 0x006D31D4 (unknown), RW 0x006D3261 (unknown).
Addresses: S-080: RW 0x006D315D, RW 0x006D31D4, RW 0x006D3261; S-093: RW 0x00DE3B84; S-189: RW 0x00DE4364.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

#### Task
Trace the RotWK mechanism and callers, implement only verified missing behavior, and close or narrow the relevant stops.

#### Tests
Add synthetic regression tests for the verified behavior and adjacent branches; run the full test target.

#### In game
Choose a verified affected Ennorath object and document reproduction and expected retail behavior.

#### Do not touch
Unrelated stops, other open task areas, game files and generated source maps.

#### Done
RW instructions and tests justify each change; report any remaining stops and inference.

### Port N: close mechanism linked stops S-2006: stops S-2006

#### Problem
1 open stops may share a mechanism. Verify each link; do not assume the grouping proves ownership.
- S-2006 (code): Ability weapons without their hit (FACT-C) — Cast through their buttons (they fire: the recharge starts), these special weapons land no hit of their size on the target in 200 frames (EffectProbe DAMAGE: the first hit after the cast is the weapon's Weapon.ini x Armor.ini): the mountain giant's Stomp (WildMountainGiantStomp, MinimumAttackRange 0.8: no hit), Karsh's Whisper of Death (AngmarWhisperofDeath: its first hit is not the weapon's); the necromancers' Black Ice (a SpecialPowerModule wit.

#### Known
Kinds: code. Classes: SpecialPowerModule.
Functions: RW 0x0086C4EA (unknown).
Addresses: S-2006: RW 0x0086C4EA.
Red buttons: Command_AngmarRangersIceArrowAbility, Command_AngmarTrollChargeAttack, Command_AxtschwingerAxeThrow, Command_BeregondMountNew, Command_BigScorpionUnderground, Command_BlackRiderMorgulBladeForHorde, Command_FireDrakeBroodInferno, Command_GorkilBombThrow, Command_HalftrollChargeAttack, Command_IsenfurtWchterSmite, Command_NecromancerDeathBargain, Command_RavenPostSecondBonus, Command_SmaugIncinerateNew, Command_SpecialAbilityBalrogBreathAI, Command_SpecialAbilityBoromirHorn, Command_SpecialAbilityBoromirHornRing, Command_SpecialAbilityBoromirTapfererWiderstand, Command_SpecialAbilityBuhrdurFressen, Command_SpecialAbilityDenethorNotstandssituation, Command_SpecialAbilityDenethorSehenderStein1, Command_SpecialAbilityDenethorVerzweiflungsakt, Command_SpecialAbilityDurinMeisterGestein, Command_SpecialAbilityElendilSummoned, Command_SpecialAbilityGeheimeWege, Command_SpecialAbilityGulzarMOrgulSeucheAcolyte, Command_SpecialAbilityHaradScoutCloak, Command_SpecialAbilityKingsFavorEdainTheo, Command_SpecialAbilityMordorBalrogWhipAI, Command_SpecialAbilityNaturmagierErntesegen, Command_SpecialAbilitySaatdesgoldenenWaldes, Command_SpecialAbilityScreechDunklerMarschall, Command_SpecialAbilityTributeGainTwo, Command_ThranduilKnigDsterwalds.
Unused fields: none linked.
Existing task matches: #599 Port 251: The special-power chain every Ennorath ability runs through: SpecialPowerModule and UnpauseSpecialPowerUpgrade's uncited RotWK functions.

#### Task
Trace the RotWK mechanism and callers, implement only verified missing behavior, and close or narrow the relevant stops.

#### Tests
Add synthetic regression tests for the verified behavior and adjacent branches; run the full test target.

#### In game
Choose a verified affected Ennorath object and document reproduction and expected retail behavior.

#### Do not touch
Unrelated stops, other open task areas, game files and generated source maps.

#### Done
RW instructions and tests justify each change; report any remaining stops and inference.

### Port N: close mechanism RW 0x00853468: stops S-703, S-7224

#### Problem
2 open stops may share a mechanism. Verify each link; do not assume the grouping proves ownership.
- S-703 (code): Unit voice: message types — Message types of RW 0x8DEDBB not answered: weapon modes (0x43A), the planned action queue (0x46A), the Ring voices (0x7E4, 0x7E5). Ported: special powers (0x410 .. 0x412, 0x456, RW 0x8DF7FE) and enter state (0x7EC, RW 0x8DFCE1) via SpecialPower UnitSpecificSoundToUse* (TOP20 #11); combine hordes (0x423, RW 0x8DF2EB: no target -> nothing, the candidate's horde's ComboHorde entry for the TARGET-side horde's template supplies InitiateVoice, else row.
- S-7224 (code): The other sounds of SpecialAbilityUpdate: PrepSoundLoop, PackSound, UnpackSound, TriggerModelCondition sounds (PORT-93, issue #241) — `ActiveLoopSound` (data + 0x34) is ported (RW 0x854008 .. 0x85405B start, RW 0x85311B .. 0x853131 stop). NOT ported, same module: `PrepSoundLoop` (+ 0x10) — the retail code sits in `startPreparation` RW 0x853EAC .. 0x853EC8 (added for the object, handle kept at module + 0x34) and its removal is inside `endPreparation` (RW 0x85261D .. 0x852628); `PackSound` (+ 0x08, RW 0x853596 area) and `UnpackSound` (+ 0x0C) with their handles (+ 0x38 removed th.

#### Known
Kinds: code. Classes: SpecialAbilityUpdate.
Functions: RW 0x007546DD (unknown), RW 0x0085260A (unknown), RW 0x008530BA (ported), RW 0x00853468 (ported), RW 0x00853BEC (ported), RW 0x00853EDF (ported), RW 0x008DEDBB (ported), RW 0x008E3174 (ported).
Addresses: S-703: RW 0x007547AF, RW 0x0085368D, RW 0x008DEDBB, RW 0x008DF2EB, RW 0x008DF7FE, RW 0x008DF97F, RW 0x008DF9AD, RW 0x008DF9D2, RW 0x008DFCE1, RW 0x008DFD0D, RW 0x008DFE7B, RW 0x008E3205; S-7224: RW 0x0085261D, RW 0x00852628, RW 0x0085311B, RW 0x00853131, RW 0x00853596, RW 0x00853EAC, RW 0x00853EC8, RW 0x00854008, RW 0x0085405B.
Red buttons: Command_AngmarTrollChargeAttack, Command_SpecialAbilityLoneRangerSummonFinal, Command_SpecialAbilityNaturmagierErntesegen, Command_ThranduilKnigDsterwalds.
Unused fields: none linked.
Existing task matches: none.

#### Task
Trace the RotWK mechanism and callers, implement only verified missing behavior, and close or narrow the relevant stops.

#### Tests
Add synthetic regression tests for the verified behavior and adjacent branches; run the full test target.

#### In game
Choose a verified affected Ennorath object and document reproduction and expected retail behavior.

#### Do not touch
Unrelated stops, other open task areas, game files and generated source maps.

#### Done
RW instructions and tests justify each change; report any remaining stops and inference.

### Port N: close mechanism RW 0x0073F684: stops S-7226

#### Problem
1 open stops may share a mechanism. Verify each link; do not assume the grouping proves ownership.
- S-7226 (code): Unit voice: the post of message 0x7EA (VoiceStartCharging) (PORT-87, issue #235) — Ported (PORT-87): the dispatcher's case itself — RW 0x8DEDBB is entered with 0x7EA and reaches RW 0x8DFD0D, which pushes the literal at RW 0xC77A74 (`VoiceStartCharging`) and asks the name lookup RW 0x8DDB2A (the template's `UnitSpecificSounds`, tt + 0x388, table RW 0xDA4378, parser RW 0x73F684); the port's `UnitVoiceResponse::pickAndPlay` answers the id (`VOICE_EVENT_START_CHARGING`, `fillName`). NOT PORTED: **who posts 0x7EA in RotWK 2.01** — t.

#### Known
Kinds: code. Classes: AIAttackState, AIChargeTargetState, SpecialAbilityUpdate, UnitVoiceResponse.
Functions: RW 0x0073F684 (unknown), RW 0x00852DC5 (ported), RW 0x00853EDF (ported), RW 0x008DDB2A (ported), RW 0x008DEDBB (ported).
Addresses: S-7226: RW 0x0073F684, RW 0x00852DC5, RW 0x008547C5, RW 0x0085481A, RW 0x008DDB2A, RW 0x008DEDBB, RW 0x008DFD0D, RW 0x00C77A74, RW 0x00DA4378.
Red buttons: Command_AngmarTrollChargeAttack, Command_SpecialAbilityLoneRangerSummonFinal, Command_SpecialAbilityNaturmagierErntesegen, Command_ThranduilKnigDsterwalds.
Unused fields: none linked.
Existing task matches: none.

#### Task
Trace the RotWK mechanism and callers, implement only verified missing behavior, and close or narrow the relevant stops.

#### Tests
Add synthetic regression tests for the verified behavior and adjacent branches; run the full test target.

#### In game
Choose a verified affected Ennorath object and document reproduction and expected retail behavior.

#### Do not touch
Unrelated stops, other open task areas, game files and generated source maps.

#### Done
RW instructions and tests justify each change; report any remaining stops and inference.

### Port N: close mechanism RW 0x00797A96: stops S-303

#### Problem
1 open stops may share a mechanism. Verify each link; do not assume the grouping proves ownership.
- S-303 (code): Castle unpack — The instant unpack of RW 0x79C265 / 0x79BE6A / 0x7987EE is ported (transform, angle, owner by FilterValidOwnedEntries, the starting fortress and the maps' castles); the legality call RW 0x797A96 passes 5 / 0x100 (terrain, object overlap, the castle's own members ignored; lane BUILDCLEAR-1); the port passes 0x104: its terrain sample classification (S-161, the cell types standing for RW 0x794A2B's GetCellType) refused members retail places (the sta.

#### Known
Kinds: code. Classes: CastleBehavior, CastleUpgrade, WallUpgradeUpdate.
Functions: RW 0x00797A96 (ported), RW 0x007987EE (ported), RW 0x0079A518 (ported), RW 0x0079A5EC (ported), RW 0x0079AC19 (ported), RW 0x0079BE6A (ported), RW 0x0079C265 (ported), RW 0x00858701 (ported), RW 0x00858C41 (ported), RW 0x00858CFC (unknown), RW 0x00859198 (ported).
Addresses: S-303: RW 0x00794A2B, RW 0x00797A96, RW 0x007987EE, RW 0x0079A518, RW 0x0079A5EC, RW 0x0079AC19, RW 0x0079BE6A, RW 0x0079C265, RW 0x00858701, RW 0x00858C41, RW 0x00858CFC, RW 0x00859198, RW 0x00C30DE8.
Red buttons: none linked.
Unused fields: CastleBehavior.CrewPrepareFX, CastleBehavior.CrewPrepareInterval, CastleBehavior.CrewReleaseFX.
Existing task matches: #766 Local 100: upgrade modules: CommandSetUpgrade (515 modules), CostModifierUpgrade, CastleUpgrade, GiveUpgradeUpdate, AudioLoopUpgrade, WallUpgradeUpdate, LocomotorSetUpgrade: 27 uncited own functions; #688 Port 296: Castles вЂ” CastleBehavior, CastleMemberBehavior and FoundationAIUpdate: the uncited own functions (27) and their five unread fields.

#### Task
Trace the RotWK mechanism and callers, implement only verified missing behavior, and close or narrow the relevant stops.

#### Tests
Add synthetic regression tests for the verified behavior and adjacent branches; run the full test target.

#### In game
Choose a verified affected Ennorath object and document reproduction and expected retail behavior.

#### Do not touch
Unrelated stops, other open task areas, game files and generated source maps.

#### Done
RW instructions and tests justify each change; report any remaining stops and inference.

### Port N: close mechanism RespawnUpdate: stops S-854

#### Problem
1 open stops may share a mechanism. Verify each link; do not assume the grouping proves ownership.
- S-854 (code): RespawnUpdate details (HERO-1) — The revive-time producer exit query is ported: `onRevived` copies the producer position and calls its exit interface's slot 9 `getNaturalRallyPoint(out, true)` (RW 0x8B38B0 .. 0x8B38E7); the result is discarded by RotWK. The separate slot-2 `exitObjectViaDoor` placement at animation end is already ported (RW 0x8B3AD5 .. 0x8B3AEA). Still not ported: respawn health above 100% raises max health (RW 0x8C47F6)..

#### Known
Kinds: code. Classes: ActiveBody, RespawnUpdate.
Functions: RW 0x008B38A2 (unknown), RW 0x008B3A35 (ported), RW 0x008C47DE (ported).
Addresses: S-854: RW 0x008B38B0, RW 0x008B38E7, RW 0x008B3AD5, RW 0x008B3AEA, RW 0x008C47F6.
Red buttons: none linked.
Unused fields: ActiveBody.EnteringDamagedTransitionTime, ActiveBody.ReallyDamagedAttributeModifier, RespawnUpdate.DeathAnimationTime.
Existing task matches: #690 Port 298: RespawnUpdate вЂ” hero respawn: its unread DeathAnimationTime and 4 uncited own functions; #567 Port 243: Body damage states: ActiveBody EnteringDamagedTransitionTime and the Damaged / ReallyDamaged attribute modifiers (ActiveBody, RespawnBody).

#### Task
Trace the RotWK mechanism and callers, implement only verified missing behavior, and close or narrow the relevant stops.

#### Tests
Add synthetic regression tests for the verified behavior and adjacent branches; run the full test target.

#### In game
Choose a verified affected Ennorath object and document reproduction and expected retail behavior.

#### Do not touch
Unrelated stops, other open task areas, game files and generated source maps.

#### Done
RW instructions and tests justify each change; report any remaining stops and inference.

### Port N: close mechanism HordeContain: stops S-082, S-1102, S-6004, S-6027, S-7293

#### Problem
5 open stops may share a mechanism. Verify each link; do not assume the grouping proves ownership.
- S-082 (code): HordeContain data — HordeContain fields parsed exactly but with no reader found in the RotWK binary (spec horde-and-movement.md section 4): RankInfo `Leader <rank> <index>` (slot record +0x18), BackUpMinDelayTime / BackUpMaxDelayTime / BackUpMinDistance / BackUpMaxDistance / BackupPercentage, CowerRadius, RanksToJustFreeWhenAttacking, and RanksToReleaseWhenAttacking (lane FLANK-1: FrontAngle +0x264 and FlankedDuration +0x26C are read by the flank test RW 0x876FC4, F.
- S-1102 (code): Horde garrison link (GARRISON-1) — HordeContain's garrison slots are ported (HordeGarrisonLink.cpp): the members on their way (RW 0x86EE1B / 0x86EBF4 / 0x8757FC / 0x86D755), the garrisoned byte (RW 0x872ADF / 0x872B76), enterContainer RW 0x875DC3 (the horde first, then every member ordered in), the members' walk RW 0x875C93, the per-frame push RW 0x876B25 (CloseEnoughDist, MaxNumMembersToForceToImmediatelyEnter, WaitToForceMemberToEnterDelay) and exitContainer RW 0x875EFD; the AI .
- S-6004 (code): HordeContain::splitHorde (MODGAP-2) — Ported from RW 0x8791C5 with BFME2 HordeContainRva00475F61.cpp for the shape; SplitHordeSpecialPower RW 0x8CCA45. NOT ported: the attribute modifiers' removal RW 0x86C962 (slot 0x1E8), a defected horde's hand-over RW 0x6930C9, the weapon set flag 0x18 of the first member. INFERENCE: the held member destroyed is the banner carrier.
- S-6027 (code): HordeContain/ComboHorde join gate (PORT-35, issue #120) — The entry lookup RW 0x87043B and the combine arm RW 0x871E89 .. 0x871F12 are ported (`HordeContain::canCombineWith`), but interface slot 0x70 (RW 0x871D41) has five more tests: RW 0x9325B4 with module data + 0x1C8, RW 0x46E918 (ecx, 0xda), RW 0x693A1A (ecx, 0), RW 0x68B678 on both objects (`[[ecx + 0x31C]]`, objects compare, equal required) and the member count arm at RW 0x871E4C (`[[obj + 0x26C] + 0x24] <= 1` refuses). The callees of the first t.
- S-7293 (code): HordeContain/NotComboFormation: the AICombineState it is read from (PORT-112, issue #272) — The byte (module data + 0x25C, getter RW 0x872B91, interface slot 136) has one reader, RW 0x751716: the onEnter (slot 4 of vtable RW 0xC29548, the state's name key 0xD2CCD3C pushed by its constructor RW 0x7448DE) of AICombineState — RW 0x751738 asks the getter through the ordered object's contain (RW 0x68C866) and RW 0x751746 calls slot 0x60 (RW 0x86C7F6, the formation swap). The state itself is NOT ported: LOCAL-18 executes the combine order ins.

#### Known
Kinds: code. Classes: AICombineState, CombineHordeSpecialPower, HordeContain, RankInfo, SplitHordeSpecialPower.
Functions: RW 0x0046E918 (ported), RW 0x00667144 (ported), RW 0x0068B678 (ported), RW 0x0068C866 (ported), RW 0x006930C9 (unknown), RW 0x00693A1A (ported), RW 0x007448DE (unknown), RW 0x00751716 (unknown), RW 0x00855830 (ported), RW 0x0086C7F6 (ported), RW 0x0086C962 (ported), RW 0x0086D755 (ported), RW 0x0086EBF4 (ported), RW 0x0086EE1B (ported), RW 0x0087043B (ported), RW 0x00871D41 (ported), RW 0x008722AE (unknown), RW 0x00872ADF (ported), RW 0x00872B76 (none), RW 0x00872B91 (ported), RW 0x00872EFC (ported), RW 0x00873FE8 (ported), RW 0x008757FC (ported), RW 0x00875C93 (ported), RW 0x00875DC3 (ported), RW 0x00875EFD (ported), RW 0x00876481 (ported), RW 0x00876B25 (ported), RW 0x00876FC4 (ported), RW 0x008791C5 (ported), RW 0x008CCA45 (ported), RW 0x008CCCB4 (ported), RW 0x009325B4 (ported).
Addresses: S-082: RW 0x00872EFC, RW 0x00876FC4; S-1102: RW 0x00667144, RW 0x00855830, RW 0x0086D755, RW 0x0086EBF4, RW 0x0086EE1B, RW 0x00872ADF, RW 0x00872B76, RW 0x00873FE8, RW 0x008757FC, RW 0x00875C93, RW 0x00875DC3, RW 0x00875EFD, RW 0x00876B25; S-6004: RW 0x006930C9, RW 0x0086C962, RW 0x008791C5, RW 0x008CCA45; S-6027: RW 0x0046E918, RW 0x0068B678, RW 0x00693A1A, RW 0x0087043B, RW 0x00871D41, RW 0x00871E4C, RW 0x00871E89, RW 0x00871F12, RW 0x0087230F, RW 0x00872368, RW 0x008765CC, RW 0x009325B4; S-7293: RW 0x0068C866, RW 0x007448DE, RW 0x00751716, RW 0x00751738, RW 0x00751746, RW 0x0086C7F6, RW 0x00872B91, RW 0x008CCCB4, RW 0x00C29548.
Red buttons: none linked.
Unused fields: HordeContain.LivingWorldOverloadTemplate, HordeContain.RanksToJustFreeWhenAttacking.
Existing task matches: #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate; #607 Port 259: Horde-level powers: SplitHordeSpecialPower, HordeDispatchSpecialPower, SiegeDeployHordeSpecialPower, SiegeDeploySpecialPower.

#### Task
Trace the RotWK mechanism and callers, implement only verified missing behavior, and close or narrow the relevant stops.

#### Tests
Add synthetic regression tests for the verified behavior and adjacent branches; run the full test target.

#### In game
Choose a verified affected Ennorath object and document reproduction and expected retail behavior.

#### Do not touch
Unrelated stops, other open task areas, game files and generated source maps.

#### Done
RW instructions and tests justify each change; report any remaining stops and inference.

### Port N: close mechanism GiantBirdSlowDeathBehavior: stops S-5606

#### Problem
1 open stops may share a mechanism. Verify each link; do not assume the grouping proves ownership.
- S-5606 (code): GiantBirdSlowDeathBehavior (MODGAP-1) — Ported from RW 0x86511C (the crash's start), 0x865009 (update), 0x66A61A (the clear point). FXHitGround, OCLHitGround and DelayFromGroundToFinalDeath are parsed and read by none of these functions (PORT-304, issue #696: CrashAvoidStrength + 0x1BC and NeedToMaintainFlailingHeight + 0x1C0 are read by nothing in RotWK, written only by the data constructor RW 0x865076; the chain is in GiantBirdSlowDeathBehavior.h); the base's HIT_GROUND phase (the mo.

#### Known
Kinds: code. Classes: GiantBirdSlowDeathBehavior.
Functions: RW 0x00865076 (unknown), RW 0x0086511C (ported).
Addresses: S-5606: RW 0x00865076, RW 0x0086511C.
Red buttons: none linked.
Unused fields: GiantBirdSlowDeathBehavior.CrashAvoidStrength, GiantBirdSlowDeathBehavior.NeedToMaintainFlailingHeight.
Existing task matches: none.

#### Task
Trace the RotWK mechanism and callers, implement only verified missing behavior, and close or narrow the relevant stops.

#### Tests
Add synthetic regression tests for the verified behavior and adjacent branches; run the full test target.

#### In game
Choose a verified affected Ennorath object and document reproduction and expected retail behavior.

#### Do not touch
Unrelated stops, other open task areas, game files and generated source maps.

#### Done
RW instructions and tests justify each change; report any remaining stops and inference.

### Port N: close mechanism linked stops S-564, S-7100: stops S-564, S-7100

#### Problem
2 open stops may share a mechanism. Verify each link; do not assume the grouping proves ownership.
- S-564 (code): Object look inputs — RW 0x694A45 / 0x68E4E2 are ported for the owner / allies / REVEAL_TO_ALL mask, ShroudClearingRange, UNDER_CONSTRUCTION (the bounding radius), VisionSide / VisionRear, the VisionBonus height bonus (RW 0x69264D's ground reference) and the HORDE_MEMBER rule; the SHROUD_CLEARING modifier (RW 0x804F39 type 0x14, RW 0x68E500) and the object's own range (Object + 0x1B4, RW 0x68C234) since lane DECOMP-1; not read: the spied mask (player + 0x3C8), object .
- S-7100 (code): DynamicShroudClearingRangeUpdate grid decal (LOCAL-35, issue #285) — The module drives the shroud clearing range (RW 0x88E9B0 -> `Object::setShroudClearingRange` RW 0x68C234), but the 30 `RadiusDecal` grid decals retail also creates / animates / clears (RW 0x88E87F / 0x88E8C7 / 0x88E6AF) are not drawn; `m_decalsCreated` is kept so the logic and the save match retail.

#### Known
Kinds: code. Classes: DynamicShroudClearingRangeUpdate, HordeContain, RadiusDecal.
Functions: RW 0x0068C234 (ported), RW 0x0068E4E2 (ported), RW 0x0069264D (unknown), RW 0x00694A45 (unknown), RW 0x00804F39 (ported), RW 0x00872B9E (ported), RW 0x00872BAB (ported), RW 0x0088E6AF (ported), RW 0x0088E87F (ported), RW 0x0088E8C7 (ported), RW 0x0088E9B0 (ported).
Addresses: S-564: RW 0x0068C234, RW 0x0068E4E2, RW 0x0068E500, RW 0x0069264D, RW 0x00694A45, RW 0x00694B79, RW 0x00694B9B, RW 0x00804F39, RW 0x00872B9E, RW 0x00872BAB; S-7100: RW 0x0068C234, RW 0x0088E6AF, RW 0x0088E87F, RW 0x0088E8C7, RW 0x0088E9B0.
Red buttons: none linked.
Unused fields: HordeContain.LivingWorldOverloadTemplate, HordeContain.RanksToJustFreeWhenAttacking.
Existing task matches: #684 Port 292: HordeContain вЂ” the uncited own functions of every horde (62), and its unread RanksToJustFreeWhenAttacking / LivingWorldOverloadTemplate.

#### Task
Trace the RotWK mechanism and callers, implement only verified missing behavior, and close or narrow the relevant stops.

#### Tests
Add synthetic regression tests for the verified behavior and adjacent branches; run the full test target.

#### In game
Choose a verified affected Ennorath object and document reproduction and expected retail behavior.

#### Do not touch
Unrelated stops, other open task areas, game files and generated source maps.

#### Done
RW instructions and tests justify each change; report any remaining stops and inference.

### Port N: close mechanism RW 0x0068B6F5: stops S-2456

#### Problem
1 open stops may share a mechanism. Verify each link; do not assume the grouping proves ownership.
- S-2456 (code): Radar colour (RADAR-1) — A blip's colour is Object::getIndicatorColor (RW 0x68B6F5: the custom colour + 0x324, else the team owner's colour, else 0xFF000000) with its HSV saturation times 1.6 (RW 0x6D8C53), times the draw tint (-1). Not ported: the custom indicator colour (none set here), the disguise colour of RW 0x6D90DC and the object + 0x250 colour interface; a capture's colour refresh (RW 0x6D9004) happens with every snapshot..

#### Known
Kinds: code. Classes: StealthUpdate.
Functions: RW 0x0068B6F5 (ported), RW 0x006D8C53 (ported), RW 0x006D9004 (none), RW 0x006D9042 (ported).
Addresses: S-2456: RW 0x0068B6F5, RW 0x006D8C53, RW 0x006D9004, RW 0x006D90DC.
Red buttons: none linked.
Unused fields: StealthUpdate.HintDetectableConditions.
Existing task matches: none.

#### Task
Trace the RotWK mechanism and callers, implement only verified missing behavior, and close or narrow the relevant stops.

#### Tests
Add synthetic regression tests for the verified behavior and adjacent branches; run the full test target.

#### In game
Choose a verified affected Ennorath object and document reproduction and expected retail behavior.

#### Do not touch
Unrelated stops, other open task areas, game files and generated source maps.

#### Done
RW instructions and tests justify each change; report any remaining stops and inference.

### Port N: close mechanism SubObjectsUpgrade: stops S-484

#### Problem
1 open stops may share a mechanism. Verify each link; do not assume the grouping proves ownership.
- S-484 (code): SubObjectsUpgrade drawing — the fade (RW 0x8B8F40) and the house recolour (RW 0x67273A) are not drawn: counted in `UpgradeModuleClasses::stats()`; sub objects show / hide at once (Drawable::showModule / showSubObject, permanent form). The UpgradeTexture swap is drawn (RW 0x8B911E: every draw module of the drawable takes the module's list through vslot 0x84, replacing the one it had; the removal hands an empty list): the RotWK heavy armour look of most infantry, Ennorath's A.

#### Known
Kinds: code. Classes: SubObjectsUpgrade.
Functions: RW 0x0067273A (ported), RW 0x008B8F40 (partial), RW 0x008B911E (ported).
Addresses: S-484: RW 0x0067273A, RW 0x008B8F40, RW 0x008B911E.
Red buttons: none linked.
Unused fields: SubObjectsUpgrade.WaitBeforeFadeInSeconds.
Existing task matches: #570 Port 246: Client fades: W3DTreeDraw FadeDistance / FadeTarget, W3DFloorDraw FloorFadeRateOnObjectDeath, SubObjectsUpgrade WaitBeforeFadeInSeconds.

#### Task
Trace the RotWK mechanism and callers, implement only verified missing behavior, and close or narrow the relevant stops.

#### Tests
Add synthetic regression tests for the verified behavior and adjacent branches; run the full test target.

#### In game
Choose a verified affected Ennorath object and document reproduction and expected retail behavior.

#### Do not touch
Unrelated stops, other open task areas, game files and generated source maps.

#### Done
RW instructions and tests justify each change; report any remaining stops and inference.

### Port N: close mechanism FlingPassengerSpecialAbilityUpdate: stops S-7758

#### Problem
1 open stops may share a mechanism. Verify each link; do not assume the grouping proves ownership.
- S-7758 (project): FlingPassengerSpecialAbilityUpdate FlingPassengerVelocity has no reader in RotWK 2.01 or BFME2 1.06 (Local 125, fork issue 809) — Parse table RW 0xC0556C (`FlingPassengerVelocity` +0xD0, `FlingPassengerLandingWarhead` +0xDC). ModuleData ctor RW 0x89870E zeroes +0xD0..+0xD8 and writes vtable 0xC763A8 (all inherited slots). Module vtable 0xC652D8 has only four own methods (dtor RW 0x89873E, name RW 0x8986A3, xfer RW 0x8C6E67, getModuleNameKey RW 0x8986C9) plus `triggerAbilityEffect` RW 0x89875A, which reads only `FlingPassengerLandingWarhead` (+0xDC, RW 0x89878C) and never re.

#### Known
Kinds: project. Classes: FlingPassengerSpecialAbilityUpdate, ModuleData.
Functions: RW 0x008986A3 (ported), RW 0x008986C9 (ported), RW 0x0089870E (unknown), RW 0x0089873E (ported), RW 0x0089875A (ported), RW 0x008C6E67 (ported).
Addresses: S-7758: RW 0x008986A3, RW 0x008986C9, RW 0x0089870E, RW 0x0089873E, RW 0x0089875A, RW 0x0089878C, RW 0x008C6E67, RW 0x00C0556C.
Red buttons: none linked.
Unused fields: FlingPassengerSpecialAbilityUpdate.FlingPassengerVelocity.
Existing task matches: none.

#### Task
Trace the RotWK mechanism and callers, implement only verified missing behavior, and close or narrow the relevant stops.

#### Tests
Add synthetic regression tests for the verified behavior and adjacent branches; run the full test target.

#### In game
Choose a verified affected Ennorath object and document reproduction and expected retail behavior.

#### Do not touch
Unrelated stops, other open task areas, game files and generated source maps.

#### Done
RW instructions and tests justify each change; report any remaining stops and inference.

### Port N: close mechanism RW 0x00646F9C: stops S-002

#### Problem
1 open stops may share a mechanism. Verify each link; do not assume the grouping proves ownership.
- S-002 (code): INI startup — Startup position of `TheBannerUI`, `TheFontLibrary`, `ArmySummaryDescription`, `StrategicHUD`, `InGameNotificationBox`, `AptButtonTooltipMap` not recovered: callers end at virtual dispatch. The last three are in known relative order (RW 0x647101/06/0B). None defines or uses macros..

#### Known
Kinds: code. Classes: AptButtonTooltipMap, ArmySummaryDescription.
Functions: RW 0x00646F9C (unknown).
Addresses: S-002: RW 0x00647101.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

#### Task
Trace the RotWK mechanism and callers, implement only verified missing behavior, and close or narrow the relevant stops.

#### Tests
Add synthetic regression tests for the verified behavior and adjacent branches; run the full test target.

#### In game
Choose a verified affected Ennorath object and document reproduction and expected retail behavior.

#### Do not touch
Unrelated stops, other open task areas, game files and generated source maps.

#### Done
RW instructions and tests justify each change; report any remaining stops and inference.

### Port N: close mechanism linked stops S-023, S-390: stops S-023, S-390

#### Problem
2 open stops may share a mechanism. Verify each link; do not assume the grouping proves ownership.
- S-023 (code): W3D materials — BFME2 FX materials draw with the lighting of normalmapped.fxo, decoded from Shaders.big (lane RENDER-1, `tools/render/d3d9_disasm.py`: per pixel `tex * saturate(AmbientLight * AmbientColor * 0.5) * 2 + sum(Light.Color * (tex * DiffuseColor * N.L + SpecularColor * N.H^SpecularExponent))`, normal map in the frame -binormal, -tangent, normal with xy * BumpScale (the file's tangent and bitangent go into the vertex buffer raw, FXShaderGeometry.cpp:557.
- S-390 (code): Object lighting — the light environment of the retail effects is filled like ZH's (scene ambient = the map's objects light 0 ambient clamped to [0, 1], directional lights = the objects lights 0..2 whose diffuse reaches 0.05, direction -lightPos; infantry drawables use the map's infantry set): the RotWK fill code and the per-drawable choice were not read. Target facts: the SAS callbacks (RW 0x54F406 -> 0x54CF11 / 0x54CFD0 / 0x54D083) send both colours doubled when .

#### Known
Kinds: code. Classes: none.
Functions: RW 0x004AC4ED (unknown), RW 0x0054F406 (unknown).
Addresses: S-023: ; S-390: RW 0x004AC97F, RW 0x0054CF11, RW 0x0054CF3B, RW 0x0054CFD0, RW 0x0054D083, RW 0x0054F406, RW 0x00DD3454.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

#### Task
Trace the RotWK mechanism and callers, implement only verified missing behavior, and close or narrow the relevant stops.

#### Tests
Add synthetic regression tests for the verified behavior and adjacent branches; run the full test target.

#### In game
Choose a verified affected Ennorath object and document reproduction and expected retail behavior.

#### Do not touch
Unrelated stops, other open task areas, game files and generated source maps.

#### Done
RW instructions and tests justify each change; report any remaining stops and inference.

### Port N: close mechanism RW 0x0073F24D: stops S-073, S-075

#### Problem
2 open stops may share a mechanism. Verify each link; do not assume the grouping proves ownership.
- S-073 (code): Module registry — The name and purpose of the ModuleData vtable slot 7 predicate (RW 0x73F4A9 calls it only for a ChildObject and clears the list entries that answer true: behaviour verified, true for the 11 BODY-mask classes except InactiveBody) and the meaning of interface mask 0x2000 (PillageModule alone) and of 0x1000 (all six ClientBehavior classes) are not identified..
- S-075 (code): Module factory — A module declaration naming a class the binary does not register: RotWK's `newModuleDataFromINI` returns NULL (RW 0x656EF1) and `parseModuleName` calls through it (RW 0x73F496), a crash. This port raises an INIException (code 3) naming the class..

#### Known
Kinds: code. Classes: INIException, InactiveBody, ModuleData, PillageModule.
Functions: RW 0x00656EF1 (unknown), RW 0x0073F24D (unknown).
Addresses: S-073: RW 0x0073F4A9; S-075: RW 0x00656EF1, RW 0x0073F496.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

#### Task
Trace the RotWK mechanism and callers, implement only verified missing behavior, and close or narrow the relevant stops.

#### Tests
Add synthetic regression tests for the verified behavior and adjacent branches; run the full test target.

#### In game
Choose a verified affected Ennorath object and document reproduction and expected retail behavior.

#### Do not touch
Unrelated stops, other open task areas, game files and generated source maps.

#### Done
RW instructions and tests justify each change; report any remaining stops and inference.

### Port N: close mechanism RW 0x0042D400: stops S-076

#### Problem
1 open stops may share a mechanism. Verify each link; do not assume the grouping proves ownership.
- S-076 (code): INI block bodies — `BeginScript` bodies end at the first line whose first token is ENDSCRIPT (RW 0x42D400); RW has no end-of-file test in that loop and never returns when the terminator is missing. This port reports `Missing 'ENDSCRIPT' token`..

#### Known
Kinds: code. Classes: none.
Functions: RW 0x0042D400 (unknown).
Addresses: S-076: RW 0x0042D400.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

#### Task
Trace the RotWK mechanism and callers, implement only verified missing behavior, and close or narrow the relevant stops.

#### Tests
Add synthetic regression tests for the verified behavior and adjacent branches; run the full test target.

#### In game
Choose a verified affected Ennorath object and document reproduction and expected retail behavior.

#### Do not touch
Unrelated stops, other open task areas, game files and generated source maps.

#### Done
RW instructions and tests justify each change; report any remaining stops and inference.

### Port N: close mechanism RW 0x00436A90: stops S-077

#### Problem
1 open stops may share a mechanism. Verify each link; do not assume the grouping proves ownership.
- S-077 (code): Object model: validate — `ThingTemplate::validate` (RW 0x73CE7E, called at the end of parseObjectDefinition) is not ported: it fills the shadow-texture string with `shadows` / `shadow` when empty (RW 0xC26AF8, 0xBE59F8) and copies two audio event strings (RW 0x436A90)..

#### Known
Kinds: code. Classes: ThingTemplate.
Functions: RW 0x00436A90 (library), RW 0x0073CE7E (unknown).
Addresses: S-077: RW 0x00436A90, RW 0x0073CE7E, RW 0x00BE59F8, RW 0x00C26AF8.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

#### Task
Trace the RotWK mechanism and callers, implement only verified missing behavior, and close or narrow the relevant stops.

#### Tests
Add synthetic regression tests for the verified behavior and adjacent branches; run the full test target.

#### In game
Choose a verified affected Ennorath object and document reproduction and expected retail behavior.

#### Do not touch
Unrelated stops, other open task areas, game files and generated source maps.

#### Done
RW instructions and tests justify each change; report any remaining stops and inference.

### Port N: close mechanism RW 0x004B33FB: stops S-092, S-682, S-7194

#### Problem
3 open stops may share a mechanism. Verify each link; do not assume the grouping proves ownership.
- S-092 (code): Draw blending — RESOLVED for the weight: RW 0x4B33FB passes track 0 and track 1 to the pose evaluator with percentage = 1 - countdown / initial (the countdown starts at `max(1, min(blend > 0 ? blend : 5, frames - 1))` of the incoming track, RW 0x4B55D5, and falls at the incoming track's rate, RW 0x4BF560); `W3DDrawFrame::blendPercentage` is that number. Still unverified: the Godot side feeding it to `Blend_Pose` (the instancer was written without a Godot run on .
- S-682 (code): Live effect player — GameClient/LiveFX plays the logic's effect events, EnteringStateFX and the ParticleSysBone systems of every model draw. Since PORT-50 (issue #154) a ParticleSysBone system follows its ANIMATED bone where the module data's ParticlesAttachedToAnimatedBones is on: the gate RW 0x4C7795 -> RW 0x4C779E (slot 27 of vtable 0xBE3B78 = RW 0x4B6D5F) is ported in `GameClient/ParticleBoneFollow` (record per system from RW 0x4C6BAD .. 0x4C6C39, the per-frame w.
- S-7194 (code): ParticleSysBone follow (PORT-50, issue #154) — Reachable reader proven: RW 0x4C72BE (doDrawModule) reads the module data's ParticlesAttachedToAnimatedBones (+0x108) at RW 0x4C7795 and calls slot 27 (+0x6C) of the draw module's +0xC object with 0 at RW 0x4C779E .. 0x4C77A5 (vtable RW 0xBE3B78 -> RW 0x4B6D5F); the draw's tracker list at +0x58 holds one node per created ParticleSysBone system (the record copy RW 0x4B6CDD, the follow flag stored at RW 0x4C6B49), and RW 0x4B6D5F walks it: RW 0x5F5.

#### Known
Kinds: code. Classes: FXEvent, W3DDrawFrame, W3DDrawPoseView.
Functions: RW 0x004B33FB (unknown), RW 0x004B55D5 (ported), RW 0x004B6CDD (ported), RW 0x004B6D5F (ported), RW 0x004BF560 (ported), RW 0x004C6514 (ported), RW 0x004C72BE (ported), RW 0x005F2F55 (ported), RW 0x005F32A7 (ported), RW 0x005F530A (ported), RW 0x00672B5B (ported), RW 0x0067DAE6 (unknown), RW 0x0068BBE0 (ported), RW 0x00694C0D (ported), RW 0x006CCB4F (unknown), RW 0x006EACB9 (ported).
Addresses: S-092: RW 0x004B33FB, RW 0x004B55D5, RW 0x004BF560; S-682: RW 0x004B33FB, RW 0x004B6D5F, RW 0x004C6514, RW 0x004C662D, RW 0x004C678F, RW 0x004C67B6, RW 0x004C69BE, RW 0x004C6A4B, RW 0x004C6A94, RW 0x004C6BAD, RW 0x004C6C39, RW 0x004C6C87, RW 0x004C7795, RW 0x004C779E, RW 0x005F2F55, RW 0x005F32A7, RW 0x0067DAE6, RW 0x0068BBE0, RW 0x00694C0D, RW 0x006CCB91, RW 0x006EACB9; S-7194: RW 0x004B33FB, RW 0x004B6CDD, RW 0x004B6D5F, RW 0x004B6DCC, RW 0x004B6DD5, RW 0x004B6DDF, RW 0x004B6E2F, RW 0x004B6E57, RW 0x004B6E79, RW 0x004B6E8B, RW 0x004C6514, RW 0x004C678F, RW 0x004C6B49, RW 0x004C72BE, RW 0x004C7795, RW 0x004C779E, RW 0x004C77A5, RW 0x005F530A, RW 0x00672B5B, RW 0x00BE3B78.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

#### Task
Trace the RotWK mechanism and callers, implement only verified missing behavior, and close or narrow the relevant stops.

#### Tests
Add synthetic regression tests for the verified behavior and adjacent branches; run the full test target.

#### In game
Choose a verified affected Ennorath object and document reproduction and expected retail behavior.

#### Do not touch
Unrelated stops, other open task areas, game files and generated source maps.

#### Done
RW instructions and tests justify each change; report any remaining stops and inference.

### Port N: close mechanism RW 0x004B67D4: stops S-095

#### Problem
1 open stops may share a mechanism. Verify each link; do not assume the grouping proves ownership.
- S-095 (code): Draw state fields — `SimilarRestart`, `ShareAnimation`, `AllowRepeatInRandomPick`, `UseWeaponTiming`, `Distance`, `AnimationMustCompleteBlend`, and the FXEvent / LuaEvent / ParticleSysBone playback of a state are parsed and stored; `SimilarRestart`, `AllowRepeatInRandomPick`, `UseWeaponTiming`, `Distance`, `AnimationMustCompleteBlend` and `ShareAnimation` are acted on by the transcribed routines (UseWeaponTiming and Distance need a weapon timing / object speed sourc.

#### Known
Kinds: code. Classes: FXEvent.
Functions: RW 0x004B67D4 (unknown), RW 0x0068B34C (ported).
Addresses: S-095: RW 0x004B67D4, RW 0x0068B34C.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

#### Task
Trace the RotWK mechanism and callers, implement only verified missing behavior, and close or narrow the relevant stops.

#### Tests
Add synthetic regression tests for the verified behavior and adjacent branches; run the full test target.

#### In game
Choose a verified affected Ennorath object and document reproduction and expected retail behavior.

#### Do not touch
Unrelated stops, other open task areas, game files and generated source maps.

#### Done
RW instructions and tests justify each change; report any remaining stops and inference.

### Port N: close mechanism RW 0x004B52C8: stops S-096

#### Problem
1 open stops may share a mechanism. Verify each link; do not assume the grouping proves ownership.
- S-096 (code): Draw fades — PORT-49 applies the model draws' camera distance fade (`AlphaCameraFadeOuterRadius` / `InnerRadius` / `AtInnerRadius`, RW 0x4B52C8..0x4B5494) through `Drawable::advanceCameraFade`, independently of the animation `needsAdvance` gate, and the Godot model view. The per animation `FadeBeginFrame` / `FadeEndFrame` / `FadingIn` opacity ramp (RW 0x4B5497) is computed into W3DDrawFrame::opacity / hidden (it needs a begin frame above 0, as the donor test .

#### Known
Kinds: code. Classes: DrawServices, W3DDrawFrame.
Functions: RW 0x004B52C8 (ported), RW 0x004B5497 (unknown).
Addresses: S-096: RW 0x004B52C8, RW 0x004B5474, RW 0x004B548B, RW 0x004B5494, RW 0x004B5497.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

#### Task
Trace the RotWK mechanism and callers, implement only verified missing behavior, and close or narrow the relevant stops.

#### Tests
Add synthetic regression tests for the verified behavior and adjacent branches; run the full test target.

#### In game
Choose a verified affected Ennorath object and document reproduction and expected retail behavior.

#### Do not touch
Unrelated stops, other open task areas, game files and generated source maps.

#### Done
RW instructions and tests justify each change; report any remaining stops and inference.

### Port N: close mechanism RW 0x008E168A: stops S-1021, S-7507

#### Problem
2 open stops may share a mechanism. Verify each link; do not assume the grouping proves ownership.
- S-1021 (code): Emotion nuggets (MODULES-2, MODULES-3) — TheEmotionSystem (GameLogic/System/EmotionSystem.h) parses the EmotionNugget blocks (RW 0x8E0D59, table RW 0xC77FD0: first definition kept, CopyFrom, ModelConditions / ModelConditionsClear also filling the exit flags, the threat pairs) and keeps the SCARY / HERO list (RW 0x835D55 / 0x835B0A); the per-object nugget runs canApply (RW 0x8E12F5), start (RW 0x8E0ED7: end frame, StartFXList, the model conditions on the object or on a horde's members th.
- S-7507 (code): EmotionTrackerUpdate's clear slot (PORT-263, fork issue 611) — RW 0x8B56EB (the + 0x20 interface's slot, RW 0xC6D19C + 0x0) stops the running nugget (RW 0x8E168A), ends every nugget of the + 0x90 vector (RW 0x8E116B) and erases the range (RW 0x5FF9D3). It is the only entry of table RW 0xC6D19C (the + 0x20 interface head the constructor installs at RW 0x8B6308; the module tables RW 0xC6D26C, RW 0xC6D1B0 and RW 0xC6D1A0 and the data table RW 0xC6D120 do not hold it), and the class's own code never calls it: th.

#### Known
Kinds: code. Classes: AIState, EmotionNugget, EmotionSystem, EmotionTrackerUpdate.
Functions: RW 0x005FF9D3 (ported), RW 0x00662FC8 (ported), RW 0x00663053 (ported), RW 0x0066309C (ported), RW 0x00735F53 (ported), RW 0x00835B0A (ported), RW 0x00835D55 (ported), RW 0x008B5427 (ported), RW 0x008B56CF (ported), RW 0x008B56EB (ported), RW 0x008B62BE (unknown), RW 0x008E0D59 (unknown), RW 0x008E0ED7 (ported), RW 0x008E116B (ported), RW 0x008E11A1 (ported), RW 0x008E12F5 (ported), RW 0x008E168A (ported).
Addresses: S-1021: RW 0x00662FC8, RW 0x00663053, RW 0x0066309C, RW 0x00735F53, RW 0x00835B0A, RW 0x00835D55, RW 0x008E0D59, RW 0x008E0ED7, RW 0x008E0FA9, RW 0x008E11A1, RW 0x008E12F5, RW 0x008E168A, RW 0x008E178B, RW 0x00C77FD0; S-7507: RW 0x005FF9D3, RW 0x008B5427, RW 0x008B56CF, RW 0x008B56EB, RW 0x008B6308, RW 0x008E116B, RW 0x008E168A, RW 0x00C6D120, RW 0x00C6D19C, RW 0x00C6D1A0, RW 0x00C6D1B0, RW 0x00C6D26C.
Red buttons: none linked.
Unused fields: none linked.
Existing task matches: none.

#### Task
Trace the RotWK mechanism and callers, implement only verified missing behavior, and close or narrow the relevant stops.

#### Tests
Add synthetic regression tests for the verified behavior and adjacent branches; run the full test target.

#### In game
Choose a verified affected Ennorath object and document reproduction and expected retail behavior.

#### Do not touch
Unrelated stops, other open task areas, game files and generated source maps.

#### Done
RW instructions and tests justify each change; report any remaining stops and inference.
