// OpenBFME unit tests. GPL-3.0.
// Lane CHEAT-1: the open Cheat Mode (GameLogic/CheatMode.h): MSG_OPENBFME_CHEAT runs only in a Cheat Mode game and only for the cheat seat's player; every
// command (run or refused) is logged and announced; the effects are exact and deterministic; a game without Cheat Mode keeps its hash. The wire, replay and
// snapshot forms of the setup's Cheat Mode. The games SKIP without ROTWK_INSTALL / BFME2_INSTALL.

#include "doctest.h"
#include "HudTestUtil.h"

#include "Common/Player.h"
#include "Common/PlayerScience.h"
#include "Common/Thing/ThingFactory.h"
#include "Common/Thing/ThingTemplate.h"
#include "GameClient/EvaAnnounceClientCreate.h"
#include "GameLogic/CheatMode.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/Damage.h"
#include "Common/PlayerHeroList.h"
#include "Common/Upgrade.h"
#include "GameLogic/Module/ProductionUpdate.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/Object/ObjectGeometry.h"
#include "GameLogic/SkirmishAI/SkirmishAIManager.h"
#include "GameLogic/GameLogicDispatch.h"
#include "GameLogic/GameMessage.h"
#include "GameLogic/Object/ExperienceTracker.h"
#include "GameLogic/VictoryConditions.h"
#include "GameNetwork/NetPacket.h"

#include <cstdint>
#include <string>
#include <vector>

using namespace hudtest;
using CheatMode::Command;

namespace
{
int templateId(GameLogic &logic, const char *name)
{
	const ThingTemplate *tt = logic.things().findTemplate(name);
	REQUIRE_MESSAGE(tt != nullptr, name);
	return (int)tt->getTemplateID();
}

std::vector<Object *> objectsOf(GameLogic &logic, const Player *owner, const std::string &templateName)
{
	std::vector<Object *> out;
	for (Object *o = logic.getFirstObject(); o; o = o->getNextObject())
	{
		if (o->getControllingPlayer() == owner && o->getTemplate()->getName() == templateName && !o->isEffectivelyDead())
		{
			out.push_back(o);
		}
	}
	return out;
}

const CheatMode::LogEntry &lastEntry(GameLogic &logic)
{
	REQUIRE(!logic.cheatMode().log().empty());
	return logic.cheatMode().log().back();
}

struct Run
{
	std::vector<std::uint32_t> hashes;
	std::uint32_t money = 0;
	std::vector<std::string> lines;
};

// `cheats`: the game is a Cheat Mode game with Player_1 (the local player) in the cheat seat; `commands`: the scripted commands are sent
Run play(SharedWorld &s, bool cheats, bool commands)
{
	Run out;
	Rig r(s);
	GameLogic &logic = r.logic();
	Player *host = r.local;
	Player *other = r.game->players().findPlayerWithName("Player_2");
	REQUIRE(other != nullptr);
	if (cheats)
	{
		logic.cheatMode().configure(0, host->getPlayerIndex());
	}
	r.frame(2);
	const int h = host->getPlayerIndex(), o = other->getPlayerIndex();
	if (commands)
	{
		r.game->commands().append(CheatMode::makeMessage(h, Command::AddResources, h, 2000));
		r.game->commands().append(CheatMode::makeMessage(o, Command::AddResources, o, 5000)); // not the cheat seat: refused
		r.game->commands().append(CheatMode::makeMessage(h, Command::AddCommandPoints, h, 50));
		r.game->commands().append(CheatMode::makeMessage(h, Command::AddPowerPoints, h, 7));
	}
	for (int i = 0; i < 10; ++i)
	{
		r.frame(1);
		out.hashes.push_back(logic.computeStateHash());
	}
	out.money = host->getMoney()->countMoney();
	for (const CheatMode::LogEntry &e : logic.cheatMode().log())
	{
		out.lines.push_back(e.text);
	}
	return out;
}

} // namespace

TEST_CASE("cheat1 mode: a game without Cheat Mode refuses every command, announces the refusal and keeps its hash")
{
	if (!haveWorld("cheat1 mode"))
	{
		return;
	}
	SharedWorld &s = shared();
	const Run quiet = play(s, false, false);
	const Run refused = play(s, false, true);
	CHECK(refused.hashes == quiet.hashes); // nothing ran and nothing is hashed for a game without Cheat Mode
	CHECK(refused.money == quiet.money);
	REQUIRE(refused.lines.size() == 4);
	for (const std::string &line : refused.lines)
	{
		CHECK(line == "Cheat Mode: refused: this is not a Cheat Mode game");
	}
}

TEST_CASE("cheat1 mode: resources, command points and power points go to the target exactly; another seat is refused; deterministic")
{
	if (!haveWorld("cheat1 mode"))
	{
		return;
	}
	SharedWorld &s = shared();
	const Run base = play(s, true, false);
	const Run one = play(s, true, true);
	const Run two = play(s, true, true);
	CHECK(one.hashes == two.hashes);
	CHECK(one.lines == two.lines);
	CHECK(one.money == base.money + 2000u); // the same game without the commands: the economy paid both the same
	REQUIRE(one.lines.size() == 4);
	CHECK(one.lines[0] == "Cheat Mode: Player_1 gave 2000 resources to Player_1");
	CHECK(one.lines[1] == "Cheat Mode: refused: Player_2 is not the host");
	CHECK(one.lines[2] == "Cheat Mode: Player_1 gave 50 command points to Player_1");
	CHECK(one.lines[3] == "Cheat Mode: Player_1 gave 7 power points to Player_1");
	CHECK(one.hashes.front() != base.hashes.front()); // a Cheat Mode game hashes its counts
}

TEST_CASE("cheat1 mode: every command's effect and limits in a Cheat Mode game")
{
	if (!haveWorld("cheat1 mode"))
	{
		return;
	}
	SharedWorld &s = shared();
	Rig r(s);
	GameLogic &logic = r.logic();
	Player *host = r.local;
	Player *other = r.game->players().findPlayerWithName("Player_2");
	REQUIRE(other != nullptr);
	logic.cheatMode().configure(0, host->getPlayerIndex());
	const int h = host->getPlayerIndex(), o = other->getPlayerIndex();
	auto send = [&](const GameMessage &m) {
		r.game->commands().append(m);
		r.frame(1);
		return lastEntry(logic);
	};
	SUBCASE("limits and malformed commands: refused, logged, never the dispatcher's unhandled count")
	{
		CHECK(!send(CheatMode::makeMessage(h, Command::AddResources, o, 0)).executed);
		CHECK(lastEntry(logic).text == "Cheat Mode: Player_1 (not done): resources must be 1 .. 100000");
		CHECK(!send(CheatMode::makeMessage(h, Command::AddResources, o, 100001)).executed);
		CHECK(!send(CheatMode::makeMessage(h, Command::AddCommandPoints, o, 1001)).executed);
		CHECK(!send(CheatMode::makeMessage(h, Command::AddPowerPoints, o, 101)).executed);
		CHECK(!send(CheatMode::makeMessage(h, Command::AddResources, 99, 10)).executed);
		CHECK(lastEntry(logic).text == "Cheat Mode: Player_1 (not done): there is no player 99 in the game");
		GameMessage shortOne(MSG_OPENBFME_CHEAT, h);
		shortOne.appendIntegerArgument((int)Command::AddResources);
		CHECK(!send(shortOne).executed);
		CHECK(lastEntry(logic).text == "Cheat Mode: refused: a malformed command");
		GameMessage unknown(MSG_OPENBFME_CHEAT, h);
		for (int v : { 42, h, 1 })
		{
			unknown.appendIntegerArgument(v);
		}
		CHECK(!send(unknown).executed);
		CHECK(lastEntry(logic).text == "Cheat Mode: refused: unknown command 42");
		CHECK(r.game->dispatch().unhandled().count(MSG_OPENBFME_CHEAT) == 0);
		CHECK(logic.cheatMode().executedCount() == 0);
		CHECK(logic.cheatMode().refusedCount() == 7);
	}
	SUBCASE("command points and power points: exact")
	{
		const int bonus = other->commandPoints().getBonus();
		const int points = other->science().getSciencePurchasePoints();
		CHECK(send(CheatMode::makeMessage(h, Command::AddCommandPoints, o, 120)).executed);
		CHECK(send(CheatMode::makeMessage(h, Command::AddPowerPoints, o, 3)).executed);
		CHECK(other->commandPoints().getBonus() == bonus + 120);
		CHECK(other->science().getSciencePurchasePoints() == points + 3);
	}
	SUBCASE("spawn: units and heroes for a player on the map; structures, unknown ids and off-map points refused")
	{
		const Coord3D at{ 600.0f, 600.0f, 0.0f };
		const std::size_t before = objectsOf(logic, other, "GondorFighterHorde").size();
		CHECK(send(CheatMode::makeMessage(h, Command::Spawn, o, 3, templateId(logic, "GondorFighterHorde"), &at)).executed);
		CHECK(lastEntry(logic).text == "Cheat Mode: Player_1 spawned 3 GondorFighterHorde for Player_2");
		CHECK(objectsOf(logic, other, "GondorFighterHorde").size() == before + 3);
		CHECK(send(CheatMode::makeMessage(h, Command::Spawn, h, 1, templateId(logic, "GondorBoromir"), &at)).executed);
		CHECK(objectsOf(logic, host, "GondorBoromir").size() == 1);
		// the test sandbox (2026-11-03): a structure is spawned too
		const Coord3D away{ 1400.0f, 1400.0f, 0.0f };
		CHECK(send(CheatMode::makeMessage(h, Command::Spawn, h, 2, templateId(logic, "GondorBarracks"), &away)).executed);
		const std::vector<Object *> barracks = objectsOf(logic, host, "GondorBarracks");
		REQUIRE(barracks.size() == 2);
		// side by side, a footprint apart: not on top of one another
		const float dx = barracks[1]->getPosition()->x - barracks[0]->getPosition()->x, dy = barracks[1]->getPosition()->y - barracks[0]->getPosition()->y;
		float lo[3], hi[3];
		ObjectGeometry::localBox(ObjectGeometry::cachedShapesOf(*barracks[0]->getTemplate()), lo, hi);
		CHECK(std::sqrt(dx * dx + dy * dy) >= std::max(hi[0] - lo[0], hi[1] - lo[1])); // tolerance: a footprint apart at least (the free spot search may move one further)
		CHECK(!send(CheatMode::makeMessage(h, Command::Spawn, h, 1, templateId(logic, "CreateAHero"), &at)).executed);
		CHECK(lastEntry(logic).text == "Cheat Mode: Player_1 (not done): CreateAHero is a Create-a-Hero: it is made from its player's own record, never on its own");
		CHECK(!send(CheatMode::makeMessage(h, Command::Spawn, h, 11, templateId(logic, "GondorBoromir"), &at)).executed);
		CHECK(!send(CheatMode::makeMessage(h, Command::Spawn, h, 1, 0xFFFF, &at)).executed);
		const Coord3D off{ -100000.0f, 600.0f, 0.0f };
		CHECK(!send(CheatMode::makeMessage(h, Command::Spawn, h, 1, templateId(logic, "GondorBoromir"), &off)).executed);
		CHECK(lastEntry(logic).text == "Cheat Mode: Player_1 (not done): a spawn point is off the map");
		CHECK(!send(CheatMode::makeMessage(h, Command::Spawn, h, 1, templateId(logic, "GondorBoromir"), nullptr)).executed);
		CHECK(lastEntry(logic).text == "Cheat Mode: Player_1 (not done): a spawn needs a location");
	}
	SUBCASE("spawn: a unit with EvaAnnounceClientCreate starts its announcement, as every creator's onBuildComplete does (RW 0x68D252, 0x68D27A)")
	{
		const Coord3D at{ 600.0f, 600.0f, 0.0f };
		REQUIRE(send(CheatMode::makeMessage(h, Command::Spawn, o, 1, templateId(logic, "SummonedDragon"), &at)).executed);
		r.frame(1);
		const std::vector<Object *> dragons = objectsOf(logic, other, "SummonedDragon");
		REQUIRE(dragons.size() == 1);
		Drawable *d = r.game->drawables().findByObject(dragons[0]->getID());
		REQUIRE(d != nullptr);
		const EvaAnnounceClientCreate *eva = nullptr;
		for (const auto &module : d->clientModules())
		{
			if (const auto *found = dynamic_cast<const EvaAnnounceClientCreate *>(module.get()))
			{
				eva = found;
			}
		}
		REQUIRE(eva != nullptr);
		CHECK(eva->started());
	}
	SUBCASE("level up: a hero of the target gains the levels; another player's object or a dead one is refused")
	{
		const Coord3D at{ 600.0f, 600.0f, 0.0f };
		REQUIRE(send(CheatMode::makeMessage(h, Command::Spawn, h, 1, templateId(logic, "GondorBoromir"), &at)).executed);
		const std::vector<Object *> heroes = objectsOf(logic, host, "GondorBoromir");
		REQUIRE(heroes.size() == 1);
		ExperienceTracker *xp = heroes[0]->getExperienceTracker();
		REQUIRE(xp != nullptr);
		const int rank = xp->getRank();
		CHECK(send(CheatMode::makeMessage(h, Command::LevelUp, h, 2, (int)heroes[0]->getID())).executed);
		CHECK(xp->getRank() == rank + 2);
		CHECK(lastEntry(logic).text == "Cheat Mode: Player_1 gave 2 levels to Player_1's GondorBoromir");
		CHECK(!send(CheatMode::makeMessage(h, Command::LevelUp, o, 1, (int)heroes[0]->getID())).executed);
		CHECK(!send(CheatMode::makeMessage(h, Command::LevelUp, h, 11, (int)heroes[0]->getID())).executed);
	}
	SUBCASE("the sandbox's object commands: freeze, hold fire, heal, kill, levels, ready powers, give to a player")
	{
		const Coord3D at{ 600.0f, 600.0f, 0.0f };
		REQUIRE(send(CheatMode::makeMessage(h, Command::Spawn, o, 1, templateId(logic, "GondorFighterHorde"), &at)).executed);
		REQUIRE(send(CheatMode::makeMessage(h, Command::Spawn, h, 1, templateId(logic, "GondorBoromir"), &at)).executed);
		r.frame(5);
		const std::vector<Object *> hordes = objectsOf(logic, other, "GondorFighterHorde");
		const std::vector<Object *> heroes = objectsOf(logic, host, "GondorBoromir");
		REQUIRE(hordes.size() == 1);
		REQUIRE(heroes.size() == 1);
		Object *horde = hordes[0];
		Object *hero = heroes[0];
		REQUIRE(horde->getContain() != nullptr);
		const std::vector<Object *> members(horde->getContain()->getContainedItemsList()->begin(), horde->getContain()->getContainedItemsList()->end());
		REQUIRE(!members.empty());
		auto objects = [&](Command c, int target, int value, std::vector<ObjectID> ids) { return send(CheatMode::makeObjectsMessage(h, c, target, value, ids)); };

		// freeze: the horde and its members are held; release clears it
		CHECK(objects(Command::Freeze, h, 1, { horde->getID() }).executed);
		CHECK(lastEntry(logic).text == "Cheat Mode: Player_1 froze 1 object");
		CHECK(horde->isDisabledByType(DISABLED_HELD));
		for (Object *m : members)
		{
			CHECK(m->isDisabledByType(DISABLED_HELD));
		}
		CHECK(objects(Command::Freeze, h, 0, { horde->getID() }).executed);
		CHECK_FALSE(horde->isDisabledByType(DISABLED_HELD));
		CHECK_FALSE(members[0]->isDisabledByType(DISABLED_HELD));

		// hold fire: the hold-fire button's status on the horde and its members
		static const int noAuto = CombatNames::status("NO_AUTO_ACQUIRE");
		CHECK(objects(Command::HoldFire, h, 1, { horde->getID() }).executed);
		CHECK(horde->testStatus((unsigned)noAuto));
		CHECK(members[0]->testStatus((unsigned)noAuto));
		CHECK(objects(Command::HoldFire, h, 0, { horde->getID() }).executed);
		CHECK_FALSE(horde->testStatus((unsigned)noAuto));

		// give to a player: the horde and its members change owner (from here no enemy stands beside the hero)
		CHECK(objects(Command::GiveTo, h, 0, { horde->getID() }).executed);
		CHECK(lastEntry(logic).text == "Cheat Mode: Player_1 gave 1 object to Player_1");
		CHECK(horde->getControllingPlayer() == host);
		CHECK(members[0]->getControllingPlayer() == host);

		// heal: a wounded hero has its full health again
		DamageInfo hit;
		hit.m_input.m_damageType = DAMAGE_UNRESISTABLE;
		hit.m_input.m_amount = 100.0f;
		hero->attemptDamage(hit);
		REQUIRE(hero->getBodyModule()->getHealth() < hero->getBodyModule()->getMaxHealth()); // tolerance: the hero's armour sets the loss; only "wounded" matters
		CHECK(objects(Command::Heal, h, 0, { hero->getID() }).executed);
		CHECK(hero->getBodyModule()->getHealth() == hero->getBodyModule()->getMaxHealth());

		// levels on the named objects
		const int rank = hero->getExperienceTracker()->getRank();
		CHECK(objects(Command::LevelUpObjects, h, 2, { hero->getID() }).executed);
		CHECK(hero->getExperienceTracker()->getRank() == rank + 2);
		CHECK(!objects(Command::LevelUpObjects, h, 11, { hero->getID() }).executed);

		// ready powers: every special power module is ready now
		std::size_t powers = 0;
		for (const auto &module : hero->modules())
		{
			if (SpecialPowerModuleInterface *sp = module->getSpecialPower())
			{
				sp->setReadyFrame(logic.getFrame() + 3000);
				++powers;
			}
		}
		REQUIRE(powers > 0); // tolerance: Boromir has several powers; any number of them is the scene
		CHECK(objects(Command::ResetCooldowns, h, 0, { hero->getID() }).executed);
		for (const auto &module : hero->modules())
		{
			if (SpecialPowerModuleInterface *sp = module->getSpecialPower())
			{
				CHECK(sp->getReadyFrame() <= logic.getFrame()); // tolerance: ready at the command's frame or earlier: the check runs a frame later
			}
		}

		// kill; then the named objects are gone and the command is refused
		CHECK(objects(Command::Kill, h, 0, { hero->getID(), hero->getID() }).executed);
		CHECK(lastEntry(logic).text == "Cheat Mode: Player_1 killed 1 object");
		CHECK(hero->isEffectivelyDead());
		CHECK(!objects(Command::Heal, h, 0, { hero->getID() }).executed);
		CHECK(lastEntry(logic).text == "Cheat Mode: Player_1 (not done): none of the named objects is alive");

		// a list without its ids, or longer than the limit: malformed
		GameMessage shortList(MSG_OPENBFME_CHEAT, h);
		for (int v : { (int)Command::Heal, h, 0, 2, (int)horde->getID() })
		{
			shortList.appendIntegerArgument(v);
		}
		CHECK(!send(shortList).executed);
		CHECK(lastEntry(logic).text == "Cheat Mode: refused: a malformed command");
		CHECK(!objects(Command::Heal, h, 0, std::vector<ObjectID>(CheatMode::kMaxObjects + 1, horde->getID())).executed);
		CHECK(lastEntry(logic).text == "Cheat Mode: refused: a malformed command");
	}
	SUBCASE("pause AI: the player's skirmish AI does not update while paused; hashed only once used, and saved")
	{
		const std::uint32_t before = logic.computeStateHash();
		CHECK(send(CheatMode::makeMessage(h, Command::PauseAI, o, 1)).executed);
		CHECK(lastEntry(logic).text == "Cheat Mode: Player_1 paused the AI of Player_2");
		CHECK(logic.cheatMode().aiPaused(o));
		CHECK(!send(CheatMode::makeMessage(h, Command::PauseAI, o, 1)).executed);
		CHECK(before != logic.computeStateHash());
		std::vector<std::uint8_t> saved;
		std::string err;
		REQUIRE_MESSAGE(logic.saveSnapshot(saved, &err), err);
		const std::uint32_t atSave = logic.computeStateHash();
		CHECK(send(CheatMode::makeMessage(h, Command::PauseAI, o, 0)).executed);
		CHECK(!logic.cheatMode().aiPaused(o));
		REQUIRE_MESSAGE(logic.loadSnapshot(saved, &err), err);
		CHECK(logic.cheatMode().aiPaused(o));
		CHECK(logic.computeStateHash() == atSave);
	}
	SUBCASE("instant build: on for the cheat seat's player from the start; an upgrade takes one frame there, its own time elsewhere; switched per player")
	{
		CHECK(logic.cheatMode().instantBuild(h));
		CHECK_FALSE(logic.cheatMode().instantBuild(o));
		const Coord3D mine{ 1400.0f, 1400.0f, 0.0f }, theirs{ 600.0f, 1400.0f, 0.0f };
		REQUIRE(send(CheatMode::makeMessage(h, Command::Spawn, h, 1, templateId(logic, "GondorBarracks"), &mine)).executed);
		REQUIRE(send(CheatMode::makeMessage(h, Command::Spawn, o, 1, templateId(logic, "GondorBarracks"), &theirs)).executed);
		Object *a = objectsOf(logic, host, "GondorBarracks").front();
		Object *b = objectsOf(logic, other, "GondorBarracks").front();
		const UpgradeTemplate *level2 = s.world->upgrades().findUpgrade("Upgrade_TechnologyGondorHeavyArmor"); // a player upgrade, researched once per player
		REQUIRE(level2 != nullptr);
		REQUIRE(level2->calcTimeToBuild(host) > 5); // tolerance: its own time is longer than the frames below
		REQUIRE(a->getProductionUpdate()->queueUpgrade(level2));
		REQUIRE(b->getProductionUpdate()->queueUpgrade(level2));
		r.frame(3);
		CHECK(host->hasUpgradeComplete(level2));
		CHECK_FALSE(other->hasUpgradeComplete(level2));
		// a hero's recruit counts on its record (PlayerHeroList::progressAt, RW 0x780C11), not on the entry: one frame for the host, its time for the other
		PlayerHeroList &mineHeroes = host->heroes(), &theirHeroes = other->heroes();
		REQUIRE(mineHeroes.size() > 0); // tolerance: Gondor has several recruitable heroes; the first is the scene
		REQUIRE(theirHeroes.size() > 0); // tolerance: as above
		REQUIRE(mineHeroes.startProduction(0, 0x7FFF0001u, logic.getFrame()));
		REQUIRE(theirHeroes.startProduction(0, 0x7FFF0002u, logic.getFrame()));
		r.frame(1);
		CHECK(mineHeroes.progressAt(logic, *host, 0, nullptr) >= 1.0); // tolerance: done is any progress of 1 or more
		CHECK(theirHeroes.progressAt(logic, *other, 0, nullptr) < 1.0); // tolerance: still recruiting is any progress under 1
		mineHeroes.cancelProduction(0x7FFF0001u);
		theirHeroes.cancelProduction(0x7FFF0002u);
		CHECK(send(CheatMode::makeMessage(h, Command::InstantBuild, o, 1)).executed);
		CHECK(lastEntry(logic).text == "Cheat Mode: Player_1 made building instant for Player_2");
		r.frame(3);
		CHECK(other->hasUpgradeComplete(level2));
		CHECK(send(CheatMode::makeMessage(h, Command::InstantBuild, h, 0)).executed);
		CHECK_FALSE(logic.cheatMode().instantBuild(h));
		CHECK(!send(CheatMode::makeMessage(h, Command::InstantBuild, h, 0)).executed);
	}
	SUBCASE("reveal: once per player, undo once")
	{
		CHECK(send(CheatMode::makeMessage(h, Command::RevealMap, o, 1)).executed);
		CHECK(logic.cheatMode().revealed(o));
		CHECK(!send(CheatMode::makeMessage(h, Command::RevealMap, o, 1)).executed);
		CHECK(lastEntry(logic).text == "Cheat Mode: Player_1 (not done): the map is revealed for Player_2 already");
		CHECK(send(CheatMode::makeMessage(h, Command::RevealMap, o, 0)).executed);
		CHECK(!logic.cheatMode().revealed(o));
		CHECK(!send(CheatMode::makeMessage(h, Command::RevealMap, o, 0)).executed);
	}
	SUBCASE("end game: every player outside the target's alliance is killed")
	{
		CHECK(send(CheatMode::makeMessage(h, Command::EndGame, h, 0)).executed);
		CHECK(lastEntry(logic).text == "Cheat Mode: Player_1 ended the game: Player_1's alliance wins");
		CHECK(other->isDefeated());
		CHECK(!host->isDefeated());
		CHECK(!send(CheatMode::makeMessage(h, Command::EndGame, h, 0)).executed);
	}
	SUBCASE("void game: every player is killed, no winner, once")
	{
		CHECK(send(CheatMode::makeMessage(h, Command::VoidGame, -1, 0)).executed);
		CHECK(logic.cheatMode().voided());
		CHECK(other->isDefeated());
		CHECK(host->isDefeated());
		CHECK(!send(CheatMode::makeMessage(h, Command::VoidGame, -1, 0)).executed);
		CHECK(lastEntry(logic).text == "Cheat Mode: Player_1 (not done): the game is void already");
	}
}

TEST_CASE("cheat1 mode: the command names and the message")
{
	for (int c = CheatMode::kFirstCommand; c <= CheatMode::kLastCommand; ++c)
	{
		CHECK(CheatMode::commandFromName(CheatMode::commandName(c)) == c);
	}
	CHECK(CheatMode::commandFromName("give_everything") == 0);
	CHECK(std::string(GameMessageTypeName(MSG_OPENBFME_CHEAT)) == "MSG_OPENBFME_CHEAT");
	CHECK(IsNetworkLogicMessageType(MSG_OPENBFME_CHEAT));
	CHECK(!IsNetworkLogicMessageType(32000));
	// through the network's encoding and back
	const Coord3D at{ 100.5f, 200.25f, 0.0f };
	const GameMessage m = CheatMode::makeMessage(3, Command::Spawn, 2, 4, 1234, &at);
	NetByteWriter w;
	std::string err;
	REQUIRE(NetPacket::writeGameMessage(w, m, &err));
	NetByteReader rd(w.data());
	std::unique_ptr<GameMessage> back = NetPacket::readGameMessage(rd, &err);
	REQUIRE_MESSAGE(back != nullptr, err);
	CHECK(back->getType() == MSG_OPENBFME_CHEAT);
	REQUIRE(back->getArgumentCount() == 5);
	CHECK(back->getArgument(3)->integer == 1234);
	CHECK(back->getArgument(4)->location.x == 100.5f);
	CHECK(back->getArgument(4)->location.y == 200.25f);
}

TEST_CASE("cheat1 mode: the setup's Cheat Mode on the wire; an older format reads as off; a seat that is not a human slot is refused")
{
	SkirmishGameInfo g;
	g.slots[0].state = SLOT_PLAYER;
	g.slots[1].state = SLOT_HARD_AI;
	g.mapName = "maps/map mp fall back 4p";
	g.cheatMode = 0;
	NetByteWriter w;
	NetPacket::writeGameInfo(w, g);
	SkirmishGameInfo back;
	std::string err;
	{
		NetByteReader r(w.data());
		REQUIRE_MESSAGE(NetPacket::readGameInfo(r, back, &err), err);
		CHECK(r.atEnd());
		CHECK(back == g);
		CHECK(back.cheatModeOn());
	}
	{
		// the replay format 7 / LAN protocol 3 shape: no Cheat Mode field (the reader is told so): off, and the 4 bytes are left over
		NetByteReader r(w.data());
		REQUIRE_MESSAGE(NetPacket::readGameInfo(r, back, &err, false, true, false), err);
		CHECK(back.cheatMode == -1);
		CHECK(r.remaining() == 4);
	}
	for (int bad : { 1, 8, -2 })
	{
		g.cheatMode = bad; // slot 1 is a computer: never a cheat seat
		NetByteWriter w2;
		NetPacket::writeGameInfo(w2, g);
		NetByteReader r(w2.data());
		CHECK(!NetPacket::readGameInfo(r, back, &err));
		CHECK(err == "GameInfo Cheat Mode seat " + std::to_string(bad) + " is not a human slot");
	}
}
