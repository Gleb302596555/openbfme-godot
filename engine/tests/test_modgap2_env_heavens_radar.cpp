// OpenBFME. GPL-3.0.
// Editions: both RotWK 2.01 and BFME2 1.06.
// DestroyEnvironmentUpdate: RW 0x64F128 / BFME2 0x64F43C
// PartTheHeavensUpdate: RW 0x64F1B1 / BFME2 0x64F4C5
// RadarMarkerClientUpdate: RW 0x65287A / BFME2 0x652B1F
// WallUpgradeUpdate: RW 0x8AEBD3 / BFME2 0x4AB4D8
// RunOffMapBehavior: RW 0x8652BB / BFME2 0x46217B

#include "Common/FunctionCurve.h"
#include "Common/GameEdition.h"
#include "Common/INI.h"
#include "Common/Thing/ModuleFactory.h"
#include "Common/XferLoad.h"
#include "Common/XferSave.h"
#include "GameClient/HudContext.h"
#include "GameClient/InGameUI.h"
#include "GameClient/ProjectedShadowManager.h"
#include "GameClient/Radar.h"
#include "GameClient/RadarMarkerClientUpdate.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/ActiveBody.h"
#include "GameLogic/Module/DestroyEnvironmentUpdate.h"
#include "GameLogic/Module/LogicModules.h"
#include "GameLogic/Module/PartTheHeavensUpdate.h"
#include "GameLogic/Module/RainOfFireUpdate.h"
#include "GameLogic/Module/RunOffMapBehavior.h"
#include "GameLogic/Module/WallUpgradeUpdate.h"
#include "GameLogic/Object/Object.h"
#include "doctest.h"

#include <cmath>
#include <vector>

TEST_CASE("modgap2: FunctionCurve constant and multi-key Hermite spline evaluation")
{
	// 1. Constant curve
	FunctionCurve c1(42.0f);
	CHECK(c1(0.0f) == 42.0f);
	CHECK(c1(10.0f) == 42.0f);
	CHECK(c1(-5.0f) == 42.0f);

	// 2. Linear / Hermite curve between (0, 0) and (10, 100)
	FunctionCurve c2;
	c2.addKey(0.0f, 0.0f);
	c2.addKey(10.0f, 100.0f);
	CHECK(c2(0.0f) == 0.0f);
	CHECK(c2(10.0f) == 100.0f);
	CHECK(c2(5.0f) == 50.0f);

	// 3. Padding modes
	// Default HOLD
	CHECK(c2(-2.0f) == 0.0f);
	CHECK(c2(12.0f) == 100.0f);

	// EXTRAPOLATE
	c2.m_inPadding = CurvePadding::EXTRAPOLATE;
	c2.m_outPadding = CurvePadding::EXTRAPOLATE;
	CHECK(c2(-2.0f) == -20.0f);
	CHECK(c2(12.0f) == 120.0f);
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

TEST_CASE("modgap2: PartTheHeavensUpdate curve sampling, decal projection emitter and xfer")
{
	for (GameEdition edition : { GameEdition::RotWK201, GameEdition::BFME2_106 })
	{
		CAPTURE(gameEditionName(edition));

		ProjectedShadowManager shadowMgr;
		CHECK(TheProjectedShadowManager == &shadowMgr);

		PartTheHeavensUpdateModuleData data;
		data.m_texture = "SCCommandBar";
		data.m_color = 0x00FFFFFF;
		data.m_radius = FunctionCurve(100.0f);
		data.m_opacity = FunctionCurve(0.5f);
		data.m_angle = FunctionCurve(45.0f);

		{
			PartTheHeavensUpdate pth(nullptr, &data);
			CHECK(pth.update() == UPDATE_SLEEP_NONE);

			// Radius is doubled (2.0f * 100 = 200)
			CHECK(pth.currentRadius() == 200.0f);
			// Angle is 45 degrees converted to radians (45 * 0.0174532925)
			float expectedAngle = 45.0f * 0.0174532925f;
			CHECK(pth.currentAngleRads() == expectedAngle);
			// Opacity is 0.5 * 255 = 127
			CHECK((int)pth.currentOpacity() == 127);

			// Verify TheProjectedShadowManager has received the emitter (RW 0x8B01BC / BFME2 0x004ACADA)
			REQUIRE(shadowMgr.shadows().size() == 1);
			Shadow *s = shadowMgr.shadows().front().get();
			REQUIRE(s != nullptr);
			CHECK(s->m_texture == "SCCommandBar");
			CHECK(s->m_opacity == 127);
			CHECK((s->m_color & 0x00FFFFFF) == 0x00FFFFFF);

			// Verify dual child decals (child 1 at angle, child 2 at -angle)
			REQUIRE(s->m_58 != nullptr);
			REQUIRE(s->m_5c != nullptr);
			CHECK(s->m_58->m_sizeX == 200.0f);
			CHECK(s->m_58->m_sizeY == 200.0f);
			CHECK(s->m_58->m_angle == expectedAngle);
			CHECK(s->m_5c->m_angle == -expectedAngle);

			// xfer roundtrip
			XferSave save;
			pth.xfer(save);
			CHECK_FALSE(save.data().empty());

			PartTheHeavensUpdate restored(nullptr, &data);
			XferLoad load(save.data());
			restored.xfer(load);
			CHECK(restored.startFrame() == pth.startFrame());
		}

		// When pth is destroyed, shadow is released (RW 0x8AFF59)
		shadowMgr.prune();
		CHECK(shadowMgr.shadows().empty());
	}
}

TEST_CASE("modgap2: RadarMarkerClientUpdate TheRadar marker query, ping creation, movement and fadeout")
{
	for (GameEdition edition : { GameEdition::RotWK201, GameEdition::BFME2_106 })
	{
		CAPTURE(gameEditionName(edition));

		Radar radar;
		CHECK(TheRadar == &radar);

		RadarMarkerClientUpdateModuleData data;
		data.m_markerType = "PingOneRing";

		// Direct marker query on TheRadar (vslot 12 / 0x30, RW 0x44F0FA / BFME2 0x4C9CF7)
		RadarMarker *marker = TheRadar->createMarker("PingOneRing");
		REQUIRE(marker != nullptr);
		CHECK(marker->markerType() == "PingOneRing");
		CHECK(TheRadar->markers().size() == 1);

		// Position marker within default radar extent (0..1)
		marker->setPosition(Coord3D{ 0.2f, 0.3f, 0.0f });

		// Draw events projects marker to radar pixel (RW 0x44EDAE / BFME2 0x0004F515)
		std::vector<Radar::PingCall> calls;
		radar.drawEvents(0, 0, 128, 128, calls);

		// PingCall::Create and PingCall::Move emitted
		bool created = false;
		bool moved = false;
		for (const auto &c : calls)
		{
			if (c.kind == Radar::PingCall::Create && c.name == "PingOneRing")
			{
				created = true;
			}
			if (c.kind == Radar::PingCall::Move && c.id == marker->pingId())
			{
				moved = true;
			}
		}
		CHECK(created);
		CHECK(moved);

		// Move marker position by 0.4 units (moving > 50 radar pixels)
		calls.clear();
		marker->setPosition(Coord3D{ 0.6f, 0.7f, 0.0f });
		radar.drawEvents(0, 0, 128, 128, calls);
		bool moveAgain = false;
		for (const auto &c : calls)
		{
			if (c.kind == Radar::PingCall::Move && c.id == marker->pingId())
			{
				moveAgain = true;
			}
		}
		CHECK(moveAgain);

		// DeleteReference releases marker (RW 0x6D8711 / BFME2 0x002D76BB)
		marker->DeleteReference();
		CHECK(marker->isReleased());

		// Next drawEvents fades out ping and removes released marker
		calls.clear();
		radar.drawEvents(0, 0, 128, 128, calls);
		bool faded = false;
		for (const auto &c : calls)
		{
			if (c.kind == Radar::PingCall::FadeOut)
			{
				faded = true;
			}
		}
		CHECK(faded);
		CHECK(TheRadar->markers().empty());
	}
}

TEST_CASE("modgap2: WallUpgradeUpdate rubble countdown, upgrade reset and body restore")
{
	for (GameEdition edition : { GameEdition::RotWK201, GameEdition::BFME2_106 })
	{
		CAPTURE(gameEditionName(edition));

		WallUpgradeUpdateModuleData data;
		WallUpgradeUpdate wuu(nullptr, &data);

		CHECK(wuu.countdown() == 0);

		// On entering rubble damage state, arms 40 frames countdown (RW 0x8AEAE6 / BFME2 0x4AB3EB)
		wuu.onBodyDamageStateChange(nullptr, BODY_PRISTINE, BODY_RUBBLE);
		CHECK(wuu.countdown() == 40);

		// Update ticks countdown down each frame (RW 0x8AEBD3 / BFME2 0x4AB4D8)
		for (int f = 0; f < 39; ++f)
		{
			CHECK(wuu.update() == UPDATE_SLEEP_NONE);
			CHECK(wuu.countdown() == 39 - f);
		}

		// 40th frame completes countdown, retries after 5 frames when no attached building
		CHECK(wuu.update() == (UpdateSleepTime)5);
		CHECK(wuu.countdown() == 0);

		// xfer roundtrip
		XferSave save;
		wuu.xfer(save);
		CHECK_FALSE(save.data().empty());

		WallUpgradeUpdate restored(nullptr, &data);
		XferLoad load(save.data());
		restored.xfer(load);
		CHECK(restored.countdown() == wuu.countdown());
	}
}

TEST_CASE("modgap2: RunOffMapBehavior dormancy and trigger checks")
{
	for (GameEdition edition : { GameEdition::RotWK201, GameEdition::BFME2_106 })
	{
		CAPTURE(gameEditionName(edition));

		RunOffMapBehaviorModuleData data;
		data.m_requiresSpecificTrigger = true;
		data.m_waypointName = "OffMapPoint";
		data.m_dieOnMap = false;

		RunOffMapBehavior romb(nullptr, &data);
		CHECK_FALSE(romb.hasTriggered());

		// In skirmish / without trigger, stays dormant (stop S-1995)
		CHECK_FALSE(romb.hasTriggered());
		CHECK(data.m_requiresSpecificTrigger);

		// xfer roundtrip
		XferSave save;
		romb.xfer(save);
		CHECK_FALSE(save.data().empty());

		RunOffMapBehavior restored(nullptr, &data);
		XferLoad load(save.data());
		restored.xfer(load);
		CHECK(restored.hasTriggered() == romb.hasTriggered());
	}
}
