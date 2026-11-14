// OpenBFME. GPL-3.0.
// PORT-165: the shared wait state in the horde attack-on-position machine.
#pragma once

#include "GameLogic/AI/AIStateMachine.h"

// RW 0x740E16, vtable 0xC277E8; both 0x2C0 and 0x2BE use this state.
class AIHordePositionWaitState final : public AIState
{
public:
	explicit AIHordePositionWaitState(AIStateMachine &machine);
	StateReturnType onEnter() override;
	StateReturnType update() override;
	void xfer(class Xfer &x) override { xferNoFields(x); }
};
