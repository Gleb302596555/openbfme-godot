// OpenBFME unit tests. GPL-3.0.
// LOCAL-120 (fork issue 804): summoned and created objects (both RotWK 2.01 and BFME2 1.06):
//   - P-165: SPECIAL_SPELL_BOOK_EAGLE_ALLIES (0x60) location shroud rule in ActionManager::canDoSpecialPowerAtLocation
//     (RW 0x82DFB7 -> table RW 0x82EE0D / 0x82EDE1 -> RW 0x82ED95: RW 0xB4D9A0 != CELLSHROUD_SHROUDED (2);
//     DONOR BFME2 0x0041D60B ActionManagerCanDoSpecialPowerAtLocation.cpp:293-294) vs ground summon RW 0x82E586 (CLEAR == 0).
//   - P-168: ObjectCreationUpgrade::update (RW 0x8B8464, loop RW 0x8B86DB .. 0x8B8708; DONOR BFME2 0x004B42DB
//     ObjectCreationUpgrade.cpp:661-670) walks spawned object's behavior modules for SlavedUpdateInterface (slot 0x68)
//     and calls onEnslave(owner) (slot 4) so DieOnMastersDeath kills the spawned object when its creator dies.
//   - P-169: Locomotor::locomotorMaintainCurrentPosition (RW 0x5E7CC7, tail RW 0x5E7E64 .. 0x5E7E84 -> RW 0x5E76AC
//     case 0 RW 0x5E774A .. 0x5E7816 -> RW 0x70BA76) snaps an idle NO_Z_MOTIVE_FORCE ground unit created above terrain
//     (e.g. OCL ABSOLUTE_ANGLE from a projectile detonation) to its layer surface unless zMotionSuppressed (DISABLED_HELD / flight).

#include "doctest.h"

#include "CombatTestUtil.h"
#include "EconTestUtil.h"
#include "EnnorathScenario.h"

#include "Common/Player.h"
#include "Common/SpecialPower.h"
#include "Common/Upgrade.h"
#include "GameLogic/ActionManager.h"
#include "GameLogic/Damage.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/ExtraModules.h"
#include "GameLogic/Module/LogicModules.h"
#include "GameLogic/Module/PhysicsBehavior.h"
#include "GameLogic/Module/SlaveWatcherBehavior.h"
#include "GameLogic/Module/SlavedUpdate.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/ObjectCreationList.h"
#include "GameLogic/System/ShroudManager.h"

#include <cmath>
#include <string>
#include <vector>

namespace
{

const char kLocal120SpikesIni[] =
	"Upgrade L120RazorSpines\n"
	"  Type = OBJECT\n"
	"End\n"
	"Upgrade L120FreeSpikes\n"
	"  Type = OBJECT\n"
	"End\n"
	"Object L120Spikes\n"
	"  KindOf = STRUCTURE IMMOBILE NOT_AUTOACQUIRABLE UNATTACKABLE\n"
	"  Body = HighlanderBody ModuleTag_Body\n"
	"    MaxHealth = 1.0\n"
	"  End\n"
	"  Behavior = SlavedUpdate ModuleTag_Slave\n"
	"    DieOnMastersDeath = Yes\n"
	"  End\n"
	"End\n"
	"Object L120FreeSpikesObj\n"
	"  KindOf = STRUCTURE IMMOBILE NOT_AUTOACQUIRABLE UNATTACKABLE\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100.0\n"
	"  End\n"
	"  Behavior = SlavedUpdate ModuleTag_Slave\n"
	"    DieOnMastersDeath = No\n"
	"  End\n"
	"End\n"
	"Object L120Fortress\n"
	"  KindOf = STRUCTURE SELECTABLE\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 1000.0\n"
	"  End\n"
	"  Behavior = SlaveWatcherBehavior WatchTheDrake\n"
	"    LetSlaveLive = Yes\n"
	"  End\n"
	"  Behavior = ObjectCreationUpgrade CreateTheSpikes\n"
	"    TriggeredBy = L120RazorSpines\n"
	"    Delay = 0.0\n"
	"    ThingToSpawn = L120Spikes\n"
	"  End\n"
	"  Behavior = ObjectCreationUpgrade CreateFreeSpikes\n"
	"    TriggeredBy = L120FreeSpikes\n"
	"    Delay = 0.0\n"
	"    ThingToSpawn = L120FreeSpikesObj\n"
	"  End\n"
	"End\n";

const char kLocal120GroundSnapIni[] =
	"Locomotor L120GroundLoco\n"
	"  Surfaces = GROUND RUBBLE\n"
	"  Speed = 52\n"
	"  TurnTime = 400\n"
	"  Acceleration = 1000\n"
	"  Braking = 500\n"
	"  ZAxisBehavior = NO_Z_MOTIVE_FORCE\n"
	"  Appearance = TWO_LEGS\n"
	"  StickToGround = Yes\n"
	"End\n"
	"Object L120SummonedHero\n"
	"  KindOf = INFANTRY SELECTABLE HERO SUMMONED\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 1000.0\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = L120GroundLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 52\n"
	"  End\n"
	"  Behavior = PhysicsBehavior ModuleTag_Phys\n"
	"    GravityMult = 1.0\n"
	"  End\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 5.6\n"
	"  GeometryHeight = 16.0\n"
	"End\n";

const char kLocal120GroundSnapOcl[] =
	"ObjectCreationList OCL_L120SpawnHero\n"
	"  CreateObject\n"
	"    ObjectNames = L120SummonedHero\n"
	"    Count = 1\n"
	"    IgnoreCommandPointLimit = Yes\n"
	"    Disposition = ABSOLUTE_ANGLE\n"
	"    DispositionAngle = 90\n"
	"    Offset = X:0 Y:0 Z:0\n"
	"  End\n"
	"End\n";

const char kLocal120ShroudSp[] =
	"SpecialPower L120EaglePower\n"
	"  Enum = SPECIAL_SPELL_BOOK_EAGLE_ALLIES\n"
	"  Flags = WATER_OK\n"
	"  ReloadTime = 1000\n"
	"  RadiusCursorRadius = 50.0\n"
	"End\n"
	"SpecialPower L120EntPower\n"
	"  Enum = SPECIAL_SPELL_BOOK_ENT_ALLIES\n"
	"  ReloadTime = 1000\n"
	"  RadiusCursorRadius = 50.0\n"
	"End\n";

const char kLocal120ShroudOcl[] =
	"ObjectCreationList OCL_L120Dummy\n"
	"  CreateObject\n"
	"    ObjectNames = L120SpellBook\n"
	"    Count = 1\n"
	"  End\n"
	"End\n";

const char kLocal120ShroudObj[] =
	"Object L120SpellBook\n"
	"  KindOf = IMMOBILE UNATTACKABLE\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100.0\n"
	"  End\n"
	"  Behavior = OCLSpecialPower ModuleTag_Eagle\n"
	"    SpecialPowerTemplate = L120EaglePower\n"
	"    OCL = OCL_L120Dummy\n"
	"  End\n"
	"  Behavior = OCLSpecialPower ModuleTag_Ent\n"
	"    SpecialPowerTemplate = L120EntPower\n"
	"    OCL = OCL_L120Dummy\n"
	"  End\n"
	"End\n";

Object *findFirstByTemplate(GameLogic &logic, const std::string &name)
{
	for (Object *o = logic.getFirstObject(); o; o = o->getNextObject())
	{
		if (o->getTemplate() && o->getTemplate()->getName() == name)
		{
			return o;
		}
	}
	return nullptr;
}

void stepAllPhases(GameLogic &logic, int frames)
{
	for (int i = 0; i < frames; ++i)
	{
		for (int phase = 1; phase <= 6; ++phase)
		{
			logic.update(phase);
		}
	}
}

struct ScopedOCLAndSP
{
	SpecialPowerStore spStore;
	ObjectCreationListStore oclStore;
	SpecialPowerStore *prevSp = nullptr;
	ObjectCreationListStore *prevOcl = nullptr;

	explicit ScopedOCLAndSP(logictest::LogicWorld &lw)
		: oclStore(lw.w.keys)
	{
		prevSp = TheSpecialPowerStore;
		prevOcl = TheObjectCreationListStore;
		TheSpecialPowerStore = &spStore;
		TheObjectCreationListStore = &oclStore;
		lw.w.fx.env.blocks.registerBlock("SpecialPower", [](INI *ini) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(ini); });
		lw.w.fx.env.blocks.registerBlock("ObjectCreationList", [](INI *ini) { ObjectCreationListStore::parseObjectCreationListDefinitionGlobal(ini); });
		SpecialPowerModules::registerAll(lw.w.modules);
	}
	~ScopedOCLAndSP()
	{
		TheSpecialPowerStore = prevSp;
		TheObjectCreationListStore = prevOcl;
	}
};

} // namespace

// Expectations from RotWK 2.01 RW 0x82DFB7 -> table RW 0x82EE0D / RW 0x82EDE1 -> RW 0x82ED95 (0x60 != CELLSHROUD_SHROUDED)
// and RW 0x82E586 (0x5F == CELLSHROUD_CLEAR); DONOR BFME2 0x0041D60B ActionManagerCanDoSpecialPowerAtLocation.cpp:293-294.
TEST_CASE("LOCAL-120 P-165 synthetic: SPECIAL_SPELL_BOOK_EAGLE_ALLIES (0x60) allows CLEAR and FOGGED, refuses SHROUDED; ground summon refuses FOGGED (RW 0x82DFB7 / RW 0x82ED95)")
{
	combattest::CombatWorld cw;
	ScopedOCLAndSP sp(cw);
	REQUIRE_MESSAGE(cw.w.load(kLocal120ShroudSp, INI_LOAD_OVERWRITE, "sp.ini").empty(), "sp.ini");
	REQUIRE_MESSAGE(cw.w.load(kLocal120ShroudOcl, INI_LOAD_OVERWRITE, "ocl.ini").empty(), "ocl.ini");
	REQUIRE_MESSAGE(cw.w.load(kLocal120ShroudObj, INI_LOAD_OVERWRITE, "obj.ini").empty(), "obj.ini");

	ShroudManager shroud(*cw.logic);
	shroud.init(20.0f, 0);
	shroud.setExtent(0.0f, 0.0f, 1000.0f, 1000.0f, 20.0f);
	cw.logic->setShroud(&shroud);

	Object *book = cw.unit("L120SpellBook", 'A', 200.0f, 200.0f);
	REQUIRE(book != nullptr);
	const int pIdx = book->getControllingPlayer()->getPlayerIndex();
	const SpecialPowerTemplate *eagle = TheSpecialPowerStore->findSpecialPowerTemplate("L120EaglePower");
	const SpecialPowerTemplate *ent = TheSpecialPowerStore->findSpecialPowerTemplate("L120EntPower");
	REQUIRE(eagle != nullptr);
	REQUIRE(ent != nullptr);

	const Coord3D pt{ 500.0f, 500.0f, 0.0f };
	// 1. Initially every cell is SHROUDED (2): both eagle (0x60) and ent (0x5F) must refuse.
	REQUIRE(shroud.getStatusAt(pIdx, pt.x, pt.y) == CELLSHROUD_SHROUDED);
	CHECK_FALSE(ActionManager::canDoSpecialPowerAtLocation(*book, pt, eagle, 0));
	CHECK_FALSE(ActionManager::canDoSpecialPowerAtLocation(*book, pt, ent, 0));

	// 2. revealMapForPlayer (RW 0xB4F570) puts the map in FOGGED (1): eagle (0x60, RW 0x82ED95 != 2) allows, ent (0x5F, RW 0x82E586 == 0) refuses.
	shroud.revealMapForPlayer(pIdx);
	REQUIRE(shroud.getStatusAt(pIdx, pt.x, pt.y) == CELLSHROUD_FOGGED);
	CHECK(ActionManager::canDoSpecialPowerAtLocation(*book, pt, eagle, 0));
	CHECK_FALSE(ActionManager::canDoSpecialPowerAtLocation(*book, pt, ent, 0));

	// 3. revealMapForPlayerPermanently (RW 0xB4F5D0) puts the map in CLEAR (0): both allow.
	shroud.revealMapForPlayerPermanently(pIdx);
	REQUIRE(shroud.getStatusAt(pIdx, pt.x, pt.y) == CELLSHROUD_CLEAR);
	CHECK(ActionManager::canDoSpecialPowerAtLocation(*book, pt, eagle, 0));
	CHECK(ActionManager::canDoSpecialPowerAtLocation(*book, pt, ent, 0));

	cw.logic->setShroud(nullptr);
}

// Expectations from RotWK 2.01 RW 0x8B8464 (loop RW 0x8B86DB .. 0x8B8708 -> SlavedUpdateInterface::onEnslave RW 0x8A510B)
// and SlavedUpdate::update RW 0x8A5D00 (DieOnMastersDeath -> kill at RW 0x8A5DA6); DONOR BFME2 0x004B42DB ObjectCreationUpgrade.cpp:661-670.
TEST_CASE("LOCAL-120 P-168 synthetic: ObjectCreationUpgrade enslaves spawned SlavedUpdate objects so DieOnMastersDeath kills them on creator death (RW 0x8B86DB .. 0x8B8708)")
{
	econtest::Fx f(kLocal120SpikesIni);
	Object *fortress = f.makeAt("L120Fortress", "Alice", 100.0f, 100.0f);
	REQUIRE(fortress != nullptr);
	const ObjectID fortId = fortress->getID();

	const UpgradeTemplate *utSpikes = TheUpgradeCenter->findUpgrade("L120RazorSpines");
	const UpgradeTemplate *utFree = TheUpgradeCenter->findUpgrade("L120FreeSpikes");
	REQUIRE(utSpikes != nullptr);
	REQUIRE(utFree != nullptr);
	fortress->giveUpgrade(utSpikes);
	fortress->giveUpgrade(utFree);
	f.frames(3);

	Object *spikes = findFirstByTemplate(*f.logic, "L120Spikes");
	Object *freeSpikes = findFirstByTemplate(*f.logic, "L120FreeSpikesObj");
	REQUIRE(spikes != nullptr);
	REQUIRE(freeSpikes != nullptr);
	const ObjectID spikesId = spikes->getID();
	const ObjectID freeId = freeSpikes->getID();

	// Both spawned objects must have been enslaved to the fortress (RW 0x8B86DB .. 0x8B8708).
	SlavedUpdateInterface *slavedSpikes = SlavedUpdateInterface::of(*spikes);
	SlavedUpdateInterface *slavedFree = SlavedUpdateInterface::of(*freeSpikes);
	REQUIRE(slavedSpikes != nullptr);
	REQUIRE(slavedFree != nullptr);
	CHECK(slavedSpikes->getSlaverID() == fortId);
	CHECK(slavedFree->getSlaverID() == fortId);

	// Kill the fortress (SlaveWatcherBehavior has LetSlaveLive = Yes, so only SlavedUpdate::DieOnMastersDeath kills L120Spikes).
	fortress->kill(DAMAGE_UNRESISTABLE, 0);
	f.frames(10);

	Object *spikesAfter = f.logic->findObjectByID(spikesId);
	CHECK((spikesAfter == nullptr || spikesAfter->isEffectivelyDead()));

	// Neighbouring behaviour: DieOnMastersDeath = No stays alive after the creator dies.
	Object *freeAfter = f.logic->findObjectByID(freeId);
	REQUIRE(freeAfter != nullptr);
	CHECK_FALSE(freeAfter->isEffectivelyDead());
}

// Expectations from RotWK 2.01 RW 0x5E7CC7 (maintainCurrentPosition tail RW 0x5E7E64 .. 0x5E7E84 -> RW 0x5E76AC case 0
// RW 0x5E774A .. 0x5E7816 -> RW 0x70BA76) and RW 0x5E774A DISABLED_HELD suppression.
TEST_CASE("LOCAL-120 P-169 synthetic: idle NO_Z_MOTIVE_FORCE unit spawned above ground by OCL ABSOLUTE_ANGLE snaps to ground in maintainCurrentPosition (RW 0x5E7CC7 / RW 0x5E774A)")
{
	combattest::CombatWorld cw(kLocal120GroundSnapIni);
	ScopedOCLAndSP sp(cw);
	REQUIRE_MESSAGE(cw.w.load(kLocal120GroundSnapOcl, INI_LOAD_OVERWRITE, "ocl.ini").empty(), "ocl.ini");

	Object *caster = cw.unit("L120SummonedHero", 'A', 100.0f, 100.0f);
	REQUIRE(caster != nullptr);

	const ObjectCreationList *ocl = TheObjectCreationListStore->findObjectCreationList("OCL_L120SpawnHero");
	REQUIRE(ocl != nullptr);

	const float groundZ = cw.logic->getGroundHeight(200.0f, 200.0f);
	const Coord3D airborne{ 200.0f, 200.0f, groundZ + 25.0f };
	std::vector<Object *> made = ocl->create(*cw.logic, caster, airborne);
	REQUIRE(made.size() == 1);
	Object *hero = made[0];
	REQUIRE(hero != nullptr);
	// Immediately after OCL ABSOLUTE_ANGLE (RW 0x5F1F93 -> 0x5F2079), hero is at the detonation height.
	CHECK(hero->getPosition()->z == groundZ + 25.0f);

	// After logic frames run maintainCurrentPosition (RW 0x5E7CC7 -> RW 0x5E774A .. 0x5E7816 -> RW 0x70BA76), hero stands on ground.
	stepAllPhases(*cw.logic, 2);
	CHECK(hero->getPosition()->z == groundZ);

	// Neighbouring behaviour: DISABLED_HELD (RW 0x5E774A, obj + 0x1C8 bit 3) suppresses the ground snap.
	std::vector<Object *> madeHeld = ocl->create(*cw.logic, caster, Coord3D{ 260.0f, 200.0f, groundZ + 25.0f });
	REQUIRE(madeHeld.size() == 1);
	Object *held = madeHeld[0];
	held->setDisabled(3, cw.logic->getFrame() + 100);
	stepAllPhases(*cw.logic, 2);
	CHECK(held->getPosition()->z == groundZ + 25.0f);
}

// Expectations from Ennorath 1.9.8.3 INI (specialpower.ini SpellBookEagleAllies, wildfortress.ini WildFortress +
// Upgrade_WildFortressRazorSpines -> WildFortressRazorSpines, objectcreationlist.inc OCL_GloinSpawnWeapon -> DwarvenGloin_OinSummoned)
// and RotWK 2.01 RW 0x82ED95, RW 0x8B86DB .. 0x8B8708, RW 0x5E7CC7 / RW 0x5E774A.
TEST_CASE("LOCAL-120 ENNORATH scenario: P-165 SpellBookEagleAllies shroud, P-168 WildFortress RazorSpines die with fortress, P-169 OCL_GloinSpawnWeapon lands on ground")
{
	ennorathscenario::World w;
	if (!w.load("LOCAL-120 ENNORATH scenario"))
	{
		return;
	}

	// P-165: SpellBookEagleAllies (Enum = SPECIAL_SPELL_BOOK_EAGLE_ALLIES = 0x60) in Ennorath
	{
		structtest::Arena arena(w.shared, "FactionElves", "FactionWild");
		ShroudManager shroud(arena.logic);
		shroud.init(20.0f, 0);
		shroud.setExtent(0.0f, 0.0f, 1200.0f, 1200.0f, 20.0f);
		arena.logic.setShroud(&shroud);

		arena.logic.economy().context().gameKind = 3;
		Object *book = w.place(arena, "ElvesSpellBook", 0, 200.0f, 200.0f);
		REQUIRE(book != nullptr);
		Player *p0 = book->getControllingPlayer();
		REQUIRE(p0 != nullptr);
		SpellGameMode sm = p0->science().mode();
		sm.skirmishOrMultiplayer = false;
		p0->science().bind(p0->getPlayerTemplate(), sm, nullptr);
		const int pIdx = p0->getPlayerIndex();
		const SpecialPowerTemplate *eagles = TheSpecialPowerStore->findSpecialPowerTemplate("SpellBookEagleAllies");
		REQUIRE(eagles != nullptr);
		for (ScienceType st : eagles->getRequiredSciences())
		{
			p0->science().addScience(st);
		}
		stepAllPhases(arena.logic, 2);
		SpecialPowerModuleInterface *spMod = SpecialPowerModules::findModule(*book, eagles);
		REQUIRE(spMod != nullptr);
		REQUIRE(spMod->requirementsMet());
		const Coord3D pt{ 300.0f, 300.0f, 0.0f };

		CHECK(shroud.getStatusAt(pIdx, pt.x, pt.y) == CELLSHROUD_SHROUDED);
		CHECK_FALSE(ActionManager::canDoSpecialPowerAtLocation(*book, pt, eagles, 0));

		shroud.revealMapForPlayer(pIdx);
		CHECK(shroud.getStatusAt(pIdx, pt.x, pt.y) == CELLSHROUD_FOGGED);
		CHECK(ActionManager::canDoSpecialPowerAtLocation(*book, pt, eagles, 0));

		shroud.revealMapForPlayerPermanently(pIdx);
		CHECK(shroud.getStatusAt(pIdx, pt.x, pt.y) == CELLSHROUD_CLEAR);
		CHECK(ActionManager::canDoSpecialPowerAtLocation(*book, pt, eagles, 0));

		arena.logic.setShroud(nullptr);
	}

	// P-168: WildFortress + Upgrade_WildFortressRazorSpines -> WildFortressRazorSpines dies when fortress is destroyed
	{
		structtest::Arena arena(w.shared, "FactionWild", "FactionMen");
		Object *fortress = w.place(arena, "WildFortressCitadel", 0, 400.0f, 400.0f);
		REQUIRE(fortress != nullptr);
		const ObjectID fortId = fortress->getID();

		const UpgradeTemplate *utSpines = TheUpgradeCenter->findUpgrade("Upgrade_WildFortressRazorSpines");
		REQUIRE(utSpines != nullptr);
		fortress->giveUpgrade(utSpines);
		stepAllPhases(arena.logic, 3);

		Object *spines = findFirstByTemplate(arena.logic, "WildFortressRazorSpines");
		REQUIRE(spines != nullptr);
		const ObjectID spinesId = spines->getID();
		SlavedUpdateInterface *slaved = SlavedUpdateInterface::of(*spines);
		REQUIRE(slaved != nullptr);
		CHECK(slaved->getSlaverID() == fortId);

		fortress->kill(DAMAGE_UNRESISTABLE, 0);
		stepAllPhases(arena.logic, 10);

		Object *spinesAfter = arena.logic.findObjectByID(spinesId);
		CHECK((spinesAfter == nullptr || spinesAfter->isEffectivelyDead()));
	}

	// P-169: OCL_GloinSpawnWeapon creates DwarvenGloin_OinSummoned at airborne detonation pos -> lands on ground
	{
		structtest::Arena arena(w.shared, "FactionDwarves", "FactionWild");
		Object *oin = w.place(arena, "DwarvenOin", 0, 300.0f, 300.0f);
		REQUIRE(oin != nullptr);
		const ObjectCreationList *ocl = TheObjectCreationListStore->findObjectCreationList("OCL_GloinSpawnWeapon");
		REQUIRE(ocl != nullptr);

		const float groundZ = arena.logic.getGroundHeight(340.0f, 300.0f);
		std::vector<Object *> made = ocl->create(arena.logic, oin, Coord3D{ 340.0f, 300.0f, groundZ + 20.0f });
		REQUIRE(made.size() == 1);
		Object *gloin = made[0];
		REQUIRE(gloin != nullptr);
		REQUIRE(gloin->getTemplate()->getName() == "DwarvenGloin_OinSummoned");
		CHECK(gloin->getPosition()->z == groundZ + 20.0f);

		stepAllPhases(arena.logic, 2);
		CHECK(gloin->getPosition()->z == groundZ);
	}
}
