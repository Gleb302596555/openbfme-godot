// OpenBFME. GPL-3.0.
// Narrow tests for lane LOCAL-57 (issue #413, P-74): TEAM_WANDER and AIWanderState (RW 0x7CC3FF -> 0x7C955B, 0x74469E).

#include "doctest.h"
#include "EnnorathScenario.h"
#include "ScriptTestUtil.h"
#include "CombatTestUtil.h"

#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/PlayerTemplate.h"
#include "Common/Team.h"
#include "Common/XferLoad.h"
#include "Common/XferSave.h"
#include "GameClient/Drawable.h"
#include "GameClient/DrawableManager.h"
#include "GameClient/GUI/Skirmish/IniSkirmishSetupSource.h"
#include "GameClient/LiveGame.h"
#include "GameClient/MapChunks.h"
#include "GameClient/MapCreationHooks.h"
#include "GameClient/MapObjectDrawables.h"
#include "GameLogic/AI/AIWaypointPath.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Map/TerrainLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/LogicModules.h"
#include "GameLogic/NewGame/NewGame.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/ScriptEngine/ScriptActions.h"
#include "GameLogic/ScriptEngine/ScriptEngine.h"
#include "Libraries/WWVegas/WW3D2/assetmgr.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace
{
void ensureEnnorathMapOptions(ennorathscenario::World &world)
{
	if (world.shared.options.factionColors.empty())
	{
		std::string error;
		REQUIRE_MESSAGE(MapObjectGameData::load(*world.merged.fs, world.shared.options, &error), error);
		REQUIRE_MESSAGE(MapObjectGameData::loadPlayerTemplates(*world.merged.fs, world.shared.options, &error), error);
		REQUIRE_MESSAGE(MapCreationHooks::load(*world.merged.fs, world.shared.options.creationScripts, &error), error);
	}
}

void setupSkirmishStart(ennorathscenario::World &world, const std::string &mapPath, NewGameStart &start)
{
	std::string error;
	GameLogicSettings settings;
	REQUIRE_MESSAGE(GameLogicSettingsLoader::load(*world.merged.fs, settings, &error), error);
	std::vector<MapCacheEntry> maps;
	REQUIRE_MESSAGE(IniSkirmishSetupSource::loadMapCache(*world.merged.fs, maps, &error), error);

	auto templateIndex = [&](const std::string &name) {
		for (int i = 0; i < world.shared.world->playerTemplates().getPlayerTemplateCount(); ++i)
		{
			if (world.shared.world->playerTemplates().getNthPlayerTemplate(i)->getName() == name)
			{
				return i;
			}
		}
		return -1;
	};

	NewGameMessage msg;
	msg.game.mapName = mapPath;
	msg.game.seed = 5;
	msg.game.startingCash = 1000;
	for (int i = 0; i < 2; ++i)
	{
		SkirmishGameSlot &sl = msg.game.slots[(size_t)i];
		sl.state = i == 0 ? SLOT_PLAYER : SLOT_EASY_AI;
		sl.name = i == 0 ? u"Host" : u"Easy";
		sl.accepted = true;
		sl.playerTemplate = templateIndex(i == 0 ? "FactionMen" : "FactionMordor");
		const bool tmplFound = sl.playerTemplate >= 0;
		REQUIRE(tmplFound);
		sl.startPos = i;
		sl.color = i;
	}
	REQUIRE_MESSAGE(NewGame::prepareNewGame(msg, world.shared.world->playerTemplates(), settings, maps, RandomAlgorithm::ZH_CarryChain, start, &error), error);
}

WorldHeightMap flatWanderMap(int w, int h)
{
	WorldHeightMap m;
	m.m_width = w;
	m.m_height = h;
	m.m_borderSize = 0;
	m.m_dataSize = w * h;
	m.m_data.assign((size_t)w * h, 0);
	m.m_hasBlendTileData = true;
	m.m_tileNdxes.assign((size_t)w * h, 0);
	m.m_blendTileNdxes.assign((size_t)w * h, 0);
	m.m_extraBlendTileNdxes.assign((size_t)w * h, 0);
	m.m_cliffInfoNdxes.assign((size_t)w * h, 0);
	m.m_planeStride = (w + 7) / 8;
	m.m_blendedTiles.assign(1, TBlendTileInfo());
	m.m_cliffInfo.assign(1, TCliffInfo());
	m.m_boundaries = { { w, h } };
	return m;
}

MapObject makeWaypoint(int id, const char *name, float x, float y, const char *label)
{
	MapObject a;
	a.m_objectName = "*Waypoints/Waypoint";
	a.m_location = { x, y, 0.0f };
	a.m_properties.setInt("waypointID", id);
	a.m_properties.setAsciiString("waypointName", name);
	a.m_properties.setAsciiString("waypointPathLabel1", label);
	a.m_isWaypoint = true;
	return a;
}
} // namespace

TEST_CASE("LOCAL-57 ennorath: TEAM_WANDER moves Dungeons of Erebor civilian teams onto the map (RW 0x7C955B)")
{
	ennorathscenario::World world;
	if (!world.load("LOCAL-57 Dungeons of Erebor"))
	{
		return;
	}
	auto scope = world.shared.world->enterContext();
	ensureEnnorathMapOptions(world);
	NewGameStart start(RandomAlgorithm::ZH_CarryChain);
	setupSkirmishStart(world, "maps/map mp dungeons of erebor/map mp dungeons of erebor.map", start);

	ArchiveW3DFileSource source(*world.merged.fs);
	WW3DAssetManager assets(source);
	LiveGame game(*world.shared.world, *world.merged.fs, assets, world.shared.options);
	LiveGame::Options o;
	o.start = &start;
	std::string error;
	REQUIRE_MESSAGE(game.load(o, &error), error);

	struct Tracked
	{
		std::string tmpl;
		std::string team;
		Coord3D pos0;
	};
	std::map<ObjectID, Tracked> tracked;
	for (Object *obj = game.logic().getFirstObject(); obj; obj = obj->getNextObject())
	{
		const ThingTemplate *tt = obj->getTemplate();
		const Team *tm = obj->getTeam();
		const std::string teamName = (tm && tm->getPrototype()) ? tm->getPrototype()->getName() : "";
		if (tt && (teamName == "Passenger" || teamName == "Party" || teamName == "Miner"))
		{
			tracked[obj->getID()] = { tt->getName(), teamName, *obj->getPosition() };
		}
	}
	REQUIRE(tracked.size() == 52);

	for (int f = 0; f < 300; ++f)
	{
		game.advance(0.2);
	}

	int passengerMoved = 0;
	int torchOnMap = 0;
	int partyMoved = 0;
	int minerMoved = 0;
	for (const auto &kv : tracked)
	{
		Object *obj = game.logic().findObjectByID(kv.first);
		REQUIRE(obj != nullptr);
		REQUIRE(!obj->isEffectivelyDead());
		const Coord3D *p = obj->getPosition();
		const float dx = p->x - kv.second.pos0.x;
		const float dy = p->y - kv.second.pos0.y;
		const bool moved = (dx * dx + dy * dy) > 100.0f;
		if (kv.second.team == "Passenger" && moved)
		{
			++passengerMoved;
			if (kv.second.tmpl == "GondorTownsmanTorch" && p->x > 0.0f && p->x < 5000.0f)
			{
				++torchOnMap;
			}
		}
		else if (kv.second.team == "Party" && moved)
		{
			++partyMoved;
		}
		else if (kv.second.team == "Miner" && moved)
		{
			++minerMoved;
		}
	}
	// RW 0x7C955B: TEAM_WANDER moves Passenger (20 units including 2 GondorTownsmanTorch from outside map bounds), Party (18) and Miner (5)
	CHECK(passengerMoved >= 15); // tolerance: 20 Passenger units include slow/blocked walkers over 300 frames
	CHECK(torchOnMap == 2);
	CHECK(partyMoved >= 12); // tolerance: 18 Party units include local congestion around waypoints
	CHECK(minerMoved >= 3); // tolerance: 5 Miner units include local congestion around waypoints

	game.logic().reset();
}

TEST_CASE("LOCAL-57 ennorath: Lamedon and Rhun Shore civilians exist and Rhun Shore torch bearers patrol")
{
	ennorathscenario::World world;
	if (!world.load("LOCAL-57 Lamedon & Rhun Shore"))
	{
		return;
	}
	auto scope = world.shared.world->enterContext();
	ensureEnnorathMapOptions(world);

	{
		NewGameStart start(RandomAlgorithm::ZH_CarryChain);
		setupSkirmishStart(world, "maps/map mp lamedon/map mp lamedon.map", start);
		ArchiveW3DFileSource source(*world.merged.fs);
		WW3DAssetManager assets(source);
		LiveGame game(*world.shared.world, *world.merged.fs, assets, world.shared.options);
		LiveGame::Options o;
		o.start = &start;
		std::string error;
		REQUIRE_MESSAGE(game.load(o, &error), error);

		int civs = 0;
		for (Object *obj = game.logic().getFirstObject(); obj; obj = obj->getNextObject())
		{
			const ThingTemplate *tt = obj->getTemplate();
			if (tt && tt->getName().find("GondorTowns") == 0)
			{
				++civs;
				CHECK(game.drawables().findByObject(obj->getID()) != nullptr);
			}
		}
		CHECK(civs == 3); // map mp lamedon.map has GondorTownsman, GondorTownsWoman, GondorTownsPair
		game.logic().reset();
	}

	{
		NewGameStart start(RandomAlgorithm::ZH_CarryChain);
		setupSkirmishStart(world, "maps/map mp rhun shore/map mp rhun shore.map", start);
		ArchiveW3DFileSource source(*world.merged.fs);
		WW3DAssetManager assets(source);
		LiveGame game(*world.shared.world, *world.merged.fs, assets, world.shared.options);
		LiveGame::Options o;
		o.start = &start;
		std::string error;
		REQUIRE_MESSAGE(game.load(o, &error), error);

		std::map<ObjectID, Coord3D> torches;
		for (Object *obj = game.logic().getFirstObject(); obj; obj = obj->getNextObject())
		{
			const ThingTemplate *tt = obj->getTemplate();
			if (tt && tt->getName() == "GondorTownsmanTorch")
			{
				torches[obj->getID()] = *obj->getPosition();
			}
		}
		CHECK(torches.size() == 5);
		for (int f = 0; f < 100; ++f)
		{
			game.advance(0.2);
		}
		int moved = 0;
		for (const auto &kv : torches)
		{
			Object *obj = game.logic().findObjectByID(kv.first);
			REQUIRE(obj != nullptr);
			const float dx = obj->getPosition()->x - kv.second.x;
			const float dy = obj->getPosition()->y - kv.second.y;
			if (dx * dx + dy * dy > 100.0f)
			{
				++moved;
			}
		}
		CHECK(moved == 5);
		game.logic().reset();
	}
}

TEST_CASE("LOCAL-57 synthetic: TEAM_WANDER enters AI_WANDER, sets LOCOMOTORSET_WANDER and WANDER flag, xfers, and restores on exit (RW 0x7C955B, 0x750B75, 0x74BD02)")
{
	const char *extraIni = R"(
Locomotor WanderLoco
  Surfaces = GROUND
  Speed = 25
  WanderWidthFactor = 1.5
End
Object WanderCivilian
  KindOf = INFANTRY SELECTABLE
  Body = ActiveBody ModuleTag_Body
    MaxHealth = 100
  End
  Behavior = AIUpdateInterface ModuleTag_AI
  End
  LocomotorSet
    Locomotor = WalkerLoco
    Condition = SET_NORMAL
    Speed = 10
  End
  LocomotorSet
    Locomotor = WanderLoco
    Condition = SET_WANDER
    Speed = 25
  End
End
Object ImmobileProp
  KindOf = STRUCTURE IMMOBILE
  Body = ActiveBody ModuleTag_Body
    MaxHealth = 100
  End
  Behavior = AIUpdateInterface ModuleTag_AI
  End
End
)";
	combattest::CombatWorld w(extraIni, 101);
	WorldHeightMap hmap = flatWanderMap(101, 101);
	MapChunks chunks;
	chunks.objects = {
		makeWaypoint(1, "WP1", 200.0f, 200.0f, "WanderPath"),
		makeWaypoint(2, "WP2", 400.0f, 200.0f, "WanderPath"),
		makeWaypoint(3, "WP3", 600.0f, 200.0f, "WanderPath"),
	};
	chunks.waypointLinks = { { 1, 2 }, { 2, 3 } };
	TerrainLogic terrain;
	std::vector<std::string> problems;
	terrain.init(hmap, chunks, &problems);
	REQUIRE(problems.empty());
	w.logic->setTerrain(&terrain);

	std::vector<ScriptList> lists(w.players.getPlayerCount());
	std::vector<ScriptEngine::SideScripts> sides((size_t)w.players.getPlayerCount());
	for (int i = 0; i < w.players.getPlayerCount(); ++i)
	{
		sides[(size_t)i].sideName = w.players.getNthPlayer(i)->getPlayerName();
		sides[(size_t)i].lists.push_back(&lists[(size_t)i]);
	}
	w.logic->scriptEngine().newGame(sides, nullptr, nullptr, false);

	Object *civ = w.unit("WanderCivilian", 'A', 380.0f, 200.0f);
	Object *prop = w.unit("ImmobileProp", 'A', 100.0f, 100.0f);
	AIUpdateInterface *ai = civ->getAIUpdateInterface();
	AIUpdateInterface *propAi = prop->getAIUpdateInterface();
	REQUIRE(ai != nullptr);
	REQUIRE(propAi != nullptr);
	CHECK(ai->curLocomotorSet() == LOCOMOTORSET_NORMAL);
	// RW 0x664858: immobile objects refuse aiWander
	CHECK(!propAi->aiWander(2, CMD_FROM_SCRIPT));

	// Issue TEAM_WANDER on teamA along "WanderPath" (RW 0x7CC3FF -> 0x7C955B)
	Team *t = w.teamA();
	const std::string teamQual = "Alice/" + t->getPrototype()->getName();
	ScriptActionRec act = scripttest::act("TEAM_WANDER", { scripttest::sp(3, teamQual), scripttest::sp(24, "WanderPath") });
	ScriptActions::execute(w.logic->scriptEngine(), act);
	CHECK(w.logic->scriptEngine().stats().unportedActions.empty());

	w.logic->runLogicFrame();
	CHECK(ai->curLocomotorSet() == LOCOMOTORSET_WANDER);
	CHECK(ai->currentStateId() == (unsigned)AI_WANDER);
	const int wanderBit = CombatNames::modelCondition("WANDER");
	const bool wanderBitValid = wanderBit >= 0;
	REQUIRE(wanderBitValid);
	const bool wanderSet = (civ->getModelConditionBits()[(size_t)wanderBit / 32u] & (1u << (wanderBit % 32))) != 0;
	CHECK(wanderSet);

	// xfer round-trip mid-wander (RW 0x742BB6)
	std::vector<std::uint8_t> bytes;
	{
		XferSave save;
		ai->stateMachine().xfer(save);
		bytes = save.data();
	}
	{
		XferLoad load(bytes);
		ai->stateMachine().xfer(load);
		REQUIRE(load.atEnd());
	}
	CHECK(ai->currentStateId() == (unsigned)AI_WANDER);

	// Step until end of WanderPath (WP3 has no links) -> AI_IDLE, WANDER cleared, LOCOMOTORSET_NORMAL restored (RW 0x74BD02)
	for (int i = 0; i < 150; ++i)
	{
		w.logic->runLogicFrame();
	}
	CHECK(ai->currentStateId() == (unsigned)AI_IDLE);
	CHECK(ai->curLocomotorSet() == LOCOMOTORSET_NORMAL);
	CHECK(((civ->getModelConditionBits()[(size_t)wanderBit / 32u] & (1u << (wanderBit % 32))) == 0));
	CHECK(civ->getPosition()->x > 550.0f); // tolerance: WanderWidthFactor 1.5 adds +/-20 cell offset around WP3 (x=600)

	w.logic->setTerrain(nullptr);
}
