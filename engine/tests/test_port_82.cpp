// OpenBFME. PORT-82 (issue #214): in RotWK 2.01 AttributeModifierNugget/DamageFXType is parsed but not read.
// Synthetic INI only. RW evidence is in docs/ennorath/port82-attributemodifier-damagefxtype-rw.md.
#include "doctest.h"

#include "CombatTestUtil.h"

#include "GameClient/FXList.h"
#include "GameLogic/AttributeModifiers.h"
#include "GameLogic/Combat/WeaponDelivery.h"
#include "GameLogic/DamageFX.h"
#include "GameLogic/FXEvents.h"
#include "GameLogic/Module/ActiveBody.h"
#include "GameLogic/Object/AttributeModifierPool.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/WeaponNugget.h"
#include "GameLogic/WeaponStores.h"

#include <string>
#include <vector>

using namespace combattest;

namespace
{
const char kPort82Data[] =
	"FXList FX_Port82Modifier\n"
	"End\n"
	"FXList FX_Port82DamageNugget\n"
	"End\n"
	"DamageFX Port82DamageFX\n"
	"  AmountForMajorFX = SWORD_SLASH 30\n"
	"  MajorFX = SWORD_SLASH FX_Port82Modifier\n"
	"  MinorFX = SWORD_SLASH FX_Port82Modifier\n"
	"  AmountForMajorFX = GOOD_ARROW_PIERCE 30\n"
	"  MajorFX = GOOD_ARROW_PIERCE FX_Port82DamageNugget\n"
	"  MinorFX = GOOD_ARROW_PIERCE FX_Port82DamageNugget\n"
	"End\n"
	"ModifierList Port82Buff\n"
	"  Category = BUFF\n"
	"  Duration = 20000\n"
	"  Modifier = RECHARGE_TIME 100%\n"
	"End\n"
	"Object Port82Source\n"
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"End\n"
	"Object Port82Victim\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = NoArmor\n"
	"    DamageFX = Port82DamageFX\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = AttributeModifierPoolUpdate ModuleTag_Pool\n"
	"  End\n"
	"End\n"
	"Weapon Port82ModifierOnly\n"
	"  AttributeModifierNugget\n"
	"    AttributeModifier = Port82Buff\n"
	"    DamageFXType = SWORD_SLASH\n"
	"    AntiFX = None\n"
	"  End\n"
	"End\n"
	"Weapon Port82ModifierDefault\n"
	"  AttributeModifierNugget\n"
	"    AttributeModifier = Port82Buff\n"
	"  End\n"
	"End\n"
	"Weapon Port82DamageNugget\n"
	"  DamageNugget\n"
	"    Damage = 10\n"
	"    DamageType = PIERCE\n"
	"    DeathType = NORMAL\n"
	"    DamageFXType = GOOD_ARROW_PIERCE\n"
	"  End\n"
	"End\n";

// A fully synthetic world with stores needed by the INI and by the hit-FX path.
struct Port82World : CombatWorld
{
	AttributeModifierStore modifiers;
	AttributeModifierStore *savedModifiers = TheAttributeModifierStore;
	FXListStore fxLists;
	FXListStore *savedFxLists = TheFXListStore;

	Port82World()
		: CombatWorld()
	{
		TheAttributeModifierStore = &modifiers;
		TheFXListStore = &fxLists;
		modifiers.registerBlock(w.fx.env.blocks);
		w.fx.env.blocks.registerBlock("FXList", [](INI *ini) { ParseFXListDefinitionGlobal(ini); });
		w.fx.env.blocks.registerBlock("DamageFX", [](INI *ini) { DamageFXStore::parseDamageFXDefinitionGlobal(ini); });
		const std::string err = w.load(kPort82Data, INI_LOAD_OVERWRITE, "port82.ini");
		REQUIRE_MESSAGE(err.empty(), err);
	}

	~Port82World()
	{
		TheAttributeModifierStore = savedModifiers;
		TheFXListStore = savedFxLists;
	}
};

struct Recorder : FXEventSink
{
	std::vector<FXEvent> events;
	void onFXEvent(const FXEvent &e) override { events.push_back(e); }
	std::vector<FXEvent> site(const std::string &name) const
	{
		std::vector<FXEvent> found;
		for (const FXEvent &e : events)
		{
			if (std::string(e.site) == name)
			{
				found.push_back(e);
			}
		}
		return found;
	}
	int count(const std::string &name) const
	{
		int n = 0;
		for (const FXEvent &e : events)
		{
			n += e.site == name ? 1 : 0;
		}
		return n;
	}
};
} // namespace

TEST_CASE("PORT-82: AttributeModifierNugget parses DamageFXType; constructor and omitted-field default are 29")
{
	Port82World w;
	const AttributeModifierNugget fresh;
	CHECK(fresh.m_damageFXType == 29); // RW ctor 0x90EDA8 writes 0x1D at +0x14C

	const WeaponTemplate *withField = TheWeaponStore->findWeaponTemplate("Port82ModifierOnly");
	REQUIRE(withField != nullptr);
	REQUIRE(withField->m_nuggets.size() == 1);
	const AttributeModifierNugget *parsed = dynamic_cast<const AttributeModifierNugget *>(withField->m_nuggets[0].get());
	REQUIRE(parsed != nullptr);
	CHECK(parsed->m_damageFXType == 0); // SWORD_SLASH is index 0 of RW list 0xDB5F40

	const WeaponTemplate *withoutField = TheWeaponStore->findWeaponTemplate("Port82ModifierDefault");
	REQUIRE(withoutField != nullptr);
	REQUIRE(withoutField->m_nuggets.size() == 1);
	const AttributeModifierNugget *defaulted = dynamic_cast<const AttributeModifierNugget *>(withoutField->m_nuggets[0].get());
	REQUIRE(defaulted != nullptr);
	CHECK(defaulted->m_damageFXType == 29);
}

TEST_CASE("PORT-82: applying AttributeModifierNugget with DamageFXType emits no hit FX; adjacent DamageNugget keeps its own FX")
{
	Port82World w;
	Object *source = w.unit("Port82Source", 'A', 300.0f, 300.0f);
	Object *victim = w.unit("Port82Victim", 'B', 312.0f, 300.0f);
	w.frames(2);

	AttributeModifierPool *pool = dynamic_cast<AttributeModifierPool *>(victim->findModule("AttributeModifierPoolUpdate"));
	REQUIRE(pool != nullptr);
	const WeaponTemplate *modifierWeapon = TheWeaponStore->findWeaponTemplate("Port82ModifierOnly");
	REQUIRE(modifierWeapon != nullptr);
	const WeaponTemplate *damageWeapon = TheWeaponStore->findWeaponTemplate("Port82DamageNugget");
	REQUIRE(damageWeapon != nullptr);

	Recorder rec;
	w.logic->fxEvents().setSink(&rec);
	unsigned long long unported = 0;
	DeliverNuggets(*w.logic, source->getID(), *modifierWeapon, WeaponBonus{}, victim, victim->getPosition(), false, &unported);
	CHECK(pool->hasList("Port82Buff"));
	CHECK(rec.count("DamageFX") == 0); // SWORD_SLASH is configured on the victim, but this nugget does not emit a hit-FX event
	CHECK(unported == 0);

	REQUIRE(DeliverNuggets(*w.logic, source->getID(), *damageWeapon, WeaponBonus{}, victim, victim->getPosition(), false, &unported) == 1u);
	const std::vector<FXEvent> damageFx = rec.site("DamageFX");
	REQUIRE(damageFx.size() == 1);
	CHECK(damageFx[0].kind == FXEvent::OBJECT_FX);
	CHECK(damageFx[0].fxList != nullptr);
	CHECK(*damageFx[0].fxList == "FX_Port82DamageNugget");
	CHECK(damageFx[0].primary == victim->getID());
	CHECK(damageFx[0].secondary == source->getID());
	CHECK(unported == 0);
}
