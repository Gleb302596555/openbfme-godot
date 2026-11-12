// OpenBFME. PORT-27 (issue #99) tests: Weapon/ProjectileStreamName, the object a weapon makes when it fires.
//
// How the binary does it (RotWK 2.01 `game.dat`, md5 daed3668006cd90f01c34e5a7da1901f, S-001 caveat; every address read through
// `tools/mod/rw.py dis`):
//
//   * the field: WeaponTemplate row RW 0xC173F8, ascii parser RW 0x42EE5E, WeaponTemplate + 0x178.
//   * RW 0x6CD1FD, called by privateFireWeapon's step 6 (RW 0x6CF012, after the status write-back at RW 0x6CDCE7):
//       RW 0x6CD212  call RW 0x401E64   empty GameAsciiString -> the whole block is skipped
//       RW 0x6CD227  call RW 0x449681   the weapon's owner; none -> + 0x5C is cleared (RW 0x6CD233)
//       RW 0x6CD23D  call RW 0x449681   the stream object at + 0x5C; still there -> nothing to do (RW 0x6CD244 `jne 0x6CD294`)
//       RW 0x6CD246  and [esi + 0x5C], eax   a stale id is dropped
//       RW 0x6CD252  call RW 0x6D1305   ThingFactory::findTemplate(name) (factory at RW 0xDE4A40); null -> done
//       RW 0x6CD285  call RW 0x6D165E   newObject(template, [player + 0x30C] default team, status 0, position (0,0,0) from RW 0xA3CF28)
//       RW 0x6CD291  mov [esi + 0x5C], eax   the new object's id (Thing + 0x74) is kept in the weapon
//     One object per weapon, made on the first shot that gets this far and reused by every later shot.
//   * RW 0x90FB7A .. 0x90FC5A, the object-target ProjectileNugget deliver, the rest of what a shot does with the stream:
//       RW 0x90FB80  push [esi + 0x5C] -> RW 0x90FB85 call RW 0x449681   the stream object of the shooting weapon
//       RW 0x90FBEE  call RW 0x74094C   the stream object's setPosition = the OWNER's geometry centre (owner + 0x38)
//       RW 0x90FC5A  call RW 0x7408A8   ProjectileStreamUpdate::addProjectile(owner id, victim | null, victim id | 0, projectile id, &owner centre)
//     The position-target deliver (RW 0x90FC70) has no such block: only an object target is recorded.
//   * RW 0x6CD0BE, the Weapon destructor (reached through the vtable RW 0xC18404 slot 0 = the deleting wrapper RW 0x6CDB57):
//       RW 0x6CD0E8  call RW 0x449681 / RW 0x6CD0F4 call RW 0x62BBAB   GameLogic::destroyObject of the stream the weapon holds
//     So the stream lives exactly as long as the weapon that made it.
//
// Synthetic data only (combattest::CombatWorld), no game files, no Ennorath files. The synthetic stream object is the one the mod's own stream
// templates have (data/ini/object/evilfaction/units/wild/firedrake.ini: KindOf INERT, a W3DProjectileStreamDraw and an empty
// ProjectileStreamUpdate), written from the RotWK field syntax; only the template NAME in the tests comes from the mod.
#include "doctest.h"

#include "CombatTestUtil.h"

#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/Combat/ObjectWeapons.h"
#include "GameLogic/Combat/ProjectileLauncher.h"
#include "GameLogic/Combat/WeaponDelivery.h"
#include "GameLogic/Module/ProjectileStreamUpdate.h"

using namespace combattest;

namespace
{

// The stream object as the mod writes it (firedrake.ini `Object FlamethrowerProjectileStream`): INERT, the draw module, the (fieldless) update.
// Port27Stream sits on no weapon at all unless a weapon names it; Port27NoUpdate is the same object without the update module (retail then has a
// null interface at RW 0x90FBEE and the record does nothing).
const char kPort27Objects[] =
	"Object Port27Stream\n"
	"  KindOf = INERT NO_COLLIDE\n"
	"  Behavior = ProjectileStreamUpdate ModuleTag_02\n"
	"  End\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 1.0\n"
	"  GeometryMinorRadius = 1.0\n"
	"  GeometryHeight = 1.0\n"
	"End\n"
	"Object Port27NoUpdate\n"
	"  KindOf = INERT NO_COLLIDE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 1.0\n"
	"  GeometryMinorRadius = 1.0\n"
	"  GeometryHeight = 1.0\n"
	"End\n";

// FireDrakeFlame's shape in the mod's weapon.ini (a 3000 ms delay, a 1-shot clip that reloads by itself, a ProjectileNugget): the two fields the
// stream path reads are DelayBetweenShots and ProjectileStreamName, so the shots can be counted by hand.
const char kPort27Weapons[] =
	"Weapon Port27Flame\n"
	"  AttackRange = 200\n"
	"  WeaponSpeed = 100\n"
	"  DelayBetweenShots = 3000\n"
	"  HitPercentage = 100%\n"
	"  ClipSize = 1\n"
	"  AutoReloadsClip = Yes\n"
	"  ClipReloadTime = 2000\n"
	"  ProjectileStreamName = Port27Stream\n"
	"  ProjectileNugget\n"
	"    ProjectileTemplateName = Arrow\n"
	"    WarheadTemplateName = Port27Warhead\n"
	"  End\n"
	"End\n"
	"Weapon Port27FlameNoStream\n"
	"  AttackRange = 200\n"
	"  WeaponSpeed = 100\n"
	"  DelayBetweenShots = 3000\n"
	"  HitPercentage = 100%\n"
	"  ClipSize = 1\n"
	"  AutoReloadsClip = Yes\n"
	"  ClipReloadTime = 2000\n"
	"  ProjectileNugget\n"
	"    ProjectileTemplateName = Arrow\n"
	"    WarheadTemplateName = Port27Warhead\n"
	"  End\n"
	"End\n"
	// the same weapon pointed at a name that is not an object template: RW 0x6CD252 answers null and the weapon stays without a stream
	"Weapon Port27FlameBadName\n"
	"  AttackRange = 200\n"
	"  WeaponSpeed = 100\n"
	"  DelayBetweenShots = 3000\n"
	"  HitPercentage = 100%\n"
	"  ClipSize = 1\n"
	"  AutoReloadsClip = Yes\n"
	"  ClipReloadTime = 2000\n"
	"  ProjectileStreamName = Port27NoSuchObject\n"
	"  ProjectileNugget\n"
	"    ProjectileTemplateName = Arrow\n"
	"    WarheadTemplateName = Port27Warhead\n"
	"  End\n"
	"End\n"
	"Weapon Port27Warhead\n"
	"  ProjectileCollidesWith = ALLIES ENEMIES NEUTRAL\n"
	"  DamageNugget\n"
	"    Damage = 1\n"
	"    Radius = 0\n"
	"    DamageType = PIERCE\n"
	"    DeathType = NORMAL\n"
	"  End\n"
	"End\n";

// Three shooters (the stream template above is on none of them unless its weapon names it), so a stream object made for one of them cannot be
// mistaken for another's.
const char kPort27Shooters[] =
	"Object Port27FlameShooter\n"
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY Port27Flame\n"
	"  End\n"
	"  WeaponSet\n"
	"    Conditions = PLAYER_UPGRADE\n"
	"    Weapon = PRIMARY Port27FlameNoStream\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = DestroyDie ModuleTag_Destroy\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"End\n"
	"Object Port27PlainShooter\n"
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY Port27FlameNoStream\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = DestroyDie ModuleTag_Destroy\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"End\n"
	"Object Port27BadNameShooter\n"
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY Port27FlameBadName\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = DestroyDie ModuleTag_Destroy\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"End\n";

// CombatWorld with the stream objects and the stream weapons added (a derived fixture so the world is built once per case).
struct Port27World : CombatWorld
{
	// the weapons come first: a WeaponSet stores the template it resolved while the object template was parsed (an unknown name is a null slot then),
	// which is why the shooters are loaded after the weapons and not through CombatWorld's extraObjects (those parse before this constructor's body)
	Port27World()
		: CombatWorld()
	{
		std::string err = w.load(kPort27Weapons, INI_LOAD_OVERWRITE, "port27.ini");
		REQUIRE_MESSAGE(err.empty(), err);
		err = w.load((std::string(kPort27Objects) + kPort27Shooters).c_str(), INI_LOAD_OVERWRITE, "port27_objects.ini");
		REQUIRE_MESSAGE(err.empty(), err);
		combat().setAutoAcquireEnabled(false); // the shooters fire because a test says so
	}
	// every object of `name` in the world (the port's object list order)
	std::vector<Object *> objectsNamed(const std::string &name)
	{
		std::vector<Object *> out;
		for (Object *o = logic->getFirstObject(); o; o = o->getNextObject())
		{
			if (!o->isDestroyed() && o->getTemplate()->getName() == name)
			{
				out.push_back(o);
			}
		}
		return out;
	}
	size_t liveObjects() const
	{
		size_t n = 0;
		for (Object *o = logic->getFirstObject(); o; o = o->getNextObject())
		{
			if (!o->isDestroyed())
			{
				++n;
			}
		}
		return n;
	}
	// the frames a 1-shot clip needs before it is ready again (ClipReloadTime 2000 ms = 10 frames; a few more for the frame in progress)
	void waitForReload(Object *from)
	{
		ObjectWeapons *ow = from->getWeapons();
		REQUIRE(ow);
		for (int i = 0; i < 40 && (!ow->currentWeapon() || ow->currentStatus() != WEAPON_READY_TO_FIRE); ++i)
		{
			frames(1);
		}
		REQUIRE_MESSAGE(ow->currentStatus() == WEAPON_READY_TO_FIRE, "the clip reloaded; status " << ow->currentStatus());
	}
	// one shot through the whole fire path of Object::fireCurrentWeapon (RW 0x69200A -> privateFireWeapon RW 0x6CEF6D)
	void shoot(Object *from, Object *victim)
	{
		ObjectWeapons *ow = from->getWeapons();
		REQUIRE(ow);
		REQUIRE_MESSAGE(ow->currentWeapon() != nullptr, "a current weapon");
		waitForReload(from); // a 1-shot clip that reloads by itself is WEAPON_RELOADING_CLIP until ClipReloadTime has passed
		ow->fireCurrentWeapon(victim, nullptr);
	}
};

} // namespace

// The creation itself (RW 0x6CD1FD): the first shot makes ONE object of the named template, keeps its id in the weapon (Weapon + 0x5C,
// RW 0x6CD291) and every later shot reuses it (RW 0x6CD23D / 0x6CD244). Without the port's step 6 no object is made at all.
TEST_CASE("PORT-27 stream: the first shot makes one object of ProjectileStreamName, later shots reuse it (RW 0x6CD1FD)")
{
	Port27World world;
	Object *shooter = world.unit("Port27FlameShooter", 'A', 300, 300);
	Object *victim = world.unit("Dummy", 'B', 380, 300);
	world.frames(2);
	Weapon *weapon = shooter->getWeapons()->currentWeapon();
	REQUIRE(weapon);
	REQUIRE(weapon->getTemplate()->getName() == "Port27Flame");
	REQUIRE(world.objectsNamed("Port27Stream").empty());
	REQUIRE(weapon->projectileStreamID() == 0); // RW ctor 0x6CCF5D stores 0 at + 0x5C

	world.shoot(shooter, victim);
	const std::vector<Object *> first = world.objectsNamed("Port27Stream");
	REQUIRE_MESSAGE(first.size() == 1, "one stream object after the first shot");
	CHECK(weapon->projectileStreamID() == first[0]->getID()); // RW 0x6CD291
	// RW 0x6CD285 -> RW 0x6D165E: the object is the owner's player's default team's
	CHECK(first[0]->getTeam() == world.teamA());
	// RW 0x90FBEE -> RW 0x74094C: the same shot then puts the stream object on the shooter, so it does not stay where it was made
	CHECK(first[0]->getPosition()->x == shooter->getPosition()->x);
	CHECK(first[0]->getPosition()->y == shooter->getPosition()->y);

	// the second shot: the stream is still found (RW 0x6CD244), so no second object and the same id
	world.waitForReload(shooter);
	world.shoot(shooter, victim);
	const std::vector<Object *> second = world.objectsNamed("Port27Stream");
	REQUIRE_MESSAGE(second.size() == 1, "a later shot reuses the stream object");
	CHECK(second[0]->getID() == first[0]->getID());
	CHECK(weapon->projectileStreamID() == first[0]->getID());
}

// A weapon without the field makes nothing. RW 0x6CD212 (`call RW 0x401E64`, `jne 0x6CD295`) leaves the function before any lookup, so this is the
// control that keeps the test above from passing on "some object was made" rather than on the field.
TEST_CASE("PORT-27 stream: a weapon without ProjectileStreamName makes no stream object (RW 0x6CD212)")
{
	Port27World world;
	Object *shooter = world.unit("Port27PlainShooter", 'A', 300, 300);
	Object *victim = world.unit("Dummy", 'B', 380, 300);
	world.frames(2);
	Weapon *weapon = shooter->getWeapons()->currentWeapon();
	REQUIRE(weapon);
	REQUIRE(weapon->getTemplate()->getName() == "Port27FlameNoStream");
	world.shoot(shooter, victim);
	CHECK(world.objectsNamed("Port27Stream").empty());
	CHECK(weapon->projectileStreamID() == 0);
	// the shot itself happened: the projectile of the nugget is in the world (so the empty result above is the field's doing, not a shot that
	// never fired)
	CHECK(world.objectsNamed("Arrow").size() == 1);
}

// RW 0x6CD252 -> RW 0x6D1305: a name that is not an object template answers null, no object is made and + 0x5C stays 0. No silent template.
TEST_CASE("PORT-27 stream: a ProjectileStreamName that is not an object makes nothing (RW 0x6CD252)")
{
	Port27World world;
	Object *shooter = world.unit("Port27BadNameShooter", 'A', 300, 300);
	Object *victim = world.unit("Dummy", 'B', 380, 300);
	world.frames(2);
	Weapon *weapon = shooter->getWeapons()->currentWeapon();
	REQUIRE(weapon);
	CHECK(weapon->getTemplate()->m_projectileStreamName == "Port27NoSuchObject");
	const size_t before = world.liveObjects();
	world.shoot(shooter, victim);
	CHECK(world.objectsNamed("Port27Stream").empty());
	CHECK(weapon->projectileStreamID() == 0);
	// the shot still fired (its projectile is there): only the stream was not made
	CHECK(world.objectsNamed("Arrow").size() == 1);
	CHECK(world.liveObjects() == before + 1); // + the one projectile
}

// What each shot does with the stream (RW 0x7408A8 from RW 0x90FC5A): the stream object's ProjectileStreamUpdate records the projectile and the
// victim, and the stream object is put on the OWNER's geometry centre (RW 0x74094C from RW 0x90FBEE). Without the record the stream draws nothing.
TEST_CASE("PORT-27 stream: each shot records the projectile and the victim, and puts the stream object on the shooter (RW 0x90FB7A)")
{
	Port27World world;
	Object *shooter = world.unit("Port27FlameShooter", 'A', 300, 300);
	Object *victim = world.unit("Dummy", 'B', 380, 300);
	world.frames(2);
	world.shoot(shooter, victim);
	const std::vector<Object *> streams = world.objectsNamed("Port27Stream");
	REQUIRE(streams.size() == 1);
	auto *update = dynamic_cast<ProjectileStreamUpdate *>(streams[0]->findModule("ProjectileStreamUpdate"));
	REQUIRE(update);
	CHECK(update->owningObject() == shooter->getID());     // RW 0x90FC57 pushes the owner's id
	CHECK(update->targetObject() == victim->getID());      // RW 0x90FC55 the victim's id
	CHECK(update->targetPosition().x == 0.0f);             // the victim-id branch clears the position (addProjectile's object case)
	// RW 0x7408A8 / addProjectile (RW 0x890B90): a NEW target first writes a 0 into the ring (the hole that breaks the stream, RW 0x890BF7 .. RW 0x890C07)
	// and only then the projectile, so one shot at a fresh target leaves the hole at 0 and the projectile at 1
	CHECK(update->nextFreeIndex() == 2);
	CHECK(update->projectileID(0) == INVALID_ID);           // the hole
	const ObjectID projectile = update->projectileID(1);
	REQUIRE(projectile != INVALID_ID);
	Object *arrow = world.byId(projectile);
	REQUIRE(arrow);
	CHECK(arrow->getTemplate()->getName() == "Arrow");
	// RW 0x90FBEE / RW 0x74094C: the stream object sits on the owner's geometry centre (Object + 0x38), i.e. the shooter's position
	CHECK(streams[0]->getPosition()->x == shooter->getPosition()->x);
	CHECK(streams[0]->getPosition()->y == shooter->getPosition()->y);
	CHECK(streams[0]->getPosition()->z == shooter->getPosition()->z);
}

// The neighbouring behaviour that must not move: two shooters have two streams (the id lives in each Weapon, not in the template or the object), and
// a shot with no victim (a position) is not recorded at all — RW 0x90FC70, the position-target deliver, has no stream block.
TEST_CASE("PORT-27 stream: one stream per weapon, and a position shot is not recorded (RW 0x90FC70 has no stream block)")
{
	Port27World world;
	Object *first = world.unit("Port27FlameShooter", 'A', 300, 300);
	Object *second = world.unit("Port27FlameShooter", 'A', 300, 500);
	Object *victim = world.unit("Dummy", 'B', 380, 300);
	world.frames(2);
	world.shoot(first, victim);
	world.shoot(second, victim);
	const std::vector<Object *> streams = world.objectsNamed("Port27Stream");
	REQUIRE(streams.size() == 2);
	CHECK(first->getWeapons()->currentWeapon()->projectileStreamID() != second->getWeapons()->currentWeapon()->projectileStreamID());

	// the position shot of the first shooter: the projectile flies, the stream records nothing new
	Object *streamOfFirst = world.byId(first->getWeapons()->currentWeapon()->projectileStreamID());
	REQUIRE(streamOfFirst);
	auto *update = dynamic_cast<ProjectileStreamUpdate *>(streamOfFirst->findModule("ProjectileStreamUpdate"));
	REQUIRE(update);
	REQUIRE(update->owningObject() == first->getID());
	const int recorded = update->nextFreeIndex();
	world.waitForReload(first);
	const size_t arrowsBefore = world.objectsNamed("Arrow").size();
	Coord3D pos{ 480.0f, 300.0f, 0.0f };
	first->getWeapons()->fireCurrentWeapon(nullptr, &pos);
	CHECK(world.objectsNamed("Arrow").size() == arrowsBefore + 1); // the position shot fired its projectile
	CHECK(update->nextFreeIndex() == recorded);                    // and the stream recorded nothing of it
}

// The stream lives as long as the weapon that made it (RW 0x6CD0BE, the Weapon destructor: RW 0x6CD0F4 "destroyObject" of + 0x5C). The port frees
// the weapon when the object's weapon set is rebuilt (rebuildWeapons resets every Weapon), so an upgrade that swaps the set is the cleanest reachable
// witness of that branch.
TEST_CASE("PORT-27 stream: the stream object goes with the weapon that made it (RW 0x6CD0BE)")
{
	Port27World world;
	Object *shooter = world.unit("Port27FlameShooter", 'A', 300, 300);
	Object *victim = world.unit("Dummy", 'B', 380, 300);
	world.frames(2);
	world.shoot(shooter, victim);
	REQUIRE(world.objectsNamed("Port27Stream").size() == 1);
	const ObjectID streamID = shooter->getWeapons()->currentWeapon()->projectileStreamID();
	REQUIRE(streamID != INVALID_ID);
	// the flame weapon is in the "None" set only: any weapon set flag with no set of its own rebuilds the set (RW 0x6C99E2) and frees the weapons
	shooter->getWeapons()->setWeaponSetFlag(CombatNames::weaponSetBit("PLAYER_UPGRADE"), true);
	world.frames(1); // the destroy list runs in phase 5 (processDestroyList, RW 0x62EBB1)
	CHECK(world.byId(streamID) == nullptr);
	CHECK(world.objectsNamed("Port27Stream").empty());
}
