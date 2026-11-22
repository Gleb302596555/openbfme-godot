// OpenBFME. GPL-3.0.
// LOCAL-113 (fork issue 797): AutoAbilityBehavior uncited own functions (RW 0x0085D436, 0x0085D43C, 0x0085D3ED, 0x0085D5CE),
// WorkingRadius unread verdict (RW 0xC57EC0 / RW 0x85D37D), and Ennorath scenarios for Saleme's traps and Angmar's troll sling plague barrels.

#include "doctest.h"
#include "CombatTestUtil.h"
#include "EnnorathScenario.h"

#include "Common/SpecialPower.h"
#include "GameClient/ControlBarCommands.h"
#include "GameClient/FXList.h"
#include "GameLogic/ActionManager.h"
#include "GameLogic/AttributeModifiers.h"
#include "GameLogic/Combat/CombatQueries.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/ActiveBody.h"
#include "GameLogic/Module/HeroAbilityModules.h"
#include "GameLogic/Module/SpecialAbilityModules.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/Object/PartitionManager.h"
#include "GameLogic/Weapon.h"

#include <cmath>
#include <string>

namespace
{
const char kLocal113AutoAbilityIni[] = R"ini(
SpecialPower Local113AutoPower
 Enum = SPECIAL_GENERAL_TARGETLESS
 ReloadTime = 30000
End

Weapon Local113AutoWeapon
 AttackRange = 300.0
 DelayBetweenShots = 0
 PreAttackDelay = 0
 FiringDuration = 0
 RadiusDamageAffects = ENEMIES
 DamageNugget
  Damage = 25
  Radius = 300.0
  DamageType = SLASH
  DeathType = NORMAL
 End
End

CommandButton Command_Local113AutoPower
 Command = SPECIAL_POWER
 SpecialPower = Local113AutoPower
 AutoAbility = Yes
 InPalantir = Yes
End

CommandSet Local113CasterSet
 1 = Command_Local113AutoPower
End

Object Local113Caster
 VisionRange = 600.0
 CommandSet = Local113CasterSet
 KindOf = SELECTABLE CAN_ATTACK INFANTRY
 Body = ActiveBody ModuleTag_Body
  MaxHealth = 500
 End
 Behavior = SpecialPowerModule ModuleTag_SP
  SpecialPowerTemplate = Local113AutoPower
  UpdateModuleStartsAttack = Yes
 End
 Behavior = WeaponFireSpecialAbilityUpdate ModuleTag_WF
  SpecialPowerTemplate = Local113AutoPower
  SpecialWeapon = Local113AutoWeapon
  UnpackTime = 0
  PreparationTime = 1
  PackTime = 0
  StartAbilityRange = 300.0
 End
 Behavior = AutoAbilityBehavior ModuleTag_Auto
  SpecialAbility = Local113AutoPower
  MaxScanRange = 100.0
  WorkingRadius = 500.0
  StartsActive = Yes
  Query = 1 ALL ENEMIES
 End
End

Object Local113Target
 VisionRange = 200.0
 KindOf = SELECTABLE INFANTRY
 Body = ActiveBody ModuleTag_Body
  MaxHealth = 500
 End
End
)ini";

struct Local113World : combattest::CombatWorld
{
	SpecialPowerStore powers;
	CommandStore commandStore;
	FXListStore fxLists;
	AttributeModifierStore modifiers;

	SpecialPowerStore *savedPowers = TheSpecialPowerStore;
	CommandStore *savedCommands = TheCommandStore;
	FXListStore *savedFx = TheFXListStore;
	AttributeModifierStore *savedModifiers = TheAttributeModifierStore;

	Local113World()
		: combattest::CombatWorld("", 40)
	{
		TheSpecialPowerStore = &powers;
		TheCommandStore = &commandStore;
		TheFXListStore = &fxLists;
		TheAttributeModifierStore = &modifiers;
		commandStore.setThingFactory(&w.things);

		modifiers.registerBlock(w.fx.env.blocks);
		w.fx.env.blocks.registerBlock("FXList", [](INI *ini) { ParseFXListDefinitionGlobal(ini); });
		w.fx.env.blocks.registerBlock("SpecialPower", [](INI *ini) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(ini); });
		w.fx.env.blocks.registerBlock("CommandButton", [](INI *ini) { CommandStore::parseCommandButtonDefinitionGlobal(ini); });
		w.fx.env.blocks.registerBlock("CommandSet", [](INI *ini) { CommandStore::parseCommandSetDefinitionGlobal(ini); });

		SpecialAbilityModules::registerAll(w.modules);
		SpecialPowerModules::registerAll(w.modules);
		SpecialPowerModules::installScienceHooks(*logic);
		HeroAbilityModules::registerHandlers(dispatcher);

		const std::string err = w.load(kLocal113AutoAbilityIni, INI_LOAD_OVERWRITE, "local113.ini");
		REQUIRE_MESSAGE(err.empty(), err);

		logic->partition().setRegion(0.0f, 0.0f, 4000.0f, 4000.0f);
		logic->combat().setAutoAcquireEnabled(false);
	}

	~Local113World()
	{
		logic->reset();
		TheSpecialPowerStore = savedPowers;
		TheCommandStore = savedCommands;
		TheFXListStore = savedFx;
		TheAttributeModifierStore = savedModifiers;
	}
};
} // namespace

// RW 0xC57EC0 (FieldParse), RW 0x85D355 / RW 0x85D37D (AutoAbilityBehaviorModuleData ctor zeroes +0x10), RW 0x85D9D1 (update reads only MaxScanRange +0x08).
TEST_CASE("LOCAL-113: AutoAbilityBehavior WorkingRadius is parsed without altering MaxScanRange (RW 0xC57EC0 / RW 0x85D37D)")
{
	Local113World f;
	Object *caster = f.unit("Local113Caster", 'A', 300.0f, 300.0f);
	Object *farEnemy = f.unit("Local113Target", 'B', 500.0f, 300.0f); // distance 200: inside WorkingRadius (500), outside MaxScanRange (100)
	REQUIRE(caster != nullptr);
	REQUIRE(farEnemy != nullptr);

	AutoAbilityBehavior *aa = AutoAbilityBehavior::forPower(*caster, "Local113AutoPower");
	REQUIRE(aa != nullptr);
	CHECK(aa->data()->m_workingRadius == 500.0f);
	CHECK(aa->data()->m_maxScanRange == 100.0f);
	CHECK(aa->autoButton() == "Command_Local113AutoPower");

	f.frames(20);
	CHECK(aa->castRecord().casts == 0);

	// Move enemy inside MaxScanRange (distance 60 < 100): auto-cast now triggers (RW 0x85D9D1).
	const Coord3D nearPos{ 360.0f, 300.0f, 0.0f };
	farEnemy->setPosition(&nearPos);
	f.frames(20);
	CHECK(aa->castRecord().casts == 1);
}

// RW 0x0085D436 (getModuleName), RW 0x0085D43C (getModuleNameKey), RW 0x0085D3ED (~AutoAbilityBehavior), RW 0x0085D5CE (scalar deleting dtor),
// RW 0x0085D76C (setAutoButton), RW 0x0085D7B6 (setAutoAbility).
TEST_CASE("LOCAL-113: AutoAbilityBehavior registration, toggle off, and clean destruction (RW 0x0085D436 / 0x0085D43C / 0x0085D3ED / 0x0085D5CE)")
{
	Local113World f;
	Object *caster = f.unit("Local113Caster", 'A', 300.0f, 300.0f);
	REQUIRE(caster != nullptr);
	const CommandButton *cb = TheCommandStore->findCommandButton("Command_Local113AutoPower");
	REQUIRE(cb != nullptr);

	AutoAbilityBehavior *aa = AutoAbilityBehavior::forPower(*caster, "Local113AutoPower");
	REQUIRE(aa != nullptr);
	CHECK(aa->isAutoButton(*cb));

	aa->setAutoAbility(cb, false); // RW 0x85D7B6: matching button + false clears and sleeps forever
	CHECK_FALSE(aa->isAutoButton(*cb));
	CHECK(aa->autoButton().empty());

	aa->setAutoButton(cb); // RW 0x85D76C: sets button and wakes
	CHECK(aa->isAutoButton(*cb));

	aa->setAutoButton(nullptr); // RW 0x85D76C: null clears and sleeps
	CHECK(aa->autoButton().empty());

	f.logic->destroyObject(caster); // exercises ~AutoAbilityBehavior (RW 0x0085D3ED / 0x0085D5CE)
	f.frames(2);
}

// Sources: data/ini/object/evilfaction/units/harad/salemetraps.ini (SalemeExplodeTrap, SalemePoisonTrap, AutoAbilityBehavior MaxScanRange 80 StartsActive Yes),
// data/ini/includes/weapon.inc (SalemeDetonateFallen, SalemeExplodeExplosionsfalleWeapon), RW 0x85D9D1, RW 0x8961F7, RW 0x90E28C, RW 0x8609A8.
TEST_CASE("LOCAL-113 ENNORATH scenario: Saleme trap auto-detonation and manual detonation damage nearby enemies")
{
	ennorathscenario::World w;
	if (!w.load("LOCAL-113 ENNORATH Saleme traps"))
	{
		return;
	}
	structtest::Arena a(w.shared, "FactionEvilmen", "FactionMen");
	auto step = [&](int n) {
		for (int i = 0; i < n; ++i)
		{
			for (int p = 1; p <= 6; ++p)
			{
				a.logic.update(p);
			}
		}
	};
	auto totalHordeHealth = [&](Object *horde) -> float {
		float sum = 0.0f;
		if (!horde || !horde->getContain() || !horde->getContain()->getContainedItemsList())
		{
			return sum;
		}
		for (Object *m : *horde->getContain()->getContainedItemsList())
		{
			if (const ActiveBody *b = m ? dynamic_cast<const ActiveBody *>(m->getBodyModule()) : nullptr)
			{
				sum += b->getHealth();
			}
		}
		return sum;
	};

	// 1. Auto-detonation: SalemeExplodeTrap (StartsActive = Yes, MaxScanRange = 80, Query = 5 ANY ENEMIES +INFANTRY)
	Object *explodeTrap = w.place(a, "SalemeExplodeTrap", 0, 500.0f, 500.0f);
	Object *foe1 = w.place(a, "GondorFighterHorde", 1, 530.0f, 500.0f);
	REQUIRE(explodeTrap != nullptr);
	REQUIRE(foe1 != nullptr);
	const ObjectID trapId = explodeTrap->getID();
	const ObjectID foe1Id = foe1->getID();
	const float hpBefore1 = totalHordeHealth(foe1);
	REQUIRE(hpBefore1 > 0.0f); // tolerance: horde total HP depends on Ennorath GondorFighterHorde payload

	step(40);
	Object *trapAfter = a.logic.findObjectByID(trapId);
	CHECK((trapAfter == nullptr || trapAfter->isEffectivelyDead()));
	Object *foe1After = a.logic.findObjectByID(foe1Id);
	const float hpAfter1 = totalHordeHealth(foe1After);
	CHECK(hpAfter1 < hpBefore1); // tolerance: blast hits vary with horde member formation spread

	// 2. Manual detonation: SalemePoisonTrap with AutoAbility paused, detonated via SpecialAbilityDetonate
	Object *poisonTrap = w.place(a, "SalemePoisonTrap", 0, 1100.0f, 500.0f);
	REQUIRE(poisonTrap != nullptr);
	if (AutoAbilityBehavior *aa = AutoAbilityBehavior::forPower(*poisonTrap, "SpecialAbilityDetonate"))
	{
		aa->setAutoButton(nullptr);
	}
	Object *foe2 = w.place(a, "GondorFighterHorde", 1, 1135.0f, 500.0f);
	REQUIRE(foe2 != nullptr);
	const ObjectID poisonId = poisonTrap->getID();
	const ObjectID foe2Id = foe2->getID();
	const float hpBefore2 = totalHordeHealth(foe2);
	REQUIRE(hpBefore2 > 0.0f); // tolerance: horde total HP depends on Ennorath GondorFighterHorde payload

	const SpecialPowerTemplate *detonate = TheSpecialPowerStore->findSpecialPowerTemplate("SpecialAbilityDetonate");
	REQUIRE(detonate != nullptr);
	CHECK(SpecialPowerModules::doSpecialPower(*poisonTrap, detonate, 0u, false));

	step(40);
	Object *poisonAfter = a.logic.findObjectByID(poisonId);
	CHECK((poisonAfter == nullptr || poisonAfter->isEffectivelyDead()));
	Object *foe2After = a.logic.findObjectByID(foe2Id);
	const float hpAfter2 = totalHordeHealth(foe2After);
	CHECK(hpAfter2 < hpBefore2); // tolerance: DOT poison ticks vary with horde member formation spread
}

// Sources: data/ini/object/evilfaction/units/angmar/angmartrollsling.ini (AngmarTrollSling, SpecialAbilityAngmarCatapultPlagueBarrels,
// Upgrade_AngmarHexerdesZwielichtsTrigger), data/ini/includes/weapon.inc (AngmarCatapultPoisonBarrel, MinimumAttackRange 150), RW 0x893D61.
TEST_CASE("LOCAL-113 ENNORATH scenario: AngmarTrollSling plague barrels enforce MinimumAttackRange and fire at valid distance")
{
	ennorathscenario::World w;
	if (!w.load("LOCAL-113 ENNORATH AngmarTrollSling"))
	{
		return;
	}
	structtest::Arena a(w.shared, "FactionAngmar", "FactionMen");
	auto step = [&](int n) {
		for (int i = 0; i < n; ++i)
		{
			for (int p = 1; p <= 6; ++p)
			{
				a.logic.update(p);
			}
		}
	};

	a.player(0)->addCompletedUpgrade("Upgrade_AngmarHexerdesZwielichtsTrigger");
	Object *sling = w.place(a, "AngmarTrollSling", 0, 500.0f, 500.0f);
	Object *foe = w.place(a, "GondorFighterHorde", 1, 800.0f, 500.0f); // distance 300 > MinimumAttackRange (150) + boundingRadius
	REQUIRE(sling != nullptr);
	REQUIRE(foe != nullptr);
	step(5);

	const CommandButton *cb = TheCommandStore->findCommandButton("Command_SpecialAbilityAngmarCatapultPoisonBarrels");
	const SpecialPowerTemplate *sp = TheSpecialPowerStore->findSpecialPowerTemplate("SpecialAbilityAngmarCatapultPlagueBarrels");
	REQUIRE(cb != nullptr);
	REQUIRE(sp != nullptr);
	SpecialPowerModuleInterface *spm = SpecialPowerModules::findModule(*sling, sp);
	REQUIRE(spm != nullptr);
	CHECK(spm->getPercentReady() == 1.0f);

	// Too close (distance 140 < MinimumAttackRange 150 + boundingRadius): refused by WeaponFireSpecialAbilityUpdate::allowsSpecialPowerUse (RW 0x893D61).
	const Coord3D tooClose{ 640.0f, 500.0f, 0.0f };
	CHECK_FALSE(ActionManager::canDoSpecialPowerAtLocation(*sling, tooClose, sp, cb->m_options));

	// Valid range (distance 300): accepted and fires.
	const Coord3D validLoc = *foe->getPosition();
	CHECK(ActionManager::canDoSpecialPowerAtLocation(*sling, validLoc, sp, cb->m_options));
	CHECK(SpecialPowerModules::doSpecialPowerAtLocation(*sling, sp, validLoc, cb->m_options, false));

	step(80);
	CHECK(spm->getPercentReady() < 1.0f); // tolerance: cooldown progress depends on unpack + travel frame
}
