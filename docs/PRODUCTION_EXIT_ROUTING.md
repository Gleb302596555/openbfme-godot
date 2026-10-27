# Retail production doorway routing

Production exits use the RotWK quick-path branch in both Faithful and Uber.
There is no producer-footprint exemption or special line-passability rule.
Docking, harvesting and ordinary movement retain their normal path checks.

Verified directly in the user's RotWK 2.01 game.dat with LLVM objdump:

- RW 0x666036..0x66605C tests state 7 and AI byte +0x3BA.
- RW 0x666065 calls computeQuickPath (0x665A83) without a line test.
- On success, RW 0x666079 clears the flag and 0x66607C calls 0x6654AD
  with CMD_FROM_AI. That forwards through 0x6636D9 to the machine goal
  setter at 0x8DB810. It does not issue a new move order.
- AIFollowPathState entry (RW 0x74F88C) tests state 7 at 0x74F8E6 and
  sets +0x3BA at 0x74F8EE. Update at 0x74FA18 publishes its segment goal
  at 0x74FA28 before calling the internal move update. The port arms the
  flag only at entry, rather than rearming each waypoint.
- The earlier review address 0x74BB56 also writes this byte, but belongs to
  AIFollowWaypointPathExactState update, not production entry: vtable
  0xC29068, name getter 0x744698 -> string 0xC290B0. Production uses the
  AIFollowPathState table 0xC29228, name getter 0x744777 -> 0xC29270.

The matching BFME2 `AIUpdateInterface_computePath.cpp` consumes the same
flag and updates the goal after success. `AIFollowPathStateMethods.cpp`
arms it in onEnter, and update publishes the active segment goal each tick.
The port mirrors that goal bookkeeping for exit paths so the first exit
waypoint cannot remain the machine's goal after a later rally segment.

The previous Uber-only approach and broad allied-goal line exemption have
been removed. A blocked first leg is intentionally not rejected by the
ordinary pathfinder: retail constructs a two-node straight path. Later
searches use normal rules after the flag is consumed.

Tests cover unconditional first paths through a blocked line, one-shot
consumption, rotated factories and rallies in both profiles, cancellation,
and the original production-clearing behavior. S-166 retains its unrelated
movement gaps; it no longer reports the quick-path branch as unported.
