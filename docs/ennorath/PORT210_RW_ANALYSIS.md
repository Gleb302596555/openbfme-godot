# Port 210: wall contact investigation (in progress)

The starting tree at b327df7 has `HordeContain::collideWithWall` but its only
callers are tests in `test_hud_horde2_retail.cpp`; the gameplay contact passes
in `AIWorld.cpp` do not call it. Do not add a second entry to the climb itself.

DONOR BFME2 0x0046950D (`Code/GameEngine/Source/GameLogic/Object/Contain/HordeContainIface24Slot0.cpp`): the +0x24 interface slot 0 tests the target,
locomotor and absolute facing dot, then calls the horde interface's wall setter.
This is a lead, not proof of the caller in RotWK.

RW disassembly from `game.dat` (MD5 daed3668006cd90f01c34e5a7da1901f):

* 0x86C61E..0x86C6E8: null target, missing SCALEABLE_WALL bit, destroyed
  target, previously remembered ID, absent AI / locomotor, or disabled
  ScalesWalls each return. The absolute dot of owner and target direction must
  be at least the float at 0xBD869C before slot +0x23C receives the target ID.
* 0x5E8956..0x5E89B9: locomotor look-ahead skips a contained object and a
  HORDE, and requires ScalesWalls and CAN_CLIMB_WALLS. This path does not
  establish the horde caller.
* 0x68E31F..0x68E411: collision registration excludes NO_COLLIDE and conditionally
  excludes HORDE according to AI data, before calling 0xB6C470. The full
  registration and contact eligibility still need tracing.
* 0xB6D060..0xB6D1A9: contact table walk; an uncached pair is tested through
  virtual slot +0x20, successful contacts dispatch virtual +0x24 to each
  surviving endpoint, reversing the normal for the second. The adapter at vtable 0xC12368 slot +0x24 is 0x68EEC3: it iterates
  modules, calls their behavior-interface slot +4 to obtain the collide
  interface, and calls that interface slot 0. It checks self-destruction before
  each callback (0x68EF00..0x68EF1E).

The subsequent class and dispatch trace below establishes the collision manager
as a caller. The current port includes a narrow static-wall stand-in; it is not
a full collision manager and its gameplay and synthetic tests still need to be
run. Pair shape, ordering and interactions with existing passes remain subject
to that limitation.

## Class and dispatch chain

* DONOR BFME2 0x0046950D (`HordeContainIface24Slot0.cpp`),
  0x0046F543 (`HordeContainCtor.cpp`), and 0x0024B8F0
  (`HordeContainFriendNew.cpp`): creation installs the +0x24 collide table.
* RW 0x64B5D5 allocates 0x30C bytes and calls constructor 0x8727F9;
  0x872846 installs vtable 0xC5D250 at module +0x24. The vtable's slot 0
  is 0x86C61E and slot 1 is 0x7FEAC1 (false). RW 0x68EEC3 dispatches
  module collide slot 0 from the object collision adapter. Thus the collision
  manager callback is the missing route, not the locomotor look-ahead.
* RW 0xB6D730 rejects equal nonzero collision groups at 0xB6D73F..0xB6D74F
  and orders IDs at 0xB6D755..0xB6D76B. RW 0x68EEA5 returns group 1 for
  IMMOBILE, otherwise container identity or the object itself.
* RW 0x86BCF9 sets SCALING_WALL only on first wall, clears SCALING_WALL and
  SCALING_WALL_HORDE when passed zero, and writes the wall ID. RW 0x86D170
  increments an inactivity count while no member has those climb conditions,
  and clears on the fifth such pass.

Starting-tree departure: `HordeContain` did not expose `getCollide`, so even
the existing moving-object and gate passes could not dispatch its retail
entry. The branch exposes it and adds a wall-only stand-in for the separate
static wall contact gap in `AIWorld::processCollisions` (S-780). The stand-in
excludes pairs already covered by mobile and gate passes and checks collision
registration and geometry, but its equivalence to retail has not been shown.

## Related existing stop S-1751: horde member AI update

RW 0x66E6AC..0x66E6CA checks SCALING_WALL and the attacking/not-moving
condition, sets the AI wall-scaling byte (AI +0x3CE; disassembly uses the
secondary-interface pointer +0x3BE), and clears the local quick-update flag.
RW 0x66E6DB..0x66E70C selects the normal AI update 0x6695EF when that byte
is set instead of the horde-member quick update 0x66C748. When SCALING_WALL
is absent, RW 0x66E711..0x66E720 clears the byte. The quick update at
0x66C755..0x66C76B clears CLIMBING.

DONOR BFME2 (`Code/GameEngine/Source/GameLogic/Object/Update/AIUpdate/AIUpdateInterfaceUpdate.cpp`,
0x0026C4EB / 0x0026A05D are the two update bodies) corroborates the branch;
RW is the source of the offsets and branch above. In the port,
`AIUpdateInterface::update` / `runsHordeMemberUpdate` and
`AIUpdateInterface::hordeMemberUpdate` in `AIUpdate.cpp` and
`AIUpdateHordeMember.cpp` do not implement this exclusion (existing S-1751).
This is a separate shared AI update mechanism, not the HordeContain collide
caller, and adding a shortcut only for this task would violate the scope rule.
Whether it prevents the full walking/climbing scenario must be tested; the
collision-only tests cannot establish that it is harmless. Do not close S-1751
or claim full in-game wall scaling based on the collision tests alone.

## Synthetic moving-wall diagnostic

The test added in `test_port_210.cpp` sends the horde across the position of a
single wall installed in the pathfinder, without teleporting either object. It
checks contact and progress beyond the wall, not a climb. A temporary diagnostic
runner (not the required complete `openbfme_tests`) tried requiring a member's
height to increase. With the test's short wall the horde contacted it but went
around: horde Y ranged from 275 to 305, the horde and member ended at (405,
305, 0), and the height assertion failed. Increasing the synthetic wall's
minor radius to 100 produced a detour to Y 185; increasing it to 600 led to a
path to Y 953.322 and no observed contact. These are fixture observations, not
evidence that S-1751 causes or does not cause a real climbing failure. A
representative wall/path scenario still needs to be established before making
that conclusion.
