// OpenBFME. PORT-25 (issue #97) tests: HordeContain's VisionRearOverride / VisionSideOverride — the horde's rear / side vision factors.
//
// Every expectation below is read off RotWK 2.01 (game.dat md5 daed3668, caveat S-001) with tools/mod/rw.py, never off the ported code:
//
//   * the field rows: VisionRearOverride is the HordeContain table row RW 0xC5BDC0 (offset 0x274) and VisionSideOverride RW 0xC5BDB0
//     (offset 0x278); both parse with RW 0x42EEFA (`parsePercentToReal`, the float32 of the percent). The module data constructor RW 0x878EE5
//     writes 0.0 to both (RW 0x8790C0 / RW 0x8790C8: `movss [esi+0x274], xmm0` / `[esi+0x278]` with xmm0 = 0 from RW 0x878F6C `xorps xmm0,xmm0`).
//   * the getters: HordeContainInterface vslots 0x258 / 0x25C, RW 0x872B9E (`mov eax, [ecx-0x118]; fld dword ptr [eax+0x274]`) and RW 0x872BAB
//     (the same read of + 0x278). The tables that carry them are the horde's contain tables RW 0xC5BE98 / 0xC5CFA8 (+0x258) and RW 0xC5BE9C /
//     RW 0xC5CFAC (+0x25C); RW 0x8798A1 / 0x8798FF install them at module + 0x11C, which is the -0x118 displacement.
//   * the one reader is the object's look, RW 0x694A45 (the port's ShroudManager::retailObjectSource()):
//       RW 0x694B6A  call RW 0x68C866           the contain's horde interface (Object + 0x258 slot 0x7C), 0 when there is none
//       RW 0x694B75  call [esi+0x25c]           side override  -> stored to [ebp+0x14]
//       RW 0x694B82  fldz; fld [ebp+0x14]; fcompi st(1); jbe 0x694B97     (override > 0 -> RW 0x694B8D takes it as the side factor)
//       RW 0x694B97  call [esi+0x258]           rear override  -> stored to [ebp+0x14]
//       RW 0x694BB2  ja 0x694BB9                (override > 0 keeps it as the rear factor, else RW 0x694BB4 = template + 0x4C8)
//       RW 0x694BBD / 0x694BCA mulss            side / rear = range * factor, SSE binary32
//     The base factors are the final template's VisionSide (template + 0x4C4, RW 0x694B4E) and VisionRear (+ 0x4C8, RW 0x694B5D); an override
//     of exactly 0 (an omitted INI row) therefore keeps the template's factor.
//
// The synthetic objects use the Ennorath shape: AngmarDunedainPikemanHorde (ShroudClearingRange 325, VisionSide 50%, VisionRear 25%) and its
// alternate formation AngmarDunedainPikemanHordePorcupine, whose HordeContain block sets VisionRearOverride = 100% and VisionSideOverride = 100%
// (all 55 Ennorath definitions of both rows are 100%, see the PR note). No game or mod file is read here.
//
// The fixture binds the ported runtimes the way RetailObjectWorld does (MapHordeSpawn::bindHordeContainData + LogicModules::registerAll)
// BEFORE its templates are parsed: without that the HordeContain class is an UnportedModule and the object has no contain at all.
//
// TEST_CASE names start with PORT-25 (the run filter -tc="PORT-25*").
#include "doctest.h"
#include "LogicTestUtil.h"

#include "Common/Player.h"
#include "Common/Thing/ThingTemplate.h"
#include "GameClient/MapHordeSpawn.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Module/LogicModules.h"
#include "GameLogic/Object/Contain/HordeContainRuntime.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/SimMath.h"
#include "GameLogic/System/ShroudManager.h"

#include <cmath>

using namespace logictest;

namespace
{
// Cell size 40 as GameData PartitionCellSize, the extent large enough that no look leaves it.
const float kCellSize = 40.0f;

const char kPort25Objects[] =
	// the alternate formation: the horde's contain REPLACES both factors with 100%
	"Object P25PikeHordePorcupine\n"
	"  KindOf = INFANTRY SELECTABLE HORDE MELEE_HORDE\n"
	"  ShroudClearingRange = 325\n"
	"  VisionRange = 200\n"
	"  VisionSide = 50%\n"
	"  VisionRear = 25%\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 5\n"
	"  GeometryMinorRadius = 5\n"
	"  GeometryHeight = 20\n"
	"  Behavior = HordeContain ModuleTag_HordeContain\n"
	"    Slots = 10\n"
	"    VisionRearOverride = 100%\n"
	"    VisionSideOverride = 100%\n"
	"  End\n"
	"End\n"
	// the main formation: no override rows, so the module data keeps the constructor's 0.0 and the template's factors stand
	"Object P25PikeHorde\n"
	"  KindOf = INFANTRY SELECTABLE HORDE MELEE_HORDE\n"
	"  ShroudClearingRange = 325\n"
	"  VisionRange = 200\n"
	"  VisionSide = 50%\n"
	"  VisionRear = 25%\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 5\n"
	"  GeometryMinorRadius = 5\n"
	"  GeometryHeight = 20\n"
	"  Behavior = HordeContain ModuleTag_HordeContain\n"
	"    Slots = 10\n"
	"  End\n"
	"End\n"
	// a rear override alone: the side keeps the template's 50%, only the rear changes
	"Object P25RearOnly\n"
	"  KindOf = INFANTRY SELECTABLE HORDE MELEE_HORDE\n"
	"  ShroudClearingRange = 200\n"
	"  VisionSide = 50%\n"
	"  VisionRear = 25%\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 5\n"
	"  GeometryMinorRadius = 5\n"
	"  GeometryHeight = 20\n"
	"  Behavior = HordeContain ModuleTag_HordeContain\n"
	"    Slots = 10\n"
	"    VisionRearOverride = 80%\n"
	"  End\n"
	"End\n"
	// a horde contain whose override rows are explicitly 0: RW's `> 0` test keeps the template's factors
	"Object P25ZeroOverride\n"
	"  KindOf = INFANTRY SELECTABLE HORDE MELEE_HORDE\n"
	"  ShroudClearingRange = 200\n"
	"  VisionSide = 50%\n"
	"  VisionRear = 25%\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 5\n"
	"  GeometryMinorRadius = 5\n"
	"  GeometryHeight = 20\n"
	"  Behavior = HordeContain ModuleTag_HordeContain\n"
	"    Slots = 10\n"
	"    VisionRearOverride = 0%\n"
	"    VisionSideOverride = 0%\n"
	"  End\n"
	"End\n"
	// the neighbour: a plain object with no contain at all (RW 0x694B71 `test esi, esi; je 0x694BB4` takes the template's factors)
	"Object P25Scout\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  ShroudClearingRange = 325\n"
	"  VisionSide = 50%\n"
	"  VisionRear = 25%\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 5\n"
	"  GeometryHeight = 20\n"
	"End\n"
	// the neighbour: a contain that is not a horde (no horde interface: RW 0x68C866 answers 0), with its own overrides spelled out so that a
	// second, wrong reader would show
	"Object P25Tower\n"
	"  KindOf = STRUCTURE IMMOBILE SELECTABLE\n"
	"  ShroudClearingRange = 200\n"
	"  VisionSide = 50%\n"
	"  VisionRear = 25%\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 5\n"
	"  GeometryHeight = 20\n"
	"  Behavior = GarrisonContain ModuleTag_Contain\n"
	"    ContainMax = 5\n"
	"  End\n"
	"End\n";

struct Port25World : LogicWorld
{
	ShroudManager shroud;
	Port25World()
		: LogicWorld(RandomAlgorithm::ZH_CarryChain)
		, shroud(*logic)
	{
		// the ported contain runtimes, bound as RetailObjectWorld binds them (LogicModules::registerAll) BEFORE the templates are parsed: the
		// HordeContain module data is typed only then (MapHordeSpawn::bindHordeContainData) and the class is the runtime, not an UnportedModule
		MapHordeSpawn::bindHordeContainData(w.modules);
		LogicModules::registerAll(w.modules);
		const std::string err = w.load(kPort25Objects);
		REQUIRE_MESSAGE(err.empty(), err);
		shroud.init(kCellSize, 1);
		shroud.setExtent(0.0f, 0.0f, 4000.0f, 3200.0f);
		shroud.attach();
	}
	// the look of one object as RW 0x694A45 computes it, through the port's retail source
	ShroudManager::LookInputs lookOf(Object &obj)
	{
		ShroudManager::LookInputs in;
		ShroudManager::retailObjectSource().look(obj, ShroudManager::templateVision(*obj.getTemplate()->getFinalOverride()), in);
		return in;
	}
	Object *place(const char *tmpl, float x, float y)
	{
		Object *o = make(tmpl, teamOf("Alice"));
		Coord3D p{ x, y, 0.0f };
		o->setPosition(&p);
		return o;
	}
};

// RW 0x42EEFA parsePercentToReal -> RW 0x42EB18 `call 0x42EAAD; fmul dword ptr [0xBE5600]` (0.01f): the stored float is the x87 24-bit
// product of the number and 0.01f, NOT the decimal literal (80% is 0.79999998, not 0.80000001)
float percent(float number)
{
	return SimMath::pc24Mul(number, 0.01f);
}
} // namespace

TEST_CASE("PORT-25 a horde's VisionSideOverride / VisionRearOverride of 100% replace the template's 50% / 25% factors (RW 0x694B79 .. 0x694B8B, 0x694B9B .. 0x694BB2)")
{
	Port25World w;
	// the horde's range is its ShroudClearingRange (325, no attribute modifier, no height bonus): RW 0x68E4E2
	const float range = 325.0f;
	// static_cast<float> of the percent literals (RW 0x42EEFA's parse: 100% -> 1.0f, 50% -> 0.5f, 25% -> 0.25f)
	const float hundred = percent(100.0f), fifty = percent(50.0f), quarter = percent(25.0f);

	Object *plain = w.place("P25PikeHorde", 1000.0f, 1000.0f);
	Object *over = w.place("P25PikeHordePorcupine", 1000.0f, 1600.0f);
	REQUIRE(plain->getContain() != nullptr);
	REQUIRE(over->getContain() != nullptr);
	REQUIRE(plain->getContain()->getHordeContainInterface() != nullptr);
	REQUIRE(over->getContain()->getHordeContainInterface() != nullptr);

	// the getters read module data + 0x274 / + 0x278 (RW 0x872B9E / 0x872BAB) as parsed
	CHECK(plain->getContain()->getHordeContainInterface()->getVisionRearOverride() == 0.0f);
	CHECK(plain->getContain()->getHordeContainInterface()->getVisionSideOverride() == 0.0f);
	CHECK(over->getContain()->getHordeContainInterface()->getVisionRearOverride() == hundred);
	CHECK(over->getContain()->getHordeContainInterface()->getVisionSideOverride() == hundred);

	const ShroudManager::LookInputs a = w.lookOf(*plain);
	CHECK(a.forward == range);
	CHECK(a.side == range * fifty);   // no override row -> the template's VisionSide (RW 0x694B4E)
	CHECK(a.rear == range * quarter); // no override row -> the template's VisionRear (RW 0x694B5D)

	// the overrides above 0 REPLACE both factors (RW 0x694B8D / 0x694BA4), so the round look comes back
	const ShroudManager::LookInputs b = w.lookOf(*over);
	CHECK(b.forward == range);
	CHECK(b.side == range * hundred);
	CHECK(b.rear == range * hundred);
}

TEST_CASE("PORT-25 the rear override is tested on its own: 80% replaces the rear factor and leaves the side at the template's 50% (RW 0x694B8B vs 0x694BB2)")
{
	Port25World w;
	const float range = 200.0f;
	Object *o = w.place("P25RearOnly", 1000.0f, 1000.0f);
	REQUIRE(o->getContain() != nullptr);
	REQUIRE(o->getContain()->getHordeContainInterface() != nullptr);
	CHECK(o->getContain()->getHordeContainInterface()->getVisionRearOverride() == percent(80.0f));
	CHECK(o->getContain()->getHordeContainInterface()->getVisionSideOverride() == 0.0f);

	const ShroudManager::LookInputs in = w.lookOf(*o);
	CHECK(in.forward == range);
	CHECK(in.side == range * percent(50.0f));  // VisionSide, no side override
	CHECK(in.rear == range * percent(80.0f));  // VisionRearOverride
}

TEST_CASE("PORT-25 an override of exactly 0 keeps the template's factor (RW 0x694B82 fldz/fcompi: the branch needs the value strictly above 0)")
{
	Port25World w;
	const float range = 200.0f;
	Object *o = w.place("P25ZeroOverride", 1000.0f, 1000.0f);
	REQUIRE(o->getContain() != nullptr);
	REQUIRE(o->getContain()->getHordeContainInterface() != nullptr);
	CHECK(o->getContain()->getHordeContainInterface()->getVisionRearOverride() == 0.0f);
	CHECK(o->getContain()->getHordeContainInterface()->getVisionSideOverride() == 0.0f);

	const ShroudManager::LookInputs in = w.lookOf(*o);
	CHECK(in.side == range * percent(50.0f));
	CHECK(in.rear == range * percent(25.0f));
}

TEST_CASE("PORT-25 an object without a contain, or with a contain that is not a horde, keeps the template's factors (RW 0x694B71, RW 0x68C866)")
{
	Port25World w;
	const float range = 325.0f;
	Object *scout = w.place("P25Scout", 1000.0f, 1000.0f);
	CHECK(scout->getContain() == nullptr);
	const ShroudManager::LookInputs a = w.lookOf(*scout);
	CHECK(a.forward == range);
	CHECK(a.side == range * percent(50.0f));
	CHECK(a.rear == range * percent(25.0f));

	Object *tower = w.place("P25Tower", 1400.0f, 1000.0f);
	REQUIRE(tower->getContain() != nullptr);
	CHECK(tower->getContain()->getHordeContainInterface() == nullptr); // RW 0x68C866 answers 0 for a garrison
	const ShroudManager::LookInputs b = w.lookOf(*tower);
	CHECK(b.forward == 200.0f);
	CHECK(b.side == 200.0f * percent(50.0f));
	CHECK(b.rear == 200.0f * percent(25.0f));
}

TEST_CASE("PORT-25 the override reaches the shroud grid: the horde's rear look behind it is the full radius, the plain horde's is a quarter (RW 0x694BCA; expected cells from the look radii of RW 0x694BB4 / 0x694BBD)")
{
	Port25World w;
	const int alice = w.players.findPlayerWithName("Alice")->getPlayerIndex();
	// facing +x (angle 0). 325 / 40 -> 9 cells forward, side and rear; without the override the rear is ceil(325 * 0.25 / 40) = 3 cells.
	Object *plain = w.place("P25PikeHorde", 1000.0f, 800.0f);      // cell (25, 20)
	Object *over = w.place("P25PikeHordePorcupine", 1000.0f, 1600.0f); // cell (25, 40)
	plain->setOrientation(0.0f);
	over->setOrientation(0.0f);
	w.shroud.update();
	REQUIRE(w.shroud.isRegistered(*plain));
	REQUIRE(w.shroud.isRegistered(*over));
	CHECK(w.shroud.lookRadius(*plain) == 9);
	CHECK(w.shroud.lookRadius(*over) == 9);
	// the look cell is the horde's own cell (25, 20) / (25, 40) and the facing is +x, so the facing axis behind it runs along x.
	// behind the plain horde the rear radius is ceil(325 * 0.25 / 40) = 3 cells: (25 - 3) is clear and (25 - 4) was never looked at
	CHECK(w.shroud.getCellStatus(alice, 22, 20) == CELLSHROUD_CLEAR);
	CHECK(w.shroud.getCellStatus(alice, 21, 20) == CELLSHROUD_SHROUDED);
	// behind the overridden horde the rear radius is the full ceil(325 / 40) = 9 cells: (25 - 8) is clear and (25 - 10) was never looked at
	CHECK(w.shroud.getCellStatus(alice, 17, 40) == CELLSHROUD_CLEAR);
	CHECK(w.shroud.getCellStatus(alice, 15, 40) == CELLSHROUD_SHROUDED);
}

TEST_CASE("PORT-25 the module data keeps the constructor's 0.0 for an omitted row and the parsed float for a set one (RW 0x8790C0 / 0x8790C8, RW 0x42EEFA)")
{
	Port25World w;
	Object *plain = w.place("P25PikeHorde", 1000.0f, 1000.0f);
	HordeContain *hc = dynamic_cast<HordeContain *>(plain->getContain());
	REQUIRE(hc != nullptr);
	CHECK(hc->hordeData().m_visionRearOverride == 0.0f);
	CHECK(hc->hordeData().m_visionSideOverride == 0.0f);

	Object *over = w.place("P25PikeHordePorcupine", 1000.0f, 1000.0f);
	HordeContain *ho = dynamic_cast<HordeContain *>(over->getContain());
	REQUIRE(ho != nullptr);
	CHECK(ho->hordeData().m_visionRearOverride == 1.0f);
	CHECK(ho->hordeData().m_visionSideOverride == 1.0f);
}
