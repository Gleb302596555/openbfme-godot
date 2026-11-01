// OpenBFME. GPL-3.0. See W3DStreakDraw.h (RW 0x4CFD13 / 0xBE39F8 / 0x4CF884, donor BFME1 0x77D6B0).

#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DStreakDraw.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif

namespace
{
// RW 0xDA3A84
const char *const kWeatherNames[] = { "NORMAL", "SNOWY", nullptr };

// RW 0x4CFEB9: WeatherTexture = WEATHER NAME (parseIndexList over RW 0xDA3A84, then parseAsciiString), appended.
void parseWeatherTexture(INI *ini, void *, void *store, const void *)
{
	const int weather = INI::scanIndexList(ini->getNextToken(), kWeatherNames);
	const std::string name = ini->getNextAsciiString();
	static_cast<std::vector<std::pair<int, std::string>> *>(store)->emplace_back(weather, name);
}

#define STREAK_ROW(name, parse, field) { name, parse, nullptr, (int)offsetof(W3DStreakDrawModuleData, field) }
// RW 0xBE39F8, 7 rows in the binary's order
const FieldParse kStreakFieldParse[] = {
	STREAK_ROW("Length", INI::parseReal, m_length),
	STREAK_ROW("Width", INI::parseReal, m_width),
	STREAK_ROW("Additive", INI::parseBool, m_additive),
	STREAK_ROW("Color", INI::parseRGBColor, m_color),
	STREAK_ROW("Texture", INI::parseAsciiString, m_texture),
	STREAK_ROW("NumSegments", INI::parseUnsignedInt, m_numSegments),
	STREAK_ROW("WeatherTexture", parseWeatherTexture, m_weatherTextures),
	{ nullptr, nullptr, nullptr, 0 }
};
#undef STREAK_ROW

// RW 0x4CF8E5 ..: float32 squares summed z, y, x (SSE), the root by x87 fsqrt stored to float
float distance(const float a[3], const float b[3])
{
	const float x = a[0] - b[0];
	const float y = a[1] - b[1];
	const float z = a[2] - b[2];
	const float square = z * z + y * y + x * x;
	return (float)std::sqrt((double)square);
}

void setPoint(W3DStreakPoint &p, const float pos[3], float length)
{
	p.pos[0] = pos[0];
	p.pos[1] = pos[1];
	p.pos[2] = pos[2];
	p.length = length;
}

} // namespace

void W3DStreakDrawModuleData::buildFieldParse(MultiIniFieldParse &p)
{
	p.add(kStreakFieldParse); // registry: tables [0xBE39F8]
}

namespace W3DStreakDrawTables
{
const FieldParse *streak() { return kStreakFieldParse; }
}

void W3DStreakTrail::update(const W3DStreakDrawModuleData &data, const float position[3])
{
	W3DStreakPoint np;
	const size_t n = m_points.size();
	if (n == 0)
	{
		setPoint(np, position, 0.0f);
		m_points.push_back(np);
		return;
	}
	if (n == 1)
	{
		setPoint(np, position, distance(position, m_points[0].pos) + m_points[0].length);
		m_points.push_back(np);
		return;
	}
	const float d = distance(position, m_points[n - 2].pos);
	const float len = d + m_points[n - 2].length;
	setPoint(m_points[n - 1], position, len);
	// RW 0x4CF9B1: Length / (float)NumSegments (the unsigned int converted with the 2^32 correction); a zero count divides to +inf
	const float segment = data.m_length / (float)data.m_numSegments;
	if (d > segment)
	{
		setPoint(np, position, len);
		m_points.push_back(np);
	}
	while (m_points.size() >= 2 && len - data.m_length > m_points[1].length)
	{
		m_points.erase(m_points.begin());
	}
}

void W3DStreakDiagnostics::recordFailure(const std::string &materialKey, const std::string &message)
{
	m_failed.insert(materialKey);
	m_errors.push_back(message);
}

std::string W3DStreakDrawStopLine()
{
	return "[S-391] streak drawing: W3DStreakDraw's points follow RW 0x4CF884; lane PROJ-2 (S-1002) ported the texture's V along the trail (RW 0x525E1F: 1 at the "
		"head, 0 one Length behind), the blend of the two shader presets (RW 0x4CFA9A: Additive ONE / ONE, else SRC_ALPHA / INV_SRC_ALPHA), the colour and opacity "
		"(RW 0x4CFA25) and the WeatherTexture choice; the ribbon is SimpleStreakRendererClass::RenderStreak's mitred strip (BFME2 Rva 0x126433; RW 0x525E1F "
		"INFERENCE: the same size and arguments, not compared instruction by instruction)";
}

std::string W3DStreakRenderStopLine()
{
	return "[S-1002] streak render (lane PROJ-2): ported from RotWK: the texture V along the trail (RW 0x525E1F: v = (point length - (head length - Length)) / Length, "
		"the head at V 1), the shader presets (RW 0x4CFA9A: Additive DAT 0xD9B2E4 = SRCBLEND ONE / DSTBLEND ONE, else DAT 0xD9B2F0 = SRC_ALPHA / INV_SRC_ALPHA), "
		"the colour and the drawable's opacity (RW 0x4CF884), the WeatherTexture of the map's weather; ported from the BFME2 donor (SimpleStreakRendererClass::"
		"RenderStreak Rva 0x126433, WorldBuilder 0xA1E840), INFERENCE for RW: the vertex construction (the edge planes through the eye, the mitre rays, a fold "
		"swapping the edges, 129-point chunks) and the clamped U / V address modes (value 1 = TEXTURE_ADDRESS_CLAMP); not ported: the shadow-map pass, the sort "
		"of the transparent pass, the primary gradient 6 (drawn as the texture times the colour), the screen handedness of the view (the left / right of the "
		"strip follows the D3D view space: x right, y up, z forward, INFERENCE)";
}

const std::string &W3DStreakTexture(const W3DStreakDrawModuleData &data, int weather)
{
	for (const std::pair<int, std::string> &w : data.m_weatherTextures)
	{
		if (w.first == weather)
		{
			return w.second; // RW 0x4CFA9A: the first entry of the current weather (GlobalData +0x138)
		}
	}
	return data.m_texture;
}

namespace
{
struct V3
{
	float x = 0.0f, y = 0.0f, z = 0.0f;
};
V3 operator+(V3 a, V3 b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
V3 operator-(V3 a, V3 b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
V3 operator-(V3 a) { return { -a.x, -a.y, -a.z }; }
V3 operator*(V3 a, float s) { return { a.x * s, a.y * s, a.z * s }; }
float dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
V3 cross(V3 a, V3 b) { return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; } // Vector3::Cross_Product
V3 normalized(V3 v) // Vector3::Normalize: a zero vector stays zero
{
	const float length2 = dot(v, v);
	if (length2 != 0.0f)
	{
		v = v * (1.0f / std::sqrt(length2));
	}
	return v;
}
bool equalWithinEpsilon(V3 a, V3 b, float epsilon)
{
	return std::fabs(a.x - b.x) < epsilon && std::fabs(a.y - b.y) < epsilon && std::fabs(a.z - b.z) < epsilon;
}
// the point of the edge plane (through the eye, `plane` its unit normal) nearest to `point`, as a direction
V3 projectedDirection(V3 plane, V3 point)
{
	return normalized(point - plane * dot(plane, point));
}
// the ray through the eye where the edges of two consecutive segments meet at `mid`: the planes' crossing when they are not near parallel, else the plane
// between them (parallel_factor 0.9)
V3 mitreDirection(V3 before, V3 after, V3 mid)
{
	const float vdp = dot(before, after);
	if (std::fabs(vdp) < 0.9f)
	{
		V3 d = normalized(cross(before, after));
		if (dot(d, mid) < 0.0f)
		{
			d = -d;
		}
		return d;
	}
	const V3 plane = normalized(vdp > 0.0f ? before + after : before - after);
	return normalized(mid - plane * dot(plane, mid));
}
} // namespace

W3DStreakMesh W3DStreakRibbon(const std::vector<W3DStreakPoint> &input, float width, float length, const W3DStreakCamera &camera)
{
	W3DStreakMesh out;
	const size_t numPoints = input.size();
	if (numPoints < 2 || !(length > 0.0f))
	{
		return out;
	}
	const V3 eye = { camera.eye[0], camera.eye[1], camera.eye[2] };
	const V3 right = { camera.right[0], camera.right[1], camera.right[2] };
	const V3 up = { camera.up[0], camera.up[1], camera.up[2] };
	const V3 forward = { camera.forward[0], camera.forward[1], camera.forward[2] };
	size_t chunkSize = 129; // BFME2 0x126433: chunk_size = 129, clipped to the point count
	if (chunkSize > numPoints)
	{
		chunkSize = numPoints;
	}
	const float radius = width * 0.5f;
	for (size_t chunkIndex = 0; chunkIndex < numPoints - 1; chunkIndex += chunkSize - 1)
	{
		const size_t count = std::min(numPoints - chunkIndex, chunkSize);
		std::vector<V3> pts(count);
		std::vector<float> texV(count);
		for (size_t i = 0; i < count; ++i)
		{
			const W3DStreakPoint &p = input[chunkIndex + i];
			const V3 d = V3{ p.pos[0], p.pos[1], p.pos[2] } - eye;
			pts[i] = { dot(d, right), dot(d, up), dot(d, forward) }; // the view matrix (D3D view space)
		}
		// the texture positions are the points' lengths, read from the start of the array for every chunk (retail does not advance them: only a trail of more
		// than 129 points has a second chunk)
		const float end = input[count - 1].length - length;
		for (size_t i = 0; i < count; ++i)
		{
			texV[i] = (input[i].length - end) / length;
		}
		// segment s (1 .. count - 1) runs from point s - 1 to point s; its two edge planes are kept as unit normals
		std::vector<V3> topPlane(count), bottomPlane(count);
		bool switchEdges = false;
		for (size_t s = 1; s < count; ++s)
		{
			V3 &curr = pts[s - 1];
			V3 &next = pts[s];
			if (equalWithinEpsilon(curr, next, 0.0001f))
			{
				next.x += 0.001f;
			}
			const V3 segdir = normalized(next - curr);
			const V3 nearest = curr + segdir * -dot(segdir, curr);
			const V3 offset = normalized(cross(segdir, nearest));
			const V3 top = curr + offset * radius;
			const V3 bottom = curr + offset * -radius;
			const V3 topNormal = normalized(cross(top, segdir));
			const V3 bottomNormal = normalized(cross(segdir, bottom));
			topPlane[s] = topNormal;
			bottomPlane[s] = bottomNormal;
			if (s > 1)
			{
				const V3 prevPlane = normalized(cross(pts[s - 2], curr));
				const V3 currPlane = normalized(cross(curr, next));
				if (dot(prevPlane, currPlane) < 0.0f)
				{
					switchEdges = !switchEdges; // the trail folds back on itself
				}
			}
			if (switchEdges)
			{
				topPlane[s] = -bottomNormal;
				bottomPlane[s] = -topNormal;
			}
		}
		// the rays through the eye of each point's two vertices (intersection i + 1 of the retail arrays belongs to point i)
		std::vector<V3> topDir(count), bottomDir(count);
		topDir[0] = projectedDirection(topPlane[1], pts[0]);
		bottomDir[0] = projectedDirection(bottomPlane[1], pts[0]);
		topDir[count - 1] = projectedDirection(topPlane[count - 1], pts[count - 1]);
		bottomDir[count - 1] = projectedDirection(bottomPlane[count - 1], pts[count - 1]);
		for (size_t i = 1; i + 1 < count; ++i)
		{
			topDir[i] = mitreDirection(topPlane[i], topPlane[i + 1], pts[i]);
			bottomDir[i] = mitreDirection(bottomPlane[i], bottomPlane[i + 1], pts[i]);
		}
		const unsigned base = (unsigned)out.vertices.size();
		for (size_t i = 0; i < count; ++i)
		{
			const V3 vertices[2] = { topDir[i] * dot(pts[i], topDir[i]), bottomDir[i] * dot(pts[i], bottomDir[i]) };
			for (int side = 0; side < 2; ++side)
			{
				const V3 &c = vertices[side];
				// back from view space to the space of the points
				W3DStreakVertex v;
				v.pos[0] = camera.eye[0] + camera.right[0] * c.x + camera.up[0] * c.y + camera.forward[0] * c.z;
				v.pos[1] = camera.eye[1] + camera.right[1] * c.x + camera.up[1] * c.y + camera.forward[1] * c.z;
				v.pos[2] = camera.eye[2] + camera.right[2] * c.x + camera.up[2] * c.y + camera.forward[2] * c.z;
				v.u = (float)side; // the top vertex takes u 0, the bottom u 1
				v.v = texV[i];
				out.vertices.push_back(v);
			}
		}
		for (unsigned i = 1; i < (unsigned)count; ++i)
		{
			const unsigned lastTop = base + 2 * (i - 1), lastBottom = lastTop + 1, top = base + 2 * i, bottom = top + 1;
			const unsigned tris[6] = { lastTop, lastBottom, top, lastBottom, bottom, top };
			out.indices.insert(out.indices.end(), tris, tris + 6);
		}
	}
	return out;
}
