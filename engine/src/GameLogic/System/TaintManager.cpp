// OpenBFME. GPL-3.0.
//
// TaintManager: RotWK's TheTaintManager grid (TaintManager.h holds the target facts).

#include "GameLogic/System/TaintManager.h"

#include "Common/StateHash.h"
#include "Common/Xfer.h"
#include "GameLogic/SimMath.h"

#include <string>

TaintManager::TaintManager()
{
	// RW 0xAD55F0: the region of zeros, size 1.0, then RW 0xAD5340 with them
	resize(Region{}, 1.0f);
}

// RW 0xAD5660
void TaintManager::setRegion(const Region &region, float cellSize)
{
	// RW 0xAD5660 .. 0xAD5675: a size of 0 or less (fcomp with 0.0, RW 0xC1B594) takes + 0x18
	if (!(cellSize > 0.0f))
	{
		cellSize = m_cellSize;
	}
	// RW 0xAD567D .. 0xAD56A0: a negative extent (hi - lo below 0.0) changes nothing; x87 under precision control 24
	if (SimMath::pc24Sub(region.hiX, region.loX) < 0.0f || SimMath::pc24Sub(region.hiY, region.loY) < 0.0f)
	{
		return;
	}
	resize(region, cellSize);
}

// RW 0xAD5340
void TaintManager::resize(const Region &in, float cellSize)
{
	Region region = in;
	// RW 0xAD535E .. 0xAD539E: an extent below 1.0 (RW 0xBD1908) is widened to lo + 1.0
	if (SimMath::pc24Sub(region.hiX, region.loX) < 1.0f)
	{
		region.hiX = SimMath::pc24Add(region.loX, 1.0f);
	}
	if (SimMath::pc24Sub(region.hiY, region.loY) < 1.0f)
	{
		region.hiY = SimMath::pc24Add(region.loY, 1.0f);
	}
	// RW 0xAD53A1 .. 0xAD541D: inverse = 1 / size (stored as a float); counts = max(1, ceil(extent * inverse))
	const float inv = SimMath::pc24Div(1.0f, cellSize);
	int nx = SimMath::ceilToInt(SimMath::pc24Mul(SimMath::pc24Sub(region.hiX, region.loX), inv));
	int ny = SimMath::ceilToInt(SimMath::pc24Mul(SimMath::pc24Sub(region.hiY, region.loY), inv));
	if (nx < 1)
	{
		nx = 1;
	}
	if (ny < 1)
	{
		ny = 1;
	}
	std::vector<Cell> cells((size_t)nx * (size_t)ny); // RW 0xAD5447: constructor RW 0xAD4D00 for each
	// RW 0xAD5490 .. 0xAD5560: every new cell takes the value byte of the old cell under it
	for (int y = 0; y < ny; ++y)
	{
		const int oy = SimMath::floorToInt(SimMath::pc24Mul(SimMath::pc24Sub(SimMath::pc24Add(SimMath::pc24MulD((double)(std::uint32_t)y, cellSize), region.loY),
			m_region.loY), m_cellSizeInv));
		if (oy < 0 || oy >= m_countY)
		{
			continue;
		}
		for (int x = 0; x < nx; ++x)
		{
			const int ox = SimMath::floorToInt(SimMath::pc24Mul(SimMath::pc24Sub(SimMath::pc24Add(SimMath::pc24MulD((double)(std::uint32_t)x, cellSize),
				region.loX), m_region.loX), m_cellSizeInv));
			if (ox >= 0 && ox < m_countX)
			{
				cells[(size_t)y * (size_t)nx + (size_t)x].value = m_cells[(size_t)oy * (size_t)m_countX + (size_t)ox].value;
			}
		}
	}
	// RW 0xAD5566 .. 0xAD55D3
	m_cells.swap(cells);
	m_region = region;
	m_cellSizeInv = inv;
	m_countX = nx;
	m_countY = ny;
	m_cellSize = cellSize;
	recount();
}

void TaintManager::recount()
{
	m_nonDefault = 0;
	for (const Cell &c : m_cells)
	{
		m_nonDefault += isDefault(c) ? 0 : 1;
	}
}

// RW 0xAD4BB0 (vtable slot 4; reset slot 0x24 -> RW 0xA39010 -> slot 4)
void TaintManager::reset()
{
	setRegion(Region{}, 0.0f); // the one cell left keeps the old (0, 0) value (RW 0xAD5534)
}

// RW 0xAD4C10
void TaintManager::paint(const Coord3D &pos, float radius, int value, bool set, std::int32_t owner)
{
	// RW 0xAD4C13 .. 0xAD4C38: ceil(radius * inverse), stored as a float, then fistp
	const int r = (int)SimMath::fistp32((double)(float)SimMath::ceilD((double)SimMath::pc24Mul(radius, m_cellSizeInv)));
	// RW 0xAD4C43 .. 0xAD4C63: floor((y - lo.y) * inverse), then RW 0xAD4C67 .. 0xAD4C88: x
	const int cy = SimMath::floorToInt(SimMath::pc24Mul(SimMath::pc24Sub(pos.y, m_region.loY), m_cellSizeInv));
	const int cx = SimMath::floorToInt(SimMath::pc24Mul(SimMath::pc24Sub(pos.x, m_region.loX), m_cellSizeInv));
	// RW 0xAD56E0: nothing for a negative radius, or a value of 0 without `set`
	if (r < 0)
	{
		return;
	}
	if (value == 0 && !set)
	{
		return;
	}
	circle(cx, cy, r, value, set, owner);
}

// RW 0xAD52B0: the midpoint circle of rows
void TaintManager::circle(int cx, int cy, int r, int value, bool set, std::int32_t owner)
{
	int a = 0;       // [esp + 0x10]
	int x0 = cx;     // ebp
	int x1 = cx;     // ebx
	int s = r;       // esi
	int d = 2 - 2 * r; // edi
	for (;;)
	{
		// RW 0xAD52D1 .. 0xAD5322
		if (d + s > 0)
		{
			if (s == 0 && r == 1)
			{
				++a;
				++x1;
				--x0;
			}
			row(x0, x1, cy + s, value, set, owner);
			if (s == 0)
			{
				return; // RW 0xAD52FD
			}
			row(x0, x1, cy - s, value, set, owner);
			--s;
			d += 1 - 2 * s;
		}
		// RW 0xAD5326 .. 0xAD5331
		if (a > d)
		{
			++a;
			++x1;
			--x0;
			d += 2 * a + 1;
		}
	}
}

// RW 0xAD5040 (the row's cells through RW 0xAD4E00)
void TaintManager::row(int x0, int x1, int y, int value, bool set, std::int32_t owner)
{
	// RW 0xAD4E00: no cell for x1 < 0, x0 >= the width, y outside; else from max(x0, 0) to min(x1 + 1, width)
	if (x1 < 0 || x0 >= m_countX || y < 0 || y >= m_countY)
	{
		return;
	}
	const int begin = x0 > 0 ? x0 : 0;
	const int end = x1 < m_countX ? x1 + 1 : m_countX;
	int x = x0; // RW 0xAD5070 .. 0xAD509B: the callback's x counts from the unclamped x0 (ebx)
	for (int i = begin; i < end; ++i, ++x)
	{
		Cell &c = m_cells[(size_t)y * (size_t)m_countX + (size_t)i];
		const bool wasDefault = isDefault(c);
		writeCell(c, value, set, owner);
		const bool nowDefault = isDefault(c);
		if (wasDefault != nowDefault)
		{
			m_nonDefault = nowDefault ? m_nonDefault - 1 : m_nonDefault + 1;
		}
		if (m_callback)
		{
			m_callback(x, y, c.value); // RW 0xAD508E: the grid's + 0x2C
		}
	}
}

// RW 0xAD5740
void TaintManager::writeCell(Cell &c, int value, bool set, std::int32_t owner)
{
	if (owner != -1 && value == NEUTRAL && owner != c.owner)
	{
		return;
	}
	if (!set)
	{
		value += c.value;
	}
	if (value < 0)
	{
		value = 0;
	}
	else if (value > 0xFF)
	{
		value = 0xFF;
	}
	c.value = (std::uint8_t)value;
	if (c.value == NEUTRAL)
	{
		c.owner = -1;
		c.state = STATE_NONE;
		return;
	}
	c.state = c.value > NEUTRAL ? STATE_ELVEN_WOOD : STATE_TAINT;
	c.owner = owner;
}

// RW 0xAD4D30
const TaintManager::Cell *TaintManager::cellAt(const Coord3D &pos) const
{
	const int cx = SimMath::floorToInt(SimMath::pc24Mul(SimMath::pc24Sub(pos.x, m_region.loX), m_cellSizeInv));
	if (cx < 0 || cx >= m_countX)
	{
		return nullptr;
	}
	const int cy = SimMath::floorToInt(SimMath::pc24Mul(SimMath::pc24Sub(pos.y, m_region.loY), m_cellSizeInv));
	if (cy < 0 || cy >= m_countY)
	{
		return nullptr;
	}
	return &m_cells[(size_t)cy * (size_t)m_countX + (size_t)cx];
}

// RW 0xAD4FA0
std::int32_t TaintManager::stateAt(const Coord3D &pos, std::int32_t *owner) const
{
	const Cell *c = cellAt(pos);
	if (!c)
	{
		return STATE_NONE;
	}
	if (owner)
	{
		*owner = c->owner;
	}
	return c->state;
}

// RW 0xAD4FD0
int TaintManager::valueAt(int cx, int cy) const
{
	if (cx < 0 || cx >= m_countX || cy < 0 || cy >= m_countY)
	{
		return NEUTRAL;
	}
	return cell(cx, cy).value;
}

// RW 0xAD5010
int TaintManager::valueAt(const Coord3D &pos) const
{
	const Cell *c = cellAt(pos);
	return c ? c->value : NEUTRAL;
}

// RW 0xAD4F00
void TaintManager::notifyAll() const
{
	if (!m_callback)
	{
		return;
	}
	for (int y = 0; y < m_countY; ++y)
	{
		for (int x = 0; x < m_countX; ++x)
		{
			m_callback(x, y, cell(x, y).value);
		}
	}
}

// RW 0xAD4B80 (version 1) -> RW 0xAD50B0 (version 1) -> RW 0xAD57E0 (version 2 per cell)
void TaintManager::xfer(Xfer &x)
{
	const bool loading = x.isLoading();
	XferVersion v{ 1, 1 };
	x.xferVersion(v); // RW 0xAD4B9C
	XferVersion gv{ 1, 1 };
	x.xferVersion(gv); // RW 0xAD50F4
	Region region = m_region;
	x.xferReal(&region.loX); // RW 0xAD50FC: the region (Xfer slot 0x58)
	x.xferReal(&region.loY);
	x.xferReal(&region.loZ);
	x.xferReal(&region.hiX);
	x.xferReal(&region.hiY);
	x.xferReal(&region.hiZ);
	float size = m_cellSize, inv = m_cellSizeInv;
	x.xferReal(&size); // RW 0xAD5107 (+ 0x18)
	x.xferReal(&inv);  // RW 0xAD5112 (+ 0x1C)
	std::int32_t nx = m_countX, ny = m_countY;
	x.xferInt(&nx); // RW 0xAD511D (+ 0x20)
	x.xferInt(&ny); // RW 0xAD5128 (+ 0x24)
	std::uint32_t zero = 0;
	x.xferUnsignedInt(&zero); // RW 0xAD513C
	std::vector<Cell> cells;
	if (loading)
	{
		if (nx < 0 || ny < 0)
		{
			throw XferException("taint manager: negative cell counts " + std::to_string(nx) + " x " + std::to_string(ny));
		}
		// one cell at its shortest: version (1), value (1), state (4) (version 1 has no owner); checked before anything is allocated
		std::uint32_t count = (std::uint32_t)nx * (std::uint32_t)ny;
		if (ny != 0 && count / (std::uint32_t)ny != (std::uint32_t)nx)
		{
			throw XferException("taint manager: cell counts " + std::to_string(nx) + " x " + std::to_string(ny) + " overflow");
		}
		x.checkLoadedCount(count, 6, sizeof(Cell));
		cells.resize(count);
	}
	std::vector<Cell> &target = loading ? cells : m_cells;
	for (Cell &c : target)
	{
		XferVersion cv{ 1, 2 };
		x.xferVersion(cv); // RW 0xAD57FC
		x.xferUnsignedByte(&c.value); // RW 0xAD5804 (Xfer slot 0x88)
		x.xferUser(&c.state, 4);      // RW 0xAD5814
		if (cv.current >= 2)
		{
			x.xferInt(&c.owner); // RW 0xAD5826
		}
	}
	if (loading)
	{
		m_region = region;
		m_cellSize = size;
		m_cellSizeInv = inv;
		m_countX = nx;
		m_countY = ny;
		m_cells.swap(cells);
		recount();
		notifyAll(); // RW 0xAD5290
	}
}

void TaintManager::crc(StateHasher &h) const
{
	h.addFloat(m_region.loX);
	h.addFloat(m_region.loY);
	h.addFloat(m_region.hiX);
	h.addFloat(m_region.hiY);
	h.addFloat(m_cellSize);
	h.addI32(m_countX);
	h.addI32(m_countY);
	for (const Cell &c : m_cells)
	{
		h.addU32(c.value);
		h.addI32(c.state);
		h.addI32(c.owner);
	}
}
