// OpenBFME. PORT-309 (issue #701): re-verification of ExperienceLevel's level-up tint (ShowLevelUpTint, LevelUpTintColor, LevelUpTintPreColorTime,
// LevelUpTintSustainColorTime, LevelUpTintPostColorTime; S-7196 / PORT-44 already in main).
//
// Independent verification in RotWK 2.01 game.dat (md5 daed3668006cd90f01c34e5a7da1901f):
//   1. Parse table RW 0xC11BF0 (22 FieldParse entries, 16 bytes each):
//        - ShowLevelUpTint             row RW 0xC11C90 -> parser RW 0x42E558 (INI::parseBool)      -> offset +0xD8
//        - LevelUpTintColor            row RW 0xC11CA0 -> parser RW 0x42EF99 (INI::parseRGBColor)  -> offset +0xDC (3 floats)
//        - LevelUpTintPreColorTime     row RW 0xC11CB0 -> parser RW 0x42EC5E (INI::parseInt)       -> offset +0xE8
//        - LevelUpTintPostColorTime    row RW 0xC11CC0 -> parser RW 0x42EC5E (INI::parseInt)       -> offset +0xEC
//        - LevelUpTintSustainColorTime row RW 0xC11CD0 -> parser RW 0x42EC5E (INI::parseInt)       -> offset +0xF0
//        - LevelUpTintFrequency        row RW 0xC11CE0 -> parser RW 0x42ED00 (INI::parseReal)      -> offset +0xF4
//        - LevelUpTintAmplitude        row RW 0xC11CF0 -> parser RW 0x42ED00 (INI::parseReal)      -> offset +0xF8
//   2. Only instructions touching +0xD8..+0xF8 on ExperienceLevelTemplate in .text:
//        - Constructor RW 0x689A4E (0x689ADB..0x689B01): zeroes +0xD8..+0xF8;
//        - Copy constructor RW 0x689FEA (0x689FEA..0x68A05E, used by map.ini override copy RW 0x68A37F): copies +0xD8..+0xF8.
//   3. All 92 references to TheExperienceLevelSystem (0xDE4704), all functions in 0x688D00..0x68AE00, ExperienceTracker (0x79D000..0x79DE00:
//      checkLevels RW 0x79D141, applyLevel RW 0x79D76D, onLevelGranted RW 0x79D804, addExperiencePoints RW 0x79D833, gainExpForLevel RW 0x79DA0A,
//      onLevelReached RW 0x79DA9D), DelayedExperienceLevelGrantSystem::grant (RW 0x821319, LevelUpFx/OCL RW 0x8211EC, LEVELED 210 RW 0x8213DB),
//      ExperienceLevelCreate (RW 0x8BD350), and all callers of Drawable::colorFlash (RW 0x675749), Drawable::colorTint (RW 0x6757BB),
//      Drawable::setCustomTintEnvelope (RW 0x671A73), TintEnvelope::play (RW 0x674818) never read +0xD8..+0xF8.
//   4. DONOR BFME2 1.06 (Open-BFME-2/Code/GameEngine/Source/GameLogic/System/ExperienceLevelSystem.cpp:542-548): identical 7 FieldParse entries
//      (+0xD8..+0xF8) and no reader in Code/.
// Verdict: already documented in main as S-7196 / PORT-44 (GameLogic/ExperienceLevels.cpp:159-165, 473-488; test_port_44.cpp).

#include "doctest.h"

#include "CombatTestUtil.h"
#include "IniTestUtil.h"

#include "Common/NumericState.h"
#include "GameClient/ClientEvents.h"
#include "GameClient/FXList.h"
#include "GameLogic/ExperienceLevels.h"
#include "GameLogic/ExperienceWorld.h"
#include "GameLogic/Object/ExperienceTracker.h"
#include "GameLogic/Object/Object.h"

#include <algorithm>
#include <string>
#include <vector>

using namespace combattest;

namespace
{
const char kPort309FxList[] = "FXList Port309LevelUpFX\nEnd\n";

const char kPort309TintedLevel[] =
	"ExperienceLevel Port309Level2\n"
	"  TargetNames = Swordsman\n"
	"  RequiredExperience = 40\n"
	"  ExperienceAward = 5\n"
	"  Rank = 2\n"
	"  LevelUpFx = FX:Port309LevelUpFX\n"
	"  ShowLevelUpTint = Yes\n"
	"  LevelUpTintColor = R:255 G:128 B:64\n"
	"  LevelUpTintPreColorTime = 500\n"
	"  LevelUpTintSustainColorTime = 500\n"
	"  LevelUpTintPostColorTime = 3000\n"
	"End\n";

const char kPort309UntintedOverride[] =
	"ExperienceLevel Port309Level2\n"
	"  ShowLevelUpTint = No\n"
	"End\n";

struct Port309Store
{
	initest::Fixture fx;
	FXListStore fxLists;
	ExperienceLevelSystem levels;
	ExperienceLevelSystem *savedLevels = TheExperienceLevelSystem;
	FXListStore *savedFx = TheFXListStore;

	Port309Store()
	{
		levels.registerBlocks(fx.env.blocks);
		fx.env.blocks.registerBlock("FXList", [](INI *ini) { ParseFXListDefinitionGlobal(ini); });
		TheExperienceLevelSystem = &levels;
		TheFXListStore = &fxLists;
	}

	~Port309Store()
	{
		TheExperienceLevelSystem = savedLevels;
		TheFXListStore = savedFx;
	}

	std::string load(const std::string &text, INILoadType type = INI_LOAD_OVERWRITE)
	{
		return initest::loadError(fx.env, "port309.ini", text, type);
	}
};

std::vector<std::string> drainClientEvents(ClientEventRecorder &events)
{
	std::vector<std::string> out;
	for (const ClientEvent &e : events.take())
	{
		out.push_back(std::to_string(static_cast<int>(e.kind)) + "/" + std::to_string(e.bit) + "/" + (e.on ? "on" : "off"));
	}
	return out;
}
} // namespace

TEST_CASE("PORT-309: ExperienceLevel tint fields parse into RW 0xC11BF0 (+0xD8..+0xF8), copy on override (RW 0x689FEA), and emit S-7196")
{
	Port309Store s;
	REQUIRE(s.load(kPort309FxList).empty());
	REQUIRE(s.load(kPort309TintedLevel).empty());

	const ExperienceLevelTemplate *level = s.levels.findLevel("Port309Level2");
	REQUIRE(level);
	CHECK(level->m_showLevelUpTint);
	CHECK(level->m_levelUpTintColor[0] == NumericState::pc24Mul(255.0f, 1.0f / 255.0f));
	CHECK(level->m_levelUpTintColor[1] == NumericState::pc24Mul(128.0f, 1.0f / 255.0f));
	CHECK(level->m_levelUpTintColor[2] == NumericState::pc24Mul(64.0f, 1.0f / 255.0f));
	CHECK(level->m_levelUpTintPreColorTime == 500);
	CHECK(level->m_levelUpTintSustainColorTime == 500);
	CHECK(level->m_levelUpTintPostColorTime == 3000);

	REQUIRE(s.load(kPort309UntintedOverride, INI_LOAD_CREATE_OVERRIDES).empty());
	const ExperienceLevelTemplate *over = s.levels.findLevel("Port309Level2");
	REQUIRE(over);
	REQUIRE(over != level);
	CHECK_FALSE(over->m_showLevelUpTint);
	CHECK(over->m_levelUpTintColor[1] == level->m_levelUpTintColor[1]);
	CHECK(over->m_levelUpTintPreColorTime == 500);
	CHECK(over->m_levelUpTintSustainColorTime == 500);
	CHECK(over->m_levelUpTintPostColorTime == 3000);

	bool hasS7196 = false;
	for (const std::string &line : s.levels.stopLines())
	{
		if (line.rfind("[S-7196]", 0) == 0)
		{
			hasS7196 = true;
			CHECK(line.find("1 of 1 levels say ShowLevelUpTint = Yes") != std::string::npos);
		}
	}
	CHECK(hasS7196);
}

TEST_CASE("PORT-309: ExperienceTracker::gainExpForLevel (RW 0x79DA0A) with ShowLevelUpTint Yes vs No produces identical client events (no tint)")
{
	CombatWorld w;
	ClientEventRecorder events(*w.logic);
	w.logic->setClientHooks(&events);
	struct Unhook
	{
		GameLogic &logic;
		~Unhook() { logic.setClientHooks(nullptr); }
	} unhook{ *w.logic };

	Object *a = w.unit("Swordsman", 'A', 300.0f, 300.0f);
	Object *b = w.unit("Swordsman", 'A', 420.0f, 300.0f);
	w.frames(12); // RW 0x8213BC: feedback effects require frame >= 10

	ExperienceLevelSystem levels;
	FXListStore fxLists;
	ExperienceLevelSystem *savedLevels = TheExperienceLevelSystem;
	FXListStore *savedFx = TheFXListStore;
	struct Restore
	{
		ExperienceLevelSystem *levelSlot;
		FXListStore *fxSlot;
		~Restore()
		{
			TheExperienceLevelSystem = levelSlot;
			TheFXListStore = fxSlot;
		}
	} restore{ savedLevels, savedFx };

	levels.registerBlocks(w.w.fx.env.blocks);
	w.w.fx.env.blocks.registerBlock("FXList", [](INI *ini) { ParseFXListDefinitionGlobal(ini); });
	TheExperienceLevelSystem = &levels;
	TheFXListStore = &fxLists;
	REQUIRE(w.w.load(kPort309FxList).empty());
	REQUIRE(w.w.load(kPort309TintedLevel).empty());
	REQUIRE(levels.findLevel("Port309Level2")->m_showLevelUpTint);

	ExperienceTracker *xpA = a->getExperienceTracker();
	REQUIRE(xpA);
	events.take();
	REQUIRE(xpA->gainExpForLevel(1, true, false)); // RW 0x79DA0A -> 0x79D833 -> 0x79D68D -> 0x79D141 -> 0x79DA9D
	CHECK(w.logic->experience().pendingGrants() == 1);
	w.frames(1); // RW 0x821567 -> 0x821319
	CHECK(xpA->getRank() == 2);
	CHECK(xpA->getLevelName() == "Port309Level2");
	CHECK(a->testModelCondition(210)); // LEVELED (RW 0x8213DB)
	CHECK(w.logic->experience().counters().levelUpFxEvents == 1);
	const std::vector<std::string> eventsA = drainClientEvents(events);
	const std::string leveledOn = std::to_string(static_cast<int>(ClientEvent::MODEL_CONDITION)) + "/210/on";
	CHECK(std::find(eventsA.begin(), eventsA.end(), leveledOn) != eventsA.end());
	CHECK(eventsA.size() == 2);

	REQUIRE(w.w.load(kPort309UntintedOverride, INI_LOAD_CREATE_OVERRIDES).empty());
	REQUIRE_FALSE(levels.findLevel("Port309Level2")->m_showLevelUpTint);

	ExperienceTracker *xpB = b->getExperienceTracker();
	REQUIRE(xpB);
	events.take();
	REQUIRE(xpB->gainExpForLevel(1, true, false));
	CHECK(w.logic->experience().pendingGrants() == 1);
	w.frames(1);
	CHECK(xpB->getRank() == 2);
	CHECK(xpB->getLevelName() == "Port309Level2");
	CHECK(b->testModelCondition(210));
	CHECK(w.logic->experience().counters().levelUpFxEvents == 2);
	const std::vector<std::string> eventsB = drainClientEvents(events);
	CHECK(eventsB == eventsA);
}
