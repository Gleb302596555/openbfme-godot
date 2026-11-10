// OpenBFME unit tests: LOCAL-26 water and sky retail parity (issue #188).
//
// RotWK 2.01 addresses & evidence:
//   WaterSet:
//     Keyword: "WaterSet" in block table (RW 0xDC51C0)
//     Parser: RW 0x600564: INI::parseWaterSettingDefinition (INIWater.cpp)
//     Parse table: RW 0xC1E4A0 (12 fields: SkyTexture, WaterTexture, Vertex00Color, Vertex10Color,
//       Vertex01Color, Vertex11Color, DiffuseColor, TransparentDiffuseColor, UScrollPerMS,
//       VScrollPerMS, SkyTexelsPerUnit, WaterRepeatCount)
//     Store: WaterSettings[6] array at RW 0xDE4BE8 (indexed by TimeOfDay 0..5, RW 0xDA3A68)
//     Readers:
//       loadSetting RW 0x47E865 called by init RW 0x47F544 for TOD 1..4 (Morning, Afternoon, Evening, Night)
//       Active runtime renderer reader: drawRiverWater RW 0x5004B6 (reads DiffuseColor to modulate RiverArea vertex colors)
//       Remaining 11 fields are legacy fixed-function DX8/DX9 residuals (stop S-035 / S-7220, unread by shaders)
//   WaterTextureList:
//     Keyword: "WaterTextureList" in block table (RW 0xDC51C0)
//     Parser: RW 0x60047C: INI::parseWaterTextureList (INIWater.cpp)
//     Parse table: RW 0xC350A8
//     Store: pointer at RW 0xDE8790 (.bss, always NULL in retail; zero write instructions in binary, only 1 read at 0x6004AA; caller count = 1 at 0x7B2379)
//     Reader: None in RotWK 2.01 runtime rendering. Dead WB remnant (WB 0xB29220). Stop S-7220.
//   SkyboxTextureSet:
//     Keyword: "SkyboxTextureSet" in block table (RW 0xDC51C0)
//     Parser: RW 0x60D3E9
//     Parse table: RW 0xC1E7B0
//     Store: RW 0xDE4EDC
//     Reader: None in RotWK 2.01 runtime rendering. Stop S-1653.

#include "doctest.h"
#include "BigTestUtil.h"
#include "IniTestUtil.h"
#include "RetailTestMount.h"

#include "Common/ArchiveFileSystem.h"
#include "Common/INI.h"
#include "Common/INI/INIBlockStubs.h"
#include "GameClient/MapWeather.h"
#include "GameClient/WaterGeometry.h"
#include "GameClient/WeatherSettings.h"
#include "GameEngineDevice/Win32Device/Common/Win32BIGFileSystem.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace
{

bool isColorEqual(const RGBAColorInt &c, int r, int g, int b, int a)
{
	return c.red == r && c.green == g && c.blue == b && c.alpha == a;
}

bool containsKeyword(const std::vector<std::string> &keywords, const std::string &keyword)
{
	return std::find(keywords.begin(), keywords.end(), keyword) != keywords.end();
}

bool mountTestBig(Win32BIGFileSystem &fsys, const std::vector<std::pair<std::string, std::string>> &files, const std::string &archiveName = "test.big")
{
	std::string error;
	auto archive = fsys.openArchiveFromMemory(archiveName, std::make_shared<const std::vector<std::uint8_t>>(bigtest::makeBig("BIGF", files)), &error);
	if (!archive)
	{
		return false;
	}
	fsys.mountArchive(std::move(archive), archiveName, false);
	return true;
}

} // namespace

TEST_CASE("LOCAL-26b: WaterSet parses all 12 fields into WaterSetting (RW 0xC1E4A0)")
{
	const std::string text =
		"WaterSet EVENING\n"
		"  SkyTexture = sky_ev.tga\n"
		"  WaterTexture = water_ev.tga\n"
		"  Vertex00Color = R:1 G:2 B:3 A:4\n"
		"  Vertex10Color = R:5 G:6 B:7 A:8\n"
		"  Vertex01Color = R:9 G:10 B:11 A:12\n"
		"  Vertex11Color = R:13 G:14 B:15 A:16\n"
		"  DiffuseColor = R:225 G:225 B:225 A:255\n"
		"  TransparentDiffuseColor = R:21 G:22 B:23 A:24\n"
		"  UScrollPerMS = 0.05\n"
		"  VScrollPerMS = -0.02\n"
		"  SkyTexelsPerUnit = 3.5\n"
		"  WaterRepeatCount = 7\n"
		"End\n"
		"WaterSet AFTERNOON\n"
		"  DiffuseColor = R:185 G:185 B:185 A:255\n"
		"End\n";

	WaterSetScan::Settings settings;
	std::string error;
	REQUIRE_MESSAGE(WaterSetScan::applyText(text, settings, &error), error);

	// EVENING slot (3) has all 12 fields populated
	const WaterSetting &ev = settings.waterSettings[3];
	CHECK(ev.skyTextureFile == "sky_ev.tga");
	CHECK(ev.waterTextureFile == "water_ev.tga");
	CHECK(isColorEqual(ev.vertex00Diffuse, 1, 2, 3, 4));
	CHECK(isColorEqual(ev.vertex10Diffuse, 5, 6, 7, 8));
	CHECK(isColorEqual(ev.vertex01Diffuse, 9, 10, 11, 12));
	CHECK(isColorEqual(ev.vertex11Diffuse, 13, 14, 15, 16));
	CHECK(isColorEqual(ev.waterDiffuseColor, 225, 225, 225, 255));
	CHECK(isColorEqual(ev.transparentWaterDiffuse, 21, 22, 23, 24));
	CHECK(ev.uScrollPerMs == 0.05f);
	CHECK(ev.vScrollPerMs == -0.02f);
	CHECK(ev.skyTexelsPerUnit == 3.5f);
	CHECK(ev.waterRepeatCount == 7);

	// AFTERNOON slot (2) has DiffuseColor set, others at default
	const WaterSetting &aft = settings.waterSettings[2];
	CHECK(isColorEqual(aft.waterDiffuseColor, 185, 185, 185, 255));
	CHECK(aft.skyTextureFile.empty());
	CHECK(aft.waterRepeatCount == 0);

	// Unmentioned slots retain default constructor values
	const WaterSetting &none = settings.waterSettings[0]; // NONE
	CHECK(isColorEqual(none.waterDiffuseColor, 0, 0, 0, 0));
	const WaterSetting &interp = settings.waterSettings[5]; // INTERPOLATE
	CHECK(isColorEqual(interp.waterDiffuseColor, 0, 0, 0, 0));

	// Rejection of invalid fields leaves existing state untouched
	CHECK_FALSE(WaterSetScan::applyText("WaterSet MORNING\n  DiffuseColor = invalid\nEnd\n", settings, &error));
	CHECK(isColorEqual(settings.waterSettings[3].waterDiffuseColor, 225, 225, 225, 255));
}

TEST_CASE("LOCAL-26c: WaterSetScan loadSettings hierarchy and retail parity")
{
	Win32BIGFileSystem fsys;
	const std::string baseWater =
		"WaterSet MORNING\n  DiffuseColor = R:175 G:175 B:175 A:255\nEnd\n"
		"WaterSet AFTERNOON\n  DiffuseColor = R:185 G:185 B:185 A:255\nEnd\n"
		"WaterSet EVENING\n  DiffuseColor = R:225 G:225 B:225 A:255\nEnd\n"
		"WaterSet NIGHT\n  DiffuseColor = R:100 G:100 B:100 A:255\nEnd\n";
	const std::string mapWater =
		"WaterSet MORNING\n  DiffuseColor = R:200 G:200 B:200 A:255\nEnd\n";

	REQUIRE(mountTestBig(fsys, {
		{ "data\\ini\\water.ini", baseWater },
		{ "maps\\map_test\\map.ini", mapWater }
	}));

	WaterSetScan::Settings loaded;
	std::string err;
	REQUIRE_MESSAGE(WaterSetScan::loadSettings(fsys, "maps\\map_test", loaded, &err), err);

	// Map override replaced MORNING slot
	CHECK(isColorEqual(loaded.waterSettings[1].waterDiffuseColor, 200, 200, 200, 255));
	// Unoverridden slots preserve values from base water.ini
	CHECK(isColorEqual(loaded.waterSettings[2].waterDiffuseColor, 185, 185, 185, 255));
	CHECK(isColorEqual(loaded.waterSettings[3].waterDiffuseColor, 225, 225, 225, 255));
	CHECK(isColorEqual(loaded.waterSettings[4].waterDiffuseColor, 100, 100, 100, 255));

	// Pure retail data validation when retail archives are mounted
	retailtest::Mount *m = retailtest::pureMount();
	if (m && m->fs)
	{
		WaterSetScan::Settings retailSettings;
		REQUIRE_MESSAGE(WaterSetScan::loadSettings(*m->fs, "maps\\no_such_map", retailSettings, &err), err);
		CHECK(isColorEqual(retailSettings.waterSettings[1].waterDiffuseColor, 175, 175, 175, 255));
		CHECK(isColorEqual(retailSettings.waterSettings[2].waterDiffuseColor, 185, 185, 185, 255));
		CHECK(isColorEqual(retailSettings.waterSettings[3].waterDiffuseColor, 225, 225, 225, 255));
		CHECK(isColorEqual(retailSettings.waterSettings[4].waterDiffuseColor, 100, 100, 100, 255));
	}
}

TEST_CASE("LOCAL-26d: river vertex tinting reproduces retail drawRiverWater (RW 0x5004B6)")
{
	WaterSetScan::Settings settings;
	std::string error;
	REQUIRE(WaterSetScan::applyText(
		"WaterSet AFTERNOON\n  DiffuseColor = R:185 G:185 B:185 A:255\nEnd\n"
		"WaterSet NIGHT\n  DiffuseColor = R:100 G:100 B:100 A:255\nEnd\n", settings, &error));

	float diffuseAft[4]{};
	REQUIRE(WaterSetScan::diffuseOf(settings, 2, diffuseAft, &error));
	CHECK(diffuseAft[0] == 185.0f / 255.0f);
	CHECK(diffuseAft[1] == 185.0f / 255.0f);
	CHECK(diffuseAft[2] == 185.0f / 255.0f);
	CHECK(diffuseAft[3] == 1.0f);

	// drawRiverWater RW 0x5004B6 modulates RiverArea vertex colour
	TimeOfDayLights tod;
	for (int c = 0; c < 3; ++c)
	{
		tod.terrain[0].ambient[c] = 1.0f;
	}

	RiverArea riverWhite;
	riverWhite.r = 255;
	riverWhite.g = 255;
	riverWhite.b = 255;
	float outLit[4]{};
	WaterGeometry::riverVertexColour(riverWhite, &tod, diffuseAft, outLit);
	CHECK(outLit[0] == 185.0f / 255.0f);
	CHECK(outLit[1] == 185.0f / 255.0f);
	CHECK(outLit[2] == 185.0f / 255.0f);
	CHECK(outLit[3] == 1.0f);

	// Black river (0, 0, 0) in retail is treated as unlit white (1.0f, 1.0f, 1.0f)
	RiverArea riverBlack;
	riverBlack.r = 0;
	riverBlack.g = 0;
	riverBlack.b = 0;
	float outBlack[4]{};
	WaterGeometry::riverVertexColour(riverBlack, &tod, diffuseAft, outBlack);
	CHECK(outBlack[0] == 1.0f);
	CHECK(outBlack[1] == 1.0f);
	CHECK(outBlack[2] == 1.0f);
	CHECK(outBlack[3] == 1.0f);

	// Out-of-bounds TOD indices return false and leave the buffer untouched
	float untouched[4] = { 1.0f, 2.0f, 3.0f, 4.0f };
	CHECK_FALSE(WaterSetScan::diffuseOf(settings, -1, untouched, &error));
	CHECK(untouched[0] == 1.0f);
	CHECK_FALSE(WaterSetScan::diffuseOf(settings, 6, untouched, &error));
	CHECK(untouched[0] == 1.0f);
}

TEST_CASE("LOCAL-26e: WaterTextureList and SkyboxTextureSet binary facts (S-1653, S-7220)")
{
	const auto &kw = RotwkBlockKeywords();
	CHECK(containsKeyword(kw, "WaterSet"));
	CHECK(containsKeyword(kw, "WaterTextureList"));
	CHECK(containsKeyword(kw, "SkyboxTextureSet"));

	// S-7220: WaterTextureList parser at RW 0x60047C, store at RW 0xDE8790 (.bss, always NULL in retail).
	// S-1653: SkyboxTextureSet parser at RW 0x60D3E9, store at RW 0xDE4EDC (no runtime reader).
	initest::Fixture fx;
	INIBlockRecorder rec;
	RegisterRecordingBlockStubs(fx.env.blocks, rec, {});

	const char *kUnusedBlocksIni =
		"WaterTextureList TestList\n"
		"  Texture = test_tex.tga\n"
		"End\n"
		"SkyboxTextureSet TestSky\n"
		"  Texture = test_skybox.tga\n"
		"End\n";

	REQUIRE(initest::loadError(fx.env, "test_unused.ini", kUnusedBlocksIni).empty());
	CHECK(rec.countByKeyword["WaterTextureList"] == 1);
	CHECK(rec.countByKeyword["SkyboxTextureSet"] == 1);
}
