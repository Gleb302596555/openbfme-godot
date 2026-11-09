// OpenBFME. GPL-3.0.
// The model draw's shared matrix hook: RW 0x4B686D assembles the matrix and calls
// vtable +0x104 at RW 0x4B6B26, before the later shared transform adjustments.
// PARTIAL (S-112): RW reaches that call only after a bone lookup succeeds (RW
// 0x4B6A77 / 0x4B6AA2). The port has no cross-module placement yet, so the
// renderer supplies its ordinary object matrix; this is not exact bone parity.
#pragma once

#include "Common/INIDataTypes.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DScriptedModelDraw.h"
#include "Libraries/WWVegas/WWMath/matrix3d.h"

#include <array>

inline Matrix3D W3DModelDrawMatrix(W3DScriptedModelDraw &draw, const float *basis, const Coord3D &pos, float scale)
{
	Matrix3D matrix;
	for (int r = 0; r < 3; ++r)
	{
		for (int c = 0; c < 3; ++c)
		{
			matrix.Row[r][c] = basis[r * 3 + c] * scale;
		}
	}
	matrix.Row[0][3] = pos.x;
	matrix.Row[1][3] = pos.y;
	matrix.Row[2][3] = pos.z;
	draw.adjustModelMatrix(matrix); // the ONLY shared +0x104 dispatch; base is a no-op (RW 0x9F3A3C)
	return matrix;
}

inline std::array<float, 9> W3DModelDrawBasis(const Matrix3D &matrix)
{
	std::array<float, 9> basis{};
	for (int r = 0; r < 3; ++r)
	{
		for (int c = 0; c < 3; ++c)
		{
			basis[r * 3 + c] = matrix.Row[r][c];
		}
	}
	return basis;
}
