// OpenBFME unit tests: PORT-228 (fork issue 551) - the Locomotor fields RotWK 2.01 stores, copies and never reads:
// MaxThrustAngle, RudderCorrectionDegree, RudderCorrectionRate, SwoopStandoffRadius, SwoopStandoffHeight,
// SwoopTerminalVelocity, SwoopAccelerationRate.
// GPL-3.0.
//
// Reference: game.dat 2.01, md5 daed3668006cd90f01c34e5a7da1901f, read with tools/mod/rw.py.
//
//   Parse table RW 0xBF4478, 16-byte entries {name, ParseFn, defaultSource, offset}:
//     RW 0xBF45D8 MaxThrustAngle          + 0x6C  (angle parser RW 0x42EE15)
//     RW 0xBF48C8 RudderCorrectionDegree  + 0x10C (real  parser RW 0x42ED00)
//     RW 0xBF48D8 RudderCorrectionRate    + 0x110
//     RW 0xBF4928 SwoopStandoffRadius     + 0x124
//     RW 0xBF4938 SwoopStandoffHeight     + 0x128
//     RW 0xBF4948 SwoopTerminalVelocity   + 0x12C
//     RW 0xBF4958 SwoopAccelerationRate   + 0x130
//
//   Template constructor RW 0x5E4326 (the xorps xmm0,xmm0 at RW 0x5E4362 makes xmm0 zero, RW 0x5E4518 loads 200.0
//   from 0xBE4170, RW 0x5E4530 0.07 from 0xBF4A2C, RW 0x5E4548 0.003 from 0xBF4A28):
//     RW 0x5E4462 [+0x6C] = 0.0    RW 0x5E45B9 [+0x10C] = 0.0   RW 0x5E45C1 [+0x110] = 0.0
//     RW 0x5E4520 [+0x124] = 200.0 RW 0x5E4528 [+0x128] = 200.0 RW 0x5E4540 [+0x12C] = 0.07
//     RW 0x5E45E9 [+0x130] = 0.003
//
//   The template's assignment RW 0x5E320B copies the whole struct, these members included:
//     RW 0x5E327D/0x5E3280 (+0x6C), RW 0x5E3493/0x5E3499 (+0x10C), RW 0x5E349F/0x5E34A5 (+0x110),
//     RW 0x5E34DB/0x5E34E1 (+0x124), RW 0x5E34E7/0x5E34ED (+0x128), RW 0x5E34F3/0x5E34F9 (+0x12C),
//     RW 0x5E34FF/0x5E3505 (+0x130); the copy constructor RW 0x5E3480 range repeats them at RW 0x5E491E..0x5E4990.
//
//   No other code in the image reads them (stop S-7387): every other access to those displacements belongs to
//   another structure - the flag words RW 0x5E8F8F / 0x5E8F9B (`test/or [x+0x110],0x20000000`), RW 0x5E581C /
//   0x5E583A (`and byte [x+0x12a],0xfe`), the rotation block RW 0x5E5558..0x5E55B7 (+0x68/+0x6C/+0x78/+0x7C moved
//   as two 2D pairs), the sub-object construction RW 0x5EA313 (`lea ecx,[x+0x110]` before a constructor call), the
//   client's own state words RW 0x673037..0x6730B2, and the vtable calls / arrays `[reg+0x10C]`, `[reg+ecx*4+0x10C]`.
//   BFME2 1.06 (donor, byte matched) has the same seven names only in its parse table
//   (Code/GameEngine/Source/Common/Rva001E72B4Reload.cpp:194, 241, 242, 247-250).
//
// The expectations below are the reference's, not this port's: the parsed values come from the field parser table,
// the defaults from the constructor stores, the copy from the assignment.

#include "doctest.h"
#include "ObjectTestUtil.h"
#include "IniTestUtil.h"

#include "GameLogic/Locomotor.h"

#include <string>

using namespace initest;

namespace
{
struct LocomotorFixture
{
	Fixture fx;
	LocomotorStore store;
	LocomotorStore *savedStore;

	LocomotorFixture()
	{
		savedStore = TheLocomotorStore;
		TheLocomotorStore = &store;
		fx.env.blocks.registerBlock("Locomotor", [](INI *ini) { LocomotorStore::parseLocomotorTemplateDefinitionGlobal(ini); });
	}

	~LocomotorFixture()
	{
		TheLocomotorStore = savedStore;
	}

	std::string load(const std::string &iniText)
	{
		return loadError(fx.env, "loco_port_228.ini", iniText);
	}
};
} // namespace

TEST_CASE("PORT-228: the seven Locomotor fields parse into LocomotorTemplate as the reference table says")
{
	LocomotorFixture fix;
	// The values are Ennorath 1.9.8.3's own (locomotor.inc: MaxThrustAngle 1, RudderCorrectionDegree 0.099,
	// RudderCorrectionRate 0.027, SwoopStandoffRadius 310.0, SwoopStandoffHeight 119.0,
	// SwoopTerminalVelocity 0.18, SwoopAccelerationRate 0.012); MaxThrustAngle is given in degrees here because
	// its parser is the angle one (RW 0x42EE15).
	const std::string ini =
		"Locomotor Port228Loco\n"
		"  Surfaces = GROUND\n"
		"  Speed = 60\n"
		"  MaxThrustAngle = 45d\n"
		"  RudderCorrectionDegree = 0.099\n"
		"  RudderCorrectionRate = 0.027\n"
		"  SwoopStandoffRadius = 310.0\n"
		"  SwoopStandoffHeight = 119.0\n"
		"  SwoopTerminalVelocity = 0.18\n"
		"  SwoopAccelerationRate = 0.012\n"
		"End\n";

	REQUIRE(fix.load(ini).empty());
	const LocomotorTemplate *tmpl = fix.store.findLocomotorTemplate("Port228Loco");
	REQUIRE(tmpl != nullptr);

	CHECK(tmpl->m_maxThrustAngle == doctest::Approx(0.7853982f)); // 45 degrees
	CHECK(tmpl->m_rudderCorrectionDegree == doctest::Approx(0.099f));
	CHECK(tmpl->m_rudderCorrectionRate == doctest::Approx(0.027f));
	CHECK(tmpl->m_swoopStandoffRadius == doctest::Approx(310.0f));
	CHECK(tmpl->m_swoopStandoffHeight == doctest::Approx(119.0f));
	CHECK(tmpl->m_swoopTerminalVelocity == doctest::Approx(0.18f));
	CHECK(tmpl->m_swoopAccelerationRate == doctest::Approx(0.012f));
}

TEST_CASE("PORT-228: the constructor defaults of the seven members are the reference's (RW 0x5E4326 stores)")
{
	LocomotorTemplate def;
	CHECK(def.m_maxThrustAngle == 0.0f);          // RW 0x5E4462
	CHECK(def.m_rudderCorrectionDegree == 0.0f);  // RW 0x5E45B9
	CHECK(def.m_rudderCorrectionRate == 0.0f);    // RW 0x5E45C1
	CHECK(def.m_swoopStandoffRadius == 200.0f);   // RW 0x5E4520 (0xBE4170)
	CHECK(def.m_swoopStandoffHeight == 200.0f);   // RW 0x5E4528 (0xBE4170)
	CHECK(def.m_swoopTerminalVelocity == 0.07f);  // RW 0x5E4540 (0xBF4A2C)
	CHECK(def.m_swoopAccelerationRate == 0.003f); // RW 0x5E45E9 (0xBF4A28)
}

TEST_CASE("PORT-228: the template copy and the assignment keep the seven members (RW 0x5E320B, RW 0x5E3480)")
{
	LocomotorFixture fix;
	const std::string ini =
		"Locomotor Port228CopyLoco\n"
		"  Surfaces = GROUND\n"
		"  Speed = 45\n"
		"  MaxThrustAngle = 30d\n"
		"  RudderCorrectionDegree = 0.089\n"
		"  RudderCorrectionRate = 0.127\n"
		"  SwoopStandoffRadius = 310.0\n"
		"  SwoopStandoffHeight = 119.0\n"
		"  SwoopTerminalVelocity = 0.18\n"
		"  SwoopAccelerationRate = 0.012\n"
		"End\n";
	REQUIRE(fix.load(ini).empty());
	const LocomotorTemplate *src = fix.store.findLocomotorTemplate("Port228CopyLoco");
	REQUIRE(src != nullptr);

	const LocomotorTemplate copyCtor(*src);
	CHECK(copyCtor.m_maxThrustAngle == src->m_maxThrustAngle);
	CHECK(copyCtor.m_rudderCorrectionDegree == src->m_rudderCorrectionDegree);
	CHECK(copyCtor.m_rudderCorrectionRate == src->m_rudderCorrectionRate);
	CHECK(copyCtor.m_swoopStandoffRadius == src->m_swoopStandoffRadius);
	CHECK(copyCtor.m_swoopStandoffHeight == src->m_swoopStandoffHeight);
	CHECK(copyCtor.m_swoopTerminalVelocity == src->m_swoopTerminalVelocity);
	CHECK(copyCtor.m_swoopAccelerationRate == src->m_swoopAccelerationRate);

	LocomotorTemplate assigned;
	assigned = *src;
	CHECK(assigned.m_maxThrustAngle == src->m_maxThrustAngle);
	CHECK(assigned.m_rudderCorrectionDegree == src->m_rudderCorrectionDegree);
	CHECK(assigned.m_rudderCorrectionRate == src->m_rudderCorrectionRate);
	CHECK(assigned.m_swoopStandoffRadius == src->m_swoopStandoffRadius);
	CHECK(assigned.m_swoopStandoffHeight == src->m_swoopStandoffHeight);
	CHECK(assigned.m_swoopTerminalVelocity == src->m_swoopTerminalVelocity);
	CHECK(assigned.m_swoopAccelerationRate == src->m_swoopAccelerationRate);
}
