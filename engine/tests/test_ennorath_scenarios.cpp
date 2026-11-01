// Ennorath scenarios: what the owner would otherwise check by hand, on the mod's own objects (docs/ennorath/PROCESS.md, section 8). GPL-3.0.
// A case named "ENNORATH scenario <Class>: ..." takes that class off the owner's list (tools/mod/ennorath_queue.py checklist).
// The owner still checks what is seen and heard. Expected values come from the INI file each case names.
#include "EnnorathScenario.h"
#include "Common/Audio/AudioAssetCache.h"
#include "Common/Audio/AudioIni.h"
#include "Common/Audio/GameAudio.h"
#include "Common/Audio/SimulatedAudioDevice.h"
#include "Common/INI.h"
#include "Common/ModelState.h"
#include "Common/Money.h"
#include "Common/Audio/AudioIni.h"
#include "GameClient/EvaAnnounceClientCreate.h"
#include "GameClient/ClientEvents.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/Thing/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "GameClient/DrawableManager.h"
#include "GameClient/LiveGame.h"
#include "GameClient/LiveGameAudio.h"
#include "GameClient/ModelConditionAudioLoopClientBehavior.h"
#include "GameLogic/Damage.h"
#include "GameLogic/Economy.h"
#include "GameLogic/Module/GiveUpgradeUpdate.h"
#include "GameLogic/Module/ReflectDamage.h"
#include "GameLogic/Module/SalvageCrateCollide.h"
#include "GameLogic/Module/AnnounceBirthAndDeathBehavior.h"
#include "GameLogic/Module/TooltipUpgrade.h"

using ennorathscenario::World;

namespace
{
float health(Object *o) { return o->getBodyModule()->getHealth(); }

void damage(Object *victim, ObjectID source, int type, float amount)
{
	DamageInfo info;
	info.m_input.m_sourceID = source;
	info.m_input.m_damageType = type;
	info.m_input.m_deathType = DEATH_NORMAL;
	info.m_input.m_amount = amount;
	victim->attemptDamage(info);
}
} // namespace

TEST_CASE("ENNORATH scenario ReflectDamage: the Witch-king returns 30% of a hit to the attacker, for every damage type the mod's plus-only list leaves on")
{
	World world;
	if (!world.load("Ennorath scenario ReflectDamage")) return;
	structtest::Arena arena(world.shared, "FactionAngmar", "FactionMen");
	// data\ini\object\evilfaction\units\angmar\angmarwitchking.ini: DamageTypesToReflect = +CRUSH +SLASH +HERO +URUK +SPECIALIST, ReflectDamagePercentage = 30%
	Object *witchKing = world.place(arena, "AngmarWitchking_mod", 0, 400, 400);
	Object *attacker = world.place(arena, "AngmarOrcWarrior", 1, 420, 400);
	Object *twin = world.place(arena, "AngmarOrcWarrior", 1, 440, 400);
	REQUIRE(dynamic_cast<ReflectDamage *>(witchKing->findModule("ReflectDamage")));
	const float full = health(attacker);
	REQUIRE(health(twin) == full);
	damage(witchKing, attacker->getID(), DAMAGE_SLASH, 100.0f);
	// the same 30 of reflected damage on an identical unit: the attacker's armour counts on both
	damage(twin, witchKing->getID(), DAMAGE_REFLECTED, 30.0f);
	CHECK(health(attacker) < full); // tolerance: the exact amount is the twin's, the next line
	CHECK(health(attacker) == doctest::Approx(health(twin))); // tolerance: float health of the same arithmetic
	// the mod's list has no NONE: retail's parser starts from ALL, so every damage type is reflected (RW 0x73A5D5, test_modgap1_reflect.cpp)
	damage(witchKing, attacker->getID(), DAMAGE_PIERCE, 100.0f);
	damage(twin, witchKing->getID(), DAMAGE_REFLECTED, 30.0f);
	CHECK(health(attacker) == doctest::Approx(health(twin))); // tolerance: float health of the same arithmetic
}

TEST_CASE("ENNORATH scenario ModelConditionAudioLoopClientBehavior: Faramir's scout taunts while it raises a flag and falls silent when engaged; a fortress holds its collapse sound for RUBBLE")
{
	World world;
	if (!world.load("Ennorath scenario ModelConditionAudioLoopClientBehavior")) return;
	hudtest::Rig game(world.shared);
	INIEnvironment env;
	env.fileSystem = world.merged.fs.get();
	AudioIniState ini;
	ini.registerBlocks(env.blocks);
	INI reader(env);
	ini.loadAll(reader);
	AudioAssetCache cache(world.merged.fs.get(), 32u << 20);
	SimulatedAudioDevice device(&cache);
	AudioManager mgr(ini, cache, device, RandomAlgorithm::ZH_CarryChain, 3);
	mgr.enableEventLog(true);
	LiveGameAudio att(*game.game, mgr);
	// data\ini\object\goodfaction\units\men\faramirscout.ini: Required:RAISING_FLAG Excluded:DYING ENGAGED Sound:EmotionMenGoodCompilationBFME1and2VoxTauntSinglesLoop
	const std::string taunt = "EmotionMenGoodCompilationBFME1and2VoxTauntSinglesLoop";
	std::string error;
	Object *scout = game.game->createObject("GondorFaramirScout", game.index(), Coord3D{ 1200.0f, 1200.0f, 0.0f }, 0.0f, &error);
	REQUIRE_MESSAGE(scout != nullptr, error);
	mgr.setListenerPosition(*scout->getPosition(), Coord3D{ 0.0f, 1.0f, 0.0f });
	double t = 0.0;
	auto frames = [&](int n) {
		for (int i = 0; i < n; ++i)
		{
			game.logic().runLogicFrame();
			for (int c = 0; c < 6; ++c)
			{
				game.game->refreshClient(1000.0 / 30.0, (double)c / 6.0);
				t += 1000.0 / 30.0;
				device.setTime(t);
				mgr.update(t);
			}
		}
	};
	frames(2);
	ModelConditionAudioLoopClientBehavior *loop = nullptr;
	for (const auto &m : game.game->drawables().findByObject(scout->getID())->clientModules())
	{
		loop = loop ? loop : dynamic_cast<ModelConditionAudioLoopClientBehavior *>(m.get());
	}
	REQUIRE(loop != nullptr);
	CHECK(loop->handle() == AHSV_NoSound);
	scout->setModelConditionState(ModelCondition::indexOf("RAISING_FLAG"), true);
	frames(2);
	CHECK(loop->heldSound() == taunt);
	REQUIRE(loop->handle() >= AHSV_FirstHandle); // tolerance: the audio manager numbers its handles; any real one is at least the first
	const AudioHandle playing = loop->handle();
	CHECK(mgr.isCurrentlyPlaying(playing));
	bool logged = false;
	for (const AudioLogEntry &e : mgr.takeEventLog())
	{
		logged = logged || (e.event == taunt && e.drawableId == (std::uint32_t)game.game->drawables().findByObject(scout->getID())->getID());
	}
	CHECK(logged);
	scout->setModelConditionState(ModelCondition::indexOf("ENGAGED"), true);
	frames(2);
	CHECK(loop->heldSound().empty());
	// the loop ends: a removed event stops looping and plays its Decay (AudioManager::stopAudioEvent); one still looping would never end
	for (int i = 0; i < 100 && mgr.isCurrentlyPlaying(playing); ++i)
	{
		frames(1);
	}
	CHECK_FALSE(mgr.isCurrentlyPlaying(playing));
	// data\ini\object\goodfaction\structures\arnor\arnorfortress.ini: REQUIRED:RUBBLE Sound:BuildingSink
	const ThingTemplate *fortress = world.shared.world->things().findTemplate("ArnorFortress");
	REQUIRE(fortress != nullptr);
	const ModelConditionAudioLoopClientBehaviorModuleData *data = nullptr;
	for (const auto &n : fortress->clientBehaviorModules().nuggets())
	{
		data = data ? data : dynamic_cast<const ModelConditionAudioLoopClientBehaviorModuleData *>(n.data.get());
	}
	REQUIRE(data != nullptr);
	ModelConditionFlags rubble;
	rubble.set(ModelCondition::indexOf("RUBBLE"));
	CHECK(data->selectSound(rubble) == "BuildingSink");
	CHECK(data->selectSound(ModelConditionFlags()).empty());
}

TEST_CASE("ENNORATH scenario SalvageCrateCollide: a unit on the treasure chest collects the 5000 the chest holds")
{
	World world;
	if (!world.load("Ennorath scenario SalvageCrateCollide")) return;
	structtest::Arena arena(world.shared, "FactionAngmar", "FactionMen");
	// data\ini\object\projectile.ini, ZweiteVarianteGoldOld: ResourceChance = 100%, MinResource = MaxResource = 5000, AllowAIPickup = Yes
	const ThingTemplate *tmpl = world.shared.world->things().findTemplate("ZweiteVarianteGoldOld");
	REQUIRE(tmpl);
	Object *chest = arena.logic.newObject(tmpl, arena.players.getNeutralPlayer()->getDefaultTeam(), ObjectStatusMaskType{});
	REQUIRE(chest);
	const Coord3D spot{ 400.0f, 400.0f, 0.0f };
	chest->setPosition(&spot);
	const ObjectID chestId = chest->getID();
	Object *unit = world.place(arena, "AngmarOrcWarrior", 0, 400, 400);
	const std::uint32_t before = arena.player(0)->getMoney()->countMoney();
	for (int i = 0; i < 5 && arena.logic.findObjectByID(chestId); ++i) arena.logic.runLogicFrame();
	CHECK(arena.logic.findObjectByID(chestId) == nullptr);
	CHECK(unit != nullptr);
	// RW 0x8C14DD: a skirmish multiplies the amount by MultiPlayMoneyMult (test_salvage_crate.cpp reads it the same way)
	const Economy &economy = arena.logic.economy();
	const float mult = economy.isMultiplayerGame() ? economy.multiPlayMoneyMult(economy.livePlayableCount(false)) : 1.0f;
	CHECK(arena.player(0)->getMoney()->countMoney() == before + (std::uint32_t)(int)(float)(5000.0 * (double)mult));
}

TEST_CASE("ENNORATH scenario GiveUpgradeUpdate: a Dwarven porter carries Forged Blades to the nearest axeman and fades away")
{
	World world;
	if (!world.load("Ennorath scenario GiveUpgradeUpdate")) return;
	structtest::Arena arena(world.shared, "FactionDwarves", "FactionMen");
	// data\ini\object\goodfaction\units\dwarven\dwarvenporter.ini: ModuleTag_UpgradeNearestUpdate DeliverUpgrade = Yes, StartAbilityRange = 15, FadeOutSpeed = 0.1
	// dwarvenaxtschwinger.ini: TriggeredBy = Upgrade_DwarvenForgedBlades
	Object *porter = world.place(arena, "DwarvenPorter", 0, 300, 300);
	Object *nearest = world.place(arena, "DwarvenAxtschwinger", 0, 310, 300);
	Object *farther = world.place(arena, "DwarvenAxtschwinger", 0, 500, 300);
	Object *enemy = world.place(arena, "DwarvenAxtschwinger", 1, 305, 300);
	const UpgradeTemplate *blades = TheUpgradeCenter->findUpgrade("Upgrade_DwarvenForgedBlades");
	REQUIRE(blades);
	REQUIRE(nearest->affectedByUpgrade(blades));
	porter->giveUpgrade(blades);
	const ObjectID porterId = porter->getID();
	for (int i = 0; i < 300 && arena.logic.findObjectByID(porterId); ++i) arena.logic.runLogicFrame();
	CHECK(nearest->hasUpgrade(blades));
	CHECK_FALSE(farther->hasUpgrade(blades));
	CHECK_FALSE(enemy->hasUpgrade(blades));
	CHECK(arena.logic.findObjectByID(porterId) == nullptr);
}

TEST_CASE("ENNORATH scenario EvaAnnounceClientCreate: visible dropped ring announces after its delay")
{
 World world;
 if (!world.load("Ennorath scenario EvaAnnounceClientCreate")) return;
 // data/ini/crate.ini: TheDroppedRing, ModuleTag_AnnounceCreate.
 const ThingTemplate *tt = world.shared.world->things().findTemplate("TheDroppedRing");
 REQUIRE(tt);
 const EvaAnnounceClientCreateModuleData *data = nullptr;
 for (const auto &n : tt->clientUpdateModules().nuggets())
  if (n.name == "EvaAnnounceClientCreate") data = dynamic_cast<const EvaAnnounceClientCreateModuleData *>(n.data.get());
 REQUIRE(data);
 const int discovered = world.shared.world->audio().eva.findIndex("DiscoveredRing", false);
 REQUIRE(discovered != -1);
 CHECK(data->owner == discovered);
 CHECK(data->ally == discovered);
 CHECK(data->enemy == discovered);
 CHECK(data->delay == 8);
 CHECK(data->onlyVisible);
 CHECK(data->usePosition);
 CHECK(data->fakeRadar);
 structtest::Arena arena(world.shared, "FactionAngmar", "FactionMen");
 ClientEventRecorder recorder(arena.logic);
 arena.logic.setClientHooks(&recorder);
 struct Unbind { GameLogic &logic; ~Unbind() { logic.setClientHooks(nullptr); } } unbind{arena.logic};
 Object *ring = world.place(arena, "TheDroppedRing", 0, 400, 400);
 const auto creation = recorder.take(false);
 for (const auto &e : creation) CHECK(e.kind != ClientEvent::BUILD_COMPLETE);
 ring->friend_onBuildComplete();
 const auto completed = recorder.take(false);
 REQUIRE(completed.size() == 1);
 CHECK(completed[0].kind == ClientEvent::BUILD_COMPLETE);
 CHECK(completed[0].object == ring->getID());
 EvaAnnounceClientCreate m(nullptr, data); m.onBuildCompleteAt(10);
 bool sight = false;
 CHECK(m.update(17, false, true, false, 0, sight) == -1);
 CHECK(m.update(18, false, true, false, 0, sight) == discovered);
}

TEST_CASE("ENNORATH scenario TooltipUpgrade: Gondor infantry banner changes tooltip name to GondorHauptmann on HerzdesFeuers upgrade")
{
	World world;
	if (!world.load("Ennorath scenario TooltipUpgrade")) return;
	structtest::Arena arena(world.shared, "FactionMen", "FactionMordor");
	Object *banner = world.place(arena, "GondorInfantryBanner", 0, 400, 400);
	REQUIRE(banner);
	auto *tu = dynamic_cast<TooltipUpgrade *>(banner->findModule("TooltipUpgrade"));
	REQUIRE(tu);
	CHECK(TooltipUpgrade::displayNameOf(*banner).empty());

	const UpgradeTemplate *upgrade = TheUpgradeCenter->findUpgrade("Upgrade_HerzdesFeuers");
	REQUIRE(upgrade);
	banner->giveUpgrade(upgrade);
	CHECK(TooltipUpgrade::displayNameOf(*banner) == "OBJECT:GondorHauptmann");
	CHECK(tu->activeDisplayName() == "OBJECT:GondorHauptmann");

	bool stopped = false;
	for (const auto &line : arena.logic.report().stops)
	{
		if (line == TooltipUpgrade::stopLine())
		{
			stopped = true;
			break;
		}
	}
	CHECK(stopped);
}

TEST_CASE("ENNORATH scenario AnnounceBirthAndDeathBehavior: GondorEarnur hero initial spawn and death trigger announcements and stop S-6012")
{
	World world;
	if (!world.load("Ennorath scenario AnnounceBirthAndDeathBehavior")) return;
	structtest::Arena arena(world.shared, "FactionMen", "FactionMordor");
	Object *earnur = world.place(arena, "GondorEarnur", 0, 400, 400);
	REQUIRE(earnur);
	auto *announce = dynamic_cast<AnnounceBirthAndDeathBehavior *>(earnur->findModule("AnnounceBirthAndDeathBehavior"));
	REQUIRE(announce);

	CHECK(announce->birthAnnouncementCount() == 1);
	CHECK(announce->lastNotificationType() == AnnounceBirthAndDeathBehavior::kHeroInitialSpawnType);
	CHECK(AnnounceBirthAndDeathBehavior::shouldAnnounceHeroEvent(*earnur, arena.player(0)));

	bool stopped = false;
	for (const auto &line : arena.logic.report().stops)
	{
		if (line == AnnounceBirthAndDeathBehavior::stopLine())
		{
			stopped = true;
			break;
		}
	}
	CHECK(stopped);

	damage(earnur, INVALID_ID, DAMAGE_SLASH, 10000.0f);
	CHECK(announce->deathAnnouncementCount() == 1);
	CHECK(announce->lastNotificationType() == AnnounceBirthAndDeathBehavior::kHeroDeathType);
}
