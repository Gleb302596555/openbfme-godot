// OpenBFME unit tests. GPL-3.0.
// PORT-257 (issue #605): Flying monsters — GiantBirdAIUpdate, GiantBirdSlowDeathBehavior, FellBeastSwoopPower.
// Synthetic INI only (no retail game files); expectations come from RotWK 2.01 game.dat instructions cited at each case.

#include "doctest.h"
#include "CombatTestUtil.h"

#include "Common/SpecialPower.h"
#include "Common/StateHash.h"
#include "Common/XferLoad.h"
#include "Common/XferSave.h"
#include "GameLogic/AI/AIStateMachine.h"
#include "GameLogic/AI/AIWorld.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/Combat/CombatQueries.h"
#include "GameLogic/Damage.h"
#include "GameLogic/Module/CollideModule.h"
#include "GameLogic/Module/GiantBirdAIUpdate.h"
#include "GameLogic/Module/GiantBirdSlowDeathBehavior.h"
#include "GameLogic/Module/HeroAbilityModules.h"
#include "GameLogic/Module/PhysicsBehavior.h"
#include "GameLogic/Module/SpecialAbilityModules.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/Object/PartitionManager.h"
#include "GameLogic/ObjectTemplateInfo.h"
#include "GameLogic/SimMath.h"

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

using namespace combattest;

namespace
{
struct BirdWorld : combattest::CombatWorld
{
	SpecialPowerStore powers;
	SpecialPowerStore *savedPowers = TheSpecialPowerStore;

	explicit BirdWorld(const char *iniText)
		: combattest::CombatWorld("", 100)
	{
		TheSpecialPowerStore = &powers;
		w.fx.env.blocks.registerBlock("SpecialPower", [](INI *ini) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(ini); });
		SpecialAbilityModules::registerAll(w.modules);
		SpecialPowerModules::registerAll(w.modules);
		const std::string err = w.load(iniText, INI_LOAD_OVERWRITE, "port257.ini");
		REQUIRE_MESSAGE(err.empty(), err);
		logic->partition().setRegion(0.0f, 0.0f, 1000.0f, 1000.0f);
	}
	~BirdWorld()
	{
		TheSpecialPowerStore = savedPowers;
	}
};

// Synthetic templates modeled on Ennorath 1.9.8.3 FellBeastLocomotor / MordorWitchKingOnFellBeast / GondorGwaihir
const char kBirdIni[] =
	"Locomotor Port257BirdLoco\n"
	"  Surfaces = AIR\n"
	"  TurnTime = 3500\n"
	"  TurnTimeDamaged = 3500\n"
	"  Acceleration = 400\n"
	"  Braking = 1000\n"
	"  MinSpeed = 0\n"
	"  SlowTurnRadius = 32\n"
	"  FastTurnRadius = 48\n"
	"  MinTurnSpeed = 30%\n"
	"  PreferredHeight = 121\n"
	"  PreferredAttackHeight = 5\n"
	"  AirborneTargetingHeight = 25\n"
	"  ZAxisBehavior = SURFACE_RELATIVE_HEIGHT\n"
	"  Appearance = GIANT_BIRD\n"
	"  CloseEnoughDist = 2\n"
	"  SwoopStandoffRadius = 310\n"
	"  SwoopStandoffHeight = 119\n"
	"  SwoopTerminalVelocity = 0.18\n"
	"  SwoopAccelerationRate = 0.012\n"
	"  SlideIntoPlaceTime = 900\n"
	"End\n"
	"SpecialPower SpecialAbilityPort257LevelAttack\n"
	"  Enum = SPECIAL_LEVEL_ATTACK\n"
	"  ReloadTime = 1000\n"
	"End\n"
	"Weapon Port257PlowWeapon\n"
	"  AttackRange = 60\n"
	"  DelayBetweenShots = 200\n"
	"  FireFX = None\n"
	"  DamageNugget\n"
	"    Damage = 100\n"
	"    Radius = 30\n"
	"    DamageType = SIEGE\n"
	"    DeathType = NORMAL\n"
	"  End\n"
	"End\n"
	"Object Port257FellBeast\n"
	"  KindOf = PRELOAD SELECTABLE CAN_CAST_REFLECTIONS SCORE HERO GIANT_BIRD CAN_ATTACK MONSTER\n"
	"  VisionRange = 400\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 25\n"
	"  GeometryMinorRadius = 25\n"
	"  GeometryHeight = 40\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY Port257PlowWeapon\n"
	"  End\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = NoArmor\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 3000\n"
	"  End\n"
	"  Behavior = PhysicsBehavior ModuleTag_Physics\n"
	"  End\n"
	"  Behavior = GiantBirdAIUpdate ModuleTag_GiantBirdAI\n"
	"    AutoAcquireEnemiesWhenIdle = No\n"
	"    FollowThroughDistance = 160\n"
	"    FollowThroughCheckStep = 50\n"
	"    FollowThroughGradient = 1.0\n"
	"    GrabTossTimeTrigger = 2.5\n"
	"    GrabTossHeightTrigger = 100.0\n"
	"  End\n"
	"  Behavior = SpecialPowerModule ModuleTag_SwoopStarter\n"
	"    SpecialPowerTemplate = SpecialAbilityPort257LevelAttack\n"
	"    UpdateModuleStartsAttack = Yes\n"
	"  End\n"
	"  Behavior = FellBeastSwoopPower ModuleTag_FellBeastSwoopPower\n"
	"    SpecialPowerTemplate = SpecialAbilityPort257LevelAttack\n"
	"    UnpackTime = 1\n"
	"    StartAbilityRange = 1000.0\n"
	"  End\n"
	"  Behavior = GiantBirdSlowDeathBehavior ModuleTag_GiantBirdSlowDeath\n"
	"    DeathTypes = ALL\n"
	"    DestructionDelay = 20000\n"
	"    CrashAvoidKindOfs = STRUCTURE\n"
	"    CrashAvoidRadius = 200\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = Port257BirdLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 120\n"
	"  End\n"
	"End\n"
	"Object Port257TallObstacle\n"
	"  KindOf = STRUCTURE IMMOBILE SELECTABLE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 30\n"
	"  GeometryMinorRadius = 30\n"
	"  GeometryHeight = 220\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = NoArmor\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 5000\n"
	"  End\n"
	"End\n"
	"Object Port257Victim\n"
	"  KindOf = INFANTRY SELECTABLE SCORE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 15\n"
	"  GeometryMinorRadius = 15\n"
	"  GeometryHeight = 30\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = NoArmor\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 5000\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 50\n"
	"  End\n"
	"End\n"
	"Object Port257GroundHero\n"
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK HERO\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryMinorRadius = 10\n"
	"  GeometryHeight = 25\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = NoArmor\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 1000\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = SpecialPowerModule ModuleTag_SwoopStarter\n"
	"    SpecialPowerTemplate = SpecialAbilityPort257LevelAttack\n"
	"    UpdateModuleStartsAttack = Yes\n"
	"  End\n"
	"  Behavior = FellBeastSwoopPower ModuleTag_FellBeastSwoopPower\n"
	"    SpecialPowerTemplate = SpecialAbilityPort257LevelAttack\n"
	"    UnpackTime = 1\n"
	"  End\n"
	"End\n";

GiantBirdAIUpdate *birdAI(Object *o)
{
	return o ? dynamic_cast<GiantBirdAIUpdate *>(o->getAIUpdateInterface()) : nullptr;
}

FellBeastSwoopPower *swoopOf(Object *o)
{
	if (!o)
	{
		return nullptr;
	}
	for (const std::unique_ptr<BehaviorModule> &m : o->modules())
	{
		if (auto *p = dynamic_cast<FellBeastSwoopPower *>(m.get()))
		{
			return p;
		}
	}
	return nullptr;
}
} // namespace

// RW 0x008CB2CE (FellBeastSwoopPower::update) -> RW 0x00662B28 (AIUpdateInterface vslot 0x1D0) -> RW 0x008BD372 (AIGiantBirdFollowThruState vslot 0x34):
// FellBeastSwoopPower finishes and clears SPECIAL_WEAPON_ONE (bit 244) as soon as the bird enters STATE_FOLLOW_THRU (0x3F5), while the AI is NOT idle.
TEST_CASE("PORT-257 FellBeastSwoopPower finishes on AIGiantBirdFollowThruState via AI vslot 0x1D0 before AI idles (RW 0x8CB2CE, RW 0x662B28, RW 0x8BD372)")
{
	BirdWorld w(kBirdIni);
	Object *beast = w.unit("Port257FellBeast", 'A', 300.0f, 300.0f);
	Object *victim = w.unit("Port257Victim", 'B', 500.0f, 300.0f);
	REQUIRE(beast != nullptr);
	REQUIRE(victim != nullptr);
	GiantBirdAIUpdate *ai = birdAI(beast);
	FellBeastSwoopPower *swoop = swoopOf(beast);
	REQUIRE(ai != nullptr);
	REQUIRE(swoop != nullptr);

	// Initiate level swoop power against victim (AI command 0x3F -> STATE_SWOOP_LEVEL 0x3F3)
	swoop->initiateIntentToDoSpecialPower(swoop->abilityData()->m_specialPowerTemplate, victim, nullptr, 0, 0);
	bool enteredSwoop = false;
	bool sawFollowThru = false;
	bool finishedDuringFollowThru = false;
	for (int f = 0; f < 300; ++f)
	{
		w.frames(1);
		const unsigned st = ai->currentStateId();
		if (st == GiantBirdAIUpdate::STATE_SWOOP_LEVEL)
		{
			enteredSwoop = true;
			CHECK_FALSE(ai->isInFollowThruState()); // RW 0x662B28: false in Swoop state (vslot 0x34 -> RW 0x9188EB)
		}
		if (st == GiantBirdAIUpdate::STATE_FOLLOW_THRU)
		{
			sawFollowThru = true;
			REQUIRE_FALSE(ai->isIdle()); // still executing follow-through, NOT idle
			CHECK(ai->isInFollowThruState()); // RW 0xC2DAA0 slot 0x34 -> RW 0x8BD372
			// Give FellBeastSwoopPower its update frame in follow-through (in case it ran before GiantBirdAIUpdate on the transition frame)
			if (swoop->friend_getNextCallFrame() != (UnsignedInt)UPDATE_SLEEP_FOREVER)
			{
				w.frames(1);
			}
			// Must STILL be in STATE_FOLLOW_THRU (not idle) when FellBeastSwoopPower finishes!
			REQUIRE(ai->currentStateId() == GiantBirdAIUpdate::STATE_FOLLOW_THRU);
			REQUIRE_FALSE(ai->isIdle());
			CHECK(swoop->friend_getNextCallFrame() == (UnsignedInt)UPDATE_SLEEP_FOREVER);
			CHECK_FALSE(beast->testModelCondition(244)); // SPECIAL_WEAPON_ONE cleared by finishAbility (RW 0x8CB194)
			finishedDuringFollowThru = (swoop->friend_getNextCallFrame() == (UnsignedInt)UPDATE_SLEEP_FOREVER);
			break;
		}
	}
	CHECK(enteredSwoop);
	CHECK(sawFollowThru);
	CHECK(finishedDuringFollowThru);
	CHECK(swoop->swoopCommands() == 1u);
}

// RW 0x0076B56B (GiantBirdAIUpdate::onCollide, +0x24 vtable RW 0x00C2E19C) + RW 0x0076CF57 .. 0x0076D11B (update collision push) + AIWorld::processGiantBirdContacts:
// verifies CollideModuleInterface wiring, all onCollide guards (ABOUT_TO_HIT, target==other, IMMOBILE, adjacency vs grab flag 0x40), and the push reaction in update().
TEST_CASE("PORT-257 GiantBirdAIUpdate onCollide and update collision push from IMMOBILE obstacle (RW 0x76B56B, RW 0x76CF57)")
{
	BirdWorld w(kBirdIni);
	Object *beast = w.unit("Port257FellBeast", 'A', 300.0f, 300.0f);
	Object *victim = w.unit("Port257Victim", 'B', 800.0f, 300.0f);
	Object *farObstacle = w.unit("Port257TallObstacle", 'B', 320.0f, 300.0f); // overlaps beast (r=25+30=55, dist=20); far from victim (dist=480)
	Object *nearObstacle = w.unit("Port257TallObstacle", 'B', 790.0f, 300.0f); // adjacent to victim (gap=0 <= (25+30)^2)
	Object *mobileOther = w.unit("Port257Victim", 'B', 310.0f, 300.0f); // not IMMOBILE
	REQUIRE(beast != nullptr);
	REQUIRE(victim != nullptr);
	REQUIRE(farObstacle != nullptr);
	REQUIRE(nearObstacle != nullptr);
	REQUIRE(mobileOther != nullptr);
	GiantBirdAIUpdate *ai = birdAI(beast);
	REQUIRE(ai != nullptr);

	// Collide interface at +0x24 (RW 0xC2E19C): getCollide() returns this, wouldLikeToCollideWith() returns false (RW 0x7FEAC1)
	CHECK(ai->getCollide() == static_cast<CollideModuleInterface *>(ai));
	CHECK_FALSE(ai->wouldLikeToCollideWith(*farObstacle));
	CHECK(std::find(beast->collideModules().begin(), beast->collideModules().end(), static_cast<CollideModuleInterface *>(ai)) != beast->collideModules().end());

	// Guard 1: without a victim / goalObject, onCollide ignores the IMMOBILE obstacle (RW 0x76B5A0 .. 0x76B5BE)
	ai->onCollide(farObstacle, nullptr, nullptr);
	CHECK(ai->collisionPush().x == 0.0f);
	CHECK(ai->collisionPush().y == 0.0f);
	CHECK(ai->collisionPush().z == 0.0f);

	// Start swoop attack on victim (sets goalObject = victim)
	REQUIRE(ai->aiAttackObject(victim, CMD_FROM_PLAYER));

	// Guard 2: colliding with target itself is ignored (RW 0x76B5C4)
	ai->onCollide(victim, nullptr, nullptr);
	CHECK(ai->collisionPush().x == 0.0f);

	// Guard 3: colliding with non-IMMOBILE object is ignored (RW 0x76B5CC)
	ai->onCollide(mobileOther, nullptr, nullptr);
	CHECK(ai->collisionPush().x == 0.0f);

	// Guard 4: ABOUT_TO_HIT model condition (bit 0x9B = 155, RW 0x46E918) suppresses onCollide (RW 0x76B58C)
	const int aboutToHit = AIUpdateInterface::modelConditionBit("ABOUT_TO_HIT");
	REQUIRE(aboutToHit == 155);
	beast->setModelConditionState(aboutToHit, true);
	ai->onCollide(farObstacle, nullptr, nullptr);
	CHECK(ai->collisionPush().x == 0.0f);
	beast->setModelConditionState(aboutToHit, false);

	// Guard 5: IMMOBILE obstacle adjacent to target is ignored when grab flag 0x40 is clear (RW 0x76B5D8 .. 0x76B60B)...
	ai->onCollide(nearObstacle, nullptr, nullptr);
	CHECK(ai->collisionPush().x == 0.0f);

	// ...but IS accepted when grab flag 0x40 is set (carrying a grabbed victim, RW 0x76B5E4)
	ai->flight().flags |= 0x40u;
	ai->onCollide(nearObstacle, nullptr, nullptr);
	{
		const float dx = SimMath::subf32(beast->getPosition()->x, nearObstacle->getPosition()->x);
		const float dy = SimMath::subf32(beast->getPosition()->y, nearObstacle->getPosition()->y);
		const float dz = SimMath::subf32(beast->getPosition()->z, nearObstacle->getPosition()->z);
		const float inv = SimMath::divf32(1.0f, (float)SimMath::length3d(dx, dy, dz));
		CHECK(ai->collisionPush().x == SimMath::mulf32(dx, inv));
		CHECK(ai->collisionPush().y == SimMath::mulf32(dy, inv));
		CHECK(ai->collisionPush().z == SimMath::mulf32(dz, inv));
	}
	ai->flight().flags &= ~0x40u;

	// Now test the full partitionAndCollision -> AIWorld::processGiantBirdContacts() -> onCollide -> update() pipeline!
	// Reset push by running one fresh bird that overlaps farObstacle in the world.
	BirdWorld w2(kBirdIni);
	Object *b2 = w2.unit("Port257FellBeast", 'A', 300.0f, 300.0f);
	Object *v2 = w2.unit("Port257Victim", 'B', 800.0f, 300.0f);
	Object *obs2 = w2.unit("Port257TallObstacle", 'B', 320.0f, 300.0f);
	REQUIRE(b2 != nullptr);
	REQUIRE(v2 != nullptr);
	REQUIRE(obs2 != nullptr);
	GiantBirdAIUpdate *ai2 = birdAI(b2);
	REQUIRE(ai2 != nullptr);
	// Place bird at height 60 (well inside obs2's height 220) and establish m_lastPosition via one idle tick
	Coord3D p0{ 300.0f, 300.0f, 60.0f };
	b2->setPosition(&p0);
	w2.frames(1);
	const float zBefore = b2->getPosition()->z;
	const float orientBefore = b2->getOrientation();
	ai2->flight().speed = 40.0f;
	const float speedBefore = ai2->flight().speed;
	REQUIRE(ai2->aiAttackObject(v2, CMD_FROM_PLAYER));

	const unsigned long long contactsBefore = w2.ai->giantBirdContacts();
	// partitionAndCollision runs processGiantBirdContacts() -> onCollide populates m_collisionPush, then GiantBirdAIUpdate::update() consumes it on the same frame!
	w2.frames(1);
	CHECK(w2.ai->giantBirdContacts() == contactsBefore + 1);
	CHECK(ai2->collisionPushed());
	CHECK(ai2->collisionPush().x == 0.0f);
	CHECK(ai2->collisionPush().y == 0.0f);
	CHECK(ai2->collisionPush().z == 0.0f);
	// RW 0x76CFFA (RW 0xBD88C0 = 4.0f): Z lifted to m_lastPosition.z + 4.0f
	CHECK(b2->getPosition()->z == SimMath::addf32(zBefore, 4.0f));
	// RW 0x76CFF3: CLIMBING model condition (bit 0x67 = 103) set
	const int climbing = AIUpdateInterface::modelConditionBit("CLIMBING");
	REQUIRE(climbing == 103);
	CHECK(b2->testModelCondition(climbing));
	// RW 0x76D044 .. 0x76D0B4: RotWK's swapped clamp always turns by -0.2f
	float expectedOrient = SimMath::addf32(orientBefore, -0.2f);
	const float pi = 3.1415927410125732421875f;
	const float twoPi = 6.283185482025146484375f;
	while (expectedOrient > pi) expectedOrient = SimMath::subf32(expectedOrient, twoPi);
	while (expectedOrient <= -pi) expectedOrient = SimMath::addf32(expectedOrient, twoPi);
	CHECK(b2->getOrientation() == expectedOrient);
	// RW 0x76CEBA (*0.9f), AIGiantBirdSwoopState::update (*(1 - 0.4f)), then RW 0x76D0FB (*0.5f): 40 * 0.9 * 0.6 * 0.5 = 10.8
	const float decayed = SimMath::mulf32(speedBefore, 0.9f);
	const float swooped = SimMath::mulf32(SimMath::subf32(1.0f, 0.4f), decayed);
	CHECK(ai2->flight().speed == SimMath::mulf32(swooped, 0.5f));
	// Move obstacle away so no second collision fires, and confirm next frame clears m_collisionPushed and rebuilds spline (RW 0x76D120)
	Coord3D away{ 50.0f, 50.0f, 0.0f };
	obs2->setPosition(&away);
	w2.frames(1);
	CHECK_FALSE(ai2->collisionPushed());
	CHECK(ai2->spline().valid());
}

// RW 0x00769E50 -> RW 0x007695C2 (states 0x3FD STATE_FLY_OFF_MAP, 0x3FE STATE_DESTROY_SELF), RW 0x0076DAB0 -> RW 0x0076A0E6 (AI cmd 0x41), RW 0x007695A7 (destroyObject)
TEST_CASE("PORT-257 GiantBirdAIUpdate AI command 0x41 enters STATE_FLY_OFF_MAP (0x3FD) and destroys self via STATE_DESTROY_SELF (0x3FE) (RW 0x7695C2, RW 0x76A0E6, RW 0x7695A7)")
{
	BirdWorld w(kBirdIni);
	Object *beast = w.unit("Port257FellBeast", 'A', 300.0f, 300.0f);
	REQUIRE(beast != nullptr);
	GiantBirdAIUpdate *ai = birdAI(beast);
	REQUIRE(ai != nullptr);
	const ObjectID id = beast->getID();

	// Deferral check while attacking (RW 0x76D9B6 .. 0x76D9C7): command 0x41 is deferred until attacking clears
	ai->setAttacking(true);
	const Coord3D exitPos{ 420.0f, 300.0f, 80.0f };
	REQUIRE(ai->aiFlyOffMap(exitPos, CMD_FROM_AI));
	CHECK(ai->currentStateId() != GiantBirdAIUpdate::STATE_FLY_OFF_MAP);
	ai->setAttacking(false);
	w.frames(1);
	REQUIRE(w.byId(id) != nullptr);
	CHECK(ai->currentStateId() == GiantBirdAIUpdate::STATE_FLY_OFF_MAP);
	CHECK(ai->goalType() == 1);

	// Fly to exitPos -> STATE_FLY_OFF_MAP finishes -> transitions to STATE_DESTROY_SELF (0x3FE) -> destroyObject (RW 0x7695A7 -> 0x62BBAB)
	bool destroyed = false;
	for (int f = 0; f < 250; ++f)
	{
		w.frames(1);
		if (w.byId(id) == nullptr)
		{
			destroyed = true;
			break;
		}
	}
	CHECK(destroyed);
}

// PROCESS 15.5 + RW 0x0076C96B: non-zero m_collisionPush contributes to crc() and both m_collisionPush and STATE_FLY_OFF_MAP round-trip through xferAIState()
TEST_CASE("PORT-257 GiantBirdAIUpdate crc and xfer preserve active collision push and STATE_FLY_OFF_MAP (RW 0x76C96B)")
{
	BirdWorld a(kBirdIni);
	BirdWorld b(kBirdIni);
	Object *ba = a.unit("Port257FellBeast", 'A', 300.0f, 300.0f);
	Object *va = a.unit("Port257Victim", 'B', 800.0f, 300.0f);
	Object *oa = a.unit("Port257TallObstacle", 'B', 320.0f, 300.0f);
	Object *bb = b.unit("Port257FellBeast", 'A', 300.0f, 300.0f);
	Object *vb = b.unit("Port257Victim", 'B', 800.0f, 300.0f);
	Object *ob = b.unit("Port257TallObstacle", 'B', 320.0f, 300.0f);
	REQUIRE(ba != nullptr);
	REQUIRE(bb != nullptr);
	GiantBirdAIUpdate *aia = birdAI(ba);
	GiantBirdAIUpdate *aib = birdAI(bb);
	REQUIRE(aia != nullptr);
	REQUIRE(aib != nullptr);

	// Baseline identical hashes
	CHECK(a.logic->computeStateHash() == b.logic->computeStateHash());

	// Enter STATE_FLY_OFF_MAP on `a` and accumulate a non-zero collision push via onCollide (with victim set temporarily)
	REQUIRE(aia->aiAttackObject(va, CMD_FROM_PLAYER));
	REQUIRE(aib->aiAttackObject(vb, CMD_FROM_PLAYER));
	CHECK(a.logic->computeStateHash() == b.logic->computeStateHash());

	// Accumulate push on `a` only -> CRC MUST differ!
	aia->onCollide(oa, nullptr, nullptr);
	{
		const float dx = SimMath::subf32(ba->getPosition()->x, oa->getPosition()->x);
		const float dy = SimMath::subf32(ba->getPosition()->y, oa->getPosition()->y);
		const float dz = SimMath::subf32(ba->getPosition()->z, oa->getPosition()->z);
		const float inv = SimMath::divf32(1.0f, (float)SimMath::length3d(dx, dy, dz));
		REQUIRE(aia->collisionPush().x == SimMath::mulf32(dx, inv));
	}
	CHECK(a.logic->computeStateHash() != b.logic->computeStateHash());

	// Also switch `a` into STATE_FLY_OFF_MAP (0x3FD) while keeping the non-zero push pending
	REQUIRE(aia->aiFlyOffMap(Coord3D{ 500.0f, 300.0f, 100.0f }, CMD_FROM_AI));
	REQUIRE(aia->currentStateId() == GiantBirdAIUpdate::STATE_FLY_OFF_MAP);

	// Save `a` and load into `b`: xfer bytes, collisionPush, state 0x3FD, and subsequent frame hashes must match bit-for-bit!
	XferSave saved;
	aia->xferAIState(saved);
	{
		XferLoad loader(saved.data());
		aib->xferAIState(loader);
	}
	XferSave resaved;
	aib->xferAIState(resaved);
	CHECK(saved.data() == resaved.data());
	CHECK(aib->currentStateId() == GiantBirdAIUpdate::STATE_FLY_OFF_MAP);
	CHECK(aib->collisionPush().x == aia->collisionPush().x);
	CHECK(aib->collisionPush().y == aia->collisionPush().y);
	CHECK(aib->collisionPush().z == aia->collisionPush().z);
	CHECK(a.logic->computeStateHash() == b.logic->computeStateHash());

	// Move obstacles out of the way and step both games for 5 frames — hashes remain identical
	Coord3D away{ 50.0f, 50.0f, 0.0f };
	oa->setPosition(&away);
	ob->setPosition(&away);
	for (int f = 0; f < 5; ++f)
	{
		a.frames(1);
		b.frames(1);
		CHECK(a.logic->computeStateHash() == b.logic->computeStateHash());
	}
}

// Neighbor test for GiantBirdSlowDeathBehavior (RW 0x86511C, RW 0x865009, RW 0x86502B)
TEST_CASE("PORT-257 neighbor GiantBirdSlowDeathBehavior: killed bird picks clear crash target, flies at speed 30, and xfer round-trips crashTarget (RW 0x86511C, RW 0x86502B)")
{
	BirdWorld w(kBirdIni);
	Object *beast = w.unit("Port257FellBeast", 'A', 300.0f, 300.0f);
	Object *block = w.unit("Port257TallObstacle", 'B', 300.0f, 300.0f);
	REQUIRE(beast != nullptr);
	REQUIRE(block != nullptr);
	GiantBirdSlowDeathBehavior *death = dynamic_cast<GiantBirdSlowDeathBehavior *>(beast->findModule("GiantBirdSlowDeathBehavior"));
	REQUIRE(death != nullptr);

	w.frames(1);
	beast->kill(DAMAGE_UNRESISTABLE, DEATH_NORMAL);
	REQUIRE(death->isSlowDeathActivated());
	// Flyer radius 25 + obstacle radius 30 = 55; clear ring lies outside obstacle
	const float dx = death->crashTarget().x - 300.0f;
	const float dy = death->crashTarget().y - 300.0f;
	// tolerance: findClearPoint searches discrete ring samples outside combined radii (25 + 30 = 55)
	CHECK(std::hypot(dx, dy) >= 55.0f);
	PhysicsBehavior *physics = beast->physicsModule();
	REQUIRE(physics != nullptr);
	CHECK(physics->isFlying());
	CHECK(physics->speed() == 30.0f); // RW 0xBDAE54

	// Xfer round-trip (RW 0x86502B)
	XferSave saved;
	death->xfer(saved);
	Object *beast2 = w.unit("Port257FellBeast", 'A', 600.0f, 600.0f);
	GiantBirdSlowDeathBehavior *death2 = dynamic_cast<GiantBirdSlowDeathBehavior *>(beast2->findModule("GiantBirdSlowDeathBehavior"));
	REQUIRE(death2 != nullptr);
	{
		XferLoad loader(saved.data());
		death2->xfer(loader);
	}
	CHECK(death2->crashTarget().x == death->crashTarget().x);
	CHECK(death2->crashTarget().y == death->crashTarget().y);
	CHECK(death2->crashTarget().z == death->crashTarget().z);
}

// Neighbor test for GiantBirdAIUpdate normal flight + FellBeastSwoopPower on a non-bird AI (RW 0x76CDFB, RW 0x8CB28B)
TEST_CASE("PORT-257 neighbor GiantBirdAIUpdate solo flight has no spurious onCollide push and FellBeastSwoopPower on ground AI keeps unported count (RW 0x76CDFB, RW 0x8CB28B)")
{
	BirdWorld w(kBirdIni);
	Object *beast = w.unit("Port257FellBeast", 'A', 200.0f, 300.0f);
	Object *obs = w.unit("Port257TallObstacle", 'B', 350.0f, 300.0f);
	Object *hero = w.unit("Port257GroundHero", 'A', 200.0f, 600.0f);
	REQUIRE(beast != nullptr);
	REQUIRE(obs != nullptr);
	REQUIRE(hero != nullptr);
	GiantBirdAIUpdate *ai = birdAI(beast);
	FellBeastSwoopPower *groundSwoop = swoopOf(hero);
	REQUIRE(ai != nullptr);
	REQUIRE(groundSwoop != nullptr);

	// Solo move order (no victim / goalObject): even passing near/over an IMMOBILE structure produces zero onCollide push
	ai->aiMoveToPosition(Coord3D{ 500.0f, 300.0f, 0.0f }, CMD_FROM_PLAYER);
	CHECK(ai->currentStateId() == GiantBirdAIUpdate::STATE_FLY_TO);
	for (int f = 0; f < 60; ++f)
	{
		w.frames(1);
		CHECK_FALSE(ai->collisionPushed());
	}
	CHECK(beast->testStatus((unsigned)CombatNames::status("AIRBORNE_TARGET")));

	// Ground hero with FellBeastSwoopPower (command 0x40): counts unported command and finishes when idle
	const Coord3D at{ 400.0f, 600.0f, 0.0f };
	groundSwoop->initiateIntentToDoSpecialPower(groundSwoop->abilityData()->m_specialPowerTemplate, nullptr, &at, 0, 0);
	w.frames(5);
	CHECK(groundSwoop->swoopCommands() == 1u);
	CHECK(hero->getAIUpdateInterface()->unportedCommands().count("level attack position (AI command 0x40: GiantBirdAIUpdate vslot 0x94)") == 1u);
	CHECK(groundSwoop->friend_getNextCallFrame() == (UnsignedInt)UPDATE_SLEEP_FOREVER);
}
