// OpenBFME unit tests, lane PORT-247 (fork issue 571): TeleportSpecialAbilityUpdate's MaxDistance, the one field of the three the issue lists that
// RotWK 2.01 reads (FlingPassengerSpecialAbilityUpdate/FlingPassengerVelocity and AutoAbilityBehavior/WorkingRadius are parsed and never read;
// docs/ennorath/PORT-247-RW-ANALYSIS.md).
//
// Every expectation is read from vanilla RotWK 2.01 `game.dat` with tools/mod/rw.py dis, never from this engine:
//   * the field table RW 0xC054B0 (rows at 0xC054B0 .. 0xC054E0, sentinel 0xC054F0) puts `MaxDistance` (parser RW 0x42ED00, a plain real) at
//     data + 0xDC; the class's special power update table RW 0xC6490C overrides slot 8 (offset 0x20) with RW 0x896608, where the base table
//     RW 0xC64658 has RW 0x851B6E.
//   * RW 0x896608 (read linearly): a null place (0x89660C), a null data pointer (0x896611), a null object (0x896629) and MaxDistance at or below
//     zero (0x896618 against the zero RW 0xC1B594) fall back to the base; otherwise |place - position|^2 (0x896630 .. 0x896675 over the object's
//     + 0x38 / + 0x3C / + 0x40) above MaxDistance^2 (0x896679, jbe) answers no (0x89667E); within it the base answers (0x896682).
//   * the place reaches the slot from the location requirements RW 0x897E01: 0x897E14 takes the requirements' first argument, 0x897E18 allows it
//     to be null, 0x897E7F pushes it into `call [eax + 0x20]` at 0x897E82. The object form RW 0x82DAAB passes none, so the null branch is real.
//   * the numbers are the mod's own shape: every Ennorath power of a TeleportSpecialAbilityUpdate (`SpecialAbilityWildShelobTunnel`,
//     `SpecialAbilityKarshBlink`, the blink and reposition powers) takes `Enum = SPECIAL_BALROG_WINGS`; the mod's `MaxDistance` values are
//     WILD_SHELOB_TUNNEL_DISTANCE and 1000 / 500. The test writes its own smaller value so the map's edge (ActionManager RW 0x82D56A) or the
//     cursor radius is not what answers.
//   * `SPECIAL_BALROG_WINGS` is type 0x41 (name list RW 0xDA5C90), whose per-type location test RW 0x82E252 is not ported (stop S-1874): the
//     ActionManager refuses that type before any module is asked, in retail and in the port alike today. The end-to-end cases therefore use a
//     type the port answers true for (0x34, `SPECIAL_ROUSING_SPEECH`, the FINAL_YES row of RW 0x82ED8E) so that the module is what decides, and
//     the mod's own type is exercised through the interface method the requirements call.

#include "doctest.h"
#include "CombatTestUtil.h"

#include "Common/SpecialPower.h"
#include "GameLogic/ActionManager.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/HeroAbilityModules.h"
#include "GameLogic/Module/SpecialAbilityModules.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/Object/Object.h"

#include <string>

namespace
{
// CombatTestUtil's CombatWorld plus the special power store and the two module sets these abilities need.
struct Port247World : combattest::CombatWorld
{
	SpecialPowerStore powers;
	SpecialPowerStore *savedPowers = TheSpecialPowerStore;
	explicit Port247World(const char *objects) : combattest::CombatWorld()
	{
		TheSpecialPowerStore = &powers;
		w.fx.env.blocks.registerBlock("SpecialPower", [](INI *ini) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(ini); });
		SpecialAbilityModules::registerAll(w.modules);
		SpecialPowerModules::registerAll(w.modules);
		HeroAbilityModules::registerAll(w.modules);
		const std::string err = w.load(objects);
		REQUIRE_MESSAGE(err.empty(), err);
	}
	~Port247World() { TheSpecialPowerStore = savedPowers; }
};

// A hero shaped like the mod's teleporting heroes (Shelob, Karsh, the drakes): a body, an AI, the power and the ability that drives it.
#define PORT247_HERO(name, power, abilityLines)                                                                                                  \
	"Object " name "\n"                                                                                                                         \
	" KindOf = INFANTRY SELECTABLE HERO\n"                                                                                                      \
	" Geometry = CYLINDER\n GeometryMajorRadius = 8\n GeometryMinorRadius = 8\n GeometryHeight = 20\n"                                          \
	" Body = ActiveBody BodyTag\n  MaxHealth = 100\n End\n"                                                                                     \
	" Behavior = AIUpdateInterface AiTag\n End\n"                                                                                               \
	" Behavior = SpecialPowerModule PowerTag\n  SpecialPowerTemplate = " power "\n End\n"                                                       \
	" Behavior = TeleportSpecialAbilityUpdate TunnelTag\n"                                                                                      \
	"  SpecialPowerTemplate = " power "\n"                                                                                                     \
	"  UnpackTime = 400\n  PackTime = 400\n  ApproachRequiresLOS = No\n  BusyForDuration = 1800\n" abilityLines " End\nEnd\n"

const char kPort247[] =
	"SpecialPower SpecialAbilityTestTunnel\n  Enum = SPECIAL_BALROG_WINGS\n  ReloadTime = 1000\nEnd\n"
	"SpecialPower SpecialAbilityTestTunnelLocation\n  Enum = SPECIAL_ROUSING_SPEECH\n  ReloadTime = 1000\nEnd\n"
	PORT247_HERO("TunnelHero", "SpecialAbilityTestTunnel", "  MaxDistance = 200\n")
	PORT247_HERO("LocHero", "SpecialAbilityTestTunnelLocation", "  MaxDistance = 200\n")
	PORT247_HERO("NoLimitHero", "SpecialAbilityTestTunnelLocation", "") // no MaxDistance: the parse leaves + 0xDC zero (the data constructor RW 0x89870E zeroes + 0xD0 .. + 0xD8)
	PORT247_HERO("MountedHero", "SpecialAbilityTestTunnelLocation", "  MaxDistance = 200\n  RequiredConditions = MOUNTED\n");

struct Rig
{
	Port247World f;
	Object *hero = nullptr;
	TeleportSpecialAbilityUpdate *tp = nullptr;
	const TeleportSpecialAbilityUpdateModuleData *data = nullptr;
	explicit Rig(const char *name, float x = 300.0f, float y = 300.0f) : f(kPort247)
	{
		hero = f.unit(name, 'A', x, y);
		REQUIRE(hero);
		tp = dynamic_cast<TeleportSpecialAbilityUpdate *>(hero->findModule("TeleportSpecialAbilityUpdate"));
		REQUIRE(tp);
		data = dynamic_cast<const TeleportSpecialAbilityUpdateModuleData *>(tp->abilityData());
		REQUIRE(data);
		REQUIRE(data->m_specialPowerTemplate);
	}
};
} // namespace

// RW 0x896608: the module refuses a place further than MaxDistance from the object, and answers the base's yes inside it. The far case fails on
// the module alone, so this holds the reader itself; a null place takes the 0x89660C branch to the base.
TEST_CASE("PORT-247 teleport: the module refuses a place beyond MaxDistance, one inside it passes (RW 0x896608)")
{
	Rig r("TunnelHero");
	CHECK(r.data->m_maxDistance == 200.0f);
	const Coord3D near = { 400.0f, 300.0f, 0.0f }; // 100 from the object
	const Coord3D far = { 700.0f, 300.0f, 0.0f };  // 400 from the object
	CHECK(r.tp->allowsSpecialPowerUse(&near));
	CHECK_FALSE(r.tp->allowsSpecialPowerUse(&far)); // RW 0x89667E
	CHECK(r.tp->allowsSpecialPowerUse(nullptr));    // RW 0x89660C: no place given, the base's answer
}

// The place reaches the module through the location requirements RW 0x897E01 (0x897E82 pushes it): a place beyond MaxDistance stops the whole
// command, one inside it passes. Before the change the far place passed, because the base never reads the place.
TEST_CASE("PORT-247 teleport: the location requirement cuts the place by MaxDistance (RW 0x897E82 / 0x896608)")
{
	Rig r("LocHero");
	const Coord3D near = { 400.0f, 300.0f, 0.0f };
	const Coord3D far = { 700.0f, 300.0f, 0.0f };
	CHECK(ActionManager::canDoSpecialPowerAtLocation(*r.hero, near, r.data->m_specialPowerTemplate, 0u));
	CHECK_FALSE(ActionManager::canDoSpecialPowerAtLocation(*r.hero, far, r.data->m_specialPowerTemplate, 0u));
}

// The neighbours the override must not change: with no MaxDistance the data's zero takes the 0x896620 branch back to the base, so the same far
// place passes; and the base's RequiredConditions (RW 0x851551) still refuse a place inside MaxDistance when the object does not meet them.
TEST_CASE("PORT-247 teleport: without MaxDistance the far place passes and the base's conditions still rule (RW 0x896620 / 0x851B6E)")
{
	Rig free("NoLimitHero");
	CHECK(free.data->m_maxDistance == 0.0f);
	const Coord3D far = { 700.0f, 300.0f, 0.0f };
	CHECK(free.tp->allowsSpecialPowerUse(&far));
	CHECK(ActionManager::canDoSpecialPowerAtLocation(*free.hero, far, free.data->m_specialPowerTemplate, 0u));
	// MOUNTED is a model condition the hero does not have: the base's RW 0x851551 refuses even the place inside MaxDistance
	Rig mounted("MountedHero", 300.0f, 400.0f);
	CHECK(mounted.data->m_maxDistance == 200.0f);
	const Coord3D nearMounted = { 400.0f, 400.0f, 0.0f };
	CHECK_FALSE(mounted.tp->allowsSpecialPowerUse(&nearMounted));
	CHECK_FALSE(ActionManager::canDoSpecialPowerAtLocation(*mounted.hero, nearMounted, mounted.data->m_specialPowerTemplate, 0u));
}
