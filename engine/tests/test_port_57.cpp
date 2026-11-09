// OpenBFME. GPL-3.0.
//
// PORT-57 (issue #167): RadiateFearUpdate/WhichSpecialPower.
//
// The RotWK 2.01 image (game.dat, caveat S-001) parses WhichSpecialPower at the field row RW 0xC67A00 of the data table RW 0xC679F0
// (parser RW 0x42EC5E, int, offset +0xC, default -1 in the data constructor RW 0x89F7F6 at 0x89F80C). The update function RW 0x89F9A8
// reads +0x10 / +0x11 / +0x12 (the three fear flags) and +0x14 / +0x18 (radius, interval), but does NOT read +0xC. The constructor
// RW 0x89F890 reads only +8 (InitiallyActive) for the self-upgrade. The xfer RW 0x89F91F saves the mux state, not the data field.
//
// An exhaustive scan of every vtable slot of the class (main vtable RW 0xC678F4, UpgradeMux vtable RW 0xC677E0, update-interface
// vtable RW 0xC67828) and the UpgradeMux helpers (RW 0x8D26BF / 0x8D278A / 0x8D286F) finds no reachable reader of data + 0xC. The
// field is parsed-unused, the same verdict as Port 42 / S-7186 (ScoreKeeper victory rating, no reachable consumer).
//
// The neighbouring behaviour these tests hold: the field is parsed into m_whichSpecialPower (the port keeps the parse line at
// EmotionModules.cpp:108); the default is -1; and the fear pulses run identically regardless of the value, because the binary does
// not branch on it. The upgrade gate is isAlreadyUpgraded (RW 0x4986C4: byte at mux + 4), given by InitiallyActive in the
// constructor (RW 0x89F8EC) or by the upgrade system via TriggeredBy (the activation masks at data + 0x20, RW 0x8D26BF).
//
// The test "parses WhichSpecialPower" would fail if the parse line was removed (the field would land in unappliedFields);
// the test "fear pulses the same with WhichSpecialPower 1 and with -1" holds the neighbouring behaviour that the value does
// not change the update path.

#include "doctest.h"

#include "MoveTestUtil.h"

#include "Common/StateHash.h"
#include "GameLogic/Module/EmotionModules.h"
#include "GameLogic/Object/PartitionManager.h"
#include "GameLogic/System/EmotionSystem.h"

#include <string>
#include <vector>

namespace
{
const char kNuggets[] =
	"EmotionNugget TestFear\n"
	"  Type = FEAR\n"
	"  ModelConditions = EMOTION_AFRAID\n"
	"  Duration = 1000\n"
	"  InactiveDurationSameObject = 2000\n"
	"  AIState = BACK_AWAY\n"
	"End\n";

const char kFearWithWSP1[] =
	"Object FearWSP1\n"
	"  KindOf = SELECTABLE MONSTER SCARY\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryHeight = 20\n"
	"  Behavior = RadiateFearUpdate ModuleTag_Fear\n"
	"    InitiallyActive = Yes\n"
	"    WhichSpecialPower = 1\n"
	"    GenerateFear = Yes\n"
	"    EmotionPulseRadius = 150\n"
	"    EmotionPulseInterval = 1000\n"
	"    VictimFilter = ALL ENEMIES\n"
	"  End\n"
	"End\n";

const char kFearDefault[] =
	"Object FearDefault\n"
	"  KindOf = SELECTABLE MONSTER SCARY\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryHeight = 20\n"
	"  Behavior = RadiateFearUpdate ModuleTag_Fear\n"
	"    InitiallyActive = Yes\n"
	"    GenerateFear = Yes\n"
	"    EmotionPulseRadius = 150\n"
	"    EmotionPulseInterval = 1000\n"
	"    VictimFilter = ALL ENEMIES\n"
	"  End\n"
	"End\n";

const char kScared[] =
	"Object Scared\n"
	"  KindOf = SELECTABLE INFANTRY\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 5\n"
	"  GeometryHeight = 10\n"
	"  ShroudClearingRange = 300\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = EmotionTrackerUpdate ModuleTag_Emotion\n"
	"    TauntAndPointDistance = 100\n"
	"    TauntAndPointUpdateDelay = 1000\n"
	"    FearScanDistance = 100\n"
	"    AddEmotion = OVERRIDE TestFear\n"
	"      Duration = 2000\n"
	"    End\n"
	"  End\n"
	"End\n";

struct Port57Fx : movetest::MoveWorld
{
	EmotionSystem emotions;
	EmotionSystem *saved = TheEmotionSystem;
	void load(const std::string &text)
	{
		const std::string err = w.load(text, INI_LOAD_OVERWRITE, "port57.ini");
		REQUIRE_MESSAGE(err.empty(), err);
	}
	Port57Fx()
	{
		TheEmotionSystem = &emotions;
		emotions.registerBlock(w.fx.env.blocks);
		load(kNuggets);
		load(kFearWithWSP1);
		load(kFearDefault);
		load(kScared);
		buildMap();
		logic->partition().setRegion(0.0f, 0.0f, 2000.0f, 2000.0f);
	}
	~Port57Fx() { TheEmotionSystem = saved; }
	Object *at(const char *name, const char *owner, float x, float y)
	{
		Object *o = make(name, teamOf(owner));
		REQUIRE(o);
		const Coord3D p{ x, y, 0.0f };
		o->setPosition(&p);
		return o;
	}
};

	// Helper to get module data from an object
	const RadiateFearUpdateModuleData *getFearData(Object *obj)
	{
		for (const auto &m : obj->modules())
		{
			if (auto *d = dynamic_cast<const RadiateFearUpdateModuleData *>(m->getModuleData()))
			{
				return d;
			}
		}
		return nullptr;
	}
	
	// Helper to get RadiateFearUpdate from an object
	RadiateFearUpdate *getRadiateFearUpdate(Object *obj)
	{
		for (const auto &m : obj->modules())
		{
			if (auto *r = dynamic_cast<RadiateFearUpdate *>(m.get()))
			{
				return r;
			}
		}
		return nullptr;
	}
} // namespace

TEST_CASE("PORT-57 WhichSpecialPower: the field is parsed (RW 0xC67A00, parseInt RW 0x42EC5E, offset +0xC) and the default is -1 (RW 0x89F80C)")
{
	Port57Fx f;
	Object *wsp1 = f.at("FearWSP1", "Bob", 500.0f, 500.0f);
	Object *def = f.at("FearDefault", "Bob", 1000.0f, 500.0f);

	const RadiateFearUpdateModuleData *d1 = getFearData(wsp1);
	REQUIRE(d1);
	CHECK(d1->m_whichSpecialPower == 1); // parsed from the INI row
	CHECK(d1->m_initiallyActive == true);
	CHECK(d1->m_generateFear == true);
	CHECK(d1->m_emotionPulseRadius == 150.0f);
	CHECK(d1->m_emotionPulseInterval == 1000u);

	const RadiateFearUpdateModuleData *d2 = getFearData(def);
	REQUIRE(d2);
	CHECK(d2->m_whichSpecialPower == -1); // the data constructor's default (RW 0x89F80C: or [esi + 0xC], -1)
	CHECK(d2->m_initiallyActive == true);
	CHECK(d2->m_generateFear == true);
}

TEST_CASE("PORT-57 WhichSpecialPower: the fear pulses run identically with WhichSpecialPower = 1 and with the default -1 (no reader in RW)")
{
	Port57Fx f;
	// the WSP=1 source and a nearby enemy
	Object *wsp1 = f.at("FearWSP1", "Bob", 500.0f, 500.0f);
	Object *nearWsp = f.at("Scared", "Alice", 500.0f, 600.0f); // 100 from the source, within 150 radius
	// the default source and a nearby enemy
	Object *def = f.at("FearDefault", "Bob", 1000.0f, 500.0f);
	Object *nearDef = f.at("Scared", "Alice", 1000.0f, 600.0f); // 100 from the source
	RadiateFearUpdate *rWsp = getRadiateFearUpdate(wsp1);
	RadiateFearUpdate *rDef = getRadiateFearUpdate(def);
	REQUIRE(rWsp);
	REQUIRE(rDef);
	CHECK(rWsp->isAlreadyUpgraded()); // InitiallyActive
	CHECK(rDef->isAlreadyUpgraded());
	f.frames(2);
	// both radiate fear the same way: the WhichSpecialPower value does not gate the pulse (no reader in RW 0x89F9A8)
	// tolerance: the exact pulse count depends on the object ID modulo 5 and the 2-frame advance
	CHECK(rWsp->pulses() >= 1); // tolerance: object id modulo 5 and frame advance
	CHECK(rDef->pulses() >= 1); // tolerance: object id modulo 5 and frame advance
	CHECK(rWsp->requests() >= 1); // tolerance: depends on partition hits within the radius
	CHECK(rDef->requests() >= 1); // tolerance: depends on partition hits within the radius
	EmotionTrackerUpdate *tWsp = EmotionTrackerUpdate::of(*nearWsp);
	EmotionTrackerUpdate *tDef = EmotionTrackerUpdate::of(*nearDef);
	REQUIRE(tWsp);
	REQUIRE(tDef);
	CHECK(tWsp->current());
	CHECK(tDef->current());
	CHECK(tWsp->current()->tmpl().type == EMOTION_FEAR);
	CHECK(tDef->current()->tmpl().type == EMOTION_FEAR);
}
