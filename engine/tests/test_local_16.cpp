// OpenBFME unit tests (lane LOCAL-16, issue #137, TOP20 #19):
// Fields with no known reader:
// 1. AIUpdateInterface/FadeOnPortals:
//    - Field table RW 0xC0F530 row 18, offset +0x54, parseBool (RW 0x42E558)
//    - Module data constructor RW 0x66E98A clears byte ptr [esi + 0x54] = 0
//    - Reader in AIUpdateInterface::update (RW 0x66E58F):
//      checks byte ptr [m_moduleData + 0x54] != 0 (RW 0x66E726) -> calls updatePortalFade (RW 0x664F93)
//    - updatePortalFade (RW 0x664F93):
//      if on portal (m_curPortal / m_onPortal, RW 0x5E2DF4):
//        if !m_portalFaded (+0x3CB):
//          m_portalFaded = true (RW 0x66500C)
//          drawable->fadeOut(5) (RW 0x664FF2, 5 frames = RW 0xD9F608)
//          drawable->setShadowsEnabled(false) (RW 0x665004)
//      else (off portal):
//        if m_portalFaded:
//          m_portalFaded = false (RW 0x66504C)
//          drawable->fadeIn(5) (RW 0x665031, 5 frames = RW 0xD9F608)
//          drawable->setShadowsEnabled(true) (RW 0x665044)
//    - Snapshot: AIUpdate::xfer (RW 0x6679C8) transfers m_portalFaded (+0x3CB)
// 2. Object/EmotionRange (TOP20 #19):
//    - Field table RW 0xDA3DF8 row 26, offset +0x4DC, parseReal (RW 0x42ED00)
//    - Object ctor (RW 0x699DFD..0x699E05) copies to Object +0x1BC
//    - Snapshot (RW 0x698AA3) transfers [ebx + 0x15C] (Object +0x1BC)
//    - Object crc (RW 0x69355D) does not hash +0x1BC
//    - Zero logic reads of [reg + 0x1BC] across .text
//    - EmotionTrackerUpdate hero/fear scan at RW 0x8B5A7E calls Object::getVisionRange (RW 0x68E4E2) directly
//    - visionRangeOf in EmotionModules.cpp:149 delegates to ShroudManager::visionRange (RW 0x68E4E2)
// Both BFME2 1.06 and RotWK 2.01 have these identical mechanisms ("both").

#include "doctest.h"

#include "MoveTestUtil.h"

#include "Common/ArchiveFileSystem.h"
#include "Common/INI.h"
#include "Common/XferLoad.h"
#include "Common/XferSave.h"
#include "GameClient/ClientEvents.h"
#include "GameLogic/AI/AIMove.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/System/ShroudManager.h"

#include <vector>

namespace
{
const char kFaderObject[] =
	"Object PortalFaderWalker\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  VisionRange = 100\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"    FadeOnPortals = Yes\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"End\n"
	"Object NonFaderWalker\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"    FadeOnPortals = No\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"End\n";
} // namespace

TEST_CASE("LOCAL-16: AIUpdateInterface/FadeOnPortals raw parsing into combatSettings")
{
	movetest::MoveWorld w(40, 20, kFaderObject);
	w.buildMap();

	Object *fader = w.spawn("PortalFaderWalker", 55.0f, 105.0f);
	REQUIRE(fader);
	AIUpdateInterface *aiFader = w.aiOf(fader);
	REQUIRE(aiFader);
	CHECK(aiFader->combatSettings().fadeOnPortals == true);

	Object *nonFader = w.spawn("NonFaderWalker", 55.0f, 105.0f);
	REQUIRE(nonFader);
	AIUpdateInterface *aiNonFader = w.aiOf(nonFader);
	REQUIRE(aiNonFader);
	CHECK(aiNonFader->combatSettings().fadeOnPortals == false);

	Object *defaultWalker = w.spawn("Walker", 55.0f, 105.0f);
	REQUIRE(defaultWalker);
	AIUpdateInterface *aiDefault = w.aiOf(defaultWalker);
	REQUIRE(aiDefault);
	CHECK(aiDefault->combatSettings().fadeOnPortals == false);
}

TEST_CASE("LOCAL-16: AIUpdateInterface/FadeOnPortals fades on portal and restores on exit")
{
	movetest::MoveWorld w(40, 20, kFaderObject);
	for (int y = 0; y < 20; ++y)
	{
		w.terrain.cliff.insert({ 19, y });
		w.terrain.cliff.insert({ 20, y });
	}
	Object *u = w.spawn("PortalFaderWalker", 55.0f, 105.0f);
	Object *owner = w.spawn("Block", 305.0f, 25.0f);
	w.buildMap();

	Pathfinder &pf = w.ai->pathfinder();
	auto make = [&](float x) {
		PathfindWaypoint wp;
		wp.pos = { x, 105.0f, 0.0f };
		wp.type = PWAYPOINT_WALK;
		wp.owner = owner->getID();
		return pf.createWaypoint(wp);
	};
	const int a = make(175.0f), b = make(235.0f), m = make(205.0f), n = make(195.0f);
	pf.addWaypointLink(a, b);
	pf.addPortal(a);
	pf.setWaypointNext(a, m);
	pf.addWaypointLink(b, a);
	pf.addPortal(b);
	pf.setWaypointNext(b, n);

	ClientEventRecorder events(*w.logic);
	w.logic->setClientHooks(&events);
	struct Detach { GameLogic &logic; ~Detach() { logic.setClientHooks(nullptr); } } detach{ *w.logic };

	AIUpdateInterface *ai = w.aiOf(u);
	REQUIRE(ai);
	CHECK_FALSE(ai->isPortalFaded());

	ai->aiMoveToPosition(Coord3D{ 305.0f, 105.0f, 0.0f }, CMD_FROM_PLAYER);

	int portalFadedFrames = 0;
	bool sawFadeOut = false;
	bool sawFadeIn = false;
	bool sawShadowDisable = false;
	bool sawShadowEnable = false;

	for (int f = 0; f < 600; ++f)
	{
		w.frames(1);
		if (ai->isPortalFaded())
		{
			++portalFadedFrames;
		}

		auto batch = events.take(true);
		for (const auto &ev : batch)
		{
			if (ev.object == u->getID())
			{
				if (ev.kind == ClientEvent::FADE_OUT)
				{
					sawFadeOut = true;
					CHECK(ev.fadeFrames == 5); // RW 0x664FF2 (5 frames = RW 0xD9F608)
				}
				else if (ev.kind == ClientEvent::FADE_IN)
				{
					sawFadeIn = true;
					CHECK(ev.fadeFrames == 5); // RW 0x665031 (5 frames = RW 0xD9F608)
				}
				else if (ev.kind == ClientEvent::SET_SHADOWS)
				{
					if (!ev.shadowsOn)
					{
						sawShadowDisable = true; // RW 0x665004
					}
					else
					{
						sawShadowEnable = true; // RW 0x665044
					}
				}
			}
		}

		const Coord3D p = *u->getPosition();
		if (p.x > 300.0f && !ai->isMoving())
		{
			break;
		}
	}

	CHECK(portalFadedFrames > 0); // tolerance: unit spends several frames traversing the portal waypoint segment according to its speed
	CHECK(sawFadeOut);
	CHECK(sawShadowDisable);
	CHECK(sawFadeIn);
	CHECK(sawShadowEnable);
	CHECK_FALSE(ai->isPortalFaded());
}

TEST_CASE("LOCAL-16: AIUpdateInterface/FadeOnPortals No does not trigger portal fade")
{
	movetest::MoveWorld w(40, 20, kFaderObject);
	for (int y = 0; y < 20; ++y)
	{
		w.terrain.cliff.insert({ 19, y });
		w.terrain.cliff.insert({ 20, y });
	}
	Object *u = w.spawn("NonFaderWalker", 55.0f, 105.0f);
	Object *owner = w.spawn("Block", 305.0f, 25.0f);
	w.buildMap();

	Pathfinder &pf = w.ai->pathfinder();
	auto make = [&](float x) {
		PathfindWaypoint wp;
		wp.pos = { x, 105.0f, 0.0f };
		wp.type = PWAYPOINT_WALK;
		wp.owner = owner->getID();
		return pf.createWaypoint(wp);
	};
	const int a = make(175.0f), b = make(235.0f), m = make(205.0f), n = make(195.0f);
	pf.addWaypointLink(a, b);
	pf.addPortal(a);
	pf.setWaypointNext(a, m);
	pf.addWaypointLink(b, a);
	pf.addPortal(b);
	pf.setWaypointNext(b, n);

	ClientEventRecorder events(*w.logic);
	w.logic->setClientHooks(&events);
	struct Detach { GameLogic &logic; ~Detach() { logic.setClientHooks(nullptr); } } detach{ *w.logic };

	AIUpdateInterface *ai = w.aiOf(u);
	REQUIRE(ai);
	CHECK_FALSE(ai->isPortalFaded());

	ai->aiMoveToPosition(Coord3D{ 305.0f, 105.0f, 0.0f }, CMD_FROM_PLAYER);

	bool sawAnyFadeEvent = false;

	for (int f = 0; f < 600; ++f)
	{
		w.frames(1);
		CHECK_FALSE(ai->isPortalFaded());

		auto batch = events.take(true);
		for (const auto &ev : batch)
		{
			if (ev.object == u->getID())
			{
				if (ev.kind == ClientEvent::FADE_OUT || ev.kind == ClientEvent::FADE_IN)
				{
					sawAnyFadeEvent = true;
				}
			}
		}

		const Coord3D p = *u->getPosition();
		if (p.x > 300.0f && !ai->isMoving())
		{
			break;
		}
	}

	CHECK_FALSE(sawAnyFadeEvent);
	CHECK_FALSE(ai->isPortalFaded());
}

TEST_CASE("LOCAL-16: AIUpdateInterface/FadeOnPortals snapshot xfer round-trip")
{
	movetest::MoveWorld w(40, 20, kFaderObject);
	for (int y = 0; y < 20; ++y)
	{
		w.terrain.cliff.insert({ 19, y });
		w.terrain.cliff.insert({ 20, y });
	}
	Object *u = w.spawn("PortalFaderWalker", 55.0f, 105.0f);
	Object *owner = w.spawn("Block", 305.0f, 25.0f);
	w.buildMap();

	Pathfinder &pf = w.ai->pathfinder();
	auto make = [&](float x) {
		PathfindWaypoint wp;
		wp.pos = { x, 105.0f, 0.0f };
		wp.type = PWAYPOINT_WALK;
		wp.owner = owner->getID();
		return pf.createWaypoint(wp);
	};
	const int a = make(175.0f), b = make(235.0f), m = make(205.0f), n = make(195.0f);
	pf.addWaypointLink(a, b);
	pf.addPortal(a);
	pf.setWaypointNext(a, m);
	pf.addWaypointLink(b, a);
	pf.addPortal(b);
	pf.setWaypointNext(b, n);

	AIUpdateInterface *ai = w.aiOf(u);
	REQUIRE(ai);
	ai->aiMoveToPosition(Coord3D{ 305.0f, 105.0f, 0.0f }, CMD_FROM_PLAYER);

	// Advance until walker is on the portal and faded
	bool reachedPortal = false;
	for (int f = 0; f < 300; ++f)
	{
		w.frames(1);
		if (ai->isPortalFaded())
		{
			reachedPortal = true;
			break;
		}
	}
	REQUIRE(reachedPortal);
	CHECK(ai->isPortalFaded());

	// Save AI state (RW 0x6679C8 transfers m_portalFaded)
	XferSave save;
	ai->xferAIState(save);

	// Load into a fresh walker
	Object *u2 = w.spawn("PortalFaderWalker", 55.0f, 105.0f);
	AIUpdateInterface *ai2 = w.aiOf(u2);
	REQUIRE(ai2);
	CHECK_FALSE(ai2->isPortalFaded());

	XferLoad load(save.data());
	ai2->xferAIState(load);
	CHECK(ai2->isPortalFaded());
}

TEST_CASE("LOCAL-16: Object/EmotionRange retail unused check and visionRangeOf matching RW 0x68E4E2")
{
	// Evidence from RotWK 2.01 (game.dat md5 daed3668...) and BFME2 1.06 (game.dat md5 34391b25...):
	// - Field table: RW 0xDA3DF8 row 26, offset 1244 (0x4DC), parseReal (RW 0x42ED00)
	// - Object ctor: RW 0x699DFD..0x699E05 copies [edx + 0x4DC] to [esi + 0x1BC]
	// - Object snapshot: RW 0x698AA3 transfers [ebx + 0x15C] (Object + 0x1BC)
	// - Object crc: RW 0x69355D does not read or hash +0x1BC
	// - Disassembly of entire .text: ZERO logic instructions read [reg + 0x1BC]
	// - EmotionTrackerUpdate hero/fear scan: RW 0x8B5A7E calls Object::getVisionRange (RW 0x68E4E2) directly
	//   and squares it (mulss xmm0, xmm0), exactly as EmotionModules.cpp:667 visionRangeOf does.
	movetest::MoveWorld w(40, 20, kFaderObject);
	w.buildMap();
	Object *u = w.spawn("PortalFaderWalker", 55.0f, 105.0f);
	REQUIRE(u);

	// Verify that AIUpdateInterface::objectVisionRangeOf and ShroudManager::visionRange match RW 0x68E4E2
	const float vr = AIUpdateInterface::objectVisionRangeOf(*u);
	CHECK(vr == 100.0f);
	if (ShroudManager *sm = w.logic->shroud())
	{
		CHECK(sm->visionRange(*u) == vr);
	}
}
