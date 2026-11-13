// OpenBFME. PORT-168 (fork issue 397) tests: the troll's two grab mechanics of RotWK 2.01 —
//   * the eat's heal (SpecialAbilityUpdate's grab finish RW 0x8532F9 .. 0x853331 reading GrabPassengerHealGainPercent, module data + 0x30, parsed by
//     RW 0x42ED6F with no 0.01 scale: the INI number IS the percentage added to the health ratio);
//   * the thrown held passenger (the weapon's ProjectileFilterInContainer, template + 0x120: RW 0x90FA81 .. 0x90FAC9 takes the first matching
//     contained object out of the container — contain slot 0x13C RW 0x866367 — and flies it as the projectile; RW 0x6CD298 makes the weapon's clip
//     read that count instead of its own ammo).
//
// Synthetic data only (the engine's INI reader and field tables, no retail / Ennorath / mod file, value or byte). Every test name begins PORT-168 so
// the lane's narrow filter is -tc="PORT-168*".
//
// Ennorath's data these tests stand for (values are the mod's, the tests use their own the same way):
//   object/evilfaction/units/mordor/mountaintroll.ini — `GrabPassengerHealGainPercent = 50.0` on the grab ability, `EatObjectEntry = MyHealth:99%
//   TargetHealth:100% Filter:NONE +ORC`;
//   object/evilfaction/units/wild/cavetroll.ini — a TransportContain with `TypeTwoForWeaponSet = INFANTRY` and a WeaponSet
//   `Conditions = PASSENGER_TYPE_TWO Weapon = PRIMARY <throw>`; weapon.ini — `ProjectileFilterInContainer = NONE +INFANTRY` with a ProjectileNugget
//   that has no ProjectileTemplateName, the held unit being the projectile.

#include "doctest.h"

#include "CombatTestUtil.h"

#include "Common/Player.h"
#include "Common/SpecialPower.h"
#include "Common/Thing/ThingTemplate.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/Module/ActiveBody.h"
#include "GameLogic/Module/GrabPassengerSpecialPower.h"
#include "GameLogic/Module/ProjectileModules.h"
#include "GameLogic/Module/SpecialAbilityModules.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/Object/Contain/TransportContainRuntime.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/WeaponState.h"

#include <string>

using namespace combattest;

namespace
{
const char kPowers[] = R"ini(
SpecialPower Port168GrabPower
 Enum = SPECIAL_GRAB_PASSENGER
 ReloadTime = 0
End
)ini";

// The grab power is a store record (RW 0x8C94D7 asks TheSpecialPowerStore); the mod's own block for SPECIAL_GRAB_PASSENGER is the same row, so every
// fixture below loads this one before its objects and installs the store for as long as it lives.
struct PowerStore
{
	SpecialPowerStore powers;
	SpecialPowerStore *saved = TheSpecialPowerStore;
	explicit PowerStore(CombatWorld &w)
	{
		TheSpecialPowerStore = &powers;
		w.w.fx.env.blocks.registerBlock("SpecialPower", [](INI *ini) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(ini); });
		std::string err = w.w.load(kPowers, INI_LOAD_OVERWRITE, "port168powers.ini");
		REQUIRE_MESSAGE(err.empty(), err);
	}
	~PowerStore() { TheSpecialPowerStore = saved; }
};

// Port168Eater: the mountain troll's shape cut down to the grab — one slot, the grab ability with GrabPassengerAnimAndDuration (TriggerTime 1400 ms = 7
// frames) and the heal percentage broken out as `healLines` so a test can leave it out (the parser's own default, + 0x30).
std::string eaterObjects(const std::string &healLines)
{
	return std::string("Object Port168Eater\n"
	                   "  KindOf = INFANTRY SELECTABLE\n"
	                   "  TransportSlotCount = 1\n"
	                   "  Geometry = CYLINDER\n"
	                   "  GeometryMajorRadius = 20\n"
	                   "  GeometryMinorRadius = 20\n"
	                   "  GeometryHeight = 40\n"
	                   "  Body = ActiveBody ModuleTag_Body\n"
	                   "    MaxHealth = 2400\n"
	                   "  End\n"
	                   "  Behavior = GrabPassengerSpecialPower ModuleTag_Grab\n"
	                   "    SpecialPowerTemplate = SpecialAbilityGrabPassenger\n"
	                   "    UpdateModuleStartsAttack = Yes\n"
	                   "  End\n"
	                   "  Behavior = SpecialAbilityUpdate ModuleTag_Ability\n"
	                   "    SpecialPowerTemplate = SpecialAbilityGrabPassenger\n"
	                   "    StartAbilityRange = 8.0\n"
	                   "    UnpackTime = 300\n"
	                   "    PreparationTime = 1\n"
	                   "    PersistentPrepTime = 630\n"
	                   "    PackTime = 1000\n"
	                   "    GrabPassengerAnimAndDuration = AnimState:EATING AnimTime:3000 TriggerTime:1400\n")
	       + healLines
	       + "  End\n"
	         "  Behavior = TransportContain ModuleTag_Contain\n"
	         "    Slots = 1\n"
	         "    AllowEnemiesInside = Yes\n"
	         "    AllowAlliesInside = Yes\n"
	         "    ObjectStatusOfContained = UNSELECTABLE\n"
	         "    PassengerFilter = ANY +ORC\n"
	         "  End\n"
	         "End\n"
	         "Object Port168Orc\n"
	         "  KindOf = INFANTRY ORC SELECTABLE\n"
	         "  Geometry = CYLINDER\n"
	         "  GeometryMajorRadius = 8\n"
	         "  GeometryMinorRadius = 8\n"
	         "  GeometryHeight = 20\n"
	         "  ArmorSet\n"
	         "    Conditions = None\n"
	         "    Armor = NoArmor\n"
	         "  End\n"
	         "  Body = ActiveBody ModuleTag_Body\n"
	         "    MaxHealth = 100\n"
	         "  End\n"
	         "End\n");
}

template <class T>
T *moduleOf(Object *o)
{
	for (const auto &m : o->modules())
	{
		if (T *x = dynamic_cast<T *>(m.get()))
		{
			return x;
		}
	}
	return nullptr;
}

SpecialAbilityUpdate *grabAbilityOf(Object &o)
{
	for (const auto &m : o.modules())
	{
		SpecialAbilityUpdate *u = dynamic_cast<SpecialAbilityUpdate *>(m.get());
		if (u && u->abilityData()->m_specialPowerTemplate &&
		    u->abilityData()->m_specialPowerTemplate->getSpecialPowerType() == 0x27)
		{
			return u;
		}
	}
	return nullptr;
}

// the troll eats `victim` through its SPECIAL_GRAB_PASSENGER power with the eat option 0x4000 (what AutoPickUpUpdate's EatObjectEntry passes, RW
// 0x8998D8) and the logic runs to the end of the ability; the returns are the frame the victim left the world (-1: never), the health before the order
// and the health the frame after the ability finished
struct Eaten
{
	float before = 0.0f;
	float after = 0.0f;
	bool gone = false; // the orc is no longer in the world: the contain's eat countdown (RW 0x865A5C) destroyed it
	bool ate = false;
};
Eaten eat(const std::string &healLines)
{
	CombatWorld w(eaterObjects(healLines).c_str());
	const PowerStore store(w);
	w.combat().setAutoAcquireEnabled(false);
	Object *eater = w.unit("Port168Eater", 'A', 200.0f, 200.0f);
	Object *orc = w.unit("Port168Orc", 'A', 260.0f, 200.0f);
	REQUIRE(eater != nullptr);
	REQUIRE(orc != nullptr);
	w.frames(3);
	// the troll arrives damaged: 50% of its 2400 — the health bar shows the heal, and MyHealth:99% is what the mod's entry asks for
	BodyModuleInterface *body = eater->getBodyModule();
	REQUIRE(body != nullptr);
	body->setInitialHealthPercent(50.0f);
	REQUIRE(w.health(eater) == 1200.0f);
	GrabPassengerSpecialPower *grab = moduleOf<GrabPassengerSpecialPower>(eater);
	SpecialAbilityUpdate *ability = grabAbilityOf(*eater);
	REQUIRE(grab != nullptr);
	REQUIRE(ability != nullptr);
	Eaten e;
	e.before = w.health(eater);
	const ObjectID orcId = orc->getID();
	grab->doSpecialPowerAtObject(orc, 0x4000u); // RW 0x8C94D7
	REQUIRE(grab->grabsStarted() == 1);
	e.ate = ability->isActive();
	for (int i = 0; i < 120 && !e.gone; ++i)
	{
		w.frames(1);
		e.gone = w.logic.findObjectByID(orcId) == nullptr;
	}
	w.frames(1);
	e.after = w.health(eater);
	return e;
}
} // namespace

// RW 0xC561C0 parses GrabPassengerHealGainPercent with RW 0x42ED6F: scanReal, store, refuse below 0 — no 0.01 factor (unlike the percent parser). The
// value therefore stays the INI number, and RW 0x853302 adds it straight to `health / max * 100` (RW 0x8532F9 / RW 0x8532FC, the constant 100.0 at
// RW 0xBD88D8): with the mod's 50.0 a troll at half health is set to 100% of its health by body slot 0x58 (RW 0x853331, ActiveBody RW 0x8C47DE).
TEST_CASE("PORT-168 heal: GrabPassengerHealGainPercent is the INI number itself, not a fraction (RW 0xC561C0 / RW 0x42ED6F, RW 0x8532F9)")
{
	const Eaten e = eat("    GrabPassengerHealGainPercent = 50.0\n");
	REQUIRE(e.ate);
	REQUIRE(e.gone);                        // the orc was taken out of the world (OpenContain's eat countdown RW 0x865A5C)
	CHECK(e.before == 1200.0f);             // 50% of 2400
	CHECK(e.after == 2400.0f);              // 50 + 50 = 100%: min(100, ...) at RW 0x853317
}

// The neighbouring behaviour: the field left out is the data's own default (+ 0x30 = 100.0), so the same eat still tops the troll up — the heal is not
// tied to the number being written. And a smaller number heals less: 20.0 gives 70% of the max health.
TEST_CASE("PORT-168 heal: the data's default and a smaller percentage (module data + 0x30, RW 0x853302)")
{
	const Eaten byDefault = eat("");
	REQUIRE(byDefault.ate);
	CHECK(byDefault.after == 2400.0f); // 50 + the default 100, clamped to 100

	const Eaten smaller = eat("    GrabPassengerHealGainPercent = 20.0\n");
	REQUIRE(smaller.ate);
	CHECK(smaller.after == 1680.0f); // 50 + 20 = 70% of 2400
}

namespace
{
// Port168Thrower: the cave troll's shape cut down to the throw — a one-slot TransportContain, the held unit as the weapon's ammo
// (ProjectileFilterInContainer) and a ProjectileNugget with no ProjectileTemplateName. `filter` is the ammo filter, `clipSize` the weapon's clip.
std::string throwerObjects(const std::string &filter, const std::string &nuggetLines)
{
	return std::string("Weapon Port168Throw\n"
	                   "  AttackRange = 200\n"
	                   "  WeaponSpeed = 100\n"
	                   "  HoldDuringReload = Yes\n"
	                   "  DelayBetweenShots = 6100\n"
	                   "  PreAttackDelay = 1400\n"
	                   "  PreAttackType = PER_SHOT\n"
	                   "  FiringDuration = 2000\n"
	                   "  AutoReloadsClip = No\n")
	       + (filter.empty() ? std::string() : "  ProjectileFilterInContainer = " + filter + "\n")
	       + "  ClipSize = 1\n"
	       + "  ProjectileNugget\n"
	       + nuggetLines
	       + "    WarheadTemplateName = Port168ThrowWarhead\n"
	         "  End\n"
	         "End\n"
	         "Weapon Port168ThrowWarhead\n"
	         "  DamageNugget\n"
	         "    Damage = 300\n"
	         "    Radius = 12.0\n"
	         "    DamageType = SIEGE\n"
	         "    DeathType = NORMAL\n"
	         "  End\n"
	         "End\n"
	         "Object Port168Thrower\n"
	         "  KindOf = INFANTRY SELECTABLE\n"
	         "  TransportSlotCount = 2\n"
	         "  Geometry = CYLINDER\n"
	         "  GeometryMajorRadius = 20\n"
	         "  GeometryMinorRadius = 20\n"
	         "  GeometryHeight = 40\n"
	         "  Body = ActiveBody ModuleTag_Body\n"
	         "    MaxHealth = 2400\n"
	         "  End\n"
	         "  WeaponSet\n"
	         "    Conditions = None\n"
	         "    Weapon = PRIMARY SwordWeapon\n"
	         "  End\n"
	         "  WeaponSet\n"
	         "    Conditions = PASSENGER_TYPE_TWO\n"
	         "    Weapon = PRIMARY Port168Throw\n"
	         "  End\n"
	         "  Behavior = TransportContain ModuleTag_Contain\n"
	         "    Slots = 1\n"
	         "    AllowEnemiesInside = Yes\n"
	         "    AllowAlliesInside = Yes\n"
	         "    ObjectStatusOfContained = UNSELECTABLE\n"
	         "    TypeTwoForWeaponSet = INFANTRY\n"
	         "  End\n"
	         "End\n"
	         "Object Port168Goblin\n"
	         "  KindOf = INFANTRY SELECTABLE\n"
	         "  TransportSlotCount = 1\n"
	         "  Geometry = CYLINDER\n"
	         "  GeometryMajorRadius = 8\n"
	         "  GeometryMinorRadius = 8\n"
	         "  GeometryHeight = 20\n"
	         "  ArmorSet\n"
	         "    Conditions = None\n"
	         "    Armor = NoArmor\n"
	         "  End\n"
	         "  Body = ActiveBody ModuleTag_Body\n"
	         "    MaxHealth = 100\n"
	         "  End\n"
	         "  Behavior = DestroyDie ModuleTag_Die\n"
	         "  End\n"
	         "  Behavior = BezierProjectileBehavior ModuleTag_Projectile\n"
	         "    FirstHeight = 24\n"
	         "    SecondHeight = 24\n"
	         "    FirstPercentIndent = 30%\n"
	         "    SecondPercentIndent = 70%\n"
	         "    DieOnImpact = Yes\n"
	         "  End\n"
	         "End\n"
	         "Object Port168Target\n"
	         "  KindOf = INFANTRY SELECTABLE\n"
	         "  Geometry = CYLINDER\n"
	         "  GeometryMajorRadius = 8\n"
	         "  GeometryMinorRadius = 8\n"
	         "  GeometryHeight = 20\n"
	         "  ArmorSet\n"
	         "    Conditions = None\n"
	         "    Armor = NoArmor\n"
	         "  End\n"
	         "  Body = ActiveBody ModuleTag_Body\n"
	         "    MaxHealth = 400\n"
	         "  End\n"
	         "End\n";
}

// the nugget with no ProjectileTemplateName is the cave troll's own (the held unit IS the projectile); the second one names its object, the shape a
// projectile-firing weapon of any other unit uses
const char *const kOwnNugget = "";
const char *const kNamedNugget = "    ProjectileTemplateName = Port168Goblin\n";

struct ThrowResult
{
	bool held = false;
	int weaponStatus = -1;
	bool thrown = false;  // the goblin left the container: RW 0x90FAC3 took it out
	bool stillInContainer = true;
	float goblinHealth = 0.0f;
	float targetHealth = 400.0f;
};

// the goblin enters the troll's container, the troll attacks the target, and the logic runs `framesToRun` frames. `takeContainer` may REMOVE the
// container from the troll between the order and the first frame (a thrower that loses its contain, or a second thrower sharing the world).
ThrowResult throwGoblin(const std::string &filter, const char *nuggetLine, bool takeContainerAway, int framesToRun)
{
	ThrowResult r;
	CombatWorld w(throwerObjects(filter, nuggetLine).c_str());
	{
		const PowerStore store(w);
		w.combat().setAutoAcquireEnabled(false);
		Object *thrower = w.unit("Port168Thrower", 'A', 200.0f, 200.0f);
		Object *goblin = w.unit("Port168Goblin", 'A', 230.0f, 200.0f);
		Object *target = w.unit("Port168Target", 'B', 320.0f, 200.0f);
		REQUIRE(thrower != nullptr);
		REQUIRE(goblin != nullptr);
		REQUIRE(target != nullptr);
		w.frames(3);
		TransportContain *contain = dynamic_cast<TransportContain *>(thrower->getContain());
		REQUIRE(contain != nullptr);
		r.held = contain->addToContain(goblin);
		REQUIRE(r.held);
		// the container's weapon set: PASSENGER_TYPE_TWO from a held INFANTRY (TransportContain::letRidersUpgradeWeaponSet RW 0x86A731) selects the
		// throw weapon's WeaponSet
		w.frames(2);
		ObjectWeapons *weapons = thrower->getWeapons();
		REQUIRE(weapons != nullptr);
		Weapon *throwWeapon = weapons->weaponInSlot(PRIMARY_WEAPON);
		REQUIRE(throwWeapon != nullptr);
		REQUIRE(throwWeapon->getTemplate() != nullptr);
		REQUIRE(std::string(throwWeapon->getTemplate()->getName()) == "Port168Throw");
		r.weaponStatus = weapons->weaponStatus(*throwWeapon);
		if (takeContainerAway)
		{
			// the ammo is the container's count: with no container the reader answers nothing (RW 0x6CD2C9 -> RW 0x6CD2E1, the clip's own status)
			contain->removeFromContain(goblin);
			REQUIRE(goblin->getContainedBy() == nullptr);
			r.weaponStatus = weapons->weaponStatus(*throwWeapon);
			r.stillInContainer = false;
			r.goblinHealth = w.health(goblin);
			r.targetHealth = w.health(target);
			return r;
		}
		REQUIRE(thrower->getAIUpdateInterface() != nullptr);
		REQUIRE(thrower->getAIUpdateInterface()->aiAttackObject(target, CMD_FROM_PLAYER));
		const ObjectID goblinId = goblin->getID();
		for (int i = 0; i < framesToRun; ++i)
		{
			w.frames(1);
			r.thrown = r.thrown || !w.logic.findObjectByID(goblinId) || goblin->getContainedBy() != thrower;
		}
		r.stillInContainer = goblin->getContainedBy() == thrower;
		r.goblinHealth = w.health(goblin);
		r.targetHealth = w.health(target);
	}
	return r;
}
} // namespace

// The held goblin is the weapon's ammo (RW 0x6CD298: the clip reads the contain's count through ProjectileFilterInContainer, contain slot 0x114
// RW 0x865CE3) and the shot's projectile (RW 0x90FA81 .. 0x90FAC9: contain slot 0x13C RW 0x866367 takes the first matching rider out and returns it, then
// it flies with its own BezierProjectileBehavior, so the cave troll's nugget carries no ProjectileTemplateName). Without the two readers the weapon is
// permanently out of ammo (a stub that answers false) and the throw never happens: the troll only swings.
TEST_CASE("PORT-168 throw: the container's INFANTRY passenger is the projectile (RW 0x90FA81 .. 0x90FAC9, RW 0x866367)")
{
	const ThrowResult r = throwGoblin("NONE +INFANTRY", kOwnNugget, false, 60);
	CHECK(r.weaponStatus != WEAPON_OUT_OF_AMMO); // RW 0x6CD142's third gate: container ammo counts as ammo
	CHECK(r.thrown);                             // the goblin left the container on the fire
	CHECK_FALSE(r.stillInContainer);
	CHECK(r.goblinHealth == 100.0f);             // it flies whole; its own DieOnImpact decides the landing
	CHECK(r.targetHealth == 100.0f);             // the 300 SIEGE warhead through NoArmor's DEFAULT 100%: 400 - 300
}

// Neighbouring behaviours the container branch must not disturb:
//   * a filter that does not allow the held unit (NONE +ORC against a non-ORC goblin) leaves the weapon out of ammo: contain slot 0x114 counts nothing
//     (RW 0x865CE3 -> RW 0x7640C1), and nothing leaves the hand;
//   * with the container gone the reader answers like retail's own no-container branch (RW 0x6CD2C9 -> RW 0x6CD2E1): the weapon reads its clip, not a
//     passenger;
//   * a nugget that names its projectile is untouched by the branch (RW 0x90FA8E `je` skips to RW 0x90FACB): the held unit stays held and the named
//     object is what flies.
TEST_CASE("PORT-168 neighbours: a filter that allows nothing, no container, and a nugget with its own projectile (RW 0x6CD298, RW 0x90FACB)")
{
	const ThrowResult refuses = throwGoblin("NONE +ORC", kOwnNugget, false, 30);
	CHECK(refuses.weaponStatus == WEAPON_OUT_OF_AMMO); // no contained rider the filter allows
	CHECK(refuses.stillInContainer);                   // the goblin stays held
	CHECK(refuses.goblinHealth == 100.0f);
	CHECK(refuses.targetHealth == 400.0f);             // nothing was thrown

	const ThrowResult noContainer = throwGoblin("NONE +INFANTRY", kOwnNugget, true, 0);
	CHECK(noContainer.weaponStatus != WEAPON_OUT_OF_AMMO); // the container is gone: the clip's own ammo answers, not a passenger
	CHECK_FALSE(noContainer.stillInContainer);

	const ThrowResult named = throwGoblin("NONE +INFANTRY", kNamedNugget, false, 60);
	CHECK(named.weaponStatus != WEAPON_OUT_OF_AMMO);
	CHECK(named.stillInContainer);    // the passenger is NOT the ammo of a nugget that names its projectile
	CHECK(named.goblinHealth == 100.0f);
}
