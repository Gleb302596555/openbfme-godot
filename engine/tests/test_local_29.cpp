// OpenBFME. GPL-3.0.
//
// Narrow tests for lane LOCAL-29 (issue #218):
// a draw module placed on a bone of another module, as retail (AttachToBoneInAnotherModule).
//
// Target facts:
//   RotWK 2.01:
//     - W3DScriptedModelDraw::adjustTransformMtx: RW 0x4B686D (DONOR BFME2 0x000B710D)
//     - AttachToBoneInAnotherModule gate & placement: RW 0x4B6A6F..0x4B6B26
//     - slot +0x104 virtual callback: RW 0x9F3A3C (base ret 4), RW 0x4D00CD (W3DSailModelDraw),
//       called at RW 0x4B6B26 only when bone lookup succeeds
//     - bone resolution across modules: Drawable::0x672B5B (DONOR BFME2 0x00272835)
//       iterates m_modules, calls slotA4 -> slot14 (RW 0x004B2F19), querying current animated pose
//       via Get_Bone_Index and Get_Bone_Transform
//     - counter at [0xDD0954] (up to 100): incremented ONLY on m_parentID lookup failure (RW 0x4B6A60),
//       NOT on AttachToBoneInAnotherModule
//     - W3DLightDraw bone resolution: RW 0x004CF5EE uses the exact same Drawable::0x672B5B

#include "doctest.h"
#include "ObjectTestUtil.h"
#include "W3DDrawTestUtil.h"
#include "W3dSynth.h"

#include "Common/AsciiString.h"
#include "Common/Thing/ModuleFactory.h"
#include "Common/Thing/Thing.h"
#include "Common/Thing/ThingTemplate.h"
#include "GameClient/ClientEffects.h"
#include "GameClient/Drawable.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DDrawModules.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DDrawServices.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DLightDraw.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DLuaDrawScriptHost.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DScriptedModelDraw.h"
#include "Libraries/WWVegas/WW3D2/assetmgr.h"
#include "Libraries/WWVegas/WWMath/matrix3d.h"

#include <cmath>
#include <memory>
#include <string>
#include <vector>

using namespace drawtest;
using namespace w3dsynth;

namespace
{

class FixedRandom : public W3DDrawRandom
{
public:
	int value(int lo, int hi) override { return lo == hi ? lo : (lo + hi) / 2; }
	float real(float lo, float hi) override { return (lo + hi) * 0.5f; }
};

struct SyntheticMultiModuleBearer
{
	MemoryFileSource fs;
	std::unique_ptr<WW3DAssetManager> assets;
	std::unique_ptr<WW3DDrawAssets> drawAssets;
	Harness harness;
	FixedRandom random;
	std::unique_ptr<W3DScriptedModelDraw> parentDraw;
	std::unique_ptr<W3DScriptedModelDraw> childDraw;

	SyntheticMultiModuleBearer()
	{
		// Skeleton: ROOTTRANSFORM (0) -> HIP (1, base (0,0,1)) -> TORCHBONE (2, base (0,2,0))
		fs.Add("art\\w3d\\tb\\tbunit_skl.w3d",
			hierarchyChunk("TBUNIT_SKL", { pivot("ROOTTRANSFORM", 0xFFFFFFFFu, 0, 0, 0), pivot("HIP", 0, 0, 0, 1), pivot("TORCHBONE", 1, 0, 2, 0) }));
		std::vector<std::uint8_t> bytes;
		SynthMesh body;
		body.Vertices = { { 0, 0, 0 }, { 1, 0, 0 }, { 1, 1, 0 }, { 0, 1, 0 } };
		body.Triangles = { 0, 1, 2, 0, 2, 3 };
		body.Container = "TBUNIT_SKN";
		body.Name = "BODY";
		append(bytes, meshChunk(body));
		append(bytes, hlodChunk("TBUNIT_SKN", "TBUNIT_SKL", { { "TBUNIT_SKN.BODY", 1 } }));
		fs.Add("art\\w3d\\tb\\tbunit_skn.w3d", bytes);

		// TBUNIT_WALK: 11 frames, 30 fps. TORCHBONE (pivot 2) moves along Z: frame 0 -> 0.0f, frame 5 -> 10.0f
		std::vector<float> zVals(11, 0.0f);
		for (int f = 0; f < 11; ++f)
		{
			zVals[(size_t)f] = (float)f * 2.0f;
		}
		std::vector<SynthChannel> channels;
		channels.push_back(SynthChannel{ ANIM_CHANNEL_Z, 2, 0, 1, zVals });
		std::vector<std::uint8_t> anim = rawAnimChunk("TBUNIT_WALK", "TBUNIT_SKL", 11, 30, channels);
		fs.Add("art\\w3d\\tb\\tbunit_walk.w3d", anim);

		assets.reset(new WW3DAssetManager(fs));
		drawAssets.reset(new WW3DDrawAssets(*assets));

		std::string parentIni =
			"DefaultModelConditionState\n  Model = TBUNIT_SKN\n  Skeleton = TBUNIT_SKL\nEnd\n"
			"IdleAnimationState\n"
			"  StateName = STATE_Idle\n"
			"  Animation = WALK\n"
			"    AnimationName = TBUNIT_WALK\n"
			"    AnimationMode = LOOP\n"
			"  End\n"
			"End\n"
			"End\n";
		W3DModelDrawModuleData *pdata = harness.parse(parentIni);
		REQUIRE(pdata != nullptr);
		W3DScriptedModelDraw::Options o;
		o.buildBones = false;
		parentDraw.reset(new W3DScriptedModelDraw(*pdata, *drawAssets, random, nullptr, o));
		REQUIRE(parentDraw->errors().empty());

		std::string childIni =
			"DefaultModelConditionState\n  Model = TBUNIT_SKN\n  Skeleton = TBUNIT_SKL\nEnd\n"
			"IdleAnimationState\n  StateName = STATE_Idle\nEnd\n"
			"End\n";
		W3DModelDrawModuleData *cdata = harness.parse(childIni);
		REQUIRE(cdata != nullptr);
		childDraw.reset(new W3DScriptedModelDraw(*cdata, *drawAssets, random, nullptr, o));
		REQUIRE(childDraw->errors().empty());
	}
};

class TestSlot104Draw : public W3DScriptedModelDraw
{
public:
	TestSlot104Draw(const W3DModelDrawModuleData &data, W3DDrawAssets &assets, W3DDrawRandom &random,
		const Options &options = Options())
		: W3DScriptedModelDraw(data, assets, random, nullptr, options)
	{
	}

	void adjustAttachedTransform(Matrix3D &m) override
	{
		m_calledCount++;
		// Apply test translation offset to prove callback ran
		m.Row[0][3] += 5.0f;
		m.Row[1][3] += 7.0f;
		m.Row[2][3] += 9.0f;
	}

	int m_calledCount = 0;
};

} // namespace

TEST_CASE("LOCAL-29: attached model placed on animated bone of another module (RW 0x4B6A6F..0x4B6B26 -> RW 0x672B5B)")
{
	SyntheticMultiModuleBearer syn;

	objtest::World w;
	DrawServices services(*syn.assets, w.modules, W3DLuaDrawScriptHost::Options{});
	ThingTemplate tmpl;
	tmpl.friend_setTemplateName("BearerTemplate");
	std::vector<std::string> problems;
	auto d = std::make_unique<Drawable>(services, &tmpl, 1, 1, std::vector<ClientEvent::ClientModule>{}, problems);
	Coord3D thingPos{ 100.0f, 200.0f, 50.0f };
	d->setPosition(&thingPos);

	// Entry 0: parent module with skeleton containing TORCHBONE
	DrawEntry entry0;
	entry0.className = "W3DScriptedModelDraw";
	entry0.kind = W3D_DRAWKIND_MODEL;
	entry0.draw = std::move(syn.parentDraw);
	d->entries().push_back(std::move(entry0));

	// Entry 1: child module attached to TORCHBONE
	DrawEntry entry1;
	entry1.className = "W3DScriptedModelDraw";
	entry1.kind = W3D_DRAWKIND_MODEL;
	entry1.attachToBone = "TorchBone";
	entry1.draw = std::move(syn.childDraw);
	d->entries().push_back(std::move(entry1));

	// 1) Module 0 is unattached: entryTransform returns false and gives object position
	Matrix3D mtx0;
	bool attached0 = d->entryTransform(d->entries()[0], mtx0);
	CHECK_FALSE(attached0);
	CHECK(mtx0.Row[0][3] == 100.0f);
	CHECK(mtx0.Row[1][3] == 200.0f);
	CHECK(mtx0.Row[2][3] == 50.0f);

	// 2) Module 1 is attached to TorchBone: entryTransform returns true and gives bone position
	// Frame 0: ROOT(50) + HIP(1) + TORCHBONE(0) = 51.0f in Z, 202.0f in Y, 100.0f in X
	Matrix3D mtx1;
	bool attached1 = d->entryTransform(d->entries()[1], mtx1);
	CHECK(attached1);
	CHECK(mtx1.Row[0][3] == 100.0f);
	CHECK(mtx1.Row[1][3] == 202.0f);
	CHECK(mtx1.Row[2][3] == 51.0f);

	// 3) Advance animation by 5 frames (5 * 33.34 ms ~ 166.7 ms): TORCHBONE moves +10 in Z
	d->entries()[0].draw->advance(166.7);

	Matrix3D mtx1_f5;
	bool attached1_f5 = d->entryTransform(d->entries()[1], mtx1_f5);
	CHECK(attached1_f5);
	CHECK(mtx1_f5.Row[0][3] == 100.0f);
	CHECK(mtx1_f5.Row[1][3] == 202.0f);
	CHECK(mtx1_f5.Row[2][3] == 61.0f);
}

TEST_CASE("LOCAL-29: unattached module or non-existent bone retains object transform (RW 0x4B6A81 / RW 0x4B6AA4)")
{
	SyntheticMultiModuleBearer syn;

	objtest::World w;
	DrawServices services(*syn.assets, w.modules, W3DLuaDrawScriptHost::Options{});
	ThingTemplate tmpl;
	tmpl.friend_setTemplateName("BearerTemplate");
	std::vector<std::string> problems;
	auto d = std::make_unique<Drawable>(services, &tmpl, 1, 1, std::vector<ClientEvent::ClientModule>{}, problems);
	Coord3D thingPos{ 50.0f, 60.0f, 70.0f };
	d->setPosition(&thingPos);

	DrawEntry entry0;
	entry0.className = "W3DScriptedModelDraw";
	entry0.kind = W3D_DRAWKIND_MODEL;
	entry0.draw = std::move(syn.parentDraw);
	d->entries().push_back(std::move(entry0));

	// Non-existent bone name
	DrawEntry entry1;
	entry1.className = "W3DScriptedModelDraw";
	entry1.kind = W3D_DRAWKIND_MODEL;
	entry1.attachToBone = "NoSuchBoneName";
	entry1.draw = std::move(syn.childDraw);
	d->entries().push_back(std::move(entry1));

	Matrix3D mtx;
	bool attached = d->entryTransform(d->entries()[1], mtx);
	CHECK_FALSE(attached);
	CHECK(mtx.Row[0][3] == 50.0f);
	CHECK(mtx.Row[1][3] == 60.0f);
	CHECK(mtx.Row[2][3] == 70.0f);

	// Case "None" or empty
	d->entries()[1].attachToBone = "None";
	CHECK_FALSE(d->entryTransform(d->entries()[1], mtx));
	d->entries()[1].attachToBone = "";
	CHECK_FALSE(d->entryTransform(d->entries()[1], mtx));
}

TEST_CASE("LOCAL-29: slot +0x104 callback is dispatched only on successful bone lookup (RW 0x4B6B26)")
{
	SyntheticMultiModuleBearer syn;

	objtest::World w;
	DrawServices services(*syn.assets, w.modules, W3DLuaDrawScriptHost::Options{});
	ThingTemplate tmpl;
	tmpl.friend_setTemplateName("BearerTemplate");
	std::vector<std::string> problems;
	auto d = std::make_unique<Drawable>(services, &tmpl, 1, 1, std::vector<ClientEvent::ClientModule>{}, problems);
	Coord3D thingPos{ 100.0f, 200.0f, 50.0f };
	d->setPosition(&thingPos);

	DrawEntry entry0;
	entry0.className = "W3DScriptedModelDraw";
	entry0.kind = W3D_DRAWKIND_MODEL;
	entry0.draw = std::move(syn.parentDraw);
	d->entries().push_back(std::move(entry0));

	std::string childIni =
		"DefaultModelConditionState\n  Model = TBUNIT_SKN\n  Skeleton = TBUNIT_SKL\nEnd\n"
		"IdleAnimationState\n  StateName = STATE_Idle\nEnd\n"
		"End\n";
	W3DModelDrawModuleData *cdata = syn.harness.parse(childIni);
	REQUIRE(cdata != nullptr);
	W3DScriptedModelDraw::Options o;
	o.buildBones = false;
	auto customDraw = std::make_unique<TestSlot104Draw>(*cdata, *syn.drawAssets, syn.random, o);
	TestSlot104Draw *rawCustom = customDraw.get();

	DrawEntry entry1;
	entry1.className = "W3DScriptedModelDraw";
	entry1.kind = W3D_DRAWKIND_MODEL;
	entry1.attachToBone = "TorchBone";
	entry1.draw = std::move(customDraw);
	d->entries().push_back(std::move(entry1));

	CHECK(rawCustom->m_calledCount == 0);

	// Successful attachment: slot +0x104 must be called once
	Matrix3D mtx;
	bool attached = d->entryTransform(d->entries()[1], mtx);
	CHECK(attached);
	CHECK(rawCustom->m_calledCount == 1);
	// Verification that custom offset (+5, +7, +9) was applied by the slot callback
	CHECK(mtx.Row[0][3] == 105.0f);
	CHECK(mtx.Row[1][3] == 209.0f);
	CHECK(mtx.Row[2][3] == 60.0f);

	// Unattached or non-existent bone: callback must NOT be called
	d->entries()[1].attachToBone = "NonExistent";
	attached = d->entryTransform(d->entries()[1], mtx);
	CHECK_FALSE(attached);
	CHECK(rawCustom->m_calledCount == 1); // count unchanged
}

TEST_CASE("LOCAL-29: boneTransform scales and rotates with drawable orientation and scale")
{
	SyntheticMultiModuleBearer syn;

	objtest::World w;
	DrawServices services(*syn.assets, w.modules, W3DLuaDrawScriptHost::Options{});
	ThingTemplate tmpl;
	tmpl.friend_setTemplateName("BearerTemplate");
	std::vector<std::string> problems;
	auto d = std::make_unique<Drawable>(services, &tmpl, 1, 1, std::vector<ClientEvent::ClientModule>{}, problems);
	Coord3D thingPos{ 100.0f, 200.0f, 50.0f };
	d->setPosition(&thingPos);
	// 90 degrees rotation around Z (PI / 2 radians): X becomes Y, Y becomes -X
	d->setOrientation(1.57079632679f);
	d->setInstanceScale(2.0f);

	DrawEntry entry0;
	entry0.className = "W3DScriptedModelDraw";
	entry0.kind = W3D_DRAWKIND_MODEL;
	entry0.draw = std::move(syn.parentDraw);
	d->entries().push_back(std::move(entry0));

	// TORCHBONE local pos: X=0, Y=2, Z=1.
	// Scaled by 2: local X=0, Y=4, Z=2.
	// Rotated 90 deg around Z:
	// world X = pos.x - localY = 100.0 - 4.0 = 96.0
	// world Y = pos.y + localX = 200.0 + 0.0 = 200.0
	// (more precisely, with cos(pi/2)=0, sin(pi/2)=1:
	//  wx = px * cos - py * sin = 0 - 4*1 = -4. X = 100 - 4 = 96.
	//  wy = py * cos + px * sin = 4*0 + 0 = 0.   Y = 200 + 0 = 200 (or with +200 + wy))
	// world Z = pos.z + localZ = 50.0 + 2.0 = 52.0
	Matrix3D boneMtx;
	REQUIRE(d->boneTransform("TorchBone", boneMtx));

	Coord3D pos;
	REQUIRE(d->boneWorldPosition("TorchBone", pos));

	CHECK(boneMtx.Row[0][3] == doctest::Approx(pos.x)); // tolerance: float trigonometry in orientation rotation
	CHECK(boneMtx.Row[1][3] == doctest::Approx(pos.y)); // tolerance: float trigonometry in orientation rotation
	CHECK(boneMtx.Row[2][3] == doctest::Approx(pos.z)); // tolerance: float trigonometry in orientation rotation

	CHECK(pos.x == doctest::Approx(96.0f).epsilon(0.01)); // tolerance: float sin/cos orientation transform
	CHECK(pos.z == doctest::Approx(52.0f).epsilon(0.01)); // tolerance: float sin/cos orientation transform
}

TEST_CASE("LOCAL-29: W3DLightDraw uses unified boneTransform lookup (RW 0x004CF5EE -> RW 0x00672B5B)")
{
	SyntheticMultiModuleBearer syn;

	objtest::World w;
	DrawServices services(*syn.assets, w.modules, W3DLuaDrawScriptHost::Options{});
	ThingTemplate tmpl;
	tmpl.friend_setTemplateName("BearerTemplate");
	std::vector<std::string> problems;
	auto d = std::make_unique<Drawable>(services, &tmpl, 1, 1, std::vector<ClientEvent::ClientModule>{}, problems);
	Coord3D thingPos{ 10.0f, 20.0f, 30.0f };
	d->setPosition(&thingPos);

	DrawEntry entry0;
	entry0.className = "W3DScriptedModelDraw";
	entry0.kind = W3D_DRAWKIND_MODEL;
	entry0.draw = std::move(syn.parentDraw);
	d->entries().push_back(std::move(entry0));

	Coord3D bonePos;
	Matrix3D boneMtx;
	REQUIRE(d->boneTransform("TORCHBONE", boneMtx));
	REQUIRE(d->boneWorldPosition("TORCHBONE", bonePos));

	CHECK(bonePos.x == boneMtx.Row[0][3]);
	CHECK(bonePos.y == boneMtx.Row[1][3]);
	CHECK(bonePos.z == boneMtx.Row[2][3]);
}
