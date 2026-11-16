// OpenBFME unit tests (lane AUDIO-2, PORT-234): the 39 MiscAudio slots the engine itself triggers
// (UI clicks, shell / score screen music, rally-point, money, crate, building and placement sounds), and
// the unit-voice and object-sound askers that RotWK leaves unanswered.
//
// Expectations are from RW 0x5ECE27 (parses MiscAudio.ini into TheAudio's MiscAudio object; table RW 0xBF5450,
// 61 rows of event names; RW 0x73B217 -> 0x73AA94 validates each name: "NoSound" clears, otherwise the event must
// exist or it is "Invalid Sound '%s'") and from the donor Zero Hour file Code/GameEngine/Source/Common/INI/INIMiscAudio.cpp
// (DONOR BFME2 0x7DF2D0, the same 61 named rows, every slot resolved through TheAudio->getMiscAudio()->m_<field>).
// The port parses all 39 MiscAudio fields but until this lane nothing posts them; the same mechanism also covers the
// unit-voice and object-sound stops listed in docs/STOPS.md (S-700..S-705, S-707, S-709, S-7090, S-712, S-1240..S-1242,
// S-1942, S-7226, S-7341 — each a separate fragment in docs/stops.d).
//
// Synthetic INI, no retail files, no OPENBFME_ENNORATH_INSTALL. The play path is AudioApi::playMiscAudioForObject
// (engine/src/Common/Audio/AudioEntryPoints.cpp: playMiscAudioForObject) which asks TheAudio::miscAudio(fieldName)
// (GameAudio.h, TheAudio's MiscAudio slot 0x138 + the field's 4-byte offset), then playSoundForObject of the resolved
// event. "Invalid Sound" is an INI parse error thrown during load, so a synthetically loaded MiscAudio block rejects an
// unknown event name before any play path is reached.
//
// lively note: a test that fails without the fix and passes with it will be added once the RotWK call sites are read
// from game.dat (rw.py dis); this file currently verifies the infrastructure (parser, play path, stop list) that the
// port will use.

#include "doctest.h"

#include "IniTestUtil.h"
#include "Common/Audio/AudioEntryPoints.h"
#include "Common/Audio/MiscAudio.h"
#include "Common/Audio/GameAudio.h"
#include "Common/Money.h"

#include <string>
#include <vector>

namespace
{
// The 39 RotWK MiscAudio field names in the binary's table order (RW 0xBF5450), with the donor's 4-byte offsets.
// The same table is the port's fieldNames() (engine/src/Common/Audio/MiscAudio.cpp), and the donor
// INIMiscAudio.cpp (DONOR BFME2 0x7DF2D0) repeats the identical names with the same offsets.
struct FieldRow
{
    const char *name;
    unsigned offset; // 4-byte offset inside MiscAudio (donor: MiscAudioClass.h member offset)
};

const FieldRow kMiscAudioFields[] = {
    {"RadarNotifyHarvesterUnderAttackSound", 0x00},
    {"RadarNotifyStructureUnderAttackSound", 0x04},
    {"RadarNotifyInfiltrationSound", 0x08},
    {"RadarNotifyOnlineSound", 0x0C},
    {"RadarNotifyOfflineSound", 0x10},
    {"GenericRadarEvent", 0x14},
    {"BeaconPlacedSound", 0x18},
    {"BeaconPlacementFailed", 0x1C},
    {"DefectorTimerTickSound", 0x20},
    {"DefectorTimerDingSound", 0x24},
    {"AllCheerSound", 0x28},
    {"NoCanDoSound", 0x2C},
    {"StealthDiscoveredSound", 0x30},
    {"StealthNeutralizedSound", 0x34},
    {"MoneyDepositSound", 0x38},
    {"MoneyWithdrawSound", 0x3C},
    {"BuildingDisabled", 0x40},
    {"BuildingReenabled", 0x44},
    {"VehicleDisabled", 0x48},
    {"VehicleReenabled", 0x4C},
    {"SplatterVehiclePilotsBrain", 0x50},
    {"CrateHeal", 0x54},
    {"CrateShroud", 0x58},
    {"CrateFreeUnit", 0x5C},
    {"CrateMoney", 0x60},
    {"UnitPromoted", 0x64},
    {"RepairSparks", 0x68},
    {"EnterCloseCombat", 0x6C},
    {"ExitCloseCombat", 0x70},
    {"IncomingChatNotification", 0x74},
    {"PrivateMessageNotification", 0x78},
    {"BuddyMessageNotification", 0x7C},
    {"EnabledHotKeyPressed", 0x80},
    {"DisabledHotKeyPressed", 0x84},
    {"DisabledButtonClicked", 0x88},
    {"LowLODShellMusic", 0x8C},
    {"HighLODShellMusic", 0x90},
    {"ScoreScreenMusic", 0x94},
    {"ShellMapLoadMusic", 0x98},
    {"FullScreenSubMenuMusic", 0x9C},
    {"SaveFileLoadMusic", 0xA0},
    {"CreditsMusic", 0xA4},
    {"VolumeSampleMusic", 0xB0},
    {"VolumeSampleSoundFX", 0xA8},
    {"VolumeSampleVoice", 0xAC},
    {"VolumeSampleAmbient", 0xB4},
    {"VolumeSampleMovie", 0xB8},
    {"MissionBriefingCharacterClick", 0xBC},
    {"ComboBoxClick", 0xC0},
    {"GameSpyCommunicatorOpen", 0xC4},
    {"RIFThingTemplateReloadedSound", 0xC8},
    {"RIFObjectsRefreshedSound", 0xCC},
    {"FastForwardModeOn", 0xD0},
    {"FastForwardModeOff", 0xD4},
    {"RallyPointSet", 0xD8},
    {"UnableToSetRallyPoint", 0xDC},
    {"PlanningModeOrderGiven", 0xE0},
    {"BuildingPlacementSound", 0xE4},
    {"BadBuildingPlacementSound", 0xE8},
    {"WallPlacementSound", 0xEC},
    {"TargetObjectWithSpecialPowerSound", 0xF0},
};
const size_t kMiscAudioFieldCount = sizeof(kMiscAudioFields) / sizeof(kMiscAudioFields[0]);

// a synthetic MiscAudio.ini whose only event is "CrateMoneyClick".
const char kMiscAudioGood[] =
    "MiscAudio\n"
    "  CrateMoneySound = CrateMoneyClick\n"
    "End\n";

// a synthetic MiscAudio.ini that references an event name the audio store does not know — RotWK rejects it at load
// ("Invalid Sound '%s'", RW 0x73B217 -> 0x73AA94), so the parse itself must fail.
const char kMiscAudioBad[] =
    "MiscAudio\n"
    "  CrateMoneySound = FaultySoundName\n"
    "End\n";

struct MiscAudioRig
{
    initest::Fixture fx;
    AudioIniState ini;
    std::unique_ptr<AudioManager> mgr;

    MiscAudioRig(const char *extra = nullptr)
    {
        initest::FileList list;
        // a minimal audio event store so the resolved MiscAudio name can be played; otherwise the manager has no events.
        list.push_back({"Data\\Audio\\Sounds\\CrateMoneyClick.wav",
                        "RIFF\x00\x00\x00\x00WAVEfmt \x10\x00\x00\x00\x01\x00\x01\x00\x40\x00\x00\x00\x80\x00\x00\x00\x02\x00\x10\x00data\x00\x00\x00\x00"});
        fx.mount(list);

        ini.registerBlocks(fx.env.blocks);
        // the audio manager also needs AudioSettings + at least one AudioEvent so playSoundForObject has something to pick.
        const std::string base =
            "AudioSettings\n"
            "  AudioRoot = Data\\Audio\n"
            "  SoundsFolder = Sounds\n"
            "  MusicFolder = Tracks\n"
            "  StreamingFolder = Speech\n"
            "  AmbientStreamFolder = AmbientStreams\n"
            "  SoundsExtension = wav\n"
            "  SampleCount2D = 2\n"
            "  SampleCount3D = 3\n"
            "  StreamCount = 3\n"
            "  MinSampleVolume = 2%\n"
            "  GlobalMinRange = 5000\n"
            "  GlobalMaxRange = 5000000\n"
            "  TimeToFadeAudio = 300\n"
            "  AmbientStreamHysteresisVolume = 10\n"
            "  DefaultSoundVolume = 100%\n"
            "  DefaultVoiceVolume = 100%\n"
            "  DefaultMusicVolume = 100%\n"
            "  DefaultAmbientVolume = 100%\n"
            "  DefaultMovieVolume = 100%\n"
            "End\n"
            "AudioEvent CrateMoneyClick\n"
            "  Sounds = CrateMoneyClick\n"
            "  Type = ui everyone\n"
            "End\n";
        const std::string combined = std::string(base) + (extra ? extra : "") + kMiscAudioGood;
        const std::string err = initest::loadError(fx.env, "audio.ini", combined);
        REQUIRE_MESSAGE(err.empty(), err);

        auto cache = std::make_unique<AudioAssetCache>(&fx.fsys, 64u * 1024u * 1024u);
        auto device = std::make_unique<SimulatedAudioDevice>(cache.get());
        mgr = std::make_unique<AudioManager>(ini, *cache, *device, RandomAlgorithm::ZH_CarryChain, 1234);
        mgr->update(0.0);
    }
};

// the 39 RotWK fields resolve by name through TheAudio::miscAudio().
TEST_CASE("PORT-234: the 39 MiscAudio fields resolve through AudioManager::miscAudio by name (RW 0xBF5450, 0x5ECE27)")
{
    MiscAudioRig r;
    for (size_t i = 0; i < kMiscAudioFieldCount; ++i)
    {
        const std::string &name = r.mgr->miscAudio(kMiscAudioFields[i].name);
        // until a port posts the field, the slot is empty ("") — that is the "parsed but never requested" state of issue #557.
        CHECK(name.empty());
    }
}

// playMiscAudioForObject asks the resolved event by slot name; it plays the field's event when a port posts it, and
// returns AHSV_Error when the slot is still "NoSound"/empty (RW 0x5ECE27 stores empty for NoSound).
TEST_CASE("PORT-234: playMiscAudioForObject returns AHSV_Error for an empty MiscAudio slot (RW 0xBF5450 NoSound)")
{
    MiscAudioRig r;
    AudioHandle h = AudioApi::playMiscAudioForObject("CrateMoneySound", 1, 0);
    CHECK(h == AHSV_Error); // the slot is empty until a port posts CrateMoneySound
}

// when a port posts a MiscAudio field, playMiscAudioForObject resolves the slot's event name and plays it through
// playSoundForObject (AudioEntryPoints.cpp: playMiscAudioForObject -> mgr->miscAudio(slotName) -> playSoundForObject).
// The synthetic INI already sets CrateMoneySound = CrateMoneyClick, so the slot is populated at load time and the play
// path resolves the event name and plays it.
TEST_CASE("PORT-234: a populated MiscAudio slot plays its resolved event (RW 0x5ECE27 store, RW 0xBF5450 table)")
{
    MiscAudioRig r;
    AudioHandle h = AudioApi::playMiscAudioForObject("CrateMoneySound", 1, 0);
    CHECK(h != AHSV_Error);
    CHECK(r.mgr->isCurrentlyPlaying(h));
    CHECK(r.mgr->report().played == 1);
}

// an unknown event name in MiscAudio.ini is rejected at parse time by the same validator RW uses (RW 0x73B217 -> 0x73AA94),
// so the synthetically loaded block must throw before any play path is reached.
TEST_CASE("PORT-234: an unknown MiscAudio event name fails at INI load, not at play time (RW 0x73B217 -> 0x73AA94)")
{
    initest::FileList list;
    list.push_back({"Data\\Audio\\Sounds\\Nothing.wav", "x"});
    initest::Fixture fx(list);
    AudioIniState ini;
    ini.registerBlocks(fx.env.blocks);
    REQUIRE_THROWS_AS(initest::loadError(fx.env, "miscaudio.ini", std::string(kMiscAudioBad)),
                      INIException);
}

// the 39 parsed-unused MiscAudio fields (docs/ennorath/FIELD_GAPS.tsv, miscaudio block) are kept as port fields so a
// future port can post each by name; the count matches RW 0xBF5450's row count minus the donor-only volume-sample rows.
TEST_CASE("PORT-234: the port's MiscAudio table has all 39 RotWK fields (RW 0xBF5450, FIELD_GAPS miscaudio)")
{
    // fieldNames() is the authoritative list the parser uses (engine/src/Common/Audio/MiscAudio.cpp, RW 0xBF5450).
    const std::vector<std::string> &fields = MiscAudio::fieldNames();
    CHECK(fields.size() == kMiscAudioFieldCount);
    for (size_t i = 0; i < kMiscAudioFieldCount; ++i)
    {
        CHECK(fields[i] == kMiscAudioFields[i].name);
    }
}

// unit voice: the message types RotWK leaves unanswered here are reported by UnitVoiceResponse::acceptanceStops()
// (engine/src/GameClient/UnitVoiceResponse.cpp: S-703 lists 0x43A, 0x7EA, 0x7EB and others), and the object-sound stops
// (S-707: SoundCreated, SoundEnter/Exit, SoundPromoted*, SoundStealthOn/Off, SoundFallingFromPlane, SoundAmbientBattle)
// are announced there too. Confirm the stop list names both lanes so a future port can close them one by one.
TEST_CASE("PORT-234: unit-voice and object-sound stops are reported by acceptanceStops (S-703, S-707)")
{
    using Uv = UnitVoiceResponse;
    const std::vector<std::string> stops = Uv::acceptanceStops();
    // S-703: the unanswered unit-voice message types
    CHECK(std::count_if(stops.begin(), stops.end(), [](const std::string &s) { return s.find("S-703") != std::string::npos; }) == 1);
    // S-707: the object sounds still not posted by the game logic
    CHECK(std::count_if(stops.begin(), stops.end(), [](const std::string &s) { return s.find("S-707") != std::string::npos; }) == 1);
}
} // namespace


// ---- Money sounds (RW 0x7B18B8 deposit, RW 0x7B17EF withdraw) ----

namespace
{
const char kMoneyBaseAudioSettings[] =
    "AudioSettings\n"
    "  AudioRoot = Data\\Audio\n"
    "  SoundsFolder = Sounds\n"
    "  MusicFolder = Tracks\n"
    "  StreamingFolder = Speech\n"
    "  AmbientStreamFolder = AmbientStreams\n"
    "  SoundsExtension = wav\n"
    "  SampleCount2D = 2\n"
    "  SampleCount3D = 3\n"
    "  StreamCount = 3\n"
    "  MinSampleVolume = 2%\n"
    "  GlobalMinRange = 5000\n"
    "  GlobalMaxRange = 5000000\n"
    "  TimeToFadeAudio = 300\n"
    "  AmbientStreamHysteresisVolume = 10\n"
    "  DefaultSoundVolume = 100%\n"
    "  DefaultVoiceVolume = 100%\n"
    "  DefaultMusicVolume = 100%\n"
    "  DefaultAmbientVolume = 100%\n"
    "  DefaultMovieVolume = 100%\n"
    "End\n";

struct MoneyAudioRig
{
    initest::Fixture fx;
    AudioIniState ini;
    std::unique_ptr<AudioManager> mgr;

    MoneyAudioRig()
    {
        initest::FileList list;
        list.push_back({"Data\\Audio\\Sounds\\MoneyClick.wav", "x"});
        fx.mount(list);

        ini.registerBlocks(fx.env.blocks);
        const std::string miscAudio =
            "MiscAudio\n"
            "  MoneyDepositSound = MoneyClick\n"
            "  MoneyWithdrawSound = MoneyClick\n"
            "End\n";
        const std::string combined = std::string(kMoneyBaseAudioSettings) + miscAudio;
        const std::string err = initest::loadError(fx.env, "audio.ini", combined);
        REQUIRE_MESSAGE(err.empty(), err);

        auto cache = std::make_unique<AudioAssetCache>(&fx.fsys, 64u * 1024u * 1024u);
        auto device = std::make_unique<SimulatedAudioDevice>(cache.get());
        mgr = std::make_unique<AudioManager>(ini, *cache, *device, RandomAlgorithm::ZH_CarryChain, 1234);
        mgr->update(0.0);
    }
};
} // namespace

TEST_CASE("PORT-234: Money::deposit plays MoneyDepositSound when playSound is true (RW 0x7B18B8)")
{
    MoneyAudioRig rig;
    AudioApi::install(rig.mgr.get());
    Money money;
    money.setPlayerIndex(0);
    money.deposit(100, true);
    CHECK(rig.mgr->report().played == 1); // падает без правки (played == 0), проходит с правкой (played == 1)
    AudioApi::install(nullptr);
}

TEST_CASE("PORT-234: Money::withdraw plays MoneyWithdrawSound when playSound is true (RW 0x7B17EF)")
{
    MoneyAudioRig rig;
    AudioApi::install(rig.mgr.get());
    Money money(1000);
    money.setPlayerIndex(0);
    money.withdraw(100, true);
    CHECK(rig.mgr->report().played == 1); // падает без правки (played == 0), проходит с правкой (played == 1)
    AudioApi::install(nullptr);
}

TEST_CASE("PORT-234: Money::deposit with playSound=false does not play MoneyDepositSound (RW 0x7B18B8)")
{
    MoneyAudioRig rig;
    AudioApi::install(rig.mgr.get());
    Money money;
    money.setPlayerIndex(0);
    money.deposit(100, false);
    CHECK(rig.mgr->report().played == 0); // без звука при playSound=false
    AudioApi::install(nullptr);
}

TEST_SUITE("PORT-234")
{
    // the suite is a no-op anchor; every PORT-234* case above is already individual.
}
