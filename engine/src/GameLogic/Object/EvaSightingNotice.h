// OpenBFME. GPL-3.0.
//
// Local 13 / Issue #134 (TOP20 #2, Stop S-709): the logic half of RotWK's Eva sighting announcements:
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

#include <functional>
#include <string>

class GameLogic;
class Object;
class Player;
class ThingTemplate;
struct Coord3D;

namespace EvaSighting
{
// Find closest object belonging to `localPlayer` within `scanRange` that is alive, not `sightingObj`,
// not INERT_SHROUD_REVEALER (122) and not IGNORE_FOR_EVA_SPEECH_POSITION (158).
// Returns true and writes outPos if found, false if none found (RW 0x672DFB).
bool findClosestLocalObject(const GameLogic &logic, const Object &sightingObj, const Player &localPlayer, float scanRange, Coord3D &outPos);

// Walk the objects belonging to `owner` (RW 0x6ABABD / RW 0x672D90).
// Returns true if another object exists that is:
// - not `obj`
// - not effectively dead
// - not IGNORED_IN_GUI (kindOf 47)
// - has the same EvaEnemyObjectSightedEvent
// - not fully obscured (via `isFullyObscured(id)`)
// - visible to localPlayer as enemy (isVisibleTo)
bool ownerHasAnotherSightedObject(const GameLogic &logic, const Object &obj, const Player &owner, const Player &localPlayer, const std::string &eventName, const std::function<bool(ObjectID)> &isFullyObscured);

// Check if obj has SlavedUpdate with useSlaverAsControlForEvaObjectSightedEvents == true (RW 0x678D1D..0x678D24).
// If so, writes slaverId and returns true.
bool isSlaveControlledBySlaver(const Object &obj, ObjectID &outSlaverId);

// For a slaver object, iterate all spawned slave IDs (RW 0x670983).
void forEachSpawnedSlave(Object &slaverObj, const std::function<void(ObjectID)> &fn);

// Sighting of a castle member (RW 0x678DE7):
// If obj has CastleMemberBehavior, resolves castleId. If castle Object has CastleBehavior with
// EvaEnemyCastleSightedEvent set (and not empty), writes outCastleId and outCastleEvent and returns true.
bool getCastleInfo(Object &obj, ObjectID &outCastleId, std::string &outCastleEvent);

// Iterate all member IDs of a castle (keep, foundations, members, crew, defense foundations, and castle itself) (RW 0x6709EA).
void forEachCastleMember(Object &castleObj, const std::function<void(ObjectID)> &fn);
} // namespace EvaSighting
