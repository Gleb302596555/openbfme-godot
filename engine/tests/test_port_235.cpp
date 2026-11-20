// OpenBFME unit tests. PORT-235 (issue #558): GameData DefaultStructureRepairBuffFxList.
// RW 0x8C2FC1 / 0x8C30FA / 0x8C3113: after health rises, ActiveBody::attemptHealing plays module data +0x48 on the object,
// except when its template is STRUCTURE (RW 0x8C30EE) or it has AIRBORNE_TARGET (RW 0x8C3101).

#include "doctest.h"

#include "CombatTestUtil.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/FXEvents.h"
#include "GameLogic/GameLogic.h"

#include <string>

namespace
{
const char *const kPort235Objects =
	"Object P235WellBody\n"
	"  KindOf = IMMOBILE SELECTABLE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  Body = StructureBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"    MaxHealthDamaged = 75\n"
	"    MaxHealthReallyDamaged = 50\n"
	"  End\n"
	"End\n"
	"Object P235ActualStructure\n"
	"  KindOf = STRUCTURE IMMOBILE SELECTABLE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  Body = StructureBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"    MaxHealthDamaged = 75\n"
	"    MaxHealthReallyDamaged = 50\n"
	"  End\n"
	"End\n"
	"Object P235ExplicitOverride\n"
	"  KindOf = IMMOBILE SELECTABLE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  Body = StructureBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"    HealingBuffFx = FX_PORT235_EXPLICIT\n"
	"  End\n"
	"End\n"
	"Object P235ExplicitNone\n"
	"  KindOf = IMMOBILE SELECTABLE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  Body = StructureBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"    HealingBuffFx = None\n"
	"  End\n"
	"End\n";

const char *const kPort235GameData =
	"GameData\n"
	"  ForceModelsToFollowTimeOfDay = No\n"
	"  ForceModelsToFollowWeather = No\n"
	"  UnitDamagedThreshold = 50%\n"
	"  UnitReallyDamagedThreshold = 25%\n"
	"  DefaultStartingCash = 1000\n"
	"  DefaultStructureRepairBuffFxList = FX_PORT235_STRUCTURE_REPAIR\n"
	"End\n";
} // namespace

TEST_CASE("PORT-235: DefaultStructureRepairBuffFxList plays on a healed non-STRUCTURE StructureBody, but not on STRUCTURE or AIRBORNE_TARGET")
{
	combattest::CombatWorld world(kPort235Objects);
	std::string error;
	REQUIRE_MESSAGE(GameLogicSettingsLoader::scanGameData(kPort235GameData, world.logic->settings(), &error), error);
	CHECK(world.logic->settings().defaultStructureRepairBuffFxList == "FX_PORT235_STRUCTURE_REPAIR");

	Object *well = world.unit("P235WellBody", 'A', 150.0f, 150.0f);
	Object *building = world.unit("P235ActualStructure", 'A', 300.0f, 150.0f);
	Object *explicitFx = world.unit("P235ExplicitOverride", 'A', 450.0f, 150.0f);
	Object *explicitNone = world.unit("P235ExplicitNone", 'A', 600.0f, 150.0f);
	REQUIRE(well != nullptr);
	REQUIRE(building != nullptr);
	REQUIRE(explicitFx != nullptr);
	REQUIRE(explicitNone != nullptr);
	REQUIRE_FALSE(well->isKindOfName("STRUCTURE"));
	REQUIRE(building->isKindOfName("STRUCTURE"));
	REQUIRE(well->getBodyModule() != nullptr);
	REQUIRE(building->getBodyModule() != nullptr);

	well->getBodyModule()->setInitialHealth(50);
	const float wellBefore = well->getBodyModule()->getHealth();
	world.logic->fxEvents().beginFrame();
	well->attemptHealing(10.0f, nullptr);
	CHECK(well->getBodyModule()->getHealth() == wellBefore + 10.0f);
	const auto &wellEvents = world.logic->fxEvents().frameEvents();
	REQUIRE(wellEvents.size() == 1);
	CHECK(wellEvents[0].kind == FXEvent::OBJECT_FX);
	CHECK(std::string(wellEvents[0].site) == "HealingBuffFx");
	REQUIRE(wellEvents[0].fxList != nullptr);
	CHECK(*wellEvents[0].fxList == "FX_PORT235_STRUCTURE_REPAIR");
	CHECK(wellEvents[0].primary == well->getID());

	// A module-local HealingBuffFx still wins over the class-specific GameData default.
	explicitFx->getBodyModule()->setInitialHealth(50);
	world.logic->fxEvents().beginFrame();
	explicitFx->attemptHealing(10.0f, nullptr);
	const auto &overrideEvents = world.logic->fxEvents().frameEvents();
	REQUIRE(overrideEvents.size() == 1);
	REQUIRE(overrideEvents[0].fxList != nullptr);
	CHECK(*overrideEvents[0].fxList == "FX_PORT235_EXPLICIT");

	// An explicit None is also an override; it does not fall back to the GameData default.
	explicitNone->getBodyModule()->setInitialHealth(50);
	world.logic->fxEvents().beginFrame();
	explicitNone->attemptHealing(10.0f, nullptr);
	CHECK(world.logic->fxEvents().frameEvents().empty());

	building->getBodyModule()->setInitialHealth(50);
	const float buildingBefore = building->getBodyModule()->getHealth();
	world.logic->fxEvents().beginFrame();
	building->attemptHealing(10.0f, nullptr);
	CHECK(building->getBodyModule()->getHealth() == buildingBefore + 10.0f);
	CHECK(world.logic->fxEvents().frameEvents().empty()); // RW 0x8C30EE excludes every KindOf STRUCTURE.

	// Preserve RW's second exclusion independently: this is still a non-STRUCTURE StructureBody, now AIRBORNE_TARGET.
	well->setStatus((unsigned)CombatNames::status("AIRBORNE_TARGET"), true);
	well->getBodyModule()->setInitialHealth(50);
	const float airborneBefore = well->getBodyModule()->getHealth();
	world.logic->fxEvents().beginFrame();
	well->attemptHealing(10.0f, nullptr);
	CHECK(well->getBodyModule()->getHealth() == airborneBefore + 10.0f);
	CHECK(world.logic->fxEvents().frameEvents().empty()); // RW 0x8C3101 excludes AIRBORNE_TARGET.
}
