// OpenBFME unit tests for Local 64 (P-132): re-capturing a tavern's CaptureFlag lowers the previous
// owner's flag (TRANS_FlagDown) before switching sub-objects and house colour to the new capturer.
//
// TARGET FACTS (RotWK 2.01 game.dat, MD5 daed3668006cd90f01c34e5a7da1901f):
//   * W3DScriptedModelDraw::select (RW 0x4BF15E) calls apply (RW 0x4BF1DE -> 0x4BE587), which runs
//     the state's BeginScript. When BeginScript calls CurDrawableSetTransitionAnimState (RW 0x734ACE),
//     apply selects the TransitionState (RW 0x4BE7C4), updating m_curAnimState (+0x18).
//   * select then calls fireLuaEvents (RW 0x4B91E2) at RW 0x4BF214 on m_curAnimState (the transition
//     state), NOT on the target state. The target state's OnStateEnter fires only when advance
//     (RW 0x4BF56C -> 0x4BFA2D) finishes the transition and selects the target state.
//   * data/ini/object/neutral/captureflag.ini: START_CAPTURE (StateName = FlagUp, LuaEvent = OnStateEnter)
//     checks CurDrawablePrevAnimationState(); from IdleCaptured it requests TRANS_FlagDown (CAPFLAG_DN).
//     data/scripts/scripts.lua (OnCaptureFlagGenericEvent) switches FLAG_<SIDE> sub-objects on OnStateEnter.

#include "doctest.h"
#include "Common/Player.h"
#include "Common/SpecialPower.h"
#include "EnnorathScenario.h"
#include "GameClient/ClientEvents.h"
#include "GameClient/DrawableManager.h"
#include "GameClient/LiveScripting.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DScriptedModelDraw.h"
#include "GameLogic/Module/SpecialAbilityModules.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/Object/PartitionManager.h"

namespace
{

SpecialAbilityUpdate *findCaptureUpdate(Object &obj)
{
	for (const auto &m : obj.modules())
	{
		if (auto *sau = dynamic_cast<SpecialAbilityUpdate *>(m.get()))
		{
			if (sau->abilityData() && sau->abilityData()->m_specialPowerTemplate &&
				sau->abilityData()->m_specialPowerTemplate->m_type == 0x1D)
			{
				return sau;
			}
		}
	}
	return nullptr;
}

const W3DScriptedModelDraw *mainDraw(const Drawable *d)
{
	if (!d) return nullptr;
	for (const DrawEntry &e : d->entries())
	{
		if (auto *s = dynamic_cast<const W3DScriptedModelDraw *>(e.draw.get()))
		{
			if (!s->frame().animationStateName.empty() || !s->log().empty())
			{
				return s;
			}
		}
	}
	return nullptr;
}

bool isFlagHidden(const W3DDrawFrame &f, const std::string &suffix)
{
	for (const std::string &n : f.hiddenSubObjects)
	{
		if (n == suffix || (n.size() > suffix.size() && n.substr(n.size() - suffix.size()) == suffix))
		{
			return true;
		}
	}
	return false;
}

} // namespace

TEST_CASE("LOCAL-64: Ennorath CaptureFlag + Inn re-capture lowers the previous owner's flag before raising the new one (RW 0x4BF1DE -> 0x4BF214)")
{
	ennorathscenario::World world;
	if (!world.load("LOCAL-64 Ennorath CaptureFlag re-capture"))
	{
		return;
	}
	structtest::Arena arena(world.shared, "FactionElves", "FactionAngmar");
	arena.logic.combat().setAutoAcquireEnabled(false);
	arena.logic.settings().enableHouseColor = true;
	arena.logic.partition().setRegion(0.0f, 0.0f, 4000.0f, 4000.0f);

	Player *elves = arena.player(0);
	Player *angmar = arena.player(1);
	REQUIRE(elves != nullptr);
	REQUIRE(angmar != nullptr);
	elves->setPlayerColor(0xFF112233u);
	angmar->setPlayerColor(0xFFAABBCCu);

	ArchiveW3DFileSource source(*world.shared.mount->fs);
	WW3DAssetManager assets(source);
	DrawableManager drawables(assets, arena.logic);
	ClientEventRecorder events(arena.logic);
	arena.logic.setClientHooks(&events);
	LiveScripting scripting(arena.logic, *world.shared.mount->fs, world.shared.world->nameKeys());
	std::string err;
	REQUIRE_MESSAGE(scripting.start("", &err), err);
	struct Detach
	{
		GameLogic &logic;
		LiveScripting &scripting;
		~Detach()
		{
			scripting.detach();
			logic.setClientHooks(nullptr);
		}
	} detach{ arena.logic, scripting };

	Object *flag = world.place(arena, "CaptureFlag", -1, 300.0f, 300.0f);
	Object *inn = world.place(arena, "Inn", -1, 340.0f, 300.0f);
	Object *captB = world.place(arena, "ArthedainCapitan", 1, 302.0f, 300.0f);
	Object *captA = world.place(arena, "ArthedainCapitan", 0, 298.0f, 300.0f);
	REQUIRE(flag != nullptr);
	REQUIRE(inn != nullptr);
	REQUIRE(captB != nullptr);
	REQUIRE(captA != nullptr);

	for (int i = 0; i < 3; ++i)
	{
		arena.logic.runLogicFrame();
		drawables.applyEvents(events.take());
		drawables.advance(200.0);
	}

	const Drawable *fd = drawables.findByObject(flag->getID());
	REQUIRE(fd != nullptr);
	const W3DScriptedModelDraw *sd = mainDraw(fd);
	REQUIRE(sd != nullptr);

	// 1. Enemy (Angmar) captures the neutral tavern flag and linked Inn.
	SpecialAbilityUpdate *uB = findCaptureUpdate(*captB);
	REQUIRE(uB != nullptr);
	uB->initiateIntentToDoSpecialPower(uB->abilityData()->m_specialPowerTemplate, flag, nullptr, 0, 0);
	for (int i = 0; i < 150 && flag->getControllingPlayer() != angmar; ++i)
	{
		arena.logic.runLogicFrame();
		drawables.applyEvents(events.take());
		drawables.advance(200.0);
	}
	REQUIRE(flag->getControllingPlayer() == angmar);
	REQUIRE(inn->getControllingPlayer() == angmar);
	for (int i = 0; i < 10; ++i)
	{
		arena.logic.runLogicFrame();
		drawables.applyEvents(events.take());
		drawables.advance(200.0);
	}
	REQUIRE(sd->frame().animationStateName == "IdleCaptured");
	REQUIRE_FALSE(isFlagHidden(sd->frame(), "FLAG_ANGMAR"));
	REQUIRE(isFlagHidden(sd->frame(), "FLAG_ELVES"));
	REQUIRE(fd->getHouseColor() == angmar->getPlayerColor());

	// 2. Player (Elves) re-captures the flag: TRANS_FlagDown must lower Angmar's flag first,
	//    and only when FlagUp starts should FLAG_ELVES and Elves' colour appear.
	SpecialAbilityUpdate *uA = findCaptureUpdate(*captA);
	REQUIRE(uA != nullptr);
	uA->initiateIntentToDoSpecialPower(uA->abilityData()->m_specialPowerTemplate, flag, nullptr, 0, 0);

	int downFrames = 0;
	int upFrames = 0;
	bool sawLoweredBeforeRaise = false;
	for (int i = 0; i < 130; ++i)
	{
		arena.logic.runLogicFrame();
		drawables.applyEvents(events.take());
		drawables.advance(200.0);
		const W3DDrawFrame frame = sd->frame();
		if (frame.animationStateName == "TRANS_FlagDown")
		{
			++downFrames;
			CHECK(frame.motion0 != nullptr);
			if (frame.motion0)
			{
				CHECK(std::string(frame.motion0->Get_Name()) == "CAPFLAG_SKL.CAPFLAG_DN");
			}
			// While lowering, the previous owner's flag and colour must still be shown.
			CHECK_FALSE(isFlagHidden(frame, "FLAG_ANGMAR"));
			CHECK(isFlagHidden(frame, "FLAG_ELVES"));
			CHECK(fd->getHouseColor() == angmar->getPlayerColor());
		}
		else if (frame.animationStateName == "FlagUp")
		{
			if (downFrames > 0)
			{
				sawLoweredBeforeRaise = true;
			}
			++upFrames;
			CHECK(isFlagHidden(frame, "FLAG_ANGMAR"));
			CHECK_FALSE(isFlagHidden(frame, "FLAG_ELVES"));
			CHECK(fd->getHouseColor() == elves->getPlayerColor());
		}
	}
	CHECK(downFrames == 16);
	CHECK(upFrames == 84);
	CHECK(sawLoweredBeforeRaise);
	CHECK(flag->getControllingPlayer() == elves);
	CHECK(inn->getControllingPlayer() == elves);
	CHECK(sd->frame().animationStateName == "IdleCaptured");
	CHECK(isFlagHidden(sd->frame(), "FLAG_ANGMAR"));
	CHECK_FALSE(isFlagHidden(sd->frame(), "FLAG_ELVES"));
	CHECK(fd->getHouseColor() == elves->getPlayerColor());
}

TEST_CASE("LOCAL-64: Ennorath CaptureFlag cancelled re-capture restores the previous owner's flag after TRANS_FlagDownCancel")
{
	ennorathscenario::World world;
	if (!world.load("LOCAL-64 Ennorath CaptureFlag cancel"))
	{
		return;
	}
	structtest::Arena arena(world.shared, "FactionElves", "FactionAngmar");
	arena.logic.combat().setAutoAcquireEnabled(false);
	arena.logic.settings().enableHouseColor = true;
	arena.logic.partition().setRegion(0.0f, 0.0f, 4000.0f, 4000.0f);

	Player *elves = arena.player(0);
	Player *angmar = arena.player(1);
	REQUIRE(elves != nullptr);
	REQUIRE(angmar != nullptr);
	elves->setPlayerColor(0xFF112233u);
	angmar->setPlayerColor(0xFFAABBCCu);

	ArchiveW3DFileSource source(*world.shared.mount->fs);
	WW3DAssetManager assets(source);
	DrawableManager drawables(assets, arena.logic);
	ClientEventRecorder events(arena.logic);
	arena.logic.setClientHooks(&events);
	LiveScripting scripting(arena.logic, *world.shared.mount->fs, world.shared.world->nameKeys());
	std::string err;
	REQUIRE_MESSAGE(scripting.start("", &err), err);
	struct Detach
	{
		GameLogic &logic;
		LiveScripting &scripting;
		~Detach()
		{
			scripting.detach();
			logic.setClientHooks(nullptr);
		}
	} detach{ arena.logic, scripting };

	Object *flag = world.place(arena, "CaptureFlag", -1, 300.0f, 300.0f);
	Object *captA = world.place(arena, "ArthedainCapitan", 0, 298.0f, 300.0f);
	REQUIRE(flag != nullptr);
	REQUIRE(captA != nullptr);

	flag->setCapturedTeam(angmar->getDefaultTeam());
	flag->setModelConditionState(0x77, true); // CAPTURED
	for (int i = 0; i < 3; ++i)
	{
		arena.logic.runLogicFrame();
		drawables.applyEvents(events.take());
		drawables.advance(200.0);
	}
	const Drawable *fd = drawables.findByObject(flag->getID());
	REQUIRE(fd != nullptr);
	const W3DScriptedModelDraw *sd = mainDraw(fd);
	REQUIRE(sd != nullptr);
	REQUIRE(sd->frame().animationStateName == "IdleCaptured");
	REQUIRE_FALSE(isFlagHidden(sd->frame(), "FLAG_ANGMAR"));

	// Start re-capture until FlagUp is entered (FLAG_ELVES is now raising).
	SpecialAbilityUpdate *uA = findCaptureUpdate(*captA);
	REQUIRE(uA != nullptr);
	uA->initiateIntentToDoSpecialPower(uA->abilityData()->m_specialPowerTemplate, flag, nullptr, 0, 0);
	for (int i = 0; i < 40 && sd->frame().animationStateName != "FlagUp"; ++i)
	{
		arena.logic.runLogicFrame();
		drawables.applyEvents(events.take());
		drawables.advance(200.0);
	}
	REQUIRE(sd->frame().animationStateName == "FlagUp");
	REQUIRE_FALSE(isFlagHidden(sd->frame(), "FLAG_ELVES"));

	// Cancel capture (abort ability): flag goes through TRANS_FlagDownCancel -> Trans_CancelReraise -> IdleCaptured.
	uA->finishAbility(true, false);
	bool sawCancelTransition = false;
	for (int i = 0; i < 30; ++i)
	{
		arena.logic.runLogicFrame();
		drawables.applyEvents(events.take());
		drawables.advance(200.0);
		if (sd->frame().animationStateName == "TRANS_FlagDownCancel")
		{
			sawCancelTransition = true;
		}
	}
	CHECK(sawCancelTransition);
	CHECK(flag->getControllingPlayer() == angmar);
	CHECK(sd->frame().animationStateName == "IdleCaptured");
	CHECK_FALSE(isFlagHidden(sd->frame(), "FLAG_ANGMAR"));
	CHECK(isFlagHidden(sd->frame(), "FLAG_ELVES"));
	CHECK(fd->getHouseColor() == angmar->getPlayerColor());
}

