// OpenBFME. PORT-75 tests (issue #207): BannerCarrierUpdate's MorphCondition (RW 0x89A90F) and BannerMorphFX (RW 0xC6600C, data + 0x30) — a banner
// carrier that joins a horde takes the look of the horde it leads.
//
// Sources: the morph step RW 0x89A5C5 and its caller (the contain's accept path) are ported from the byte-matched Open-BFME-2 C++
// (DONOR BFME2 0x00496E05 / 0x0049714F / 0x00473212 -> 0x00473197); the RW addresses of the TU follow from the constant shift 0x4037C0 that the six
// already-read RW addresses of this TU pin (docs/ennorath/port75-analysis.md). The expected model states come from the INI lines the fixtures write,
// i.e. from data, and the clear list from RW's own name table (RW 0xD9FAD8) plus DONOR .rdata 0x00C4F6E0.
#include "doctest.h"

#include <stdexcept>
#include <string>
#include "CombatTestUtil.h"
#include "Horde2TestUtil.h"

#include "Common/ModelState.h"
#include "GameLogic/FXEvents.h"
#include "GameLogic/Module/BannerCarrierUpdate.h"
#include "GameLogic/Object/ExperienceTracker.h"

using namespace combattest;
using namespace horde2test;

namespace
{
HordeContain *hc(Object *h)
{
	return dynamic_cast<HordeContain *>(h->getContain());
}

// the HORDE-2 fixture with the raw MorphCondition line replaced by two parsed ones: type A ("Swordsman", the payload, no locomotor) and type B
// ("Pikeman", SET_NORMAL, and the +NAME form of a model condition list), plus BannerMorphFX and an ExpLevelDraw line that stays a raw token
std::string morphObjects()
{
	std::string s(kBannerObjects);
	const std::string line = "    MorphCondition = UnitType:Swordsman ModelState:\"USER_2\"\n";
	const size_t at = s.find(line);
	REQUIRE(at != std::string::npos);
	s.replace(at, line.size(),
		"    MorphCondition = UnitType:Swordsman ModelState:\"USER_2\"\n"
		"    MorphCondition = UnitType:Pikeman Locomotor:SET_NORMAL ModelState:\"+USER_7\"\n"
		"    BannerMorphFX = FXBannerMorph\n"
		"    ExpLevelDraw = Level:2\n");
	return s;
}

// the bit of a model-condition name in RW's table (RW 0xD9FAD8); an unknown name is an error of the test, not a bound
int bit(const char *name)
{
	const int index = ModelCondition::indexOf(name);
	if (index < 0)
	{
		throw std::logic_error(std::string("the model condition name is not in RW 0xD9FAD8: ") + name);
	}
	return index;
}

// does the carrier test positive for ANY of the 81 morph bits (DONOR .rdata 0x00C4F6E0)?
bool anyMorphBit(const Object &o)
{
	static const int kMorphBits[] = {214, 194, 195, 196, 197, 198, 184, 185, 186, 187, 188};
	for (int b : kMorphBits)
	{
		if (o.testModelCondition(b))
		{
			return true;
		}
	}
	for (int b = 458; b <= 527; ++b)
	{
		if (o.testModelCondition(b))
		{
			return true;
		}
	}
	return false;
}

struct Spawned
{
	Object *carrier = nullptr;
	std::vector<FXEvent> morphEvents;
};

// rank 1 makes the horde spawn its carrier (RW 0x8719E4 -> 0x870204); the frames are stepped one by one so the morph's FX event is caught in the
// frame it is emitted
Spawned spawnCarrier(CombatWorld &w, Object *horde)
{
	Spawned out;
	ExperienceTrackerTestAccess::setRank(*horde->getExperienceTracker(), 1);
	for (int i = 0; i < 4 && !out.carrier; ++i)
	{
		w.frames(1);
		for (const FXEvent &e : w.logic->fxEvents().frameEvents())
		{
			if (e.kind == FXEvent::OBJECT_FX && e.fxList && *e.fxList == "FXBannerMorph")
			{
				out.morphEvents.push_back(e);
			}
		}
		if (hc(horde)->bannerCarrier() != 0)
		{
			out.carrier = w.byId(hc(horde)->bannerCarrier());
		}
	}
	return out;
}

} // namespace

TEST_CASE("PORT-75 parse: MorphCondition is UnitType + optional Locomotor + a quoted ModelState (RW 0x89A90F)")
{
	const std::string objects = morphObjects();
	CombatWorld w(objects.c_str());
	w.combat().setAutoAcquireEnabled(false);
	Object *horde = w.unit("BannerHorde", 'A', 300, 300);
	w.frames(10);
	REQUIRE(hc(horde) != nullptr);
	Spawned spawned = spawnCarrier(w, horde);
	REQUIRE(spawned.carrier != nullptr);
	BannerCarrierUpdate *update = dynamic_cast<BannerCarrierUpdate *>(spawned.carrier->findModule("BannerCarrierUpdate"));
	REQUIRE(update != nullptr);
	const BannerCarrierUpdateModuleData &data = update->data();
	REQUIRE(data.m_morphConditions.size() == 2);
	// type A: UnitType and ModelState only (DONOR 0x0049714F's mandatory pair)
	CHECK(data.m_morphConditions[0].unitType == "Swordsman");
	CHECK(data.m_morphConditions[0].locomotor.empty());
	CHECK(data.m_morphConditions[0].modelState.test(bit("USER_2")));
	CHECK(data.m_morphConditions[0].modelState.count() == 1);
	// type B: Locomotor between them, and the +NAME form of the list
	CHECK(data.m_morphConditions[1].unitType == "Pikeman");
	CHECK(data.m_morphConditions[1].locomotor == "SET_NORMAL");
	CHECK(data.m_morphConditions[1].modelState.test(bit("USER_7")));
	CHECK(data.m_morphConditions[1].modelState.count() == 1);
	CHECK(data.m_bannerMorphFX == "FXBannerMorph");
	CHECK(data.m_expLevelDraws.size() == 1); // ExpLevelDraw is still a raw token (stop S-587)

	// the 81 morph bits of DONOR .rdata 0x00C4F6E0 as RW's own name table (RW 0xD9FAD8) spells them: MOUNTED, PASSENGER_VARIATION_1..5, USER_1..75.
	// If the generated name table ever moves, the clear list would stop matching retail, so it is pinned here.
	CHECK(std::string(ModelCondition::nameOf(214)) == "MOUNTED");
	CHECK(std::string(ModelCondition::nameOf(194)) == "PASSENGER_VARIATION_1");
	CHECK(std::string(ModelCondition::nameOf(198)) == "PASSENGER_VARIATION_5");
	CHECK(std::string(ModelCondition::nameOf(184)) == "USER_1");
	CHECK(std::string(ModelCondition::nameOf(188)) == "USER_5");
	CHECK(std::string(ModelCondition::nameOf(458)) == "USER_6");
	CHECK(std::string(ModelCondition::nameOf(527)) == "USER_75");
	// and the names the fixtures use sit on the numbers the clear list holds: USER_2 (type A) and USER_7 (type B)
	CHECK(bit("USER_2") == 185);
	CHECK(bit("USER_7") == 459);
}

TEST_CASE("PORT-75 morph: a banner carrier that joins a Swordsman horde takes USER_2 and plays BannerMorphFX (RW 0x89A5C5)")
{
	const std::string objects = morphObjects();
	CombatWorld w(objects.c_str());
	w.combat().setAutoAcquireEnabled(false);
	Object *horde = w.unit("BannerHorde", 'A', 300, 300);
	w.frames(10);
	REQUIRE(hc(horde) != nullptr);
	Spawned spawned = spawnCarrier(w, horde);
	REQUIRE(spawned.carrier != nullptr);
	// the join (RW 0x870204 -> the contain's accept path) morphs the carrier to the horde's member template, "Swordsman"
	CHECK(spawned.carrier->testModelCondition(bit("USER_2")));
	CHECK_FALSE(spawned.carrier->testModelCondition(bit("USER_7")));
	REQUIRE(spawned.morphEvents.size() == 1);
	CHECK(spawned.morphEvents[0].primary == spawned.carrier->getID());
}

TEST_CASE("PORT-75 morph: type B replaces type A's state, and a type with no line clears every morph bit (DONOR 0x00496D43's all-zero miss)")
{
	const std::string objects = morphObjects();
	CombatWorld w(objects.c_str());
	w.combat().setAutoAcquireEnabled(false);
	Object *horde = w.unit("BannerHorde", 'A', 300, 300);
	w.frames(10);
	REQUIRE(hc(horde) != nullptr);
	Spawned spawned = spawnCarrier(w, horde);
	REQUIRE(spawned.carrier != nullptr);
	BannerCarrierUpdate *update = dynamic_cast<BannerCarrierUpdate *>(spawned.carrier->findModule("BannerCarrierUpdate"));
	REQUIRE(update != nullptr);
	Object &carrier = *spawned.carrier;
	REQUIRE(carrier.testModelCondition(bit("USER_2")));

	// a look bit that is NOT one of the 81 morph bits survives the morph
	const int keep = bit("ATTACKING");
	Object::ModelConditionBits none{}, keepBits{};
	keepBits[(size_t)keep >> 5] |= 1u << (keep & 31);
	carrier.clearAndSetModelConditionFlags(none, keepBits);

	update->morphForTemplate("Pikeman", false);
	CHECK(carrier.testModelCondition(bit("USER_7")));
	CHECK_FALSE(carrier.testModelCondition(bit("USER_2"))); // A's look is cleared, not stacked
	CHECK_FALSE(carrier.testModelCondition(bit("USER_1")));
	CHECK(carrier.testModelCondition(keep));

	// a template no line names: retail fills the set with zeros and still clears the morph list (DONOR 0x00496D43 -> Rva0028F59A(0, 0xb8))
	update->morphForTemplate("NoSuchHorde", false);
	CHECK_FALSE(anyMorphBit(carrier));
	CHECK(carrier.testModelCondition(keep));

	// and back to A
	update->morphForTemplate("Swordsman", false);
	CHECK(carrier.testModelCondition(bit("USER_2")));
	CHECK_FALSE(carrier.testModelCondition(bit("USER_7")));
	CHECK(carrier.testModelCondition(keep));
}

TEST_CASE("PORT-75 keep: a horde without BannerMorphFX plays no effect, and a line that names no type leaves no morph bit (DONOR 0x00496D43's all-zero miss)")
{
	const std::string objects = kBannerObjects; // the HORDE-2 fixture: one line, UnitType:Swordsman -> USER_2, no BannerMorphFX
	CombatWorld w(objects.c_str());
	w.combat().setAutoAcquireEnabled(false);
	Object *horde = w.unit("BannerHorde", 'A', 300, 300);
	w.frames(10);
	REQUIRE(hc(horde) != nullptr);
	Spawned spawned = spawnCarrier(w, horde);
	REQUIRE(spawned.carrier != nullptr);
	BannerCarrierUpdate *update = dynamic_cast<BannerCarrierUpdate *>(spawned.carrier->findModule("BannerCarrierUpdate"));
	REQUIRE(update != nullptr);
	CHECK(update->data().m_bannerMorphFX.empty());
	CHECK(spawned.morphEvents.empty()); // RW 0x89A5C5 asks for the FX; an empty name is not one (RW 0x73A302's None rule)
	CHECK(spawned.carrier->testModelCondition(bit("USER_2"))); // the join still morphed it to the horde's Swordsmen

	// the fixture has no lane for "Pikeman": retail fills the set with zeros and clears the morph bits all the same
	update->morphForTemplate("Pikeman", true);
	CHECK_FALSE(anyMorphBit(*spawned.carrier));
}
