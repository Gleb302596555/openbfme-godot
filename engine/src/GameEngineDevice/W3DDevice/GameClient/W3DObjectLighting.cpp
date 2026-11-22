// OpenBFME. GPL-3.0. See W3DObjectLighting.h (sources: RW 0x54F406 / 0x54CF11 / 0x54CFD0 / 0x54D083 / 0x4AC97F, ZH
// W3DDisplay::setTimeOfDay and LightEnvironmentClass).

#include "GameEngineDevice/W3DDevice/GameClient/W3DObjectLighting.h"

#include "GameClient/MapChunks.h"

#include <algorithm>
#include <cmath>

namespace
{

// ZH LightEnvironmentClass::Add_Light: "Don't accept lights that are almost black".
bool acceptedLight(const GlobalLight &l)
{
	return !(l.diffuse[0] < 0.05f && l.diffuse[1] < 0.05f && l.diffuse[2] < 0.05f);
}

void fillSet(const GlobalLight (&lights)[3], float scale, const float *lightScale, W3DLightSet &out)
{
	out = W3DLightSet();
	// ZH: the scene ambient is light 0's ambient (the global lights themselves carry none); Pre_Render_Update clamps it.
	for (int c = 0; c < 3; ++c)
	{
		out.ambient[c] = std::clamp(lights[0].ambient[c], 0.0f, 1.0f) * scale;
	}
	for (int i = 0; i < 3; ++i)
	{
		// RTS3DScene::updateFixedLightEnvironments (RW 0x46E009): the light's diffuse is set to diffuse * the map's light scale BEFORE
		// Add_Light, so the 0.05 test sees the scaled colour
		GlobalLight l = lights[i];
		for (int c = 0; c < 3; ++c)
		{
			l.diffuse[c] = lightScale ? lightScale[c] * lights[i].diffuse[c] : lights[i].diffuse[c];
		}
		if (!acceptedLight(l))
		{
			continue;
		}
		W3DLightSet &s = out;
		for (int c = 0; c < 3; ++c)
		{
			s.color[s.count][c] = l.diffuse[c] * scale;
			s.toLight[s.count][c] = -l.lightPos[c]; // ZH Init_From_Directional_Light: Direction = -transform Z = -lightPos
		}
		++s.count;
	}
}

void fillFloor(const GlobalLight (&terrain)[3], float scale, W3DFloorLighting &out)
{
	// S-4974 (RW 0x4E44FB calcFloorVertexColor + RW 0x4E5631 W3DFloorBuffer::Draw via terrain.fxo Default RenderingMode 3)
	out = W3DFloorLighting();
	out.colorScale = scale;
	for (int c = 0; c < 3; ++c)
	{
		// RW 0x4E44FB: terrain[0].ambient + terrain[0].diffuse sun
		out.baseAmbient[c] = terrain[0].ambient[c];
		out.sunColor[c] = terrain[0].diffuse[c];
		out.sunDir[c] = -terrain[0].lightPos[c];
	}
	for (int i = 1; i < 3; ++i)
	{
		const float nz = std::min(std::fabs(-terrain[i].lightPos[2]), 1.0f);
		for (int c = 0; c < 3; ++c)
		{
			out.accentDiffuse[c] += nz * terrain[i].diffuse[c];
		}
	}
}

} // namespace

namespace W3DObjectLightingUtil
{

float effectColorScale(const GlobalLightingData &lighting)
{
	return lighting.version >= 5 && lighting.terrainLightingMultiplier > 1.0f ? 2.0f : 1.0f;
}

bool fromMap(const GlobalLightingData &lighting, W3DObjectLighting &out, std::string *error, const float *lightScale)
{
	if (lighting.timeOfDay < 1 || lighting.timeOfDay > 4)
	{
		if (error)
		{
			*error = "GlobalLighting: time of day " + std::to_string(lighting.timeOfDay) + " is not 1..4 (Morning .. Night)";
		}
		return false;
	}
	out = W3DObjectLighting();
	out.timeOfDay = lighting.timeOfDay;
	// RW 0x4AC97F: version >= 5 and multiplier > 1.0 sets the byte RW 0xD9B03C; the SAS callbacks then scale by 2.0 (RW 0xBD889C).
	out.colorScale = effectColorScale(lighting);
	const TimeOfDayLights &tod = lighting.tod[lighting.timeOfDay - 1];
	fillSet(tod.objects, out.colorScale, lightScale, out.objects);
	fillSet(tod.infantry, out.colorScale, lightScale, out.infantry);
	fillFloor(tod.terrain, out.colorScale, out.floor);
	return true;
}

void fromLivingWorldMap(const float ambient[3], const float rgb[3][3], const float dirDegrees[3][2], W3DObjectLighting &out)
{
	out = W3DObjectLighting();
	out.colorScale = 2.0f;
	W3DLightSet &set = out.objects;
	for (int c = 0; c < 3; ++c)
	{
		set.ambient[c] = std::clamp(ambient[c], 0.0f, 1.0f) * out.colorScale;
	}
	const float degToRad = 3.14159265358979f / 180.0f;
	for (int i = 0; i < 3; ++i)
	{
		if (rgb[i][0] < 0.05f && rgb[i][1] < 0.05f && rgb[i][2] < 0.05f)
		{
			continue;
		}
		const float el = dirDegrees[i][0] * degToRad;
		const float az = dirDegrees[i][1] * degToRad;
		const float toLight[3] = { std::cos(el) * std::cos(az), std::cos(el) * std::sin(az), std::sin(el) };
		for (int c = 0; c < 3; ++c)
		{
			set.color[set.count][c] = rgb[i][c] * out.colorScale;
			set.toLight[set.count][c] = toLight[c];
		}
		++set.count;
	}
	out.infantry = set;
	// S-4974 (RW 0x4E44FB / 0x4E5631): LivingWorld floor light environment follows terrain sun/ambient
	out.floor.colorScale = out.colorScale;
	for (int c = 0; c < 3; ++c)
	{
		// RW 0x4E44FB / 0x4E5631
		out.floor.baseAmbient[c] = std::clamp(ambient[c], 0.0f, 1.0f);
		out.floor.sunColor[c] = set.count > 0 ? rgb[0][c] : 0.0f;
		out.floor.sunDir[c] = set.count > 0 ? set.toLight[0][c] : (c == 2 ? 1.0f : 0.0f);
	}
}

std::vector<std::string> stopLines()
{
	return {
		"[S-390] object lighting: the light environment is filled like ZH's (scene ambient = the map's objects light 0 ambient clamped to [0, 1], "
		"the directional lights = the objects lights 0..2 whose diffuse reaches 0.05, direction -lightPos), infantry drawables use the map's "
		"infantry set, and structure floors (W3DFloorDraw / W3DFloorBuffer, RW 0x4E5631 / 0x4E44FB, S-4974) use tod.terrain via terrain.fxo Default RenderingMode 3; "
		"the doubling of both colours (GlobalLighting multiplier > 1, RW 0x4AC97F -> 0x54CF3B) and the effect formulas "
		"(normalmapped.fxo, defaultw3d.fxo) are target facts; the cloud shadow texture, point lights, shadows, shroud and fog of the effects are not applied",
	};
}

} // namespace W3DObjectLightingUtil
