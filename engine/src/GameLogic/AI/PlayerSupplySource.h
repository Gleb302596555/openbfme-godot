// OpenBFME. GPL-3.0.
// PORT-16, RotWK only: isolated supply-source selection (RW 0x8F6BD2 .. 0x8F6DCF).
// This helper has no runtime caller yet; the Player AI lifecycle, position,
// cached enemy query and enemy structure bounds remain to be wired separately.
#pragma once

#include <cstdint>

class GameLogic;
class Object;
class Player;
struct Coord3D;

namespace PlayerSupplySource
{
// Chooses the closest eligible STRUCTURE + SUPPLY_SOURCE world object for
// `player` relative to `ownPosition` (AI +0x38). When the Player AI's virtual
// enemy query returned an enemy player, pass the midpoint of that enemy's
// team-structure bounds in `enemyMidpoint`; pass nullptr otherwise.
Object *choose(GameLogic &logic, const Player &player, const Coord3D &ownPosition, const Coord3D *enemyMidpoint, std::int32_t minValue);
} // namespace PlayerSupplySource
