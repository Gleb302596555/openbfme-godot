// OpenBFME. GPL-3.0.
//
// W3DTaintMask (stop S-5261; TOP20 "Fire": BurntTerrainColor / FuelIndicatorColor): the terrain's taint mask, the texture terrain.fxo's Taint struct reads.
// RotWK keeps it in a second instance of W3DShroud's class at BaseHeightMap + 0x387C (the shroud's is + 0x3878; both made over the height map at GameData
// PartitionCellSize, RW 0x46CF61 / 0x46CF8D -> RW 0x4732A9). Client only.
//
// TARGET FACTS (RotWK game.dat, caveat S-001; read in this lane):
//   * the cell write RW 0x4735AB(x, y, value, flag), from TheTaintManager's callback (RW 0x6D8B6E -> TheDisplay slot 0x13C = RW 0x4449C4) and from the
//     shroud's cell write (RW 0x44499D, with TheTaintManager's value at the cell): nothing without texels (+ 0x18), outside the cells or without GameData
//     TaintOn (+ 0xC6A); the texel is value << 24 | R << 16 | G << 8 | B with each channel the colour times a factor through _ftol (RW 0xA3CFA4).
//     When the terrain draws with its effect (render object RW 0xDD169C slot 0x24C = RW 0x4E0F5D: the effect's + 0x1C above 0, the shader path), the
//     colour is TheFireManager's (RW 0xDE4EFC) slot 0x3C at the cell's centre (trunc((x + 0.5) * cellW), trunc((y + 0.5) * cellH), SSE) and the factor
//     255; else (the fixed-function path, not ported) the colour is TaintColor / ElvenWoodColor with a per-cell pattern, times the fire colour, the
//     factor the shroud level.
//   * TheFireManager slot 0x3C RW 0x497F60(out, x, y): white without TheFireLogicSystem; else the cell type (RW 0x686459): 1 (burning) -> the manager's
//     + 0x20 (FuelIndicatorColor, copied from the Fire block + 0x14 at RW 0x70C5ED), 2 (burnt out) -> + 0x14 (BurntTerrainColor, from + 0x08 at RW
//     0x70C5E2), else white.
//   * terrain.fxo (Default ps_2_0, every pass): a = mask.a; d = 2|a - 0.5|; the taint texture is TaintLow (TSTaintMed.tga) for a <= 0.5, else TaintHigh
//     (TSElvenMed.tga), both sampled at the layer's own UV (WRAP, LINEAR); albedo = Taint.IsEnabled ? mask.rgb * lerp(albedo, taint, d) : albedo. The
//     mask's UV (vs): (world.xy + Offset) * Scale with Offset = cell size - W3DTaint + 0x2C / + 0x30 (RW 0x4E2033) and Scale = 1 / (texture size *
//     cell size) (RW 0x4E2096); IsEnabled = TaintOn (RW 0x4E2002); the mask sampler CLAMP, LINEAR.
//   * the texture (RW 0x473E83 .. 0x473EE0, the shroud's layout): numCells + 2 texels a side; a dirty border (+ 0x35) fills it all through RW 0x47321C with
//     the border level (+ 0x36 = GameData TaintAlpha, RW 0x46CEC7) in all four bytes, then the cells are copied at texel (1, 1).
// INFERENCE (S-5261): the mask is rebuilt from the presented TaintView whenever TheTaintManager or the fire grid moved (retail writes the changed cells).

#pragma once

#include "Common/INIDataTypes.h"

#include <cstdint>
#include <vector>

struct FireSetting;

// the logic's ground the mask is made from, as the presented snapshot carries it (LogicSnapshot::taint)
struct TaintView
{
	std::uint64_t taintVersion = 0, fireVersion = 0;
	int countX = 0, countY = 0;   ///< TheTaintManager's cells
	float originX = 0.0f, originY = 0.0f, cellSize = 1.0f;
	std::vector<std::uint8_t> values;    ///< the cell values, row-major
	std::vector<std::uint8_t> fireTypes; ///< TheFireLogicSystem's type at each cell's centre (RW 0x686459), row-major
};

namespace W3DTaintMask
{
// RW 0x497F60
RGBColor fireColour(const FireSetting &fire, int type);
// RW 0x4738F3 .. 0x4739A4 (the shader path): value << 24 | (R * 255) << 16 | (G * 255) << 8 | B * 255, each product truncated (_ftol)
std::uint32_t texel(int value, const RGBColor &colour);
// RW 0x473905 .. 0x473928: trunc((n + 0.5) * cellSize) in SSE
int cellCentre(int n, float cellSize);
// the texture: (countX + 2) x (countY + 2) ARGB texels, the border `borderLevel` (TaintAlpha) in every byte
void build(const TaintView &view, const FireSetting &fire, std::uint8_t borderLevel, std::vector<std::uint32_t> &texels, int &width, int &height);
} // namespace W3DTaintMask
