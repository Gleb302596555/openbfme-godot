// OpenBFME. GPL-3.0. See TerrainTracks.h.

#include "GameEngineDevice/W3DDevice/GameClient/TerrainTracks.h"

#include "Common/AsciiString.h"
#include "GameClient/MapChunks.h"
#include "Libraries/WWVegas/WW3D2/assetmgr.h"
#include "Libraries/WWVegas/WW3D2/htree.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace
{

Coord3D defaultNormal()
{
	return Coord3D{ 0.0f, 0.0f, 1.0f };
}

float sampleTrackHeight(float x, float y, int layer, const TerrainTracksHeightSampler &sampler, Coord3D *outNormal)
{
	if (outNormal)
	{
		*outNormal = defaultNormal();
	}
	if (layer != TerrainTrack::LAYER_GROUND)
	{
		if (sampler.layerHeight)
		{
			return TerrainTrack::BRIDGE_OFFSET_FACTOR + sampler.layerHeight(x, y, layer, outNormal);
		}
		if (sampler.groundHeight)
		{
			return TerrainTrack::BRIDGE_OFFSET_FACTOR + sampler.groundHeight(x, y, outNormal);
		}
		return TerrainTrack::BRIDGE_OFFSET_FACTOR;
	}
	return sampler.groundHeight ? sampler.groundHeight(x, y, outNormal) : 0.0f;
}

void writeEdgeEndpoints(TerrainTrack::Edge &edge, const Coord3D &centerPos, float dx, float dy, const Coord3D &normal, float width)
{
	// RW 0x483D27 / 0x484157 / 0x4842B5: 2D-normalized direction in XY (Z forced to 0) crossed with terrain normal.
	const float len2xy = dx * dx + dy * dy;
	float ndx = dx;
	float ndy = dy;
	if (len2xy > 0.0f)
	{
		const float invLen = 1.0f / std::sqrt(len2xy);
		ndx = dx * invLen;
		ndy = dy * invLen;
	}
	// Vector3::Cross_Product((ndx, ndy, 0), (nx, ny, nz))
	const float vx = ndy * normal.z;
	const float vy = -(normal.z * ndx);
	const float vz = ndx * normal.y - ndy * normal.x;
	const float halfWidth = width * 0.5f;

	edge.endPointPos[0].x = centerPos.x - vx * halfWidth;
	edge.endPointPos[0].y = centerPos.y - vy * halfWidth;
	edge.endPointPos[0].z = centerPos.z - vz * halfWidth + TerrainTrack::HEIGHT_ABOVE_TERRAIN;

	edge.endPointPos[1].x = centerPos.x + vx * halfWidth;
	edge.endPointPos[1].y = centerPos.y + vy * halfWidth;
	edge.endPointPos[1].z = centerPos.z + vz * halfWidth + TerrainTrack::HEIGHT_ABOVE_TERRAIN;
}

void writeEdgeUV(TerrainTrack::Edge &edge, int totalEdgesAdded)
{
	const float v = (totalEdgesAdded & 1) ? 0.0f : 1.0f;
	edge.endPointUV[0][0] = 0.0f;
	edge.endPointUV[0][1] = v;
	edge.endPointUV[1][0] = 1.0f;
	edge.endPointUV[1][1] = v;
}

} // namespace

void TerrainTrack::clear()
{
	m_endPoints[0] = Coord3D{ 0.0f, 0.0f, 0.0f };
	m_endPoints[1] = Coord3D{ 0.0f, 0.0f, 0.0f };
	m_textureName.clear();
	m_haveAnchor = false;
	m_haveCap = true;
	m_bound = false;
	m_topIndex = 0;
	m_bottomIndex = 0;
	m_activeEdgeCount = 0;
	m_totalEdgesAdded = 0;
	m_ownerAlive = false;
	m_airborne = false;
}

void TerrainTrack::init(float width, float length, const std::string &textureName, int maxEdges)
{
	const Coord3D savedLeft = m_endPoints[0];
	const Coord3D savedRight = m_endPoints[1];
	clear();
	m_endPoints[0] = savedLeft;
	m_endPoints[1] = savedRight;
	m_width = width;
	m_length = length;
	m_textureName = AsciiStringUtil::lowered(textureName);
	m_lastAnchor = Coord3D{ 0.0f, 1.0f, 2.25f };
	resizeEdges(maxEdges);
}

void TerrainTrack::resizeEdges(int maxEdges)
{
	const size_t cap = (size_t)std::max(1, maxEdges);
	if (m_edges.size() != cap)
	{
		m_edges.assign(cap, Edge());
	}
}

bool TerrainTrack::addCapEdgeToTrack(float x, float y, int layer, const TerrainTracksHeightSampler &sampler, std::int32_t syncTimeMs, int maxEdges)
{
	if (m_haveCap)
	{
		return false;
	}
	if (m_activeEdgeCount <= 1)
	{
		m_haveCap = true;
		m_haveAnchor = false;
		return false;
	}
	if (maxEdges <= 0)
	{
		m_haveCap = true;
		m_haveAnchor = false;
		return false;
	}
	resizeEdges(maxEdges);

	Coord3D normal = defaultNormal();
	const float eHeight = sampleTrackHeight(x, y, layer, sampler, &normal);
	const Coord3D pos{ x, y, eHeight };
	const float dx = x - m_lastAnchor.x;
	const float dy = y - m_lastAnchor.y;
	const float dz = eHeight - m_lastAnchor.z;

	bool changed = false;
	if (dx * dx + dy * dy + dz * dz < m_length * m_length)
	{
		const int lastAdded = m_topIndex > 0 ? (m_topIndex - 1) : (maxEdges - 1);
		if (m_edges[(size_t)lastAdded].alpha != 0.0f)
		{
			m_edges[(size_t)lastAdded].alpha = 0.0f;
			changed = true;
		}
		m_haveCap = true;
		m_haveAnchor = false;
		return changed;
	}

	if (m_activeEdgeCount >= maxEdges)
	{
		++m_bottomIndex;
		--m_activeEdgeCount;
		if (m_bottomIndex >= maxEdges)
		{
			m_bottomIndex = 0;
		}
	}
	if (m_topIndex >= maxEdges)
	{
		m_topIndex = 0;
	}

	Edge &topEdge = m_edges[(size_t)m_topIndex];
	writeEdgeEndpoints(topEdge, pos, dx, dy, normal, m_width);
	writeEdgeUV(topEdge, m_totalEdgesAdded);
	topEdge.timeAdded = syncTimeMs;
	topEdge.alpha = 0.0f;
	m_lastAnchor = pos;
	++m_activeEdgeCount;
	++m_totalEdgesAdded;
	++m_topIndex;
	m_haveCap = true;
	m_haveAnchor = false;
	return true;
}

bool TerrainTrack::addEdgeToTrack(float x, float y, const float basis[9], const Coord3D &pos, int layer, const TerrainTracksHeightSampler &sampler,
	std::int32_t syncTimeMs, int maxEdges)
{
	if (!m_ownerAlive || maxEdges <= 0)
	{
		return false;
	}
	if (!m_haveAnchor)
	{
		const float h = sampleTrackHeight(x, y, layer, sampler, nullptr);
		m_lastAnchor = Coord3D{ x, y, h };
		m_haveAnchor = true;
		m_airborne = true;
		m_haveCap = true;
		return false;
	}

	m_haveCap = false;

	if (layer != LAYER_GROUND)
	{
		return false;
	}

	resizeEdges(maxEdges);

	// RW 0x483FBF .. 0x484090: transform local bone endpoints by object transform matrix (row-major 3x3 basis + translation),
	// then average X and Y of the two transformed endpoints.
	const float lx0 = basis[0] * m_endPoints[0].x + basis[1] * m_endPoints[0].y + basis[2] * m_endPoints[0].z + pos.x;
	const float ly0 = basis[3] * m_endPoints[0].x + basis[4] * m_endPoints[0].y + basis[5] * m_endPoints[0].z + pos.y;
	const float lx1 = basis[0] * m_endPoints[1].x + basis[1] * m_endPoints[1].y + basis[2] * m_endPoints[1].z + pos.x;
	const float ly1 = basis[3] * m_endPoints[1].x + basis[4] * m_endPoints[1].y + basis[5] * m_endPoints[1].z + pos.y;
	const float mx = (lx0 + lx1) * 0.5f;
	const float my = (ly0 + ly1) * 0.5f;

	Coord3D normal = defaultNormal();
	const float eHeight = sampler.groundHeight ? sampler.groundHeight(mx, my, &normal) : 0.0f;
	const Coord3D centerPos{ mx, my, eHeight };
	const float dx = mx - m_lastAnchor.x;
	const float dy = my - m_lastAnchor.y;
	const float dz = eHeight - m_lastAnchor.z;

	if (dx * dx + dy * dy + dz * dz < m_length * m_length)
	{
		if (m_activeEdgeCount > 0)
		{
			const int lastAdded = m_topIndex > 0 ? (m_topIndex - 1) : (maxEdges - 1);
			writeEdgeEndpoints(m_edges[(size_t)lastAdded], centerPos, dx, dy, normal, m_width);
			return true;
		}
		return false;
	}

	if (m_activeEdgeCount >= maxEdges)
	{
		++m_bottomIndex;
		--m_activeEdgeCount;
		if (m_bottomIndex >= maxEdges)
		{
			m_bottomIndex = 0;
		}
	}
	if (m_topIndex >= maxEdges)
	{
		m_topIndex = 0;
	}

	Edge &topEdge = m_edges[(size_t)m_topIndex];
	writeEdgeEndpoints(topEdge, centerPos, dx, dy, normal, m_width);
	writeEdgeUV(topEdge, m_totalEdgesAdded);
	topEdge.timeAdded = syncTimeMs;
	topEdge.alpha = (m_airborne || m_activeEdgeCount <= 1) ? 0.0f : 1.0f;
	m_airborne = false;

	m_lastAnchor = centerPos;
	++m_activeEdgeCount;
	++m_totalEdgesAdded;
	++m_topIndex;
	return true;
}

float TerrainTracksSystem::computeTrackSpacing(const RenderObjPrototype *proto, float scale, const std::string &leftBone, const std::string &rightBone,
	Coord3D outLocalEndpoints[2])
{
	outLocalEndpoints[0] = Coord3D{ 0.0f, 0.0f, 0.0f };
	outLocalEndpoints[1] = Coord3D{ 0.0f, 0.0f, 0.0f };
	float spacing = TerrainTrack::DEFAULT_TRACK_SPACING;
	if (!proto || !proto->Tree || leftBone.empty() || rightBone.empty())
	{
		return spacing;
	}
	const int leftIdx = proto->Tree->Get_Bone_Index(leftBone);
	const int rightIdx = proto->Tree->Get_Bone_Index(rightBone);
	if (leftIdx == 0 || rightIdx == 0)
	{
		return spacing;
	}
	HTreePose pose;
	proto->Tree->Base_Pose(Matrix3D(), pose);
	if ((size_t)leftIdx >= pose.Transform.size() || (size_t)rightIdx >= pose.Transform.size())
	{
		return spacing;
	}
	const Matrix3D &lm = pose.Transform[(size_t)leftIdx];
	const Matrix3D &rm = pose.Transform[(size_t)rightIdx];
	outLocalEndpoints[0] = Coord3D{ lm.Row[0][3] * scale, lm.Row[1][3] * scale, lm.Row[2][3] * scale };
	outLocalEndpoints[1] = Coord3D{ rm.Row[0][3] * scale, rm.Row[1][3] * scale, rm.Row[2][3] * scale };
	const float dx = outLocalEndpoints[1].x - outLocalEndpoints[0].x;
	const float dy = outLocalEndpoints[1].y - outLocalEndpoints[0].y;
	const float dz = outLocalEndpoints[1].z - outLocalEndpoints[0].z;
	spacing = std::sqrt(dx * dx + dy * dy + dz * dz) + TerrainTrack::DEFAULT_TRACK_WIDTH;
	return spacing;
}

void TerrainTracksSystem::init(const TerrainTracksConfig &config)
{
	m_config = config;
	m_pool.clear();
	m_usedIds.clear();
	m_freeIds.clear();
	m_syncTimeMs = 0;
	m_syncAccumMs = 0.0;
	m_totalEdgesAdded = 0;
	m_poolExhausted = 0;

	const int count = std::max(0, config.maxTerrainTracks);
	m_pool.resize((size_t)count);
	m_freeIds.reserve((size_t)count);
	// Match retail init loop (pushing onto list head so last allocated is popped first, or deterministic index order).
	for (int i = count - 1; i >= 0; --i)
	{
		m_pool[(size_t)i].clear();
		m_freeIds.push_back(i);
	}
	++m_revision;
}

void TerrainTracksSystem::reset()
{
	while (!m_usedIds.empty())
	{
		releaseTrack(m_usedIds.back());
	}
	++m_revision;
}

void TerrainTracksSystem::setDetail(int maxEdges, int maxOpaqueEdges, int fadeDelayMs)
{
	// RW 0x4839B6: clear existing tracks (RW 0x483985), copy limits, resize edge buffers.
	for (int id : m_usedIds)
	{
		TerrainTrack &t = m_pool[(size_t)id];
		t.setHaveAnchor(false);
		const bool keepBound = t.isBound();
		const bool keepOwner = t.isOwnerAlive();
		const std::string tex = t.textureName();
		const float w = t.width();
		const float l = t.length();
		const Coord3D left = t.localEndPoints()[0];
		const Coord3D right = t.localEndPoints()[1];
		t.init(w, l, tex, maxEdges);
		t.setLocalEndPoints(left, right);
		t.setBound(keepBound);
		t.setOwnerAlive(keepOwner);
	}
	m_config.maxTankTrackEdges = maxEdges;
	m_config.maxTankTrackOpaqueEdges = maxOpaqueEdges;
	m_config.maxTankTrackFadeDelay = fadeDelayMs;
	++m_revision;
}

void TerrainTracksSystem::setMakeTrackMarks(bool on)
{
	if (m_config.makeTrackMarks == on)
	{
		return;
	}
	m_config.makeTrackMarks = on;
	if (!on)
	{
		for (int id : m_usedIds)
		{
			m_pool[(size_t)id].setHaveAnchor(false);
		}
	}
	++m_revision;
}

int TerrainTracksSystem::bindTrack(const RenderObjPrototype *proto, float scale, float length, const std::string &textureName,
	const std::string &leftBone, const std::string &rightBone)
{
	if (textureName.empty() || m_freeIds.empty())
	{
		if (!textureName.empty() && m_freeIds.empty())
		{
			++m_poolExhausted;
		}
		return -1;
	}
	const int id = m_freeIds.back();
	m_freeIds.pop_back();
	m_usedIds.push_back(id);

	TerrainTrack &t = m_pool[(size_t)id];
	Coord3D localEnds[2];
	const float width = computeTrackSpacing(proto, scale, leftBone, rightBone, localEnds);
	t.init(width, length, textureName, m_config.maxTankTrackEdges);
	t.setLocalEndPoints(localEnds[0], localEnds[1]);
	t.setBound(true);
	t.setOwnerAlive(true);
	++m_revision;
	return id;
}

void TerrainTracksSystem::unbindTrack(int trackId)
{
	if (trackId < 0 || (size_t)trackId >= m_pool.size())
	{
		return;
	}
	TerrainTrack &t = m_pool[(size_t)trackId];
	t.setBound(false);
	t.setOwnerAlive(false);
}

void TerrainTracksSystem::releaseTrack(int trackId)
{
	if (trackId < 0 || (size_t)trackId >= m_pool.size())
	{
		return;
	}
	auto it = std::find(m_usedIds.begin(), m_usedIds.end(), trackId);
	if (it != m_usedIds.end())
	{
		m_usedIds.erase(it);
	}
	m_pool[(size_t)trackId].clear();
	m_freeIds.push_back(trackId);
}

bool TerrainTracksSystem::addCapEdge(int trackId, float x, float y, int layer, const TerrainTracksHeightSampler &sampler)
{
	if (trackId < 0 || (size_t)trackId >= m_pool.size())
	{
		return false;
	}
	TerrainTrack &t = m_pool[(size_t)trackId];
	const int before = t.totalEdgesAdded();
	const bool changed = t.addCapEdgeToTrack(x, y, layer, sampler, m_syncTimeMs, m_config.maxTankTrackEdges);
	if (t.totalEdgesAdded() > before)
	{
		m_totalEdgesAdded += (std::uint64_t)(t.totalEdgesAdded() - before);
	}
	if (changed)
	{
		++m_revision;
	}
	return changed;
}

bool TerrainTracksSystem::addEdge(int trackId, float x, float y, const float basis[9], const Coord3D &pos, int layer,
	const TerrainTracksHeightSampler &sampler)
{
	if (trackId < 0 || (size_t)trackId >= m_pool.size())
	{
		return false;
	}
	TerrainTrack &t = m_pool[(size_t)trackId];
	const int before = t.totalEdgesAdded();
	const bool changed = t.addEdgeToTrack(x, y, basis, pos, layer, sampler, m_syncTimeMs, m_config.maxTankTrackEdges);
	if (t.totalEdgesAdded() > before)
	{
		m_totalEdgesAdded += (std::uint64_t)(t.totalEdgesAdded() - before);
	}
	if (changed)
	{
		++m_revision;
	}
	return changed;
}

void TerrainTracksSystem::setAirborne(int trackId, bool airborne)
{
	if (trackId < 0 || (size_t)trackId >= m_pool.size())
	{
		return;
	}
	m_pool[(size_t)trackId].setAirborne(airborne);
}

void TerrainTracksSystem::update(double elapsedMs)
{
	if (elapsedMs > 0.0)
	{
		m_syncAccumMs += elapsedMs;
		const double whole = std::floor(m_syncAccumMs);
		if (whole >= 1.0)
		{
			const std::int64_t step = (std::int64_t)whole;
			m_syncAccumMs -= whole;
			m_syncTimeMs = (std::int32_t)((std::int64_t)m_syncTimeMs + step);
		}
	}

	const int maxEdges = m_config.maxTankTrackEdges;
	const int fadeDelay = m_config.maxTankTrackFadeDelay;
	bool anyMutated = false;

	std::vector<int> toRelease;
	for (int id : m_usedIds)
	{
		TerrainTrack &mod = m_pool[(size_t)id];
		if (!m_config.makeTrackMarks)
		{
			mod.setHaveAnchor(false);
		}
		const int initialCount = mod.activeEdgeCount();
		if (initialCount == 0)
		{
			if (!mod.isBound())
			{
				toRelease.push_back(id);
			}
			continue;
		}
		if (maxEdges <= 0)
		{
			mod.setBottomAndCount(0, 0);
			anyMutated = true;
			if (!mod.isBound())
			{
				toRelease.push_back(id);
			}
			continue;
		}

		int bottom = mod.bottomIndex();
		int activeCount = initialCount;
		int index = bottom;
		std::vector<TerrainTrack::Edge> &edges = mod.edgesMutable();
		for (int i = 0; i < initialCount; ++i, ++index)
		{
			if (index >= maxEdges)
			{
				index = 0;
			}
			float diff = 0.0f;
			if (fadeDelay > 0)
			{
				const std::uint32_t elapsed = (std::uint32_t)m_syncTimeMs - (std::uint32_t)edges[(size_t)index].timeAdded;
				diff = 1.0f - (float)elapsed / (float)fadeDelay;
				if (diff < 0.0f)
				{
					diff = 0.0f;
				}
			}
			if (edges[(size_t)index].alpha > 0.0f && edges[(size_t)index].alpha != diff)
			{
				edges[(size_t)index].alpha = diff;
				anyMutated = true;
			}
			if (diff == 0.0f)
			{
				++bottom;
				--activeCount;
				if (bottom >= maxEdges)
				{
					bottom = 0;
				}
				anyMutated = true;
			}
		}
		mod.setBottomAndCount(bottom, activeCount);
		if (activeCount == 0 && !mod.isBound())
		{
			toRelease.push_back(id);
		}
	}
	for (int id : toRelease)
	{
		releaseTrack(id);
		anyMutated = true;
	}
	if (anyMutated)
	{
		++m_revision;
	}
}

std::uint32_t TerrainTracksSystem::trackDiffuseBase(const TerrainTracksShade &shade)
{
	int v[3];
	for (int k = 0; k < 3; ++k)
	{
		float c = shade.diffuse[k] * 0.5f + shade.ambient[k];
		if (c < 0.0f)
		{
			c = 0.0f;
		}
		else if (c > 1.0f)
		{
			c = 1.0f;
		}
		v[k] = std::clamp((int)(c * 255.0f), 0, 255);
	}
	return ((std::uint32_t)v[0] << 16) | ((std::uint32_t)v[1] << 8) | (std::uint32_t)v[2];
}

TerrainTracksShade TerrainTracksSystem::shadeFrom(const TimeOfDayLights *lights)
{
	TerrainTracksShade s;
	for (int k = 0; k < 3; ++k)
	{
		s.ambient[k] = lights ? lights->terrain[0].ambient[k] : 0.0f;
		s.diffuse[k] = lights ? lights->terrain[0].diffuse[k] : 0.0f;
	}
	return s;
}

void TerrainTracksSystem::buildFlushBatches(const TerrainTracksShade &shade, std::vector<FlushBatch> &outBatches) const
{
	outBatches.clear();
	const int maxEdges = m_config.maxTankTrackEdges;
	if (m_usedIds.empty() || maxEdges < 2)
	{
		return;
	}

	const std::uint32_t rgb = trackDiffuseBase(shade);
	const int opaqueEdges = m_config.maxTankTrackOpaqueEdges;
	const float numFadedEdges = (float)(maxEdges - opaqueEdges);

	std::map<std::string, size_t> batchByTexture;
	for (int id : m_usedIds)
	{
		const TerrainTrack &mod = m_pool[(size_t)id];
		const int count = mod.activeEdgeCount();
		if (count < 2 || mod.textureName().empty())
		{
			continue;
		}
		const size_t neededVerts = (size_t)count * 2u;
		size_t batchIndex = 0;
		auto it = batchByTexture.find(mod.textureName());
		if (it == batchByTexture.end() || outBatches[it->second].vertices.size() + neededVerts > 65000u)
		{
			batchIndex = outBatches.size();
			outBatches.push_back(FlushBatch{ mod.textureName(), {}, {} });
			batchByTexture[mod.textureName()] = batchIndex;
		}
		else
		{
			batchIndex = it->second;
		}

		FlushBatch &batch = outBatches[batchIndex];
		const std::uint16_t baseVertex = (std::uint16_t)batch.vertices.size();
		const std::vector<TerrainTrack::Edge> &edges = mod.edges();
		int idx = mod.bottomIndex();
		for (int j = 0; j < count; ++j, ++idx)
		{
			if (idx >= maxEdges)
			{
				idx = 0;
			}
			const TerrainTrack::Edge &edge = edges[(size_t)idx];
			float finalAlpha = 0.0f;
			if (j != count - 1)
			{
				float fade = 1.0f;
				if (count - j - 1 >= opaqueEdges && numFadedEdges > 0.0f)
				{
					fade = 1.0f - (float)(count - opaqueEdges - j) / numFadedEdges;
					if (fade < 0.0f)
					{
						fade = 0.0f;
					}
				}
				finalAlpha = edge.alpha * fade;
			}
			const std::uint32_t aByte = (std::uint32_t)std::clamp((int)(finalAlpha * 255.0f), 0, 255);
			const std::uint32_t argb = (aByte << 24) | rgb;
			batch.vertices.push_back(Vertex{ edge.endPointPos[0].x, edge.endPointPos[0].y, edge.endPointPos[0].z, argb, edge.endPointUV[0][0],
				edge.endPointUV[0][1] });
			batch.vertices.push_back(Vertex{ edge.endPointPos[1].x, edge.endPointPos[1].y, edge.endPointPos[1].z, argb, edge.endPointUV[1][0],
				edge.endPointUV[1][1] });
		}
		// RW 0x48376D .. 0x48379A: quad triangles (2i, 2i+1, 2i+3), (2i, 2i+3, 2i+2)
		for (int i = 0; i < count - 1; ++i)
		{
			const std::uint16_t v0 = (std::uint16_t)(baseVertex + i * 2);
			const std::uint16_t v1 = (std::uint16_t)(v0 + 1);
			const std::uint16_t v2 = (std::uint16_t)(v0 + 2);
			const std::uint16_t v3 = (std::uint16_t)(v0 + 3);
			batch.indices.push_back(v0);
			batch.indices.push_back(v1);
			batch.indices.push_back(v3);
			batch.indices.push_back(v0);
			batch.indices.push_back(v3);
			batch.indices.push_back(v2);
		}
	}
}

const TerrainTrack *TerrainTracksSystem::track(int trackId) const
{
	if (trackId < 0 || (size_t)trackId >= m_pool.size())
	{
		return nullptr;
	}
	return &m_pool[(size_t)trackId];
}

TerrainTracksSystem::Stats TerrainTracksSystem::stats() const
{
	Stats s;
	s.poolSize = (int)m_pool.size();
	s.usedTracks = (int)m_usedIds.size();
	s.freeTracks = (int)m_freeIds.size();
	s.totalEdgesAdded = m_totalEdgesAdded;
	s.poolExhausted = m_poolExhausted;
	for (int id : m_usedIds)
	{
		const TerrainTrack &t = m_pool[(size_t)id];
		if (t.isBound())
		{
			++s.boundTracks;
		}
		s.activeEdges += t.activeEdgeCount();
	}
	return s;
}
