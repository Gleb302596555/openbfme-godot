// OpenBFME unit tests (Local 161, fork issue 928): RotWK 2.01 ScriptActions ports verified from game.dat disassembly:
//   1. MAP_REVEAL_ALL (case 105, RW 0x7BC3FF), MAP_REVEAL_ALL_PERM / MAP_REVEAL_ALL_UNDO_PERM (cases 233/234, RW 0x7BC479),
//      MAP_SHROUD_ALL (case 149, RW 0x7BC50C): empty player mask falls back to every PLAYER_HUMAN (Player + 0x5C == 0).
//   2. NAMED_SET_STRICT_CONTROL_ENABLED (case 432, RW 0x7BC814 -> Object::setScriptStatus(0x20, on) RW 0x69317D).
//   3. NAMED_SET_HELD (case 229, RW 0x7BC9D6 -> Object::setDisabledUntil(DISABLED_HELD=3, held ? 0x3FFFFFFF : 0) RW 0x6907F1).
//   4. UNIT_SET_EXPERIENCE_POINTS (case 360, RW 0x7BD41D -> ExperienceTracker::setExperienceAndLevel((float)pts, true) RW 0x79D8EF).
//   5. UNIT_GAIN_LEVEL (case 523, RW 0x7BDE10 -> ExperienceTracker::gainLevel(feedback) RW 0x79D7AD).
//   6. NAMED_SET_SPECIAL_WEAPONSET (case 431, RW 0x7BDB15 -> Object::setWeaponSetFlag(0x17, on) RW 0x691059 / 0x691106).
//   7. DAMAGE_MEMBERS_OF_TEAM (case 12, RW 0x7BF46E -> Team::damageTeamMembers(amount) RW 0x7A1080: < 0 kills, >= 0 damages).
//   8. Client-only ScriptActions added to clientActions() (S-1182: RW 0x7BD65B, RW 0x7BB434, RW 0x7BDAB7, ...).
// Synthetic tests only: no retail or mod files are required.
// GPL-3.0.

#include "doctest.h"
#include "ScriptTestUtil.h"

#include "GameLogic/Damage.h"
#include "GameLogic/ExperienceLevels.h"
#include "GameLogic/Module/ActiveBody.h"
#include "GameLogic/Module/LogicModules.h"
#include "GameLogic/Object/ExperienceTracker.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/ObjectTypes.h"
#include "GameLogic/ScriptEngine/ScriptActions.h"
#include "GameLogic/System/ShroudManager.h"

#include <string>

using namespace scripttest;

namespace
{

const char kLocal161Ini[] =
	"ExperienceLevel L161Level1\n"
	"  TargetNames = L161Unit\n"
	"  RequiredExperience = 1\n"
	"  ExperienceAward = 5\n"
	"  Rank = 1\n"
	"End\n"
	"ExperienceLevel L161Level2\n"
	"  TargetNames = L161Unit\n"
	"  RequiredExperience = 50\n"
	"  ExperienceAward = 10\n"
	"  Rank = 2\n"
	"End\n"
	"ExperienceLevel L161Level3\n"
	"  TargetNames = L161Unit\n"
	"  RequiredExperience = 120\n"
	"  ExperienceAward = 15\n"
	"  Rank = 3\n"
	"End\n"
	"Object L161Unit\n"
	"  KindOf = PRELOAD SELECTABLE CAN_ATTACK INFANTRY\n"
	"  IsTrainable = Yes\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 10.0\n"
	"  GeometryHeight = 10.0\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 200\n"
	"    MaxHealthDamaged = 100\n"
	"    MaxHealthReallyDamaged = 20\n"
	"  End\n"
	"End\n";

struct Local161Game : ScriptGame
{
	ExperienceLevelSystem levels;
	ExperienceLevelSystem *savedLevels = TheExperienceLevelSystem;
	Local161Game()
	{
		LogicModules::registerAll(lw.w.modules);
		levels.registerBlocks(lw.w.fx.env.blocks);
		TheExperienceLevelSystem = &levels;
		const std::string err = lw.w.load(kLocal161Ini, INI_LOAD_OVERWRITE, "local161.ini");
		REQUIRE_MESSAGE(err.empty(), err);
	}
	~Local161Game()
	{
		TheExperienceLevelSystem = savedLevels;
	}

	Object *spawnNamed(const std::string &name, Team *team)
	{
		Object *o = lw.make("L161Unit", team);
		REQUIRE(o != nullptr);
		o->setName(name);
		return o;
	}
};

struct Local161Shroud
{
	GameLogic &logic;
	ShroudManager shroud;
	explicit Local161Shroud(GameLogic &l) : logic(l), shroud(l)
	{
		shroud.init(20.0f, 0);
		shroud.setExtent(0.0f, 0.0f, 200.0f, 200.0f, 20.0f);
		logic.setShroud(&shroud);
	}
	~Local161Shroud() { logic.setShroud(nullptr); }
};

} // namespace

// RW 0x7BC3FF (MAP_REVEAL_ALL), RW 0x7BC479 (MAP_REVEAL_ALL_PERM / MAP_REVEAL_ALL_UNDO_PERM), RW 0x7BC50C (MAP_SHROUD_ALL):
// When getPlayerMask returns 0, RotWK loops over PlayerList and applies the shroud change to every PLAYER_HUMAN (Player + 0x5C == 0).
TEST_CASE("LOCAL-161 ScriptActions: MAP_REVEAL_ALL / MAP_SHROUD_ALL empty mask falls back to all PLAYER_HUMAN (RW 0x7BC3FF / 0x7BC479 / 0x7BC50C)")
{
	Local161Game g;
	Local161Shroud sh(*g.lw.logic);
	Player *alice = g.lw.players.getNthPlayer(1); // PLAYER_HUMAN
	Player *bob = g.lw.players.getNthPlayer(2);   // PLAYER_COMPUTER
	REQUIRE(alice != nullptr);
	REQUIRE(bob != nullptr);
	REQUIRE(alice->getPlayerType() == PLAYER_HUMAN);
	REQUIRE(bob->getPlayerType() == PLAYER_COMPUTER);

	CHECK(sh.shroud.getCellStatus(1, 2, 2) == CELLSHROUD_SHROUDED);
	CHECK(sh.shroud.getCellStatus(2, 2, 2) == CELLSHROUD_SHROUDED);

	g.lists[1].items.push_back(item(script("S1", {{cond("CONDITION_TRUE", {})}}, {
		act("MAP_REVEAL_ALL_PERM", {sp(11, "NoSuchPlayer")}),
	}, {}, true)));
	g.start();
	g.frame();

	CHECK(sh.shroud.getCellStatus(1, 2, 2) == CELLSHROUD_CLEAR);
	CHECK(sh.shroud.getCellStatus(2, 2, 2) == CELLSHROUD_SHROUDED);

	// Undo perm with empty mask clears Alice's permanent reveal back to fogged, then MAP_SHROUD_ALL re-shrouds Alice
	ScriptActions::execute(g.engine(), act("MAP_REVEAL_ALL_UNDO_PERM", {sp(11, "NoSuchPlayer")}));
	ScriptActions::execute(g.engine(), act("MAP_SHROUD_ALL", {sp(11, "NoSuchPlayer")}));
	CHECK(sh.shroud.getCellStatus(1, 2, 2) == CELLSHROUD_SHROUDED);

	// Neighboring behavior: explicit player name ("Bob") reveals only Bob, leaving Alice shrouded
	ScriptActions::execute(g.engine(), act("MAP_REVEAL_ALL", {sp(11, "Bob")}));
	CHECK(sh.shroud.getCellStatus(1, 2, 2) == CELLSHROUD_SHROUDED);
	CHECK(sh.shroud.getCellStatus(2, 2, 2) == CELLSHROUD_FOGGED);
}

// RW 0x7BC814 (case 432, NAMED_SET_STRICT_CONTROL_ENABLED): Object::setScriptStatus(0x20, on) (RW 0x69317D).
TEST_CASE("LOCAL-161 ScriptActions: NAMED_SET_STRICT_CONTROL_ENABLED toggles scriptStatus bit 0x20 (RW 0x7BC814)")
{
	Local161Game g;
	g.start();
	Object *u = g.spawnNamed("StrictUnit", g.lw.players.getNthPlayer(1)->getDefaultTeam());
	u->setScriptStatus(0x04u, true); // neighboring bit SCRIPT_UNSELLABLE must stay intact

	ScriptActions::execute(g.engine(), act("NAMED_SET_STRICT_CONTROL_ENABLED", {sp(14, "StrictUnit"), ip(8, 1)}));
	CHECK((u->scriptStatus() & 0x20u) == 0x20u);
	CHECK((u->scriptStatus() & 0x04u) == 0x04u);
	CHECK(g.engine().stats().unportedActions.empty());

	ScriptActions::execute(g.engine(), act("NAMED_SET_STRICT_CONTROL_ENABLED", {sp(14, "StrictUnit"), ip(8, 0)}));
	CHECK((u->scriptStatus() & 0x20u) == 0u);
	CHECK((u->scriptStatus() & 0x04u) == 0x04u);
}

// RW 0x7BC9D6 (case 229, NAMED_SET_HELD): Object::setDisabledUntil(DISABLED_HELD=3, held ? 0x3FFFFFFF : 0) (RW 0x6907F1).
TEST_CASE("LOCAL-161 ScriptActions: NAMED_SET_HELD sets and clears DISABLED_HELD (RW 0x7BC9D6)")
{
	Local161Game g;
	g.start();
	Object *u = g.spawnNamed("HeldUnit", g.lw.players.getNthPlayer(1)->getDefaultTeam());
	CHECK_FALSE(u->isDisabledByType(DISABLED_HELD));

	ScriptActions::execute(g.engine(), act("NAMED_SET_HELD", {sp(14, "HeldUnit"), ip(8, 1)}));
	CHECK(u->isDisabledByType(DISABLED_HELD));
	CHECK(g.engine().stats().unportedActions.empty());

	ScriptActions::execute(g.engine(), act("NAMED_SET_HELD", {sp(14, "HeldUnit"), ip(8, 0)}));
	CHECK_FALSE(u->isDisabledByType(DISABLED_HELD));
}

// RW 0x7BD41D (case 360, UNIT_SET_EXPERIENCE_POINTS): ExperienceTracker::setExperienceAndLevel((float)pts, true) (RW 0x79D8EF).
// RW 0x7BDE10 (case 523, UNIT_GAIN_LEVEL): ExperienceTracker::gainLevel(feedback) (RW 0x79D7AD).
TEST_CASE("LOCAL-161 ScriptActions: UNIT_SET_EXPERIENCE_POINTS and UNIT_GAIN_LEVEL advance unit XP and rank (RW 0x7BD41D / 0x7BDE10)")
{
	Local161Game g;
	g.start();
	Object *u = g.spawnNamed("XpUnit", g.lw.players.getNthPlayer(1)->getDefaultTeam());
	ExperienceTracker *xp = u->getExperienceTracker();
	REQUIRE(xp != nullptr);

	ScriptActions::execute(g.engine(), act("UNIT_SET_EXPERIENCE_POINTS", {sp(14, "XpUnit"), ip(0, 60)}));
	CHECK(xp->getExperience() == 60.0f);
	CHECK(xp->getRank() == 2);
	CHECK(g.engine().stats().unportedActions.empty());

	ScriptActions::execute(g.engine(), act("UNIT_GAIN_LEVEL", {sp(14, "XpUnit"), ip(8, 1)}));
	CHECK(xp->getRank() == 3);
	CHECK(xp->getExperience() == 120.0f);
}

// RW 0x7BDB15 (case 431, NAMED_SET_SPECIAL_WEAPONSET): Object::setWeaponSetFlag(0x17, param1 > 0) (RW 0x691059 / RW 0x691106).
TEST_CASE("LOCAL-161 ScriptActions: NAMED_SET_SPECIAL_WEAPONSET toggles weaponset flag 0x17 (RW 0x7BDB15)")
{
	Local161Game g;
	g.start();
	Object *u = g.spawnNamed("WsUnit", g.lw.players.getNthPlayer(1)->getDefaultTeam());
	CHECK((u->getWeaponSetFlags()[0] & (1u << 0x17)) == 0u);

	ScriptActions::execute(g.engine(), act("NAMED_SET_SPECIAL_WEAPONSET", {sp(14, "WsUnit"), ip(8, 1)}));
	CHECK((u->getWeaponSetFlags()[0] & (1u << 0x17)) == (1u << 0x17));
	CHECK(g.engine().stats().unportedActions.empty());

	ScriptActions::execute(g.engine(), act("NAMED_SET_SPECIAL_WEAPONSET", {sp(14, "WsUnit"), ip(8, 0)}));
	CHECK((u->getWeaponSetFlags()[0] & (1u << 0x17)) == 0u);
}

// RW 0x7BF46E (case 12, DAMAGE_MEMBERS_OF_TEAM) -> RW 0x7A1080 (Team::damageTeamMembers):
// amount >= 0 deals DAMAGE_UNRESISTABLE (RW 0x698E7D); amount < 0 kills every living member (RW 0x698EC3).
TEST_CASE("LOCAL-161 ScriptActions: DAMAGE_MEMBERS_OF_TEAM damages (>= 0) and kills (< 0) safely (RW 0x7BF46E / 0x7A1080)")
{
	Local161Game g;
	g.start();
	Team *team = g.lw.players.getNthPlayer(1)->getDefaultTeam();
	team->setActive();
	const std::string teamName = "Alice/" + team->getPrototype()->getName();
	Object *u1 = g.spawnNamed("DmgUnit1", team);
	Object *u2 = g.spawnNamed("DmgUnit2", team);
	REQUIRE(u1->getBodyModule() != nullptr);
	REQUIRE(u2->getBodyModule() != nullptr);
	const float hp0 = u1->getBodyModule()->getHealth();

	ScriptActions::execute(g.engine(), act("DAMAGE_MEMBERS_OF_TEAM", {sp(3, teamName), rp(50.0f)}));
	CHECK(u1->getBodyModule()->getHealth() == 150.0f);
	CHECK(u2->getBodyModule()->getHealth() == 150.0f);
	CHECK_FALSE(u1->isEffectivelyDead());
	CHECK(g.engine().stats().unportedActions.empty());

	// Negative amount kills every member safely even as members leave the team / die
	ScriptActions::execute(g.engine(), act("DAMAGE_MEMBERS_OF_TEAM", {sp(3, teamName), rp(-1.0f)}));
	CHECK(u1->isEffectivelyDead());
	CHECK(u2->isEffectivelyDead());
}

// S-1182 client-only actions added in Local 161 (RW 0x7BD65B, RW 0x7BB434, RW 0x7BDAB7): recorded as client requests, not unportedActions.
TEST_CASE("LOCAL-161 ScriptActions: client-only actions record client requests without unportedAction (RW 0x7BD65B / 0x7BB434 / 0x7BDAB7)")
{
	Local161Game g;
	g.start();

	ScriptActions::execute(g.engine(), act("COMMANDBAR_ADD_BUTTON_OBJECTTYPE_SLOT", {sp(2, "L161Unit"), sp(2, "Command_Dummy"), ip(0, 1)}));
	ScriptActions::execute(g.engine(), act("AUDIO_MAKE_SOUND_SUBJECT_TO_FADE", {sp(2, "DummySound")}));
	ScriptActions::execute(g.engine(), act("LOCK_CAMERA", {ip(8, 1)}));

	CHECK(g.engine().stats().unportedActions.empty());
	CHECK(g.engine().pendingClientRequests().size() == 3);
}
