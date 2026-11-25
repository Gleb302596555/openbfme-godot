// OpenBFME unit tests: LOCAL-173 objects and modules part 3 of 4 (fork issue 954).
//
// Retail RotWK 2.01 facts verified by disassembly:
//   * CivilianSpawnUpdateModuleData ctor (RW 0x882CB2):
//     - RW 0x882CE2..0x882CEA: m_spawnDelay (+0x08) = [0xD9F608] = LOGICFRAMES_PER_SECOND (5 frames).
//     - RW 0x882CED: m_maximumDistance (+0x10) = 0x12C (300); table RW 0xC5EC68 parses it via
//       INI::parseInt (RW 0x42EC5E); update() converts int to float via cvtsi2ss (RW 0x882E0C).
//   * BannerCarrierUpdateModuleData ctor (RW 0x89A765):
//     - RW 0x89A795..0x89A79E: m_idleSpawnRate (+0x08) = 15 * LOGICFRAMES_PER_SECOND = 75 frames.
//     - RW 0x89A7A1..0x89A7AA: m_meleeFreeUnitSpawnTime (+0x0C) = 10 * LOGICFRAMES_PER_SECOND = 50 frames.
//     - RW 0x89A7AD..0x89A7B6: m_diedRespawnTime (+0x10) = 10 * LOGICFRAMES_PER_SECOND = 50 frames.
//     - RW 0x89A7B9..0x89A7C2: m_meleeFreeBannerReSpawnTime (+0x14) = 10 * LOGICFRAMES_PER_SECOND = 50 frames.

#include "doctest.h"
#include "CombatTestUtil.h"

#include "Common/Thing/ThingTemplate.h"
#include "GameLogic/Module/BannerCarrierUpdate.h"
#include "GameLogic/Module/CivilianSpawnUpdate.h"
#include "GameLogic/Object/Object.h"

namespace
{

const char kLocal173Ini[] =
	"Object Local173Civ\n"
	"  KindOf = INFANTRY\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 5\n"
	"  GeometryHeight = 15\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 50\n"
	"  End\n"
	"End\n"
	"Object Local173CivSpawnerDefault\n"
	"  KindOf = STRUCTURE SELECTABLE IMMOBILE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 20\n"
	"  GeometryMinorRadius = 20\n"
	"  GeometryHeight = 30\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"  Behavior = CivilianSpawnUpdate ModuleTag_CivDefault\n"
	"    Civilian = Local173Civ\n"
	"  End\n"
	"End\n"
	"Object Local173CivSpawnerExplicit\n"
	"  KindOf = STRUCTURE SELECTABLE IMMOBILE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 20\n"
	"  GeometryMinorRadius = 20\n"
	"  GeometryHeight = 30\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"  Behavior = CivilianSpawnUpdate ModuleTag_CivExplicit\n"
	"    SpawnDelayTime = 2000\n"
	"    MaximumDistance = 150\n"
	"    Civilian = Local173Civ\n"
	"  End\n"
	"End\n"
	"Object Local173BannerDefault\n"
	"  KindOf = INFANTRY\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 5\n"
	"  GeometryHeight = 15\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = BannerCarrierUpdate ModuleTag_BannerDefault\n"
	"  End\n"
	"End\n"
	"Object Local173BannerExplicit\n"
	"  KindOf = INFANTRY\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 5\n"
	"  GeometryHeight = 15\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = BannerCarrierUpdate ModuleTag_BannerExplicit\n"
	"    IdleSpawnRate = 4000\n"
	"    MeleeFreeUnitSpawnTime = 2000\n"
	"    DiedRespawnTime = 6000\n"
	"    MeleeFreeBannerReSpawnTime = 8000\n"
	"  End\n"
	"End\n";

struct Local173Rig
{
	combattest::CombatWorld world;

	Local173Rig()
		: world()
	{
		CivilianSpawnUpdate::registerClass(world.w.modules);
		BannerCarrierUpdate::registerClass(world.w.modules);
		std::string err = world.w.load(kLocal173Ini, INI_LOAD_OVERWRITE, "local173_test.ini");
		REQUIRE_MESSAGE(err.empty(), err);
	}
};

} // namespace

TEST_CASE("LOCAL-173 CivilianSpawnUpdateModuleData retail defaults (RW 0x882CB2)")
{
	// Source: RW 0x882CB2 (m_spawnDelay = 5 frames, m_maximumDistance = 300 int), RW 0xC5EC68 (INI::parseInt RW 0x42EC5E)
	Local173Rig rig;
	Object *defObj = rig.world.unit("Local173CivSpawnerDefault", 'A', 100.0f, 100.0f);
	REQUIRE(defObj != nullptr);
	auto *defMod = dynamic_cast<CivilianSpawnUpdate *>(defObj->findModule("CivilianSpawnUpdate"));
	REQUIRE(defMod != nullptr);
	const auto *defData = dynamic_cast<const CivilianSpawnUpdateModuleData *>(defMod->getModuleData());
	REQUIRE(defData != nullptr);
	CHECK(defData->m_spawnDelay == 5u);
	CHECK(defData->m_maximumDistance == 300);
	// Runtime ctor picks random(m_spawnDelay/2, m_spawnDelay) = random(2, 5)
	CHECK(defMod->spawnFrame() >= 2u); // tolerance: uniform RNG draw in [m_spawnDelay/2, m_spawnDelay] = [2, 5] (RW 0x882C31)
	CHECK(defMod->spawnFrame() <= 5u); // tolerance: uniform RNG draw in [m_spawnDelay/2, m_spawnDelay] = [2, 5] (RW 0x882C31)

	// Neighbor check: explicit INI values override the retail defaults (2000 ms @ 5 Hz = 10 frames)
	Object *expObj = rig.world.unit("Local173CivSpawnerExplicit", 'A', 200.0f, 100.0f);
	REQUIRE(expObj != nullptr);
	auto *expMod = dynamic_cast<CivilianSpawnUpdate *>(expObj->findModule("CivilianSpawnUpdate"));
	REQUIRE(expMod != nullptr);
	const auto *expData = dynamic_cast<const CivilianSpawnUpdateModuleData *>(expMod->getModuleData());
	REQUIRE(expData != nullptr);
	CHECK(expData->m_spawnDelay == 10u);
	CHECK(expData->m_maximumDistance == 150);
}

TEST_CASE("LOCAL-173 BannerCarrierUpdateModuleData retail defaults (RW 0x89A765)")
{
	// Source: RW 0x89A765 (m_idleSpawnRate = 15*5 = 75, m_meleeFreeUnitSpawnTime = 10*5 = 50,
	// m_diedRespawnTime = 10*5 = 50, m_meleeFreeBannerReSpawnTime = 10*5 = 50)
	Local173Rig rig;
	Object *defObj = rig.world.unit("Local173BannerDefault", 'A', 100.0f, 100.0f);
	REQUIRE(defObj != nullptr);
	auto *defMod = dynamic_cast<BannerCarrierUpdate *>(defObj->findModule("BannerCarrierUpdate"));
	REQUIRE(defMod != nullptr);
	const BannerCarrierUpdateModuleData &defData = defMod->data();
	CHECK(defData.m_idleSpawnRate == 75u);
	CHECK(defData.m_meleeFreeUnitSpawnTime == 50u);
	CHECK(defData.m_diedRespawnTime == 50u);
	CHECK(defData.m_meleeFreeBannerReSpawnTime == 50u);

	// Neighbor check: explicit INI durations override the retail defaults (at 5 Hz)
	Object *expObj = rig.world.unit("Local173BannerExplicit", 'A', 200.0f, 100.0f);
	REQUIRE(expObj != nullptr);
	auto *expMod = dynamic_cast<BannerCarrierUpdate *>(expObj->findModule("BannerCarrierUpdate"));
	REQUIRE(expMod != nullptr);
	const BannerCarrierUpdateModuleData &expData = expMod->data();
	CHECK(expData.m_idleSpawnRate == 20u);
	CHECK(expData.m_meleeFreeUnitSpawnTime == 10u);
	CHECK(expData.m_diedRespawnTime == 30u);
	CHECK(expData.m_meleeFreeBannerReSpawnTime == 40u);
}
