// OpenBFME. PORT-44 (issue #148) verdict: RotWK 2.01 parses the ExperienceLevel level-up tint and reads it nowhere.
//
// The seven rows of the ExperienceLevel field table RW 0xC11BF0 (22 rows) store the tint at these offsets of ExperienceLevelTemplate:
//   ShowLevelUpTint +0xD8 (parseBool RW 0x42E558), LevelUpTintColor +0xDC (parseRGBColor RW 0x42EF99, three floats scaled by 1/255), LevelUpTintPreColorTime
//   +0xE8, LevelUpTintPostColorTime +0xEC, LevelUpTintSustainColorTime +0xF0 (parseInt RW 0x42EC5E), LevelUpTintFrequency +0xF4, LevelUpTintAmplitude
//   +0xF8 (parseReal RW 0x42ED00).
// What the binary does with them, read function by function:
//   * the constructor RW 0x689A4E zeroes them: `mov byte [esi+0xD8], bl` RW 0x689AE1, the RGBColor at +0xDC through its constructor (`lea ecx, [esi+0xDC]`
//     RW 0x689ADB), `mov dword [esi+0xE8/0xEC/0xF0], ebx` RW 0x689AE7 / 0x689AED / 0x689AF3 and `movss [esi+0xF4/0xF8], xmm0` RW 0x689AF9 / 0x689B01 with
//     xmm0 = 0.0f;
//   * the copy RW 0x689FEA is the only other code that touches them on an ExperienceLevelTemplate: it reads +0xD8 (RW 0x68A01C), +0xE8 (0x68A028),
//     +0xEC (0x68A034), +0xF0 (0x68A040), +0xF4 (0x68A04C) and +0xF8 (0x68A058) to write the same values into the copy - that is the map.ini override's
//     `new ExperienceLevelTemplate(*final)` (RW 0x68A37F), not a consumer;
//   * a linear sweep of every instruction reachable from the image's call, jump and function-pointer seeds (2.67 million instructions; .text seeds from
//     every E8/E9 rel32 target plus every dword of the other sections pointing into .text) finds no other instruction whose memory operand uses one of
//     +0xD8 / +0xDC / +0xE8 / +0xEC / +0xF0 / +0xF4 / +0xF8 with a register base. The constructor and the copy are the whole access set: nobody reads the
//     tint at any of the seven offsets;
//   * the level-up path itself, read linearly: checkLevels RW 0x79D141 (the next level RW 0x689E3C while RequiredExperience <= trunc(experience)) calls
//     onLevelReached RW 0x79DA9D, which queues the grant (RW 0x8215C2), hands the level's SelectionDecal +0xA4 to the drawable (RW 0x79DACD), stores the
//     level's name and sets the rank (RW 0x79D72A) and counts a hero / unit level-up (RW 0x79DB50 / 0x79DB6B); the grant RW 0x821319 applies
//     AttributeModifiers +0x30 (RW 0x82133D), Upgrades +0x4C (RW 0x821368), the Create-a-Hero rank, the LevelUpFx +0x3C / LevelUpOCL +0x48 (RW 0x8211EC)
//     together with the LEVELED model condition 210 for 3 x LOGICFRAMES_PER_SECOND (RW 0x8213DB), the level's own ModelConditionState +0x58 (RW 0x8213F1),
//     InformUpdateModule +0x100 (RW 0x89A468) and EmotionType +0x104 (RW 0x68F37F). Neither reads +0xD8..+0xF8;
//   * every way a colour tint or flash can start on a drawable is enumerated, and none of its call sites is on the level path: Drawable::colorFlash
//     RW 0x675749 has six callers (Drawable::colorTint RW 0x6757BB, Drawable's own update RW 0x675D7D, the message dispatcher RW 0x81E234 flashing white
//     for 8 frames, a module's RW 0x871B24 / 0x871B6B, RW 0x8A02A3's white flash), Drawable::setCustomTintEnvelope RW 0x671A73 has three (W3DTankDraw
//     RW 0x4D1281, TintDrawableFXNugget RW 0x5E0160, the object tint setter RW 0x6ED1C2) and TintEnvelope::play RW 0x674818 is called only from Drawable's
//     own code (RW 0x6757AA, 0x67597D, 0x675DD1, 0x675E1C, 0x675E86, 0x675ED4, 0x67910E). No caller receives an ExperienceLevelTemplate.
// Verdict: the tint is data RotWK never applies; the port parses and copies it and must not start a tint of its own (stop S-7196). What a level gained does
// show in RotWK, and what these tests pin, is the rest of the grant above.
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
// RW 0xC11BF0's tint rows with values of the test's own (the mod's numbers are not copied into the tree): colour R:204 G:102 B:51, the three times 100 / 600 /
// 100 ms, Frequency 0.5, Amplitude 0.25. LevelUpFx names the FX list RW 0x73A302 validates at parse time.
const char kPort44Levels[] =
	"ExperienceLevel Port44TintedLevel2\n"
	"  TargetNames = Swordsman\n"
	"  RequiredExperience = 50\n"
	"  ExperienceAward = 7\n"
	"  Rank = 2\n"
	"  LevelUpFx = FX:Port44LevelUpFX\n"
	"  ShowLevelUpTint = Yes\n"
	"  LevelUpTintColor = R:204 G:102 B:51\n"
	"  LevelUpTintPreColorTime = 100\n"
	"  LevelUpTintPostColorTime = 600\n"
	"  LevelUpTintSustainColorTime = 100\n"
	"  LevelUpTintFrequency = 0.5\n"
	"  LevelUpTintAmplitude = 0.25\n"
	"End\n";

// the same level as a map.ini override (RW 0x689F96): one level whose two copies differ only in the flag byte
const char kPort44LevelsNoTint[] =
	"ExperienceLevel Port44TintedLevel2\n"
	"  ShowLevelUpTint = No\n"
	"End\n";

// a level that names none of the tint fields: the constructor's values (RW 0x689AE1)
const char kPort44PlainLevel[] =
	"ExperienceLevel Port44PlainLevel2\n"
	"  TargetNames = Port44Nonexistent\n"
	"  RequiredExperience = 90\n"
	"  Rank = 3\n"
	"End\n";

const char kPort44FxList[] = "FXList Port44LevelUpFX\nEnd\n";

// the level store of one fixture, with the two stores the parsers look up in place for the store's lifetime (both are per thread)
struct LevelStore
{
	initest::Fixture fx;
	FXListStore fxLists;
	ExperienceLevelSystem levels;
	ExperienceLevelSystem *savedLevels = TheExperienceLevelSystem;
	FXListStore *savedFx = TheFXListStore;

	LevelStore()
	{
		levels.registerBlocks(fx.env.blocks);
		fx.env.blocks.registerBlock("FXList", [](INI *ini) { ParseFXListDefinitionGlobal(ini); });
		TheExperienceLevelSystem = &levels;
		TheFXListStore = &fxLists;
	}

	~LevelStore()
	{
		TheExperienceLevelSystem = savedLevels;
		TheFXListStore = savedFx;
	}

	std::string load(const std::string &text, INILoadType type = INI_LOAD_OVERWRITE)
	{
		return initest::loadError(fx.env, "port44.ini", text, type);
	}
};

// what the client is told, as the ordered list of "kind/bit/on": the simulation's only channel to a drawable (S-810), so a colour flash would have to appear
// here as an event of its own kind
std::vector<std::string> drained(ClientEventRecorder &events)
{
	std::vector<std::string> out;
	for (const ClientEvent &e : events.take())
	{
		out.push_back(std::to_string((int)e.kind) + "/" + std::to_string(e.bit) + "/" + (e.on ? "on" : "off"));
	}
	return out;
}

const std::string kLeveledOn = std::to_string((int)ClientEvent::MODEL_CONDITION) + "/210/on"; // RW 0x8213DB
} // namespace

TEST_CASE("PORT-44: the level-up tint rows land where RW 0xC11BF0 stores them (+0xD8, +0xDC, +0xE8, +0xEC, +0xF0, +0xF4, +0xF8); a level that names none "
		  "keeps the constructor's zeros, and the map.ini copy carries them")
{
	LevelStore s;
	REQUIRE(s.load(kPort44FxList).empty());
	REQUIRE(s.load(kPort44Levels).empty());
	REQUIRE(s.load(kPort44PlainLevel).empty());

	const ExperienceLevelTemplate *tinted = s.levels.findLevel("Port44TintedLevel2");
	REQUIRE(tinted);
	CHECK(tinted->m_showLevelUpTint);                                                      // +0xD8, parseBool RW 0x42E558
	CHECK(tinted->m_levelUpTintColor[0] == NumericState::pc24Mul(204.0f, 1.0f / 255.0f)); // +0xDC, the parse of RW 0x42EF99 (1/255)
	CHECK(tinted->m_levelUpTintColor[1] == NumericState::pc24Mul(102.0f, 1.0f / 255.0f));
	CHECK(tinted->m_levelUpTintColor[2] == NumericState::pc24Mul(51.0f, 1.0f / 255.0f));
	CHECK(tinted->m_levelUpTintPreColorTime == 100);     // +0xE8, parseInt RW 0x42EC5E
	CHECK(tinted->m_levelUpTintPostColorTime == 600);    // +0xEC
	CHECK(tinted->m_levelUpTintSustainColorTime == 100); // +0xF0
	CHECK(tinted->m_levelUpTintFrequency == 0.5f);       // +0xF4, parseReal RW 0x42ED00
	CHECK(tinted->m_levelUpTintAmplitude == 0.25f);      // +0xF8

	// RW 0x689AE1: the constructor leaves every tint field zeroed and the flag false
	const ExperienceLevelTemplate *plain = s.levels.findLevel("Port44PlainLevel2");
	REQUIRE(plain);
	CHECK_FALSE(plain->m_showLevelUpTint);
	CHECK(plain->m_levelUpTintColor[0] == 0.0f);
	CHECK(plain->m_levelUpTintColor[1] == 0.0f);
	CHECK(plain->m_levelUpTintColor[2] == 0.0f);
	CHECK(plain->m_levelUpTintPreColorTime == 0);
	CHECK(plain->m_levelUpTintPostColorTime == 0);
	CHECK(plain->m_levelUpTintSustainColorTime == 0);
	CHECK(plain->m_levelUpTintFrequency == 0.0f);
	CHECK(plain->m_levelUpTintAmplitude == 0.0f);

	// RW 0x689FEA reads +0xD8 / +0xE8 / +0xEC / +0xF0 / +0xF4 / +0xF8 to copy them into the override and writes them back unchanged
	REQUIRE(s.load(kPort44LevelsNoTint, INI_LOAD_CREATE_OVERRIDES).empty());
	const ExperienceLevelTemplate *final = s.levels.findLevel("Port44TintedLevel2");
	REQUIRE(final);
	CHECK(final != tinted);
	CHECK_FALSE(final->m_showLevelUpTint);
	CHECK(final->m_levelUpTintColor[1] == tinted->m_levelUpTintColor[1]);
	CHECK(final->m_levelUpTintPostColorTime == 600);
	CHECK(final->m_levelUpTintFrequency == 0.5f);
	CHECK(final->m_requiredExperience == 50);
}

TEST_CASE("PORT-44 verdict: a level gained starts no colour flash — ShowLevelUpTint Yes and No give the client the same events; the visible level-up is "
		  "LEVELED for 3 s and the LevelUpFx (RW 0x8213DB, RW 0x8211EC)")
{
	// the two units exist before the level store does, so neither starts at a level (a tracker whose store is in place takes the first level at creation,
	// RW 0x79D77F's applyLevel): the level gained here is the ordinary one, addExperiencePoints -> checkLevels RW 0x79D141 -> onLevelReached RW 0x79DA9D
	CombatWorld w;
	// the client hooks must be in place before the objects are made: the CREATED event binds an object to its client (RW 0x628882, ClientEvents.cpp), and
	// without that binding the model-condition events reach nobody
	ClientEventRecorder events(*w.logic);
	w.logic->setClientHooks(&events);
	struct Unhook
	{
		GameLogic &logic;
		~Unhook() { logic.setClientHooks(nullptr); }
	} unhook{ *w.logic };
	Object *a = w.unit("Swordsman", 'A', 300.0f, 300.0f);
	Object *b = w.unit("Swordsman", 'A', 420.0f, 300.0f);
	w.frames(12); // the logic frame must be past 10 for the grant's feedback effects (RW 0x8213BC)

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
	REQUIRE(w.w.load(kPort44FxList).empty());
	{
		const std::string err = w.w.load(kPort44Levels);
		REQUIRE_MESSAGE(err.empty(), err);
	}
	const ExperienceLevelTemplate *tinted = levels.findLevel("Port44TintedLevel2");
	REQUIRE(tinted);
	REQUIRE(tinted->m_showLevelUpTint);

	ExperienceTracker *xp = a->getExperienceTracker();
	REQUIRE(xp);
	REQUIRE(xp->isAcceptingExperiencePoints());
	events.take(); // the creation events
	unsigned long long granted = w.logic->experience().counters().levelsGranted;
	xp->addExperiencePoints(50.0f, false, false, true); // RequiredExperience 50 (checkLevels RW 0x79D141)
	CHECK(w.logic->experience().pendingGrants() == 1);   // onLevelReached RW 0x79DA9D -> queue RW 0x8215C2
	w.frames(1);                                        // the grant system's update RW 0x821567
	CHECK(w.logic->experience().pendingGrants() == 0);
	CHECK(w.logic->experience().counters().levelsGranted == granted + 1);
	CHECK(xp->getRank() == 2);                                    // setRank RW 0x79D72A with the level's Rank
	CHECK(xp->getLevelName() == "Port44TintedLevel2");            // RW 0x79DAD8
	CHECK(w.logic->experience().counters().levelUpFxEvents == 1); // LevelUpFx RW 0x8213C6 -> RW 0x8211EC
	CHECK(a->testModelCondition(210));                            // LEVELED, 3 x LOGICFRAMES_PER_SECOND (RW 0x8213DB)
	const std::vector<std::string> eventsA = drained(events);
	CHECK(std::find(eventsA.begin(), eventsA.end(), kLeveledOn) != eventsA.end());
	CHECK(eventsA.size() == 2); // MODEL_CONDITION(210) and its MODEL_FLUSH: nothing else reaches the client

	// the same level with ShowLevelUpTint = No as a map.ini override (RW 0x689F96): same data, one byte differs
	{
		const std::string err = w.w.load(kPort44LevelsNoTint, INI_LOAD_CREATE_OVERRIDES);
		REQUIRE_MESSAGE(err.empty(), err);
	}
	REQUIRE_FALSE(levels.findLevel("Port44TintedLevel2")->m_showLevelUpTint);
	CHECK(levels.findLevel("Port44TintedLevel2") != tinted);
	ExperienceTracker *xpb = b->getExperienceTracker();
	REQUIRE(xpb);
	REQUIRE(xpb->isAcceptingExperiencePoints());
	events.take(); // nothing new since a's window: b's creation events were the ones taken above
	granted = w.logic->experience().counters().levelsGranted;
	xpb->addExperiencePoints(50.0f, false, false, true);
	CHECK(w.logic->experience().pendingGrants() == 1);
	w.frames(1);
	CHECK(w.logic->experience().counters().levelsGranted == granted + 1);
	CHECK(xpb->getRank() == 2);
	CHECK(xpb->getLevelName() == "Port44TintedLevel2");
	CHECK(w.logic->experience().counters().levelUpFxEvents == 2); // the override copied the LevelUpFx (RW 0x689FEA)
	CHECK(b->testModelCondition(210));
	const std::vector<std::string> eventsB = drained(events);
	// the verdict: the flag changes nothing the client is told - no flash, no tint, no colour event of any kind
	CHECK(eventsB == eventsA);
	// the tint values were carried by the level all along (parsed, only never applied)
	CHECK(tinted->m_levelUpTintColor[0] == NumericState::pc24Mul(204.0f, 1.0f / 255.0f));
	CHECK(tinted->m_levelUpTintPreColorTime == 100);
	CHECK(tinted->m_levelUpTintFrequency == 0.5f);
}

TEST_CASE("PORT-44: the level store reports the level-up tint as read by nobody (S-7196), with the number of levels that ask for it; S-632 keeps its line")
{
	LevelStore s;
	REQUIRE(s.load(kPort44FxList).empty());
	REQUIRE(s.load(kPort44Levels).empty());
	REQUIRE(s.load(kPort44PlainLevel).empty());

	const std::vector<std::string> stops = s.levels.stopLines();
	std::string tintStop, storeStop;
	for (const std::string &line : stops)
	{
		if (line.rfind("[S-7196]", 0) == 0)
		{
			tintStop = line;
		}
		if (line.rfind("[S-632]", 0) == 0)
		{
			storeStop = line;
		}
	}
	REQUIRE_FALSE(tintStop.empty());
	MESSAGE(tintStop);
	// the count comes from the parsed data: one of the two levels says ShowLevelUpTint = Yes
	CHECK(tintStop.find("1 of 2 levels say ShowLevelUpTint = Yes") != std::string::npos);
	// the seven rows and the addresses the verdict rests on
	CHECK(tintStop.find("ShowLevelUpTint") != std::string::npos);
	CHECK(tintStop.find("LevelUpTintColor") != std::string::npos);
	CHECK(tintStop.find("LevelUpTintPreColorTime") != std::string::npos);
	CHECK(tintStop.find("LevelUpTintPostColorTime") != std::string::npos);
	CHECK(tintStop.find("LevelUpTintSustainColorTime") != std::string::npos);
	CHECK(tintStop.find("LevelUpTintFrequency") != std::string::npos);
	CHECK(tintStop.find("LevelUpTintAmplitude") != std::string::npos);
	CHECK(tintStop.find("0x689AE1") != std::string::npos);
	CHECK(tintStop.find("0x689FEA") != std::string::npos);
	CHECK(tintStop.find("0x8213DB") != std::string::npos);
	// the neighbouring stop (the store itself) is unchanged and still first
	REQUIRE_FALSE(storeStop.empty());
	CHECK(stops[0] == storeStop);
	CHECK(storeStop.find("2 levels, 0 scalar tables") != std::string::npos);
}
