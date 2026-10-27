# Uber production doorway routing

Standalone recruits in the Uber profile ignore their producer for the first
exit segment and use a continuous quick path when that doorway line passes
terrain/enemy checks. Allied recruits and their goal reservations do not bend
this first leg into the nearest wall. The exemption ends at the next segment
or on interruption. Ordinary docking/harvesting line tests keep their existing
rules, even when they combine an ignored object with path-through-units.
Faithful retains the upstream path, ignored-source and cleanup behavior.

This is an Uber routing improvement, not a retail parity claim. The RotWK 2.01
review identified a different mechanism: exit-state entry sets a one-shot flag
at RW 0x74BB56; computePath consumes it in state 7 at RW 0x666036, calling
computeQuickPath at RW 0x665A83 without ignoring the producer or testing the
line. The flag is cleared afterward. The port's S-166 records earlier exit and
guard-goal regressions from enabling that branch, including the machine goal
update. Resolving that parity gap remains separate from this Uber option.

Tests cover rotated factories with a rally behind the building, allied door
reservations, cancellation and neighboring obstacles in Uber. Original
Faithful production-exit clearing assertions remain unchanged. The manual
`fortress_exit_test.gd` explicitly selects Uber and checks hero/builder exits.
