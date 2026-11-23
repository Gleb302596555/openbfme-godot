// OpenBFME. GPL-3.0.
//
// LOCAL-136 (fork issue 853): shared RotWK 2.01 helpers (both BFME2 1.06 and RotWK 2.01):
//   * SpecialPowerTemplate::getName (RW 0x00896AA7 / DONOR BFME2 0x00493313): walks Overridable::getFinalOverride() (RW 0x688D3C) before reading m_name (+0x5C).
//   * Object::getPlanarDirectionTo (RW 0x00665342 -> RW 0x006635F4 / DONOR BFME2 Object::getPlanarDirectionTo): returns (other.x - this.x, other.y - this.y, 0.0f) in float32 (subss).

#include "Common/SpecialPower.h"
#include "GameLogic/Object/Object.h"
#include "LogicTestUtil.h"
#include "doctest.h"

// Expectations from RotWK 2.01 RW 0x00896AA7 (call 0x688d3c getFinalOverride; add eax, 0x5c m_name).
TEST_CASE("LOCAL-136: SpecialPowerTemplate::getName follows final override (RW 0x00896AA7)")
{
	SpecialPowerTemplate base;
	base.m_name = "BaseSpecialPower";

	SpecialPowerTemplate ov1;
	ov1.m_name = "OverrideOne";
	ov1.m_isOverride = true;

	SpecialPowerTemplate ov2;
	ov2.m_name = "OverrideFinal";
	ov2.m_isOverride = true;

	CHECK(base.getName() == "BaseSpecialPower");

	base.m_nextOverride = &ov1;
	CHECK(base.getName() == "OverrideOne");

	ov1.m_nextOverride = &ov2;
	CHECK(base.getName() == "OverrideFinal");
	CHECK(ov1.getName() == "OverrideFinal");
	CHECK(ov2.getName() == "OverrideFinal");
}

// Neighbor behavior: RW 0x008DD9F4 / RW 0x008DD9FD voice getters on SpecialPowerTemplate also follow getFinalOverride().
TEST_CASE("LOCAL-136: SpecialPowerTemplate voice getters follow final override (RW 0x008DD9F4)")
{
	SpecialPowerTemplate base;
	base.m_unitSpecificInitiateVoice = "BaseInitVoice";
	base.m_unitSpecificEnterStateVoice = "BaseEnterVoice";

	SpecialPowerTemplate ov;
	ov.m_isOverride = true;
	ov.m_unitSpecificInitiateVoice = "OverrideInitVoice";
	ov.m_unitSpecificEnterStateVoice = "OverrideEnterVoice";
	base.m_nextOverride = &ov;

	CHECK(base.getUnitSpecificSoundToUseAsInitiateIntendToDoVoice() == "OverrideInitVoice");
	CHECK(base.getUnitSpecificSoundToUseAsEnterStateInitiateIntendToDoVoice() == "OverrideEnterVoice");
}

// Expectations from RotWK 2.01 RW 0x00665342 -> RW 0x006635F4 (subss other.x - this.x, other.y - this.y, z = 0.0f).
TEST_CASE("LOCAL-136: Object::getPlanarDirectionTo computes planar delta with zero z (RW 0x00665342)")
{
	logictest::LogicWorld lw;
	lw.w.load("Object Local136Unit\nEnd\n");

	Object *a = lw.make("Local136Unit");
	Object *b = lw.make("Local136Unit");
	REQUIRE(a != nullptr);
	REQUIRE(b != nullptr);

	const Coord3D posA{ 10.5f, 20.25f, 5.0f };
	const Coord3D posB{ 35.0f, -4.75f, 99.0f };
	a->setPosition(&posA);
	b->setPosition(&posB);

	const Coord3D ab = a->getPlanarDirectionTo(b);
	CHECK(ab.x == 24.5f);
	CHECK(ab.y == -25.0f);
	CHECK(ab.z == 0.0f);

	const Coord3D ba = b->getPlanarDirectionTo(a);
	CHECK(ba.x == -24.5f);
	CHECK(ba.y == 25.0f);
	CHECK(ba.z == 0.0f);

	const Coord3D self = a->getPlanarDirectionTo(a);
	CHECK(self.x == 0.0f);
	CHECK(self.y == 0.0f);
	CHECK(self.z == 0.0f);

	const Coord3D nullDir = a->getPlanarDirectionTo(nullptr);
	CHECK(nullDir.x == 0.0f);
	CHECK(nullDir.y == 0.0f);
	CHECK(nullDir.z == 0.0f);
}
