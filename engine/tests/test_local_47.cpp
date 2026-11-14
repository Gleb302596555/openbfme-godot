// OpenBFME unit tests: LOCAL-47 (issue #403, P-80 follow-up to LOCAL-25) - Giant bird banking in turns:
// eagle and fell beast presentation roll in Drawable::calcPhysicsXformGiantBird (RW 0x670E09 / DONOR BFME2 0x00270B0F).
// GPL-3.0.
//
// Retail RotWK 2.01 (game.dat md5 DAED3668006CD90F01C34E5A7DA1901F):
//   - Drawable::calcPhysicsXformGiantBird (RW 0x670E09 .. 0x670E9F):
//     1. Reads obj = Drawable + 0xFC, ai = obj + 0x260, asks ai->vslot 0x168 (RW 0x8BD372, true only for GiantBirdAIUpdate).
//        When false (0x670E31), returns without touching m_giantBirdRoll (Drawable + 0x140) or info.m_totalRoll (+4).
//     2. Reads target input via ai->vslot 0x188 (RW 0x8CEF91) -> AI + 0x600 (GiantBirdFlight::turnRate).
//     3. Computes roll = clamp(m_giantBirdRoll * 0.8f + (turnRate * 4.0f) * 0.2f, -0.8f, 0.8f)
//        (RW constants 0xbd88c0=4.0, 0xbde8d8=0.8, 0xbdad78=0.2, 0xc10ea0=-0.8).
//     4. Apply order in Drawable::applyPhysicsXform (RW 0x67BE90): Rotate_X(-info.m_totalRoll).

#include "doctest.h"
#include "EnnorathScenario.h"
#include "ObjectTestUtil.h"
#include "W3dSynth.h"

#include "GameClient/ClientEvents.h"
#include "GameClient/Drawable.h"
#include "GameClient/DrawableManager.h"
#include "GameClient/LogicSnapshot.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/GiantBirdAIUpdate.h"
#include "Libraries/WWVegas/WW3D2/assetmgr.h"
#include "Libraries/WWVegas/WWMath/matrix3d.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace objtest;
using namespace w3dsynth;

TEST_CASE("LOCAL-47: calcPhysicsXformGiantBird (RW 0x670E09) banks by GiantBirdAIUpdate turnRate and gates on AI vslot 0x168")
{
	objtest::World w;
	const std::string objIni =
		"Object TestGiantBirdObj\n"
		"  Geometry = SPHERE\n"
		"  GeometryMajorRadius = 15.0\n"
		"End\n";
	REQUIRE(w.load(objIni).empty());
	const ThingTemplate *tt = w.get("TestGiantBirdObj");
	REQUIRE(tt != nullptr);

	MemoryFileSource fs;
	WW3DAssetManager assets(fs);
	W3DLuaDrawScriptHost::Options hostOpt;
	DrawServices services(assets, w.modules, hostOpt);
	services.groundHeight = [](float /*x*/, float /*y*/, Coord3D *normal) -> float {
		if (normal)
		{
			*normal = Coord3D{ 0.0f, 0.0f, 1.0f };
		}
		return 0.0f;
	};

	std::vector<std::string> problems;
	Drawable d(services, tt, (DrawableID)1, (ObjectID)10, {}, problems);
	REQUIRE(problems.empty());

	LocomotorTemplate birdLoco;
	birdLoco.m_appearance = LOCO_GIANT_BIRD;

	ObjectSnapshot snap;
	snap.id = 10;
	snap.tmpl = tt;
	snap.position = Coord3D{ 200.0f, 200.0f, 120.0f };
	snap.previousPos = snap.position;
	snap.angle = 0.0f; // facing +X; local +Y is left wing, local +Z is up
	snap.aboveTerrain = true;
	snap.locomotorTemplate = &birdLoco;

	Drawable::SyncPrep prep;
	d.prepareSync(snap, 1, 0.0, false, prep);
	d.commitSync(prep);

	// 1) Gate RW 0x670E29 / 0x670E31: when AI vslot 0x168 is false (giantBirdAI = false),
	//    roll stays zero even if giantBirdTurnRate is non-zero.
	snap.giantBirdAI = false;
	snap.giantBirdTurnRate = 0.25f;
	unsigned clientFrame = 1;
	services.clientFrame = &clientFrame;
	d.applyPhysicsXform(snap, prep.pose);
	REQUIRE(d.locoInfo() != nullptr);
	CHECK(d.locoInfo()->m_giantBirdRoll == 0.0f);

	float basis[9] = {};
	Coord3D shownAt;
	d.getPresentationPose(basis, shownAt);
	CHECK(basis[8] == 1.0f);
	CHECK(basis[5] == 0.0f);

	// 2) Exact RW 0x670E3D..0x670E98 formula for positive turnRate (left turn):
	//    step 1: 0.0 * 0.8 + (0.1 * 4.0) * 0.2 = 0.08 rad
	//    step 2: 0.08 * 0.8 + 0.08 = 0.144 rad
	snap.giantBirdAI = true;
	snap.giantBirdTurnRate = 0.1f;

	clientFrame = 2;
	d.applyPhysicsXform(snap, prep.pose);
	// tolerance: IEEE binary32 float multiply/add rounding
	CHECK(d.locoInfo()->m_giantBirdRoll == doctest::Approx(0.08f).epsilon(1e-5));

	clientFrame = 3;
	d.applyPhysicsXform(snap, prep.pose);
	const float expectedStep2 = 0.144f;
	// tolerance: IEEE binary32 float multiply/add rounding across two steps
	CHECK(d.locoInfo()->m_giantBirdRoll == doctest::Approx(expectedStep2).epsilon(1e-5));

	// RW 0x67BFFB (applyPhysicsXform Rotate_X(-roll)): for +X heading and roll > 0,
	// left wing (col 1 z = basis[7]) dips down (-sin(roll)) and up vector (col 2 y = basis[5]) tilts left (+sin(roll)).
	d.getPresentationPose(basis, shownAt);
	// tolerance: sin/cos trigonometric rounding in Matrix3D::Rotate_X
	CHECK(basis[5] == doctest::Approx(std::sin(expectedStep2)).epsilon(1e-5));
	// tolerance: sin/cos trigonometric rounding in Matrix3D::Rotate_X
	CHECK(basis[7] == doctest::Approx(-std::sin(expectedStep2)).epsilon(1e-5));
	// tolerance: sin/cos trigonometric rounding in Matrix3D::Rotate_X
	CHECK(basis[8] == doctest::Approx(std::cos(expectedStep2)).epsilon(1e-5));

	// 3) If giantBirdAI becomes false afterward, RW 0x670E31 returns without altering m_giantBirdRoll,
	//    and applyPhysicsXform writes no roll for that frame (m_totalRoll stays 0 -> unrolled basis).
	snap.giantBirdAI = false;
	clientFrame = 4;
	d.applyPhysicsXform(snap, prep.pose);
	// tolerance: IEEE binary32 value preserved from step 2
	CHECK(d.locoInfo()->m_giantBirdRoll == doctest::Approx(expectedStep2).epsilon(1e-5));
	d.getPresentationPose(basis, shownAt);
	CHECK(basis[5] == 0.0f);
	CHECK(basis[8] == 1.0f);

	// 4) Clamp to +0.8f (RW 0xbde8d8) and -0.8f (RW 0xc10ea0), then decay * 0.8f at zero turnRate.
	snap.giantBirdAI = true;
	snap.giantBirdTurnRate = 2.0f;
	for (int i = 0; i < 20; ++i)
	{
		++clientFrame;
		d.applyPhysicsXform(snap, prep.pose);
	}
	// tolerance: IEEE binary32 float clamp value 0.8f
	CHECK(d.locoInfo()->m_giantBirdRoll == doctest::Approx(0.8f).epsilon(1e-5));

	snap.giantBirdTurnRate = -2.0f;
	for (int i = 0; i < 30; ++i)
	{
		++clientFrame;
		d.applyPhysicsXform(snap, prep.pose);
	}
	// tolerance: IEEE binary32 float clamp value -0.8f
	CHECK(d.locoInfo()->m_giantBirdRoll == doctest::Approx(-0.8f).epsilon(1e-5));

	snap.giantBirdTurnRate = 0.0f;
	++clientFrame;
	d.applyPhysicsXform(snap, prep.pose);
	// tolerance: IEEE binary32 float decay -0.8f * 0.8f
	CHECK(d.locoInfo()->m_giantBirdRoll == doctest::Approx(-0.64f).epsilon(1e-5));
	services.clientFrame = nullptr;
}

TEST_CASE("LOCAL-47 Ennorath: eagle (GondorGwaihir, ElvenFortressEagle) and fell beast (MordorFellBeast) bank in turns (RW 0x670E09)")
{
	ennorathscenario::World world;
	if (!world.load("LOCAL-47 GiantBird banking"))
	{
		return;
	}

	// Neighbour check first: hovering / uncommanded fell beast has zero turnRate and does not phantom-bank.
	{
		structtest::Arena arena(world.shared, "FactionMen", "FactionMordor", 240);
		ArchiveW3DFileSource source(*world.merged.fs);
		WW3DAssetManager assets(source);
		DrawableManager drawables(assets, arena.logic);
		ClientEventRecorder events(arena.logic);
		arena.logic.setClientHooks(&events);
		struct UnbindHooks
		{
			GameLogic &logic;
			~UnbindHooks() { logic.setClientHooks(nullptr); }
		} unbind{ arena.logic };

		Object *idleBeast = world.place(arena, "MordorFellBeast", 1, 600.0f, 600.0f);
		idleBeast->setOrientation(0.0f);
		std::shared_ptr<const LogicSnapshot> snap;
		for (int i = 0; i < 10; ++i)
		{
			arena.logic.runLogicFrame();
			drawables.applyEvents(events.take());
			snap = LogicSnapshot::build(arena.logic, snap.get(), false, 0);
			drawables.advance(200.0);
			drawables.syncTransforms(*snap, 0.5, true);
		}
		const Drawable *d = drawables.findByObject(idleBeast->getID());
		REQUIRE(d != nullptr);
		REQUIRE(d->locoInfo() != nullptr);
		CHECK(d->locoInfo()->m_giantBirdRoll == 0.0f);
	}

	// Active turn checks on Ennorath eagles and fell beast:
	// Ennorath / RotWK giant birds use GiantBirdAIUpdate + GIANT_BIRD Locomotors
	// (data\ini\object\goodfaction\units\men\gwaihir.ini, elvenfortresseagle.ini, evilfaction\units\mordor\fellbeast.ini).
	for (const char *tmplName : { "GondorGwaihir", "ElvenFortressEagle", "MordorFellBeast" })
	{
		CAPTURE(tmplName);
		const int side = (std::string(tmplName).rfind("Mordor", 0) == 0) ? 1 : 0;
		structtest::Arena arena(world.shared, "FactionMen", "FactionMordor", 240);
		ArchiveW3DFileSource source(*world.merged.fs);
		WW3DAssetManager assets(source);
		DrawableManager drawables(assets, arena.logic);
		ClientEventRecorder events(arena.logic);
		arena.logic.setClientHooks(&events);
		struct UnbindHooks
		{
			GameLogic &logic;
			~UnbindHooks() { logic.setClientHooks(nullptr); }
		} unbind{ arena.logic };

		Object *flyer = world.place(arena, tmplName, side, 600.0f, 600.0f);
		GiantBirdAIUpdate *birdAi = dynamic_cast<GiantBirdAIUpdate *>(flyer->getAIUpdateInterface());
		REQUIRE_MESSAGE(birdAi != nullptr, tmplName << " must use GiantBirdAIUpdate");
		flyer->setOrientation(0.0f); // facing +X

		std::shared_ptr<const LogicSnapshot> snap;
		auto step = [&]() {
			arena.logic.runLogicFrame();
			drawables.applyEvents(events.take());
			snap = LogicSnapshot::build(arena.logic, snap.get(), false, 0);
			drawables.advance(200.0);
			drawables.syncTransforms(*snap, 0.5, true);
		};
		step();
		step();

		// Order a 90-degree turn (+Y) so Locomotor::giantBirdFollowPath updates GiantBirdFlight::turnRate.
		birdAi->aiMoveToPosition(Coord3D{ 600.0f, 1500.0f, 0.0f }, CMD_FROM_PLAYER);

		float maxAbsTurnRate = 0.0f;
		float maxAbsRoll = 0.0f;
		float maxLateralUpTilt = 0.0f;
		for (int i = 0; i < 80; ++i)
		{
			step();
			maxAbsTurnRate = std::max(maxAbsTurnRate, std::fabs(birdAi->flight().turnRate));
			const Drawable *d = drawables.findByObject(flyer->getID());
			REQUIRE(d != nullptr);
			REQUIRE(d->locoInfo() != nullptr);
			maxAbsRoll = std::max(maxAbsRoll, std::fabs(d->locoInfo()->m_giantBirdRoll));

			float shownBasis[9] = {};
			Coord3D shownAt;
			d->getPresentationPose(shownBasis, shownAt);
			// Lateral projection of presentation up vector (col 2) onto horizontal left vector (col 1 projected onto XY).
			// Strictly positive when Rotate_X(-roll) tilts the wing / up vector during a turn.
			const float yaw = flyer->getOrientation();
			const float leftX = -std::sin(yaw);
			const float leftY = std::cos(yaw);
			const float lateralUp = std::fabs(shownBasis[2] * leftX + shownBasis[5] * leftY);
			maxLateralUpTilt = std::max(maxLateralUpTilt, lateralUp);
		}
		std::printf("LOCAL-47 %s maxTurnRate=%.4f maxRoll=%.4f maxLateralUpTilt=%.4f\n",
			tmplName, maxAbsTurnRate, maxAbsRoll, maxLateralUpTilt);
		// tolerance: mod Locomotor turn rates vary per template (observed ~0.10-0.13); asserts non-trivial turn
		CHECK(maxAbsTurnRate > 0.01f);
		// tolerance: mod Locomotor turn rate & damping vary per template; asserts non-trivial banked roll (observed ~0.18-0.27 rad)
		CHECK(maxAbsRoll > 0.03f);
		// tolerance: pitch tilts local up vector slightly; asserts significant lateral bank tilt (observed ~0.18-0.27)
		CHECK(maxLateralUpTilt > 0.03f);
	}
}
