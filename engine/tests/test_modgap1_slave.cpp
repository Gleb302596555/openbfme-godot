// OpenBFME. MODGAP-1 tests, group 4 (MODULE_GAPS.md): SlaveWatcherBehavior (RW 0x887A35 update, 0x887AF5 setSlave, 0x8879C5 the kill), its link in
// ObjectCreationUpgrade's ThingToSpawn path (RW 0x8B86AB) and OCLUpdate (RW 0x89EC43 / 0x89EBBA) on synthetic data. The expected values are the data's own.
#include "doctest.h"
#include "CombatTestUtil.h"

#include "Common/Player.h"
#include "GameLogic/Module/OCLUpdate.h"
#include "GameLogic/Module/SlaveWatcherBehavior.h"
#include "GameLogic/ObjectCreationList.h"

using namespace combattest;

namespace
{
const char kSlaveUnit[] =
	"Object SlvUnit\n"
	"  KindOf = INFANTRY SELECTABLE SCORE\n"
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
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = DestroyDie ModuleTag_Destroy\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"End\n";

// a structure that makes its slave when it gets Upgrade_Barracks2 and watches it
std::string masterObject(const char *name, const char *watcherFields)
{
	return std::string("Object ") + name + "\n"
		"  KindOf = STRUCTURE\n"
		"  Geometry = CYLINDER\n"
		"  GeometryMajorRadius = 20\n"
		"  GeometryMinorRadius = 20\n"
		"  GeometryHeight = 20\n"
		"  ArmorSet\n"
		"    Conditions = None\n"
		"    Armor = NoArmor\n"
		"  End\n"
		"  Body = ActiveBody ModuleTag_Body\n"
		"    MaxHealth = 500\n"
		"  End\n"
		"  Behavior = DestroyDie ModuleTag_Destroy\n"
		"  End\n"
		"  Behavior = ObjectCreationUpgrade ModuleTag_Make\n"
		"    TriggeredBy = Upgrade_Barracks2\n"
		"    ThingToSpawn = SlvUnit\n"
		"    GrantUpgrade = Upgrade_Anything\n"
		"  End\n"
		"  Behavior = SlaveWatcherBehavior ModuleTag_Watch\n" + watcherFields + "  End\n"
		"End\n";
}

const std::string kObjects = std::string(kSlaveUnit) + masterObject("SlvMaster", "    GrantUpgrade = Upgrade_Insurance\n    RemoveUpgrade = Upgrade_Anything\n") +
	masterObject("SlvMasterLive", "    LetSlaveLive = Yes\n") + masterObject("SlvMasterShare", "    ShareUpgrades = Yes\n") +
	"Object OclMaker\n"
	"  KindOf = STRUCTURE\n"
	"  Behavior = OCLUpdate ModuleTag_Make\n"
	"    OCL = OCL_MakeUnit\n"
	"    MinDelay = 1000\n"
	"    MaxDelay = 1000\n"
	"    Amount = 2\n"
	"  End\n"
	"End\n";

struct SlaveWorld : CombatWorld
{
	ObjectCreationListStore store{ w.keys };
	ObjectCreationListStore *saved = TheObjectCreationListStore;
	SlaveWorld()
		: CombatWorld(kObjects.c_str())
	{
		TheObjectCreationListStore = &store;
		w.fx.env.blocks.registerBlock("ObjectCreationList", [](INI *ini) { ObjectCreationListStore::parseObjectCreationListDefinitionGlobal(ini); });
		const std::string err = w.load("ObjectCreationList OCL_MakeUnit\n  CreateObject\n    ObjectNames = SlvUnit\n    Count = 1\n    Disposition = LIKE_EXISTING\n  End\nEnd\n",
			INI_LOAD_OVERWRITE, "objectcreationlist.ini");
		REQUIRE_MESSAGE(err.empty(), err);
	}
	~SlaveWorld() { TheObjectCreationListStore = saved; }
	int count(const char *templateName)
	{
		int n = 0;
		for (Object *o = logic->getFirstObject(); o; o = o->getNextObject())
		{
			n += !o->isDestroyed() && o->getTemplate()->getName() == templateName ? 1 : 0;
		}
		return n;
	}
	static SlaveWatcherBehavior *watcher(Object *o) { return dynamic_cast<SlaveWatcherBehavior *>(o->findModule("SlaveWatcherBehavior")); }
};
} // namespace

TEST_CASE("modgap1 slave watcher: ObjectCreationUpgrade tells the watcher its slave; when the slave dies RemoveUpgrade is taken and GrantUpgrade given")
{
	SlaveWorld w;
	Object *master = w.unit("SlvMaster", 'A', 400, 400);
	SlaveWatcherBehavior *watcher = SlaveWorld::watcher(master);
	REQUIRE(watcher != nullptr);
	CHECK(watcher->getSlaveID() == INVALID_ID);
	master->giveUpgrade("Upgrade_Barracks2");
	w.frames(4);
	REQUIRE(watcher->getSlaveID() != INVALID_ID);
	Object *slave = w.byId(watcher->getSlaveID());
	REQUIRE(slave != nullptr);
	CHECK(slave->getTemplate()->getName() == "SlvUnit");
	CHECK(master->hasUpgrade("Upgrade_Anything")); // ObjectCreationUpgrade's GrantUpgrade
	CHECK_FALSE(master->hasUpgrade("Upgrade_Insurance"));
	slave->kill(DAMAGE_UNRESISTABLE, 0);
	w.frames(4);
	CHECK_FALSE(master->hasUpgrade("Upgrade_Anything"));  // the watcher's RemoveUpgrade
	CHECK(master->hasUpgrade("Upgrade_Insurance"));       // the watcher's GrantUpgrade
	CHECK(watcher->getSlaveID() == INVALID_ID);
}

TEST_CASE("modgap1 slave watcher: when the master goes the slave dies, unless LetSlaveLive")
{
	SUBCASE("without LetSlaveLive the slave is killed")
	{
		SlaveWorld w;
		Object *master = w.unit("SlvMaster", 'A', 400, 400);
		master->giveUpgrade("Upgrade_Barracks2");
		w.frames(4);
		const ObjectID slaveId = SlaveWorld::watcher(master)->getSlaveID();
		REQUIRE(w.byId(slaveId) != nullptr);
		w.logic->destroyObject(master);
		w.frames(4);
		Object *slave = w.byId(slaveId);
		CHECK((slave == nullptr || slave->isEffectivelyDead()));
	}
	SUBCASE("with LetSlaveLive it lives")
	{
		SlaveWorld w;
		Object *master = w.unit("SlvMasterLive", 'A', 400, 400);
		master->giveUpgrade("Upgrade_Barracks2");
		w.frames(4);
		const ObjectID slaveId = SlaveWorld::watcher(master)->getSlaveID();
		REQUIRE(w.byId(slaveId) != nullptr);
		w.logic->destroyObject(master);
		w.frames(4);
		Object *slave = w.byId(slaveId);
		REQUIRE(slave != nullptr);
		CHECK_FALSE(slave->isEffectivelyDead());
	}
}

TEST_CASE("modgap1 slave watcher: ShareUpgrades copies the master's upgrades to the slave, now and later (RW 0x887B1B, 0x887A66)")
{
	SlaveWorld w;
	Object *master = w.unit("SlvMasterShare", 'A', 400, 400);
	Object *other = w.unit("SlvUnit", 'A', 440, 400);
	master->giveUpgrade("Upgrade_Basic");
	SlaveWatcherBehavior *watcher = SlaveWorld::watcher(master);
	REQUIRE(watcher != nullptr);
	CHECK_FALSE(other->hasUpgrade("Upgrade_Basic"));
	watcher->setSlave(other->getID());
	CHECK(other->hasUpgrade("Upgrade_Basic")); // at once, from setSlave
	master->giveUpgrade("Upgrade_Harvest");
	w.frames(3);
	CHECK(other->hasUpgrade("Upgrade_Harvest")); // later, from the update
}

TEST_CASE("modgap1 OCLUpdate: it makes its list every MinDelay, Amount times, and then sleeps")
{
	SlaveWorld w;
	w.unit("OclMaker", 'A', 400, 400);
	CHECK(w.count("SlvUnit") == 0);
	w.frames(3);
	CHECK(w.count("SlvUnit") == 0); // the first call only schedules, the first list comes MinDelay (5 frames) later
	w.frames(40);
	CHECK(w.count("SlvUnit") == 2);  // Amount = 2
	w.frames(40);
	CHECK(w.count("SlvUnit") == 2);  // and no more
}

TEST_CASE("modgap1 OCLUpdate: the stop line names CreateAtEdge")
{
	const std::string line = OCLUpdate::stopLine();
	CHECK(line.compare(0, 8, "[S-5501]") == 0);
	CHECK(line.find("CreateAtEdge") != std::string::npos);
}
