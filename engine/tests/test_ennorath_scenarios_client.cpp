// Ennorath client-side rendering scenarios (docs/ennorath/PROCESS.md, section 8; docs/ennorath/SUBSYSTEMS.md, section 7). GPL-3.0.
// Editions: both RotWK 2.01 and BFME2 1.06 (RW 0x964C00 / BFME2 0x00562DDB RenderObjectDrawModuleDoParticles.cpp,
// RW 0x5FBBC4 / BFME2 0x005FC49D RenderParticleConstructor.cpp).
// Expected values come from Ennorath 1.9.8 INI files (data\ini\fxparticlesystem.ini, data\ini\object\goodfaction\goodfactionprops.ini).

#include "EnnorathScenario.h"

#include "Common/GameEdition.h"
#include "GameClient/DynamicDecalMesh.h"
#include "GameClient/FXPlayback.h"
#include "GameClient/LiveFX.h"
#include "GameClient/ParticleDraw.h"
#include "GameClient/ParticleTexture.h"
#include "Libraries/WWVegas/WW3D2/assetmgr.h"
#include "Common/Audio/AudioEntryPoints.h"
#include "Common/NameKeyGenerator.h"
#include "GameClient/LiveGameAudio.h"
#include "GameClient/UnitVoiceResponse.h"
#include "GameLogic/Module/SpecialAbilityModules.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "HudAudioTestUtil.h"
#include "Common/SpecialPower.h"
#include "GameClient/Drawable.h"
#include "GameClient/DrawableManager.h"
#include "GameClient/CameraSettings.h"
#include "GameClient/DrawableIconUI.h"
#include "GameClient/GUI/Image.h"
#include "GameLogic/Module/BehaviorModule.h"

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

TEST_CASE("ENNORATH scenario SpecialPowerAudio: initiate sound, fortress trigger sound, Draft delayed horn and Theoden tax treasure pickup player filter (P-23, P-24, RW 0x897183, RW 0x8DF819, RW 0x853FC0 / BFME2 0x85116E, RW 0x4533A9 / BFME2 0x004533A9)")
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

		// 1. Eomer (InitiateSound on SpecialPowerModule: RW 0x8DF819, RW 0x897183)
		// PROCESS 14 expectation update: InitiateSound on SpecialPowerModule sounds through unit voice (RW 0x8DF819)
		// on special power order (MSG_DO_SPECIAL_POWER_AT_OBJECT), not logic triggerSpecialPower (RW 0x897183).
		// Logic trigger plays only template InitiateSound (+0x34); exactly one sound per command (PROCESS 15.2).
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

		// Attach unit voice picker to audio manager (as InGameHud does)
		AudioApiVoiceSink voiceSink;
		UnitVoiceResponse voice(game.logic(), voiceSink);
		AudioApi::installUnitVoiceHandler([&](const AudioApi::UnitVoiceParams &p) {
			UnitVoiceResponse::Info info;
			info.producer = (ObjectID)p.producerId;
			info.powerTemplate = p.powerTemplate;
			info.target = (ObjectID)p.targetId;
			info.hasPosition = p.hasPosition;
			info.position = Coord3D{ p.posX, p.posY, p.posZ };
			voice.pickAndPlay({ (ObjectID)p.objectId }, p.voiceEvent, &info);
		}, &voice);

		// Order voice (RW 0x8DF819): plays EomerVoiceAttackSpear
		UnitVoiceResponse::Info orderInfo;
		orderInfo.powerTemplate = spm->getSpecialPowerTemplate();
		orderInfo.target = eomer->getID();
		CHECK(voice.pickAndPlay({ eomer->getID() }, MSG_DO_SPECIAL_POWER_AT_OBJECT, &orderInfo));

		// Logic trigger (RW 0x897183): template sound is empty, so no second sound is queued
		spm->triggerFromUpdate();
		stepAudio(100.0 / 3.0);

		// Exactly one sound on one click (PROCESS 15.2): 1 queued request and 1 started play line
		int eomerQueued = 0;
		int eomerStarted = 0;
		for (const AudioLogEntry &e : audio.manager->takeEventLog())
		{
			if (e.event == "EomerVoiceAttackSpear")
			{
				if (e.outcome == "queued")
				{
					++eomerQueued;
				}
				if (e.play)
				{
					++eomerStarted;
				}
				CHECK(e.outcome.find("refused") == std::string::npos);
			}
		}
		CHECK(eomerQueued == 1);
		CHECK(eomerStarted == 1);

		int deviceStarts = 0;
		for (const auto &devEntry : audio.device->log())
		{
			if (devEntry.kind == SimulatedAudioDevice::LogEntry::Start &&
				(devEntry.file.find("GUEomer_voiatcc") != std::string::npos ||
				 devEntry.file.find("GUEomer_voisebf") != std::string::npos))
			{
				++deviceStarts;
			}
		}
		CHECK(deviceStarts == 1);
		AudioApi::uninstallUnitVoiceHandler(&voice);

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

TEST_CASE("LOCAL-3: TintEnvelope phases, pulse modulation, and Drawable selection/tint evaluation (RW 0x674818 / 0x6748B7 / 0x671B92 / 0x46F4E7)")
{
	TintEnvelope env;
	CHECK(env.isResting() == true);
	CHECK(env.isEffective() == false);
	CHECK(env.state() == TintEnvelope::ENV_STATE_REST);
	CHECK(env.getColor()->red == 0.0f);

	// Attack = 2 frames, decay = 2 frames, sustain = 1 frame
	const RGBColor peak{ 1.0f, 0.5f, 0.25f };
	env.play(&peak, 2u, 2u, 1u);
	CHECK(env.isEffective() == true);
	CHECK(env.isResting() == false);
	CHECK(env.state() == TintEnvelope::ENV_STATE_ATTACK);
	CHECK(env.sustainCounter() == 1u);

	// Step 1: attack step -> currentColor moves halfway towards peak
	env.update();
	CHECK(env.state() == TintEnvelope::ENV_STATE_ATTACK);
	CHECK(env.getColor()->red == 0.5f);
	CHECK(env.getColor()->green == 0.25f);
	CHECK(env.getColor()->blue == 0.125f);

	// Step 2: attack step reaches peak
	env.update();
	CHECK(env.getColor()->red == 1.0f);
	CHECK(env.getColor()->green == 0.5f);
	CHECK(env.getColor()->blue == 0.25f);

	// Step 3: deltaLen <= 0.001f -> transitions to SUSTAIN
	env.update();
	CHECK(env.state() == TintEnvelope::ENV_STATE_SUSTAIN);
	CHECK(env.sustainCounter() == 1u);

	// Step 4: sustain counter decrements from 1 to 0
	env.update();
	CHECK(env.state() == TintEnvelope::ENV_STATE_SUSTAIN);
	CHECK(env.sustainCounter() == 0u);

	// Step 5: sustain counter was 0 -> transitions to DECAY
	env.update();
	CHECK(env.state() == TintEnvelope::ENV_STATE_DECAY);

	// Step 6: decay step -> moves halfway back to 0
	env.update();
	CHECK(env.getColor()->red == 0.5f);
	CHECK(env.getColor()->green == 0.25f);
	CHECK(env.getColor()->blue == 0.125f);

	// Step 7: decay step reaches 0
	env.update();
	CHECK(env.getColor()->red == 0.0f);
	CHECK(env.getColor()->green == 0.0f);
	CHECK(env.getColor()->blue == 0.0f);

	// Step 8: curLen <= 0.001f -> REST
	env.update();
	CHECK(env.state() == TintEnvelope::ENV_STATE_REST);
	CHECK(env.isEffective() == false);
	CHECK(env.isResting() == true);
	CHECK(env.getColor()->red == 0.0f);
	CHECK(env.getColor()->green == 0.0f);
	CHECK(env.getColor()->blue == 0.0f);

	// Test immediate peak transition (diff <= 0.001f):
	env.play(&peak, 1u, 1u, 3u);
	CHECK(env.state() == TintEnvelope::ENV_STATE_ATTACK);
	env.update(); // reaches peak
	env.update(); // transitions to sustain (sustain=3)
	CHECK(env.state() == TintEnvelope::ENV_STATE_SUSTAIN);
	CHECK(env.sustainCounter() == 3u);
	// Now currentColor == peak; calling play with peak again:
	env.play(&peak, 2u, 2u, 5u);
	// diff is <= 0.001f -> immediately transitions to SUSTAIN!
	CHECK(env.state() == TintEnvelope::ENV_STATE_SUSTAIN);
	CHECK(env.sustainCounter() == 5u);

	// Sine pulse modulation test (RW 0x671B92):
	// modulated = currentColor * (sin(logicFrame * freq) * amp * 0.5 + 0.5)
	env.setPulse(0.5f, 0.1f);
	// logicFrame = 0: sin(0) = 0 -> factor = 0.5
	const RGBColor *mod0 = env.getColor(0u);
	CHECK(mod0->red == 0.5f);
	CHECK(mod0->green == 0.25f);
	CHECK(mod0->blue == 0.125f);
}

TEST_CASE("LOCAL-3: Ennorath TintDrawable FXList drives drawable custom tint envelope in both editions (RW 0x5E0108 -> 0x671A73 -> 0x675E9A)")
{
	World world;
	if (!world.load("LOCAL-3 Ennorath scenario TintDrawable (FX_LurtzBloodRageLong / FX_AufhellLeadership)"))
	{
		return;
	}

	for (GameEdition edition : { GameEdition::RotWK201, GameEdition::BFME2_106 })
	{
		CAPTURE(gameEditionName(edition));
		hudtest::Rig game(world.shared);
		game.logic().setEdition(edition);
		game.game->shroud().revealMapForPlayerPermanently(game.index());

		FXPlayback playback(*world.merged.fs, RandomAlgorithm::RotWK_GameDat_LCG);
		playback.loadRetailData(world.shared.world->report().macrosBeforeFX);
		LiveFX liveFX(*game.game, playback);

		// Verify Ennorath FXLists exist:
		// FX_LurtzBloodRageLong: TintDrawable Color=R:180 G:0 B:0, Pre=10, Post=10, Sustained=900
		const FXList *bloodRage = playback.fxLists().findFXList("FX_LurtzBloodRageLong");
		REQUIRE(bloodRage != nullptr);

		// FX_AufhellLeadership: TintDrawable Color=R:255 G:255 B:255, Pre=0, Post=0, Sustained=30
		const FXList *aufhell = playback.fxLists().findFXList("FX_AufhellLeadership");
		REQUIRE(aufhell != nullptr);

		std::string createErr;
		Object *soldier = game.game->createObject("GondorFighter", game.index(), Coord3D{ 500.0f, 500.0f, 0.0f }, 0.0f, &createErr);
		REQUIRE_MESSAGE(soldier != nullptr, createErr);

		// Run logic and client refresh so client events create the drawable
		game.logic().runLogicFrame();
		game.game->refreshClient(1000.0 / 30.0, 1.0);

		Drawable *d = game.game->drawables().findByObject(soldier->getID());
		REQUIRE(d != nullptr);

		// Initially no tint or selection flash
		RGBColor renderTint{ -1.0f, -1.0f, -1.0f };
		CHECK(d->getTintColor() == nullptr);
		CHECK(d->getSelectionColor() == nullptr);
		CHECK(d->evaluateRenderTint(0u, renderTint) == false);

		// Test Drawable::flashAsSelected with explicit white (RW 0x8E395E capture building flash)
		const RGBColor white{ 1.0f, 1.0f, 1.0f };
		d->flashAsSelected(&white);
		CHECK(d->getSelectionColor() != nullptr);
		// Advance one client tick so updateTint() steps from initial (0,0,0) to peak (1,1,1)
		game.game->drawables().advance(1000.0 / 30.0);
		CHECK(d->evaluateRenderTint(0u, renderTint) == true);
		CHECK(renderTint.red == 1.0f);
		CHECK(renderTint.green == 1.0f);
		CHECK(renderTint.blue == 1.0f);

		// Step client: selection flash decays over 4 frames after sustain
		for (int f = 0; f < 10; ++f)
		{
			game.game->drawables().advance(1000.0 / 30.0);
		}
		CHECK(d->getSelectionColor() == nullptr);

		// Now fire Ennorath's FX_LurtzBloodRageLong on the object (RW 0x5E0108 -> 0x671A73)
		FXEvent ev;
		ev.kind = FXEvent::OBJECT_FX;
		ev.primary = soldier->getID();
		ev.position = *soldier->getPosition();
		const std::string fxName = "FX_LurtzBloodRageLong";
		ev.fxList = &fxName;
		liveFX.onFXEvent(ev);
		liveFX.flushPending();

		// Custom tint envelope was set on the drawable with status TINT_STATUS_CUSTOM (0x20)
		CHECK(d->testTintStatus(Drawable::TINT_STATUS_CUSTOM) == true);

		// Frame 1: updateTint() activates and plays the custom envelope
		game.game->drawables().advance(1000.0 / 30.0);
		// Frame 2: steps the attack phase (1/10th of peak)
		game.game->drawables().advance(1000.0 / 30.0);

		const RGBColor *tintColor = d->getTintColor();
		REQUIRE(tintColor != nullptr);
		// PreColorTime was 10 frames, so on step 1 of attack it is 1/10th of peak (180/255 = 0.70588)
		const float expectedPeakR = 180.0f / 255.0f;
		CHECK(tintColor->red == doctest::Approx(expectedPeakR * 0.1f)); // tolerance: float division in attack rate
		CHECK(tintColor->green == 0.0f);
		CHECK(tintColor->blue == 0.0f);

		CHECK(d->evaluateRenderTint(0u, renderTint) == true);
		CHECK(renderTint.red == tintColor->red);
	}
}

TEST_CASE("ENNORATH scenario LOCAL-4: Container occupant pips show for Ennorath siege engines and transports in both editions (TOP20 #17)")
{
	World world;
	if (!world.load("Ennorath scenario LOCAL-4 container occupant pips"))
	{
		return;
	}

	MappedImageCollection images;
	INIEnvironment imageEnv;
	imageEnv.fileSystem = world.merged.fs.get();
	images.registerBlocks(imageEnv.blocks);
	INI ini(imageEnv);
	ini.loadDirectory("Data\\INI\\MappedImages", true, INI_LOAD_OVERWRITE);

	for (GameEdition edition : { GameEdition::RotWK201, GameEdition::BFME2_106 })
	{
		CAPTURE(gameEditionName(edition));
		hudtest::Rig game(world.shared);
		game.logic().setEdition(edition);
		game.game->shroud().revealMapForPlayerPermanently(game.index());

		std::string createErr;
		// 1. IsengardBatteringRam: ShowPips = No in INI -> getContainerPipCounts returns false
		Object *ram = game.game->createObject("IsengardBatteringRam", game.index(), Coord3D{ 500.0f, 500.0f, 0.0f }, 0.0f, &createErr);
		REQUIRE_MESSAGE(ram != nullptr, createErr);

		ContainModuleInterface *ramContain = ram->getContain();
		REQUIRE(ramContain != nullptr);
		int ramMax = 0, ramFilled = 0, ramHero = 0;
		CHECK(ramContain->getContainerPipCounts(ramMax, ramFilled, ramHero) == false);

		// 2. EvilMenTransportShip: ShowPips = Yes in INI, Slots = 2 -> getContainerPipCounts returns true
		Object *ship = game.game->createObject("EvilMenTransportShip", game.index(), Coord3D{ 600.0f, 600.0f, 0.0f }, 0.0f, &createErr);
		REQUIRE_MESSAGE(ship != nullptr, createErr);
		game.game->advance(0.2);

		ContainModuleInterface *contain = ship->getContain();
		REQUIRE(contain != nullptr);
		int maxPips = 0, filledPips = 0, heroCount = 0;
		CHECK(contain->getContainerPipCounts(maxPips, filledPips, heroCount) == true);
		// In Ennorath mod data, transport ship has 5 slots (Vanilla has 2)
		CHECK(maxPips == 5);

		// Look at the ship so it is on screen
		const Coord3D &p = *ship->getPosition();
		game.view.set(Coord3D{ p.x, p.y - 300.0f, p.z + 400.0f }, p);

		// Test DrawableIconUI queueing
		DrawableIconUI iconUI;
		IconUISettings s;
		s.showObjectHealth = true;
		s.images = &images;
		s.zoom = 1.0f;

		// Unselected: 0 pips
		std::vector<IconUIOp> ops;
		iconUI.build(game.game->logic(), game.game->drawables(), game.view, s, {}, 0, ops);
		size_t pipOps = 0;
		for (const auto &op : ops)
		{
			if (op.object == ship->getID() && (op.image == "ContainPip" || op.image == "ContainPipFrame"))
			{
				++pipOps;
			}
		}
		CHECK(pipOps == 0);

		// Selected: queues ContainPip and ContainPipFrame ops for the slots (RW 0x677994)
		ops.clear();
		iconUI.build(game.game->logic(), game.game->drawables(), game.view, s, { ship->getID() }, 0, ops);
		size_t containPipCount = 0, frameCount = 0;
		for (const auto &op : ops)
		{
			if (op.object == ship->getID())
			{
				if (op.image == "ContainPip") ++containPipCount;
				if (op.image == "ContainPipFrame") ++frameCount;
			}
		}
		CHECK(containPipCount == 5); // 5 empty pips (0xFF7F7F7F)
		CHECK(frameCount == 5);      // 5 slot frames
	}
}

TEST_CASE("LOCAL-3: Shared 30 Hz client clock preserves envelope duration across 30 and 144 fps (RW 0x6487D6 / 0x675996)")
{
	World world;
	if (!world.load("LOCAL-3 Ennorath scenario TintDrawable (FX_LurtzBloodRageLong / FX_AufhellLeadership)"))
	{
		return;
	}

	hudtest::Rig game(world.shared);
	game.game->shroud().revealMapForPlayerPermanently(game.index());

	std::string createErr;
	Object *soldier = game.game->createObject("GondorFighter", game.index(), Coord3D{ 500.0f, 500.0f, 0.0f }, 0.0f, &createErr);
	REQUIRE_MESSAGE(soldier != nullptr, createErr);
	game.logic().runLogicFrame();
	game.game->refreshClient(1000.0 / 30.0, 1.0);

	Drawable *d = game.game->drawables().findByObject(soldier->getID());
	REQUIRE(d != nullptr);

	// Test 1: Lurtz Carnage sustained duration (600 client frames = 20.0 seconds at 30 fps)
	// At 144 fps, 10.0 seconds of simulated wall time is 1440 render frames.
	// Without the 30 Hz clock, 1440 render frames would deplete 600 sustain frames in ~4.1 seconds.
	// With the 30 Hz client clock, 10.0 seconds equals exactly 300 client frames, leaving sustainCounter at 300.
	const RGBColor carnageColor{ 1.0f, 0.0f, 0.0f };
	d->setCustomTintEnvelope(carnageColor, 0u, 10u, 600u, 0.0f, 0.0f);
	// Tick 1: updateTint() activates the envelope (status -> play)
	game.game->drawables().advance(1000.0 / 30.0);
	CHECK(d->colorTintEnvelope().isEffective() == true);
	// Tick 2: updateTint() steps attack phase to peak color (RW 0x674A87)
	game.game->drawables().advance(1000.0 / 30.0);
	// Tick 3: updateTint() transitions deltaLen==0 to SUSTAIN (RW 0x674A85 -> 0x674ABA)
	game.game->drawables().advance(1000.0 / 30.0);
	CHECK(d->colorTintEnvelope().state() == TintEnvelope::ENV_STATE_SUSTAIN);
	CHECK(d->colorTintEnvelope().sustainCounter() == 600u);

	// Advance 1440 frames of 1000.0 / 144.0 ms (exactly 10.0 seconds = 300 client ticks)
	const double dt144 = 1000.0 / 144.0;
	for (int f = 0; f < 1440; ++f)
	{
		game.game->drawables().advance(dt144);
	}

	// At 10.0 seconds, exactly 300 client ticks passed out of 600 sustain frames!
	CHECK(d->colorTintEnvelope().state() == TintEnvelope::ENV_STATE_SUSTAIN);
	CHECK(d->colorTintEnvelope().sustainCounter() == 300u);
	CHECK(d->colorTintEnvelope().isEffective() == true);

	// Advance another 10.0 seconds (1440 frames at 144 fps = another 300 client ticks)
	for (int f = 0; f < 1440; ++f)
	{
		game.game->drawables().advance(dt144);
	}

	// At 20.0 seconds, sustain reaches 0
	CHECK(d->colorTintEnvelope().sustainCounter() == 0u);

	// One more client tick transitions to DECAY
	game.game->drawables().advance(1000.0 / 30.0);
	CHECK(d->colorTintEnvelope().state() == TintEnvelope::ENV_STATE_DECAY);

	// Advance decay (10 client frames = 333.3 ms)
	for (int f = 0; f < 50; ++f)
	{
		game.game->drawables().advance(dt144);
	}
	CHECK(d->colorTintEnvelope().isResting() == true);
}

TEST_CASE("LOCAL-3: Selection flash wires through InGameUI with player house color for units, buildings, hordes (RW 0x678FA7 / 0x81DC79 / 0x81E0A7)")
{
	World world;
	if (!world.load("LOCAL-3 Ennorath scenario TintDrawable (FX_LurtzBloodRageLong / FX_AufhellLeadership)"))
	{
		return;
	}

	hudtest::Rig game(world.shared);
	game.game->shroud().revealMapForPlayerPermanently(game.index());

	// Set distinct player color (blue: 0x0000FF)
	const std::uint32_t houseColor = 0xFF0000FF;
	game.local->setPlayerColor(houseColor);

	std::string createErr;
	Object *soldier = game.game->createObject("GondorFighter", game.index(), Coord3D{ 500.0f, 500.0f, 0.0f }, 0.0f, &createErr);
	REQUIRE_MESSAGE(soldier != nullptr, createErr);

	Object *barracks = game.game->createObject("GondorBarracks", game.index(), Coord3D{ 600.0f, 500.0f, 0.0f }, 0.0f, &createErr);
	REQUIRE_MESSAGE(barracks != nullptr, createErr);

	Object *horde = game.game->createObject("GondorFighterHorde", game.index(), Coord3D{ 700.0f, 500.0f, 0.0f }, 0.0f, &createErr);
	REQUIRE_MESSAGE(horde != nullptr, createErr);

	game.logic().runLogicFrame();
	game.game->refreshClient(1000.0 / 30.0, 1.0);

	Drawable *dSoldier = game.game->drawables().findByObject(soldier->getID());
	REQUIRE(dSoldier != nullptr);
	Drawable *dBarracks = game.game->drawables().findByObject(barracks->getID());
	REQUIRE(dBarracks != nullptr);
	Drawable *dHorde = game.game->drawables().findByObject(horde->getID());
	REQUIRE(dHorde != nullptr);

	// Expected selection flash color for blue house color:
	// base = (0, 0, 1), sat = 0.6
	// red = 0 * 0.6 - 0.3 = -0.3
	// green = 0 * 0.6 - 0.3 = -0.3
	// blue = 1 * 0.6 - 0.3 = +0.3
	const float baseR = (float)((houseColor >> 16) & 0xFF) / 255.0f;
	const float baseG = (float)((houseColor >> 8) & 0xFF) / 255.0f;
	const float baseB = (float)(houseColor & 0xFF) / 255.0f;
	const float sat = 0.6f;
	const float expectedR = baseR * sat - sat * 0.5f;
	const float expectedG = baseG * sat - sat * 0.5f;
	const float expectedB = baseB * sat - sat * 0.5f;

	// 1. Single Unit Selection Flash via InGameUI::selectObject (RW 0x81E0A7)
	// In Ennorath EnableHouseColor = No: unit has no model house color, but indicator color (+0x454) is set
	CHECK(dSoldier->getHouseColor() == 0u);
	CHECK(dSoldier->indicatorColor() == houseColor);
	CHECK(dSoldier->selectionFlashEnvelope().isEffective() == false);
	game.input->ui().selectObject(soldier->getID());
	CHECK(dSoldier->selectionFlashEnvelope().isEffective() == true);

	// Step one client tick to reach peak
	game.game->drawables().advance(1000.0 / 30.0);
	const RGBColor *selColor = dSoldier->getSelectionColor();
	REQUIRE(selColor != nullptr);
	CHECK(selColor->red == doctest::Approx(expectedR)); // tolerance: float color multiplication and subtraction
	CHECK(selColor->green == doctest::Approx(expectedG)); // tolerance: float color multiplication and subtraction
	CHECK(selColor->blue == doctest::Approx(expectedB)); // tolerance: float color multiplication and subtraction

	// 2. Building / Structure Selection Flash (RW 0x81DC79 / 0x678FA7)
	CHECK(dBarracks->indicatorColor() == houseColor);
	CHECK(dBarracks->selectionFlashEnvelope().isEffective() == false);
	game.input->ui().selectObject(barracks->getID());
	CHECK(dBarracks->selectionFlashEnvelope().isEffective() == true);
	game.game->drawables().advance(1000.0 / 30.0);
	const RGBColor *bldColor = dBarracks->getSelectionColor();
	REQUIRE(bldColor != nullptr);
	CHECK(bldColor->red == doctest::Approx(expectedR)); // tolerance: float color multiplication and subtraction
	CHECK(bldColor->green == doctest::Approx(expectedG)); // tolerance: float color multiplication and subtraction
	CHECK(bldColor->blue == doctest::Approx(expectedB)); // tolerance: float color multiplication and subtraction

	// 3. Horde Selection Flash (RW 0x678FDF..0x679032: contain vslot 0x7C / 0x10C member list iteration)
	const ContainModuleInterface *hordeContain = horde->getContain();
	REQUIRE(hordeContain != nullptr);
	const auto *hordeMembers = hordeContain->getContainedItemsList();
	REQUIRE(hordeMembers != nullptr);
	REQUIRE(!hordeMembers->empty());

	game.input->ui().selectObject(horde->getID());
	// Horde container itself does not play selection flash (RW 0x679049 -> 0x679113)
	CHECK(dHorde->selectionFlashEnvelope().isEffective() == false);

	// All contained horde member drawables flash in player house color
	size_t memberFlashCount = 0;
	for (const Object *mObj : *hordeMembers)
	{
		if (mObj)
		{
			Drawable *m = game.game->drawables().findByObject(mObj->getID());
			REQUIRE(m != nullptr);
			CHECK(m->selectionFlashEnvelope().isEffective() == true);
			CHECK(m->indicatorColor() == houseColor);
			CHECK(m->getHouseColor() == 0u);
			++memberFlashCount;
		}
	}
	CHECK(memberFlashCount == hordeMembers->size());
}

TEST_CASE("ENNORATH scenario PORT-176: Mordor SpellBookMordorTaintNoTime triggers FX_TaintSpellFX1 DynamicDecal exmordorquake02 with AuxPngPath and 43x43 terrain-conforming mesh (P-107, RW 0x50C050 / 0x530D29)")
{
	World world;
	if (!world.load("Ennorath scenario PORT-176 (Mordor Desecrated earth / FX_TaintSpellFX1)"))
	{
		return;
	}

	// Retail + Ennorath archive has both compiled JPG RGB and PNG alpha mask for exmordorquake02 (RW 0x50B73E / 0x530D29)
	const ParticleTextureResolution texRes = ResolveParticleTexture(
		"exmordorquake02",
		[&](const std::string &path) { return world.merged.fs->doesFileExist(path); });
	REQUIRE(texRes.Found == true);
	CHECK(texRes.Kind == ParticleTextureResolution::JPG);
	CHECK(texRes.Path == "art\\compiledtextures\\ex\\exmordorquake02.jpg");
	CHECK(texRes.AuxPngPath == "art\\compiledtextures\\ex\\exmordorquake02.png");

	std::vector<std::uint8_t> jpgBytes, pngBytes;
	std::string readErr;
	REQUIRE_MESSAGE(world.merged.fs->readFile(texRes.Path, jpgBytes, &readErr), readErr);
	REQUIRE_MESSAGE(world.merged.fs->readFile(texRes.AuxPngPath, pngBytes, &readErr), readErr);
	CHECK(jpgBytes.empty() == false);
	CHECK(pngBytes.empty() == false);

	// Load Ennorath FXLists (data/ini/fxlist.ini:10981..11127) and play FX_TaintSpellFX1
	FXPlayback playback(*world.merged.fs, RandomAlgorithm::RotWK_GameDat_LCG);
	const std::vector<std::string> fxErrors = playback.loadRetailData(world.shared.world->report().macrosBeforeFX);
	CHECK(fxErrors.empty());
	REQUIRE(playback.playFXList("FX_TaintSpellFX1", Coord3D{ 500.0f, 500.0f, 0.0f }, false));
	REQUIRE(playback.decals().size() == 1u);

	const DecalFx &decal = playback.decals()[0];
	CHECK(decal.desc.name == "exmordorquake02");
	CHECK(decal.desc.size == 420.0f);
	CHECK(decal.desc.shader == DYNAMIC_DECAL_SHADER_ALPHA);
	CHECK(decal.desc.opacityPeak == 200);
	CHECK(decal.desc.fadeOneFrames == 150);
	CHECK(decal.desc.lifetimeFrames == 2700);

	for (int s = 0; s < 150; ++s)
	{
		playback.step();
	}
	REQUIRE(playback.decals().size() == 1u);
	CHECK(playback.decals()[0].opacityAt(playback.clientFrame()) == 200);

	TerrainScorchBuffer::Terrain terrain;
	terrain.xExtent = 128;
	terrain.yExtent = 128;
	terrain.border = 4;
	terrain.heights.resize(128u * 128u, 512);
	terrain.flip.resize(128u * 128u, 0);
	for (int j = 0; j < 128; ++j)
	{
		for (int i = 0; i < 128; ++i)
		{
			terrain.heights[(size_t)j * 128u + (size_t)i] = (std::uint16_t)(256 + ((i % 7) * 48) + ((j % 5) * 32));
			terrain.flip[(size_t)j * 128u + (size_t)i] = (std::uint8_t)((i + j) & 1);
		}
	}

	DynamicDecalMesh built;
	BuildDynamicDecalMesh({ &playback.decals()[0] }, playback.clientFrame(), &terrain, nullptr, built);
	CHECK(built.decalsBuilt == 1);
	CHECK(built.vertices.size() == 43u * 43u);
	CHECK(built.indices.size() == 42u * 42u * 6u);
}

