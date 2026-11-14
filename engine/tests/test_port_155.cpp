// OpenBFME. GPL-3.0.
// PORT-155 (issue #373): the port applies RotWK's reverb send, dry level and offscreen occlusion from the
// AudioEventInfo fields ReverbEffectLevel, DryLevel and ZoomedInOffscreenOcclusionPercent when playing sounds.
//
// RW facts verified in workspace/RotWK/game.dat (caveat S-001):
//   * Reader fn RW 0x7767A9 reads the sound event info ([ecx+4] at RW 0x7767BF) and, at the audio call RW 0x5DD9EE,
//     pushes [edi+0xA4] (RW 0x776AA5) = ZoomedInOffscreenOcclusionPercent, [edi+0xA8] (RW 0x776B1A) =
//     ReverbEffectLevel and [edi+0xAC] (RW 0x7769D1) = DryLevel.
//   * The field table rows T at RW 0xBEFF40/0xBEFF50/0xBEFF60, parser P RW 0x42EEFA (parsePercentToReal),
//     storage offsets +0xA4/+0xA8/+0xAC match AudioEventInfo exactly (verified by direct PE table inspection).
//
// Synthetic data only: no retail file is read, so the tests pass without the game and the mod.
#include "doctest.h"

#include "IniTestUtil.h"

#include "Common/Audio/AudioEntryPoints.h"
#include "Common/Audio/AudioIni.h"
#include "Common/Audio/GameAudio.h"
#include "Common/Audio/SimulatedAudioDevice.h"

#include <cstdint>
#include <memory>
#include <string>

namespace
{
// AudioManager fixture (synthetic INI, no game files): mirrors the Port141Audio / test_audio_manager Rig pattern.
struct Port155Audio
{
    initest::Fixture fx;
    AudioIniState ini;
    std::unique_ptr<AudioAssetCache> cache;
    std::unique_ptr<SimulatedAudioDevice> device;
    std::unique_ptr<AudioManager> mgr;

    Port155Audio()
    {
        fx.mount({ { "data\\audio\\sounds\\click.wav", "x" } });
        ini.registerBlocks(fx.env.blocks);
        const std::string err = initest::loadError(fx.env, "audio.ini",
            "AudioSettings\n"
            "  AudioRoot = Data\\Audio\n"
            "  SoundsFolder = Sounds\n"
            "  SoundsExtension = wav\n"
            "  SampleCount2D = 2\n"
            "  SampleCount3D = 4\n"
            "  StreamCount = 2\n"
            "  MinSampleVolume = 10\n"
            "End\n"
            // DryLevel = 50%, ReverbEffectLevel = 75%, ZoomedInOffscreenOcclusionPercent = 30%
            "AudioEvent Port155Full\n"
            "  Sounds = click\n"
            "  Type = ui everyone\n"
            "  DryLevel = 50\n"
            "  ReverbEffectLevel = 75\n"
            "  ZoomedInOffscreenOcclusionPercent = 30\n"
            "End\n"
            // no per-event fields: the AudioEventInfo defaults apply (DryLevel 100%, ReverbEffectLevel 100%, occlusion 0%)
            "AudioEvent Port155Defaults\n"
            "  Sounds = click\n"
            "  Type = ui everyone\n"
            "End\n"
            // occlusion is applied only while zoomed in and offscreen
            "AudioEvent Port155Occl\n"
            "  Sounds = click\n"
            "  Type = world everyone\n"
            "  ZoomedInOffscreenOcclusionPercent = 30\n"
            "End\n");
        REQUIRE_MESSAGE(err.empty(), err);
        cache = std::make_unique<AudioAssetCache>(&fx.fsys, 1024u * 1024u);
        device = std::make_unique<SimulatedAudioDevice>(cache.get());
        device->setFileLength("Data\\Audio\\Sounds\\click.wav", 250.0);
        mgr = std::make_unique<AudioManager>(ini, *cache, *device, RandomAlgorithm::ZH_CarryChain, 1u);
        mgr->setListenerPosition(Coord3D{ 0.0f, 0.0f, 0.0f }, Coord3D{ 0.0f, 1.0f, 0.0f });
        mgr->update(0.0);
        AudioApi::install(mgr.get());
    }
    ~Port155Audio() { AudioApi::install(nullptr); }

    double t = 0.0;
    // as test_audio_manager.cpp Rig::run: the device clock moves with the manager in 10 ms steps (a queued RQ_Play starts on a step)
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

    // plays one 2D sound and returns the params of the voice the manager started
    const VoiceParams &play2d(const std::string &event)
    {
        const AudioHandle h = mgr->playSound(event);
        // tolerance: the manager allocates the handle; only that it is a real handle is stable
        REQUIRE(h >= AHSV_FirstHandle);
        run(100.0); // addAudioEvent queues RQ_Play; the voice starts when the manager steps (click lasts 250 ms: still playing)
        REQUIRE(device->voices().size() == 1);
        return device->voices().begin()->second.params;
    }
};

TEST_CASE("PORT-155: DryLevel and ReverbEffectLevel are applied from the event info")
{
    Port155Audio a;
    const VoiceParams &p = a.play2d("Port155Full");
    CHECK(p.dryLevel == doctest::Approx(0.5f)); // tolerance: the parsed percent is an exact float; Approx documents the expected retail percent
    CHECK(p.reverbSend == doctest::Approx(0.75f)); // tolerance: the parsed percent is an exact float; Approx documents the expected retail percent
    // no world queries in the fixture: not zoomed in / offscreen, so no occlusion
    CHECK(p.occlusion == doctest::Approx(0.0f)); // tolerance: no queries installed, so occlusion is exactly zero
}

TEST_CASE("PORT-155: an event without the fields keeps the AudioEventInfo defaults")
{
    Port155Audio a;
    const VoiceParams &p = a.play2d("Port155Defaults");
    CHECK(p.dryLevel == doctest::Approx(1.0f)); // tolerance: the event info's default (INI default 100%) is an exact float
    CHECK(p.reverbSend == doctest::Approx(1.0f)); // tolerance: the event info's default (INI default 100%) is an exact float
    CHECK(p.occlusion == doctest::Approx(0.0f)); // tolerance: occlusion defaults to zero
}

TEST_CASE("PORT-155: offscreen occlusion is applied only while zoomed in and offscreen")
{
    Port155Audio a;
    AudioWorldQueries q;
    q.zoomFadeFactor = [] { return 1.0f; };                        // fully zoomed in
    q.isOnScreen = [](const Coord3D &) { return false; };          // the source is off screen
    q.offscreenDistanceSq = [](const Coord3D &) { return 100.0f; }; // past the max-effect edge -> full fade
    a.mgr->setWorldQueries(q);

    const AudioHandle h = a.mgr->playSoundAt("Port155Occl", Coord3D{ 100.0f, 0.0f, 0.0f });
    // tolerance: the manager allocates the handle; only that it is a real handle is stable
    REQUIRE(h >= AHSV_FirstHandle);
    a.run(100.0);
    REQUIRE(a.device->voices().size() == 1);
    const VoiceParams &p = a.device->voices().begin()->second.params;
    CHECK(p.occlusion == doctest::Approx(0.3f)); // tolerance: 30% * 1.0 (zoom) * 1.0 (max offscreen distance) = 0.3, an exact float
    CHECK(p.dryLevel == doctest::Approx(1.0f));   // tolerance: DryLevel not set on this event; the info default is an exact float
    CHECK(p.reverbSend == doctest::Approx(1.0f));  // tolerance: ReverbEffectLevel not set on this event; the info default is an exact float
}
} // namespace
