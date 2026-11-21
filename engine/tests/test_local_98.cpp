// OpenBFME. LOCAL-98 tests (fork issue 763): death modules.
//   - StructureCollapseUpdate::getDisabledTypesToProcess (RW 0x00654FD1): collapses while DISABLED_USER_FROZEN.
#include "doctest.h"
#include "CombatTestUtil.h"
#include "EnnorathScenario.h"
#include "GameLogic/Module/CombatModules.h"

#include "GameLogic/Module/StructureModules.h"
#include "GameLogic/Module/TransitionDamageFX.h"
#include <vector>
#include "GameLogic/ObjectTypes.h"

#include <string>

using namespace combattest;

namespace
{
const char *kCollapseIni =
	"Object Local98CollapseBuilding\n"
	"  KindOf = STRUCTURE\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = StructureCollapseUpdate ModuleTag_Collapse\n"
	"    MinCollapseDelay = 1\n"
	"    MaxCollapseDelay = 1\n"
	"    CollapseDamping = 0.5\n"
	"    MaxShudder = 0.6\n"
	"    MinBurstDelay = 5\n"
	"    MaxBurstDelay = 5\n"
	"    BigBurstFrequency = 4\n"
	"  End\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 20\n"
	"  GeometryMinorRadius = 20\n"
	"  GeometryHeight = 20\n"
	"End\n";

StructureCollapseUpdate *findCollapse(Object &o)
{
	for (const std::unique_ptr<BehaviorModule> &m : o.modules())
	{
		if (auto *c = dynamic_cast<StructureCollapseUpdate *>(m.get()))
		{
			return c;
		}
	}
	return nullptr;
}
} // namespace

TEST_CASE("LOCAL-98 StructureCollapseUpdate: collapses while DISABLED_USER_FROZEN (RW 0x00654FD1)")
{
	CombatWorld cw;
	StructureModules::registerAll(cw.w.modules);
	const std::string err = cw.w.load(kCollapseIni, INI_LOAD_OVERWRITE, "local98_collapse.ini");
	REQUIRE_MESSAGE(err.empty(), err);

	Object *b = cw.unit("Local98CollapseBuilding", 'A', 400.0f, 400.0f);
	REQUIRE(b != nullptr);
	StructureCollapseUpdate *scu = findCollapse(*b);
	REQUIRE(scu != nullptr);

	// RW 0x00654FD1: UpdateInterface vtable 0x00C6902C slot 1 returns (1u << DISABLED_USER_FROZEN).
	CHECK(scu->getDisabledTypesToProcess() == (DisabledMaskType)(1u << DISABLED_USER_FROZEN));

	b->setDisabled(DISABLED_USER_FROZEN, 1000);
	b->kill(DAMAGE_UNRESISTABLE, DEATH_NORMAL);
	CHECK(scu->state() == COLLAPSESTATE_WAITINGFORCOLLAPSESTART);

	for (int f = 0; f < 40 && scu->state() != COLLAPSESTATE_DONE; ++f)
	{
		cw.logic->runLogicFrame();
	}
	CHECK(scu->state() == COLLAPSESTATE_DONE);
}

TEST_CASE("LOCAL-98 StructureCollapseUpdate: stays paused while DISABLED_HELD (RW 0x00654FD1)")
{
	CombatWorld cw;
	StructureModules::registerAll(cw.w.modules);
	const std::string err = cw.w.load(kCollapseIni, INI_LOAD_OVERWRITE, "local98_collapse_held.ini");
	REQUIRE_MESSAGE(err.empty(), err);

	Object *b = cw.unit("Local98CollapseBuilding", 'A', 400.0f, 400.0f);
	REQUIRE(b != nullptr);
	StructureCollapseUpdate *scu = findCollapse(*b);
	REQUIRE(scu != nullptr);

	b->setDisabled(DISABLED_HELD, 1000);
	b->kill(DAMAGE_UNRESISTABLE, DEATH_NORMAL);
	CHECK(scu->state() == COLLAPSESTATE_WAITINGFORCOLLAPSESTART);

	for (int f = 0; f < 10; ++f)
	{
		cw.logic->runLogicFrame();
	}
	// DISABLED_HELD is not in RW 0x00654FD1's mask, so the update stays paused.
	CHECK(scu->state() == COLLAPSESTATE_WAITINGFORCOLLAPSESTART);
}

namespace
{
const char *kTdfxIni =
	"Object Local98TdfxDoor\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"    MaxHealthDamaged = 75\n"
	"    MaxHealthReallyDamaged = 50\n"
	"  End\n"
	"  Behavior = TransitionDamageFX ModuleTag_Tdfx\n"
	"    PristineShowSubObject = DOOR_OK\n"
	"    PristineHideSubObject = DOOR_D1 DOOR_D2\n"
	"    DamagedShowSubObject = DOOR_D1\n"
	"    DamagedHideSubObject = DOOR_OK DOOR_D2\n"
	"  End\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryMinorRadius = 10\n"
	"  GeometryHeight = 10\n"
	"End\n";

class Local98SubObjectHooks : public ObjectClientHooks
{
public:
	std::vector<std::string> calls;
	void objectCreated(Object &) override {}
	void objectDestroyed(Object &) override {}
	void showSubObject(Object &, const std::string &name, bool visible, bool permanent) override
	{
		calls.push_back(name + (visible ? " show" : " hide") + (permanent ? " permanent" : ""));
	}
};
} // namespace

TEST_CASE("LOCAL-98 TransitionDamageFX: onDrawableBoundToObject applies pristine subobjects (RW 0x008BDEE5)")
{
	CombatWorld cw;
	TransitionDamageFXModule::registerClass(cw.w.modules);
	const std::string err = cw.w.load(kTdfxIni, INI_LOAD_OVERWRITE, "local98_tdfx.ini");
	REQUIRE_MESSAGE(err.empty(), err);

	Object *o = cw.unit("Local98TdfxDoor", 'A', 400.0f, 400.0f);
	REQUIRE(o != nullptr);
	Local98SubObjectHooks hooks;
	o->friend_bindToClient(&hooks);
	CHECK(hooks.calls == std::vector<std::string>{
		"DOOR_D1 hide permanent", "DOOR_D2 hide permanent", "DOOR_OK show permanent" });
	o->friend_bindToClient(nullptr);
}

TEST_CASE("LOCAL-98 TransitionDamageFX: onDrawableBoundToObject applies damaged subobjects when already damaged (RW 0x008BDEE5)")
{
	CombatWorld cw;
	TransitionDamageFXModule::registerClass(cw.w.modules);
	const std::string err = cw.w.load(kTdfxIni, INI_LOAD_OVERWRITE, "local98_tdfx_dmg.ini");
	REQUIRE_MESSAGE(err.empty(), err);

	Object *o = cw.unit("Local98TdfxDoor", 'A', 400.0f, 400.0f);
	REQUIRE(o != nullptr);
	DamageInfo hit;
	hit.m_input.m_amount = 30.0f;
	hit.m_input.m_damageType = DAMAGE_FLAME;
	o->attemptDamage(hit);
	REQUIRE(o->getBodyModule()->getDamageState() == BODY_DAMAGED);

	Local98SubObjectHooks hooks;
	o->friend_bindToClient(&hooks);
	CHECK(hooks.calls == std::vector<std::string>{
		"DOOR_OK hide permanent", "DOOR_D2 hide permanent", "DOOR_D1 show permanent" });
	o->friend_bindToClient(nullptr);
}

TEST_CASE("LOCAL-98 ENNORATH scenario: IsengardOrthancDoor Pristine subobjects on bind (RW 0x008BDEE5), GondorBarracks collapse while DISABLED_USER_FROZEN (RW 0x00654FD1), GondorFighter SlowDeathBehavior")
{
	ennorathscenario::World world;
	if (!world.load("LOCAL-98 Ennorath death modules scenario"))
	{
		return;
	}

	hudtest::Rig game(world.shared, "map mp fall back 8p", "FactionMen");
	game.logic().combat().setAutoAcquireEnabled(false);
	const int p1 = game.index();
	std::string err;

	// 1. Real Ennorath object with TransitionDamageFX PristineHide/ShowSubObject: IsengardOrthancDoor
	Object *door = game.game->createObject("IsengardOrthancDoor", p1, Coord3D{ 1700.0f, 2200.0f, 0.0f }, 0.0f, &err);
	REQUIRE_MESSAGE(door != nullptr, err);
	Local98SubObjectHooks hooks;
	door->friend_bindToClient(&hooks);
	CHECK(hooks.calls == std::vector<std::string>{
		"IBGATEDOORR_D1 hide permanent",
		"IBGATEDOORR_D2 hide permanent",
		"IBGATEDOORL_D1 hide permanent",
		"IBGATEDOORL_D2 hide permanent",
		"IBGATEDOORL show permanent",
		"IBGATEDOORR show permanent" });
	door->friend_bindToClient(nullptr);

	// 2. Real Ennorath building with StructureCollapseUpdate: GondorBarracks under DISABLED_USER_FROZEN
	Object *barracks = game.game->createObject("GondorBarracks", p1, Coord3D{ 1850.0f, 2200.0f, 0.0f }, 0.0f, &err);
	REQUIRE_MESSAGE(barracks != nullptr, err);
	StructureCollapseUpdate *scu = findCollapse(*barracks);
	REQUIRE(scu != nullptr);
	CHECK(scu->getDisabledTypesToProcess() == (DisabledMaskType)(1u << DISABLED_USER_FROZEN));
	const ObjectID barracksId = barracks->getID();
	barracks->setDisabled(DISABLED_USER_FROZEN, 1000);
	barracks->kill(DAMAGE_UNRESISTABLE, DEATH_NORMAL);
	for (int f = 0; f < 120 && game.logic().findObjectByID(barracksId) != nullptr; ++f)
	{
		game.logic().runLogicFrame();
	}
	// DestroyObjectWhenDone = Yes -> destroyed once collapse completes despite DISABLED_USER_FROZEN
	CHECK(game.logic().findObjectByID(barracksId) == nullptr);

	// 3. Real Ennorath unit with SlowDeathBehavior: GondorFighter
	Object *fighter = game.game->createObject("GondorFighter", p1, Coord3D{ 1950.0f, 2200.0f, 0.0f }, 0.0f, &err);
	REQUIRE_MESSAGE(fighter != nullptr, err);
	const ObjectID fighterId = fighter->getID();
	fighter->kill(DAMAGE_UNRESISTABLE, DEATH_NORMAL);
	bool activated = false;
	for (const std::unique_ptr<BehaviorModule> &m : fighter->modules())
	{
		if (auto *sd = dynamic_cast<SlowDeathBehavior *>(m.get()))
		{
			activated = activated || sd->isSlowDeathActivated();
		}
	}
	CHECK(activated);
	for (int f = 0; f < 200 && game.logic().findObjectByID(fighterId) != nullptr; ++f)
	{
		game.logic().runLogicFrame();
	}
	CHECK(game.logic().findObjectByID(fighterId) == nullptr);
}
