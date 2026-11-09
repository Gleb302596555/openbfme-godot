# GameData: the rows the port does not read (Port 70, issue #184)

The field audit v6 (2026-11-09) lists the GameData rows the port parses into
`GameLogicSettings`' catch-all and nothing applies (stop S-152). This page sorts them:
every row with its RotWK 2.01 address, whether RotWK reads it and where, and its class
(`gameplay`, `presentation`, `debug / editor`, `not read`).

**No engine code**: `engine/src/GameLogic/GameLogicSettings.cpp` is unchanged by Port 70.
The point of the sort is the `gameplay` block — those rows are the next ports (last
section); the rest is a record so that nobody re-opens them one by one.

## Provenance of every number

* **The row.** RotWK's GameData table is `RW 0xBFF580`: rows of 16 bytes
  `{name, parser, userData, member offset}`, the last row before `RW 0xC01210`
  (batch-18, batch-20, batch-21 agree; the table is reached from the block's parse at
  `RW 0x6441A5` / `RW 0x64423A` with `ecx = [0xDE4364]`). The **row addresses below are
  derived**, not each read out of game.dat: `row = 0xBFF580 + 16 * index`, with the index,
  the parser and the member offset from BFME2 1.06's own table
  (`Code/GameEngine/Source/Common/GlobalData.cpp`, `s_GlobalDataFieldParseTable`, 457 rows),
  which lists the same fields in the same order with the same offsets (the `docs/ennorath/arena-triage`
  batches read the RotWK table straight from the image and cite the same row and offset for
  435 of those 457 rows; e.g. `BaseRegenDelay` → `0xBFF580 + 16*275 = 0xC006B0` +0xB9C,
  batch-16#18, and `CommandCenterHealAmount` +0xA90 / `BuildSpeed` +0xA64, batch-21).
  `tools/mod/gamedata_rows.py` reads the same table out of game.dat and prints the real
  row addresses, so the derivation is re-checkable where the game is.
* **The reader.** "reads" means an instruction that loads the member off the object
  `[0xDE4364]` (TheWritableGlobalData), with the instruction address named; the class of the
  object is proved by that load, never by an offset that happens to match (arena README,
  "Уроки" 1 and 3). "no reader proved" means exactly that: the search found no such load —
  it does not claim RotWK has no reader, and says where the search stopped.
* **The donor.** BFME2 1.06 is cited only for the table's shape, the member offsets and the
  constructor default (a name and an offset, never behaviour); where a reader exists in BFME2
  and RotWK was not read to the same instruction, the row is `gameplay, reader open`.
* **Not rows at all.** `gamedatadebug.ini`'s `ShowCollisionExtents`, `ShroudOn`,
  `StateMachineDebug`, `FogOfWarOn` and the `Debug*Tile*` group do not occur in game.dat in
  any case (arena README correction 6): RotWK cannot parse them, there is no row, and the
  port's catch-all is right. They are not repeated here.
* **The port's own rows.** Of the 78 rows below, none has a `kGameDataFields` row — that is
  what `not-read` means. The 63 rows the port *does* have are outside this page; the test
  `PORT-70` pins both sides (that every row of this page is a real GameData row of the donor
  table **and** has no port row, and that the rows the port has keep their rows).

## Summary

| class | rows | what it means |
|---|---|---|
| `gameplay` | 23 | a rule: base regeneration, low-power economy, rank health bonuses, command-centre heal, the solo-player / AI health bonuses, the charge and engagement defaults, the build-speed and multi-factory factors. **Next ports.** |
| `presentation` | 12 | RotWK reads it in client code (terrain, trees, water, ammo pips, audio, shell map): visible, no rule. |
| `debug / editor` | 22 | a debug switch, a stats overlay, a firewall or network row, a shell-map or developer switch: nothing happens in a game. |
| `not read` | 21 | nobody reads it — the `AnimationSharing*` block, the network metrics, the water position rows. Keep the rows, stop worrying. |

PORT-66 (#180, 2026-11-10) gave `AutoFireParticle*`, `AutoSmokeParticle*` and `AutoAflameParticle*` their `kGameDataFields` rows; the four of them this page listed (`AutoAflameParticlePrefix` / `System` / `Max`, `AutoFireParticleSmallPrefix`) left the page.

Stop **S-7197** records the block: the `not read` rows are settled (RotWK's constructor write and its parser are the whole
access set), the `gameplay` rows are **open, not refused** — their readers are not proved either, so a port of one of them
starts by reading the instruction that loads it off `[0xDE4364]`.

## The rows

`row` = the table row's address (`derived`, above), `off` = the member's offset in the
GameData object, `verdict` = what RotWK 2.01 does, `port` = whether `kGameDataFields`
has a row of that name, `evidence` = the address or the search, `batch` = the field-triage
report that read it.

| field | row | off | verdict | class | port | what RotWK does / where the search stopped | batch |
| `BaseRegenDelay` | 0xC006B0 | 0xB9C | no reader proved | gameplay | catch-all | the port regains nothing at a base: no regen of a structure or a hero near a base. RotWK: `parseDurationUnsignedInt` RW 0x73A429 writes `+0xB9C`; the candidate reads `[esi+0xB9C]` (RW 0xAB9CE7 / RW 0xAB9DAC) belong to the object built at RW 0xAADA65, not to `[0xDE4364]`, so **no GameData reader is proved** — the field stays `gameplay, reader open`, batch-16#18 — no `[0xDE4364]` load of this offset: the constructor default and the parser are the whole access set (field-triage batch group) | batch-16#18 |
| `BaseRegenHealthPercentPerSecond` | 0xC006A0 | 0xB98 | no reader proved | gameplay | catch-all | the share of health a base regains per second (`parsePercentToReal`, `+0xB98`); paired with `BaseRegenDelay` above. No reader proved; the offset sits next to the ported `SellPercentage` (`+0xB94`), which proves the block (`+0xB90 .. +0xB9C`) — no `[0xDE4364]` load of this offset: the constructor default and the parser are the whole access set (field-triage batch group) | batch-16#19 |
| `BuildSpeed` | 0xC002A0 | 0xA64 | no reader proved | gameplay | catch-all | the global build-time factor (`parseReal`, `+0xA64`); the port times a build from the structure's own data. No reader proved; neighbours `+0xA68` / `+0xA6C` are ported (`BuildAssistant` RW 0x794BF2) — no `[0xDE4364]` load of this offset: the constructor default and the parser are the whole access set (field-triage batch group) | batch-16#20 |
| `CommandCenterHealAmount` | 0xC00340 | 0xA90 | no reader proved | gameplay | catch-all | how much a command centre heals per interval (`parseReal`, `+0xA90`); the port has no command-centre heal. Issue #184 cites RW 0x42ED00, which is the row's **parser**, not a reader — no `[0xDE4364]` load of this offset: the constructor default and the parser are the whole access set (field-triage batch group) | batch-16#22 |
| `CommandCenterHealRange` | 0xC00330 | 0xA8C | no reader proved | gameplay | catch-all | the radius of that heal (`parseReal`, `+0xA8C`) — no `[0xDE4364]` load of this offset: the constructor default and the parser are the whole access set (field-triage batch group) | batch-16#23 |
| `MinLowEnergyProductionSpeed` | 0xC002E0 | 0xA74 | no reader proved | gameplay | catch-all | low-power economy: the slowest production while the player is at negative power (`+0xA74`). The port's `ProductionUpdate` has no low-power branch — no `[0xDE4364]` load of this offset: the constructor default and the parser are the whole access set (field-triage batch group) | — |
| `MaxLowEnergyProductionSpeed` | 0xC002F0 | 0xA78 | no reader proved | gameplay | catch-all | the fastest one (`+0xA78`) — no `[0xDE4364]` load of this offset: the constructor default and the parser are the whole access set (field-triage batch group) | — |
| `LowEnergyPenaltyModifier` | 0xC00300 | 0xA7C | no reader proved | gameplay | catch-all | the multiplier applied to it (`+0xA7C`) — no `[0xDE4364]` load of this offset: the constructor default and the parser are the whole access set (field-triage batch group) | — |
| `MultipleFactory` | 0xC00310 | 0xA80 | no reader proved | gameplay | catch-all | the second-factory speed factor (`+0xA80`); the port computes factory bonuses in `ProductionUpdate` without it — no `[0xDE4364]` load of this offset: the constructor default and the parser are the whole access set (field-triage batch group) | — |
| `RefundPercent` | 0xC00320 | 0xA84 | no reader proved | gameplay | catch-all | GameData's `+0xA84` (batch-20#14: only the constructor default RW 0x642F85 and the parser; `RW 0x8BE804` is another class's byte). The port sells at the ported `SellPercentage`; `RefundPercent` is the **second** refund row (its own table RW 0xC06998) and stays a separate question — no `[0xDE4364]` load of this offset: the constructor default and the parser are the whole access set (field-triage batch group) | batch-20#14 |
| `HealthBonus_Veteran` | 0xC00430 | 0xAD8 | no reader proved | gameplay | catch-all | health a rank-1 unit gains (`parsePercentToReal`, `+0xAD8`); the port takes rank health from `Object/ExperienceLevel*` data only — no `[0xDE4364]` load of this offset: the constructor default and the parser are the whole access set (field-triage batch group) | — |
| `HealthBonus_Elite` | 0xC00440 | 0xADC | no reader proved | gameplay | catch-all | rank 2 (`+0xADC`) — no `[0xDE4364]` load of this offset: the constructor default and the parser are the whole access set (field-triage batch group) | — |
| `HealthBonus_Heroic` | 0xC00450 | 0xAE0 | no reader proved | gameplay | catch-all | rank 3 (`+0xAE0`) — no `[0xDE4364]` load of this offset: the constructor default and the parser are the whole access set (field-triage batch group) | — |
| `HumanSoloPlayerHealthBonus_Easy` | 0xC00460 | 0x954 | no reader proved | gameplay | catch-all | the solo-player health bonus against the AI (`+0x954`), four rows; the port has no difficulty modifier — no `[0xDE4364]` load of this offset: the constructor default and the parser are the whole access set (field-triage batch group) | batch-18#24 |
| `HumanSoloPlayerHealthBonus_Normal` | 0xC00470 | 0x958 | no reader proved | gameplay | catch-all | `+0x958` — no `[0xDE4364]` load of this offset: the constructor default and the parser are the whole access set (field-triage batch group) | batch-18#26 |
| `HumanSoloPlayerHealthBonus_Hard` | 0xC00480 | 0x95C | no reader proved | gameplay | catch-all | `+0x95C` — no `[0xDE4364]` load of this offset: the constructor default and the parser are the whole access set (field-triage batch group) | batch-18#25 |
| `AISoloPlayerHealthBonus_Easy` | 0xC004A0 | 0x964 | no reader proved | gameplay | catch-all | the AI side of the same rule (`+0x964`) — no `[0xDE4364]` load of this offset: the constructor default and the parser are the whole access set (field-triage batch group) | — |
| `AISoloPlayerHealthBonus_Normal` | 0xC004B0 | 0x968 | no reader proved | gameplay | catch-all | `+0x968` — no `[0xDE4364]` load of this offset: the constructor default and the parser are the whole access set (field-triage batch group) | — |
| `AISoloPlayerHealthBonus_Hard` | 0xC004C0 | 0x96C | no reader proved | gameplay | catch-all | `+0x96C` — no `[0xDE4364]` load of this offset: the constructor default and the parser are the whole access set (field-triage batch group) | — |
| `DefaultVoiceAttackChargeTimeout` | 0xC00880 | 0xC34 | no reader proved | gameplay | catch-all | the charge-voice timeout (`parseDurationUnsignedInt`, `+0xC34`). The port's charge (PORT-5) uses the ability's own timers — no `[0xDE4364]` load of this offset: the constructor default and the parser are the whole access set (field-triage batch group) | batch-17#18 |
| `DefaultMaxDistanceForEngaged` | 0xC00890 | 0xC38 | no reader proved | gameplay | catch-all | distance at which an idle unit counts as engaged (`+0xC38`). The port's `AIUpdate` engagement uses its own ranges — no `[0xDE4364]` load of this offset: the constructor default and the parser are the whole access set (field-triage batch group) | batch-17#15 |
| `DefaultEngagedStateTimeout` | 0xC008A0 | 0xC3C | no reader proved | gameplay | catch-all | how long the engaged state holds (`+0xC3C`) — no `[0xDE4364]` load of this offset: the constructor default and the parser are the whole access set (field-triage batch group) | batch-17#14 |
| `UseCameraInReplay` | 0xC000D0 | 0xB71 | no reader proved | gameplay | catch-all | `+0xB71`: the port plays replays with a free camera; RotWK's replay camera flag is the row — no `[0xDE4364]` load of this offset: the constructor default and the parser are the whole access set (field-triage batch group) | batch-22#16 |
| `AdjustCliffTextures` | 0xBFF6B0 | 0x4B | reads (client) | presentation | catch-all | terrain cliff texture choice; reader RW 0x4ADF7A `cmp byte ptr [eax+0x4b],0` with eax=[0xDE4364] at RW 0x4ADF71 — batch-15#15 | — |
| `AllowTreeFading` | 0xBFF690 | 0x40 | reads (client) | presentation | catch-all | tree fade switch; readers RW 0x4EAA52 / RW 0x4EBA5A / RW 0x4ED68E / RW 0x4ED79B — batch-15#16 | — |
| `AmmoPipScaleFactor` | 0xBFFA10 | 0x104 | reads (client) | presentation | catch-all | ammo pip size; readers RW 0x4431F4 .. RW 0x448E4E (`RW 0xDE4368 = GlobalData+0x104`) — batch-15#17 | — |
| `AmmoPipScreenOffset` | 0xBFFA50 | 0xF4 | reads (client) | presentation | catch-all | ammo pip screen offset; reader RW 0x673C63 (`mulss xmm1,[eax+0xf8]`) — batch-15#18 | — |
| `AmmoPipWorldOffset` | 0xBFFA30 | 0xDC | reads (client) | presentation | catch-all | ammo pip world offset; reader RW 0x673BFA — batch-15#19 | — |
| `AudioOn` | 0xC00050 | 0x99C | reads (client) | presentation | catch-all | master audio switch; readers RW 0x460DC1 / RW 0x461DC2 / RW 0x63065D / RW 0x63C849 — batch-15#26 | — |
| `ShowProps` | 0xBFF5F0 | 0x1C | reads (client) | presentation | catch-all | prop draw gate; six readers RW 0x4E6C80 .. RW 0x4EE5C3 — batch-20#23 | batch-20#23 |
| `UseLightMap` | 0xBFF6A0 | 0x48 | reads (client) | presentation | catch-all | terrain material flag; RW 0x4E1BEC, written at RW 0x4E0C48 — batch-22#22 | batch-22#22 |
| `UseShadowMapping` | 0xBFF7E0 | 0x62 | reads (client) | presentation | catch-all | shadow mode; readers RW 0x442E0C .. RW 0x91A402 — batch-22#24 | batch-22#24 |
| `TreeFadeObjectFilter` | 0xC00C20 | 0xEB0 | reads (client) | presentation | catch-all | the filter of objects that fade with the trees; RW 0x4ED58D registers `&gc->+0xEB0` — batch-22#13 | batch-22#13 |
| `MaxUnitSelectSounds` | 0xC005C0 | 0xB50 | reads (client) | presentation | catch-all | voice-count when a group is selected; RW 0xABEB58, RW 0xABA025 — batch-18#18 | batch-19#18 |
| `ShellMapName` | 0xC00500 | 0xAEC | reads (client) | presentation | catch-all | the shell map's name; RW 0x75DEC1 / RW 0x7BA9B8 copy it into `+0xAC0` for the client shell loader — batch-20#21 | batch-20#21 |
| `DrawEntireTerrain` | 0xBFF760 | 0x4E | reads (client) | debug / editor | catch-all | a terrain debug draw switch (batch-22#10-#11 group: constructor write only) — constructor write and/or parser only; no `[0xDE4364]` read, or a client switch of the shell | batch-17#19 |
| `TerrainLOD` | 0xBFF770 | 0x50 | reads (client) | debug / editor | catch-all | the row carries userData (a name list); the terrain renderer picks its LOD itself — constructor write and/or parser only; no `[0xDE4364]` read, or a client switch of the shell | batch-21#3 |
| `TerrainLODTargetTimeMS` | 0xBFF780 | 0x58 | reads (client) | debug / editor | catch-all | the same block — constructor write and/or parser only; no `[0xDE4364]` read, or a client switch of the shell | batch-21#4 |
| `DumpAssetUsage` | 0xBFF620 | 0x27 | reads (client) | debug / editor | catch-all | the debug CSV dumper RW 0x912E41 is gated by `GameData + 0x9AC` (TOP20, `ThingClass` note); the port has no dumper — constructor write and/or parser only; no `[0xDE4364]` read, or a client switch of the shell | — |
| `ShowMetrics` | 0xC006E0 | 0xBB4 | reads (client) | debug / editor | catch-all | the metrics overlay switch; no reader — constructor write and/or parser only; no `[0xDE4364]` read, or a client switch of the shell | — |
| `DebugAI` | 0xC000E0 | 0x9B8 | reads (client) | debug / editor | catch-all | the row exists (RW 0xC000E0) and gates AI debug output; nothing in a game — constructor write and/or parser only; no `[0xDE4364]` read, or a client switch of the shell | batch-16#28 |
| `DebugAIObstacles` | 0xC000F0 | 0x9BC | reads (client) | debug / editor | catch-all | row RW 0xC000F0, same — constructor write and/or parser only; no `[0xDE4364]` read, or a client switch of the shell | batch-16#29 |
| `MaxShellScreens` | 0xBFF640 | 0x38 | reads (client) | debug / editor | catch-all | batch-19#16: 0 accesses to `[GD+0x38]` — constructor write and/or parser only; no `[0xDE4364]` read, or a client switch of the shell | batch-19#16 |
| `RightMouseAlwaysScrolls` | 0xBFF790 | 0x5D | reads (client) | debug / editor | catch-all | batch-20#16: 137 `[reg+0x5D]` instructions, none from `[0xDE4364]` — constructor write and/or parser only; no `[0xDE4364]` read, or a client switch of the shell | batch-20#16 |
| `UseCloudPlane` | 0xBFF7A0 | 0x5F | reads (client) | debug / editor | catch-all | batch-22#18: constructor write RW 0x642A54 only — constructor write and/or parser only; no `[0xDE4364]` read, or a client switch of the shell | batch-22#18 |
| `UseHalfHeightMap` | 0xBFF6E0 | 0x4D | reads (client) | debug / editor | catch-all | batch-22#20: the constructor write RW 0x642A38 is the only GameData one — constructor write and/or parser only; no `[0xDE4364]` read, or a client switch of the shell | batch-22#20 |
| `PlayStats` | 0xC00870 | 0xC30 | reads (client) | debug / editor | catch-all | the stats overlay switch — constructor write and/or parser only; no `[0xDE4364]` read, or a client switch of the shell | — |
| `FixedSeed` | 0xC004F0 | 0x9E8 | reads (client) | debug / editor | catch-all | a developer determinism switch; the port's RNG is its own — constructor write and/or parser only; no `[0xDE4364]` read, or a client switch of the shell | — |
| `TintUnitIfPathingForMoreThan` | 0xC01050 | 0x11C0 | reads (client) | debug / editor | catch-all | a client debug flash for a slow path search (RW 0x6ED163 -> RW 0x6ED185, RED flash); arena README correction 8 — constructor write and/or parser only; no `[0xDE4364]` read, or a client switch of the shell | batch-22#12 |
| `UseHelpTextSystem` | 0xC00A00 | 0xDD0 | reads (client) | debug / editor | catch-all | the help-text toggle; no reader — constructor write and/or parser only; no `[0xDE4364]` read, or a client switch of the shell | batch-22#21 |
| `ProgressMovieOffset` | 0xC009E0 | 0xDC0 | reads (client) | debug / editor | catch-all | the loading-movie frame; the port's load screen is its own — constructor write and/or parser only; no `[0xDE4364]` read, or a client switch of the shell | batch-20#12 |
| `ProgressMovieSize` | 0xC009F0 | 0xDC8 | reads (client) | debug / editor | catch-all | same — constructor write and/or parser only; no `[0xDE4364]` read, or a client switch of the shell | batch-20#13 |
| `FirewallBehavior` | 0xC00550 | 0xA4C | reads (client) | debug / editor | catch-all | the port has no firewall negotiation — constructor write and/or parser only; no `[0xDE4364]` read, or a client switch of the shell | — |
| `FirewallPortOverride` | 0xC00560 | 0xA54 | reads (client) | debug / editor | catch-all | same — constructor write and/or parser only; no `[0xDE4364]` read, or a client switch of the shell | — |
| `FirewallPortAllocationDelta` | 0xC00570 | 0xA58 | reads (client) | debug / editor | catch-all | same — constructor write and/or parser only; no `[0xDE4364]` read, or a client switch of the shell | — |
| `SkipMapUnroll` | 0xC00530 | 0x2D | reads (client) | debug / editor | catch-all | a map-cache switch (`+0x2D`); the port has no map unrolling — constructor write and/or parser only; no `[0xDE4364]` read, or a client switch of the shell | batch-20#25 |
| `LiveCampaignMode` | 0xBFF720 | 0x86 | reads (client) | debug / editor | catch-all | a Living World campaign switch; the port has no such mode — constructor write and/or parser only; no `[0xDE4364]` read, or a client switch of the shell | — |
| `AnimationSharingCap` | 0xC008B0 | 0xC40 | not read | not read | catch-all | RotWK does not use it — constructor write RW 0x643396 is the only access to `+0xC40` / `RW 0xDE4FA4` | — |
| `AnimationSharingFrameTolerance` | 0xC008C0 | 0xC44 | not read | not read | catch-all | RotWK does not use it — RW 0x64339C; 0 accesses to `+0xC44` | — |
| `AnimationSharingSpeedTolerance` | 0xC008D0 | 0xC48 | not read | not read | catch-all | RotWK does not use it — RW 0x6433A4; 0 accesses to `+0xC48` | — |
| `AnimationSharingWorryThreshold` | 0xC008E0 | 0xC4C | not read | not read | catch-all | RotWK does not use it — RW 0x6433B4; 0 accesses to `+0xC4C` | — |
| `AnimationSharingDrasticThreshold` | 0xC008F0 | 0xC50 | not read | not read | catch-all | RotWK does not use it — RW 0x6433CE; 0 accesses to `+0xC50` — the whole `AnimationSharing*` group is dead in 2.01 (batch-15#20-#24, README check) | — |
| `NetworkFPSHistoryLength` | 0xC007D0 | 0xC08 | not read | not read | catch-all | RotWK does not use it — the GameData xfer block RW 0xA28BB0 / RW 0xA28C10 / RW 0xA28D70 (save) is the only user | batch-19#23 |
| `NetworkLatencyHistoryLength` | 0xC007E0 | 0xC0C | not read | not read | catch-all | RotWK does not use it — same | batch-19#25 |
| `NetworkRunAheadMetricsTime` | 0xC007F0 | 0xC10 | not read | not read | catch-all | RotWK does not use it — same | — |
| `NetworkCushionHistoryLength` | 0xC00800 | 0xC14 | not read | not read | catch-all | RotWK does not use it — batch-19#21: only the xfer | batch-19#21 |
| `NetworkRunAheadSlack` | 0xC00810 | 0xC18 | not read | not read | catch-all | RotWK does not use it — same | — |
| `NetworkKeepAliveDelay` | 0xC00820 | 0xC1C | not read | not read | catch-all | RotWK does not use it — same | batch-19#24 |
| `NetworkDisconnectTime` | 0xC00830 | 0xC20 | not read | not read | catch-all | RotWK does not use it — same | — |
| `NetworkPlayerTimeoutTime` | 0xC00840 | 0xC24 | not read | not read | catch-all | RotWK does not use it — same | — |
| `NetworkDisconnectScreenNotifyTime` | 0xC00850 | 0xC28 | not read | not read | catch-all | RotWK does not use it — same | batch-19#22 |
| `WaterPositionX` | 0xBFF840 | 0x6C | not read | not read | catch-all | RotWK does not use it — batch-23#1: `[reg+0x6C]` reads exist but the base is not attributed; the water runtime object copies the field (INFERENCE) | batch-23#1 |
| `WaterPositionY` | 0xBFF850 | 0x70 | not read | not read | catch-all | RotWK does not use it — batch-23#2 | batch-23#2 |
| `WaterPositionZ` | 0xBFF860 | 0x74 | not read | not read | catch-all | RotWK does not use it — batch-23#3 | batch-23#3 |
| `WaterType` | 0xBFF890 | 0x80 | not read | not read | catch-all | RotWK does not use it — batch-23#4 | batch-23#4 |
| `WaterExtentX` | 0xBFF870 | 0x78 | not read | not read | catch-all | RotWK does not use it — batch-23 group | batch-22#29 |
| `WaterExtentY` | 0xBFF880 | 0x7C | not read | not read | catch-all | RotWK does not use it — batch-23 group | batch-22#30 |
| `FeatherWater` | 0xBFF8A0 | 0x90 | not read | not read | catch-all | RotWK does not use it — batch-23 group | — |

## The `gameplay` rows as proposed next ports

One line each; nothing here is started by Port 70.

* `BaseRegenDelay` + `BaseRegenHealthPercentPerSecond` — **one port**: a body near its base regains health after a delay. First find the reader: the pair is one mechanism (`BaseRegenDelay` gates `BaseRegenHealthPercentPerSecond`), and the batch-16#18 leak (reader on the wrong object) means the reader's search must start over from the body's damage path, not from the offset.
* `CommandCenterHealAmount` + `CommandCenterHealRange` — **one port**: a command centre heals what stands inside its radius; needs the `COMMANDCENTER` kind and a per-tick heal on the contain/partition query.
* `BuildSpeed` + `MultipleFactory` — **one port**: the global build factor and the second-factory bonus, together with `ProductionUpdate`'s speed line.
* `MinLowEnergyProductionSpeed` + `MaxLowEnergyProductionSpeed` + `LowEnergyPenaltyModifier` — **one port**: production while the player's power is negative (clamped speed + a penalty modifier).
* `RefundPercent` — a port of its own: the second refund row (table `RW 0xC06998`), distinct from the ported `SellPercentage`; check which one RotWK applies when a structure is sold.
* `HealthBonus_Veteran` / `_Elite` / `_Heroic` — **one port**: rank health bonuses on top of the `ExperienceLevel*` data the port already applies.
* `HumanSoloPlayerHealthBonus_Easy` / `_Normal` / `_Hard` + `AISoloPlayerHealthBonus_*` — **one port**: the difficulty health bonus in a solo game, six rows.
* `DefaultVoiceAttackChargeTimeout`, `DefaultMaxDistanceForEngaged`, `DefaultEngagedStateTimeout` — three defaults of the existing charge / engage code; each is a small port once its reader is read.
* `UseCameraInReplay` — a client row (the replay camera), not a rule: list it with `presentation` when it is taken.
