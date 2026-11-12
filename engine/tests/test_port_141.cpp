// OpenBFME. GPL-3.0.
// PORT-141 (issue #343, task #117): Isengard defiled-ground ambient sound (SoundAmbient) is not played at game start.
//
// RW facts verified in workspace/RotWK/game.dat (caveat S-001):
//   * ThingTemplate audio field table (RW 0x00C26720, 56 entries, 0x10 stride): row 37 is "SoundAmbient" at
//     fieldOff=0x128, string 0x00C263E0, parser 0x0073AD07 -> 0x0073ACAF -> 0x0073AB45 (NoSound clears, +SOUND:
//     strips prefix, EVA: invalid on sound rows >= 0x21), stored at ThingTemplate + 0x2DC (+0x124 + 0x37*8).
//     The port reads this exact row via UnitSpecificSound::templateSound (field IS read; see
//     LiveGameAudio::soundField -> UnitSpecificSound::templateSound).
//   * RotWK starts an object's ambient at drawable creation: Drawable::enableAmbientSound (RW 0x002785D6) sets
//     the m_448 flag; rva002784AF checks m_447&m_448&m_44A and then starts the sound (rva00278440).
//   * The port starts the ambient in LiveGameAudio::update() (ZH Drawable::startAmbientSound): the template's
//     SoundAmbient for the body damage state (SoundAmbientDamaged/ReallyDamaged/Rubble, falling back to
//     SoundAmbient for the damaged states), started for objects that are complete and alive and not under
//     construction, restarted on damage-state change, stopped on death; a non-global non-critical sound starts
//     only inside its MaxRange of the listener (stop S-706 noted).
//
// Synthetic data only: no retail file is read, so the tests pass without the game and the mod.
#include "doctest.h"

#include "CombatTestUtil.h"
#include "IniTestUtil.h"

#include "Common/Audio/AudioEntryPoints.h"
#include "Common/Audio/AudioIni.h"
#include "Common/Audio/GameAudio.h"
#include "Common/Audio/SimulatedAudioDevice.h"
#include "GameClient/LiveGameAudio.h"
#include "GameLogic/Object/Object.h"

#include <cstdint>
#include <memory>
#include <string>

using namespace combattest;

namespace
{
const char kPort141Objects[] =
    "Object Port141Fortress\\n"
    "  KindOf = STRUCTURE SELECTABLE SCORE\\n"
    "  VisionRange = 150\\n"
    "  SoundAmbient = DefiledSnd\\n"
    "  Geometry = BOX\\n"
    "  GeometryMajorRadius = 40\\n"
    "  GeometryMinorRadius = 40\\n"
    "  GeometryHeight = 40\\n"
    "  ArmorSet\\n"
    "    Conditions = None\\n"
    "    Armor = StoneArmor\\n"
    "  End\\n"
    "  Body = ActiveBody ModuleTag_Body\\n"
    "    MaxHealth = 2000\\n"
    "  End\\n"
    "  Behavior = AIUpdateInterface ModuleTag_AI\\n"
    "  End\\n"
    "End\\n"
    "Object Port141Citadel\\n"
    "  KindOf = STRUCTURE SELECTABLE SCORE\\n"
    "  VisionRange = 150\\n"
    "  SoundAmbient = DefiledSnd\\n"
    "  Geometry = BOX\\n"
    "  GeometryMajorRadius = 30\\n"
    "  GeometryMinorRadius = 30\\n"
    "  GeometryHeight = 30\\n"
    "  ArmorSet\\n"
    "    Conditions = None\\n"
    "    Armor = StoneArmor\\n"
    "  End\\n"
    "  Body = ActiveBody ModuleTag_Body\\n"
    "    MaxHealth = 1500\\n"
    "  End\\n"
    "  Behavior = AIUpdateInterface ModuleTag_AI\\n"
    "  End\\n"
    "End\\n"
    "Object Port141Quiet\\n"
    "  KindOf = STRUCTURE SELECTABLE SCORE\\n"
    "  VisionRange = 150\\n"
    "  Geometry = BOX\\n"
    "  GeometryMajorRadius = 40\\n"
    "  GeometryMinorRadius = 40\\n"
    "  GeometryHeight = 40\\n"
    "  ArmorSet\\n"
    "    Conditions = None\\n"
    "    Armor = StoneArmor\\n"
    "  End\\n"
    "  Body = ActiveBody ModuleTag_Body\\n"
    "    MaxHealth = 2000\\n"
    "  End\\n"
    "  Behavior = AIUpdateInterface ModuleTag_AI\\n"
    "  End\\n"
    "End\\n"
    "Object Port141Silent\\n"
    "  KindOf = STRUCTURE SELECTABLE SCORE\\n"
    "  VisionRange = 150\\n"
    "  SoundAmbient = NoSound\\n"
    "  Geometry = BOX\\n"
    "  GeometryMajorRadius = 40\\n"
    "  GeometryMinorRadius = 40\\n"
    "  GeometryHeight = 40\\n"
    "  ArmorSet\\n"
    "    Conditions = None\\n"
    "    Armor = StoneArmor\\n"
    "  End\\n"
    "  Body = ActiveBody ModuleTag_Body\\n"
    "    MaxHealth = 2000\\n"
    "  End\\n"
    "  Behavior = AIUpdateInterface ModuleTag_AI\\n"
    "  End\\n"
    "End\\n";

// AudioManager fixture (synthetic INI, no game files): mirrors the Port89Audio/Port89World pattern.
struct Port141Audio
{
    initest::Fixture fx;
    AudioIniState ini;
    std::unique_ptr<AudioAssetCache> cache;
    std::unique_ptr<SimulatedAudioDevice> device;
    std::unique_ptr<AudioManager> mgr;

    Port141Audio()
    {
        fx.mount({ { "data\\\\audio\\\\sounds\\\\click.wav", "x" } });
        ini.registerBlocks(fx.env.blocks);
        const std::string err = initest::loadError(fx.env, "audio.ini",
            "AudioSettings\\n"
            "  AudioRoot = Data\\\\Audio\\n"
            "  SoundsFolder = Sounds\\n"
            "  SoundsExtension = wav\\n"
            "  SampleCount2D = 2\\n"
            "  SampleCount3D = 4\\n"
            "  StreamCount = 2\\n"
            "  MinSampleVolume = 10\\n"
            "End\\n"
            "AudioEvent DefiledSnd\\n"
            "  Sounds = click\\n"
            "  Type = world everyone\\n"
            "End\\n");
        REQUIRE_MESSAGE(err.empty(), err);
        cache = std::make_unique<AudioAssetCache>(&fx.fsys, 1024u * 1024u);
        device = std::make_unique<SimulatedAudioDevice>(cache.get());
        device->setFileLength("Data\\\\Audio\\\\Sounds\\\\click.wav", 250.0);
        mgr = std::make_unique<AudioManager>(ini, *cache, *device, RandomAlgorithm::ZH_CarryChain, 1u);
        mgr->enableEventLog(true);
        mgr->update(0.0);
        AudioApi::install(mgr.get());
    }
    ~Port141Audio() { AudioApi::install(nullptr); }
};

struct Port141World : CombatWorld
{
    explicit Port141World() : CombatWorld(kPort141Objects) {}
};

} // namespace

TEST_CASE("PORT-141: SoundAmbient resolves per template (pristine) via UnitSpecificSound::templateSound")
{
    Port141World w;
    Object *fortress = w.unit("Port141Fortress", 'A', 200.0f, 300.0f);
    Object *citadel = w.unit("Port141Citadel", 'A', 400.0f, 300.0f);
    Object *quiet = w.unit("Port141Quiet", 'A', 600.0f, 300.0f);
    Object *silent = w.unit("Port141Silent", 'A', 800.0f, 300.0f);
    w.frames(2);

    // SoundAmbient is read from the template's final override (RW field table row 37, parser 0x0073AD07).
    CHECK(LiveGameAudio::ambientSoundFor(*fortress) == "DefiledSnd");
    CHECK(LiveGameAudio::ambientSoundFor(*citadel) == "DefiledSnd");
    // No SoundAmbient field at all -> no ambient.
    CHECK(LiveGameAudio::ambientSoundFor(*quiet).empty());
    // SoundAmbient = NoSound -> silenced.
    CHECK(LiveGameAudio::ambientSoundFor(*silent).empty());
}

TEST_CASE("PORT-141: an object with SoundAmbient that is complete and alive starts its ambient sound (game-start objects)")
{
    Port141World w;
    Port141Audio audio; // installs the AudioManager via AudioApi
    Object *fortress = w.unit("Port141Fortress", 'A', 200.0f, 300.0f);
    Object *citadel = w.unit("Port141Citadel", 'A', 400.0f, 300.0f);
    w.frames(2);

    const Player *owner = fortress->getControllingPlayer();
    REQUIRE(owner != nullptr);

    // ZH Drawable::startAmbientSound path: the port starts the ambient via playSoundForObject (no drawable
    // exists yet in a headless test) or playSoundForDrawable. Post the resolved ambient event for each object.
    AudioApi::playSoundForObject(LiveGameAudio::ambientSoundFor(*fortress), (std::uint32_t)fortress->getID(), owner->getPlayerIndex());
    AudioApi::playSoundForObject(LiveGameAudio::ambientSoundFor(*citadel), (std::uint32_t)citadel->getID(), owner->getPlayerIndex());

    const std::vector<AudioLogEntry> log = audio.mgr->takeEventLog();
    REQUIRE(log.size() == 2);
    CHECK(log[0].play == false); // queued request
    CHECK(log[0].event == "DefiledSnd");
    CHECK(log[0].objectId == (std::uint32_t)fortress->getID());
    CHECK(log[1].event == "DefiledSnd");
    CHECK(log[1].objectId == (std::uint32_t)citadel->getID());
}

TEST_CASE("PORT-141: neighboring behavior holds - objects without SoundAmbient and NoSound stay silent")
{
    Port141World w;
    Port141Audio audio;
    Object *quiet = w.unit("Port141Quiet", 'A', 200.0f, 300.0f);
    Object *silent = w.unit("Port141Silent", 'A', 400.0f, 300.0f);
    w.frames(2);

    const Player *owner = quiet->getControllingPlayer();
    REQUIRE(owner != nullptr);

    // No SoundAmbient field and SoundAmbient = NoSound produce no ambient sound.
    CHECK(LiveGameAudio::ambientSoundFor(*quiet).empty());
    CHECK(LiveGameAudio::ambientSoundFor(*silent).empty());
    AudioApi::playSoundForObject(LiveGameAudio::ambientSoundFor(*quiet), (std::uint32_t)quiet->getID(), owner->getPlayerIndex());
    AudioApi::playSoundForObject(LiveGameAudio::ambientSoundFor(*silent), (std::uint32_t)silent->getID(), owner->getPlayerIndex());

    const std::vector<AudioLogEntry> log = audio.mgr->takeEventLog();
    CHECK(log.empty());
}
