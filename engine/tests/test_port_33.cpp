// OpenBFME unit tests. GPL-3.0.
// Lane PORT-33 (issue #105): SkirmishAIData's FarmingThreshold (+ 0x984) and LogicFramesTillAISelfDestructs (+ 0x9A8) as AIWoTRForfeitTactic reads them —
// the targetless prototype 8 of RotWK's tactic generator (ctor RW 0x9B7316, vtable RW 0xC89B74, docs/ennorath/ports/port33-forfeit.md).
//
// The expectations follow the RW instructions, not the port: the throttle RW 0x9B74A4 / 0x9B74B1, the threshold's `jge` RW 0x9B7D74, the three checks
// RW 0x9B7D7F, the countdown RW 0x9B7DAC with its 0x46 warning frame RW 0x9B7DAF, the tail RW 0x9B7E64, and the six bias terms RW 0x9B7C41 .. 0x9B7D70 with
// the floats 0xBD889C (2.0), 0xBDE8C8 (1.5), 0xBDD42C (3.0) and 0xC10438 (9.0). The state lives in the AI, so the driver is the same transition function
// the tactic uses; no game files are needed.

#include "doctest.h"

#include "GameLogic/SkirmishAI/AITacticalAI.h"
#include "GameLogic/SkirmishAI/SkirmishAIData.h"

#include <functional>

namespace
{
using AITacticalAI::ForfeitFrame; // the answer of one frame; ForfeitState / ForfeitTerms are the port's structs (AITacticalAI.h)

// the four data fields the tactic reads; the rest keeps SkirmishAIData's constructor values (RW 0x6A9807)
SkirmishAIData data(int farmingThreshold, int framesTillSelfDestructs = 900, int frameBetweenChecks = 300)
{
	SkirmishAIData d;
	d.farmingThreshold = farmingThreshold;
	d.logicFramesTillAISelfDestructs = framesTillSelfDestructs;
	d.logicFrameBetweenRetreatChecks = frameBetweenChecks;
	return d;
}

// the state the constructor leaves (RW 0x9B7316 / 0x9B735D): + 0x58 / + 0x60 / + 0x64 clear and + 0x5C = LogicFramesTillAISelfDestructs. Without that the
// tail RW 0x9B7E64 would fire on the first frame, as it does in RW for a data value of 0
ForfeitState fresh(const SkirmishAIData &d)
{
	ForfeitState s;
	s.counter = d.logicFramesTillAISelfDestructs;
	return s;
}

// one frame whose score is `score`, whatever the frame asks for
ForfeitFrame step(ForfeitState &s, int &next, unsigned frame, int score, const SkirmishAIData &d, int *asked = nullptr)
{
	return AITacticalAI::forfeitFrame(s, next, frame, [&] {
		if (asked)
		{
			++*asked;
		}
		return score;
	}, d);
}
} // namespace

TEST_CASE("PORT-33 AIWoTRForfeitTactic: the throttle of the score check (RW 0x9B74A4 .. 0x9B74C2)")
{
	const SkirmishAIData d = data(10);
	ForfeitState s = fresh(d);
	int next = 0;
	int asked = 0;
	// the launch puts the current frame there (RW 0x9B7260), so the first check runs on that very frame
	CHECK(step(s, next, 0, 50, d, &asked).checked);
	CHECK(asked == 1);
	CHECK(next == 300); // RW 0x9B74B1: frame + LogicFrameBetweenRetreatChecks (+ 0x9B4)
	CHECK(s.badChecks == 0);
	// the frames before the record's frame do not check and do not move it
	CHECK_FALSE(step(s, next, 1, 0, d, &asked).checked);
	CHECK_FALSE(step(s, next, 299, 0, d, &asked).checked);
	CHECK(asked == 1);
	CHECK(next == 300);
	// the recorded frame itself checks (RW 0x9B74A4 uses `ja`: equal passes)
	CHECK(step(s, next, 300, 0, d, &asked).checked);
	CHECK(asked == 2);
	CHECK(next == 600);
	CHECK(s.badChecks == 1);
}

TEST_CASE("PORT-33 AIWoTRForfeitTactic: FarmingThreshold resets the streak (RW 0x9B7D74 / 0x9B7DA3)")
{
	const SkirmishAIData d = data(10);
	ForfeitState s = fresh(d);
	int next = 0;
	CHECK_FALSE(step(s, next, 0, 9, d).forfeit);
	CHECK(s.badChecks == 1);
	CHECK_FALSE(step(s, next, 300, 9, d).forfeit);
	CHECK(s.badChecks == 2);
	// a score equal to the threshold is not below it: `cmp eax, [ebp - 0xBC]; jge`
	CHECK_FALSE(step(s, next, 600, 10, d).forfeit);
	CHECK(s.badChecks == 0);
	CHECK_FALSE(s.triggered);
	// and the streak starts again from zero
	CHECK_FALSE(step(s, next, 900, 0, d).forfeit);
	CHECK(s.badChecks == 1);
	CHECK_FALSE(s.triggered);
}

TEST_CASE("PORT-33 AIWoTRForfeitTactic: three checks below the threshold start the countdown (RW 0x9B7D7C .. 0x9B7D9B)")
{
	const SkirmishAIData d = data(10, 900);
	ForfeitState s = fresh(d);
	int next = 0;
	CHECK_FALSE(step(s, next, 0, 9, d).forfeit);
	CHECK_FALSE(step(s, next, 300, 9, d).forfeit);
	CHECK(s.badChecks == 2);
	// the third check: `cmp dword ptr [edi + 0x64], 3; jl 0x9B7E5F` falls through
	CHECK_FALSE(step(s, next, 600, 9, d).forfeit); // the counter is 900, the tail does not fire in this frame
	CHECK(s.triggered);
	CHECK(s.counter == 900); // LogicFramesTillAISelfDestructs (+ 0x9B8 = table + 0x9A8)
	CHECK_FALSE(s.warned);
}

TEST_CASE("PORT-33 AIWoTRForfeitTactic: the countdown, its warning frame and the self-destruct (RW 0x9B7DAC .. 0x9B7E67)")
{
	const SkirmishAIData d = data(10, 900);
	ForfeitState s;
	s.triggered = true;
	s.counter = 900; // the value the constructor put there (RW 0x9B735D)
	int next = 12345; // never reached while the surrender runs
	int asked = 0;
	int warnings = 0;
	int warnFrame = -1;
	int warnCounter = -1;
	int fired = 0;
	int firedFrame = -1;
	for (int frame = 0; frame < 900; ++frame)
	{
		const ForfeitFrame f = step(s, next, (unsigned)frame, 0, d, &asked);
		CHECK(f.checked == false); // the triggered branch jumps over the throttle (RW 0x9B7495 `jne 0x9B7DAC`)
		if (f.warned)
		{
			++warnings;
			warnFrame = frame;
			warnCounter = s.counter; // RW 0x9B7DAF: the message of the frame `counter < 70`
		}
		if (f.forfeit)
		{
			++fired;
			firedFrame = frame;
			break; // RW ends the tactic here (RW 0x9B7E99), so the port never sees a second frame
		}
	}
	CHECK(asked == 0);    // the score is not computed while the surrender runs
	CHECK(warnings == 1); // + 0x60 keeps it a one-shot (RW 0x9B7DB9)
	CHECK(warnFrame == 830);
	CHECK(warnCounter == 69); // 900 - 831 decrements, the first frame under 0x46
	CHECK(fired == 1);
	CHECK(firedFrame == 899); // 900 decrements: the counter is 0 in the 900th frame (RW 0x9B7E64 `cmp [esi + 0x5C], 0; jg`)
	CHECK(s.counter == 0);
	CHECK(next == 12345); // the record's frame is untouched by the countdown
}

TEST_CASE("PORT-33 AIWoTRForfeitTactic: a counter without frames fires at once (RW 0x9B7E64)")
{
	// LogicFramesTillAISelfDestructs = 0: the constructor's counter is 0, so the tail fires on the first update even though no surrender was decided
	const SkirmishAIData d = data(10, 0);
	ForfeitState s = fresh(d);
	int next = 0;
	CHECK(step(s, next, 0, 9, d).forfeit);
	CHECK_FALSE(s.triggered);
	// a positive counter with a score at the threshold never fires: nothing runs it down unless the surrender is decided
	const SkirmishAIData high = data(10, 900);
	ForfeitState open = fresh(high);
	int nextOpen = 0;
	for (int frame = 0; frame < 900; frame += 300)
	{
		CHECK_FALSE(step(open, nextOpen, (unsigned)frame, 10, high).forfeit);
	}
	CHECK_FALSE(open.triggered);
	CHECK(open.counter == 900);
}

TEST_CASE("PORT-33 AIWoTRForfeitTactic: the six bias terms (RW 0x9B7C41 .. 0x9B7D70)")
{
	const SkirmishAIData d; // the table's biases: army quality 2, army quantity 1, hero quality 1, map control 2, base strength 1, ring 2
	CHECK(d.armyQualityBias == 2);
	CHECK(d.armyQuantityBias == 1);
	CHECK(d.heroQualityBias == 1);
	CHECK(d.mapControlBias == 2);
	CHECK(d.baseStrengthBias == 1);
	CHECK(d.ringOwnershipBias == 2);
	ForfeitTerms own;
	ForfeitTerms enemy;
	// 1: `comiss own, 2.0 * enemy; jbe` — twice is not more than twice
	own.armyQuality = 100.0f;
	enemy.armyQuality = 50.0f;
	CHECK(AITacticalAI::forfeitScoreFrom(own, enemy, false, d) == 0);
	enemy.armyQuality = 49.0f;
	CHECK(AITacticalAI::forfeitScoreFrom(own, enemy, false, d) == 2);
	own.armyQuality = 50.0f;
	enemy.armyQuality = 100.1f;
	CHECK(AITacticalAI::forfeitScoreFrom(own, enemy, false, d) == -2);
	// 2: 1.5 times
	own = ForfeitTerms{};
	enemy = ForfeitTerms{};
	own.armyQuantity = 150.0f;
	enemy.armyQuantity = 100.0f;
	CHECK(AITacticalAI::forfeitScoreFrom(own, enemy, false, d) == 0);
	own.armyQuantity = 150.5f;
	CHECK(AITacticalAI::forfeitScoreFrom(own, enemy, false, d) == 1);
	// 3: a difference of 3.0, not 3.0 times
	own = ForfeitTerms{};
	enemy = ForfeitTerms{};
	own.heroQuality = 4.0f;
	enemy.heroQuality = 1.0f;
	CHECK(AITacticalAI::forfeitScoreFrom(own, enemy, false, d) == 0);
	own.heroQuality = 5.0f;
	CHECK(AITacticalAI::forfeitScoreFrom(own, enemy, false, d) == 1);
	own.heroQuality = 1.0f;
	enemy.heroQuality = 5.0f;
	CHECK(AITacticalAI::forfeitScoreFrom(own, enemy, false, d) == -1);
	// 4: both twice the other side's and more than 9.0 (RW 0x9B7CE6 .. 0x9B7D17)
	own = ForfeitTerms{};
	enemy = ForfeitTerms{};
	own.mapControl = 10.0f;
	enemy.mapControl = 4.0f;
	CHECK(AITacticalAI::forfeitScoreFrom(own, enemy, false, d) == 2);
	own.mapControl = 9.0f; // not more than 9.0
	enemy.mapControl = 4.0f;
	CHECK(AITacticalAI::forfeitScoreFrom(own, enemy, false, d) == 0);
	own.mapControl = 12.0f;
	enemy.mapControl = 8.0f; // not twice the enemies'
	CHECK(AITacticalAI::forfeitScoreFrom(own, enemy, false, d) == 0);
	// 5: base strength, 1.5 times
	own = ForfeitTerms{};
	enemy = ForfeitTerms{};
	own.baseStrength = 150.0f;
	enemy.baseStrength = 100.0f;
	CHECK(AITacticalAI::forfeitScoreFrom(own, enemy, false, d) == 0);
	own.baseStrength = 151.0f;
	CHECK(AITacticalAI::forfeitScoreFrom(own, enemy, false, d) == 1);
	// 6: the ring only ever adds (RW 0x9B7D41 `cmovne`)
	CHECK(AITacticalAI::forfeitScoreFrom(ForfeitTerms{}, ForfeitTerms{}, true, d) == 2);
	// 7: the fortress bonus is a float added before the truncation (RW 0x9B7D67 .. 0x9B7D70)
	own = ForfeitTerms{};
	own.armyQuantity = 150.5f;
	enemy = ForfeitTerms{};
	enemy.armyQuantity = 100.0f;
	own.fortressBonus = 1.9f;
	CHECK(AITacticalAI::forfeitScoreFrom(own, enemy, false, d) == 2); // 1 + 1.9 truncated
	own.fortressBonus = 2.0f;
	CHECK(AITacticalAI::forfeitScoreFrom(own, enemy, false, d) == 3);
}
