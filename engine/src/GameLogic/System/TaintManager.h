// OpenBFME. GPL-3.0.
//
// TaintManager (TOP20 "Fire, scorches" follow-up, stops S-5260 / S-921 / S-856): RotWK's TheTaintManager (RW global 0xDE435C, the name RW 0xBFECFC, made
// by RW 0x63BF6C .. 0x63BF81 with 0x14 bytes, constructor RW 0xAD4A50, vtable RW 0xD006B4, snapshot vtable RW 0xD006A4 at + 0xC, save block
// "CHUNK_TaintManager" RW 0xC1A4CC). The manager forwards to a grid at + 0x10 (0x30 bytes, constructor RW 0xAD55F0). The ground under Angmar's Taint
// and Lothlorien's Elven Wood is a value per cell: 0x80 is untouched, below it the taint, above it the elven wood. The Taint / Elven Wood spells paint it,
// AttributeModifierAuraUpdate's RequiredConditions TAINT / ELVEN_WOOD read it, and the radius clear (TerrainLogic RW 0x683393) paints it back to 0x80.
//
// TARGET FACTS (RotWK game.dat, caveat S-001; read in this lane):
//   * the grid: + 0x00 .. + 0x14 the region (Region3D lo, hi), + 0x18 the cell size (1.0 from the constructor), + 0x1C its inverse, + 0x20 / + 0x24 the
//     cell counts, + 0x28 the cells, + 0x2C the client callback (x, y, value; set by RW 0xAD4960 -> 0xA81470 from RW 0x6A859D / 0x6A8EFE with RW
//     0x6D8B6E: TheTacticalView (RW 0xDE4418) slot 0x13C and TheRadar (RW 0xDE4AB8) slot 0x2C).
//   * a cell (12 bytes, constructor RW 0xAD4D00): + 0 the value byte (0x80), + 4 the state (0), + 8 the owner, an ObjectID (-1).
//   * the cell write RW 0xAD5740(value, set, owner): an owner other than -1 painting 0x80 leaves a cell of another owner alone; without `set` the value is
//     added to the cell's; clamped to 0 .. 255 and stored as a byte; 0x80 makes the state 0 and the owner -1, else the state is 1 above 0x80 (the elven
//     wood) and 2 below (the taint) and the owner is the given one.
//   * paint RW 0xAD4C10(position, radius, value, set, owner): r = ceil(radius * inverse) (x87, stored as a float), cy = floor((y - lo.y) * inverse), cx =
//     floor((x - lo.x) * inverse); RW 0xAD56E0: nothing for r < 0, or for value 0 without `set`; the midpoint circle RW 0xAD52B0 of rows RW 0xAD5040; a
//     row's cells are clamped into the grid (RW 0xAD4E00), each written and then the callback is called with the row's x counting from the UNCLAMPED x0.
//   * the state at a position RW 0xAD49B0 -> 0xAD4FA0: the cell (RW 0xAD4D30, floor((p - lo) * inverse), 0 outside); its state, the owner through the out
//     argument; outside: 0 and the out argument untouched. The value at a cell RW 0xAD49A0 -> 0xAD4FD0 (0x80 outside), at a position RW 0xAD5010.
//   * the region RW 0xAD4970 -> 0xAD5660(region, cellSize): a cell size of 0 or less keeps + 0x18; a negative extent changes nothing; then RW 0xAD5340: an
//     extent below 1.0 is widened to 1.0 (RW 0xBD1908), the inverse 1 / size, the counts max(1, ceil(extent * inverse)), every new cell takes the VALUE
//     (only the value byte: the state and owner stay 0 / -1) of the old cell under it (floor((n * size + newLo - oldLo) * oldInverse), the counter an
//     fild of the unsigned index), then the region, size, inverse and counts are stored. No callback.
//   * GameLogic's init (RW 0x62CFE3 .. 0x62CFFC): the region of zeros with GameData PartitionCellSize (GlobalData + 0xD4); the new game (RW 0x62FCE2 ..
//     0x62FD01, after ThePartitionManager's region, load progress 0x11): the map's extent with size 0 (kept), then RW 0xAD4980 -> 0xAD4F00: the callback
//     for every cell in order. init / reset (vtable slots 4 / 0x24, RW 0xAD4BB0): the region of zeros with size 0. update (slot 0x28, RW 0xAD4950) and
//     the snapshot's crc (RW 0xA88AC0) do nothing.
//   * xfer RW 0xAD4B80: version 1, then the grid RW 0xAD50B0: version 1, the region (Xfer slot 0x58), size and inverse (reals), the counts (ints), an
//     unsigned int 0; a load makes counts.x * counts.y default cells; every cell in rows (RW 0xAD57E0): version 2, the value byte, the state (4 bytes of
//     user data), the owner (int, version 2); a load then calls the callback for every cell (RW 0xAD4F00).
// INFERENCE (S-5261): the region's z (TerrainLogic's extent) is 0 here; the port keeps the save in its own block "obfme:TaintManager" with retail's layout.

#pragma once

#include "Common/INIDataTypes.h"

#include <cstdint>
#include <functional>
#include <vector>

class StateHasher;
class Xfer;

class TaintManager
{
public:
	static constexpr std::uint8_t NEUTRAL = 0x80; // RW 0xAD4D02, 0xAD5006
	enum State : std::int32_t
	{
		STATE_NONE = 0,
		STATE_ELVEN_WOOD = 1, ///< value above 0x80 (RW 0xAD5792)
		STATE_TAINT = 2,      ///< value below 0x80
	};
	struct Cell
	{
		std::uint8_t value = NEUTRAL; ///< + 0
		std::int32_t state = STATE_NONE; ///< + 4
		std::int32_t owner = -1;      ///< + 8, an ObjectID
	};
	struct Region
	{
		float loX = 0, loY = 0, loZ = 0, hiX = 0, hiY = 0, hiZ = 0;
	};
	using Callback = std::function<void(int x, int y, int value)>;

	// RW 0xAD55F0: the region of zeros at size 1.0
	TaintManager();

	// RW 0xAD5660
	void setRegion(const Region &region, float cellSize);
	// RW 0xAD4BB0 (init, reset): the region of zeros with the kept size
	void reset();
	// RW 0xAD4C10
	void paint(const Coord3D &pos, float radius, int value, bool set, std::int32_t owner);
	// RW 0xAD4FA0: the state of the cell at the position; `owner` is written only inside the grid
	std::int32_t stateAt(const Coord3D &pos, std::int32_t *owner) const;
	// RW 0xAD4FD0 / RW 0xAD5010
	int valueAt(int cx, int cy) const;
	int valueAt(const Coord3D &pos) const;
	// RW 0xAD4F00
	void notifyAll() const;

	// the client callback (RW 0xA81470)
	void setCallback(Callback cb) { m_callback = std::move(cb); }

	const Region &region() const { return m_region; }
	float cellSize() const { return m_cellSize; }
	float cellSizeInv() const { return m_cellSizeInv; }
	int countX() const { return m_countX; }
	int countY() const { return m_countY; }
	const Cell &cell(int cx, int cy) const { return m_cells[(size_t)cy * (size_t)m_countX + (size_t)cx]; }
	// whether any cell was ever written: the hash section exists only then (a game without taint keeps its hash)
	bool anyPainted() const { return m_painted; }

	void xfer(Xfer &x);
	void crc(StateHasher &h) const;

private:
	// RW 0xAD5340
	void resize(const Region &region, float cellSize);
	// RW 0xAD52B0 / 0xAD5040 / 0xAD5740
	void circle(int cx, int cy, int r, int value, bool set, std::int32_t owner);
	void row(int x0, int x1, int y, int value, bool set, std::int32_t owner);
	static void writeCell(Cell &c, int value, bool set, std::int32_t owner);
	const Cell *cellAt(const Coord3D &pos) const;

	Region m_region;
	float m_cellSize = 1.0f;
	float m_cellSizeInv = 0.0f;
	int m_countX = 0, m_countY = 0;
	std::vector<Cell> m_cells;
	Callback m_callback;
	bool m_painted = false;
};
