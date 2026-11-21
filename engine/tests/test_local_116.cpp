// OpenBFME. GPL-3.0.
// LOCAL-116 (fork issue 800): hero voices and ability sounds (P-159, P-161, P-163).
// Sources of expectations:
//   - P-159A (low-health VoiceFear 0x7E0): RW 0x8C44F6 .. 0x8C4585 (ActiveBody::attemptDamage),
//     DONOR BFME2 ActiveBody.cpp:2220; Ennorath data/ini/object/goodfaction/units/elven/legolas.ini (VoiceFear = LegolasVoiceHelpMe).
//   - P-159B (hero death EVA): RW 0x6990DD .. 0x699145 (Object::onDie), RW 0x5DD750 .. 0x5DD802 (Eva::update);
//     Ennorath data/ini/object/goodfaction/units/elven/legolas.ini (EvaEventDieOwner = LegolasDie, DeathFX = FX_LegolasDieToRespawn).
//   - P-161 (corsair firebomb detonation FireFX): RW 0x85F1FE .. 0x85F213 -> RW 0x6CF4D6 -> RW 0x6CF38E -> RW 0x6CEF6D
//     -> RW 0x6CC915 (FireFX RW 0x6CCB4B .. 0x6CCBF3), DONOR BFME2 0x002CE8AA -> 0x002CBBF5;
//     Ennorath data/ini/includes/weapon.inc (CorsairFireBombBombardHarad / CorsairFireBombWarheadMod FireFX = FX_ImpactCorsairFireBomb).
//   - P-163 (Harad SpellBookHaradRaid): RW 0x5E21D8 .. 0x5E2235 -> RW 0xB4D9A0 -> RW 0xB4FB20 (FXList::doFXPos shroud check);
//     Ennorath data/ini/object/system/includes/systemevil.inc (TriggerFX = FX_HaradRaid), data/ini/includes/fxlist.inc (GloinShakeFoundation, SpellHaradRaid).

#include "doctest.h"
#include "CombatTestUtil.h"
#include "EnnorathScenario.h"

#include "Common/Audio/AudioAssetCache.h"
#include "Common/Audio/AudioEntryPoints.h"
#include "Common/Audio/AudioIni.h"
#include "Common/Audio/GameAudio.h"
#include "Common/Audio/SimulatedAudioDevice.h"
#include "Common/INI.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/Science.h"
#include "Common/SpecialPower.h"
#include "GameClient/Eva.h"
#include "GameClient/FXPlayback.h"
#include "GameClient/GUI/Skirmish/IniSkirmishSetupSource.h"
#include "GameClient/LiveFX.h"
#include "GameClient/LiveGame.h"
#include "GameClient/LiveGameAudio.h"
#include "GameClient/SpellBookUI.h"
#include "GameClient/UnitVoiceResponse.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/Combat/ObjectWeapons.h"
#include "GameLogic/Damage.h"
#include "GameLogic/FXEvents.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/ActiveBody.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/NewGame/NewGame.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/WeaponSetToggle.h"
#include "Libraries/WWVegas/WW3D2/assetmgr.h"

#include <algorithm>
#include <cctype>
#include <memory>
#include <string>
#include <vector>

namespace
{
struct AudioGuard
{
	const void *voiceOwner = nullptr;
	~AudioGuard()
	{
		if (voiceOwner)
		{
			AudioApi::uninstallUnitVoiceHandler(voiceOwner);
		}
		AudioApi::installEva(nullptr);
		AudioApi::install(nullptr);
	}
};

struct DummyClientHooks : ObjectClientHooks
{
	void objectCreated(Object &obj) override { obj.friend_bindToClient(this); }
	void objectDestroyed(Object &) override {}
};

bool containsNoCase(const std::string &haystack, const std::string &needle)
{
	auto lower = [](std::string s) {
		for (char &c : s)
			c = (char)std::tolower((unsigned char)c);
		return s;
	};
	return lower(haystack).find(lower(needle)) != std::string::npos;
}

bool deviceStartedMatching(const SimulatedAudioDevice &dev, size_t fromIdx, const std::string &substr)
{
	const auto &log = dev.log();
	for (size_t i = fromIdx; i < log.size(); ++i)
	{
		if (log[i].kind == SimulatedAudioDevice::LogEntry::Start && containsNoCase(log[i].file, substr))
		{
			return true;
		}
	}
	return false;
}

void stepLive(LiveGame &game, LiveFX &fx, FXPlayback &playback, LiveGameAudio &audio, AudioManager &mgr, Eva &eva, SimulatedAudioDevice &dev, double &clockMs, int logicFrames)
{
	for (int f = 0; f < logicFrames; ++f)
	{
		for (int p = 0; p < 6; ++p)
		{
			game.advance(0.2 / 6.0);
			LiveGameAudio::drainDeferredAudio();
			fx.flushPending();
			fx.updateAttachedSystems();
			playback.step();
			audio.update();
			clockMs += AudioManager::tickIntervalMs();
			dev.setTime(clockMs);
			mgr.update(clockMs);
			eva.update(clockMs);
		}
	}
}
} // namespace

// Source: RW 0x8C44F6 .. 0x8C4585 (ActiveBody::attemptDamage), DONOR BFME2 ActiveBody.cpp:2220.
TEST_CASE("LOCAL-116 P-159A synthetic: ActiveBody posts VOICE_EVENT_FEAR (0x7E0) when health crosses below 35%")
{
	const char *kIni =
		"Object Local116Hero\n"
		"  Side = Men\n"
		"  KindOf = SELECTABLE HERO INFANTRY\n"
		"  Body = ActiveBody ModuleTag_Body\n"
		"    MaxHealth = 1000\n"
		"  End\n"
		"End\n";

	combattest::CombatWorld w(kIni);
	DummyClientHooks hooks;
	w.logic->setClientHooks(&hooks);
	Object *hero = w.unit("Local116Hero", 'A', 100.0f, 100.0f);
	REQUIRE(hero != nullptr);

	std::vector<std::pair<int, std::uint32_t>> posted;
	int ownerToken = 0;
	AudioGuard guard;
	guard.voiceOwner = &ownerToken;
	AudioApi::installUnitVoiceHandler([&](const AudioApi::UnitVoiceParams &p) {
		posted.emplace_back(p.voiceEvent, p.objectId);
	}, &ownerToken);

	BodyModuleInterface *body = hero->getBodyModule();
	REQUIRE(body != nullptr);

	// Neighbour 1: drop from 100% to 40% (still >= 35%) -> no VOICE_EVENT_FEAR
	{
		DamageInfo d;
		d.m_input.m_damageType = DAMAGE_UNRESISTABLE;
		d.m_input.m_amount = 600.0f;
		body->attemptDamage(d);
		CHECK(posted.empty());
	}
	// Target case: drop from 40% to 30% (crosses below 35%, still alive) -> posts 0x7E0
	{
		DamageInfo d;
		d.m_input.m_damageType = DAMAGE_UNRESISTABLE;
		d.m_input.m_amount = 100.0f;
		body->attemptDamage(d);
		REQUIRE(posted.size() == 1);
		CHECK(posted[0].first == 0x7E0);
		CHECK(posted[0].second == (std::uint32_t)hero->getID());
	}
	// Neighbour 2: already below 35% (30% -> 20%) -> does not post again
	{
		DamageInfo d;
		d.m_input.m_damageType = DAMAGE_UNRESISTABLE;
		d.m_input.m_amount = 100.0f;
		body->attemptDamage(d);
		CHECK(posted.size() == 1);
	}

	// Neighbour 3: lethal hit from 100% directly to 0 HP (currentHealth == 0) -> no VOICE_EVENT_FEAR (RW 0x8C452C)
	Object *hero2 = w.unit("Local116Hero", 'A', 120.0f, 100.0f);
	REQUIRE(hero2 != nullptr);
	{
		DamageInfo d;
		d.m_input.m_damageType = DAMAGE_UNRESISTABLE;
		d.m_input.m_amount = 2000.0f;
		hero2->getBodyModule()->attemptDamage(d);
		CHECK(posted.size() == 1);
	}
}

// Source: RW 0x85F1FE .. 0x85F213 -> RW 0x6CF4D6 -> RW 0x6CF38E -> RW 0x6CEF6D -> RW 0x6CC915 (FireFX RW 0x6CCB4B .. 0x6CCBF3),
// DONOR BFME2 0x002CE8AA -> 0x002CBBF5.
TEST_CASE("LOCAL-116 P-161 synthetic: BezierProjectileBehavior detonation emits warhead FireFX")
{
	const char *kIni =
		"Weapon Local116BombWarhead\n"
		"  FireFX = FX_Local116BombImpact\n"
		"  RadiusDamageAffects = ENEMIES\n"
		"  DamageNugget\n"
		"    Damage = 10\n"
		"    Radius = 20\n"
		"    DamageType = FLAME\n"
		"    DeathType = BURNED\n"
		"  End\n"
		"End\n"
		"Weapon Local116SilentWarhead\n"
		"  RadiusDamageAffects = ENEMIES\n"
		"  DamageNugget\n"
		"    Damage = 10\n"
		"    Radius = 20\n"
		"    DamageType = FLAME\n"
		"    DeathType = BURNED\n"
		"  End\n"
		"End\n"
		"Weapon Local116BombLauncher\n"
		"  AttackRange = 300\n"
		"  WeaponSpeed = 200\n"
		"  DelayBetweenShots = 2000\n"
		"  ClipSize = 1\n"
		"  AutoReloadsClip = Yes\n"
		"  NoVictimNeeded = Yes\n"
		"  ProjectileNugget\n"
		"    ProjectileTemplateName = Local116BombProj\n"
		"    WarheadTemplateName = Local116BombWarhead\n"
		"  End\n"
		"End\n"
		"Weapon Local116SilentLauncher\n"
		"  AttackRange = 300\n"
		"  WeaponSpeed = 200\n"
		"  DelayBetweenShots = 2000\n"
		"  ClipSize = 1\n"
		"  AutoReloadsClip = Yes\n"
		"  NoVictimNeeded = Yes\n"
		"  ProjectileNugget\n"
		"    ProjectileTemplateName = Local116BombProj\n"
		"    WarheadTemplateName = Local116SilentWarhead\n"
		"  End\n"
		"End\n"
		"Object Local116BombProj\n"
		"  Side = Mordor\n"
		"  KindOf = PROJECTILE\n"
		"  Body = ActiveBody ModuleTag_Body\n"
		"    MaxHealth = 10\n"
		"  End\n"
		"  Behavior = BezierProjectileBehavior ModuleTag_Proj\n"
		"    FirstHeight = 20\n"
		"    SecondHeight = 20\n"
		"    FirstPercentIndent = 30%\n"
		"    SecondPercentIndent = 70%\n"
		"    DetonateCallsKill = No\n"
		"  End\n"
		"End\n"
		"Object Local116Thrower\n"
		"  Side = Mordor\n"
		"  KindOf = SELECTABLE INFANTRY\n"
		"  Body = ActiveBody ModuleTag_Body\n"
		"    MaxHealth = 200\n"
		"  End\n"
		"  Behavior = AIUpdateInterface ModuleTag_AI\n"
		"  End\n"
		"  WeaponSet\n"
		"    Weapon = PRIMARY Local116BombLauncher\n"
		"  End\n"
		"End\n"
		"Object Local116SilentThrower\n"
		"  Side = Mordor\n"
		"  KindOf = SELECTABLE INFANTRY\n"
		"  Body = ActiveBody ModuleTag_Body\n"
		"    MaxHealth = 200\n"
		"  End\n"
		"  Behavior = AIUpdateInterface ModuleTag_AI\n"
		"  End\n"
		"  WeaponSet\n"
		"    Weapon = PRIMARY Local116SilentLauncher\n"
		"  End\n"
		"End\n";

	combattest::CombatWorld w(kIni);
	struct Collector : FXEventSink { std::vector<FXEvent> events; void onFXEvent(const FXEvent &e) override { events.push_back(e); } } collector;
	w.logic->fxEvents().setSink(&collector);

	// Target case: warhead with FireFX emits WEAPON_FIRE_FX on detonation
	Object *thrower = w.unit("Local116Thrower", 'A', 100.0f, 100.0f);
	REQUIRE(thrower != nullptr);
	const Coord3D targetPos{ 200.0f, 100.0f, 0.0f };
	thrower->getAIUpdateInterface()->aiAttackPosition(targetPos, 1, CMD_FROM_PLAYER);
	for (int i = 0; i < 30; ++i)
	{
		for (int phase = 1; phase <= 6; ++phase) w.logic->update(phase);
	}
	REQUIRE(w.logic->combat().counters().projectilesDetonated == 1u);
	bool sawImpactFX = false;
	for (const FXEvent &e : collector.events)
	{
		if (e.kind == FXEvent::WEAPON_FIRE_FX && e.fxList && *e.fxList == "FX_Local116BombImpact")
		{
			sawImpactFX = true;
			break;
		}
	}
	CHECK(sawImpactFX);

	// Neighbour: warhead without FireFX emits no detonation FireFX
	collector.events.clear();
	Object *silent = w.unit("Local116SilentThrower", 'A', 100.0f, 200.0f);
	REQUIRE(silent != nullptr);
	const Coord3D silentTarget{ 200.0f, 200.0f, 0.0f };
	silent->getAIUpdateInterface()->aiAttackPosition(silentTarget, 1, CMD_FROM_PLAYER);
	const auto prevDet = w.logic->combat().counters().projectilesDetonated;
	for (int i = 0; i < 30; ++i)
	{
		for (int phase = 1; phase <= 6; ++phase) w.logic->update(phase);
	}
	REQUIRE(w.logic->combat().counters().projectilesDetonated == prevDet + 1u);
	bool sawAnyWeaponFireFX = false;
	for (const FXEvent &e : collector.events)
	{
		if (e.kind == FXEvent::WEAPON_FIRE_FX)
		{
			sawAnyWeaponFireFX = true;
		}
	}
	CHECK_FALSE(sawAnyWeaponFireFX);
}

// Source: Ennorath 1.9.8.3 INI (legolas.ini, haradcorsairsofumbar.ini, weapon.inc, fxlist.ini, fxlist.inc, systemevil.inc)
// + RW 0x8C44F6..0x8C4585 (P-159A), RW 0x6990DD..0x699145 / 0x5DD750..0x5DD802 (P-159B),
//   RW 0x85F1FE -> 0x6CF4D6 -> 0x6CC915 (P-161), RW 0x5E21D8..0x5E2235 (P-163).
TEST_CASE("LOCAL-116 ENNORATH scenario: P-159 hero voices, P-161 corsair bombs, P-163 Harad Steal Slaves reach audio device")
{
	ennorathscenario::World w;
	if (!w.load("LOCAL-116 ENNORATH scenario"))
	{
		return;
	}
	AudioGuard guard;
	INIEnvironment env;
	env.fileSystem = w.merged.fs.get();
	AudioIniState ini;
	ini.registerBlocks(env.blocks);
	{
		INI parser(env);
		ini.loadAll(parser);
		ini.loadEva(parser);
		ini.loadLargeGroupAudio(parser);
	}
	AudioAssetCache cache(w.merged.fs.get(), 64u * 1024u * 1024u);
	SimulatedAudioDevice device(&cache);
	AudioManager manager(ini, cache, device, RandomAlgorithm::RotWK_GameDat_LCG, 1u);
	AudioApi::install(&manager);
	Eva eva(ini, manager);
	AudioApi::installEva(&eva);

	std::string error;
	std::vector<MapCacheEntry> mapCache;
	REQUIRE_MESSAGE(IniSkirmishSetupSource::loadMapCache(*w.merged.fs, mapCache, &error), error);
	REQUIRE(!mapCache.empty());
	NewGameMessage m;
	m.game.mapName = mapCache.front().name;
	for (const auto &mc : mapCache)
	{
		if (mc.name.find("fords of isen") != std::string::npos || mc.name.find("fall back") != std::string::npos)
		{
			m.game.mapName = mc.name;
			break;
		}
	}
	m.game.seed = 42;
	m.game.startingCash = 5000;
	auto findFaction = [&](const char *name) {
		for (int t = 0; t < w.shared.world->playerTemplates().getPlayerTemplateCount(); ++t)
		{
			if (w.shared.world->playerTemplates().getNthPlayerTemplate(t)->getName() == name)
				return t;
		}
		return -1;
	};
	m.game.slots[0].state = SLOT_PLAYER;
	m.game.slots[0].name = u"Player0";
	m.game.slots[0].playerTemplate = findFaction("FactionEvilmen");
	m.game.slots[0].startPos = 0;
	m.game.slots[0].color = 0;
	m.game.slots[0].teamNumber = 0;
	m.game.slots[1].state = SLOT_MED_AI;
	m.game.slots[1].name = u"Player1";
	m.game.slots[1].playerTemplate = findFaction("FactionElves");
	m.game.slots[1].startPos = 1;
	m.game.slots[1].color = 1;
	m.game.slots[1].teamNumber = 1;
	REQUIRE(m.game.slots[0].playerTemplate != -1);
	REQUIRE(m.game.slots[1].playerTemplate != -1);

	GameLogicSettings settings;
	settings.multiplayerColors.resize(8);
	NewGameStart start(RandomAlgorithm::ZH_CarryChain);
	REQUIRE_MESSAGE(NewGame::prepareNewGame(m, w.shared.world->playerTemplates(), settings, mapCache, RandomAlgorithm::ZH_CarryChain, start, &error), error);
	ArchiveW3DFileSource source(*w.merged.fs);
	WW3DAssetManager assets(source);
	LiveGame::Options opts;
	opts.start = &start;
	LiveGame game(*w.shared.world, *w.merged.fs, assets, w.shared.options);
	REQUIRE_MESSAGE(game.load(opts, &error), error);

	FXPlayback playback(*w.merged.fs, RandomAlgorithm::RotWK_GameDat_LCG);
	REQUIRE(playback.loadRetailData(w.shared.world->report().macrosBeforeFX).empty());
	LiveFX fx(game, playback);
	std::unique_ptr<LiveGameAudio> audio = LiveGameAudio::attachToInstalled(game, nullptr, &error, nullptr);
	REQUIRE_MESSAGE(audio != nullptr, error);

	// Wire UnitVoiceResponse exactly as InGameHud.cpp:213 does in a live game
	AudioApiVoiceSink voiceSink;
	UnitVoiceResponse voice(game.logic(), voiceSink);
	Player *local = game.players().getLocalPlayer();
	REQUIRE(local != nullptr);
	voice.setLocalPlayerIndex(local->getPlayerIndex());
	int voiceToken = 0;
	guard.voiceOwner = &voiceToken;
	AudioApi::installUnitVoiceHandler([&](const AudioApi::UnitVoiceParams &p) {
		UnitVoiceResponse::Info info;
		info.producer = (ObjectID)p.producerId;
		info.powerTemplate = p.powerTemplate;
		info.target = (ObjectID)p.targetId;
		info.hasPosition = p.hasPosition;
		info.position = Coord3D{ p.posX, p.posY, p.posZ };
		info.air = p.air;
		voice.pickAndPlay({ (ObjectID)p.objectId }, p.voiceEvent, &info);
	}, &voiceToken);

	double clockMs = 0.0;
	stepLive(game, fx, playback, *audio, manager, eva, device, clockMs, 2);

	Player *enemy = nullptr;
	for (int i = 0; i < game.players().getPlayerCount(); ++i)
	{
		Player *p = game.players().getNthPlayer(i);
		if (p && p != local && p->getRelationship(local) == ENEMIES)
		{
			enemy = p;
			break;
		}
	}
	REQUIRE(enemy != nullptr);

	Coord3D clearPos{ 1000.0f, 1000.0f, 0.0f };
	for (Object *o = game.logic().getFirstObject(); o; o = o->getNextObject())
	{
		if (o->getControllingPlayer() == local && o->isKindOfName("STRUCTURE"))
		{
			clearPos = *o->getPosition();
			break;
		}
	}
	manager.setListenerPosition(clearPos, Coord3D{ 0.0f, 1.0f, 0.0f });

	// --- P-159: ElvenLegolas low-health VoiceFear (LegolasVoiceHelpMe -> GULegol_voihel*.wav) & death EVA (CampElfDieLegolas -> GCElLeg_diea.wav) ---
	{
		eva.setLocalSide("Elves");
		const ThingTemplate *legT = w.shared.world->things().findTemplate("ElvenLegolas");
		REQUIRE(legT != nullptr);
		Object *leg = game.logic().newObject(legT, local->getDefaultTeam(), ObjectStatusMaskType{});
		REQUIRE(leg != nullptr);
		leg->setPosition(&clearPos);
		stepLive(game, fx, playback, *audio, manager, eva, device, clockMs, 2);

		const size_t beforeLow = device.log().size();
		BodyModuleInterface *body = leg->getBodyModule();
		REQUIRE(body != nullptr);
		DamageInfo dmg;
		dmg.m_input.m_damageType = DAMAGE_UNRESISTABLE;
		dmg.m_input.m_amount = body->getMaxHealth() * 0.8f; // 100% -> 20% (< 35%)
		body->attemptDamage(dmg);
		stepLive(game, fx, playback, *audio, manager, eva, device, clockMs, 3);
		CHECK_MESSAGE(deviceStartedMatching(device, beforeLow, "GULegol_voihel"), "P-159A: LegolasVoiceHelpMe must reach audio device");

		const size_t beforeDie = device.log().size();
		DamageInfo killDmg;
		killDmg.m_input.m_damageType = DAMAGE_UNRESISTABLE;
		killDmg.m_input.m_amount = body->getMaxHealth() * 2.0f;
		body->attemptDamage(killDmg);
		stepLive(game, fx, playback, *audio, manager, eva, device, clockMs, 5);
		CHECK_MESSAGE(deviceStartedMatching(device, beforeDie, "GCElLeg_diea"), "P-159B: CampElfDieLegolas must reach audio device");
	}

	// --- P-161: HaradCorsairsOfUmbar firebomb bombard explosion (ImpactCorsairFireBomb -> WIFire_bomb*.wav) ---
	{
		const ThingTemplate *corT = w.shared.world->things().findTemplate("HaradCorsairsOfUmbar");
		REQUIRE(corT != nullptr);
		Object *cor = game.logic().newObject(corT, local->getDefaultTeam(), ObjectStatusMaskType{});
		REQUIRE(cor != nullptr);
		const Coord3D corPos{ clearPos.x + 80.0f, clearPos.y, clearPos.z };
		cor->setPosition(&corPos);
		cor->setOrientation(3.14159265f);
		WeaponSetToggle::setObjectWeaponSetFlag(*cor, WeaponSetToggle::WEAPONSET_TOGGLE_1, true);
		stepLive(game, fx, playback, *audio, manager, eva, device, clockMs, 6);

		const size_t beforeBomb = device.log().size();
		ObjectWeapons::setObjectWeaponLock(*cor, TERTIARY_WEAPON, LOCKED_PERMANENTLY);
		const Coord3D bombPos{ clearPos.x + 20.0f, clearPos.y, clearPos.z };
		cor->getAIUpdateInterface()->aiAttackPosition(bombPos, 1, CMD_FROM_PLAYER);
		stepLive(game, fx, playback, *audio, manager, eva, device, clockMs, 40);
		CHECK_MESSAGE(deviceStartedMatching(device, beforeBomb, "WIFireB_throw"), "P-161: FirebombThrow must reach audio device");
		CHECK_MESSAGE(deviceStartedMatching(device, beforeBomb, "WIFire_bomb"), "P-161: ImpactCorsairFireBomb must reach audio device");
	}

	// --- P-163: Harad SpellBookHaradRaid ("Steal slaves") via EvilmenSpellBook on visible enemy farm ---
	{
		Object *farm = game.logic().newObject(w.shared.world->things().findTemplate("GondorFarm"), enemy->getDefaultTeam(), ObjectStatusMaskType{});
		REQUIRE(farm != nullptr);
		farm->setPosition(&clearPos);
		if (TheScienceStore)
		{
			ScienceType sc = TheScienceStore->getScienceFromInternalName("SCIENCE_HaradRaid");
			REQUIRE(sc != SCIENCE_INVALID);
			local->science().grantScience(sc);
		}
		Object *book = SpecialPowerModules::getSpellBookObject(game.logic(), *local);
		const SpecialPowerTemplate *sp = TheSpecialPowerStore ? TheSpecialPowerStore->findSpecialPowerTemplate("SpellBookHaradRaid") : nullptr;
		REQUIRE(book != nullptr);
		REQUIRE(sp != nullptr);

		const size_t beforeSpell = device.log().size();
		REQUIRE(SpecialPowerModules::doSpecialPowerAtObject(*book, sp, farm, 0u, true));
		stepLive(game, fx, playback, *audio, manager, eva, device, clockMs, 5);
		CHECK_MESSAGE(deviceStartedMatching(device, beforeSpell, "GUGloin_shakefa"), "P-163: GloinShakeFoundation must reach audio device on clear target");
		CHECK_MESSAGE(deviceStartedMatching(device, beforeSpell, "harad_raid02"), "P-163: SpellHaradRaid must reach audio device on clear target");
	}
}
