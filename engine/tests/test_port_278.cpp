// PORT-278 (fork issue 645): the buff powers whose attribute modifier changes nothing measurable in the button matrix — Boromir's horn
// (Ennorath ModuleTag_HornStarter: SpecialPowerTemplate SpecialAbilityHornOfGondor02, no AttributeModifierAffects, AntiCategory commented out, the
// modifier list carries only `Duration = 5000`), Dorwinion's harvest blessing (ModuleTag_ErntesegenPower: SPECIAL_GENERAL_TARGETLESS, AttributeModifierRange
// 150000, AttributeModifierAffectsSelf No, AttributeModifierAffects = ANY +ElvenMallornTree ALLIES; the modifier is PRODUCTION 125% / 90000 ms) and
// Ghan-buri-Ghan's secret paths (ModuleTag_ElfCloakStarter: AttributeModifierRange 200, AttributeModifierAffectsSelf No, AttributeModifierAffects =
// NONE +DruedainKrieger +DruedainKriegerHorde +DruedainWeiserBanner ALLIES; the modifier is DAMAGE_MULT 125% / 15000 ms).
//
// Synthetic INI (CombatTestUtil), no ROTWK_INSTALL / OPENBFME_ENNORATH_INSTALL. The expectations are the RotWK 2.01 binary's rules, read linearly with
// tools/mod/rw.py dis on game.dat (caveat S-001):
//
//   * the trigger RW 0x897987 builds the scan's filter chain around the centre (RW 0x897BAB .. 0x897C47): ThePartitionManager within
//     AttributeModifierRange (FROM_CENTER_2D, sort mode near to far), the filter chain appended by RW 0xA394C0 in RW's order: RW 0xBE4CC8 the
//     AttributeModifierAffects filter (its test RW 0x66122D -> RW 0x7640C1 -> RW 0x763543), RW 0xC11DC0 the relationship mask (its test RW 0x660B85:
//     the caster's relationship toward the victim), RW 0xC10E20 alive. RW 0xC0F374 (the shared "outside the playable area" bit, Object + 0x458 bit 3)
//     is not ported (S-920).
//   * the relationship mask (RW 0x897B4F .. 0x897BA8): TargetEnemy -> ENEMIES, TargetAllSides -> all three, an AntiCategory power without TargetEnemy
//     swaps ENEMIES and ALLIES; otherwise the mask is ALLIES. A victim whose relationship is not in the mask is not a victim.
//   * the AttributeModifierAffects filter RW 0x763543: 1 side, 2 relationships, 3 the `S:` names, 4 the resolved include names (a matching template —
//     RW 0x73D5C2 isEquivalentTo — answers TRUE) and exclude names (FALSE), 5 the exclude KindOf mask against the template's KindOf (+ 0x108), then
//     6 the rule (+ 0x80): ALL -> TRUE, ANY -> the include KindOf mask (+ 0x48) non-empty and sharing a bit, NONE -> the include KindOf mask non-empty
//     and contained in the template's KindOf (RW 0x70B8C7(objectMask, includeMask, staticMask 0xDE49E4): the static must-be-clear mask's bytes in game.dat
//     are all zero and no absolute-address write to 0xDE49E4 exists in .text, so the must-be-clear test is vacuous; the include ⊆ object test stands).
//     Consequence for the mod's data: in a filter whose `+name` tokens are template names rather than KindOf names (the Erntesegen's +ElvenMallornTree,
//     the secret paths' +DruedainKrieger), the KindOf include mask is EMPTY — rule ANY of the Erntesegen then answers only in step 4 (the tree by name),
//     rule NONE of the secret paths answers only in step 4 (the Druedain by name), and every other object answers FALSE. A filter-less module (the horn)
//     gets the constructor's final replacement (RW 0x89693A / 0x89693D -> RW 0x763D11: a NONE filter with two empty masks, rule +0x80 = 1,
//     flag +0x88 cleared) — and reaches nobody.
//   * the victims are given the modifier RW 0x89763B (module vslot 0x34): Object::addAttributeModifier(name, -1) (RW 0x68F1A8), so the
//     entry runs for the list's own Duration; the AntiCategory is disabled until the frame + the list's Duration (RW 0x897859 .. 0x8978BB).
//   * AttributeModifierAffectsSelf (data + 0x20) is false by default; RW 0x897C5A .. 0x897CF7: a horde adds its members, anything else adds itself.
//
// The three trigger paths of the mod use UpdateModuleStartsAttack = Yes, so the tests fire triggerFromUpdate() — the module's vslot 0x38, RW 0x897D25 ->
// RW 0x897987(null) — which is what SpecialAbilityUpdate calls for those powers.
// GPL-3.0.

#include "doctest.h"
#include "CombatTestUtil.h"

#include "Common/INI.h"
#include "Common/Player.h"
#include "Common/SpecialPower.h"
#include "GameLogic/AttributeModifiers.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/Object/AttributeModifierPool.h"
#include "GameLogic/Object/Object.h"

#include <string>

namespace
{
struct Port278World : combattest::CombatWorld
{
	SpecialPowerStore powers;
	SpecialPowerStore *savedPowers = TheSpecialPowerStore;
	AttributeModifierStore modifiers;
	AttributeModifierStore *savedModifiers = TheAttributeModifierStore;

	explicit Port278World(const char *objects)
	{
		TheSpecialPowerStore = &powers;
		TheAttributeModifierStore = &modifiers;
		modifiers.registerBlock(w.fx.env.blocks);
		w.fx.env.blocks.registerBlock("SpecialPower", [](INI *ini) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(ini); });
		SpecialPowerModules::registerAll(w.modules);
		const std::string err = w.load(objects);
		REQUIRE_MESSAGE(err.empty(), err);
	}
	~Port278World()
	{
		TheSpecialPowerStore = savedPowers;
		TheAttributeModifierStore = savedModifiers;
	}
};

// The three shapes of the mod's powers plus the plain one. Every module uses UpdateModuleStartsAttack = Yes, as the three Ennorath modules do; the tests
// fire triggerFromUpdate(). The targets are an allied plain infantryman, an allied Druedain Krieger (in the Ghan filter), an allied Mallorn tree (in the
// Erntesegen filter), an allied structure of another kind (in no filter) and the same templates on the enemy side (in the filters, but not allied).
const char kPort278Objects[] =
	"ModifierList Port278Buff\n"
	"  Category = SPELL\n"
	"  Modifier = DAMAGE_MULT 125%\n"
	"  Duration = 5000\n"
	"End\n"
	"ModifierList Port278Harvest\n"
	"  Category = SPELL\n"
	"  Modifier = PRODUCTION 125%\n"
	"  Duration = 5000\n"
	"End\n"
	// the vanilla BoromirGondorHornAntiCategory (Ennorath data/ini/attributemodifier.ini: `Duration = 5000`, nothing else)
	"ModifierList Port278HornList\n"
	"  Duration = 5000\n"
	"End\n"
	"SpecialPower Port278AnyPower\n"
	"  Enum = SPECIAL_GENERAL_TARGETLESS\n"
	"  ReloadTime = 1000\n"
	"End\n"
	"SpecialPower Port278HarvestPower\n"
	"  Enum = SPECIAL_GENERAL_TARGETLESS\n"
	"  ReloadTime = 1000\n"
	"End\n"
	"SpecialPower Port278KriegerPower\n"
	"  Enum = SPECIAL_GENERAL_TARGETLESS\n"
	"  ReloadTime = 1000\n"
	"End\n"
	"SpecialPower Port278HornPower\n"
	"  Enum = SPECIAL_GENERAL_TARGETLESS\n"
	"  ReloadTime = 1000\n"
	"End\n"
	"Object Port278Caster\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody BodyTag\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = AttributeModifierPoolUpdate PoolTag\n"
	"  End\n"
	// the plain shape: a filter that names the victims' KindOf (Gamling's horn shape)
	"  Behavior = SpecialPowerModule AnyTag\n"
	"    SpecialPowerTemplate = Port278AnyPower\n"
	"    UpdateModuleStartsAttack = Yes\n"
	"    AttributeModifier = Port278Buff\n"
	"    AttributeModifierRange = 500\n"
	"    AttributeModifierAffects = ANY +INFANTRY ALLIES\n"
	"  End\n"
	// the harvest blessing: the filter names the tree template (ANY +<template>), exactly data/ini/... for NaturmagierErntesegen
	"  Behavior = SpecialPowerModule HarvestTag\n"
	"    SpecialPowerTemplate = Port278HarvestPower\n"
	"    UpdateModuleStartsAttack = Yes\n"
	"    AttributeModifier = Port278Harvest\n"
	"    AttributeModifierRange = 150000\n"
	"    AttributeModifierAffectsSelf = No\n"
	"    AttributeModifierAffects = ANY +Port278Tree ALLIES\n"
	"  End\n"
	// the secret paths: the filter names the Druedain templates (NONE +<template> ...), exactly for GhanSecretPathsLeadership
	"  Behavior = SpecialPowerModule KriegerTag\n"
	"    SpecialPowerTemplate = Port278KriegerPower\n"
	"    UpdateModuleStartsAttack = Yes\n"
	"    AttributeModifier = Port278Buff\n"
	"    AttributeModifierRange = 200\n"
	"    AttributeModifierAffectsSelf = No\n"
	"    AttributeModifierAffects = NONE +Port278Krieger +Port278WeiserBanner ALLIES\n"
	"  End\n"
	// Boromir's horn as the mod ships it: a modifier and a range, and no AttributeModifierAffects at all
	"  Behavior = SpecialPowerModule HornTag\n"
	"    SpecialPowerTemplate = Port278HornPower\n"
	"    UpdateModuleStartsAttack = Yes\n"
	"    AttributeModifier = Port278HornList\n"
	"    AttributeModifierRange = 100\n"
	"  End\n"
	"End\n"
	"Object Port278Ally\n"
	"  KindOf = INFANTRY\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody BodyTag\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = AttributeModifierPoolUpdate PoolTag\n"
	"  End\n"
	"End\n"
	"Object Port278Krieger\n"
	"  KindOf = INFANTRY\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody BodyTag\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = AttributeModifierPoolUpdate PoolTag\n"
	"  End\n"
	"End\n"
	"Object Port278WeiserBanner\n"
	"  KindOf = INFANTRY\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody BodyTag\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = AttributeModifierPoolUpdate PoolTag\n"
	"  End\n"
	"End\n"
	"Object Port278Tree\n"
	"  KindOf = STRUCTURE IMMOBILE ECONOMY_STRUCTURE FS_CASH_PRODUCER\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryMinorRadius = 10\n"
	"  GeometryHeight = 10\n"
	"  Body = ActiveBody BodyTag\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = AttributeModifierPoolUpdate PoolTag\n"
	"  End\n"
	"End\n"
	"Object Port278Other\n"
	"  KindOf = STRUCTURE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryMinorRadius = 10\n"
	"  GeometryHeight = 10\n"
	"  Body = ActiveBody BodyTag\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = AttributeModifierPoolUpdate PoolTag\n"
	"  End\n"
	"End\n";

SpecialPowerModule *moduleOf(Object *o, const char *powerName)
{
	const SpecialPowerTemplate *t = TheSpecialPowerStore->findSpecialPowerTemplate(powerName);
	REQUIRE(t);
	for (const std::unique_ptr<BehaviorModule> &m : o->modules())
	{
		SpecialPowerModule *sp = dynamic_cast<SpecialPowerModule *>(m.get());
		if (sp && sp->getSpecialPowerTemplate() == t)
		{
			return sp;
		}
	}
	return nullptr;
}

AttributeModifierPool *poolOf(Object *o)
{
	return dynamic_cast<AttributeModifierPool *>(o->findModule("AttributeModifierPoolUpdate"));
}

float productionOf(Object *o)
{
	float mult = 1.0f;
	if (!o->attributeModifierProduct(ATTRIBUTE_PRODUCTION, nullptr, true, mult))
	{
		return 1.0f;
	}
	return mult;
}

float damageMultOf(Object *o)
{
	float mult = 1.0f;
	if (!o->attributeModifierProduct(ATTRIBUTE_DAMAGE_MULT, nullptr, true, mult))
	{
		return 1.0f;
	}
	return mult;
}
} // namespace

TEST_CASE("PORT-278: ANY +INFANTRY ALLIES — the allied infantryman in range gets the list, the structure does not, the enemy does not (RW 0x897BAB .. 0x897C47, RW 0x66122D)")
{
	Port278World f(kPort278Objects);
	Object *caster = f.unit("Port278Caster", 'A', 300, 300);
	Object *ally = f.unit("Port278Ally", 'A', 320, 300);
	Object *other = f.unit("Port278Other", 'A', 330, 300);
	Object *enemy = f.unit("Port278Ally", 'B', 340, 300);
	REQUIRE(caster);
	REQUIRE(ally);
	REQUIRE(other);
	REQUIRE(enemy);
	f.frames(1);

	SpecialPowerModule *power = moduleOf(caster, "Port278AnyPower");
	REQUIRE(power);
	power->triggerFromUpdate();
	REQUIRE(power->triggers() == 1);
	REQUIRE(power->victimsAffected() == 1);

	REQUIRE(poolOf(ally));
	CHECK(poolOf(ally)->hasList("Port278Buff"));
	CHECK(damageMultOf(ally) == doctest::Approx(1.25f)); // the changed value, not just the list's name
	// RW 0x66122D: the structure is not in the filter; RW 0x660B85: the enemy is not an ally; RW 0x897C5A: the caster does not AffectsSelf
	CHECK_FALSE(poolOf(other)->hasList("Port278Buff"));
	CHECK_FALSE(poolOf(enemy)->hasList("Port278Buff"));
	CHECK_FALSE(poolOf(caster)->hasList("Port278Buff"));
}

TEST_CASE("PORT-278: the harvest blessing shape (ANY +<tree template> ALLIES) — the allied tree gets PRODUCTION, the infantryman and the other structure do not (RW 0x763543, RW 0x73D5C2)")
{
	Port278World f(kPort278Objects);
	Object *caster = f.unit("Port278Caster", 'A', 300, 300);
	Object *tree = f.unit("Port278Tree", 'A', 320, 300);
	Object *other = f.unit("Port278Other", 'A', 330, 300);
	Object *ally = f.unit("Port278Ally", 'A', 340, 300);
	Object *enemyTree = f.unit("Port278Tree", 'B', 350, 300);
	REQUIRE(caster);
	REQUIRE(tree);
	REQUIRE(other);
	REQUIRE(ally);
	REQUIRE(enemyTree);
	f.frames(1);

	SpecialPowerModule *power = moduleOf(caster, "Port278HarvestPower");
	REQUIRE(power);
	power->triggerFromUpdate();
	REQUIRE(power->triggers() == 1);
	REQUIRE(power->victimsAffected() == 1);

	REQUIRE(poolOf(tree));
	CHECK(poolOf(tree)->hasList("Port278Harvest"));
	CHECK(productionOf(tree) == doctest::Approx(1.25f));
	// the filter names the tree template: the infantryman and the other structure are not in it, the enemy tree is not allied
	CHECK_FALSE(poolOf(ally)->hasList("Port278Harvest"));
	CHECK_FALSE(poolOf(other)->hasList("Port278Harvest"));
	CHECK_FALSE(poolOf(enemyTree)->hasList("Port278Harvest"));
	CHECK_FALSE(poolOf(caster)->hasList("Port278Harvest")); // AttributeModifierAffectsSelf = No
}

TEST_CASE("PORT-278: the secret paths shape (NONE +<template names> ALLIES) — the named allied Druedain get the modifier, the plain infantryman does not (RW 0x763543, RW 0x73D5C2)")
{
	Port278World f(kPort278Objects);
	Object *caster = f.unit("Port278Caster", 'A', 300, 300);
	Object *krieger = f.unit("Port278Krieger", 'A', 320, 300);
	Object *banner = f.unit("Port278WeiserBanner", 'A', 330, 300);
	Object *ally = f.unit("Port278Ally", 'A', 340, 300);
	Object *enemyKrieger = f.unit("Port278Krieger", 'B', 350, 300);
	REQUIRE(caster);
	REQUIRE(krieger);
	REQUIRE(banner);
	REQUIRE(ally);
	REQUIRE(enemyKrieger);
	f.frames(1);

	SpecialPowerModule *power = moduleOf(caster, "Port278KriegerPower");
	REQUIRE(power);
	power->triggerFromUpdate();
	REQUIRE(power->triggers() == 1);
	REQUIRE(power->victimsAffected() == 2);

	CHECK(poolOf(krieger)->hasList("Port278Buff"));
	CHECK(poolOf(banner)->hasList("Port278Buff"));
	CHECK(damageMultOf(krieger) == doctest::Approx(1.25f));
	// the rule NONE with an empty KindOf include mask passes nobody: the plain infantryman matches no name and answers FALSE in RW 0x7638F6 .. 0x7638FD;
	// the enemy Krieger fails the ALLIES relationship first (the relationship mask check, RW 0x897B4F .. 0x897BA8)
	CHECK_FALSE(poolOf(ally)->hasList("Port278Buff"));
	CHECK_FALSE(poolOf(enemyKrieger)->hasList("Port278Buff"));
	CHECK_FALSE(poolOf(caster)->hasList("Port278Buff")); // AttributeModifierAffectsSelf = No
}

TEST_CASE("PORT-278: an expiring list leaves the pool after its Duration (RW 0x80585F; Duration 5000 ms -> 25 frames via NumericState::ceilScaled(ms, 0.005f))")
{
	Port278World f(kPort278Objects);
	Object *caster = f.unit("Port278Caster", 'A', 300, 300);
	Object *ally = f.unit("Port278Ally", 'A', 320, 300);
	REQUIRE(caster);
	REQUIRE(ally);
	f.frames(1);

	SpecialPowerModule *power = moduleOf(caster, "Port278AnyPower");
	REQUIRE(power);
	power->triggerFromUpdate();
	CHECK(poolOf(ally)->hasList("Port278Buff"));
	f.frames(10);
	CHECK(poolOf(ally)->hasList("Port278Buff")); // 10 frames in: still running
	f.frames(20);                                 // 30 frames in: the entry expired (expire = trigger frame + 25)
	CHECK_FALSE(poolOf(ally)->hasList("Port278Buff"));
	CHECK(damageMultOf(ally) == doctest::Approx(1.0f)); // and the changed value is gone with it
}

TEST_CASE("PORT-278: a power whose module names no AttributeModifierAffects reaches nobody — the constructor's final replacement is a NONE filter with empty masks (RW 0x89693A / 0x89693D -> RW 0x763D11), the matcher's rule 1 requires a non-empty include mask (RW 0x7638F6 .. 0x7638FD)")
{
	Port278World f(kPort278Objects);
	Object *caster = f.unit("Port278Caster", 'A', 300, 300);
	Object *ally = f.unit("Port278Ally", 'A', 320, 300);
	Object *krieger = f.unit("Port278Krieger", 'A', 330, 300);
	REQUIRE(caster);
	REQUIRE(ally);
	REQUIRE(krieger);
	f.frames(1);

	// Boromir's horn as the mod ships it (data/ini/object/goodfaction/units/men/boromir.ini, .../gondor/gondorboromir.ini, all three blocks:
	// no AttributeModifierAffects, AntiCategory commented out): nobody can be picked, whatever stands next to the caster
	SpecialPowerModule *horn = moduleOf(caster, "Port278HornPower");
	REQUIRE(horn);
	horn->triggerFromUpdate();
	REQUIRE(horn->triggers() == 1);
	CHECK(horn->victimsAffected() == 0);
	CHECK_FALSE(poolOf(ally)->hasList("Port278HornList"));
	CHECK_FALSE(poolOf(krieger)->hasList("Port278HornList"));
	CHECK_FALSE(poolOf(caster)->hasList("Port278HornList"));
}
