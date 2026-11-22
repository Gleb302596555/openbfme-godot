// OpenBFME unit tests. LOCAL-86: Derived Contain classes missing / fixed own functions (both editions).
// Synthetic INI, no retail files. Expectations come from RotWK 2.01 game.dat instructions cited at each case.

#include "doctest.h"
#include "CombatTestUtil.h"

#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/Team.h"
#include "GameLogic/AI/AIStateMachine.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/Damage.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/ActiveBody.h"
#include "GameLogic/Module/DamageModule.h"
#include "GameLogic/Object/Contain/GarrisonContainRuntime.h"
#include "GameLogic/Object/Contain/SiegeEngineContainRuntime.h"
#include "GameLogic/Object/Contain/TransportContainRuntime.h"
#include "GameLogic/Object/Contain/TunnelContainRuntime.h"
#include "GameLogic/ExperienceLevels.h"
#include "GameLogic/Object/ExperienceTracker.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/Object/PartitionManager.h"
#include "GameLogic/UpgradeTypes.h"

#include <string>
#include <vector>

using namespace combattest;

namespace
{

const char kLocal86Levels[] =
	"ExperienceLevel Local86Lvl1\n"
	"  TargetNames = Local86AodMember\n"
	"  RequiredExperience = 0\n"
	"  ExperienceAward = 10\n"
	"  Rank = 1\n"
	"End\n"
	"ExperienceLevel Local86Lvl2\n"
	"  TargetNames = Local86AodMember\n"
	"  RequiredExperience = 50\n"
	"  ExperienceAward = 20\n"
	"  Rank = 2\n"
	"End\n"
	"ExperienceLevel Local86Lvl3\n"
	"  TargetNames = Local86AodMember\n"
	"  RequiredExperience = 100\n"
	"  ExperienceAward = 30\n"
	"  Rank = 3\n"
	"End\n";

const char kLocal86Ini[] =
	"Upgrade Upgrade_Local86Test\n"
	"  Type = OBJECT\n"
	"End\n"
	"\n"
	"Object Local86Passenger\n"
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK\n"
	"  TransportSlotCount = 1\n"
	"  VisionRange = 200\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 5\n"
	"  GeometryMinorRadius = 5\n"
	"  GeometryHeight = 20\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = PlainArmor\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = StatusBitsUpgrade ModuleTag_Upg\n"
	"    TriggeredBy = Upgrade_Local86Test\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = PlainLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 50\n"
	"  End\n"
	"End\n"
	"\n"
	"Object Local86Mine\n"
	"  KindOf = INFANTRY SELECTABLE MINE\n"
	"  TransportSlotCount = 1\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 5\n"
	"  GeometryMinorRadius = 5\n"
	"  GeometryHeight = 10\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 50\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"End\n"
	"\n"
	"Object Local86Crew\n"
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK ARCHER\n"
	"  TransportSlotCount = 1\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 5\n"
	"  GeometryMinorRadius = 5\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = TemporarilyDefectUpdate ModuleTag_Defect\n"
	"    DefectDuration = 5000\n"
	"  End\n"
	"End\n"
	"\n"
	"Object Local86Transport\n"
	"  KindOf = VEHICLE SELECTABLE TRANSPORT\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 15\n"
	"  GeometryMinorRadius = 15\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = TransportContain ModuleTag_Contain\n"
	"    Slots = 5\n"
	"    AllowAlliesInside = Yes\n"
	"    AllowEnemiesInside = No\n"
	"    AllowNeutralInside = No\n"
	"  End\n"
	"End\n"
	"\n"
	"Object Local86HordeTransport\n"
	"  KindOf = VEHICLE SELECTABLE TRANSPORT\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 15\n"
	"  GeometryMinorRadius = 15\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = HordeTransportContain ModuleTag_Contain\n"
	"    Slots = 5\n"
	"    AllowAlliesInside = Yes\n"
	"    AllowEnemiesInside = No\n"
	"    AllowNeutralInside = No\n"
	"  End\n"
	"End\n"
	"\n"
	"Object Local86GarrisonBuilding\n"
	"  KindOf = STRUCTURE SELECTABLE GARRISONABLE_UNTIL_DESTROYED\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 20\n"
	"  GeometryMinorRadius = 20\n"
	"  GeometryHeight = 40\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 1000\n"
	"  End\n"
	"  Behavior = GarrisonContain ModuleTag_Contain\n"
	"    ContainMax = 5\n"
	"    AllowAlliesInside = Yes\n"
	"    AllowEnemiesInside = No\n"
	"    AllowNeutralInside = No\n"
	"  End\n"
	"End\n"
	"\n"
	"Object Local86FragileGarrison\n"
	"  KindOf = STRUCTURE SELECTABLE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 20\n"
	"  GeometryMinorRadius = 20\n"
	"  GeometryHeight = 40\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 1000\n"
	"  End\n"
	"  Behavior = GarrisonContain ModuleTag_Contain\n"
	"    ContainMax = 5\n"
	"    AllowAlliesInside = Yes\n"
	"    AllowEnemiesInside = No\n"
	"    AllowNeutralInside = No\n"
	"  End\n"
	"End\n"
	"\n"
	"Object Local86HordeGarrisonKill\n"
	"  KindOf = STRUCTURE SELECTABLE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 20\n"
	"  GeometryMinorRadius = 20\n"
	"  GeometryHeight = 40\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 1000\n"
	"  End\n"
	"  Behavior = HordeGarrisonContain ModuleTag_Contain\n"
	"    ContainMax = 5\n"
	"    KillPassengersOnDeath = Yes\n"
	"    EjectPassengersOnDeath = No\n"
	"    ExitOffset = X:30 Y:10 Z:0\n"
	"    AllowAlliesInside = Yes\n"
	"    AllowEnemiesInside = No\n"
	"    AllowNeutralInside = No\n"
	"  End\n"
	"End\n"
	"\n"
	"Object Local86HordeGarrisonEject\n"
	"  KindOf = STRUCTURE SELECTABLE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 20\n"
	"  GeometryMinorRadius = 20\n"
	"  GeometryHeight = 40\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 1000\n"
	"  End\n"
	"  Behavior = HordeGarrisonContain ModuleTag_Contain\n"
	"    ContainMax = 5\n"
	"    KillPassengersOnDeath = No\n"
	"    EjectPassengersOnDeath = Yes\n"
	"    ExitOffset = X:30 Y:0 Z:0\n"
	"    AllowAlliesInside = Yes\n"
	"    AllowEnemiesInside = No\n"
	"    AllowNeutralInside = No\n"
	"  End\n"
	"End\n"
	"\n"
	"Object Local86SiegeEngine\n"
	"  KindOf = VEHICLE SELECTABLE MACHINE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 15\n"
	"  GeometryMinorRadius = 15\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = TemporarilyDefectUpdate ModuleTag_Defect\n"
	"    DefectDuration = 5000\n"
	"  End\n"
	"  Behavior = SiegeEngineContain ModuleTag_Contain\n"
	"    Slots = 5\n"
	"    CrewFilter = NONE +ARCHER\n"
	"    CrewMax = 2\n"
	"    AllowAlliesInside = Yes\n"
	"    AllowEnemiesInside = No\n"
	"    AllowNeutralInside = No\n"
	"  End\n"
	"End\n"
	"\n"
	"Object Local86Tunnel\n"
	"  KindOf = STRUCTURE SELECTABLE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 20\n"
	"  GeometryMinorRadius = 20\n"
	"  GeometryHeight = 30\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"  Behavior = TunnelContain ModuleTag_Contain\n"
	"    TimeForFullHeal = 5000\n"
	"    AllowAlliesInside = Yes\n"
	"    AllowEnemiesInside = No\n"
	"    AllowNeutralInside = No\n"
	"  End\n"
	"End\n"
	"\n"
	"Object Local86AodMember\n"
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 5\n"
	"  GeometryMinorRadius = 5\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"End\n"
	"\n"
	"Object Local86AodHorde\n"
	"  KindOf = INFANTRY SELECTABLE HORDE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 20\n"
	"  GeometryMinorRadius = 20\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = AODHordeContain ModuleTag_Contain\n"
	"    RankInfo = RankNumber:1 UnitType:Local86AodMember Position:X:0 Y:0\n"
	"  End\n"
	"End\n";

struct Local86World : CombatWorld
{
	ExperienceLevelSystem levels;
	ExperienceLevelSystem *savedLevels = TheExperienceLevelSystem;
	Local86World() : CombatWorld()
	{
		TheExperienceLevelSystem = &levels;
		levels.registerBlocks(w.fx.env.blocks);
		std::string err = w.load(kLocal86Levels, INI_LOAD_OVERWRITE, "local86-levels.ini");
		REQUIRE_MESSAGE(err.empty(), err);
		err = w.load(kLocal86Ini, INI_LOAD_OVERWRITE, "local86.ini");
		REQUIRE_MESSAGE(err.empty(), err);
		logic->partition().setRegion(0.0f, 0.0f, 1000.0f, 1000.0f);
	}
	~Local86World()
	{
		TheExperienceLevelSystem = savedLevels;
	}

	void step(int frames)
	{
		for (int i = 0; i < frames; ++i)
		{
			logic->runLogicFrame();
		}
	}
};

} // namespace

TEST_CASE("LOCAL-86 TransportContain::onCapture (RW 0x0086A3DB / 0x0086B057)")
{
	Local86World w;
	Object *transport = w.unit("Local86Transport", 'A', 100.0f, 100.0f);
	Object *rider = w.unit("Local86Passenger", 'A', 100.0f, 100.0f);
	REQUIRE(transport != nullptr);
	REQUIRE(rider != nullptr);
	auto *c = dynamic_cast<TransportContain *>(transport->getContain());
	REQUIRE(c != nullptr);
	REQUIRE(c->addToContain(rider));
	REQUIRE(c->getContainCount() == 1u);

	Player *pA = w.teamA()->getControllingPlayer();
	Player *pB = w.teamB()->getControllingPlayer();

	// Same owner: onCapture is a no-op (RW 0x0086A3E0..0x0086A3E4).
	c->onCapture(pA, pA);
	CHECK(c->getContainCount() == 1u);

	// DISABLED_UNMANNED (type 5, RW 0x0086A3E9): immediately removes all contained.
	transport->setDisabled(5, (UnsignedInt)UPDATE_SLEEP_FOREVER);
	c->onCapture(pA, pB);
	CHECK(c->getContainCount() == 0u);
	CHECK(rider->getContainedBy() == nullptr);
}

TEST_CASE("LOCAL-86 HordeTransportContain::onContainerUpgrade (RW 0x0087A988 / 0x0087A572)")
{
	Local86World w;
	const UpgradeTemplate *upg = Player::resolveUpgrade("Upgrade_Local86Test", true);
	REQUIRE(upg != nullptr);

	// HordeTransportContain forwards container upgrades to contained passengers.
	Object *ht = w.unit("Local86HordeTransport", 'A', 100.0f, 100.0f);
	Object *rider1 = w.unit("Local86Passenger", 'A', 100.0f, 100.0f);
	REQUIRE(ht->getContain()->addToContain(rider1));
	REQUIRE(!rider1->hasUpgrade(upg));
	ht->giveUpgrade(upg);
	CHECK(ht->hasUpgrade(upg));
	CHECK(rider1->hasUpgrade(upg));

	// Plain TransportContain does NOT forward upgrades (retail slot 0x174 is no-op).
	Object *pt = w.unit("Local86Transport", 'A', 200.0f, 100.0f);
	Object *rider2 = w.unit("Local86Passenger", 'A', 200.0f, 100.0f);
	REQUIRE(pt->getContain()->addToContain(rider2));
	pt->giveUpgrade(upg);
	CHECK(pt->hasUpgrade(upg));
	CHECK(!rider2->hasUpgrade(upg));
}

TEST_CASE("LOCAL-86 GarrisonContain::onBodyDamageStateChange (RW 0x0087BA06)")
{
	Local86World w;

	// Fragile garrison (without GARRISONABLE_UNTIL_DESTROYED) orders passengers out on BODY_REALLYDAMAGED.
	Object *fragile = w.unit("Local86FragileGarrison", 'A', 100.0f, 100.0f);
	Object *r1 = w.unit("Local86Passenger", 'A', 100.0f, 100.0f);
	REQUIRE(fragile->getContain()->addToContain(r1));
	REQUIRE(fragile->getContain()->getContainCount() == 1u);

	DamageInfo dmg;
	dmg.m_input.m_amount = 800.0f; // 1000 -> 200: enters BODY_REALLYDAMAGED
	fragile->getBodyModule()->attemptDamage(dmg);
	CHECK(fragile->getBodyModule()->getDamageState() == BODY_REALLYDAMAGED);
	w.step(5);
	CHECK(fragile->getContain()->getContainCount() == 0u);

	// Garrison with GARRISONABLE_UNTIL_DESTROYED keeps its passengers on BODY_REALLYDAMAGED.
	Object *sturdy = w.unit("Local86GarrisonBuilding", 'A', 250.0f, 100.0f);
	Object *r2 = w.unit("Local86Passenger", 'A', 250.0f, 100.0f);
	REQUIRE(sturdy->getContain()->addToContain(r2));
	sturdy->getBodyModule()->attemptDamage(dmg);
	CHECK(sturdy->getBodyModule()->getDamageState() == BODY_REALLYDAMAGED);
	w.step(5);
	CHECK(sturdy->getContain()->getContainCount() == 1u);
}

TEST_CASE("LOCAL-86 GarrisonContain::getApparentControllingPlayer (RW 0x0087B392) and getNaturalRallyPoint (RW 0x0087CF57)")
{
	Local86World w;
	Object *b = w.unit("Local86HordeGarrisonKill", 'A', 100.0f, 200.0f);
	ContainModuleInterface *c = b->getContain();
	REQUIRE(c != nullptr);

	Player *pA = w.teamA()->getControllingPlayer();
	Player *pB = w.teamB()->getControllingPlayer();

	// Normal (unhidden): apparent controlling player is the owner.
	CHECK(c->getApparentControllingPlayer(pB) == pA);

	// HordeGarrisonContain::getNaturalRallyPoint returns transformed ExitOffset (X:30 Y:10).
	Coord3D rally{ 0.0f, 0.0f, 0.0f };
	REQUIRE(c->getNaturalRallyPoint(rally));
	CHECK(rally.x == 130.0f);
	CHECK(rally.y == 210.0f);

	// Plain GarrisonContain returns false for getNaturalRallyPoint.
	Object *plain = w.unit("Local86FragileGarrison", 'A', 200.0f, 200.0f);
	CHECK(!plain->getContain()->getNaturalRallyPoint(rally));
}

TEST_CASE("LOCAL-86 HordeGarrisonContain::onDie (RW 0x0087CF74)")
{
	Local86World w;

	// KillPassengersOnDeath = Yes -> kills contained passengers on death.
	Object *bKill = w.unit("Local86HordeGarrisonKill", 'A', 100.0f, 100.0f);
	Object *rKill = w.unit("Local86Passenger", 'A', 100.0f, 100.0f);
	REQUIRE(bKill->getContain()->addToContain(rKill));
	bKill->kill(DAMAGE_UNRESISTABLE, 0);
	CHECK(rKill->isEffectivelyDead());

	// KillPassengersOnDeath = No -> orders passengers out instead of killing them.
	Object *bEject = w.unit("Local86HordeGarrisonEject", 'A', 250.0f, 100.0f);
	Object *rEject = w.unit("Local86Passenger", 'A', 250.0f, 100.0f);
	REQUIRE(bEject->getContain()->addToContain(rEject));
	bEject->kill(DAMAGE_UNRESISTABLE, 0);
	CHECK(!rEject->isEffectivelyDead());
}

TEST_CASE("LOCAL-86 SiegeEngineContain onDefect, onDefectionEnded, iterateContained, hasMinePassenger")
{
	Local86World w;
	Object *siege = w.unit("Local86SiegeEngine", 'A', 100.0f, 100.0f);
	Object *crew = w.unit("Local86Crew", 'A', 100.0f, 100.0f);
	Object *mine = w.unit("Local86Mine", 'A', 100.0f, 100.0f);
	auto *c = dynamic_cast<SiegeEngineContain *>(siege->getContain());
	REQUIRE(c != nullptr);

	CHECK(!c->hasMinePassenger());
	REQUIRE(c->addToContain(crew));
	CHECK(c->getCrewCount() == 1);
	CHECK(!c->hasMinePassenger());

	REQUIRE(c->addToContain(mine));
	CHECK(c->hasMinePassenger()); // RW 0x0087F2F5

	// iterateContained (RW 0x0087F10E): riders (bit 0) + crew (bit 2), forward vs reverse (bit 3)
	std::vector<ObjectID> fwd, rev;
	c->iterateContained([&](Object *o) { fwd.push_back(o->getID()); }, 1u | 4u);
	c->iterateContained([&](Object *o) { rev.push_back(o->getID()); }, 1u | 4u | 8u);
	REQUIRE(fwd.size() == 2u);
	CHECK(fwd[0] == mine->getID());
	CHECK(fwd[1] == crew->getID());
	REQUIRE(rev.size() == 2u);
	CHECK(rev[0] == crew->getID());
	CHECK(rev[1] == mine->getID());

	// onDefect (RW 0x0087F1BA) & onDefectionEnded (RW 0x0087F18B): crew defects with engine and restores on end.
	Player *pA = w.teamA()->getControllingPlayer();
	Player *pB = w.teamB()->getControllingPlayer();
	Object *defector = w.unit("Local86Passenger", 'B', 200.0f, 100.0f);
	siege->defect(defector, false);
	CHECK(siege->getControllingPlayer() == pB);
	CHECK(crew->getControllingPlayer() == pB);

	siege->endDefection();
	CHECK(siege->getControllingPlayer() == pA);
	CHECK(crew->getControllingPlayer() == pA);
}

TEST_CASE("LOCAL-86 TunnelContain::iterateContained (RW 0x00880F78)")
{
	Local86World w;
	Object *tunnel = w.unit("Local86Tunnel", 'A', 100.0f, 100.0f);
	tunnel->friend_onBuildComplete();
	Object *u1 = w.unit("Local86Passenger", 'A', 100.0f, 100.0f);
	Object *u2 = w.unit("Local86Passenger", 'A', 100.0f, 100.0f);
	ContainModuleInterface *c = tunnel->getContain();
	REQUIRE(c->addToContain(u1));
	REQUIRE(c->addToContain(u2));

	std::vector<ObjectID> fwd, rev, none;
	c->iterateContained([&](Object *o) { fwd.push_back(o->getID()); }, 1u);
	c->iterateContained([&](Object *o) { rev.push_back(o->getID()); }, 1u | 8u);
	c->iterateContained([&](Object *o) { none.push_back(o->getID()); }, 0u);
	REQUIRE(fwd.size() == 2u);
	CHECK(fwd[0] == u1->getID());
	CHECK(fwd[1] == u2->getID());
	REQUIRE(rev.size() == 2u);
	CHECK(rev[0] == u2->getID());
	CHECK(rev[1] == u1->getID());
	CHECK(none.empty());
}

TEST_CASE("LOCAL-86 AODHordeContain::onContaining gainLevel (RW 0x0087D649 -> 0x0079D7AD)")
{
	Local86World w;
	Object *horde = w.unit("Local86AodHorde", 'A', 100.0f, 100.0f);
	Object *member = w.unit("Local86AodMember", 'A', 100.0f, 100.0f);
	REQUIRE(horde != nullptr);
	REQUIRE(member != nullptr);
	ExperienceTracker *xp = member->getExperienceTracker();
	REQUIRE(xp != nullptr);
	const int rankBefore = xp->getRank();
	REQUIRE(horde->getContain()->addToContain(member));
	CHECK(xp->getRank() == rankBefore + 1);
}
