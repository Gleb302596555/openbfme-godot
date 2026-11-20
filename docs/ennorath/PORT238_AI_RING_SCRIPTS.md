# PORT-238: AI ring-script creation candidates (partial)

This is a **bounded source inventory**, not the requested complete inventory of ring bearers, powers, upgrades or appearance. It does not establish effective template inheritance, the mounted archive chosen at runtime, script activation in an actual game, movement/contain delivery, transformations or visuals. Those remain open in #561.

## Evidence and scope

The extracted Ennorath 1.9.8.3 INI archive (MD5 `c53b6dea885cab044f80e783565f41f3`) contains `libraries/ai_men of the west/ai_men of the west.map` (MD5 `96e708b4a7fcff42b27218fd94a502c4`). The repository's independent readers `tools/maps/oracle/census.py`, `tools/script/script_census.py` and `tools/mod/script_map.py` decode its RefPack/chunk tree and resolve action and condition names against `ScriptTemplateTables.inc`. This scan read that extracted file directly, **not** a full runtime mount: it found 1,595 scripts in this map, including exactly 56 enabled action records `CREATE_NAMED_ON_TEAM_AT_OBJECTTYPE` in seven `Ring/<faction> RING` groups, eight player slots each. No script bytes or full script transcript are committed here. `docs/ennorath/SCRIPT_MAP.md` is an earlier snapshot and its claim that these action cases are absent is **stale** on the current branch: main commit `8ba1f721` (LOCAL-81) added them. Do not regenerate the shared generated map as part of #561.

Each of the 56 creation scripts is initially marked inactive and not one-shot. Each has the enabled non-inverted `TEAM_SOME_HAVE_OBJECT_STATUS` condition for its own `Player_N/teamPlayer_N` and `HOLDING_THE_RING`, an enabled create action with the named reference `Ringheld`, followed by `TEAM_ENTER_NAMED` for the same team and reference. Other actions and, for most factions, one or more preceding nearest-existing-hero reference actions also occur; an existing reference and a successful create are distinct alternatives and must not be conflated. The slots differ in player/team number. A separate `Singlespieler` script is initially inactive and can enable `Ring` on an AI difficulty upgrade event; each faction's initially active one-shot `RingS__N` checks starting position N and enables the corresponding creation script. Parent group state, activation ordering, all prerequisites and actual runtime execution are **not** proven by these flags alone.

| Script group | Create template | Target object type used for placement | Direct INI declaration / ring-entry evidence |
| --- | --- | --- | --- |
| Isengart | `IsengardSaruman_Mod` | `IsengardOrthancMod` | `evilfaction/units/isengard/saruman.ini:1,1298` |
| Angmar | `AngmarWitchking_mod` | `AngmarVasallenhaus` | `evilfaction/units/angmar/angmarwitchking.ini:22,1315` |
| Mordor | `MordorGorthaur` | `BaraddurMod` | `evilfaction/units/mordor/mordorsauron.ini:1166,2311` |
| Lorien | `LothlorienGaladriel` | `LothlorienBarracks2` | `goodfaction/units/lothlorien/lothloriengaladriel.ini:8,961` |
| Imladris | `ImladrisElrond` | `ImladrisBarracks` | `goodfaction/units/imladris/imladriselrond.ini:8,1783` |
| Gondor | `GondorGandalfWhite_mod` | `GondorBarracks` | child of `GondorGandalf_mod`: `goodfaction/units/gondor/gondorgandalf.ini:2055,2789` |
| Rohan | `RohanRadagast` | `RohanFortress` | `goodfaction/units/rohan/rohanradagast.ini:8,965` |

The cited ring-entry lines are direct declarations of `UpgradeForRingEntry = Upgrade_RingHero Upgrade_FortressRingHero` inside a `CitadelSlaughterHordeContain` in each source object. They are candidate evidence, not proof that the runtime effective object gets both upgrades. For example, the Imladris object separately declares `DoCommandUpgrade` and a toggle to `ImladrisElrond_RingHero`; the created base object is not itself proof that this final form exists or functions. Similarly, the Galadriel base has a conditional ring-decision command set, not proof of the outcome of that decision. The table's placement type is an input to a nearest-template lookup, not necessarily a fortress or the player's own building; do not infer ownership filtering from the script name.

## Engine status and test boundary

| Item | Status / evidence |
| --- | --- |
| `CREATE_NAMED_ON_TEAM_AT_OBJECTTYPE` | **Already ported** by LOCAL-81 `8ba1f721`: script action dispatch RW `0x7CBD6F` to `0x7C5B3B`; `ScriptActions.cpp::createOnTeamAtWaypoint` looks for an existing live named object, resolves team and nearest matching placement type if no waypoint, then uses the creation host and build-completion callback. This is source/previous-lane evidence, not an independent complete RW reread here. |
| `TEAM_ENTER_NAMED` | **Already ported** by LOCAL-81: RW `0x7CBFCB -> 0x7BFA20 -> 0x7724BE`; `ScriptActionsOrders.cpp` looks up the named target and orders AI group members to enter it. Admission, queue and ring delivery remain separately bounded by S-7321 / S-7446 and `PORT238_RW_ANALYSIS.md`. |
| LOCAL-81 coverage | `engine/tests/test_local_81.cpp` includes synthetic creation and team-enter cases. Its Ennorath scenario reads the actual map/templates when files are available, manually enables the Ring group, creates a carrier with the ring status and checks only Isengard's named base object/team/position after scripted updates. It does not check delivery, upgrade, transformation, powers or visuals; missing game files cause the scenario to return early. Its recorded upstream night-watch result is not a fresh run on this branch. Run `LOCAL-81*` on the current complete build and explicitly report whether the real-data case ran. |
| Other script actions and activation | **Not independently audited** here. LOCAL-81 covers several, but the reference/activation chain and game scheduling must not be treated as verified merely because action names have cases. |
| Other bearers, all seven forms and powers | **Not read / incomplete**: this single AI library is only one discovery path. Additional libraries, campaign/WOTR maps, construction, starting objects, OCL, includes, transforms, inheritance and upgrade/command/visual consumers must be resolved before claiming a complete inventory. |

The available `workspace/RotWK/game.dat` enables RW comparison but is not a complete RotWK installation; the extracted Ennorath files are outside the repository. Neither the map inventory nor a synthetic test substitutes for a playable mod check.
