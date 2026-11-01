// Both editions: RotWK 2.01 and BFME2 1.06.
#include "doctest.h"
#include "CombatTestUtil.h"

#include "Common/GameEdition.h"
#include "Common/INI.h"
#include "Common/StateHash.h"
#include "Common/XferLoad.h"
#include "Common/XferSave.h"
#include "GameClient/EvaAnnounceClientCreate.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/SpawnUnitBehavior.h"
#include "GameLogic/Object/Object.h"

namespace
{
const char kSpawnIni[] =
    "Object TestSpawnObj\n"
    "  KindOf = STRUCTURE\n"
    "  Body = ActiveBody BodyTag\n"
    "    MaxHealth = 1000\n"
    "  End\n"
    "  Behavior = SpawnUnitBehavior ModuleTag_Spawn\n"
    "    UnitName = TestSpawnedUnit\n"
    "    SpawnOnce = Yes\n"
    "  End\n"
    "End\n"
    "Object TestSpawnedUnit\n"
    "  KindOf = INFANTRY\n"
    "  Body = ActiveBody BodyTag\n"
    "    MaxHealth = 100\n"
    "  End\n"
    "End\n";
} // namespace

TEST_CASE("modgap2 spawn: SpawnUnitBehavior ModuleData and update gates (both editions: RW 0x8BC9B0 / BFME2 0x89ECC0)")
{
	for (GameEdition edition : { GameEdition::RotWK201, GameEdition::BFME2_106 })
	{
		CAPTURE(gameEditionName(edition));
		combattest::CombatWorld w(kSpawnIni);
		w.logic->setEdition(edition);

		Object *obj = w.unit("TestSpawnObj", 'A', 50.0f, 50.0f);
		REQUIRE(obj != nullptr);

		auto *spawn = dynamic_cast<SpawnUnitBehavior *>(obj->findModule("SpawnUnitBehavior"));
		REQUIRE(spawn != nullptr);
		CHECK(spawn->getUnitName() == "TestSpawnedUnit");
		CHECK(spawn->spawned() == true);

		// While under construction, does not spawn
		obj->setStatus(OBJECT_STATUS_UNDER_CONSTRUCTION, true);
		CHECK(spawn->update() == UPDATE_SLEEP_NONE);
		CHECK(spawn->spawned() == true);
		obj->setStatus(OBJECT_STATUS_UNDER_CONSTRUCTION, false);

		// xfer roundtrip
		XferSave save;
		spawn->xfer(save);
		CHECK_FALSE(save.data().empty());

		XferLoad load(save.data().data(), save.data().size());
		spawn->xfer(load);
		CHECK(spawn->spawned() == true);

		StateHasher h1, h2;
		spawn->crc(h1);
		spawn->crc(h2);
		CHECK(h1.value() == h2.value());
	}
}

TEST_CASE("modgap2 eva: EvaAnnounceClientCreate first-sight and state xfer (both editions: RW 0x8CDED6 / BFME2 0x4C9A51)")
{
	for (GameEdition edition : { GameEdition::RotWK201, GameEdition::BFME2_106 })
	{
		CAPTURE(gameEditionName(edition));

		EvaAnnounceClientCreateModuleData data;
		data.enemy = 10;
		data.ally = 20;
		data.owner = 30;
		data.delay = 100;
		data.firstSight = true;
		data.onlyVisible = false;

		EvaAnnounceClientCreate eva(nullptr, &data);
		eva.onBuildCompleteAt(0);

		// Frame before due -> returns -1
		bool sight = false;
		CHECK(eva.update(10, false, true, false, 0, sight, false) == -1);
		CHECK(sight == false);

		// Frame after due -> returns event and sets sight
		int event = eva.update(200, false, true, false, 0, sight, false);
		CHECK(event == 10); // enemy
		CHECK(sight == true);

		// If already sighted, first-sight event does not trigger
		eva.restoreLoaded(true, 50);
		event = eva.update(100, false, true, false, 0, sight, true);
		CHECK(event == -1);
		CHECK(sight == false);

		// xfer roundtrip
		eva.restoreLoaded(true, 77);
		XferSave save;
		eva.xfer(save);
		CHECK_FALSE(save.data().empty());

		EvaAnnounceClientCreate restored(nullptr, &data);
		XferLoad load(save.data().data(), save.data().size());
		restored.xfer(load);
		CHECK(restored.started() == true);
		CHECK(restored.due() == 77);
	}
}

