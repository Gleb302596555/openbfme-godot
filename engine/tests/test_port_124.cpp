// OpenBFME. GPL-3.0.
// PORT-124 (#308): the Ennorath hand-off is entry of a ring carrier into a
// CitadelSlaughterHordeContain on a ringbearer hero. The HUD admission uses
// ActionManager::canEnterObject (RW 0x82CBD5), MSG_ENTER reaches logic at
// RW 0x77AC26, and the ring-status branch follows the inherited contain check
// at RW 0x88399E..0x8839A5. Synthetic INI mirrors the supplied Ennorath fields;
// no game or mod files are used. The supplied data names but does not expand
// CARRYTHERING_OBJECTFILTER, so the test uses a synthetic name-only filter; it
// exercises AttachUpdate, not the Ennorath macro's membership. The entry
// callback's donor-only equivalence remains S-6010, so end-state expectations
// below are the owner's explicit Ennorath acceptance, not a claim that its
// callback has been read in RotWK.
//
// The third case pins stop S-7321. The donor's HUD offers no way around it: both
// entry actions take the same mode argument (action 8's DONT_CHECK_CAPACITY
// branch is unreachable from the screen, and action 9 is the "special" variant,
// not a cheaper check), and the ring case is that special flag — which is this
// citadel branch here (RW 0x88399E..): the inherited check first, then the ring
// status. What still gates entry for a carrier is the capacity mode's
// TransportSlotCount test, which runs before the contain is consulted.
#include "doctest.h"
#include "CombatTestUtil.h"
#include "GameClient/FXList.h"
#include "GameClient/HudInput.h"
#include "GameLogic/AI/AIGarrisonStates.h"
#include "GameLogic/AI/GarrisonCommands.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/Object/Contain/GarrisonContainRuntime.h"
#include "GameLogic/Object/PartitionManager.h"

using namespace combattest;

namespace
{
const std::string kPort124Data =
	// FXForRingEntry is resolved at parse time (RW 0x73A302: an unknown name is an
	// error), so the citadel's list must exist before the object that names it.
	"FXList FX_OneRingFlare\nEnd\n"
	"Upgrade Upgrade_RingHero\n Type = OBJECT\nEnd\n"
	"Upgrade Upgrade_FortressRingHero\n Type = OBJECT\nEnd\n"
	"Object P124Carrier\n"
	" KindOf = INFANTRY HERO SELECTABLE SCORE\n"
	" TransportSlotCount = 1\n"
	" Geometry = CYLINDER\n GeometryMajorRadius = 5\n GeometryHeight = 10\n"
	" Body = ActiveBody Body\n MaxHealth = 100\n End\n"
	" Behavior = AIUpdateInterface AI\n End\n"
	" LocomotorSet\n Locomotor = WalkerLoco\n Condition = SET_NORMAL\n Speed = 55\n End\n"
	"End\n"
	"Object P124RingBearer\n"
	" KindOf = INFANTRY HERO SELECTABLE SCORE\n"
	" TransportSlotCount = 1\n"
	" Geometry = CYLINDER\n GeometryMajorRadius = 8\n GeometryHeight = 20\n"
	" Body = ActiveBody Body\n MaxHealth = 1000\n End\n"
	" Behavior = AIUpdateInterface AI\n End\n"
	" Behavior = CitadelSlaughterHordeContain RingEntry\n"
	"  PassengerFilter = NONE +P124DroppedRing\n"
	"  ObjectStatusOfContained = UNSELECTABLE ENCLOSED\n"
	"  ContainMax = 1\n"
	"  AllowEnemiesInside = No\n AllowAlliesInside = Yes\n AllowNeutralInside = No\n"
	"  AllowOwnPlayerInsideOverride = Yes\n"
	"  EntryOffset = X:0 Y:0 Z:0\n EntryPosition = X:0 Y:0 Z:0\n ExitOffset = X:0 Y:0 Z:0\n"
	"  StatusForRingEntry = HOLDING_THE_RING\n"
	"  UpgradeForRingEntry = Upgrade_RingHero Upgrade_FortressRingHero\n"
	"  ObjectToDestroyForRingEntry = NONE +P124DroppedRing\n"
	"  FXForRingEntry = FX_OneRingFlare\n"
	"  CashBackPercent = 25%\n"
	" End\n"
	" LocomotorSet\n Locomotor = WalkerLoco\n Condition = SET_NORMAL\n Speed = 55\n End\n"
	"End\n"
	"Object P124DroppedRing\n"
	" KindOf = CRATE ONE_RING NO_COLLIDE\n"
	" Geometry = CYLINDER\n GeometryMajorRadius = 2\n GeometryHeight = 2\n"
	" Behavior = AttachUpdate Attach\n"
	"  ObjectFilter = ANY +P124Carrier\n"
	"  ScanRange = 10\n"
	"  ParentStatus = HOLDING_THE_RING\n"
	"  AnchorToTopOfGeometry = Yes\n"
	" End\n"
	"End\n";

void orderThroughHud(CombatWorld &w, Object &carrier, Object &ringBearer){
	Player *player = w.playerOf('A');
	player->selection() = { carrier.getID() };
	w.players.setLocalPlayer(player);
	GarrisonCommands::registerHandlers(w.dispatcher);

	PinholeView view({ 300.0f, 100.0f, 500.0f }, { 400.0f, 400.0f, 0.0f }, 1024, 768, 0.7f);
	MouseSettings mouse;
	MetaMap meta;
	HudInput hud(*w.logic, w.ai.get(), view, w.commands, mouse, meta);
	hud.ui().mirrorSelection({ carrier.getID() });
	hud.commandTranslator().setUseAlternateMouse(true); // RotWK's right-click-orders setup.

	// The player's actual gesture, not a direct translator call: move the pointer over
	// the ringbearer, then press and release the right button (RotWK's alternate setup).
	ICoord2D pixel{};
	REQUIRE(view.worldToScreen(*ringBearer.getPosition(), pixel));
	hud.framePointer(pixel.x, pixel.y, 0);
	hud.update();
	hud.mouseMove(pixel.x, pixel.y, 0);
	size_t messages = hud.update();
	CHECK(messages == 0); // hover alone orders nothing
	CHECK(hud.ui().cursor() == MouseCursorName::DeliverRing);

	std::vector<GameMessage> observed;
	hud.setLogicMessageObserver([&](const GameMessage &message) { observed.push_back(message); });
	hud.mouseButton(HudInput::Button::Right, true, pixel.x, pixel.y, 0, 1000);
	hud.mouseButton(HudInput::Button::Right, false, pixel.x, pixel.y, 0, 1040);
	CHECK(hud.flush() == 1);
	REQUIRE(observed.size() == 1);
	CHECK(observed.front().getType() == MSG_ENTER);
	REQUIRE(observed.front().getArgumentCount() == 2);
	CHECK(observed.front().getArgument(0)->objectID == carrier.getID());
	CHECK(observed.front().getArgument(1)->objectID == ringBearer.getID());
}
} // namespace

// FXForRingEntry is resolved at parse time (RW 0x73A302), so the list store and the
// FXList block have to exist before the citadel object is loaded. The derived world
// registers them and loads its own data after the base fixture is up (as the FX
// lanes do, e.g. test_port_82.cpp).
struct Port124World : CombatWorld
{
	FXListStore fxLists;
	FXListStore *savedFx = TheFXListStore;
	explicit Port124World(const char *extra = "")
		: CombatWorld()
	{
		TheFXListStore = &fxLists;
		w.fx.env.blocks.registerBlock("FXList", [](INI *ini) { ParseFXListDefinitionGlobal(ini); });
		const std::string err = w.load(std::string(kPort124Data) + extra, INI_LOAD_OVERWRITE, "port124.ini");
		REQUIRE_MESSAGE(err.empty(), err);
	}
	~Port124World() { TheFXListStore = savedFx; }
};

TEST_CASE("PORT-124 an attached ring carrier enters a mobile ringbearer through the HUD MSG_ENTER path and receives the ring hand-off")
{
	Port124World w;
	w.logic->partition().setRegion(0.0f, 0.0f, 1000.0f, 1000.0f);
	Object *ringBearer = w.unit("P124RingBearer", 'A', 500.0f, 500.0f);
	Object *carrier = w.unit("P124Carrier", 'A', 350.0f, 500.0f);
	Object *ring = w.unit("P124DroppedRing", 'A', 350.0f, 500.0f);
	const ObjectID ringID = ring->getID();
	w.frames(3); // AttachUpdate scans the carrier and applies ParentStatus.

	REQUIRE(carrier->testStatus((unsigned)CombatNames::status("HOLDING_THE_RING")));
	auto *contain = dynamic_cast<CitadelSlaughterHordeContain *>(ringBearer->getContain());
	REQUIRE(contain != nullptr);
	// The supplied PassengerFilter admits only TheDroppedRing. The carrier must
	// enter via CitadelSlaughterHordeContain's ring-status branch, not the base filter.
	CHECK_FALSE(contain->openContainAllows(*carrier));
	CHECK(GarrisonRules::canEnterObject(*carrier, ringBearer, CMD_FROM_PLAYER, 0, false));
	orderThroughHud(w, *carrier, *ringBearer);

	const auto acceptedBefore = GarrisonCommands::stats().enterOrders;
	w.frames(1); // The one MSG_ENTER emitted above reaches the logic dispatcher.
	CHECK(GarrisonCommands::stats().enterOrders == acceptedBefore + 1);
	for (int frame = 0; frame < 600 && !ringBearer->hasUpgrade("Upgrade_RingHero"); ++frame)
	{
		w.frames(1);
	}
	CHECK(ringBearer->hasUpgrade("Upgrade_RingHero"));
	CHECK(ringBearer->hasUpgrade("Upgrade_FortressRingHero"));
	CHECK(w.byId(ringID) == nullptr);
	CHECK_FALSE(carrier->testStatus((unsigned)CombatNames::status("HOLDING_THE_RING")));
	// The carrier leaves on the update after the delivery (RW 0x480FE2 / the citadel's
	// update), so the exit is its own bounded wait and not the delivery's frame.
	for (int frame = 0; frame < 60 && carrier->getContainedBy() != nullptr; ++frame)
	{
		w.frames(1);
	}
	CHECK(carrier->getContainedBy() == nullptr);
	CHECK(carrier->isInWorld());
	CHECK_FALSE(carrier->isEffectivelyDead());
}

TEST_CASE("PORT-124 the ring-entry override still refuses an ordinary passenger without HOLDING_THE_RING")
{
	Port124World w;
	Object *ringBearer = w.unit("P124RingBearer", 'A', 500.0f, 500.0f);
	Object *ordinary = w.unit("P124Carrier", 'A', 350.0f, 500.0f);
	auto *contain = dynamic_cast<CitadelSlaughterHordeContain *>(ringBearer->getContain());
	REQUIRE(contain != nullptr);
	CHECK_FALSE(GarrisonRules::canEnterObject(*ordinary, ringBearer, CMD_FROM_PLAYER, 0, false));
	CHECK_FALSE(contain->isValidContainerFor(*ordinary, true, false));
}

// Second synthetic carrier: the same object without TransportSlotCount. On this template
// template + 0x601 reads 0 (the field is simply absent), and the HUD's enter test is the
// capacity mode, where RW 0x82CE99 reads it before the container is asked (RW 0x69029B).
// The donor's screen path is the check with additionalChecking=true (BFME2
// InGameUICanSelectedObjectsDoAction.cpp action 8 -> CHECK_CAPACITY), so this pins the
// port's gate: with the field the ring branch decides, without it the order never exists.
const char *const kNoSlotCarrier =
	"Object P124CarrierNoSlot\n"
	" KindOf = INFANTRY HERO SELECTABLE SCORE\n"
	" Geometry = CYLINDER\n GeometryMajorRadius = 5\n GeometryHeight = 10\n"
	" Body = ActiveBody Body\n MaxHealth = 100\n End\n"
	" Behavior = AIUpdateInterface AI\n End\n"
	" LocomotorSet\n Locomotor = WalkerLoco\n Condition = SET_NORMAL\n Speed = 55\n End\n"
	"End\n"
	// Its own ring object: the filter names this carrier, and both live in one load (a
	// later load cannot extend the filter of an already parsed AttachUpdate).
	"Object P124DroppedRingNoSlot\n"
	" KindOf = CRATE ONE_RING NO_COLLIDE\n"
	" Geometry = CYLINDER\n GeometryMajorRadius = 2\n GeometryHeight = 2\n"
	" Behavior = AttachUpdate Attach\n"
	"  ObjectFilter = ANY +P124CarrierNoSlot\n"
	"  ScanRange = 10\n"
	"  ParentStatus = HOLDING_THE_RING\n"
	"  AnchorToTopOfGeometry = Yes\n"
	" End\n"
	"End\n";

TEST_CASE("PORT-124 a carrier without TransportSlotCount is refused by the HUD enter test before the ring branch")
{
	Port124World w(kNoSlotCarrier);
	Object *ringBearer = w.unit("P124RingBearer", 'A', 500.0f, 500.0f);
	Object *carrier = w.unit("P124CarrierNoSlot", 'A', 350.0f, 500.0f);
	w.unit("P124DroppedRingNoSlot", 'A', 350.0f, 500.0f);
	w.frames(3); // AttachUpdate puts HOLDING_THE_RING on the carrier
	REQUIRE(carrier->testStatus((unsigned)CombatNames::status("HOLDING_THE_RING")));

	CHECK_FALSE(GarrisonRules::canEnterObject(*carrier, ringBearer, CMD_FROM_PLAYER, 0, false)); // screen path (capacity)
	CHECK(GarrisonRules::canEnterObject(*carrier, ringBearer, CMD_FROM_PLAYER, 1, false));       // AI path (no capacity)
	auto *contain = dynamic_cast<CitadelSlaughterHordeContain *>(ringBearer->getContain());
	REQUIRE(contain != nullptr);
	CHECK(contain->isValidContainerFor(*carrier, true, false)); // the ring branch itself admits it

	Player *player = w.playerOf('A');
	player->selection() = { carrier->getID() };
	w.players.setLocalPlayer(player);
	GarrisonCommands::registerHandlers(w.dispatcher);
	PinholeView view({ 300.0f, 100.0f, 500.0f }, { 400.0f, 400.0f, 0.0f }, 1024, 768, 0.7f);
	MouseSettings mouse;
	MetaMap meta;
	HudInput hud(*w.logic, w.ai.get(), view, w.commands, mouse, meta);
	hud.ui().mirrorSelection({ carrier->getID() });
	hud.commandTranslator().setUseAlternateMouse(true);
	ICoord2D pixel{};
	REQUIRE(view.worldToScreen(*ringBearer->getPosition(), pixel));
	hud.framePointer(pixel.x, pixel.y, 0);
	hud.update();
	hud.mouseMove(pixel.x, pixel.y, 0);
	hud.update();
	CHECK(hud.ui().cursor() != MouseCursorName::DeliverRing);
	std::vector<GameMessage> observed;
	hud.setLogicMessageObserver([&](const GameMessage &message) { observed.push_back(message); });
	hud.mouseButton(HudInput::Button::Right, true, pixel.x, pixel.y, 0, 1000);
	hud.mouseButton(HudInput::Button::Right, false, pixel.x, pixel.y, 0, 1040);
	CHECK(hud.flush() == 0);
	CHECK(observed.empty());
}
