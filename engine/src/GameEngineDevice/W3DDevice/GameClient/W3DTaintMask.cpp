// OpenBFME. GPL-3.0.
//
// W3DTaintMask: the terrain's taint mask (W3DTaintMask.h holds the target facts).

#include "GameEngineDevice/W3DDevice/GameClient/W3DTaintMask.h"

#include "GameClient/FireSetting.h"
#include "GameLogic/SimMath.h"

namespace W3DTaintMask
{
// RW 0x497F60
RGBColor fireColour(const FireSetting &fire, int type)
{
	if (type == 1)
	{
		return fire.fuelIndicatorColor; // RW 0x497F9B: the manager's + 0x20
	}
	if (type == 2)
	{
		return fire.burntTerrainColor; // RW 0x497FA5: + 0x14
	}
	return RGBColor{ 1.0f, 1.0f, 1.0f }; // RW 0x497F66: 1.0 (RW 0xBD1908)
}

namespace
{
// RW 0x473952 .. 0x47398B: fild the factor (255), fld the channel, fmul (x87, precision control 24), _ftol (RW 0xA3CFA4, truncation), and 0xFF
std::uint32_t channel(float c)
{
	return SimMath::ftol2Low32(SimMath::pc24MulW((double)c, 255.0)) & 0xFFu;
}
} // namespace

std::uint32_t texel(int value, const RGBColor &colour)
{
	std::uint32_t t = (std::uint32_t)value & 0xFFu;
	t = (t << 8) | channel(colour.red);
	t = (t << 8) | channel(colour.green);
	t = (t << 8) | channel(colour.blue);
	return t;
}

int cellCentre(int n, float cellSize)
{
	return SimMath::truncToInt32(SimMath::mulf32(SimMath::addf32((float)n, 0.5f), cellSize));
}

void build(const TaintView &view, const FireSetting &fire, std::uint8_t borderLevel, std::vector<std::uint32_t> &texels, int &width, int &height)
{
	width = view.countX + 2;
	height = view.countY + 2;
	const std::uint32_t b = borderLevel;
	const std::uint32_t border = (b << 24) | (b << 16) | (b << 8) | b; // RW 0x47323E .. 0x47324D
	texels.assign((size_t)width * (size_t)height, border);
	for (int y = 0; y < view.countY; ++y)
	{
		for (int x = 0; x < view.countX; ++x)
		{
			const size_t i = (size_t)y * (size_t)view.countX + (size_t)x;
			texels[(size_t)(y + 1) * (size_t)width + (size_t)(x + 1)] = texel(view.values[i], fireColour(fire, view.fireTypes[i]));
		}
	}
}
} // namespace W3DTaintMask
