// Ennorath scenarios: RotWK + BFME2 + the player's own Ennorath overlay, loaded once for the process (as hudtest::shared() the retail world), in an Arena of real data. GPL-3.0.
// The expected values of a scenario come from the mod's INI (name the file in a comment), never from the port. The mod stays outside the repository (PLAN rule 6).
#pragma once
#include "doctest.h"
#include "StructureArena.h"
#include "Common/ModMount.h"
#include "Common/RetailArchivePolicy.h"

#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace ennorathscenario
{
// what the mount and the parse make: the world's data does not change in a game, each case's game is its own
struct LoadedWorld
{
	Win32BIGFileSystem pure;
	retailtest::Mount merged;
	hudtest::SharedWorld shared;
	bool loaded = false;
};

inline LoadedWorld &loadedWorld()
{
	static LoadedWorld world;
	return world;
}

struct World
{
	std::string rotwk = env("ROTWK_INSTALL"), bfme2 = env("BFME2_INSTALL"), mod = env("OPENBFME_ENNORATH_INSTALL");
	Win32BIGFileSystem &pure = loadedWorld().pure;
	retailtest::Mount &merged = loadedWorld().merged;
	hudtest::SharedWorld &shared = loadedWorld().shared;
	std::unique_ptr<RetailObjectWorld::ContextScope> context;

	static std::string env(const char *name)
	{
		const char *value = std::getenv(name);
		return value ? value : "";
	}
	// an object of the mod's own world (structtest::Arena::place reads the shared retail world, which has no mod)
	Object *place(structtest::Arena &arena, const char *templateName, int side, float x, float y)
	{
		const ThingTemplate *t = shared.world->things().findTemplate(templateName);
		REQUIRE_MESSAGE(t != nullptr, "template of the mod: " << templateName);
		Object *o = arena.logic.newObject(t, arena.player(side)->getDefaultTeam(), ObjectStatusMaskType{});
		REQUIRE_MESSAGE(o != nullptr, templateName);
		const Coord3D p{ x, y, 0.0f };
		o->setPosition(&p);
		return testutil::completeHorde(o);
	}
	bool available() const { return !rotwk.empty() && !bfme2.empty() && !mod.empty(); }
	// false with a printed SKIP when the three installs are not set; a mount or parse failure fails the test
	bool load(const char *what)
	{
		if (!available())
		{
			retailtest::printSkip((std::string(what) + " (set ROTWK_INSTALL, BFME2_INSTALL, OPENBFME_ENNORATH_INSTALL)").c_str());
			return false;
		}
		if (loadedWorld().loaded)
		{
			context = shared.world->enterContext();
			return true;
		}
		std::string err;
		std::vector<RetailInstall> installs(2);
		installs[0].label = "rotwk";
		installs[0].root = rotwk;
		installs[1].label = "bfme2";
		installs[1].root = bfme2;
		REQUIRE_MESSAGE(RetailArchivePolicy::loadBuiltin("rotwk-201", installs[0].policy, &err), err);
		REQUIRE_MESSAGE(RetailArchivePolicy::loadBuiltin("bfme2-106", installs[1].policy, &err), err);
		RetailMountOptions options;
		options.md5CachePath = (std::filesystem::temp_directory_path() / "openbfme-ennorath-scenario-md5.tsv").u8string();
		const RetailMountReport retail = MountRetailArchives(pure, installs, options);
		REQUIRE_MESSAGE(retail.ok, "retail mount failed");
		merged.fs = std::make_unique<Win32BIGFileSystem>();
		ModMountOptions modOptions;
		modOptions.rotwkInstallRoot = rotwk;
		modOptions.md5CachePath = options.md5CachePath;
		const ModMountReport mounted = MountInstallOverlay(*merged.fs, retail, mod, modOptions);
		REQUIRE_MESSAGE(mounted.ok, "Ennorath overlay mount failed");
		shared.mount = &merged;
		shared.world = std::make_unique<RetailObjectWorld>(*merged.fs);
		{
			const auto loadContext = shared.world->enterContext();
			REQUIRE_MESSAGE(shared.world->load(&err), err);
		}
		REQUIRE(shared.world->report().errors.empty());
		loadedWorld().loaded = true;
		context = shared.world->enterContext();
		return true;
	}
};
} // namespace ennorathscenario
