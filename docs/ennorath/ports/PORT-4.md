# PORT-4 — DisableScatterForTargetsOnWall (#56)

Read with `tools/mod/rw.py ... dis` from RotWK 2.01 game.dat (S-001).

- RW 0x6CCC59 .. 0x6CCC6C: ScatterRadiusVsInfantry (+0x14C) <= 0 skips the infantry miss.
- RW 0x6CCC6E .. 0x6CCC7C: no victim or target template without INFANTRY skips it.
- RW 0x6CCC7E .. 0x6CCC82: DisableScatterForTargetsOnWall (+0x3D) false jumps to the miss write at 0x6CCC9B.
- RW 0x6CCC84 .. 0x6CCC92: otherwise query the pathfinder at the victim position, second argument **0** (do not require a clear cell).
- RW 0x6CCC97 .. 0x6CCC9F: true skips the infantry miss; false sets it.
- RW 0x6EA857 .. 0x6EA887: lookup the ground cell; no cell returns false. The six-bit layer at bits 4..9 must be **greater than** 0x10 (not equal: ramp is excluded). Only when the second argument is nonzero must the low four type bits also be zero.
- RW 0x6CCCA7 .. 0x6CCCB5: HitPercentage is evaluated separately afterwards. Suppressing the infantry miss does not suppress this roll.

Existing port mechanism: Pathfinder::isPointOnWall already implements this query in AIPathfindWalls.cpp. WeaponDelivery's ground-only comment is stale; use the existing query, not a new wall flag or a constant false. No new simulation state is needed.
