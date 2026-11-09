// OpenBFME unit tests: LOCAL-9 horde member models by detail level (issue #114, TOP20 #23, #8).
//
// RotWK addresses:
//   AllowMultipleModels parse: RW 0xBDCA20 (+0x00 in W3DLodOptions)
//   isAllowMultipleModels getter: RW 0x4789D6
//   W3DHordeModelDraw::setModelState: RW 0x47A0AD .. 0x47A1DE
//     RW 0x47A1BE: calls isAllowMultipleModels for current LOD level
//     RW 0x4B430B: ModelConditionInfo::getModelName(variation)
//     RW 0x4B42F5: ModelConditionInfo::getModelName(0)
//   W3DScriptedModelDraw::setModelState: RW 0x4C46C4
//   Model resolution by LOD & condition flags: RW 0x4C2505 .. 0x4C2973
//     RW 0x4C2576: StaticModelLODMode (+0x135)
//     RW 0x4C256C: RW 0x4B24C4 (detail level: 0 High, 1 Med, 2 Low)
//     RW 0x4B61BD / RW 0xBD89FC: appends 'L' for detail 2
//     RW 0x4B61C4 / RW 0xBD17D0: appends 'M' for detail 1
//     RW 0x4C26BD: doesModelExist check, fallback RW 0x4C27C6
//     RW 0x4C27D4: UseStandardModelNames (+0x137)
//     RW 0x4C27E1 .. 0x4C281C: flags DAMAGED(3), REALLYDAMAGED(4), RUBBLE(5), NIGHT(7), SNOW(8)
//     RW 0x4C288E .. 0x4C2916: candidate search <base>_<n><d|e|r><s>

#include "doctest.h"

#include "Common/AsciiString.h"
#include "Common/ModelState.h"
#include "W3DDrawTestUtil.h"
#include "W3dSynth.h"

#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DModelDraw.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DScriptedModelDraw.h"

#include <deque>
#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace drawtest;
using namespace w3dsynth;

namespace
{

class TestRandom : public W3DDrawRandom
{
public:
	int value(int, int) override { return 0; }
	float real(float lo, float hi) override { return (lo + hi) * 0.5f; }
};

void addBareMesh(MemoryFileSource &fs, const std::string &modelName)
{
	std::string lower = AsciiStringUtil::lowered(modelName);
	std::string prefix = lower.size() >= 2 ? lower.substr(0, 2) : "xx";
	std::string path = "art\\w3d\\" + prefix + "\\" + lower + ".w3d";
	std::vector<std::uint8_t> bytes;
	SynthMesh body;
	body.Vertices = { { 0, 0, 0 }, { 1, 0, 0 }, { 1, 1, 0 } };
	body.Triangles = { 0, 1, 2 };
	body.Container = "";
	body.Name = modelName;
	append(bytes, meshChunk(body));
	fs.Add(path, bytes);
}

ModelConditionFlags parseFlags(const std::string &tokenStr)
{
	Harness h;
	return h.parseFlags(tokenStr);
}

} // namespace

TEST_CASE("LOCAL-9 horde member model selection: AllowMultipleModels (RW 0x4789D6, RW 0x47A1BE, RW 0x4B430B)")
{
	MemoryFileSource fs;
	addBareMesh(fs, "SOLDIER_A");
	addBareMesh(fs, "SOLDIER_B");
	addBareMesh(fs, "SOLDIER_C");

	WW3DAssetManager assets(fs);
	WW3DDrawAssets drawAssets(assets);
	TestRandom random;

	const std::string iniText =
		"LodOptions = LOW\n"
		"  AllowMultipleModels = No\n"
		"End\n"
		"LodOptions = MEDIUM\n"
		"  AllowMultipleModels = Yes\n"
		"End\n"
		"LodOptions = HIGH\n"
		"  AllowMultipleModels = Yes\n"
		"End\n"
		"DefaultModelConditionState\n"
		"  Model = SOLDIER_A\n"
		"  Model = SOLDIER_B ExtraMesh:Yes\n"
		"  Model = SOLDIER_C ExtraMesh:Yes\n"
		"End\n"
		"IdleAnimationState\n"
		"End\n"
		"End\n";

	Harness harness;
	W3DModelDrawModuleData *data = harness.parse(iniText, W3D_DRAW_HORDE_MODEL);
	REQUIRE_MESSAGE(data != nullptr, harness.error);
	auto *hordeData = dynamic_cast<W3DHordeModelDrawModuleData *>(data);
	REQUIRE(hordeData != nullptr);

	// HIGH detail (lodLevel 2): AllowMultipleModels = Yes
	// Variation 1 picks SOLDIER_B (1 % 3 = 1)
	{
		W3DScriptedModelDraw::Options opt;
		opt.variation = 1;
		opt.buildBones = false;
		W3DHordeModelDraw draw(*hordeData, 2, drawAssets, random, nullptr, opt);
		CHECK(draw.frame().modelName == "SOLDIER_B");
	}

	// HIGH detail (lodLevel 2): Variation 2 picks SOLDIER_C (2 % 3 = 2)
	{
		W3DScriptedModelDraw::Options opt;
		opt.variation = 2;
		opt.buildBones = false;
		W3DHordeModelDraw draw(*hordeData, 2, drawAssets, random, nullptr, opt);
		CHECK(draw.frame().modelName == "SOLDIER_C");
	}

	// MEDIUM detail (lodLevel 1): AllowMultipleModels = Yes
	// Variation 1 picks SOLDIER_B
	{
		W3DScriptedModelDraw::Options opt;
		opt.variation = 1;
		opt.buildBones = false;
		W3DHordeModelDraw draw(*hordeData, 1, drawAssets, random, nullptr, opt);
		CHECK(draw.frame().modelName == "SOLDIER_B");
	}

	// LOW detail (lodLevel 0): AllowMultipleModels = No
	// RW 0x47A1BE calls isAllowMultipleModels() -> false -> calls getModelName(0) -> SOLDIER_A
	// Even though variation is 1 or 2, LOW detail forces model 0!
	{
		W3DScriptedModelDraw::Options opt;
		opt.variation = 1;
		opt.buildBones = false;
		W3DHordeModelDraw draw(*hordeData, 0, drawAssets, random, nullptr, opt);
		CHECK(draw.frame().modelName == "SOLDIER_A");
	}
	{
		W3DScriptedModelDraw::Options opt;
		opt.variation = 2;
		opt.buildBones = false;
		W3DHordeModelDraw draw(*hordeData, 0, drawAssets, random, nullptr, opt);
		CHECK(draw.frame().modelName == "SOLDIER_A");
	}

	// Base W3DScriptedModelDraw always respects variation (RW 0x4C46C4)
	{
		W3DScriptedModelDraw::Options opt;
		opt.variation = 2;
		opt.buildBones = false;
		W3DScriptedModelDraw draw(*hordeData, drawAssets, random, nullptr, opt);
		CHECK(draw.frame().modelName == "SOLDIER_C");
	}
}

TEST_CASE("LOCAL-9 static model LOD mode: StaticModelLODMode appends M and L suffixes (RW 0x4C2576, RW 0x4B617F)")
{
	MemoryFileSource fs;
	addBareMesh(fs, "TOWER");
	addBareMesh(fs, "TOWERM");
	addBareMesh(fs, "TOWERL");
	addBareMesh(fs, "GATE"); // only base exists, GATEM and GATEL do not exist

	WW3DAssetManager assets(fs);
	WW3DDrawAssets drawAssets(assets);
	TestRandom random;

	const std::string iniText =
		"StaticModelLODMode = Yes\n"
		"DefaultModelConditionState\n"
		"  Model = TOWER\n"
		"End\n"
		"IdleAnimationState\n"
		"End\n"
		"End\n";

	Harness harness;
	W3DModelDrawModuleData *data = harness.parse(iniText, W3D_DRAW_HORDE_MODEL);
	REQUIRE_MESSAGE(data != nullptr, harness.error);
	auto *hordeData = dynamic_cast<W3DHordeModelDrawModuleData *>(data);
	REQUIRE(hordeData != nullptr);

	// static LOD number 0 (RW 0x4B24C4: high model detail): no suffix -> TOWER; the horde's dynamic LOD row (RW 0x4789BC) stays HIGH
	{
		W3DScriptedModelDraw::Options opt;
		opt.buildBones = false;
		opt.animationLodNumber = 0; // the static model LOD (RW 0x4B24C4 via GlobalData + 0x1774)
		W3DHordeModelDraw draw(*hordeData, 2, drawAssets, random, nullptr, opt);
		CHECK(draw.frame().modelName == "TOWER");
	}

	// static LOD number 1: appends 'M' -> TOWERM
	{
		W3DScriptedModelDraw::Options opt;
		opt.buildBones = false;
		opt.animationLodNumber = 1; // the static model LOD (RW 0x4B24C4 via GlobalData + 0x1774)
		W3DHordeModelDraw draw(*hordeData, 2, drawAssets, random, nullptr, opt);
		CHECK(draw.frame().modelName == "TOWERM");
	}

	// static LOD number 2: appends 'L' -> TOWERL
	{
		W3DScriptedModelDraw::Options opt;
		opt.buildBones = false;
		opt.animationLodNumber = 2; // the static model LOD (RW 0x4B24C4 via GlobalData + 0x1774)
		W3DHordeModelDraw draw(*hordeData, 2, drawAssets, random, nullptr, opt);
		CHECK(draw.frame().modelName == "TOWERL");
	}

	// Fallback when LOD candidate does not exist: GATE -> falls back to base GATE (RW 0x4C27C6)
	const std::string gateIni =
		"StaticModelLODMode = Yes\n"
		"DefaultModelConditionState\n"
		"  Model = GATE\n"
		"End\n"
		"IdleAnimationState\n"
		"End\n"
		"End\n";

	Harness gateHarness;
	W3DModelDrawModuleData *gateData = gateHarness.parse(gateIni, W3D_DRAW_HORDE_MODEL);
	REQUIRE_MESSAGE(gateData != nullptr, gateHarness.error);
	auto *gateHordeData = dynamic_cast<W3DHordeModelDrawModuleData *>(gateData);
	REQUIRE(gateHordeData != nullptr);

	{
		W3DScriptedModelDraw::Options opt;
		opt.buildBones = false;
		opt.animationLodNumber = 2; // the static model LOD (RW 0x4B24C4 via GlobalData + 0x1774)
		W3DHordeModelDraw draw(*gateHordeData, 2, drawAssets, random, nullptr, opt);
		CHECK(draw.frame().modelName == "GATE");
	}
}

TEST_CASE("LOCAL-9 standard model names: UseStandardModelNames candidate matching (RW 0x4C27D4 .. 0x4C2973)")
{
	MemoryFileSource fs;
	addBareMesh(fs, "HOUSE");
	addBareMesh(fs, "HOUSE_d");
	addBareMesh(fs, "HOUSE_e");
	addBareMesh(fs, "HOUSE_r");
	addBareMesh(fs, "HOUSE_n");
	addBareMesh(fs, "HOUSE_s");
	addBareMesh(fs, "HOUSE_nd");
	addBareMesh(fs, "HOUSE_nds");
	addBareMesh(fs, "BARRACKS");
	addBareMesh(fs, "BARRACKS_d"); // has only damaged, no really damaged or rubble

	WW3DAssetManager assets(fs);
	WW3DDrawAssets drawAssets(assets);
	TestRandom random;

	const std::string iniText =
		"UseStandardModelNames = Yes\n"
		"DefaultModelConditionState\n"
		"  Model = HOUSE\n"
		"End\n"
		"IdleAnimationState\n"
		"End\n"
		"End\n";

	Harness harness;
	W3DModelDrawModuleData *data = harness.parse(iniText, W3D_DRAW_SCRIPTED_MODEL);
	REQUIRE_MESSAGE(data != nullptr, harness.error);

	W3DScriptedModelDraw::Options opt;
	opt.buildBones = false;
	W3DScriptedModelDraw draw(*data, drawAssets, random, nullptr, opt);

	// Initially no condition flags -> base model HOUSE
	CHECK(draw.frame().modelName == "HOUSE");

	// DAMAGED -> HOUSE_d
	draw.setModelConditionFlags(parseFlags("DAMAGED"));
	CHECK(draw.frame().modelName == "HOUSE_d");

	// REALLYDAMAGED -> HOUSE_e
	draw.setModelConditionFlags(parseFlags("REALLYDAMAGED"));
	CHECK(draw.frame().modelName == "HOUSE_e");

	// RUBBLE -> HOUSE_r
	draw.setModelConditionFlags(parseFlags("RUBBLE"));
	CHECK(draw.frame().modelName == "HOUSE_r");

	// NIGHT -> HOUSE_n
	draw.setModelConditionFlags(parseFlags("NIGHT"));
	CHECK(draw.frame().modelName == "HOUSE_n");

	// SNOW -> HOUSE_s
	draw.setModelConditionFlags(parseFlags("SNOW"));
	CHECK(draw.frame().modelName == "HOUSE_s");

	// NIGHT + DAMAGED -> HOUSE_nd
	draw.setModelConditionFlags(parseFlags("NIGHT DAMAGED"));
	CHECK(draw.frame().modelName == "HOUSE_nd");

	// NIGHT + DAMAGED + SNOW -> HOUSE_nds
	draw.setModelConditionFlags(parseFlags("NIGHT DAMAGED SNOW"));
	CHECK(draw.frame().modelName == "HOUSE_nds");

	// Clearing flags -> back to HOUSE
	draw.setModelConditionFlags(ModelConditionFlags());
	CHECK(draw.frame().modelName == "HOUSE");

	// Fallback test: BARRACKS has only _d. Asking for REALLYDAMAGED or RUBBLE falls back to _d
	const std::string barracksIni =
		"UseStandardModelNames = Yes\n"
		"DefaultModelConditionState\n"
		"  Model = BARRACKS\n"
		"End\n"
		"IdleAnimationState\n"
		"End\n"
		"End\n";

	Harness bHarness;
	W3DModelDrawModuleData *bData = bHarness.parse(barracksIni, W3D_DRAW_SCRIPTED_MODEL);
	REQUIRE_MESSAGE(bData != nullptr, bHarness.error);

	W3DScriptedModelDraw bDraw(*bData, drawAssets, random, nullptr, opt);
	bDraw.setModelConditionFlags(parseFlags("RUBBLE"));
	CHECK(bDraw.frame().modelName == "BARRACKS_d");
}

TEST_CASE("LOCAL-9 retail Ennorath data: W3DHordeModelDraw defaults and module data")
{
	// RW 0x478289 constructor defaults:
	// LOW: allowMultipleModels = false
	// MEDIUM: allowMultipleModels = true
	// HIGH: allowMultipleModels = true
	W3DHordeModelDrawModuleData fresh;
	CHECK(fresh.m_lodOptions[0].allowMultipleModels == false);
	CHECK(fresh.m_lodOptions[1].allowMultipleModels == true);
	CHECK(fresh.m_lodOptions[2].allowMultipleModels == true);
	CHECK(fresh.m_useStandardModelNames == false);
	CHECK(fresh.m_staticModelLODMode == false);
}
