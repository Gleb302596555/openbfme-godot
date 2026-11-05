// OpenBFME. GPL-3.0.
//
// TerrainTrees. See GameLogic/Map/TerrainTrees.h for the target facts. Lane SPELL-3.

#include "GameLogic/Map/TerrainTrees.h"
#include "GameLogic/Map/FireLogicSystem.h" 

#include "Common/Thing/ThingFactory.h"
#include "Common/Thing/ThingTemplate.h"
#include "Common/Xfer.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DTreeDraw.h"
#include "GameLogic/Economy.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/Object/PartitionManager.h"
#include "GameLogic/ObjectTemplateInfo.h"
#include "GameLogic/Module/NotifyCrushModules.h"
#include "GameLogic/Module/SquishCollide.h"
#include "GameLogic/SimMath.h"

#include <stdexcept>
#include <variant>

namespace
{
const float kBucketScale = 49.9f;    // RW 0xBE4AD0
const float kQueryMargin = 7.0f;     // RW 0xBE4AD4
const float kCritterRadius = 5.0f;   // RW 0xBDAE58
const float kPropClearRadius = 25.0f;        // RW 0xC69574 (lane TREES1)
const float kCrushTreeHeight = 20.0f;        // RW 0xBDBC6C (lane TREES1)
const float kConstructionTreeHeight = 35.0f; // RW 0xBE4CB8 (lane TREES1)
const float kConstructionTreeRadius = 14.0f; // RW 0xBDD368 (lane TREES1)
const float kMorphAreaMargin = 50.0f;        // RW 0xBD88C4 (S-921)

bool hasTreeDraw(const ThingTemplate &tmpl)
{
	for (const ThingTemplate::Nugget &n : tmpl.drawModules().nuggets())
	{
		if (n.name == "W3DTreeDraw")
		{
			return true;
		}
	}
	return false;
}

bool boolField(const ThingTemplate &tmpl, const char *name)
{
	const FieldValue *v = tmpl.getFinalOverride()->findField(name);
	const bool *b = v ? std::get_if<bool>(v) : nullptr;
	return b && *b;
}

// (clamped - lo) / (hi - lo) * 49.9 on the x87 (PC24), then the CRT floor or ceil of the double and fistp of the stored float (RW 0x67D46D .. 0x67D4BE)
int gridIndex(float p, float lo, float hi, bool up)
{
	const float t = SimMath::pc24Mul(SimMath::pc24Div(SimMath::pc24Sub(p, lo), SimMath::pc24Sub(hi, lo)), kBucketScale);
	const double r = up ? SimMath::ceilD((double)t) : SimMath::floorD((double)t);
	return SimMath::fistp32((double)SimMath::fstpDword(r));
}
} // namespace

TerrainTrees::TerrainTrees(GameLogic &logic) : m_logic(logic)
{
	reset();
}

void TerrainTrees::reset()
{
	m_records.clear();
	m_events.clear();
	m_heads.assign((size_t)BUCKETS * BUCKETS, (std::int16_t)-1);
	m_hasExtent = false;
	m_nextIndex = 0;
	m_toppleSpeed = 0.0f;
	m_changedFrame = 0;
	m_changed = 0;
	m_morphing = 0;
	m_start.clear();
}

// lane SAVE-4e: the records as the map made them, kept before the first one changes (a load restores them, then applies the snapshot's changed records)
void TerrainTrees::keepStart()
{
	if (m_start.empty() && m_changed == 0)
	{
		m_start = m_records;
	}
}

void TerrainTrees::setExtent(float loX, float loY, float hiX, float hiY)
{
	m_lo[0] = loX;
	m_lo[1] = loY;
	m_hi[0] = hiX;
	m_hi[1] = hiY;
	m_hasExtent = true;
}

// RW 0x67D400
int TerrainTrees::bucketOf(float x, float y) const
{
	// the clamps (SSE comiss): lo first, then hi
	x = m_lo[0] > x ? m_lo[0] : x;
	y = m_lo[1] > y ? m_lo[1] : y;
	x = x > m_hi[0] ? m_hi[0] : x;
	y = y > m_hi[1] ? m_hi[1] : y;
	return gridIndex(y, m_lo[1], m_hi[1], false) * BUCKETS + gridIndex(x, m_lo[0], m_hi[0], false);
}

// RW 0x683D89
bool TerrainTrees::addTree(const ThingTemplate &tmpl, const Coord3D &position, std::uint32_t clientId)
{
	m_changedFrame = m_logic.getFrame(); // RW 0x683D9F
	m_logic.fireLogic().registerTree(clientId, position, &tmpl); // RW 0x683F80
	if (!hasTreeDraw(tmpl) || !m_hasExtent)
	{
		return false; // RW 0x683DF6: the debug report only
	}
	const int bucket = bucketOf(position.x, position.y);
	if (bucket < 0 || bucket >= BUCKETS * BUCKETS)
	{
		return false; // the clamps keep the index inside; a degenerate extent (hi == lo) does not
	}
	Record r;
	r.position = position;
	r.clientId = clientId;
	r.index = m_nextIndex++;
	r.tmpl = &tmpl;
	r.drawTmpl = &tmpl; // the tree buffer's type of the template (RW 0x4EC93A)
	r.next = m_heads[(size_t)bucket]; // RW 0x683E86 .. 0x683E9B: the new record is the bucket's head
	m_heads[(size_t)bucket] = (std::int16_t)m_records.size();
	const long long overrideValue = Economy::templateInt(tmpl, "SupplyOverride"); // ThingTemplate + 0x5C8
	r.value = overrideValue > 0 ? (int)overrideValue : m_logic.economy().settings().supplyBoxesPerTree; // RW 0x683E9E .. 0x683EB6
	r.grabbable = boolField(tmpl, "IsGrabbable");     // + 0x5FB
	r.harvestable = boolField(tmpl, "IsHarvestable"); // + 0x5FC
	m_records.push_back(r);
	return true;
}

void TerrainTrees::bucketRange(const Coord3D &where, float r, int &x0, int &y0, int &x1, int &y1) const
{
	float x = SimMath::subf32(where.x, r), y = SimMath::subf32(where.y, r);
	x = m_lo[0] > x ? m_lo[0] : x;
	y = m_lo[1] > y ? m_lo[1] : y;
	x = x > m_hi[0] ? m_hi[0] : x;
	y = y > m_hi[1] ? m_hi[1] : y;
	x0 = gridIndex(x, m_lo[0], m_hi[0], false);
	y0 = gridIndex(y, m_lo[1], m_hi[1], false);
	x = SimMath::addf32(where.x, r);
	y = SimMath::addf32(where.y, r);
	x = m_lo[0] > x ? m_lo[0] : x;
	y = m_lo[1] > y ? m_lo[1] : y;
	x = x > m_hi[0] ? m_hi[0] : x;
	y = y > m_hi[1] ? m_hi[1] : y;
	x1 = gridIndex(x, m_lo[0], m_hi[0], true);
	y1 = gridIndex(y, m_lo[1], m_hi[1], true);
}

void TerrainTrees::markChanged(Record &tree)
{
	if (!tree.changed)
	{
		tree.changed = true;
		++m_changed;
	}
}

// RW 0x67D3C2: the record is cleared (the bucket link stays)
void TerrainTrees::clearRecord(Record &tree)
{
	if (tree.clientId != 0)
	{
		m_logic.fireLogic().unregisterTree(tree.clientId, tree.position); // RW 0x684C54 / 0x686B2E
	}
	if (tree.morph != 0)
	{
		--m_morphing; // the client's tree is gone with it (slot 0x58 / 0x7C): it never updates again
	}
	tree.drawTmpl = nullptr;
	tree.morph = 0;
	tree.morphLeft = 0;
	tree.morphTarget = false;
	tree.position = Coord3D{};
	tree.clientId = 0;
	tree.index = 0;
	tree.tmpl = nullptr;
	tree.toppled = false;
	tree.toppleDirection = Coord3D{};
	tree.value = 1;
	tree.grabbable = true;
	tree.harvestable = true;
}

// RW 0x67F52B -> 0x67E078 with the collector RW 0x67D519
TerrainTrees::Record *TerrainTrees::findClosestTree(const Coord3D &where, float radius, bool ignoreToppled, int mode, const std::function<bool(const Record &)> &accept)
{
	if (m_records.empty() || !m_hasExtent)
	{
		return nullptr;
	}
	const float r = SimMath::addf32(radius, kQueryMargin);
	int x0, y0, x1, y1;
	bucketRange(where, r, x0, y0, x1, y1);
	const float r2 = SimMath::mulf32(r, r);
	Record *best = nullptr;
	float bestD = 0.0f;
	const int n = (int)m_records.size();
	for (int bx = x0; bx < x1; ++bx)
	{
		for (int by = y0; by < y1; ++by)
		{
			if (bx < 0 || by < 0 || bx >= BUCKETS || by >= BUCKETS)
			{
				continue; // the clamps keep the corners in [0, 50]: row 50 / column 50 would read the next row in retail; an index past the grid ends nothing here
			}
			for (int i = m_heads[(size_t)(by * BUCKETS + bx)]; i != -1;)
			{
				if (i < 0 || i >= n)
				{
					break; // RW 0x67E259 .. 0x67E263
				}
				Record &t = m_records[(size_t)i];
				const bool pass = t.clientId != 0 && !(ignoreToppled && t.toppled) && (mode != 1 || t.grabbable) && (mode != 2 || t.harvestable);
				if (pass)
				{
					const float dx = SimMath::subf32(t.position.x, where.x);
					const float dy = SimMath::subf32(t.position.y, where.y);
					const float dz = SimMath::subf32(t.position.z, where.z);
					const float d3 = SimMath::addf32(SimMath::addf32(SimMath::mulf32(dz, dz), SimMath::mulf32(dy, dy)), SimMath::mulf32(dx, dx));
					if (r2 > d3)
					{
						// RW 0x67D519: the 2D squared distance from the query point, dy^2 + dx^2
						const float cx = SimMath::subf32(where.x, t.position.x);
						const float cy = SimMath::subf32(where.y, t.position.y);
						const float d2 = SimMath::addf32(SimMath::mulf32(cy, cy), SimMath::mulf32(cx, cx));
						if ((!best || bestD > d2) && (!accept || accept(t)))
						{
							best = &t;
							bestD = d2;
						}
					}
				}
				i = t.next;
			}
		}
	}
	return best;
}

const TerrainTrees::Record *TerrainTrees::findClosestTree(const Coord3D &where, float radius, bool ignoreToppled, int mode) const
{
	return const_cast<TerrainTrees *>(this)->findClosestTree(where, radius, ignoreToppled, mode); // the query changes nothing
}

// RW 0x6849F8
void TerrainTrees::toppleTree(Record &tree, const Coord3D &from, const Object *source)
{
	if (tree.toppled)
	{
		return;
	}
	const Coord3D &o = source ? *source->getPosition() : from;
	Coord3D dir{ SimMath::subf32(tree.position.x, o.x), SimMath::subf32(tree.position.y, o.y), 0.0f };
	// TheTerrainVisual slot 0x70 (the tree buffer's topple, RW 0x4E9E4E): INFERENCE (S-930) true for a record with an id; a morphing tree refuses it
	// (RW 0x4E9EAD: + 0x688, S-921)
	if (tree.clientId == 0 || tree.morph != 0)
	{
		return;
	}
	Event e;
	e.kind = Event::TOPPLE;
	e.clientId = tree.clientId;
	e.direction = dir;
	e.speed = m_toppleSpeed;
	e.frame = m_logic.getFrame();
	m_events.push_back(e);
	keepStart(); // lane SAVE-4e review r2: the start is kept before the first change touches a record
	tree.toppled = true;
	tree.toppleDirection = dir;
	markChanged(tree);
	// RW 0x684AB6 .. 0x684B14: the CRITTER_EMITTER objects within 5.0 are destroyed (the birds of the tree)
	static const int critter = ObjectTemplateInfoBuilder::kindOfIndex("CRITTER_EMITTER");
	if (critter >= 0)
	{
		PartitionFilterFn kind([](Object &obj) { return obj.isKindOf((unsigned)critter); });
		const PartitionHits hits = m_logic.partition().iterateObjectsInRange(tree.position, kCritterRadius, FROM_CENTER_3D, { &kind }, ITER_FASTEST);
		for (const PartitionHit &h : hits)
		{
			m_logic.destroyObject(h.object);
		}
	}
	// RW 0x684B14 .. 0x684B3F (lane TREES1): slot 0x4C, the props a cylinder of 25.0 (RW 0xC69574: height and both radii) at the tree, 5.0 (RW 0xBDAE58) higher,
	// overlaps (the prop buffer's RW 0x4EEB15); a client event
	m_events.back().position = Coord3D{ tree.position.x, tree.position.y, SimMath::addf32(tree.position.z, kCritterRadius) };
	ObjectGeometry::Shape c;
	c.type = ObjectGeometry::SHAPE_CYLINDER;
	c.height = kPropClearRadius;
	c.majorRadius = kPropClearRadius;
	c.minorRadius = kPropClearRadius;
	m_events.back().shapes.push_back(c);
}

// RW 0x67D6E1
void TerrainTrees::burnTreeLumber(std::uint32_t treeId, int amount)
{
	// RW 0x67F4C2 -> 0x67D6E1
	for (Record &tree : m_records)
	{
		if (tree.clientId == treeId && tree.clientId != 0)
		{
			harvestTree(tree, amount);
			break;
		}
	}
}

int TerrainTrees::harvestTree(Record &tree, int amount)
{
	const int taken = amount < tree.value ? amount : tree.value; // RW 0x67D6EC .. 0x67D6FD: `cmp amount, value; jl`
	keepStart(); // lane SAVE-4e review r2: the start is kept before the first change touches a record
	tree.value -= taken;
	markChanged(tree);
	if (tree.value <= 0)
	{
		Event e;
		e.kind = Event::REMOVE; // TheTerrainVisual slot 0x7C (RW 0x67D713 -> RW 0x4EA068: the tree sinks)
		e.clientId = tree.clientId;
		e.frame = m_logic.getFrame();
		m_events.push_back(e);
		clearRecord(tree);
		m_changedFrame = m_logic.getFrame(); // RW 0x67D71D
	}
	return taken;
}

// RW 0x6858BE (lane TREES1)
int TerrainTrees::unitMoved(const Object &obj)
{
	// TheTerrainVisual slot 0x6C (RW 0x4E70F5: the shrubs' push aside) is the client's and not sent (S-5260)
	static const int immobile = ObjectTemplateInfoBuilder::kindOfIndex("IMMOBILE");      // template + 0x108 bit 2
	static const int machine = ObjectTemplateInfoBuilder::kindOfIndex("MACHINE");        // + 0x109 bit 3
	static const int armyOfDead = ObjectTemplateInfoBuilder::kindOfIndex("ARMY_OF_DEAD"); // + 0x11A bit 7
	if (immobile < 0 || machine < 0 || armyOfDead < 0)
	{
		throw std::logic_error("terrain trees: the KindOf names IMMOBILE / MACHINE / ARMY_OF_DEAD are not known");
	}
	if (obj.isKindOf((unsigned)immobile))
	{
		return 0;
	}
	// RW 0x6858E9 .. 0x685917: crusher level (RW 0x695070, a signed byte) above 1, or MACHINE; and not ARMY_OF_DEAD
	if (!(ObjectCrush::crusherLevel(obj) > 1 || obj.isKindOf((unsigned)machine)) || obj.isKindOf((unsigned)armyOfDead))
	{
		return 0;
	}
	// (obj + 0x38, + 0x44, + 0xA8 with its bounding circle + 0xB8, 1, 0); the circle (RW 0xAD2860) from the object's own shapes (lane MODCOMPAT-MOD1), as the
	// pathfinder's geometry computes it (a game without a pathfinder has crushers too)
	const std::vector<ObjectGeometry::Shape> &shapes = obj.geometryShapes();
	PathfindGeometry pg;
	ObjectGeometry::fillPathfindGeometry(shapes, pg);
	return toppleTreesInGeometry(*obj.getPosition(), obj.getOrientation(), shapes, pg.boundingCircleRadius(), obj, true, 0);
}

// RW 0x6854FD (lane TREES1)
int TerrainTrees::toppleTreesInGeometry(const Coord3D &position, float angle, const std::vector<ObjectGeometry::Shape> &shapes, float circle, const Object &source,
                                        bool ignoreToppled, int mode)
{
	// the geometry's bounding circle (`circle`: GeometryInfo + 0x10, RW 0xAD2860) and its box centre (+ 0x18, RW 0xAD15D0) added to the position (SSE)
	const Coord3D offset = ObjectGeometry::boxCentre(shapes);
	const Coord3D centre{ SimMath::addf32(position.x, offset.x), SimMath::addf32(position.y, offset.y), SimMath::addf32(position.z, offset.z) };
	if (m_records.empty() || !m_hasExtent)
	{
		return 0; // RW 0x685568: no record, nothing (the extent is the map's, always set while records exist)
	}
	const float r = SimMath::addf32(circle, kQueryMargin);
	int x0, y0, x1, y1;
	bucketRange(centre, r, x0, y0, x1, y1);
	const float r2 = SimMath::mulf32(r, r);
	// RW 0x685783 .. 0x6857E3: the tree's GeometryInfo (RW 0x450429(CYLINDER, not small, 20.0, 7.0, 7.0))
	std::vector<ObjectGeometry::Shape> tree(1);
	tree[0].type = ObjectGeometry::SHAPE_CYLINDER;
	tree[0].height = kCrushTreeHeight;
	tree[0].majorRadius = kQueryMargin;
	tree[0].minorRadius = kQueryMargin;
	int toppled = 0;
	const int n = (int)m_records.size();
	for (int bx = x0; bx < x1; ++bx)
	{
		for (int by = y0; by < y1; ++by)
		{
			if (bx < 0 || by < 0 || bx >= BUCKETS || by >= BUCKETS)
			{
				continue; // as in findClosestTree
			}
			for (int i = m_heads[(size_t)(by * BUCKETS + bx)]; i != -1;)
			{
				if (i < 0 || i >= n)
				{
					break; // RW 0x685733 .. 0x68573D
				}
				Record &t = m_records[(size_t)i];
				const bool pass = t.clientId != 0 && !(ignoreToppled && t.toppled) && (mode != 1 || t.grabbable) && (mode != 2 || t.harvestable);
				if (pass)
				{
					const float dx = SimMath::subf32(t.position.x, centre.x);
					const float dy = SimMath::subf32(t.position.y, centre.y);
					const float dz = SimMath::subf32(t.position.z, centre.z);
					const float d3 = SimMath::addf32(SimMath::addf32(SimMath::mulf32(dz, dz), SimMath::mulf32(dy, dy)), SimMath::mulf32(dx, dx));
					// RW 0x685840: the geometry (ECX) at the object's position and angle against the tree's at its position, angle 0
					if (r2 > d3 && NotifyCrushModules::geometriesOverlap(shapes, position, angle, tree, t.position, 0.0f))
					{
						const bool was = t.toppled;
						toppleTree(t, centre, &source); // RW 0x684851 with the source object: the direction is from the object's position
						toppled += (!was && t.toppled) ? 1 : 0;
					}
				}
				i = t.next;
			}
		}
	}
	return toppled;
}

// RW 0x684BCA (lane TREES1)
int TerrainTrees::removeTreesForConstruction(const Coord3D &position, const std::vector<ObjectGeometry::Shape> &shapes, float angle)
{
	// RW 0x684BD7 .. 0x684C02: the tree's GeometryInfo (RW 0x450429(CYLINDER, not small, 35.0, 14.0, 14.0))
	std::vector<ObjectGeometry::Shape> tree(1);
	tree[0].type = ObjectGeometry::SHAPE_CYLINDER;
	tree[0].height = kConstructionTreeHeight;
	tree[0].majorRadius = kConstructionTreeRadius;
	tree[0].minorRadius = kConstructionTreeRadius;
	int removed = 0;
	for (Record &t : m_records) // RW 0x684C1A: the record list in order (no buckets)
	{
		if (t.clientId == 0 || !NotifyCrushModules::geometriesOverlap(shapes, position, angle, tree, t.position, 0.0f))
		{
			continue;
		}
		// RW 0x684C43 .. 0x684C54: the fire grid (TheFireLogicSystem) is not ported (S-5260)
		keepStart();
		Event e;
		e.kind = Event::REMOVE_NOW; // slot 0x58 (RW 0x684C69 -> RW 0x4E9C81)
		e.clientId = t.clientId;
		e.frame = m_logic.getFrame();
		m_events.push_back(e);
		clearRecord(t);
		markChanged(t);
		m_changedFrame = m_logic.getFrame(); // RW 0x684C74
		++removed;
	}
	// RW 0x684C86 .. 0x684C9B: slot 0x48, the shrubs and props the geometry overlaps (a client event)
	Event e;
	e.kind = Event::CLEAR_FOOTPRINT;
	e.frame = m_logic.getFrame();
	e.position = position;
	e.angle = angle;
	e.shapes = shapes;
	m_events.push_back(e);
	return removed;
}

// RW 0x67F63F
int TerrainTrees::removeTreesInRadius(const Coord3D &position, float radius)
{
	int removed = 0;
	for (Record &t : m_records) // RW 0x67F649 .. 0x67F6E3: the record list in order
	{
		if (t.clientId == 0) // RW 0x67F65E
		{
			continue;
		}
		// RW 0x67F664 .. 0x67F6A1: the difference in SSE, then RW 0x405482 (x87: y * y + x * x, the CRT root); radius < length skips the tree
		const float dx = SimMath::subf32(position.x, t.position.x), dy = SimMath::subf32(position.y, t.position.y);
		const double len = NumericState::sqrtPC24(NumericState::pc24AddW(NumericState::pc24MulW(dy, dy), NumericState::pc24MulW(dx, dx)));
		if ((double)radius < len)
		{
			continue;
		}
		keepStart();
		Event e;
		e.kind = Event::REMOVE_NOW; // RW 0x67F6C9: TheTerrainVisual slot 0x58
		e.clientId = t.clientId;
		e.frame = m_logic.getFrame();
		m_events.push_back(e);
		clearRecord(t); // RW 0x67F6B4 (the fire grid, RW 0x686B2E) and RW 0x67F6BB (RW 0x67D3C2)
		markChanged(t);
		m_changedFrame = m_logic.getFrame(); // RW 0x67F6D4
		++removed;
	}
	return removed;
}

// RW 0x683465 .. 0x683498: TheTerrainVisual slot 0x5C with a copy of the position
void TerrainTrees::hideScorches(const Coord3D &position, float radius)
{
	Event e;
	e.kind = Event::HIDE_SCORCHES;
	e.frame = m_logic.getFrame();
	e.position = position;
	e.radius = radius;
	m_events.push_back(e);
}

// S-921 ----------------------------------------------------------------------------------------------------------------------------------------------
const W3DTreeDrawModuleData *TerrainTrees::treeDrawOf(const ThingTemplate *tmpl)
{
	if (!tmpl)
	{
		return nullptr;
	}
	for (const ThingTemplate::Nugget &n : tmpl->drawModules().nuggets())
	{
		if (n.name == "W3DTreeDraw")
		{
			return dynamic_cast<const W3DTreeDrawModuleData *>(n.data.get());
		}
	}
	return nullptr;
}

// RW 0x4EC7B6: an empty MorphTree, an unknown template or one without a W3DTreeDraw is -2 (no target)
const ThingTemplate *TerrainTrees::morphTargetOf(const ThingTemplate *tmpl) const
{
	const W3DTreeDrawModuleData *d = treeDrawOf(tmpl);
	if (!d || d->m_morphTree.empty())
	{
		return nullptr;
	}
	const ThingTemplate *t = m_logic.things().findTemplate(d->m_morphTree); // RW 0x6D1305
	return treeDrawOf(t) ? t : nullptr;
}

// RW 0x67F6F0 -> 0x67E879 (the bucket walk) -> TheTerrainVisual slot 0x74 for every record found
int TerrainTrees::morphTreesInRadius(const Coord3D &position, float radius, int type)
{
	if (m_records.empty() || !m_hasExtent)
	{
		return 0; // RW 0x67E895: no records
	}
	const float r = SimMath::addf32(radius, kQueryMargin); // RW 0x67E89B
	int x0, y0, x1, y1;
	bucketRange(position, r, x0, y0, x1, y1);
	const float r2 = SimMath::mulf32(r, r);
	const int n = (int)m_records.size();
	int started = 0;
	for (int bx = x0; bx < x1; ++bx) // RW 0x67EB0E: x outer
	{
		for (int by = y0; by < y1; ++by)
		{
			if (bx < 0 || by < 0 || bx >= BUCKETS || by >= BUCKETS)
			{
				continue;
			}
			for (int i = m_heads[(size_t)(by * BUCKETS + bx)]; i != -1;)
			{
				if (i < 0 || i >= n)
				{
					break; // RW 0x67EA5A .. 0x67EA64
				}
				Record &t = m_records[(size_t)i];
				if (t.clientId != 0) // RW 0x67EA73 (ignoreToppled 0, mode 0)
				{
					const float dx = SimMath::subf32(t.position.x, position.x);
					const float dy = SimMath::subf32(t.position.y, position.y);
					const float dz = SimMath::subf32(t.position.z, position.z);
					const float d3 = SimMath::addf32(SimMath::addf32(SimMath::mulf32(dz, dz), SimMath::mulf32(dy, dy)), SimMath::mulf32(dx, dx));
					if (r2 > d3 && startMorph(t, type)) // RW 0x67D6A5 -> slot 0x74
					{
						++started;
					}
				}
				i = t.next; // RW 0x67EAF3: read after the call (a cleared record keeps its link)
			}
		}
	}
	return started;
}

// RW 0x4EC8EB / 0x4EC40E
bool TerrainTrees::startMorph(Record &tree, int type)
{
	if (tree.morphLeft != 0 || tree.morph != 0) // RW 0x4EC910 / 0x4EC915: the tree is still morphing
	{
		return false;
	}
	const W3DTreeDrawModuleData *d = treeDrawOf(tree.drawTmpl);
	if (!d)
	{
		return false; // the tree buffer's types are W3DTreeDraw's
	}
	if ((type == 1 && d->m_taintedTree) || (type == 2 && !d->m_taintedTree)) // RW 0x4EC442 .. 0x4EC472
	{
		return false;
	}
	if (tree.toppled) // RW 0x4EC486: + 0x640, TheTerrainLogic RW 0x67F738(id)
	{
		const std::uint32_t id = tree.clientId;
		for (Record &r : m_records)
		{
			if (r.clientId == id)
			{
				harvestTree(r, r.value); // RW 0x67F75F: the whole value
				break;
			}
		}
		return false;
	}
	keepStart();
	tree.morphLeft = (std::int32_t)d->m_morphTime; // RW 0x4EC4C7 .. 0x4EC4D2
	tree.morph = type;
	const ThingTemplate *target = morphTargetOf(tree.tmpl); // + 0x690: the add-time type's MorphTree
	tree.morphTarget = morphTargetOf(tree.drawTmpl) != nullptr && target != nullptr; // RW 0x4EC613: the type's MorphTree
	markChanged(tree);
	++m_morphing;
	Event e;
	e.kind = Event::MORPH;
	e.clientId = tree.clientId;
	e.frame = m_logic.getFrame();
	e.position = tree.position;
	e.morphType = type;
	e.from = tree.drawTmpl;
	e.to = tree.morphTarget ? target : nullptr;
	e.morphTime = tree.morphLeft;
	m_events.push_back(e);
	return true;
}

// RW 0x67D4CE -> TheTerrainVisual slot 0x40
void TerrainTrees::morphArea(const Coord3D &position, float radius, int type)
{
	Event e;
	e.kind = Event::MORPH_AREA;
	e.frame = m_logic.getFrame();
	e.position = position;
	e.radius = SimMath::addf32(radius, kMorphAreaMargin); // RW 0x8C7DF8 / 0x8C8EA5 (addss)
	e.morphType = type;
	m_events.push_back(e);
}

// RW 0x67D08D -> TheTerrainVisual slot 0x44 -> RW 0x4EAD92
void TerrainTrees::update()
{
	if (m_morphing == 0)
	{
		return;
	}
	for (Record &t : m_records) // the tree buffer's order (its trees in add order, INFERENCE S-921)
	{
		if (t.morph == 0 || t.clientId == 0)
		{
			continue;
		}
		if (t.toppled) // RW 0x4EACED: + 0x640 ends it at once
		{
			endMorph(t);
			continue;
		}
		--t.morphLeft; // RW 0x4EACFA
		if (t.morphLeft == 0) // RW 0x4EAD76
		{
			endMorph(t);
		}
	}
}

// RW 0x4EAAA7
void TerrainTrees::endMorph(Record &tree)
{
	const bool upright = !tree.toppled; // RW 0x4EAAD3 / 0x4EAADF
	const int type = tree.morph;
	const bool hadTarget = tree.morphTarget;
	keepStart();
	if (upright)
	{
		if (type == 1)
		{
			if (hadTarget)
			{
				tree.drawTmpl = morphTargetOf(tree.tmpl); // RW 0x4EAAFC: + 0x690
			}
		}
		else
		{
			tree.drawTmpl = tree.tmpl; // RW 0x4EAB04: + 0x68C
		}
	}
	tree.morph = 0; // RW 0x4EABCB (+ 0x694 keeps what it had)
	tree.morphTarget = false;
	--m_morphing;
	markChanged(tree);
	Event e;
	e.kind = Event::MORPH_END;
	e.clientId = tree.clientId;
	e.frame = m_logic.getFrame();
	e.position = tree.position;
	e.to = tree.drawTmpl;
	m_events.push_back(e);
	if (upright && !hadTarget)
	{
		removeTreeNow(tree.clientId); // RW 0x4EAB74
	}
}

// RW 0x684412
void TerrainTrees::removeTreeNow(std::uint32_t treeId)
{
	m_changedFrame = m_logic.getFrame(); // RW 0x684429
	for (Record &r : m_records)
	{
		if (r.clientId == treeId)
		{
			keepStart();
			clearRecord(r); // RW 0x684452 (the fire grid) and RW 0x684459
			markChanged(r);
			break;
		}
	}
	Event e;
	e.kind = Event::REMOVE_NOW; // RW 0x684467: slot 0x58
	e.clientId = treeId;
	e.frame = m_logic.getFrame();
	m_events.push_back(e);
}

void TerrainTrees::crc(StateHasher &h) const
{
	h.addU32((std::uint32_t)m_changed);
	for (size_t i = 0; i < m_records.size(); ++i)
	{
		const Record &r = m_records[i];
		if (!r.changed)
		{
			continue;
		}
		h.addU32((std::uint32_t)i);
		h.addU32(r.clientId);
		h.addBool(r.toppled);
		h.addFloat(r.toppleDirection.x);
		h.addFloat(r.toppleDirection.y);
		h.addI32(r.value);
		if (r.morph != 0 || r.morphLeft != 0 || r.drawTmpl != r.tmpl) // S-921: only a tree a morph touched (the hash of other games stays)
		{
			h.addI32(r.morph);
			h.addI32(r.morphLeft);
			h.addBool(r.morphTarget);
			h.addString(r.drawTmpl ? r.drawTmpl->getName() : std::string());
		}
	}
}

// lane SAVE-4d. Retail saves the tree records inside TerrainLogic::xfer (CHUNK_TerrainLogic, RW 0x683F8C, window (1, 2), tier A: the record count, every
// record, the 2,500 bucket heads, + 0x1910, + 0x1914 and, from version 2, + 0x48). The port's trees are a part of TerrainLogic that the map builds at the
// START, so the block is OpenBFME's own (S-4252): the scalars, then only the records that changed since the map start (the ones the state hash covers),
// each by its list index and in retail's record layout (RW 0x680753, tier A: the decomp's TerrainLogicXfer.cpp Rva0027C230::rva0028002F; window
// (1, 2)): the position, the client id (XferDrawableID, 4 bytes), the template's name, toppled, the topple direction, the value, the bucket link (a short),
// IsGrabbable, IsHarvestable and, from version 2, the running index. The buckets never change after the START (a cleared record keeps its link). The client
// events (topples and removals the tree buffer shows) are not saved: a loaded game's client is not told which trees lie (S-4252).
// The load (SAVE-4a review r1 rules, SAVES.md section 8) reads every record into a temporary and checks it (an index of the map's list given once, a
// template name the data has, the bucket link the START gave that record) before the game's records change. A version 1 record has no running index
// (it keeps the map's).
void TerrainTrees::xfer(Xfer &x)
{
	const bool loading = x.isLoading();
	XferVersion v{ 1, 1 };
	x.xferVersion(v);
	std::uint32_t records = loading ? 0 : (std::uint32_t)m_records.size();
	x.xferUnsignedInt(&records);
	std::int32_t nextIndex = loading ? 0 : m_nextIndex;
	x.xferInt(&nextIndex);
	if (loading && (records != m_records.size() || nextIndex != m_nextIndex))
	{
		throw XferException("terrain trees: the snapshot's map has " + std::to_string(records) + " trees (next index " + std::to_string(nextIndex) +
			"), this game's " + std::to_string(m_records.size()) + " (next index " + std::to_string(m_nextIndex) + ")");
	}
	std::uint32_t changedFrame = loading ? 0 : m_changedFrame;
	float toppleSpeed = loading ? 0.0f : m_toppleSpeed;
	x.xferUnsignedInt(&changedFrame);
	x.xferReal(&toppleSpeed);
	std::uint32_t changed = loading ? 0 : (std::uint32_t)m_changed;
	// one changed record at its shortest (version 1, which has no running index): its index (4), version (1), position (12), client id (4), an empty
	// name (1), toppled (1), direction (12), value (4), link (2), two flags (2)
	x.xferCount(&changed, 43, sizeof(std::uint32_t) + sizeof(Record));
	std::vector<std::pair<std::uint32_t, Record>> loaded;
	if (loading)
	{
		loaded.reserve(changed);
	}
	size_t next = 0;
	for (std::uint32_t n = 0; n < changed; ++n)
	{
		std::uint32_t index = 0;
		Record r;
		if (!loading)
		{
			while (!m_records[next].changed)
			{
				++next;
			}
			index = (std::uint32_t)next++;
			r = m_records[index];
		}
		x.xferUnsignedInt(&index);
		XferVersion rv{ 1, 3 };
		x.xferVersion(rv);
		x.xferReal(&r.position.x);
		x.xferReal(&r.position.y);
		x.xferReal(&r.position.z);
		x.xferUnsignedInt(&r.clientId);
		std::string name = r.tmpl ? r.tmpl->getName() : std::string();
		x.xferAsciiString(&name);
		x.xferBool(&r.toppled);
		x.xferReal(&r.toppleDirection.x);
		x.xferReal(&r.toppleDirection.y);
		x.xferReal(&r.toppleDirection.z);
		x.xferInt(&r.value);
		x.xferShort(&r.next);
		x.xferBool(&r.grabbable);
		x.xferBool(&r.harvestable);
		if (rv.current >= 2)
		{
			x.xferInt(&r.index);
		}
		// version 3 (S-921): the tree buffer's morph mirrored in the record: the type now (a template name), + 0x688, + 0x694, model B made
		std::string drawName = r.drawTmpl ? r.drawTmpl->getName() : std::string();
		if (rv.current >= 3)
		{
			x.xferAsciiString(&drawName);
			x.xferInt(&r.morph);
			x.xferInt(&r.morphLeft);
			x.xferBool(&r.morphTarget);
		}
		if (!loading)
		{
			continue;
		}
		if (rv.current < 2 && index < m_records.size())
		{
			r.index = (m_start.empty() ? m_records[index] : m_start[index]).index; // RW 0x680753: version 1 leaves + 0x10 as the record was made
		}
		const std::string tree = "terrain trees: tree " + std::to_string(index);
		if (index >= m_records.size())
		{
			throw XferException(tree + " of " + std::to_string(m_records.size()));
		}
		for (const auto &e : loaded)
		{
			if (e.first == index)
			{
				throw XferException(tree + " twice");
			}
		}
		r.tmpl = name.empty() ? nullptr : m_logic.things().findTemplate(name);
		if (!name.empty() && !r.tmpl)
		{
			throw XferException(tree + " names the template '" + name + "', which this game lacks");
		}
		if (rv.current < 3)
		{
			drawName = name; // the add-time type, no morph
			r.morph = 0;
			r.morphLeft = 0;
			r.morphTarget = false;
		}
		r.drawTmpl = drawName.empty() ? nullptr : m_logic.things().findTemplate(drawName);
		if (!drawName.empty() && !r.drawTmpl)
		{
			throw XferException(tree + " is drawn as the template '" + drawName + "', which this game lacks");
		}
		if (r.morph < 0 || r.morph > 2)
		{
			throw XferException(tree + ": the morph type " + std::to_string(r.morph));
		}
		const Record &startRecord = m_start.empty() ? m_records[index] : m_start[index];
		if (r.next != startRecord.next)
		{
			// the buckets are the START's and never change (a cleared record keeps its link, RW 0x67D3C2): any other link could loop a query
			throw XferException(tree + ": a bucket link to " + std::to_string(r.next) + " (the map's is " + std::to_string(startRecord.next) + ")");
		}
		r.changed = true;
		loaded.emplace_back(index, r);
	}
	if (!loading)
	{
		return;
	}
	// lane SAVE-4e: the game's records go back to the map's (kept before the first change), then take the snapshot's changes: a game whose trees changed
	// since its START loads too (a refused load is put back by loading the game's own snapshot, LiveGame::loadSnapshot)
	keepStart();
	m_records = m_start;
	for (const auto &e : loaded)
	{
		m_records[e.first] = e.second;
	}
	m_changed = (int)changed;
	m_morphing = 0;
	for (const Record &r : m_records)
	{
		m_morphing += (r.morph != 0 && r.clientId != 0) ? 1 : 0;
	}
	m_changedFrame = changedFrame;
	m_toppleSpeed = toppleSpeed;
}

std::string TerrainTrees::stopLine()
{
	return "[S-930] terrain trees (lane SPELL-3): the logic's tree records (RW 0x683D89), their 50 x 50 buckets (RW 0x67D400), the closest-tree query (RW 0x67E078 / "
		   "0x67D519), topple (RW 0x6849F8, with its CRITTER_EMITTER destruction) and harvest (RW 0x67D6E1) run; INFERENCE: the client's topple answer "
		   "(TheTerrainVisual slot 0x70) is taken as true, the client id is the map object index + 1, the props' removal at the tree (slot 0x4C, a 25.0 "
		   "cylinder) and the sink (slot 0x7C) are client events, the CRITTER_EMITTER query uses the partition's 3D centre distance; the stealth forest "
		   "check reads these records";
}

std::string TerrainTrees::trees1StopLine()
{
	return "[S-5260] terrain trees (lane TREES1): units crush trees (RW 0x6858BE / 0x6854FD: crusher level 2 or more, or MACHINE) and a dozer's structure "
		   "removes the trees, shrubs and props under it (RW 0x684BCA). Not ported: "
		   "the shrubs' push aside (slot 0x6C), the fire grid (TheFireLogicSystem: burning cells take a tree's lumber, RW 0x6886E0), a tree made an object "
		   "(RW 0x68449B: attacked, grabbed, set alight by FireSpreadUpdate), the grab powers, "
		   "ClearRemovables (RW 0x5F0EE6) and trees an ObjectCreationList adds (the radius clear RW 0x683393 runs: TerrainTaintClear.h); INFERENCE: TheTerrainVisual's "
		   "harvest check (slot 0x80, RW 0x4EA005: the tree toppled, not sinking, not morphing) is answered by the logic. Taint / ElvenWood's tree morph "
		   "runs (S-921: RW 0x67F6F0 -> 0x4EC40E, the frame RW 0x4EACB0 from TerrainLogic's update, kept in the records)";
}
