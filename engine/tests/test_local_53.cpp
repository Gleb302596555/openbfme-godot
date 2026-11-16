// OpenBFME tests, Local 53 (fork issue 409, P-98): with the Powers screen (the abilities palantir, SpellStore.apt) open, a thin light vertical line
// showed on the right, at x = 1680 of 1920 x 1080. GPL-3.0.
//
// Found with the real data (Ennorath 1.9.8.3 over RotWK 2.01, the screen from Ennorath's widescreen.big): no image of the screen crosses that
// column. The Powers screen's own 3D background is the View3D clip `_level<N>.SpellStoreBackground` (`_RenderObj` SFE_MenuSpells), whose
// rectangle at 1920 x 1080 is x = 240 .. 1680 over the full height: its right edge is the line. RotWK renders a View3D straight into the clip's
// viewport (RW 0xB5470F); the port composites it (GodotAptView3D: the model over black and over white, and the screen behind the clip read from a
// back buffer copy of the clip's rectangle). That rectangle is fractional, and a copy of a fractional rectangle (Godot converts it to whole pixels
// by truncation) left the last column out, so the composite read a stale back buffer there. AptView3DCopyRect gives the copy the clip's whole
// pixels.

#include "doctest.h"

#include "HudTestUtil.h"

#include "GameClient/AptCanvas.h"
#include "GameClient/AptView3D.h"
#include "GameClient/GameTextTableSource.h"
#include "GameClient/GUI/AptScreens/AptSpellStore.h"
#include "GameClient/GUI/ShellServices.h"
#include "GameClient/GUI/WindowManager.h"
#include "GameClient/InGameHud.h"
#include "Libraries/Source/Apt/AptRenderList.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

using namespace hudtest;

namespace
{
struct StoreRig
{
	Rig rig;
	RecordingShellServices services;
	GameTextTableSource text;
	std::unique_ptr<InGameHud> hud;
	StoreRig(SharedWorld &s, const char *map, const char *faction) : rig(s, map, faction)
	{
		std::string error;
		REQUIRE_MESSAGE(text.table.loadRetail(*s.mount->fs, &error), error);
		InGameHud::Config cfg{ *rig.game, *s.world, *s.mount->fs, services, rig.view, s.mouse, s.meta, &text };
		hud = std::make_unique<InGameHud>(cfg);
		REQUIRE_MESSAGE(hud->boot(&error), error);
	}
	~StoreRig() { hud.reset(); }
	void frames(int n)
	{
		for (int i = 0; i < n; ++i)
		{
			hud->update(0.033);
			rig.game->advance(0.033);
		}
	}
};

// the window rectangle GodotAptPlayer gives a View3D placeholder (its bounds through the stage matrix and mapping, GodotAptPlayer.cpp "lane UI-2")
struct ClipRect
{
	float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
};
ClipRect clipRectOf(const AptCanvasOp &op)
{
	ClipRect r;
	const float xs[4] = { op.bounds[0], op.bounds[2], op.bounds[2], op.bounds[0] };
	const float ys[4] = { op.bounds[1], op.bounds[1], op.bounds[3], op.bounds[3] };
	for (int i = 0; i < 4; ++i)
	{
		const float sx = (op.matrix.a * xs[i] + op.matrix.c * ys[i] + op.matrix.tx) * op.scaleX + op.offsetX;
		const float sy = (op.matrix.b * xs[i] + op.matrix.d * ys[i] + op.matrix.ty) * op.scaleY + op.offsetY;
		r.x0 = std::min(r.x0, sx), r.x1 = std::max(r.x1, sx), r.y0 = std::min(r.y0, sy), r.y1 = std::max(r.y1, sy);
	}
	return r;
}
} // namespace

TEST_CASE("LOCAL-53: the View3D back buffer copy covers every pixel of a fractional clip rectangle")
{
	float out[4];
	// the Powers screen's background at 1920 x 1080 as the stage mapping gives it (LOCAL-53 retail): x 239.999603 .. 1680.000488,
	// y 0.008326 .. 1079.994385
	AptView3DCopyRect(239.999603f, 0.008326f, 1440.000885f, 1079.986059f, out);
	CHECK(out[0] == 239.0f);
	CHECK(out[1] == 0.0f);
	CHECK(out[0] + out[2] == 1681.0f); // through the clip's last (partial) column, x = 1680
	CHECK(out[1] + out[3] == 1080.0f);
	// whole pixels stay as they are
	AptView3DCopyRect(240.0f, 0.0f, 1440.0f, 1080.0f, out);
	CHECK(out[0] == 240.0f);
	CHECK(out[2] == 1440.0f);
	CHECK(out[3] == 1080.0f);
	// a truncating conversion (Godot's Rect2 -> Rect2i) of the same rectangle ends at 239 + 1440 = 1679: column 1679, the clip's right edge, was
	// not copied (the line the owner saw)
	CHECK((int)239.999603f + (int)1440.000885f == 1679);
}

TEST_CASE("LOCAL-53 retail: the Powers screen's 3D background (SpellStoreBackground, SFE_MenuSpells) gets a back buffer copy through its last column")
{
	if (!haveWorld("LOCAL-53 retail"))
	{
		return;
	}
	SharedWorld &s = shared();
	const char *map = std::getenv("OPENBFME_SWEEP_MAP") ? std::getenv("OPENBFME_SWEEP_MAP") : "map mp fall back 4p";
	for (const auto &size : { std::pair<int, int>{ 1920, 1080 }, std::pair<int, int>{ 2560, 1440 } })
	{
		CAPTURE(size.first);
		StoreRig b(s, map, "FactionMordor");
		REQUIRE(b.hud->windows().loadBackground()); // the shell loads Background.apt at boot (GodotAptPlayer); the test HUD does not
		b.hud->setWindowSize(size.first, size.second);
		b.frames(10);
		std::string error;
		REQUIRE_MESSAGE(b.hud->openSpellStore(&error), error);
		b.frames(40);
		REQUIRE(b.hud->spellStore());
		AptArchiveFileSource source(*s.mount->fs);
		AptTextureStore textures(source);
		GameTextTable strings;
		AptRenderList rl;
		b.hud->apt().buildRenderList(rl);
		AptCanvasInputs in;
		in.mapping.windowW = size.first;
		in.mapping.windowH = size.second;
		in.textures = &textures;
		in.text = &strings;
		AptCanvasList out;
		BuildAptCanvas(rl, in, out, false);
		const AptCanvasOp *bg = nullptr;
		for (const AptCanvasOp &op : out.ops)
		{
			if (op.kind == AptCanvasOp::Kind::Placeholder && op.symbolName == "View3D" && op.renderObject == "SFE_MenuSpells")
			{
				bg = &op;
			}
		}
		REQUIRE_MESSAGE(bg, "no View3D of SFE_MenuSpells on the open Powers screen");
		const ClipRect r = clipRectOf(*bg);
		MESSAGE(bg->path << " x " << std::to_string(r.x0) << " .. " << std::to_string(r.x1) << " y " << std::to_string(r.y0) << " .. " << std::to_string(r.y1));
		// the clip spans the screen's height and ends at the right edge of the centred 4:3 area (1680 at 1920 x 1080)
		const float right = (float)size.first / 2.0f + (float)size.second * 4.0f / 3.0f / 2.0f;
		CHECK(r.x1 == doctest::Approx(right).epsilon(0.001)); // tolerance: the clip's float stage mapping lands a hair off the whole pixel
		CHECK(r.y1 - r.y0 == doctest::Approx((float)size.second).epsilon(0.001)); // tolerance: the same float mapping
		float copy[4];
		AptView3DCopyRect(r.x0, r.y0, r.x1 - r.x0, r.y1 - r.y0, copy);
		// every column and row the clip touches is copied: the composite never reads a stale back buffer at the clip's edges
		CHECK(copy[0] == std::floor(r.x0));
		CHECK(copy[0] + copy[2] == std::ceil(r.x1));
		CHECK(copy[1] == std::floor(r.y0));
		CHECK(copy[1] + copy[3] == std::ceil(r.y1));
		// the old copy (truncated position and size) ended one column short of the clip's right edge: column right - 1 was the line
		CHECK((int)r.x0 + (int)(r.x1 - r.x0) == (int)right - 1);
	}
}
