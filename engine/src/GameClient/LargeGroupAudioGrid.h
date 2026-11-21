// OpenBFME. GPL-3.0. Client-only spatial weights of one LargeGroupAudio Sound
// block.
#pragma once
#include "Common/Audio/LargeGroupAudio.h"
#include "GameLogic/LargeGroupAudioLink.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

class LargeGroupAudioGrid
{
public:
	// Centres in half-cell units: parity encodes the grid, so neighbouring
	// centres are one step apart and no two grids share a cell identifier.
	using Cell = std::pair<int, int>;
	using Selection = std::set<Cell>;
	LargeGroupAudioGrid(float size, float maxX, float maxY) : width(size)
	{
		for (int g = 0; g < 4; ++g) // RW 0x97037B, RW 0x9D763E
		{
			cols[g] = int(std::ceil((maxX + (g & 1) * width * 0.5f) / width));
			rows[g] = int(std::ceil((maxY + (g >> 1) * width * 0.5f) / width));
		}
	}
	// RW 0x971FA7: after frame one at most one block per pass allocates its grids.
	void advance(unsigned start, unsigned stop, unsigned frame, bool &allocated)
	{
		handoffs.clear();
		if (!active)
		{
			if (total < start || (allocated && frame > 1))
				return;
			active = true;
			allocated = true;
			// RW 0x9715BA: replay each position/weight entry, including duplicates.
			for (const auto &[x, y, w] : waiting)
				change(x, y, w, true);
			waiting.clear();
		}
		else if (total <= 0)
		{
			active = false;
			weights.clear();
			dirty.clear();
			playing.clear();
			return;
		}
		if (dirty.empty())
			return;
		Selection locked;
		auto lock = [&](Cell c) {
			locked.insert(c);
			neighbours(c, [&](Cell n, int, int) { locked.insert(n); });
		};
		auto take = [&](Cell c) {
			playing.insert(c);
			lock(c);
		};
		// A changed playing cell below stop or any playing neighbour strictly lighter
		// than a changed cell loses its event. Detect before selecting replacements.
		std::vector<Cell> displaced;
		auto displace = [&](Cell c) {
			if (playing.erase(c))
				displaced.push_back(c);
		};
		for (Cell c : dirty)
		{
			unsigned w = weight(c);
			if (playing.count(c) && w < stop)
				displace(c);
			neighbours(c, [&](Cell n, int, int) {
				if (weight(n) > w)
					displace(c);
				else if (weight(n) < w)
					displace(n);
			});
		}
		for (Cell c : playing)
			lock(c);
		auto better = [&](Cell a, Cell b) { // RW 0x9701ED
			return weight(a) != weight(b) ? weight(a) > weight(b) : missing(a) < missing(b);
		};
		// RW 0x971FA7 at 0x972139: try each displaced event on its eight neighbours,
		// then its old cell. At equal rank keep the first. This precedes fresh starts.
		for (Cell old : displaced)
		{
			Cell best;
			bool found = false;
			auto consider = [&](Cell c) {
				if (weight(c) < start || locked.count(c))
					return;
				dirty.insert(c);
				if (!found || better(c, best))
				{
					best = c;
					found = true;
				}
			};
			neighbours(old, [&](Cell n, int, int) { consider(n); });
			consider(old);
			if (found)
			{
				take(best);
				handoffs.emplace(best, old);
			}
		}
		std::vector<Cell> candidates(dirty.begin(), dirty.end());
		std::stable_sort(candidates.begin(), candidates.end(), better);
		for (Cell c : candidates)
		{
			if (weight(c) < start)
				break;
			if (!locked.count(c))
				take(c); // RW 0x9D7D78 overlap locks
		}
		dirty.clear();
	}
	Coord3D position(Cell c) const // RW 0x9D7DF6: bias centre by the eight
								   // neighbours, independently per axis.
	{
		float tx = 0, ty = 0, sx = 0, sy = 0;
		neighbours(c, [&](Cell n, int dx, int dy) {
			float w = weight(n);
			if (dx && dy)
				w = static_cast<float>(w * 0.7071067811865476);
			if (dx)
			{
				tx += w;
				sx += dx * w;
			}
			if (dy)
			{
				ty += w;
				sy += dy * w;
			}
		});
		return {c.first * width * 0.5f + (tx ? sx / tx * width / 3 : 0), c.second * width * 0.5f + (ty ? sy / ty * width / 3 : 0), 0};
	}
	// RW 0x7EF032 / RW 0x7EEF0D / RW 0x7EF0A1: apply each notification to this
	// Sound only.
	void notify(const LargeGroupAudioMap &map, const LargeGroupAudioSound &sound, const LargeGroupAudioEvent &e, std::int32_t gate)
	{
		if (!keyed(sound, e.keys))
			return;
		const bool nowQ = masks(map, e.conditions, e.status) && !(map.ignoreStealthedUnits && e.stealthed);
		const bool oldQ =
			masks(map, e.storedConditions, e.storedStatus) && !(map.ignoreStealthedUnits && e.storedStealthed) && (std::int32_t)e.storedFrame > gate;
		switch (e.kind)
		{
		case LargeGroupAudioEvent::ADD:
			if (nowQ)
			{
				change(e.x, e.y, e.weight, true);
				++members;
				total += e.weight;
			}
			break;
		case LargeGroupAudioEvent::UPDATE:
			if (nowQ && oldQ)
			{
				if (!(e.x == e.storedX && e.y == e.storedY))
				{
					// RW 0x971157: if the two lookups name the same cell leave its word alone (also after overflow).
					if (!active)
					{
						change(e.storedX, e.storedY, e.weight, false);
						change(e.x, e.y, e.weight, true);
					}
					else
						for (int g = 0; g < 4; ++g)
						{
							Cell old, now;
							bool hasOld = lookup(g, e.storedX, e.storedY, old), hasNow = lookup(g, e.x, e.y, now);
							if (hasOld && hasNow && old == now)
								continue;
							if (hasOld)
								changeCell(old, e.weight, false);
							if (hasNow)
								changeCell(now, e.weight, true);
						}
				}
			}
			else if (nowQ)
			{
				change(e.x, e.y, e.weight, true);
				++members;
				total += e.weight;
			}
			else if (oldQ)
			{
				change(e.storedX, e.storedY, e.weight, false);
				if (members)
					--members;
				total -= e.weight;
			}
			break;
		case LargeGroupAudioEvent::REMOVE:
			if (oldQ)
			{
				change(e.storedX, e.storedY, e.weight, false);
				if (members)
					--members;
				total -= e.weight;
			}
			break;
		}
	}
	size_t members = 0;
	Selection playing;
	std::map<Cell, Cell> handoffs; // recipient -> donor of this pass

private:
	// INFERENCE: allocation order (RW 0x9715BA / RW 0x9D7BC8) models the address order
	// RW 0x9709DA -> RW 0x42C22B for equal candidates: grid, then row-major index.
	struct AllocationOrder
	{
		bool operator()(Cell a, Cell b) const
		{
			if (grid(a) != grid(b))
				return grid(a) < grid(b);
			return std::tie(a.second, a.first) < std::tie(b.second, b.first);
		}
	};
	std::set<Cell, AllocationOrder> dirty;
	std::multiset<std::tuple<float, float, unsigned short>> waiting;
	int missing(Cell c) const
	{ // RW 0x9D7D1E: upper nibble counts absent neighbours
		int result = 8;
		neighbours(c, [&](Cell, int, int) { --result; });
		return result;
	}
	void change(float x, float y, unsigned short w, bool add)
	{
		if (!active)
		{
			if (add)
				waiting.emplace(x, y, w);
			else
			{
				auto i = waiting.find({x, y, w});
				if (i != waiting.end())
					waiting.erase(i);
			}
			return;
		}
		for (int g = 0; g < 4; ++g)
		{
			Cell c;
			if (lookup(g, x, y, c))
				changeCell(c, w, add);
		}
	}
	void changeCell(Cell c, unsigned short amount, bool add)
	{
		auto &w = weights[c]; // RW 0x9D7DCD stores the low word; RW 0x9D7D5B clamps subtraction.
		w = add ? static_cast<unsigned short>(w + amount) : (w >= amount ? w - amount : 0);
		if (!w)
			weights.erase(c);
		dirty.insert(c); // RW 0x971121
	}
	static bool masks(const LargeGroupAudioMap &map, const ModelConditionMask &mc, const ObjectStatusMaskType &st)
	{
		for (size_t w = 0; w < mc.size(); ++w)
		{
			if ((mc[w] & map.requiredModelConditionFlags[w]) != map.requiredModelConditionFlags[w] ||
				(mc[w] & map.excludedModelConditionFlags[w]) != 0)
			{
				return false;
			}
		}
		for (size_t w = 0; w < st.size(); ++w)
		{
			if ((st[w] & map.requiredObjectStatusBits[w]) != map.requiredObjectStatusBits[w] || (st[w] & map.excludedObjectStatusBits[w]) != 0)
			{
				return false;
			}
		}
		return true;
	}
	static bool keyed(const LargeGroupAudioSound &snd, const std::vector<std::string> *keys)
	{
		if (!keys)
		{
			return false;
		}
		for (const std::string &k : snd.keys)
		{
			for (const std::string &mk : *keys)
			{
				if (k == mk) // RW 0x970617: the interned keys (exact bytes, RW 0x7EE97A)
				{
					return true;
				}
			}
		}
		return false;
	}
	int64_t total = 0;
	bool active = false;
	float width;
	int cols[4], rows[4];
	std::map<Cell, unsigned short> weights;
	static int grid(Cell c)
	{
		return (1 - (c.first & 1)) + 2 * (1 - (c.second & 1));
	}
	unsigned weight(Cell c) const
	{
		auto i = weights.find(c);
		return i == weights.end() ? 0 : i->second;
	}
	bool lookup(int g, float x, float y,
				Cell &c) const // RW 0x9D76E3 (truncate, not floor)
	{
		int ix = int((x + (g & 1) * width * 0.5f) / width), iy = int((y + (g >> 1) * width * 0.5f) / width);
		if (ix < 0 || ix >= cols[g] || iy < 0 || iy >= rows[g])
			return false;
		c = {2 * ix + 1 - (g & 1), 2 * iy + 1 - (g >> 1)};
		return true;
	}
	template <class F> void neighbours(Cell c, F f) const // RW 0x9D7745
	{
		static constexpr int offsets[][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
		for (auto &d : offsets)
		{
			Cell target{c.first + d[0], c.second + d[1]}, n;
			if (lookup(grid(target), target.first * width * 0.5f, target.second * width * 0.5f, n))
				f(n, d[0], d[1]);
		}
	}
};
