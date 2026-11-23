// OpenBFME LOCAL-134 (fork issue 850):
// Expectations from RotWK 2.01 retail game.dat:
//   * StoreObjectsSpecialPower::triggerAbilityEffect (RW 0x008D1EA7): builds PartitionFilterSameMapStatus
//     at RW 0x008D1F4C (vtable RW 0x00C0F374, slot +4 RW 0x00660E93: Object +0x458 bit 3 matches caster).
//   * TeleportToCasterSpecialPower::triggerAbilityEffect (RW 0x008D1AEB):
//     - Stored-list path (RW 0x008D1B23 .. RW 0x008D1B3A): teleports the already-stored list via
//       teleportList (RW 0x008D1979) and clears it (RW 0x008D1E93) without re-running PartitionFilterSameMapStatus.
//     - Fallback partition-scan path (RW 0x008D1B3F .. RW 0x008D1D0A): builds PartitionFilterSameMapStatus
//       at RW 0x008D1BAE (vtable RW 0x00C0F374, slot +4 RW 0x00660E93: Object +0x458 bit 3 matches caster).
#include "doctest.h"
#include "CombatTestUtil.h"

#include "Common/SpecialPower.h"
#include "GameLogic/Module/StoreObjectsSpecialPower.h"
#include "GameLogic/Module/TeleportToCasterSpecialPower.h"
#include "GameLogic/Object/PartitionManager.h"
#include "GameLogic/Map/TerrainLogic.h"
#include "GameEngineDevice/W3DDevice/GameClient/WorldHeightMap.h"

#include <algorithm>
#include <cmath>

namespace
{
const char kLocal134TeleportIni[] =
	"SpecialPower L134StorePower\n Enum = SPECIAL_SCREECH\n ReloadTime = 100\n ObjectFilter = NONE +INFANTRY SAME_PLAYER\nEnd\n"
	"SpecialPower L134TeleportPower\n Enum = SPECIAL_MTTROLL_BORED\n ReloadTime = 100\n ObjectFilter = NONE +INFANTRY SAME_PLAYER\nEnd\n"
	"Object L134Caster\n KindOf = INFANTRY SELECTABLE HERO\n"
	" Body = ActiveBody BodyTag\n MaxHealth = 100\n End\n"
	" Behavior = AIUpdateInterface AITag\n End\n"
	" Behavior = SpecialPowerModule StorePowerTag\n SpecialPowerTemplate = L134StorePower\n UpdateModuleStartsAttack = Yes\n End\n"
	" Behavior = SpecialPowerModule GoPowerTag\n SpecialPowerTemplate = L134TeleportPower\n UpdateModuleStartsAttack = Yes\n End\n"
	" Behavior = StoreObjectsSpecialPower StoreTag\n"
	"  SpecialPowerTemplate = L134StorePower\n Radius = 150\n StartAbilityRange = 100000\n"
	"  UnpackTime = 200\n PreparationTime = 200\n PackTime = 200\n IgnoreFacingCheck = Yes\n"
	" End\n"
	" Behavior = TeleportToCasterSpecialPower GoTag\n"
	"  SpecialPowerTemplate = L134TeleportPower\n Radius = 150\n MinDestinationRadius = 15\n MaxDestinationRadius = 40\n StartAbilityRange = 100000\n"
	"  UnpackTime = 200\n PreparationTime = 200\n PackTime = 200\n IgnoreFacingCheck = Yes\n"
	" End\nEnd\n"
	"Object L134Follower\n KindOf = INFANTRY SELECTABLE\n"
	" Body = ActiveBody BodyTag\n MaxHealth = 100\n End\n"
	" Behavior = AIUpdateInterface AITag\n End\n"
	"End\n";

struct Local134BoundaryWorld : combattest::CombatWorld
{
	SpecialPowerStore powers;
	SpecialPowerStore *previous = TheSpecialPowerStore;
	WorldHeightMap hm;
	MapChunks chunks;
	TerrainLogic boundaryTerrain;

	Local134BoundaryWorld()
	{
		logic->partition().setRegion(0.0f, 0.0f, 2000.0f, 2000.0f);
		hm.m_width = 200;
		hm.m_height = 200;
		hm.m_data.assign(200u * 200u, 0);
		hm.m_boundaries = { ICoord2D{ 100, 100 }, ICoord2D{ 200, 200 } }; // active boundary: (0,0) .. (1000,1000)
		boundaryTerrain.init(hm, chunks);
		logic->setTerrain(&boundaryTerrain);

		TheSpecialPowerStore = &powers;
		w.fx.env.blocks.registerBlock("SpecialPower", [](INI *ini) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(ini); });
		const std::string error = w.load(kLocal134TeleportIni);
		REQUIRE_MESSAGE(error.empty(), error);
	}
	~Local134BoundaryWorld()
	{
		logic->setTerrain(nullptr);
		TheSpecialPowerStore = previous;
	}
};

float dist2D(const Coord3D &a, const Coord3D &b)
{
	return std::hypot(a.x - b.x, a.y - b.y);
}
} // namespace

// RW 0x008D1EA7 / RW 0x008D1F4C -> PartitionFilterSameMapStatus (vtable RW 0x00C0F374, slot +4 RW 0x00660E93)
TEST_CASE("LOCAL-134 StoreObjectsSpecialPower filters candidates by caster off-map status on both sides of boundary (RW 0x008D1F4C / RW 0x00660E93)")
{
	SUBCASE("on-map caster stores only on-map target and skips off-map target inside radius")
	{
		Local134BoundaryWorld w;
		Object *caster = w.unit("L134Caster", 'A', 950.0f, 500.0f);
		Object *onMapTarget = w.unit("L134Follower", 'A', 970.0f, 500.0f);
		Object *offMapTarget = w.unit("L134Follower", 'A', 1020.0f, 500.0f);
		REQUIRE(caster);
		REQUIRE(onMapTarget);
		REQUIRE(offMapTarget);
		REQUIRE_FALSE(caster->isOffMap());
		REQUIRE_FALSE(onMapTarget->isOffMap());
		REQUIRE(offMapTarget->isOffMap());

		auto *store = dynamic_cast<StoreObjectsSpecialPower *>(caster->findModule("StoreObjectsSpecialPower"));
		REQUIRE(store);
		const Coord3D targetPos{ 990.0f, 500.0f, 0.0f };
		store->initiateIntentToDoSpecialPower(store->abilityData()->m_specialPowerTemplate, nullptr, &targetPos, 0, 0);
		w.frames(20);

		REQUIRE(store->storedObjects().size() == 1);
		CHECK(store->storedObjects()[0] == onMapTarget->getID());
	}

	SUBCASE("off-map caster stores only off-map target and skips on-map target inside radius")
	{
		Local134BoundaryWorld w;
		Object *caster = w.unit("L134Caster", 'A', 1050.0f, 500.0f);
		Object *onMapTarget = w.unit("L134Follower", 'A', 970.0f, 500.0f);
		Object *offMapTarget = w.unit("L134Follower", 'A', 1020.0f, 500.0f);
		REQUIRE(caster);
		REQUIRE(onMapTarget);
		REQUIRE(offMapTarget);
		REQUIRE(caster->isOffMap());
		REQUIRE_FALSE(onMapTarget->isOffMap());
		REQUIRE(offMapTarget->isOffMap());

		auto *store = dynamic_cast<StoreObjectsSpecialPower *>(caster->findModule("StoreObjectsSpecialPower"));
		REQUIRE(store);
		const Coord3D targetPos{ 990.0f, 500.0f, 0.0f };
		store->initiateIntentToDoSpecialPower(store->abilityData()->m_specialPowerTemplate, nullptr, &targetPos, 0, 0);
		w.frames(20);

		REQUIRE(store->storedObjects().size() == 1);
		CHECK(store->storedObjects()[0] == offMapTarget->getID());
	}
}

// RW 0x008D1AEB / RW 0x008D1BAE -> PartitionFilterSameMapStatus (vtable RW 0x00C0F374, slot +4 RW 0x00660E93)
TEST_CASE("LOCAL-134 TeleportToCasterSpecialPower fallback scan filters by caster off-map status on both sides of boundary (RW 0x008D1BAE / RW 0x00660E93)")
{
	SUBCASE("on-map caster teleports only on-map ally and leaves off-map ally unmoved")
	{
		Local134BoundaryWorld w;
		Object *caster = w.unit("L134Caster", 'A', 300.0f, 500.0f);
		Object *onMapAlly = w.unit("L134Follower", 'A', 970.0f, 500.0f);
		Object *offMapAlly = w.unit("L134Follower", 'A', 1020.0f, 500.0f);
		REQUIRE(caster);
		REQUIRE(onMapAlly);
		REQUIRE(offMapAlly);
		REQUIRE_FALSE(caster->isOffMap());
		REQUIRE_FALSE(onMapAlly->isOffMap());
		REQUIRE(offMapAlly->isOffMap());

		const Coord3D offOrig = *offMapAlly->getPosition();
		auto *go = dynamic_cast<TeleportToCasterSpecialPower *>(caster->findModule("TeleportToCasterSpecialPower"));
		REQUIRE(go);
		const Coord3D scanCenter{ 990.0f, 500.0f, 0.0f };
		go->initiateIntentToDoSpecialPower(go->abilityData()->m_specialPowerTemplate, nullptr, &scanCenter, 0, 0);
		w.frames(20);

		CHECK(dist2D(*onMapAlly->getPosition(), *caster->getPosition()) <= 45.0f); // tolerance: findPositionAround ring is MinDestinationRadius (15) .. MaxDestinationRadius (40)
		CHECK(dist2D(*offMapAlly->getPosition(), offOrig) == 0.0f);
	}

	SUBCASE("off-map caster teleports only off-map ally and leaves on-map ally unmoved")
	{
		Local134BoundaryWorld w;
		Object *caster = w.unit("L134Caster", 'A', 1400.0f, 500.0f);
		Object *onMapAlly = w.unit("L134Follower", 'A', 970.0f, 500.0f);
		Object *offMapAlly = w.unit("L134Follower", 'A', 1020.0f, 500.0f);
		REQUIRE(caster);
		REQUIRE(onMapAlly);
		REQUIRE(offMapAlly);
		REQUIRE(caster->isOffMap());
		REQUIRE_FALSE(onMapAlly->isOffMap());
		REQUIRE(offMapAlly->isOffMap());

		const Coord3D onOrig = *onMapAlly->getPosition();
		auto *go = dynamic_cast<TeleportToCasterSpecialPower *>(caster->findModule("TeleportToCasterSpecialPower"));
		REQUIRE(go);
		const Coord3D scanCenter{ 990.0f, 500.0f, 0.0f };
		go->initiateIntentToDoSpecialPower(go->abilityData()->m_specialPowerTemplate, nullptr, &scanCenter, 0, 0);
		w.frames(20);

		CHECK(dist2D(*offMapAlly->getPosition(), *caster->getPosition()) <= 45.0f); // tolerance: findPositionAround ring is MinDestinationRadius (15) .. MaxDestinationRadius (40)
		CHECK(dist2D(*onMapAlly->getPosition(), onOrig) == 0.0f);
	}
}

// RW 0x008D1AEB -> RW 0x008D1B2D (teleportList RW 0x008D1979) -> RW 0x008D1E93 (clearStoredObjects)
TEST_CASE("LOCAL-134 TeleportToCasterSpecialPower stored-list path teleports previously stored object without re-checking off-map status (RW 0x008D1B2D)")
{
	Local134BoundaryWorld w;
	Object *caster = w.unit("L134Caster", 'A', 950.0f, 500.0f);
	Object *onMapTarget = w.unit("L134Follower", 'A', 970.0f, 500.0f);
	Object *offMapTarget = w.unit("L134Follower", 'A', 1020.0f, 500.0f);
	REQUIRE(caster);
	REQUIRE(onMapTarget);
	REQUIRE(offMapTarget);
	REQUIRE_FALSE(caster->isOffMap());
	REQUIRE_FALSE(onMapTarget->isOffMap());
	REQUIRE(offMapTarget->isOffMap());

	auto *store = dynamic_cast<StoreObjectsSpecialPower *>(caster->findModule("StoreObjectsSpecialPower"));
	auto *go = dynamic_cast<TeleportToCasterSpecialPower *>(caster->findModule("TeleportToCasterSpecialPower"));
	REQUIRE(store);
	REQUIRE(go);

	// Step 1: store while caster is on-map -> only onMapTarget is recorded.
	const Coord3D scanCenter{ 990.0f, 500.0f, 0.0f };
	store->initiateIntentToDoSpecialPower(store->abilityData()->m_specialPowerTemplate, nullptr, &scanCenter, 0, 0);
	w.frames(20);
	REQUIRE(store->storedObjects().size() == 1);
	REQUIRE(store->storedObjects()[0] == onMapTarget->getID());

	// Step 2: move caster off-map after storing; stored list must still teleport without re-filtering.
	const Coord3D offMapCasterPos{ 1100.0f, 500.0f, 0.0f };
	caster->setPosition(&offMapCasterPos);
	REQUIRE(caster->isOffMap());
	REQUIRE_FALSE(onMapTarget->isOffMap());

	const Coord3D dest{ 400.0f, 500.0f, 0.0f };
	const Coord3D offOrig = *offMapTarget->getPosition();
	go->initiateIntentToDoSpecialPower(go->abilityData()->m_specialPowerTemplate, nullptr, &dest, 0, 0);
	w.frames(20);

	CHECK(store->storedObjects().empty());
	CHECK(dist2D(*onMapTarget->getPosition(), dest) <= 45.0f); // tolerance: findPositionAround ring is MinDestinationRadius (15) .. MaxDestinationRadius (40)
	CHECK(dist2D(*offMapTarget->getPosition(), offOrig) == 0.0f);
}
