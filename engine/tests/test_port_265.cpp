// OpenBFME unit tests (lane PORT-265, fork issue 613): the horde's own once-every-5th-frame split check, RW 0x86DF44 — the class's own module slot +0x7C
// called from HordeContain::update at RW 0x8730C2 (the divisor at RW 0xD9F608), ending the update at RW 0x873172 when the horde split.
//
// After the split-allowed test (interface slot +0x1FC, RW 0x86DFBF) and the rank test (the experience tracker, Object + 0x26C -> + 0x24, at most 1,
// RW 0x86DFCD .. 0x86DFD9) RotWK gates the split by SplitHordeNumber (module data + 0x258, RW 0x86E013 / 0x86E01C): with a positive number only a horde
// holding no more members than that number splits, with 0 the members must all be one template apart from the BANNER carriers (RW 0x86E044 .. 0x86E06D).
// The split itself is the already ported RW 0x8791C5 (interface slot +0x1F8, RW 0x86E090).
//
// Ennorath's data sets SplitHordeNumber on the Last Alliance hordes (data/ini/object/cinematic/lastallianceunits.ini, values 5 and 15); the port parsed
// the field and nothing read it, so a horde that had been reduced to that size never split on its own.

#include "doctest.h"

#include "CombatTestUtil.h"
#include "Common/Thing/ThingTemplate.h"

namespace
{
const char kPort265Objects[] =
	"Object P265SwordHorde\n"
	" KindOf = HORDE MELEE_HORDE SELECTABLE\n"
	" Geometry = BOX\n GeometryMajorRadius = 30\n GeometryMinorRadius = 45\n GeometryHeight = 20\n"
	" Body = ImmortalBody BodyTag\n MaxHealth = 1\n End\n"
	" Behavior = HordeAIUpdate AITag\n End\n"
	" Behavior = HordeContain ContainTag\n"
	"  RankInfo = RankNumber:1 UnitType:Swordsman Position:X:20 Y:0\n"
	" End\n"
	" LocomotorSet\n Locomotor = HordeLoco\n Condition = SET_NORMAL\n Speed = 50\n End\n"
	"End\n"
	"Object P265ArcherHorde\n"
	" KindOf = HORDE SELECTABLE\n"
	" Geometry = BOX\n GeometryMajorRadius = 30\n GeometryMinorRadius = 45\n GeometryHeight = 20\n"
	" Body = ImmortalBody BodyTag\n MaxHealth = 1\n End\n"
	" Behavior = HordeAIUpdate AITag\n End\n"
	" Behavior = HordeContain ContainTag\n"
	"  RankInfo = RankNumber:1 UnitType:Archer Position:X:20 Y:0\n"
	" End\n"
	" LocomotorSet\n Locomotor = HordeLoco\n Condition = SET_NORMAL\n Speed = 50\n End\n"
	"End\n"
	// SplitHordeNumber 2 with two members: the check lets the horde split (count 2 is not above the number)
	"Object P265DueHorde\n"
	" KindOf = HORDE COMBO_HORDE MELEE_HORDE SELECTABLE\n"
	" Geometry = BOX\n GeometryMajorRadius = 30\n GeometryMinorRadius = 45\n GeometryHeight = 20\n"
	" Body = ImmortalBody BodyTag\n MaxHealth = 1\n End\n"
	" Behavior = HordeAIUpdate AITag\n End\n"
	" Behavior = HordeContain ContainTag\n"
	"  Slots = 2\n"
	"  SplitHordeNumber = 2\n"
	"  RankInfo = RankNumber:1 UnitType:Swordsman Position:X:20 Y:0\n"
	"  RankInfo = RankNumber:2 UnitType:Archer Position:X:-20 Y:0\n"
	"  InitialPayload = Swordsman 1\n  InitialPayload = Archer 1\n"
	"  SplitHorde = SplitResult:P265SwordHorde UnitType:Swordsman\n"
	"  SplitHorde = SplitResult:P265ArcherHorde UnitType:Archer\n"
	" End\n"
	" LocomotorSet\n Locomotor = HordeLoco\n Condition = SET_NORMAL\n Speed = 50\n End\n"
	"End\n"
	// SplitHordeNumber 1 with two members: more members than the number, the check refuses (RW 0x86E01C)
	"Object P265CrowdedHorde\n"
	" KindOf = HORDE COMBO_HORDE MELEE_HORDE SELECTABLE\n"
	" Geometry = BOX\n GeometryMajorRadius = 30\n GeometryMinorRadius = 45\n GeometryHeight = 20\n"
	" Body = ImmortalBody BodyTag\n MaxHealth = 1\n End\n"
	" Behavior = HordeAIUpdate AITag\n End\n"
	" Behavior = HordeContain ContainTag\n"
	"  Slots = 2\n"
	"  SplitHordeNumber = 1\n"
	"  RankInfo = RankNumber:1 UnitType:Swordsman Position:X:20 Y:0\n"
	"  RankInfo = RankNumber:2 UnitType:Archer Position:X:-20 Y:0\n"
	"  InitialPayload = Swordsman 1\n  InitialPayload = Archer 1\n"
	"  SplitHorde = SplitResult:P265SwordHorde UnitType:Swordsman\n"
	"  SplitHorde = SplitResult:P265ArcherHorde UnitType:Archer\n"
	" End\n"
	" LocomotorSet\n Locomotor = HordeLoco\n Condition = SET_NORMAL\n Speed = 50\n End\n"
	"End\n"
	// no SplitHordeNumber and two different templates: the mixed-template rule lets this one split too (RW 0x86E044 .. 0x86E06D)
	"Object P265MixedHorde\n"
	" KindOf = HORDE COMBO_HORDE MELEE_HORDE SELECTABLE\n"
	" Geometry = BOX\n GeometryMajorRadius = 30\n GeometryMinorRadius = 45\n GeometryHeight = 20\n"
	" Body = ImmortalBody BodyTag\n MaxHealth = 1\n End\n"
	" Behavior = HordeAIUpdate AITag\n End\n"
	" Behavior = HordeContain ContainTag\n"
	"  Slots = 2\n"
	"  RankInfo = RankNumber:1 UnitType:Swordsman Position:X:20 Y:0\n"
	"  RankInfo = RankNumber:2 UnitType:Archer Position:X:-20 Y:0\n"
	"  InitialPayload = Swordsman 1\n  InitialPayload = Archer 1\n"
	"  SplitHorde = SplitResult:P265SwordHorde UnitType:Swordsman\n"
	"  SplitHorde = SplitResult:P265ArcherHorde UnitType:Archer\n"
	" End\n"
	" LocomotorSet\n Locomotor = HordeLoco\n Condition = SET_NORMAL\n Speed = 50\n End\n"
	"End\n"
	// no SplitHordeNumber and one template only: nothing to split by template, the check refuses
	"Object P265UniformHorde\n"
	" KindOf = HORDE MELEE_HORDE SELECTABLE\n"
	" Geometry = BOX\n GeometryMajorRadius = 30\n GeometryMinorRadius = 45\n GeometryHeight = 20\n"
	" Body = ImmortalBody BodyTag\n MaxHealth = 1\n End\n"
	" Behavior = HordeAIUpdate AITag\n End\n"
	" Behavior = HordeContain ContainTag\n"
	"  Slots = 2\n"
	"  RankInfo = RankNumber:1 UnitType:Swordsman Position:X:20 Y:0\n"
	"  RankInfo = RankNumber:2 UnitType:Swordsman Position:X:-20 Y:0\n"
	"  InitialPayload = Swordsman 2\n"
	"  SplitHorde = SplitResult:P265SwordHorde UnitType:Swordsman\n"
	" End\n"
	" LocomotorSet\n Locomotor = HordeLoco\n Condition = SET_NORMAL\n Speed = 50\n End\n"
	"End\n";

struct Port265World : combattest::CombatWorld
{
	Port265World()
	{
		const std::string error = w.load(kPort265Objects);
		REQUIRE_MESSAGE(error.empty(), error);
	}
};

} // namespace

// RW 0x86DFDF .. 0x86E01C: SplitHordeNumber 2 and two members — the update's own check (RW 0x8730C2, every 5th frame) splits the horde without any
// special power. Without the change the horde keeps both members for as long as it runs.
TEST_CASE("PORT-265 HordeContain: a horde at its SplitHordeNumber splits on its own")
{
	for (GameEdition edition : { GameEdition::RotWK201, GameEdition::BFME2_106 })
	{
		Port265World w;
		w.logic->setEdition(edition);
		CAPTURE(gameEditionName(edition));
		Object *horde = w.unit("P265DueHorde", 'A', 300, 300);
		REQUIRE(horde);
		const ObjectID id = horde->getID();
		w.frames(2);
		REQUIRE(horde->getContain());
		REQUIRE(horde->getContain()->getContainCount() == 2);
		w.frames(6); // the check runs when the logic frame is a multiple of 5 (RW 0xD9F608)
		CHECK(w.byId(id) == nullptr); // the horde split itself away (RW 0x86E090 -> RW 0x8791C5 destroys it)
	}
}

// RW 0x86E01C: a horde holding more members than SplitHordeNumber is left alone by the check
TEST_CASE("PORT-265 HordeContain: a horde above its SplitHordeNumber does not split")
{
	for (GameEdition edition : { GameEdition::RotWK201, GameEdition::BFME2_106 })
	{
		Port265World w;
		w.logic->setEdition(edition);
		CAPTURE(gameEditionName(edition));
		Object *horde = w.unit("P265CrowdedHorde", 'A', 300, 300);
		REQUIRE(horde);
		const ObjectID id = horde->getID();
		w.frames(2);
		REQUIRE(horde->getContain());
		REQUIRE(horde->getContain()->getContainCount() == 2);
		w.frames(10);
		REQUIRE(w.byId(id) == horde);
		CHECK(horde->getContain()->getContainCount() == 2);
	}
}

// RW 0x86E03A .. 0x86E06D: with SplitHordeNumber 0 the members' templates decide — a horde of one template splits, a mixed one does not
TEST_CASE("PORT-265 HordeContain: without SplitHordeNumber the member templates decide")
{
	Port265World w;
	Object *mixed = w.unit("P265MixedHorde", 'A', 300, 300);
	Object *uniform = w.unit("P265UniformHorde", 'A', 380, 300);
	REQUIRE(mixed);
	REQUIRE(uniform);
	const ObjectID mixedId = mixed->getID();
	const ObjectID uniformId = uniform->getID();
	w.frames(2);
	REQUIRE(mixed->getContain());
	REQUIRE(uniform->getContain());
	REQUIRE(mixed->getContain()->getContainCount() == 2);
	w.frames(6);
	CHECK(w.byId(uniformId) == nullptr);   // Swordsman only: one template, the horde splits (RW 0x86E066)
	REQUIRE(w.byId(mixedId) == mixed);     // Swordsman + Archer: two templates, the check sends it home (RW 0x86E06D .. 0x86E0AD)
	CHECK(mixed->getContain()->getContainCount() == 2);
}
