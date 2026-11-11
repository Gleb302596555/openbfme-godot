// OpenBFME unit tests: PORT-112 HordeContain/NotComboFormation (issue #272).
//
// Target facts (RotWK 2.01 game.dat, read with tools/mod/rw.py; the same addresses are in the comments of
// GameLogic/Object/Contain/HordeContainRuntime.cpp and GameLogic/Module/SpecialPowerModules.cpp):
//   * NotComboFormation is the HordeContain module data byte + 0x25C; the field table row RW 0xC5B8BC of the table RW 0xC5BD50 (parser RW 0x42E558)
//     writes exactly that offset, and the port's row carries the same INI name (HordeContain.cpp, offsetof(HordeContainModuleData, m_notComboFormation)). Getter RW 0x872B91 (interface slot 136 / +0x220): `mov eax, [ecx - 0x118]` (module data, the interface sits at
//     module + 0x11C), `mov al, [eax + 0x25c]`, `ret`; the slot is in RW 0xC5B1F8 (the horde contains' shared base), RW 0xC5BE98 (HordeContain) and
//     RW 0xC5CFA8 (AODHordeContain).
//   * Its ONLY reader is RW 0x751716, the onEnter (slot 4 of vtable RW 0xC29548) of AICombineState, whose constructor RW 0x7448DE pushes the name key
//     0xD2CCD3C: the ordered object's contain (RW 0x68C866) is asked the getter (RW 0x751738), and when the byte is set RW 0x751746 calls slot 0x60
//     (RW 0x86C7F6, the formation swap to the AlternateFormation template, lane HORDE-2). A porcupine formation is therefore NOT refused a combine: the
//     horde leaves the formation first and the combine then runs.
//   * The port's order (LOCAL-18 CombineHordeSpecialPower::doSpecialPower, unit path RW 0x8CCCB4) has no AI state: the scan and the combine run there,
//     so the swap sits at the order's start (stop S-7293).

#include "doctest.h"
#include "CombatTestUtil.h"
#include "Common/SpecialPower.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/Object/Contain/HordeContainRuntime.h"
#include "GameLogic/Object/Object.h"

namespace {

const char kPort112Ini[] =
	"SpecialPower SpecialAbilityCombineWithNearest\n"
	" Enum = SPECIAL_GIVE_UPGRADE_NEAREST\n"
	" ReloadTime = 100\n"
	"End\n"
	"Object Port112Swordsman\n"
	" KindOf = INFANTRY SELECTABLE\n"
	" Body = ActiveBody BodyTag\n"
	"  MaxHealth = 100\n"
	" End\n"
	" Behavior = AIUpdateInterface AITag\n"
	" End\n"
	"End\n"
	"Object Port112Banner\n"
	" KindOf = INFANTRY BANNER SELECTABLE\n"
	" Body = ActiveBody BodyTag\n"
	"  MaxHealth = 100\n"
	" End\n"
	" Behavior = AIUpdateInterface AITag\n"
	" End\n"
	" Behavior = CombineHordeSpecialPower ModuleTag_Combine\n"
	"  SpecialPowerTemplate = SpecialAbilityCombineWithNearest\n"
	"  ScanRange = 400.0\n"
	" End\n"
	"End\n"
	// the main formation: the AlternateFormation the porcupine formation swaps back to
	"Object Port112PikemanHorde\n"
	" KindOf = HORDE SELECTABLE\n"
	" Geometry = BOX\n GeometryMajorRadius = 30\n GeometryMinorRadius = 45\n GeometryHeight = 20\n"
	" Body = ImmortalBody BodyTag\n  MaxHealth = 1\n End\n"
	" Behavior = HordeAIUpdate AITag\n End\n"
	" Behavior = HordeContain ContainTag\n"
	"  Slots = 1\n"
	"  RankInfo = RankNumber:1 UnitType:Port112Banner Position:X:0 Y:0\n"
	"  InitialPayload = Port112Banner 1\n"
	"  ThisFormationIsTheMainFormation = Yes\n"
	" End\n"
	" LocomotorSet\n  Locomotor = HordeLoco\n  Condition = SET_NORMAL\n  Speed = 50\n End\n"
	"End\n"
	// the porcupine formation: NotComboFormation = Yes, its AlternateFormation is the main one (as IsengardPikemanHordePorcupine in Ennorath)
	"Object Port112PikemanHordePorcupine\n"
	" KindOf = HORDE SELECTABLE\n"
	" Geometry = BOX\n GeometryMajorRadius = 30\n GeometryMinorRadius = 45\n GeometryHeight = 20\n"
	" Body = ImmortalBody BodyTag\n  MaxHealth = 1\n End\n"
	" Behavior = HordeAIUpdate AITag\n End\n"
	" Behavior = HordeContain ContainTag\n"
	"  Slots = 1\n"
	"  RankInfo = RankNumber:1 UnitType:Port112Banner Position:X:0 Y:0\n"
	"  InitialPayload = Port112Banner 1\n"
	"  AlternateFormation = Port112PikemanHorde\n"
	"  IsPorcupineFormation = Yes\n"
	"  ThisFormationIsTheMainFormation = No\n"
	"  NotComboFormation = Yes\n"
	" End\n"
	" LocomotorSet\n  Locomotor = HordeLoco\n  Condition = SET_NORMAL\n  Speed = 50\n End\n"
	"End\n"
	// a main-formation horde without the field: the same order must leave it alone
	"Object Port112PlainHorde\n"
	" KindOf = HORDE SELECTABLE\n"
	" Geometry = BOX\n GeometryMajorRadius = 30\n GeometryMinorRadius = 45\n GeometryHeight = 20\n"
	" Body = ImmortalBody BodyTag\n  MaxHealth = 1\n End\n"
	" Behavior = HordeAIUpdate AITag\n End\n"
	" Behavior = HordeContain ContainTag\n"
	"  Slots = 1\n"
	"  RankInfo = RankNumber:1 UnitType:Port112Banner Position:X:0 Y:0\n"
	"  InitialPayload = Port112Banner 1\n"
	"  ThisFormationIsTheMainFormation = Yes\n"
	" End\n"
	" LocomotorSet\n  Locomotor = HordeLoco\n  Condition = SET_NORMAL\n  Speed = 50\n End\n"
	"End\n"
	// the horde the porcupine formation combines with: its ComboHorde entry names the porcupine template as the Target (Ennorath does the same)
	"Object Port112ReceiverHorde\n"
	" KindOf = HORDE SELECTABLE\n"
	" Geometry = BOX\n GeometryMajorRadius = 30\n GeometryMinorRadius = 45\n GeometryHeight = 20\n"
	" Body = ImmortalBody BodyTag\n  MaxHealth = 1\n End\n"
	" Behavior = HordeAIUpdate AITag\n End\n"
	" Behavior = HordeContain ContainTag\n"
	"  Slots = 2\n"
	"  RankInfo = RankNumber:1 UnitType:Port112Swordsman Position:X:20 Y:0\n"
	"  RankInfo = RankNumber:2 UnitType:Port112Swordsman Position:X:-20 Y:0\n"
	"  InitialPayload = Port112Swordsman 2\n"
	"  ThisFormationIsTheMainFormation = Yes\n"
	"  ComboHorde = Target:Port112PikemanHordePorcupine Result:Port112ComboHorde InitiateVoice:NoSound\n"
	" End\n"
	" LocomotorSet\n  Locomotor = HordeLoco\n  Condition = SET_NORMAL\n  Speed = 50\n End\n"
	"End\n"
	"Object Port112ComboHorde\n"
	" KindOf = HORDE COMBO_HORDE SELECTABLE\n"
	" Geometry = BOX\n GeometryMajorRadius = 30\n GeometryMinorRadius = 45\n GeometryHeight = 20\n"
	" Body = ImmortalBody BodyTag\n  MaxHealth = 1\n End\n"
	" Behavior = HordeAIUpdate AITag\n End\n"
	" Behavior = HordeContain ContainTag\n"
	"  Slots = 3\n"
	"  RankInfo = RankNumber:1 UnitType:Port112Swordsman Position:X:20 Y:0 Position:X:20 Y:20\n"
	"  RankInfo = RankNumber:2 UnitType:Port112Banner Position:X:-20 Y:0\n"
	" End\n"
	" LocomotorSet\n  Locomotor = HordeLoco\n  Condition = SET_NORMAL\n  Speed = 50\n End\n"
	"End\n";

struct Port112World : combattest::CombatWorld
{
	SpecialPowerStore powers;
	SpecialPowerStore *previous = TheSpecialPowerStore;

	Port112World()
	{
		TheSpecialPowerStore = &powers;
		w.fx.env.blocks.registerBlock("SpecialPower", [](INI *ini) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(ini); });
		const std::string error = w.load(kPort112Ini, INI_LOAD_OVERWRITE, "port112.ini");
		REQUIRE_MESSAGE(error.empty(), error);
	}

	~Port112World()
	{
		TheSpecialPowerStore = previous;
	}
};

HordeContain *hordeContainOf(Object *object)
{
	return object ? dynamic_cast<HordeContain *>(object->getContain()) : nullptr;
}

// the module of the banner the horde keeps as its member
CombineHordeSpecialPower *combinePowerOf(combattest::CombatWorld &world, Object *horde)
{
	const std::vector<Object *> members = world.membersOf(horde);
	for (Object *member : members)
	{
		if (auto *power = dynamic_cast<CombineHordeSpecialPower *>(member->findModule("CombineHordeSpecialPower")))
		{
			return power;
		}
	}
	return nullptr;
}

} // namespace

// The parse and the getter: the row "NotComboFormation" (the RW row RW 0xC5B8BC that writes module data + 0x25C) is parsed, and slot 136 returns it.
TEST_CASE("PORT-112: NotComboFormation is the module data byte the slot-136 getter returns")
{
	Port112World world;
	Object *porcupine = world.unit("Port112PikemanHordePorcupine", 'A', 300, 300);
	Object *plain = world.unit("Port112PlainHorde", 'A', 1200, 1200);
	REQUIRE(porcupine);
	REQUIRE(plain);
	world.frames(2);

	HordeContain *porcupineContain = hordeContainOf(porcupine);
	HordeContain *plainContain = hordeContainOf(plain);
	REQUIRE(porcupineContain);
	REQUIRE(plainContain);
	CHECK(porcupineContain->hordeData().m_notComboFormation);
	CHECK_FALSE(plainContain->hordeData().m_notComboFormation);
	// the same answer through the interface slot the RW reader calls
	HordeContainInterface *porcupineInterface = porcupineContain;
	HordeContainInterface *plainInterface = plainContain;
	CHECK(porcupineInterface->notComboFormation());
	CHECK_FALSE(plainInterface->notComboFormation());
}

// The one call RW 0x751746 makes: a horde in a NotComboFormation formation leaves it (slot 0x60, RW 0x86C7F6) when the combine order starts.
// Without the ported call the horde keeps the porcupine HordeContain and this test fails.
TEST_CASE("PORT-112: the combine order takes a porcupine formation out of its alternate formation")
{
	Port112World world;
	Object *horde = world.unit("Port112PikemanHordePorcupine", 'A', 300, 300);
	REQUIRE(horde);
	world.frames(2);

	HordeContain *before = hordeContainOf(horde);
	REQUIRE(before);
	REQUIRE(before->hordeData().m_isPorcupineFormation);
	const unsigned membersBefore = before->getContainCount();
	REQUIRE(membersBefore == 1);

	CombineHordeSpecialPower *power = combinePowerOf(world, horde);
	REQUIRE(power);
	power->doSpecialPower(0);

	// no partner is in range: the horde stays, out of the porcupine formation
	CHECK_FALSE(horde->isDestroyed());
	HordeContain *after = hordeContainOf(horde);
	REQUIRE(after);
	CHECK(after != before);
	CHECK_FALSE(after->hordeData().m_isPorcupineFormation);
	CHECK(after->hordeData().m_thisFormationIsTheMainFormation);
	CHECK_FALSE(after->hordeData().m_notComboFormation);
	CHECK(after->getContainCount() == membersBefore);
}

// The neighbour of the ported call: the combine itself still runs after the swap (the porcupine horde's template is the ComboHorde Target).
TEST_CASE("PORT-112: a porcupine horde still combines after leaving the formation")
{
	Port112World world;
	Object *porcupine = world.unit("Port112PikemanHordePorcupine", 'A', 300, 300);
	Object *receiver = world.unit("Port112ReceiverHorde", 'A', 340, 300);
	REQUIRE(porcupine);
	REQUIRE(receiver);
	world.frames(2);

	CombineHordeSpecialPower *power = combinePowerOf(world, porcupine);
	REQUIRE(power);
	power->doSpecialPower(0);

	CHECK(porcupine->isDestroyed());
	CHECK(receiver->isDestroyed());
	bool foundCombo = false;
	for (Object *cur = world.logic->getFirstObject(); cur; cur = cur->getNextObject())
	{
		if (!cur->isDestroyed() && cur->getTemplate()->getName() == "Port112ComboHorde")
		{
			foundCombo = true;
			CHECK(cur->getTeam() == world.teamA());
			break;
		}
	}
	CHECK(foundCombo);
}

// The other side of the gate: without NotComboFormation the order changes nothing (RW 0x75173E falls through to the debug log and the base onEnter).
TEST_CASE("PORT-112: a horde without NotComboFormation keeps its contain when the order starts")
{
	Port112World world;
	Object *horde = world.unit("Port112PlainHorde", 'A', 300, 300);
	REQUIRE(horde);
	world.frames(2);

	HordeContain *before = hordeContainOf(horde);
	REQUIRE(before);
	REQUIRE_FALSE(before->hordeData().m_notComboFormation);
	const unsigned membersBefore = before->getContainCount();

	CombineHordeSpecialPower *power = combinePowerOf(world, horde);
	REQUIRE(power);
	power->doSpecialPower(0);

	CHECK_FALSE(horde->isDestroyed());
	HordeContain *after = hordeContainOf(horde);
	REQUIRE(after);
	CHECK(after == before);
	CHECK(after->getContainCount() == membersBefore);
}
