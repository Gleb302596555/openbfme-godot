// OpenBFME unit tests: lane SDK-1, the modding SDK's asset packs made cosmetic-only and checked (GameClient/AssetPacks.h). GPL-3.0.
// Every pack, model and archive here is written by the test (no retail bytes).

#include "doctest.h"
#include "BigTestUtil.h"
#include "RetailTestMount.h"
#include "W3dSynth.h"

#include "Common/ArchiveFile.h"
#include "Common/AsciiString.h"
#include "GameClient/AssetPacks.h"
#include "GameEngineDevice/Win32Device/Common/Win32BIGFileSystem.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <random>
#include <tuple>

using namespace bigtest;
using namespace w3dsynth;
namespace fs = std::filesystem;

namespace
{

struct TempDir
{
	fs::path path;
	TempDir()
	{
		std::mt19937_64 rng((unsigned long long)std::chrono::steady_clock::now().time_since_epoch().count());
		path = fs::temp_directory_path() / ("openbfme-sdk1-" + std::to_string(rng()));
		fs::create_directories(path);
	}
	~TempDir()
	{
		std::error_code ec;
		fs::remove_all(path, ec);
	}
};

void writeFile(const fs::path &p, const std::string &bytes)
{
	fs::create_directories(p.parent_path());
	std::ofstream out(p, std::ios::binary);
	out.write(bytes.data(), (std::streamsize)bytes.size());
}

std::string str(const std::vector<std::uint8_t> &v)
{
	return std::string(v.begin(), v.end());
}

std::string joined(const std::vector<std::string> &v)
{
	std::string out;
	for (const std::string &s : v)
	{
		out += s + "\n";
	}
	return out;
}

std::string readSource(W3DFileSource &src, const std::string &path)
{
	std::vector<std::uint8_t> out;
	std::string error;
	if (!src.Read(path, out, &error))
	{
		return "<" + error + ">";
	}
	return str(out);
}

// a made-up unit: its skeleton file (gumaarms_skl.w3d: ROOTTRANSFORM, B_SPINE, B_HEAD) and its model (gumaarms.w3d: HLOD GUMaArms on it)
std::vector<std::uint8_t> skeleton(const char *spine = "B_SPINE", std::uint32_t headParent = 1)
{
	return hierarchyChunk("GUMaArms_SKL", { pivot("ROOTTRANSFORM", 0xffffffffu, 0, 0, 0), pivot(spine, 0, 0, 0, 1), pivot("B_HEAD", headParent, 0, 0, 2) });
}

std::vector<std::uint8_t> model(const char *hierarchy = "GUMaArms_SKL", const char *mesh = "GUMaArms.BODY")
{
	return hlodChunk("GUMaArms", hierarchy, { { mesh, 1 } });
}

void mount(Win32BIGFileSystem &fsys, const char *name, const std::vector<std::pair<std::string, std::string>> &files, bool overwrite)
{
	std::string error;
	auto a = fsys.openArchiveFromMemory(name, std::make_shared<const std::vector<std::uint8_t>>(makeBig("BIGF", files)), &error);
	REQUIRE_MESSAGE(a, error);
	fsys.mountArchive(std::move(a), name, overwrite);
}

} // namespace

TEST_CASE("SDK-1: a pack holds models, textures and its own text only; any other file refuses the whole pack")
{
	TempDir tmp;
	writeFile(tmp.path / "Clean" / "README.md", "about");
	writeFile(tmp.path / "Clean" / "LICENSE", "cc-by");
	writeFile(tmp.path / "Clean" / "credits.txt", "me");
	writeFile(tmp.path / "Clean" / "gumaarms.dds", "DDS ");
	AssetPacks packs;
	std::vector<std::string> errors;
	REQUIRE_MESSAGE(packs.addPack((tmp.path / "Clean").u8string(), errors), joined(errors));
	CHECK(packs.notes().empty());

	for (const char *data : { "data/ini/weapon.ini", "!!HD.big", "data/scripts/scripts.scb", "maps/m/m.map", "audio/sound.wav", "x.lua", "d3d8.dll" })
	{
		CAPTURE(data);
		TempDir t;
		writeFile(t.path / "Sneaky" / "tree.dds", "DDS ");
		writeFile(t.path / "Sneaky" / data, "data");
		AssetPacks p;
		std::vector<std::string> e;
		CHECK_FALSE(p.addPack((t.path / "Sneaky").u8string(), e));
		REQUIRE(e.size() == 1);
		CHECK(e[0] == "asset pack Sneaky: " + std::string(data) + " is not a model or texture (.w3d, .dds, .tga, .png, .jpg): an asset pack only "
			"changes how the game looks; game data (INI, scripts, maps, archives, sounds, programs) belongs in a mod (-mod)");
		CHECK(p.empty()); // nothing of the pack, not even its texture
	}
}

TEST_CASE("SDK-1: a replacement model must keep the skeleton, models and animations of the file it replaces")
{
	Win32BIGFileSystem fsys;
	mount(fsys, "W3D.big", { { "art\\w3d\\gu\\gumaarms_skl.w3d", str(skeleton()) }, { "art\\w3d\\gu\\gumaarms.w3d", str(model()) } }, false);
	ArchiveW3DFileSource mounted(fsys);

	SUBCASE("an HD model on the same skeleton fits; a model replacing nothing is a note")
	{
		TempDir tmp;
		writeFile(tmp.path / "HD" / "gumaarms.w3d", str(model("gumaarms_skl", "GUMaArms.BODY_HD"))); // other meshes, same hierarchy (any case)
		writeFile(tmp.path / "HD" / "gumaarms_skl.w3d", str(skeleton())); // the same bones, new rest pose values would be fine too
		writeFile(tmp.path / "HD" / "gumystery.w3d", str(model()));
		AssetPacks packs;
		std::vector<std::string> errors;
		REQUIRE(packs.addPack((tmp.path / "HD").u8string(), errors));
		CHECK_MESSAGE(packs.checkModels(mounted, errors), joined(errors));
		CHECK(packs.notes() == std::vector<std::string>{ "HD: gumystery.w3d replaces no model of the game or the mod (art\\w3d\\gu\\gumystery.w3d): not used" });
	}
	SUBCASE("a renamed bone, a moved bone, another hierarchy or a dropped HLOD leaves that model out with a note; a broken file is an error")
	{
		const std::string tail = "): not used, the game's model is drawn (a replacement model keeps the original's skeleton, models and animations, "
								 "or the game's animations and effects would not fit it)";
		struct Case
		{
			const char *what;
			std::string bytes;
			std::string file;
			std::string why;
		};
		const std::vector<Case> cases = {
			{ "renamed bone", str(skeleton("B_TORSO")), "gumaarms_skl.w3d",
				"Bad: gumaarms_skl.w3d does not fit art\\w3d\\gu\\gumaarms_skl.w3d (its hierarchy GUMaArms_SKL: bone 1 is B_TORSO (parent 0), the "
				"original's B_SPINE (parent 0)" + tail },
			{ "moved bone", str(skeleton("B_SPINE", 0)), "gumaarms_skl.w3d",
				"Bad: gumaarms_skl.w3d does not fit art\\w3d\\gu\\gumaarms_skl.w3d (its hierarchy GUMaArms_SKL: bone 2 is B_HEAD (parent 0), the "
				"original's B_HEAD (parent 1)" + tail },
			{ "other hierarchy", str(model("GUOther_SKL")), "gumaarms.w3d",
				"Bad: gumaarms.w3d does not fit art\\w3d\\gu\\gumaarms.w3d (its model GUMaArms uses the hierarchy GUOther_SKL, the original's "
				"GUMaArms_SKL" + tail },
			{ "fewer bones", str(hierarchyChunk("GUMaArms_SKL", { pivot("ROOTTRANSFORM", 0xffffffffu, 0, 0, 0), pivot("B_SPINE", 0, 0, 0, 1) })),
				"gumaarms_skl.w3d",
				"Bad: gumaarms_skl.w3d does not fit art\\w3d\\gu\\gumaarms_skl.w3d (its hierarchy GUMaArms_SKL has 2 bones, the original's 3" + tail },
			{ "bones in another order", str(hierarchyChunk("GUMaArms_SKL", { pivot("ROOTTRANSFORM", 0xffffffffu, 0, 0, 0), pivot("B_HEAD", 0, 0, 0, 2),
											   pivot("B_SPINE", 0, 0, 0, 1) })),
				"gumaarms_skl.w3d",
				"Bad: gumaarms_skl.w3d does not fit art\\w3d\\gu\\gumaarms_skl.w3d (its hierarchy GUMaArms_SKL: bone 1 is B_HEAD (parent 0), the "
				"original's B_SPINE (parent 0)" + tail },
			{ "dropped HLOD", str(skeleton()), "gumaarms.w3d",
				"Bad: gumaarms.w3d does not fit art\\w3d\\gu\\gumaarms.w3d (it has no model GUMaArms (the original's HLOD)" + tail },
		};
		for (const Case &c : cases)
		{
			CAPTURE(c.what);
			TempDir tmp;
			writeFile(tmp.path / "Bad" / c.file, c.bytes);
			writeFile(tmp.path / "Bad" / "gumaarms.dds", "DDS ");
			AssetPacks packs;
			std::vector<std::string> errors;
			REQUIRE(packs.addPack((tmp.path / "Bad").u8string(), errors));
			CHECK(packs.checkModels(mounted, errors));
			CHECK(errors.empty());
			CHECK(packs.notes() == std::vector<std::string>{ c.why });
			CHECK(packs.model(c.file) == nullptr); // left out: the game's model is drawn
			CHECK(packs.texture("gumaarms.tga") != nullptr); // the rest of the pack stays
		}
		TempDir tmp;
		writeFile(tmp.path / "Bad" / "gumaarms.w3d", "not a w3d");
		AssetPacks packs;
		std::vector<std::string> errors;
		REQUIRE(packs.addPack((tmp.path / "Bad").u8string(), errors));
		CHECK_FALSE(packs.checkModels(mounted, errors));
		REQUIRE(errors.size() == 1);
		CHECK(errors[0].rfind("asset pack Bad: gumaarms.w3d is not a W3D file the game can read: ", 0) == 0);
	}
}

TEST_CASE("SDK-1: a replacement animation file must keep the original's animations")
{
	const std::vector<SynthChannel> channel = { { ANIM_CHANNEL_X, 1, 0, 1, { 0.0f, 1.0f } } };
	Win32BIGFileSystem fsys;
	mount(fsys, "W3D.big", { { "art\\w3d\\gu\\gumaarms_atka.w3d", str(rawAnimChunk("GUMaArms_ATKA", "GUMaArms_SKL", 2, 30, channel)) } }, false);
	ArchiveW3DFileSource mounted(fsys);
	W3DFileContents original;
	std::string error;
	std::vector<std::uint8_t> bytes = rawAnimChunk("GUMaArms_ATKA", "GUMaArms_SKL", 2, 30, channel);
	REQUIRE_MESSAGE(Load_W3D_File(bytes.data(), bytes.size(), original, &error), error);
	REQUIRE(original.RawAnims.size() == 1);
	const std::string animName = original.RawAnims[0].Get_Name();

	TempDir tmp;
	writeFile(tmp.path / "Same" / "gumaarms_atka.w3d", str(rawAnimChunk("GUMaArms_ATKA", "GUMaArms_SKL", 2, 30, { { ANIM_CHANNEL_X, 1, 0, 1, { 0.0f, 2.0f } } })));
	writeFile(tmp.path / "Renamed" / "gumaarms_atka.w3d", str(rawAnimChunk("GUMaArms_ATKB", "GUMaArms_SKL", 2, 30, channel)));
	AssetPacks same, renamed;
	std::vector<std::string> errors;
	REQUIRE(same.addPack((tmp.path / "Same").u8string(), errors));
	CHECK(same.checkModels(mounted, errors));
	CHECK(same.notes().empty()); // other key values on the same animation: fits
	REQUIRE(renamed.addPack((tmp.path / "Renamed").u8string(), errors));
	CHECK(renamed.checkModels(mounted, errors));
	REQUIRE(renamed.notes().size() == 1);
	CHECK(renamed.notes()[0].find("(it has no animation " + animName + ")") != std::string::npos);
	CHECK(renamed.model("gumaarms_atka.w3d") == nullptr);
}

TEST_CASE("SDK-1: precedence: base game < mod < asset pack for drawing; the simulation keeps the mod's file and the skeleton check uses it")
{
	Win32BIGFileSystem fsys;
	mount(fsys, "W3D.big", { { "art\\w3d\\gu\\gumaarms.w3d", "retail" } }, false);
	// the mod's archive mounted over the game's (as -mod does: Common/ModMount.h), with its own skeleton name
	mount(fsys, "!!Mod.big", { { "art\\w3d\\gu\\gumaarms.w3d", str(model("GUModArms_SKL")) } }, true);
	ArchiveW3DFileSource simulation(fsys);
	REQUIRE(readSource(simulation, "art\\w3d\\gu\\gumaarms.w3d") == str(model("GUModArms_SKL")));

	TempDir tmp;
	writeFile(tmp.path / "HD" / "GUMaArms.w3d", str(model("GUModArms_SKL", "GUMaArms.HD")));
	AssetPacks packs;
	std::vector<std::string> errors;
	REQUIRE(packs.addPack((tmp.path / "HD").u8string(), errors));
	CHECK_MESSAGE(packs.checkModels(simulation, errors), joined(errors)); // fits the mod's model, which is what it replaces
	AssetPackW3DFileSource render(std::unique_ptr<W3DFileSource>(new ArchiveW3DFileSource(fsys)), packs);
	CHECK(readSource(render, "art\\w3d\\gu\\gumaarms.w3d") == str(model("GUModArms_SKL", "GUMaArms.HD")));
	CHECK(readSource(simulation, "art\\w3d\\gu\\gumaarms.w3d") == str(model("GUModArms_SKL"))); // unchanged: the logic never sees a pack

	// a pack made for the unmodded game (the retail skeleton) does not fit the mod's model: left out, rather than drawing a broken unit
	TempDir other;
	writeFile(other.path / "Vanilla" / "gumaarms.w3d", str(model()));
	AssetPacks vanilla;
	REQUIRE(vanilla.addPack((other.path / "Vanilla").u8string(), errors));
	CHECK(vanilla.checkModels(simulation, errors));
	REQUIRE(vanilla.notes().size() == 1);
	CHECK(vanilla.notes()[0].find("its model GUMaArms uses the hierarchy GUMaArms_SKL, the original's GUModArms_SKL") != std::string::npos);
	CHECK(vanilla.model("gumaarms.w3d") == nullptr);
}

namespace
{
std::string envOr(const char *name)
{
	const char *v = std::getenv(name);
	return v ? std::string(v) : std::string();
}
} // namespace

TEST_CASE("SDK-1 retail: a pack holding a retail model fits it; the same model with a renamed bone is left out")
{
	retailtest::Mount *m = retailtest::pureMount();
	if (!m)
	{
		retailtest::printSkip("SDK-1 retail skeleton check");
		return;
	}
	REQUIRE_MESSAGE(m->fs, m->error);
	ArchiveW3DFileSource mounted(*m->fs);
	// the Gondor archer's skeleton, copied from the player's own archives at run time (nothing retail is committed)
	const std::string path = W3D_Asset_Path("guarcher_skl", mounted.FlatAssetLayout());
	std::vector<std::uint8_t> bytes;
	std::string error;
	REQUIRE_MESSAGE(mounted.Read(path, bytes, &error), error);
	W3DFileContents original;
	REQUIRE_MESSAGE(Load_W3D_File(bytes.data(), bytes.size(), original, &error), error);
	REQUIRE(!original.HTrees.empty());
	REQUIRE(original.HTrees[0].Num_Pivots() > 2); // tolerance: any retail skeleton with a bone 2 serves (its size is the player's data)

	TempDir tmp;
	writeFile(tmp.path / "Same" / "guarcher_skl.w3d", str(bytes));
	AssetPacks same;
	std::vector<std::string> errors;
	REQUIRE(same.addPack((tmp.path / "Same").u8string(), errors));
	CHECK_MESSAGE(same.checkModels(mounted, errors), joined(errors));

	// rename bone 2 in the file's pivot table (W3dPivotStruct.Name, 16 bytes) to a name no retail skeleton has
	const std::string bone = original.HTrees[0].Get_Pivot(2).Name;
	std::string renamed = str(bytes);
	const size_t at = renamed.find(bone + std::string(1, '\0'));
	REQUIRE(at != std::string::npos);
	renamed.replace(at, 4, "ZZZZ");
	writeFile(tmp.path / "Renamed" / "guarcher_skl.w3d", renamed);
	AssetPacks bad;
	REQUIRE(bad.addPack((tmp.path / "Renamed").u8string(), errors));
	CHECK(bad.checkModels(mounted, errors));
	REQUIRE(bad.notes().size() == 1);
	CHECK(bad.notes()[0].find("Renamed: guarcher_skl.w3d does not fit " + path + " (its hierarchy ") == 0);
	CHECK(bad.model(path) == nullptr);
}

TEST_CASE("SDK-1 retail: the asset pack in OPENBFME_ASSET_PACK loads over 2.01; its models that do not fit are listed")
{
	const std::string pack = envOr("OPENBFME_ASSET_PACK");
	retailtest::Mount *m = retailtest::pureMount();
	if (!m || pack.empty())
	{
		retailtest::printSkip("SDK-1 asset pack corpus (OPENBFME_ASSET_PACK unset)");
		return;
	}
	REQUIRE_MESSAGE(m->fs, m->error);
	ArchiveW3DFileSource mounted(*m->fs);
	AssetPacks packs;
	std::vector<std::string> errors;
	REQUIRE_MESSAGE(packs.addPack(pack, errors), joined(errors));
	const bool fits = packs.checkModels(mounted, errors);
	MESSAGE(packs.modelCount() << " models used, " << packs.textureCount() << " textures; " << packs.notes().size()
							   << " notes (models left out or replacing nothing); " << packs.onlineRefusals().size() << " files offline only");
	for (const AssetPacks::Refusal &r : packs.onlineRefusals())
		MESSAGE("offline only: " << r.relative << ": " << r.why);
	MESSAGE(joined(errors));
	MESSAGE(joined(packs.notes()));
	CHECK(fits);
	// the online offset rule's measure: how far each used model's drawn box moved, against the original's largest size
	std::map<std::string, std::unique_ptr<W3DFileContents>> trees;
	auto hierarchy = [&](const std::string &name) -> const HTreeClass * {
		const std::string k = AsciiStringUtil::lowered(name);
		auto it = trees.find(k);
		if (it == trees.end())
		{
			auto c = std::make_unique<W3DFileContents>();
			std::vector<std::uint8_t> b;
			std::string e;
			if (!mounted.Read(W3D_Asset_Path(k, mounted.FlatAssetLayout()), b, &e) || !Load_W3D_File(b.data(), b.size(), *c, &e))
				c.reset();
			it = trees.emplace(k, std::move(c)).first;
		}
		return it->second ? it->second->Find_HTree(name) : nullptr;
	};
	std::vector<std::pair<float, std::string>> moves;
	std::vector<std::tuple<float, float, std::string>> bones; // a skeleton's largest bone move (of its size) and turn (degrees)
	for (const auto &kv : packs.models())
	{
		const std::string stem = kv.first.substr(0, kv.first.rfind('.'));
		std::vector<std::uint8_t> mine, theirs;
		std::string e;
		W3DFileContents a, b;
		if (!AssetPacks::read(kv.second, mine, &e) || !Load_W3D_File(mine.data(), mine.size(), b, &e) ||
			!mounted.Read(W3D_Asset_Path(stem, mounted.FlatAssetLayout()), theirs, &e) || !Load_W3D_File(theirs.data(), theirs.size(), a, &e))
			continue;
		for (const HTreeClass &o : a.HTrees)
		{
			const HTreeClass *r = b.Find_HTree(o.Get_Name());
			if (!r || r->Num_Pivots() != o.Num_Pivots())
				continue;
			HTreePose po, pr;
			o.Base_Pose(Matrix3D(), po);
			r->Base_Pose(Matrix3D(), pr);
			float lo[3] = { 0, 0, 0 }, hi[3] = { 0, 0, 0 }, worstMove = 0.0f, worstDot = 1.0f;
			for (int i = 0; i < po.Num_Pivots(); ++i)
				for (int k = 0; k < 3; ++k)
				{
					lo[k] = std::min(lo[k], po.Transform[(size_t)i].Row[k][3]);
					hi[k] = std::max(hi[k], po.Transform[(size_t)i].Row[k][3]);
				}
			const float span = std::max({ 1.0f, hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2] });
			for (int i = 0; i < po.Num_Pivots(); ++i)
			{
				float d2 = 0.0f;
				for (int k = 0; k < 3; ++k)
					d2 += std::pow(pr.Transform[(size_t)i].Row[k][3] - po.Transform[(size_t)i].Row[k][3], 2.0f);
				worstMove = std::max(worstMove, std::sqrt(d2) / span);
				for (int c = 0; c < 3; ++c)
					worstDot = std::min(worstDot, po.Transform[(size_t)i].Row[0][c] * pr.Transform[(size_t)i].Row[0][c] +
							po.Transform[(size_t)i].Row[1][c] * pr.Transform[(size_t)i].Row[1][c] + po.Transform[(size_t)i].Row[2][c] * pr.Transform[(size_t)i].Row[2][c]);
			}
			bones.emplace_back(worstMove, std::acos(std::min(1.0f, worstDot)) * 180.0f / 3.14159265f, kv.second.relative);
		}
		float a0[3], a1[3], b0[3], b1[3];
		if (!AssetPacks::modelBounds(a, stem, hierarchy, a0, a1) || !AssetPacks::modelBounds(b, stem, hierarchy, b0, b1))
			continue;
		float span = 1.0f;
		for (int i = 0; i < 3; ++i)
			span = std::max(span, a1[i] - a0[i]);
		float moved = 0.0f;
		for (int i = 0; i < 3; ++i)
			moved = std::max(moved, std::fabs((b0[i] + b1[i]) - (a0[i] + a1[i])) * 0.5f / span);
		moves.emplace_back(moved, kv.second.relative);
	}
	std::sort(moves.rbegin(), moves.rend());
	for (size_t i = 0; i < moves.size() && i < 6; ++i)
		MESSAGE("centre move " << moves[i].first << " of the original's size: " << moves[i].second);
	std::sort(bones.rbegin(), bones.rend());
	for (size_t i = 0; i < bones.size() && i < 4; ++i)
		MESSAGE("bone move " << std::get<0>(bones[i]) << " of the skeleton's size: " << std::get<2>(bones[i]));
	std::sort(bones.begin(), bones.end(), [](const auto &x, const auto &y) { return std::get<1>(x) > std::get<1>(y); });
	for (size_t i = 0; i < bones.size() && i < 4; ++i)
		MESSAGE("bone turn " << std::get<1>(bones[i]) << " degrees: " << std::get<2>(bones[i]));
}

TEST_CASE("SDK-1b (a): suspended packs answer nothing: a LAN / online game draws the game's own files; the packs stay loaded")
{
	TempDir tmp;
	writeFile(tmp.path / "HD" / "gumaarms.dds", "DDS ");
	writeFile(tmp.path / "HD" / "gumaarms.w3d", str(model()));
	AssetPacks packs;
	std::vector<std::string> errors;
	REQUIRE(packs.addPack((tmp.path / "HD").u8string(), errors));
	Win32BIGFileSystem fsys;
	mount(fsys, "W3D.big", { { "art\\w3d\\gu\\gumaarms.w3d", "retail" } }, false);
	AssetPackW3DFileSource render(std::unique_ptr<W3DFileSource>(new ArchiveW3DFileSource(fsys)), packs);
	REQUIRE(packs.texture("GUMaArms.tga") != nullptr);
	REQUIRE(readSource(render, "art\\w3d\\gu\\gumaarms.w3d") == str(model()));
	packs.setSuspended(true);
	CHECK(packs.suspended());
	CHECK(packs.loaded());
	CHECK(packs.empty()); // RetailFileSystem::renderW3DSource then hands out the archives alone
	CHECK(packs.texture("GUMaArms.tga") == nullptr);
	CHECK(packs.model("art\\w3d\\gu\\gumaarms.w3d") == nullptr);
	CHECK(readSource(render, "art\\w3d\\gu\\gumaarms.w3d") == "retail"); // a source made before also stops answering with the pack
	packs.setSuspended(false);
	CHECK(packs.texture("GUMaArms.tga") != nullptr);
}

namespace
{
// a made-up building `stem`: a hierarchy with its root, a box mesh of half-size `h` (x and y) and height `z` raised by `dz`, and its HLOD
struct Tower
{
	std::string stem = "gbtower";
	float h = 10, z = 40, dz = 0;
	bool mesh = true;
	bool lowLodOnly = false;   // the body only in a lower LOD; the top LOD (what is drawn) empty
	bool seeThrough = false;   // the body alpha blended, with `texture` and vertex material opacity `opacity`
	float opacity = 1.0f;
	std::string texture = "GBTower.tga";
	bool hidden = false;
	std::string aggregate;     // an aggregate sub-object (another file's model or an emitter)
	float topZ = 40;           // the hierarchy's second bone, B_TOP: its height and whether it is turned a quarter round x
	bool topTurned = false;
	std::vector<float> animZ;  // an animation IDLE: B_TOP's height per frame
};

std::string tower(const Tower &t)
{
	std::string upper = t.stem;
	for (char &c : upper)
		c = (char)std::toupper((unsigned char)c);
	W3dPivotStruct top = pivot("B_TOP", 0, 0, 0, t.topZ);
	if (t.topTurned)
	{
		top.Rotation.Q[0] = 0.70710678f;
		top.Rotation.Q[3] = 0.70710678f;
	}
	std::vector<std::uint8_t> file = hierarchyChunk(upper.c_str(), { pivot("ROOTTRANSFORM", 0xffffffffu, 0, 0, 0), top });
	if (t.mesh)
	{
		SynthMesh m;
		m.Name = "BODY";
		m.Container = upper;
		m.Attributes = t.hidden ? W3D_MESH_FLAG_HIDDEN : 0;
		m.Vertices = { { -t.h, -t.h, t.dz }, { t.h, -t.h, t.dz }, { t.h, t.h, t.dz + t.z }, { -t.h, t.h, t.dz + t.z } };
		m.Triangles = { 0, 1, 2, 0, 2, 3 };
		W3dShaderStruct sh = {};
		sh.DepthCompare = 3; // LEQUAL (the file stores the D3DCMP value - 1)
		sh.DepthMask = W3DSHADER_DEPTHMASK_WRITE_ENABLE;
		sh.SrcBlend = t.seeThrough ? W3DSHADER_SRCBLENDFUNC_SRC_ALPHA : W3DSHADER_SRCBLENDFUNC_ONE;
		sh.DestBlend = t.seeThrough ? W3DSHADER_DESTBLENDFUNC_ONE_MINUS_SRC_ALPHA : W3DSHADER_DESTBLENDFUNC_ZERO;
		sh.Texturing = 1;
		m.Shaders = { sh };
		SynthVMat vm;
		vm.Opacity = t.opacity;
		m.VertexMaterials = { vm };
		m.Textures = { SynthTexture{ t.texture } };
		SynthPass pass;
		pass.VertexMaterialIds = { 0 };
		pass.ShaderIds = { 0 };
		SynthStage stage;
		stage.TextureIds = { 0 };
		stage.TexCoords.assign(4, W3dTexCoordStruct{ 0, 0 });
		pass.Stages = { stage };
		m.Passes = { pass };
		append(file, meshChunk(m));
	}
	const std::vector<SynthSub> body = t.mesh ? std::vector<SynthSub>{ { upper + ".BODY", 0 } } : std::vector<SynthSub>{};
	std::vector<SynthSub> aggregates;
	if (!t.aggregate.empty())
		aggregates.push_back({ t.aggregate, 0 });
	if (!t.animZ.empty())
		append(file, rawAnimChunk("IDLE", upper.c_str(), (std::uint32_t)t.animZ.size(), 30, { { ANIM_CHANNEL_Z, 1, 0, 1, t.animZ } }));
	append(file, t.lowLodOnly ? hlodLodsChunk(upper.c_str(), upper.c_str(), { body, {} }, aggregates) : hlodLodsChunk(upper.c_str(), upper.c_str(), { body }, aggregates));
	return str(file);
}

std::string building(float h, float z, bool withMesh = true, const char *stem = "gbtower")
{
	Tower t;
	t.stem = stem;
	t.h = h;
	t.z = z;
	t.mesh = withMesh;
	return tower(t);
}
} // namespace

TEST_CASE("SDK-1b (b): the online rules: the visibility deny-list and the size rule; offline every file is used")
{
	Win32BIGFileSystem fsys;
	mount(fsys, "W3D.big", { { "art\\w3d\\gb\\gbtower.w3d", building(10, 40) }, { "art\\w3d\\gb\\gbtower2.w3d", building(10, 40, true, "gbtower2") },
								{ "art\\w3d\\gb\\gbtower3.w3d", building(10, 40, true, "gbtower3") }, { "art\\w3d\\mp\\mpbeacon.w3d", building(5, 5, true, "mpbeacon") } },
		false);
	ArchiveW3DFileSource mounted(fsys);
	TempDir tmp;
	writeFile(tmp.path / "HD" / "gbtower.w3d", building(14, 70));     // 1.4 x 1.75: an honest remake
	writeFile(tmp.path / "HD" / "gbtower2.w3d", building(40, 40, true, "gbtower2"));    // four times as wide: could hide units
	writeFile(tmp.path / "HD" / "gbtower3.w3d", building(10, 40, false, "gbtower3")); // draws nothing: could reveal what the tower hides
	writeFile(tmp.path / "HD" / "mpbeacon.w3d", building(5, 5, true, "mpbeacon"));
	writeFile(tmp.path / "HD" / "EXFireScroll2.dds", "DDS ");
	writeFile(tmp.path / "HD" / "gbtower.dds", "DDS ");
	AssetPacks packs;
	std::vector<std::string> errors;
	REQUIRE(packs.addPack((tmp.path / "HD").u8string(), errors));
	REQUIRE_MESSAGE(packs.checkModels(mounted, errors), joined(errors));
	std::map<std::string, std::string> why;
	for (const AssetPacks::Refusal &r : packs.onlineRefusals())
		why[r.relative] = r.why;
	CHECK(why.size() == 4);
	CHECK(why["gbtower2.w3d"] == "it is 4.00 times the original's size along x (online: at most 3 times either way)");
	CHECK(why["gbtower3.w3d"] == "it draws nothing where the original draws a model");
	CHECK(why["mpbeacon.w3d"].rfind("a visibility-relevant asset: the multiplayer beacon (object/civilian/civilianprop.ini MultiplayerBeacon)", 0) == 0);
	CHECK(why["EXFireScroll2.dds"].rfind("a visibility-relevant asset: the burning effects play even inside the shroud", 0) == 0);
	for (const char *m : { "gbtower.w3d", "gbtower2.w3d", "gbtower3.w3d", "mpbeacon.w3d" })
		CHECK(packs.model(std::string("art\\w3d\\xx\\") + m) != nullptr); // offline: all used
	CHECK(packs.texture("EXFireScroll2.tga") != nullptr);
	packs.setOnline(true);
	CHECK(packs.model("art\\w3d\\gb\\gbtower.w3d") != nullptr); // the honest remake stays online
	CHECK(packs.texture("gbtower.tga") != nullptr);
	for (const char *m : { "gbtower2.w3d", "gbtower3.w3d", "mpbeacon.w3d" })
		CHECK(packs.model(std::string("art\\w3d\\xx\\") + m) == nullptr);
	CHECK(packs.texture("exfirescroll2.tga") == nullptr);
}

TEST_CASE("SDK-1b (b, Sol r1): the online model rules follow what the renderer draws: its top LOD, its aggregates, see-through surfaces, the box's place")
{
	auto check = [](const Tower &original, const Tower &replacement) {
		W3DFileContents a, b;
		std::string error;
		const std::string ab = tower(original), bb = tower(replacement);
		REQUIRE_MESSAGE(Load_W3D_File((const std::uint8_t *)ab.data(), ab.size(), a, &error), error);
		REQUIRE_MESSAGE(Load_W3D_File((const std::uint8_t *)bb.data(), bb.size(), b, &error), error);
		REQUIRE(AssetPacks::compareSkeletons(a, b).empty()); // each one passes the offline check
		// the see-through shares the Godot side would decode: the tower's own texture 10 %, a texture of its own 95 %
		auto coverage = [](const std::string &stem, bool, double &share, std::string &error) {
			if (stem == "gbtower" || stem == "exclear")
			{
				share = stem == "gbtower" ? 0.1 : 0.95;
				return true;
			}
			error = "no texture " + stem;
			return false;
		};
		auto none = [](const std::string &) -> const HTreeClass * { return nullptr; };
		return AssetPacks::onlineModelCheck(a, b, original.stem, none, none, coverage);
	};
	Tower base, glass;
	glass.seeThrough = true; // a window: the original draws it see-through
	Tower honest = base;
	honest.h = 12;
	honest.z = 46;
	honest.dz = 2;
	CHECK(check(base, honest) == "");
	Tower lowLod = base;
	lowLod.lowLodOnly = true; // the body kept, but only in a LOD the renderer does not draw
	CHECK(check(base, lowLod) == "it draws nothing where the original draws a model");
	Tower blended = base;
	blended.seeThrough = true; // blended now, with its 10 % see-through texture: within the rule
	CHECK(check(base, blended) == "");
	Tower clear = blended;
	clear.opacity = 0.0f; // vertex material opacity 0: an invisible tower of the right size
	CHECK(check(base, clear) == "its see-through mesh GBTOWER.BODY has an opacity of 0.00, the original's 1.00 (online: at most 0.25 lower)");
	CHECK(check(glass, clear) == "its see-through mesh GBTOWER.BODY has an opacity of 0.00, the original's 1.00 (online: at most 0.25 lower)");
	Tower other = glass;
	other.texture = "EXClear.tga"; // a texture of its own, mostly see-through
	CHECK(check(glass, other) == "its see-through mesh GBTOWER.BODY draws the texture exclear, 95% see-through; the original's see-through surfaces 10% "
								 "(online: at most 25 points more)");
	Tower unknown = glass;
	unknown.texture = "Nowhere.tga";
	CHECK(check(glass, unknown) == "its see-through mesh GBTOWER.BODY's texture nowhere cannot be checked: no texture nowhere");
	Tower moved = base;
	moved.dz = 10000; // the same size, far above the map
	CHECK(check(base, moved) == "its centre is 10000.0 units from the original's along z (online: at most 50% of the original's size)");
	Tower borrowed = base;
	borrowed.aggregate = "PTPINE01.TRUNK"; // another file's model drawn with it
	CHECK(check(base, borrowed) == "it draws ptpine01.trunk, which the original does not");
	Tower hidden = base;
	hidden.hidden = true;
	CHECK(check(base, hidden) == "its mesh GBTOWER.BODY is hidden where the original's is shown");
	// a skeleton replaced alone moves every model on it: its rest pose stays the original's (the HD Edition's skeletons match exactly)
	Tower raised = base;
	raised.topZ = 45; // 5 units of a 40 unit skeleton
	CHECK(check(base, raised) == "its hierarchy GBTOWER moves the bone B_TOP by 5.0 units (online: at most 10% of the skeleton's size)");
	Tower turned = base;
	turned.topTurned = true;
	CHECK(check(base, turned) == "its hierarchy GBTOWER turns the bone B_TOP by over 10 degrees");
	Tower nudged = base;
	nudged.topZ = 42;
	CHECK(check(base, nudged) == "");
	// an animation file replaced: its bones keep to the original's path (a unit drawn elsewhere, or under the ground, hides it)
	Tower idle = base, lifted = base, swaying = base;
	idle.animZ = { 40, 42 };
	lifted.animZ = { 40, 142 };
	swaying.animZ = { 40, 44 };
	CHECK(check(idle, swaying) == "");
	CHECK(check(idle, lifted) == "its animation GBTOWER.IDLE moves bone 1 by 100.0 units from the original's at frame 1 (online: at most 10% of the "
								 "animation's reach, 4.2 units)");
	Tower longer = idle;
	longer.animZ = { 40, 42, 900 }; // one frame more than the original, and the renderer plays it
	CHECK(check(idle, longer).rfind("its animation GBTOWER.IDLE moves bone 1 by 858.0 units from the original's at frame 2", 0) == 0);
}

TEST_CASE("SDK-1b (b, review r1): a pack file the checks cannot read through is left out with a named reason, never a crash")
{
	// a compressed (timecoded) animation whose only key is at frame 5: its frames 0 .. 4 have no value (HCompressedAnimClass throws
	// UndefinedFrameError for them)
	ChunkWriter anim;
	W3dCompressedAnimHeaderStruct ah = {};
	ah.Version = 1; // the classic channel form (hcanim.cpp)
	setName(ah.Name, sizeof(ah.Name), "IDLE");
	setName(ah.HierarchyName, sizeof(ah.HierarchyName), "GBTOWER");
	ah.NumFrames = 2;
	ah.FrameRate = 30;
	ah.Flavor = ANIM_FLAVOR_TIMECODED;
	anim.chunk(W3D_CHUNK_COMPRESSED_ANIMATION_HEADER, ChunkWriter::of(ah));
	Bytes channel;
	channel.u32(1).u16(1).u8(1).u8(ANIM_CHANNEL_Z).u32(5).f32(40.0f);
	anim.chunk(W3D_CHUNK_COMPRESSED_ANIMATION_CHANNEL, channel.v);
	ChunkWriter late;
	late.wrapper(W3D_CHUNK_COMPRESSED_ANIMATION, anim);

	Tower idle;
	idle.animZ = { 40, 42 };
	Win32BIGFileSystem fsys;
	mount(fsys, "W3D.big", { { "art\\w3d\\gb\\gbtower.w3d", tower(idle) } }, false);
	ArchiveW3DFileSource mounted(fsys);
	TempDir tmp;
	Tower plain;
	std::vector<std::uint8_t> bytes;
	const std::string body = tower(plain);
	bytes.assign(body.begin(), body.end());
	append(bytes, late.bytes);
	writeFile(tmp.path / "Late" / "gbtower.w3d", str(bytes));
	AssetPacks packs;
	std::vector<std::string> errors;
	REQUIRE(packs.addPack((tmp.path / "Late").u8string(), errors));
	REQUIRE_MESSAGE(packs.checkModels(mounted, errors), joined(errors));
	std::map<std::string, std::string> why;
	for (const AssetPacks::Refusal &r : packs.onlineRefusals())
		why[r.relative] = r.why;
	for (const AssetPacks::Refusal &r : packs.misfits())
		why[r.relative] = r.why;
	MESSAGE(why["gbtower.w3d"]);
	CHECK(why["gbtower.w3d"] == "its animation GBTOWER.IDLE has no value for bone 1 at frame 0 where the original has one");
	// the threshold, measured on the real RotWK 2.02 HD Edition (the corpus test below prints it): its honest remakes' centres move by at
	// most 0.376 of the original's largest size (the dwarf ram); the one over 0.5 is the siege ball, refused for its size already
	CHECK(AssetPacks::kOnlineMaxOffset == 0.5f);
	CHECK(0.376f < AssetPacks::kOnlineMaxOffset); // tolerance: the threshold sits above the measured honest maximum
}

TEST_CASE("SDK-1b (b): alpha coverage: the share of see-through pixels")
{
	std::vector<std::uint8_t> rgba(4 * 10, 255);
	CHECK(AssetPacks::alphaCoverage(rgba.data(), 10) == 0.0);
	rgba[3] = 0;
	rgba[7] = 127;
	rgba[11] = 128;
	CHECK(AssetPacks::alphaCoverage(rgba.data(), 10) == 2.0 / 10.0); // alpha 0 and 127 of 10; 128 is solid
	// the threshold, measured on the real RotWK 2.02 HD Edition (55 of its textures that retail draws with alpha test / blending): its honest
	// remakes differ by at most 0.107; a see-through tree (0.3 -> 0.8) differs by 0.5
	CHECK(AssetPacks::kOnlineMaxAlphaDelta == 0.25);
	CHECK(0.107 < AssetPacks::kOnlineMaxAlphaDelta); // tolerance: the threshold sits between the measured honest maximum and a see-through remake
	CHECK(0.5 > AssetPacks::kOnlineMaxAlphaDelta); // tolerance: as above
}
