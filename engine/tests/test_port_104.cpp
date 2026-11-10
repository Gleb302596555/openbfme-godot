// OpenBFME unit tests. GPL-3.0.
// PORT-104 (issue #264): CreateObjectDie / DebrisPortionOfSelf — parsed and stored, never read by RotWK 2.01.
//
// Evidence (full table in docs/ennorath/PORT104_RW_ANALYSIS.md):
//   * Field name RW 0xC610FC, field-table entry RW 0xC61130 in the CreateObjectDie table RW 0xC61120:
//     CreationList (+0x38), DebrisPortionOfSelf (+0x3C, parseAsciiString), UpgradeRequired (+0x40).
//   * CreateObjectDie::onDie RW 0x888F04 (read by the MODULES-1 lane, see ExtraDieModules.h): DieMux gate, the
//     UpgradeRequired checks, then the CreationList created with the dying object as primary — +0x3C is not read.
//   * DONOR BFME2 1.06 (byte-exact decomp, Open-BFME-2): the same table at 0xC4A928, onDie 0x485C86 reads +0x38/+0x40
//     only, the module-data constructor 0x485D88 only clears the +0x3C string; a whole-tree search finds no reader
//     of the field in any class.
//
// So RotWK has no "leave its debris" behaviour in this field to port: the debris of a destroyed trebuchet or troll
// sling comes from its CreationList alone. These tests pin that (they fail if a spawn of the named debris template
// is added next to the CreationList path), and the last test holds the neighbouring behaviour that *is* RotWK's:
// without the field, the same death creates the same CreationList objects.
#include "doctest.h"

#include "CombatTestUtil.h"

#include "GameLogic/Module/ExtraDieModules.h"
#include "GameLogic/ObjectCreationList.h"

using namespace combattest;

namespace
{

const char kObjects[] =
	"Object Port104Wreck\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"End\n"
	// the object the dead field names: a real template, so a reader *could* spawn it — RotWK does not
	"Object Port104DebrisPiece\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"End\n"
	"Object Port104SiegeWithDebris\n"
	"  KindOf = STRUCTURE SELECTABLE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryMinorRadius = 10\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = CreateObjectDie ModuleTag_Die\n"
	"    CreationList = OCL_Port104Death\n"
	"    DebrisPortionOfSelf = Port104DebrisPiece\n"
	"  End\n"
	"End\n"
	"Object Port104SiegePlain\n"
	"  KindOf = STRUCTURE SELECTABLE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryMinorRadius = 10\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = CreateObjectDie ModuleTag_Die\n"
	"    CreationList = OCL_Port104Death\n"
	"  End\n"
	"End\n"
	"Object Port104SiegeDebrisOnly\n"
	"  KindOf = STRUCTURE SELECTABLE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryMinorRadius = 10\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = CreateObjectDie ModuleTag_Die\n"
	"    DebrisPortionOfSelf = Port104DebrisPiece\n"
	"  End\n"
	"End\n";

const char kLists[] =
	"ObjectCreationList OCL_Port104Death\n"
	"  CreateObject\n"
	"    ObjectNames = Port104Wreck\n"
	"  End\n"
	"End\n";

struct Port104World : CombatWorld
{
	ObjectCreationListStore store{ w.keys };
	ObjectCreationListStore *saved = TheObjectCreationListStore;

	Port104World()
		: CombatWorld(kObjects)
	{
		TheObjectCreationListStore = &store;
		w.fx.env.blocks.registerBlock("ObjectCreationList", [](INI *ini) { ObjectCreationListStore::parseObjectCreationListDefinitionGlobal(ini); });
		const std::string err = w.load(kLists, INI_LOAD_OVERWRITE, "objectcreationlist.ini");
		REQUIRE_MESSAGE(err.empty(), err);
	}
	~Port104World() { TheObjectCreationListStore = saved; }

	int countNamed(const char *name)
	{
		int n = 0;
		for (ObjectID id = 1; id < 64; ++id)
		{
			Object *o = byId(id);
			if (o && o->getTemplate()->getName() == name)
			{
				++n;
			}
		}
		return n;
	}

	CreateObjectDie *dieModule(Object *o)
	{
		BehaviorModule *m = o->findModule("CreateObjectDie");
		REQUIRE(m);
		CreateObjectDie *d = dynamic_cast<CreateObjectDie *>(m);
		REQUIRE(d);
		return d;
	}
};

const CreateObjectDieModuleData *dieData(Port104World &w, const char *objectName)
{
	const ThingTemplate *t = w.w.get(objectName);
	REQUIRE(t);
	const ThingTemplate::Nugget *n = t->behaviorModules().find("ModuleTag_Die");
	REQUIRE(n);
	const CreateObjectDieModuleData *d = dynamic_cast<const CreateObjectDieModuleData *>(n->data.get());
	REQUIRE(d);
	return d;
}

} // namespace

// PORT-104: the field parses into its own member at +0x3C, next to CreationList (+0x38) — the half of the verdict
// the port already had; pinned here because the stop rests on it.
TEST_CASE("PORT-104: DebrisPortionOfSelf parses into CreateObjectDieModuleData, CreationList unchanged")
{
	Port104World w;
	const CreateObjectDieModuleData *with = dieData(w, "Port104SiegeWithDebris");
	CHECK(with->m_debrisPortionOfSelf == "Port104DebrisPiece");
	CHECK(with->m_creationList == "OCL_Port104Death");
	const CreateObjectDieModuleData *plain = dieData(w, "Port104SiegePlain");
	CHECK(plain->m_debrisPortionOfSelf.empty());
	CHECK(plain->m_creationList == "OCL_Port104Death");
	const CreateObjectDieModuleData *only = dieData(w, "Port104SiegeDebrisOnly");
	CHECK(only->m_debrisPortionOfSelf == "Port104DebrisPiece");
	CHECK(only->m_creationList.empty());
}

// PORT-104: a death with DebrisPortionOfSelf creates exactly the CreationList's object (RW 0x888F04 reads +0x38
// and +0x40 only); no object of the template the field names appears. This fails if a debris spawn is invented.
TEST_CASE("PORT-104: death with DebrisPortionOfSelf creates the CreationList object only, no debris piece")
{
	Port104World w;
	Object *siege = w.unit("Port104SiegeWithDebris", 'B', 400, 300);
	REQUIRE(siege);
	CreateObjectDie *die = w.dieModule(siege);
	const size_t before = w.logic->getObjectCount();
	siege->kill(DAMAGE_UNRESISTABLE, DEATH_NORMAL);
	CHECK(siege->isEffectivelyDead());
	CHECK(die->createdCount() == 1);
	CHECK(w.logic->getObjectCount() == before + 1);
	CHECK(w.countNamed("Port104Wreck") == 1);
	CHECK(w.countNamed("Port104DebrisPiece") == 0);
}

// PORT-104: the field alone (no CreationList) creates nothing at all on death.
TEST_CASE("PORT-104: DebrisPortionOfSelf without a CreationList creates nothing on death")
{
	Port104World w;
	Object *siege = w.unit("Port104SiegeDebrisOnly", 'B', 400, 300);
	REQUIRE(siege);
	CreateObjectDie *die = w.dieModule(siege);
	const size_t before = w.logic->getObjectCount();
	siege->kill(DAMAGE_UNRESISTABLE, DEATH_NORMAL);
	CHECK(siege->isEffectivelyDead());
	CHECK(die->createdCount() == 0);
	CHECK(w.logic->getObjectCount() == before);
	CHECK(w.countNamed("Port104DebrisPiece") == 0);
}

// PORT-104 (neighbour): without the field the same death is unchanged — the CreationList still runs.
TEST_CASE("PORT-104: death without DebrisPortionOfSelf still creates the CreationList object")
{
	Port104World w;
	Object *siege = w.unit("Port104SiegePlain", 'B', 400, 300);
	REQUIRE(siege);
	CreateObjectDie *die = w.dieModule(siege);
	const size_t before = w.logic->getObjectCount();
	siege->kill(DAMAGE_UNRESISTABLE, DEATH_NORMAL);
	CHECK(siege->isEffectivelyDead());
	CHECK(die->createdCount() == 1);
	CHECK(w.logic->getObjectCount() == before + 1);
	CHECK(w.countNamed("Port104Wreck") == 1);
}
