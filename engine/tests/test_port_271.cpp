// OpenBFME. GPL-3.0. PORT-271 (fork issue #638, P-151): several units and heroes selected — the panel, the portrait and the voice
// are the senior hero's. Synthetic INI only: no retail or mod assets.
//
// TARGET FACTS (RotWK 2.01 game.dat, caveat S-001):
//  * ControlBar::populateMultiSelect (RW 0x94475A) walks the multi-selection for a non-IGNORED_IN_GUI HERO whose ExperienceTracker
//    (RW 0x79D12A -> RW 0x689024 -> RW 0x688EEC) answers a rank, keeps the highest rank and, on a tie, the smaller ObjectID, and makes
//    that hero the context of the command set and of AptPalantir::setDrawable (0x94482B -> RW 0x6D4671 -> portrait RW 0x92FE33);
//    ported by PORT-213 / PORT-221.
//  * the voice picker (RW 0x8DEDBB) ranks the same selection by {special, hero, hero level, ordinal, VoicePriority} (rankLess RW 0x8DD663):
//    a hero's level is its ExperienceTracker rank (0x8DF0A3 reads obj + 0x26C through the same three calls, an invalid level leaving
//    the minimum), a higher signed level wins (0x8DD688) and only an equal level falls back to the smaller 1-based ordinal (0x8DD68E).
//    Without the level the two heroes of a selection tie and the ordinal alone decides: the panel showed the senior hero while the
//    junior one answered — the P-151 report.
//  * the selection the two sides read is built by InGameUI::selectDrawable (RW 0x6A4109), which push_fronts. Select-all (RW 0x69C860)
//    and select-matching (RW 0x6A43FE, the E key and the double click) walk the drawables newest-first, so m_selected ends oldest-first
//    (ascending ObjectID) and the voice's ordinal tie-break agrees with populateMultiSelect's smallest-ObjectID tie-break. Here the
//    object list runs oldest-first (GameLogic::getFirstObject), so both walks take it from getLastObject() backwards.

#include "doctest.h"

#include "CombatTestUtil.h"

#include "GameClient/ControlBar.h"
#include "GameClient/ControlBarCommands.h"
#include "GameClient/MessageStream/CommandXlat.h"
#include "GameClient/PalantirCommandUI.h"
#include "GameClient/UnitVoiceResponse.h"
#include "GameLogic/ExperienceLevels.h"
#include "GameLogic/GameMessage.h"
#include "GameLogic/Object/ExperienceTracker.h"
#include "GameLogic/Object/Object.h"

#include <memory>
#include <set>
#include <string>
#include <vector>

using namespace combattest;

namespace
{
// Imladris' two heroes share the level set; the Glorfindel stand-in is a HERO no ExperienceLevel names (0x9447DA skips it, its voice
// level stays the minimum).
const char kPort271Levels[] =
	"ExperienceLevel Port271HeroLevel1\n"
	"  TargetNames = Port271Arwen Port271Elrond\n"
	"  RequiredExperience = 1\n"
	"  ExperienceAward = 10\n"
	"  Rank = 1\n"
	"End\n"
	"ExperienceLevel Port271HeroLevel2\n"
	"  TargetNames = Port271Arwen Port271Elrond\n"
	"  RequiredExperience = 100\n"
	"  ExperienceAward = 20\n"
	"  Rank = 2\n"
	"End\n"
	"ExperienceLevel Port271HeroLevel3\n"
	"  TargetNames = Port271Arwen Port271Elrond\n"
	"  RequiredExperience = 250\n"
	"  ExperienceAward = 30\n"
	"  Rank = 3\n"
	"End\n";

const char kPort271Commands[] =
	"CommandButton Command_Port271_AttackMove\n"
	"  Command = ATTACK_MOVE\n"
	"  Options = OK_FOR_MULTI_SELECT\n"
	"  InPalantir = Yes\n"
	"  ButtonImage = UCCommon_AttackMove\n"
	"End\n"
	"CommandButton Command_Port271_Stop\n"
	"  Command = STOP\n"
	"  Options = OK_FOR_MULTI_SELECT\n"
	"  InPalantir = Yes\n"
	"  ButtonImage = UCCommon_Stop\n"
	"End\n"
	"CommandButton Command_Port271_ArwenOnly\n"
	"  Command = GUARD\n"
	"  InPalantir = Yes\n"
	"  ButtonImage = HIArwen_Flood\n"
	"End\n"
	"CommandButton Command_Port271_ElrondOnly\n"
	"  Command = GUARD\n"
	"  InPalantir = Yes\n"
	"  ButtonImage = HIElrond_Athelas\n"
	"End\n"
	"CommandSet Port271ArwenCommandSet\n"
	"  1 = Command_Port271_ArwenOnly\n"
	"  2 = Command_Port271_AttackMove\n"
	"  3 = Command_Port271_Stop\n"
	"End\n"
	"CommandSet Port271ElrondCommandSet\n"
	"  1 = Command_Port271_ElrondOnly\n"
	"  2 = Command_Port271_AttackMove\n"
	"  3 = Command_Port271_Stop\n"
	"End\n"
	"CommandSet Port271UnitCommandSet\n"
	"  1 = Command_Port271_AttackMove\n"
	"  2 = Command_Port271_Stop\n"
	"End\n";

const char kPort271Objects[] =
	"Object Port271Arwen\n"
	"  KindOf = INFANTRY SELECTABLE HERO\n"
	"  SelectPortrait = HP_Port271Arwen\n"
	"  BuildCost = 1500\n"
	"  VoicePriority = 55\n"
	"  VoiceSelect = Port271ArwenSelect\n"
	"  VoiceMove = Port271ArwenMove\n"
	"  CommandSet = Port271ArwenCommandSet\n"
	"  Geometry = CYLINDER\n GeometryMajorRadius = 5\n GeometryHeight = 10\n"
	"  Body = ActiveBody Body\n MaxHealth = 2000\n End\n"
	"  Behavior = AIUpdateInterface AI\n End\n"
	"  LocomotorSet\n Locomotor = WalkerLoco\n Condition = SET_NORMAL\n Speed = 55\n End\n"
	"End\n"
	"Object Port271Elrond\n"
	"  KindOf = INFANTRY SELECTABLE HERO\n"
	"  SelectPortrait = HP_Port271Elrond\n"
	"  BuildCost = 3500\n"
	"  VoicePriority = 82\n"
	"  VoiceSelect = Port271ElrondSelect\n"
	"  VoiceMove = Port271ElrondMove\n"
	"  CommandSet = Port271ElrondCommandSet\n"
	"  Geometry = CYLINDER\n GeometryMajorRadius = 5\n GeometryHeight = 10\n"
	"  Body = ActiveBody Body\n MaxHealth = 3500\n End\n"
	"  Behavior = AIUpdateInterface AI\n End\n"
	"  LocomotorSet\n Locomotor = WalkerLoco\n Condition = SET_NORMAL\n Speed = 45\n End\n"
	"End\n"
	// a HERO without an ExperienceLevel of its own: no rank for either side
	"Object Port271Glorfindel\n"
	"  KindOf = INFANTRY SELECTABLE HERO\n"
	"  SelectPortrait = HP_Port271Glorfindel\n"
	"  BuildCost = 5000\n"
	"  VoicePriority = 99\n"
	"  VoiceSelect = Port271GlorfindelSelect\n"
	"  VoiceMove = Port271GlorfindelMove\n"
	"  CommandSet = Port271UnitCommandSet\n"
	"  Geometry = CYLINDER\n GeometryMajorRadius = 5\n GeometryHeight = 10\n"
	"  Body = ActiveBody Body\n MaxHealth = 2500\n End\n"
	"  Behavior = AIUpdateInterface AI\n End\n"
	"  LocomotorSet\n Locomotor = WalkerLoco\n Condition = SET_NORMAL\n Speed = 50\n End\n"
	"End\n"
	"Object Port271Swordsman\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  SelectPortrait = UP_Port271Swordsman\n"
	"  VoicePriority = 40\n"
	"  VoiceSelect = Port271SwordsmanSelect\n"
	"  VoiceMove = Port271SwordsmanMove\n"
	"  CommandSet = Port271UnitCommandSet\n"
	"  Geometry = CYLINDER\n GeometryMajorRadius = 5\n GeometryHeight = 10\n"
	"  Body = ActiveBody Body\n MaxHealth = 250\n End\n"
	"  Behavior = AIUpdateInterface AI\n End\n"
	"  LocomotorSet\n Locomotor = WalkerLoco\n Condition = SET_NORMAL\n Speed = 40\n End\n"
	"End\n"
	// the loudest unit of the selection: a VoicePriority above every hero's, which never beats a hero (0x8DD680)
	"Object Port271Archer\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  SelectPortrait = UP_Port271Archer\n"
	"  VoicePriority = 120\n"
	"  VoiceSelect = Port271ArcherSelect\n"
	"  VoiceMove = Port271ArcherMove\n"
	"  CommandSet = Port271UnitCommandSet\n"
	"  Geometry = CYLINDER\n GeometryMajorRadius = 5\n GeometryHeight = 10\n"
	"  Body = ActiveBody Body\n MaxHealth = 180\n End\n"
	"  Behavior = AIUpdateInterface AI\n End\n"
	"  LocomotorSet\n Locomotor = WalkerLoco\n Condition = SET_NORMAL\n Speed = 40\n End\n"
	"End\n";

struct RecordingSink : UnitVoiceResponse::Sink
{
	std::set<std::string> known;
	std::vector<std::string> sounds;
	std::vector<ObjectID> owners;
	bool soundExists(const std::string &name) override { return known.count(name) != 0; }
	void playSound(const std::string &name, ObjectID object, int) override
	{
		sounds.push_back(name);
		owners.push_back(object);
	}
	void reportEva(const std::string &, const Coord3D *) override {}
	void clear()
	{
		sounds.clear();
		owners.clear();
	}
};

struct Port271World
{
	ExperienceLevelSystem levels;
	ExperienceLevelSystem *savedLevels = TheExperienceLevelSystem;
	CommandStore commands;
	CommandStore *savedCommands = TheCommandStore;
	CombatWorld w;
	PinholeView view{ { 200.0f, -200.0f, 400.0f }, { 200.0f, 200.0f, 0.0f }, 1024, 768, 0.7f };
	InGameUI ui;
	MessageStream stream;
	HudContext ctx{ *w.logic, w.ai.get(), view, ui, stream, &commands, {} };
	ControlBar bar{ ctx };
	CommandTranslator translator{ ctx };
	RecordingSink sink;
	std::unique_ptr<UnitVoiceResponse> voice;

	Port271World()
	{
		TheExperienceLevelSystem = &levels;
		TheCommandStore = &commands;
		levels.registerBlocks(w.w.fx.env.blocks);
		w.w.fx.env.blocks.registerBlock("CommandButton", [](INI *ini) { CommandStore::parseCommandButtonDefinitionGlobal(ini); });
		w.w.fx.env.blocks.registerBlock("CommandSet", [](INI *ini) { CommandStore::parseCommandSetDefinitionGlobal(ini); });
		std::string err = w.w.load(kPort271Levels, INI_LOAD_OVERWRITE, "port271-levels.ini");
		REQUIRE_MESSAGE(err.empty(), err);
		err = w.w.load(kPort271Commands, INI_LOAD_OVERWRITE, "port271-commands.ini");
		REQUIRE_MESSAGE(err.empty(), err);
		err = w.w.load(kPort271Objects, INI_LOAD_OVERWRITE, "port271-objects.ini");
		REQUIRE_MESSAGE(err.empty(), err);
		w.players.setLocalPlayer(w.playerOf('A'));
		voice = std::make_unique<UnitVoiceResponse>(*w.logic, sink);
		voice->setLocalPlayerIndex(w.playerOf('A')->getPlayerIndex());
		for (const char *s : { "Port271ArwenSelect", "Port271ArwenMove", "Port271ElrondSelect", "Port271ElrondMove",
				 "Port271GlorfindelSelect", "Port271GlorfindelMove", "Port271SwordsmanSelect", "Port271SwordsmanMove",
				 "Port271ArcherSelect", "Port271ArcherMove" })
		{
			sink.known.insert(s);
		}
	}
	~Port271World()
	{
		TheExperienceLevelSystem = savedLevels;
		TheCommandStore = savedCommands;
	}

	Object *unit(const char *name, float x, float y) { return w.unit(name, 'A', x, y); }

	// `objs` in the order RW's selection holds them: selectDrawable push_fronts, so the list is filled back to front
	void select(const std::vector<Object *> &objs)
	{
		ui.deselectAll(false);
		for (auto it = objs.rbegin(); it != objs.rend(); ++it)
		{
			ui.selectObject((*it)->getID());
		}
		bar.update();
	}

	int rankOf(Object &o) const
	{
		int rank = -1;
		ExperienceTracker *xp = o.getExperienceTracker();
		return xp && xp->currentLevelRank(rank) ? rank : -1;
	}
	void levelTo(Object &o, float experience)
	{
		ExperienceTracker *xp = o.getExperienceTracker();
		REQUIRE(xp != nullptr);
		xp->setExperienceAndLevel(experience, false);
	}
	// what the Palantir shows for the control bar's context
	std::string portrait() { return PalantirCommandUI::portraitFor(ctx, bar.contextObject(), ui.selected()); }
	// the object the voice picker chose, INVALID_ID when it said nothing
	ObjectID say(int msgType, const Coord3D *destination = nullptr)
	{
		sink.clear();
		UnitVoiceResponse::Info info;
		if (destination)
		{
			info.hasPosition = true;
			info.position = *destination;
		}
		const bool said = voice->pickAndPlay(ui.selected(), msgType, destination ? &info : nullptr);
		return said && !sink.owners.empty() ? sink.owners.back() : (ObjectID)INVALID_ID;
	}
	bool palantirHas(const char *buttonName) const
	{
		const CommandButton *want = commands.findCommandButton(buttonName);
		for (const ControlBarButton &b : bar.palantirButtons())
		{
			if (b.button && b.button == want)
			{
				return true;
			}
		}
		return false;
	}
	std::vector<ObjectID> ids(const std::vector<Object *> &objs) const
	{
		std::vector<ObjectID> out;
		for (Object *o : objs)
		{
			out.push_back(o->getID());
		}
		return out;
	}
};
} // namespace

TEST_CASE("PORT-271: units and heroes selected together: the senior hero's panel, portrait and voice are one hero (RW 0x94475A / RW 0x8DEDBB)")
{
	Port271World w;
	Object *arwen = w.unit("Port271Arwen", 200.0f, 200.0f);
	Object *elrond = w.unit("Port271Elrond", 230.0f, 200.0f);
	Object *swordsman = w.unit("Port271Swordsman", 200.0f, 230.0f);
	Object *archer = w.unit("Port271Archer", 230.0f, 230.0f);
	REQUIRE(arwen != nullptr);
	REQUIRE(elrond != nullptr);
	REQUIRE(swordsman != nullptr);
	REQUIRE(archer != nullptr);
	// tolerance: the ids are the logic's, the case needs only their order (creation order): the older hero also has the smaller id
	REQUIRE(arwen->getID() < elrond->getID());

	SUBCASE("the higher rank wins over the earlier selected hero, the lower BuildCost and the louder VoicePriority")
	{
		w.levelTo(*arwen, 1.0f);
		w.levelTo(*elrond, 260.0f);
		REQUIRE(w.rankOf(*arwen) == 1);
		REQUIRE(w.rankOf(*elrond) == 3);

		// Arwen is the first of the selection (ordinal 1) and the smaller id; Elrond's rank 3 must still take panel, portrait and voice
		w.select({ arwen, elrond, swordsman, archer });
		CHECK(w.bar.contextObject() == elrond->getID());
		CHECK(w.bar.commandSetName() == "Port271ElrondCommandSet");
		CHECK(w.palantirHas("Command_Port271_ElrondOnly"));
		CHECK_FALSE(w.palantirHas("Command_Port271_ArwenOnly"));
		CHECK(w.portrait() == "HP_Port271Elrond");
		CHECK(w.say(MSG_CREATE_SELECTED_GROUP) == elrond->getID());
		CHECK(w.sink.sounds == std::vector<std::string>{ "Port271ElrondSelect" });
		const Coord3D goal{ 300.0f, 260.0f, 0.0f };
		CHECK(w.say(MSG_DO_MOVETO, &goal) == elrond->getID());
		CHECK(w.sink.sounds == std::vector<std::string>{ "Port271ElrondMove" });
	}

	SUBCASE("the cheaper, quieter hero at the higher rank wins both sides")
	{
		// Arwen: rank 3 against Elrond's rank 1, with the lower BuildCost (1500 < 3500), the lower VoicePriority (55 < 82)
		// and the later place in the selection (ordinal 2)
		w.levelTo(*arwen, 260.0f);
		w.levelTo(*elrond, 1.0f);
		REQUIRE(w.rankOf(*arwen) == 3);
		REQUIRE(w.rankOf(*elrond) == 1);

		w.select({ elrond, arwen, swordsman, archer });
		CHECK(w.bar.contextObject() == arwen->getID());
		CHECK(w.bar.commandSetName() == "Port271ArwenCommandSet");
		CHECK(w.palantirHas("Command_Port271_ArwenOnly"));
		CHECK(w.portrait() == "HP_Port271Arwen");
		CHECK(w.say(MSG_CREATE_SELECTED_GROUP) == arwen->getID());
		CHECK(w.sink.sounds == std::vector<std::string>{ "Port271ArwenSelect" });
		const Coord3D goal{ 300.0f, 260.0f, 0.0f };
		CHECK(w.say(MSG_DO_MOVETO, &goal) == arwen->getID());
		CHECK(w.sink.sounds == std::vector<std::string>{ "Port271ArwenMove" });
	}

	SUBCASE("a HERO no ExperienceLevel names has no rank: the ranked hero leads both sides")
	{
		Object *glorfindel = w.unit("Port271Glorfindel", 170.0f, 200.0f);
		REQUIRE(glorfindel != nullptr);
		REQUIRE(w.rankOf(*glorfindel) == -1); // RW 0x689024 answers no level: the panel skips it (0x9447DA)
		w.levelTo(*arwen, 1.0f);
		REQUIRE(w.rankOf(*arwen) == 1);

		// the unranked hero is first in the selection and has the loudest VoicePriority of the three (99)
		w.select({ glorfindel, arwen, swordsman });
		CHECK(w.bar.contextObject() == arwen->getID());
		CHECK(w.bar.commandSetName() == "Port271ArwenCommandSet");
		CHECK(w.portrait() == "HP_Port271Arwen");
		CHECK(w.say(MSG_CREATE_SELECTED_GROUP) == arwen->getID());
		CHECK(w.sink.sounds == std::vector<std::string>{ "Port271ArwenSelect" });
	}

	SUBCASE("two heroes of the same rank: the smaller id leads the panel and the first of the selection speaks, the same hero")
	{
		w.levelTo(*arwen, 1.0f);
		w.levelTo(*elrond, 1.0f);
		REQUIRE(w.rankOf(*arwen) == w.rankOf(*elrond));

		w.select({ arwen, elrond, swordsman, archer }); // the order select-all and the drag box leave: oldest first
		CHECK(w.bar.contextObject() == arwen->getID());
		CHECK(w.say(MSG_CREATE_SELECTED_GROUP) == arwen->getID());
		CHECK(w.sink.sounds == std::vector<std::string>{ "Port271ArwenSelect" });
	}

	SUBCASE("no hero in the selection: no panel context, the loudest VoicePriority speaks")
	{
		w.select({ swordsman, archer });
		CHECK(w.bar.contextObject() == (ObjectID)INVALID_ID);
		CHECK(w.say(MSG_CREATE_SELECTED_GROUP) == archer->getID()); // 120 > 40 (0x8DD6A6)
		CHECK(w.sink.sounds == std::vector<std::string>{ "Port271ArcherSelect" });
	}
}

TEST_CASE("PORT-271: select-all and select-matching leave the selection oldest first, so the panel's hero is the one that answers (RW 0x69C860 / RW 0x6A43FE -> RW 0x6A4109)")
{
	SUBCASE("select all (CommandMap SELECT_ALL): ascending ids, the senior hero of the two equal ranks answers")
	{
		Port271World w;
		Object *elrond = w.unit("Port271Elrond", 200.0f, 200.0f);
		Object *arwen = w.unit("Port271Arwen", 230.0f, 200.0f);
		Object *swordsman = w.unit("Port271Swordsman", 200.0f, 230.0f);
		// tolerance: the ids are the logic's, the case needs only their order: Elrond is created first, so the selection must start with him
		REQUIRE(elrond->getID() < arwen->getID());
		w.levelTo(*elrond, 1.0f);
		w.levelTo(*arwen, 1.0f);

		ClientMessage &msg = w.stream.append(CMSG_META_SELECT_ALL);
		CHECK(w.translator.translate(msg) == MessageDisposition::Destroy);
		CHECK(w.ui.selected() == w.ids({ elrond, arwen, swordsman }));

		w.bar.update();
		CHECK(w.bar.contextObject() == elrond->getID());
		CHECK(w.bar.commandSetName() == "Port271ElrondCommandSet");
		CHECK(w.portrait() == "HP_Port271Elrond");
		CHECK(w.say(MSG_CREATE_SELECTED_GROUP) == elrond->getID());
		CHECK(w.sink.sounds == std::vector<std::string>{ "Port271ElrondSelect" });
	}

	SUBCASE("select matching (E): the units of the selection's template join it oldest first")
	{
		Port271World w;
		Object *first = w.unit("Port271Arwen", 200.0f, 200.0f);
		Object *second = w.unit("Port271Arwen", 230.0f, 200.0f);
		Object *third = w.unit("Port271Arwen", 260.0f, 200.0f);
		// tolerance: the ids are the logic's, the case needs only their order: the three are created oldest first
		REQUIRE(first->getID() < second->getID());
		// tolerance: as above, the order of the ids is the precondition, not their values
		REQUIRE(second->getID() < third->getID());
		w.levelTo(*first, 1.0f);
		w.levelTo(*second, 1.0f);
		w.levelTo(*third, 1.0f);

		// the newest clone is selected, E adds the two older ones across the map
		w.ui.selectObject(third->getID());
		CHECK(w.ui.selectMatching(w.ctx, true) == 2);
		CHECK(w.ui.selected() == w.ids({ first, second, third }));

		w.bar.update();
		CHECK(w.bar.contextObject() == first->getID());
		CHECK(w.say(MSG_CREATE_SELECTED_GROUP) == first->getID());
	}
}
