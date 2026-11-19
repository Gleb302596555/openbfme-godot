// PORT-277: first check the existing main path before changing the engine.
// Initial synthetic cases. RW 0x911A58: a horde locks LockWeaponSlot temporarily,
// then orders members through HordeContainInterface vtable +0x04 (RW 0x875221).
// A single firer without a contain returns without an attack (RW 0x911A58, contain check at +0x1E).
#include "doctest.h"
#include "CombatTestUtil.h"
#include "GameLogic/Object/Contain/HordeContainRuntime.h"

using namespace combattest;
namespace
{
const char kHit[] = R"ini(
Weapon P277Hit
AttackRange = 400
DelayBetweenShots = 10000
DamageNugget
	Damage = 7
	DamageType = MAGIC
	DeathType = NORMAL
End
End
)ini";
const char kPort277[] =
	"Weapon P277Rangefinder\n"
	"  AttackRange = 150\n"
	"  DelayBetweenShots = 1000\n"
	"  LeechRangeWeapon = Yes\n"
	"  HordeAttackNugget\n"
	"    LockWeaponSlot = SECONDARY\n"
	"  End\n"
	"End\n"
	"Object P277Archer\n"
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK SCORE\n"
	"  VisionRange = 250\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = PlainArmor\n"
	"  End\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY BowWeapon\n"
	"    Weapon = SECONDARY P277Hit\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 60\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = DestroyDie ModuleTag_Destroy\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"End\n"
	"Object P277ArcherHorde\n"
	"  KindOf = HORDE LARGE_RECTANGLE_PATHFIND SELECTABLE CAN_ATTACK\n"
	"  VisionRange = 250\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 30\n"
	"  GeometryMinorRadius = 45\n"
	"  GeometryHeight = 20\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY P277Rangefinder\n"
	"  End\n"
	"  Body = ImmortalBody ModuleTag_Body\n"
	"    MaxHealth = 1\n"
	"  End\n"
	"  Behavior = HordeAIUpdate ModuleTag_AI\n"
	"  End\n"
	"  Behavior = HordeContain ModuleTag_Contain\n"
	"    RankInfo = RankNumber:1 UnitType:P277Archer Position:X:50 Y:0 Position:X:50 Y:20 Position:X:50 Y:-20\n"
	"    RankInfo = RankNumber:2 UnitType:P277Archer Position:X:30 Y:0 Position:X:30 Y:20 Position:X:30 Y:-20\n"
	"    InitialPayload = P277Archer 6\n"
	"    RanksToReleaseWhenAttacking = 1 2\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = HordeLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 50\n"
	"  End\n"
	"End\n";
} // namespace

TEST_CASE("PORT-277: existing rangefinder orders members to fire SECONDARY and damage the target")
{
	std::string ini = std::string(kHit) + kPort277;
	SUBCASE("member weapon has a direct DamageNugget") {}
	SUBCASE("member weapon has only a ProjectileNugget; damage is in its warhead")
	{
		const std::string assignment = "Weapon = SECONDARY P277Hit";
		const auto at = ini.find(assignment);
		REQUIRE(at != std::string::npos);
		ini.replace(at, assignment.size(), "Weapon = SECONDARY Arc20");
	}
	CombatWorld w(ini.c_str());
	w.combat().setAutoAcquireEnabled(false);
	Object *horde = w.unit("P277ArcherHorde", 'A', 300, 300);
	Object *target = w.unit("Dummy", 'B', 430, 300);
	w.frames(12);
	const auto members = w.membersOf(horde);
	REQUIRE(members.size() == 6);
	const auto *rangefinder = w.stores.weapons().findWeaponTemplate("P277Rangefinder");
	REQUIRE(rangefinder != nullptr);
	const float health = w.health(target);
	// No ordinary attack order to the horde: only the ability's extra weapon fires.
	ObjectWeapons::createAndFireTempWeaponAt(rangefinder, horde, *target);
	CHECK(w.hordeOf(horde)->attackStats().orders == 6);
	CHECK(w.health(target) == health); // the rangefinder has no damage of its own
	for (Object *m : members)
	{
		REQUIRE(m->getWeapons() != nullptr);
		CHECK(m->getWeapons()->curSlot() == 1);
		CHECK(m->getWeapons()->isCurWeaponLocked());
	}
	// tolerance: timeout only; direct hits and projectile flight have different arrival frames.
	CHECK(w.runUntil([&] { return w.health(target) < health; }, 100) < 100);
	// tolerance: stop at the first damaging frame; one or more of six shots can arrive together.
	CHECK(w.health(target) <= health - 7.0f);
}

TEST_CASE("PORT-277: single firer with no HordeContain does not turn the rangefinder into a damaging weapon")
{
	const std::string ini = std::string(kHit) + kPort277;
	CombatWorld w(ini.c_str());
	w.combat().setAutoAcquireEnabled(false);
	Object *single = w.unit("P277Archer", 'A', 300, 300);
	Object *target = w.unit("Dummy", 'B', 430, 300);
	w.frames(12);
	REQUIRE(single->getContain() == nullptr);
	const auto *rangefinder = w.stores.weapons().findWeaponTemplate("P277Rangefinder");
	REQUIRE(rangefinder != nullptr);
	const float health = w.health(target);
	ObjectWeapons::createAndFireTempWeaponAt(rangefinder, single, *target);
	w.frames(20);
	CHECK(w.health(target) == health);
	CHECK(single->getWeapons()->curSlot() == 0);
	CHECK_FALSE(single->getWeapons()->isCurWeaponLocked());
}

// RW 0x911A58 / 0x911AC9 read LockWeaponSlot at nugget +0x14C and call
// 0x69121A before dispatch. The previous test defined P277Dummy AFTER its
// reference: RW 0x73AE79 -> 0x6CC5DF binds a template immediately, not lazily.
// Define it first and require the bound slot before checking the lock.
TEST_CASE("PORT-277: a dummy member weapon never borrows damage from PRIMARY or the rangefinder")
{
	std::string ini = "Weapon P277Dummy\n AttackRange = 400\n NoVictimNeeded = Yes\n End\n" + std::string(kHit) + kPort277;
	const std::string assignment = "Weapon = SECONDARY P277Hit";
	const auto at = ini.find(assignment);
	REQUIRE(at != std::string::npos);
	ini.replace(at, assignment.size(), "Weapon = SECONDARY P277Dummy");
	CombatWorld w(ini.c_str());
	w.combat().setAutoAcquireEnabled(false);
	Object *source = w.unit("P277ArcherHorde", 'A', 300, 300);
	Object *target = w.unit("Dummy", 'B', 430, 300);
	w.frames(12);
	for (Object *m : w.membersOf(source))
	{
		const Weapon *dummy = m->getWeapons()->weaponInSlot(SECONDARY_WEAPON);
		REQUIRE(dummy != nullptr);
		CHECK(dummy->getTemplate()->m_nuggets.empty());
	}
	const float health = w.health(target);
	const auto *weapon = w.stores.weapons().findWeaponTemplate("P277Rangefinder");
	REQUIRE(weapon != nullptr);
	SUBCASE("object target")
	{
		ObjectWeapons::createAndFireTempWeaponAt(weapon, source, *target);
		CHECK(w.hordeOf(source)->attackStats().orders == 6);
	}
	SUBCASE("position target")
	{
		ObjectWeapons::createAndFireTempWeapon(weapon, source, *target->getPosition());
		CHECK(w.hordeOf(source)->attackStats().positionOrders == 6);
	}
	for (Object *member : w.membersOf(source)) CHECK(member->getWeapons()->curSlot() == 1);
	w.frames(100);
	CHECK(w.health(target) == health);
}

// Preserve the counterexample that diagnosed S-7474: the later definition
// exists in the store but cannot retroactively repair an earlier binding.
TEST_CASE("PORT-277: late weapon definition leaves an unbound slot before the rangefinder fires")
{
	std::string ini = std::string(kHit) + kPort277;
	const std::string assignment = "Weapon = SECONDARY P277Hit";
	ini.replace(ini.find(assignment), assignment.size(), "Weapon = SECONDARY P277Late");
	ini += "Weapon P277Late\n AttackRange = 400\n End\n";
	CombatWorld w(ini.c_str());
	w.combat().setAutoAcquireEnabled(false);
	Object *source = w.unit("P277ArcherHorde", 'A', 300, 300);
	REQUIRE(w.stores.weapons().findWeaponTemplate("P277Late") != nullptr);
	w.frames(12);
	const auto members = w.membersOf(source);
	REQUIRE(members.size() == 6);
	for (Object *member : members)
	{
		CHECK(member->getWeapons()->weaponInSlot(SECONDARY_WEAPON) == nullptr);
		CHECK(member->getWeapons()->curSlot() == PRIMARY_WEAPON);
	}
}

// Synthetic causal shapes, authored here (not extracted INI blocks). Names,
// formation size, target armour and damage sentinels are test scaffolding.
// Slot, power binding, projectile/warhead, paused unlock and OCL delivery
// shapes come from the earlier INI 1.9.8.3 audit; RW dispatch is documented
// in docs/ennorath/PORT-277.md. No install, archive or environment is read.
#include "Common/SpecialPower.h"
#include "GameClient/ControlBarCommands.h"
#include "GameLogic/ActionManager.h"
#include "GameLogic/Module/HeroModules.h"
#include "GameLogic/Module/SpecialAbilityModules.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/Module/SpellEffectModules.h"
#include "GameLogic/ObjectCreationList.h"
#include "GameLogic/WeaponNugget.h"
#include <sstream>

namespace port277buttons
{
struct Shape
{
	const char *name;
	const char *slot;
	int slotIndex;
	bool horde;
	bool mismatched;
	bool paused;
	bool dummy;
	int ocl; // 0 none, 1 volley, 2 egg -> delayed death -> projectile
	int range;
	int preAttack;
};
const Shape shapes[] = {
	{ "IsenfurtWchter", "TERTIARY", TERTIARY_WEAPON, false, false, false, false, 0, 420, 500 },
	{ "IsenfurtWchterCreep", "TERTIARY", TERTIARY_WEAPON, false, false, false, false, 0, 420, 500 },
	{ "IsenfurtWchterHorde", "TERTIARY", TERTIARY_WEAPON, true, false, false, false, 0, 420, 500 },
	{ "IsenfurtWchterHordeHerold", "TERTIARY", TERTIARY_WEAPON, true, true, false, false, 0, 420, 500 },
	{ "DwarvenAxtschwingerHorde", "TERTIARY", TERTIARY_WEAPON, true, false, false, false, 0, 250, 1500 },
	{ "AngmarDarkRangerHorde", "QUINARY", QUINARY_WEAPON, true, false, true, false, 0, 450, 400 },
	{ "GondorRangerHorde", "QUINARY", QUINARY_WEAPON, true, false, false, true, 1, 400, 0 },
	{ "DwarvenDain_mod", "QUINARY", QUINARY_WEAPON, false, false, false, true, 2, 400, 0 },
};

std::string modules(const Shape &s, bool member = false)
{
	const bool ocl = s.ocl && !member;
	const std::string power = s.mismatched && !member ? "P277Undefined" : "P277Power";
	std::ostringstream out;
	out << "Behavior = " << (ocl ? "OCLSpecialPower" : "SpecialPowerModule") << " Starter\n"
		<< "SpecialPowerTemplate = " << power << "\n"
		<< "UpdateModuleStartsAttack = " << (ocl ? "No" : "Yes") << "\nStartsPaused = " << (s.paused ? "Yes" : "No") << "\n";
	if (ocl) out << "OCL = P277Delivery\nCreateLocation = CREATE_AT_LOCATION\n";
	out << "End\nBehavior = WeaponFireSpecialAbilityUpdate Ability\n"
		<< "SpecialPowerTemplate = " << power << "\nSpecialWeapon = P277RF\n";
	if (s.dummy) out << "NeedLivingTargets = No\n";
	if (s.paused) out << "PackTime = 3100\nStartAbilityRange = 400\nApproachUntilMembersInRange = Yes\n";
	out << "End\n";
	if (s.paused) out << "Behavior = UnpauseSpecialPowerUpgrade Unlock\n"
		<< "SpecialPowerTemplate = P277Power\nTriggeredBy = Upgrade_ObjectLevel1\nEnd\n";
	return out.str();
}

std::string ini(const Shape &s)
{
	std::ostringstream out;
	// All definitions precede references that bind eagerly (RW 0x73AE79).
	out << "Upgrade Upgrade_ObjectLevel1\nType = OBJECT\nEnd\n"
		<< "SpecialPower P277Power\nEnum = SPECIAL_AT_VISIBLE_OBJECT\nReloadTime = 0\nEnd\n"
		<< "CommandButton P277Button\nCommand = SPECIAL_POWER\nSpecialPower = P277Power\nEnd\n"
		<< "CommandSet P277Commands\n1 = P277Button\nEnd\n"
		<< "Weapon P277RF\nAttackRange = " << s.range << "\nHordeAttackNugget\nLockWeaponSlot = " << s.slot << "\nEnd\nEnd\n"
		<< "Weapon P277Warhead\nHitStoredTarget = Yes\nRadiusDamageAffects = ENEMIES\n";
	// Causal member-weapon values recorded from rohanwestfoldwchter.ini /
	// dwarvenaxtschwinger.ini / angmarranger.ini and their weapon includes.
	if (!s.dummy)
	{
		if (s.paused) out << "DamageNugget\nDamage = 50\nDamageType = FROST\nDeathType = NORMAL\nEnd\n"
			<< "DamageNugget\nDamage = 150\nDamageType = PIERCE\nDeathType = NORMAL\nEnd\n";
		else out << "DamageNugget\nDamage = 100\nDamageType = " << (s.preAttack == 1500 ? "SLASH" : "CAVALRY_RANGED")
			<< "\nDeathType = NORMAL\nEnd\n";
	}
	out << "End\nWeapon P277Shot\nAttackRange = " << s.range << "\n";
	if (!s.dummy) out << "WeaponSpeed = " << (s.paused ? 351 : 161)
		<< "\nPreAttackDelay = " << s.preAttack << "\nFiringDuration = " << (s.paused ? 0 : 1600) << "\n"
		<< "ProjectileNugget\nProjectileTemplateName = Arrow\nWarheadTemplateName = P277Warhead\nEnd\n";
	out << "End\n";
	if (s.ocl)
	{
		// Damage=7/radius=25 are sentinels, not a claim about mod balance.
		// The real causal paths use FireWeaponUpdate at 4700ms (volley),
		// or 0-lifetime egg / 4000ms slow death / 600ms projectile shot.
		out << "Weapon P277Area\nRadiusDamageAffects = ENEMIES\nDamageNugget\n"
			<< "Damage = 7\nRadius = 25\nDamageType = MAGIC\nDeathType = NORMAL\nEnd\nEnd\n"
			<< "ObjectCreationList P277Delivery\nCreateObject\nObjectNames = "
			<< (s.ocl == 1 ? "P277Volley" : "P277Egg") << "\nCount = 1\nEnd\nEnd\n"
			<< "ObjectCreationList P277ProjectileList\nCreateObject\nObjectNames = P277Volley\nCount = 1\nEnd\nEnd\n"
			<< "Object P277Volley\nKindOf = IMMOBILE UNATTACKABLE INERT\n"
			<< "Body = ActiveBody Body\nMaxHealth = 99999\nEnd\n"
			<< "Behavior = FireWeaponUpdate Fire\nFireWeaponNugget\nWeaponName = P277Area\nFireDelay = "
			<< (s.ocl == 1 ? 4700 : 600) << "\nOneShot = Yes\nEnd\nEnd\nEnd\n"
			<< "Object P277Egg\nKindOf = IMMOBILE UNATTACKABLE INERT\n"
			<< "Body = ActiveBody Body\nMaxHealth = 99999\nEnd\n"
			<< "Behavior = LifetimeUpdate Life\nMinLifetime = 0\nMaxLifetime = 0\nEnd\n"
			<< "Behavior = SlowDeathBehavior Death\nDeathTypes = ALL\nDestructionDelay = 4000\n"
			<< "OCL = FINAL P277ProjectileList\nEnd\nEnd\n";
	}
	out << "Object P277Member\nKindOf = INFANTRY SELECTABLE CAN_ATTACK\nVisionRange = 500\n"
		<< "Geometry = CYLINDER\nGeometryMajorRadius = 8\nGeometryMinorRadius = 8\nGeometryHeight = 20\n"
		<< "Body = ActiveBody Body\nMaxHealth = 60\nEnd\n"
		<< "WeaponSet\nConditions = None\nWeapon = PRIMARY BowWeapon\nWeapon = " << s.slot << " P277Shot\nEnd\n"
		<< "Behavior = AIUpdateInterface AI\nEnd\n"
		<< "LocomotorSet\nLocomotor = WalkerLoco\nCondition = SET_NORMAL\nSpeed = 55\nEnd\n"
		<< "CommandSet = P277Commands\n" << modules(s, s.horde) << "End\n"
		<< "Object P277Horde\nKindOf = HORDE SELECTABLE CAN_ATTACK LARGE_RECTANGLE_PATHFIND"
		<< (!s.dummy && !s.paused ? " MELEE_HORDE" : "") << "\nVisionRange = 500\n"
		<< "Body = ImmortalBody Body\nMaxHealth = 1\nEnd\n"
		<< "WeaponSet\nConditions = None\nWeapon = PRIMARY P277RF\nWeapon = " << s.slot << " P277RF\nEnd\n"
		<< "Behavior = HordeAIUpdate AI\nEnd\n"
		<< "LocomotorSet\nLocomotor = HordeLoco\nCondition = SET_NORMAL\nSpeed = 50\nEnd\n"
		<< "Behavior = HordeContain Contain\nInitialPayload = P277Member 3\n"
		<< "RankInfo = RankNumber:1 UnitType:P277Member Position:X:50 Y:0 Position:X:50 Y:20 Position:X:50 Y:-20\n"
		<< "RanksToReleaseWhenAttacking = 1\nEnd\nCommandSet = P277Commands\n" << modules(s) << "End\n";
	return out.str();
}

struct World : CombatWorld
{
	SpecialPowerStore powers;
	SpecialPowerStore *savedPowers = TheSpecialPowerStore;
	CommandStore buttons;
	CommandStore *savedButtons = TheCommandStore;
	ObjectCreationListStore ocls{ w.keys };
	ObjectCreationListStore *savedOcls = TheObjectCreationListStore;
	explicit World(const Shape &s)
	{
		combat().setAutoAcquireEnabled(false);
		TheSpecialPowerStore = &powers;
		TheCommandStore = &buttons;
		TheObjectCreationListStore = &ocls;
		buttons.setThingFactory(&w.things);
		w.fx.env.blocks.registerBlock("SpecialPower", [](INI *i) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(i); });
		w.fx.env.blocks.registerBlock("CommandButton", [](INI *i) { CommandStore::parseCommandButtonDefinitionGlobal(i); });
		w.fx.env.blocks.registerBlock("CommandSet", [](INI *i) { CommandStore::parseCommandSetDefinitionGlobal(i); });
		w.fx.env.blocks.registerBlock("ObjectCreationList", [](INI *i) { ObjectCreationListStore::parseObjectCreationListDefinitionGlobal(i); });
		SpecialAbilityModules::registerAll(w.modules);
		SpecialPowerModules::registerAll(w.modules);
		HeroModules::registerAll(w.modules);
		SpellEffectModules::registerAll(w.modules);
		const std::string error = w.load(ini(s), INI_LOAD_OVERWRITE, "port277-synthetic.ini");
		REQUIRE_MESSAGE(error.empty(), error);
	}
	~World()
	{
		logic->reset();
		TheObjectCreationListStore = savedOcls;
		TheCommandStore = savedButtons;
		TheSpecialPowerStore = savedPowers;
	}
};

void check(size_t index)
{
	const Shape &s = shapes[index];
	INFO(s.name);
	World w(s);
	Object *source = w.unit(s.horde ? "P277Horde" : "P277Member", 'A', 300, 300);
	Object *target = w.unit("Dummy", 'B', 450, 300);
	w.frames(12);
	const auto *button = w.buttons.findCommandButton("P277Button");
	REQUIRE(button != nullptr);
	const auto *power = w.powers.findSpecialPowerTemplate(button->m_specialPowerName);
	REQUIRE(power != nullptr);
	const auto *rf = w.stores.weapons().findWeaponTemplate("P277RF");
	REQUIRE(rf != nullptr);
	REQUIRE(rf->m_nuggets.size() == 1);
	const auto *nugget = dynamic_cast<const HordeAttackNugget *>(rf->m_nuggets.front().get());
	REQUIRE(nugget != nullptr);
	CHECK(nugget->m_lockWeaponSlot == s.slotIndex); // RW nugget +0x14C, 0x911A58 / 0x911AC9
	unsigned matching = 0;
	for (const auto &m : source->modules())
	{
		if (const auto *wf = dynamic_cast<const WeaponFireSpecialAbilityUpdate *>(m.get()))
		{
			REQUIRE(wf->ownedWeapon() != nullptr);
			CHECK(wf->ownedWeapon()->getTemplate() == rf);
			CHECK(wf->drivesSpecialPower(power) == !s.mismatched);
			++matching;
		}
	}
	CHECK(matching == 1);
	if (s.mismatched)
	{
		// RW 0x73B22F, 0x68C26D, 0x8969D1, 0x82D925: no matching interface.
		CHECK(w.powers.findSpecialPowerTemplate("P277Undefined") == nullptr);
		CHECK(SpecialPowerModules::findModule(*source, power) == nullptr);
		CHECK_FALSE(ActionManager::canDoSpecialPowerAtObject(*source, target, CMD_FROM_PLAYER, power, 0, false));
	}
	else
	{
		auto *starter = dynamic_cast<SpecialPowerModule *>(SpecialPowerModules::findModule(*source, power));
		REQUIRE(starter != nullptr);
		if (s.paused)
		{
			CHECK(starter->pauseCount() == 1);
			source->giveUpgrade("Upgrade_ObjectLevel1");
			w.frames(2);
			CHECK(starter->pauseCount() == 0);
		}
	}

	std::set<ObjectID> shooters;
	if (s.horde)
	{
		for (Object *member : w.membersOf(source))
		{
			shooters.insert(member->getID());
			const Weapon *weapon = member->getWeapons()->weaponInSlot(s.slotIndex);
			REQUIRE(weapon != nullptr); // references must bind before testing RW's lock
			CHECK(weapon->getTemplate()->getName() == "P277Shot");
			CHECK(weapon->getTemplate()->m_nuggets.empty() == s.dummy);
		}
		CHECK(shooters.size() == 3);
	}
	else CHECK(source->getContain() == nullptr);
	bool memberDamage = false;
	const ObjectID victim = target->getID();
	w.combat().setDamageTrace([&](const Object &v, std::uint32_t attacker, float, float taken) {
		if (v.getID() == victim && taken > 0 && shooters.count(attacker)) memberDamage = true;
	});
	const float health = w.health(target);
	const Coord3D at = *target->getPosition();
	// Boundary agreed with Local 76: execute the already-fired special weapon.
	if (s.dummy) ObjectWeapons::createAndFireTempWeapon(rf, source, at);
	else ObjectWeapons::createAndFireTempWeaponAt(rf, source, *target);
	CHECK(w.health(target) == health); // the rangefinder itself has no damage
	if (s.horde)
	{
		if (s.dummy) CHECK(w.hordeOf(source)->attackStats().positionOrders == shooters.size());
		else CHECK(w.hordeOf(source)->attackStats().orders == shooters.size());
		for (Object *member : w.membersOf(source))
		{
			CHECK(member->getWeapons()->curSlot() == s.slotIndex);
			CHECK(member->getWeapons()->isCurWeaponLocked());
		}
	}
	w.frames(100);
	CHECK(memberDamage == (s.horde && !s.dummy));
	if (!s.horde || s.dummy) CHECK(w.health(target) == health);
	w.combat().setDamageTrace({});
	if (s.ocl)
	{
		auto *ocl = dynamic_cast<OCLSpecialPower *>(SpecialPowerModules::findModule(*source, power));
		REQUIRE(ocl != nullptr);
		bool oclDamage = false;
		w.combat().setDamageTrace([&](const Object &v, std::uint32_t attacker, float, float taken) {
			const Object *owner = w.logic->findObjectByID(attacker);
			if (v.getID() == victim && taken > 0 && owner && owner->getTemplate()->getName() == "P277Volley") oclDamage = true;
		});
		ocl->doSpecialPowerAtLocation(at, 0);
		w.frames(200);
		CHECK(oclDamage); // independent OCL -> FireWeaponUpdate, RW 0x8C75D8 / 0x88F554
		w.combat().setDamageTrace({});
	}
}
} // namespace port277buttons

TEST_CASE("PORT-277 button IsenfurtWchter Smite: no HordeContain") { port277buttons::check(0); }
TEST_CASE("PORT-277 button IsenfurtWchterCreep Smite: no HordeContain") { port277buttons::check(1); }
TEST_CASE("PORT-277 button IsenfurtWchterHorde Smite: TERTIARY projectile warhead") { port277buttons::check(2); }
TEST_CASE("PORT-277 button IsenfurtWchterHordeHerold Smite: binding rejects before working nugget") { port277buttons::check(3); }
TEST_CASE("PORT-277 button DwarvenAxtschwingerHorde Axe Throw: TERTIARY projectile warhead") { port277buttons::check(4); }
TEST_CASE("PORT-277 button AngmarDarkRangerHorde Ice Arrows: unlock and QUINARY projectile warhead") { port277buttons::check(5); }
TEST_CASE("PORT-277 button GondorRangerHorde Long Shot: QUINARY dummy and independent volley") { port277buttons::check(6); }
TEST_CASE("PORT-277 button DwarvenDain_mod Speerfeuer: no horde and independent egg delivery") { port277buttons::check(7); }
