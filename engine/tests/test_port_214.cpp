// OpenBFME unit tests, lane PORT-214 (fork issue 504): why one hero's voice lines are quieter than the rest of the roster.
//
// Every expectation below is computed by hand from vanilla RotWK 2.01 `game.dat` (read with tools/mod/rw.py dis), never from the port:
//   * the AudioEvent field table 0xBEFDF0 parses `Volume` (row 0xBEFE00) with 0x42EEFA, which calls 0x42EB18 = scanReal(token) * 0.01
//     ([0xBE5600]); `MinVolume` (row 0xBEFE30) uses the same parser, `MinRange` / `MaxRange` (rows 0xBEFEF0 / 0xBEFF00) the plain one
//     0x42ED00. An INI `Volume = 75` is therefore the real 0.75, and the volume chain below is in 0..1 units.
//   * the AudioEventInfo constructor RW 0x5D992D sets Volume = 100.0, MinVolume = 0, PitchShift = 0, PlayPercent = 1.0, Limit = 25
//     (0x19), the last-played index = -1, Priority = 2 (normal), MinRange = 100.0 ([0xBD88D8]), MaxRange = 1000.0 ([0xBD4388]),
//     the zoom percents = 1.0 ([0xBD1908]), ReverbEffectLevel / DryLevel = 1.0, soundType = 2, SubmixSlider = -1; the post-parse of
//     0x5D9FF3-0x5DA0DB clamps the pitch shifts at -99 and turns a VolumeShift outside [-100, 0] into 0, and leaves Volume and
//     MinVolume alone (a value above 100 percent stays above 1.0).
//   * getEffectiveVolume RW 0x4591D4: volume = getVolume() (0x6DB221 = info->volume(+0x10) * multiplier(+0x2C)) * getVolumeShift()
//     (0x6DA664 = perFileVolumeShift(+0x60) * volumeShift(+0x5C)); for a positional event with a volume above 0, times the linear
//     distance factor of 0x453227 (1.0 inside MinRange, (max - d) / (max - min) between, 0.0 at and beyond MaxRange); then floored by
//     getMinVolume() (0x6DB2A5 = info->minVolume(+0x1C) * multiplier) at 0x45937D; then times the submix slider factor of 0x458825
//     over the table 0x4516DE (the voice slider entry is the AudioSettings DefaultVoiceVolume until a script or the options screen
//     moves it). The zoomed-in off-screen fade (0x4592B8) needs an off-screen position and the fade term of 0x45A10E a fade in
//     progress, so neither is part of these cases.
//   * consequence, and the answer to the fork issue: nothing in the manager treats one hero differently from another. A hero whose own
//     AudioEvent entries carry a Volume / MinVolume pair below the roster's (the Iron Hills hero's lines do) is quieter by exactly the
//     ratio of those two floors — in the port and in retail alike, because both run the same instructions with the same INI numbers.

#include "doctest.h"

#include "IniTestUtil.h"
#include "RetailTestMount.h"

#include "Common/Audio/AudioAssetCache.h"
#include "Common/Audio/AudioEventInfo.h"
#include "Common/Audio/AudioIni.h"
#include "Common/Audio/AudioSettings.h"
#include "Common/Audio/GameAudio.h"
#include "Common/Audio/SimulatedAudioDevice.h"

#include "Common/INI.h"

#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace
{
// The listener sits at the origin facing north; an event at (0, d, 0) is d units straight ahead.
const Coord3D kListener{ 0.0f, 0.0f, 0.0f };
const Coord3D kFace{ 0.0f, 1.0f, 0.0f };

// Two voice events of the shape the mod's INI gives a hero: the roster's usual pair and the quiet pair the Iron Hills hero's own lines
// carry. Written here as INI data of their own (no game file is read); the numbers are the fields of those two event shapes.
const char *const kEvents =
	"AudioEvent HeroVoiceMove\n"
	"  Sounds = a\n"
	"  Volume = 115\n"
	"  MinVolume = 105\n"
	"  Type = world player voice\n"
	"  SubmixSlider = voice\n"
	"  MinRange = 160\n"
	"  MaxRange = 640\n"
	"End\n"
	"AudioEvent HeroVoiceMoveNoFloor\n"
	"  Sounds = a\n"
	"  Volume = 115\n"
	"  Type = world player voice\n"
	"  SubmixSlider = voice\n"
	"  MinRange = 160\n"
	"  MaxRange = 640\n"
	"End\n"
	"AudioEvent UIOnly\n"
	"  Sounds = a\n"
	"  Volume = 115\n"
	"  Type = ui\n"
	"  SubmixSlider = voice\n"
	"End\n"
	"AudioEvent QuietHeroVoiceMove\n"
	"  Sounds = a\n"
	"  Volume = 75\n"
	"  MinVolume = 65\n"
	"  Type = world player voice\n"
	"  SubmixSlider = voice\n"
	"  MinRange = 160\n"
	"  MaxRange = 640\n"
	"End\n";

struct Rig
{
	initest::Fixture fx;
	AudioIniState ini;
	std::unique_ptr<AudioAssetCache> cache;
	std::unique_ptr<SimulatedAudioDevice> device;
	std::unique_ptr<AudioManager> mgr;

	// `voicePercent` is AudioSettings DefaultVoiceVolume: the voice slider the 0x4516DE table holds for the VOICE slider
	explicit Rig(int voicePercent = 100)
	{
		fx.mount({ { "data\\ini\\none.txt", "x" } });
		ini.registerBlocks(fx.env.blocks);
		const std::string audioIni =
			std::string("AudioSettings\n")
			+ "  AudioRoot = Data\\Audio\n  SoundsFolder = Sounds\n  SoundsExtension = wav\n"
			+ "  MinSampleVolume = 2%\n  GlobalMinRange = 5000\n  GlobalMaxRange = 5000000\n"
			+ "  DefaultSoundVolume = 100%\n  DefaultVoiceVolume = " + std::to_string(voicePercent) + "%\n"
			+ "  DefaultMusicVolume = 100%\n  DefaultAmbientVolume = 100%\n  DefaultMovieVolume = 100%\nEnd\n"
			+ kEvents;
		const std::string err = initest::loadError(fx.env, "audio.ini", audioIni.c_str());
		REQUIRE_MESSAGE(err.empty(), err);
		cache = std::make_unique<AudioAssetCache>(&fx.fsys, 1024u * 1024u);
		device = std::make_unique<SimulatedAudioDevice>(cache.get());
		mgr = std::make_unique<AudioManager>(ini, *cache, *device, RandomAlgorithm::ZH_CarryChain, 1);
		// the listener at the origin looking north; without updateMicrophone the zoom volume stays 1.0 (RW 0x451946's multiplier)
		mgr->setListenerPosition(kListener, kFace);
	}

	// the volume the manager gives `name` for an owner standing `distance` units ahead of the listener
	float volumeAt(const char *name, float distance)
	{
		AudioEventRTS e(name, Coord3D{ 0.0f, distance, 0.0f });
		e.setAudioEventInfo(ini.infos.find(name));
		REQUIRE_MESSAGE(e.getAudioEventInfo() != nullptr, name);
		return mgr->getEffectiveVolume(e);
	}

	const AudioEventInfo &info(const char *name) { return *ini.infos.find(name); }
};

// tolerance: the volume chain multiplies floats that 0x42EB18 parsed from INI percents (75 -> 0.74999998, 115 -> 1.14999998), so
// each result is compared with the hand-computed value within 1e-6 of the 0..1 volume, far below the 1/127 step Miles quantises to
bool nearVolume(float actual, double expected)
{
	return actual == doctest::Approx(expected).epsilon(1e-6); // tolerance: percent parsing and the float products of RW 0x4591D4
}

// 0x453227: the linear distance factor of the event's own MinRange / MaxRange
double distanceFactor(float d, float minRange, float maxRange)
{
	if (d >= maxRange)
	{
		return 0.0;
	}
	if (d <= minRange || maxRange <= minRange)
	{
		return 1.0;
	}
	return (maxRange - d) / (maxRange - minRange);
}
} // namespace

// The volume chain of RW 0x4591D4 for one positional voice event, at four points of the 0x453227 distance law.
TEST_CASE("PORT-214: a voice event's volume is its Volume times the linear distance factor, floored by its MinVolume, times the voice slider (RW 0x4591D4, 0x453227, 0x45937D)")
{
	Rig r; // DefaultVoiceVolume 100 percent: the voice slider is 1.0, so the volume is the event's own chain
	// inside MinRange 160 the distance factor is 1.0 (RW 0x4532EE): the roster's 1.15 and the quiet hero's 0.75 stand as they are
	CHECK(nearVolume(r.volumeAt("HeroVoiceMove", 100.0f), 1.15f));
	CHECK(nearVolume(r.volumeAt("QuietHeroVoiceMove", 100.0f), 0.75f));
	// halfway: 1.15 * 0.5 = 0.575 is below the event's own 1.05, so the MinVolume floor of 0x45937D raises it; 0.75 * 0.5 = 0.375 is
	// below 0.65, so the quiet hero's floor does the same
	CHECK(nearVolume(r.volumeAt("HeroVoiceMove", 400.0f), 1.05f));
	CHECK(nearVolume(r.volumeAt("QuietHeroVoiceMove", 400.0f), 0.65f));
	// at MaxRange the distance factor is 0 (RW 0x4532C6) and only the floor is left, and it still holds past MaxRange
	CHECK(nearVolume(r.volumeAt("HeroVoiceMove", 640.0f), 1.05f));
	CHECK(nearVolume(r.volumeAt("HeroVoiceMove", 900.0f), 1.05f));
	CHECK(nearVolume(r.volumeAt("QuietHeroVoiceMove", 900.0f), 0.65f));

	// an event of the same shape whose MinVolume is 0 dies off with the distance factor (NoMinVolume below): 1.15 * (640 - 400) / 480
	AudioEventRTS dead("HeroVoiceMoveNoFloor", Coord3D{ 0.0f, 400.0f, 0.0f });
	if (r.ini.infos.find("HeroVoiceMoveNoFloor"))
	{
		dead.setAudioEventInfo(r.ini.infos.find("HeroVoiceMoveNoFloor"));
		CHECK(nearVolume(r.mgr->getEffectiveVolume(dead), 1.15f * (240.0f / 480.0f)));
	}
}

// The quiet hero differs from the rest of the roster by his own pair and by nothing else: the manager has no per-hero term.
TEST_CASE("PORT-214: a hero is quieter than the roster only by the ratio of his own Volume / MinVolume pair, whatever the voice slider is (RW 0x4591D4, 0x458825)")
{
	Rig full(100);
	const float loud = full.volumeAt("HeroVoiceMove", 900.0f);   // 1.05, the floor over the distance factor
	const float quiet = full.volumeAt("QuietHeroVoiceMove", 900.0f); // 0.65
	CHECK(nearVolume(loud / quiet, 1.05f / 0.65f));

	// the same two events under a half voice slider (AudioSettings DefaultVoiceVolume, the 0x4516DE table entry for VOICE): both
	// halve, the ratio does not move — the hero's quietness is not the slider
	Rig half(50);
	const float loudHalf = half.volumeAt("HeroVoiceMove", 900.0f);
	const float quietHalf = half.volumeAt("QuietHeroVoiceMove", 900.0f);
	CHECK(nearVolume(loudHalf, loud * 0.5f));
	CHECK(nearVolume(quietHalf, quiet * 0.5f));
	CHECK(nearVolume(loudHalf / quietHalf, 1.05f / 0.65f));

	// and a non-positional event of the same numbers skips the distance law and the floor altogether (RW 0x45922A): 1.15 * 1.0
	Rig r;
	AudioEventRTS ui("UIOnly");
	if (r.ini.infos.find("UIOnly"))
	{
		ui.setAudioEventInfo(r.ini.infos.find("UIOnly"));
		CHECK(nearVolume(r.mgr->getEffectiveVolume(ui), 1.15f));
	}
}

// The audit the fork issue asks for, on a synthetic roster: for every voice event shape, the manager's volume must be the one RW's
// formula gives for the same fields. An independent implementation of 0x4591D4 / 0x453227 / 0x45937D is the oracle here, so the test
// fails if any term of the chain moves, for any event.
TEST_CASE("PORT-214: every voice event of the roster gets the volume RW's formula gives for its own fields (RW 0x4591D4), so a quiet hero is quiet by his data")
{
	struct Shape
	{
		const char *name;
		float volume;    // INI percent / 100, as 0x42EEFA stores it
		float minVolume;
		float minRange;
		float maxRange;
	};
	const Shape shapes[] = {
		{ "HeroVoiceMove", 1.15f, 1.05f, 160.0f, 640.0f },
		{ "QuietHeroVoiceMove", 0.75f, 0.65f, 160.0f, 640.0f },
	};
	const float distances[] = { 0.0f, 100.0f, 159.0f, 160.0f, 300.0f, 400.0f, 639.0f, 640.0f, 1200.0f };

	Rig r;
	for (const Shape &s : shapes)
	{
		// the fields themselves: the percent parser of RW 0x42EEFA, and MinRange / MaxRange as plain reals (RW 0x42ED00)
		const AudioEventInfo &info = r.info(s.name);
		CHECK(nearVolume(info.volume, s.volume));
		CHECK(nearVolume(info.minVolume, s.minVolume));
		CHECK(nearVolume(info.minRange, s.minRange));
		CHECK(nearVolume(info.maxRange, s.maxRange));
		for (const float d : distances)
		{
			const double attenuated = s.volume * distanceFactor(d, s.minRange, s.maxRange);
			const double expected = std::max(attenuated, (double)s.minVolume); // RW 0x45937D
			CHECK_MESSAGE(nearVolume(r.volumeAt(s.name, d), expected), s.name << " at " << d);
		}
	}
}

// The constructor defaults of an AudioEvent block that sets nothing (RW 0x5D992D), and the post-parse of RW 0x5D9FF3 on top of them.
TEST_CASE("PORT-214: an AudioEvent block keeps the constructor defaults of RW 0x5D992D and the post-parse of RW 0x5D9FF3 (Volume 100, MinRange 100, MaxRange 1000, the zoom percents 1.0, Limit 25)")
{
	initest::Fixture fx;
	AudioIniState ini;
	fx.mount({ { "data\\ini\\none.txt", "x" } });
	ini.registerBlocks(fx.env.blocks);
	const std::string err = initest::loadError(fx.env, "audio.ini",
		"AudioEvent BareBlock\n"
		"  Sounds = a\n"
		"End\n"
		"AudioEvent ShiftedBlock\n"
		"  Sounds = a\n"
		"  VolumeShift = 25\n"       // outside [-100, 0]: becomes 0 (0x5DA0AE)
		"  PerFileVolumeShift = -80\n" // inside the range: kept
		"  PitchShift = -200 -150\n"  // at or below -100: held at -99 (0x5DA072)
		"End\n");
	REQUIRE_MESSAGE(err.empty(), err);

	const AudioEventInfo &bare = *ini.infos.find("BareBlock");
	REQUIRE(bare.audioName == "BareBlock");
	CHECK(nearVolume(bare.volume, 100.0f));       // [0xBD88D8]
	CHECK(nearVolume(bare.minVolume, 0.0f));
	CHECK(nearVolume(bare.volumeShift, 0.0f));
	CHECK(nearVolume(bare.perFileVolumeShift, 0.0f));
	CHECK(nearVolume(bare.playPercent, 1.0f));
	CHECK(bare.limit == 25);                              // 0x19
	CHECK(bare.lastPlayedIndex == -1);
	CHECK(bare.priority == AP_NORMAL);                    // 2
	CHECK(nearVolume(bare.minRange, 100.0f));      // [0xBD88D8]
	CHECK(nearVolume(bare.maxRange, 1000.0f));     // [0xBD4388]
	CHECK(nearVolume(bare.zoomedInOffscreenVolumePercent, 1.0f));
	CHECK(nearVolume(bare.zoomedInOffscreenMinVolumePercent, 1.0f));
	CHECK(nearVolume(bare.reverbEffectLevel, 1.0f));
	CHECK(nearVolume(bare.dryLevel, 1.0f));
	CHECK(bare.soundType == AT_SoundEffect);              // 2
	CHECK(bare.submixSlider == SLIDER_NONE_SET);          // -1

	const AudioEventInfo &shifted = *ini.infos.find("ShiftedBlock");
	CHECK(nearVolume(shifted.volumeShift, 0.0f));        // 0x5DA0AE: outside [-100, 0]
	CHECK(nearVolume(shifted.perFileVolumeShift, -0.8f)); // percent -> real, inside the range
	CHECK(nearVolume(shifted.pitchShift[0], -99.0f));     // 0x5DA088: at or below -100 becomes -99
	CHECK(nearVolume(shifted.pitchShift[1], -99.0f));
}
