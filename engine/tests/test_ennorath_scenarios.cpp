// Ennorath scenarios: what the owner would otherwise check by hand, on the mod's own objects (docs/ennorath/PROCESS.md, section 8). GPL-3.0.
// A case named "ENNORATH scenario <Class>: ..." takes that class off the owner's list (tools/mod/ennorath_queue.py checklist).
// The owner still checks what is seen and heard. Expected values come from the INI file each case names.
#include "EnnorathScenario.h"
#include "Common/Money.h"
#include "Common/Audio/AudioIni.h"
#include "GameClient/EvaAnnounceClientCreate.h"
#include "GameClient/ClientEvents.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/Upgrade.h"
#include "GameLogic/Damage.h"
#include "GameLogic/Economy.h"
#include "GameLogic/Module/GiveUpgradeUpdate.h"
#include "GameLogic/Module/ReflectDamage.h"
#include "GameLogic/Module/SalvageCrateCollide.h"

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
