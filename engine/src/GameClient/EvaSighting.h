// OpenBFME. GPL-3.0.
//
// Local 13 / Issue #134 (TOP20 #2, Stop S-709): the client half of RotWK's Eva sighting announcements:
//   Object/EvaEnemyObjectSightedEvent (+0x5AC), EvaEnemyObjectSightedAfterRespawnEvent (+0x5B0),
//   EvaEnemyCastleSightedEvent (CastleBehaviorModuleData +0x4C), and
//   EvaOnFirstSightingEventEnemy (+0x5B4) / NonEnemy (+0x5B8).
//
// RotWK sources (game.dat):
//   RW 0x678F54: Drawable::setFullyObscuredByShroud calls RW 0x678C69 when obscured changes 1 -> 0
//   RW 0x678C69: logicFrame > 5 gate, Object::isVisibleTo(localPlayer, true) gate (RW 0x694CCC)
//   RW 0x672DFB: find closest local object using PartitionManager::getClosestObject within EnemySightedMaxVoicePositionScanRange
//   RW 0x672D90 / 0x6ABABD: owner walk to check if another visible unobscured unit with same event exists
//   RW 0x6709A2: mark slaver and all slave drawables (RW 0x670983)
//   RW 0x678DE7: CastleMemberBehavior / CastleBehavior sighting
//   RW 0x67561E: Object::onDie / Drawable::notifyDead sets m_evaEnemySightedAfterRespawn = 1
//
// Both BFME2 1.06 and RotWK 2.01 have these sighting mechanisms ("both").

#pragma once

#include "Common/GameCommon.h"
#include "GameLogic/ObjectTypes.h"

#include <cstdint>
#include <string>

class DrawableManager;
class GameLogic;
struct LogicSnapshot;

namespace EvaSighting
{
struct Stats
{
	std::uint64_t sightings = 0;                  ///< transitions out of shroud (takeShroudRevealed)
	std::uint64_t enemySightedReported = 0;        ///< EvaEnemyObjectSightedEvent reports
	std::uint64_t respawnSightedReported = 0;      ///< EvaEnemyObjectSightedAfterRespawnEvent reports
	std::uint64_t castleSightedReported = 0;       ///< EvaEnemyCastleSightedEvent reports
	std::uint64_t firstSightingEnemyReported = 0;  ///< EvaOnFirstSightingEventEnemy reports
	std::uint64_t firstSightingNonEnemyReported = 0;///< EvaOnFirstSightingEventNonEnemy reports
	std::uint64_t ownerSilenced = 0;               ///< silenced by owner walk (RW 0x672D90)
	std::uint64_t voiceSuppressedNoFriendly = 0;   ///< voice announcement suppressed: no friendly object within scan range (RW 0x672DFB)
	std::string lastEvent;                         ///< last reported event name
};

// Check shroud reveal transitions for all drawables and run RW 0x678C69.
// Called at the end of DrawableManager::syncTransforms.
void update(DrawableManager &drawables, const LogicSnapshot &snapshot, GameLogic &logic, Stats &stats);
} // namespace EvaSighting
