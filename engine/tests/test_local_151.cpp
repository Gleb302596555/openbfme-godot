// OpenBFME. GPL-3.0.
// LOCAL-151 (fork issue 884):
//   - MouseCursor Texture: Win32Mouse::initCursorResources (RW 0x441506, parse table RW 0xBF6108 +0x28) reads Texture, not Image (+0x2C, RW 0xBF60F8).
//   - Weather CloudTextureSize (+0xA4) / CloudOffsetPerSecond (+0xAC): RW 0x4E2521 (ScaleUVOffsetPerSecondUV, DONOR BFME2 W3DTerrainFXVectorParams.cpp:303).
//   - Weather SnowPointSize (+0x28) / SnowMaxPointSize (+0x2C) / SnowMinPointSize (+0x30) / SnowPointSprites (+0x44): RW 0x600ADE + RW 0x494117 (0x494190, 0x4942C4..0x494350).

#include "doctest.h"

#include "GameClient/CursorFile.h"
#include "GameClient/SnowManager.h"
#include "GameClient/WeatherSetting.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DDrawServices.h"

#include <cmath>
#include <map>
#include <string>

namespace
{
struct FixedDrawRandom final : W3DDrawRandom
{
	int value(int lo, int) override { return lo; }
	float real(float lo, float) override { return lo; }
};
} // namespace

TEST_CASE("LOCAL-151 MouseCursor Texture: ParseMouseCursors reads Texture for Win32Mouse::initCursorResources (RW 0x441506)")
{
	// Expectations from RW 0x441506 (Win32Mouse::initCursorResources: lea esi, [edi - 0x28] -> CursorInfo::textureName +0x28, RW 0xBF6108).
	const std::string ini =
		"MouseCursor Arrow\n"
		"  Image = WrongImage\n"
		"  Texture = SCCPointer.cur\n"
		"  HotSpot = X:2 Y:3\n"
		"End\n"
		"MouseCursor CustomOnlyTex\n"
		"  Texture = CustomAnim\n"
		"End\n"
		"MouseCursor Scroll\n"
		"  Image = WrongScroll.cur\n"
		"  Texture = SCCScroll.cur\n"
		"  Directions = 8\n"
		"End\n";

	std::map<std::string, MouseCursorEntry> cursors;
	std::string error;
	REQUIRE_MESSAGE(ParseMouseCursors(ini, cursors, &error), error);
	REQUIRE(cursors.size() == 3);

	CHECK(cursors["Arrow"].texture == "SCCPointer.cur");
	CHECK(cursors["Arrow"].image == "WrongImage");
	CHECK(CursorFileName(cursors["Arrow"].texture, 1, 0) == "SCCPointer.cur");

	CHECK(cursors["CustomOnlyTex"].texture == "CustomAnim");
	CHECK(cursors["CustomOnlyTex"].image.empty());
	CHECK(CursorFileName(cursors["CustomOnlyTex"].texture, 1, 0) == "CustomAnim.ani");

	CHECK(cursors["Scroll"].texture == "SCCScroll.cur");
	CHECK(cursors["Scroll"].directions == 8);
	CHECK(CursorFileName(cursors["Scroll"].texture, cursors["Scroll"].directions, 3) == "SCCScroll3.cur");
}

TEST_CASE("LOCAL-151 Weather CloudTextureSize and CloudOffsetPerSecond: ScaleUVOffsetPerSecondUV (RW 0x4E2521)")
{
	// Expectations from RW 0x4E2521 (ScaleUVOffsetPerSecondUV):
	// defaults: (0.0015151515, 0.0015151515, -0.012, -0.018) (RW 0xBE462C / 0xBE4628 / 0xBE4624);
	// override: (1 / CloudTextureSize.x, 1 / CloudTextureSize.y, CloudOffsetPerSecond.x, CloudOffsetPerSecond.y).
	WeatherSetting def;
	float out[4] = {};
	def.cloudScaleOffset(out);
	CHECK(out[0] == 1.0f / 660.0f);
	CHECK(out[1] == 1.0f / 660.0f);
	CHECK(out[2] == -0.012f);
	CHECK(out[3] == -0.018f);

	WeatherSettingStore store;
	std::string error;
	const std::string baseIni =
		"Weather\n"
		"  CloudTextureSize = X:800.0 Y:400.0\n"
		"  CloudOffsetPerSecond = X:0.05 Y:-0.025\n"
		"End\n";
	REQUIRE_MESSAGE(store.loadText(nullptr, "weather.ini", baseIni, 1, &error), error);
	store.finalSetting().cloudScaleOffset(out);
	CHECK(out[0] == 1.0f / 800.0f);
	CHECK(out[1] == 1.0f / 400.0f);
	CHECK(out[2] == 0.05f);
	CHECK(out[3] == -0.025f);

	// Neighboring override behavior (map.ini overrides weather.ini)
	const std::string mapIni =
		"Weather\n"
		"  CloudTextureSize = X:500.0 Y:250.0\n"
		"  CloudOffsetPerSecond = X:-0.03 Y:0.01\n"
		"End\n";
	REQUIRE_MESSAGE(store.loadText(nullptr, "map.ini", mapIni, 2, &error), error);
	store.finalSetting().cloudScaleOffset(out);
	CHECK(out[0] == 1.0f / 500.0f);
	CHECK(out[1] == 1.0f / 250.0f);
	CHECK(out[2] == -0.03f);
	CHECK(out[3] == 0.01f);
}

TEST_CASE("LOCAL-151 Weather SnowPointSize, Min/Max and SnowPointSprites (RW 0x600ADE, 0x494117)")
{
	// Expectations from RW 0x600ADE (updateIniSettings: +0x28/+0x2C/+0x30) and RW 0x494117 (W3DSnowManager::render:
	// SnowPointSprites +0x44 RW 0x494190, D3DRS_POINTSIZE 0x4942DC, MIN 0x4942F3, MAX 0x49430A, POINTSCALE_C=1.0).
	// Exercised in Ennorath maps (map mp fords of glanduin, map mp greenleaf forest: SnowPointSprites = yes).
	WeatherSettingStore store;
	std::string error;
	const std::string ini =
		"Weather\n"
		"  SnowEnabled = yes\n"
		"  IsSnowing = yes\n"
		"  SnowPointSprites = yes\n"
		"  SnowPointSize = 5.0\n"
		"  SnowMinPointSize = 40.0\n"
		"  SnowMaxPointSize = 80.0\n"
		"  SnowQuadSize = 1.5\n"
		"  SnowBoxHeight = 200.0\n"
		"  SnowSpacing = 50.0\n"
		"  SnowSpeed = 30.0\n"
		"  NumberTiles = 2\n"
		"End\n";
	REQUIRE_MESSAGE(store.loadText(nullptr, "weather.ini", ini, 1, &error), error);

	SnowManager snow;
	snow.init(store.finalSetting());
	FixedDrawRandom rng;
	const SnowDrawParams p = snow.drawParams(store.finalSetting(), 100, 100, 0, rng);
	CHECK(p.draw);
	CHECK(p.pointSprites);
	CHECK(p.pointSize == 5.0f);
	CHECK(p.minPointSize == 40.0f);
	CHECK(p.maxPointSize == 80.0f);

	// D3D9 point sprite screen size: clamp(Vh * pointSize / d, minPointSize, maxPointSize)
	CHECK(SnowManager::pointSpriteSizePx(p, 1000.0f, 100.0f) == 50.0f);
	CHECK(SnowManager::pointSpriteSizePx(p, 1000.0f, 200.0f) == 40.0f);
	CHECK(SnowManager::pointSpriteSizePx(p, 1000.0f, 50.0f) == 80.0f);

	// Neighboring behavior: SnowPointSprites = no keeps quad path active without changing quadHalf
	const std::string quadIni =
		"Weather\n"
		"  SnowPointSprites = no\n"
		"  SnowQuadSize = 2.0\n"
		"End\n";
	REQUIRE_MESSAGE(store.loadText(nullptr, "map.ini", quadIni, 2, &error), error);
	snow.updateIniSettings(store.finalSetting());
	const SnowDrawParams q = snow.drawParams(store.finalSetting(), 100, 100, 0, rng);
	CHECK_FALSE(q.pointSprites);
	CHECK(q.quadHalf == 2.0f * 0.707f);
}
