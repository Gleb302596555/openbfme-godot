// Both editions: RW 0x004D102E, BFME2 0x004D181D.
#include "doctest.h"
#include "ObjectTestUtil.h"

#include "Common/BfmeGame.h"
#include "Common/GameEdition.h"
#include "Common/INI.h"
#include "Common/XferLoad.h"
#include "Common/XferSave.h"
#include "Common/Thing/Thing.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DTornadoDraw.h"

#include <cstring>
#include <string>

namespace
{
const char kTornadoIni[] =
    "Object TestTornadoObj\n"
    "  Draw = W3DTornadoDraw ModuleTag_TornadoDraw\n"
    "    DecalTemplate\n"
    "      Texture = EXWhirlwindground\n"
    "      Style = SHADOW_ALPHA_DECAL\n"
    "      OpacityMin = 5%\n"
    "      OpacityMax = 40%\n"
    "      MaxRadius = 100.0\n"
    "      RotationsPerMinute = 0.5\n"
    "      SpiralAcceleration = 1.0\n"
    "    End\n"
    "    DecalCount = 7\n"
    "    DecalMaxRadius = 300.0\n"
    "  End\n"
    "End\n";
} // namespace

TEST_CASE("modgap2 tornado: module data defaults and field table (both editions: RW 0x004D0EC5 / 0x00BE4130, BFME2 0x004D16B4 / 0x00BCE150)")
{
	W3DTornadoDrawModuleData d;
	CHECK(d.m_decalCount == 0u);
	CHECK(d.m_decalMaxRadius == 200.0f);
	CHECK(d.m_decalTemplate.style == 0x20u); // SHADOW_ALPHA_DECAL

	const FieldParse *t = W3DTornadoDrawTables::tornado();
	const char *const expected[] = { "DecalTemplate", "DecalCount", "DecalMaxRadius" };
	int n = 0;
	for (; t[n].token; ++n)
	{
		// tolerance: bounded table enumeration check
		REQUIRE(n < 3);
		CHECK(std::strcmp(t[n].token, expected[n]) == 0);
	}
	CHECK(n == 3);
}

TEST_CASE("modgap2 tornado: parsing INI with DecalTemplate, DecalCount, DecalMaxRadius")
{
	for (GameEdition edition : { GameEdition::RotWK201, GameEdition::BFME2_106 })
	{
		CAPTURE(gameEditionName(edition));
		objtest::World w;
		W3DTornadoDraw::registerClass(w.modules);
		REQUIRE(w.load(kTornadoIni).empty());

		const ThingTemplate *tmpl = w.get("TestTornadoObj");
		REQUIRE(tmpl != nullptr);
		const auto &n = tmpl->drawModules().nuggets();
		REQUIRE(n.size() == 1u);
		const auto *data = dynamic_cast<const W3DTornadoDrawModuleData *>(n[0].data.get());
		REQUIRE(data != nullptr);
		CHECK(data->m_decalCount == 7u);
		CHECK(data->m_decalMaxRadius == 300.0f);
		CHECK(data->m_decalTemplate.maxRadius == 100.0f);
	}
}

TEST_CASE("modgap2 tornado: runtime creates decals, follows transform, and xfers (both editions)")
{
	for (GameEdition edition : { GameEdition::RotWK201, GameEdition::BFME2_106 })
	{
		CAPTURE(gameEditionName(edition));
		objtest::World w;
		W3DTornadoDraw::registerClass(w.modules);
		REQUIRE(w.load(kTornadoIni).empty());

		const ThingTemplate *tmpl = w.get("TestTornadoObj");
		REQUIRE(tmpl != nullptr);
		const auto &n = tmpl->drawModules().nuggets();
		REQUIRE(n.size() == 1u);
		const ModuleData *md = n[0].data.get();
		REQUIRE(md != nullptr);

		Thing thing(tmpl);
		const Coord3D p1{ 150.0f, 250.0f, 0.0f };
		thing.setPosition(&p1);

		W3DTornadoDraw tornado(&thing, md);
		CHECK(tornado.decalCount() == 7u);
		REQUIRE(tornado.decals().size() == 7u);
		CHECK(tornado.decals()[0].position.x == 150.0f);
		CHECK(tornado.decals()[0].position.y == 250.0f);

		const Coord3D p2{ 300.0f, 400.0f, 10.0f };
		thing.setPosition(&p2);
		tornado.reactToTransformChange(nullptr, 0.0f);
		CHECK(tornado.decals()[0].position.x == 300.0f);
		CHECK(tornado.decals()[0].position.y == 400.0f);
		CHECK(tornado.decals()[6].position.x == 300.0f);
		CHECK(tornado.decals()[6].position.y == 400.0f);

		// Snapshot save and load
		XferSave save;
		tornado.xfer(save);

		W3DTornadoDraw restored(&thing, md);
		restored.clearDecals();
		CHECK(restored.decalCount() == 0u);

		const auto &buffer = save.data();
		XferLoad load(buffer.data(), buffer.size());
		restored.xfer(load);
		CHECK(restored.decalCount() == 7u);
		CHECK(restored.decals()[0].position.x == 300.0f);
		CHECK(restored.decals()[0].position.y == 400.0f);
	}
}
