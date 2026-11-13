// OpenBFME. GPL-3.0.
// Derived from Command & Conquer Generals Zero Hour, (c) 2001-2003 Electronic Arts Inc., GPL-3.0.
//
// ObjectProjectileLauncher (lane PROJ-1): ProjectileNugget::deliver of the binary. A weapon's ProjectileNugget makes an object of its ProjectileTemplateName, gives it the firing object as
// producer and tells the first of its modules that answers as a projectile update interface to launch at the victim (slot 5, RW 0x90FA48) or at a position (slot 6, RW 0x90FC70).
//
// TARGET FACTS (RotWK game.dat, caveat S-001): the object is made by ThingFactory::newObject (RW 0x6D165E) for the controlling player's default team of the source; the module loop walks the
// object's module list (RW + 0x24C) asking each for the projectile interface (vtable + 0x48) and stops at the first answer; with none the new object is destroyed (RW 0x90FB6C); the
// producer is set (RW 0x68B6A1) before the launch call, whose arguments are (victim | null, position | null, source, weapon slot, barrel, the firing weapon, the warhead, null); the slot is the
// nugget's WeaponLaunchBoneSlotOverride when it is 0 .. 5 (RW 0x90FC05..0x90FC17). ProjectileSelf selects the source object before testing ProjectileTemplateName
// (RW 0x90FACB / 0x90FDAA). For position targets its launch transform is the source's transform (RW 0x90FDB6 .. 0x90FE2E); for object targets it uses the launch bone.
// Without ProjectileSelf, a nugget without a ProjectileTemplateName (the resolved template at RW + 0x14C is null) launches nothing.
// STREAM (issue #99 / PORT-27): after the projectile is flying, the deliver records it in the firing weapon's ProjectileStreamName object
// (RW 0x90FB7A .. 0x90FC5A, the stream half of the same object-target deliver; see ProjectileLauncher.cpp).
// CONTAINER AMMO (RW 0x90FA81 .. 0x90FAC9, lane SPELL-2): a fired weapon whose template sets ProjectileFilterInContainer (Weapon + 0x120, RW 0x762977)
// on a source that has a contain takes the first contained object the filter allows OUT of the container (contain slot 0x13C RW 0x866367, which removes
// it at RW 0x8663AC) and that object is the projectile -- the cave troll's held goblin, thrown by ProjectileFilterInContainer = NONE +INFANTRY. The
// launch transform is still the source's (Weapon RW 0x6CAB85 at RW 0x90FAAA), and the object flies with its own BezierProjectileBehavior. The test
// runs before ProjectileSelf and before ProjectileTemplateName (RW 0x90FA8E `je` to RW 0x90FACB).

#pragma once

#include "GameLogic/Combat/WeaponDelivery.h"

class ObjectProjectileLauncher : public ProjectileLauncher
{
public:
	void launch(GameLogic &logic, const ProjectileShot &shot) override;
};
