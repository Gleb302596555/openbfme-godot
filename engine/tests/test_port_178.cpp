// OpenBFME. GPL-3.0.
//
// PORT-178 (fork issue 431, P-76: the tainted ground of Isengard is not heard). RotWK keeps a looping sound alive when it loses its channel:
// RW 0x45FF7F requeues the victim (RW 0x45A188). The port released it for good, and a building asks for its ambient once, so the ambient (the
// lowest priority, always the first victim) was silent for the rest of the game. Also here: RW's order of sounds (RW 0x450E81: priority, then
// volume), the inaudible cull without the sliders (RW 0x45BC4E), the "audible anyway" test (RW 0x452726) and the new audio log lines.
// The events below are the tests' own (no game or mod data).

#include "doctest.h"

#include "IniTestUtil.h"

#include "Common/Audio/AudioEventInfo.h"
#include "Common/Audio/AudioSettings.h"
#include "Common/Audio/GameAudio.h"
#include "Common/Audio/SimulatedAudioDevice.h"

#include <cmath>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace
{
std::vector<std::uint8_t> makeWav(double ms)
{
	const std::uint32_t rate = 22050;
	const std::uint32_t frames = (std::uint32_t)(ms * rate / 1000.0);
	std::vector<std::uint8_t> out;
	auto u32 = [&](std::uint32_t v) {
		for (int i = 0; i < 4; ++i)
		{
			out.push_back((std::uint8_t)(v >> (8 * i)));
		}
	};
	auto u16 = [&](std::uint16_t v) {
		out.push_back((std::uint8_t)v);
		out.push_back((std::uint8_t)(v >> 8));
	};
	auto tag = [&](const char *t) { out.insert(out.end(), t, t + 4); };
	tag("RIFF");
	u32(36 + frames * 2);
	tag("WAVE");
	tag("fmt ");
	u32(16);
	u16(1);
	u16(1);
	u32(rate);
	u32(rate * 2);
	u16(2);
	u16(16);
	tag("data");
	u32(frames * 2);
	out.insert(out.end(), frames * 2, 0);
	return out;
}

// two 3D samples (SampleCount3D = 2), MinSampleVolume 2%, every slider at 100%
const char kBase[] =
	"AudioSettings\n  AudioRoot = Data\\Audio\n  SoundsFolder = Sounds\n  MusicFolder = Tracks\n  StreamingFolder = Speech\n  AmbientStreamFolder = AmbientStreams\n  SoundsExtension = wav\n"
	"  SampleCount2D = 2\n  SampleCount3D = 2\n  StreamCount = 3\n  MinSampleVolume = 2%\n  GlobalMinRange = 5000\n  GlobalMaxRange = 5000000\n  TimeToFadeAudio = 300\n"
	"  AmbientStreamHysteresisVolume = 10\n  DefaultSoundVolume = 100%\n  DefaultVoiceVolume = 100%\n  DefaultMusicVolume = 100%\n  DefaultAmbientVolume = 100%\n  DefaultMovieVolume = 100%\nEnd\n"
	"AudioEvent DefaultSoundEffect\n  Volume = 100\n  Priority = high\n  Limit = 4\n  MinRange = 160\n  MaxRange = 640\n  PitchShift = 0 0\n  PlayPercent = 100\n  Type = ui everyone\n  Type = +DEFAULT\nEnd\n"
	"MusicTrack DefaultMusicTrack\n  Filename = NoFilename\n  Volume = 100\n  Type = DEFAULT\n  SubmixSlider = music\nEnd\n"
	"DialogEvent DefaultDialog\n  Volume = 100\n  Type = ui everyone\n  Type = +DEFAULT\nEnd\n"
	"AmbientStream DefaultAmbientStream\n  Filename = NoFilename\n  Volume = 100\n  Type = DEFAULT\n  MinRange = 100\n  MaxRange = 1000\n  SubmixSlider = ambient\nEnd\n";

// world sounds heard at full volume up to 100 and not at all from 500: the volume at distance d between them is (500 - d) / 400
#define PORT178_WORLD "  Limit = 0\n  MinRange = 100\n  MaxRange = 500\n  Type = world everyone\n  SubmixSlider = SoundFX\nEnd\n"
const char kEvents[] =
	"AudioEvent Hum\n  Sounds = hum\n  Control = loop\n  Priority = lowest\n" PORT178_WORLD    // a building's ambient: a loop of the lowest priority
	"AudioEvent Bang\n  Sounds = bang\n  Priority = high\n" PORT178_WORLD                     // a short one-shot of a higher priority
	"AudioEvent Tone\n  Sounds = tone\n  Priority = normal\n" PORT178_WORLD
	"AudioEvent Murmur\n  Sounds = tone\n  Priority = lowest\n" PORT178_WORLD
	"AudioEvent Floor\n  Sounds = tone\n  MinVolume = 5\n  ZoomedInOffscreenMinVolumePercent = 10\n  Priority = normal\n" PORT178_WORLD
	// the shape of the building ambient of fork issue 431 (a loud loop of the lowest priority with a Limit, shrouded), with the tests' own numbers
	"AudioEvent Ground\n  Sounds = hum\n  Volume = 120\n  Control = loop\n  Limit = 3\n  Priority = lowest\n  MinRange = 150\n  MaxRange = 600\n"
	"  Type = world shrouded everyone\n  SubmixSlider = SoundFX\nEnd\n";
#undef PORT178_WORLD

std::string soundFile(const char *name)
{
	return std::string("Data\\Audio\\Sounds\\") + name + ".wav";
}

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
		initest::FileList list;
		const std::pair<const char *, double> files[] = { { "hum", 3000.0 }, { "bang", 400.0 }, { "tone", 3000.0 } };
		for (const auto &f : files)
		{
			const std::vector<std::uint8_t> wav = makeWav(f.second);
			list.push_back({ std::string("data\\audio\\sounds\\") + f.first + ".wav", std::string(wav.begin(), wav.end()) });
		}
		fx.mount(list);
		ini.registerBlocks(fx.env.blocks);
		const std::string err = initest::loadError(fx.env, "audio.ini", std::string(kBase) + kEvents);
		REQUIRE_MESSAGE(err.empty(), err);
		// the tactical microphone row of the AUDIO-5 tests (test_audio5.cpp): used only where a test calls updateMicrophone
		MicrophoneSettings &m = ini.settings.microphone[0];
		m.preferredFractionCameraToGround = 0.86f;
		m.minDistanceToCamera = 100.0f;
		m.maxDistanceToCamera = 300.0f;
		m.pullTowardsTerrainLookAtPointPercent = 0.60f;
		m.preferredFractionCameraToGroundSq = m.preferredFractionCameraToGround * m.preferredFractionCameraToGround;
		m.minDistanceToCameraSq = m.minDistanceToCamera * m.minDistanceToCamera;
		m.maxDistanceToCameraSq = m.maxDistanceToCamera * m.maxDistanceToCamera;
		m.zoomMinDistance = 130.0f;
		m.zoomMaxDistance = 425.0f;
		m.zoomSoundVolumePercentageAmount = 0.20f;
		m.zoomMinDistanceSq = m.zoomMinDistance * m.zoomMinDistance;
		m.zoomMaxDistanceSq = m.zoomMaxDistance * m.zoomMaxDistance;
		cache = std::make_unique<AudioAssetCache>(&fx.fsys, 64u * 1024u * 1024u);
		device = std::make_unique<SimulatedAudioDevice>(cache.get());
		mgr = std::make_unique<AudioManager>(ini, *cache, *device, RandomAlgorithm::ZH_CarryChain, 1234);
		mgr->setListenerPosition(Coord3D{ 0.0f, 0.0f, 0.0f }, Coord3D{ 0.0f, 1.0f, 0.0f });
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

	// a sound `distance` in front of the listener
	AudioHandle play(const char *event, float distance) { return mgr->playSoundAt(event, Coord3D{ 0.0f, distance, 0.0f }); }

	// the voices of `name`.wav the device is playing now
	int sounding(const char *name)
	{
		const std::string file = soundFile(name);
		int n = 0;
		for (const auto &kv : device->voices())
		{
			n += (kv.second.start.file == file && device->isVoicePlaying(kv.first)) ? 1 : 0;
		}
		return n;
	}

	// what the device has for the only sounding voice of `name`.wav (null: none or several)
	const VoiceParams *paramsOf(const char *name)
	{
		const std::string file = soundFile(name);
		const VoiceParams *params = nullptr;
		int n = 0;
		for (const auto &kv : device->voices())
		{
			if (kv.second.start.file == file && device->isVoicePlaying(kv.first))
			{
				params = &kv.second.params;
				++n;
			}
		}
		return n == 1 ? params : nullptr;
	}

	// the volume the device has for the only sounding voice of `name`.wav (-1: none or several)
	float volumeOf(const char *name)
	{
		const std::string file = soundFile(name);
		float volume = -1.0f;
		int n = 0;
		for (const auto &kv : device->voices())
		{
			if (kv.second.start.file == file && device->isVoicePlaying(kv.first))
			{
				volume = kv.second.params.volume;
				++n;
			}
		}
		return n == 1 ? volume : -1.0f;
	}
};

int linesOf(const std::vector<AudioLogEntry> &log, const char *event, const std::string &outcomeStart)
{
	int n = 0;
	for (const AudioLogEntry &e : log)
	{
		n += (e.event == event && e.outcome.rfind(outcomeStart, 0) == 0) ? 1 : 0;
	}
	return n;
}
} // namespace

TEST_CASE("PORT-178: a looping sound that loses its channel to a higher priority one waits in the requests and returns when a channel is free (RW 0x45FF7F)")
{
	Rig r;
	const AudioHandle hum = r.play("Hum", 50.0f);
	r.run(70.0);
	REQUIRE(r.sounding("hum") == 1);
	r.play("Bang", 60.0f);
	r.run(40.0);
	REQUIRE(r.mgr->playingCount(VoiceKind::Sample3D) == 2); // SampleCount3D = 2: every 3D sample is taken
	r.play("Bang", 70.0f);
	r.run(40.0);
	// the second Bang took Hum's channel (the lowest priority is the first victim); Hum loops, so it went back to the requests
	CHECK(r.mgr->report().stolenChannels == 1);
	CHECK(r.mgr->report().requeuedLoops == 1);
	CHECK(r.sounding("hum") == 0);
	CHECK(r.sounding("bang") == 2);
	CHECK(r.mgr->pendingRequests() == 1);
	r.run(200.0);
	CHECK(r.sounding("hum") == 0); // both channels are still taken by higher sounds: it keeps waiting
	CHECK(r.mgr->pendingRequests() == 1);
	r.run(400.0); // the Bangs (400 ms) have ended
	CHECK(r.sounding("hum") == 1);
	CHECK(r.device->startsOf(soundFile("hum")) == 2);
	CHECK(r.mgr->isCurrentlyPlaying(hum)); // under its own handle: the owner's stop still reaches it
	CHECK(r.mgr->pendingRequests() == 0);
	r.run(4000.0);
	CHECK(r.sounding("hum") == 1); // and it loops on (the 3000 ms file started again)
	CHECK(r.device->startsOf(soundFile("hum")) == 3);
}

TEST_CASE("PORT-178: a one-shot that loses its channel is gone; a stop reaches a looping sound that waits for a channel")
{
	Rig r;
	const AudioHandle tone = r.play("Tone", 50.0f);
	const AudioHandle hum = r.play("Hum", 50.0f);
	r.run(70.0);
	REQUIRE(r.mgr->playingCount(VoiceKind::Sample3D) == 2);
	r.play("Bang", 60.0f); // takes Hum's channel (lowest), then the next one Tone's (normal)
	r.play("Bang", 70.0f);
	r.run(70.0);
	CHECK(r.mgr->report().stolenChannels == 2);
	CHECK(r.mgr->report().requeuedLoops == 1); // only the loop went back
	CHECK_FALSE(r.mgr->isCurrentlyPlaying(tone));
	CHECK(r.mgr->pendingRequests() == 1);
	r.mgr->removeAudioEvent(hum); // the building is gone while its ambient waits
	r.run(700.0);
	CHECK(r.mgr->pendingRequests() == 0);
	CHECK(r.sounding("hum") == 0);
	CHECK(r.sounding("tone") == 0);
	CHECK(r.device->startsOf(soundFile("hum")) == 1);
}

TEST_CASE("PORT-178: with every channel taken a louder sound takes the channel of the quietest one of its own priority, a quieter one finds none (RW 0x450E81)")
{
	Rig r;
	const AudioHandle distant = r.play("Tone", 400.0f); // volume 0.25
	const AudioHandle middle = r.play("Tone", 300.0f);  // volume 0.5
	r.run(70.0);
	REQUIRE(r.mgr->playingCount(VoiceKind::Sample3D) == 2);
	CHECK(r.play("Tone", 450.0f) == AHSV_NoSound); // volume 0.125: nothing lower is playing
	CHECK(r.play("Tone", 400.0f) == AHSV_NoSound); // volume 0.25: as loud as the quietest is not louder
	CHECK(r.mgr->report().stolenChannels == 0);
	const AudioHandle close = r.play("Tone", 150.0f); // volume 0.875
	CHECK(close >= AHSV_FirstHandle); // tolerance: any real handle; a refused request answers a special value below AHSV_FirstHandle
	r.run(70.0);
	CHECK(r.mgr->report().stolenChannels == 1);
	CHECK_FALSE(r.mgr->isCurrentlyPlaying(distant)); // the quietest gave its channel
	CHECK(r.mgr->isCurrentlyPlaying(middle));
	CHECK(r.mgr->isCurrentlyPlaying(close));
}

TEST_CASE("PORT-178: a request of the lowest priority takes the channel of a quieter sound of the lowest priority (RW 0x45B4E8 has no exception for it)")
{
	Rig r;
	const AudioHandle distant = r.play("Murmur", 400.0f); // volume 0.25
	const AudioHandle middle = r.play("Murmur", 300.0f);  // volume 0.5
	r.run(70.0);
	REQUIRE(r.mgr->playingCount(VoiceKind::Sample3D) == 2);
	const AudioHandle close = r.play("Murmur", 150.0f); // volume 0.875
	CHECK(close >= AHSV_FirstHandle); // tolerance: any real handle; a refused request answers a special value below AHSV_FirstHandle
	r.run(70.0);
	CHECK(r.mgr->report().stolenChannels == 1);
	CHECK_FALSE(r.mgr->isCurrentlyPlaying(distant));
	CHECK(r.mgr->isCurrentlyPlaying(middle));
	CHECK(r.mgr->isCurrentlyPlaying(close));
	// the priority decides first: a quiet sound of a higher priority takes the quietest Murmur, and no Murmur takes that channel back
	const AudioHandle tone = r.play("Tone", 450.0f); // normal, volume 0.125
	r.run(70.0);
	CHECK(r.mgr->report().stolenChannels == 2);
	CHECK(r.mgr->isCurrentlyPlaying(tone));
	CHECK_FALSE(r.mgr->isCurrentlyPlaying(middle));
	CHECK(r.mgr->isCurrentlyPlaying(close));
	CHECK(r.play("Murmur", 200.0f) == AHSV_NoSound); // lowest, volume 0.75: Tone is above it, the playing Murmur (0.875) is louder
	CHECK(r.mgr->report().stolenChannels == 2);
}

TEST_CASE("PORT-178: the inaudible cull does not see the volume sliders (RW 0x45BC4E asks RW 0x45A10E without them)")
{
	Rig r;
	const AudioHandle hum = r.play("Hum", 460.0f); // volume (500 - 460) / 400 = 0.1
	r.run(70.0);
	REQUIRE(r.mgr->isCurrentlyPlaying(hum));
	CHECK(r.volumeOf("hum") == doctest::Approx(0.1f)); // tolerance: float arithmetic of the falloff
	r.mgr->setSliderVolume(SLIDER_SOUNDFX, 0.1f); // heard at 0.1 x 0.1 = 0.01, below MinSampleVolume (2%)
	r.run(200.0);
	CHECK(r.mgr->isCurrentlyPlaying(hum));
	CHECK(r.volumeOf("hum") == doctest::Approx(0.01f)); // tolerance: float arithmetic of the falloff times the slider
	CHECK(r.mgr->report().requeuedLoops == 0);
	// beyond its range the volume without the sliders is 0: culled, and as a loop it waits for the listener to come back
	r.mgr->setSliderVolume(SLIDER_SOUNDFX, 1.0f);
	r.mgr->setListenerPosition(Coord3D{ 0.0f, -60.0f, 0.0f }, Coord3D{ 0.0f, 1.0f, 0.0f }); // distance 520
	r.run(200.0);
	CHECK(r.sounding("hum") == 0);
	CHECK(r.mgr->report().requeuedLoops == 1);
	CHECK(r.mgr->pendingRequests() == 1);
	r.mgr->setListenerPosition(Coord3D{ 0.0f, 0.0f, 0.0f }, Coord3D{ 0.0f, 1.0f, 0.0f });
	r.run(200.0);
	CHECK(r.sounding("hum") == 1);
	CHECK(r.mgr->isCurrentlyPlaying(hum));
}

TEST_CASE("PORT-178: a sound whose MinVolume is at or above MinSampleVolume is never culled as inaudible (RW 0x452726)")
{
	Rig r;
	// off screen at full zoom, the event's MinVolume (5%) is scaled by ZoomedInOffscreenMinVolumePercent (10%): 0.5%, below MinSampleVolume (2%)
	AudioWorldQueries q;
	q.zoomFadeFactor = []() { return 1.0f; };
	q.isOnScreen = [](const Coord3D &) { return false; };
	q.offscreenDistanceSq = [](const Coord3D &) { return 1.0e9f; };
	r.mgr->bindWorldQueries(q, &r);
	const AudioHandle floor = r.play("Floor", 600.0f); // beyond MaxRange: only its MinVolume is left
	REQUIRE(floor >= AHSV_FirstHandle); // tolerance: any real handle; a refused request answers a special value below AHSV_FirstHandle
	r.run(200.0);
	CHECK(r.mgr->isCurrentlyPlaying(floor));
	CHECK(r.volumeOf("tone") == doctest::Approx(0.005f)); // tolerance: float arithmetic of MinVolume times the zoom fade
	r.mgr->clearWorldQueries(&r);
}

TEST_CASE("PORT-178: after a start burst took its channel, a building's looping ambient plays again at RW's gain for the start camera (RW 0x45FF7F, 0x4591D4, 0x45235B)")
{
	Rig r;
	// the start camera looks at the building from 600 above and 450 behind it
	const Coord3D building{ 1000.0f, 1000.0f, 0.0f };
	const Coord3D camera{ 1000.0f, 550.0f, 600.0f };
	r.mgr->updateMicrophone(camera, building);
	const AudioHandle ambient = r.mgr->playSoundAt("Ground", building);
	r.run(70.0);
	REQUIRE(r.sounding("hum") == 1);
	// the start burst: two sounds of a higher priority beside the building take both 3D samples
	r.mgr->playSoundAt("Bang", Coord3D{ 1010.0f, 1000.0f, 0.0f });
	r.mgr->playSoundAt("Bang", Coord3D{ 990.0f, 1000.0f, 0.0f });
	r.run(100.0);
	CHECK(r.sounding("hum") == 0);
	CHECK(r.sounding("bang") == 2);
	CHECK(r.mgr->report().stolenChannels == 1);
	r.run(600.0); // the burst (400 ms) is over
	REQUIRE(r.sounding("hum") == 1);
	CHECK(r.mgr->isCurrentlyPlaying(ambient));
	// RW's chain for this voice, by hand
	const double length = std::sqrt(450.0 * 450.0 + 600.0 * 600.0); // camera to look-at: 750
	const double s = 300.0 / length;                               // RW 0x452451: 0.86^2 x 750^2 >= 300^2, the microphone is 300 from the camera
	const double micY = 550.0 + 450.0 * s;                         // 730
	const double micZ = 600.0 - 600.0 * s;                         // 360
	const double pulledY = (1000.0 - micY) * 0.60 + micY;          // RW 0x4524D8: x and y go 60 in 100 of the way to the look-at: 892
	const double distance = std::sqrt((1000.0 - pulledY) * (1000.0 - pulledY) + micZ * micZ);              // microphone to building
	const double falloff = (600.0 - distance) / (600.0 - 150.0);                                           // RW 0x4532DE: MinRange 150, MaxRange 600
	const double eye = std::sqrt((pulledY - 550.0) * (pulledY - 550.0) + (600.0 - micZ) * (600.0 - micZ)); // camera to microphone
	const double zoom = 1.0 - (eye - 130.0) / (425.0 - 130.0) * 0.20;                                      // RW 0x451946: the zoom volume
	const double volume = 1.20 * falloff * zoom;                                                           // Volume 120, the sound slider at 100
	const double level = 0.5 * std::pow(volume, 1.6666666269302368); // Miles Fast 2D, straight ahead: each channel gets half of volume ^ (5/3)
	CHECK(distance == doctest::Approx(375.8510).epsilon(1e-6)); // tolerance: the literal is the hand result to seven digits
	CHECK(volume == doctest::Approx(0.48110).epsilon(1e-4));     // tolerance: the literal is the hand result to five digits
	CHECK(level == doctest::Approx(0.14769).epsilon(1e-4));      // tolerance: the literal is the hand result to five digits
	const VoiceParams *params = r.paramsOf("hum");
	REQUIRE(params != nullptr);
	CHECK(params->volume == doctest::Approx(volume).epsilon(1e-4));   // tolerance: the engine computes in float, the hand chain in double
	CHECK(params->gainLeft == doctest::Approx(level).epsilon(1e-4));  // tolerance: as above, and the pan is acos(0) / pi in float
	CHECK(params->gainRight == doctest::Approx(level).epsilon(1e-4)); // tolerance: as above
	// the Godot device plays a voice at the bus volume (left + right) / 2 (GodotAudioDevice::applyParams): the same number here
	CHECK(0.5 * (params->gainLeft + params->gainRight) == doctest::Approx(level).epsilon(1e-4)); // tolerance: as above
}

TEST_CASE("PORT-178: the audio log has the channel steal, the stop of a looping voice and the level of a looping 3D voice")
{
	Rig r;
	r.mgr->enableEventLog(true);
	const AudioHandle hum = r.play("Hum", 50.0f);
	r.run(70.0);
	r.play("Bang", 60.0f);
	r.run(40.0);
	r.play("Bang", 70.0f);
	r.run(40.0);
	std::vector<AudioLogEntry> log = r.mgr->takeEventLog();
	CHECK(linesOf(log, "Hum", "started ") == 1);
	CHECK(linesOf(log, "Hum", "channel taken by Bang (loop requeued)") == 1);
	r.run(600.0);
	log = r.mgr->takeEventLog();
	CHECK(linesOf(log, "Hum", "started ") == 1); // back on a free channel
	// one level line per 150 audio steps (5 s) while it loops: the volume, both channel gains and the distance the device was given
	r.run(5200.0);
	log = r.mgr->takeEventLog();
	REQUIRE(linesOf(log, "Hum", "level: ") == 1);
	for (const AudioLogEntry &e : log)
	{
		if (e.event == "Hum" && e.outcome.rfind("level: ", 0) == 0)
		{
			CHECK(e.outcome == "level: volume 1.000, left 0.5000, right 0.5000, distance 50");
		}
	}
	CHECK(linesOf(log, "Bang", "level: ") == 0);
	r.mgr->removeAudioEvent(hum);
	r.run(70.0);
	log = r.mgr->takeEventLog();
	CHECK(linesOf(log, "Hum", "stopped") == 1);
	r.mgr->enableEventLog(false);
}

TEST_CASE("PORT-178: what stays unported of RW's playing list and channel steal is reported (S-7326) and registered in docs/STOPS.md")
{
	int lines = 0;
	for (const std::string &line : AudioManager::unverified())
	{
		if (line.rfind("S-7326:", 0) == 0)
		{
			++lines;
			CHECK(line.find("0x45BC24") != std::string::npos); // the shroud test of a playing sound
			CHECK(line.find("0x45E98F") != std::string::npos); // the first step of the channel steal
			CHECK(line.find("0x45BD57") != std::string::npos); // the requeue of a loop without a position
		}
	}
	CHECK(lines == 1);
	std::ifstream in(std::string(OPENBFME_DOCS_DIR) + "/STOPS.md");
	REQUIRE_MESSAGE(static_cast<bool>(in), "cannot read docs/STOPS.md");
	std::stringstream ss;
	ss << in.rdbuf();
	CHECK(ss.str().find("| S-7326 | code |") != std::string::npos);
}
