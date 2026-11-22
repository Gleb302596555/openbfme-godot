// PORT-273: AIGroup::groupDoSpecialPowerAtLocation must run the point ActionManager validation before dispatching a cast.
// RotWK 2.01 RW 0x76F774 enters the location group path; RW 0x76F881 calls ActionManager::canDoSpecialPowerAtLocation (RW 0x82DFB7),
// and RW 0x76F88A invokes the module only when that answer is true. RW 0x82DEA6 scans within RadiusCursorRadius for a live object
// accepted by ObjectFilter when NEEDS_OBJECT_FILTER is set. Synthetic INI: an OCL power requiring a friendly target at the clicked point.
// No game installation or Ennorath files are used.

#include "doctest.h"
#include "CombatTestUtil.h"

#include "Common/INI.h"
#include "Common/SpecialPower.h"
#include "GameLogic/GameLogicDispatch.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/ObjectCreationList.h"
#include "GameLogic/SpellCommands.h"

#include <string>

namespace
{
const char kPort273Objects[] =
	"SpecialPower Port273PointPower\n"
	"  Enum = SPECIAL_SPELL_BOOK_BOMBARD\n"
	"  ReloadTime = 1\n"
	"  RadiusCursorRadius = 50\n"
	"  Flags = NEEDS_OBJECT_FILTER LIMIT_DISTANCE\n"
	"  MaxCastRange = 9999\n"
	"  ObjectFilter = ANY +Port273Target ALLIES\n"
	"End\n"
	"ObjectCreationList Port273PointOCL\n"
	"  CreateObject\n"
	"    ObjectNames = Port273Summon\n"
	"    Count = 1\n"
	"  End\n"
	"End\n"
	"Object Port273Caster\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryMinorRadius = 10\n"
	"  GeometryHeight = 20\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = PlainArmor\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"  Behavior = OCLSpecialPower ModuleTag_PointOCL\n"
	"    SpecialPowerTemplate = Port273PointPower\n"
	"    OCL = Port273PointOCL\n"
	"    CreateLocation = CREATE_AT_LOCATION\n"
	"  End\n"
	"End\n"
	"Object Port273Target\n"
	"  KindOf = STRUCTURE IMMOBILE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryMinorRadius = 10\n"
	"  GeometryHeight = 10\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = PlainArmor\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"End\n"
	"Object Port273Summon\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 5\n"
	"  GeometryMinorRadius = 5\n"
	"  GeometryHeight = 10\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = PlainArmor\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"End\n";

struct Port273World : combattest::CombatWorld
{
	SpecialPowerStore powers;
	SpecialPowerStore *savedPowers = TheSpecialPowerStore;
	ObjectCreationListStore ocls{ w.keys };
	ObjectCreationListStore *savedOcls = TheObjectCreationListStore;
	SpellCommands commands;
	GameLogicDispatch dispatch{ *logic };
	Object *caster = nullptr;

	Port273World()
	{
		TheSpecialPowerStore = &powers;
		TheObjectCreationListStore = &ocls;
		w.fx.env.blocks.registerBlock("SpecialPower", [](INI *ini) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(ini); });
		w.fx.env.blocks.registerBlock("ObjectCreationList", [](INI *ini) { ObjectCreationListStore::parseObjectCreationListDefinitionGlobal(ini); });
		SpecialPowerModules::registerAll(w.modules);
		commands.registerHandlers(dispatch);
		const std::string err = w.load(kPort273Objects, INI_LOAD_OVERWRITE, "port273.ini");
		REQUIRE_MESSAGE(err.empty(), err);
	}
	~Port273World()
	{
		TheSpecialPowerStore = savedPowers;
		TheObjectCreationListStore = savedOcls;
	}

	void setUp()
	{
		caster = unit("Port273Caster", 'A', 200.0f, 200.0f);
		REQUIRE(caster);
		for (int i = 0; i < 10; ++i)
		{
			logic->runLogicFrame();
		}
	}

	void castAt(const Coord3D &point)
	{
		const SpecialPowerTemplate *p = powers.findSpecialPowerTemplate("Port273PointPower");
		REQUIRE(p);
		GameMessage m(MSG_DO_SPECIAL_POWER_AT_LOCATION, caster->getControllingPlayer()->getPlayerIndex());
		m.appendIntegerArgument((int)p->getID());
		m.appendLocationArgument(point);
		m.appendObjectIDArgument(INVALID_ID);
		m.appendIntegerArgument(0);
		m.appendObjectIDArgument(caster->getID());
		dispatch.dispatch(m);
	}

	OCLSpecialPower *powerModule() const
	{
		for (const std::unique_ptr<BehaviorModule> &m : caster->modules())
		{
			if (auto *ocl = dynamic_cast<OCLSpecialPower *>(m.get()))
			{
				return ocl;
			}
		}
		return nullptr;
	}
};
} // namespace

TEST_CASE("PORT-273: location powers refuse a point without the required nearby object (RW 0x76F881 -> 0x82DFB7 -> 0x82DEA6)")
{
	Port273World f;
	f.setUp();
	OCLSpecialPower *module = f.powerModule();
	REQUIRE(module);
	f.castAt(Coord3D{ 400.0f, 400.0f, 0.0f });
	CHECK(module->created().empty());
	CHECK(f.commands.refused() == 1);
}

TEST_CASE("PORT-273: an eligible object at the point still permits the OCL cast (RW 0x82DEA6; adjacent behavior)")
{
	Port273World f;
	f.setUp();
	Object *target = f.unit("Port273Target", 'A', 400.0f, 400.0f);
	REQUIRE(target);
	OCLSpecialPower *module = f.powerModule();
	REQUIRE(module);
	f.castAt(Coord3D{ 400.0f, 400.0f, 0.0f });
	CHECK(module->created().size() == 1);
	CHECK(f.commands.casts() == 1);
}
