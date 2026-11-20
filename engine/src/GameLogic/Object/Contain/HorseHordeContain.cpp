// OpenBFME. GPL-3.0.
//
// HorseHordeContain (Port 283, issue #650): cavalry horde contain, ported from RotWK 2.01 game.dat.
//
// TARGET FACTS (RotWK 2.01 game.dat, all 17 functions in RW_FUNCTION_MAP.tsv):
//   * Factory RW 0x64B664 (alloc 0x30C bytes, same size as HordeContain: zero extra instance fields) -> ctor RW 0x879849 (HordeContain ctor RW 0x8727F9,
//     then installs vtables +0x00 = RW 0xC5C288, +0x0C = RW 0xC5C27C, +0x10 = RW 0xC5C270, +0x20 = RW 0xC5C110, +0x34 = RW 0xC5C100, +0x11C = RW 0xC5BE98);
//     dtor RW 0x8798B7 / scalar deleting dtor RW 0x879991; getClassName RW 0x8798B1; getModuleKey RW 0x87994C; module data factory RW 0x65330B (alloc 0x284
//     bytes -> HordeContainModuleData ctor RW 0x870396, inherited dtor RW 0x63F3BF, no added INI rows).
//   * Snapshot xfer RW 0x87992C (+0x00 slot 3): version 1 (RW 0x404F15), then HordeContain::xfer (RW 0x8780E4); the port's native snapshot also persists the
//     subclass's reform latch ([this + 0x121]) so a mid-reform / mid-factory-exit save resumes identically.
//   * Adjustor forwarding thunks: RW 0x87D917 (+0x00 getInterfaceMask -> RW 0x86C8F2), RW 0x879947 (+0x10 crc -> RW 0x873FE8), RW 0x879927 (+0x10 xfer ->
//     RW 0x8759FF), RW 0x879922 (+0x10 loadPostProcess -> RW 0x86EBF4), RW 0x87D91C (-> RW 0x86CCB0).
//   * Live virtual overrides over HordeContain:
//     - ContainModuleInterface (+0x20) slot 0xB4 isPassengerAllowedToFire: RW 0x9188EB (`xor al, al; ret` — always false, overriding RW 0x86C713);
//     - HordeContainInterface (+0x11C) slot 0x3C beginReform: RW 0x8799AD (if the horde's curLocomotor has speed > 0, sets speed to 0);
//     - HordeContainInterface (+0x11C) slot 0x1C0 isFormationReady(angle): RW 0x8799CF (caches maxTurnRate RW 0x5E372D from the first contained member with
//       a locomotor while the cache is 0; returns false on the first member with |normalizeAngle(angle - member.orientation)| > maxTurnRate, else true);
//     - Primary (+0x00) slot 0x90 memberOrder: RW 0x879FDB (cavalry wheel-before-walk, unconditional ahead-of-slot wait when ![this + 0x121], melee delegates
//       to HordeContain::moveHub RW 0x87468B, formation walk calls horseUpdatePosition RW 0x879A83).
//   * Verified zero-caller virtual slots in RotWK 2.01 (dead across all of game.dat): +0x00 slot 0x9C RW 0x9F3A3C, slot 0xA0 RW 0x87990E, slot 0xA4
//     RW 0x7FEAC1, slot 0xA8 RW 0x879A72, and +0x11C slot 0x08 RW 0x87545C.
//   * horseUpdatePosition RW 0x879A83: sets [this + 0x121] while the horde is in AI state 7 (AI_FOLLOW_EXITPRODUCTION_PATH); clears MOVING on physics-disabled
//     members; syncs busy / idle (when the horde's state is active, idles non-idle non-active riders so mounted archers keep firing on the move instead of
//     being made busy); snaps/turns in the near arm and clears DECELERATE (136) when parked; in the far arm skips tiny stationary nudges (speed == 0 &&
//     accel > dist), issues hordeMemberMoveTo + setPathExtraDistance + setDesiredSpeed(dist), sets MOVING, clears ACCELERATE (135) and drives DECELERATE (136)
//     with threshold 0.66f * maxSpeed (RW 0xC0F1C4).

#include "GameLogic/Object/Contain/HordeContainRuntime.h"

#include "Common/StateHash.h"
#include "Common/Xfer.h"
#include "GameLogic/AI/AIMove.h"
#include "GameLogic/AI/AIPathfind.h"
#include "GameLogic/AI/AIStateMachine.h"
#include "GameLogic/AI/AIWorld.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Object/Contain/HordeContainCore.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/ObjectTemplateInfo.h"
#include "GameLogic/SimMath.h"

namespace
{
const float kPi = 3.14159274f;
const float kTwoPi = 6.28318548f;
const float kQuarterSpeed = 0.25f;         // RW 0xBD1904 (RW 0x5E4CA7 isFasterThanQuarterSpeed)
const float kHalfTurnRate = 0.5f;          // RW 0xBD869C (RW 0x87A167)
const float kSpeedFraction = 0.2f;         // RW 0xBDAD78 (RW 0x879C69)
const float kTurnThreshold = 0.174532935f; // RW 0xC5B69C (RW 0x879D51, ~10 degrees)
const float kDecelSpeedFrac = 0.660000026f; // RW 0xC0F1C4 (RW 0x879EEF)

float absF32(float a)
{
	return a < 0.0f ? SimMath::subf32(0.0f, a) : a;
}

// RW 0x644FD0 normalizeAngle into (-pi, pi]
float normalizeAngle(float a)
{
	if (a > kPi)
	{
		do
		{
			a = SimMath::subf32(a, kTwoPi);
		} while (a > kPi);
	}
	if (-kPi < a)
	{
		return a;
	}
	do
	{
		a = SimMath::addf32(a, kTwoPi);
	} while (-kPi >= a);
	return a;
}

struct HorseBits
{
	int moving = AIUpdateInterface::modelConditionBit("MOVING");           // condition 0x3D ([obj+0x110] bit 29)
	int accelerate = AIUpdateInterface::modelConditionBit("ACCELERATE");   // condition 0x87 ([obj+0x11C] bit 7)
	int decelerate = AIUpdateInterface::modelConditionBit("DECELERATE");   // condition 0x88 ([obj+0x11C] bit 8)
	int runningDown = ObjectTemplateInfoBuilder::objectStatusIndex("RUNNING_DOWN_FROM_BEHIND"); // status 0x4B
};

const HorseBits &horseBits()
{
	static const HorseBits b;
	return b;
}
} // namespace

// RW 0x64B664 -> RW 0x879849 (data RW 0x65330B)
HorseHordeContain::HorseHordeContain(Thing *thing, const HordeContainBehaviorData *data)
	: HordeContain(thing, data)
{
}

// RW 0x8798ED / scalar RW 0x879893
HorseHordeContain::~HorseHordeContain() = default;

// RW 0x9188EB (+0x20 slot 0xB4: `xor al, al; ret`)
bool HorseHordeContain::isPassengerAllowedToFire() const
{
	return false;
}

// RW 0x8799AD (+0x11C slot 0x3C): stops the horde object's locomotor when it has positive forward speed before a formation reform
void HorseHordeContain::beginReform()
{
	Object *horde = getObject();
	AIUpdateInterface *ai = horde ? horde->getAIUpdateInterface() : nullptr;
	Locomotor *loco = ai ? ai->curLocomotor() : nullptr;
	if (loco && loco->speed() > 0.0f) // RW 0x8799BC .. 0x8799C7 (RW 0x5E36F7 > 0 -> RW 0x5E3721(0))
	{
		loco->setSpeed(0.0f);
	}
}

void HorseHordeContain::endReform()
{
	HordeContain::endReform();
	m_horseReforming = true; // RW 0x8780B0: [this + 0x121] = 1
}

// RW 0x8799CF (+0x11C slot 0x1C0): true when every contained rider's heading sits within one turn-step (maxTurnRate) of `angle`
bool HorseHordeContain::isFormationReady(float angle) const
{
	float maxTurnRate = 0.0f;
	for (const Object *m : containedMembers())
	{
		if (!m)
		{
			continue;
		}
		if (maxTurnRate == 0.0f) // RW 0x8799FD .. 0x879A22: lazy cache from the first rider that has a locomotor
		{
			AIUpdateInterface *mai = const_cast<Object *>(m)->getAIUpdateInterface();
			Locomotor *loco = mai ? mai->curLocomotor() : nullptr;
			if (loco)
			{
				maxTurnRate = loco->getMaxTurnRate(mai->locomotorHost()); // RW 0x5E372D
			}
		}
		const float diff = absF32(normalizeAngle(SimMath::subf32(angle, m->getOrientation()))); // RW 0x879A24 .. 0x879A43
		if (diff > maxTurnRate) // RW 0x879A46 .. 0x879A4B
		{
			return false;
		}
	}
	return true;
}

// RW 0x879FDB (+0x00 slot 0x90)
void HorseHordeContain::memberOrder(Object &member, const Coord3D &slotPos, float orientation, bool force)
{
	Object *horde = getObject();
	AIUpdateInterface *a = member.getAIUpdateInterface();
	AIUpdateInterface *b = horde ? horde->getAIUpdateInterface() : nullptr;
	if (a && a->locomotorHost().physicsMotionDisabled()) // RW 0x879FEC .. 0x87A00F ([member + 0x264]->[+0x5C] != 0)
	{
		return;
	}
	if (member.isEffectivelyDead() || !b || !a) // RW 0x87A015 .. 0x87A02C
	{
		return;
	}
	++mutablePassStats().orders;
	const bool ownerMoving = b->isMoving();
	const HorseBits &hb = horseBits();

	// RW 0x87A032 .. 0x87A091: active rider stands down from slot movement unless the horde moves and RUNNING_DOWN_FROM_BEHIND (0x4B) is set
	// (no leash order, no SCALING_WALL_HORDE clearing and no MACHINE teleport on HorseHordeContain)
	if (a->isStateActive())
	{
		const bool chase = hb.runningDown >= 0 && member.testStatus((unsigned)hb.runningDown);
		if (!ownerMoving || !chase)
		{
			if (a->isMoving())
			{
				a->mover().endingMove(); // RW 0x6627CB
			}
			if (hb.moving >= 0)
			{
				member.setModelConditionState(hb.moving, false);
			}
			a->setLocomotorGoalNone(); // AI vslot 0x220
			++mutablePassStats().attackHolds;
			return;
		}
	}

	// RW 0x87A096 .. 0x87A0D3: melee engages HordeContain::moveHub (RW 0x87468B) directly unless walking an explicit-Z portal path
	if (meleeEngagedFlag())
	{
		if (a->mover().goalType() == AIGOAL_EXPLICIT_WITH_PATH && a->mover().path() && a->mover().path()->hasExplicitZ())
		{
			setWorkDone();
			return;
		}
		moveHub(member, slotPos, orientation);
		return;
	}

	// RW 0x87A0D8 .. 0x87A0E8: keep following an active obstacle-avoidance path
	if (a->mover().goalType() == AIGOAL_EXPLICIT_WITH_PATH && a->mover().path())
	{
		setWorkDone();
		return;
	}

	// RW 0x87A0EE .. 0x87A1AF: unless the horde exceeds quarter speed (RW 0x5E4CA7) or this member is the banner carrier and the horde sits ahead of
	// the carrier's slot, a rider more than 0.5 * maxTurnRate off `orientation` wheels by one clamped turn step before walking
	Locomotor *hordeLoco = b->curLocomotor();
	const bool hordeFast =
		hordeLoco != nullptr && hordeLoco->speed() > SimMath::mulf32(hordeLoco->getMaxSpeedForCondition(b->locomotorHost()), kQuarterSpeed);
	const bool bannerBehindHorde = bannerCarrierID() == member.getID() && isMemberAheadOfSlot(*horde, slotPos);
	if (!hordeFast && !bannerBehindHorde)
	{
		if (Locomotor *memberLoco = a->curLocomotor())
		{
			const float curAngle = member.getOrientation();
			const float rate = memberLoco->getMaxTurnRate(a->locomotorHost()); // RW 0x5E372D
			const float diff = normalizeAngle(SimMath::subf32(orientation, curAngle));
			if (absF32(diff) > SimMath::mulf32(rate, kHalfTurnRate)) // RW 0x87A164 .. 0x87A175
			{
				const float negRate = SimMath::subf32(0.0f, rate);
				float step = diff;
				if (step > rate)
				{
					step = rate;
				}
				else if (negRate > step)
				{
					step = negRate;
				}
				a->setLocomotorGoalOrientation(SimMath::addf32(curAngle, step)); // RW 0x87A19C .. 0x87A1A9 (AI vslot 0x21C)
				a->wakeUpNow();
				setWorkDone();
				++mutablePassStats().turns;
				return;
			}
		}
	}

	// RW 0x87A1B1 .. 0x87A22B: ahead-of-slot rider stops and waits regardless of UseSlowHordeMovement unless forced, reforming ([this + 0x121]) or backing up
	if (!force && !m_horseReforming && isMemberAheadOfSlot(member, slotPos) && backUpEmpty() && (ownerMoving || slotCellClear(member)))
	{
		if (a->mover().isMoving())
		{
			a->mover().endingMove(); // RW 0x6627CB
		}
		if (hb.moving >= 0)
		{
			member.setModelConditionState(hb.moving, false);
		}
		a->setLocomotorGoalNone(); // AI vslot 0x220
		setWorkDone();             // RW 0x87A224: [this + 0x120] = 1
		++mutablePassStats().waits;
		return;
	}

	// RW 0x87A22D .. 0x87A27A: adjust destination (RW 0x877CB0) and drive the rider (RW 0x879A83)
	Coord3D dest = slotPos;
	adjustMemberDestination(member, dest);
	horseUpdatePosition(member, dest, orientation);
}

// RW 0x879A83
void HorseHordeContain::horseUpdatePosition(Object &member, const Coord3D &destIn, float orientation)
{
	Object *horde = getObject();
	AIUpdateInterface *a = member.getAIUpdateInterface();
	AIUpdateInterface *b = horde ? horde->getAIUpdateInterface() : nullptr;
	if (!b || !a)
	{
		return;
	}
	// RW 0x879AF7 .. 0x879B08: while the horde runs AI_FOLLOW_EXITPRODUCTION_PATH (state 7), hold [this + 0x121] so ahead riders keep moving out
	if (b->currentStateId() == AI_FOLLOW_EXITPRODUCTION_PATH)
	{
		m_horseReforming = true;
	}
	Locomotor *l = a->curLocomotor();
	Locomotor *k = b->curLocomotor();
	if (!l || !k)
	{
		return;
	}
	const HorseBits &hb = horseBits();
	// RW 0x879B2D .. 0x879B4B: physics-disabled rider clears MOVING and receives no movement order
	if (a->locomotorHost().physicsMotionDisabled())
	{
		if (hb.moving >= 0 && member.testModelCondition(hb.moving))
		{
			member.setModelConditionState(hb.moving, false);
		}
		return;
	}

	const Coord3D oldPosition = *member.getPosition();
	const float dx = SimMath::subf32(destIn.x, oldPosition.x);
	const float dy = SimMath::subf32(destIn.y, oldPosition.y);
	const float distance = SimMath::length2d(dx, dy); // RW 0x879B50 .. 0x879B84

	// RW 0x879B87 .. 0x879BB8: extra path distance (remaining + distance) - hordeLoco->speed()
	float extraDist = distance;
	if (LocomotorPath *path = b->mover().path())
	{
		const LocomotorPathPoint ahead = path->computePointAhead(k->speed());
		const float remaining = path->remainingDistanceFrom(ahead);
		extraDist = SimMath::subf32(SimMath::addf32(remaining, distance), k->speed());
	}

	const bool ownerMoving = b->isMoving();

	// RW 0x879BBB .. 0x879C4C: busy / idle sync. Unlike HordeContain::moveHub, when !melee and the horde's state is active (RW 0x879C00 .. 0x879C08),
	// a rider neither idle nor active gets aiIdle (not aiBusy) so mounted archers stay free to acquire/fire while marching.
	if (meleeEngagedFlag())
	{
		if (!a->isIdle() && !a->isStateActive())
		{
			a->aiIdle(CMD_FROM_AI);
		}
	}
	else if ((!a->isStateActive() || !b->isStateActive()) && (ownerMoving || a->isIdle()))
	{
		if (b->isStateActive())
		{
			if (!a->isIdle() && !a->isStateActive())
			{
				a->aiIdle(CMD_FROM_AI); // RW 0x879C0A .. 0x879C30
			}
		}
		else if (!a->isBusy())
		{
			a->aiBusy(CMD_FROM_AI); // RW 0x879C32 .. 0x879C47
			++mutablePassStats().busyOrders;
		}
	}

	// RW 0x879C4C .. 0x879C94: snap threshold min(accel, 0.2f * maxSpeed)
	LocomotorHost &host = a->locomotorHost();
	const float maxSpeed = l->getMaxSpeedForCondition(host);
	const float accel = l->getMaxAccelerationForSpeed(maxSpeed);
	float minDist = accel;
	const float speedLimit = SimMath::mulf32(maxSpeed, kSpeedFraction);
	if (minDist > speedLimit)
	{
		minDist = speedLimit;
	}

	AIWorld &world = a->world();
	Pathfinder &pf = world.pathfinder();
	if (minDist > distance) // RW 0x879C90: comiss minDist, distance; jbe far_arm
	{
		// ---- near arm (RW 0x879C9A .. 0x879DF2) ----
		Coord3D q = destIn;
		if (l->getTemplate().m_zAxisBehavior != Z_FLOATING_Z) // RW 0x879CBE: template + 0x70 != 8
		{
			q.z = oldPosition.z;
			if (world.mapReady() && pf.validMovementPosition(&a->adapter(), a->locomotorInfo(), a->adapter().getLayer(), &q))
			{
				q.z = a->layerHeightAt(q.x, q.y);
			}
		}
		const float angleDiff = absF32(normalizeAngle(SimMath::subf32(orientation, member.getOrientation())));
		if (!meleeEngagedFlag() && angleDiff > kTurnThreshold) // RW 0x879D3D .. 0x879D79 (no DOZER snap branch)
		{
			a->setLocomotorGoalOrientation(orientation);
			a->wakeUpNow();
			setWorkDone();
			++mutablePassStats().turns;
			return;
		}
		if (hb.moving >= 0 && member.testModelCondition(hb.moving))
		{
			member.setModelConditionState(hb.moving, false); // RW 0x879D80 .. 0x879D92
		}
		a->setLocomotorGoalNone(); // RW 0x879D97 .. 0x879D9B
		++mutablePassStats().snaps;
		if (ownerMoving) // RW 0x879DA1 .. 0x879DAB
		{
			setWorkDone();
			return;
		}
		member.setPosition(&q); // RW 0x879DAD .. 0x879DF2
		q = *member.getPosition();
		host.setPendingPosition(q);
		if (world.mapReady())
		{
			pf.updateGoal(a->adapter(), member.getPosition(), a->adapter().getLayer());
		}
		if (hb.decelerate >= 0 && member.testModelCondition(hb.decelerate))
		{
			member.setModelConditionState(hb.decelerate, false); // RW 0x879DDE .. 0x879DF2: clear DECELERATE (0x88) once settled
		}
		return;
	}

	// ---- far arm (RW 0x879DF7 .. 0x879FD4) ----
	const float curSpeed = l->speed(); // RW 0x879DFA (RW 0x5E36F7)
	if (curSpeed == 0.0f && accel > distance) // RW 0x879E0E .. 0x879E26: ignore sub-acceleration stationary drift
	{
		return;
	}
	setWorkDone(); // RW 0x879E3E: [this + 0x120] = 1
	a->mover().ignoreObstacle(horde->getProducerID());
	a->hordeMemberMoveTo(destIn);      // RW 0x879E59: AI vslot 0x214
	a->setPathExtraDistance(extraDist); // RW 0x879E68: RW 0x6631AE
	a->setDesiredSpeed(distance);       // RW 0x879E76: RW 0x6625DA
	if (hb.moving >= 0)
	{
		member.setModelConditionState(hb.moving, true); // RW 0x879E7B .. 0x879E90
	}
	if (hb.accelerate >= 0 && member.testModelCondition(hb.accelerate))
	{
		member.setModelConditionState(hb.accelerate, false); // RW 0x879E95 .. 0x879EAB: clear ACCELERATE (0x87)
	}
	if (hb.decelerate >= 0)
	{
		// RW 0x879EB0 .. 0x879FD4: DECELERATE (0x88) triggers when braking below 0.66 * maxSpeed and the horde itself is decelerating or parked, and stays
		// latched while speed remains positive under the same horde condition; otherwise it is cleared.
		const bool hordeDecelOk = horde->testModelCondition(hb.decelerate) || !ownerMoving;
		const bool keepDecel = member.testModelCondition(hb.decelerate) && hordeDecelOk;
		const float thresh = SimMath::mulf32(maxSpeed, kDecelSpeedFrac); // RW 0x879EEF (0.66f, RW 0xC0F1C4)
		const bool wantDecel = (curSpeed > distance && thresh > curSpeed && hordeDecelOk) || (keepDecel && curSpeed > 0.0f);
		if (member.testModelCondition(hb.decelerate) != wantDecel)
		{
			member.setModelConditionState(hb.decelerate, wantDecel);
		}
	}
	++mutablePassStats().walks;
}

void HorseHordeContain::crc(StateHasher &hasher) const
{
	HordeContain::crc(hasher);
	hasher.addBool(m_horseReforming);
}

// RW 0x87992C (+0x00 slot 3: version 1, then HordeContain::xfer RW 0x871D34); also persists m_horseReforming ([this + 0x121])
void HorseHordeContain::xfer(Xfer &x)
{
	XferVersion v{ 1, 1 };
	x.xferVersion(v);
	HordeContain::xfer(x);
	x.xferBool(&m_horseReforming);
}
