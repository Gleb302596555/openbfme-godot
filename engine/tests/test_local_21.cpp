// OpenBFME unit tests: LOCAL-21 model state lifecycle, forced replacement and per-mesh recolour (issue #161, unblocks Port 47 #151).
//
// RotWK addresses:
//   W3DScriptedModelDraw::setModelState: RW 0x4C451D (slot 0x100; BFME2 0x4C4E23)
//     RW 0x4C460C..0x4C4648: reuse condition: curModelInfo != null, model names match, indexChanged false,
//                            new state has no textures, cur state has no textures, turrets match, force == 0
//     RW 0x4C467C: RetainSubObjects test: if (!state->retainSubObjects) m_subObjectOps.clear();
//     RW 0x4B2C18: teardown old render object, resets per-mesh overrides, increments generation
//     RW 0x4C4BD7..0x4C4C6B: texture swap deduplication: upgradeTextureSwaps first, then state textures (if not in list),
//                            then randomTextureSwaps (if not in list)
//   W3DScriptedModelDraw::setUpgradeTextureSwaps: RW 0x4C1068 (slot 0x84): push 1; push [esi+0x14]; call [eax+0x100] (force = 1)
//   W3DScriptedModelDraw::setModelConditionFlags: RW 0x4BF2D8 (replaceModelConditionState)
//     RW 0x4BF3AE..0x4BF416: if UseStandardModelNames and flags 0, 3, 4, 5, 7, 8 changed -> force = 1
//   Per-mesh material state:
//     Base palette: RW 0x536B2E (slot +0x1F4) applied to all meshes on model create
//     Upgrade recolour: RW 0x4B8701 -> slot +0x1F8 (RW 0x54BDE0):
//       RW 0x54BE43: strchr(name, '.') -> _stricmp(after_dot, excludeSubobject) == 0 skips recolour (keeps base palette)

#include "doctest.h"

#include "Common/AsciiString.h"
#include "Common/ModelState.h"
#include "W3DDrawTestUtil.h"
#include "W3dSynth.h"

#include "GameClient/Drawable.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DModelDraw.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DScriptedModelDraw.h"
#include "GameEngineDevice/W3DDevice/GameClient/HouseColor.h"

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

void addHlodModel(MemoryFileSource &fs, const std::string &modelName, const std::vector<std::string> &parts)
{
	std::string lower = AsciiStringUtil::lowered(modelName);
	std::string prefix = lower.size() >= 2 ? lower.substr(0, 2) : "xx";
	std::string path = "art\\w3d\\" + prefix + "\\" + lower + ".w3d";
	std::vector<std::uint8_t> bytes;
	SynthMesh body;
	body.Vertices = { { 0, 0, 0 }, { 1, 0, 0 }, { 1, 1, 0 }, { 0, 1, 0 } };
	body.Triangles = { 0, 1, 2, 0, 2, 3 };
	body.Container = modelName;
	std::vector<SynthSub> subs;
	for (const std::string &part : parts)
	{
		body.Name = part;
		append(bytes, meshChunk(body));
		subs.push_back({ modelName + "." + part, 0 });
	}
	append(bytes, hlodChunk(modelName.c_str(), "TEST_SKL", subs));
	fs.Add(path, bytes);
}

ModelConditionFlags parseFlags(const std::string &tokenStr)
{
	Harness h;
	return h.parseFlags(tokenStr);
}

} // namespace

TEST_CASE("LOCAL-21a: forced model replacement via setUpgradeTextureSwaps (RW 0x4C1068, RW 0x4C451D)")
{
	MemoryFileSource fs;
	addBareMesh(fs, "UPG_TEST");

	WW3DAssetManager assets(fs);
	WW3DDrawAssets drawAssets(assets);
	TestRandom random;

	const std::string iniText =
		"DefaultModelConditionState\n"
		"  Model = UPG_TEST\n"
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

	// Initial model created
	CHECK(draw.renderObjectsCreated() == 1);
	CHECK(draw.renderObjectGeneration() == 1);

	// Re-applying flags without change does NOT recreate
	draw.setModelConditionFlags(ModelConditionFlags());
	CHECK(draw.renderObjectsCreated() == 1);
	CHECK(draw.renderObjectGeneration() == 1);

	// setUpgradeTextureSwaps calls setModelState with force = 1 (RW 0x4C1068)
	// Bypasses reuse condition (RW 0x4C4648) and triggers full recreate
	draw.setUpgradeTextureSwaps({ { "base.tga", "upg.tga" } });
	CHECK(draw.renderObjectsCreated() == 2);
	CHECK(draw.renderObjectGeneration() == 2);

	// poseView reflects the new generation and the active texture swap
	W3DDrawPoseView v;
	draw.poseView(v);
	CHECK(v.renderObjectGeneration == 2);
	REQUIRE(v.textureSwaps != nullptr);
	REQUIRE(v.textureSwaps->size() == 1);
	CHECK((*v.textureSwaps)[0].first == "base.tga");
	CHECK((*v.textureSwaps)[0].second == "upg.tga");
}

TEST_CASE("LOCAL-21a: RetainSubObjects preserves script hide across forced recreation (RW 0x4C467C)")
{
	MemoryFileSource fs;
	addHlodModel(fs, "RETAIN_TEST", { "BODY", "SWORD", "SHIELD" });

	WW3DAssetManager assets(fs);
	WW3DDrawAssets drawAssets(assets);
	TestRandom random;

	const std::string iniText =
		"DefaultModelConditionState\n"
		"  Model = RETAIN_TEST\n"
		"  RetainSubObjects = No\n"
		"End\n"
		"ModelConditionState = USER_1\n"
		"  Model = RETAIN_TEST\n"
		"  RetainSubObjects = Yes\n"
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

	// Non-retaining state: hide subobject
	draw.hideSubObject("RETAIN_TEST.SWORD");
	const auto *swordSub = draw.currentModel();
	REQUIRE(swordSub != nullptr);
	int swordIdx = -1;
	for (size_t i = 0; i < swordSub->SubObjects.size(); ++i)
	{
		if (swordSub->SubObjects[i].Name == "RETAIN_TEST.SWORD")
		{
			swordIdx = (int)i;
			break;
		}
	}
	REQUIRE(swordIdx == 1);
	CHECK(draw.isSubObjectHidden(swordIdx));

	// Forced replacement with RetainSubObjects = No clears non-permanent ops (RW 0x4C4685..0x4C468F)
	draw.setUpgradeTextureSwaps({ { "a.tga", "b.tga" } });
	CHECK(!draw.isSubObjectHidden(swordIdx));

	// Switch to state with RetainSubObjects = Yes
	draw.setModelConditionFlags(parseFlags("USER_1"));
	draw.hideSubObject("RETAIN_TEST.SWORD");
	draw.hideSubObjectPermanently("RETAIN_TEST.SHIELD");
	CHECK(draw.isSubObjectHidden(swordIdx));

	// Forced replacement on state with RetainSubObjects = Yes preserves non-permanent ops (RW 0x4C467C)
	draw.setUpgradeTextureSwaps({ { "c.tga", "d.tga" } });
	CHECK(draw.isSubObjectHidden(swordIdx));

	// Permanent hide also survives
	int shieldIdx = -1;
	for (size_t i = 0; i < swordSub->SubObjects.size(); ++i)
	{
		if (swordSub->SubObjects[i].Name == "RETAIN_TEST.SHIELD")
		{
			shieldIdx = (int)i;
			break;
		}
	}
	REQUIRE(shieldIdx == 2);
	CHECK(draw.isSubObjectHidden(shieldIdx));
}

TEST_CASE("LOCAL-21a: texture swap deduplication precedence (RW 0x4C4BD7, BFME2 0x4C4E23)")
{
	MemoryFileSource fs;
	addBareMesh(fs, "DEDUP_TEST");

	WW3DAssetManager assets(fs);
	WW3DDrawAssets drawAssets(assets);
	TestRandom random;

	const std::string iniText =
		"DefaultModelConditionState\n"
		"  Model = DEDUP_TEST\n"
		"  Texture = Base.tga StateA.tga\n"
		"  Texture = Extra.tga StateExtra.tga\n"
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

	// Initially only state textures
	W3DDrawPoseView v1;
	draw.poseView(v1);
	REQUIRE(v1.textureSwaps != nullptr);
	CHECK(v1.textureSwaps->size() == 2);
	CHECK((*v1.textureSwaps)[0].first == "Base.tga");
	CHECK((*v1.textureSwaps)[0].second == "StateA.tga");

	// Add upgrade textures: "BASE.TGA" overrides "Base.tga" from state
	draw.setUpgradeTextureSwaps({ { "BASE.TGA", "UpgradeA.tga" }, { "Upg2.tga", "Upgrade2.tga" } });

	W3DDrawPoseView v2;
	draw.poseView(v2);
	REQUIRE(v2.textureSwaps != nullptr);
	// Upgrade swaps added first (2), then non-duplicate state swaps (1: Extra.tga). Base.tga duplicate skipped!
	REQUIRE(v2.textureSwaps->size() == 3);
	CHECK((*v2.textureSwaps)[0].first == "BASE.TGA");
	CHECK((*v2.textureSwaps)[0].second == "UpgradeA.tga");
	CHECK((*v2.textureSwaps)[1].first == "Upg2.tga");
	CHECK((*v2.textureSwaps)[1].second == "Upgrade2.tga");
	CHECK((*v2.textureSwaps)[2].first == "Extra.tga");
	CHECK((*v2.textureSwaps)[2].second == "StateExtra.tga");
}

TEST_CASE("LOCAL-21a: damage and snow model condition flags force recreation with UseStandardModelNames (RW 0x4BF3AE..0x4BF416)")
{
	MemoryFileSource fs;
	addBareMesh(fs, "BLD");
	addBareMesh(fs, "BLD_d");
	addBareMesh(fs, "BLD_s");

	WW3DAssetManager assets(fs);
	WW3DDrawAssets drawAssets(assets);
	TestRandom random;

	const std::string iniText =
		"UseStandardModelNames = Yes\n"
		"DefaultModelConditionState\n"
		"  Model = BLD\n"
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

	CHECK(draw.renderObjectsCreated() == 1);
	CHECK(draw.renderObjectGeneration() == 1);
	CHECK(draw.frame().modelName == "BLD");

	// An unrelated flag (e.g. ATTACKING) does not force recreate
	draw.setModelConditionFlags(parseFlags("ATTACKING"));
	CHECK(draw.renderObjectsCreated() == 1);
	CHECK(draw.renderObjectGeneration() == 1);

	// Setting DAMAGED (bit 3) forces recreation via RW 0x4BF416
	draw.setModelConditionFlags(parseFlags("DAMAGED"));
	CHECK(draw.renderObjectsCreated() == 2);
	CHECK(draw.renderObjectGeneration() == 2);
	CHECK(draw.frame().modelName == "BLD_d");

	// Setting SNOW (bit 8) forces recreation
	draw.setModelConditionFlags(parseFlags("SNOW"));
	CHECK(draw.renderObjectsCreated() == 3);
	CHECK(draw.renderObjectGeneration() == 3);
	CHECK(draw.frame().modelName == "BLD_s");
}

TEST_CASE("LOCAL-21b: per-mesh material state: slot +0x1F8 excludes matching subobject after first dot (RW 0x54BE43)")
{
	MemoryFileSource fs;
	addHlodModel(fs, "GUHero_SKN", { "GUHero_Body", "Sword", "Shield" });

	WW3DAssetManager assets(fs);
	WW3DDrawAssets drawAssets(assets);
	TestRandom random;

	const std::string iniText =
		"DefaultModelConditionState\n"
		"  Model = GUHero_SKN\n"
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

	const auto *m = draw.currentModel();
	REQUIRE(m != nullptr);
	CHECK(m->Type == RenderObjPrototype::PROTO_HLOD);
	CHECK(m->SubObjects.size() == 3);

	// Base palette: kind 1, Red
	HouseColorParams basePalette;
	basePalette.kind = 1;
	basePalette.colors[0] = 0x00FF0000u;
	draw.setBaseHouseColor(basePalette);
	const uint32_t genAfterBase = draw.materialStateGeneration();

	// Upgrade recolour: kind 1, Blue, with ExcludeSubobjects = Sword
	HouseColorParams upgradePalette;
	upgradePalette.kind = 1;
	upgradePalette.colors[0] = 0x000000FFu;
	draw.recolorRenderObject("Sword", upgradePalette);

	CHECK(genAfterBase == 2);
	CHECK(draw.materialStateGeneration() == 3);

	const auto &meshColors = draw.meshHouseColors();
	REQUIRE(meshColors.size() == 3);

	// Excluded "Sword" keeps base red palette (RW 0x54BE43)
	auto itSword = meshColors.find("GUHero_SKN.Sword");
	REQUIRE(itSword != meshColors.end());
	CHECK(itSword->second.kind == 1);
	CHECK(itSword->second.colors[0] == 0x00FF0000u);

	// Non-excluded "GUHero_Body" gets upgrade blue palette
	auto itBody = meshColors.find("GUHero_SKN.GUHero_Body");
	REQUIRE(itBody != meshColors.end());
	CHECK(itBody->second.kind == 1);
	CHECK(itBody->second.colors[0] == 0x000000FFu);

	// Non-excluded "Shield" gets upgrade blue palette
	auto itShield = meshColors.find("GUHero_SKN.Shield");
	REQUIRE(itShield != meshColors.end());
	CHECK(itShield->second.kind == 1);
	CHECK(itShield->second.colors[0] == 0x000000FFu);

	// poseView propagates meshHouseColors pointer and generation
	W3DDrawPoseView v;
	draw.poseView(v);
	REQUIRE(v.meshHouseColors != nullptr);
	CHECK(v.meshHouseColors->size() == 3);
	CHECK(v.materialStateGeneration == draw.materialStateGeneration());
}

TEST_CASE("LOCAL-21b: forced replacement via setUpgradeTextureSwaps clears per-mesh overrides and resets to base palette (RW 0x4C46B6, RW 0x536B2E)")
{
	MemoryFileSource fs;
	addHlodModel(fs, "RESET_HERO", { "Body", "Sword" });

	WW3DAssetManager assets(fs);
	WW3DDrawAssets drawAssets(assets);
	TestRandom random;

	const std::string iniText =
		"DefaultModelConditionState\n"
		"  Model = RESET_HERO\n"
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

	HouseColorParams basePalette;
	basePalette.kind = 1;
	basePalette.colors[0] = 0x00FF0000u;
	draw.setBaseHouseColor(basePalette);

	HouseColorParams upgPalette;
	upgPalette.kind = 1;
	upgPalette.colors[0] = 0x0000FF00u;
	draw.recolorRenderObject("Sword", upgPalette);
	CHECK(!draw.meshHouseColors().empty());

	// Forced replacement (e.g. from setUpgradeTextureSwaps, force = 1) tears down render object
	// and clears per-mesh overrides (RW 0x4B2C18), restoring base palette (RW 0x536B2E)
	draw.setUpgradeTextureSwaps({ { "x.tga", "y.tga" } });
	CHECK(draw.meshHouseColors().empty());
}
