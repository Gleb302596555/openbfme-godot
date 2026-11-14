// OpenBFME. PORT-427 (issue #427, P-110 in docs/ennorath/PROBLEMS.md) tests: a porcupine formation (HordeContain with IsPorcupineFormation = Yes,
// the Ennorath shape of AngmarDunedainPikemanHordePorcupine and the 51 other active porcupine objects) makes every member face OUTWARD, each its own way.
// Before the fix every member faced the horde's angle (the order's facing was owner.angle + the slot record's angle word, and the port never set a
// non-zero slot record angle, so all members took the same facing).
//
// The expectation is read off RotWK 2.01 (game.dat md5 daed3668, caveat S-001), never off the ported code:
//
//   * the member pass is RW 0x873FE8 (the order vfunc vslot 0x90, RW 0x877A7A = HordeMemberOrder::apply, with force = 1 - isMoving). Per member it calls the
//     horde interface's slot 7 (RW 0x877D89 through the call RW 0x8740AC: the formation slot position; the port's getSlotWorldPos) to get the slot position,
//     and the slot record's angle word it returns is 0 for these formations (RW 0x87591D fstp's +0x14 of the slot record, which the builder RW 0x87761A
//     writes as 0.0f at RW 0x877A17 territory; nothing in the image sets it for a RankInfo).
//   * when the interface vslot +0xF0 (RW 0x872BB8, isPorcupineFormation) answers true, RW 0x8740C1 .. 0x874117 RECOMPUTES the goal facing and overwrites it:
//       dx = slot.x - owner.x  (Object + 0x38 / + 0x3C, the horde's position, [ebp - 0x24])
//       dy = slot.y - owner.y
//       if dx*dx + dy*dy == 0.0f exactly (RW 0x8740FA ucomiss + 0x8740B `test ah, 0x44` + 0x8740B `jp 0x87411C`; the jump is NOT taken on an exactly zero sum)
//           angle = [owner + 0x44]            (the horde's own orientation, RW 0x87410D .. 0x874112)
//       else
//           normalize dx, dy in place (RW 0x405595, MSVCR71 sqrt)
//           a = ACos(x')                      (RW 0x42F4F0 -> MSVCR71 jmp [0xBD05E0]; the port's deterministic SimMath::acosDet, parity stop S-167)
//           if (dy >= 0) angle = a            (comiss 0.0f, dy; jbe keeps a, RW 0x874139 .. 0x87413A)
//           else angle = 2*pi - a             (movss 2*pi at VA 0xBDD38C; subss a from it, RW 0x87413F .. 0x874153)
//   * the member's turn goes through the move hub (RW 0x87468B -> the near arm RW 0x874FA6 .. 0x8750E1): a member more than 10 degrees (RW 0xC5B69C,
//     the port's kTurnThreshold) off the goal clears MOVING and sets the locomotor goal orientation; the member's AI (AIGOAL_ANGLE) then puts the
//     object's orientation on the goal exactly.
//
// The synthetic objects mirror the Ennorath porcupine hordes (a ring of RankInfo positions around the anchor, IsPorcupineFormation = Yes, the main
// formation the twin without the flag) and the Ennorath zero-offset case (RandomOffset omitted). No game or mod file is read here.
//
// TEST_CASE names start with PORT-427 (the run filter -tc="PORT-427*").
#include "doctest.h"
#include "MoveTestUtil.h"

#include "GameLogic/Object/Contain/HordeContainCore.h"
#include "GameLogic/SimMath.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <vector>

using namespace movetest;

namespace
{
// eight slots in a ring (cardinal radius 50, diagonal radius 50): the AngmarDunedainPikemanHordePorcupine shape (Ennorath 1.9.8.2)
const char kP427Objects[] =
	"Object P427PorcupineHorde\n"
	"  KindOf = HORDE MELEE_HORDE LARGE_RECTANGLE_PATHFIND SELECTABLE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 30\n"
	"  GeometryMinorRadius = 45\n"
	"  GeometryHeight = 20\n"
	"  Behavior = HordeAIUpdate ModuleTag_AI\n"
	"  End\n"
	"  Behavior = HordeContain ModuleTag_Contain\n"
	"    Slots = 8\n"
	"    IsPorcupineFormation = Yes\n"
	"    RankInfo = RankNumber:1 UnitType:MemberOfHorde Position:X:50 Y:0 Position:X:0 Y:50 Position:X:-50 Y:0 Position:X:0 Y:-50 Position:X:35.35534 Y:35.35534 Position:X:-35.35534 Y:35.35534 Position:X:-35.35534 Y:-35.35534 Position:X:35.35534 Y:-35.35534\n"
	"    InitialPayload = MemberOfHorde 8\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = HordeLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 50\n"
	"  End\n"
	"End\n"
	// the same horde without the flag: the line formation, RW 0x8740C1 not taken (every member keeps the horde's facing)
	"Object P427LineHorde\n"
	"  KindOf = HORDE MELEE_HORDE LARGE_RECTANGLE_PATHFIND SELECTABLE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 30\n"
	"  GeometryMinorRadius = 45\n"
	"  GeometryHeight = 20\n"
	"  Behavior = HordeAIUpdate ModuleTag_AI\n"
	"  End\n"
	"  Behavior = HordeContain ModuleTag_Contain\n"
	"    Slots = 8\n"
	"    RankInfo = RankNumber:1 UnitType:MemberOfHorde Position:X:50 Y:0 Position:X:0 Y:50 Position:X:-50 Y:0 Position:X:0 Y:-50 Position:X:35.35534 Y:35.35534 Position:X:-35.35534 Y:35.35534 Position:X:-35.35534 Y:-35.35534\n"
	"    InitialPayload = MemberOfHorde 8\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = HordeLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 50\n"
	"  End\n"
	"End\n"
	// a porcupine horde with a member on the anchor itself: the exactly-zero vector keeps the horde's own orientation (RW 0x8740FA .. 0x874117)
	"Object P427CenterHorde\n"
	"  KindOf = HORDE MELEE_HORDE LARGE_RECTANGLE_PATHFIND SELECTABLE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 30\n"
	"  GeometryMinorRadius = 45\n"
	"  GeometryHeight = 20\n"
	"  Behavior = HordeAIUpdate ModuleTag_AI\n"
	"  End\n"
	"  Behavior = HordeContain ModuleTag_Contain\n"
	"    Slots = 3\n"
	"    IsPorcupineFormation = Yes\n"
	"    RankInfo = RankNumber:1 UnitType:MemberOfHorde Position:X:-50 Y:0 Position:X:0 Y:0 Position:X:50 Y:0\n"
	"    InitialPayload = MemberOfHorde 3\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = HordeLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 50\n"
	"  End\n"
	"End\n";

struct Port427World : MoveWorld
{
	Port427World()
		: MoveWorld(100, 100, kP427Objects)
	{
		buildMap();
	}
	// the slot's world position from the contain's own table (the placement the member pass uses)
	Coord3D slotWorld(HordeContain *hc, const Object *horde, const Object *member, float &slotAngle)
	{
		HordeContainCore::Placement pl;
		pl.position = *horde->getPosition();
		pl.angle = horde->getOrientation();
		return const_cast<HordeContainCore &>(hc->core()).getSlotWorldPos(member->getID(), pl, &slotAngle);
	}
};

// the radial bearing of RW 0x8740C1 .. 0x874153, spelled out from the binary: dx = slot - owner, normalize, ACos of the normalized x, 2*pi - a when dy < 0.
// The test computes the expectation here, not from the ported member pass (S-167: the CRT ACos is the reference; std::acos is its libm spelling).
float radialBearingFromBinary(const Coord3D &slot, const Coord3D &owner)
{
	const float dx = slot.x - owner.x, dy = slot.y - owner.y;
	const float len2 = dx * dx + dy * dy;
	// tolerance: the zero vector is the caller's case; here the caller guarantees a nonzero offset
	REQUIRE(len2 > 0.0f);
	const float len = std::sqrt(len2);
	float c = dx / len;
	if (c > 1.0f)
		c = 1.0f;
	else if (c < -1.0f)
		c = -1.0f;
	float a = (float)std::acos((double)c);
	if (dy < 0.0f)
	{
		a = 6.28318548f - a; // 2*pi at VA 0xBDD38C
	}
	return a;
}

float angleDiff(float a, float b)
{
	float d = std::fmod(a - b, 6.28318548f);
	if (d > 3.14159274f)
	{
		d -= 6.28318548f;
	}
	else if (d < -3.14159274f)
	{
		d += 6.28318548f;
	}
	return std::fabs(d);
}

// runs until every member stands within `tol` of its slot (or `maxFrames`); then a few extra frames for the AIGOAL_ANGLE goal to land.
// returns the worst slot error seen after the run (the caller asserts it: members that never reached their slots turn too late to be checked)
float settle(MoveWorld &mw, Object *horde, float tol, int maxFrames)
{
	HordeContain *hc = mw.hordeOf(horde);
	int f = 0;
	for (; f < maxFrames; ++f)
	{
		mw.frames(1);
		HordeContainCore::Placement pl;
		pl.position = *horde->getPosition();
		pl.angle = horde->getOrientation();
		pl.computeTrig();
		float worst = 0.0f;
		for (const Object *m : mw.membersOf(horde))
		{
			float a = 0.0f;
			const Coord3D slot = const_cast<HordeContainCore &>(hc->core()).getSlotWorldPos(m->getID(), pl, &a);
			const float e = dist2d(slot, *m->getPosition());
			if (e > worst)
			{
				worst = e;
			}
		}
		if (mw.aiOf(horde)->isIdle() && worst <= tol)
		{
			break;
		}
	}
	for (int i = 0; i < 5; ++i)
	{
		mw.frames(1);
	}
	// the final error, measured after the last frame
	HordeContainCore::Placement pl;
	pl.position = *horde->getPosition();
	pl.angle = horde->getOrientation();
	pl.computeTrig();
	float worst = 0.0f;
	for (const Object *m : mw.membersOf(horde))
	{
		float a = 0.0f;
		const Coord3D slot = const_cast<HordeContainCore &>(hc->core()).getSlotWorldPos(m->getID(), pl, &a);
		const float e = dist2d(slot, *m->getPosition());
		if (e > worst)
		{
			worst = e;
		}
	}
	return worst;
}

std::vector<float> orientationsOf(MoveWorld &mw, Object *horde)
{
	std::vector<float> out;
	for (const Object *m : mw.membersOf(horde))
	{
		out.push_back(m->getOrientation());
	}
	return out;
}
} // namespace

TEST_CASE("PORT-427 a porcupine horde's members face outward, each its own radial bearing, not the horde's angle (RotWK 2.01 RW 0x873FE8 -> RW 0x8740C1)")
{
	Port427World w;
	Object *h = w.spawn("P427PorcupineHorde", 505.0f, 505.0f, 0.0f);
	REQUIRE(w.membersOf(h).size() == 8);
	// tolerance: the members spawn on their slots; 1.0 world units is generous, the walk is zero-length
	REQUIRE(settle(w, h, 1.0f, 300) < 1.0f);
	HordeContain *hc = w.hordeOf(h);
	REQUIRE(hc->hordeData().m_isPorcupineFormation);
	const Coord3D ownerPos = *h->getPosition();
	int distinct = 0;
	{
		const std::vector<float> f = orientationsOf(w, h);
		distinct = (int)std::set<float>(f.begin(), f.end()).size();
	}
	CHECK(distinct == 8); // before the fix: every member faced the horde's angle (P-110)
	for (const Object *m : w.membersOf(h))
	{
		float slotAngle = 0.0f;
		const Coord3D slot = w.slotWorld(hc, h, m, slotAngle);
		const float expected = radialBearingFromBinary(slot, ownerPos);
		// tolerance: the AIGOAL_ANGLE path copies the goal onto the object; only the deterministic-vs-CRT ACos parity (S-167) can differ
		CHECK(angleDiff(m->getOrientation(), expected) < 1e-4f);
	}
}

TEST_CASE("PORT-427 the same ring as a line formation (no IsPorcupineFormation) keeps every member on the horde's facing (RW 0x8740C1 not taken; the reported P-110 contrast)")
{
	Port427World w;
	Object *h = w.spawn("P427LineHorde", 505.0f, 505.0f, 0.0f);
	REQUIRE(w.membersOf(h).size() == 8);
	// tolerance: the members spawn on their slots; 1.0 world units is generous, the walk is zero-length
	REQUIRE(settle(w, h, 1.0f, 300) < 1.0f);
	HordeContain *hc = w.hordeOf(h);
	REQUIRE_FALSE(hc->hordeData().m_isPorcupineFormation);
	for (const Object *m : w.membersOf(h))
	{
		// tolerance: the 10 degree no-turn threshold (RW 0xC5B69C); a member that walked keeps its heading, it is not snapped
		CHECK(angleDiff(m->getOrientation(), h->getOrientation()) < 0.2f);
	}
}

TEST_CASE("PORT-427 a rotated porcupine horde: the facing follows the rotated world slot position (RW 0x8740C4 .. RW 0x8740E3), not the formation frame")
{
	Port427World w;
	const float hordeAngle = 1.0f;
	Object *h = w.spawn("P427PorcupineHorde", 505.0f, 505.0f, hordeAngle);
	REQUIRE(w.membersOf(h).size() == 8);
	// tolerance: the members spawn on their slots; 1.0 world units is generous, the walk is zero-length
	REQUIRE(settle(w, h, 1.0f, 300) < 1.0f);
	HordeContain *hc = w.hordeOf(h);
	const Coord3D ownerPos = *h->getPosition();
	// tolerance: the spawn's orientation is stored verbatim
	REQUIRE(h->getOrientation() == doctest::Approx(hordeAngle));
	for (const Object *m : w.membersOf(h))
	{
		float slotAngle = 0.0f;
		const Coord3D slot = w.slotWorld(hc, h, m, slotAngle);
		const float expected = radialBearingFromBinary(slot, ownerPos);
		// tolerance: the AIGOAL_ANGLE path copies the goal onto the object; only the deterministic-vs-CRT ACos parity (S-167) can differ
		CHECK(angleDiff(m->getOrientation(), expected) < 1e-4f);
	}
}

TEST_CASE("PORT-427 a member whose slot coincides exactly with the horde's position keeps the horde's own orientation (RW 0x8740FA .. 0x874117: [owner + 0x44])")
{
	Port427World w;
	const float hordeAngle = 1.0f;
	Object *h = w.spawn("P427CenterHorde", 505.0f, 505.0f, hordeAngle);
	REQUIRE(w.membersOf(h).size() == 3);
	// tolerance: the members spawn on their slots; 1.0 world units is generous, the walk is zero-length
	REQUIRE(settle(w, h, 1.0f, 300) < 1.0f);
	HordeContain *hc = w.hordeOf(h);
	const Coord3D ownerPos = *h->getPosition();
	bool centerChecked = false;
	for (const Object *m : w.membersOf(h))
	{
		float slotAngle = 0.0f;
		const Coord3D slot = w.slotWorld(hc, h, m, slotAngle);
		const float dx = slot.x - ownerPos.x, dy = slot.y - ownerPos.y;
		if (dx * dx + dy * dy == 0.0f)
		{
			centerChecked = true;
			// tolerance: the zero-vector branch copies the horde's orientation verbatim; only float noise can differ
			CHECK(angleDiff(m->getOrientation(), h->getOrientation()) < 1e-4f);
			// tolerance: the spawn's orientation is stored verbatim
			CHECK(h->getOrientation() == doctest::Approx(hordeAngle));
		}
		else
		{
			const float expected = radialBearingFromBinary(slot, ownerPos);
			// tolerance: the AIGOAL_ANGLE path copies the goal onto the object; only the deterministic-vs-CRT ACos parity (S-167) can differ
			CHECK(angleDiff(m->getOrientation(), expected) < 1e-4f);
		}
	}
	CHECK(centerChecked);
}
