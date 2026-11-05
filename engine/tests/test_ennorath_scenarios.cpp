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
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Economy.h"
#include "GameLogic/Module/GiveUpgradeUpdate.h"
#include "GameLogic/Module/ReflectDamage.h"
#include "GameLogic/Module/SalvageCrateCollide.h"
#include "GameLogic/Module/DestroyEnvironmentUpdate.h"
#include "GameLogic/Module/PartTheHeavensUpdate.h"
#include "GameClient/RadarMarkerClientUpdate.h"
#include "GameLogic/Module/AnnounceBirthAndDeathBehavior.h"
#include "GameLogic/Module/TooltipUpgrade.h"
#include "GameLogic/Object/ObjectDoCommandButton.h"
#include "GameLogic/Object/Contain/HordeContainRuntime.h"
#include "GameLogic/Module/UpgradeModuleClasses.h"
#include "GameLogic/Map/FireLogicSystem.h"
#include "GameLogic/Combat/WeaponDelivery.h"
#include "GameLogic/WeaponStores.h"

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

TEST_CASE("ENNORATH scenario PartTheHeavensUpdate: PartTheHeavens spell effect has configured curves")
{
	World world;
	if (!world.load("Ennorath scenario PartTheHeavensUpdate")) return;
	const ThingTemplate *tt = world.shared.world->things().findTemplate("PartTheHeavens");
	if (!tt) return;
	const PartTheHeavensUpdateModuleData *data = nullptr;
	for (const auto &m : tt->behaviorModules().nuggets())
	{
		if (m.name == "PartTheHeavensUpdate")
		{
			data = dynamic_cast<const PartTheHeavensUpdateModuleData *>(m.data.get());
			break;
		}
	}
	REQUIRE(data);
	CHECK(data->m_texture == "SCCommandBar");
	CHECK(data->m_color == 0xFFFFFFFF);
	CHECK(data->m_radius(0.0f) == 0.0f);
	CHECK(data->m_radius(20.0f) == 100.0f);
}

TEST_CASE("ENNORATH scenario DestroyEnvironmentUpdate: PartTheHeavens specifies environment destruction times")
{
	World world;
	if (!world.load("Ennorath scenario DestroyEnvironmentUpdate")) return;
	const ThingTemplate *tt = world.shared.world->things().findTemplate("PartTheHeavens");
	if (!tt) return;
	const DestroyEnvironmentUpdateModuleData *data = nullptr;
	for (const auto &m : tt->behaviorModules().nuggets())
	{
		if (m.name == "DestroyEnvironmentUpdate")
		{
			data = dynamic_cast<const DestroyEnvironmentUpdateModuleData *>(m.data.get());
			break;
		}
	}
	REQUIRE(data);
	CHECK(data->m_destructionTime > 0); // tolerance: valid positive destruction countdown in frames
}

TEST_CASE("ENNORATH scenario RadarMarkerClientUpdate: TheDroppedRing specifies PingOneRing marker type")
{
	World world;
	if (!world.load("Ennorath scenario RadarMarkerClientUpdate")) return;
	const ThingTemplate *tt = world.shared.world->things().findTemplate("TheDroppedRing");
	if (!tt) return;
	const RadarMarkerClientUpdateModuleData *data = nullptr;
	for (const auto &n : tt->clientUpdateModules().nuggets())
	{
		if (n.name == "RadarMarkerClientUpdate")
		{
			data = dynamic_cast<const RadarMarkerClientUpdateModuleData *>(n.data.get());
			break;
		}
	}
	REQUIRE(data);
	CHECK(data->m_markerType == "PingOneRing");
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

	damage(earnur, INVALID_ID, DAMAGE_SLASH, 50000.0f);
	CHECK(announce->deathAnnouncementCount() == 1);
	CHECK(announce->lastNotificationType() == AnnounceBirthAndDeathBehavior::kHeroDeathType);
}

TEST_CASE("ENNORATH scenario AISpecialPowerUpdate: GondorFighterHorde toggles formation with Command_ToggleFormationGondorFighter")
{
	World world;
	if (!world.load("Ennorath scenario AISpecialPowerUpdate formation")) return;
	structtest::Arena arena(world.shared, "FactionMen", "FactionMordor");
	Object *horde = world.place(arena, "GondorFighterHorde", 0, 400, 400);
	REQUIRE(horde);
	ContainModuleInterface *c = horde->getContain();
	REQUIRE(c);
	HordeContain *hc = dynamic_cast<HordeContain *>(c);
	REQUIRE(hc);
	const CommandButton *btn = TheCommandStore->findCommandButton("Command_ToggleFormationGondorFighter");
	REQUIRE(btn);
	const unsigned long long before = ObjectDoCommandButton::stats().formationToggles;
	horde->doCommandButton(btn, CMD_FROM_SCRIPT, false);
	CHECK(ObjectDoCommandButton::stats().formationToggles == before + 1);
}

TEST_CASE("ENNORATH scenario AISpecialPowerUpdate: Command_AttackMove sets attack-move state on unit")
{
	World world;
	if (!world.load("Ennorath scenario AISpecialPowerUpdate attack-move")) return;
	structtest::Arena arena(world.shared, "FactionMen", "FactionMordor");
	Object *fighter = world.place(arena, "GondorFighter", 0, 400, 400);
	REQUIRE(fighter);
	AIUpdateInterface *ai = fighter->getAIUpdateInterface();
	REQUIRE(ai);
	const CommandButton *btn = TheCommandStore->findCommandButton("Command_AttackMove");
	REQUIRE(btn);
	const unsigned long long before = ObjectDoCommandButton::stats().attackMoves;
	fighter->doCommandButtonAtPosition(btn, Coord3D{ 600, 600, 0 }, CMD_FROM_SCRIPT, false);
	CHECK(ObjectDoCommandButton::stats().attackMoves == before + 1);
	CHECK(ai->attackMoveActive());
}

TEST_CASE("ENNORATH scenario DoCommandUpgrade: Command_MountToggleBlackrider on BlackRiderHorde toggles formation")
{
	World world;
	if (!world.load("Ennorath scenario DoCommandUpgrade Blackrider")) return;
	structtest::Arena arena(world.shared, "FactionMordor", "FactionMen");
	Object *horde = world.place(arena, "MordorSchwarzeReiterHorde", 0, 400, 400);
	REQUIRE(horde);
	HordeContain *hc = dynamic_cast<HordeContain *>(horde->getContain());
	REQUIRE(hc);
	const ThingTemplate *oldAlt = hc->alternateFormationTemplate();
	REQUIRE(oldAlt);
	const CommandButton *btn = TheCommandStore->findCommandButton("Command_MountToggleBlackrider");
	REQUIRE(btn);
	const unsigned long long before = ObjectDoCommandButton::stats().formationToggles;
	DoCommandUpgrade::doCommandButton(*horde, *btn);
	CHECK(ObjectDoCommandButton::stats().formationToggles == before + 1);
	HordeContain *newHc = dynamic_cast<HordeContain *>(horde->getContain());
	REQUIRE(newHc != nullptr);
	CHECK(newHc != hc);
	CHECK(newHc->alternateFormationTemplate() != oldAlt);
}

// Subsystem 5 (TheFireLogicSystem, lane FIRE): Carthaen torch and fire arrows ignite grass,
// fire decays, Freezing Rain extinguishes ground fire, and PorterFireExtinguisher puts out fires.
// RW 0x6886E0, 0x6878D1, 0x687C4C, 0x912241, 0x8C918D.
TEST_CASE("ENNORATH scenario FireLogic: Carthaen torch and fire arrows ignite grass and fire decays")
{
	World world;
	if (!world.load("Ennorath scenario FireLogic torch and arrows")) return;
	structtest::Arena arena(world.shared, "FactionMen", "FactionMordor");

	// data\ini\includes\weapon.inc: Weapon CarthaenTorchWeapon (INCREASE_BURN_RATE 15, INCREASE_FUEL 30)
	// data\ini\weapon.ini: Weapon GondorArcherBowBombardFireWarhead (INCREASE_BURN_RATE)
	FireLogicSystem &fire = arena.logic.fireLogic();
	fire.newMap(0.0f, 0.0f, 1000.0f, 1000.0f);

	const WeaponTemplate *torch = TheWeaponStore ? TheWeaponStore->findWeaponTemplate("CarthaenTorchWeapon") : nullptr;
	REQUIRE(torch != nullptr);

	const Coord3D at{ 500.0f, 500.0f, 0.0f };
	CHECK(!fire.isCellBurning(50, 50));

	unsigned long long unported = 0;
	DeliverNuggets(arena.logic, INVALID_ID, *torch, WeaponBonus{}, nullptr, &at, false, &unported);
	CHECK(unported == 0u);

	// Cell under torch should now be burning
	CHECK(fire.isCellBurning(50, 50));
	CHECK(fire.getCellBurnRate(50, 50) == 15); // CarthaenTorchWeapon INCREASE_BURN_RATE 15

	// Gondor fire arrows warhead at (520, 500)
	const WeaponTemplate *fireArrow = TheWeaponStore ? TheWeaponStore->findWeaponTemplate("GondorArcherBowBombardFireWarhead") : nullptr;
	REQUIRE(fireArrow != nullptr);

	const Coord3D atArrow{ 520.0f, 500.0f, 0.0f };
	DeliverNuggets(arena.logic, INVALID_ID, *fireArrow, WeaponBonus{}, nullptr, &atArrow, false, &unported);
	CHECK(unported == 0u);
	CHECK(fire.isCellBurning(52, 50));

	// Let fire burn and consume fuel over frames until it decays
	for (int i = 0; i < 60; ++i)
	{
		fire.update();
	}
	// tolerance: fuel drains from 130 as slices process burning frames
	CHECK(fire.getCellFuel(50, 50) < 50);
}

TEST_CASE("ENNORATH scenario FireLogic: Freezing Rain extinguishes all active fires across map")
{
	World world;
	if (!world.load("Ennorath scenario FireLogic Freezing Rain")) return;
	structtest::Arena arena(world.shared, "FactionIsengard", "FactionMen");

	// data\ini\specialpower.ini: SpecialPower SpellBookFreezingRain
	// data\ini\object\evilfaction\units\isengard\saruman.ini: Behavior = FreezingRainSpecialPower (BurnRateModifier = -100)
	// RW 0x8C918D: FreezingRainSpecialPower calls fireLogic().changeBurnRateAll(m_data->m_burnRateModifier) (RW 0x687C4C)
	FireLogicSystem &fire = arena.logic.fireLogic();
	fire.newMap(0.0f, 0.0f, 1000.0f, 1000.0f);

	Coord3D pos1{ 200.0f, 200.0f, 0.0f };
	Coord3D pos2{ 700.0f, 800.0f, 0.0f };
	fire.changeBurnRateInArea(pos1, 20.0f, 25, false);
	fire.changeBurnRateInArea(pos2, 20.0f, 25, false);

	for (int i = 0; i < 10; ++i)
	{
		fire.update();
	}

	CHECK(fire.anyBurning());
	CHECK(fire.isCellBurning(20, 20));
	CHECK(fire.isCellBurning(70, 80));

	// Freezing Rain strikes: changeBurnRateAll(-100) per specialpower.ini BurnRateModifier = -100
	fire.changeBurnRateAll(-100);

	CHECK(!fire.anyBurning());
	CHECK(!fire.isCellBurning(20, 20));
	CHECK(!fire.isCellBurning(70, 80));
}

TEST_CASE("ENNORATH scenario FireLogic: Command_PorterExtinguishFire delivers PorterFireExtinguisher")
{
	World world;
	if (!world.load("Ennorath scenario FireLogic Porter Extinguish")) return;
	structtest::Arena arena(world.shared, "FactionMen", "FactionMordor");

	// data\ini\commandbutton.ini: Command_PorterExtinguishFire
	// data\ini\object\includes\porterextinguishfire.inc: WeaponFireSpecialAbilityUpdate (PorterFireExtinguisher)
	// data\ini\weapon.ini: Weapon PorterFireExtinguisher (DECREASE_BURN_RATE)
	FireLogicSystem &fire = arena.logic.fireLogic();
	fire.newMap(0.0f, 0.0f, 1000.0f, 1000.0f);

	const Coord3D targetPos{ 450.0f, 450.0f, 0.0f };
	fire.changeBurnRateInArea(targetPos, 15.0f, 30, false);
	CHECK(fire.isCellBurning(45, 45));

	const WeaponTemplate *extinguisher = TheWeaponStore ? TheWeaponStore->findWeaponTemplate("PorterFireExtinguisher") : nullptr;
	REQUIRE(extinguisher != nullptr);

	unsigned long long unported = 0;
	DeliverNuggets(arena.logic, INVALID_ID, *extinguisher, WeaponBonus{}, nullptr, &targetPos, false, &unported);
	CHECK(unported == 0u);

	// Fire in that area should now be extinguished
	CHECK(!fire.isCellBurning(45, 45));
}

// TOP20 rampage on the mod's own data (PROCESS 15.11). ScriptEvents.xml of the mod: MumakilFunctions has ReallyDamagedAndAflame (+REALLYDAMAGED +AFLAME) ->
// GoIntoRampage (Scripts.lua: ObjectEnterRampageState); mumakil.ini: MordorMumakil RampageTime 7960 (40 frames), RampageRequiresAflame Yes.
// TrollFunctions lists ReallyDamaged twice (GoIntoRampage, then OnHeroInjured): the second replaces the first (RW 0x733FAF finds the event's record and
// assigns the new function), so the mod's trolls (MordorTrollupgrade: TrollFunctions, RampageTime 4470) never rampage
TEST_CASE("ENNORATH scenario AIRampageState: an aflame, badly hurt MordorMumakil rampages through the mod's Lua; a badly hurt MordorTrollupgrade does not (its handler is replaced)")
{
	World world;
	if (!world.load("Ennorath scenario AIRampageState")) return;
	hudtest::Rig game(world.shared);
	std::string error;
	Object *mumak = game.game->createObject("MordorMumakil", game.index(), Coord3D{ 1200.0f, 1200.0f, 0.0f }, 0.0f, &error);
	REQUIRE_MESSAGE(mumak != nullptr, error);
	Object *troll = game.game->createObject("MordorTrollupgrade", game.index(), Coord3D{ 1600.0f, 1200.0f, 0.0f }, 0.0f, &error);
	REQUIRE_MESSAGE(troll != nullptr, error);
	game.logic().runLogicFrame();
	game.logic().runLogicFrame();
	AIUpdateInterface *ai = mumak->getAIUpdateInterface();
	AIUpdateInterface *trollAi = troll->getAIUpdateInterface();
	REQUIRE(ai != nullptr);
	REQUIRE(trollAi != nullptr);
	CHECK(ai->combatSettings().rampageTime == 40u);
	CHECK(trollAi->combatSettings().rampageTime == 23u);
	// set aflame (status 10 for getRampageTime, the model condition for the event) and down to 20%: REALLYDAMAGED
	mumak->setStatus((unsigned)CombatNames::status("AFLAME"), true);
	mumak->setModelConditionState(ModelCondition::indexOf("AFLAME"), true);
	damage(mumak, INVALID_ID, DAMAGE_UNRESISTABLE, health(mumak) * 0.8f);
	damage(troll, INVALID_ID, DAMAGE_UNRESISTABLE, health(troll) * 0.8f);
	REQUIRE(troll->testModelCondition(ModelCondition::indexOf("REALLYDAMAGED")));
	bool rampaged = false, trollRampaged = false;
	for (int i = 0; i < 10; ++i)
	{
		game.logic().runLogicFrame();
		rampaged = rampaged || ai->currentStateId() == 0x2Du;
		trollRampaged = trollRampaged || trollAi->currentStateId() == 0x2Du;
	}
	REQUIRE(rampaged); // the mod's Lua called ObjectEnterRampageState
	CHECK(mumak->testModelCondition(ModelCondition::indexOf("WEAPONSET_RAMPAGE"))); // what GoIntoRampage tests before it broadcasts BeAfraidOfRampage
	CHECK_FALSE(trollRampaged);
}

// TOP20 #21: the burning death on the mod's objects. GondorFighter (data\ini\object\goodfaction\units\men\gondorfighter.ini: ActiveBody
// BurningDeathBehavior = Yes, BurningDeathFX = FX_InfantryBurningFlame; AIUpdateInterface BurningDeathTime = BURNINGDEATH_DURATION_INFANTRY,
// data\ini\_gamedata.inc #define 5000 ms = ceil(5000 * 0.005) = 25 frames); WildDrogothnew's DrogothGroundFireBreath (data\ini\includes\weapon.inc:
// two FLAME and two MAGIC DamageNuggets)
TEST_CASE("ENNORATH scenario AIBurningDeathState: a GondorFighter hit by a fatal FLAME blow burns for BURNINGDEATH_DURATION_INFANTRY (25 frames) at 1 health, then dies; the dragon's breath sets the horde burning")
{
	World world;
	if (!world.load("Ennorath scenario AIBurningDeathState")) return;
	hudtest::Rig game(world.shared);
	const int enemy = game.game->players().findPlayerWithName("Player_2")->getPlayerIndex();
	std::string error;
	Object *horde = game.game->createObject("GondorFighterHorde", game.index(), Coord3D{ 1200.0f, 1200.0f, 0.0f }, 0.0f, &error);
	REQUIRE_MESSAGE(horde != nullptr, error);
	Object *dragon = game.game->createObject("WildDrogothnew", enemy, Coord3D{ 1260.0f, 1200.0f, 0.0f }, 0.0f, &error);
	REQUIRE_MESSAGE(dragon != nullptr, error);
	for (int i = 0; i < 5; ++i) game.logic().runLogicFrame();
	REQUIRE(horde->getContain());
	std::vector<Object *> members(horde->getContain()->getContainedItemsList()->begin(), horde->getContain()->getContainedItemsList()->end());
	REQUIRE(members.size() == 15u); // data/ini/object/goodfaction/hordes/men/menhordes.ini: InitialPayload = GondorFighter GOOD_MEN_GIANT_HORDE_SIZE (15)
	Object *m0 = members[0];
	AIUpdateInterface *a0 = m0->getAIUpdateInterface();
	REQUIRE(a0 != nullptr);
	CHECK(a0->combatSettings().burningDeathTime == 25u); // the macro resolved (it read 0 before: every burning unit died the next frame)
	damage(m0, dragon->getID(), DAMAGE_FLAME, 5000.0f);
	CHECK(health(m0) == 1.0f);
	CHECK(a0->currentStateId() == 0x4Au);
	CHECK(m0->getContainedBy() == nullptr); // out of its horde (RW 0x8C4779)
	for (int f = 0; f < 24; ++f) game.logic().runLogicFrame();
	CHECK_FALSE(m0->isEffectivelyDead());
	CHECK(a0->currentStateId() == 0x4Au);
	for (int f = 0; f < 3; ++f) game.logic().runLogicFrame();
	CHECK(m0->isEffectivelyDead());
	// the breath: members its FLAME nuggets would kill burn instead of dying at once
	const WeaponTemplate *breath = TheWeaponStore ? TheWeaponStore->findWeaponTemplate("DrogothGroundFireBreath") : nullptr;
	REQUIRE(breath != nullptr);
	int burningAlive = 0;
	for (int shot = 0; shot < 20 && burningAlive == 0; ++shot)
	{
		const Coord3D at = *members[1]->getPosition();
		unsigned long long unported = 0;
		DeliverNuggets(game.logic(), dragon->getID(), *breath, WeaponBonus{}, nullptr, &at, false, &unported);
		for (int i = 0; i < 5; ++i) game.logic().runLogicFrame();
		for (Object *m : members)
		{
			if (m != m0 && !m->isEffectivelyDead() && m->testModelCondition(CombatNames::modelCondition("BURNINGDEATH")))
			{
				++burningAlive;
			}
		}
	}
	CHECK(burningAlive > 0); // tolerance: how many of the horde the breath reaches depends on the members' places; one burning survivor proves the path
}
