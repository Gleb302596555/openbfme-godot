// OpenBFME. GPL-3.0.
//
// P-93 (docs/ennorath/PROBLEMS.md, owner 2026-11-12): the skirmish menu's lists and drop-downs made no click. RotWK plays MiscAudio ComboBoxClick
// (MiscAudio + 0xC0, field table RW 0xBF5750; miscaudio.ini of RotWK and Ennorath: GUIComboBoxClick):
//   * a drop-down's open / close toggle, open or close (RW 0x724495, sound RW 0x724619 .. 0x72465D);
//   * a list with audioFeedback (ListboxData + 0xE, RW 0x725885) on GWM_LEFT_DOWN (5) and GWM_RIGHT_DOWN (13) (RW 0x7270B8 / 0x7270D0 -> RW 0x7271BA ->
//     RW 0x725868) and on Enter (RW 0x72734E); a list without it stays silent.
// Synthetic INI and gadgets; no game files.
#include "doctest.h"

#include "IniTestUtil.h"
#include "Common/Audio/AudioEntryPoints.h"
#include "Common/Audio/GameAudio.h"
#include "Common/Audio/SimulatedAudioDevice.h"
#include "GameClient/GUI/GameWindowManager.h"
#include "GameClient/GUI/Gadget.h"
#include "GameClient/GUI/Gadgets.h"
#include "GameClient/GUI/GameWindow.h"

#include <memory>
#include <string>

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

const char kAudio[] =
	"AudioSettings\n  AudioRoot = Data\\Audio\n  SoundsFolder = Sounds\n  MusicFolder = Tracks\n  StreamingFolder = Speech\n  AmbientStreamFolder = AmbientStreams\n  SoundsExtension = wav\n"
	"  SampleCount2D = 8\n  SampleCount3D = 4\n  StreamCount = 3\n  MinSampleVolume = 2%\n  DefaultSoundVolume = 100%\n  DefaultVoiceVolume = 100%\n  DefaultMusicVolume = 100%\n  DefaultAmbientVolume = 100%\n  DefaultMovieVolume = 100%\nEnd\n"
	"AudioEvent DefaultSoundEffect\n  Volume = 100\n  Priority = high\n  Limit = 0\n  MinRange = 160\n  MaxRange = 640\n  PlayPercent = 100\n  Type = ui everyone\n  Type = +DEFAULT\nEnd\n"
	"AudioEvent GUIComboBoxClick\n  Sounds = click\n  Limit = 0\n  Type = ui everyone\nEnd\n"
	"MiscAudio\n  ComboBoxClick = GUIComboBoxClick\nEnd\n";

struct SoundRig
{
	initest::Fixture fx;
	AudioIniState ini;
	std::unique_ptr<AudioAssetCache> cache;
	std::unique_ptr<SimulatedAudioDevice> device;
	std::unique_ptr<AudioManager> audio;
	double t = 0.0;
	SoundRig()
	{
		fx.mount({ { "data\\audio\\sounds\\click.wav", wav(10) } });
		ini.registerBlocks(fx.env.blocks);
		const std::string err = initest::loadError(fx.env, "audio.ini", kAudio);
		REQUIRE_MESSAGE(err.empty(), err);
		cache = std::make_unique<AudioAssetCache>(&fx.fsys, 8u << 20);
		device = std::make_unique<SimulatedAudioDevice>(cache.get());
		audio = std::make_unique<AudioManager>(ini, *cache, *device, RandomAlgorithm::ZH_CarryChain, 1);
		audio->update(0.0);
		AudioApi::install(audio.get());
	}
	~SoundRig() { AudioApi::install(nullptr); }
	int clicks()
	{
		t += 200.0;
		device->setTime(t);
		audio->update(t);
		int n = 0;
		for (const auto &e : device->log())
		{
			n += e.kind == SimulatedAudioDevice::LogEntry::Start && e.file.find("click") != std::string::npos ? 1 : 0;
		}
		return n;
	}
};
} // namespace

TEST_CASE("P-93 gadget sounds: a list with audioFeedback clicks on left and right press and Enter, one without is silent (RW 0x725868)")
{
	SoundRig s;
	GameWindowManager gwm;
	WinInstanceData data;
	data.m_style = GWS_SCROLL_LISTBOX | GWS_MOUSE_TRACK;
	ListboxData ld;
	ld.listLength = 8;
	ld.columns = 1;
	GameWindow *list = gwm.gogoGadgetListBox(nullptr, WIN_STATUS_ENABLED, 0, 0, 200, 120, &data, &ld, nullptr, false);
	REQUIRE(list);
	gwm.winSendInputMsg(list, GWM_LEFT_DOWN, 0, 0);
	CHECK(s.clicks() == 0); // audioFeedback off
	GadgetListBoxSetAudioFeedback(list, true);
	gwm.winSendInputMsg(list, GWM_LEFT_DOWN, 0, 0);
	CHECK(s.clicks() == 1);
	gwm.winSendInputMsg(list, GWM_RIGHT_DOWN, 0, 0);
	CHECK(s.clicks() == 2);
	gwm.winSendInputMsg(list, GWM_CHAR, KEY_ENTER, KEY_STATE_UP);
	CHECK(s.clicks() == 3);
	gwm.winSendInputMsg(list, GWM_LEFT_UP, 0, 0);
	CHECK(s.clicks() == 3); // the release is no click
}

TEST_CASE("P-93 gadget sounds: a drop-down clicks once when it opens and once when it closes (RW 0x724495 -> RW 0x724619)")
{
	SoundRig s;
	GameWindowManager gwm;
	WinInstanceData data;
	data.m_style = GWS_COMBO_BOX | GWS_MOUSE_TRACK;
	ComboBoxData cd;
	cd.maxDisplay = 5;
	cd.listboxTemplate.listLength = 8;
	cd.listboxTemplate.columns = 1;
	GameWindow *combo = gwm.gogoGadgetComboBox(nullptr, WIN_STATUS_ENABLED, 0, 0, 200, 30, &data, &cd, nullptr, false);
	REQUIRE(combo);
	GadgetComboBoxAddEntry(combo, u"one", 0xFFFFFFFFu);
	GadgetComboBoxAddEntry(combo, u"two", 0xFFFFFFFFu);
	const int before = s.clicks();
	gwm.winSendInputMsg(combo, GWM_LEFT_UP, 0, 0); // opens
	CHECK(s.clicks() == before + 1);
	gwm.winSendInputMsg(combo, GWM_LEFT_UP, 0, 0); // closes
	CHECK(s.clicks() == before + 2);
}
