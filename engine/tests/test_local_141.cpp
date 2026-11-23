// OpenBFME unit tests: LOCAL-141 (fork issue 858) — engine/data/client stop closures. GPL-3.0.
// Synthetic INI only: no ROTWK_INSTALL / OPENBFME_ENNORATH_INSTALL, no retail files.
//
// Target facts verified against RotWK 2.01 game.dat (md5 daed3668...):
//   * S-7364: ControlBar::processCommandUI (RW 0x940435):
//       - RW 0x9404A1..0x9404AB (DONOR BFME2 ControlBarCommandProcessing.cpp:362):
//         `if (repeated && commandType != GUI_COMMAND_UNIT_BUILD (3)) return CBC_COMMAND_NOT_USED`.
//       - RW 0x940BE6..0x940BF0 (DONOR BFME2 ControlBarCommandProcessing.cpp:515):
//         in GUI_COMMAND_UNIT_BUILD, `if (repeated && !multiple) return CBC_COMMAND_NOT_USED`.
//   * S-704: UnitVoiceResponse::moveVoices (RW 0x8DE838 -> RW 0x8DE97E..0x8DE9D6,
//       DONOR BFME2 PickAndPlayUnitVoiceResponse.cpp:1049):
//       when !enterState, InGameUI +0x8B9 (isInForceMoveToMode) is true, player +0x770 != 2,
//       and target passes Object::canCrushOrSquish(target, 2) (RW 0x69519A), pick "VoiceCrush"
//       (RW 0xC77A58) with rank.special = 1; else if MSG_DO_SALVAGE (0x442), pick "VoiceSalvage".
//   * S-1462: AudioManager::shouldPlayLocally (RW 0x4533A9 -> RW 0x453488..0x4534C8):
//       for VIEW_LIVING_WORLD (1), resolve owner (RW 0x6DADCC), compare with local player
//       (RW 0x6B3557): equal -> ST_PLAYER, else localPlayer && isAlly(owner) (RW 0x6E1EC4) ->
//       ST_ALLIES, else -> ST_ENEMIES.
//   * S-1952: MultiplayerSettings::MultiplayerSettings (RW 0x7836C0, DONOR BFME2 Rva 0x003811FB)
//       constructs +0x44 (observer) and +0x84 (random) via MultiplayerColorDefinition ctor
//       (RW 0x783136, DONOR BFME2 Rva 0x00380C4E), setting m_color (+0x10) = 0xFFFFFFFF (RW 0x783171);
//       MultiplayerSettings::getColor (RW 0x78385B) returns +0x84 for -1 and +0x44 for -2.

#include "doctest.h"

#include "IniTestUtil.h"
#include "MoveTestUtil.h"
#include "ProdTestUtil.h"

#include "Common/Audio/AudioAssetCache.h"
#include "Common/Audio/AudioIni.h"
#include "Common/Audio/GameAudio.h"
#include "Common/Audio/SimulatedAudioDevice.h"
#include "GameClient/ControlBar.h"
#include "GameClient/ControlBarCommands.h"
#include "GameClient/GUI/PlayerStatusInfo.h"
#include "GameClient/HudContext.h"
#include "GameClient/InGameUI.h"
#include "GameClient/MessageStream/MessageStream.h"
#include "GameClient/UnitVoiceResponse.h"
#include "GameLogic/GameMessage.h"
#include "GameLogic/Object/Object.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace
{

// ---- S-7364 rig (ControlBar::pressButton repeated-press gate) ----
struct RepeatRig : prodtest::ProdWorld
{
	PinholeView view;
	InGameUI ui;
	MessageStream stream;
	HudContext ctx{ *logic, nullptr, view, ui, stream, &commands, {} };
	ControlBar bar{ ctx };
	Object *barracks = nullptr;

	RepeatRig()
	{
		const std::string objects =
			"Object L141Soldier\n"
			"  BuildCost = 100\n"
			"  BuildTime = 10.0\n"
			"  CommandPoints = 4\n"
			"  KindOf = SELECTABLE INFANTRY\n"
			"  Behavior = AIUpdateInterface ModuleTag_AI\n"
			"  End\n"
			"End\n"
			"Object L141UniqueHero\n"
			"  BuildCost = 100\n"
			"  BuildTime = 10.0\n"
			"  CommandPoints = 4\n"
			"  MaxSimultaneousOfType = 1\n"
			"  KindOf = SELECTABLE INFANTRY\n"
			"  Behavior = AIUpdateInterface ModuleTag_AI\n"
			"  End\n"
			"End\n"
			"Object L141Barracks\n"
			"  KindOf = SELECTABLE STRUCTURE FS_FACTORY\n"
			"  CommandSet = L141BarracksCS\n"
			"  Behavior = ProductionUpdate ModuleTag_Prod\n"
			"    NumDoorAnimations = 0\n"
			"    MaxQueueEntries = 9\n"
			"  End\n"
			"  Behavior = DefaultProductionExitUpdate ModuleTag_Exit\n"
			"    UnitCreatePoint = X:20 Y:0 Z:0\n"
			"    NaturalRallyPoint = X:50 Y:0 Z:0\n"
			"  End\n"
			"End\n";
		const std::string cmds =
			"CommandButton Cmd_L141Soldier\n"
			"  Command = UNIT_BUILD\n"
			"  Object = L141Soldier\n"
			"  InPalantir = Yes\n"
			"  Options = CANCELABLE\n"
			"End\n"
			"CommandButton Cmd_L141Hero\n"
			"  Command = UNIT_BUILD\n"
			"  Object = L141UniqueHero\n"
			"  InPalantir = Yes\n"
			"  Options = CANCELABLE\n"
			"End\n"
			"CommandButton Cmd_L141Sell\n"
			"  Command = SELL\n"
			"  InPalantir = Yes\n"
			"End\n"
			"CommandSet L141BarracksCS\n"
			"  1 = Cmd_L141Soldier\n"
			"  2 = Cmd_L141Hero\n"
			"  3 = Cmd_L141Sell\n"
			"End\n";
		load(objects);
		load(cmds);
		alice()->getMoney()->deposit(5000);
		barracks = make("L141Barracks", teamOf("Alice"));
		REQUIRE(barracks != nullptr);
		const Coord3D pos{ 100.0f, 100.0f, 0.0f };
		barracks->setPosition(&pos);
		players.setLocalPlayer(alice());
		ui.selectObject(barracks->getID());
		bar.update();
	}
};

// ---- S-704 voice sink ----
struct CrushVoiceSink : UnitVoiceResponse::Sink
{
	std::vector<std::string> sounds;
	bool soundExists(const std::string &name) override { return !name.empty(); }
	void playSound(const std::string &name, ObjectID, int) override { sounds.push_back(name); }
	void reportEva(const std::string &, const Coord3D *) override {}
};

// ---- S-1462 audio fixture ----
constexpr const char *kL141AudioBase =
	"AudioSettings\n"
	"  AudioRoot = Data\\Audio\n"
	"  SoundsFolder = Sounds\n"
	"  MusicFolder = Music\n"
	"  StreamingFolder = Tracks\n"
	"  SoundsExtension = .wav\n"
	"  SampleCount2D = 8\n"
	"  SampleCount3D = 16\n"
	"  StreamCount = 4\n"
	"  GlobalMinRange = 160\n"
	"  GlobalMaxRange = 640\n"
	"  TimeToFadeAudio = 1000\n"
	"  AudioFootprintInBytes = 2000000\n"
	"  MinSampleVolume = 1\n"
	"  AmbientStreamHysteresisVolume = 10\n"
	"  DefaultSoundVolume = 100%\n"
	"  DefaultVoiceVolume = 100%\n"
	"  DefaultMusicVolume = 100%\n"
	"  DefaultAmbientVolume = 100%\n"
	"  DefaultMovieVolume = 100%\n"
	"End\n"
	"AudioEvent DefaultSoundEffect\n"
	"  Volume = 100\n"
	"  Priority = high\n"
	"  Limit = 4\n"
	"  MinRange = 160\n"
	"  MaxRange = 640\n"
	"  PitchShift = 0 0\n"
	"  PlayPercent = 100\n"
	"  Type = ui everyone\n"
	"  Type = +DEFAULT\n"
	"End\n"
	"MusicTrack DefaultMusicTrack\n"
	"  Filename = NoFilename\n"
	"  Volume = 100\n"
	"  Type = DEFAULT\n"
	"  SubmixSlider = music\n"
	"End\n"
	"DialogEvent DefaultDialog\n"
	"  Volume = 100\n"
	"  Type = ui everyone\n"
	"  Type = +DEFAULT\n"
	"End\n"
	"AmbientStream DefaultAmbientStream\n"
	"  Filename = NoFilename\n"
	"  Volume = 100\n"
	"  Type = DEFAULT\n"
	"  MinRange = 100\n"
	"  MaxRange = 1000\n"
	"  SubmixSlider = ambient\n"
	"End\n";

std::string l141WavBytes(double ms)
{
	const std::uint32_t rate = 22050, frames = (std::uint32_t)(ms * rate / 1000.0);
	std::string out;
	auto u32 = [&](std::uint32_t v) {
		for (int i = 0; i < 4; ++i)
		{
			out.push_back((char)(std::uint8_t)(v >> (8 * i)));
		}
	};
	auto u16 = [&](std::uint16_t v) {
		out.push_back((char)(std::uint8_t)v);
		out.push_back((char)(std::uint8_t)(v >> 8));
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

struct L141Audio
{
	initest::Fixture fx;
	AudioIniState ini;
	std::unique_ptr<AudioAssetCache> cache;
	std::unique_ptr<SimulatedAudioDevice> device;
	std::unique_ptr<AudioManager> mgr;

	explicit L141Audio(const std::string &events)
	{
		fx.mount({ { "data\\audio\\sounds\\a.wav", l141WavBytes(500.0) } });
		ini.registerBlocks(fx.env.blocks);
		const std::string err = initest::loadError(fx.env, "audio.ini", std::string(kL141AudioBase) + events);
		REQUIRE_MESSAGE(err.empty(), err);
		cache = std::make_unique<AudioAssetCache>(&fx.fsys, 1u << 20);
		device = std::make_unique<SimulatedAudioDevice>(cache.get());
		mgr = std::make_unique<AudioManager>(ini, *cache, *device, RandomAlgorithm::ZH_CarryChain, 7);
	}
};

} // namespace

TEST_CASE("LOCAL-141 S-7364: ControlBar::pressButton repeated-press gate (RW 0x9404A1, RW 0x940BE6)")
{
	// Expectations from RW 0x9404A1..0x9404AB and RW 0x940BE6..0x940BF0 (DONOR BFME2 ControlBarCommandProcessing.cpp:362, :515).
	RepeatRig rig;
	auto findBtn = [&](const char *name) -> const ControlBarButton * {
		for (const ControlBarButton &b : rig.bar.palantirButtons())
		{
			if (b.button && b.button->m_name == name)
			{
				return &b;
			}
		}
		return nullptr;
	};
	const ControlBarButton *soldierBtn = findBtn("Cmd_L141Soldier");
	const ControlBarButton *heroBtn = findBtn("Cmd_L141Hero");
	const ControlBarButton *sellBtn = findBtn("Cmd_L141Sell");
	REQUIRE(soldierBtn != nullptr);
	REQUIRE(heroBtn != nullptr);
	REQUIRE(sellBtn != nullptr);

	// 1. UNIT_BUILD without Shift (multiple == false) + repeated == true -> refused (RW 0x940BE6..0x940BF0)
	CHECK_FALSE(rig.bar.pressButton(soldierBtn->slot, true, /*shift=*/false, /*repeated=*/true));
	CHECK(rig.stream.propagate().empty());

	// 2. UNIT_BUILD with Shift and MaxSimultaneousOfType == 0 (multiple == true) + repeated == true -> allowed
	CHECK(rig.bar.pressButton(soldierBtn->slot, true, /*shift=*/true, /*repeated=*/true));
	{
		std::vector<ClientMessage> out = rig.stream.propagate();
		REQUIRE(out.size() == 1);
		CHECK(out[0].type() == MSG_QUEUE_UNIT_CREATE);
		REQUIRE(out[0].argumentCount() == 5);
		CHECK(out[0].arg(3).boolean == true);
	}

	// 3. UNIT_BUILD with Shift but MaxSimultaneousOfType == 1 (multiple == false) + repeated == true -> refused
	CHECK_FALSE(rig.bar.pressButton(heroBtn->slot, true, /*shift=*/true, /*repeated=*/true));
	CHECK(rig.stream.propagate().empty());

	// 4. Non-UNIT_BUILD command (SELL) + repeated == true -> refused even with Shift (RW 0x9404A1..0x9404AB)
	CHECK_FALSE(rig.bar.pressButton(sellBtn->slot, true, /*shift=*/true, /*repeated=*/true));
	CHECK(rig.stream.propagate().empty());

	// 5. Neighbouring behaviour: non-repeated press of SELL still works
	CHECK(rig.bar.pressButton(sellBtn->slot, true, /*shift=*/false, /*repeated=*/false));
	{
		std::vector<ClientMessage> out = rig.stream.propagate();
		REQUIRE(out.size() == 1);
		CHECK(out[0].type() == MSG_SELL);
	}
}

TEST_CASE("LOCAL-141 S-704: UnitVoiceResponse::moveVoices VoiceCrush (RW 0x8DE97E..0x8DE9D6, RW 0x69519A)")
{
	// Expectations from RW 0x8DE97E..0x8DE9D6 (DONOR BFME2 PickAndPlayUnitVoiceResponse.cpp:1049).
	static const char *kCrushData =
		"Object L141Cavalry\n"
		"  KindOf = SELECTABLE CAVALRY\n"
		"  CrusherLevel = 2\n"
		"  VoiceMove = CavMove\n"
		"  UnitSpecificSounds\n"
		"    VoiceCrush = CavCrush\n"
		"    VoiceSalvage = CavSalvage\n"
		"  End\n"
		"  Behavior = AIUpdateInterface ModuleTag_AI\n"
		"  End\n"
		"End\n"
		"Object L141Infantry\n"
		"  KindOf = SELECTABLE INFANTRY\n"
		"  CrushableLevel = 1\n"
		"  Behavior = AIUpdateInterface ModuleTag_AI\n"
		"  End\n"
		"End\n"
		"Object L141Heavy\n"
		"  KindOf = SELECTABLE INFANTRY\n"
		"  CrushableLevel = 3\n"
		"  Behavior = AIUpdateInterface ModuleTag_AI\n"
		"  End\n"
		"End\n";

	movetest::MoveWorld mw(40, 40, kCrushData);
	Team *aliceTeam = mw.players.findPlayerWithName("Alice")->getDefaultTeam();
	Team *bobTeam = mw.players.findPlayerWithName("Bob")->getDefaultTeam();
	Object *cav = mw.spawn("L141Cavalry", 100.0f, 100.0f, 0.0f, aliceTeam);
	Object *enemyWeak = mw.spawn("L141Infantry", 150.0f, 100.0f, 0.0f, bobTeam);
	Object *enemyHeavy = mw.spawn("L141Heavy", 180.0f, 100.0f, 0.0f, bobTeam);
	REQUIRE(cav != nullptr);
	REQUIRE(enemyWeak != nullptr);
	REQUIRE(enemyHeavy != nullptr);

	InGameUI ui;
	CrushVoiceSink sink;
	UnitVoiceResponse voice(*mw.logic, sink);
	voice.setInGameUI(&ui);
	voice.setLocalPlayerIndex(mw.playerIndex("Alice"));

	UnitVoiceResponse::Info info;
	info.target = enemyWeak->getID();
	info.hasPosition = true;
	info.position = Coord3D{ 150.0f, 100.0f, 0.0f };

	// 1. Force-move mode on + crushable enemy -> VoiceCrush (RW 0x8DE97E..0x8DE9D6)
	ui.setForceMoveToMode(true);
	CHECK(voice.pickAndPlay({ cav->getID() }, MSG_DO_MOVETO, &info));
	CHECK(sink.sounds == std::vector<std::string>{ "CavCrush" });

	// 2. Neighbour: force-move mode off -> VoiceMove
	sink.sounds.clear();
	ui.setForceMoveToMode(false);
	CHECK(voice.pickAndPlay({ cav->getID() }, MSG_DO_MOVETO, &info));
	CHECK(sink.sounds == std::vector<std::string>{ "CavMove" });

	// 3. Neighbour: force-move mode on, but target is not crushable (CrushableLevel 3 > CrusherLevel 2) -> VoiceMove
	sink.sounds.clear();
	ui.setForceMoveToMode(true);
	UnitVoiceResponse::Info heavyInfo = info;
	heavyInfo.target = enemyHeavy->getID();
	CHECK(voice.pickAndPlay({ cav->getID() }, MSG_DO_MOVETO, &heavyInfo));
	CHECK(sink.sounds == std::vector<std::string>{ "CavMove" });

	// 4. Neighbour: MSG_DO_SALVAGE is skipped when force-move branch is entered (else-if at RW 0x8DE985..0x8DE9FB),
	//    and plays VoiceSalvage when force-move is off.
	sink.sounds.clear();
	ui.setForceMoveToMode(true);
	CHECK(voice.pickAndPlay({ cav->getID() }, MSG_DO_SALVAGE, &heavyInfo));
	CHECK(sink.sounds == std::vector<std::string>{ "CavMove" });

	sink.sounds.clear();
	ui.setForceMoveToMode(false);
	CHECK(voice.pickAndPlay({ cav->getID() }, MSG_DO_SALVAGE, &heavyInfo));
	CHECK(sink.sounds == std::vector<std::string>{ "CavSalvage" });

	// 5. Neighbour: enter-state move never triggers VoiceCrush (RW 0x8DE881)
	sink.sounds.clear();
	ui.setForceMoveToMode(true);
	voice.pickAndPlay({ cav->getID() }, UnitVoiceResponse::VOICE_EVENT_ENTER_STATE_MOVE, &info);
	for (const std::string &played : sink.sounds)
	{
		CHECK(played != "CavCrush");
	}
}

TEST_CASE("LOCAL-141 S-1462: AudioManager::shouldPlayLocally Living World owner filter (RW 0x453488..0x4534C8)")
{
	// Expectations from RW 0x453488..0x4534C8 (RW 0x6DADCC owner, RW 0x6B3557 local player, RW 0x6E1EC4 isAlly).
	static const char *kEvents =
		"AudioEvent L141Mine\n"
		"  Sounds = a\n"
		"  Type = player\n"
		"  Limit = 0\n"
		"End\n"
		"AudioEvent L141Ally\n"
		"  Sounds = a\n"
		"  Type = allies\n"
		"  Limit = 0\n"
		"End\n"
		"AudioEvent L141Foe\n"
		"  Sounds = a\n"
		"  Type = enemies\n"
		"  Limit = 0\n"
		"End\n";

	L141Audio a(kEvents);
	AudioWorldQueries q;
	q.livingWorldLocalPlayerIndex = []() { return 1; };
	q.livingWorldIsAlly = [](int owner) { return owner == 2; };
	a.mgr->setWorldQueries(q);

	auto send = [&](const char *name, int owner) {
		AudioEventRTS ev(name, VIEW_LIVING_WORLD);
		ev.setPlayerIndex(owner);
		return a.mgr->addAudioEvent(ev);
	};

	// Local player (owner == 1): only ST_PLAYER plays
	CHECK(send("L141Mine", 1) == AHSV_FirstHandle);
	CHECK(send("L141Ally", 1) == AHSV_NotForLocal);
	CHECK(send("L141Foe", 1) == AHSV_NotForLocal);

	// Ally (owner == 2): only ST_ALLIES plays
	CHECK(send("L141Mine", 2) == AHSV_NotForLocal);
	CHECK(send("L141Ally", 2) == AHSV_FirstHandle + 4);
	CHECK(send("L141Foe", 2) == AHSV_NotForLocal);

	// Enemy / neutral (owner == 3): only ST_ENEMIES plays
	CHECK(send("L141Mine", 3) == AHSV_NotForLocal);
	CHECK(send("L141Ally", 3) == AHSV_NotForLocal);
	CHECK(send("L141Foe", 3) == AHSV_FirstHandle + 8);

	CHECK(a.mgr->report().missingHooks.empty());

	// Neighbour: without Living World queries bound, event plays and reports S-1462 missing hook
	L141Audio unbound(kEvents);
	AudioEventRTS ev("L141Foe", VIEW_LIVING_WORLD);
	ev.setPlayerIndex(1);
	CHECK(unbound.mgr->addAudioEvent(ev) == AHSV_FirstHandle);
	REQUIRE(unbound.mgr->report().missingHooks.size() == 1);
	CHECK(unbound.mgr->report().missingHooks.begin()->first.find("S-1462") != std::string::npos);
}

TEST_CASE("LOCAL-141 S-1952: PlayerStatusInput random and observer colours match MultiplayerSettings ctor (RW 0x7836C0 -> RW 0x783136)")
{
	// Expectations from RW 0x7836C0 (MultiplayerSettings ctor) -> RW 0x783136 (MultiplayerColorDefinition ctor:
	// m_color +0x10 = 0xFFFFFFFF at RW 0x783171), RW 0x78385B (getColor: -1 -> +0x84, -2 -> +0x44).
	PlayerStatusInput in;
	CHECK(in.randomColor == 0xFFFFFFFFu);
	CHECK(in.observerColor == 0xFFFFFFFFu);
}
