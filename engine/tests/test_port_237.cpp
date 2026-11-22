// OpenBFME unit tests. PORT-237 (issue #560):
//   - ObjectCreationList nuggets (ApplyRandomForce RW 0x5EF96D, FireWeapon RW 0x5EF8D8, Attack RW 0x5F0328, CreateObject SkipIfSignificantlyAirborne RW 0x5F0D84)
//   - ObjectCreationList fields (OffsetInLocalSpace RW 0x5E07F7, WaypointSpawnPoints RW 0x5F1A60, InvulnerableTime RW 0x68C6C8 -> DefectionHelper RW 0x8E3ABB)
//   - OCL::create(primary, secondary) (RW 0x5F0126), CreateObjectDie (S-980 closed), AODCrushCollide (S-6008 closed), OCLSpecialPower & TransitionDamageFX secondary forwarding
//   - TerrainLogic::findClosestEdgePoint (RW 0x67D949) & findFarthestEdgePoint (RW 0x67DA48), OCLUpdate CreateAtEdge & getCountdownPercent (S-5501 closed)
//   - Player::applyBattlePlanBonusesForObject / removeBattlePlanBonusesForObject / applyBattlePlanBonusesForPlayerObjects (RW 0x6AD2B5..0x6AD4AE, S-1712)

#include "doctest.h"
#include "CombatTestUtil.h"

#include "Common/Player.h"
#include "Common/SpecialPower.h"
#include "Common/Team.h"
#include "GameLogic/Combat/ObjectWeapons.h"
#include "GameLogic/Damage.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Map/TerrainLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/ActiveBody.h"
#include "GameLogic/Module/AODCrushCollide.h"
#include "GameLogic/Module/ExtraDieModules.h"
#include "GameLogic/Module/ExtraModules.h"
#include "GameLogic/Module/ObjectHelper.h"
#include "GameLogic/Module/OCLUpdate.h"
#include "GameLogic/Module/PhysicsBehavior.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/ObjectCreationList.h"
#include "GameLogic/ObjectTemplateInfo.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/WeaponSet.h"

#include <cstdint>
#include <string>
#include <vector>

using namespace combattest;

namespace
{
const char kPort237Objects[] =
	"SpecialPower SpecialPort237OCL\n"
	"  Enum = SPECIAL_SPELL_BOOK_ARROW_VOLLEY_GOOD\n"
	"  ReloadTime = 1000\n"
	"End\n"
	"Object Port237Unit\n"
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK\n"
	"  ShroudClearingRange = 100.0\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = NoArmor\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 200\n"
	"  End\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY BowWeapon\n"
	"    Weapon = SECONDARY SwordWeapon\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = PhysicsBehavior ModuleTag_Physics\n"
	"  End\n"
	"  Behavior = DestroyDie ModuleTag_Die\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 40\n"
	"  End\n"
	"End\n"
	"Object Port237Caster\n"
	"  KindOf = STRUCTURE\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"  Behavior = OCLSpecialPower ModuleTag_SP\n"
	"    SpecialPowerTemplate = SpecialPort237OCL\n"
	"    OCL = OCL_Port237SpawnMove\n"
	"    CreateLocation = USE_OWNER_OBJECT\n"
	"  End\n"
	"End\n"
	"Object Port237DieVictim\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 50\n"
	"  End\n"
	"  Behavior = CreateObjectDie ModuleTag_Die\n"
	"    CreationList = OCL_Port237FireWeapon\n"
	"  End\n"
	"  Behavior = DestroyDie ModuleTag_Destroy\n"
	"  End\n"
	"End\n"
	"Object Port237Crusher\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 300\n"
	"  End\n"
	"  Behavior = PhysicsBehavior ModuleTag_Physics\n"
	"  End\n"
	"  Behavior = AODCrushCollide ModuleTag_Crush\n"
	"    Damage = 1.0\n"
	"    SmallObjectCreationList = OCL_Port237Spawned\n"
	"  End\n"
	"End\n"
	"Object Port237EdgeSpawner\n"
	"  KindOf = STRUCTURE\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 200\n"
	"  End\n"
	"  Behavior = OCLUpdate ModuleTag_OCLUpdate\n"
	"    OCL = OCL_Port237Spawned\n"
	"    MinDelay = 2000\n"
	"    MaxDelay = 2000\n"
	"    Amount = 1\n"
	"    CreateAtEdge = Yes\n"
	"  End\n"
	"End\n"
	"Object Port237Spawned\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = PhysicsBehavior ModuleTag_Physics\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 40\n"
	"  End\n"
	"End\n"
	"ObjectCreationList OCL_Port237Spawned\n"
	"  CreateObject\n"
	"    ObjectNames = Port237Spawned\n"
	"    Count = 1\n"
	"    Disposition = LIKE_EXISTING\n"
	"  End\n"
	"End\n"
	"ObjectCreationList OCL_Port237SpawnMove\n"
	"  CreateObject\n"
	"    ObjectNames = Port237Spawned\n"
	"    Count = 1\n"
	"    IssueMoveAfterCreation = Yes\n"
	"  End\n"
	"End\n"
	"ObjectCreationList OCL_Port237RandomForce\n"
	"  ApplyRandomForce\n"
	"    MinForceMagnitude = 10.0\n"
	"    MaxForceMagnitude = 10.0\n"
	"    MinForcePitch = 45.0\n"
	"    MaxForcePitch = 45.0\n"
	"  End\n"
	"End\n"
	"ObjectCreationList OCL_Port237FireWeapon\n"
	"  FireWeapon\n"
	"    Weapon = SwordWeapon\n"
	"  End\n"
	"End\n"
	"ObjectCreationList OCL_Port237Attack\n"
	"  Attack\n"
	"    NumberOfShots = 2\n"
	"    WeaponSlot = SECONDARY\n"
	"  End\n"
	"End\n"
	"ObjectCreationList OCL_Port237AirborneSkip\n"
	"  CreateObject\n"
	"    ObjectNames = Port237Spawned\n"
	"    Count = 1\n"
	"    SkipIfSignificantlyAirborne = Yes\n"
	"  End\n"
	"End\n"
	"ObjectCreationList OCL_Port237LocalOffset\n"
	"  CreateObject\n"
	"    ObjectNames = Port237Spawned\n"
	"    Count = 1\n"
	"    Offset = X:10 Y:0 Z:0\n"
	"    OffsetInLocalSpace = Yes\n"
	"    Disposition = LIKE_EXISTING\n"
	"  End\n"
	"End\n"
	"ObjectCreationList OCL_Port237Waypoints\n"
	"  CreateObject\n"
	"    ObjectNames = Port237Spawned\n"
	"    Count = 1\n"
	"    WaypointSpawnPoints = Reinforce_\n"
	"    Disposition = LIKE_EXISTING\n"
	"  End\n"
	"End\n"
	"ObjectCreationList OCL_Port237Invuln\n"
	"  CreateObject\n"
	"    ObjectNames = Port237Spawned\n"
	"    Count = 1\n"
	"    InvulnerableTime = 1000\n"
	"    Disposition = LIKE_EXISTING\n"
	"  End\n"
	"End\n";

struct Port237World : CombatWorld
{
	SpecialPowerStore powers;
	SpecialPowerStore *savedPowers = TheSpecialPowerStore;
	ObjectCreationListStore oclStore{ w.keys };
	ObjectCreationListStore *savedOcl = TheObjectCreationListStore;
	explicit Port237World()
	{
		TheSpecialPowerStore = &powers;
		TheObjectCreationListStore = &oclStore;
		w.fx.env.blocks.registerBlock("SpecialPower", [](INI *ini) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(ini); });
		w.fx.env.blocks.registerBlock("ObjectCreationList", [](INI *ini) { ObjectCreationListStore::parseObjectCreationListDefinitionGlobal(ini); });
		SpecialPowerModules::registerAll(w.modules);
		ExtraModules::registerAll(w.modules);
		AODCrushCollide::registerClass(w.modules);
		OCLUpdate::registerClass(w.modules);
		const std::string err = w.load(kPort237Objects, INI_LOAD_OVERWRITE, "port237.ini");
		REQUIRE_MESSAGE(err.empty(), err);
	}
	~Port237World()
	{
		TheSpecialPowerStore = savedPowers;
		TheObjectCreationListStore = savedOcl;
	}
};

bool hasStop(const GameLogic &logic, const std::string &needle)
{
	for (const std::string &line : logic.report().stops)
	{
		if (line.find(needle) != std::string::npos)
		{
			return true;
		}
	}
	return false;
}
} // namespace

TEST_CASE("PORT-237 OCL nuggets: ApplyRandomForce, FireWeapon, Attack, and SkipIfSignificantlyAirborne")
{
	Port237World w;
	Object *src = w.unit("Port237Unit", 'A', 100.0f, 100.0f);
	Object *tgt = w.unit("Port237Unit", 'B', 110.0f, 100.0f);
	REQUIRE(src);
	REQUIRE(tgt);

	// ApplyRandomForce (RW 0x5EF96D) flings source via PhysicsBehavior
	const ObjectCreationList *oclForce = TheObjectCreationListStore->findObjectCreationList("OCL_Port237RandomForce");
	REQUIRE(oclForce);
	PhysicsBehavior *phys = PhysicsBehavior::find(*src);
	REQUIRE(phys);
	CHECK_FALSE(phys->isFlying());
	oclForce->create(*w.logic, src, nullptr);
	CHECK(phys->isFlying());
	w.frames(2);

	// FireWeapon (RW 0x5EF882 -> 0x5EF8D8) damages target at secondary position
	const ObjectCreationList *oclFire = TheObjectCreationListStore->findObjectCreationList("OCL_Port237FireWeapon");
	REQUIRE(oclFire);
	const float hpBefore = tgt->getBodyModule()->getHealth();
	oclFire->create(*w.logic, src, tgt);
	CHECK(tgt->getBodyModule()->getHealth() == hpBefore - 10.0f);

	// Attack (RW 0x5EF882 -> 0x5F0328) locks SECONDARY weapon slot temporarily and orders aiAttackPosition
	const ObjectCreationList *oclAttack = TheObjectCreationListStore->findObjectCreationList("OCL_Port237Attack");
	REQUIRE(oclAttack);
	oclAttack->create(*w.logic, src, tgt);
	REQUIRE(src->getWeapons() != nullptr);
	CHECK(src->getWeapons()->curSlot() == SECONDARY_WEAPON);
	CHECK(src->getWeapons()->isCurWeaponLocked());
	CHECK(src->getAIUpdateInterface()->currentStateId() == (unsigned)AI_ATTACK_POSITION);

	// SkipIfSignificantlyAirborne (RW 0x5F0D84 -> 0x70C55D)
	const ObjectCreationList *oclAir = TheObjectCreationListStore->findObjectCreationList("OCL_Port237AirborneSkip");
	REQUIRE(oclAir);
	const Coord3D highPos{ 100.0f, 100.0f, 200.0f };
	src->setPosition(&highPos);
	CHECK(src->isSignificantlyAboveTerrain());
	CHECK(oclAir->create(*w.logic, src, nullptr).empty());
	const Coord3D groundPos{ 100.0f, 100.0f, 0.0f };
	src->setPosition(&groundPos);
	CHECK_FALSE(src->isSignificantlyAboveTerrain());
	CHECK(oclAir->create(*w.logic, src, nullptr).size() == 1);
}

TEST_CASE("PORT-237 OCL fields: OffsetInLocalSpace, WaypointSpawnPoints, InvulnerableTime + DefectionHelper snapshot, and OCLSpecialPower secondary")
{
	Port237World w;
	WorldHeightMap hm;
	hm.m_width = 32;
	hm.m_height = 32;
	hm.m_borderSize = 0;
	hm.m_data.assign(32u * 32u, 0);
	hm.m_boundaries.push_back(ICoord2D{ 20, 10 }); // playable extent 200 x 100
	MapChunks chunks;
	MapObject wp1;
	wp1.m_isWaypoint = true;
	wp1.m_location = Coord3D{ 77.0f, 88.0f, 0.0f };
	wp1.m_properties.setInt("waypointID", 1);
	wp1.m_properties.setAsciiString("waypointName", "Reinforce_North");
	MapObject wpOther;
	wpOther.m_isWaypoint = true;
	wpOther.m_location = Coord3D{ 11.0f, 22.0f, 0.0f };
	wpOther.m_properties.setInt("waypointID", 2);
	wpOther.m_properties.setAsciiString("waypointName", "reinforce_Lowercase"); // case-sensitive prefix must NOT match
	chunks.objects.push_back(wp1);
	chunks.objects.push_back(wpOther);
	TerrainLogic terrain;
	terrain.init(hm, chunks);
	w.logic->setTerrain(&terrain);

	// TerrainLogic::findClosestEdgePoint (RW 0x67D949) & findFarthestEdgePoint (RW 0x67DA48)
	const Coord3D closest = terrain.findClosestEdgePoint(Coord3D{ 150.0f, 80.0f, 0.0f });
	CHECK(closest.x == 150.0f);
	CHECK(closest.y == 100.0f);
	const Coord3D farthest = terrain.findFarthestEdgePoint(Coord3D{ 30.0f, 80.0f, 0.0f });
	CHECK(farthest.x == 200.0f);
	CHECK(farthest.y == 0.0f);

	// OCLUpdate CreateAtEdge (S-5501 closed) spawns at findClosestEdgePoint and reports getCountdownPercent
	Object *spawner = w.unit("Port237EdgeSpawner", 'A', 150.0f, 80.0f);
	REQUIRE(spawner);
	OCLUpdate *oclUpd = dynamic_cast<OCLUpdate *>(spawner->findModule("OCLUpdate"));
	REQUIRE(oclUpd);
	w.frames(1); // frame 1 schedules nextFrame = 11
	CHECK(oclUpd->nextFrame() == 11u);
	w.frames(5); // frame 6 -> 5/10 = 0.5
	CHECK(oclUpd->getCountdownPercent() == 0.5f);
	const size_t beforeSpawnCount = w.logic->getObjectCount();
	w.frames(5); // frame 11 triggers OCL at closest edge (150, 100)
	CHECK(w.logic->getObjectCount() == beforeSpawnCount + 1);
	CHECK(oclUpd->created() == 1);
	CHECK_FALSE(hasStop(*w.logic, "[S-5501]"));

	// OffsetInLocalSpace (RW 0x5E07F7): rotate source by +90 deg so local (+10, 0, 0) becomes world (0, +10, 0)
	Object *src = w.unit("Port237Unit", 'A', 50.0f, 50.0f);
	Object *otherTeam = w.unit("Port237Unit", 'B', 150.0f, 150.0f);
	REQUIRE(src);
	REQUIRE(otherTeam);
	src->setOrientation(1.5707964f);
	const ObjectCreationList *oclOffset = TheObjectCreationListStore->findObjectCreationList("OCL_Port237LocalOffset");
	REQUIRE(oclOffset);
	std::vector<Object *> offMade = oclOffset->create(*w.logic, src, nullptr);
	REQUIRE(offMade.size() == 1);
	// tolerance: pi/2 rotation cos/sin rounding in float basis
	CHECK(offMade[0]->getPosition()->x == doctest::Approx(50.0f).epsilon(0.01f));
	// tolerance: pi/2 rotation cos/sin rounding in float basis
	CHECK(offMade[0]->getPosition()->y == doctest::Approx(60.0f).epsilon(0.01f));

	// WaypointSpawnPoints (RW 0x5F1A60): case-sensitive prefix matches Reinforce_North, ignoring reinforce_Lowercase
	const ObjectCreationList *oclWp = TheObjectCreationListStore->findObjectCreationList("OCL_Port237Waypoints");
	REQUIRE(oclWp);
	std::vector<Object *> wpMade = oclWp->create(*w.logic, src, nullptr);
	REQUIRE(wpMade.size() == 1);
	CHECK(wpMade[0]->getPosition()->x == 77.0f);
	CHECK(wpMade[0]->getPosition()->y == 88.0f);

	// InvulnerableTime (RW 0x68C6C8 -> 0x8E3ABB): sets m_undetectable, relationship NEUTRAL, expires via DefectionHelper, survives snapshot
	const ObjectCreationList *oclInv = TheObjectCreationListStore->findObjectCreationList("OCL_Port237Invuln");
	REQUIRE(oclInv);
	std::vector<Object *> invMade = oclInv->create(*w.logic, src, nullptr);
	REQUIRE(invMade.size() == 1);
	Object *ghost = invMade[0];
	CHECK(ghost->isUndetectable());
	CHECK(ghost->getRelationship(*otherTeam) == NEUTRAL);
	CHECK(otherTeam->getRelationship(*ghost) == NEUTRAL);
	CHECK(w.logic->report().helperShells.count("DefectionHelper") == 1);
	CHECK(w.logic->report().unportedUpdateCalls == 0);

	// Snapshot round-trip while undetectable
	std::vector<std::uint8_t> stream;
	std::string err;
	REQUIRE_MESSAGE(w.logic->saveSnapshot(stream, &err), err);
	const std::uint32_t savedHash = w.hash();
	Port237World loaded;
	REQUIRE_MESSAGE(loaded.logic->loadSnapshot(stream, &err), err);
	CHECK(loaded.hash() == savedHash);
	Object *loadedGhost = loaded.logic->findObjectByID(ghost->getID());
	REQUIRE(loadedGhost);
	CHECK(loadedGhost->isUndetectable());

	// Advance 5 frames (1000ms) -> DefectionHelper clears undetectable and returns relationship to ENEMIES
	w.frames(5);
	CHECK_FALSE(ghost->isUndetectable());
	CHECK(ghost->getRelationship(*otherTeam) == ENEMIES);

	// OCLSpecialPower USE_OWNER_OBJECT forwards secondary position (triggerSpecialPower counts +1 for null clientHooks, not OCLSpecialPower default)
	Object *caster = w.unit("Port237Caster", 'A', 20.0f, 20.0f);
	REQUIRE(caster);
	OCLSpecialPower *sp = dynamic_cast<OCLSpecialPower *>(caster->findModule("OCLSpecialPower"));
	REQUIRE(sp);
	sp->setReadyFrame(0);
	const unsigned long long unportedBefore = sp->unportedEffects();
	sp->doSpecialPowerAtLocation(Coord3D{ 90.0f, 90.0f, 0.0f }, 0);
	CHECK(sp->unportedEffects() == unportedBefore + 1);
	REQUIRE(sp->created().size() == 1);
	Object *summoned = w.logic->findObjectByID(sp->created().front());
	REQUIRE(summoned);
	CHECK(summoned->getAIUpdateInterface()->currentStateId() == (unsigned)AI_MOVE_TO);

	w.logic->setTerrain(nullptr);
}

TEST_CASE("PORT-237 CreateObjectDie (S-980 closed) and AODCrushCollide (S-6008 closed) forward secondary object")
{
	Port237World w;

	// CreateObjectDie passes victim as primary and killer as secondary to OCL::create (FireWeapon damages killer)
	Object *victim = w.unit("Port237DieVictim", 'A', 50.0f, 50.0f);
	Object *killer = w.unit("Port237Unit", 'B', 55.0f, 50.0f);
	REQUIRE(victim);
	REQUIRE(killer);
	const float killerHpBefore = killer->getBodyModule()->getHealth();
	DamageInfo dmg;
	dmg.m_input.m_sourceID = killer->getID();
	dmg.m_input.m_amount = 100.0f;
	victim->attemptDamage(dmg);
	CHECK(victim->isEffectivelyDead());
	CHECK(killer->getBodyModule()->getHealth() == killerHpBefore - 10.0f);
	CHECK_FALSE(hasStop(*w.logic, "[S-980]"));

	// AODCrushCollide passes crushed other as primary and crusher self as secondary without emitting S-6008
	Object *crusher = w.unit("Port237Crusher", 'A', 80.0f, 80.0f);
	Object *target = w.unit("Port237Unit", 'B', 82.0f, 80.0f);
	REQUIRE(crusher);
	REQUIRE(target);
	AODCrushCollide *crush = dynamic_cast<AODCrushCollide *>(crusher->findModule("AODCrushCollide"));
	REQUIRE(crush);
	const size_t objsBefore = w.logic->getObjectCount();
	crush->onCollide(target, nullptr, nullptr);
	CHECK(w.logic->getObjectCount() == objsBefore + 1);
	CHECK_FALSE(hasStop(*w.logic, "[S-6008]"));
}

TEST_CASE("PORT-237 BattlePlan bonuses (S-1712): RW 0x6AD2B5 apply/remove, ActiveBody::applyDamageScalar, initObject, setTeam, and snapshot")
{
	Port237World w;
	Player *pA = w.playerOf('A');
	Player *pB = w.playerOf('B');
	REQUIRE(pA);
	REQUIRE(pB);

	BattlePlanBonuses bonus;
	bonus.armorScalar = 0.5f;
	bonus.bombardment = 1;
	bonus.sightRangeScalar = 1.5f;
	const int infBit = ObjectTemplateInfoBuilder::kindOfIndex("INFANTRY");
	REQUIRE(infBit != -1);
	bonus.validKindOf[(size_t)infBit >> 5] |= (1u << (infBit & 31));

	Object *existing = w.unit("Port237Unit", 'A', 40.0f, 40.0f);
	REQUIRE(existing);
	pA->setBattlePlanCounts(1, 0, 0);
	pA->applyBattlePlanBonusesForPlayerObjects(bonus, *w.logic);

	auto *bodyExisting = dynamic_cast<ActiveBody *>(existing->getBodyModule());
	REQUIRE(bodyExisting);
	CHECK(bodyExisting->bodyDamageScalar() == 0.5f);
	CHECK(existing->getBaseShroudClearingRange() == 150.0f);
	CHECK((existing->weaponBonusConditionMask() & (1u << 12)) == (1u << 12));

	// New object constructed for player A gets BattlePlan bonuses during friend_initObject
	Object *spawned = w.unit("Port237Unit", 'A', 60.0f, 60.0f);
	REQUIRE(spawned);
	auto *bodySpawned = dynamic_cast<ActiveBody *>(spawned->getBodyModule());
	REQUIRE(bodySpawned);
	CHECK(bodySpawned->bodyDamageScalar() == 0.5f);
	CHECK(spawned->getBaseShroudClearingRange() == 150.0f);
	CHECK((spawned->weaponBonusConditionMask() & (1u << 12)) == (1u << 12));

	// Snapshot round-trip preserves BattlePlanBonuses and scalars
	std::vector<std::uint8_t> stream;
	std::string err;
	REQUIRE_MESSAGE(w.logic->saveSnapshot(stream, &err), err);
	const std::uint32_t savedHash = w.hash();
	Port237World loaded;
	REQUIRE_MESSAGE(loaded.logic->loadSnapshot(stream, &err), err);
	CHECK(loaded.hash() == savedHash);
	CHECK(loaded.playerOf('A')->hasActiveBattlePlans());
	CHECK(loaded.playerOf('A')->battlePlanBonuses().armorScalar == 0.5f);

	// Team change from A to B removes player A's BattlePlan bonuses
	spawned->setTeam(pB->getDefaultTeam());
	CHECK(bodySpawned->bodyDamageScalar() == 1.0f);
	CHECK(spawned->getBaseShroudClearingRange() == 100.0f);
	CHECK((spawned->weaponBonusConditionMask() & (1u << 12)) == 0u);
}
