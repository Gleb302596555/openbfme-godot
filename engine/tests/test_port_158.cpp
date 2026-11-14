// OpenBFME unit tests. GPL-3.0.
//
// PORT-158 (issue #376): the loading-screen music plays together with the skirmish menu music (P-84).
//
// TARGET FACTS (RotWK game.dat, read with rw.py; BFME2 1.06 decomp is the same code):
//   * AptLoadScreen::init RW 0x81CB08: the local slot's PlayerTemplate LoadScreenMusic (+ 0x14C, RW 0x81D203) is looked up in
//     TheAudio (the ONE audio manager, RW 0xDE42FC) by findAudioEventInfo (vslot 0x12C, RW 0x81D225); an unknown name stops
//     and plays nothing (RW 0x81D22B -> 0x81D295).
//   * The music that plays is stopped with a fade: vslot 0x8C(2, 1, 1) at RW 0x81D242 (DONOR BFME2 MilesAudioManager.cpp:
//     rva00055B40 queues request 2, rva00057297 -> removeCurrentlyPlayingMusic(viewType, fade = !at10 = true): the track
//     leaves the playing list and fades with m_at30 = timeToFadeAudio; processFade advances that counter by ELAPSED ms,
//     m_at30 += elapsed, never by a count of frames).
//   * Then the load event is added (AudioEventRTS(info, view type 2) RW 0x6DA95E, shouldFade off RW 0x6DA774, vslot 0x64 at
//     RW 0x81D278) and the manager is updated once (vslot 0x28, RW 0x81D283). Exactly one music track plays during the load.
//   * The port keeps the menu music in a second manager (the Godot host's shell child, for the BFME2 shell source) that the
//     loading screen's old device path could not reach, and its fade counted manager ticks, which the synchronous skirmish
//     load drops (AudioManager::update's catch-up cap): the menu track stayed audible next to the loading music.
//
// Expectations come from the addresses above: after the load switch the menu track is not playing on either manager and its
// fade completes on elapsed time (even delivered in one post-stall update, as the load's progress pumps deliver it), the
// load track is the one music playing, and an unknown load event stops nothing.

#include "doctest.h"

#include "IniTestUtil.h"

#include "Common/Audio/AudioEntryPoints.h"
#include "Common/Audio/GameAudio.h"
#include "Common/Audio/SimulatedAudioDevice.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace
{
const char kIni[] =
	"AudioSettings\n  AudioRoot = Data\\Audio\n  SoundsFolder = Sounds\n  MusicFolder = Tracks\n  StreamingFolder = Speech\n  AmbientStreamFolder = AmbientStreams\n  SoundsExtension = wav\n"
	"  SampleCount2D = 2\n  SampleCount3D = 3\n  StreamCount = 3\n  MinSampleVolume = 2%\n  GlobalMinRange = 5000\n  GlobalMaxRange = 5000000\n  TimeToFadeAudio = 2000\n"
	"  AmbientStreamHysteresisVolume = 10\n  DefaultSoundVolume = 100%\n  DefaultVoiceVolume = 100%\n  DefaultMusicVolume = 100%\n  DefaultAmbientVolume = 100%\n  DefaultMovieVolume = 100%\nEnd\n"
	"AudioEvent DefaultSoundEffect\n  Volume = 100\n  Priority = high\n  Limit = 4\n  MinRange = 160\n  MaxRange = 640\n  PitchShift = 0 0\n  PlayPercent = 100\n  Type = ui everyone\n  Type = +DEFAULT\nEnd\n"
	"MusicTrack DefaultMusicTrack\n  Filename = NoFilename\n  Volume = 100\n  Type = DEFAULT\n  SubmixSlider = music\nEnd\n"
	"DialogEvent DefaultDialog\n  Volume = 100\n  Type = ui everyone\n  Type = +DEFAULT\nEnd\n"
	"AmbientStream DefaultAmbientStream\n  Filename = NoFilename\n  Volume = 100\n  Type = DEFAULT\n  MinRange = 100\n  MaxRange = 1000\n  SubmixSlider = ambient\nEnd\n"
	// the menu music: a looping PLAY_ONE multisound of music tracks (retail MiscAudio LowLODShellMusic = ShellLowLOD, the same shape)
	"MusicTrack ShellTrackA\n  Filename = shella.mp3\nEnd\n"
	"MusicTrack ShellTrackB\n  Filename = shellb.mp3\nEnd\n"
	"Multisound ShellMusic\n  Control = PLAY_ONE LOOP\n  Subsounds = ShellTrackA ShellTrackB\nEnd\n"
	// the faction's loading track (the mod's LoadScreenMusic names are MusicTracks of this shape)
	"MusicTrack LoadTrack\n  Filename = loadtrack.mp3\nEnd\n";

struct Rig
{
	initest::Fixture fx;
	AudioIniState ini;
	std::unique_ptr<AudioAssetCache> cache;
	std::unique_ptr<SimulatedAudioDevice> device;
	std::unique_ptr<AudioManager> mgr;
	double t = 0.0;

	Rig()
	{
		fx.mount({ { "data\\audio\\tracks\\shella.mp3", "x" }, { "data\\audio\\tracks\\shellb.mp3", "x" }, { "data\\audio\\tracks\\loadtrack.mp3", "x" } });
		ini.registerBlocks(fx.env.blocks);
		const std::string err = initest::loadError(fx.env, "audio.ini", kIni);
		REQUIRE_MESSAGE(err.empty(), err);
		cache = std::make_unique<AudioAssetCache>(&fx.fsys, 64u * 1024u * 1024u);
		device = std::make_unique<SimulatedAudioDevice>(cache.get());
		device->setFileLength("Data\\Audio\\Tracks\\shella.mp3", 60000.0);
		device->setFileLength("Data\\Audio\\Tracks\\shellb.mp3", 60000.0);
		device->setFileLength("Data\\Audio\\Tracks\\loadtrack.mp3", 60000.0);
		mgr = std::make_unique<AudioManager>(ini, *cache, *device, RandomAlgorithm::ZH_CarryChain, 1234);
		mgr->update(0.0);
	}

	void run(double ms)
	{
		const double end = t + ms;
		while (t < end)
		{
			t += 10.0;
			device->setTime(t);
			mgr->update(t);
		}
	}

	// one update after a stall of `ms` (the synchronous load between two progress pumps), as AudioManager::update sees it
	void stall(double ms)
	{
		t += ms;
		device->setTime(t);
		mgr->update(t);
	}
};

// unregisters the AudioApi managers a test installed, even on a doctest failure path out of the test body
struct ApiGuard
{
	~ApiGuard()
	{
		AudioApi::install(nullptr);
		AudioApi::installShellMusic(nullptr);
	}
};

size_t liveStreamVoices(const Rig &r)
{
	size_t n = 0;
	for (const auto &kv : r.device->voices())
	{
		if (!kv.second.stopped && kv.second.start.kind == VoiceKind::Stream)
		{
			++n;
		}
	}
	return n;
}
} // namespace

TEST_CASE("PORT-158: starting a skirmish load stops the menu music in the shell manager; exactly one track plays")
{
	Rig mainRig;  // the installed manager (the game)
	Rig shellRig; // the shell's own manager, where this port keeps the menu music
	ApiGuard guard;
	AudioApi::install(mainRig.mgr.get());
	AudioApi::installShellMusic(shellRig.mgr.get());

	// the menu music plays (the shell's looping multisound), the game manager has no music
	shellRig.mgr->playMusic("ShellMusic");
	shellRig.run(200.0);
	mainRig.run(200.0);
	REQUIRE(shellRig.mgr->isMusicPlaying());
	CHECK_FALSE(mainRig.mgr->isMusicPlaying());

	// an unknown LoadScreenMusic stops and plays nothing (RW 0x81D22B -> 0x81D295)
	CHECK(AudioApi::playLoadScreenMusic("NoSuchLoadTrack") == AHSV_NoSound);
	CHECK(shellRig.mgr->isMusicPlaying());
	CHECK(shellRig.mgr->fadingEventNames().empty());
	CHECK_FALSE(mainRig.mgr->isMusicPlaying());

	// the load begins: the faction's track (RW 0x81D242 stop with fade, 0x81D278 add, 0x81D283 one update)
	const AudioHandle handle = AudioApi::playLoadScreenMusic("LoadTrack");
	REQUIRE(handle == AHSV_FirstHandle); // the game manager's first event is the load track (its handle counter starts there)
	CHECK(mainRig.mgr->isMusicPlaying());
	CHECK(mainRig.mgr->getMusicTrackName() == "LoadTrack");
	// the menu track is out of the playing set on its own manager too (it may only be fading now)
	CHECK_FALSE(shellRig.mgr->isMusicPlaying());

	// the load takes its time; both managers are pumped (the progress callback's tick_audio) and time passes
	shellRig.run(3000.0);
	mainRig.run(3000.0);
	CHECK(shellRig.mgr->fadingEventNames().empty()); // the fade is over: the menu track is gone, not hanging audible
	CHECK(liveStreamVoices(shellRig) == 0);
	CHECK(mainRig.mgr->isMusicPlaying());
	CHECK(mainRig.mgr->getMusicTrackName() == "LoadTrack");
	CHECK(mainRig.mgr->playingCount(VoiceKind::Stream) == 1); // exactly one music track plays during the load
	CHECK(liveStreamVoices(mainRig) == 1);
}

TEST_CASE("PORT-158: the menu track's fade runs on elapsed time across the synchronous load's stall")
{
	Rig r;
	r.mgr->playMusic("ShellMusic");
	r.run(200.0);
	REQUIRE(r.mgr->isMusicPlaying());

	// the loading screen's switch on the one manager (RW 0x81D242 / 0x81D278 / 0x81D283), spelled with the manager's own calls
	r.mgr->stopMusic(true);
	AudioEventRTS load("LoadTrack", VIEW_ANY);
	load.setShouldFade(false);
	r.mgr->addAudioEvent(load);
	r.mgr->update(r.mgr->nowMs() + AudioManager::tickIntervalMs());
	REQUIRE(r.mgr->isMusicPlaying());
	REQUIRE(r.mgr->getMusicTrackName() == "LoadTrack");
	CHECK(r.mgr->fadingEventNames().size() == 1); // the menu track is fading, as RW fades it

	// the synchronous load stalls the manager far beyond TimeToFadeAudio, then one update arrives (a load progress pump):
	// the fade is elapsed time (DONOR BFME2 processFade: m_at30 += elapsed), so the menu track is silent and gone
	r.stall(10000.0);
	CHECK(r.mgr->fadingEventNames().empty());
	CHECK(liveStreamVoices(r) == 1); // only the loading track's voice is alive
	CHECK(r.mgr->isMusicPlaying());
	CHECK(r.mgr->getMusicTrackName() == "LoadTrack");
	CHECK(r.mgr->playingCount(VoiceKind::Stream) == 1);
}
