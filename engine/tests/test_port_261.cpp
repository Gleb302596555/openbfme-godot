// PORT-261 (issue #609): the player-side powers PlayerHealSpecialPower (heal / repair the player's objects) and StoreObjectsSpecialPower
// (the 'expedition' / teleport powers: store units, release them elsewhere). Synthetic INI only, no retail data; the class bodies were
// re-read against RotWK 2.01 for this lane (RW 0x8CC37B / 0x8CC4B4 / 0x8CC574 / 0x8CC594 / 0x8D1EA7 / 0x853F2A) and match it; these tests pin
// that behaviour so a naive "fix" of the Ennorath buttons (they fail in the matrix for harness reasons, see the PR) cannot invert it.
#include "doctest.h"

#include "CombatTestUtil.h"

#include "Common/SpecialPower.h"
#include "GameLogic/BitFlags.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/Module/ActiveBody.h"
#include "GameLogic/Module/SpecialAbilityModules.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/Module/StoreObjectsSpecialPower.h"
#include "GameLogic/Object/ExperienceTracker.h"
#include "GameLogic/ObjectFilterMatch.h"

#include <memory>

namespace
{
// a Balin/Theodred-like healer and an expedition-caster (SpecialPowerModule starter with UpdateModuleStartsAttack, the SpecialAbilityUpdate subclass
// doing the work, as Balin's ModuleTag_BalinExpidition/ModuleTag_BalinExpiditionUpdate are)
const char kObjects[] =
	"SpecialPower TestHeal\n"
	"  Enum = SPECIAL_SPELL_BOOK_HEAL\n"
	"  ReloadTime = 10000\n"
	"  ObjectFilter = NONE +INFANTRY\n"
	"End\n"
	"SpecialPower TestRebuild\n"
	"  Enum = SPECIAL_SPELL_BOOK_REBUILD\n"
	"  ReloadTime = 10000\n"
	"  ObjectFilter = NONE +STRUCTURE\n"
	"End\n"
	"SpecialPower TestStorePower\n"
	"  Enum = SPECIAL_STORE_LIST_2\n"
	"  ReloadTime = 190000\n"
	"  Flags = WATER_OK\n"
	"  ObjectFilter = ANY +INFANTRY +CAVALRY +HERO +MONSTER ALLIES\n"
	"  InitiateSound = None\n"
	"End\n"
	"SpecialPower TestStorePowerNoChain\n"
	"  Enum = SPECIAL_STORE_LIST_2\n"
	"  ReloadTime = 190000\n"
	"  Flags = WATER_OK\n"
	"  ObjectFilter = ANY +INFANTRY +CAVALRY +HERO +MONSTER ALLIES\n"
	"  InitiateSound = None\n"
	"End\n"
	// the repair target: a completed, healable structure (m_constructionPercent stays -1, so the under-construction test of healObject does not refuse it)
	"Object TestShed\n"
	"  KindOf = STRUCTURE SELECTABLE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 12\n"
	"  GeometryMinorRadius = 12\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"End\n"
	"Object TestHealer\n"
	"  KindOf = INFANTRY SELECTABLE HERO CAN_ATTACK\n"
	"  VisionRange = 150\n"
	"  BountyValue = 10\n"
	"  CommandPoints = 2\n"
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
	"    Weapon = PRIMARY SwordWeapon\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"    AutoAcquireEnemiesWhenIdle = No\n"
	"  End\n"
	"  Behavior = PhysicsBehavior ModuleTag_Physics\n"
	"  End\n"
	"  Behavior = PlayerHealSpecialPower ModuleTag_Heal\n"
	"    SpecialPowerTemplate = TestHeal\n"
	"    HealAmount = 40\n" // small enough that the healed unit does not clamp at full health: the addition path stays visible

	"    HealAsPercent = No\n"
	"    HealAffects = INFANTRY\n"
	"    HealRadius = 100\n"
	"  End\n"
	"End\n"
	"Object TestRebuilder\n"
	"  KindOf = INFANTRY SELECTABLE HERO CAN_ATTACK\n"
	"  VisionRange = 150\n"
	"  BountyValue = 10\n"
	"  CommandPoints = 2\n"
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
	"    Weapon = PRIMARY SwordWeapon\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"    AutoAcquireEnemiesWhenIdle = No\n"
	"  End\n"
	"  Behavior = PhysicsBehavior ModuleTag_Physics\n"
	"  End\n"
	"  Behavior = PlayerHealSpecialPower ModuleTag_Rebuild\n"
	"    SpecialPowerTemplate = TestRebuild\n"
	"    HealAmount = 100\n"

	"    HealAsPercent = No\n"
	"    HealAffects = STRUCTURE\n"
	"    HealRadius = 220\n"
	"  End\n"
	"End\n"
	// a webbed soldier (the WEBBED kind is refused by healObject, RW 0x8CC386)
	"Object TestWebbed\n"
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK WEBBED\n"
	"  VisionRange = 150\n"
	"  BountyValue = 10\n"
	"  CommandPoints = 2\n"
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
	"    Weapon = PRIMARY SwordWeapon\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"    AutoAcquireEnemiesWhenIdle = No\n"
	"  End\n"
	"  Behavior = PhysicsBehavior ModuleTag_Physics\n"
	"  End\n"
	"End\n"
	// Balin's expedition: the SpecialPowerModule starter (UpdateModuleStartsAttack) and the StoreObjectsSpecialPower worker, the ChainedButton the first power never recharges (RW 0x853F2A)
	"Object TestBalin\n"
	"  KindOf = INFANTRY SELECTABLE HERO CAN_ATTACK\n"
	"  VisionRange = 150\n"
	"  BountyValue = 10\n"
	"  CommandPoints = 2\n"
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
	"    Weapon = PRIMARY SwordWeapon\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"    AutoAcquireEnemiesWhenIdle = No\n"
	"  End\n"
	"  Behavior = PhysicsBehavior ModuleTag_Physics\n"
	"  End\n"
	"  Behavior = SpecialPowerModule ModuleTag_Expedition\n"
	"    SpecialPowerTemplate = TestStorePower\n"
	"    UpdateModuleStartsAttack = Yes\n"
	"  End\n"
	"  Behavior = StoreObjectsSpecialPower ModuleTag_ExpeditionUpdate\n"
	"    SpecialPowerTemplate = TestStorePower\n"
	"    StartAbilityRange = 1000000.0\n"
	"    ApproachRequiresLOS = No\n"
	"    Radius = 110\n"
	"    UnpackTime = 0\n"
	"    PreparationTime = 1\n"
	"    IgnoreFacingCheck = Yes\n"
	"    ChainedButton = \"Command_TestExpeditionStoredList\"\n"
	"  End\n"
	"End\n"
	// the same power without the ChainedButton (an expedition that pays its own recharge)
	"Object TestBalinNoChain\n"
	"  KindOf = INFANTRY SELECTABLE HERO CAN_ATTACK\n"
	"  VisionRange = 150\n"
	"  BountyValue = 10\n"
	"  CommandPoints = 2\n"
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
	"    Weapon = PRIMARY SwordWeapon\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"    AutoAcquireEnemiesWhenIdle = No\n"
	"  End\n"
	"  Behavior = PhysicsBehavior ModuleTag_Physics\n"
	"  End\n"
	"  Behavior = SpecialPowerModule ModuleTag_Expedition\n"
	"    SpecialPowerTemplate = TestStorePowerNoChain\n"
	"    UpdateModuleStartsAttack = Yes\n"
	"  End\n"
	"  Behavior = StoreObjectsSpecialPower ModuleTag_ExpeditionUpdate\n"
	"    SpecialPowerTemplate = TestStorePowerNoChain\n"
	"    StartAbilityRange = 1000000.0\n"
	"    ApproachRequiresLOS = No\n"
	"    Radius = 110\n"
	"    UnpackTime = 0\n"
	"    PreparationTime = 1\n"
	"    IgnoreFacingCheck = Yes\n"
	"  End\n"
	"End\n";

struct HealWorld : combattest::CombatWorld
{
	SpecialPowerStore powers;
	SpecialPowerStore *previous = TheSpecialPowerStore;
	HealWorld()
	{
		TheSpecialPowerStore = &powers;
		w.fx.env.blocks.registerBlock("SpecialPower", [](INI *ini) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(ini); });
		const std::string error = w.load(kObjects, INI_LOAD_OVERWRITE, "port261.ini");
		REQUIRE_MESSAGE(error.empty(), error);
	}
	~HealWorld() { TheSpecialPowerStore = previous; }
	const SpecialPowerTemplate *power(const std::string &name)
	{
		const SpecialPowerTemplate *t = TheSpecialPowerStore ? TheSpecialPowerStore->findSpecialPowerTemplate(name) : nullptr;
		REQUIRE_MESSAGE(t != nullptr, "special power " << name);
		return t;
	}
	void injure(Object *o, float fraction)
	{
		ActiveBody *body = dynamic_cast<ActiveBody *>(o ? o->getBodyModule() : nullptr);
		REQUIRE(body != nullptr);
		body->setInitialHealthPercent(fraction);
	}
};

float healthOf(Object *o)
{
	ActiveBody *body = o ? dynamic_cast<ActiveBody *>(o->getBodyModule()) : nullptr;
	return body ? body->getHealth() : -1.0f;
}
} // namespace

TEST_CASE("PORT-261: PlayerHealSpecialPower heals an injured allied unit in radius, nothing else (RW 0x8CC4B4 / 0x8CC37B)")
{
	HealWorld w;
	Object *caster = w.unit("TestHealer", 'A', 300, 300);
	Object *injured = w.unit("Swordsman", 'A', 340, 300);
	Object *webbed = w.unit("TestWebbed", 'A', 315, 340);
	Object *far = w.unit("Swordsman", 'A', 450, 300);
	Object *enemy = w.unit("Swordsman", 'B', 340, 310);
	Object *full = w.unit("Swordsman", 'A', 330, 330);
	const float fullBefore = healthOf(full);
	const float farBefore = healthOf(far);
	w.injure(injured, 0.5f);
	w.injure(enemy, 0.5f);
	w.injure(webbed, 0.5f);
	const float injuredHurt = healthOf(injured);
	const float enemyHurt = healthOf(enemy);
	const float webbedHurt = healthOf(webbed);

	// the cast at the position by the group (the power path: doSpecialPowerAtLocation, RW 0x8CC574)
	const SpecialPowerTemplate *t = w.power("TestHeal");
	PlayerHealSpecialPower *heal = dynamic_cast<PlayerHealSpecialPower *>(caster->findModule("PlayerHealSpecialPower"));
	REQUIRE(heal != nullptr);
	const Coord3D at{ 340, 300, 0 };
	heal->doSpecialPowerAtLocation(at, 0);

	CHECK(heal->lastHealed() == 3); // RW 0x8CC37B has no full-health, damage or self check: the injured unit, the full-health one and the healer itself all count
	CHECK(healthOf(injured) == injuredHurt + 40.0f);
	CHECK(healthOf(full) == fullBefore); // already full: health unchanged, the victim still counts (RW 0x8CC37B has no full-health test)
	CHECK(healthOf(enemy) == enemyHurt); // a different player's unit is not healed (relationship, RW 0x68D7AB: ALLIES only)
	CHECK(healthOf(far) == farBefore);   // out of HealRadius: never entered the scan (RW 0xA39340 within +0x84)
	CHECK(healthOf(webbed) == webbedHurt); // WEBBED kind refused (RW 0x8CC386, bit 3 of template + 0x11B)
	CHECK(t != nullptr);
	CHECK(heal->triggers() == 1);
	CHECK(!heal->isReady()); // the power paid its recharge: no longer ready (RW 0x8979C2)
}

TEST_CASE("PORT-261: PlayerHealSpecialPower on HealAffects STRUCTURE heals only structures; an under-construction one is refused (RW 0x8CC37B)")
{
	HealWorld w;
	Object *caster = w.unit("TestRebuilder", 'A', 300, 300);
	Object *shed = w.unit("TestShed", 'A', 320, 320);
	Object *soldier = w.unit("Swordsman", 'A', 330, 300);
	Object *young = w.unit("TestShed", 'A', 310, 330);
	REQUIRE(shed);
	REQUIRE(soldier);
	REQUIRE(young);
	w.injure(shed, 0.5f);
	w.injure(soldier, 0.5f);
	w.injure(young, 0.5f);
	ActiveBody *youngBody = dynamic_cast<ActiveBody *>(young->getBodyModule());
	REQUIRE(youngBody != nullptr);
	young->setConstructionPercent(50.0f); // RW 0x8CC3DD .. 0x8CC3F5: [0, 99.0) is under construction, refused
	const float shedHurt = healthOf(shed);
	const float soldierHurt = healthOf(soldier);
	const float youngHurt = healthOf(young);
	const float youngPct = young->getConstructionPercent();

	PlayerHealSpecialPower *rebuild = dynamic_cast<PlayerHealSpecialPower *>(caster->findModule("PlayerHealSpecialPower"));
	REQUIRE(rebuild != nullptr);
	const Coord3D at{ 320, 320, 0 };
	rebuild->doSpecialPowerAtLocation(at, 0);

	CHECK(rebuild->lastHealed() == 1);                    // the completed own structure only
	CHECK(healthOf(shed) == shedHurt + 100.0f);
	CHECK(healthOf(soldier) == soldierHurt);              // INFANTRY is not healAffected
	CHECK(healthOf(young) == youngHurt);                  // a site being built is refused
	CHECK(young->getConstructionPercent() == youngPct);

	// a cast with no injured structure in range heals nothing: the exact outcome of the Ennorath rebuild buttons in the matrix
	const Coord3D elsewhere{ 600, 600, 0 };
	rebuild->doSpecialPowerAtLocation(elsewhere, 0);
	CHECK(rebuild->lastHealed() == 0);
}

TEST_CASE("PORT-261: PlayerHealSpecialPower::healAt refuses a disabled caster (RW 0x9325B4 on Object + 0x1C8)")
{
	HealWorld w;
	Object *caster = w.unit("TestHealer", 'A', 300, 300);
	Object *injured = w.unit("Swordsman", 'A', 340, 300);
	w.injure(injured, 0.5f);
	const float hurt = healthOf(injured);
	caster->setDisabled(3, w.logic->getFrame() + 1000); // DISABLED_HELD: the mask is set, no power is paused — exactly the healAt gate (RW 0x9325B4 on + 0x1C8)

	PlayerHealSpecialPower *heal = dynamic_cast<PlayerHealSpecialPower *>(caster->findModule("PlayerHealSpecialPower"));
	REQUIRE(heal != nullptr);
	const unsigned readyBefore = heal->getReadyFrame();
	const Coord3D at{ 340, 300, 0 };
	heal->doSpecialPowerAtLocation(at, 0);

	CHECK(heal->lastHealed() == 0);
	CHECK(healthOf(injured) == hurt);
	CHECK(heal->triggers() == 0); // the base gate (RW 0x8980A3) refused the cast before the trigger
	CHECK(heal->getReadyFrame() == readyBefore); // ... so no recharge either
}

TEST_CASE("PORT-261: StoreObjectsSpecialPower fires through the power; its ChainedButton power never starts the recharge (RW 0x853F2A)")
{
	HealWorld w;
	Object *chainedCaster = w.unit("TestBalin", 'A', 300, 300);
	Object *plainCaster = w.unit("TestBalinNoChain", 'A', 600, 600);
	Object *victimA = w.unit("Swordsman", 'A', 330, 300);
	Object *victimB = w.unit("Swordsman", 'A', 315, 330);
	Object *enemy = w.unit("Swordsman", 'B', 320, 315);
	REQUIRE(victimA);
	REQUIRE(victimB);
	REQUIRE(enemy);
	victimA->setStatus(3, true);  // UNSELECTABLE: refused by the RW 0x44DDEC(3) test of the trigger scan
	victimB->setStatus(38, true); // HORDE_MEMBER: refused by the status-38 mask (RW 0x6267C7 over bit 0x26)
	Object *uncle = w.unit("Swordsman", 'A', 310, 325); // allowlisted by neither status test: captured
	REQUIRE(uncle != nullptr);

	StoreObjectsSpecialPower *chained = dynamic_cast<StoreObjectsSpecialPower *>(chainedCaster->findModule("StoreObjectsSpecialPower"));
	StoreObjectsSpecialPower *plain = dynamic_cast<StoreObjectsSpecialPower *>(plainCaster->findModule("StoreObjectsSpecialPower"));
	REQUIRE(chained);
	REQUIRE(plain);
	SpecialPowerModuleInterface *chainedStarter = SpecialPowerModules::findModule(*chainedCaster, w.power("TestStorePower"));
	SpecialPowerModuleInterface *plainStarter = SpecialPowerModules::findModule(*plainCaster, w.power("TestStorePowerNoChain"));
	REQUIRE(chainedStarter);
	REQUIRE(plainStarter);
	const unsigned chainedReadyBefore = chainedStarter->getReadyFrame();
	const unsigned plainReadyBefore = plainStarter->getReadyFrame();
	const Coord3D atChained{ 310, 318, 0 };
	const Coord3D atPlain{ 600, 600, 0 };

	// the cast goes through the same call the message handler drives (SpellCommands -> SpecialPowerModules::doSpecialPowerAtLocation; RW 0x8CC574 with the base 0x89816C)
	chainedStarter->doSpecialPowerAtLocation(atChained, 0);
	plainStarter->doSpecialPowerAtLocation(atPlain, 0);

	// both abilities unpack, prepare (PreparationTime 1) and trigger within a handful of frames
	for (int i = 0; i < 30; ++i)
	{
		w.logic->runLogicFrame();
	}

	CHECK(chained->abilitiesTriggered() == 1);
	CHECK(plain->abilitiesTriggered() == 1);
	// the capture (RW 0x8D1EA7): the allowlisted allied unit only; the UNSELECTABLE / HORDE_MEMBER members and the enemy were not captured
	CHECK(chained->storedObjects().size() == 1);
	CHECK(chained->storedObjects()[0] == uncle->getID());
	// ChainedButton set: the first power pays nothing on trigger (RW 0x853F2A: vslot 0x3C only when data + 0xC0 is empty)
	CHECK(chainedStarter->getReadyFrame() == chainedReadyBefore);
	// no ChainedButton: the recharge runs and the frame moves
	CHECK(plainStarter->getReadyFrame() != plainReadyBefore);

	// the casting ability is the Ennorath pair exactly: a SPECIAL_STORE_LIST_2 whose ChainedButton is set; the fixture of the matrix watches readyFrame, so only the
	// trigger counter sees it fire; the mod's own data was not touched
	CHECK(chained->storedObjects()[0] != enemy->getID());
	CHECK(chainedStarter->getReadyFrame() == chainedReadyBefore);
}
