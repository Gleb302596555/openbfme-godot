// OpenBFME unit tests. GPL-3.0.
// The terrain's taint mask (W3DTaintMask.h, stop S-5261, Local 67 / issue #512): the texel of a cell (RW 0x4735AB, both the shader path mode > 0 and
// the Default_L / fixed-function path mode <= 0), the fire colour at the cell's centre (TheFireManager slot 0x3C RW 0x497F60), the texture with its
// border (RW 0x47321C), the 16x16-cell W3DTerrainBackground taint UV (RW 0x514398 at RW 0x514628 .. 0x51465F, m_width = 16 from RW 0x4E14A1), and the
// Ennorath goblins' SpellBookTaintGundabad end-to-end taint ground chain. Expectations from the RW bodies and the Fire block's constructor (RW 0x70C637).

#include "doctest.h"
#include "LogicTestUtil.h"
#include "RetailTestMount.h"

#include "Common/GameEdition.h"
#include "Common/INI.h"
#include "Common/ModMount.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/RetailArchivePolicy.h"
#include "Common/Science.h"
#include "Common/SpecialPower.h"
#include "Common/Team.h"
#include "Common/Thing/ThingTemplate.h"
#include "GameClient/ControlBarCommands.h"
#include "GameClient/FireSetting.h"
#include "GameClient/Radar.h"
#include "GameEngineDevice/W3DDevice/GameClient/W3DTaintMask.h"
#include "GameEngineDevice/Win32Device/Common/Win32BIGFileSystem.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/LogicModules.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/Module/SpellBookPowers.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/SpellStores.h"
#include "GameLogic/System/TaintManager.h"
#include "GameLogic/System/VisionSettings.h"
#include "Libraries/WWVegas/WW3D2/assetmgr.h"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <string>
#include <vector>

TEST_CASE("taint mask: the fire colour of a cell type, the texel and the cell centre (RW 0x497F60, 0x4735AB)")
{
	const FireSetting fire; // the constructor's colours: BurntTerrainColor 0.2 each, FuelIndicatorColor 0.8 / 0.7 / 0.6
	// type 1 (burning) -> FuelIndicatorColor (+ 0x20), 2 (burnt out) -> BurntTerrainColor (+ 0x14), else white
	CHECK(W3DTaintMask::fireColour(fire, 0).red == 1.0f);
	CHECK(W3DTaintMask::fireColour(fire, 1).green == 0.7f);
	CHECK(W3DTaintMask::fireColour(fire, 2).blue == 0.2f);
	CHECK(W3DTaintMask::fireColour(fire, 3).blue == 1.0f);
	// value << 24 | channels * 255 truncated: 0.8 -> 204, 0.7 -> 178 (178.49999 truncates), 0.6 -> 153, 0.2 -> 51
	CHECK(W3DTaintMask::texel(0x80, W3DTaintMask::fireColour(fire, 0)) == 0x80FFFFFFu);
	CHECK(W3DTaintMask::texel(0x00, W3DTaintMask::fireColour(fire, 1)) == 0x00CCB299u);
	CHECK(W3DTaintMask::texel(0xFF, W3DTaintMask::fireColour(fire, 2)) == 0xFF333333u);
	// trunc((n + 0.5) * cellSize)
	CHECK(W3DTaintMask::cellCentre(0, 10.0f) == 5);
	CHECK(W3DTaintMask::cellCentre(3, 10.0f) == 35);
	CHECK(W3DTaintMask::cellCentre(1, 7.5f) == 11);
}

TEST_CASE("taint mask: the texture is the cells plus a one-texel border at TaintAlpha in every byte (RW 0x47321C, 0x473E83)")
{
	const FireSetting fire;
	TaintView view;
	view.countX = 2;
	view.countY = 1;
	view.values = { 0x00, 0xFF };
	view.fireTypes = { 0, 2 };
	std::vector<std::uint32_t> texels;
	int w = 0, h = 0;
	W3DTaintMask::build(view, fire, 0x80, texels, w, h);
	REQUIRE(w == 4);
	REQUIRE(h == 3);
	REQUIRE(texels.size() == 12);
	CHECK(texels[0] == 0x80808080u);
	CHECK(texels[11] == 0x80808080u);
	CHECK(texels[4] == 0x80808080u); // row 1, column 0: the border
	CHECK(texels[5] == 0x00FFFFFFu); // cell (0, 0) at texel (1, 1)
	CHECK(texels[6] == 0xFF333333u); // cell (1, 0), burnt out
	CHECK(texels[7] == 0x80808080u);
	// Ennorath's TaintAlpha 0
	W3DTaintMask::build(view, fire, 0, texels, w, h);
	CHECK(texels[0] == 0u);
}

TEST_CASE("taint mask: TheTaintManager's version moves with every cell write and region change, not with a read")
{
	TaintManager tm;
	TaintManager::Region r;
	r.hiX = 100.0f;
	r.hiY = 100.0f;
	const std::uint64_t v0 = tm.version();
	tm.setRegion(r, 10.0f);
	const std::uint64_t v1 = tm.version();
	CHECK(v1 != v0);
	Coord3D at{ 50.0f, 50.0f, 0.0f };
	CHECK(tm.valueAt(at) == 0x80);
	CHECK(tm.version() == v1);
	tm.paint(at, 10.0f, 0x00, true, -1);
	CHECK(tm.version() != v1);
}

TEST_CASE("taint mask: the radar's taint texel and image (W3DRadar vslot 0x2C, RW 0x44E5F3)")
{
	const float taint[3] = { 120.0f / 255.0f, 90.0f / 255.0f, 80.0f / 255.0f }; // RotWK's gamedata.ini TaintColor
	const float elven[3] = { 70.0f / 255.0f, 120.0f / 255.0f, 50.0f / 255.0f }; // ElvenWoodColor
	CHECK(Radar::taintTexel(0x80, taint, elven) == 0x00FFFFFFu);
	// value 0: k = 254, the alpha 0xFE, (int)(120 / 255 * 254) = 119, 89, 79
	CHECK(Radar::taintTexel(0x00, taint, elven) == 0xFE77594Fu);
	// value 0x7F: k = 0, black and clear
	CHECK(Radar::taintTexel(0x7F, taint, elven) == 0x00000000u);
	// value 0xFF: k = 254 with ElvenWoodColor: 69, 119, 49
	CHECK(Radar::taintTexel(0xFF, taint, elven) == 0xFE457731u);
	// value 0x81: k = 2
	CHECK(Radar::taintTexel(0x81, taint, elven) == 0x02000000u);
	// the image: a radar without terrain covers 0 .. 1; one cell of size 1 spans texels 0 .. 127 (clamped) both ways
	Radar radar;
	TaintView view;
	view.countX = view.countY = 1;
	view.cellSize = 1.0f;
	view.values = { 0x00 };
	view.fireTypes = { 0 };
	std::vector<std::uint32_t> texels;
	radar.taintTexels(view, taint, elven, texels);
	REQUIRE(texels.size() == (size_t)Radar::kCells * (size_t)Radar::kCells);
	CHECK(texels.front() == 0xFE77594Fu);
	CHECK(texels.back() == 0xFE77594Fu);
}

TEST_CASE("LOCAL-67: W3DTerrainBackground 16x16-cell taint UV, Default_L lowTexel pattern and terrain_common.gdshaderinc sampling (RW 0x514398, 0x4735AB)")
{
	// 1. RW 0x4E14A1 + RW 0x514628 .. 0x51465F: W3DTerrainBackground tile width is 16 cells (160 world units at MAP_XY_FACTOR = 10),
	//    u1 = (cellX - tileX * 16) / 16, v1 = 1 - (cellY - tileY * 16) / 16; under TaintLowSampler / TaintHighSampler WRAP,
	//    terrainUv((cellX - borderSize) * 10, (cellY - borderSize) * 10, borderSize * 10, 10) wraps to (u1, v1) across every tile.
	CHECK(W3DTaintMask::kBackgroundTileCells == 16);
	const int borderCells = 10;
	const float cellSize = 10.0f;
	const float borderWidth = (float)borderCells * cellSize;
	auto wrap01 = [](float v) { return v - std::floor(v); };
	for (int tileY = 0; tileY < 3; ++tileY)
	{
		for (int tileX = 0; tileX < 3; ++tileX)
		{
			const int minX = tileX * W3DTaintMask::kBackgroundTileCells;
			const int minY = tileY * W3DTaintMask::kBackgroundTileCells;
			for (int dy = 0; dy < W3DTaintMask::kBackgroundTileCells; ++dy)
			{
				for (int dx = 0; dx < W3DTaintMask::kBackgroundTileCells; ++dx)
				{
					const int i = minX + dx;
					const int j = minY + dy;
					float uTile = 0.0f, vTile = 0.0f;
					W3DTaintMask::tileVertexUv(i, j, minX, minY, uTile, vTile);
					CHECK(uTile == ((float)dx / 16.0f));
					CHECK(vTile == (1.0f - (float)dy / 16.0f));

					const float worldX = (float)(i - borderCells) * cellSize;
					const float worldY = (float)(j - borderCells) * cellSize;
					float uWorld = 0.0f, vWorld = 0.0f;
					W3DTaintMask::terrainUv(worldX, worldY, borderWidth, cellSize, uWorld, vWorld);
					CHECK(wrap01(uWorld) == (wrap01(uTile)));
					CHECK(wrap01(vWorld) == (wrap01(vTile)));
				}
			}
		}
	}

	// 2. RW 0x473659 .. 0x4739A4: the Default_L / fixed-function path (mode <= 0) writes TaintColor / ElvenWoodColor with the per-cell
	//    modulo 5 / 3 / 2 pattern into the mask's RGB channels, multiplied by TheFireManager's colour and shroudLevel.
	const RGBColor taintCol{ 120.0f / 255.0f, 90.0f / 255.0f, 80.0f / 255.0f };
	const RGBColor elvenCol{ 70.0f / 255.0f, 120.0f / 255.0f, 50.0f / 255.0f };
	const FireSetting fire;
	// Neutral 0x80: white (0x80FFFFFF), or burnt fire colour when on burnt ground (0x80333333)
	CHECK(W3DTaintMask::lowTexel(0, 0, 0x80, taintCol, elvenCol) == 0x80FFFFFFu);
	CHECK(W3DTaintMask::lowTexel(0, 0, 0x80, taintCol, elvenCol, W3DTaintMask::fireColour(fire, 2)) == 0x80333333u);
	// Evil taint 0x00 (t = 128 * 0.0078125 = 1.0):
	//   (0, 0): dr = +0.03 (120/255 + 0.03 -> 127 = 0x7F), dg = -0.02 (90/255 - 0.02 -> 84 = 0x54), db = -0.03 (80/255 - 0.03 -> 72 = 0x48)
	//   (1, 1): dr = -0.02 (120/255 - 0.02 -> 114 = 0x72), dg = +0.03 (90/255 + 0.03 -> 97 = 0x61), db = +0.02 (80/255 + 0.02 -> 85 = 0x55)
	//   (10, 10): x % 5 == 0 && y % 2 == 0 -> dr = +0.03 (0x7F); x % 3 != 0 -> dg = +0.03 (0x61); x % 2 == 0 && y % 3 != 0 -> db = +0.02 (0x55)
	CHECK(W3DTaintMask::lowTexel(0, 0, 0x00, taintCol, elvenCol) == 0x007F5448u);
	CHECK(W3DTaintMask::lowTexel(1, 1, 0x00, taintCol, elvenCol) == 0x00726155u);
	CHECK(W3DTaintMask::lowTexel(10, 10, 0x00, taintCol, elvenCol) == 0x007F6155u);
	// Shroud factor 127 (FogAlpha) at (0, 0): (int)((120/255 + 0.03) * 127) = 63 (0x3F), 42 (0x2A), 36 (0x24)
	CHECK(W3DTaintMask::lowTexel(0, 0, 0x00, taintCol, elvenCol, RGBColor{ 1.0f, 1.0f, 1.0f }, 127) == 0x003F2A24u);
	// Elven wood 0xFF (t = 127 * 0.0078740157 = 1.0):
	//   (0, 0): R = (int)((70/255 + 0.03) * 255) = 77 (0x4D), G = (int)((120/255 - 0.02) * 255) = 114 (0x72), B = (int)((50/255 - 0.03) * 255) = 42 (0x2A)
	//   (1, 1): R = (int)((70/255 - 0.02) * 255) = 64 (0x40), G = (int)((120/255 + 0.03) * 255) = 127 (0x7F), B = (int)((50/255 + 0.02) * 255) = 55 (0x37)
	CHECK(W3DTaintMask::lowTexel(0, 0, 0xFF, taintCol, elvenCol) == 0xFF4D722Au);
	CHECK(W3DTaintMask::lowTexel(1, 1, 0xFF, taintCol, elvenCol) == 0xFF407F37u);

	// 3. godot/shaders/terrain_common.gdshaderinc must sample taint_low_tex / taint_high_tex at the 16x16-cell W3DTerrainBackground UV
	//    (and cliff class UV on cliff cells), never at the 2048x2048 tile atlas uv_unwrapped.
	const std::string shaderPath = std::string(OPENBFME_DOCS_DIR) + "/../godot/shaders/terrain_common.gdshaderinc";
	std::ifstream file(shaderPath, std::ios::binary);
	REQUIRE_MESSAGE(file.good(), shaderPath);
	const std::string shader((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	CHECK(shader.find("textureGrad(taint_low_tex, uv_unwrapped") == std::string::npos);
	CHECK(shader.find("textureGrad(taint_high_tex, uv_unwrapped") == std::string::npos);
	CHECK(shader.find("16.0 * map_cell_size") != std::string::npos);
	CHECK(shader.find("textureGrad(taint_low_tex, taint_uv") != std::string::npos);
	CHECK(shader.find("textureGrad(taint_high_tex, taint_uv") != std::string::npos);
}

namespace
{
std::string readArchiveText(ArchiveFileSystem &fs, const char *path)
{
	std::vector<std::uint8_t> bytes;
	std::string err;
	REQUIRE_MESSAGE(fs.readFile(path, bytes, &err), path << ": " << err);
	return std::string(reinterpret_cast<const char *>(bytes.data()), bytes.size());
}

std::string extractIniBlock(const std::string &text, const std::string &keyword, const std::string &name)
{
	size_t pos = std::string::npos;
	size_t searchFrom = 0;
	while (searchFrom < text.size())
	{
		const size_t hit = text.find(name, searchFrom);
		if (hit == std::string::npos) break;
		const size_t lineStart = (hit == 0) ? 0 : text.rfind('\n', hit);
		const size_t s = (lineStart == std::string::npos) ? 0 : lineStart + 1;
		size_t k = s;
		while (k < hit && (text[k] == ' ' || text[k] == '\t')) ++k;
		const size_t semi = text.find(';', s);
		if (text.compare(k, keyword.size(), keyword) == 0 && k + keyword.size() < hit && (semi == std::string::npos || semi > hit))
		{
			const size_t after = hit + name.size();
			if (after >= text.size() || text[after] == '\r' || text[after] == '\n' || text[after] == ' ' || text[after] == '\t' || text[after] == ';')
			{
				pos = k;
				break;
			}
		}
		searchFrom = hit + name.size();
	}
	REQUIRE_MESSAGE(pos != std::string::npos, keyword << " " << name);
	const size_t lineStart = (pos == 0) ? 0 : text.rfind('\n', pos);
	const size_t start = (lineStart == std::string::npos) ? 0 : lineStart + 1;
	size_t cur = pos;
	while (cur < text.size())
	{
		const size_t eol = text.find('\n', cur);
		const std::string line = text.substr(cur, eol == std::string::npos ? std::string::npos : eol - cur);
		size_t a = 0;
		while (a < line.size() && (line[a] == ' ' || line[a] == '\t')) ++a;
		if (line.compare(a, 3, "End") == 0 && (a + 3 == line.size() || line[a + 3] == '\r' || line[a + 3] == ' ' || line[a + 3] == '\t' || line[a + 3] == ';'))
		{
			return text.substr(start, (eol == std::string::npos ? text.size() : eol + 1) - start);
		}
		if (eol == std::string::npos) break;
		cur = eol + 1;
	}
	FAIL("unterminated INI block: " << keyword << " " << name);
	return std::string();
}
} // namespace

TEST_CASE("LOCAL-67: Ennorath Goblins SpellBookTaintGundabad cast paints the taint grid, builds both RW mask paths and maps TSTaintMed.tga over 16x16 terrain cells (P-126, RW 0x8C8E67, 0xAD4C10, 0x4735AB, 0x514398)")
{
	auto env = [](const char *name) -> std::string {
		const char *v = std::getenv(name);
		return v ? v : "";
	};
	const std::string rotwk = env("ROTWK_INSTALL");
	const std::string bfme2 = env("BFME2_INSTALL");
	const std::string mod = env("OPENBFME_ENNORATH_INSTALL");
	if (rotwk.empty() || bfme2.empty() || mod.empty())
	{
		retailtest::printSkip("LOCAL-67 Ennorath Goblins Taint (set ROTWK_INSTALL, BFME2_INSTALL, OPENBFME_ENNORATH_INSTALL)");
		return;
	}

	std::string err;
	Win32BIGFileSystem pure;
	std::vector<RetailInstall> installs(2);
	installs[0].label = "rotwk";
	installs[0].root = rotwk;
	installs[1].label = "bfme2";
	installs[1].root = bfme2;
	REQUIRE_MESSAGE(RetailArchivePolicy::loadBuiltin("rotwk-201", installs[0].policy, &err), err);
	REQUIRE_MESSAGE(RetailArchivePolicy::loadBuiltin("bfme2-106", installs[1].policy, &err), err);
	RetailMountOptions options;
	options.md5CachePath = (std::filesystem::temp_directory_path() / "openbfme-ennorath-scenario-md5.tsv").u8string();
	const RetailMountReport retail = MountRetailArchives(pure, installs, options);
	REQUIRE_MESSAGE(retail.ok, "retail mount failed");

	Win32BIGFileSystem fs;
	ModMountOptions modOptions;
	modOptions.rotwkInstallRoot = rotwk;
	modOptions.md5CachePath = options.md5CachePath;
	const ModMountReport mounted = MountInstallOverlay(fs, retail, mod, modOptions);
	REQUIRE_MESSAGE(mounted.ok, "Ennorath overlay mount failed");

	// 1. Ennorath's gamedata.ini taint settings (data\ini\gamedata.ini:573..576):
	//    TaintOn = Yes, TaintColor = R:120 G:90 B:80, TaintAlpha = 0, ElvenWoodColor = R:70 G:120 B:50, PartitionCellSize = 40.0
	VisionSettings vs;
	REQUIRE_MESSAGE(VisionSettings::load(fs, vs, &err), err);
	CHECK(vs.taintOn == true);
	CHECK(vs.taintAlpha == 0u);
	CHECK(vs.partitionCellSize == 40.0f);
	CHECK(std::lround(vs.taintColor[0] * 255.0f) == 120);
	CHECK(std::lround(vs.taintColor[1] * 255.0f) == 90);
	CHECK(std::lround(vs.taintColor[2] * 255.0f) == 80);
	CHECK(std::lround(vs.elvenWoodColor[0] * 255.0f) == 70);
	CHECK(std::lround(vs.elvenWoodColor[1] * 255.0f) == 120);
	CHECK(std::lround(vs.elvenWoodColor[2] * 255.0f) == 50);
	const RGBColor taintCol{ vs.taintColor[0], vs.taintColor[1], vs.taintColor[2] };
	const RGBColor elvenCol{ vs.elvenWoodColor[0], vs.elvenWoodColor[1], vs.elvenWoodColor[2] };

	// 2. Parse Ennorath's Goblins spellbook Taint chain from the mounted Ennorath archive:
	//    - data\ini\science.ini: SCIENCE_TaintGundabad (SciencePurchasePointCost = 9)
	//    - data\ini\specialpower.ini: SpellBookTaintGundabad (ReloadTime = 360000, RequiredSciences = SCIENCE_TaintGundabad)
	//    - data\ini\commandbutton.ini: Command_PurchaseSpellTaintGundabad, Command_SpellBookTaintGundabad
	//    - data\ini\includes\commandset.inc: WildSpellStoreCommandSet (slot 7 = index 6), WildSpellBookCommandSet (slot 9 = index 8)
	//    - data\ini\object\system\includes\systemevil.inc: TaintSpecialPower ModuleTag_TaintGundabad
	//    - data\ini\object\evilfaction\structures\taintland.ini: Object TaintLand
	logictest::LogicWorld lw;
	REQUIRE_MESSAGE(lw.loadError.empty(), lw.loadError);
	SpellStores stores(lw.w.keys);
	stores.install();
	SpellStores::registerBlocks(lw.w.fx.env.blocks);
	LogicModules::registerAll(lw.w.modules);
	CommandStore commands;
	commands.setThingFactory(&lw.w.things);
	CommandStore *prevCmd = TheCommandStore;
	TheCommandStore = &commands;
	struct CmdScope
	{
		CommandStore *prev;
		~CmdScope() { TheCommandStore = prev; }
	} cmdScope{ prevCmd };
	lw.w.fx.env.blocks.registerBlock("CommandButton", CommandStore::parseCommandButtonDefinitionGlobal);
	lw.w.fx.env.blocks.registerBlock("CommandSet", CommandStore::parseCommandSetDefinitionGlobal);

	const std::string scienceIni = readArchiveText(fs, "data\\ini\\science.ini");
	const std::string spIni = readArchiveText(fs, "data\\ini\\includes\\specialpower.inc");
	const std::string cbIni = readArchiveText(fs, "data\\ini\\includes\\commandbutton.inc");
	const std::string sysEvil = readArchiveText(fs, "data\\ini\\object\\system\\includes\\systemevil.inc");

	std::string slice;
	slice += "#define SPELLBOOK_TAINT_RADIUS 175\n#define SPELL_RECHARGE_TIME_TIER_2 360000\n#define SPELL_BOOK_REQUIREMENTS_FILTER NONE +CommandCenter\n#define SPELL_BOOK_REQUIREMENTS_FILTER_STRATEGIC NONE +CommandCenter\n";
	slice += "#define SPELLBOOK_TAINT_TIME 300000\n";
	slice += readArchiveText(fs, "data\\ini\\default\\science.ini") + "\n";
	slice += scienceIni + "\n";
	slice += extractIniBlock(spIni, "SpecialPower", "SpellBookTaintGundabad");
	slice += extractIniBlock(cbIni, "CommandButton", "Command_PurchaseSpellTaintGundabad");
	slice += extractIniBlock(cbIni, "CommandButton", "Command_SpellBookTaintGundabad");
	// TaintLand object and WildSpellBook's ModuleTag_TaintGundabad from Ennorath's systemevil.inc / taintland.ini:
	slice += "Object TaintLand\n"
		"  KindOf = PRELOAD UNATTACKABLE IMMOBILE TAINT\n"
		"  Behavior = DeletionUpdate ModuleTag_LifeTimeUpdate\n"
		"    MinLifetime = SPELLBOOK_TAINT_TIME\n"
		"    MaxLifetime = SPELLBOOK_TAINT_TIME\n"
		"  End\n"
		"End\n";
	slice += "Object WildSpellBook\n"
		"  KindOf = NO_COLLIDE IGNORED_IN_GUI\n"
		+ extractIniBlock(sysEvil, "Behavior", "ModuleTag_TaintGundabad")
		+ "End\n";
	err = lw.w.load(slice, INI_LOAD_OVERWRITE, "local67_ennorath.ini");
	REQUIRE_MESSAGE(err.empty(), err);

	// Verify WildSpellStoreCommandSet slot 7 and WildSpellBookCommandSet slot 9 in Ennorath's commandset.inc
	const std::string csInc = readArchiveText(fs, "data\\ini\\includes\\commandset.inc");
	const std::string wildStoreBlock = extractIniBlock(csInc, "CommandSet", "WildSpellStoreCommandSet");
	const std::string wildBookBlock = extractIniBlock(csInc, "CommandSet", "WildSpellBookCommandSet");
	CHECK(wildStoreBlock.find("7 = Command_PurchaseSpellTaintGundabad") != std::string::npos);
	CHECK(wildBookBlock.find("9 = Command_SpellBookTaintGundabad") != std::string::npos);

	const ScienceType taintSci = stores.sciences().getScienceFromInternalName("SCIENCE_TaintGundabad");
	REQUIRE(taintSci != SCIENCE_INVALID);
	const ScienceInfo *sci = stores.sciences().findScienceInfo(taintSci);
	REQUIRE(sci != nullptr);
	CHECK(sci->m_sciencePurchasePointCost == 10);
	CHECK(sci->m_sciencePurchasePointCostMP == 9);

	const SpecialPowerTemplate *spTmpl = stores.specialPowers().findSpecialPowerTemplate("SpellBookTaintGundabad");
	REQUIRE(spTmpl != nullptr);
	CHECK(spTmpl->getReloadTime() == 1800u); // 360000 ms at 5 logic frames/s (RW 0x73A429)
	REQUIRE(!spTmpl->getRequiredSciences().empty());
	CHECK(spTmpl->getRequiredSciences().front() == taintSci);

	const CommandButton *purchaseBtn = commands.findCommandButton("Command_PurchaseSpellTaintGundabad");
	REQUIRE(purchaseBtn != nullptr);
	REQUIRE(!purchaseBtn->m_science.empty());
	CHECK(purchaseBtn->m_science.front() == "SCIENCE_TaintGundabad");

	const CommandButton *castBtn = commands.findCommandButton("Command_SpellBookTaintGundabad");
	REQUIRE(castBtn != nullptr);
	CHECK(castBtn->m_specialPowerName == "SpellBookTaintGundabad");
	CHECK(castBtn->hasOption(COMMAND_OPTION_NEED_TARGET_POS));

	const ThingTemplate *taintLandTmpl = lw.w.get("TaintLand");
	REQUIRE(taintLandTmpl != nullptr);

	// 3. Cast SpellBookTaintGundabad on a 1600x1600 map in GameLogic (RW 0x8C8FF5 -> RW 0x8C8E67)
	TaintManager::Region mapRegion;
	mapRegion.loX = 0.0f;
	mapRegion.loY = 0.0f;
	mapRegion.hiX = 1600.0f;
	mapRegion.hiY = 1600.0f;
	lw.logic->taint().setRegion(mapRegion, vs.partitionCellSize); // RW 0x62CFE3 .. 0x62CFFC, 0x62FCE2: 40x40 cells of 40.0 world units
	REQUIRE(lw.logic->taint().countX() == 40);
	REQUIRE(lw.logic->taint().countY() == 40);

	Object *book = lw.make("WildSpellBook", lw.teamOf("Alice"));
	REQUIRE(book != nullptr);
	auto *taintPower = dynamic_cast<TaintSpecialPower *>(SpecialPowerModules::findModule(*book, spTmpl));
	REQUIRE(taintPower != nullptr);
	REQUIRE(taintPower->data() != nullptr);
	CHECK(taintPower->data()->m_taintObject == "TaintLand");
	CHECK(taintPower->data()->m_taintRadius == 175.0f);
	CHECK(taintPower->data()->m_taintFX == "FX_TaintSpellFX");
	CHECK(taintPower->data()->m_taintOCL == "OCL_TaintSeed");

	const Coord3D castLoc{ 400.0f, 400.0f, 0.0f };
	taintPower->doSpecialPowerAtLocation(castLoc, 0u);

	// TaintLand is created at castLoc on the Goblins player's team (RW 0x8C8DF9: setPosition RW 0x70C201, setTeam RW 0x69954A)
	const ObjectID taintLandId = taintPower->lastAreaObject();
	REQUIRE(taintLandId != INVALID_ID);
	Object *taintLand = lw.logic->findObjectByID(taintLandId);
	REQUIRE(taintLand != nullptr);
	CHECK(taintLand->getTemplate() == taintLandTmpl);
	CHECK(taintLand->isKindOfName("TAINT"));
	CHECK(taintLand->getTeam() == lw.teamOf("Alice"));
	CHECK(taintLand->getPosition()->x == 400.0f);
	CHECK(taintLand->getPosition()->y == 400.0f);

	// TheTaintManager (RW 0xAD4C10): cx = floor(400 / 40) = 10, cy = 10, r = ceil(175 / 40) = 5:
	//   cells inside the midpoint circle of radius 5 have value 0x00, STATE_TAINT (2), owner == taintLandId;
	//   cells outside have value 0x80, STATE_NONE (0), owner == -1.
	const TaintManager &tm = lw.logic->taint();
	CHECK(tm.anyPainted() == true);
	std::int32_t ownerAtCenter = -1;
	CHECK(tm.stateAt(castLoc, &ownerAtCenter) == TaintManager::STATE_TAINT);
	CHECK(ownerAtCenter == (std::int32_t)taintLandId);
	CHECK(tm.cell(10, 10).value == 0x00);
	CHECK(tm.cell(10, 10).state == TaintManager::STATE_TAINT);
	CHECK(tm.cell(10, 10).owner == (std::int32_t)taintLandId);
	CHECK(tm.cell(15, 10).value == 0x00); // cx + r = 15 on the cardinal axis
	CHECK(tm.cell(5, 10).value == 0x00);  // cx - r = 5
	CHECK(tm.cell(10, 15).value == 0x00); // cy + r = 15
	CHECK(tm.cell(10, 5).value == 0x00);  // cy - r = 5
	CHECK(tm.cell(16, 10).value == TaintManager::NEUTRAL); // outside r = 5 (painted 0x80 by the 1.25 * 175 outer ring)
	CHECK(tm.cell(16, 10).state == TaintManager::STATE_NONE);
	CHECK(tm.cell(0, 0).value == TaintManager::NEUTRAL);

	// 4. Both W3DTaint::setTaintLevel paths (RW 0x4735AB):
	//    - Shader path (mode > 0, RW 0x4738F3 .. 0x4739A4): W3DTaintMask::build produces a 42x42 mask with border 0x00000000 (TaintAlpha = 0),
	//      0x00FFFFFF at tainted cell (10, 10) -> texel (11, 11), and 0x80FFFFFF at neutral cell (0, 0) -> texel (1, 1).
	//    - Default_L / fixed-function path (mode <= 0, RW 0x473659 .. 0x4738F1): W3DTaintMask::lowTexel writes Ennorath's TaintColor
	//      (120, 90, 80) + per-cell pattern directly into the mask RGB.
	const FireSetting fire;
	TaintView view;
	view.countX = tm.countX();
	view.countY = tm.countY();
	view.originX = tm.region().loX;
	view.originY = tm.region().loY;
	view.cellSize = tm.cellSize();
	view.values.resize((size_t)view.countX * (size_t)view.countY);
	view.fireTypes.assign((size_t)view.countX * (size_t)view.countY, 0);
	for (int y = 0; y < view.countY; ++y)
	{
		for (int x = 0; x < view.countX; ++x)
		{
			view.values[(size_t)y * (size_t)view.countX + (size_t)x] = tm.cell(x, y).value;
		}
	}
	std::vector<std::uint32_t> maskTexels;
	int maskW = 0, maskH = 0;
	W3DTaintMask::build(view, fire, (std::uint8_t)vs.taintAlpha, maskTexels, maskW, maskH);
	REQUIRE(maskW == 42);
	REQUIRE(maskH == 42);
	CHECK(maskTexels[0] == 0x00000000u);                     // 1-texel border at TaintAlpha = 0
	CHECK(maskTexels[1 * 42 + 1] == 0x80FFFFFFu);            // untouched cell (0, 0): alpha 0x80 -> d = 2|0.5 - 0.5| = 0 (unmodified ground)
	CHECK(maskTexels[(10 + 1) * 42 + (10 + 1)] == 0x00FFFFFFu); // tainted cell (10, 10): alpha 0x00 -> d = 1.0 (100% TSTaintMed.tga)
	CHECK(W3DTaintMask::lowTexel(10, 10, tm.cell(10, 10).value, taintCol, elvenCol) == 0x007F6155u);
	CHECK(W3DTaintMask::lowTexel(11, 11, tm.cell(11, 11).value, taintCol, elvenCol) == 0x00726155u);
	CHECK(W3DTaintMask::lowTexel(0, 0, tm.cell(0, 0).value, taintCol, elvenCol) == 0x80FFFFFFu);

	// 5. Textures TSTaintMed.tga (RW 0x46C5DB -> art\compiledtextures\ts\tstaintmed.dds, 512x512 DXT1) and TSElvenMed.tga
	//    (RW 0x46C5FC -> art\compiledtextures\ts\tselvenmed.dds, 512x512 DXT1) in the archive, mapped over 16x16 terrain cells (RW 0x514628 .. 0x51465F):
	const std::string taintDdsPath = W3D_Compiled_Texture_Path("TSTaintMed.tga");
	const std::string elvenDdsPath = W3D_Compiled_Texture_Path("TSElvenMed.tga");
	CHECK(taintDdsPath == "art\\compiledtextures\\ts\\tstaintmed.dds");
	CHECK(elvenDdsPath == "art\\compiledtextures\\ts\\tselvenmed.dds");
	std::vector<std::uint8_t> taintDds, elvenDds;
	REQUIRE_MESSAGE(fs.readFile(taintDdsPath, taintDds, &err), err);
	REQUIRE_MESSAGE(fs.readFile(elvenDdsPath, elvenDds, &err), err);
	REQUIRE(taintDds.size() == 174904u); // 512x512 DXT1 with 10 mip levels
	REQUIRE(elvenDds.size() == 174904u);
	CHECK(std::memcmp(taintDds.data(), "DDS ", 4) == 0);
	std::uint32_t ddsH = 0, ddsW = 0;
	std::memcpy(&ddsH, taintDds.data() + 12, 4);
	std::memcpy(&ddsW, taintDds.data() + 16, 4);
	CHECK(ddsW == 512u);
	CHECK(ddsH == 512u);

	// Across the 16x16 terrain cells (320 .. 480 world units, inside the radius-175 taint circle around (400, 400)),
	// W3DTaintMask::terrainUv addresses all 16x16 = 256 distinct 32x32-texel sub-tiles (8x8 DXT1 blocks each) of the 512x512 TSTaintMed.dds texture.
	std::set<int> addressedBlocks;
	std::set<std::uint64_t> blockPayloads;
	for (int cy = 0; cy < W3DTaintMask::kBackgroundTileCells; ++cy)
	{
		for (int cx = 0; cx < W3DTaintMask::kBackgroundTileCells; ++cx)
		{
			const float wx = 320.0f + ((float)cx + 0.5f) * 10.0f;
			const float wy = 320.0f + ((float)cy + 0.5f) * 10.0f;
			float u = 0.0f, v = 0.0f;
			W3DTaintMask::terrainUv(wx, wy, 0.0f, 10.0f, u, v);
			const float uw = u - std::floor(u);
			const float vw = v - std::floor(v);
			const int texX = (int)std::floor(uw * 512.0f);
			const int texY = (int)std::floor(vw * 512.0f);
			CHECK(texX == cx * 32 + 16);
			CHECK(texY == (15 - cy) * 32 + 16);
			const int dxtBlockX = texX / 4;
			const int dxtBlockY = texY / 4;
			addressedBlocks.insert(dxtBlockY * 128 + dxtBlockX);
			std::uint64_t blockWord = 0;
			std::memcpy(&blockWord, taintDds.data() + 128 + (size_t)(dxtBlockY * 128 + dxtBlockX) * 8, 8);
			blockPayloads.insert(blockWord);
		}
	}
	CHECK(addressedBlocks.size() == 256);
	CHECK(blockPayloads.size() > 64); // tolerance: 256 samples across a natural 512x512 DXT1 texture span dozens of distinct DXT1 blocks (the old 2x2-cell atlas UV repeated at most 4 samples)
}
