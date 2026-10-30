// OpenBFME unit tests: Ennorath 1.9.8, Gondor (FactionMen) - the mounted profile against the mod's own data. GPL-3.0.
//
// Needs ROTWK_INSTALL, BFME2_INSTALL and OPENBFME_ENNORATH_INSTALL (the mod's install folder, e.g. workspace/mods-en/Ennorath/install); SKIP otherwise.
// The expected rows are read from the mod's own CommandSet / CommandButton / Science / SpecialPower text by tools/mod/ennorath_docs.py
// (docs/ennorath/FactionMen.md), never from the port. The mod stays outside the repository (PLAN rule 6).

#include "doctest.h"
#include "RetailTestMount.h"

#include "Common/AsciiString.h"
#include "Common/ModMount.h"
#include "Common/PlayerTemplate.h"
#include "Common/RetailArchivePolicy.h"
#include "Common/Science.h"
#include "Common/SpecialPower.h"
#include "Common/Thing/ThingFactory.h"
#include "Common/Thing/ThingTemplate.h"
#include "GameClient/ControlBarCommands.h"
#include "GameClient/InGameHud.h"
#include "GameEngineDevice/Win32Device/Common/Win32BIGFileSystem.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/Object/RetailObjectWorld.h"
#include "GameLogic/SpellStores.h"

#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace
{

std::string envVar(const char *name)
{
	const char *v = std::getenv(name);
	return v ? std::string(v) : std::string();
}

std::unique_ptr<Win32BIGFileSystem> retailMount(std::string *error, RetailMountReport *reportOut)
{
	std::vector<RetailInstall> installs(2);
	installs[0].label = "rotwk";
	installs[0].root = envVar("ROTWK_INSTALL");
	installs[1].label = "bfme2";
	installs[1].root = envVar("BFME2_INSTALL");
	if (!RetailArchivePolicy::loadBuiltin("rotwk-201", installs[0].policy, error) || !RetailArchivePolicy::loadBuiltin("bfme2-106", installs[1].policy, error))
	{
		return nullptr;
	}
	auto fsys = std::make_unique<Win32BIGFileSystem>();
	RetailMountOptions options;
	options.md5CachePath = (fs::temp_directory_path() / "openbfme-ennorath-men-retail-md5.tsv").u8string();
	*reportOut = MountRetailArchives(*fsys, installs, options);
	if (!reportOut->ok)
	{
		for (const std::string &e : reportOut->errors)
		{
			*error += e + "\n";
		}
		return nullptr;
	}
	return fsys;
}

// the mounted Ennorath profile and its object world, as the corpus gate loads them
struct EnnorathWorld
{
	RetailMountReport retail;
	std::unique_ptr<Win32BIGFileSystem> pure;
	Win32BIGFileSystem merged;
	std::unique_ptr<RetailObjectWorld> world;
	std::string error;
	bool load(const std::string &install)
	{
		pure = retailMount(&error, &retail);
		if (!pure)
		{
			return false;
		}
		ModMountOptions options;
		options.rotwkInstallRoot = envVar("ROTWK_INSTALL");
		options.md5CachePath = (fs::temp_directory_path() / "openbfme-ennorath-men-md5.tsv").u8string();
		const ModMountReport mount = MountInstallOverlay(merged, retail, install, options);
		for (const std::string &e : mount.errors)
		{
			error += e + "\n";
		}
		if (!mount.ok)
		{
			return false;
		}
		world = std::make_unique<RetailObjectWorld>(merged);
		if (!world->load(&error))
		{
			return false;
		}
		if (!world->report().errors.empty())
		{
			error = "Ennorath INI load has errors";
			return false;
		}
		return true;
	}
};

} // namespace

// A purchase button's science is not the science its name suggests (slot 2 buys SCIENCE_RebuildMen, slot 3 SCIENCE_HealMen, ...), and the cast set starts at slot 3
// with the powers in another order: the port must follow the data, not the names.
TEST_CASE("ENNORATH Gondor: the mounted spellbook resolves all twelve purchase sciences to the right cast powers")
{
	const std::string install = envVar("OPENBFME_ENNORATH_INSTALL");
	if (envVar("ROTWK_INSTALL").empty() || envVar("BFME2_INSTALL").empty() || install.empty())
	{
		retailtest::printSkip("Ennorath Gondor spellbook (set ROTWK_INSTALL, BFME2_INSTALL and OPENBFME_ENNORATH_INSTALL)");
		return;
	}
	EnnorathWorld loaded;
	REQUIRE_MESSAGE(loaded.load(install), loaded.error);
	RetailObjectWorld &world = *loaded.world;
	auto context = world.enterContext();

	const PlayerTemplate *faction = world.playerTemplates().findPlayerTemplate("FactionMen");
	REQUIRE(faction);
	CHECK(faction->m_playableSide);
	CHECK(faction->m_startingBuilding == "MenFortress");
	CHECK(faction->m_startingUnit[0] == "MenPorter");
	CHECK(faction->m_startingUnit[1] == "MenPorter");
	CHECK(faction->m_spellBookMp == "GondorSpellBook");
	CHECK(faction->m_purchaseScienceCommandSetMP == "GondorSpellStoreCommandSet");
	const std::vector<std::string> heroes = { "CreateAHeroDummy", "GondorGandalf_mod", "GondorBoromir_mod", "GondorAragorn_mod",
		"GondorFaramir_mod", "GondorBeregond", "RohanPippin_mod", "GondorImrahil" };
	CHECK(faction->m_buildableHeroesMP == heroes);
	CHECK(faction->m_buildableRingHeroesMP == std::vector<std::string>{ "RingHeroDummy" });
	for (const std::string &hero : heroes)
	{
		CAPTURE(hero);
		CHECK(world.things().findTemplate(hero));
	}
	CHECK(world.things().findTemplate("RingHeroDummy"));

	struct Entry
	{
		int buySlot;
		const char *buy, *science;
		int cost;
		std::vector<std::vector<const char *>> prereq; // OR of AND groups, as the mod's PrerequisiteSciences reads
		int castSlot;
		const char *cast, *power, *tag, *module;
	};
	const Entry entries[] = {
		{ 1, "Command_PurchaseSpellHornGondors", "SCIENCE_HornMen", 5, { {"SCIENCE_GOOD"}, {"SCIENCE_MEN"}, {"SCIENCE_ARNOR"} }, 4, "Command_SpellBookHornGondors", "SpellBookRallyingCall", "ModuleTag_RallyingCall", "SpecialPowerModule" },
		{ 2, "Command_PurchaseSpellHealMod", "SCIENCE_RebuildMen", 5, { {"SCIENCE_GOOD"}, {"SCIENCE_MEN"}, {"SCIENCE_ARNOR"} }, 3, "Command_SpellBookHealMod", "SpellBookHeal", "ModuleTag_Heal", "PlayerHealSpecialPower" },
		{ 3, "Command_PurchaseSpellBaumeisterGondors", "SCIENCE_HealMen", 5, { {"SCIENCE_GOOD"}, {"SCIENCE_MEN"}, {"SCIENCE_ARNOR"} }, 5, "Command_SpellBookBaumeisterGondors", "SpellBookRebuild", "ModuleTag_Rebuild", "PlayerHealSpecialPower" },
		{ 4, "Command_PurchaseSpellLehenAllies", "SCIENCE_LehenAllies", 10, { {"SCIENCE_GOOD"}, {"SCIENCE_MEN", "SCIENCE_HornMen"} }, 9, "Command_SpellBookLehenAllies", "SpellBookLehenAllies", "ModuleTag_LehenAllies", "OCLSpecialPower" },
		{ 5, "Command_PurchaseSpellWaelderIthiliens", "SCIENCE_ElvenWoodMen", 9, { {"SCIENCE_GOOD"}, {"SCIENCE_MEN", "SCIENCE_HornMen"}, {"SCIENCE_MEN", "SCIENCE_RebuildMen"} }, 7, "Command_SpellBookWaelderIthiliens", "SpellBookWlderIthilliens", "ModuleTag_WlderIthilliens", "ElvenWoodSpecialPower" },
		{ 6, "Command_PurchaseSpellGandalftheWhite", "SCIENCE_EinsamerTurmMen", 10, { {"SCIENCE_GOOD"}, {"SCIENCE_MEN", "SCIENCE_RebuildMen"}, {"SCIENCE_MEN", "SCIENCE_HealMen"} }, 6, "Command_SpellBookGandalftheWhite", "SpellBookGandalftheWhite", "ModuleTag_SpellBookGandalfWhite", "PlayerUpgradeSpecialPower" },
		{ 7, "Command_PurchaseSpellLoneTowerMen_Edain", "SCIENCE_GandalftheWhite", 9, { {"SCIENCE_GOOD"}, {"SCIENCE_MEN", "SCIENCE_HealMen"} }, 8, "Command_SpellBookLoneTower_Edain", "SpellBookSpawnLoneTower_Edain", "ModuleTag_SpawnLoneTowerGondorEdain", "OCLSpecialPower" },
		{ 8, "Command_PurchaseSpellDieGraueSchaar", "SCIENCE_GraueSchaar", 15, { {"SCIENCE_GOOD"}, {"SCIENCE_MEN", "SCIENCE_ElvenWoodMen"}, {"SCIENCE_MEN", "SCIENCE_LehenAllies"} }, 10, "Command_SpellBookDieGraueSchaar", "SpellBookDieGraueSchaar", "ModuleTag_SummonDieGraueScharGondor", "OCLSpecialPower" },
		{ 9, "Command_PurchaseSpellCloudBreak", "SCIENCE_CloudBreakMen", 13, { {"SCIENCE_GOOD"}, {"SCIENCE_MEN", "SCIENCE_EinsamerTurmMen"}, {"SCIENCE_MEN", "SCIENCE_ElvenWoodMen"} }, 11, "Command_SpellBookCloudBreak", "SpellBookCloudBreak", "ModuleTag_CloudBreak", "CloudBreakSpecialPower" },
		{ 10, "Command_PurchaseSpellAdler", "SCIENCE_Adler", 18, { {"SCIENCE_GOOD"}, {"SCIENCE_MEN", "SCIENCE_EinsamerTurmMen"}, {"SCIENCE_MEN", "SCIENCE_GandalftheWhite"} }, 12, "Command_SpellBookAdler", "SpellBookAdler", "ModuleTag_Adler", "OCLSpecialPower" },
		{ 11, "Command_PurchaseSpellArmyoftheDead", "SCIENCE_ArmyoftheDead", 25, { {"SCIENCE_GOOD"}, {"SCIENCE_MEN", "SCIENCE_CloudBreakMen"}, {"SCIENCE_MEN", "SCIENCE_GraueSchaar"} }, 13, "Command_SpellBookArmyoftheDead", "SpellBookArmyoftheDead", "ModuleTag_SummonDead", "OCLSpecialPower" },
		{ 12, "Command_PurchaseSpellSummonRohanGondor", "SCIENCE_RohanAntwortet", 25, { {"SCIENCE_GOOD"}, {"SCIENCE_MEN", "SCIENCE_Adler"}, {"SCIENCE_MEN", "SCIENCE_CloudBreakMen"} }, 14, "Command_SpellBookSummonRohanGondor", "SpellBookSummonRohanGondor", "ModuleTag_SummonRohanGondor", "OCLSpecialPower" },
	};
	const CommandSet *buy = world.commands().findCommandSet("GondorSpellStoreCommandSet");
	const CommandSet *cast = world.commands().findCommandSet("GondorSpellBookCommandSet");
	REQUIRE(buy);
	REQUIRE(cast);
	const ThingTemplate *book = world.things().findTemplate("GondorSpellBook");
	REQUIRE(book);
	auto &sciences = world.spellStores().sciences();
	CHECK(cast->getCommandButton(0) == nullptr); // the cast set starts at slot 3
	CHECK(cast->getCommandButton(1) == nullptr);
	for (const Entry &e : entries)
	{
		CAPTURE(e.buySlot);
		const CommandButton *purchase = buy->getCommandButton(e.buySlot - 1);
		const CommandButton *ability = cast->getCommandButton(e.castSlot - 1);
		REQUIRE(purchase);
		REQUIRE(ability);
		CHECK(purchase->m_name == e.buy);
		REQUIRE(purchase->m_science.size() == 1);
		CHECK(purchase->m_science[0] == e.science);
		CHECK(ability->m_name == e.cast);
		CHECK(ability->m_specialPowerName == e.power);
		const ScienceType science = sciences.getScienceFromInternalName(e.science);
		REQUIRE(science != SCIENCE_INVALID);
		CHECK(sciences.getSciencePurchaseCost(science, true) == e.cost);
		const ScienceInfo *info = sciences.findScienceInfo(science);
		REQUIRE(info);
		REQUIRE(info->m_prereqSciences.size() == e.prereq.size());
		for (size_t group = 0; group < e.prereq.size(); ++group)
		{
			REQUIRE(info->m_prereqSciences[group].size() == e.prereq[group].size());
			for (size_t i = 0; i < e.prereq[group].size(); ++i)
			{
				CHECK(info->m_prereqSciences[group][i] == sciences.getScienceFromInternalName(e.prereq[group][i]));
			}
		}
		const SpecialPowerTemplate *power = world.spellStores().specialPowers().findSpecialPowerTemplate(e.power);
		REQUIRE(power);
		REQUIRE(power->getRequiredSciences().size() == 1);
		CHECK(power->getRequiredSciences()[0] == science);
		const ThingTemplate::Nugget *module = book->behaviorModules().find(e.tag);
		REQUIRE_MESSAGE(module, e.tag);
		CHECK(module->name == e.module);
		const auto *moduleData = dynamic_cast<const SpecialPowerModuleData *>(module->data.get());
		REQUIRE_MESSAGE(moduleData, e.tag);
		CHECK(moduleData->m_specialPowerTemplateName == e.power);
		CHECK(moduleData->m_specialPowerTemplate == power);
	}
}

// Every command set, command button and object the graph walk of tools/mod/ennorath_docs.py reached from FactionMen: the engine's parse must hold the same slot -> button
// map, the same Object / Upgrade / SpecialPower / Science of each button, and a template for each object. OPENBFME_ENNORATH_EXPECTED names the tab separated file the tool
// wrote (--expected, under workspace/: it is game data and is never committed). Data only: no behaviour is judged here.
TEST_CASE("ENNORATH Gondor: every reached command set, button and object is in the engine's parse as the mod's data has it")
{
	const std::string install = envVar("OPENBFME_ENNORATH_INSTALL");
	const std::string expectedPath = envVar("OPENBFME_ENNORATH_EXPECTED");
	if (envVar("ROTWK_INSTALL").empty() || envVar("BFME2_INSTALL").empty() || install.empty() || expectedPath.empty())
	{
		retailtest::printSkip("Ennorath Gondor graph (set ROTWK_INSTALL, BFME2_INSTALL, OPENBFME_ENNORATH_INSTALL and OPENBFME_ENNORATH_EXPECTED)");
		return;
	}
	std::ifstream in(expectedPath);
	REQUIRE_MESSAGE(in.good(), expectedPath);
	EnnorathWorld loaded;
	REQUIRE_MESSAGE(loaded.load(install), loaded.error);
	RetailObjectWorld &world = *loaded.world;
	auto context = world.enterContext();
	auto toLower = [](std::string v) {
		for (char &c : v)
		{
			c = (char)std::tolower((unsigned char)c);
		}
		return v;
	};
	int sets = 0, buttons = 0, objects = 0;
	std::string line;
	while (std::getline(in, line))
	{
		if (line.empty())
		{
			continue;
		}
		std::vector<std::string> f;
		std::stringstream ss(line);
		for (std::string part; std::getline(ss, part, '\t');)
		{
			f.push_back(part);
		}
		while (f.size() < 6)
		{
			f.push_back("");
		}
		CAPTURE(line);
		if (f[0] == "set")
		{
			const CommandSet *set = world.commands().findCommandSet(f[1]);
			REQUIRE_MESSAGE(set, f[1]);
			const CommandButton *b = set->getCommandButton(std::stoi(f[2]) - 1);
			REQUIRE_MESSAGE(b, line);
			CHECK_MESSAGE(toLower(b->m_name) == toLower(f[3]), line);
			++sets;
		}
		else if (f[0] == "button")
		{
			const CommandButton *b = world.commands().findCommandButton(f[1]);
			REQUIRE_MESSAGE(b, f[1]);
			CHECK_MESSAGE(toLower(b->m_objectName) == toLower(f[2]), line);
			CHECK_MESSAGE(toLower(b->m_upgradeName) == toLower(f[3]), line);
			CHECK_MESSAGE(toLower(b->m_specialPowerName) == toLower(f[4]), line);
			std::string science;
			for (const std::string &sc : b->m_science)
			{
				science += (science.empty() ? "" : ",") + sc;
			}
			CHECK_MESSAGE(toLower(science) == toLower(f[5]), line);
			++buttons;
		}
		else if (f[0] == "object")
		{
			CHECK_MESSAGE(world.things().findTemplate(f[1]), line);
			++objects;
		}
	}
	MESSAGE("ENNORATH Gondor graph: " << sets << " set slots, " << buttons << " buttons, " << objects << " objects compared");
	CHECK(sets > 1000);
	CHECK(buttons > 300);
	CHECK(objects > 300);
}

// Ennorath ships its own Palantir.apt and SpellStore.apt, narrow on the stage for retail's stretch: the HUD must not narrow them again; retail's own stay corrected.
TEST_CASE("ENNORATH: the mod's own HUD movies keep retail's stage mapping")
{
	const std::string install = envVar("OPENBFME_ENNORATH_INSTALL");
	if (envVar("ROTWK_INSTALL").empty() || envVar("BFME2_INSTALL").empty() || install.empty())
	{
		retailtest::printSkip("Ennorath HUD movies (set ROTWK_INSTALL, BFME2_INSTALL and OPENBFME_ENNORATH_INSTALL)");
		return;
	}
	EnnorathWorld loaded;
	REQUIRE_MESSAGE(loaded.load(install), loaded.error);
	MESSAGE("palantir.apt from " << loaded.merged.getArchiveFilenameForFile("palantir.apt") << ", spellstore.apt from " << loaded.merged.getArchiveFilenameForFile("spellstore.apt"));
	CHECK(InGameHud::movieIsModAuthored(loaded.merged, "palantir"));
	CHECK(InGameHud::movieIsModAuthored(loaded.merged, "spellstore"));
	CHECK(InGameHud::movieIsModAuthored(*loaded.pure, "palantir") == false);
	CHECK(InGameHud::movieIsModAuthored(*loaded.pure, "spellstore") == false);
}
