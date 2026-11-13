// OpenBFME. GPL-3.0.
//
// P-76 (docs/ennorath/PROBLEMS.md): Ennorath's Isengard fortress plays the defiled-ground loop. Ennorath 1.9.8.2 data:
// object/evilfaction/structures/isengard/isengardfortress.ini, Object IsengardFortressCitadel: SoundAmbient = AmbientIsengartBoden;
// includes/soundeffects.inc: AudioEvent AmbientIsengartBoden (Sounds = Isengard_ambient, Control = loop, Type = world shrouded everyone, Limit 3,
// Priority lowest); the file data\audio\sounds\Isengard_ambient.wav is in ______enrth_sounds2.big. playertemplate.ini: FactionIsengard
// StartingBuilding = IsengardFortress. Skips without ROTWK_INSTALL / BFME2_INSTALL / OPENBFME_ENNORATH_INSTALL.
#include "EnnorathScenario.h"
#include "HudAudioTestUtil.h"
#include "HudTestUtil.h"
#include "Common/PlayerList.h"
#include "Common/PlayerTemplate.h"
#include "GameClient/GUI/Skirmish/IniSkirmishSetupSource.h"
#include "GameClient/LiveGame.h"
#include "GameClient/LiveGameAudio.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/NewGame/NewGame.h"
#include "Libraries/WWVegas/WW3D2/assetmgr.h"

#include <cstdio>
#include <string>
#include <vector>

namespace
{
void dumpObjects(GameLogic &logic)
{
	for (Object *o = logic.getFirstObject(); o; o = o->getNextObject())
	{
		const ThingTemplate *t = o->getTemplate();
		std::printf("P-76 object %u %s constr=%d pos=(%.0f %.0f %.0f)\n", (unsigned)o->getID(), t ? t->getName().c_str() : "?", o->isUnderConstruction() ? 1 : 0,
		            o->getPosition()->x, o->getPosition()->y, o->getPosition()->z);
	}
}
} // namespace

TEST_CASE("P-76 ennorath: the Isengard fortress's citadel plays AmbientIsengartBoden once it stands (SoundAmbient)")
{
	ennorathscenario::World world;
	if (!world.load("P-76 isengard ambient"))
	{
		return;
	}
	hudtest::Rig game(world.shared);
	game.game->shroud().revealMapForPlayerPermanently(game.index());
	hudaudio::RetailAudio audio(*world.merged.fs);
	std::string err;
	std::unique_ptr<LiveGameAudio> live = LiveGameAudio::attachToInstalled(*game.game, nullptr, &err, nullptr);
	REQUIRE_MESSAGE(live != nullptr, err);
	REQUIRE(audio.ini.infos.find("AmbientIsengartBoden") != nullptr);

	const Coord3D at = game.freeSpot(1800, 1800, 400.0f);
	// the skirmish start's IsengardFortress unpacks its castle (CastleBehavior, CastleToUnpackForFaction ... Fortress_Isengard), which places the
	// fortress's citadel; createObject does not unpack, so the citadel the castle places is made directly
	Object *fortress = game.game->createObject("IsengardFortressCitadel", game.index(), at, 0.0f, &err);
	REQUIRE_MESSAGE(fortress != nullptr, err);
	std::printf("P-76 citadel under construction: %d\n", fortress->isUnderConstruction() ? 1 : 0);
	// the camera of a skirmish start: above and behind the base, looking at it (GodotGameAudio feeds the same call)
	audio.manager->updateMicrophone(Coord3D{ at.x, at.y - 300.0f, at.z + 450.0f }, at);
	double t = 1000.0;
	for (int i = 0; i < 90; ++i)
	{
		game.game->advance(0.033);
		live->update();
		t += 33.0;
		audio.device->setTime(t);
		audio.manager->update(t);
	}
	const auto playing = live->playingAmbients();
	bool boden = false;
	for (const auto &kv : playing)
	{
		std::printf("P-76 ambient %u %s\n", (unsigned)kv.first, kv.second.c_str());
		boden = boden || kv.second == "AmbientIsengartBoden";
	}
	const LiveGameAudio::Stats &s = live->stats();
	std::printf("P-76 stats started=%llu outOfRange=%llu unknown=%llu playing=%zu\n", (unsigned long long)s.ambientStarted,
	            (unsigned long long)s.ambientOutOfRange, (unsigned long long)s.ambientUnknownEvent, s.ambientPlaying);
	if (!boden)
	{
		dumpObjects(game.logic());
		for (const AudioLogEntry &e : audio.manager->takeEventLog())
		{
			if (e.event.find("Isengart") != std::string::npos)
			{
				std::printf("P-76 log %s %s\n", e.event.c_str(), e.outcome.c_str());
			}
		}
	}
	CHECK(boden);
	int starts = 0;
	for (const auto &d : audio.device->log())
	{
		if (d.kind == SimulatedAudioDevice::LogEntry::Start && d.file.find("Isengard_ambient") != std::string::npos)
		{
			++starts;
		}
	}
	CHECK(starts >= 1); // tolerance: a loop may restart its file within the 90 frames; 0 is the symptom (no sound)
}

// The owner's audio log (2026-11-13): AmbientIsengartBoden started once and was never heard again. RotWK stops a looping sample that falls below
// MinSampleVolume but queues the event again for a later check (RW 0x45BC6D .. 0x45BC90 -> RW 0x45A188), so the loop comes back when the camera
// returns; the drawable starts its ambient only once (RW 0x67866C), so nothing else restarts it.
TEST_CASE("P-76 ennorath: the citadel's AmbientIsengartBoden comes back after the camera leaves the base and returns")
{
	ennorathscenario::World world;
	if (!world.load("P-76 isengard ambient return"))
	{
		return;
	}
	hudtest::Rig game(world.shared);
	game.game->shroud().revealMapForPlayerPermanently(game.index());
	hudaudio::RetailAudio audio(*world.merged.fs);
	std::string err;
	std::unique_ptr<LiveGameAudio> live = LiveGameAudio::attachToInstalled(*game.game, nullptr, &err, nullptr);
	REQUIRE_MESSAGE(live != nullptr, err);
	const Coord3D at = game.freeSpot(1800, 1800, 400.0f);
	REQUIRE_MESSAGE(game.game->createObject("IsengardFortressCitadel", game.index(), at, 0.0f, &err) != nullptr, err);
	double t = 1000.0;
	auto run = [&](const Coord3D &camera, const Coord3D &lookAt, int frames) {
		for (int i = 0; i < frames; ++i)
		{
			audio.manager->updateMicrophone(camera, lookAt);
			game.game->advance(0.033);
			live->update();
			t += 33.0;
			audio.device->setTime(t);
			audio.manager->update(t);
		}
	};
	auto count = [&](SimulatedAudioDevice::LogEntry::Kind kind) {
		int n = 0;
		for (const auto &d : audio.device->log())
		{
			n += (d.kind == kind && d.file.find("Isengard_ambient") != std::string::npos) ? 1 : 0;
		}
		return n;
	};
	const Coord3D nearCam{ at.x, at.y - 300.0f, at.z + 450.0f };
	const Coord3D farLook{ at.x + 3000.0f, at.y, at.z };
	const Coord3D farCam{ farLook.x, farLook.y - 300.0f, farLook.z + 450.0f };
	run(nearCam, at, 60);
	const int startsNear = count(SimulatedAudioDevice::LogEntry::Start);
	REQUIRE(startsNear >= 1); // tolerance: a loop may restart its file; 0 = the first start already failed (the other cases)
	run(farCam, farLook, 60);
	const int stopsFar = count(SimulatedAudioDevice::LogEntry::Stop);
	const int startsFar = count(SimulatedAudioDevice::LogEntry::Start);
	CHECK(stopsFar >= 1); // tolerance: the cull stops the playing voice once; a restart in between may add one; 0 = the cull did not run
	run(nearCam, at, 60);
	const int startsBack = count(SimulatedAudioDevice::LogEntry::Start) - startsFar;
	std::printf("P-76 return: starts near %d, stops far %d, starts far %d, starts after return %d\n", startsNear, stopsFar, startsFar, startsBack);
	CHECK(startsBack >= 1); // tolerance: the requeued loop may restart its file; 0 is the symptom (the loop is gone for good)
}

// The skirmish start itself: FactionIsengard's StartingBuilding IsengardFortress unpacks Fortress_Isengard (BFME2 bases.big: the layout places
// IsengardFortressCitadel), the camera looks at the base from the start camera's distance (the retail tactical camera's default: about 420 back, 520 up).
TEST_CASE("P-76 ennorath: an Isengard skirmish start plays AmbientIsengartBoden from the unpacked fortress citadel")
{
	ennorathscenario::World world;
	if (!world.load("P-76 isengard start"))
	{
		return;
	}
	std::string err;
	GameLogicSettings settings;
	REQUIRE_MESSAGE(GameLogicSettingsLoader::load(*world.merged.fs, settings, &err), err);
	std::vector<MapCacheEntry> mapCache;
	REQUIRE_MESSAGE(IniSkirmishSetupSource::loadMapCache(*world.merged.fs, mapCache, &err), err);
	NewGameMessage m;
	for (const MapCacheEntry &e : mapCache)
	{
		if (e.name.find("map mp fall back") != std::string::npos)
		{
			m.game.mapName = e.name;
		}
	}
	REQUIRE_MESSAGE(!m.game.mapName.empty(), "no map mp fall back in the Ennorath map cache");
	m.game.seed = 4711;
	m.game.startingCash = 1500;
	const char *factions[2] = { "FactionIsengard", "FactionMen" };
	for (int i = 0; i < 2; ++i)
	{
		SkirmishGameSlot &slot = m.game.slots[i];
		slot.state = i == 0 ? SLOT_PLAYER : SLOT_MED_AI;
		slot.name = std::u16string(u"Player") + (char16_t)(u'0' + i);
		slot.playerTemplate = -1;
		for (int t = 0; t < world.shared.world->playerTemplates().getPlayerTemplateCount(); ++t)
		{
			if (world.shared.world->playerTemplates().getNthPlayerTemplate(t)->getName() == factions[i])
			{
				slot.playerTemplate = t;
			}
		}
		REQUIRE(slot.playerTemplate >= 0); // tolerance: any template index; -1 = the faction is missing
		slot.startPos = i;
		slot.color = i;
		slot.teamNumber = i;
	}
	NewGameStart start(RandomAlgorithm::ZH_CarryChain);
	REQUIRE_MESSAGE(NewGame::prepareNewGame(m, world.shared.world->playerTemplates(), settings, mapCache, RandomAlgorithm::ZH_CarryChain, start, &err), err);
	ArchiveW3DFileSource source(*world.merged.fs);
	WW3DAssetManager assets(source);
	LiveGame game(*world.shared.world, *world.merged.fs, assets, world.shared.options);
	LiveGame::Options o;
	o.start = &start;
	REQUIRE_MESSAGE(game.load(o, &err), err);
	Player *local = game.players().findPlayerWithName(game.report().startSlotPlayers[0]);
	REQUIRE(local != nullptr);
	game.players().setLocalPlayer(local);
	hudaudio::RetailAudio audio(*world.merged.fs);
	std::unique_ptr<LiveGameAudio> live = LiveGameAudio::attachToInstalled(game, nullptr, &err, nullptr);
	REQUIRE_MESSAGE(live != nullptr, err);
	Object *citadel = nullptr;
	for (Object *obj = game.logic().getFirstObject(); obj; obj = obj->getNextObject())
	{
		if (obj->getControllingPlayer() == local)
		{
			std::printf("P-76 start local %u %s constr=%d\n", (unsigned)obj->getID(), obj->getTemplate() ? obj->getTemplate()->getName().c_str() : "?",
			            obj->isUnderConstruction() ? 1 : 0);
		}
		if (obj->getTemplate() && obj->getTemplate()->getName() == "IsengardFortressCitadel" && obj->getControllingPlayer() == local)
		{
			citadel = obj;
		}
	}
	REQUIRE_MESSAGE(citadel != nullptr, "the unpacked castle has no IsengardFortressCitadel of the local player");
	const Coord3D at = *citadel->getPosition();
	audio.manager->updateMicrophone(Coord3D{ at.x, at.y - 420.0f, at.z + 520.0f }, at);
	double t = 1000.0;
	for (int i = 0; i < 90; ++i)
	{
		game.advance(0.033);
		live->update();
		t += 33.0;
		audio.device->setTime(t);
		audio.manager->update(t);
	}
	bool boden = false;
	for (const auto &kv : live->playingAmbients())
	{
		std::printf("P-76 start ambient %u %s\n", (unsigned)kv.first, kv.second.c_str());
		boden = boden || (kv.first == citadel->getID() && kv.second == "AmbientIsengartBoden");
	}
	const LiveGameAudio::Stats &s = live->stats();
	std::printf("P-76 start stats started=%llu outOfRange=%llu unknown=%llu playing=%zu constr=%d\n", (unsigned long long)s.ambientStarted,
	            (unsigned long long)s.ambientOutOfRange, (unsigned long long)s.ambientUnknownEvent, s.ambientPlaying, citadel->isUnderConstruction() ? 1 : 0);
	CHECK(boden);
	game.logic().reset();
}