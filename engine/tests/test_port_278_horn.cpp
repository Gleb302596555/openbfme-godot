// PORT-278 follow-up (issue #645): the empty-modifier + AntiCategory family — Boromir's horn of Gondor02 and Aragorn's Horn of Elendil.
//
// What the mod ships (Ennorath 1.9.8.3):
//
//   Behavior = SpecialPowerModule ModuleTag_HornStarter
//     SpecialPowerTemplate = SpecialAbilityHornOfGondor02
//     UpdateModuleStartsAttack = Yes
//     AttributeModifier = BoromirGondorHornAntiCategory    ; The mod's AttributeModifier line is present, commented below (AntiCategory commented out)
//     AttributeModifierRange = 100
//   End
//
// The list `BoromirGondorHornAntiCategory` carries NO `Modifier =` row and NO `Category =` row: only `Duration = 5000` — the entry is a carrier for the
// AntiCategory lock window alone (the mod's own comment: "Used for the anti category duration only"; the AntiCategory row itself is commented out, so no
// category lock/unlock happens anywhere). The paired ModelConditionSpecialAbilityUpdate (ModuleTag_HornUpdate) drives the horn: WhichSpecialPower = 1
// sets SPECIAL_POWER_1 (RW 0x89455B) and GenerateTerror fires an EmotionPulseRadius scan of the ENEMIES within the radius (RW 0x894701) that requests
// EMOTION_TERROR on them (RW 0x68F383). The owner plays the horn and the enemies flee — the terror comes from this pulse (through the paired update),
// NOT from the special-power apply path: RW 0x89763B adds the attribute modifier to the victims the scan picks, and with no AttributeModifierAffects
// the scan picks nobody (the module keeps the constructor's replacement filter, RW 0x89693A / 0x89693D -> RW 0x763D11).
//
// What these tests pin down (synthetic INI, no game files):
//   1. The horn shape: casting runs the terror pulse on the ENEMIES in EmotionPulseRadius (EMOTION_TERROR requested), the caster and allies stay
//      calm, and the empty-modifier SpecialPowerModule reaches nobody whatever stands in range.
//   2. Elendil: the same shape with WhichSpecialPower = 2 -> SPECIAL_POWER_2 and GenerateUncontrollableFear (EMOTION_UNCONTROLLABLE_FEAR).
//   3. The matrix shape of the horn: the checker's before/after sweep around the cast. The ability ran (SPECIAL_POWER_1 set), an enemy received
//      EMOTION_TERROR, and the ONLY unsatisfied expectation is the SpecialPowerModule modifier line — exactly the row the matrix reports
//      ("SpecialPowerModule AttributeModifier ...: no result; "). The paired design is correct as a terror button: only the empty modifier does nothing.
//   4. The Duration-only list (the anti-category carrier): when the module DOES aim the modifier (a filter is set) the entry lands with the list's
//      Duration (5000 ms -> 25 frames, RW 0x80585F) and the attribute product reports 1.0 while it runs (no `Modifier =` rows) — the carrier channel
//      works; nothing about damage/production changes for its duration.
//
// GPL-3.0.

#include "doctest.h"
#include "CombatTestUtil.h"
#include "SweepEffectProbe.h"

#include "Common/Player.h"
#include "Common/SpecialPower.h"
#include "GameLogic/AttributeModifiers.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/EmotionModules.h"
#include "GameLogic/Module/SpecialAbilityModules.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/Object/AttributeModifierPool.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/System/EmotionSystem.h"

#include <memory>
#include <string>
#include <vector>

using namespace sweep_probe;

namespace
{
struct Port278HornWorld : combattest::CombatWorld
{
	SpecialPowerStore powers;
	SpecialPowerStore *savedPowers = TheSpecialPowerStore;
	AttributeModifierStore modifiers;
	AttributeModifierStore *savedModifiers = TheAttributeModifierStore;

	explicit Port278HornWorld(const char *objects)
	{
		TheSpecialPowerStore = &powers;
		TheAttributeModifierStore = &modifiers;
		modifiers.registerBlock(w.fx.env.blocks);
		w.fx.env.blocks.registerBlock("SpecialPower", [](INI *ini) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(ini); });
		SpecialAbilityModules::registerAll(w.modules);
		SpecialPowerModules::registerAll(w.modules);

		const std::string err = w.load(objects, INI_LOAD_OVERWRITE, "port278horn.ini");
		REQUIRE_MESSAGE(err.empty(), err);
	}
	~Port278HornWorld()
	{
		TheSpecialPowerStore = savedPowers;
		TheAttributeModifierStore = savedModifiers;
	}
};

// The vanilla modifier lists: Duration = 5000 only (Ennorath data/ini/attributemodifier.ini). The vanilla SpecialPowers: Enum SPECIAL_GENERAL_TARGETLESS.
const char kPort278HornObjects[] =
	"ModifierList Port278HornEmptyList\n"
	"  Duration = 5000\n"
	"End\n"
	"ModifierList Port278ElendilEmptyList\n"
	"  Duration = 5000\n"
	"End\n"
	"SpecialPower Port278Horn02Power\n"
	"  Enum = SPECIAL_GENERAL_TARGETLESS\n"
	"  ReloadTime = 1000\n"
	"End\n"
	"SpecialPower Port278ElendilPower\n"
	"  Enum = SPECIAL_GENERAL_TARGETLESS\n"
	"  ReloadTime = 1000\n"
	"End\n"
	// the horn, as it ships: UpdateModuleStartsAttack on both paired modules' power; no AttributeModifierAffects, AntiCategory commented out (absent)
	"Object Port278HornCaster\n"
	"  KindOf = INFANTRY SELECTABLE HERO\n"
	"  Geometry = CYLINDER\n  GeometryMajorRadius = 8\n  GeometryMinorRadius = 8\n  GeometryHeight = 20\n"
	"  Body = ActiveBody BodyTag\n    MaxHealth = 100\n  End\n"
	"  Behavior = AIUpdateInterface AiTag\n  End\n"
	"  LocomotorSet\n    Locomotor = WalkerLoco\n    Condition = SET_NORMAL\n    Speed = 55\n  End\n"
	"  Behavior = AttributeModifierPoolUpdate PoolTag\n  End\n"
	"  Behavior = SpecialPowerModule ModuleTag_HornStarter\n"
	"    SpecialPowerTemplate = Port278Horn02Power\n"
	"    UpdateModuleStartsAttack = Yes\n"
	"    AttributeModifier = Port278HornEmptyList\n"
	"    AttributeModifierRange = 100\n"
	"  End\n"
	"  Behavior = ModelConditionSpecialAbilityUpdate ModuleTag_HornUpdate\n"
	"    SpecialPowerTemplate = Port278Horn02Power\n"
	"    WhichSpecialPower = 1\n"
	"    GenerateTerror = Yes\n"
	"    EmotionPulseRadius = 150\n"
	"    UnpackTime = 400\n"
	"    PreparationTime = 0\n"
	"    PackTime = 400\n"
	"    IgnoreFacingCheck = Yes\n"
	"  End\n"
	"  Behavior = EmotionTrackerUpdate EmotionTag\n  End\n"
	"End\n"
	// Aragorn's Horn of Elendil: the twin shape (Anduin's horn: WhichSpecialPower = 2 -> SPECIAL_POWER_2, GenerateUncontrollableFear)
	"Object Port278ElendilCaster\n"
	"  KindOf = INFANTRY SELECTABLE HERO\n"
	"  Geometry = CYLINDER\n  GeometryMajorRadius = 8\n  GeometryMinorRadius = 8\n  GeometryHeight = 20\n"
	"  Body = ActiveBody BodyTag\n    MaxHealth = 100\n  End\n"
	"  Behavior = AIUpdateInterface AiTag\n  End\n"
	"  LocomotorSet\n    Locomotor = WalkerLoco\n    Condition = SET_NORMAL\n    Speed = 55\n  End\n"
	"  Behavior = AttributeModifierPoolUpdate PoolTag\n  End\n"
	"  Behavior = SpecialPowerModule ModuleTag_ElendilStarter\n"
	"    SpecialPowerTemplate = Port278ElendilPower\n"
	"    UpdateModuleStartsAttack = Yes\n"
	"    AttributeModifier = Port278ElendilEmptyList\n"
	"    AttributeModifierRange = 100\n"
	"  End\n"
	"  Behavior = ModelConditionSpecialAbilityUpdate ModuleTag_ElendilUpdate\n"
	"    SpecialPowerTemplate = Port278ElendilPower\n"
	"    WhichSpecialPower = 2\n"
	"    GenerateUncontrollableFear = Yes\n"
	"    EmotionPulseRadius = 150\n"
	"    UnpackTime = 400\n"
	"    PreparationTime = 0\n"
	"    PackTime = 400\n"
	"    IgnoreFacingCheck = Yes\n"
	"  End\n"
	"  Behavior = EmotionTrackerUpdate EmotionTag\n  End\n"
	"End\n"
	// the empty modifier aimed at somebody (a filter IS set): the Duration-only entry with no Modifier rows
	"Object Port278BuffCaster\n"
	"  KindOf = INFANTRY SELECTABLE HERO\n"
	"  Geometry = CYLINDER\n  GeometryMajorRadius = 8\n  GeometryMinorRadius = 8\n  GeometryHeight = 20\n"
	"  Body = ActiveBody BodyTag\n    MaxHealth = 100\n  End\n"
	"  Behavior = AIUpdateInterface AiTag\n  End\n"
	"  LocomotorSet\n    Locomotor = WalkerLoco\n    Condition = SET_NORMAL\n    Speed = 55\n  End\n"
	"  Behavior = AttributeModifierPoolUpdate PoolTag\n  End\n"
	"  Behavior = SpecialPowerModule Port278BuffTag\n"
	"    SpecialPowerTemplate = Port278Horn02Power\n"
	"    UpdateModuleStartsAttack = Yes\n"
	"    AttributeModifier = Port278HornEmptyList\n"
	"    AttributeModifierRange = 150\n"
	"    AttributeModifierAffects = ANY +INFANTRY ALLIES\n"
	"  End\n"
	"  Behavior = EmotionTrackerUpdate EmotionTag\n  End\n"
	"End\n"
	"Object Port278Infantry\n"
	"  KindOf = INFANTRY\n"
	"  Geometry = CYLINDER\n  GeometryMajorRadius = 8\n  GeometryMinorRadius = 8\n  GeometryHeight = 20\n"
	"  Body = ActiveBody BodyTag\n    MaxHealth = 100\n  End\n"
	"  Behavior = AttributeModifierPoolUpdate PoolTag\n  End\n"
	"  Behavior = EmotionTrackerUpdate EmotionTag\n  End\n"
	"End\n"
	"Object Port278AllyInfantry\n"
	"  KindOf = INFANTRY\n"
	"  Geometry = CYLINDER\n  GeometryMajorRadius = 8\n  GeometryMinorRadius = 8\n  GeometryHeight = 20\n"
	"  Body = ActiveBody BodyTag\n    MaxHealth = 100\n  End\n"
	"  Behavior = AttributeModifierPoolUpdate PoolTag\n  End\n"
	"  Behavior = EmotionTrackerUpdate EmotionTag\n  End\n"
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

SpecialAbilityUpdate *abilityOf(Object *o, const char *powerName)
{
	const SpecialPowerTemplate *t = TheSpecialPowerStore->findSpecialPowerTemplate(powerName);
	REQUIRE(t);
	for (const std::unique_ptr<BehaviorModule> &m : o->modules())
	{
		SpecialAbilityUpdate *a = dynamic_cast<SpecialAbilityUpdate *>(m.get());
		if (a && a->abilityData()->m_specialPowerTemplateName == t->getName())
		{
			return a;
		}
	}
	return nullptr;
}

AttributeModifierPool *poolOf(Object *o)
{
	return dynamic_cast<AttributeModifierPool *>(o->findModule("AttributeModifierPoolUpdate"));
}

void trackFrames(combattest::CombatWorld &f, HitTracker &hits, int frames)
{
	hits.track(*f.logic);
	for (int i = 0; i < frames; ++i)
	{
		f.frames(1);
		hits.track(*f.logic);
	}
}

void castAbility(Object *caster, SpecialAbilityUpdate &ability, const SpecialPowerTemplate *t, const Coord3D &loc)
{
	ability.initiateIntentToDoSpecialPower(t, nullptr, &loc, 0, 0);
}
} // namespace

TEST_CASE("PORT-278: the horn as the mod ships it — the terror pulse hits the ENEMIES in EmotionPulseRadius, the empty-modifier module reaches nobody, the allies stay calm")
{
	Port278HornWorld f(kPort278HornObjects);
	Object *caster = f.unit("Port278HornCaster", 'A', 300, 300);
	Object *enemy1 = f.unit("Port278Infantry", 'B', 320, 300);
	Object *enemy2 = f.unit("Port278Infantry", 'B', 350, 300);
	Object *ally = f.unit("Port278AllyInfantry", 'A', 315, 300);
	REQUIRE(caster);
	REQUIRE(enemy1);
	REQUIRE(enemy2);
	REQUIRE(ally);
	f.frames(2);

	SpecialPowerModule *power = moduleOf(caster, "Port278Horn02Power");
	REQUIRE(power);
	SpecialAbilityUpdate *ability = abilityOf(caster, "Port278Horn02Power");
	REQUIRE(ability);

	HitTracker hits;
	hits.casters.insert(caster->getID());
	const Coord3D here = *caster->getPosition();
	castAbility(caster, *ability, ability->abilityData()->m_specialPowerTemplate, here);
	trackFrames(f, hits, 6); // unpack 400 ms = 2 frames; the trigger fires right after the unpack; the requests live 1 frame (RW 0x68F383)

	CHECK(power->triggers() == 1);       // RW 0x853E4A: the paired power module's vslot 0x38 (RW 0x897D25)
	CHECK(power->victimsAffected() == 0); // no AttributeModifierAffects: nobody, whatever stands in AttributeModifierRange (RW 0x89693A / 0x89693D)
	const auto it = hits.emotionFirstSeen.find(EMOTION_TERROR); // RW 0x894701: enemies within EmotionPulseRadius only
	REQUIRE(it != hits.emotionFirstSeen.end());
	CHECK(it->second.count(enemy1->getID()) == 1);
	CHECK(it->second.count(enemy2->getID()) == 1);
	CHECK(it->second.count(ally->getID()) == 0); // allies are not ENEMIES for the pulse (RW 0x894701 filter RW 0xC1676C)
}

TEST_CASE("PORT-278: Elendil — WhichSpecialPower = 2 sets SPECIAL_POWER_2 and the pulse requests GenerateUncontrollableFear on the enemies in radius")
{
	Port278HornWorld f(kPort278HornObjects);
	Object *caster = f.unit("Port278ElendilCaster", 'A', 300, 300);
	Object *enemy1 = f.unit("Port278Infantry", 'B', 320, 300);
	Object *enemy2 = f.unit("Port278Infantry", 'B', 350, 300);
	Object *ally = f.unit("Port278AllyInfantry", 'A', 315, 300);
	REQUIRE(caster);
	REQUIRE(enemy1);
	REQUIRE(enemy2);
	REQUIRE(ally);
	f.frames(2);

	SpecialPowerModule *power = moduleOf(caster, "Port278ElendilPower");
	REQUIRE(power);
	SpecialAbilityUpdate *ability = abilityOf(caster, "Port278ElendilPower");
	REQUIRE(ability);

	HitTracker hits;
	hits.casters.insert(caster->getID());
	const Coord3D here = *caster->getPosition();
	castAbility(caster, *ability, ability->abilityData()->m_specialPowerTemplate, here);
	hits.track(*f.logic);
	f.frames(1);
	CHECK(caster->testModelCondition(CombatNames::modelCondition("SPECIAL_POWER_2"))); // RW 0x89455B: WhichSpecialPower 2 -> SPECIAL_POWER_2 (bit 20 of Object + 0x124)
	for (int i = 0; i < 5; ++i)
	{
		f.frames(1);
		hits.track(*f.logic);
	}

	CHECK(power->triggers() == 1);
	CHECK(power->victimsAffected() == 0);
	const auto it = hits.emotionFirstSeen.find(EMOTION_UNCONTROLLABLE_FEAR);
	REQUIRE(it != hits.emotionFirstSeen.end());
	CHECK(it->second.count(enemy1->getID()) == 1);
	CHECK(it->second.count(enemy2->getID()) == 1);
	CHECK(it->second.count(ally->getID()) == 0);
}

TEST_CASE("PORT-278: the matrix shape of the horn cast — SPECIAL_POWER_1 set, the enemy terrified, the ONLY unsatisfied expectation is the SpecialPowerModule modifier line")
{
	Port278HornWorld f(kPort278HornObjects);
	Object *caster = f.unit("Port278HornCaster", 'A', 300, 300);
	Object *enemy = f.unit("Port278Infantry", 'B', 320, 300);
	REQUIRE(caster);
	REQUIRE(enemy);
	f.frames(2);

	const SpecialPowerTemplate *t = TheSpecialPowerStore->findSpecialPowerTemplate("Port278Horn02Power");
	REQUIRE(t);
	HitTracker hits;
	hits.casters.insert(caster->getID());
	const EffectProbe before = EffectProbe::take(*f.logic, caster, t);
	REQUIRE(before.unasserted.empty());
	REQUIRE(before.expects.size() == 3); // ModuleTag_HornStarter (MODIFIER) + ModuleTag_HornUpdate (MODEL_CONDITION SPECIAL_POWER_1, EMOTION GenerateTerror)

	SpecialAbilityUpdate *ability = abilityOf(caster, "Port278Horn02Power");
	REQUIRE(ability);
	const Coord3D here = *caster->getPosition();
	castAbility(caster, *ability, t, here);
	hits.track(*f.logic);
	for (int i = 0; i < 6; ++i)
	{
		f.frames(1);
		hits.track(*f.logic);
	}
	REQUIRE(hits.casters.count(caster->getID()) == 1);

	std::string missing;
	const std::string effect = before.compare(EffectProbe::take(*f.logic, caster, t), missing, &hits);
	CHECK_MESSAGE(missing == "SpecialPowerModule AttributeModifier Port278HornEmptyList: no result; ", missing);
	CHECK(effect.find("SPECIAL_POWER_1") != std::string::npos);
	CHECK(effect.find("received emotion 6") != std::string::npos); // EMOTION_TERROR is EmotionType 6 (RW 0xD9FA08)
}

TEST_CASE("PORT-278: the empty modifier aimed with a filter — the Duration-5000 entry lands for 25 frames and nothing about attributes changes (no Modifier rows)")
{
	Port278HornWorld f(kPort278HornObjects);
	Object *caster = f.unit("Port278BuffCaster", 'A', 300, 300);
	Object *ally = f.unit("Port278Infantry", 'A', 320, 300);
	Object *enemy = f.unit("Port278Infantry", 'B', 320, 300);
	REQUIRE(caster);
	REQUIRE(ally);
	REQUIRE(enemy);
	f.frames(2);

	SpecialPowerModule *power = moduleOf(caster, "Port278Horn02Power");
	REQUIRE(power);
	power->triggerFromUpdate(); // UpdateModuleStartsAttack = Yes (RW 0x897D25 -> RW 0x897987(null)), the module's vslot 0x38
	REQUIRE(power->triggers() == 1);
	REQUIRE(power->victimsAffected() == 1); // the allied infantryman only (ALLIES, +INFANTRY)

	REQUIRE(poolOf(ally));
	CHECK(poolOf(ally)->hasList("Port278HornEmptyList")); // the carrier entry lands — nothing reads its Duration except the AntiCategory lock code
	CHECK_FALSE(poolOf(enemy)->hasList("Port278HornEmptyList"));

	// no Modifier rows in the list: the products are 1.0 while the entry runs
	float mult = 0.0f;
	CHECK_FALSE(ally->attributeModifierProduct(ATTRIBUTE_DAMAGE_MULT, nullptr, true, mult));
	CHECK_FALSE(ally->attributeModifierProduct(ATTRIBUTE_PRODUCTION, nullptr, true, mult));

	f.frames(30); // Duration 5000 ms -> 25 frames (RW 0x80585F; NumericState::ceilScaled(5000, 0.005f))
	CHECK_FALSE(poolOf(ally)->hasList("Port278HornEmptyList"));
	CHECK(power->triggers() == 1); // expiry is the pool's business, no re-trigger
}
