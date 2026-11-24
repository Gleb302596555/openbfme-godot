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

#include "Common/INI.h"
#include "Common/Thing/ModuleFactory.h"
#include "Common/Thing/ThingFactory.h"
#include "Common/Thing/ThingTemplate.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/BannerCarrierUpdate.h"
#include "GameLogic/Module/CivilianSpawnUpdate.h"
#include "GameLogic/Object/Object.h"

#include <cstring>

namespace
{

void parseIniString(const char *text, ThingFactory &things, ModuleFactory &modules)
{
	INI ini;
	ini.loadMemory(text, std::strlen(text), "local173.ini", INI_LOAD_OVERWRITE, &things, &modules);
}

} // namespace

TEST_CASE("LOCAL-173 CivilianSpawnUpdateModuleData retail defaults (RW 0x882CB2)")
{
	// Source: RW 0x882CB2 (m_spawnDelay = 5 frames, m_maximumDistance = 300 int), RW 0xC5EC68 (INI::parseInt RW 0x42EC5E)
	const char *kIni =
		"Object Local173CivSpawnerDefault\n"
		"  KindOf = STRUCTURE\n"
		"  Behavior = CivilianSpawnUpdate ModuleTag_CivDefault\n"
		"    Civilian = Local173Civ\n"
		"  End\n"
		"End\n"
		"Object Local173CivSpawnerExplicit\n"
		"  KindOf = STRUCTURE\n"
		"  Behavior = CivilianSpawnUpdate ModuleTag_CivExplicit\n"
		"    SpawnDelayTime = 2000\n"
		"    MaximumDistance = 150\n"
		"    Civilian = Local173Civ\n"
		"  End\n"
		"End\n";

	GameLogic logic;
	parseIniString(kIni, logic.things(), logic.modules());

	const ThingTemplate *defTpl = logic.things().findTemplate("Local173CivSpawnerDefault");
	REQUIRE(defTpl != nullptr);
	const CivilianSpawnUpdateModuleData *defData = nullptr;
	for (const auto &entry : defTpl->getBehaviorModules())
	{
		if (auto *d = dynamic_cast<const CivilianSpawnUpdateModuleData *>(entry.data.get()))
		{
			defData = d;
			break;
		}
	}
	REQUIRE(defData != nullptr);
	CHECK(defData->m_spawnDelay == 5u);
	CHECK(defData->m_maximumDistance == 300);

	// Neighbor check: explicit INI values override the retail defaults (2000 ms @ 5 Hz = 10 frames)
	const ThingTemplate *expTpl = logic.things().findTemplate("Local173CivSpawnerExplicit");
	REQUIRE(expTpl != nullptr);
	const CivilianSpawnUpdateModuleData *expData = nullptr;
	for (const auto &entry : expTpl->getBehaviorModules())
	{
		if (auto *d = dynamic_cast<const CivilianSpawnUpdateModuleData *>(entry.data.get()))
		{
			expData = d;
			break;
		}
	}
	REQUIRE(expData != nullptr);
	CHECK(expData->m_spawnDelay == 10u);
	CHECK(expData->m_maximumDistance == 150);
}
