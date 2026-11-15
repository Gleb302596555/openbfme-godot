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

Next: trace adapter vtable and dispatch to the contain interface, establish
eligibility, grouping, shape, order and interactions with existing gate passes;
then implement and test without duplicating contacts. No conclusion that this
contact manager is the horde caller is claimed yet.

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

Port departure: `HordeContain` does not expose `getCollide`, so even the
existing moving-object and gate passes cannot dispatch its retail entry. A
separate static wall contact gap remains in `AIWorld::processCollisions`
(S-780); any wall-only stand-in must not duplicate gate contacts, and must
respect collision registration and geometry.
