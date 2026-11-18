// OpenBFME tests for Port 254 (issue #602): Auras and passive buffs —
// AttributeModifierAuraUpdate, AutoHealBehavior, LargeGroupBonusUpdate, BloodthirstyUpdate.
//
// Synthetic INI only — no retail game files are required. GPL-3.0.
//
// RotWK 2.01 RW references verified from workspace/RotWK/game.dat:
//   * AttributeModifierAuraUpdate:
//       RW 0x89F459 / 0x89F1CB  ModelCondition STONED (MC 0x81, Object + 0x11C bit 1) skips pulse & target
//       RW 0x89F588 / 0x660E93  PartitionFilterSameMapStatus (Object + 0x458 bit 3 = isOffMap())
//       RW 0x89F4C7 / 0x89F63C / 0x89F1DB  WALK_ON_TOP_OF_WALL uses boundingCircleRadius (+0xB8) and requires
//                               target layer in [LAYER_WALL_FIRST (0x11), LAYER_WALL_LAST (0x40)] (RW 0x68BBE0)
//       RW 0x9188EB             UpgradeMux vt+0x20 slot +0x1C isPermanent() returns false
//   * AutoHealBehavior:
//       RW 0x85544E             UpgradeMux vt+0x20 slot +0x20 processUpgradeRemoval() clears m_nextHealFrame (+0x2C)
//                               and sets upgrade unexecuted (slot +0x24 = RW 0x5B462D)
//       RW 0x9188EB             UpgradeMux vt+0x20 slot +0x1C isPermanent() returns false
//       RW 0x855466 / 0x862758  xfer (ver 1..2) & getDamage()
//   * LargeGroupBonusUpdate:
//       RW 0x89397E / 0x660E93  PartitionFilterSameMapStatus (Object + 0x458 bit 3 = isOffMap())
//       RW 0x734403 / 0x8936C9  active() (vt+0x20 slot +0x00) & xfer (ver 1..2)
//   * BloodthirstyUpdate:
//       RW 0x851129 / 0x85121A / 0x85128B  direct Object::setStatus (RW 0x62684D) alongside setStatusAcrossHorde
//                               (RW 0x748650) for IGNORE_AI_COMMAND (0x4A)
//       RW 0x850DF9 / 0x850E83 / 0x851003 / 0x8511B5 / 0x850CD0  canSacrifice, isValidSacrifice, initiateVoice,
//                               cancelSacrifice, xfer

#include "doctest.h"

#include "MoveTestUtil.h"

#include "Common/Player.h"
#include "Common/XferLoad.h"
#include "Common/XferSave.h"
#include "GameEngineDevice/W3DDevice/GameClient/WorldHeightMap.h"
#include "GameLogic/AI/AIPathfindHost.h"
#include "GameLogic/AI/AIWorld.h"
#include "GameLogic/AttributeModifiers.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/Damage.h"
#include "GameLogic/Map/TerrainLogic.h"
#include "GameLogic/Module/ActiveBody.h"
#include "GameLogic/Module/AreaScanModules.h"
#include "GameLogic/Module/BloodthirstyUpdate.h"
#include "GameLogic/Module/HeroAbilityModules.h"
#include "GameLogic/Module/HeroModules.h"
#include "GameLogic/Object/AttributeModifierPool.h"
#include "GameLogic/Object/Contain/HordeContainRuntime.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/Object/PartitionManager.h"

#include <cstdint>
#include <string>
#include <vector>

using namespace movetest;

namespace
{
const char kPort254Ini[] =
	"Upgrade Upgrade_P254_Aura\n"
	"  Type = OBJECT\n"
	"End\n"
	"Upgrade Upgrade_P254_Heal\n"
	"  Type = OBJECT\n"
	"End\n"
	"ModifierList P254Leadership\n"
	"  Category = LEADERSHIP\n"
	"  Duration = 2000\n"
	"  Modifier = DAMAGE_MULT 150%\n"
	"End\n"
	"ModifierList P254WallBonus\n"
	"  Category = LEADERSHIP\n"
	"  Duration = 2000\n"
	"  Modifier = ARMOR 50%\n"
	"End\n"
	"ModifierList P254EnemyDebuff\n"
	"  Category = DEBUFF\n"
	"  Duration = 2000\n"
	"  Modifier = DAMAGE_MULT 75%\n"
	"End\n"
	"ModifierList P254LargeGroup\n"
	"  Category = FORMATION\n"
	"  Duration = 0\n"
	"  Modifier = DAMAGE_MULT 125%\n"
	"End\n"
	"Object P254AuraSource\n"
	"  KindOf = SELECTABLE INFANTRY HERO\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = AttributeModifierPoolUpdate ModuleTag_Pool\n"
	"  End\n"
	"  Behavior = AttributeModifierAuraUpdate ModuleTag_Aura\n"
	"    StartsActive = Yes\n"
	"    BonusName = P254Leadership\n"
	"    RefreshDelay = 1000\n"
	"    Range = 200\n"
	"    ObjectFilter = ANY +INFANTRY\n"
	"  End\n"
	"End\n"
	"Object P254PermAuraSource\n"
	"  KindOf = SELECTABLE INFANTRY HERO\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = AttributeModifierPoolUpdate ModuleTag_Pool\n"
	"  End\n"
	"  Behavior = AttributeModifierAuraUpdate ModuleTag_Aura\n"
	"    StartsActive = No\n"
	"    TriggeredBy = Upgrade_P254_Aura\n"
	"    Permanent = Yes\n"
	"    BonusName = P254Leadership\n"
	"    RefreshDelay = 1000\n"
	"    Range = 200\n"
	"    AllowSelf = Yes\n"
	"    ObjectFilter = ANY +INFANTRY\n"
	"  End\n"
	"End\n"
	"Object P254EnemyAuraSource\n"
	"  KindOf = SELECTABLE INFANTRY HERO\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = AttributeModifierPoolUpdate ModuleTag_Pool\n"
	"  End\n"
	"  Behavior = AttributeModifierAuraUpdate ModuleTag_Aura\n"
	"    StartsActive = Yes\n"
	"    BonusName = P254EnemyDebuff\n"
	"    RefreshDelay = 1000\n"
	"    Range = 150\n"
	"    TargetEnemy = Yes\n"
	"    ObjectFilter = ANY +INFANTRY\n"
	"  End\n"
	"End\n"
	"Object P254WallSegment\n"
	"  KindOf = STRUCTURE IMMOBILE WALK_ON_TOP_OF_WALL\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 60\n"
	"  GeometryMinorRadius = 60\n"
	"  GeometryHeight = 40\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"  Behavior = AttributeModifierPoolUpdate ModuleTag_Pool\n"
	"  End\n"
	"  Behavior = AttributeModifierAuraUpdate ModuleTag_WallAura\n"
	"    StartsActive = Yes\n"
	"    BonusName = P254WallBonus\n"
	"    RefreshDelay = 1000\n"
	"  End\n"
	"End\n"
	"Object P254Footman\n"
	"  KindOf = SELECTABLE INFANTRY\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"  Behavior = AttributeModifierPoolUpdate ModuleTag_Pool\n"
	"  End\n"
	"End\n"
	"Object P254AutoHealUnit\n"
	"  KindOf = SELECTABLE INFANTRY\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = AutoHealBehavior ModuleTag_Heal\n"
	"    StartsActive = No\n"
	"    TriggeredBy = Upgrade_P254_Heal\n"
	"    Permanent = Yes\n"
	"    HealingAmount = 50\n"
	"    HealingDelay = 10000\n"
	"    StartHealingDelay = 0\n"
	"  End\n"
	"End\n"
	"Object P254AutoHealDelayedUnit\n"
	"  KindOf = SELECTABLE INFANTRY\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = AutoHealBehavior ModuleTag_Heal\n"
	"    StartsActive = Yes\n"
	"    HealingAmount = 20\n"
	"    HealingDelay = 1000\n"
	"    StartHealingDelay = 1000\n"
	"    HealOnlyIfNotUnderAttack = Yes\n"
	"  End\n"
	"End\n"
	"Object P254OrcSoldier\n"
	"  KindOf = SELECTABLE INFANTRY\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 5\n"
	"  GeometryMinorRadius = 5\n"
	"  GeometryHeight = 15\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"  Behavior = AttributeModifierPoolUpdate ModuleTag_Pool\n"
	"  End\n"
	"End\n"
	"Object P254OrcHorde\n"
	"  KindOf = SELECTABLE HORDE INFANTRY\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 15\n"
	"  GeometryMinorRadius = 15\n"
	"  GeometryHeight = 15\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = HordeAIUpdate ModuleTag_AI\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = HordeLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"  Behavior = HordeContain ModuleTag_Horde\n"
	"    ObjectStatusOfContained = \n"
	"    InitialPayload = P254OrcSoldier 3\n"
	"    Slots = 3\n"
	"    PassengerFilter = NONE +INFANTRY\n"
	"    ShowPips = No\n"
	"    RankInfo = RankNumber:1 UnitType:P254OrcSoldier Position:X:0 Y:-10 Position:X:0 Y:0 Position:X:0 Y:10\n"
	"    RanksToReleaseWhenAttacking = 1\n"
	"  End\n"
	"  Behavior = AttributeModifierPoolUpdate ModuleTag_Pool\n"
	"  End\n"
	"  Behavior = LargeGroupBonusUpdate ModuleTag_LGB\n"
	"    UpdateRate = 1000\n"
	"    HordeMemberFilter = NONE +P254OrcSoldier\n"
	"    Count = 10\n"
	"    Radius = 160.0\n"
	"    RubOffRadius = 160.0\n"
	"    AlliesOnly = Yes\n"
	"    AttributeModifier = P254LargeGroup\n"
	"  End\n"
	"  Behavior = BloodthirstyUpdate ModuleTag_BT\n"
	"    SacrificeFilter = ALL\n"
	"    ExperienceModifier = 2.0\n"
	"    InitiateVoice = UrukVoiceBloodthirsty\n"
	"  End\n"
	"End\n"
	"Object P254LooseHordeBT\n"
	"  KindOf = SELECTABLE HORDE INFANTRY\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"  Behavior = BloodthirstyUpdate ModuleTag_BT\n"
	"    SacrificeFilter = ALL\n"
	"    ExperienceModifier = 1.5\n"
	"    InitiateVoice = LooseVoiceBloodthirsty\n"
	"  End\n"
	"End\n";

struct Port254World : MoveWorld
{
	AttributeModifierStore modifiers;
	AttributeModifierStore *savedModifiers = TheAttributeModifierStore;
	WorldHeightMap heightMap;
	MapChunks chunks;
	TerrainLogic boundaryTerrain;

	Port254World() : MoveWorld(100, 100, "")
	{
		TheAttributeModifierStore = &modifiers;
		modifiers.registerBlock(w.fx.env.blocks);
		HeroModules::registerAll(w.modules);
		HeroAbilityModules::registerAll(w.modules);
		AreaScanModules::registerAll(w.modules);
		const std::string err = w.load(kPort254Ini, INI_LOAD_OVERWRITE, "port254.ini");
		REQUIRE_MESSAGE(err.empty(), err);

		heightMap.m_width = 100;
		heightMap.m_height = 100;
		heightMap.m_data.assign(100u * 100u, 0);
		// Active boundary 0 -> valid map region (0, 0) .. (1000, 1000).
		heightMap.m_boundaries = { ICoord2D{ 100, 100 } };
		boundaryTerrain.init(heightMap, chunks);
		logic->setTerrain(&boundaryTerrain);
		logic->partition().setRegion(-500.0f, -500.0f, 1500.0f, 1500.0f);
		buildMap();
	}

	~Port254World()
	{
		logic->reset();
		ai.reset();
		logic->setTerrain(nullptr);
		TheAttributeModifierStore = savedModifiers;
	}
};

bool hasList(const Object *o, const char *name)
{
	if (!o)
	{
		return false;
	}
	if (const auto *pool = dynamic_cast<const AttributeModifierPool *>(o->attributeModifierPoolModule()))
	{
		return pool->hasList(name);
	}
	return false;
}

void hurt(Object *o, float amount)
{
	DamageInfo info;
	info.m_input.m_amount = amount;
	info.m_input.m_damageType = DAMAGE_SLASH;
	info.m_input.m_damageSubType = 0;
	o->attemptDamage(info);
}

} // namespace

TEST_CASE("PORT-254-1 AttributeModifierAuraUpdate STONED skips pulse and target (RW 0x89F459 / 0x89F1CB)")
{
	Port254World w;
	const int kStoned = CombatNames::modelCondition("STONED");
	REQUIRE(kStoned == 0x81);

	Object *source = w.spawn("P254AuraSource", 300.0f, 300.0f);
	Object *allyOk = w.spawn("P254Footman", 340.0f, 300.0f);
	Object *allyStoned = w.spawn("P254Footman", 350.0f, 300.0f);
	auto *aura = dynamic_cast<AttributeModifierAuraUpdate *>(source->findModule("AttributeModifierAuraUpdate"));
	REQUIRE(aura);

	// Source STONED: no pulse runs while petrified; resumes once STONED clears.
	source->setModelConditionState(kStoned, true);
	allyStoned->setModelConditionState(kStoned, true);
	w.frames(12);
	CHECK(aura->pulses() == 0);
	CHECK_FALSE(hasList(allyOk, "P254Leadership"));
	CHECK_FALSE(hasList(allyStoned, "P254Leadership"));

	// Clear STONED on source while allyStoned remains STONED: allyOk gets buff, allyStoned does not.
	source->setModelConditionState(kStoned, false);
	w.frames(15);
	CHECK(aura->pulses() == 3);
	CHECK(hasList(allyOk, "P254Leadership"));
	CHECK_FALSE(hasList(allyStoned, "P254Leadership"));

	// Clear STONED on allyStoned: next pulse buffs it too.
	allyStoned->setModelConditionState(kStoned, false);
	w.frames(15);
	CHECK(hasList(allyStoned, "P254Leadership"));
}

TEST_CASE("PORT-254-2 AttributeModifierAuraUpdate PartitionFilterSameMapStatus rejects off-map mismatch (RW 0x89F588 / 0x660E93)")
{
	Port254World w;
	Object *source = w.spawn("P254AuraSource", 50.0f, 50.0f);
	Object *onMapAlly = w.spawn("P254Footman", 80.0f, 50.0f);
	Object *offMapAlly = w.spawn("P254Footman", -20.0f, 50.0f); // distance 70 < Range 200, but x <= 0 -> off-map
	REQUIRE_FALSE(source->isOffMap());
	REQUIRE_FALSE(onMapAlly->isOffMap());
	REQUIRE(offMapAlly->isOffMap());

	w.frames(12);
	CHECK(hasList(onMapAlly, "P254Leadership"));
	CHECK_FALSE(hasList(offMapAlly, "P254Leadership"));

	// Move ally onto the map: next pulse applies the aura.
	Coord3D inside{ 20.0f, 50.0f, 0.0f };
	offMapAlly->setPosition(&inside);
	REQUIRE_FALSE(offMapAlly->isOffMap());
	w.frames(15);
	CHECK(hasList(offMapAlly, "P254Leadership"));
}

TEST_CASE("PORT-254-3 AttributeModifierAuraUpdate WALK_ON_TOP_OF_WALL uses bounding radius and wall layer (RW 0x89F4C7 / 0x89F63C / 0x89F1DB)")
{
	struct WallMeshes : PathfindWallMeshSource
	{
		bool wallGeometry(const Object &o, PathfindWallGeometry &out) override
		{
			if (o.getTemplate()->getName() != "P254WallSegment")
			{
				return false;
			}
			PathfindWallSurface s;
			PathfindWallTriangle a, b;
			a.v[0] = { 330.0f, 360.0f, 40.0f };
			a.v[1] = { 500.0f, 360.0f, 40.0f };
			a.v[2] = { 500.0f, 440.0f, 40.0f };
			b.v[0] = { 330.0f, 360.0f, 40.0f };
			b.v[1] = { 500.0f, 440.0f, 40.0f };
			b.v[2] = { 330.0f, 440.0f, 40.0f };
			s.triangles = { a, b };
			s.loX = 330.0f;
			s.hiX = 500.0f;
			s.loY = 360.0f;
			s.hiY = 440.0f;
			s.height = 40.0f;
			out.walls.push_back(s);
			return true;
		}
	};

	WallMeshes meshes;
	Port254World w;
	w.ai->setWallMeshes(&meshes);
	Object *wall = w.spawn("P254WallSegment", 400.0f, 400.0f); // GeometryMajorRadius = 60, Range = 0
	(void)wall;
	Object *onWall = w.spawn("P254Footman", 440.0f, 400.0f);     // dist 40 <= 60, on wall deck
	Object *onGround = w.spawn("P254Footman", 400.0f, 345.0f);   // dist 55 <= 60, off deck (ground layer)
	Object *farOnWall = w.spawn("P254Footman", 480.0f, 400.0f);  // dist 80 > 60, on wall deck
	Object *enemyOnWall = w.spawn("P254Footman", 430.0f, 400.0f, 0.0f, w.teamOf("Bob")); // dist 30 <= 60, enemy on wall
	Coord3D pOn{ 440.0f, 400.0f, 40.0f };
	Coord3D pFar{ 480.0f, 400.0f, 40.0f };
	Coord3D pEnemy{ 430.0f, 400.0f, 40.0f };
	onWall->setPosition(&pOn);
	farOnWall->setPosition(&pFar);
	enemyOnWall->setPosition(&pEnemy);
	w.buildMap();
	w.ai->adapterFor(*onWall).setLayer(LAYER_WALL_FIRST);
	w.ai->adapterFor(*farOnWall).setLayer(LAYER_WALL_FIRST);
	w.ai->adapterFor(*enemyOnWall).setLayer(LAYER_WALL_FIRST);

	w.frames(12);
	REQUIRE(isWallLayer(AIWorld::objectLayer(*onWall)));
	REQUIRE(isWallLayer(AIWorld::objectLayer(*farOnWall)));
	REQUIRE(isWallLayer(AIWorld::objectLayer(*enemyOnWall)));
	REQUIRE(AIWorld::objectLayer(*onGround) == LAYER_GROUND);
	CHECK(hasList(onWall, "P254WallBonus"));
	CHECK(hasList(enemyOnWall, "P254WallBonus")); // WALK_ON_TOP_OF_WALL skips ALLIES filter (RW 0x89F61F)
	CHECK_FALSE(hasList(onGround, "P254WallBonus"));
	CHECK_FALSE(hasList(farOnWall, "P254WallBonus"));
	w.ai->setWallMeshes(nullptr);
}

TEST_CASE("PORT-254-4 AttributeModifierAuraUpdate and AutoHealBehavior isPermanent returns false (RW 0x9188EB)")
{
	Port254World w;
	Object *auraObj = w.spawn("P254PermAuraSource", 300.0f, 300.0f);
	Object *healObj = w.spawn("P254AutoHealUnit", 350.0f, 300.0f);
	auto *aura = dynamic_cast<AttributeModifierAuraUpdate *>(auraObj->findModule("AttributeModifierAuraUpdate"));
	auto *heal = dynamic_cast<AutoHealBehavior *>(healObj->findModule("AutoHealBehavior"));
	REQUIRE(aura);
	REQUIRE(heal);

	CHECK_FALSE(aura->isPermanent());
	CHECK_FALSE(heal->isPermanent());

	auraObj->giveUpgrade("Upgrade_P254_Aura");
	healObj->giveUpgrade("Upgrade_P254_Heal");
	REQUIRE(aura->isAlreadyUpgraded());
	REQUIRE(heal->isAlreadyUpgraded());

	// Even though Permanent = Yes in INI, RW 0x9188EB returns false so removeUpgrade resets both.
	const UpgradeTemplate *upAura = TheUpgradeCenter->findUpgrade("Upgrade_P254_Aura");
	const UpgradeTemplate *upHeal = TheUpgradeCenter->findUpgrade("Upgrade_P254_Heal");
	REQUIRE(upAura);
	REQUIRE(upHeal);
	auraObj->removeUpgrade(upAura);
	healObj->removeUpgrade(upHeal);
	CHECK_FALSE(aura->isAlreadyUpgraded());
	CHECK_FALSE(heal->isAlreadyUpgraded());
}

TEST_CASE("PORT-254-5 AutoHealBehavior processUpgradeRemoval clears m_nextHealFrame (RW 0x85544E)")
{
	Port254World w;
	Object *u = w.spawn("P254AutoHealUnit", 300.0f, 300.0f);
	auto *heal = dynamic_cast<AutoHealBehavior *>(u->findModule("AutoHealBehavior"));
	REQUIRE(heal);

	// Take 30 damage, grant upgrade -> heals back to 100 and sets m_nextHealFrame = frame + 50 (HealingDelay 10000 ms).
	hurt(u, 30.0f);
	REQUIRE(u->getBodyModule()->getHealth() == 70.0f);
	u->giveUpgrade("Upgrade_P254_Heal");
	w.frames(2);
	REQUIRE(u->getBodyModule()->getHealth() == 100.0f);
	const unsigned healsBefore = heal->heals();
	REQUIRE(healsBefore == 1);

	// Remove upgrade (RW 0x85544E must zero m_nextHealFrame) and re-grant while still at full health.
	// update() sees full health and returns UPDATE_SLEEP_FOREVER without rewriting m_nextHealFrame.
	const UpgradeTemplate *upHeal = TheUpgradeCenter->findUpgrade("Upgrade_P254_Heal");
	REQUIRE(upHeal);
	u->removeUpgrade(upHeal);
	REQUIRE_FALSE(heal->isAlreadyUpgraded());
	u->giveUpgrade("Upgrade_P254_Heal");
	w.frames(2);
	REQUIRE(u->getBodyModule()->getHealth() == 100.0f);

	// Now take damage well before frame + 50: onDamage (RW 0x855706) checks frame > m_nextHealFrame.
	// Without RW 0x85544E clearing m_nextHealFrame, m_nextHealFrame is still ~51 so onDamage never wakes the heal!
	hurt(u, 25.0f);
	REQUIRE(u->getBodyModule()->getHealth() == 75.0f);
	w.frames(3);
	CHECK(heal->heals() == healsBefore + 1);
	CHECK(u->getBodyModule()->getHealth() == 100.0f);
}

TEST_CASE("PORT-254-6 LargeGroupBonusUpdate PartitionFilterSameMapStatus ignores off-map hordes (RW 0x89397E / 0x660E93)")
{
	Port254World w;
	// Count = 10 (threshold Count - 1 = 9 members). Each horde has 3 members.
	// Three off-map hordes at x = -20..-40 (distance 60..80 <= Radius 160 from onMap) reach 9 members among themselves,
	// but must NOT count toward or rub off onto the on-map horde at (40, 300).
	Object *onMap = w.spawn("P254OrcHorde", 40.0f, 300.0f);
	Object *offMap1 = w.spawn("P254OrcHorde", -20.0f, 300.0f);
	Object *offMap2 = w.spawn("P254OrcHorde", -30.0f, 300.0f);
	Object *offMap3 = w.spawn("P254OrcHorde", -40.0f, 300.0f);
	REQUIRE_FALSE(onMap->isOffMap());
	REQUIRE(offMap1->isOffMap());
	REQUIRE(offMap2->isOffMap());
	REQUIRE(offMap3->isOffMap());

	auto *lgbOn = dynamic_cast<LargeGroupBonusUpdate *>(onMap->findModule("LargeGroupBonusUpdate"));
	auto *lgbOff1 = dynamic_cast<LargeGroupBonusUpdate *>(offMap1->findModule("LargeGroupBonusUpdate"));
	REQUIRE(lgbOn);
	REQUIRE(lgbOff1);

	w.frames(12);
	CHECK(lgbOff1->active());
	CHECK(lgbOff1->reached());
	CHECK(lgbOn->lastCount() == 3u);
	CHECK_FALSE(lgbOn->active());
	CHECK_FALSE(hasList(onMap, "P254LargeGroup"));
}

TEST_CASE("PORT-254-7 BloodthirstyUpdate sets and clears IGNORE_AI_COMMAND directly on contain-less victim (RW 0x851129 / 0x85121A / 0x85128B)")
{
	Port254World w;
	const unsigned kIgnoreAI = (unsigned)CombatNames::status("IGNORE_AI_COMMAND");
	const unsigned kBloodthirsty = (unsigned)CombatNames::status("BLOODTHIRSTY");

	Object *sacrificer = w.spawn("P254OrcHorde", 300.0f, 300.0f);
	Object *victim = w.spawn("P254LooseHordeBT", 360.0f, 300.0f); // HORDE KindOf, no ContainModule
	auto *btSrc = BloodthirstyUpdate::find(*sacrificer);
	auto *btVic = BloodthirstyUpdate::find(*victim);
	REQUIRE(btSrc);
	REQUIRE(btVic);
	REQUIRE(victim->getContain() == nullptr);

	btSrc->sacrificeHorde(victim, CMD_FROM_PLAYER);
	CHECK(sacrificer->testStatus(kBloodthirsty));
	CHECK(victim->testStatus(kIgnoreAI));
	CHECK(btSrc->bestTargetID() == victim->getID());
	CHECK(btVic->sacrificerID() == sacrificer->getID());

	// cancelSacrifice clears IGNORE_AI_COMMAND directly (RW 0x85121A)
	btSrc->cancelSacrifice();
	CHECK_FALSE(sacrificer->testStatus(kBloodthirsty));
	CHECK_FALSE(victim->testStatus(kIgnoreAI));
	CHECK(btSrc->bestTargetID() == INVALID_ID);
	CHECK(btVic->sacrificerID() == INVALID_ID);

	// Re-start sacrifice, then kill sacrificer so victim's update() (RW 0x85123F -> 0x85128B) clears IGNORE_AI_COMMAND directly.
	btSrc->sacrificeHorde(victim, CMD_FROM_PLAYER);
	REQUIRE(victim->testStatus(kIgnoreAI));
	sacrificer->setStatus(kBloodthirsty, false);
	w.frames(15); // BloodthirstyUpdate sleeps 2*5+1 = 11 frames
	CHECK_FALSE(victim->testStatus(kIgnoreAI));
	CHECK(btVic->sacrificerID() == INVALID_ID);
}

TEST_CASE("PORT-254-8 AttributeModifierAuraUpdate neighbor: TargetEnemy, AllowSelf and RefreshDelay period")
{
	Port254World w;
	Object *enemyAura = w.spawn("P254EnemyAuraSource", 300.0f, 300.0f, 0.0f, w.teamOf("Alice"));
	Object *ally = w.spawn("P254Footman", 340.0f, 300.0f, 0.0f, w.teamOf("Alice"));
	Object *foe = w.spawn("P254Footman", 350.0f, 300.0f, 0.0f, w.teamOf("Bob"));

	w.frames(10);
	CHECK(hasList(foe, "P254EnemyDebuff"));
	CHECK_FALSE(hasList(ally, "P254EnemyDebuff"));
	CHECK_FALSE(hasList(enemyAura, "P254EnemyDebuff"));
}

TEST_CASE("PORT-254-9 AutoHealBehavior neighbor: StartHealingDelay, HealOnlyIfNotUnderAttack and xfer (RW 0x855466)")
{
	Port254World w;
	Object *u = w.spawn("P254AutoHealDelayedUnit", 300.0f, 300.0f);
	auto *heal = dynamic_cast<AutoHealBehavior *>(u->findModule("AutoHealBehavior"));
	REQUIRE(heal);
	REQUIRE(heal->getDamage() == heal);

	hurt(u, 40.0f);
	REQUIRE(u->getBodyModule()->getHealth() == 60.0f);
	// StartHealingDelay = 1000 ms (5 frames): does not heal on frame 2, heals after delay passes.
	w.frames(2);
	CHECK(u->getBodyModule()->getHealth() == 60.0f);
	w.frames(8);
	CHECK(u->getBodyModule()->getHealth() == 100.0f);

	// Xfer round-trip (RW 0x855466)
	XferSave save;
	heal->xfer(save);
	XferLoad load(save.data());
	heal->xfer(load);
	CHECK(load.atEnd());
}

TEST_CASE("PORT-254-10 LargeGroupBonusUpdate neighbor: Count-1 threshold, RubOffRadius, active() and xfer (RW 0x734403 / 0x8936C9)")
{
	Port254World w;
	// Count = 10 -> threshold = 9 members (3 hordes of 3).
	// h1..h3 within Radius 160 reach 9 members -> reached() == true, active() == true.
	// outrider at x = 395 sees only itself + h3 (dist 145 <= 160; h2 is 285 away) -> count = 6 < 9, gets active() via rub-off from h3!
	Object *h1 = w.spawn("P254OrcHorde", 100.0f, 300.0f);
	Object *h2 = w.spawn("P254OrcHorde", 110.0f, 300.0f);
	Object *h3 = w.spawn("P254OrcHorde", 250.0f, 300.0f);
	Object *outrider = w.spawn("P254OrcHorde", 395.0f, 300.0f);
	Object *alone = w.spawn("P254OrcHorde", 850.0f, 850.0f);
	(void)h2;

	auto *lgb1 = dynamic_cast<LargeGroupBonusUpdate *>(h1->findModule("LargeGroupBonusUpdate"));
	auto *lgb3 = dynamic_cast<LargeGroupBonusUpdate *>(h3->findModule("LargeGroupBonusUpdate"));
	auto *lgbOut = dynamic_cast<LargeGroupBonusUpdate *>(outrider->findModule("LargeGroupBonusUpdate"));
	auto *lgbAlone = dynamic_cast<LargeGroupBonusUpdate *>(alone->findModule("LargeGroupBonusUpdate"));
	REQUIRE(lgb1);
	REQUIRE(lgb3);
	REQUIRE(lgbOut);
	REQUIRE(lgbAlone);

	w.frames(12);
	CHECK(lgb1->active());
	CHECK(lgb1->reached());
	CHECK(hasList(h1, "P254LargeGroup"));
	CHECK(lgb3->active());
	CHECK(lgb3->reached());
	CHECK(lgbOut->active());
	CHECK_FALSE(lgbOut->reached());
	CHECK(hasList(outrider, "P254LargeGroup"));
	CHECK_FALSE(lgbAlone->active());
	CHECK_FALSE(hasList(alone, "P254LargeGroup"));

	// Xfer round-trip (RW 0x8936C9)
	XferSave save;
	lgb1->xfer(save);
	XferLoad load(save.data());
	lgb1->xfer(load);
	CHECK(load.atEnd());
	CHECK(lgb1->active());
	CHECK(lgb1->reached());
}

TEST_CASE("PORT-254-11 BloodthirstyUpdate neighbor: canSacrifice, isValidSacrifice, initiateVoice, cancelSacrifice and xfer (RW 0x850DF9 / 0x850E83 / 0x851003 / 0x8511B5 / 0x850CD0)")
{
	Port254World w;
	Object *h1 = w.spawn("P254OrcHorde", 300.0f, 300.0f);
	Object *h2 = w.spawn("P254OrcHorde", 360.0f, 300.0f);
	auto *bt1 = BloodthirstyUpdate::find(*h1);
	auto *bt2 = BloodthirstyUpdate::find(*h2);
	REQUIRE(bt1);
	REQUIRE(bt2);

	CHECK(bt1->initiateVoice() == "UrukVoiceBloodthirsty");
	CHECK(bt1->canSacrifice(h2));
	CHECK_FALSE(bt1->isValidSacrifice(h2));

	bt1->sacrificeHorde(h2, CMD_FROM_PLAYER);
	CHECK(bt1->isValidSacrifice(h2));
	CHECK(bt1->bestTargetID() == h2->getID());
	CHECK(bt2->sacrificerID() == h1->getID());

	// Xfer round-trip (RW 0x850CD0)
	XferSave save;
	bt1->xfer(save);
	XferLoad load(save.data());
	bt1->xfer(load);
	CHECK(load.atEnd());
	CHECK(bt1->bestTargetID() == h2->getID());

	bt1->cancelSacrifice();
	CHECK_FALSE(bt1->isValidSacrifice(h2));
	CHECK(bt1->bestTargetID() == INVALID_ID);
	CHECK(bt2->sacrificerID() == INVALID_ID);
}
