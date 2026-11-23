// OpenBFME. GPL-3.0.
// PORT-341 (fork issue 896): client draw modules - the uncited own functions of W3DFloorDraw, W3DTreeDraw, W3DDebrisDraw,
// W3DHordeModelDraw and W3DPropDraw.
// Sources (RW = RotWK 2.01 game.dat):
//   - W3DFloorDraw record: RW 0x004CE91B ctor (+0x14 from StartHidden +0x1E at RW 0x4CE950..0x4CE955, +0x15..+0x17 cleared at
//     RW 0x4CE93D..0x4CE94D), RW 0x004CEA1C weather texture, RW 0x004CEA6F first transform change, RW 0x004CEADF record refresh
//     (clear RW 0x46AF77 -> RW 0x4E580B, three hide gates RW 0x4CEB1F..0x4CEB2B, hand-over RW 0x467B4D -> RW 0x4E592B),
//     RW 0x004CE8CA the (flags & mask) == mask test, RW 0x004CEB80 replaceModelConditionState, RW 0x004CECA1 setSubObjectHidden,
//     RW 0x004CECB5 setHidden.
//   - W3DPropDraw record: RW 0x004CE793 (the first position, never updated afterwards) -> RW 0x467AD4 -> RW 0x4EE5B8.
//   - Ennorath INI 1.9.8.7: data/ini/object/evilfaction/structures/angmar/angmarbarracks.ini:365-371 (the floor body below) and
//     data/ini/object/civilian/civilianprop.ini:112-116 (the prop body below).

#include "doctest.h"

#include "Common/INI.h"
#include "Common/ModelState.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DTreeDraw.h"
#include "W3DDrawTestUtil.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace
{

// Parses a Draw body with the module's own field tables (the retail registry's initFromINIMultiProc; W3DDrawTestUtil's Harness covers
// only the scripted / horde / default classes).
struct TreeDrawParser
{
	INIEnvironment env;
	std::unique_ptr<W3DFloorDrawModuleData> floor;
	std::unique_ptr<W3DPropDrawModuleData> prop;
	std::string error;

	TreeDrawParser()
	{
		env.blocks.registerBlock("TestFloor", [this](INI *ini) {
			floor.reset(new W3DFloorDrawModuleData());
			ini->initFromINIMultiProc(floor.get(), W3DFloorDrawModuleData::buildFieldParse);
			while (!ini->isEOF())
			{
				ini->readLine();
			}
		});
		env.blocks.registerBlock("TestProp", [this](INI *ini) {
			prop.reset(new W3DPropDrawModuleData());
			ini->initFromINIMultiProc(prop.get(), W3DPropDrawModuleData::buildFieldParse);
			while (!ini->isEOF())
			{
				ini->readLine();
			}
		});
	}

	template <typename T>
	T *run(const char *block, const std::string &body, std::unique_ptr<T> &slot)
	{
		slot.reset();
		error.clear();
		const std::string full = std::string(block) + "\n" + body;
		INI ini(env);
		try
		{
			ini.loadMemory("test.ini", std::vector<std::uint8_t>(full.begin(), full.end()), INI_LOAD_OVERWRITE);
		}
		catch (const INIException &e)
		{
			error = e.message();
			slot.reset();
		}
		return slot.get();
	}

	W3DFloorDrawModuleData *parseFloor(const std::string &body) { return run("TestFloor", body, floor); }
	W3DPropDrawModuleData *parseProp(const std::string &body) { return run("TestProp", body, prop); }
};

// The floor body of angmarbarracks.ini:365.
const char kBarracksFloor[] =
	"\tStaticModelLODMode\t= Yes\n"
	"\tModelName\t\t\t= KBHall_Bib\n"
	"\tWeatherTexture\t\t= SNOWY KBHall_Bib_Snow.tga\n"
	"\tHideIfModelConditions\t=\tAWAITING_CONSTRUCTION\n"
	"\tHideIfModelConditions\t=\tPARTIALLY_CONSTRUCTED\n"
	"End\n";

// The prop body of civilianprop.ini:112.
const char kTorchProp[] =
	"  ModelName = GBTorch2\n"
	"End\n";

} // namespace

// PORT-341, RW 0x004CE91B: the ctor clears +0x14..+0x17 and then copies StartHidden (data +0x1E) into +0x14, the flag RW 0x004CEA6F
// tests before it adds the floor record. A StartHidden floor therefore never enters the floor list through its first transform change;
// only the refresh RW 0x004CEADF, which has no +0x14 test, hands it over (once a hide flag clears).
TEST_CASE("PORT-341: StartHidden seeds the floor registration flag, so the first transform change adds nothing (RW 0x004CE91B / RW 0x004CEA6F)")
{
	W3DFloorDrawModuleData data;
	data.m_modelName = "KBHall_Bib";                               // angmarbarracks.ini:367
	data.m_staticModelLODMode = true;                              // angmarbarracks.ini:366
	data.m_startHidden = true;
	data.m_weatherTextures.emplace_back(1, "KBHall_Bib_Snow.tga"); // angmarbarracks.ini:368 (SNOWY)

	W3DFloorDrawState floor(&data);
	floor.setMapWeather(1);                     // SNOWY: RW 0x4CEA1C reads RW 0xDE4364 + 0x138, the port passes it in
	CHECK(floor.registered());                  // RW 0x4CE950..0x4CE955
	CHECK_FALSE(floor.hiddenBySubObject());     // the three hide flags are cleared at RW 0x4CE93D..0x4CE94D
	CHECK_FALSE(floor.hiddenByModule());
	CHECK_FALSE(floor.hiddenByModelCondition());
	CHECK(floor.isFloorVisible());

	CHECK_FALSE(floor.reactToTransformChange(1)); // RW 0x4CEA83: +0x14 is set, no hand-over
	CHECK_FALSE(floor.recordAdded());

	// SetHidden walks the refresh RW 0x004CEADF: clear, gate, re-add only while visible.
	floor.setHidden(true);                        // RW 0x4CECB5
	CHECK_FALSE(floor.recordAdded());
	floor.setHidden(false);                        // RW 0x4CECB5 -> RW 0x4CEBBE -> RW 0x4CEADF
	CHECK(floor.recordAdded());
	CHECK(floor.record().modelName == "KBHall_Bib");
	CHECK(floor.record().weatherTexture == "KBHall_Bib_Snow.tga"); // RW 0x4CEB3F -> RW 0x4CEA1C, weather 1 (SNOWY)
	CHECK(floor.record().staticModelLODMode);                      // RW 0x4CEAA0
	CHECK_FALSE(floor.record().forceToBack);                       // RW 0x4CEAA9 (not in this body)
}

// PORT-341, RW 0x004CEA6F: without StartHidden the first transform change adds the record and clears +0x14; the record is not touched
// again, so a weather change after it leaves the texture it was made with. RW 0x004CEA1C leaves the texture empty when the module has no
// entry for the map's weather (the model keeps its own texture).
TEST_CASE("PORT-341: a floor without StartHidden is added once, at the first transform change, with the map weather's texture (RW 0x004CEA6F / RW 0x004CEA1C)")
{
	W3DFloorDrawModuleData data;
	data.m_modelName = "KBHall_Bib";
	data.m_weatherTextures.emplace_back(0, "KBHall_Bib_Norm.tga");
	data.m_weatherTextures.emplace_back(1, "KBHall_Bib_Snow.tga");

	W3DFloorDrawState floor(&data);
	CHECK_FALSE(floor.registered());

	CHECK(floor.reactToTransformChange(0));       // RW 0x4CEA8B sets +0x14, RW 0x4CEAB4 hands the record over
	CHECK(floor.registered());
	CHECK(floor.recordAdded());
	CHECK(floor.record().weatherTexture == "KBHall_Bib_Norm.tga");

	CHECK_FALSE(floor.reactToTransformChange(1)); // RW 0x4CEA83: one-shot
	CHECK(floor.record().weatherTexture == "KBHall_Bib_Norm.tga"); // nothing re-resolves the texture

	W3DFloorDrawModuleData bare;
	bare.m_modelName = "KBHall_Bib";
	W3DFloorDrawState second(&bare);
	CHECK(second.reactToTransformChange(1));
	CHECK(second.recordAdded());                  // the model is still added ...
	CHECK(second.record().weatherTexture.empty()); // ... with no texture for that weather
	CHECK_FALSE(second.record().staticModelLODMode);
}

// PORT-341, RW 0x004CEADF: every refresh clears the record first (RW 0x4CEB11 -> RW 0x46AF77 -> RW 0x4E580B) and re-adds it only while
// all three hide flags are clear. The setters RW 0x004CECA1 / RW 0x004CECB5 compare before they write, so a value that does not change
// does not refresh anything.
TEST_CASE("PORT-341: the record follows the three hide flags and an unchanged setter does not refresh it (RW 0x004CEADF / RW 0x004CECA1 / RW 0x004CECB5)")
{
	W3DFloorDrawModuleData data;
	data.m_modelName = "KBHall_Bib";
	data.m_forceToBack = true;

	W3DFloorDrawState floor(&data);
	CHECK(floor.reactToTransformChange(0));
	CHECK(floor.record().forceToBack);

	// A no-op setter keeps the record - the values are compared at RW 0x4CECA5 / RW 0x4CECB9.
	floor.setSubObjectHidden(false);
	CHECK(floor.recordAdded());
	floor.setHidden(false);
	CHECK(floor.recordAdded());

	// Either hide flag drops it; clearing only one of two set flags drops it again.
	floor.setSubObjectHidden(true);
	CHECK_FALSE(floor.recordAdded());
	CHECK(floor.registered()); // +0x14 is not touched by the refresh
	floor.setHidden(true);
	floor.setSubObjectHidden(false);
	CHECK_FALSE(floor.recordAdded());
	CHECK_FALSE(floor.isFloorVisible());

	floor.setHidden(false);
	CHECK(floor.isFloorVisible());
	CHECK(floor.recordAdded());
	CHECK(floor.record().modelName == "KBHall_Bib");
	CHECK(floor.record().forceToBack);
}

// PORT-341, RW 0x004CEB80 / RW 0x004CE8CA: each HideIfModelConditions line is tested with (flags & mask) == mask over 0x13 dwords - no
// case for an empty mask, which therefore matches every flag set - and only a changed +0x17 refreshes the record. A module with no
// HideIfModelConditions line returns before anything (RW 0x4CEB86..0x4CEB8C).
TEST_CASE("PORT-341: HideIfModelConditions drives the model-condition hide flag and refreshes the record only on a change (RW 0x004CEB80 / RW 0x004CE8CA)")
{
	W3DFloorDrawModuleData data;
	data.m_modelName = "KBHall_Bib";
	data.m_hideIfModelConditions.push_back(drawtest::flagsOf({ "AWAITING_CONSTRUCTION" }));
	data.m_hideIfModelConditions.push_back(drawtest::flagsOf({ "PARTIALLY_CONSTRUCTED" }));

	W3DFloorDrawState floor(&data);
	CHECK(floor.reactToTransformChange(0));
	CHECK(floor.recordAdded());

	// The construction states of angmarbarracks.ini:369-370 hide the floor.
	CHECK(floor.replaceModelConditionState(drawtest::flagsOf({ "AWAITING_CONSTRUCTION", "SELECTED" })));
	CHECK(floor.hiddenByModelCondition());
	CHECK_FALSE(floor.recordAdded());

	// A flags change that hides no less (still AWAITING_CONSTRUCTION) changes nothing (RW 0x4CEBB3).
	CHECK_FALSE(floor.replaceModelConditionState(drawtest::flagsOf({ "AWAITING_CONSTRUCTION" })));
	CHECK(floor.hiddenByModelCondition());
	CHECK_FALSE(floor.recordAdded());

	// Built: neither line matches any more.
	CHECK(floor.replaceModelConditionState(drawtest::flagsOf({ "SELECTED" })));
	CHECK_FALSE(floor.hiddenByModelCondition());
	CHECK(floor.recordAdded());

	// An empty mask matches every flag set (RW 0x4CE8CA has no empty-mask exception).
	W3DFloorDrawModuleData empty;
	empty.m_modelName = "KBHall_Bib";
	empty.m_hideIfModelConditions.push_back(ModelConditionFlags());
	W3DFloorDrawState emptyFloor(&empty);
	CHECK(emptyFloor.reactToTransformChange(0));
	CHECK(emptyFloor.replaceModelConditionState(drawtest::flagsOf({ "SELECTED" })));
	CHECK(emptyFloor.hiddenByModelCondition());

	// No line at all: the call does nothing and reports no change.
	W3DFloorDrawModuleData none;
	W3DFloorDrawState noneFloor(&none);
	CHECK_FALSE(noneFloor.replaceModelConditionState(drawtest::flagsOf({ "AWAITING_CONSTRUCTION" })));
	CHECK_FALSE(noneFloor.hiddenByModelCondition());
}

// PORT-341, RW 0x004CE793: a prop whose drawable sits exactly at (0, 0) never gets a record (RW 0x4CE7BA..0x4CE7DE returns before the
// flag at +0x0C is set), and the record keeps the first position - nothing re-reads the drawable afterwards.
TEST_CASE("PORT-341: a prop at the origin never gets a record, and the record keeps its first position (RW 0x004CE793)")
{
	W3DPropDrawModuleData data;
	data.m_modelName = "GPTownLamp2"; // civilianprop.ini:262
	data.m_distanceFog = false;

	W3DPropDrawState prop(&data);
	CHECK_FALSE(prop.reactToTransformChange(0.0f, 0.0f));
	CHECK_FALSE(prop.propCreated());
	CHECK_FALSE(prop.record().added);

	CHECK(prop.reactToTransformChange(120.5f, -30.25f));
	CHECK(prop.propCreated());
	CHECK(prop.record().added);
	CHECK(prop.record().modelName == "GPTownLamp2");
	CHECK_FALSE(prop.record().distanceFog);
	CHECK(prop.record().x == 120.5f); // exact: RW 0x4CE7B6 stores the drawable's coordinate as is
	CHECK(prop.record().y == -30.25f); // exact: RW 0x4CE7CE stores it unchanged

	// One-shot: a later transform change neither re-adds nor moves the record.
	CHECK_FALSE(prop.reactToTransformChange(0.0f, 0.0f));
	CHECK_FALSE(prop.reactToTransformChange(200.0f, 200.0f));
	CHECK(prop.record().x == 120.5f); // exact: RW 0x4CE7B6 stores the drawable's coordinate as is
	CHECK(prop.record().y == -30.25f); // exact: RW 0x4CE7CE stores it unchanged
}

// PORT-341 neighbour guard: the field tables the states read (RW 0xBE3548 floor after RW 0xBE3340 prop, RW 0xBE2E80 tree) parse the
// mod's own bodies into the fields above, and the defaults the game relies on survive a body that sets nothing.
TEST_CASE("PORT-341: the floor and prop field tables parse the mod's bodies (RW 0xBE3340 / RW 0xBE3548 / RW 0x4CF066)")
{
	TreeDrawParser parser;

	const W3DFloorDrawModuleData *floor = parser.parseFloor(kBarracksFloor);
	REQUIRE(floor != nullptr);
	CHECK(parser.error.empty());
	CHECK(floor->m_modelName == "KBHall_Bib");
	CHECK(floor->m_staticModelLODMode);
	CHECK_FALSE(floor->m_forceToBack);
	CHECK_FALSE(floor->m_startHidden);
	CHECK(floor->m_floorFadeRateOnObjectDeath == 0.0f); // the field default; the body sets no FloorFadeRateOnObjectDeath (RW 0xBE3548 row +0x20)
	REQUIRE(floor->m_weatherTextures.size() == 1);
	CHECK(floor->m_weatherTextures[0].first == 1); // SNOWY is index 1 of RW 0xDA3A84
	CHECK(floor->m_weatherTextures[0].second == "KBHall_Bib_Snow.tga");
	REQUIRE(floor->m_hideIfModelConditions.size() == 2);
	CHECK(floor->m_hideIfModelConditions[0] == drawtest::flagsOf({ "AWAITING_CONSTRUCTION" }));
	CHECK(floor->m_hideIfModelConditions[1] == drawtest::flagsOf({ "PARTIALLY_CONSTRUCTED" }));

	// The same body read through a state behaves like the constructed data of the two tests above.
	W3DFloorDrawState state(floor);
	CHECK_FALSE(state.registered());
	CHECK(state.reactToTransformChange(1));
	CHECK(state.record().weatherTexture == "KBHall_Bib_Snow.tga");
	CHECK(state.record().staticModelLODMode);

	const W3DPropDrawModuleData *prop = parser.parseProp(kTorchProp);
	REQUIRE(prop != nullptr);
	CHECK(prop->m_modelName == "GBTorch2");
	CHECK(prop->m_distanceFog); // default true (RW 0x4CE7F2 hands the data byte over as written)

	// An unknown field is an INI error, as retail's field table makes it.
	CHECK(parser.parseFloor("\tModelName = KBHall_Bib\n\tNotAFloorField = 1\nEnd\n") == nullptr);
	CHECK_FALSE(parser.error.empty());
}
