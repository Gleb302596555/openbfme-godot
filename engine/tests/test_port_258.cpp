// OpenBFME unit tests (PORT-258, issue #606): RotWK 2.01 alignment for powers on enemy units:
//   1. DominateEnemySpecialPower: area scan PartitionFilterSameMapStatus (RW 0x008D103D -> RW 0x008D10B4 / 0x00C0F374 / 0x00660E93),
//      plus AttributeModifierAffects / status / horde-redirect neighbors (RW 0x008D0F6C).
//   2. CurseSpecialPower: CursePercentage default 1.0f (RW 0x008D1192 -> RW 0x00BD1908), UNSELECTABLE status 3 instead of DESTROYED status 0
//      (RW 0x008D12E3 -> RW 0x008D130B, status table RW 0x00D8AFF0), getFinalOverride() on RadiusCursorRadius (RW 0x008D138D), plus neighbors.
//   3. DeflectSpecialPower: XFER_CRC early-out skipping m_active/m_pending (RW 0x008C99FB -> RW 0x008C9A0D .. 0x008C9A12),
//      plus XferSave/XferLoad round-trip and getCollide()/onCollide() neighbor (RW 0x008C993F, 0x008C9A3C).
//   4. SpecialPowerTimerRefreshSpecialPower: SpecialPowerModule::refreshTimer via +0xC slot 0xB0 (RW 0x008C7B26 / 0x0068BDF9 / 0x00896F99),
//      skipping SharedSyncedTimer and non-SpecialPowerModule updates, plus xfer (RW 0x008C7B43) neighbor.
// Pure synthetic INI (no retail or mod game files required). GPL-3.0.

#include "doctest.h"
#include "CombatTestUtil.h"

#include "Common/SpecialPower.h"
#include "Common/XferCRC.h"
#include "Common/XferLoad.h"
#include "Common/XferSave.h"
#include "GameClient/FXList.h"
#include "GameClient/MapChunks.h"
#include "GameEngineDevice/W3DDevice/GameClient/WorldHeightMap.h"
#include "GameLogic/AttributeModifiers.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Map/TerrainLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/DeflectSpecialPower.h"
#include "GameLogic/Module/HeroAbilityModules.h"
#include "GameLogic/Module/ProjectileModules.h"
#include "GameLogic/Module/SpecialAbilityModules.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/Object/PartitionManager.h"
#include "GameLogic/ObjectTemplateInfo.h"

#include <string>
#include <vector>

namespace
{
struct Port258World : combattest::CombatWorld
{
	SpecialPowerStore powers;
	SpecialPowerStore *savedPowers = TheSpecialPowerStore;
	FXListStore fxLists;
	FXListStore *savedFx = TheFXListStore;
	AttributeModifierStore modifiers;
	AttributeModifierStore *savedModifiers = TheAttributeModifierStore;
	WorldHeightMap heightMap;
	MapChunks mapChunks;
	TerrainLogic terrain;

	explicit Port258World(const char *ini)
	{
		TheSpecialPowerStore = &powers;
		TheFXListStore = &fxLists;
		TheAttributeModifierStore = &modifiers;
		modifiers.registerBlock(w.fx.env.blocks);
		w.fx.env.blocks.registerBlock("FXList", [](INI *in) { ParseFXListDefinitionGlobal(in); });
		w.fx.env.blocks.registerBlock("SpecialPower", [](INI *in) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(in); });
		SpecialAbilityModules::registerAll(w.modules);
		SpecialPowerModules::registerAll(w.modules);
		const std::string err = w.load(ini);
		REQUIRE_MESSAGE(err.empty(), err);

		// Synthetic flat terrain with playable boundary 0 = (0, 0) .. (1000, 1000) so Object::updateOffMapStatus() tracks on-map vs off-map.
		heightMap.m_width = 101;
		heightMap.m_height = 101;
		heightMap.m_borderSize = 0;
		heightMap.m_dataSize = 101 * 101;
		heightMap.m_data.assign((size_t)heightMap.m_dataSize, 0);
		heightMap.m_hasBlendTileData = true;
		heightMap.m_tileNdxes.assign((size_t)heightMap.m_dataSize, 0);
		heightMap.m_blendTileNdxes.assign((size_t)heightMap.m_dataSize, 0);
		heightMap.m_extraBlendTileNdxes.assign((size_t)heightMap.m_dataSize, 0);
		heightMap.m_cliffInfoNdxes.assign((size_t)heightMap.m_dataSize, 0);
		heightMap.m_planeStride = (101 + 7) / 8;
		heightMap.m_blendedTiles.assign(1, TBlendTileInfo());
		heightMap.m_cliffInfo.assign(1, TCliffInfo());
		heightMap.m_boundaries = { { 100, 100 } };
		std::vector<std::string> problems;
		terrain.init(heightMap, mapChunks, &problems);
		REQUIRE(problems.empty());
		logic->setTerrain(&terrain);
	}

	~Port258World()
	{
		logic->setTerrain(nullptr);
		TheSpecialPowerStore = savedPowers;
		TheFXListStore = savedFx;
		TheAttributeModifierStore = savedModifiers;
	}
};

SpecialAbilityUpdate *abilityOf(Object &o, const char *powerName)
{
	for (const std::unique_ptr<BehaviorModule> &m : o.modules())
	{
		if (auto *u = dynamic_cast<SpecialAbilityUpdate *>(m.get()))
		{
			if (u->abilityData()->m_specialPowerTemplate && u->abilityData()->m_specialPowerTemplate->getName() == powerName)
			{
				return u;
			}
		}
	}
	return nullptr;
}

SpecialPowerModule *powerOf(Object &o, const char *powerName)
{
	for (const std::unique_ptr<BehaviorModule> &m : o.modules())
	{
		if (auto *p = dynamic_cast<SpecialPowerModule *>(m.get()))
		{
			if (p->getSpecialPowerTemplate() && p->getSpecialPowerTemplate()->getName() == powerName)
			{
				return p;
			}
		}
	}
	return nullptr;
}

WeaponModeSpecialPowerUpdate *weaponModeOf(Object &o)
{
	for (const std::unique_ptr<BehaviorModule> &m : o.modules())
	{
		if (auto *w = dynamic_cast<WeaponModeSpecialPowerUpdate *>(m.get()))
		{
			return w;
		}
	}
	return nullptr;
}

const char kPort258DominateIni[] = R"ini(
SpecialPower SpecialAbilityP258DomArea
  Enum = SPECIAL_DOMINATE_ENEMY
  ReloadTime = 1000
End

SpecialPower SpecialAbilityP258DomFilter
  Enum = SPECIAL_DOMINATE_ENEMY
  ReloadTime = 1000
End

Object P258Dominator
  KindOf = INFANTRY SELECTABLE CAN_ATTACK HERO
  Body = ActiveBody BodyTag
    MaxHealth = 500
  End
  Behavior = AIUpdateInterface AiTag
  End
  Behavior = SpecialPowerModule AreaPowerTag
    SpecialPowerTemplate = SpecialAbilityP258DomArea
    UpdateModuleStartsAttack = Yes
  End
  Behavior = DominateEnemySpecialPower AreaDomTag
    SpecialPowerTemplate = SpecialAbilityP258DomArea
    StartAbilityRange = 300.0
    UnpackTime = 200
    PreparationTime = 1
    IgnoreFacingCheck = Yes
    AttributeModifierAffects = ALL
    DominateRadius = 80.0
    PermanentlyConvert = Yes
  End
  Behavior = SpecialPowerModule FilterPowerTag
    SpecialPowerTemplate = SpecialAbilityP258DomFilter
    UpdateModuleStartsAttack = Yes
  End
  Behavior = DominateEnemySpecialPower FilterDomTag
    SpecialPowerTemplate = SpecialAbilityP258DomFilter
    StartAbilityRange = 300.0
    UnpackTime = 200
    PreparationTime = 1
    IgnoreFacingCheck = Yes
    AttributeModifierAffects = ANY +MONSTER
    DominateRadius = 80.0
    PermanentlyConvert = Yes
  End
End

Object P258EnemyInfantry
  KindOf = INFANTRY SELECTABLE CAN_ATTACK
  Body = ActiveBody BodyTag
    MaxHealth = 100
  End
  Behavior = AIUpdateInterface AiTag
  End
End

Object P258EnemyMonster
  KindOf = MONSTER SELECTABLE CAN_ATTACK
  Body = ActiveBody BodyTag
    MaxHealth = 100
  End
  Behavior = AIUpdateInterface AiTag
  End
End
)ini";

const char kPort258CurseIni[] = R"ini(
SpecialPower SpecialAbilityP258CurseDefault
  Enum = SPECIAL_CURSE_ENEMY
  ReloadTime = 1000
  RadiusCursorRadius = 80.0
End

SpecialPower SpecialAbilityP258CurseHalf
  Enum = SPECIAL_CURSE_ENEMY
  ReloadTime = 1000
  RadiusCursorRadius = 50.0
End

SpecialPower SpecialAbilityP258VictimPower
  Enum = SPECIAL_GENERAL_TARGETLESS
  ReloadTime = 10000
End

SpecialPower SpecialAbilityP258VictimMode
  Enum = SPECIAL_GENERAL_TARGETLESS
  ReloadTime = 10000
End

Object P258CurseCaster
  KindOf = INFANTRY SELECTABLE CAN_ATTACK HERO
  Body = ActiveBody BodyTag
    MaxHealth = 500
  End
  Behavior = AIUpdateInterface AiTag
  End
  Behavior = SpecialPowerModule DefaultPowerTag
    SpecialPowerTemplate = SpecialAbilityP258CurseDefault
    UpdateModuleStartsAttack = Yes
  End
  Behavior = CurseSpecialPower DefaultCurseTag
    SpecialPowerTemplate = SpecialAbilityP258CurseDefault
    StartAbilityRange = 300.0
    UnpackTime = 200
    PreparationTime = 1
    IgnoreFacingCheck = Yes
  End
  Behavior = SpecialPowerModule HalfPowerTag
    SpecialPowerTemplate = SpecialAbilityP258CurseHalf
    UpdateModuleStartsAttack = Yes
  End
  Behavior = CurseSpecialPower HalfCurseTag
    SpecialPowerTemplate = SpecialAbilityP258CurseHalf
    StartAbilityRange = 300.0
    UnpackTime = 200
    PreparationTime = 1
    IgnoreFacingCheck = Yes
    CursePercentage = 50%
  End
End

Object P258VictimHero
  KindOf = INFANTRY SELECTABLE HERO
  Body = ActiveBody BodyTag
    MaxHealth = 500
  End
  Behavior = AIUpdateInterface AiTag
  End
  Behavior = SpecialPowerModule VictimPowerTag
    SpecialPowerTemplate = SpecialAbilityP258VictimPower
  End
  Behavior = WeaponModeSpecialPowerUpdate VictimModeTag
    SpecialPowerTemplate = SpecialAbilityP258VictimMode
    Duration = 2000
  End
End

Object P258VictimInfantry
  KindOf = INFANTRY SELECTABLE
  Body = ActiveBody BodyTag
    MaxHealth = 100
  End
  Behavior = AIUpdateInterface AiTag
  End
  Behavior = SpecialPowerModule VictimPowerTag
    SpecialPowerTemplate = SpecialAbilityP258VictimPower
  End
End
)ini";

const char kPort258DeflectIni[] = R"ini(
SpecialPower SpecialAbilityP258Deflect
  Enum = SPECIAL_DEFLECT_PROJECTILES
  ReloadTime = 0
End

Object P258DeflectSpear
  KindOf = INFANTRY SELECTABLE SCORE
  Geometry = CYLINDER
  GeometryMajorRadius = 8
  GeometryMinorRadius = 8
  GeometryHeight = 20
  Body = ActiveBody BodyTag
    MaxHealth = 100
  End
  Behavior = AIUpdateInterface AiTag
  End
  Behavior = DestroyDie DestroyTag
  End
  Behavior = DeflectSpecialPower DeflectTag
    SpecialPowerTemplate = SpecialAbilityP258Deflect
  End
End

Object P258DeflectArrow
  KindOf = PROJECTILE INERT DEFLECT_BY_SPECIAL_POWER NO_COLLIDE
  Body = ActiveBody BodyTag
    MaxHealth = 100
  End
  Behavior = DestroyDie DestroyTag
  End
  Behavior = BezierProjectileBehavior FlightTag
    FirstHeight = 9
    SecondHeight = 9
    FirstPercentIndent = 20%
    SecondPercentIndent = 90%
  End
  Geometry = Sphere
  GeometryIsSmall = Yes
  GeometryMajorRadius = 0.8
End

Object P258PlainArrow
  KindOf = PROJECTILE INERT NO_COLLIDE
  Body = ActiveBody BodyTag
    MaxHealth = 100
  End
  Behavior = DestroyDie DestroyTag
  End
  Behavior = BezierProjectileBehavior FlightTag
    FirstHeight = 9
    SecondHeight = 9
    FirstPercentIndent = 20%
    SecondPercentIndent = 90%
  End
  Geometry = Sphere
  GeometryIsSmall = Yes
  GeometryMajorRadius = 0.8
End
)ini";

const char kPort258RefreshIni[] = R"ini(
SpecialPower SpecialAbilityP258Refresh
  Enum = SPECIAL_GENERAL_TARGETLESS
  ReloadTime = 1000
End

SpecialPower SpecialAbilityP258Normal
  Enum = SPECIAL_GENERAL_TARGETLESS
  ReloadTime = 60000
End

SpecialPower SpecialAbilityP258Shared
  Enum = SPECIAL_GENERAL_TARGETLESS
  ReloadTime = 60000
  SharedSyncedTimer = Yes
End

SpecialPower SpecialAbilityP258Mode
  Enum = SPECIAL_GENERAL_TARGETLESS
  ReloadTime = 60000
End

Object P258RefreshCaster
  KindOf = INFANTRY SELECTABLE HERO
  Body = ActiveBody BodyTag
    MaxHealth = 500
  End
  Behavior = AIUpdateInterface AiTag
  End
  Behavior = SpecialPowerTimerRefreshSpecialPower RefreshTag
    SpecialPowerTemplate = SpecialAbilityP258Refresh
    AttributeModifierRange = 150.0
    AttributeModifierAffects = ALL
    AttributeModifierAffectsSelf = No
  End
End

Object P258RefreshTarget
  KindOf = INFANTRY SELECTABLE HERO
  Body = ActiveBody BodyTag
    MaxHealth = 500
  End
  Behavior = AIUpdateInterface AiTag
  End
  Behavior = SpecialPowerModule NormalPowerTag
    SpecialPowerTemplate = SpecialAbilityP258Normal
  End
  Behavior = SpecialPowerModule SharedPowerTag
    SpecialPowerTemplate = SpecialAbilityP258Shared
  End
  Behavior = WeaponModeSpecialPowerUpdate ModePowerTag
    SpecialPowerTemplate = SpecialAbilityP258Mode
    Duration = 2000
  End
End
)ini";
} // namespace

TEST_CASE("PORT-258 DominateEnemySpecialPower: area scan applies PartitionFilterSameMapStatus (RW 0x008D103D, RW 0x00C0F374 / 0x00660E93)")
{
	SUBCASE("on-map caster dominates on-map enemy and skips off-map enemy inside DominateRadius")
	{
		Port258World f(kPort258DominateIni);
		Object *caster = f.unit("P258Dominator", 'A', 40.0f, 300.0f);
		Object *onMapEnemy = f.unit("P258EnemyInfantry", 'B', 20.0f, 300.0f);
		Object *offMapEnemy = f.unit("P258EnemyInfantry", 'B', -20.0f, 300.0f);
		REQUIRE(caster != nullptr);
		REQUIRE(onMapEnemy != nullptr);
		REQUIRE(offMapEnemy != nullptr);
		REQUIRE_FALSE(caster->isOffMap());
		REQUIRE_FALSE(onMapEnemy->isOffMap());
		REQUIRE(offMapEnemy->isOffMap());

		Player *playerA = caster->getControllingPlayer();
		Player *playerB = onMapEnemy->getControllingPlayer();
		REQUIRE(playerA != playerB);

		// Confirm both victims are returned by PartitionManager within radius 80 around (0, 300) when unfiltered by SameMapStatus.
		const Coord3D center = { 0.0f, 300.0f, 0.0f };
		bool sawOnMap = false;
		bool sawOffMap = false;
		for (const PartitionHit &h : f.logic->partition().iterateObjectsInRange(center, 80.0f, FROM_CENTER_2D, {}, ITER_SORTED_NEAR_TO_FAR))
		{
			sawOnMap = sawOnMap || h.object == onMapEnemy;
			sawOffMap = sawOffMap || h.object == offMapEnemy;
		}
		REQUIRE(sawOnMap);
		REQUIRE(sawOffMap);

		auto *dom = dynamic_cast<DominateEnemySpecialPower *>(abilityOf(*caster, "SpecialAbilityP258DomArea"));
		REQUIRE(dom != nullptr);
		dom->initiateIntentToDoSpecialPower(dom->abilityData()->m_specialPowerTemplate, nullptr, &center, 0, 0);
		f.frames(20);

		CHECK(dom->dominated() == 1);
		CHECK(onMapEnemy->getControllingPlayer() == playerA);
		CHECK(offMapEnemy->getControllingPlayer() == playerB);
	}

	SUBCASE("off-map caster dominates off-map enemy and skips on-map enemy (XOR bit 3 == 0, RW 0x00660E93)")
	{
		Port258World f(kPort258DominateIni);
		Object *caster = f.unit("P258Dominator", 'A', -40.0f, 300.0f);
		Object *onMapEnemy = f.unit("P258EnemyInfantry", 'B', 20.0f, 300.0f);
		Object *offMapEnemy = f.unit("P258EnemyInfantry", 'B', -20.0f, 300.0f);
		REQUIRE(caster != nullptr);
		REQUIRE(onMapEnemy != nullptr);
		REQUIRE(offMapEnemy != nullptr);
		REQUIRE(caster->isOffMap());
		REQUIRE_FALSE(onMapEnemy->isOffMap());
		REQUIRE(offMapEnemy->isOffMap());

		Player *playerA = caster->getControllingPlayer();
		Player *playerB = onMapEnemy->getControllingPlayer();
		const Coord3D center = { 0.0f, 300.0f, 0.0f };

		auto *dom = dynamic_cast<DominateEnemySpecialPower *>(abilityOf(*caster, "SpecialAbilityP258DomArea"));
		REQUIRE(dom != nullptr);
		dom->initiateIntentToDoSpecialPower(dom->abilityData()->m_specialPowerTemplate, nullptr, &center, 0, 0);
		f.frames(20);

		CHECK(dom->dominated() == 1);
		CHECK(onMapEnemy->getControllingPlayer() == playerB);
		CHECK(offMapEnemy->getControllingPlayer() == playerA);
	}

	SUBCASE("neighbor: AttributeModifierAffects and status gates (RW 0x008D0F6C)")
	{
		Port258World f(kPort258DominateIni);
		Object *caster = f.unit("P258Dominator", 'A', 300.0f, 300.0f);
		Object *infantry = f.unit("P258EnemyInfantry", 'B', 350.0f, 300.0f);
		Object *monster = f.unit("P258EnemyMonster", 'B', 355.0f, 300.0f);
		Object *buildingMonster = f.unit("P258EnemyMonster", 'B', 360.0f, 300.0f);
		Object *airborneMonster = f.unit("P258EnemyMonster", 'B', 365.0f, 300.0f);
		REQUIRE(caster != nullptr);
		REQUIRE(infantry != nullptr);
		REQUIRE(monster != nullptr);
		REQUIRE(buildingMonster != nullptr);
		REQUIRE(airborneMonster != nullptr);

		const int kUnderConstruction = ObjectTemplateInfoBuilder::objectStatusIndex("UNDER_CONSTRUCTION");
		const int kAirborneTarget = ObjectTemplateInfoBuilder::objectStatusIndex("AIRBORNE_TARGET");
		REQUIRE(kUnderConstruction == 2);
		REQUIRE(kAirborneTarget == 6);
		buildingMonster->setStatus((unsigned)kUnderConstruction, true);
		airborneMonster->setStatus((unsigned)kAirborneTarget, true);

		Player *playerA = caster->getControllingPlayer();
		Player *playerB = infantry->getControllingPlayer();
		const Coord3D center = { 355.0f, 300.0f, 0.0f };

		auto *dom = dynamic_cast<DominateEnemySpecialPower *>(abilityOf(*caster, "SpecialAbilityP258DomFilter"));
		REQUIRE(dom != nullptr);
		dom->initiateIntentToDoSpecialPower(dom->abilityData()->m_specialPowerTemplate, nullptr, &center, 0, 0);
		f.frames(20);

		// Only the grounded, completed MONSTER passes AttributeModifierAffects = ANY +MONSTER and status checks.
		CHECK(dom->dominated() == 1);
		CHECK(infantry->getControllingPlayer() == playerB);
		CHECK(monster->getControllingPlayer() == playerA);
		CHECK(buildingMonster->getControllingPlayer() == playerB);
		CHECK(airborneMonster->getControllingPlayer() == playerB);
	}
}

TEST_CASE("PORT-258 CurseSpecialPower: CursePercentage defaults to 1.0f (RW 0x008D1192), curse rejects UNSELECTABLE status 3 (RW 0x008D130B), and uses getFinalOverride() (RW 0x008D138D)")
{
	SUBCASE("omitted CursePercentage defaults to 1.0f (RW 0x008D1192 -> RW 0x00BD1908) and resets enemy hero power to full reload")
	{
		Port258World f(kPort258CurseIni);
		Object *caster = f.unit("P258CurseCaster", 'A', 300.0f, 300.0f);
		Object *victim = f.unit("P258VictimHero", 'B', 350.0f, 300.0f);
		REQUIRE(caster != nullptr);
		REQUIRE(victim != nullptr);
		f.frames(60); // ReloadTime 10000 ms = 50 frames -> ready

		SpecialPowerModule *victimPower = powerOf(*victim, "SpecialAbilityP258VictimPower");
		WeaponModeSpecialPowerUpdate *victimMode = weaponModeOf(*victim);
		REQUIRE(victimPower != nullptr);
		REQUIRE(victimMode != nullptr);
		REQUIRE(victimPower->isReady());
		REQUIRE(victimMode->isReady());

		auto *cs = dynamic_cast<CurseSpecialPower *>(abilityOf(*caster, "SpecialAbilityP258CurseDefault"));
		REQUIRE(cs != nullptr);
		const auto *csData = dynamic_cast<const CurseSpecialPowerModuleData *>(cs->abilityData());
		REQUIRE(csData != nullptr);
		CHECK(csData->m_cursePercentage == 1.0f);

		cs->initiateIntentToDoSpecialPower(cs->abilityData()->m_specialPowerTemplate, victim, nullptr, 0, 0);
		unsigned triggerFrame = 0;
		for (int i = 0; i < 20 && cs->cursed() == 0; ++i)
		{
			f.frames(1);
			triggerFrame = f.logic->getFrame();
		}
		REQUIRE(cs->cursed() == 1);
		CHECK_FALSE(victimPower->isReady());
		CHECK(victimPower->getReadyFrame() == triggerFrame + 50);
		CHECK_FALSE(victimMode->isReady());
		CHECK(victimMode->getReadyFrame() == triggerFrame + 50);
	}

	SUBCASE("curse rejects UNSELECTABLE (status 3, RW 0x008D130B) and UNDER_CONSTRUCTION (status 2, RW 0x008D1318)")
	{
		Port258World f(kPort258CurseIni);
		Object *caster = f.unit("P258CurseCaster", 'A', 300.0f, 300.0f);
		Object *unselectableHero = f.unit("P258VictimHero", 'B', 350.0f, 300.0f);
		Object *buildingHero = f.unit("P258VictimHero", 'B', 360.0f, 300.0f);
		Object *normalHero = f.unit("P258VictimHero", 'B', 370.0f, 300.0f);
		Object *nonHero = f.unit("P258VictimInfantry", 'B', 355.0f, 300.0f);
		Object *alliedHero = f.unit("P258VictimHero", 'A', 345.0f, 300.0f);
		REQUIRE(caster != nullptr);
		REQUIRE(unselectableHero != nullptr);
		REQUIRE(buildingHero != nullptr);
		REQUIRE(normalHero != nullptr);
		REQUIRE(nonHero != nullptr);
		REQUIRE(alliedHero != nullptr);

		const int kUnderConstruction = ObjectTemplateInfoBuilder::objectStatusIndex("UNDER_CONSTRUCTION");
		const int kUnselectable = ObjectTemplateInfoBuilder::objectStatusIndex("UNSELECTABLE");
		REQUIRE(kUnderConstruction == 2);
		REQUIRE(kUnselectable == 3);
		unselectableHero->setStatus((unsigned)kUnselectable, true);
		buildingHero->setStatus((unsigned)kUnderConstruction, true);
		f.frames(60);

		SpecialPowerModule *unselectablePower = powerOf(*unselectableHero, "SpecialAbilityP258VictimPower");
		SpecialPowerModule *buildingPower = powerOf(*buildingHero, "SpecialAbilityP258VictimPower");
		SpecialPowerModule *normalPower = powerOf(*normalHero, "SpecialAbilityP258VictimPower");
		SpecialPowerModule *nonHeroPower = powerOf(*nonHero, "SpecialAbilityP258VictimPower");
		SpecialPowerModule *alliedPower = powerOf(*alliedHero, "SpecialAbilityP258VictimPower");
		REQUIRE(unselectablePower->isReady());
		REQUIRE(buildingPower->isReady());
		REQUIRE(normalPower->isReady());
		REQUIRE(nonHeroPower->isReady());
		REQUIRE(alliedPower->isReady());

		auto *cs = dynamic_cast<CurseSpecialPower *>(abilityOf(*caster, "SpecialAbilityP258CurseDefault"));
		REQUIRE(cs != nullptr);
		const Coord3D center = { 360.0f, 300.0f, 0.0f };
		cs->initiateIntentToDoSpecialPower(cs->abilityData()->m_specialPowerTemplate, nullptr, &center, 0, 0);
		f.frames(20);

		CHECK(cs->cursed() == 1);
		CHECK(unselectablePower->isReady());
		CHECK(buildingPower->isReady());
		CHECK_FALSE(normalPower->isReady());
		CHECK(nonHeroPower->isReady());
		CHECK(alliedPower->isReady());
	}

	SUBCASE("area scan reads RadiusCursorRadius from getFinalOverride() (RW 0x008D138D -> RW 0x00688D3C)")
	{
		Port258World f(kPort258CurseIni);
		// Override RadiusCursorRadius from 50.0 to 160.0 AFTER P258CurseCaster has cached the base SpecialPowerTemplate pointer.
		const char kOverride[] = R"ini(
SpecialPower SpecialAbilityP258CurseHalf
  RadiusCursorRadius = 160.0
End
)ini";
		const std::string err = f.w.load(kOverride, INI_LOAD_CREATE_OVERRIDES, "map.ini");
		REQUIRE_MESSAGE(err.empty(), err);

		Object *caster = f.unit("P258CurseCaster", 'A', 300.0f, 300.0f);
		// Distance 120 from target center (300, 300): outside base 50.0, inside override 160.0.
		Object *heroInOverride = f.unit("P258VictimHero", 'B', 420.0f, 300.0f);
		REQUIRE(caster != nullptr);
		REQUIRE(heroInOverride != nullptr);
		f.frames(60);

		SpecialPowerModule *victimPower = powerOf(*heroInOverride, "SpecialAbilityP258VictimPower");
		REQUIRE(victimPower != nullptr);
		REQUIRE(victimPower->isReady());

		auto *cs = dynamic_cast<CurseSpecialPower *>(abilityOf(*caster, "SpecialAbilityP258CurseHalf"));
		REQUIRE(cs != nullptr);
		const Coord3D center = { 300.0f, 300.0f, 0.0f };
		cs->initiateIntentToDoSpecialPower(cs->abilityData()->m_specialPowerTemplate, nullptr, &center, 0, 0);
		unsigned triggerFrame = 0;
		for (int i = 0; i < 20 && cs->cursed() == 0; ++i)
		{
			f.frames(1);
			triggerFrame = f.logic->getFrame();
		}
		REQUIRE(cs->cursed() == 1);
		CHECK_FALSE(victimPower->isReady());
		// CursePercentage = 50% on ReloadTime 10000 ms (50 frames) from 100% ready -> remaining 50% = 25 frames.
		CHECK(victimPower->getReadyFrame() == triggerFrame + 25);
	}
}

TEST_CASE("PORT-258 DeflectSpecialPower: xfer skips m_active/m_pending on XFER_CRC (RW 0x008C99FB / 0x008C9A0D), preserves save/load and onCollide neighbor")
{
	SUBCASE("XFER_CRC returns after SpecialPowerUpdateBase::xfer without hashing m_active/m_pending (RW 0x008C9A0D)")
	{
		Port258World f(kPort258DeflectIni);
		Object *idleSpear = f.unit("P258DeflectSpear", 'A', 300.0f, 300.0f);
		Object *activeSpear = f.unit("P258DeflectSpear", 'A', 340.0f, 300.0f);
		REQUIRE(idleSpear != nullptr);
		REQUIRE(activeSpear != nullptr);

		DeflectSpecialPower *idlePower = DeflectSpecialPower::of(*idleSpear);
		DeflectSpecialPower *activePower = DeflectSpecialPower::of(*activeSpear);
		REQUIRE(idlePower != nullptr);
		REQUIRE(activePower != nullptr);

		activePower->doSpecialPower(0);
		f.frames(3);
		REQUIRE_FALSE(idlePower->isActive());
		REQUIRE_FALSE(idlePower->pending());
		REQUIRE(activePower->isActive());
		REQUIRE_FALSE(activePower->pending());

		// Align base SpecialPowerUpdateBase state (readyFrame & wake frame) so ONLY m_active differs between the two modules.
		idlePower->setReadyFrame(activePower->getReadyFrame());
		idlePower->wakeFromInterface(UPDATE_SLEEP_NONE);
		activePower->wakeFromInterface(UPDATE_SLEEP_NONE);

		XferCRC crcIdle;
		idlePower->xfer(crcIdle);
		XferCRC crcActive;
		activePower->xfer(crcActive);
		CHECK(crcIdle.getCRC() == crcActive.getCRC());

		// Save/load (XFER_SAVE / XFER_LOAD) still transfers m_active and m_pending in v1 stream.
		XferSave saveActive;
		activePower->xfer(saveActive);
		REQUIRE(!saveActive.data().empty());
		CHECK(saveActive.data()[0] == 1);

		XferLoad loadIntoIdle(saveActive.data());
		idlePower->xfer(loadIntoIdle);
		CHECK(idlePower->isActive());
		CHECK_FALSE(idlePower->pending());
	}

	SUBCASE("neighbor: getCollide() (RW 0x008C993F) and onCollide() (RW 0x008C9A3C) jam only DEFLECT_BY_SPECIAL_POWER projectiles")
	{
		Port258World f(kPort258DeflectIni);
		Object *spear = f.unit("P258DeflectSpear", 'A', 300.0f, 300.0f);
		Object *deflectArrow = f.unit("P258DeflectArrow", 'B', 310.0f, 300.0f);
		Object *plainArrow = f.unit("P258PlainArrow", 'B', 310.0f, 310.0f);
		REQUIRE(spear != nullptr);
		REQUIRE(deflectArrow != nullptr);
		REQUIRE(plainArrow != nullptr);

		DeflectSpecialPower *power = DeflectSpecialPower::of(*spear);
		REQUIRE(power != nullptr);
		CHECK(power->getCollide() == static_cast<CollideModuleInterface *>(power));

		power->doSpecialPower(0);
		f.frames(3);
		REQUIRE(power->isActive());

		power->getCollide()->onCollide(plainArrow, nullptr, nullptr);
		CHECK_FALSE(plainArrow->isEffectivelyDead());
		power->getCollide()->onCollide(deflectArrow, nullptr, nullptr);
		CHECK(deflectArrow->isEffectivelyDead());
	}
}

TEST_CASE("PORT-258 SpecialPowerTimerRefreshSpecialPower: refreshes only non-shared SpecialPowerModule timers (RW 0x008C7B26 / 0x0068BDF9 / 0x00896F99) and round-trips xfer (RW 0x008C7B43)")
{
	Port258World f(kPort258RefreshIni);
	Object *caster = f.unit("P258RefreshCaster", 'A', 300.0f, 300.0f);
	Object *nearAlly = f.unit("P258RefreshTarget", 'A', 360.0f, 300.0f);
	Object *farAlly = f.unit("P258RefreshTarget", 'A', 520.0f, 300.0f);
	Object *nearEnemy = f.unit("P258RefreshTarget", 'B', 350.0f, 300.0f);
	REQUIRE(caster != nullptr);
	REQUIRE(nearAlly != nullptr);
	REQUIRE(farAlly != nullptr);
	REQUIRE(nearEnemy != nullptr);

	SpecialPowerModule *nearNormal = powerOf(*nearAlly, "SpecialAbilityP258Normal");
	SpecialPowerModule *nearShared = powerOf(*nearAlly, "SpecialAbilityP258Shared");
	WeaponModeSpecialPowerUpdate *nearMode = weaponModeOf(*nearAlly);
	SpecialPowerModule *farNormal = powerOf(*farAlly, "SpecialAbilityP258Normal");
	SpecialPowerModule *enemyNormal = powerOf(*nearEnemy, "SpecialAbilityP258Normal");
	REQUIRE(nearNormal != nullptr);
	REQUIRE(nearShared != nullptr);
	REQUIRE(nearMode != nullptr);
	REQUIRE(farNormal != nullptr);
	REQUIRE(enemyNormal != nullptr);

	nearNormal->startPowerRecharge(1.0f);
	REQUIRE(nearShared->isReady()); // first lookup registers shared ready-frame entry (RW 0x6AD1B0)
	nearShared->startPowerRecharge(1.0f);
	nearMode->startPowerRecharge(1.0f);
	farNormal->startPowerRecharge(1.0f);
	enemyNormal->startPowerRecharge(1.0f);
	REQUIRE_FALSE(nearNormal->isReady());
	REQUIRE_FALSE(nearShared->isReady());
	REQUIRE_FALSE(nearMode->isReady());
	REQUIRE_FALSE(farNormal->isReady());
	REQUIRE_FALSE(enemyNormal->isReady());

	auto *refresh = dynamic_cast<SpecialPowerTimerRefreshSpecialPower *>(powerOf(*caster, "SpecialAbilityP258Refresh"));
	REQUIRE(refresh != nullptr);
	refresh->doSpecialPower(0);
	f.frames(1);

	CHECK(refresh->refreshed() == 1);
	CHECK(nearNormal->isReady());
	CHECK(nearNormal->getReadyFrame() == 0);
	// RW 0x00896FC1 skips SharedSyncedTimer templates.
	CHECK_FALSE(nearShared->isReady());
	// RW 0x0068BDF9 invokes +0xC slot 0xB0, implemented only by SpecialPowerModule (RW 0x006519C2), not SpecialPowerUpdateBase.
	CHECK_FALSE(nearMode->isReady());
	// Out-of-range ally and in-range enemy are not refreshed.
	CHECK_FALSE(farNormal->isReady());
	CHECK_FALSE(enemyNormal->isReady());

	// RW 0x008C7B43: xfer round-trip preserves SpecialPowerTimerRefreshSpecialPower state.
	XferSave saved;
	refresh->xfer(saved);
	Port258World f2(kPort258RefreshIni);
	Object *caster2 = f2.unit("P258RefreshCaster", 'A', 300.0f, 300.0f);
	REQUIRE(caster2 != nullptr);
	auto *refresh2 = dynamic_cast<SpecialPowerTimerRefreshSpecialPower *>(powerOf(*caster2, "SpecialAbilityP258Refresh"));
	REQUIRE(refresh2 != nullptr);
	REQUIRE(refresh2->refreshed() == 0);
	XferLoad loaded(saved.data());
	refresh2->xfer(loaded);
	CHECK(refresh2->refreshed() == 1);
}
