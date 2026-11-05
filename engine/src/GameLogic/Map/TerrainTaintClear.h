// OpenBFME. GPL-3.0.
//
// TerrainTaintClear (stop S-5260's radius clear; TaintManager.h): TheTerrainLogic's two clears of the Taint / Elven Wood ground.
//
// TARGET FACTS (RotWK game.dat, caveat S-001; read in this lane):
//   * the radius clear RW 0x683393 (TheTerrainLogic RW 0xDE4690, thiscall (position, radius, owner)): nothing without a position, TheTaintManager (RW 0xDE435C),
//     ThePartitionManager (RW 0xDE4354) or TheGameLogic (RW 0xDE412C); then
//       - TheTaintManager paint (RW 0xAD4C10) of 0x80 with `set` and the owner (RW 0x6833D8 .. 0x6833EA): the cells go back to neutral, an owner's only its own;
//       - every object of KindOf TAINTEFFECT (0xAF: RW 0x444D39(0, 0xAF) / 0x445139) within the radius (RW 0xA39340: FROM_CENTER_2D, ITER_SORTED_NEAR_TO_FAR)
//         is destroyed (RW 0x62BBAB) in the iterator's order;
//       - the trees within the radius go (RW 0x67F63F: TerrainTrees::removeTreesInRadius);
//       - with TheTerrainVisual (RW 0xDE4AC8), its slot 0x5C (RW 0x46947C: the scorch marks within the radius are hidden) with a copy of the position.
//   * the castle's clear RW 0x683955 (position, radius), from the castle's unpack (RW 0x79BF1B .. 0x79BF36, after either form, with MaxCastleRadius RW
//     0x797ED7: the castle data's + 0x2C, 0 without data, at the castle's position): the same guards; every object of KindOf TAINT (0x98) within the
//     radius (FROM_CENTER_2D, ITER_SORTED_NEAR_TO_FAR) gets the radius clear at ITS position with the castle's radius and owner -1, then is destroyed; then
//     the radius clear at the position with owner -1.
//   * AttributeModifierAuraUpdate's destructor (RW 0x89EE67): an object whose template is KindOf TAINT (template + 0x11B bit 0) gets the radius clear at its
//     position (+ 0x38) with the module's Range (data + 0x1C) and its own id (+ 0x74) as the owner (HeroModules.cpp, onDelete).
// INFERENCE (S-5261): the slot 0x5C call is a client event (TerrainTrees::Event::HIDE_SCORCHES), read with the tree events.

#pragma once

#include "Common/INIDataTypes.h"

#include <cstdint>

class GameLogic;

namespace TerrainTaintClear
{
// RW 0x683393; the number of TAINTEFFECT objects destroyed
int clearRadius(GameLogic &logic, const Coord3D *position, float radius, std::int32_t owner);
// RW 0x683955; the number of TAINT objects destroyed
int clearForCastle(GameLogic &logic, const Coord3D *position, float radius);
} // namespace TerrainTaintClear
