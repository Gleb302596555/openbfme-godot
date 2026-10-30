// OpenBFME unit tests: lane MODS-1, -mod over the real RotWK 2.01 archives, and the mod corpus gate. GPL-3.0.
//
// Both need ROTWK_INSTALL and BFME2_INSTALL (SKIP otherwise). The data-only mod is generated from the player's own files at runtime (nothing
// retail is committed). The corpus gate mounts every mod found in OPENBFME_MOD_CORPUS (a folder of mod folders and mod archives on the
// developer's machine, never committed: PLAN rule 6) over retail and loads the object world through the retail parsers.

#include "doctest.h"
#include "BigTestUtil.h"
#include "RetailTestMount.h"
#include "PathfindTestUtil.h"

#include "Common/AsciiString.h"
#include "Common/MiniJson.h"
#include "Common/ModEngineFeatures.h"
#include "Common/ModMount.h"
#include "Common/RetailArchivePolicy.h"
#include "GameEngineDevice/Win32Device/Common/Win32BIGFileSystem.h"
#include "GameLogic/Object/RetailObjectWorld.h"
#include "GameLogic/SpellStores.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/PlayerScience.h"
#include "Common/Team.h"
#include "GameLogic/GameLogicDispatch.h"
#include "GameLogic/GlobalWeatherSystem.h"
#include "GameLogic/AI/AIWorld.h"
#include "GameLogic/Module/ActiveBody.h"
#include "GameLogic/Module/HeroModules.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/Object/AttributeModifierPool.h"
#include "GameLogic/SpellCommands.h"
#include "Common/Thing/ThingTemplate.h"
#include "Common/PlayerTemplate.h"
#include "Common/Science.h"
#include "Common/SpecialPower.h"
#include "GameClient/ControlBarCommands.h"
#include "GameClient/SpellBookUI.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/WeaponStores.h"
#include "Common/Thing/ThingFactory.h"
#include "GameClient/FXPlayback.h"
#include "GameClient/GUI/Skirmish/IniSkirmishSetupSource.h"
#include "GameClient/MapCreationHooks.h"
#include "GameClient/MapObjectDrawables.h"
#include "GameLogic/GameLogic.h"
#include "Libraries/Source/Apt/AptLoad.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <algorithm>
#include <memory>
#include <random>
#include <regex>

namespace fs = std::filesystem;

namespace
{

std::string env(const char *name)
{
	const char *v = std::getenv(name);
	return v ? std::string(v) : std::string();
}

struct TempDir
{
	fs::path path;
	TempDir()
	{
		std::mt19937_64 rng((unsigned long long)std::chrono::steady_clock::now().time_since_epoch().count());
		path = fs::temp_directory_path() / ("openbfme-mods1r-" + std::to_string(rng()));
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

// a fresh pure 2.01 mount (a mod changes the tree, so the process-wide retailtest::pureMount is not used); the md5 cache keeps repeat runs fast
std::unique_ptr<Win32BIGFileSystem> freshRetailMount(std::string *error, RetailMountReport *reportOut = nullptr)
{
	std::vector<RetailInstall> installs(2);
	installs[0].label = "rotwk";
	installs[0].root = env("ROTWK_INSTALL");
	installs[1].label = "bfme2";
	installs[1].root = env("BFME2_INSTALL");
	if (!RetailArchivePolicy::loadBuiltin("rotwk-201", installs[0].policy, error) || !RetailArchivePolicy::loadBuiltin("bfme2-106", installs[1].policy, error))
	{
		return nullptr;
	}
	auto fsys = std::make_unique<Win32BIGFileSystem>();
	RetailMountOptions options;
	options.md5CachePath = (fs::temp_directory_path() / "openbfme-mods1-retail-md5.tsv").u8string();
	const RetailMountReport report = MountRetailArchives(*fsys, installs, options);
	if (reportOut)
	{
		*reportOut = report;
	}
	if (!report.ok)
	{
		for (const std::string &e : report.errors)
		{
			*error += e + "\n";
		}
		return nullptr;
	}
	return fsys;
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

} // namespace

TEST_CASE("MODS-1 retail: a generated data-only mod over the 2.01 archives: a replaced INI file, a loose and an archived new object")
{
	if (env("ROTWK_INSTALL").empty() || env("BFME2_INSTALL").empty())
	{
		retailtest::printSkip("MODS-1 retail");
		return;
	}
	std::string error;
	std::unique_ptr<Win32BIGFileSystem> fsys = freshRetailMount(&error);
	REQUIRE_MESSAGE(fsys, error);

	// the mod: retail's weapon.ini with LegolasBow's AttackRange changed (a replaced file, stored with another case), one object as a loose
	// file and one in the mod folder's archive (new files that only the directory listing finds)
	std::vector<std::uint8_t> weaponIni;
	REQUIRE_MESSAGE(fsys->readFile("data\\ini\\weapon.ini", weaponIni, &error), error);
	std::string text(weaponIni.begin(), weaponIni.end());
	const std::regex attackRange(R"((Weapon LegolasBow\r?\n[\s\S]*?)AttackRange[ \t]*=[^\r\n]*)");
	std::smatch m;
	REQUIRE(std::regex_search(text, m, attackRange));
	const std::string modded = m.prefix().str() + m[1].str() + "AttackRange = 123.0" + m.suffix().str();
	REQUIRE(modded != text);

	TempDir tmp;
	const fs::path mod = tmp.path / "Mods1TestMod";
	writeFile(mod / "Data" / "INI" / "Weapon.ini", modded);
	writeFile(mod / "data" / "ini" / "object" / "mods1" / "mods1_loose.ini", "Object MODS1_LooseObject\n  KindOf = INERT\nEnd\n");
	const std::vector<std::uint8_t> big =
		bigtest::makeBig("BIGF", { { "data\\ini\\object\\mods1\\mods1_archive.ini", "Object MODS1_ArchiveObject\n  KindOf = INERT\nEnd\n" } });
	writeFile(mod / "Mods1Test.big", std::string(big.begin(), big.end()));

	ModOperand op;
	REQUIRE_MESSAGE(ParseModArgument(mod.u8string(), std::string(), op, &error), error);
	ModMountOptions options;
	options.rotwkInstallRoot = env("ROTWK_INSTALL"); // the player's install: no loose file there may shadow an archive (PLAN rule 7)
	const ModMountReport report = MountRetailMod(*fsys, op, options);
	REQUIRE_MESSAGE(report.ok, joined(report.errors));
	CHECK(report.archives.size() == 1);
	CHECK(report.loose.files.size() == 3);
	CHECK(fsys->sourcesOf("data\\ini\\weapon.ini").front() == "mod-folder:Data\\INI\\Weapon.ini");

	RetailObjectWorld world(*fsys);
	REQUIRE_MESSAGE(world.load(&error), error);
	for (const auto &e : world.report().errors)
	{
		MESSAGE(e.file << ": " << e.message);
	}
	CHECK(world.report().errors.empty()); // as without the mod (test_weapon_data.cpp "WeaponSet retail")
	CHECK(world.things().findTemplate("MODS1_LooseObject") != nullptr);
	CHECK(world.things().findTemplate("MODS1_ArchiveObject") != nullptr);
	CHECK(world.things().findTemplate("GondorFighter") != nullptr); // the rest of retail is still there
	const WeaponTemplate *bow = world.weaponStores().weapons().findWeaponTemplate("LegolasBow");
	REQUIRE(bow != nullptr);
	CHECK(bow->m_attackRange == 123.0f);
	CHECK(world.weaponStores().weapons().templates().size() == 747); // test_weapon_data.cpp: the retail count, the file replaced not added
}

TEST_CASE("MODS-1 mod corpus: every mod in OPENBFME_MOD_CORPUS mounts over 2.01 and its object world loads through the retail parsers")
{
	const std::string corpus = env("OPENBFME_MOD_CORPUS");
	if (env("ROTWK_INSTALL").empty() || env("BFME2_INSTALL").empty() || corpus.empty())
	{
		retailtest::printSkip("MODS-1 mod corpus (OPENBFME_MOD_CORPUS unset)");
		return;
	}
	std::vector<fs::path> mods;
	std::error_code ec;
	for (fs::directory_iterator it(fs::u8path(corpus), ec), end; !ec && it != end; it.increment(ec))
	{
		if (it->is_directory() || AsciiStringUtil::lowered(it->path().extension().u8string()) == ".big")
		{
			mods.push_back(it->path());
		}
	}
	REQUIRE_MESSAGE(!ec, "cannot list " << corpus << ": " << ec.message());
	REQUIRE_MESSAGE(!mods.empty(), "OPENBFME_MOD_CORPUS holds no mod folder or .big: " << corpus);
	std::sort(mods.begin(), mods.end());
	for (const fs::path &p : mods)
	{
		CAPTURE(p.u8string());
		std::string error;
		RetailMountReport retail;
		std::unique_ptr<Win32BIGFileSystem> fsys = freshRetailMount(&error, &retail);
		REQUIRE_MESSAGE(fsys, error);
		ModMountOptions options;
		options.rotwkInstallRoot = env("ROTWK_INSTALL");
		options.md5CachePath = (fs::temp_directory_path() / "openbfme-mods1-corpus-md5.tsv").u8string();
		// an All In One package laid out by tools/mod/aio_package.py (openbfme-aio.json schema 2): the install/ folders of its stack in priority
		// order are one install-folder mod (lane MODS-2), the stack's mod/ folder a -mod operand; any other folder or .big is a -mod operand
		const bool aio = fs::exists(p / "openbfme-aio.json");
		std::vector<std::string> installs;
		fs::path modPart = p;
		std::string engineEdits;
		std::vector<std::string> engineMd5s;
		if (aio)
		{
			std::ifstream manifest(p / "openbfme-aio.json", std::ios::binary);
			const std::string text((std::istreambuf_iterator<char>(manifest)), std::istreambuf_iterator<char>());
			JsonValue root;
			REQUIRE_MESSAGE(JsonValue::parse(text, root, &error), error);
			const JsonValue *stack = root.get("stack");
			if (!stack || !stack->isArray())
			{
				continue; // a dependency laid out for another package (dependency_only)
			}
			modPart.clear();
			for (const JsonValue &member : stack->array)
			{
				const fs::path folder = p.parent_path() / fs::u8path(member.get("folder")->string);
				if (fs::is_directory(folder / "install"))
				{
					installs.push_back((folder / "install").u8string());
				}
				if (modPart.empty() && fs::is_directory(folder / "mod"))
				{
					modPart = folder / "mod";
				}
			}
			if (const JsonValue *edits = root.get("engine_edits"))
			{
				for (const JsonValue &e : edits->array)
				{
					engineEdits += (engineEdits.empty() ? "" : "; ") + e.string;
					const size_t at = e.string.rfind(" md5 "); // "<package>: <file> md5 <hex>" (tools/mod/aio_package.py)
					if (at != std::string::npos)
					{
						engineMd5s.push_back(e.string.substr(at + 5));
					}
				}
			}
		}
		const bool installMod = !installs.empty();
		ModMountReport report;
		report.ok = true;
		if (installMod)
		{
			auto merged = std::make_unique<Win32BIGFileSystem>();
			report = MountInstallOverlay(*merged, retail, installs, options);
			if (report.ok)
			{
				fsys = std::move(merged);
			}
		}
		if (report.ok && !modPart.empty())
		{
			ModOperand op;
			REQUIRE_MESSAGE(ParseModArgument(modPart.u8string(), std::string(), op, &error), error);
			const ModMountReport modReport = MountRetailMod(*fsys, op, options);
			if (!installMod)
			{
				report = modReport;
			}
			else if (!modReport.ok)
			{
				report.ok = false;
				report.errors = modReport.errors;
			}
		}
		if (aio)
		{
			MESSAGE("MODCORPUS " << p.filename().u8string() << " engine edits (not loaded): " << (engineEdits.empty() ? "none" : engineEdits));
		}
		// lane MODS-3: the stack's modified game binaries turn on their ModEngineFeatures profiles (the launcher's --mod-engine); every binary of the
		// corpus must be a known one
		struct EngineGuard
		{
			~EngineGuard() { ModEngineFeatures::reset(); }
		} engineGuard;
		CHECK_MESSAGE(ModEngineFeatures::activate(engineMd5s, &error), error);
		for (const std::string &f : ModEngineFeatures::identityFeatures())
		{
			MESSAGE("MODCORPUS   " << f);
		}
		if (!report.ok)
		{
			continue;
		}
		RetailObjectWorld world(*fsys);
		const bool loaded = world.load(&error);
		CHECK_MESSAGE(loaded, error);
		const size_t errors = loaded ? world.report().errors.size() : 0;
		// the report a T1 run pins once the corpus is on the gate hosts (counts only, no mod bytes): MODS-2 turns the errors into fixes
		// std::string: doctest streams a bare const char * as its address
		MESSAGE("MODCORPUS " << p.filename().u8string() << std::string(installMod ? " (install-folder mod" : " (-mod")
							 << std::string(installMod && !modPart.empty() ? " + -mod)" : ")") << ": archives " << report.archives.size() << ", loose files " << report.loose.files.size()
							 << ", INI errors " << errors << ", load " << (loaded ? world.loadSeconds() : 0.0) << " s");
		if (loaded)
		{
			// the rest of what the game reads at start (GodotDevice GameWorld::setup, in its order): a mod that crashes or fails here never reaches
			// the menu, even with a clean object world
			MESSAGE("MODCORPUS   factions " << world.setupPlayerTemplates().playableSideIndices().size());
			MapObjectOptions mapOptions;
			CHECK_MESSAGE(MapObjectGameData::load(*fsys, mapOptions, &error), error);
			CHECK_MESSAGE(MapObjectGameData::loadPlayerTemplates(*fsys, mapOptions, &error), error);
			CHECK_MESSAGE(MapCreationHooks::load(*fsys, mapOptions.creationScripts, &error), error);
			std::vector<MapCacheEntry> mapCache;
			CHECK_MESSAGE(IniSkirmishSetupSource::loadMapCache(*fsys, mapCache, &error), error);
			MESSAGE("MODCORPUS   maps " << mapCache.size());
			GameLogicSettings settings;
			CHECK_MESSAGE(GameLogicSettingsLoader::load(*fsys, settings, &error), error);
			REQUIRE(world.report().macrosBeforeFXTaken);
			FXPlayback fx(*fsys, RandomAlgorithm::RotWK_GameDat_LCG);
			const std::vector<std::string> fxErrors = fx.loadRetailData(world.report().macrosBeforeFX);
			CHECK_MESSAGE(fxErrors.empty(), joined(fxErrors));
			// the menus and the HUD: every Apt movie of the mount loads and its imports resolve (the shell boots MainMenu from them)
			AptArchiveFileSource aptSource(*fsys);
			AptLoader apt(aptSource);
			std::vector<std::string> aptErrors;
			const std::vector<std::string> movies = aptSource.listMovies();
			for (const std::string &name : movies)
			{
				std::shared_ptr<const AptFile> movie = apt.loadMovie(name, &error);
				std::vector<AptResolvedImport> imports;
				if (!movie || !apt.resolveImports(*movie, imports, &error))
				{
					aptErrors.push_back(name + ": " + error);
				}
			}
			MESSAGE("MODCORPUS   apt movies " << movies.size());
			CHECK_MESSAGE(aptErrors.empty(), joined(aptErrors));
			for (const std::string &n : world.loadNotes())
			{
				MESSAGE("MODCORPUS   note: " << n);
			}
			size_t shown = 0;
			for (const auto &e : world.report().errors)
			{
				if (shown++ == 20)
				{
					break;
				}
				MESSAGE("MODCORPUS   " << e.file << ": " << e.message);
			}
		}
	}
}

TEST_CASE("MODS-2 retail: the pinned 2.01 mount reports S-5302 (the BFME2 apt archives retail never mounts)")
{
	if (env("ROTWK_INSTALL").empty() || env("BFME2_INSTALL").empty())
	{
		retailtest::printSkip("MODS-2 retail S-5302");
		return;
	}
	std::string error;
	RetailMountReport report;
	REQUIRE_MESSAGE(freshRetailMount(&error, &report), error);
	CHECK(report.stops == std::vector<std::string>{ RetailBaseAptStopLine() });
}

// Ennorath 1.9.8's purchase and cast menus use different slot orders for the last two powers. These expected identifiers come from the mod's
// own CommandSet, CommandButton and SpecialPower definitions, not from the port. The local mod remains outside the repository.
TEST_CASE("ENNORATH Arnor: the mounted spellbook resolves all twelve purchase sciences to the right cast powers")
{
	const std::string install = env("OPENBFME_ENNORATH_INSTALL");
	if (env("ROTWK_INSTALL").empty() || env("BFME2_INSTALL").empty() || install.empty())
	{
		retailtest::printSkip("Ennorath Arnor spellbook (set ROTWK_INSTALL, BFME2_INSTALL and OPENBFME_ENNORATH_INSTALL)");
		return;
	}
	std::string error;
	RetailMountReport retail;
	auto pure = freshRetailMount(&error, &retail);
	REQUIRE_MESSAGE(pure != nullptr, error);
	Win32BIGFileSystem merged;
	ModMountOptions options;
	options.rotwkInstallRoot = env("ROTWK_INSTALL");
	options.md5CachePath = (fs::temp_directory_path() / "openbfme-ennorath-arnor-md5.tsv").u8string();
	const ModMountReport mount = MountInstallOverlay(merged, retail, install, options);
	REQUIRE_MESSAGE(mount.ok, joined(mount.errors));
	RetailObjectWorld world(merged);
	REQUIRE_MESSAGE(world.load(&error), error);
	REQUIRE_MESSAGE(world.report().errors.empty(), "Ennorath INI load has errors");
	auto context = world.enterContext();
	const PlayerTemplate *faction = world.playerTemplates().findPlayerTemplate("FactionArnor");
	REQUIRE(faction);
	CHECK(faction->m_playableSide);
	CHECK(faction->m_startingBuilding == "ArnorFortress");
	CHECK(faction->m_spellBookMp == "ArnorSpellBook");
	CHECK(faction->m_purchaseScienceCommandSetMP == "ArnorSpellStoreCommandSet");

	struct Entry { const char *buy; const char *science; const char *cast; const char *power; const char *tag; const char *module; };
	const Entry entries[] = {
		{ "Command_PurchaseSpellHornOfArnor", "SCIENCE_HornOfArnor", "Command_SpellBookHornOfArnor", "SpellBookHornOfArnor", "ModuleTag_HornOfArnor", "SpecialPowerModule" },
		{ "Command_PurchaseSpellHealingOfTheOld", "SCIENCE_HobbitHouse", "Command_SpellBookHealingOfTheOld", "SpellBookHealingOfTheOld", "ModuleTag_HealingOfTheOld", "PlayerHealSpecialPower" },
		{ "Command_PurchaseSpellArnorHobbitHouse", "SCIENCE_HealingOfTheOld", "Command_SpellBookArnorHobbitHouse", "SpellBookArnorHobbitHouse", "ModuleTag_ArnorHobbitHouse", "OCLSpecialPower" },
		{ "Command_PurchaseSpellGiftOfTheKingdom", "SCIENCE_GiftOfTheKingdom", "Command_SpellBookGiftOfTheKingdom", "SpellBookGiftOfTheKingdom", "ModuleTag_SpellBookMerciForHealing", "PlayerUpgradeSpecialPower" },
		{ "Command_PurchaseSpellWaelderCardolans", "SCIENCE_ElvenWoodArnor", "Command_SpellBookWaelderCardolans", "SpellBookWlderCardolans", "ModuleTag_CardolanHills", "ElvenWoodSpecialPower" },
		{ "Command_PurchaseSpellArnorSummonHobbits", "SCIENCE_ArnorHobbitAllies", "Command_SpellBookArnorSummonHobbits", "SpellBookArnorSummonHobbits", "ModuleTag_ArnorSummonHobbits", "OCLSpecialPower" },
		{ "Command_PurchaseSpellNumenorMastery", "SCIENCE_NumenorMastery", "Command_SpellBookNumenorHealBuilding", "SpellBookNumenorHealBuilding", "ModuleTag_NumenorHealBuilding", "OCLSpecialPower" },
		{ "Command_PurchaseSpellCirdanHelp", "SCIENCE_SwordsOfThePast", "Command_SpellBookCirdanHelp", "SpellBookSchwerterderWesternis", "ModuleTag_SummonArnorCirdan", "OCLSpecialPower" },
		{ "Command_PurchaseSpellStarOfTheNorth", "SCIENCE_StarOfTheNorth", "Command_SpellBookStarOfTheNorth", "SpellBookStarOfTheNorth", "ModuleTag_StarOfTheNorth", "CloudBreakSpecialPower" },
		{ "Command_PurchaseSpellGlorfindelHelp", "SCIENCE_AdlerArnor", "Command_SpellBookGlorfindelHelp", "SpellBookAdlerArnor", "ModuleTag_SummonArnorGlorfindel", "OCLSpecialPower" },
		{ "Command_PurchaseSpellArnorLastHope", "SCIENCE_LastAllianceArnor", "Command_SpellBookArnorLastHope", "SpellBookSBLetzteAllianzArnor", "ModuleTag_SummonLastHopeArmy", "OCLSpecialPower" },
		{ "Command_PurchaseSpellFornostHammer", "SCIENCE_RunBitchWatersComin", "Command_SpellBookFornostHammer", "SpellBookRunBitchWatersComin", "ModuleTag_FornostHammer", "OCLSpecialPower" },
	};
	const CommandSet *buy = world.commands().findCommandSet("ArnorSpellStoreCommandSet");
	const CommandSet *cast = world.commands().findCommandSet("ArnorSpellBookCommandSet");
	REQUIRE(buy);
	REQUIRE(cast);
	const ThingTemplate *book = world.things().findTemplate("ArnorSpellBook");
	REQUIRE(book);
	auto &sciences = world.spellStores().sciences();
	for (int i = 0; i < 12; ++i)
	{
		CAPTURE(i);
		const Entry &e = entries[i];
		const CommandButton *purchase = buy->getCommandButton(i);
		// Cast slots 11 and 12 are reversed in the mod's actual CommandSet.
		const CommandButton *ability = cast->getCommandButton(i == 10 ? 11 : i == 11 ? 10 : i);
		REQUIRE(purchase);
		REQUIRE(ability);
		CHECK(purchase->m_name == e.buy);
		REQUIRE(purchase->m_science.size() == 1);
		CHECK(purchase->m_science[0] == e.science);
		CHECK(ability->m_name == e.cast);
		CHECK(ability->m_specialPowerName == e.power);
		const SpecialPowerTemplate *power = world.spellStores().specialPowers().findSpecialPowerTemplate(e.power);
		REQUIRE(power);
		const ScienceType science = sciences.getScienceFromInternalName(e.science);
		REQUIRE(science != SCIENCE_INVALID);
		REQUIRE(power->getRequiredSciences().size() == 1);
		CHECK(power->getRequiredSciences()[0] == science);
		const ThingTemplate::Nugget *module = book->behaviorModules().find(e.tag);
		REQUIRE_MESSAGE(module, e.tag);
		CHECK(module->name == e.module);
		const auto *moduleData = dynamic_cast<const SpecialPowerModuleData *>(module->data.get());
		REQUIRE_MESSAGE(moduleData, e.tag);
		CHECK(moduleData->m_specialPowerTemplateName == e.power);
		CHECK(moduleData->m_specialPowerTemplate == power);
		if (i == 1)
		{
			const auto *healing = dynamic_cast<const PlayerHealSpecialPowerModuleData *>(moduleData);
			REQUIRE(healing);
			CHECK(healing->m_healAmount == doctest::Approx(0.4f)); // source: Ennorath systemgood.inc
		}
	}
}

// The values and filter below come from Ennorath 1.9.8 systemgood.inc, ModuleTag_HealingOfTheOld:
// 40% of max health, the unit-scan radius, allied INFANTRY/CAVALRY/MONSTER/SHIP/DOZER and HealOCL.
TEST_CASE("ENNORATH Arnor: powers are purchased and cast through the message path")
{
	const std::string install = env("OPENBFME_ENNORATH_INSTALL");
	if (env("ROTWK_INSTALL").empty() || env("BFME2_INSTALL").empty() || install.empty())
	{
		retailtest::printSkip("Ennorath Arnor healing (set ROTWK_INSTALL, BFME2_INSTALL and OPENBFME_ENNORATH_INSTALL)");
		return;
	}
	std::string error;
	RetailMountReport retail;
	auto pure = freshRetailMount(&error, &retail);
	REQUIRE_MESSAGE(pure != nullptr, error);
	Win32BIGFileSystem merged;
	ModMountOptions options;
	options.rotwkInstallRoot = env("ROTWK_INSTALL");
	options.md5CachePath = (fs::temp_directory_path() / "openbfme-ennorath-arnor-md5.tsv").u8string();
	const ModMountReport mount = MountInstallOverlay(merged, retail, install, options);
	REQUIRE_MESSAGE(mount.ok, joined(mount.errors));
	RetailObjectWorld world(merged);
	REQUIRE_MESSAGE(world.load(&error), error);
	REQUIRE(world.report().errors.empty());
	auto context = world.enterContext();
	TeamFactory teams;
	PlayerList players(world.nameKeys(), world.playerTemplates(), teams);
	SkirmishSetup setup;
	for (int i = 0; i < 2; ++i)
	{
		SkirmishPlayer p;
		p.name = "arnor-heal-" + std::to_string(i);
		p.faction = i == 0 ? "FactionArnor" : "FactionMordor";
		p.human = i == 0;
		p.team = i;
		p.startIndex = i + 1;
		setup.players.push_back(p);
	}
	REQUIRE(players.setupSkirmish(setup).empty());
	Player *owner = players.findPlayerWithName("arnor-heal-0");
	Player *enemy = players.findPlayerWithName("arnor-heal-1");
	REQUIRE(owner);
	REQUIRE(enemy);
	GameLogic logic(world.things(), world.modules(), players, RandomAlgorithm::ZH_CarryChain);
	REQUIRE_MESSAGE(GameLogicSettingsLoader::load(merged, logic.settings(), &error), error);
	// Summoned hordes need the ordinary pathfinder. Keep it alive until after logic.reset().
	pathtest::SyntheticTerrain terrain(800, 900);
	AIWorldConfig aiConfig;
	REQUIRE_MESSAGE(AIWorldConfigLoader::load(merged, aiConfig, &error), error);
	AIWorld ai(logic, aiConfig, world.iniMacros());
	ai.attach();
	ai.newMap(terrain);
	GameLogicDispatch dispatch(logic);
	SpellCommands commands;
	commands.registerHandlers(dispatch);
	SpecialPowerModules::installScienceHooks(logic);
	auto place = [&](const char *name, Player *player, float x) -> Object * {
		const ThingTemplate *tt = world.things().findTemplate(name);
		REQUIRE_MESSAGE(tt, name);
		Object *o = logic.newObject(tt, player->getDefaultTeam(), ObjectStatusMaskType{});
		REQUIRE(o);
		Coord3D pos{ x, 1000.0f, 0.0f };
		o->setPosition(&pos);
		return o;
	};
	Object *citadel = place("ArnorFortressCitadel", owner, 500.0f);
	citadel->friend_onBuildComplete();
	Object *book = SpecialPowerModules::createSpellBook(logic, *owner);
	REQUIRE(book);
	const SpecialPowerTemplate *power = world.spellStores().specialPowers().findSpecialPowerTemplate("SpellBookHealingOfTheOld");
	REQUIRE(power);
	auto *module = dynamic_cast<PlayerHealSpecialPower *>(SpecialPowerModules::findModule(*book, power));
	REQUIRE(module);
	REQUIRE(module->requirementsMet());
	Object *injured = place("ArnorFighterNew", owner, 1000.0f);
	Object *outside = place("ArnorFighterNew", owner, 2000.0f);
	Object *opponent = place("ArnorFighterNew", enemy, 1020.0f);
	for (Object *o : { injured, outside, opponent })
	{
		auto *body = dynamic_cast<ActiveBody *>(o->getBodyModule());
		REQUIRE(body);
		body->internalChangeHealth(-body->getMaxHealth() * 0.8f);
	}
	const float maxHealth = injured->getBodyModule()->getMaxHealth();
	const float before = injured->getBodyModule()->getHealth();
	const float outsideBefore = outside->getBodyModule()->getHealth();
	const float opponentBefore = opponent->getBodyModule()->getHealth();
	const Coord3D where{ 1000.0f, 1000.0f, 0.0f };
	auto cast = [&]() {
		GameMessage m(MSG_DO_SPECIAL_POWER_AT_LOCATION, owner->getPlayerIndex());
		m.appendIntegerArgument((int)power->getID());
		m.appendLocationArgument(where);
		m.appendObjectIDArgument(INVALID_ID);
		m.appendIntegerArgument(0);
		m.appendObjectIDArgument(book->getID());
		dispatch.dispatch(m);
	};
	cast();
	CHECK(injured->getBodyModule()->getHealth() == before); // no science: refused
	const ScienceType science = world.spellStores().sciences().getScienceFromInternalName("SCIENCE_HobbitHouse");
	REQUIRE(science != SCIENCE_INVALID);
	owner->science().addSciencePurchasePoints(100);
	GameMessage purchase(MSG_PURCHASE_SCIENCE, owner->getPlayerIndex());
	purchase.appendIntegerArgument(owner->getPlayerIndex());
	purchase.appendIntegerArgument(science);
	dispatch.dispatch(purchase);
	REQUIRE(owner->hasScience(science));
	REQUIRE(module->isReady());
	cast();
	CHECK(injured->getBodyModule()->getHealth() == doctest::Approx(before + maxHealth * 0.4f));
	CHECK(outside->getBodyModule()->getHealth() == outsideBefore);
	CHECK(opponent->getBodyModule()->getHealth() == opponentBefore);
	CHECK_FALSE(module->isReady());
	CHECK(module->getReadyFrame() == logic.getFrame() + power->getReloadTime());
	const float after = injured->getBodyModule()->getHealth();
	cast();
	CHECK(injured->getBodyModule()->getHealth() == after);

	// The Horn's module grants SpellBookWarChant to friendly infantry within 100 units (systemgood.inc).
	const auto *horn = world.spellStores().specialPowers().findSpecialPowerTemplate("SpellBookHornOfArnor");
	REQUIRE(horn);
	const ScienceType hornScience = world.spellStores().sciences().getScienceFromInternalName("SCIENCE_HornOfArnor");
	REQUIRE(hornScience != SCIENCE_INVALID);
	GameMessage buyHorn(MSG_PURCHASE_SCIENCE, owner->getPlayerIndex());
	buyHorn.appendIntegerArgument(owner->getPlayerIndex());
	buyHorn.appendIntegerArgument(hornScience);
	dispatch.dispatch(buyHorn);
	REQUIRE(owner->hasScience(hornScience));
	SpecialPowerModuleInterface *hornModule = SpecialPowerModules::findModule(*book, horn);
	REQUIRE(hornModule);
	REQUIRE(hornModule->isReady());
	power = horn;
	cast();
	auto *allyPool = dynamic_cast<AttributeModifierPool *>(injured->findModule("AttributeModifierPoolUpdate"));
	auto *farPool = dynamic_cast<AttributeModifierPool *>(outside->findModule("AttributeModifierPoolUpdate"));
	auto *enemyPool = dynamic_cast<AttributeModifierPool *>(opponent->findModule("AttributeModifierPoolUpdate"));
	REQUIRE(allyPool);
	REQUIRE(farPool);
	REQUIRE(enemyPool);
	CHECK(allyPool->hasList("SpellBookWarChant"));
	CHECK_FALSE(farPool->hasList("SpellBookWarChant"));
	CHECK_FALSE(enemyPool->hasList("SpellBookWarChant"));
	CHECK_FALSE(hornModule->isReady());
	CHECK(hornModule->getReadyFrame() == logic.getFrame() + horn->getReloadTime());

	// The Hobbit summon's OCL creates one ArnorHobbitBowEgg at the target (objectcreationlist.inc). The egg's later horde spawn is a separate check.
	const auto *summon = world.spellStores().specialPowers().findSpecialPowerTemplate("SpellBookArnorSummonHobbits");
	REQUIRE(summon);
	const ScienceType hobbits = world.spellStores().sciences().getScienceFromInternalName("SCIENCE_ArnorHobbitAllies");
	REQUIRE(hobbits != SCIENCE_INVALID);
	GameMessage buyHobbits(MSG_PURCHASE_SCIENCE, owner->getPlayerIndex());
	buyHobbits.appendIntegerArgument(owner->getPlayerIndex());
	buyHobbits.appendIntegerArgument(hobbits);
	dispatch.dispatch(buyHobbits);
	REQUIRE(owner->hasScience(hobbits));
	SpecialPowerModuleInterface *summonModule = SpecialPowerModules::findModule(*book, summon);
	REQUIRE(summonModule);
	REQUIRE(summonModule->isReady());
	auto eggs = [&]() {
		int count = 0;
		for (Object *o = logic.getFirstObject(); o; o = o->getNextObject())
		{
			if (!o->isDestroyed() && o->getControllingPlayer() == owner && o->getTemplate()->getName() == "ArnorHobbitBowEgg") ++count;
		}
		return count;
	};
	const int eggsBefore = eggs();
	power = summon;
	cast();
	CHECK(eggs() == eggsBefore + 1);
	CHECK_FALSE(summonModule->isReady());
	const int eggsAfter = eggs();
	cast();
	CHECK(eggs() == eggsAfter);
	// The egg's lifetime is zero; its SlowDeathBehavior's 2000 ms FINAL OCL creates five summoned hobbit archer hordes.
	for (int i = 0; i < 20; ++i) logic.runLogicFrame();
	int summonedHordes = 0;
	for (Object *o = logic.getFirstObject(); o; o = o->getNextObject())
	{
		if (!o->isDestroyed() && o->getControllingPlayer() == owner && o->getTemplate()->getName() == "ArnorHobbitBowHorde_Summoned") ++summonedHordes;
	}
	CHECK(summonedHordes == 5);

	// Follow the research tree normally. The OCL names and their first created objects are the mod's own definitions;
	// later effects of each object are separate checks, not implied by an egg being created.
	owner->science().addSciencePurchasePoints(1000);
	auto buyScience = [&](const char *name) {
		const ScienceType st = world.spellStores().sciences().getScienceFromInternalName(name);
		REQUIRE_MESSAGE(st != SCIENCE_INVALID, name);
		GameMessage m(MSG_PURCHASE_SCIENCE, owner->getPlayerIndex());
		m.appendIntegerArgument(owner->getPlayerIndex());
		m.appendIntegerArgument(st);
		dispatch.dispatch(m);
		REQUIRE_MESSAGE(owner->hasScience(st), name);
	};
	auto countOwned = [&](const char *name) {
		int n = 0;
		for (Object *o = logic.getFirstObject(); o; o = o->getNextObject())
		{
			if (!o->isDestroyed() && o->getControllingPlayer() == owner && o->getTemplate()->getName() == name) ++n;
		}
		return n;
	};
	auto purchaseAndCastOCL = [&](const char *scienceName, const char *powerName, const char *objectName) {
		CAPTURE(scienceName);
		CAPTURE(powerName);
		CAPTURE(objectName);
		buyScience(scienceName);
		const SpecialPowerTemplate *t = world.spellStores().specialPowers().findSpecialPowerTemplate(powerName);
		REQUIRE(t);
		SpecialPowerModuleInterface *sp = SpecialPowerModules::findModule(*book, t);
		REQUIRE(sp);
		REQUIRE(sp->isReady());
		const int n = countOwned(objectName);
		power = t;
		cast();
		CHECK(countOwned(objectName) == n + 1);
		CHECK_FALSE(sp->isReady());
		cast();
		CHECK(countOwned(objectName) == n + 1);
	};
	purchaseAndCastOCL("SCIENCE_HealingOfTheOld", "SpellBookArnorHobbitHouse", "Arnorhobhouse_Independed");
	purchaseAndCastOCL("SCIENCE_NumenorMastery", "SpellBookNumenorHealBuilding", "StructureHealPing");
	purchaseAndCastOCL("SCIENCE_AdlerArnor", "SpellBookAdlerArnor", "GlorfindelUnitsSummonEgg");
	buyScience("SCIENCE_ElvenWoodArnor"); // the prerequisite for SCIENCE_SwordsOfThePast
	// Cardolan's forest creates a neutral CardolanHill and its stone seeds. The terrain-area effect is explicitly unported (S-921).
	const auto *forest = world.spellStores().specialPowers().findSpecialPowerTemplate("SpellBookWlderCardolans");
	REQUIRE(forest);
	SpecialPowerModuleInterface *forestModule = SpecialPowerModules::findModule(*book, forest);
	REQUIRE(forestModule);
	REQUIRE(forestModule->isReady());
	auto countAll = [&](const char *name) {
		int n = 0;
		for (Object *o = logic.getFirstObject(); o; o = o->getNextObject())
		{
			if (!o->isDestroyed() && o->getTemplate()->getName() == name) ++n;
		}
		return n;
	};
	const int hillsBefore = countAll("CardolanHill");
	const int stonesBefore = countAll("CardolanStone01Seed");
	power = forest;
	cast();
	CHECK(countAll("CardolanHill") == hillsBefore + 1);
	CHECK(countAll("CardolanStone01Seed") == stonesBefore + 1);
	CHECK_FALSE(forestModule->isReady());

	// Star of the North changes weather to CLOUDY and creates ArnorNorthStarObject sunbeams (systemgood.inc).
	// Its fire-grid interactions remain unported (S-921), so these assertions do not qualify the entire power.
	buyScience("SCIENCE_StarOfTheNorth");
	const auto *star = world.spellStores().specialPowers().findSpecialPowerTemplate("SpellBookStarOfTheNorth");
	REQUIRE(star);
	SpecialPowerModuleInterface *starModule = SpecialPowerModules::findModule(*book, star);
	REQUIRE(starModule);
	REQUIRE(starModule->isReady());
	const int starsBefore = countAll("ArnorNorthStarObject");
	GameMessage starCast(MSG_DO_SPECIAL_POWER, owner->getPlayerIndex());
	starCast.appendIntegerArgument((int)star->getID());
	starCast.appendIntegerArgument(0);
	starCast.appendObjectIDArgument(book->getID());
	dispatch.dispatch(starCast);
	CHECK(logic.weather().weather() == 1); // CLOUDY
	CHECK(countAll("ArnorNorthStarObject") > starsBefore);
	CHECK_FALSE(starModule->isReady());
	purchaseAndCastOCL("SCIENCE_SwordsOfThePast", "SpellBookSchwerterderWesternis", "CirdanUnitsSummonEgg");
	purchaseAndCastOCL("SCIENCE_LastAllianceArnor", "SpellBookSBLetzteAllianzArnor", "LastHopeSummonEgg");
	purchaseAndCastOCL("SCIENCE_RunBitchWatersComin", "SpellBookRunBitchWatersComin", "ArnorSummonedFornostHammerEgg");
	// GiftOfTheKingdom is NONPRESSABLE: the purchase button's CommandTrigger must send the extra spell-book message.
	CHECK_FALSE(owner->hasUpgradeComplete("Upgrade_AMerciOfDaKing"));
	const ScienceType giftScience = world.spellStores().sciences().getScienceFromInternalName("SCIENCE_GiftOfTheKingdom");
	REQUIRE(giftScience != SCIENCE_INVALID);
	SpellStoreModel store;
	REQUIRE_MESSAGE(store.open(world.commands(), logic, *owner, &error), error);
	const SpellStoreModel::Button *giftButton = nullptr;
	for (const auto &b : store.buttons()) if (b.science == giftScience) giftButton = &b;
	REQUIRE(giftButton);
	REQUIRE(giftButton->button);
	REQUIRE(giftButton->button->m_commandTrigger.size() == 1);
	CHECK(giftButton->button->m_commandTrigger[0] == "Command_SpellBookGiftOfTheKingdom");
	REQUIRE(store.click(giftButton->index));
	const auto giftMessages = store.close();
	REQUIRE(giftMessages.size() == 2);
	CHECK(giftMessages[0].getType() == MSG_PURCHASE_SCIENCE);
	CHECK(giftMessages[1].getType() == MSG_DO_SPELLBOOK_SPECIAL_POWER);
	for (const GameMessage &m : giftMessages) dispatch.dispatch(m);
	CHECK(owner->hasScience(giftScience));
	CHECK(owner->hasUpgradeComplete("Upgrade_AMerciOfDaKing"));
	// ArnorFighterNew has AttributeModifierUpgrade and AutoHealBehavior triggered by this player upgrade (arnorfighternew.ini).
	CHECK(allyPool->hasList("ArnorBibliothekImladrisUpgrade1"));
	CHECK_FALSE(enemyPool->hasList("ArnorBibliothekImladrisUpgrade1"));
	Object *newSoldier = place("ArnorFighterNew", owner, 4000.0f);
	auto *newPool = dynamic_cast<AttributeModifierPool *>(newSoldier->findModule("AttributeModifierPoolUpdate"));
	REQUIRE(newPool);
	CHECK(newPool->hasList("ArnorBibliothekImladrisUpgrade1"));
	auto *autoHeal = dynamic_cast<AutoHealBehavior *>(newSoldier->findModule("AutoHealBehavior"));
	REQUIRE(autoHeal);
	auto *newBody = dynamic_cast<ActiveBody *>(newSoldier->getBodyModule());
	REQUIRE(newBody);
	newBody->internalChangeHealth(-newBody->getMaxHealth() * 0.5f);
	const float beforeRegen = newBody->getHealth();
	for (int i = 0; i < 100 && newBody->getHealth() <= beforeRegen; ++i) logic.runLogicFrame();
	CHECK(newBody->getHealth() > beforeRegen);
	logic.reset();
}
