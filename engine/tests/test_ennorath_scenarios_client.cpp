// Ennorath client-side rendering scenarios (docs/ennorath/PROCESS.md, section 8; docs/ennorath/SUBSYSTEMS.md, section 7). GPL-3.0.
// Editions: both RotWK 2.01 and BFME2 1.06 (RW 0x964C00 / BFME2 0x00562DDB RenderObjectDrawModuleDoParticles.cpp,
// RW 0x5FBBC4 / BFME2 0x005FC49D RenderParticleConstructor.cpp).
// Expected values come from Ennorath 1.9.8 INI files (data\ini\fxparticlesystem.ini, data\ini\object\goodfaction\goodfactionprops.ini).

#include "EnnorathScenario.h"

#include "Common/GameEdition.h"
#include "GameClient/FXPlayback.h"
#include "GameClient/LiveFX.h"
#include "GameClient/ParticleDraw.h"
#include "Libraries/WWVegas/WW3D2/assetmgr.h"
#include "Common/NameKeyGenerator.h"
#include "GameClient/LiveGameAudio.h"
#include "GameLogic/Module/SpecialAbilityModules.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "HudAudioTestUtil.h"
#include "Common/SpecialPower.h"

#include <chrono>
#include <iostream>
#include <cmath>

using ennorathscenario::World;
using namespace FXParticleSystem;

TEST_CASE("ENNORATH scenario RenderObjectDraw: CloudBreakSunbeam attaches CloudBreakRays and builds W3D drawable instances for EXCldBrkRays in both editions (P-18, RW 0x964C00 / BFME2 0x00562DDB)")
{
	World world;
	if (!world.load("Ennorath scenario RenderObjectDraw (CloudBreakRays / CloudBreakSunbeam)"))
	{
		return;
	}

	// Verify that EXCldBrkRays.w3d resolves and loads in the mounted Ennorath + retail W3D archive
	ArchiveW3DFileSource w3dSource(*world.merged.fs);
	WW3DAssetManager assets(w3dSource);
	std::string w3dErr;
	const RenderObjPrototype *rayProto = assets.Create_Render_Obj("EXCldBrkRays", &w3dErr);
	REQUIRE_MESSAGE(rayProto != nullptr, w3dErr);

	for (GameEdition edition : { GameEdition::RotWK201, GameEdition::BFME2_106 })
	{
		CAPTURE(gameEditionName(edition));
		hudtest::Rig game(world.shared);
		game.logic().setEdition(edition);
		game.game->shroud().revealMapForPlayerPermanently(game.index());

		FXPlayback playback(*world.merged.fs, RandomAlgorithm::RotWK_GameDat_LCG);
		const std::vector<std::string> fxErrors = playback.loadRetailData(world.shared.world->report().macrosBeforeFX);
		// Verify CloudBreakRays template from data\ini\fxparticlesystem.ini:
		//   Shader = W3D_EMISSIVE, Type = DRAWABLE, ParticleName = EXCldBrkRays.W3D,
		//   Lifetime = 300 300, SystemLifetime = 200, BurstDelay = 20 90, BurstCount = 0 2,
		//   Color1 = R:194 G:151 B:117 0, Color2 = R:0 G:0 B:0 300,
		//   Alpha2 = 0.05 0.08 50, Alpha3 = 0 0 300,
		//   Update = RenderObjectUpdate (StartSizeX = 0.3, StartSizeY = 0.3, StartSizeZ = 1, Rotation = ROTATE_Z),
		//   Draw = RenderObjectDraw
		const ParticleSystemTemplate *tmpl = playback.particleTemplates().findTemplate("CloudBreakRays");
		REQUIRE(tmpl != nullptr);
		CHECK(tmpl->info().m_particleType == PARTICLE_TYPE_DRAWABLE);
		CHECK(tmpl->info().m_shaderType == PARTICLE_SHADER_W3D_EMISSIVE);
		CHECK(tmpl->info().m_particleTypeName == "EXCldBrkRays.W3D");
		REQUIRE(tmpl->module(MODULE_CATEGORY_DRAW) != nullptr);
		CHECK(tmpl->module(MODULE_CATEGORY_DRAW)->classId == MODULE_RENDER_OBJECT_DRAW);

		LiveFX liveFX(*game.game, playback);
		playback.seedClientRandom(42);
		
		// Spawn CloudBreakSunbeam (data\ini\object\goodfaction\goodfactionprops.ini: Model = None, ParticleSysBone = None CloudBreakRays)
		std::string createErr;
		Object *sunbeam = game.game->createObject("CloudBreakSunbeam", game.index(), Coord3D{ 500.0f, 600.0f, 0.0f }, 0.0f, &createErr);
		REQUIRE_MESSAGE(sunbeam != nullptr, createErr);

		// Step until CloudBreakRays emits and ramps alpha > 0 (Alpha1 = 0 at t=0, Alpha2 = 0.05..0.08 at t=50)
		for (int lf = 0; lf < 20; ++lf)
		{
			game.logic().runLogicFrame();
			game.game->refreshClient(1000.0 / 30.0, 1.0);
			liveFX.flushPending();
			liveFX.updateAttachedSystems();
			for (int c = 0; c < 6; ++c)
			{
				playback.step();
			}
		}

		W3DClientRandom renderRng(RandomAlgorithm::RotWK_GameDat_LCG);
		renderRng.seed(1);
		ParticleDrawBuilder builder(renderRng);
		const std::vector<DrawBatch> batches = builder.build(playback.particles(), DrawCamera());

		// S-195 must NOT be reported for RenderObjectDraw
		bool hasS195 = false;
		for (const std::string &u : builder.unverified())
		{
			if (u.substr(0, 5) == "S-195") hasS195 = true;
		}
		CHECK(hasS195 == false);

		const DrawBatch *rayBatch = nullptr;
		for (const DrawBatch &b : batches)
		{
			if (b.templateName == "CloudBreakRays")
			{
				rayBatch = &b;
				break;
			}
		}
		REQUIRE(rayBatch != nullptr);
		CHECK(rayBatch->kind == DrawBatch::DRAWABLES);
		CHECK(rayBatch->modelName == "EXCldBrkRays"); // stripped ".W3D" (RW 0x964A1C / BFME2 0x00562BF8)
		CHECK(rayBatch->shader == PARTICLE_SHADER_W3D_EMISSIVE);
		REQUIRE(!rayBatch->drawables.empty());

		// Verify each drawable instance matches its underlying particle's transform, W3D_EMISSIVE premultiplied color, and opacity
		const ParticleSystem *sys = playback.particles().findParticleSystemByID(rayBatch->system);
		REQUIRE(sys != nullptr);
		int pIdx = sys->firstParticle();
		for (const DrawableInstance &inst : rayBatch->drawables)
		{
			while (pIdx != -1 && playback.particles().particle(pIdx).particleID != inst.particleId)
			{
				pIdx = playback.particles().particle(pIdx).sysNext;
			}
			REQUIRE(pIdx != -1);
			const Particle &p = playback.particles().particle(pIdx);
			CHECK(inst.a == 1.0f);
			// W3D_EMISSIVE: SetEmissive(color * alpha) (RW 0x50E040 / BFME2 0x0050E643)
			CHECK(inst.r == p.color.color[0] * p.alpha.alpha);
			CHECK(inst.g == p.color.color[1] * p.alpha.alpha);
			CHECK(inst.b == p.color.color[2] * p.alpha.alpha);
			CHECK(inst.transform.Row[0][3] == p.pos.x);
			CHECK(inst.transform.Row[1][3] == p.pos.y);
			CHECK(inst.transform.Row[2][3] == p.pos.z);
			const float sx = std::max(p.update.size[0], 0.0001f);
			const float sy = std::max(p.update.size[1], 0.0001f);
			const float sz = std::max(p.update.size[2], 0.0001f);
			const float c = std::cos(p.update.angleZ);
			const float s = std::sin(p.update.angleZ);
			CHECK(inst.transform.Row[0][0] == sx * c);
			CHECK(inst.transform.Row[0][1] == sy * s);
			CHECK(inst.transform.Row[1][0] == -sx * s);
			CHECK(inst.transform.Row[1][1] == sy * c);
			CHECK(inst.transform.Row[2][2] == sz);
		}

		// Benchmark 225 CloudBreakRays systems (one full cast of SpellBookCloudBreak creates 15x15 = 225 CloudBreakSunbeam objects)
		for (int i = 1; i < 225; ++i)
		{
			const float x = 500.0f + (float)(i % 15) * 40.0f;
			const float y = 600.0f + (float)(i / 15) * 40.0f;
			playback.playParticleSystem("CloudBreakRays", Coord3D{ x, y, 0.0f });
		}
		for (int s = 0; s < 60; ++s)
		{
			playback.step();
		}
		const auto t0 = std::chrono::steady_clock::now();
		const std::vector<DrawBatch> castBatches = builder.build(playback.particles(), DrawCamera());
		const double buildMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
		size_t totalDrawables = 0;
		for (const DrawBatch &b : castBatches)
		{
			if (b.kind == DrawBatch::DRAWABLES)
			{
				totalDrawables += b.drawables.size();
			}
		}
		INFO("225 CloudBreakRays systems: " << totalDrawables << " drawable particles built in " << buildMs << " ms");
		CHECK(totalDrawables > 100u); // tolerance: stochastic particle emission across 225 systems
		CHECK(buildMs < 15.0); // tolerance: wall-clock frame build timing budget (< 15 ms for 225 systems)
	}
}

struct ExposedSpecialAbilityUpdate : public SpecialAbilityUpdate
{
	using SpecialAbilityUpdate::triggerAbilityEffect;
};

TEST_CASE("ENNORATH scenario SpecialPowerAudio: initiate sound, fortress trigger sound, Draft delayed horn and Theoden tax treasure pickup player filter (P-23, P-24, RW 0x897183 / BFME2 0x008939AB, RW 0x853FC0 / BFME2 0x85116E, RW 0x4533A9 / BFME2 0x004533A9)")
{
	World world;
	if (!world.load("Ennorath scenario SpecialPowerAudio (P-23, P-24, Eomer, Hornburg)"))
	{
		return;
	}

	for (GameEdition edition : { GameEdition::RotWK201, GameEdition::BFME2_106 })
	{
		CAPTURE(gameEditionName(edition));
		hudtest::Rig game(world.shared);
		game.logic().setEdition(edition);
		game.game->shroud().revealMapForPlayerPermanently(game.index());

		hudaudio::RetailAudio audio(*world.merged.fs);
		audio.manager->enableEventLog(true);

		std::string liveErr;
		std::unique_ptr<LiveGameAudio> liveAudio = LiveGameAudio::attachToInstalled(*game.game, nullptr, &liveErr, nullptr);
		REQUIRE_MESSAGE(liveAudio != nullptr, liveErr);
		liveAudio->enableEventLog(true);

		FXPlayback playback(*world.merged.fs, RandomAlgorithm::RotWK_GameDat_LCG);
		playback.loadRetailData(world.shared.world->report().macrosBeforeFX);
		LiveFX liveFX(*game.game, playback);
		playback.seedClientRandom(42);
		audio.manager->updateMicrophone(Coord3D{ 500.0f, 500.0f, 100.0f }, Coord3D{ 500.0f, 500.0f, 0.0f });

		double audioTimeMs = 1000.0;
		auto stepAudio = [&](double dtMs) {
			audioTimeMs += dtMs;
			audio.device->setTime(audioTimeMs);
			audio.manager->update(audioTimeMs);
		};
		stepAudio(0.0); // start clock at initial audioTimeMs

		// 1. Eomer (InitiateSound on SpecialPowerModule: RW 0x897183 / BFME2 0x008939AB)
		std::string createErr;
		Object *eomer = game.game->createObject("RohanEomer", game.index(), Coord3D{ 500.0f, 500.0f, 0.0f }, 0.0f, &createErr);
		REQUIRE_MESSAGE(eomer != nullptr, createErr);
		NameKeyType smiteKey = world.shared.world->nameKeys().nameToKey("ModuleTag_EowynSmiteStarter");
		BehaviorModule *smiteMod = eomer->findModuleByTag(smiteKey);
		REQUIRE(smiteMod != nullptr);
		SpecialPowerModule *spm = dynamic_cast<SpecialPowerModule *>(smiteMod);
		REQUIRE(spm != nullptr);
		REQUIRE(spm->spData() != nullptr);
		CHECK(spm->spData()->m_initiateSound == "EomerVoiceAttackSpear");

		spm->triggerFromUpdate();
		stepAudio(100.0 / 3.0);

		bool eomerFound = false;
		for (const AudioLogEntry &e : audio.manager->takeEventLog())
		{
			if (e.event == "EomerVoiceAttackSpear")
			{
				eomerFound = true;

				CHECK(e.outcome.find("refused") == std::string::npos);
			}
		}
		CHECK(eomerFound == true);


		bool eomerPlayed = false;
		for (const auto &devEntry : audio.device->log())
		{
			if (devEntry.kind == SimulatedAudioDevice::LogEntry::Start &&
				(devEntry.file.find("GUEomer_voiatcc") != std::string::npos ||
				 devEntry.file.find("GUEomer_voisebf") != std::string::npos))
			{
				eomerPlayed = true;
				break;
			}
		}
		CHECK(eomerPlayed == true);

		// 2. Hornburg (TriggerSound on ModelConditionSpecialAbilityUpdate: RW 0x853FC0 / BFME2 0x85116E)
		Object *tower = game.game->createObject("HelmsDeepTowerEdain", game.index(), Coord3D{ 600.0f, 600.0f, 0.0f }, 0.0f, &createErr);
		REQUIRE_MESSAGE(tower != nullptr, createErr);
		NameKeyType hornKey = world.shared.world->nameKeys().nameToKey("ModuleTag_HelmsHornUpdate");
		BehaviorModule *hornMod = tower->findModuleByTag(hornKey);
		REQUIRE(hornMod != nullptr);
		SpecialAbilityUpdate *sau = dynamic_cast<SpecialAbilityUpdate *>(hornMod);
		REQUIRE(sau != nullptr);

		static_cast<ExposedSpecialAbilityUpdate *>(sau)->triggerAbilityEffect();
		stepAudio(100.0 / 3.0);

		bool hornFound = false;
		for (const AudioLogEntry &e : audio.manager->takeEventLog())
		{
			if (e.event == "HelmsHornFortress")
			{
				hornFound = true;
				CHECK(e.outcome.find("refused") == std::string::npos);
			}
		}
		CHECK(hornFound == true);

		bool hornPlayed = false;
		for (const auto &devEntry : audio.device->log())
		{
			if (devEntry.kind == SimulatedAudioDevice::LogEntry::Start &&
				devEntry.file.find("HornofHelmHammerhand") != std::string::npos)
			{
				hornPlayed = true;
				break;
			}
		}
		CHECK(hornPlayed == true);

		// 3. Draft (P-23: InitiateAtLocationSound with 1000ms delay and player filter: RW 0x897183 / BFME2 0x008939AB)
		const SpecialPowerTemplate *draftTmpl = world.shared.world->spellStores().specialPowers().findSpecialPowerTemplate("SpellBookDraft");
		REQUIRE(draftTmpl != nullptr);
		CHECK(draftTmpl->m_initiateAtLocationSound == "SpellBauernHerrschauStereo");

		const Coord3D draftPos{ 500.0f, 500.0f, 0.0f };
		AudioApi::playSoundAtPositionForPlayer(draftTmpl->m_initiateAtLocationSound, draftPos, game.index());

		bool draftQueued = false;
		for (const AudioLogEntry &e : audio.manager->takeEventLog())
		{
			if (e.event == "SpellBauernHerrschauStereo" || e.event == "SpellBauernHerrschauHorn")
			{
				draftQueued = true;
				CHECK(e.outcome.find("refused") == std::string::npos);
			}
		}
		CHECK(draftQueued == true);

		// Advance audio by >= 1000 ms to trigger delayed horn
		for (int s = 0; s < 35; ++s)
		{
			stepAudio(100.0 / 3.0);
		}

		bool draftPlayed = false;
		for (const auto &devEntry : audio.device->log())
		{
			if (devEntry.kind == SimulatedAudioDevice::LogEntry::Start &&
				devEntry.file.find("URohan_horn01") != std::string::npos)
			{
				draftPlayed = true;
				break;
			}
		}
		CHECK(draftPlayed == true);

		// 4. Theoden's Tax (P-24: TreasurePickup on TheodenAusbeutesteuerModifier with player filter: RW 0x4533A9 / BFME2 0x004533A9)
		Object *taxMod = game.game->createObject("TheodenAusbeutesteuerModifier", game.index(), Coord3D{ 500.0f, 500.0f, 0.0f }, 0.0f, &createErr);
		REQUIRE_MESSAGE(taxMod != nullptr, createErr);

		game.logic().runLogicFrame();
		game.game->refreshClient(1000.0 / 30.0, 1.0);
		liveFX.flushPending();
		stepAudio(100.0 / 3.0);

		bool taxFound = false;
		for (const AudioLogEntry &e : audio.manager->takeEventLog())
		{
			if (e.event == "TreasurePickup")
			{
				taxFound = true;
				CHECK(e.outcome.find("refused") == std::string::npos);
			}
		}
		CHECK(taxFound == true);

		bool taxPlayed = false;
		for (const auto &devEntry : audio.device->log())
		{
			if (devEntry.kind == SimulatedAudioDevice::LogEntry::Start &&
				(devEntry.file.find("UTreasurea") != std::string::npos ||
				 devEntry.file.find("UTreasureb") != std::string::npos ||
				 devEntry.file.find("UTreasurec") != std::string::npos))
			{
				taxPlayed = true;
				break;
			}
		}
		CHECK(taxPlayed == true);
	}
}
