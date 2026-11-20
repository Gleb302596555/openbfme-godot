# Ennorath map sweep

Every map of the Ennorath install starts a two-slot skirmish (FactionMen against an easy computer FactionMordor) and plays; one map per process (`tools/maps/map_sweep.py`, test `ENNORATH sweep maps`). Rows: `MAP_SWEEP.tsv`. An error group is one text with names and numbers left out: one group is one mechanism to fix, the maps column says where it shows.

Maps: 336; errors: 255, not in the map cache: 72, ok: 9

## Maps that do not start or play

| map | status | error |
|---|---|---|

## Not in the map cache (72): not a skirmish map, or left out by the mod's mapcache.ini

`camera_demo`, `cin amon sul - castle pan2`, `cin amon sul - castle pan`, `cin amon sul - invasion`, `cin angmar - rogash run`, `cin angmar - witchking rally`, `cin barrow downs - sorcerer`, `cin barrow downs - swamp pull`, `cin fornost - invasion`, `cin fornost - legions seige`, `cin fornost - peaceful pan - clean`, `cin fornost - peaceful pan`, `cin fornost - shade of wolf`, `cin fornost - witchking fire`, `cin fornost - witchking`, `cin gundabad - snowy landscape2`, `cin gundabad - snowy landscape2_bright`, `cin gundabad - snowy landscape`, `cin gundabad - witchking`, `cin north downs - waterfall`, `cin rhudaur - hwalder battle`, `cin rhudaur - thrallmaster rally`, `cin thrallmaster battle - ithilien`, `createahero`, `map advanced tutorial`, `map ang amon sul`, `map ang angmar`, `map ang barrow downs`, `map ang barrow wights`, `map ang bonus`, `map ang carn dum`, `map ang dark eye`, `map ang fornost`, `map ang gundabad`, `map ang rhudaur`, `map beginner tutorial`, `map evil erebor`, `map evil fornost`, `map evil grey havens`, `map evil lorien`, `map evil mirkwood`, `map evil rivendell`, `map evil shire`, `map evil withered heath`, `map good blue mountain`, `map good celduin`, `map good dol guldur`, `map good erebor`, `map good ettenmoors`, `map good grey havens`, `map good high pass`, `map good rivendell`, `map mp argonath`, `map mp defence of arda`, `map mp fall back 4p`, `map mp fords of isen ii`, `map mp grey mountains`, `map mp harlindon`, `map mp old brown lands`, `map mp paths of the dead`, `map mp the heubris`, `map mp tournament gundabad`, `map mp tournament hills`, `map mp tournament mp1`, `map mp tournament rhudaur`, `map mp tournament udun`, `map mp tournament westmarch`, `map mp withered heath`, `map wor gondor`, `randomizer game of chances plains of rohan`, `shellmap1`, `shellmapbackup`

## Error groups (most maps first)

| maps | where | error |
|---:|---|---|
| 255 | play | OnCreated handler failed: LUA Alert: error: attempt to call a nil value |
| 12 | load | object <n> <name>: no template of that name |
| 1 | load | CastleBehavior of object <n> (IsengardFortress): CastleToUnpackForFaction names no base for the faction <name> |
| 1 | load | ElvenLegolas W3DScriptedModelDraw: hide/show of sub object arrow with no model |
| 1 | load | IsengardCitadelUnpack W3DScriptedModelDraw: hide/show of sub object FELLOWSHIPBADGE with no model |
| 1 | play | OnCreated handler failed: LUA Alert: error: bad argument #<n> to <name> (string expected, got nil) stack traceback:    <n>:  function <name> [C]    <n>:  functi |
| 1 | play | movement failure of AgandaurLightSphere1: S-<n>: ZAxisBehavior other than NO_Z_MOTIVE_FORCE, FLOATING_Z, SEA_LEVEL, SURFACE_RELATIVE_HEIGHT, ABSOLUTE_HEIGHT, FI |
| 1 | play | movement failure of AgandaurLightSphere2: S-<n>: ZAxisBehavior other than NO_Z_MOTIVE_FORCE, FLOATING_Z, SEA_LEVEL, SURFACE_RELATIVE_HEIGHT, ABSOLUTE_HEIGHT, FI |
| 1 | play | movement failure of AgandaurLightSphere3: S-<n>: ZAxisBehavior other than NO_Z_MOTIVE_FORCE, FLOATING_Z, SEA_LEVEL, SURFACE_RELATIVE_HEIGHT, ABSOLUTE_HEIGHT, FI |
