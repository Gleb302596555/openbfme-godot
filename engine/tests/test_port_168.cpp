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
#include "GameLogic/System/ShroudManager.h"
#include "GameLogic/ActionManager.h"
#include "Common/SpecialPower.h"
#include "Common/Thing/ThingTemplate.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/Combat/ProjectileLauncher.h"
#include "GameLogic/WeaponNugget.h"
#include "GameLogic/Module/ActiveBody.h"
#include "GameLogic/Module/GrabPassengerSpecialPower.h"
#include "GameLogic/Module/ProjectileModules.h"
#include "GameLogic/Module/SpecialAbilityModules.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/Object/Contain/TransportContainRuntime.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/WeaponState.h"

#include <cstdio>
#include <string>

using namespace combattest;

namespace
{
const char kPowers[] = R"ini(
SpecialPower SpecialAbilityGrabPassenger
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
	                   "  Behavior = AIUpdateInterface ModuleTag_AI\n"
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
	         "End\n";
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
// 0x8998D8) and the logic runs until the victim is gone; the result carries the health before the order and the frame after the ability finished
struct Eaten
{
	float before = 0.0f;
	float after = 0.0f;
	bool held = false; // the orc entered the eater's container (the grab's own reaction, RW 0x8C94D7 -> the contain's addToContain)
	bool gone = false; // the orc is no longer in the world: the contain's eat countdown (RW 0x865A5C) destroyed it
};
Eaten eat(const std::string &healLines)
{
	CombatWorld w;
	const PowerStore store(w); // the objects name SPECIAL_GRAB_PASSENGER, so the store must answer before they load (RW 0x8C94D7)
	const std::string err = w.w.load(eaterObjects(healLines), INI_LOAD_OVERWRITE, "port168eater.ini");
	REQUIRE_MESSAGE(err.empty(), err);
	w.combat().setAutoAcquireEnabled(false);
	Object *eater = w.unit("Port168Eater", 'A', 200.0f, 200.0f);
	Object *orc = w.unit("Port168Orc", 'B', 205.0f, 200.0f); // within StartAbilityRange 8.0: the grab needs no walk
	REQUIRE(eater != nullptr);
	REQUIRE(orc != nullptr);
	w.frames(3);
	// the troll arrives damaged: 50% of its 2400 — the health bar shows the heal, and MyHealth:99% is what the mod's entry asks for
	BodyModuleInterface *body = eater->getBodyModule();
	REQUIRE(body != nullptr);
	body->setInitialHealthPercent(50.0f);
	REQUIRE(w.health(eater) == 1200.0f);
	GrabPassengerSpecialPower *grab = moduleOf<GrabPassengerSpecialPower>(eater);
	REQUIRE(grab != nullptr);
	REQUIRE(grabAbilityOf(*eater) != nullptr); // the 0x27 SpecialAbilityUpdate the grab hands the target to
	Eaten e;
	e.before = w.health(eater);
	const ObjectID orcId = orc->getID();
	grab->doSpecialPowerAtObject(orc, 0x4000u); // RW 0x8C94D7
	REQUIRE(grab->grabsStarted() == 1);
	for (int i = 0; i < 120 && !e.gone; ++i)
	{
		w.frames(1);
		e.held = e.held || orc->getContainedBy() == eater;
		e.gone = w.logic->findObjectByID(orcId) == nullptr;
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
	REQUIRE(e.held);
	REQUIRE(e.gone);                        // the orc was taken out of the world (OpenContain's eat countdown RW 0x865A5C)
	CHECK(e.before == 1200.0f);             // 50% of 2400
	CHECK(e.after == 2400.0f);              // 50 + 50 = 100%: min(100, ...) at RW 0x853317
}

// The neighbouring behaviour: the field left out is the data's own default (+ 0x30 = 100.0), so the same eat still tops the troll up — the heal is not
// tied to the number being written. And a smaller number heals less: 20.0 gives 70% of the max health.
TEST_CASE("PORT-168 heal: the data's default and a smaller percentage (module data + 0x30, RW 0x853302)")
{
	const Eaten byDefault = eat("");
	REQUIRE(byDefault.held);
	CHECK(byDefault.after == 2400.0f); // 50 + the default 100, clamped to 100

	const Eaten smaller = eat("    GrabPassengerHealGainPercent = 20.0\n");
	REQUIRE(smaller.held);
	CHECK(smaller.after == 1680.0f); // 50 + 20 = 70% of 2400
}


namespace
{
// the riders the filter's kind mask decides about: one ORC, one INFANTRY that is not an ORC
const char kAmmoVictims[] =
	"Object Port168AmmoOrc\n"
	"  KindOf = INFANTRY ORC SELECTABLE\n"
	"  TransportSlotCount = 1\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"End\n"
	"Object Port168AmmoGoblin\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  TransportSlotCount = 1\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"End\n";

// The container-ammo shape cut down to the two readers: a bow whose clip is ProjectileFilterInContainer (the cave troll's CaveTrollGoblinThrow, whose
// clip is 1 and which never reloads, AutoReloadsClip = No) and the same bow without the field. Three objects share the weapons: one with a container,
// one without (the reader must fall back to the clip, RW 0x6CD2C9), and one holding the filter-less bow.
const char kAmmoWeapons[] =
	"Weapon Port168ContainerBow\n"
	"  AttackRange = 200\n"
	"  WeaponSpeed = 100\n"
	"  DelayBetweenShots = 2000\n"
	"  PreAttackDelay = 400\n"
	"  PreAttackType = PER_SHOT\n"
	"  FiringDuration = 200\n"
	"  AutoReloadsClip = No\n"
	"  ClipSize = 1\n"
	"  ProjectileFilterInContainer = %FILTER%\n"
	"  ProjectileNugget\n"
	"    WarheadTemplateName = Port168AmmoWarhead\n"
	"  End\n"
	"End\n"
	"Weapon Port168ClipBow\n"
	"  AttackRange = 200\n"
	"  WeaponSpeed = 100\n"
	"  DelayBetweenShots = 2000\n"
	"  ClipSize = 1\n"
	"  ProjectileNugget\n"
	"    ProjectileTemplateName = Arrow\n"
	"    WarheadTemplateName = Port168AmmoWarhead\n"
	"  End\n"
	"End\n"
	"Weapon Port168AmmoWarhead\n"
	"  DamageNugget\n"
	"    Damage = 300\n"
	"    Radius = 12.0\n"
	"    DamageType = SIEGE\n"
	"    DeathType = NORMAL\n"
	"  End\n"
	"End\n";

std::string archerObject(const char *name, const char *weapon, bool withContain)
{
	return std::string("Object ") + name + "\n" +
	       "  KindOf = INFANTRY SELECTABLE\n"
	       "  Geometry = CYLINDER\n"
	       "  GeometryMajorRadius = 8\n"
	       "  GeometryMinorRadius = 8\n"
	       "  GeometryHeight = 20\n"
	       "  Body = ActiveBody ModuleTag_Body\n"
	       "    MaxHealth = 100\n"
	       "  End\n"
	       "  WeaponSet\n"
	       "    Conditions = None\n"
	       "    Weapon = PRIMARY " + weapon + "\n"
	       "  End\n" +
	       (withContain ? std::string("  Behavior = TransportContain ModuleTag_Contain\n"
	                                 "    Slots = 2\n"
	                                 "    AllowEnemiesInside = Yes\n"
	                                 "    AllowAlliesInside = Yes\n"
	                                 "    ObjectStatusOfContained = UNSELECTABLE\n"
	                                 "    PassengerFilter = ANY +INFANTRY\n"
	                                 "  End\n")
	                    : std::string()) +
	       "End\n";
}

std::string ammoObjects(const std::string &filter)
{
	std::string weapons = kAmmoWeapons;
	const std::size_t at = weapons.find("%FILTER%");
	weapons.replace(at, 8, filter);
	return weapons + archerObject("Port168Archer", "Port168ContainerBow", true) + archerObject("Port168NoContain", "Port168ContainerBow", false) +
	       archerObject("Port168ClipArcher", "Port168ClipBow", true) + kAmmoVictims;
}

// the status of the archer's PRIMARY weapon; `held` (may be empty) enters the archer's container first
int ammoStatus(const char *archerName, const std::string &filter, const char *held)
{
	CombatWorld w;
	const PowerStore store(w);
	const std::string err = w.w.load(ammoObjects(filter), INI_LOAD_OVERWRITE, "port168ammo.ini");
	REQUIRE_MESSAGE(err.empty(), err);
	w.combat().setAutoAcquireEnabled(false);
	Object *archer = w.unit(archerName, 'A', 200.0f, 200.0f);
	REQUIRE(archer != nullptr);
	w.frames(3);
	if (held && *held)
	{
		Object *rider = w.unit(held, 'A', 230.0f, 200.0f);
		REQUIRE(rider != nullptr);
		TransportContain *contain = dynamic_cast<TransportContain *>(archer->getContain());
		REQUIRE(contain != nullptr);
		REQUIRE(contain->addToContain(rider));
		w.frames(1);
	}
	ObjectWeapons *weapons = archer->getWeapons();
	REQUIRE(weapons != nullptr);
	Weapon *bow = weapons->weaponInSlot(PRIMARY_WEAPON);
	REQUIRE(bow != nullptr);
	return weapons->weaponStatus(*bow);
}
} // namespace

// RW 0x6CD298 Weapon::getRemainingAmmo: with ProjectileFilterInContainer set (template + 0x120, RW 0x762977) the ammo is NOT the clip but the owner's
// container ([object + 0x258], RW 0x6CD2BF) asked with the filter (contain slot 0x114, RW 0x6CD2D9 -> RW 0x865CE3: ObjectFilterMatch RW 0x7640C1 with
// the rider and the container's player RW 0x68B678); RW 0x6CD11F then lets RW 0x6CD142 report READY for an owner whose weapon in a slot carries that
// filter. The port's host answered `false` to both, so such a weapon was permanently OUT_OF_AMMO — the cave troll's CaveTrollGoblinThrow, clip 1 and
// AutoReloadsClip = No, could never fire at all and the troll only swung its arm.
TEST_CASE("PORT-168 ammo: ProjectileFilterInContainer counts the container's passengers, not the clip (RW 0x6CD298, RW 0x6CD11F)")
{
	CHECK(ammoStatus("Port168Archer", "NONE +INFANTRY", nullptr) == WEAPON_OUT_OF_AMMO); // an empty container: nothing to throw
	CHECK(ammoStatus("Port168Archer", "NONE +INFANTRY", "Port168AmmoGoblin") == WEAPON_READY_TO_FIRE);
	// the filter's kind mask (RW 0x7640C1): an ORC-only filter leaves the non-ORC goblin out of the count
	CHECK(ammoStatus("Port168Archer", "NONE +ORC", "Port168AmmoGoblin") == WEAPON_OUT_OF_AMMO);
	CHECK(ammoStatus("Port168Archer", "NONE +ORC", "Port168AmmoOrc") == WEAPON_READY_TO_FIRE);
}

// The neighbouring behaviours of the reader:
//   * an owner with no container falls back to its own clip (RW 0x6CD2C9 -> RW 0x6CD2E1), the same answer retail gives before the contain branch of the
//     deliver (RW 0x90FA90);
//   * a weapon without ProjectileFilterInContainer never asks a container: its clip answers, even with a passenger inside.
TEST_CASE("PORT-168 neighbours: no container and no filter keep the clip's own status (RW 0x6CD2C9, RW 0x6CD298)")
{
	CHECK(ammoStatus("Port168NoContain", "NONE +INFANTRY", nullptr) == WEAPON_READY_TO_FIRE); // no contain: the reader answers nothing, clip 1
	CHECK(ammoStatus("Port168ClipArcher", "NONE +INFANTRY", nullptr) == WEAPON_READY_TO_FIRE);             // clip 1
	CHECK(ammoStatus("Port168ClipArcher", "NONE +INFANTRY", "Port168AmmoGoblin") == WEAPON_READY_TO_FIRE); // its clip, not the passenger
}

namespace
{
// the container's passenger as the projectile itself: the object that flies carries its own BezierProjectileBehavior (the cave troll's held goblin flies
// with its own module once RW 0x90FAC9 hands the taken rider over) and a victim of 400 health with NoArmor (DEFAULT 100%: the 300 SIEGE warhead leaves
// it at 100).
const char kThrownObjects[] =
	"Object Port168Thrown\n"
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
	"Object Port168AmmoVictim\n"
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

// the deliver branch without a weapon that fires: the shot record names the weapon template, its nugget (no ProjectileTemplateName: the held unit IS the
// projectile) and the source, exactly the arguments RW 0x90FA48 works from
struct DeliverResult
{
	bool taken = false;                 // the passenger left the container at the launch (RW 0x90FAC3 -> contain slot 0x13C)
	bool containedAfter = true;
	bool passengerIsProjectile = false; // the object that flew is the passenger itself, produced by the thrower
	float passengerHealth = 100.0f;
	float targetHealth = 400.0f;
};

DeliverResult deliverPassenger(const char *filter)
{
	DeliverResult r;
	CombatWorld w;
	const PowerStore store(w);
	const std::string err = w.w.load(ammoObjects(filter) + kThrownObjects + archerObject("Port168Bystander", "Port168ClipBow", false),
	                                 INI_LOAD_OVERWRITE, "port168deliver.ini");
	REQUIRE_MESSAGE(err.empty(), err);
	w.combat().setAutoAcquireEnabled(false);
	Object *thrower = w.unit("Port168Archer", 'A', 200.0f, 200.0f);
	Object *goblin = w.unit("Port168Thrown", 'A', 205.0f, 200.0f);
	Object *victim = w.unit("Port168AmmoVictim", 'B', 245.0f, 200.0f);
	REQUIRE(thrower != nullptr);
	REQUIRE(goblin != nullptr);
	REQUIRE(victim != nullptr);
	w.frames(3);
	TransportContain *contain = dynamic_cast<TransportContain *>(thrower->getContain());
	REQUIRE(contain != nullptr);
	REQUIRE(contain->addToContain(goblin));
	w.frames(1);
	ObjectWeapons *weapons = thrower->getWeapons();
	REQUIRE(weapons != nullptr);
	Weapon *bow = weapons->weaponInSlot(PRIMARY_WEAPON);
	REQUIRE(bow != nullptr);
	const WeaponTemplate *tmpl = bow->getTemplate();
	REQUIRE(tmpl != nullptr);
	REQUIRE(tmpl->m_nuggets.size() == 1);
	const ProjectileNugget *nugget = dynamic_cast<const ProjectileNugget *>(tmpl->m_nuggets.front().get());
	REQUIRE(nugget != nullptr);
	REQUIRE(nugget->m_projectileTemplateName.empty()); // the cave troll's own nugget shape
	const ObjectID goblinId = goblin->getID();
	ProjectileShot shot;
	shot.source = thrower->getID();
	shot.victim = victim->getID();
	shot.weapon = tmpl;
	shot.nugget = nugget;
	shot.slot = PRIMARY_WEAPON;
	shot.warhead = TheWeaponStore ? TheWeaponStore->findWeaponTemplate(nugget->m_warheadTemplateName) : nullptr; // RW 0x90FA48's warhead argument
	REQUIRE(shot.warhead != nullptr);
	w.combat().launcher().launch(*w.logic, shot); // the real ObjectProjectileLauncher (RW 0x90FA48)
	r.containedAfter = goblin->getContainedBy() == thrower;
	r.taken = !r.containedAfter;
	r.passengerIsProjectile = w.logic->findObjectByID(goblinId) != nullptr && goblin->getProducerID() == thrower->getID();
	w.frames(40); // the flight and the impact
	r.passengerHealth = w.logic->findObjectByID(goblinId) ? w.health(goblin) : 0.0f; // DieOnImpact removed it on the hit
	r.targetHealth = w.health(victim);
	return r;
}
} // namespace

// RW 0x90FA81 .. 0x90FAC9 (the object-target deliver): a fired weapon with ProjectileFilterInContainer on a source that HAS a container skips both the
// ProjectileSelf test (RW 0x90FA8E) and the template test (RW 0x90FAF7): contain slot 0x13C (RW 0x866367) takes the first rider the filter allows out of
// the container (RW 0x8663AC) and that object IS the projectile (RW 0x90FAC9 jumps over the creation), produced by the source and launched from the
// source's transform (Weapon RW 0x6CAB85 at RW 0x90FAAA). The port's launcher had only ProjectileSelf and the named template, so a nugget with no
// ProjectileTemplateName — the cave troll's CaveTrollGoblinThrow — threw nothing and the troll only swung its arm.
TEST_CASE("PORT-168 throw: the contained passenger becomes the projectile and leaves the container (RW 0x90FA81 .. 0x90FAC9, RW 0x866367)")
{
	const DeliverResult r = deliverPassenger("NONE +INFANTRY");
	CHECK(r.taken);                   // contain slot 0x13C: out of the hand on the launch
	CHECK_FALSE(r.containedAfter);
	CHECK(r.passengerIsProjectile);   // the same object flew, its producer the thrower
	CHECK(r.passengerHealth == 0.0f); // it hit the victim and its own DieOnImpact removed it
	CHECK(r.targetHealth == 100.0f);  // 400 - 300 SIEGE through NoArmor's DEFAULT 100%
}
