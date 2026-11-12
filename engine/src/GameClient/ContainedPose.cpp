// OpenBFME. GPL-3.0.
//
// PORT-146 (issue #363): see ContainedPose.h. Client only, plain binary32 arithmetic (the client is not hashed; the values are the ones the entries and
// the bones of the contained drawable are drawn with).

#include "GameClient/ContainedPose.h"

namespace containedpose
{
namespace
{
// row major 3x3 product: out = a * b
void multiply(const float a[9], const float b[9], float out[9])
{
	for (int r = 0; r < 3; ++r)
	{
		for (int c = 0; c < 3; ++c)
		{
			out[r * 3 + c] = a[r * 3 + 0] * b[0 * 3 + c] + a[r * 3 + 1] * b[1 * 3 + c] + a[r * 3 + 2] * b[2 * 3 + c];
		}
	}
}

// the transpose of a rotation basis, which is its inverse (every basis here is a rotation: a yaw, a tilt, or both)
void transpose(const float a[9], float out[9])
{
	for (int r = 0; r < 3; ++r)
	{
		for (int c = 0; c < 3; ++c)
		{
			out[r * 3 + c] = a[c * 3 + r];
		}
	}
}

Coord3D transform(const float basis[9], const Coord3D &v)
{
	return Coord3D{ basis[0] * v.x + basis[1] * v.y + basis[2] * v.z, basis[3] * v.x + basis[4] * v.y + basis[5] * v.z,
		            basis[6] * v.x + basis[7] * v.y + basis[8] * v.z };
}
} // namespace

Pose compose(const Pose &containerPresentation, const Pose &containerLogic, const Pose &riderLogic)
{
	float inverse[9];
	transpose(containerLogic.basis, inverse);
	// the rider's place in the container's own frame: its bone offset and its own turn, with the container's logic pose taken out
	float localBasis[9];
	multiply(inverse, riderLogic.basis, localBasis);
	const Coord3D localPos = transform(inverse, Coord3D{ riderLogic.pos.x - containerLogic.pos.x, riderLogic.pos.y - containerLogic.pos.y,
	                                                     riderLogic.pos.z - containerLogic.pos.z });

	// and drawn with the container's presentation pose instead of its logic one
	Pose out;
	multiply(containerPresentation.basis, localBasis, out.basis);
	const Coord3D offset = transform(containerPresentation.basis, localPos);
	out.pos = Coord3D{ containerPresentation.pos.x + offset.x, containerPresentation.pos.y + offset.y, containerPresentation.pos.z + offset.z };
	return out;
}
} // namespace containedpose
