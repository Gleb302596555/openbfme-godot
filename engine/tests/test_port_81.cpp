// OpenBFME unit tests. GPL-3.0.
// PORT-81 (issue #213): HordeContain / RanksToJustFreeWhenAttacking — parsed and stored, never read by RotWK 2.01.
//
// Binary verification against RotWK 2.01 game.dat (md5 daed3668006cd90f01c34e5a7da1901f); full table in
// docs/ennorath/PORT81_RW_ANALYSIS.md:
//   * Field row RW 0xC5BB90 in the HordeContain module-data table RW 0xC5BB50 (handed to INI::initFromINIMulti by RW 0x878B63,
//     `push 0xC5BB50` at RW 0x878B73): name RW 0xC5BACC "RanksToJustFreeWhenAttacking", parser RW 0x86DED1, offset +0x1C4.
//     The neighbour row RW 0xC5BB80 is "RanksToReleaseWhenAttacking" (+0x1B8) with the *same* parser.
//   * The class treats the two sets identically in its data constructor (RW 0x878F4E `lea ecx,[esi+0x1B8]`, RW 0x878F5D
//     `lea ecx,[esi+0x1C4]`, both `call 0x46C1DA`) and in the copy/lifecycle pair (RW 0x878C86 / RW 0x878C91 -> RW 0x4728D3,
//     RW 0x878DA6 / RW 0x878D97 -> RW 0x472980).
//   * +0x1B8 is read by exactly three HordeContain slots — attackTargetNow RW 0x875221 (module data at [edi-0x118], RW 0x875259 ->
//     RW 0x875264 `add eax,0x1B8`), attackPositionNow RW 0x87545C (RW 0x8754A3) and RW 0x875562 (RW 0x87558A) — and each read is
//     a rank lookup through the member map RW 0x865598, the slot map RW 0x6B4E57 and the 0x1C-byte slot record whose first field
//     is the rank (RW 0x87536C `imul eax,eax,0x1C`).
//   * +0x1C4 is read by nobody: in the whole HordeContain region RW 0x860000..0x890000 every instruction naming +0x1C4 is one of
//     the three constructor/copy sites above. The byte-by-byte resynchronisation scan of the entire .text (185 real instruction
//     sites with displacement 0x1C4) finds no other module-data access; the remaining ones are the AIUpdate vtable slot
//     `call [eax+0x1C4]` (RW 0x875327 and 18 more), unrelated classes (RW 0x903522 / RW 0x911455 keep a pointer pair at
//     +0x1BC..+0x1C8; RW 0x881960 keeps its own 0x18-stride array at +0x1B8) and array strides / stack frames.
//
// So RotWK has no "just free" behaviour to port: the port must keep parsing the field and must not invent a second release path.
// These tests pin that (they fail if a just-free release is added next to RanksToReleaseWhenAttacking), and the second test holds
// the neighbouring behaviour that *is* RotWK's: the released ranks attack and the other ranks stay in formation.

#include "doctest.h"

#include "CombatTestUtil.h"
#include "HordeTestUtil.h"

#include "GameLogic/AI/AIStateMachine.h"
#include "GameLogic/Object/Contain/HordeContainRuntime.h"

#include <set>

using namespace combattest;
using namespace initest;

namespace
{

// a horde whose HordeContain body is given verbatim: four ranks, one man each, so that a rank set is directly observable in the
// members' goals. MELEE_HORDE keeps HordeContain::attackTargetNow away from the touch-attack branch (RW 0x875320), which makes the
// released ranks take aiAttackObject like the ranged case.
std::string fourRankHorde(const std::string &name, const std::string &memberTemplate, const std::string &containBody)
{
	return
		"Object " + name + "\n"
		"  KindOf = HORDE LARGE_RECTANGLE_PATHFIND SELECTABLE CAN_ATTACK\n"
		"  VisionRange = 250\n"
		"  Geometry = BOX\n"
		"  GeometryMajorRadius = 30\n"
		"  GeometryMinorRadius = 45\n"
		"  GeometryHeight = 20\n"
		"  WeaponSet\n"
		"    Conditions = None\n"
		"    Weapon = PRIMARY HordeRangefinder\n"
		"  End\n"
		"  Body = ImmortalBody ModuleTag_Body\n"
		"    MaxHealth = 1\n"
		"  End\n"
		"  Behavior = HordeAIUpdate ModuleTag_AI\n"
		"  End\n"
		"  Behavior = HordeContain ModuleTag_Contain\n"
		"    RankInfo = RankNumber:1 UnitType:" + memberTemplate + " Position:X:50 Y:0\n"
		"    RankInfo = RankNumber:2 UnitType:" + memberTemplate + " Position:X:30 Y:0\n"
		"    RankInfo = RankNumber:3 UnitType:" + memberTemplate + " Position:X:10 Y:0\n"
		"    RankInfo = RankNumber:4 UnitType:" + memberTemplate + " Position:X:-10 Y:0\n" +
		"    InitialPayload = " + memberTemplate + " 4\n" +
		containBody +
		"  End\n"
		"  LocomotorSet\n"
		"    Locomotor = HordeLoco\n"
		"    Condition = SET_NORMAL\n"
		"    Speed = 50\n"
		"  End\n"
		"End\n";
}

// the member goal of a state machine that is attacking (the same accessor test_archer_spread.cpp uses)
Object *goalOf(Object *m)
{
	AIUpdateInterface *ai = m->getAIUpdateInterface();
	const AIStateMachine *sm = ai ? ai->stateMachineOrNull() : nullptr;
	return ai && ai->isAttacking() && sm ? sm->goalObject() : nullptr;
}

// rank of the slot a member occupies (HordeContainCore::Slot::rank, the first field of the 0x1C-byte slot record;
// RW 0x87536C `imul eax,eax,0x1C` + `mov eax,[eax+ecx]`)
int rankOf(CombatWorld &w, Object *horde, Object *member)
{
	HordeContain *hc = w.hordeOf(horde);
	const int slot = hc->core().slotOf(member->getID());
	REQUIRE(slot >= 0); // tolerance: a precondition (the member has a slot; its rank is printed on failure below)
	return hc->core().slots()[(size_t)slot].rank;
}

} // namespace

// PORT-81: the field is a rank set of the HordeContain module data, parsed exactly like RanksToReleaseWhenAttacking
// (RW 0xC5BB90 / RW 0xC5BB80 share the parser RW 0x86DED1). This is the half of the verdict the port already had; it is pinned
// here because PORT-81's stop rests on it.
TEST_CASE("PORT-81: RanksToJustFreeWhenAttacking parses into its own rank set, like RanksToReleaseWhenAttacking")
{
	HordeContainModuleData d;
	Fixture fx;
	fx.env.blocks.registerBlock("Body", [&d](INI *ini) { d.parseFromINI(ini); });
	const std::string err = loadError(fx.env, "body.ini",
		"Body\n  RanksToReleaseWhenAttacking = 1 2\n  RanksToJustFreeWhenAttacking = 1 2\nEnd\n", INI_LOAD_OVERWRITE);
	REQUIRE_MESSAGE(err.empty(), err);
	CHECK(d.m_ranksToReleaseWhenAttacking == std::set<int>{ 1, 2 });
	CHECK(d.m_ranksToJustFreeWhenAttacking == std::set<int>{ 1, 2 });
	CHECK(&d.m_ranksToReleaseWhenAttacking != &d.m_ranksToJustFreeWhenAttacking);

	// "0" is what the mod data mostly carries: it is the rank number 0, not an empty set (the same parser, RW 0x86DED1, inserts
	// every token; the constructor RW 0x878F5D makes an empty set before the row is read)
	HordeContainModuleData zero;
	Fixture fx2;
	fx2.env.blocks.registerBlock("Body", [&zero](INI *ini) { zero.parseFromINI(ini); });
	const std::string err2 = loadError(fx2.env, "body2.ini",
		"Body\n  RanksToJustFreeWhenAttacking = 0\nEnd\n", INI_LOAD_OVERWRITE);
	REQUIRE_MESSAGE(err2.empty(), err2);
	CHECK(zero.m_ranksToJustFreeWhenAttacking == std::set<int>{ 0 });
	CHECK(zero.m_ranksToReleaseWhenAttacking.empty());
}

// PORT-81 (the test that fails if a just-free release is invented): a four-rank horde with `RanksToJustFreeWhenAttacking = 1 2`
// and `RanksToReleaseWhenAttacking = 1 2` takes exactly the goals of the horde without the just-free line. RW has no reader of
// +0x1C4, so the two hordes cannot differ; ranks 3 and 4 stay in formation in both.
TEST_CASE("PORT-81: a horde with RanksToJustFreeWhenAttacking = 1 2 attacks exactly like one without the line")
{
	const std::string withFree = fourRankHorde("FreeHorde", "Archer",
		"    RanksToReleaseWhenAttacking = 1 2\n"
		"    RanksToJustFreeWhenAttacking = 1 2\n");
	const std::string withoutFree = fourRankHorde("PlainHorde", "Archer", kReleaseOnly);

	struct Case
	{
		const char *name;
		const std::string *ini;
	};
	const Case cases[] = { { "FreeHorde", &withFree }, { "PlainHorde", &withoutFree } };
	int releasedPerCase[2] = { 0, 0 };
	for (int c = 0; c < 2; ++c)
	{
		CombatWorld w(cases[c].ini->c_str());
		w.combat().setAutoAcquireEnabled(false);
		Object *horde = w.unit(cases[c].name, 'A', 300, 300);
		Object *target = w.unit("TargetHorde", 'B', 420, 300);
		w.frames(4);
		HordeContain *hc = w.hordeOf(horde);
		REQUIRE(hc != nullptr);
		REQUIRE(w.membersOf(horde).size() == 4);
		// the module data really carries the set (this is what the engine parses, RW 0xC5BB90 -> +0x1C4)
		if (c == 0)
		{
			CHECK(hc->hordeData().m_ranksToJustFreeWhenAttacking == std::set<int>{ 1, 2 });
		}
		else
		{
			CHECK(hc->hordeData().m_ranksToJustFreeWhenAttacking.empty());
		}
		CHECK(hc->hordeData().m_ranksToReleaseWhenAttacking == std::set<int>{ 1, 2 });
		REQUIRE(horde->getAIUpdateInterface()->aiAttackObject(target, CMD_FROM_PLAYER));
		const int n = w.runUntil([&] { return hc->attackStats().fires > 0; }, 120);
		CHECK(n < 120); // tolerance: the frame the first fire happens (attackTargetNow is reached within a few frames)
		// every released rank got its own order (RW 0x875221 -> RW 0x66C536); ranks 3 and 4 were not released
		CHECK(hc->attackStats().orders == 2);
		int ordered = 0;
		for (Object *m : w.membersOf(horde))
		{
			const int rank = rankOf(w, horde, m);
			const bool released = rank == 1 || rank == 2;
			if (released)
			{
				++ordered;
				CHECK_MESSAGE(goalOf(m) != nullptr, "rank " << rank << " was released by RanksToReleaseWhenAttacking");
			}
			else
			{
				CHECK_MESSAGE(goalOf(m) == nullptr, "rank " << rank << " is not in RanksToReleaseWhenAttacking: it keeps formation");
			}
		}
		CHECK(ordered == 2);
		releasedPerCase[c] = ordered;
	}
	// the two hordes agree rank for rank: +0x1C4 changes nothing (no reader in RW)
	CHECK(releasedPerCase[0] == releasedPerCase[1]);
}

// PORT-81: the neighbouring behaviour must keep holding — with no RanksToReleaseWhenAttacking at all the horde releases nobody,
// and `RanksToJustFreeWhenAttacking = 1 2 3 4` alone releases nobody either (RW reads +0x1B8 only; a just-free set that covered
// every rank would otherwise release the whole horde).
TEST_CASE("PORT-81: RanksToJustFreeWhenAttacking alone releases no rank; every goal stays with the formation")
{
	for (int c = 0; c < 2; ++c)
	{
		const std::string ini = c == 0
			? fourRankHorde("NoSetHorde", "Archer", "")
			: fourRankHorde("FreeOnlyHorde", "Archer", "    RanksToJustFreeWhenAttacking = 1 2 3 4\n");
		CombatWorld w(ini.c_str());
		w.combat().setAutoAcquireEnabled(false);
		Object *horde = w.unit(c == 0 ? "NoSetHorde" : "FreeOnlyHorde", 'A', 300, 300);
		Object *target = w.unit("TargetHorde", 'B', 420, 300);
		w.frames(4);
		HordeContain *hc = w.hordeOf(horde);
		REQUIRE(hc != nullptr);
		CHECK(hc->hordeData().m_ranksToReleaseWhenAttacking.empty());
		if (c == 1)
		{
			CHECK(hc->hordeData().m_ranksToJustFreeWhenAttacking == std::set<int>{ 1, 2, 3, 4 });
		}
		REQUIRE(horde->getAIUpdateInterface()->aiAttackObject(target, CMD_FROM_PLAYER));
		w.frames(30);
		CHECK(hc->attackStats().fires > 0); // tolerance: a precondition (the horde engaged at all: orders stay 0 by design)
		CHECK(hc->attackStats().orders == 0);
		for (Object *m : w.membersOf(horde))
		{
			CHECK(goalOf(m) == nullptr);
		}
	}
}
