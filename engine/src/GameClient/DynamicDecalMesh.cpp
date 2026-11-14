// OpenBFME. GPL-3.0. See DynamicDecalMesh.h.

#include "GameClient/DynamicDecalMesh.h"

#include <algorithm>
#include <cmath>

namespace
{

bool cellFlipped(const TerrainScorchBuffer::Terrain &t, int x, int y)
{
	if (x < 0 || y < 0 || x >= t.xExtent || y >= t.yExtent)
	{
		return false;
	}
	const size_t idx = (size_t)y * (size_t)t.xExtent + (size_t)x;
	return idx < t.flip.size() && t.flip[idx] != 0;
}

float cellHeight(const TerrainScorchBuffer::Terrain &t, int x, int y)
{
	const int cx = std::max(0, std::min(x, t.xExtent - 1));
	const int cy = std::max(0, std::min(y, t.yExtent - 1));
	return (float)t.heights[(size_t)cy * (size_t)t.xExtent + (size_t)cx] * DynamicDecalMesh::kHeightScale;
}

} // namespace

void BuildDynamicDecalMesh(
	const std::vector<const DecalFx *> &decals,
	std::uint32_t clientFrame,
	const TerrainScorchBuffer::Terrain *terrain,
	const std::function<float(float, float)> &fallbackGroundHeight,
	DynamicDecalMesh &out)
{
	out.clear();
	const bool useTerrain = terrain != nullptr && terrain->valid();
	const float invCell = 1.0f / DynamicDecalMesh::kCellWorldSize;

	for (const DecalFx *d : decals)
	{
		if (!d || !(d->desc.size > 0.0f))
		{
			continue; // RW 0x50C276 .. 0x50C2DE
		}
		const int opacity = d->opacityAt(clientFrame);
		if (opacity <= 0)
		{
			continue;
		}

		// RW 0x508E7A (decalAxesRva00108E7A) on Rotate_Z(yaw): uVector = (cos(yaw), sin(yaw), 0), vVector = (sin(yaw), -cos(yaw), 0)
		float ux = std::cos(d->desc.yaw);
		float uy = std::sin(d->desc.yaw);
		const float uLen2 = ux * ux + uy * uy;
		if (uLen2 > 0.0f)
		{
			const float invLen = 1.0f / std::sqrt(uLen2);
			ux *= invLen;
			uy *= invLen;
		}
		else
		{
			ux = 1.0f;
			uy = 0.0f;
		}
		const float vx = uy;
		const float vy = -ux;

		// RW 0x508FBD (projectRangesRva00108FBD): world X/Y min/max of the four rotated decal corners
		const float halfSize = d->desc.size * 0.5f;
		const float halfX = halfSize * (std::fabs(ux) + std::fabs(vx));
		const float halfY = halfSize * (std::fabs(uy) + std::fabs(vy));
		const float xMin = d->desc.position.x - halfX;
		const float xMax = d->desc.position.x + halfX;
		const float yMin = d->desc.position.y - halfY;
		const float yMax = d->desc.position.y + halfY;

		// RW 0x50CE01 .. 0x50CEEB: heightmap cell range [floor(min * 0.1) + border .. ceil(max * 0.1) + border]
		const int border = useTerrain ? terrain->border : 0;
		int startX = (int)std::floor(xMin * invCell) + border;
		int endX = (int)std::ceil(xMax * invCell) + border;
		int startY = (int)std::floor(yMin * invCell) + border;
		int endY = (int)std::ceil(yMax * invCell) + border;
		if (useTerrain)
		{
			startX = std::max(0, std::min(startX, terrain->xExtent - 1));
			endX = std::max(0, std::min(endX, terrain->xExtent - 1));
			startY = std::max(0, std::min(startY, terrain->yExtent - 1));
			endY = std::max(0, std::min(endY, terrain->yExtent - 1));
		}

		// RW 0x50CF14 .. 0x50CF88: symmetric trim to at most 104 vertices per axis
		const int extraX = (endX - startX + 1) - DynamicDecalMesh::kMaxVertsPerAxis;
		if (extraX > 0)
		{
			const int half = extraX / 2;
			startX += half;
			endX -= extraX - half;
		}
		const int extraY = (endY - startY + 1) - DynamicDecalMesh::kMaxVertsPerAxis;
		if (extraY > 0)
		{
			const int half = extraY / 2;
			startY += half;
			endY -= extraY - half;
		}
		if (endX <= startX || endY <= startY)
		{
			continue;
		}

		// RW 0x50C6D0 .. 0x50C729: planar UV basis centered on (pos.x, pos.y)
		const float invSize = 1.0f / d->desc.size;
		const float uVecX = ux * invSize;
		const float uVecY = uy * invSize;
		const float vVecX = vx * invSize;
		const float vVecY = vy * invSize;

		const float r = (float)((d->desc.color >> 16) & 0xFF) / 255.0f;
		const float g = (float)((d->desc.color >> 8) & 0xFF) / 255.0f;
		const float b = (float)(d->desc.color & 0xFF) / 255.0f;
		const float a = (float)opacity / 255.0f;

		const int vertsPerRow = endX - startX + 1;
		const std::int32_t base = (std::int32_t)out.vertices.size();
		out.vertices.reserve(out.vertices.size() + (size_t)vertsPerRow * (size_t)(endY - startY + 1));
		out.indices.reserve(out.indices.size() + (size_t)(endX - startX) * (size_t)(endY - startY) * 6u);

		// RW 0x50D10F .. 0x50D25B
		for (int j = startY; j <= endY; ++j)
		{
			const float wy = (float)(j - border) * DynamicDecalMesh::kCellWorldSize;
			const float dy = wy - d->desc.position.y;
			for (int i = startX; i <= endX; ++i)
			{
				const float wx = (float)(i - border) * DynamicDecalMesh::kCellWorldSize;
				const float dx = wx - d->desc.position.x;
				const float wz = useTerrain ? cellHeight(*terrain, i, j)
											: (fallbackGroundHeight ? fallbackGroundHeight(wx, wy) : d->desc.position.z);
				DynamicDecalVertex vert;
				vert.position = Coord3D{ wx, wy, wz };
				vert.u = uVecX * dx + uVecY * dy + 0.5f;
				vert.v = vVecX * dx + vVecY * dy + 0.5f;
				vert.r = r;
				vert.g = g;
				vert.b = b;
				vert.a = a;
				out.vertices.push_back(vert);
			}
		}

		// RW 0x50D2F5 .. 0x50D414: two triangles per heightmap cell, matching WorldHeightMap::getFlipState (RW 0x4AD985)
		std::int32_t rowOffset = 0;
		for (int j = startY; j < endY; ++j)
		{
			std::int32_t cellBase = base + rowOffset;
			for (int k = startX; k < endX; ++k, ++cellBase)
			{
				if (useTerrain && cellFlipped(*terrain, k, j))
				{
					out.indices.push_back(cellBase + 1);
					out.indices.push_back(cellBase + vertsPerRow);
					out.indices.push_back(cellBase);
					out.indices.push_back(cellBase + 1);
					out.indices.push_back(cellBase + 1 + vertsPerRow);
					out.indices.push_back(cellBase + vertsPerRow);
				}
				else
				{
					out.indices.push_back(cellBase);
					out.indices.push_back(cellBase + 1 + vertsPerRow);
					out.indices.push_back(cellBase + vertsPerRow);
					out.indices.push_back(cellBase);
					out.indices.push_back(cellBase + 1);
					out.indices.push_back(cellBase + 1 + vertsPerRow);
				}
			}
			rowOffset += vertsPerRow;
		}
		++out.decalsBuilt;
	}
}
