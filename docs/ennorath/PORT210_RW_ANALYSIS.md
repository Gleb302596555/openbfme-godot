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
  surviving endpoint, reversing the normal for the second. The chain from that
  adapter to the horde interface has not yet been established.

Next: trace adapter vtable and dispatch to the contain interface, establish
eligibility, grouping, shape, order and interactions with existing gate passes;
then implement and test without duplicating contacts. No conclusion that this
contact manager is the horde caller is claimed yet.
