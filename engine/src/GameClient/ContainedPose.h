// OpenBFME. GPL-3.0.
//
// PORT-146 (issue #363): the drawn pose of an object that stands on a bone of its container. INFERENCE, stop S-7325: RotWK's client takes that
// drawable's transform from its container's drawable, and the address was not read from the dump (no game.dat in this workspace).
//
// The logic puts a passenger or a crew member on the container's bones (OpenContain::putRidersAtBones, RW 0x868A70): its position is the container's
// LOGIC transform times the bone's offset, and the logic transform has no tilt and no bounce. The client draws the container with a presentation pose on
// top of that (Drawable::applyPhysicsXform, RW 0x67BE90: the terrain tilt of calcPitchRoll RW 0x670570 and the suspension / bounce of
// calcPhysicsXformWheels RW 0x677026 - the pose Drawable::getPresentationPose gives out). An object on a bone has to ride that pose too, as retail
// draws it off the container's drawable (the client part S-1104 records as the drawable bone attachment): without it a mumak's archers float over the
// uphill side and pass through the downhill one on a slope, and the archers on a driving Grond step up and down as its suspension moves (P-82, P-80).
//
// The composition is kept free of the client - a basis and a position in, the same out - so it can be checked without a drawable (tests
// PORT-146*). Client only: nothing here is simulation state, hashed or saved.
#pragma once

#include "Common/INIDataTypes.h"

namespace containedpose
{
// a drawn transform: the object's X, Y, Z axes row by row (the layout Drawable::getPresentationPose writes) and its position
struct Pose
{
	float basis[9] = { 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f };
	Coord3D pos = { 0.0f, 0.0f, 0.0f };
};

// The pose the container's presentation times the rider's place in the container's frame:
//   containerPresentation * (containerLogic^-1 * riderLogic)
// With `containerPresentation` equal to `containerLogic` this returns `riderLogic` unchanged (a flat, still container draws its riders where they
// stood), so only the extra pose the client adds to the container reaches the rider.
Pose compose(const Pose &containerPresentation, const Pose &containerLogic, const Pose &riderLogic);
} // namespace containedpose
