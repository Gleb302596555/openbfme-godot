// OpenBFME. GPL-3.0.
// See GameLogic/Module/HordeAIUpdate.h.
//
// TARGET FACTS (RotWK 2.01 game.dat, Port 283, issue #650):
//   * Factory RW 0x64E20D (0x4AC bytes) -> ctor RW 0x89DFAE (vtable +0x00 = 0xC66D48, +0x4A8 = 0xC66CE8), dtor RW 0x89DDF0 / scalar RW 0x89E01F,
//     getModuleKey RW 0x89DE21, getClassName RW 0x89DE66, loadPostProcess thunk RW 0x892C33 -> RW 0x667BEF, xfer RW 0x89DF93 (v1 + AIUpdateInterface::xfer;
//     AIUpdateSnapshot.cpp).
//   * getDockUpdateInterface RW 0x89DE6C -> DockUpdateInterface vtable RW 0xC66CE8 (20 slots): isClearToApproach RW 0x89E03B (contain slot 0x18),
//     reserveApproachPosition RW 0x89E05C (slot 0x18, else slot 0x2C RW 0x873F30, then dock slot 0x14, *index = 0), advanceApproachPosition RW 0x89DE7C (true),
//     isClearToEnter RW 0x9B501B (true), isClearToAdvance RW 0x8F8014 (false), getEnterPosition RW 0x89E0B5 (contain slot 0x1C), getDockPosition /
//     getExitPosition RW 0x9F9E80 (tail-calls dock slot 0x14), action RW 0x89E133 (status 0x26 HORDE_MEMBER = 1, contain slot 0x20, returns false),
//     isDockOpen / isAllowPassthroughType RW 0x8BD372 (true), unknownSlot40 RW 0x7FEAC1 (false), isRallyPointAfterDockType RW 0x9188EB (false),
//     on*Reached / cancelDock / setDockOpen / setDockCrippled RW 0x9F3A3C (no-op).
//   * privateMoveToPosition RW 0x89E0F6 (AI vslot 0x34) and privateEnter RW 0x89E41F (AI vslot 0x5C): bail when the object has no HordeContainInterface,
//     else forward to AIUpdateInterface (RW 0x66B3FD / RW 0x66D85F).
//   * aiDoCommand RW 0x89E169: gate (vslot 0x250), porcupine branch RW 0x89E1C0 .. 0x89E205 (slot 0xF0, slot 0x5C + RW 0x89DE81 -> slot 0x60 toggle +
//     TheControlBar +0x28 dirty RW 0x89E1EE, else RW 0x89DF11 drops the command), then hand-off switch RW 0x89E207 .. 0x89E29A (clears 0x5A, re-fetches
//     contain RW 0x89E281, slot 0x14), then base AIUpdateInterface::aiDoCommand.
//   * update RW 0x89E2D0 .. 0x89E41E: skips horde work when isGarrisoned (slot 0x12C); empty idle horde with live entering members (slot 0xA4) and neither
//     UNSELECTABLE (0x03) nor IS_LEAVING_FACTORY (0x5A) recentres on them (slot 0x98); parked horde past the 1-second last-shot cooldown (RW 0x68B645 vs
//     frame - LOGICFRAMES_PER_SECOND, RW 0xD9F608 = 5) retires 0x5A once anyMemberLeavingFactory (slot 0xA0, RW 0x86FA01) is false, consumes a pending rally
//     point (slot 0x248 -> slot 0x10(0) + aiMoveToPosition), or returns idle entering members to formation (slot 0x9C -> slot 0x10(regroup)); slot 0xDC
//     rank-weapon condition pass (RW 0x86F7A3 -> RW 0x875F18) is tracked in S-7490.

#include "GameLogic/Module/HordeAIUpdate.h"

#include "Common/GameCommon.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object/Contain/HordeContainRuntime.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/ObjectTemplateInfo.h"
#include "GameLogic/Combat/ObjectWeapons.h"

namespace
{
HordeContainInterface *hordeContainOf(Object *horde)
{
	ContainModuleInterface *contain = horde ? horde->getContain() : nullptr;
	return contain ? contain->getHordeContainInterface() : nullptr;
}

const HordeContainInterface *hordeContainOf(const Object *horde)
{
	ContainModuleInterface *contain = horde ? const_cast<Object *>(horde)->getContain() : nullptr;
	return contain ? contain->getHordeContainInterface() : nullptr;
}
} // namespace

// ---- DockUpdateInterface (RW vtable 0xC66CE8) ----

// RW 0x89E03B (dock slot + 0x00)
bool HordeAIUpdate::isClearToApproach(const Object &docker) const
{
	const HordeContainInterface *hc = hordeContainOf(getObject());
	return hc != nullptr && hc->isMemberOrEntering(docker);
}

// RW 0x89E05C (dock slot + 0x04)
bool HordeAIUpdate::reserveApproachPosition(Object &docker, Coord3D *pos, int *index)
{
	HordeContainInterface *hc = hordeContainOf(getObject());
	if (!hc)
	{
		return false;
	}
	if (!hc->isMemberOrEntering(docker))
	{
		hc->reserveSlotForEnteringMember(&docker); // slot 0x2C (RW 0x873F30)
	}
	getEnterPosition(docker, pos); // dock slot + 0x14 (RW 0x89E0B5)
	if (index)
	{
		*index = 0;
	}
	return true;
}

// RW 0x89E0B5 (dock slot + 0x14)
void HordeAIUpdate::getEnterPosition(Object &docker, Coord3D *pos)
{
	if (!pos)
	{
		return;
	}
	const HordeContainInterface *hc = hordeContainOf(getObject());
	if (!hc)
	{
		return;
	}
	*pos = hc->getMemberFormationPosition(&docker); // slot 0x1C (RW 0x877D89)
}

// RW 0x9F9E80 (dock slot + 0x18: `mov eax, [ecx]; jmp [eax+0x14]`)
void HordeAIUpdate::getDockPosition(Object &docker, Coord3D *pos)
{
	getEnterPosition(docker, pos);
}

// RW 0x9F9E80 (dock slot + 0x1C: `mov eax, [ecx]; jmp [eax+0x14]`)
void HordeAIUpdate::getExitPosition(Object &docker, Coord3D *pos)
{
	getEnterPosition(docker, pos);
}

// RW 0x89E133 (dock slot + 0x30)
bool HordeAIUpdate::action(Object &docker, Object *drone)
{
	(void)drone;
	HordeContainInterface *hc = hordeContainOf(getObject());
	if (hc)
	{
		static const int hordeMember = ObjectTemplateInfoBuilder::objectStatusIndex("HORDE_MEMBER"); // status 0x26
		if (hordeMember >= 0)
		{
			docker.setStatus((unsigned)hordeMember, true);
		}
		hc->acceptMemberFromGarrison(&docker); // slot 0x20 (RW 0x872A7F -> RW 0x8757FC)
	}
	return false;
}

// ---- Guarded AI command bodies ----

// RW 0x89E0F6 (AI vslot + 0x34)
void HordeAIUpdate::privateMoveToPosition(const Coord3D &pos, CommandSourceType source, bool postMoveVoice)
{
	if (!hordeContainOf(getObject()))
	{
		return;
	}
	AIUpdateInterface::privateMoveToPosition(pos, source, postMoveVoice);
}

// RW 0x89E41F (AI vslot + 0x5C)
bool HordeAIUpdate::privateEnter(Object *container, CommandSourceType source)
{
	if (!hordeContainOf(getObject()))
	{
		return false;
	}
	return AIUpdateInterface::privateEnter(container, source);
}

// ---- Update (RW 0x89E2D0 .. 0x89E41E) ----

void HordeAIUpdate::updateBeforeBase()
{
	Object *horde = getObject();
	ContainModuleInterface *contain = horde->getContain();
	HordeContainInterface *hc = contain ? contain->getHordeContainInterface() : nullptr;
	if (!hc || hc->isGarrisoned()) // RW 0x89E2F2 .. 0x89E2FA: skip straight to AIUpdateInterface::update while garrisoned
	{
		return;
	}

	static const int unselectable = ObjectTemplateInfoBuilder::objectStatusIndex("UNSELECTABLE");         // status 0x03
	static const int leavingFactory = ObjectTemplateInfoBuilder::objectStatusIndex("IS_LEAVING_FACTORY"); // status 0x5A
	const bool isUnselectable = unselectable >= 0 && horde->testStatus((unsigned)unselectable);
	const bool isLeaving = leavingFactory >= 0 && horde->testStatus((unsigned)leavingFactory);

	// RW 0x89E300 .. 0x89E35A: an idle horde with an empty contain list, live entering members (+0xA4, RW 0x86F9CA) and neither UNSELECTABLE nor
	// IS_LEAVING_FACTORY recentres on those members (+0x98, RW 0x86F18F)
	if (contain->getContainCount() == 0 && isIdle() && hc->hasLiveEnteringMembers() && !isUnselectable && !isLeaving)
	{
		hc->recentreOnMembers(horde);
	}

	// RW 0x89E360 .. 0x89E409: parked horde factory-exit retirement + pending rally hand-off + idle entering-member reunion (gated by RW 0x68B645 lastShotFrame
	// < frame - LOGICFRAMES_PER_SECOND, unsigned sub + jae at RW 0x89E380 .. 0x89E388) and the 1-second formation refresh
	if (!isMoving())
	{
		const unsigned frame = horde->logic().getFrame();
		const unsigned lastShot = horde->getWeapons() ? horde->getWeapons()->firingTracker().lastShotFrame() : 0u; // RW 0x68B645
		if (frame > (unsigned)LOGICFRAMES_PER_SECOND && lastShot < frame - (unsigned)LOGICFRAMES_PER_SECOND)
		{
			bool regroup = true;
			bool canReturn = true;
			if (isLeaving && !hc->anyMemberLeavingFactory()) // RW 0x89E38A .. 0x89E3E8 (slot 0xA0, RW 0x86FA01)
			{
				regroup = false;
				horde->setStatus((unsigned)leavingFactory, false);
				Coord3D rally{ 0.0f, 0.0f, 0.0f };
				if (hc->consumePendingRallyPoint(rally)) // slot 0x248 (RW 0x86BD98)
				{
					hc->returnToFormation(false);           // slot 0x10(0) (RW 0x8759FF)
					aiMoveToPosition(rally, CMD_FROM_AI);   // RW 0x66C4CA(&rally, 2)
					canReturn = false;
				}
			}
			if (canReturn && hc->allEnteringMembersIdle()) // RW 0x89E3EA .. 0x89E403 (slot 0x9C, RW 0x86F93F)
			{
				hc->returnToFormation(regroup);
			}
		}
		const unsigned last = hc->formationRefreshFrame();
		if (frame > (unsigned)LOGICFRAMES_PER_SECOND && last < frame - (unsigned)LOGICFRAMES_PER_SECOND)
		{
			hc->updateFormation(); // slot 0xDC (RW 0x86F7A3; rank-weapon condition sub-call RW 0x875F18 tracked in S-7490)
		}
	}
}

// ---- Porcupine command filter (RW 0x89DE81 / 0x89DF11 / 0x89E1C0 .. 0x89E205) ----

bool HordeAIUpdate::commandTogglesPorcupine(int command, CommandSourceType source)
{
	if (command == AIUpdateInterface::kCommandUnidentifiedMove)
	{
		command = 0x00;
	}
	if (command < 0 || command > 0x53)
	{
		return false; // RW 0x89DE8A -> 0x89DEA8
	}
	switch (command) // RW 0x89DEBD index table -> RW 0x89DEAD jump table
	{
	// index 2 -> RW 0x89DE9A (always true, regardless of source)
	case 0x2E:
	case 0x30:
		return true;
	// index 1 / index 3 -> RW 0x89DEA8 (always false)
	case 0x05:
	case 0x15:
	case 0x16:
	case 0x26:
	case 0x27:
	case 0x2D:
	case 0x2F:
	case 0x3B:
	case 0x43:
	case 0x52:
		return false;
	// index 0 -> RW 0x89DE9E (`cmd->source == CMD_FROM_PLAYER`)
	default:
		return source == CMD_FROM_PLAYER;
	}
}

bool HordeAIUpdate::commandBlockedInPorcupine(int command)
{
	if (command == AIUpdateInterface::kCommandUnidentifiedMove)
	{
		command = 0x00;
	}
	if (command < 0 || command > 0x53)
	{
		return false; // RW 0x89DF1A -> 0x89DF2E
	}
	// RW 0x89DF3F index table -> RW 0x89DF33 jump table: only 0x0B (attack object) and 0x52 (set stance) return false; all other 0x00..0x53 return true
	return command != 0x0B && command != 0x52;
}

bool HordeAIUpdate::filterAcceptedCommand(CommandSourceType source, int command)
{
	if (command == -1)
	{
		return true; // unnumbered internal caller (e.g. HordeFormation::toggleFormation's porcupine-entry aiIdle)
	}
	HordeContainInterface *hc = hordeContainOf(getObject());
	if (!hc || !hc->isPorcupineFormation()) // RW 0x89E1BC .. 0x89E1CC (slot 0xF0)
	{
		return true;
	}
	if (hc->canToggleFormationHorde() && commandTogglesPorcupine(command, source)) // RW 0x89E1CE .. 0x89E1EB (slot 0x5C -> slot 0x60)
	{
		hc->toggleFormationHorde();
		return true;
	}
	return !commandBlockedInPorcupine(command); // RW 0x89E1F9 .. 0x89E205
}

// ---- lane MOVE-2 r2: the command hand-off (RW 0x89E169) ----
bool HordeAIUpdate::commandEndsMemberOrders(int command)
{
	switch (command)
	{
	case 0x00: case 0x01: case 0x03: case 0x04: case 0x06: case 0x07: case 0x08: case 0x09: case 0x0B: case 0x0C: case 0x0E: case 0x0F: case 0x17: case 0x18:
	case 0x24: case 0x32: case 0x33: case 0x34: case 0x36: case 0x38: case 0x41: case 0x42: case 0x4E:
	case AIUpdateInterface::kCommandUnidentifiedMove:
		return true;
	default:
		return false;
	}
}

void HordeAIUpdate::commandAccepted(CommandSourceType source, int command, Object *target)
{
	(void)source;
	// RW 0x89E207 .. 0x89E29A: after the gate (vslot 0x250) and the porcupine filter above (which may have swapped the contain through slot 0x60; re-read here,
	// matching RW 0x89E281), the hand-off commands clear IS_LEAVING_FACTORY (status 0x5A) and call the contain's slot 0x14 (RW 0x87594C) with the command's
	// object (commands 0xB / 0xC) or null unless the horde stands in a container whose contain answers vslot 0x10 (RW 0x89E1AC; INFERENCE S-1501: contained)
	if (!commandEndsMemberOrders(command))
	{
		return;
	}
	Object *horde = getObject();
	if (horde->getContainedBy())
	{
		return;
	}
	HordeContainInterface *hc = hordeContainOf(horde);
	if (!hc)
	{
		return;
	}
	static const int leavingFactory = ObjectTemplateInfoBuilder::objectStatusIndex("IS_LEAVING_FACTORY");
	if (leavingFactory >= 0 && horde->testStatus((unsigned)leavingFactory))
	{
		horde->setStatus((unsigned)leavingFactory, false); // RW 0x62684D(0x5A, 0)
	}
	endMelee(); // slot 0x14 starts with slot 0x138 (RW 0x86C0F9) when the horde melees: the port keeps the melee runtime in this AI
	hc->prepareMembersForCommand(command == 0x0B || command == 0x0C ? target : nullptr);
}
