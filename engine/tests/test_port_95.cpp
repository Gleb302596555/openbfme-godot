// OpenBFME unit tests. GPL-3.0.
// Port 95 (issue #243): KeepObjectDie/StayOnRadar — the remains of a destroyed building stay on the minimap.
//
// RW 0x889FA7 KeepObjectDie::onDie, read with tools/mod/rw.py (RotWK 2.01 game.dat, caveat S-001):
//   * RW 0x889FAB .. 0x889FD8: an object whose controlling player (RW 0x68B678) has Player + 0x754 set (Player::isPlayerActive, RW 0x6AAC4B) is destroyed at once
//     (TheGameLogic's destroyObject RW 0x62BBAB) and the onDie ends;
//   * RW 0x889FDB .. 0x889FE8: setSpecialModelConditionState (RW 0x68B581) with model condition 0x14F (COLLAPSING, TheModelConditionNames[335]) and data + 0x38
//     (CollapsingTime) frames;
//   * RW 0x889FED: isDieApplicable (RW 0x85FED5) — the DieMux rows RW 0xC76BD8 decide whether this death reaches the module;
//   * RW 0x889FFD .. 0x88A004: StayOnRadar (data + 0x3C, the row RW 0x42E558 of the table RW 0xC06C2C) 0: nothing more;
//   * RW 0x88A006 .. 0x88A011: no object, or the radar record (Object + 0x268) already set: nothing more;
//   * RW 0x88A013 .. 0x88A01A: TheRadar (RW 0xDE4AB8) addObject (RW 0x6D9042): the remains are on the radar.
// Object::onDie RW 0x698F06 at RW 0x698F70 .. 0x698F80 takes the record off when it is set (TheRadar's removeObject RW 0x6D876A), BEFORE the die modules
// (the module walk is RW 0x698F86 .. 0x698FA1), so the add in the KeepObjectDie's onDie is what keeps a dead object on the radar.
// The radar's live list is the port's Radar::captureObject (Radar.cpp, RADAR-1: what the radar lists of a live object); the tests drive it, and the radar's
// own blip pass (Radar::renderObjects), which the game draws from.
#include "doctest.h"

#include "ProdTestUtil.h"

#include "Common/GameCommon.h"
#include "GameClient/Radar.h"
#include "GameLogic/Damage.h"
#include "Common/Module.h"
#include "GameLogic/Module/CombatModules.h"
#include "GameLogic/Object/Object.h"

using namespace prodtest;

namespace
{
// The keeper: a SELECTABLE structure with an ActiveBody (so a real damage death reaches the die modules) and a KeepObjectDie whose CollapsingTime is the data's
// default 0x19 (25 frames, the ctor RW 0x65391A writes 0x19) or none, and StayOnRadar on or off. The keeper's template keeps a radar entry: a structure whose
// controlling player is the radar's local player (RW 0x6D9042's gates; the property is the port's, RADAR-1).
std::string keeperIni(const char *name, const char *stayOnRadar, const char *collapsingTime)
{
	std::string text = std::string("Object ") + name + "\n"
		"  KindOf = STRUCTURE SELECTABLE IMMOBILE\n"
		"  Geometry = CYLINDER\n"
		"  GeometryMajorRadius = 30\n"
		"  GeometryHeight = 60\n"
		"  ArmorSet\n"
		"    Conditions = None\n"
		"    Armor = NoArmor\n"
		"  End\n"
		"  Body = ActiveBody ModuleTag_Body\n"
		"    MaxHealth = 100\n"
		"  End\n"
		"  Behavior = KeepObjectDie ModuleTag_Keep\n";
	if (stayOnRadar)
	{
		text += std::string("    StayOnRadar = ") + stayOnRadar + "\n";
	}
	if (collapsingTime)
	{
		text += std::string("    CollapsingTime = ") + collapsingTime + "\n";
	}
	text += "  End\n"
		"End\n";
	return text;
}

// the mod's two cases (10 uses, neutral/inn.ini:196 StayOnRadar = Yes) and the neighbour that keeps the object and not the blip
const char kPort95Objects[] =
	"Object P95Inn\n"
	"  KindOf = STRUCTURE SELECTABLE IMMOBILE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 30\n"
	"  GeometryHeight = 60\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = NoArmor\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = KeepObjectDie ModuleTag_Keep\n"
	"    StayOnRadar = Yes\n"
	"  End\n"
	"End\n"
	"Object P95Shed\n"
	"  KindOf = STRUCTURE SELECTABLE IMMOBILE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 30\n"
	"  GeometryHeight = 60\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = NoArmor\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = KeepObjectDie ModuleTag_Keep\n"
	"    StayOnRadar = No\n"
	"  End\n"
	"End\n"
	// no KeepObjectDie at all: the object leaves the world with its record
	"Object P95Post\n"
	"  KindOf = STRUCTURE SELECTABLE IMMOBILE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 30\n"
	"  GeometryHeight = 60\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = NoArmor\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"End\n";

struct Port95World : ProdWorld
{
	Port95World()
	{
		load(kPort95Objects);
		logic->settings().bodyThresholdsLoaded = true; // MoveTestUtil's convention: the bodies' damage states need no GameData
		local = alice();                              // the radar's local player (Radar::captureObject's `local`)
	}
	Player *local = nullptr;

	// the radar as RADAR-1 keeps it of the object: what the game's blip pass draws (Radar::renderObjects goes through the same entry)
	ObjectSnapshot::RadarEntry capture(Object &o)
	{
		ObjectSnapshot::RadarEntry e;
		Radar::captureObject(*logic, o, local, e);
		return e;
	}
	// a real damage death: the body's death calls Object::friend_onDie, which runs every die module
	void kill(Object &o)
	{
		DamageInfo info;
		info.m_input.m_damageType = DAMAGE_UNRESISTABLE;
		info.m_input.m_deathType = DEATH_NORMAL;
		info.m_input.m_amount = 1000.0f;
		o.attemptDamage(info);
	}
};

const char *kCollapsing = "COLLAPSING";
} // namespace

TEST_CASE("PORT-95 KeepObjectDie/StayOnRadar: the remains of a destroyed structure stay on the radar, without it they leave")
{
	Port95World w;
	Object *inn = w.make("P95Inn", w.teamOf("Alice"));
	Object *shed = w.make("P95Shed", w.teamOf("Alice"));
	REQUIRE(inn != nullptr);
	REQUIRE(shed != nullptr);
	Coord3D a{ 700.0f, 700.0f, 0.0f };
	Coord3D b{ 900.0f, 700.0f, 0.0f };
	inn->setPosition(&a);
	shed->setPosition(&b);
	// alive: both are listed (the radar's own record of a live object, RW 0x6D9042's add of an alive SELECTABLE structure of the local player)
	CHECK(w.capture(*inn).listed);
	CHECK(w.capture(*shed).listed);

	w.kill(*inn);
	w.kill(*shed);
	REQUIRE(inn->isEffectivelyDead()); // the ActiveBody's death ran the die modules (Object::friend_onDie RW 0x698F06)
	REQUIRE(shed->isEffectivelyDead());
	REQUIRE_FALSE(inn->isDestroyed()); // KeepObjectDie keeps the object in the world (no DestroyDie on the template)

	// RW 0x889FFD: StayOnRadar = Yes -> the object is on the radar again (RW 0x88A01A); StayOnRadar = No -> it is not
	CHECK(inn->stayOnRadarAfterDeath());
	CHECK_FALSE(shed->stayOnRadarAfterDeath());
	CHECK(w.capture(*inn).listed);      // the blip of the remains
	CHECK_FALSE(w.capture(*shed).listed); // Object::onDie RW 0x698F80 took the record off and nothing added it back
	// the blip keeps the structure's shape and the owner's priority (the add RW 0x6D9042 asks the same questions of a live and of a kept object)
	CHECK(w.capture(*inn).shape == Radar::SHAPE_DOT);
	CHECK(w.capture(*inn).priority == Radar::RADAR_PRIORITY_STRUCTURE);
	CHECK(w.capture(*inn).local);
	// the body's death also sets the model condition RW 0x889FE8 asked for; the next logic frame keeps it in the object's conditions
	w.frames(1);
	CHECK(inn->testModelCondition(CombatNames::modelCondition(kCollapsing)));
}

TEST_CASE("PORT-95 neighbour: an object without StayOnRadar leaves the radar at once, and a save keeps the kept object's row")
{
	Port95World w;
	Object *post = w.make("P95Post", w.teamOf("Alice"));
	Object *inn = w.make("P95Inn", w.teamOf("Alice"));
	REQUIRE(post != nullptr);
	REQUIRE(inn != nullptr);
	Coord3D a{ 700.0f, 700.0f, 0.0f };
	Coord3D b{ 900.0f, 700.0f, 0.0f };
	post->setPosition(&a);
	inn->setPosition(&b);
	CHECK(w.capture(*post).listed);

	w.kill(*post);
	w.kill(*inn);
	// a template with no KeepObjectDie is simply destroyed by its body's death and the radar drops it (RW 0x698F80)
	CHECK(post->isEffectivelyDead());
	CHECK_FALSE(w.capture(*post).listed);
	CHECK_FALSE(post->stayOnRadarAfterDeath());
	CHECK(w.capture(*inn).listed);

	// lane SAVE-4b: the decision is simulation state, so it travels in the object's snapshot (Object::xfer)
	const std::uint32_t before = w.logic->computeStateHash();
	inn->setStayOnRadarAfterDeath(false);
	CHECK(w.logic->computeStateHash() != before);
	inn->setStayOnRadarAfterDeath(true);
	CHECK(w.logic->computeStateHash() == before);
}

TEST_CASE("PORT-95 construction: KeepObjectDie's CollapsingTime (data + 0x38) is RW's, and StayOnRadar (data + 0x3C) parses to the module data")
{
	Port95World w;
	// the data ctor RW 0x65391A: CollapsingTime 0x19 (25 frames) and StayOnRadar 0 (RW 0x6538E9's stores); a template that names only StayOnRadar keeps 25
	const std::string text = keeperIni("P95Defaults", "Yes", nullptr) + keeperIni("P95Timed", "Yes", "40");
	CHECK(w.w.load(text).empty());
	for (const char *name : { "P95Defaults", "P95Timed" })
	{
		Object *o = w.make(name, w.teamOf("Alice"));
		REQUIRE(o != nullptr);
		const KeepObjectDie *keep = dynamic_cast<const KeepObjectDie *>(o->findModule("KeepObjectDie"));
		REQUIRE(keep != nullptr);
		const KeepObjectDieModuleData *data = dynamic_cast<const KeepObjectDieModuleData *>(keep->getModuleData());
		REQUIRE(data != nullptr);
		CHECK(data->m_stayOnRadar);
		CHECK(data->m_collapsingTime == (std::string(name) == "P95Timed" ? 40u : 0x19u));
	}
}
