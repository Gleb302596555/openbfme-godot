// OpenBFME unit tests. PORT-136: synthetic, no game installation. GPL-3.0.
// Probe file (temporary diagnostics): the summoned giant bird of the spell book does not attack on its own.
#include "doctest.h"

#include <iomanip>

#include "CombatTestUtil.h"
#include "IniTestUtil.h"
#include "GameLogic/AI/AIClosestEnemy.h"
#include "GameLogic/AI/AIWorld.h"
#include "GameLogic/Object/PartitionManager.h"
#include "GameLogic/Module/GiantBirdAIUpdate.h"
#include "GameLogic/Module/StancesBehavior.h"
#include "GameLogic/Object/Object.h"

namespace
{
// the real mod shape of GwaihirEagleSpell_Summoned (Ennorath 1.9.8.2): its KindOf list, VisionRange 450, AttackRange 20, the two DamageNuggets (FORCE,
// HERO), a MetaImpactNugget, the TransportContain grab and LifetimeUpdate; `extras` adds the two modules beside GiantBirdAIUpdate.
std::string eagleIni(bool extras)
{
	std::string s = R"ini(
Locomotor Port136BirdLoco
 Surfaces = AIR
 Appearance = GIANT_BIRD
 TurnTime = 1000
 Acceleration = 1000
 Braking = 1000
 PreferredHeight = 100
 ZAxisBehavior = SURFACE_RELATIVE_HEIGHT
End
Weapon Port136EagleAttack
 AttackRange = 20.0
 DelayBetweenShots = 4000
 AntiAirborneVehicle = Yes
 AntiAirborneMonster = Yes
 AntiAirborneInfantry = Yes
 RadiusDamageAffects = ENEMIES ALLIES SELF
 DamageNugget
  Damage = 50
  DamageType = FORCE
  DeathType = NORMAL
  Radius = 0
 End
End
Object Port136Eagle
 KindOf = PRELOAD CAN_ATTACK CAN_CAST_REFLECTIONS PASS_EXPERIENCE_TO_PRODUCER SCORE NO_FREEWILL_ENTER MONSTER SCARY CANNOT_RETALIATE SUMMONED
 VisionRange = 450
 ShroudClearingRange = 650
 Geometry = CYLINDER
 GeometryMajorRadius = 10
 GeometryHeight = 30
 WeaponSet
  Conditions = None
  Weapon = PRIMARY Port136EagleAttack
 End
 Body = ActiveBody ModuleTag_Body
  MaxHealth = 5000
 End
 Behavior = GiantBirdAIUpdate ModuleTag_AI
  AutoAcquireEnemiesWhenIdle = Yes ATTACK_BUILDINGS
 End
)ini";
	if (extras)
	{
		s += R"ini( Behavior = StancesBehavior ModuleTag_Stances
  StanceTemplate = Port136Hero
 End
)ini";
		s += R"ini( Behavior = TransportContain ModuleTag_Contain
  Slots = 1
  PassengerBonePrefix = PassengerBone:B_Passenger KindOf:GRAB_AND_DROP
  DamagePercentToUnits = 0%
 End
 Behavior = LifetimeUpdate ModuleTag_Life
  MinLifetime = 120000
  MaxLifetime = 120000
 End
)ini";
	}
	s += R"ini( LocomotorSet
  Locomotor = Port136BirdLoco
  Condition = SET_NORMAL
  Speed = 120
 End
End
StanceTemplate Port136Hero
    Stance Aggressive
        MeleeBehavior = Amoeba
        End
    End
    Stance Battle
        MeleeBehavior = Swarm
        End
    End
    Stance HoldGround
        MeleeBehavior = HoldGround
        End
    End
End
Armor Port136EagleArmor
 Armor = DEFAULT 100%
 Armor = HERO 100%
 Armor = FORCE 100%
End
)ini";
	return s;
}

struct Outcome
{
	unsigned state = 0;
	float z = 0.0f;
	float health = 0.0f;
	bool withinRange = false;
	bool canAttackFromHere = false;
	std::string target;
};

Outcome probe(const char *label, bool extras, float horizontal, float birdZ, int frames)
{
	combattest::CombatWorld w;
	const std::string error = w.w.load(eagleIni(extras), INI_LOAD_OVERWRITE, "port136-probe.ini");
	REQUIRE_MESSAGE(error.empty(), error);
	Object *bird = w.unit("Port136Eagle", 'A', 200.0f, 200.0f);
	Object *enemy = w.unit("Swordsman", 'B', 200.0f + horizontal, 200.0f);
	REQUIRE(bird);
	REQUIRE(enemy);
	auto *ai = dynamic_cast<GiantBirdAIUpdate *>(bird->getAIUpdateInterface());
	REQUIRE(ai);
	if (birdZ != 0.0f)
	{
		Coord3D p = *bird->getPosition();
		p.z = birdZ;
		bird->setPosition(&p);
	}
	Outcome out;
	out.withinRange = bird->getWeapons()->isSlotWithinAttackRange(0, *enemy, 0.0f, true);
	out.canAttackFromHere = AI::canAttackFromHere(*bird, *enemy);
	Object *mood = ai->getNextMoodTarget(false, true);
	out.target = mood ? mood->getTemplate()->getName() : std::string("none");
	MESSAGE(label << ": playerTypeA=" << (int)w.playerOf('A')->getPlayerType() << " extras=" << extras << " horiz=" << horizontal << " z=" << birdZ << " withinRange=" << out.withinRange << " canAttackFromHere="
		      << out.canAttackFromHere << " mood(no cadence)=" << out.target);
	w.frames(frames);
	out.state = ai->currentStateId();
	out.z = bird->getPosition()->z;
	out.health = w.health(enemy);
	MESSAGE(label << ": after " << frames << " frames state=" << out.state << " z=" << out.z << " enemy health=" << out.health);
	return out;
}
}

TEST_CASE("PORT-136 probe D: the mod eagle shape, enemy at 20 horizontally")
{
	probe("D1", true, 20.0f, 100.0f, 60);
	probe("D2", false, 20.0f, 100.0f, 60);
	probe("D3", true, 20.0f, 0.0f, 60);
	CHECK(false);
}

TEST_CASE("PORT-136 probe F: the eagle with a StancesBehavior takes Battle and sits in AI_GUARD (RW 0x8622E9 -> 0x86223F -> 0x662A21 -> AIGuardState 16)")
{
	combattest::CombatWorld w;
	StanceTemplateStore store;
	store.registerBlock(w.w.fx.env.blocks);
	StanceTemplateStore *const saved = TheStanceTemplateStore;
	TheStanceTemplateStore = &store;
	struct Restore
	{
		StanceTemplateStore *v;
		~Restore() { TheStanceTemplateStore = v; }
	} restore{saved};
	const std::string error = w.w.load(eagleIni(true), INI_LOAD_OVERWRITE, "port136-stance.ini");
	REQUIRE_MESSAGE(error.empty(), error);
	REQUIRE(store.find("Port136Hero"));
	const float distances[] = { 400.0f, 300.0f, 200.0f, 100.0f };
	for (float d : distances)
	{
		Object *bird = w.unit("Port136Eagle", 'A', 200.0f, 200.0f);
		Object *enemy = w.unit("Swordsman", 'B', 200.0f + d, 200.0f);
		REQUIRE(bird);
		REQUIRE(enemy);
		auto *ai = dynamic_cast<GiantBirdAIUpdate *>(bird->getAIUpdateInterface());
		REQUIRE(ai);
		StancesBehavior *sb = StancesBehavior::of(*bird);
		REQUIRE(sb);
		std::string trace;
		unsigned states[10] = { 0 };
		float hp[10] = { 0 };
		for (int tick = 0; tick < 10; ++tick)
		{
			w.frames(30);
			states[tick] = ai->currentStateId();
			hp[tick] = w.health(enemy);
			trace += std::to_string(states[tick]) + "/" + std::to_string(w.health(bird)) + " ";
		}
		{
			const float vr = AI::adjustedVisionRange(*bird, 3u);
			Object *closest = AI::findClosestEnemy(*bird, vr, AI::CAN_ATTACK | AI::AUTO_ACQUIRE, nullptr, nullptr, 0);
			MESSAGE("F d=" << d << " adjustedVisionRange=" << vr << " findClosestEnemy=" << (closest ? closest->getTemplate()->getName() : std::string("none")) << " autoAcquireFlags=" << ai->autoAcquireFlags());
		}
		MESSAGE("F d=" << d << ": stance=" << (int)sb->getStance() << " state/hp(300 frames, every 30)=" << trace << " enemy hp=" << w.health(enemy) << " bird z=" << bird->getPosition()->z << " bird hp=" << w.health(bird));
	}
	CHECK(false);
}

TEST_CASE("PORT-136 probe E: the mod eagle shape, enemy at 100/400 horizontally")
{
	probe("E1", true, 100.0f, 100.0f, 60);
	probe("E2", true, 400.0f, 100.0f, 120);
	CHECK(false);
}

namespace
{
// the same weapon and vision as the eagle, on a foot unit: the reference for "is the search's reach object-specific?"
const char *kFootIni = R"ini(
Object Port136Foot
 KindOf = INFANTRY SELECTABLE CAN_ATTACK SCORE
 VisionRange = 450
 Geometry = CYLINDER
 GeometryMajorRadius = 8
 GeometryHeight = 20
 ArmorSet
  Conditions = None
  Armor = PlainArmor
 End
 WeaponSet
  Conditions = None
  Weapon = PRIMARY Port136EagleAttack
 End
 Body = ActiveBody ModuleTag_Body
  MaxHealth = 1000
 End
 Behavior = AIUpdateInterface ModuleTag_AI
  AutoAcquireEnemiesWhenIdle = Yes
  MoodAttackCheckRate = 400
 End
 Behavior = PhysicsBehavior ModuleTag_Physics
 End
 LocomotorSet
  Locomotor = WalkerLoco
  Condition = SET_NORMAL
  Speed = 55
 End
End
)ini";

std::string searchOf(Object &me, float range, unsigned q)
{
	Object *found = AI::findClosestEnemy(me, range, q, nullptr, nullptr, 0);
	return found ? found->getTemplate()->getName() : std::string("none");
}
} // namespace

TEST_CASE("PORT-136 probe G: the reach of AI::findClosestEnemy for the eagle and the same weapon+vision on foot")
{
	combattest::CombatWorld w;
	StanceTemplateStore store;
	store.registerBlock(w.w.fx.env.blocks);
	StanceTemplateStore *const saved = TheStanceTemplateStore;
	TheStanceTemplateStore = &store;
	struct Restore
	{
		StanceTemplateStore *v;
		~Restore() { TheStanceTemplateStore = v; }
	} restore{saved};
	const std::string error = w.w.load(eagleIni(true) + kFootIni, INI_LOAD_OVERWRITE, "port136-search.ini");
	REQUIRE_MESSAGE(error.empty(), error);
	Object *bird = w.unit("Port136Eagle", 'A', 200.0f, 200.0f);
	Object *enemy = w.unit("Swordsman", 'B', 300.0f, 200.0f);
	Object *foot = w.unit("Port136Foot", 'A', 200.0f, 400.0f);
	Object *enemy2 = w.unit("Swordsman", 'B', 300.0f, 400.0f);
	REQUIRE(bird);
	REQUIRE(enemy);
	REQUIRE(foot);
	REQUIRE(enemy2);
	MESSAGE("G: vision bird=" << AI::adjustedVisionRange(*bird, 3u) << " foot=" << AI::adjustedVisionRange(*foot, 3u) << " ableToAttack bird=" << bird->isAbleToAttack()
				 << " foot=" << foot->isAbleToAttack() << " canAttackObject bird=" << bird->getWeapons()->canAttackObject(*enemy, CMD_FROM_AI, false)
				 << " foot=" << foot->getWeapons()->canAttackObject(*enemy2, CMD_FROM_AI, false));
	const unsigned quals[] = { 0u, AI::CAN_ATTACK, AI::AUTO_ACQUIRE, AI::CAN_ATTACK | AI::AUTO_ACQUIRE, AI::CAN_ATTACK | AI::AUTO_ACQUIRE | AI::NO_VISION_TEST,
		AI::CAN_ATTACK | AI::NO_VISION_TEST };
	for (unsigned q : quals)
	{
		MESSAGE("G q=0x" << std::hex << q << std::dec << " bird(100)=" << searchOf(*bird, 460.0f, q) << " foot(100)=" << searchOf(*foot, 460.0f, q) << " bird(460)=" << searchOf(*bird, 1000.0f, q)
				 << " foot(1000)=" << searchOf(*foot, 1000.0f, q));
	}
	CHECK(false);
}

TEST_CASE("PORT-136 probe H: is the candidate in the partition at all, and does its closest query see it?")
{
	combattest::CombatWorld w;
	StanceTemplateStore store;
	store.registerBlock(w.w.fx.env.blocks);
	StanceTemplateStore *const saved = TheStanceTemplateStore;
	TheStanceTemplateStore = &store;
	struct Restore
	{
		StanceTemplateStore *v;
		~Restore() { TheStanceTemplateStore = v; }
	} restore{saved};
	const std::string error = w.w.load(eagleIni(true) + kFootIni, INI_LOAD_OVERWRITE, "port136-part.ini");
	REQUIRE_MESSAGE(error.empty(), error);
	Object *bird = w.unit("Port136Eagle", 'A', 200.0f, 200.0f);
	Object *enemy = w.unit("Swordsman", 'B', 300.0f, 200.0f);
	REQUIRE(bird);
	REQUIRE(enemy);
	auto *ai = dynamic_cast<GiantBirdAIUpdate *>(bird->getAIUpdateInterface());
	REQUIRE(ai);
	PartitionManager &part = w.logic->partition();
	int layerB = -99, layerE = -99;
	std::uint32_t x = 0, y = 0, mask = 0;
	const bool regB = part.entryCells(*bird, layerB, x, y, mask);
	MESSAGE("H: bird registered=" << regB << " layer=" << layerB << " cell=" << x << "," << y);
	const bool regE = part.entryCells(*enemy, layerE, x, y, mask);
	MESSAGE("H: enemy registered=" << regE << " layer=" << layerE << " cell=" << x << "," << y << " entries=" << part.entryCount());
	MESSAGE("H: layerOf bird=" << part.layerOf(*bird) << " enemy=" << part.layerOf(*enemy) << " dist=" << (enemy->getPosition()->x - bird->getPosition()->x));
	Object *raw = part.getClosestObject(*bird->getPosition(), 1000.0f, FROM_CENTER_3D, PartitionFilterList());
	MESSAGE("H: raw closest (no filters)=" << (raw ? raw->getTemplate()->getName() : std::string("none")));
	const PartitionHits hits = part.iterateObjectsInRange(*bird->getPosition(), 1000.0f, FROM_CENTER_3D, PartitionFilterList(), ITER_FASTEST);
	MESSAGE("H: iterateObjectsInRange(1000) count=" << hits.size());
	MESSAGE("H: mood=" << (ai->getNextMoodTarget(false, true) ? ai->getNextMoodTarget(false, true)->getTemplate()->getName() : std::string("none")) << " canAttackFromHere=" << AI::canAttackFromHere(*bird, *enemy)
			   << " q0=" << searchOf(*bird, 1000.0f, 0u) << " qCAN_ATTACK=" << searchOf(*bird, 1000.0f, AI::CAN_ATTACK));
	CHECK(false);
}

TEST_CASE("PORT-136 probe I: does the partition's closest query continue past the searcher itself?")
{
	combattest::CombatWorld w;
	StanceTemplateStore store;
	store.registerBlock(w.w.fx.env.blocks);
	StanceTemplateStore *const saved = TheStanceTemplateStore;
	TheStanceTemplateStore = &store;
	struct Restore
	{
		StanceTemplateStore *v;
		~Restore() { TheStanceTemplateStore = v; }
	} restore{saved};
	const std::string error = w.w.load(eagleIni(true) + kFootIni, INI_LOAD_OVERWRITE, "port136-cont.ini");
	REQUIRE_MESSAGE(error.empty(), error);
	Object *bird = w.unit("Port136Eagle", 'A', 200.0f, 200.0f);
	Object *near = w.unit("Swordsman", 'B', 220.0f, 200.0f);
	Object *far = w.unit("Swordsman", 'B', 300.0f, 200.0f);
	REQUIRE(bird);
	REQUIRE(near);
	REQUIRE(far);
	auto *ai = dynamic_cast<GiantBirdAIUpdate *>(bird->getAIUpdateInterface());
	REQUIRE(ai);
	MESSAGE("I: bird position " << bird->getPosition()->x << "," << bird->getPosition()->y << " near " << near->getPosition()->x << " far " << far->getPosition()->x);
	const unsigned qs[] = { 0u, (unsigned)AI::CAN_ATTACK, (unsigned)(AI::CAN_ATTACK | AI::AUTO_ACQUIRE), (unsigned)(AI::CAN_ATTACK | AI::AUTO_ACQUIRE | AI::NO_VISION_TEST),
		(unsigned)(AI::CAN_ATTACK | AI::AUTO_ACQUIRE | AI::ATTACK_BUILDINGS) };
	for (unsigned q : qs)
	{
		Coord3D at = *bird->getPosition();
		Object *fromPos = AI::findClosestEnemy(*bird, at, 1000.0f, q, nullptr, nullptr, 0);
		Coord3D nearEnemy{ near->getPosition()->x - 5.0f, near->getPosition()->y, 0.0f };
		Object *fromNear = AI::findClosestEnemy(*bird, nearEnemy, 1000.0f, q, nullptr, nullptr, 0);
		MESSAGE("I q=0x" << q << " fromBirdPos=" << (fromPos ? fromPos->getTemplate()->getName() + "@" + std::to_string((int)fromPos->getPosition()->x) : std::string("none"))
				 << " fromNearPos=" << (fromNear ? fromNear->getTemplate()->getName() + "@" + std::to_string((int)fromNear->getPosition()->x) : std::string("none")) << " mood=" << (ai->getNextMoodTarget(false, true) ? "yes" : "none")
				 << " flags=" << ai->autoAcquireFlags());
	}
	CHECK(false);
}

namespace
{
struct AllowAllButSelf : PartitionFilter
{
	explicit AllowAllButSelf(Object &self) : m_self(self) {}
	bool allow(Object &obj) override { return &obj != &m_self; }
	Object &m_self;
};

struct AllowOnly : PartitionFilter
{
	explicit AllowOnly(Object &self) : m_self(self) {}
	bool allow(Object &obj) override { return &obj == &m_self; }
	Object &m_self;
};
} // namespace

TEST_CASE("PORT-136 probe J: the enemies filter's layer mask and the closest walk with a plain filter")
{
	combattest::CombatWorld w;
	StanceTemplateStore store;
	store.registerBlock(w.w.fx.env.blocks);
	StanceTemplateStore *const saved = TheStanceTemplateStore;
	TheStanceTemplateStore = &store;
	struct Restore
	{
		StanceTemplateStore *v;
		~Restore() { TheStanceTemplateStore = v; }
	} restore{saved};
	const std::string error = w.w.load(eagleIni(true) + kFootIni, INI_LOAD_OVERWRITE, "port136-mask.ini");
	REQUIRE_MESSAGE(error.empty(), error);
	Object *bird = w.unit("Port136Eagle", 'A', 200.0f, 200.0f);
	Object *enemy = w.unit("Swordsman", 'B', 300.0f, 200.0f);
	REQUIRE(bird);
	REQUIRE(enemy);
	const Player *pa = bird->getControllingPlayer();
	const Player *pb = enemy->getControllingPlayer();
	REQUIRE(pa);
	REQUIRE(pb);
	MESSAGE("J: players bird=" << pa->getPlayerIndex() << " enemy=" << pb->getPlayerIndex() << " rel(a->b team)=" << (int)pa->getRelationship(pb->getDefaultTeam())
			   << " rel(b->a team)=" << (int)pb->getRelationship(pa->getDefaultTeam()));
	PartitionManager &part = w.logic->partition();
	AllowAllButSelf plain(*bird);
	PartitionFilter *const oneList[] = { &plain };
	PartitionFilterList one(oneList, 1);
	Object *found = part.getClosestObject(*bird->getPosition(), 1000.0f, FROM_CENTER_3D, one);
	MESSAGE("J: closest with a pass-all-but-self filter=" << (found ? found->getTemplate()->getName() : std::string("none")));
	Object *raw = part.getClosestObject(*bird->getPosition(), 1000.0f, FROM_CENTER_3D, PartitionFilterList());
	MESSAGE("J: closest with no filters=" << (raw ? raw->getTemplate()->getName() : std::string("none")));
	auto *ai = dynamic_cast<GiantBirdAIUpdate *>(bird->getAIUpdateInterface());
	REQUIRE(ai);
	Object *mood = ai->getNextMoodTarget(false, true);
	MESSAGE("J: mood=" << (mood ? mood->getTemplate()->getName() : std::string("none")) << " findClosest(q0)=" << searchOf(*bird, 1000.0f, 0u));
	const PartitionHits walkAll = part.iterateObjectsInRange(*bird->getPosition(), 1000.0f, FROM_CENTER_3D, one, ITER_FASTEST);
	MESSAGE("J: iterate allow-all-but-self count=" << walkAll.size());
	AllowOnly onlyEnemy(*enemy);
	PartitionFilter *const onlyList[] = { &onlyEnemy };
	PartitionFilterList onlyOne(onlyList, 1);
	Object *foundOnly = part.getClosestObject(*bird->getPosition(), 1000.0f, FROM_CENTER_3D, onlyOne);
	MESSAGE("J: closest with allow-only-enemy=" << (foundOnly ? foundOnly->getTemplate()->getName() : std::string("none")));
	const PartitionHits walkOnly = part.iterateObjectsInRange(*bird->getPosition(), 1000.0f, FROM_CENTER_3D, onlyOne, ITER_FASTEST);
	MESSAGE("J: iterate allow-only-enemy count=" << walkOnly.size());
	Object *found2 = part.getClosestObject(*bird->getPosition(), 5000.0f, FROM_CENTER_3D, one);
	MESSAGE("J: closest allow-all-but-self with a 5000 range=" << (found2 ? found2->getTemplate()->getName() : std::string("none")));
	CHECK(false);
}

TEST_CASE("PORT-136 probe J2: the fixture's partition region was never set; set it and repeat the filtered search")
{
	combattest::CombatWorld w;
	StanceTemplateStore store;
	store.registerBlock(w.w.fx.env.blocks);
	StanceTemplateStore *const saved = TheStanceTemplateStore;
	TheStanceTemplateStore = &store;
	struct Restore
	{
		StanceTemplateStore *v;
		~Restore() { TheStanceTemplateStore = v; }
	} restore{saved};
	const std::string error = w.w.load(eagleIni(true), INI_LOAD_OVERWRITE, "port136-region.ini");
	REQUIRE_MESSAGE(error.empty(), error);
	Object *bird = w.unit("Port136Eagle", 'A', 200.0f, 200.0f);
	Object *enemy = w.unit("Swordsman", 'B', 600.0f, 200.0f);
	REQUIRE(bird);
	REQUIRE(enemy);
	PartitionManager &part = w.logic->partition();
	auto *ai = dynamic_cast<GiantBirdAIUpdate *>(bird->getAIUpdateInterface());
	REQUIRE(ai);
	MESSAGE("J2 before setRegion: findClosest(q0)=" << searchOf(*bird, 1000.0f, 0u));
	part.setRegion(0.0f, 0.0f, 1000.0f, 1000.0f);
	MESSAGE("J2 after setRegion: findClosest(q0)=" << searchOf(*bird, 1000.0f, 0u) << " findClosest(canAttack)=" << searchOf(*bird, 1000.0f, AI::CAN_ATTACK)
			   << " mood=" << (ai->getNextMoodTarget(false, true) ? ai->getNextMoodTarget(false, true)->getTemplate()->getName() : std::string("none")));
	std::string trace;
	for (int i = 0; i < 10; ++i)
	{
		w.frames(30);
		trace += std::to_string(ai->currentStateId()) + " ";
	}
	MESSAGE("J2 after 300 frames: enemy health=" << w.health(enemy) << " bird z=" << bird->getPosition()->z << " states=" << trace);
	CHECK(false);
}
