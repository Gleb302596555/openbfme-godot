// OpenBFME. GPL-3.0. Synthetic data only; expectations from RotWK 2.01.
#include "doctest.h"
#include "MoveTestUtil.h"
#include "GameClient/InGameUI.h"
#include "GameClient/UnitVoiceResponse.h"
#include "GameLogic/GameMessage.h"
#include "GameLogic/Module/AIUpdate.h"

namespace
{
struct Sink462 : UnitVoiceResponse::Sink
{
	std::string planning = "Plan462";
	std::vector<std::string> played;
	bool soundExists(const std::string &) override { return true; }
	std::string planningModeOrderSound() const override { return planning; }
	void playSound(const std::string &s, ObjectID, int) override { played.push_back(s); }
	void reportEva(const std::string &, const Coord3D *) override {}
};
struct Rig462
{
	movetest::MoveWorld f;
	Sink462 sink;
	InGameUI ui;
	UnitVoiceResponse voice{*f.logic, sink};
	Object *unit = nullptr;
	Rig462()
	{
		const std::string error = f.w.load(
			"Object Unit462\n"
			" KindOf = INFANTRY SELECTABLE\n"
			" VoiceMove = Move462\n VoiceAttack = Attack462\n VoiceAttackCharge = Charge462\n"
			" VoiceSelect = Select462\n VoiceEnterStateMove = EnterMove462\n"
			" VoiceAttackChargeTimeout = 1000\n"
			" UnitSpecificSounds\n VoiceEnter = Enter462\n End\n"
			" Behavior = AIUpdateInterface AI462\n End\nEnd\n",
			INI_LOAD_OVERWRITE, "port462.ini");
		REQUIRE_MESSAGE(error.empty(), error);
		unit = f.make("Unit462", f.teamOf("Alice"));
		REQUIRE(unit);
		voice.setLocalPlayerIndex(unit->getControllingPlayer()->getPlayerIndex());
		voice.setInGameUI(&ui);
	}
	bool play(int message)
	{
		sink.played.clear();
		return voice.pickAndPlay({unit->getID()}, message);
	}
};
}

TEST_CASE("PORT-462 planning replaces move attack and enter exactly once")
{
	// RW 0x8DE901/0x8DDFD7/0x8DF416: mode 2 replaces before ordinary rules.
	for (int message : {MSG_DO_MOVETO, MSG_DO_ATTACK_OBJECT, MSG_ENTER})
	{
		Rig462 r;
		r.unit->getControllingPlayer()->setOrderMode(2);
		CHECK(r.play(message));
		CHECK(r.sink.played == std::vector<std::string>{"Plan462"});
		// RW 0x8DE94B followed by exit: empty/NoSound does not fall through.
		r.sink.planning.clear();
		CHECK_FALSE(r.play(message));
		CHECK(r.sink.played.empty());
	}
}

TEST_CASE("PORT-462 planning does not replace selection or enter-state movement")
{
	// RW 0x8DE8F3 excludes enter-state; selection is a separate switch case.
	Rig462 r;
	r.unit->getControllingPlayer()->setOrderMode(2);
	CHECK(r.play(MSG_CREATE_SELECTED_GROUP));
	CHECK(r.sink.played == std::vector<std::string>{"Select462"});
	CHECK(r.play(UnitVoiceResponse::VOICE_EVENT_ENTER_STATE_MOVE));
	CHECK(r.sink.played == std::vector<std::string>{"EnterMove462"});
	// RW 0x8DE908: queued mode 1 is not planning mode 2.
	r.unit->getControllingPlayer()->setOrderMode(1);
	CHECK(r.play(MSG_DO_MOVETO));
	CHECK(r.sink.played == std::vector<std::string>{"Move462"});
}

TEST_CASE("PORT-462 waypoint movement gate is live and does not mute attacks or planning")
{
	// RW 0x8DEA45–0x8DEA5C: non-enter-state AND waypoint AND moving.
	Rig462 r;
	auto *ai = r.unit->getAIUpdateInterface();
	REQUIRE(ai);
	r.ui.setWaypointMode(true);
	CHECK(r.play(MSG_DO_MOVETO)); // stationary: no gate
	ai->mover().startingMove();
	REQUIRE(ai->isMoving());
	CHECK_FALSE(r.play(MSG_DO_MOVETO));
	CHECK(r.sink.played.empty());
	CHECK(r.play(MSG_DO_ATTACK_OBJECT)); // no waypoint gate in attack helper
	CHECK(r.sink.played == std::vector<std::string>{"Charge462"});
	r.ui.setWaypointMode(false); // query current UI, not a cached bool
	CHECK(r.play(MSG_DO_MOVETO));
	r.ui.setWaypointMode(true);
	r.unit->getControllingPlayer()->setOrderMode(2);
	CHECK(r.play(MSG_DO_MOVETO)); // planning precedes the movement gate
	CHECK(r.sink.played == std::vector<std::string>{"Plan462"});
}

TEST_CASE("PORT-462 charge guard is a drawable Object back-reference not clientHooks presence")
{
	// RW 0x8DF045/0x8DF065 -> choice +0x14 -> 0x8DE15C -> 0x671EE4.
	// The held Object exists here, even without a rendering device. Do not mute it.
	Rig462 r;
	REQUIRE(r.unit->clientHooks() == nullptr);
	CHECK(r.play(MSG_DO_ATTACK_OBJECT));
	CHECK(r.sink.played == std::vector<std::string>{"Charge462"});
	// RW 0x671F16 starts timeout on playback; RW 0x671F0B gates the next charge.
	CHECK(r.play(MSG_DO_ATTACK_OBJECT));
	CHECK(r.sink.played == std::vector<std::string>{"Attack462"});
	CHECK_FALSE(r.voice.pickAndPlay({INVALID_ID}, MSG_DO_ATTACK_OBJECT));
}

TEST_CASE("PORT-462 waypoint gate includes waiting for path but excludes enter-state")
{
	// RW 0x8DEA37–0x8DEA43 ORs isMoving with AI +0x3B1; 0x8DEA49
	// bypasses waypoint suppression for enter-state movement.
	Rig462 r;
	auto *ai = r.unit->getAIUpdateInterface();
	REQUIRE(ai);
	const Coord3D destination{100.0f, 0.0f, 0.0f};
	ai->mover().requestAttackPath(PATHFIND_INVALID_ID, destination);
	REQUIRE(ai->mover().isWaitingForPath());
	REQUIRE_FALSE(ai->isMoving());
	r.ui.setWaypointMode(true);
	CHECK_FALSE(r.play(MSG_DO_MOVETO));
	CHECK(r.sink.played.empty());
	CHECK(r.play(UnitVoiceResponse::VOICE_EVENT_ENTER_STATE_MOVE));
	CHECK(r.sink.played == std::vector<std::string>{"EnterMove462"});
}
