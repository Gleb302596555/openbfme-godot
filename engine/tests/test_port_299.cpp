// PORT-299: exercise the reachable object-level revenge weapon, not the gated-off Porcupine body fields.
#include "doctest.h"
#include "CombatTestUtil.h"
#include "Horde2TestUtil.h"
#include "GameLogic/Module/SquishCollide.h"

using namespace combattest;
using namespace horde2test;

TEST_CASE("PORT-299: a crushed unit's object-level revenge weapon damages the crusher")
{
	for (bool revenge : { false, true })
	{
		std::string objects = kCrushObjects;
		// Make the fixture's pikeman crushable by the level-one rider, with or without its revenge weapon.
		const std::string level = "Object Pikeman\n  KindOf = INFANTRY SELECTABLE CAN_ATTACK SCORE\n  CrushableLevel = 1";
		const size_t pos = objects.find(level);
		REQUIRE(pos != std::string::npos);
		objects.replace(pos, level.size(), "Object Pikeman\n  KindOf = INFANTRY SELECTABLE CAN_ATTACK SCORE\n  CrushableLevel = 0");
		if (!revenge)
		{
			const std::string line = "  CrushRevengeWeapon = PikeRevenge\n";
			const size_t weaponPos = objects.find(line);
			REQUIRE(weaponPos != std::string::npos);
			objects.erase(weaponPos, line.size());
		}
		CombatWorld w(objects.c_str());
		REQUIRE(w.w.load(kCrushWeapons, INI_LOAD_OVERWRITE, "weapon2.ini").empty());
		w.combat().setAutoAcquireEnabled(false);
		Object *rider = w.unit("Rider", 'A', 200, 300);
		Object *pike = w.unit("Pikeman", 'B', 300, 300);
		w.frames(2);
		REQUIRE(pike->getWeapons() != nullptr);
		CHECK((pike->getWeapons()->crushRevengeWeapon() != nullptr) == revenge);
		CHECK(ObjectCrush::crushableLevel(*pike) == 0);
		rider->getAIUpdateInterface()->aiMoveToPosition(Coord3D{ 400.0f, 300.0f, 0.0f }, CMD_FROM_PLAYER);
		w.runUntil([&] { return w.combat().counters().crushes > 0; }, 100);
		REQUIRE(w.combat().counters().crushes == 1);
		CHECK(w.combat().counters().crushWeaponShots == (revenge ? 2 : 1));
		// PikeRevenge deals 25 PIERCE; the rider's armour takes 80% of it.
		CHECK(w.health(rider) == doctest::Approx(revenge ? 280.0f : 300.0f));
		CHECK(w.health(pike) == doctest::Approx(40.0f));
	}
}
