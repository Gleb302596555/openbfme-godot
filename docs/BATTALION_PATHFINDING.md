# Shared battalion path retry

Extracted from BFME1 gameplay PR #109. This is an inferred OpenBFME correction,
not a verified retail algorithm, and applies to battalions in BFME1, BFME2, and RotWK.

The coarse corridor can leave a battalion at a partial endpoint behind a castle
wall even when an open gate provides a longer route. If the hierarchical search
fails or returns a partial path, retry once with the existing cell search without
corridor ordering. Each search retains its configured cell limit. Replace the
original result only if the retry completes; otherwise retain the original
partial route. A closed barrier remains impassable, and goal step-back retains
its partial-result status. Individual units keep their existing search behavior.
The shared pathfinder uses the existing `PK_HORDE` kind without consulting the game mode.

`test_pathperf1_hier.cpp` checks a longer route around an open-ended wall,
preserves individual-unit behavior, and keeps paths on the starting side of
a sealed wall.
`godot/tests/bfme1_castle_path_test.gd` issues one move behind Gondor's castle on
Fords of Isen and Rohan's castle on Anorien and checks the battalion and soldiers:

```sh
BFME1_INSTALL=/path/to/BFME1 godot --headless --path godot --script res://tests/bfme1_castle_path_test.gd
```

The installed-asset test also requires working BFME1 opening armies; that setup
is implemented in #109 and is not included in this standalone pathfinding fix.
No retail game data is included.
