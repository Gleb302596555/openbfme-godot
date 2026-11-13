// OpenBFME. GPL-3.0.
//
// P-94 (docs/ennorath/PROBLEMS.md): Space (CommandMap VIEW_LAST_RADAR_EVENT, the language archive's CommandMap.ini) did nothing in a skirmish. The jump
// (RW 0x5DCB28) walks TheEva's list (RW 0xDE3670 + 0x68); RotWK fills it from two places through RW 0x5DD1F3: the scripts' radar event actions and, in a
// skirmish the only source, TheEva itself when an event's sound starts (RW 0x5DD7FF .. 0x5DD826: handle >= 5, CountAsJumpToLocation + 0x2D, the
// position RW 0x5DC455). The port had the script path only, and the list's three MiscEvaData fields (parsed into TheEva + 0x80: field tables
// RW 0xBF2610 / 0xBF2620 / 0xBF2630) at 0.
//   * + 0x90 MaxMillisecondsBeforeResettingLastJumpTo: the jump's window, ftol(window * 0.03f) client frames (RW 0x5DCB59 .. 0x5DCB7F; 2000 -> 59).
//   * + 0x94 MinDistanceBetweenJumpToEvents: a new position within it of the list's first erases that first (RW 0x5DD216 .. 0x5DD270).
//   * + 0x8C MaxMillisecondsToKeepJumpToEvents (logic frames): TheEva's update erases from the oldest while noted + keep < now (RW 0x5DD8ED .. 0x5DD930).
// Synthetic data except the first case's map (the shared retail world; skips without ROTWK_INSTALL / BFME2_INSTALL).
#include "HudTestUtil.h"
#include "IniTestUtil.h"
#include "Common/Audio/AudioEntryPoints.h"
#include "Common/Audio/GameAudio.h"
#include "Common/Audio/SimulatedAudioDevice.h"
#include "GameClient/Eva.h"
#include "GameClient/GUI/ShellServices.h"
#include "GameClient/InGameHud.h"
#include "GameClient/MessageStream/MetaEvent.h"
#include "GameClient/Radar.h"

#include <cmath>
#include <memory>
#include <string>

using namespace hudtest;

namespace
{
std::string wav(double ms)
{
	const std::uint32_t rate = 22050, frames = (std::uint32_t)(ms * rate / 1000.0);
	std::string out;
	auto u32 = [&](std::uint32_t v) {
		for (int i = 0; i < 4; ++i)
		{
			out.push_back((char)(v >> (8 * i)));
		}
	};
	auto u16 = [&](std::uint16_t v) {
		out.push_back((char)v);
		out.push_back((char)(v >> 8));
	};
	out += "RIFF";
	u32(36 + frames * 2);
	out += "WAVEfmt ";
	u32(16);
	u16(1);
	u16(1);
	u32(rate);
	u32(rate * 2);
	u16(2);
	u16(16);
	out += "data";
	u32(frames * 2);
	out.append(frames * 2, '\0');
	return out;
}

// RotWK's and Ennorath's eva.ini MiscEvaData jump-to rows (60000 / 2000 / 400); one event that counts as a jump-to location (the default)
const char kAudioEva[] =
	"AudioSettings\n  AudioRoot = Data\\Audio\n  SoundsFolder = Sounds\n  MusicFolder = Tracks\n  StreamingFolder = Speech\n  AmbientStreamFolder = AmbientStreams\n  SoundsExtension = wav\n"
	"  SampleCount2D = 4\n  SampleCount3D = 4\n  StreamCount = 3\n  MinSampleVolume = 2%\n  DefaultSoundVolume = 100%\n  DefaultVoiceVolume = 100%\n  DefaultMusicVolume = 100%\n  DefaultAmbientVolume = 100%\n  DefaultMovieVolume = 100%\nEnd\n"
	"AudioEvent DefaultSoundEffect\n  Volume = 100\n  Priority = high\n  Limit = 0\n  MinRange = 160\n  MaxRange = 640\n  PlayPercent = 100\n  Type = ui everyone\n  Type = +DEFAULT\nEnd\n"
	"AudioEvent Help\n  Sounds = help\nEnd\n"
	"MiscEvaData\n  MaxMillisecondsToKeepJumpToEvents = 60000\n  MaxMillisecondsBeforeResettingLastJumpTo = 2000\n  MinDistanceBetweenJumpToEvents = 400\nEnd\n"
	"PredefinedEvaEvent DefaultEvaEvent\n  Priority = 5\n  TimeBetweenEventsMS = 20000\n  ExpirationTimeMS = 1500\nEnd\n"
	"NewEvaEvent UnitUnderAttack\n  Priority = 3\n  ExpirationTimeMS = 1000\n  TimeBetweenEventsMS = 0\n  SideSound\n    Side = Men\n    Sound = Help\n  End\nEnd\n";

struct EvaInstall
{
	initest::Fixture fx;
	AudioIniState ini;
	std::unique_ptr<AudioAssetCache> cache;
	std::unique_ptr<SimulatedAudioDevice> device;
	std::unique_ptr<AudioManager> audio;
	std::unique_ptr<Eva> eva;
	double t = 0.0;

	EvaInstall()
	{
		fx.mount({ { "data\\audio\\sounds\\help.wav", wav(300) } });
		ini.registerBlocks(fx.env.blocks);
		const std::string err = initest::loadError(fx.env, "audio.ini", kAudioEva);
		REQUIRE_MESSAGE(err.empty(), err);
		cache = std::make_unique<AudioAssetCache>(&fx.fsys, 32u << 20);
		device = std::make_unique<SimulatedAudioDevice>(cache.get());
		audio = std::make_unique<AudioManager>(ini, *cache, *device, RandomAlgorithm::ZH_CarryChain, 1);
		eva = std::make_unique<Eva>(ini, *audio);
		eva->setLocalSide("Men");
		audio->update(0.0);
		AudioApi::installEva(eva.get());
	}
	~EvaInstall() { AudioApi::installEva(nullptr); }
	void run(double ms)
	{
		const double end = t + ms;
		while (t < end)
		{
			t += 10.0;
			device->setTime(t);
			audio->update(t);
			eva->update(t);
		}
	}
};

struct SpaceRig
{
	Rig rig;
	RecordingShellServices services;
	std::unique_ptr<InGameHud> hud;
	explicit SpaceRig(SharedWorld &s) : rig(s)
	{
		InGameHud::Config cfg{ *rig.game, *s.world, *s.mount->fs, services, rig.view, s.mouse, s.meta, nullptr };
		hud = std::make_unique<InGameHud>(cfg);
		std::string error;
		REQUIRE_MESSAGE(hud->boot(&error), error);
		hud->setWindowSize(1024, 768);
		rig.view.setScreen(1024, 768);
	}
	~SpaceRig() { hud.reset(); }
	void frames(int n)
	{
		for (int i = 0; i < n; ++i)
		{
			hud->update(0.033);
			rig.game->advance(0.033);
		}
	}
};
} // namespace

TEST_CASE("P-94: an Eva event with a position that starts to play becomes Space's jump target in a skirmish (RW 0x5DD826 -> RW 0x5DD1F3, RW 0x5DCB28)")
{
	if (!haveWorld("P-94 space jump"))
	{
		return;
	}
	SharedWorld &s = shared();
	EvaInstall e;
	SpaceRig h(s);
	const MetaMapRec *rec = s.meta.find(CMSG_META_VIEW_LAST_RADAR_EVENT);
	REQUIRE(rec != nullptr);
	CHECK(rec->key == KEY_SPACE);
	h.rig.lookAt({ 2000, 2000, 0 });
	h.frames(4);
	REQUIRE(h.hud->radar().events().empty());
	const Coord3D attacked{ 1400.0f, 1600.0f, 0.0f };
	REQUIRE(e.eva->reportEventByName("UnitUnderAttack", &attacked, e.t));
	h.frames(1);
	CHECK(h.hud->radar().events().empty()); // reported only: RotWK adds the position when the sound starts
	e.run(30.0);
	h.frames(1);
	REQUIRE(h.hud->radar().events().size() == 1);
	CHECK(h.hud->radar().events()[0].position.x == 1400.0f);
	CHECK(h.hud->radar().events()[0].frame == h.rig.logic().getFrame());
	// Space's jump (the command translator's VIEW_LAST_RADAR_EVENT calls this, CommandXlat.cpp) looks at the event
	h.hud->radar().tryJumpToNextEvent(h.rig.view);
	// tolerance: the view's position is the float result of the camera's look-at transform; 1 unit of rounding
	CHECK(std::fabs(h.rig.view.position().x - 1400.0f) < 1.0f);
	CHECK(std::fabs(h.rig.view.position().y - 1600.0f) < 1.0f); // tolerance: as above
}

TEST_CASE("P-94 radar list: MinDistanceBetweenJumpToEvents merges with the newest, the window is ftol(2000 * 0.03f) = 59 client frames, and "
		  "MaxMillisecondsToKeepJumpToEvents drops the oldest (RW 0x5DD216, RW 0x5DCB59 .. 0x5DCB7F, RW 0x5DD8ED .. 0x5DD930)")
{
	if (!haveWorld("P-94 radar list"))
	{
		return;
	}
	SpaceRig h(shared());
	Radar &r = h.hud->radar();
	r.setEvaJumpData(2000u, 400.0f, 1800u); // 60000 ms = 1800 logic frames
	r.noteEvaPosition({ 100.0f, 100.0f, 0.0f }, 10);
	r.noteEvaPosition({ 1100.0f, 100.0f, 0.0f }, 20);
	REQUIRE(r.events().size() == 2);
	CHECK(r.events()[0].position.x == 1100.0f); // the newest first (RW 0x75991F)
	// within 400 of the list's first (1100, 100): that first is erased, the new one goes in front
	r.noteEvaPosition({ 1300.0f, 300.0f, 0.0f }, 30); // distance sqrt(80000) = 282.8
	REQUIRE(r.events().size() == 2);
	CHECK(r.events()[0].position.x == 1300.0f);
	CHECK(r.events()[1].position.x == 100.0f);
	// exactly 400 away does not merge (comiss, jbe skips: 400^2 > d^2 is false)
	r.noteEvaPosition({ 1300.0f, 700.0f, 0.0f }, 40);
	CHECK(r.events().size() == 3);

	// the window: a press at frame f + 59 still walks on, at f + 60 it starts from the newest
	r.setClientFrame(1000);
	r.tryJumpToNextEvent(h.rig.view);
	CHECK(r.eventCursor() == 0);
	r.setClientFrame(1059);
	r.tryJumpToNextEvent(h.rig.view);
	CHECK(r.eventCursor() == 1);
	r.setClientFrame(1119); // 1059 + 60: past the window
	r.tryJumpToNextEvent(h.rig.view);
	CHECK(r.eventCursor() == 0);

	// the keep time: noted frame + 1800 < now erases, from the oldest
	r.expireEvaPositions(1810); // 10 + 1800 = 1810: not below
	CHECK(r.events().size() == 3);
	r.expireEvaPositions(1811);
	REQUIRE(r.events().size() == 2);
	CHECK(r.events().back().position.y == 300.0f);
	r.expireEvaPositions(5000);
	CHECK(r.events().empty());
}
