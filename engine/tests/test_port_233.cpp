// PORT-233: EmotionTrackerUpdate's visibility predicate is viewer-relative for disguised objects.
// RotWK RW 0x8B5186..0x8B5195 passes the tracker owner's player to Object::isVisibleTo at RW 0x694CCC (mode 0).

#include "doctest.h"

#include "MoveTestUtil.h"

#include "Common/Thing/ThingTemplate.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/Module/EmotionModules.h"
#include "GameLogic/Module/InvisibilityModules.h"
#include "GameLogic/Object/Object.h"

namespace
{
const char kPort233Objects[] =
	"Object PORT233Disguiser\n"
	"  KindOf = INFANTRY SELECTABLE DISGUISER\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = StealthUpdate ModuleTag_Stealth\n"
	"  End\n"
	"End\n";

struct Port233World : movetest::MoveWorld
{
	Port233World()
		: movetest::MoveWorld(200, 200, kPort233Objects)
	{
	}
};
} // namespace

TEST_CASE("PORT-233 tracker visibility uses the viewer's relationship to a disguised object's apparent owner (RW 0x694CCC, 0x8B5186)")
{
	Port233World w;
	Object *viewer = w.make("Walker", w.teamOf("Alice"));
	Object *disguiser = w.make("PORT233Disguiser", w.teamOf("Bob"));
	Object *asAlly = w.make("Walker", w.teamOf("Alice"));
	Object *asEnemy = w.make("Walker", w.teamOf("Bob"));
	REQUIRE(viewer != nullptr);
	REQUIRE(disguiser != nullptr);
	REQUIRE(asAlly != nullptr);
	REQUIRE(asEnemy != nullptr);
	StealthUpdate *stealth = StealthUpdate::of(*disguiser);
	REQUIRE(stealth != nullptr);

	// A disguise as an ally is hidden from that viewer by RW 0x694E87..0x694ED1; the old viewer-less
	// DISGUISER shortcut returned true regardless of the apparent owner's relationship.
	stealth->disguiseAsObject(asAlly);
	REQUIRE(stealth->disguisePlayerIndex() == asAlly->getControllingPlayer()->getPlayerIndex());
	disguiser->setStatus((unsigned)CombatNames::status("STEALTHED"), true);
	disguiser->setStatus((unsigned)CombatNames::status("DETECTED"), false);
	CHECK_FALSE(EmotionModules::visibleToScans(*disguiser, viewer->getControllingPlayer()));

	// Neighbor: a disguise as an enemy remains visible to this viewer (same RW branch).
	stealth->disguiseAsObject(asEnemy);
	CHECK(EmotionModules::visibleToScans(*disguiser, viewer->getControllingPlayer()));

	// Existing callers without a viewer retain their legacy query contract.
	CHECK(EmotionModules::visibleToScans(*disguiser));
}
