// OpenBFME. GPL-3.0.
//
// PORT-119 (issue #303, problem P-56): a hero made of two figures ("two in one") stands with both figures inside each other
// instead of side by side.
//
// Ennorath data (object/goodfaction/units/imladris/imladriszwillinge.ini): the object ImladrisZwillingeEdain is ONE object with four
// W3DScriptedModelDraw modules: two rigs (DRAWShagratEmmiter / DRAWGorbagEmmiter, models euella_emmitr / euelro_emmitr, each with a
// DefaultModelConditionState that names ONLY a Model: no Skeleton line) and the two visible figures (HeroShagrat: euElladanN attached
// to FIREBONE01; HeroGorbag: euElrohirN attached to FIREBONE02), each with AttachToBoneInAnotherModule. The two figures therefore
// stand where the rigs' bones put them; they overlap when the attach does not resolve.
//
// TARGET FACTS (RotWK game.dat):
//   * W3DScriptedModelDraw::adjustTransformMtx RW 0x4B686D (DONOR BFME2 0x000B710D): after the object matrix, the module data's
//     AttachToBoneInAnotherModule (+0x40) is asked of Drawable::0x672B5B (DONOR BFME2 0x00272835, RW 0x4B6A6F..0x4B6B26), which walks
//     every draw module of the drawable, and the bone's world matrix REPLACES the object matrix; a bone that is not found leaves the
//     object matrix. Slot +0x104 (RW 0x9F3A3C base ret 4) is called only after a successful lookup.
//   * the bone is looked up in the draw modules' posed hierarchies: RW 0x004B2F19 (slot 0x14 of a module: Get_Bone_Index /
//     Get_Bone_Transform of the current animated pose). A module whose data carries no `Skeleton` line still poses the hierarchy its
//     model file names (W3D HLOD: the model's own skeleton) - the same lookup.
//
// The tests are synthetic (no game files): one object with four draw modules of the twins' shape, built out of synthetic W3D chunks.

#include "doctest.h"
#include "ObjectTestUtil.h"
#include "W3DDrawTestUtil.h"
#include "W3dSynth.h"

#include "Common/AsciiString.h"
#include "Common/Thing/ModuleFactory.h"
#include "Common/Thing/Thing.h"
#include "Common/Thing/ThingTemplate.h"
#include "GameClient/Drawable.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DDrawModules.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DDrawServices.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DLuaDrawScriptHost.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DScriptedModelDraw.h"
#include "Libraries/WWVegas/WW3D2/assetmgr.h"
#include "Libraries/WWVegas/WWMath/matrix3d.h"

#include <memory>
#include <string>
#include <vector>

using namespace drawtest;
using namespace w3dsynth;

namespace
{

class Port119Random : public W3DDrawRandom
{
public:
	int value(int lo, int hi) override { return lo == hi ? lo : (lo + hi) / 2; }
	float real(float lo, float hi) override { return (lo + hi) * 0.5f; }
};

// The twins' four modules on synthetic models. A rig (the emitters of the mod) is a skeleton with ONE bone for its figure's position:
// RIGA holds FIREBONE01 at x = +5, RIGB holds FIREBONE02 at x = -5 (the figures stand 10 apart), and both rig models are named by the
// INI's Model line only, exactly like euella_emmitr / euelro_emmitr (no Skeleton line).
struct TwoFigures
{
	MemoryFileSource fs;
	std::unique_ptr<WW3DAssetManager> assets;
	std::unique_ptr<WW3DDrawAssets> drawAssets;
	Harness rigAH, rigBH, figureAH, figureBH;
	Port119Random random;
	std::unique_ptr<W3DScriptedModelDraw> rigA, rigB, figureA, figureB;

	static void addBones(MemoryFileSource &fs, const std::string &name, const std::string &bone, float x)
	{
		std::string lower = name;
		for (char &c : lower)
			c = (char)std::tolower((unsigned char)c);
		const std::string sklName = name + "_SKL";
		const std::string sknName = name + "_SKN";
		const std::string dir = lower.substr(0, 2);
		fs.Add("art\\w3d\\" + dir + "\\" + lower + "_skl.w3d",
			hierarchyChunk(sklName.c_str(), { pivot("ROOTTRANSFORM", 0xFFFFFFFFu, 0, 0, 0), pivot("HIP", 0, 0, 0, 0), pivot(bone.c_str(), 1, x, 0, 0) }));
		std::vector<std::uint8_t> bytes;
		SynthMesh body;
		body.Vertices = { { 0, 0, 0 }, { 1, 0, 0 }, { 1, 1, 0 } };
		body.Triangles = { 0, 1, 2 };
		body.Container = sknName;
		body.Name = "BODY";
		append(bytes, meshChunk(body));
		append(bytes, hlodChunk(sknName.c_str(), sklName.c_str(), { { sknName + ".BODY", 2 } }));
		fs.Add("art\\w3d\\" + dir + "\\" + lower + "_skn.w3d", bytes);
	}

	static std::unique_ptr<W3DScriptedModelDraw> make(Harness &h, const std::string &body, W3DDrawAssets &a, W3DDrawRandom &r)
	{
		W3DModelDrawModuleData *d = h.parse(body);
		REQUIRE(d != nullptr);
		W3DScriptedModelDraw::Options o;
		o.buildBones = false;
		std::unique_ptr<W3DScriptedModelDraw> draw(new W3DScriptedModelDraw(*d, a, r, nullptr, o));
		const std::vector<std::string> errs = draw->errors();
		for (const std::string &e : errs)
		{
			CHECK_MESSAGE(false, e);
		}
		REQUIRE(errs.empty());
		return draw;
	}

	TwoFigures()
	{
		addBones(fs, "RIGA", "FIREBONE01", 5.0f);
		addBones(fs, "RIGB", "FIREBONE02", -5.0f);
		addBones(fs, "FIGA", "HAND", 0.0f);
		addBones(fs, "FIGB", "HAND", 0.0f);
		assets.reset(new WW3DAssetManager(fs));
		drawAssets.reset(new WW3DDrawAssets(*assets));
		// the rig bodies: the mod's emitter modules name only a Model (no Skeleton line)
		rigA = make(rigAH, "DefaultModelConditionState\n  Model = RIGA_SKN\nEnd\nEnd\n", *drawAssets, random);
		rigB = make(rigBH, "DefaultModelConditionState\n  Model = RIGB_SKN\nEnd\nEnd\n", *drawAssets, random);
		// the figures: attached to the rigs' bones
		figureA = make(figureAH,
			"AttachToBoneInAnotherModule = FIREBONE01\nDefaultModelConditionState\n  Model = FIGA_SKN\n  Skeleton = FIGA_SKL\nEnd\nEnd\n", *drawAssets, random);
		figureB = make(figureBH,
			"AttachToBoneInAnotherModule = FIREBONE02\nDefaultModelConditionState\n  Model = FIGB_SKN\n  Skeleton = FIGB_SKL\nEnd\nEnd\n", *drawAssets, random);
	}
};

} // namespace

TEST_CASE("PORT-119: AttachToBoneInAnotherModule reaches the draw module data and the draw entry (RW 0x4B6A6F)")
{
	TwoFigures syn;
	// the parse link the mod's INI uses: Draw = W3DScriptedModelDraw ... AttachToBoneInAnotherModule = FIREBONE01
	REQUIRE(syn.figureAH.data != nullptr);
	REQUIRE(syn.figureBH.data != nullptr);
	REQUIRE(syn.rigAH.data != nullptr);
	CHECK(syn.figureAH.data->m_attachToDrawableBone == std::string("firebone01"));
	CHECK(syn.figureBH.data->m_attachToDrawableBone == std::string("firebone02"));
	// a rig names no attach: it stands where the object stands
	CHECK(syn.rigAH.data->m_attachToDrawableBone.empty());
}

TEST_CASE("PORT-119: the two figures of a two-in-one hero stand on the rigs' bones, apart (RW 0x4B686D / 0x4B6A6F..0x4B6B26)")
{
	TwoFigures syn;
	objtest::World w;
	DrawServices services(*syn.assets, w.modules, W3DLuaDrawScriptHost::Options{});
	ThingTemplate tmpl;
	tmpl.friend_setTemplateName("ZwillingeTwoInOne");
	std::vector<std::string> problems;
	auto d = std::make_unique<Drawable>(services, &tmpl, 1, 1, std::vector<ClientEvent::ClientModule>{}, problems);
	Coord3D thingPos{ 100.0f, 200.0f, 200.0f }; // the horde/hero object stands here
	d->setPosition(&thingPos);

	auto addEntry = [&d](std::unique_ptr<W3DScriptedModelDraw> draw, const std::string &attach) {
		DrawEntry e;
		e.className = "W3DScriptedModelDraw";
		e.kind = W3D_DRAWKIND_MODEL;
		e.attachToBone = attach;
		e.draw = std::move(draw);
		d->entries().push_back(std::move(e));
	};
	addEntry(std::move(syn.rigA), "");
	addEntry(std::move(syn.figureA), "firebone01");
	addEntry(std::move(syn.rigB), "");
	addEntry(std::move(syn.figureB), "firebone02");
	REQUIRE(d->entries().size() == 4);

	Matrix3D rigMtx, figureAMtx, figureBMtx;
	CHECK_FALSE(d->entryTransform(d->entries()[0], rigMtx));
	CHECK(rigMtx.Row[0][3] == 100.0f);

	// figure A on FIREBONE01 of RIGA: x = 100 + 5, the same object y/z
	REQUIRE(d->entryTransform(d->entries()[1], figureAMtx));
	CHECK(figureAMtx.Row[0][3] == 105.0f);
	CHECK(figureAMtx.Row[1][3] == 200.0f);

	// figure B on FIREBONE02 of RIGB: x = 100 - 5
	REQUIRE(d->entryTransform(d->entries()[3], figureBMtx));
	CHECK(figureBMtx.Row[0][3] == 95.0f);
	CHECK(figureBMtx.Row[1][3] == 200.0f);

	// side by side, not one inside the other
	CHECK(figureAMtx.Row[0][3] - figureBMtx.Row[0][3] == 10.0f);
}

TEST_CASE("PORT-119: the template's four Draw modules of a two-in-one hero keep their attach (RW 0x4B6A6F)")
{
	TwoFigures syn;
	objtest::World w;
	W3DDrawModules::registerTypedDrawModuleData(w.modules); // the game's own binding (RetailObjectWorld::init), before the INI is read
	// the mod's object block: two rigs (Model only) and two attached figures, in INI order
	const std::string ini = "Object PT119TwoInOne\n"
		"    Draw = W3DScriptedModelDraw ModuleTag_RigA\n"
		"        DefaultModelConditionState\n            Model = RIGA_SKN\n        End\n"
		"    End\n"
		"    Draw = W3DScriptedModelDraw ModuleTag_FigA\n"
		"        AttachToBoneInAnotherModule = FIREBONE01\n"
		"        DefaultModelConditionState\n            Model = FIGA_SKN\n            Skeleton = FIGA_SKL\n        End\n"
		"    End\n"
		"    Draw = W3DScriptedModelDraw ModuleTag_RigB\n"
		"        DefaultModelConditionState\n            Model = RIGB_SKN\n        End\n"
		"    End\n"
		"    Draw = W3DScriptedModelDraw ModuleTag_FigB\n"
		"        AttachToBoneInAnotherModule = FIREBONE02\n"
		"        DefaultModelConditionState\n            Model = FIGB_SKN\n            Skeleton = FIGB_SKL\n        End\n"
		"    End\n"
		"End\n";
	CHECK(w.load(ini).empty());
	const ThingTemplate *tmpl = w.get("PT119TwoInOne");
	REQUIRE(tmpl != nullptr);
	DrawServices services(*syn.assets, w.modules, W3DLuaDrawScriptHost::Options{});
	std::vector<std::string> problems;
	auto d = std::make_unique<Drawable>(services, tmpl, 1, 1, std::vector<ClientEvent::ClientModule>{}, problems);
	Coord3D thingPos{ 100.0f, 200.0f, 200.0f };
	d->setPosition(&thingPos);
	CHECK(problems.empty());
	REQUIRE(d->entries().size() == 4);
	CHECK(d->entries()[0].attachToBone.empty());
	CHECK(d->entries()[1].attachToBone == std::string("firebone01"));
	CHECK(d->entries()[2].attachToBone.empty());
	CHECK(d->entries()[3].attachToBone == std::string("firebone02"));

	Matrix3D a, b;
	REQUIRE(d->entryTransform(d->entries()[1], a));
	REQUIRE(d->entryTransform(d->entries()[3], b));
	CHECK(a.Row[0][3] == 105.0f);
	CHECK(b.Row[0][3] == 95.0f);
}

TEST_CASE("PORT-119: a figure whose rig has no such bone keeps the object's transform (RW 0x4B6A81)")
{
	TwoFigures syn;
	objtest::World w;
	DrawServices services(*syn.assets, w.modules, W3DLuaDrawScriptHost::Options{});
	ThingTemplate tmpl;
	tmpl.friend_setTemplateName("ZwillingeTwoInOne");
	std::vector<std::string> problems;
	auto d = std::make_unique<Drawable>(services, &tmpl, 1, 1, std::vector<ClientEvent::ClientModule>{}, problems);
	Coord3D thingPos{ 100.0f, 200.0f, 200.0f };
	d->setPosition(&thingPos);
	DrawEntry e;
	e.className = "W3DScriptedModelDraw";
	e.kind = W3D_DRAWKIND_MODEL;
	e.attachToBone = "NOSUCHBONE";
	e.draw = std::move(syn.figureA);
	d->entries().push_back(std::move(e));
	Matrix3D mtx;
	CHECK_FALSE(d->entryTransform(d->entries()[0], mtx));
	CHECK(mtx.Row[0][3] == 100.0f);
	CHECK(mtx.Row[1][3] == 200.0f);
}
