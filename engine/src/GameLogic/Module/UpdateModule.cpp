// OpenBFME. GPL-3.0.
// See GameLogic/Module/UpdateModule.h.

#include "GameLogic/Module/UpdateModule.h"

#include "Common/StateHash.h"
#include "Common/Xfer.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object/Object.h"

void UpdateModule::setWakeFrame(Object *obj, UpdateSleepTime wakeDelay)
{
	GameLogic &logic = obj->logic();
	const UnsignedInt now = logic.getFrame();
	// ZH UpdateModule.cpp:85-89: GameLogic::friend_awakenUpdateModule(obj, this, now + delay); FOREVER overflows the clamp harmlessly
	logic.friend_awakenUpdateModule(obj, this, now + (UnsignedInt)wakeDelay);
}

// RW 0x00850BE9 / DONOR BFME2 0x0044DF28 (UpdateModule::frameToSleepTime)
UpdateSleepTime UpdateModule::frameToSleepTime(UnsignedInt f1, UnsignedInt f2, UnsignedInt f3, UnsignedInt f4) const
{
	UnsignedInt earliest = f1;
	if (earliest > f2) earliest = f2;
	if (earliest > f3) earliest = f3;
	if (earliest > f4) earliest = f4;
	const UnsignedInt now = getObject()->logic().getFrame();
	if (earliest > now)
	{
		return (UpdateSleepTime)(earliest - now);
	}
	return UPDATE_SLEEP_NONE;
}

void UpdateModule::crc(StateHasher &hasher) const
{
	hasher.addU32(m_nextCallFrame);
	hasher.addI32(m_indexInLogic);
	hasher.addI32(m_phaseInLogic);
}

// ---- lane SAVE-4b: the base xfers (see Common/Module.h) ------------------------------------------------------------------------------------------------
void BehaviorModule::xferBehaviorModule(Xfer &x)
{
	XferVersion v{ 1, 1 }; // RW 0x8CD4AA
	x.xferVersion(v);
	xferObjectModule(x);
}

void UpdateModule::xferUpdateModule(Xfer &x)
{
	XferVersion v{ 1, 1 }; // RW 0x850C60
	x.xferVersion(v);
	xferBehaviorModule(x);
	x.xferUnsignedInt(&m_nextCallFrame);
}
