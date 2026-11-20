// OpenBFME unit tests: the map sweep of Ennorath (owner 2026-11-20, after the randomizer maps did not start). GPL-3.0.
//
// One map per process: tools/maps/map_sweep.py first asks this case for the list of the mounted maps (OPENBFME_MAP_SWEEP_LIST), then runs it once per
// map (OPENBFME_MAP_SWEEP_MAP), so a crash or a hang of one map is that map's row, not the end of the sweep. Each run starts a two-slot skirmish on the
// map as the game's start does (NewGame::prepareNewGame on the map cache, LiveGame::load with the start: the map's map.ini, its objects, its scripts,
// the starting bases), plays it for OPENBFME_MAP_SWEEP_SECONDS game seconds (default 60) with a computer opponent, and appends one row to
// OPENBFME_MAP_SWEEP_REPORT: what failed at the load and what the play added. Nothing here is an expectation of the engine's own output: the case
// is a report (it fails only when it cannot run at all); the table's rows are the tasks.

#include "doctest.h"
#include "EnnorathScenario.h"
#include "HudTestUtil.h"
#include "RetailTestMount.h"

#include "Common/ArchiveFileSystem.h"
#include "GameClient/GUI/Skirmish/IniSkirmishSetupSource.h"
#include "GameClient/LiveGame.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/NewGame/NewGame.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>
#include <vector>

namespace
{
std::string envOr(const char *name, const char *fallback)
{
	const char *v = std::getenv(name);
	return v && *v ? v : fallback;
}

// one TSV field: tabs and line breaks of an engine message become spaces
std::string field(std::string s)
{
	std::replace(s.begin(), s.end(), '\t', ' ');
	std::replace(s.begin(), s.end(), '\n', ' ');
	std::replace(s.begin(), s.end(), '\r', ' ');
	return s.size() > 400 ? s.substr(0, 400) + "..." : s;
}

std::string joined(const std::vector<std::string> &v, size_t from, size_t most)
{
	std::string out;
	for (size_t i = from; i < v.size() && i < from + most; ++i)
	{
		out += (out.empty() ? "" : " | ") + field(v[i]);
	}
	return out;
}

int templateIndex(const PlayerTemplateStore &store, const std::string &name)
{
	for (int i = 0; i < store.getPlayerTemplateCount(); ++i)
	{
		if (store.getNthPlayerTemplate(i)->getName() == name)
		{
			return i;
		}
	}
	return -1;
}
} // namespace

TEST_CASE("ENNORATH sweep maps: one map of the mod's install starts a skirmish and plays (a report row; tools/maps/map_sweep.py runs every map)")
{
	const char *list = std::getenv("OPENBFME_MAP_SWEEP_LIST");
	const char *mapPath = std::getenv("OPENBFME_MAP_SWEEP_MAP");
	if (!list && !mapPath)
	{
		retailtest::printSkip("ENNORATH sweep maps (set OPENBFME_MAP_SWEEP_LIST or OPENBFME_MAP_SWEEP_MAP)");
		return;
	}
	ennorathscenario::World world;
	if (!world.load("ENNORATH sweep maps"))
	{
		return;
	}
	hudtest::SharedWorld &s = world.shared;
	auto scope = s.world->enterContext();
	ArchiveFileSystem &fs = *s.mount->fs;

	if (list)
	{
		FilenameList files;
		fs.getFileListInDirectory("", "maps\\", "*.map", files, true);
		FILE *f = std::fopen(list, "wb");
		REQUIRE_MESSAGE(f, "cannot write " << list);
		for (const std::string &name : files)
		{
			std::fprintf(f, "%s\n", name.c_str());
		}
		std::fclose(f);
		MESSAGE(files.size() << " maps listed into " << list);
		return;
	}

	const std::string map = mapPath;
	const std::string reportPath = envOr("OPENBFME_MAP_SWEEP_REPORT", "");
	const double seconds = std::atof(envOr("OPENBFME_MAP_SWEEP_SECONDS", "60").c_str());
	const std::string factions[2] = { envOr("OPENBFME_MAP_SWEEP_FACTION", "FactionMen"), envOr("OPENBFME_MAP_SWEEP_ENEMY", "FactionMordor") };

	std::string stage = "map cache", error;
	bool loaded = false;
	std::vector<std::string> loadErrors, playErrors, mapIniErrors, startErrors;
	double played = 0.0;
	size_t objects = 0;
	try
	{
		GameLogicSettings settings;
		std::vector<MapCacheEntry> maps;
		REQUIRE_MESSAGE(GameLogicSettingsLoader::load(fs, settings, &error), error);
		REQUIRE_MESSAGE(IniSkirmishSetupSource::loadMapCache(fs, maps, &error), error);
		NewGameMessage m;
		m.game.mapName = map; // the map cache names a map "maps/<dir>/<file>.map" (the archive lists it with backslashes)
		std::replace(m.game.mapName.begin(), m.game.mapName.end(), '\\', '/');
		m.game.seed = 5;
		m.game.startingCash = 1000;
		for (int i = 0; i < 2; ++i)
		{
			SkirmishGameSlot &sl = m.game.slots[(size_t)i];
			sl.state = i == 0 ? SLOT_PLAYER : SLOT_EASY_AI;
			sl.name = i == 0 ? u"Host" : u"Easy";
			sl.accepted = true;
			sl.playerTemplate = templateIndex(s.world->playerTemplates(), factions[i]);
			REQUIRE_MESSAGE(sl.playerTemplate != -1, "no PlayerTemplate " << factions[i]); // -1: templateIndex found no such name
			sl.startPos = i;
			sl.color = i;
		}
		NewGameStart start(RandomAlgorithm::ZH_CarryChain);
		stage = "prepareNewGame";
		if (NewGame::prepareNewGame(m, s.world->playerTemplates(), settings, maps, RandomAlgorithm::ZH_CarryChain, start, &error))
		{
			stage = "load";
			ArchiveW3DFileSource source(fs);
			WW3DAssetManager assets(source);
			LiveGame game(*s.world, fs, assets, s.options);
			LiveGame::Options o;
			o.start = &start;
			loaded = game.load(o, &error);
			LiveGame::Report r = game.report();
			loadErrors = r.errors;
			mapIniErrors = r.mapIniErrors;
			startErrors = r.startErrors;
			if (loaded)
			{
				stage = "play";
				for (; played < seconds; played += 1.0)
				{
					game.advance(1.0);
				}
				r = game.report();
				objects = r.mapObjects;
				for (const std::string &e : r.errors)
				{
					if (std::find(loadErrors.begin(), loadErrors.end(), e) == loadErrors.end())
					{
						playErrors.push_back(e);
					}
				}
				stage = "done";
			}
			game.logic().reset();
		}
	}
	catch (const std::exception &e)
	{
		error = std::string("exception at ") + stage + ": " + e.what();
	}

	const std::string status = stage == "done" ? (loadErrors.empty() && playErrors.empty() ? "ok" : "errors") : "failed at " + stage;
	MESSAGE(map << ": " << status << " " << field(error) << " (load errors " << loadErrors.size() << ", play errors " << playErrors.size() << ")");
	if (!reportPath.empty())
	{
		FILE *f = std::fopen(reportPath.c_str(), "ab");
		REQUIRE_MESSAGE(f, "cannot write " << reportPath);
		// map, status, error, load errors (count, first 5), map.ini errors (count, first 3), start errors, play errors (count, first 5), seconds played, map objects
		std::fprintf(f, "%s\t%s\t%s\t%zu\t%s\t%zu\t%s\t%s\t%zu\t%s\t%.0f\t%zu\n", map.c_str(), status.c_str(), field(error).c_str(), loadErrors.size(),
			joined(loadErrors, 0, 5).c_str(), mapIniErrors.size(), joined(mapIniErrors, 0, 3).c_str(), joined(startErrors, 0, 3).c_str(), playErrors.size(),
			joined(playErrors, 0, 5).c_str(), played, objects);
		std::fclose(f);
	}
}
