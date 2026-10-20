// OpenBFME unit tests. GPL-3.0.
// Installs in any language (RetailArchivePolicy.h "Languages"): the language is detected from lang/*.big, the English
// language archives are swapped for that language's (pinned when known, else mounted with a warning), and the
// language-independent archives are verified as for English.

#include "doctest.h"
#include "BigTestUtil.h"
#include "RetailTestMount.h"

#include "Common/AsciiString.h"
#include "Common/InstallLocator.h"
#include "Common/MD5.h"
#include "Common/RetailArchivePolicy.h"
#include "GameClient/GameText.h"
#include "GameEngineDevice/Win32Device/Common/Win32BIGFileSystem.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <random>

using namespace bigtest;
namespace fs = std::filesystem;

namespace
{

struct LangTempDir
{
	fs::path path;
	LangTempDir()
	{
		std::random_device rd;
		path = fs::temp_directory_path() / ("openbfme_lang_" + std::to_string(rd()) + std::to_string(rd()));
		fs::create_directories(path);
	}
	~LangTempDir()
	{
		std::error_code ec;
		fs::remove_all(path, ec);
	}
};

void writeBytes(const fs::path &p, const std::vector<std::uint8_t> &bytes)
{
	fs::create_directories(p.parent_path());
	std::ofstream out(p, std::ios::binary | std::ios::trunc);
	out.write(reinterpret_cast<const char *>(bytes.data()), (std::streamsize)bytes.size());
}

RetailArchiveEntry pin(const std::string &path, const std::vector<std::uint8_t> &bytes, const std::string &language)
{
	RetailArchiveEntry e;
	e.path = path;
	e.md5 = MD5::ofBytes(bytes.data(), bytes.size());
	e.size = bytes.size();
	e.language = language;
	return e;
}

std::map<std::string, std::uint64_t> keys(const std::vector<std::string> &paths)
{
	std::map<std::string, std::uint64_t> out;
	for (const std::string &p : paths)
		out[p] = 1;
	return out;
}

const RetailArchiveEntry *find(const RetailArchivePolicy &p, const std::string &path)
{
	for (const RetailArchiveEntry &a : p.archives)
		if (a.path == path)
			return &a;
	return nullptr;
}

std::string readVirtualFile(ArchiveFileSystem &fsys, const std::string &name)
{
	std::vector<std::uint8_t> bytes;
	std::string error;
	if (!fsys.readFile(name, bytes, &error))
		return "<" + error + ">";
	return std::string(bytes.begin(), bytes.end());
}

struct ScopedEnv
{
	std::string name;
	ScopedEnv(const char *n, const char *value) : name(n)
	{
#ifdef _WIN32
		_putenv_s(n, value);
#else
		setenv(n, value, 1);
#endif
	}
	~ScopedEnv()
	{
#ifdef _WIN32
		_putenv_s(name.c_str(), "");
#else
		unsetenv(name.c_str());
#endif
	}
};

} // namespace

TEST_CASE("anylang: the install language is the stem of its single lang/<language>.big")
{
	CHECK(DetectRetailLanguage(keys({ "lang/english.big", "lang/englishaudio.big", "lang/englishpatch201.big" })) == "English");
	CHECK(DetectRetailLanguage(keys({ "lang/german.big", "lang/germanaudio.big", "lang/germanpatch201.big", "ini.big" })) == "German");
	CHECK(DetectRetailLanguage(keys({ "lang/dutch.big", "lang/englishaudio.big", "lang/dutchpatch201.big" })) == "Dutch"); // shares EnglishAudio
	CHECK(DetectRetailLanguage(keys({ "lang/English.big", "lang/german.big" })) == "English"); // English wins: german.big is then unknown
	CHECK(DetectRetailLanguage(keys({ "lang/german.big", "lang/french.big" })).empty());       // ambiguous
	CHECK(DetectRetailLanguage(keys({ "ini.big", "lang/germanaudio.big" })).empty());          // no language's own archive
	CHECK(DetectRetailLanguage(keys({ "lang/sub/german.big" })).empty());
}

TEST_CASE("anylang: localizing the built-in policies")
{
	RetailArchivePolicy rotwk, bfme2;
	std::string error;
	REQUIRE_MESSAGE(RetailArchivePolicy::loadBuiltin("rotwk-201", rotwk, &error), error);
	REQUIRE_MESSAGE(RetailArchivePolicy::loadBuiltin("bfme2-106", bfme2, &error), error);
	std::map<std::string, std::uint64_t> rotwkDisk, bfme2Disk;
	for (const RetailArchiveEntry &a : rotwk.archives)
		if (a.language == "ALL")
			rotwkDisk[AsciiStringUtil::lowered(a.path)] = a.size;
	for (const RetailArchiveEntry &a : bfme2.archives)
		if (a.language == "ALL")
			bfme2Disk[AsciiStringUtil::lowered(a.path)] = a.size;

	SUBCASE("an English folder keeps the policy as it is")
	{
		auto disk = rotwkDisk;
		disk["lang/english.big"] = disk["lang/englishaudio.big"] = disk["lang/englishpatch201.big"] = 1;
		RetailLanguageChoice c;
		const RetailArchivePolicy same = LocalizeRetailPolicy(rotwk, disk, &c);
		CHECK(c.english);
		CHECK(c.language == "English");
		REQUIRE(same.archives.size() == rotwk.archives.size());
		for (size_t i = 0; i < same.archives.size(); ++i)
		{
			CHECK(same.archives[i].path == rotwk.archives[i].path);
			CHECK(same.archives[i].md5 == rotwk.archives[i].md5);
			CHECK(same.archives[i].pinned);
		}
		CHECK(same.excluded == rotwk.excluded);
	}

	SUBCASE("German: the known pins, the English case style and the English mount order")
	{
		auto disk = rotwkDisk;
		disk["lang/german.big"] = disk["lang/germanaudio.big"] = disk["lang/germanpatch201.big"] = 1;
		RetailLanguageChoice c;
		const RetailArchivePolicy de = LocalizeRetailPolicy(rotwk, disk, &c);
		CHECK_FALSE(c.english);
		CHECK(c.language == "German");
		CHECK(c.unpinned.empty());
		REQUIRE(de.archives.size() == rotwk.archives.size());
		CHECK(find(de, "lang/English.big") == nullptr);
		const RetailArchiveEntry *base = find(de, "lang/German.big");
		const RetailArchiveEntry *audio = find(de, "lang/GermanAudio.big");
		const RetailArchiveEntry *patch = find(de, "lang/germanpatch201.big");
		REQUIRE(base);
		REQUIRE(audio);
		REQUIRE(patch);
		CHECK(base->md5 == "dc848c78316743147da8c4faefa13813");
		CHECK(base->size == 4851405);
		CHECK(audio->md5 == "0ab4448d26b969aa745858397035827e");
		CHECK(patch->md5 == "ae6b32eca5888d9069e72df59a714771");
		CHECK(std::find(de.excluded.begin(), de.excluded.end(), "lang/germanpatch202.big") != de.excluded.end());
		// the load order puts each German archive where its English one was
		const auto en = RetailArchiveLoadOrder(rotwk.archives);
		const auto ge = RetailArchiveLoadOrder(de.archives);
		REQUIRE(en.size() == ge.size());
		for (size_t i = 0; i < en.size(); ++i)
		{
			if (en[i].language == "ALL")
				CHECK(ge[i].path == en[i].path);
			else
				CHECK(AsciiStringUtil::lowered(ge[i].path).compare(0, 11, "lang/german") == 0);
		}

		auto bdisk = bfme2Disk;
		bdisk["lang/german.big"] = bdisk["lang/germanaudio.big"] = bdisk["lang/germanpatch105.big"] = 1;
		RetailLanguageChoice bc;
		const RetailArchivePolicy bde = LocalizeRetailPolicy(bfme2, bdisk, &bc);
		CHECK(bc.unpinned.empty());
		REQUIRE(find(bde, "lang/german.big"));
		CHECK(find(bde, "lang/german.big")->md5 == "6b095f659c94f545afaa7e51b8c8e92f");
		REQUIRE(find(bde, "lang/germanpatch105.big"));
		CHECK(find(bde, "lang/germanaudio.big")->size == 557553322);
	}

	SUBCASE("a language with no pins is unpinned; a shared audio archive stays the pinned English one")
	{
		auto disk = rotwkDisk;
		disk["lang/dutch.big"] = disk["lang/dutchpatch201.big"] = disk["lang/englishaudio.big"] = 1;
		RetailLanguageChoice c;
		const RetailArchivePolicy nl = LocalizeRetailPolicy(rotwk, disk, &c);
		CHECK(c.language == "Dutch");
		CHECK(c.unpinned == std::vector<std::string>{ "lang/Dutch.big", "lang/dutchpatch201.big" });
		REQUIRE(find(nl, "lang/EnglishAudio.big"));
		CHECK(find(nl, "lang/EnglishAudio.big")->pinned);
		REQUIRE(find(nl, "lang/Dutch.big"));
		CHECK_FALSE(find(nl, "lang/Dutch.big")->pinned);
	}
}

TEST_CASE("anylang: mounting a German folder, an unknown language and strict mode")
{
	LangTempDir tmp;
	const fs::path root = tmp.path / "rotwk";
	auto ini = makeBig("BIGF", { { "data\\ini\\weapon.ini", "base" } });
	auto en = makeBig("BIGF", { { "data\\lotr.str", "english" } });
	auto enPatch = makeBig("BIGF", { { "data\\lotr.str", "englishpatch201" } });
	auto de = makeBig("BIGF", { { "lotr.csf", "german" }, { "language.ini", "de" } });
	auto dePatch = makeBig("BIGF", { { "lotr.csf", "germanpatch201" } });
	writeBytes(root / "ini.big", ini);
	writeBytes(root / "lang" / "german.big", de);
	writeBytes(root / "lang" / "germanpatch201.big", dePatch);

	RetailInstall install;
	install.label = "rotwk";
	install.root = root.u8string();
	install.policy.game = "rotwk-test"; // no built-in pins: the German archives are unpinned here
	install.policy.patch = "2.01";
	install.policy.archives = { pin("INI.big", ini, "ALL"), pin("lang/English.big", en, "EN"), pin("lang/englishpatch201.big", enPatch, "EN") };

	SUBCASE("the default mounts them with a warning, the language patch wins")
	{
		Win32BIGFileSystem fsys;
		RetailMountReport r = MountRetailArchives(fsys, { install }, RetailMountOptions());
		REQUIRE_MESSAGE(r.ok, (r.errors.empty() ? std::string() : r.errors[0]));
		CHECK(r.languages == std::vector<std::string>{ "rotwk:German" });
		CHECK(r.mounted.size() == 3);
		CHECK(r.warnings.size() == 2);
		CHECK(r.warnings[0].find("no known md5") != std::string::npos);
		CHECK(readVirtualFile(fsys, "lotr.csf") == "germanpatch201");
		CHECK(readVirtualFile(fsys, "language.ini") == "de");
		for (const MountedArchive &m : r.mounted)
			CHECK(m.md5.size() == 32);
	}

	SUBCASE("OPENBFME_STRICT_LANGUAGE refuses unpinned language archives")
	{
		ScopedEnv strict("OPENBFME_STRICT_LANGUAGE", "1");
		Win32BIGFileSystem fsys;
		RetailMountReport r = MountRetailArchives(fsys, { install }, RetailMountOptions());
		CHECK_FALSE(r.ok);
		CHECK(r.errors.size() == 2);
		CHECK(fsys.getArchiveCount() == 0);
	}

	SUBCASE("a language archive that is not a BIG is still refused")
	{
		std::vector<std::uint8_t> junk = { 'n', 'o', 'p', 'e', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
		writeBytes(root / "lang" / "germanpatch201.big", junk);
		Win32BIGFileSystem fsys;
		RetailMountReport r = MountRetailArchives(fsys, { install }, RetailMountOptions());
		CHECK_FALSE(r.ok);
		CHECK(fsys.getArchiveCount() == 0);
	}

	SUBCASE("a missing language archive names the language's file; the English archives are untouched")
	{
		fs::remove(root / "lang" / "germanpatch201.big");
		Win32BIGFileSystem fsys;
		RetailMountReport r = MountRetailArchives(fsys, { install }, RetailMountOptions());
		CHECK_FALSE(r.ok);
		REQUIRE(r.errors.size() == 1);
		CHECK(r.errors[0].find("missing archive lang/germanpatch201.big") != std::string::npos);
	}

	SUBCASE("a language-independent archive is still md5-checked")
	{
		auto bad = ini;
		bad.back() ^= 1;
		writeBytes(root / "ini.big", bad);
		Win32BIGFileSystem fsys;
		RetailMountReport r = MountRetailArchives(fsys, { install }, RetailMountOptions());
		CHECK_FALSE(r.ok);
		REQUIRE(r.errors.size() == 1);
		CHECK(r.errors[0].find("md5 mismatch for INI.big") != std::string::npos);
	}
}

TEST_CASE("anylang retail: the installs in ROTWK_INSTALL / BFME2_INSTALL check, mount and read their game text")
{
	const char *rotwkRoot = std::getenv("ROTWK_INSTALL");
	const char *bfme2Root = std::getenv("BFME2_INSTALL");
	if (!rotwkRoot || !*rotwkRoot || !bfme2Root || !*bfme2Root)
	{
		retailtest::printSkip("anylang retail");
		return;
	}
	const std::string language = DetectRetailLanguageOfInstall(rotwkRoot);
	CHECK_FALSE(language.empty());
	CHECK(DetectRetailLanguageOfInstall(bfme2Root) == language);
	MESSAGE("install language: " << language);

	const std::vector<InstallLocator::InstallCheck> checks = InstallLocator::checkPair(rotwkRoot, bfme2Root);
	REQUIRE(checks.size() == 2);
	for (const InstallLocator::InstallCheck &c : checks)
	{
		CHECK_MESSAGE(c.ok, (c.errors.empty() ? std::string() : c.errors[0]));
		CHECK(c.language == language);
	}

	retailtest::Mount *mount = retailtest::pureMount();
	REQUIRE(mount);
	REQUIRE_MESSAGE(mount->error.empty(), mount->error);
	const std::string lower = AsciiStringUtil::lowered(language);
	size_t languageArchives = 0;
	for (const std::string &a : mount->archives)
		if (AsciiStringUtil::lowered(a).find(":lang/" + lower) != std::string::npos)
			++languageArchives;
	CHECK(languageArchives == 6);

	GameTextTable text;
	std::string error;
	REQUIRE_MESSAGE(text.loadRetail(*mount->fs, &error), error);
	CHECK(text.size() > 1000); // tolerance: the label count differs per language and patch; a real table has thousands
}
