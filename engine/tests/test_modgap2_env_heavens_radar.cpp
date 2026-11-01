// OpenBFME. GPL-3.0.
// Editions: both RotWK 2.01 and BFME2 1.06.
// DestroyEnvironmentUpdate: RW 0x64F128 / BFME2 0x64F43C
// PartTheHeavensUpdate: RW 0x64F1B1 / BFME2 0x64F4C5
// RadarMarkerClientUpdate: RW 0x65287A / BFME2 0x652B1F

#include "Common/FunctionCurve.h"
#include "Common/GameEdition.h"
#include "Common/INI.h"
#include "Common/Thing/ModuleFactory.h"
#include "Common/XferLoad.h"
#include "Common/XferSave.h"
#include "GameClient/RadarMarkerClientUpdate.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/DestroyEnvironmentUpdate.h"
#include "GameLogic/Module/LogicModules.h"
#include "GameLogic/Module/PartTheHeavensUpdate.h"
#include "doctest.h"

#include <cmath>

TEST_CASE("modgap2: FunctionCurve constant and multi-key Hermite spline evaluation")
{
	// 1. Constant curve
	FunctionCurve c1(42.0f);
	CHECK(c1(0.0f) == doctest::Approx(42.0f)); // tolerance: floating-point evaluation
	CHECK(c1(10.0f) == doctest::Approx(42.0f)); // tolerance: floating-point evaluation
	CHECK(c1(-5.0f) == doctest::Approx(42.0f)); // tolerance: floating-point evaluation

	// 2. Linear / Hermite curve between (0, 0) and (10, 100)
	FunctionCurve c2;
	c2.addKey(0.0f, 0.0f);
	c2.addKey(10.0f, 100.0f);
	CHECK(c2(0.0f) == doctest::Approx(0.0f)); // tolerance: floating-point evaluation
	CHECK(c2(10.0f) == doctest::Approx(100.0f)); // tolerance: floating-point evaluation
	CHECK(c2(5.0f) == doctest::Approx(50.0f)); // tolerance: floating-point evaluation

	// 3. Padding modes
	// Default HOLD
	CHECK(c2(-2.0f) == doctest::Approx(0.0f)); // tolerance: floating-point evaluation
	CHECK(c2(12.0f) == doctest::Approx(100.0f)); // tolerance: floating-point evaluation

	// EXTRAPOLATE
	c2.m_inPadding = CurvePadding::EXTRAPOLATE;
	c2.m_outPadding = CurvePadding::EXTRAPOLATE;
	CHECK(c2(-2.0f) == doctest::Approx(-20.0f)); // tolerance: floating-point evaluation
	CHECK(c2(12.0f) == doctest::Approx(120.0f)); // tolerance: floating-point evaluation
}

TEST_CASE("modgap2: DestroyEnvironmentUpdate fields, update delay and xfer")
{
	for (GameEdition edition : { GameEdition::RotWK201, GameEdition::BFME2_106 })
	{
		CAPTURE(gameEditionName(edition));

		DestroyEnvironmentUpdateModuleData data;
		data.m_startTime = 15;
		data.m_destructionTime = 60;

		DestroyEnvironmentUpdate deu(nullptr, &data);
		CHECK(deu.startFrame() == 0);
		CHECK(deu.objectID() == 0);

		// With no object found, sleeps forever (stop S-7092)
		CHECK(deu.update() == UPDATE_SLEEP_FOREVER);

		// xfer roundtrip
		XferSave save;
		deu.xfer(save);
		CHECK_FALSE(save.data().empty());

		DestroyEnvironmentUpdate restored(nullptr, &data);
		XferLoad load(save.data());
		restored.xfer(load);
		CHECK(restored.startFrame() == deu.startFrame());
		CHECK(restored.objectID() == deu.objectID());
	}
}

TEST_CASE("modgap2: PartTheHeavensUpdate curve sampling and xfer")
{
	for (GameEdition edition : { GameEdition::RotWK201, GameEdition::BFME2_106 })
	{
		CAPTURE(gameEditionName(edition));

		PartTheHeavensUpdateModuleData data;
		data.m_texture = "SCCommandBar";
		data.m_color = 0x00FFFFFF;
		data.m_radius = FunctionCurve(100.0f);
		data.m_opacity = FunctionCurve(0.5f);
		data.m_angle = FunctionCurve(45.0f);

		PartTheHeavensUpdate pth(nullptr, &data);
		CHECK(pth.update() == UPDATE_SLEEP_NONE);

		// Radius is doubled (2.0f * 100 = 200)
		CHECK(pth.currentRadius() == doctest::Approx(200.0f)); // tolerance: floating-point evaluation
		// Angle is 45 degrees converted to radians (45 * 0.017453292)
		CHECK(pth.currentAngleRads() == doctest::Approx(45.0f * 0.0174532925f)); // tolerance: floating-point evaluation
		// Opacity is 0.5 * 255 = 127
		CHECK((int)pth.currentOpacity() == 127);

		// xfer roundtrip
		XferSave save;
		pth.xfer(save);
		CHECK_FALSE(save.data().empty());

		PartTheHeavensUpdate restored(nullptr, &data);
		XferLoad load(save.data());
		restored.xfer(load);
		CHECK(restored.startFrame() == pth.startFrame());
	}
}

TEST_CASE("modgap2: RadarMarkerClientUpdate MarkerType and registration")
{
	for (GameEdition edition : { GameEdition::RotWK201, GameEdition::BFME2_106 })
	{
		CAPTURE(gameEditionName(edition));

		RadarMarkerClientUpdateModuleData data;
		data.m_markerType = "PingOneRing";

		RadarMarkerClientUpdate rmcu(nullptr, &data);
		CHECK(rmcu.markerType() == "PingOneRing");

		// clientUpdate does not crash when radar is absent (stop S-2451)
		rmcu.clientUpdate();
	}
}

