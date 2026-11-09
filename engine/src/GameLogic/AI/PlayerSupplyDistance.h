// OpenBFME. GPL-3.0.
// PORT-16 prerequisite, RotWK only: numeric part of Player AI's supply-source
// choice (RW 0x8F6D3B .. 0x8F6DA4). No selection or script calls use it yet.
#pragma once

#include "Common/INIDataTypes.h"
#include "GameLogic/SimMath.h"

namespace PlayerSupplyDistance
{
// SSE addss/mulss; the order is y squared plus x squared, not vice versa.
inline float ownSquared(const Coord3D &candidate, const Coord3D &own)
{
	const float x = SimMath::sseSub(candidate.x, own.x);
	const float y = SimMath::sseSub(candidate.y, own.y);
	return SimMath::sseAdd(SimMath::sseMul(y, y), SimMath::sseMul(x, x));
}

// The caller runs this only when the AI's virtual enemy-player query returns
// a player. x87 operations retain their wide exponent through the comparison.
// These operands are all float-derived, so the pc24*W binary64 carrier is wide
// enough for this chain. 0.4 is binary64, whereas 0.6 is loaded as binary32.
inline bool rejectByEnemy(const Coord3D &candidate, const Coord3D &enemy, float ownDistanceSquared)
{
	const double x = SimMath::pc24SubW((double)candidate.x, (double)enemy.x);
	const double y = SimMath::pc24SubW((double)candidate.y, (double)enemy.y);
	const double own = SimMath::pc24MulW((double)ownDistanceSquared, 0.4);
	const double yy = SimMath::pc24MulW(y, y);
	const double xx = SimMath::pc24MulW(x, x);
	const double enemyDistance = SimMath::pc24AddW(yy, xx);
	const double theirs = SimMath::pc24MulW(enemyDistance, (double)0.6f);
	return own > theirs; // fcompi; ja: unordered is not rejected
}

// RW 0x8F6D9C .. 0x8F6DA4 uses COMISS / JBE: an equal or unordered
// distance does not replace the earlier candidate. Only call with a previous candidate.
inline bool closerThan(float distanceSquared, float previousSquared)
{
	return previousSquared > distanceSquared;
}
} // namespace PlayerSupplyDistance
