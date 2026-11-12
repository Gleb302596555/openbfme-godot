// OpenBFME. GPL-3.0.
//
// TerrainTrees (lane SPELL-3): the logic's list of the map's optimized trees, RotWK's TheTerrainLogic + 0x578 (the records), + 0x588 (50 x 50 bucket heads)
// and the calls on them that the spell book's Devastation makes. The trees themselves are client-only drawables (MAPOBJ_CLIENT_TREE: the tree buffer draws
// them); the logic keeps where they stand, whether they toppled and how much lumber they still hold.
//
// TARGET FACTS (RotWK game.dat, caveat S-001), read from the disassembly:
//   * the record (constructor RW 0x67D359, 0x30 bytes): + 0 the position, + 0xC the client tree id (0: removed), + 0x10 the running index (TerrainLogic + 0x48),
//     + 0x14 the template, + 0x18 toppled, + 0x1C the topple direction, + 0x28 the value (supply boxes), + 0x2C IsGrabbable (ThingTemplate + 0x5FB), + 0x2D
//     IsHarvestable (+ 0x5FC), + 0x2E the next record of the bucket (a short, -1 ends);
//   * add (RW 0x683D89; BFME2 decomp TerrainLogicXfer.cpp TerrainLogic::rva00283642, tier B same-shape): TerrainLogic + 0x1910 = the frame; a template without
//     a W3DTreeDraw module adds nothing; the bucket (RW 0x67D400) of the position becomes the list head: the new record's next is the old head; the value is
//     the template's SupplyOverride (+ 0x5C8) when above 0, else GameData SupplyBoxesPerTree (GlobalData + 0xA60); the record is appended;
//   * the bucket (RW 0x67D400): the extent is TerrainLogic vslot 0x28 (RW 0x4622AC: the map including its border, lo = -border * 10, hi = size * 10 -
//     border * 10); x and y are clamped into it, then floor((p - lo) / (hi - lo) * 49.9) on the x87 (MSVCR71 floor, fistp), bucket = y * 50 + x;
//   * the query (RW 0x67E078, through RW 0x67F52B with the closest collector RW 0x67D4F0 / 0x67D519): radius += 7.0 (RW 0xBE4AD4, SSE); the bucket rectangle
//     from floor((clamp(p - r) - lo) / (hi - lo) * 49.9) to ceil((clamp(p + r) - lo) / (hi - lo) * 49.9), exclusive; buckets walked x outer, y inner, each
//     list from its head; a record counts when its id is not 0, (ignoreToppled: not toppled), (mode 1: IsGrabbable, mode 2: IsHarvestable), and its 3D
//     squared distance (SSE (dz^2 + dy^2) + dx^2) is below r^2; the collector keeps the first record of the smallest 2D squared distance (dy^2 + dx^2,
//     strictly smaller replaces);
//   * topple (RW 0x684BAA -> 0x6849F8): a toppled record does nothing; the direction is the record's position minus the source object's position (or the
//     given location without one), z = 0; the client's topple call (TheGameClient vslot 0x70 with the client id and the direction) must answer true, then
//     toppled = 1 and the direction is kept; ThePartitionManager's objects within 5.0 (RW 0xBDAE58) of KindOf CRITTER_EMITTER (bit 0x91) are destroyed
//     (RW 0x62BBAB) in the iterator's order; a client call with a cylinder GeometryInfo of 25.0 (RW 0xC69574) at the position 5.0 higher follows (vslot 0x4C);
//   * harvest (RW 0x67D6E1): taken = min(asked, value); value -= taken; at or below 0 the client is told (vslot 0x7C with the id), the record is cleared
//     (RW 0x67D3C2: the id 0, so no query finds it again) and TerrainLogic + 0x1910 = the frame.
//   * lane TREES1: the client calls go to TheTerrainVisual (RW 0xDE4AC8, W3DTerrainVisual, vtable RW 0xBDDFC8; GameClient::init RW 0x646771 makes it), not
//     TheGameClient: its slots forward through BaseHeightMap to the tree buffer (0x54 add, 0x58 remove at once, 0x70 topple, 0x7C sink, 0x80 the harvest
//     check), the shrub buffer (0x48, 0x6C) and the prop buffer (0x48, 0x4C);
//   * units crush trees (RW 0x6858BE, from Object's cell change RW 0x69264D for an object that is not IMMOBILE and is INFANTRY, CAVALRY, MONSTER or
//     MACHINE): TheTerrainVisual slot 0x6C (the shrubs' push aside), then an object that is not IMMOBILE, not ARMY_OF_DEAD and has a crusher level (RW
//     0x695070) of 2 or more, or is a MACHINE, topples the trees its geometry touches: RW 0x6854FD(position, angle, geometry, the object, ignoreToppled 1,
//     mode 0) walks the buckets of the geometry's box centre (GeometryInfo + 0x18, RW 0xAD15D0, added to the position, SSE) with the radius GeometryInfo +
//     0x10 (the bounding circle) + 7.0 as the query does; a record passes the query's filters and its 3D squared distance from the centre ((dz^2 + dy^2) +
//     dx^2, SSE) must be below r^2; then a cylinder of height 20.0 (RW 0xBDBC6C) and radius 7.0 (RW 0x450429(1, 0, 20, 7, 7)) at the tree with angle 0 must
//     overlap the object's geometry at its position and angle (RW 0xAD2CE0); the tree is toppled from the object (RW 0x6849F8 with the source);
//   * construction (RW 0x684BCA(position, geometry, angle), removeTreesForConstruction; callers: the dozer's placement RW 0x88D44F for a structure that is
//     neither NO_COLLIDE nor DO_NOT_CLASSIFY, and the ObjectCreationList's ClearRemovables RW 0x5F0EE6): every record of the list in order with an id whose
//     cylinder (height 35.0, RW 0xBE4CB8; radius 14.0, RW 0xBDD368) at angle 0 overlaps the geometry (RW 0xAD2CE0) leaves the fire grid (RW 0x686B2E, when
//     TheFireLogicSystem exists), is cleared (RW 0x67D3C2), goes from the client at once (slot 0x58) and stamps TerrainLogic + 0x1910; then slot 0x48 takes
//     the shrubs (cylinder 35 / 14) and props (cylinder of height 5 * scale, radius 2 * scale, RW 0x4EEB15) the geometry overlaps.
// INFERENCE (stop S-930, reported): the client's topple answer is taken as true for every record whose id is set (the tree buffer's applyTopplingForce,
// RW 0x4E9E4E, refuses a tree that is not upright, which the toppled flag already filters, or one that morphs, which the port's trees never do); the client
// id is the map object's index + 1 (the classified drawable both sides share); the slot 0x4C / 0x48 / 0x58 / 0x7C calls are client events only
// (TerrainTrees::events), not simulation; the CRITTER_EMITTER objects use the partition's centre distance in 2D (FROM_CENTER_2D). Lane TREES1 (stop S-5260):
// slot 0x6C's shrub push aside is sent as PUSH_ASIDE (PORT-73: RW 0x6858D0, the client's TreeTopple bends the shrubs; the client's own two GlobalData
// gates are the stop there), and the fire grid (TheFireLogicSystem: burning cells take a tree's lumber, RW 0x6886E0 -> 0x67F4C2) is not ported.
//
// The Taint / Elven Wood tree morph (stop S-921), TARGET FACTS (RotWK game.dat, caveat S-001):
//   * the spells (RW 0x8C8E67 Taint, type 1, TaintRadius + 0x80; RW 0x8C7DBA Elven Wood, type 2, ElvenWoodRadius + 0x90), before their FX and OCL: TheTerrainLogic
//     RW 0x67F6F0(location, radius, type), then RW 0x67D4CE(location, radius + 50.0 (RW 0xBD88C4, SSE), type);
//   * RW 0x67F6F0 -> the bucket walk RW 0x67E879 (the query's: radius + 7.0, the bucket rectangle, x outer, y inner, the list from its head; ignoreToppled 0,
//     mode 0): a record with an id whose 3D squared distance ((dz^2 + dy^2) + dx^2) is below r^2 goes to TheTerrainVisual slot 0x74 (RW 0x49192E -> the
//     tree buffer's RW 0x4EC8EB(id, type)): the tree of that id that is not morphing (+ 0x694 == 0 and + 0x688 == 0) morphs (RW 0x4EC40E):
//       - its type's W3DTreeDraw (+ 0x600, the type list + 0x44578): type 1 skips a TaintedTree (+ 0x54), type 2 skips one that is not; a sinking tree
//         (+ 0x684) is skipped;
//       - a toppled tree (+ 0x640) gives TheTerrainLogic RW 0x67F738(id): the first record of that id is harvested of its whole value (RW 0x67D6E1);
//       - else MorphTime (+ 0x4C) -> + 0x694, the type -> + 0x688; the drawn model A (+ 0x698) is the type's, at alpha 1; when the type's MorphTree (+ 0x445B0,
//         from RW 0x4EC7B6: the MorphTree template's W3DTreeDraw, -2 without one) exists, model B (+ 0x69C) is that of the tree's target (+ 0x690, the
//         add-time type's MorphTree), at alpha 0; MorphFX (+ 0x50) plays at the tree (RW 0x494615);
//   * TheTerrainLogic's update (RW 0x67D08D, every logic frame, GameLogic phase 1) calls TheTerrainVisual slot 0x44 (RW 0x491804 -> RW 0x46786B: the
//     tree buffer's RW 0x4EAD92, then the shrub buffer's RW 0x4E74C5): every tree with a type and + 0x688 set (RW 0x4EACB0): a toppled or sinking one ends
//     at once; else + 0x694 is decremented, A's alpha is + 0x694 / MorphTime and B's 1 - that, and at 0 the morph ends (RW 0x4EAAA7): an upright tree
//     takes the target (+ 0x690) for type 1 with B, its add-time type (+ 0x68C) for type 2; both models go; without B, TheTerrainLogic RW 0x684412(id):
//     the frame stamped (+ 0x1910), the first record of the id leaves the fire grid and is cleared, slot 0x58 (the tree goes at once); + 0x688 = 0;
//   * a morphing tree refuses the topple (RW 0x4E9E4E: + 0x688) and the harvest check (slot 0x80, RW 0x4EA005);
//   * RW 0x67D4CE -> TheTerrainVisual slot 0x40 (RW 0x4917E5 -> 0x46782B): the shrub buffer (RW 0x4E8AEB: every shrub with a type whose 3D squared distance
//     is below the radius squared morphs the same way, RW 0x4E8A9C -> 0x4E85EE) and the prop buffer (RW 0x4EDA0B: every prop within it goes).
// INFERENCE (S-921): the tree buffer's morph state is kept in the logic records (it calls back into the logic: the harvest and the removal), so it is hashed
// and saved; the shrubs and props are the client's (the event MORPH_AREA).

#pragma once

#include "Common/INIDataTypes.h"
#include "Common/StateHash.h"
#include "GameLogic/Object/ObjectGeometry.h"
#include "GameLogic/SkirmishAI/AIPositionSampler.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

class GameLogic;
class Object;
class ThingTemplate;

class TerrainTrees
{
public:
	struct Record
	{
		Coord3D position{};                ///< + 0
		std::uint32_t clientId = 0;        ///< + 0xC (0: removed)
		int index = 0;                     ///< + 0x10
		const ThingTemplate *tmpl = nullptr; ///< + 0x14
		bool toppled = false;              ///< + 0x18
		Coord3D toppleDirection{};         ///< + 0x1C
		int value = 0;                     ///< + 0x28 (supply boxes)
		bool grabbable = false;            ///< + 0x2C
		bool harvestable = false;          ///< + 0x2D
		std::int16_t next = -1;            ///< + 0x2E
		bool changed = false;              ///< toppled or harvested since the map start (the hash covers only these)
		// the tree buffer's morph (S-921, see above), mirrored here
		const ThingTemplate *drawTmpl = nullptr; ///< the tree's type now (+ 0x600; the add-time template, + 0x68C, until a morph ends)
		int morph = 0;                     ///< + 0x688: 0, 1 Taint, 2 Elven Wood
		std::int32_t morphLeft = 0;        ///< + 0x694
		bool morphTarget = false;          ///< model B (+ 0x69C) was made
	};
	// what the client shows: a tree starts to fall (TheGameClient vslot 0x70) or its lumber is gone (vslot 0x7C)
	struct Event
	{
		enum Kind
		{
			TOPPLE, ///< slot 0x70, then slot 0x4C: the props within a cylinder of 25.0 at the tree, 5.0 higher (`position`, `shapes`)
			REMOVE, ///< slot 0x7C: the lumber is gone, the tree sinks
			REMOVE_NOW, ///< lane TREES1: slot 0x58 (construction): the tree goes at once
			CLEAR_FOOTPRINT, ///< lane TREES1: slot 0x48 (construction): the shrubs and props the geometry (`position`, `angle`, `shapes`) overlaps go
			HIDE_SCORCHES, ///< slot 0x5C (the radius clear RW 0x683393 -> RW 0x46947C): the scorch marks within `radius` of `position` are hidden
			MORPH,     ///< slot 0x74 (S-921): the tree starts to morph (`morphType`, `from` the model drawn now, `to` the target's or null, `morphTime`)
			MORPH_END, ///< the morph ended (TheTerrainVisual slot 0x44): the tree is drawn as `to` (a REMOVE_NOW follows when it had no target)
			MORPH_AREA, ///< slot 0x40: the shrubs within `radius` of `position` morph (`morphType`), the props within it go
			PUSH_ASIDE ///< PORT-73: slot 0x6C (RW 0x6858D0 -> RW 0x4E70F5): the unit `clientId` at `position` walks along `direction` with its major `radius`;
			            ///< the client bends the shrubs within radius + 7.0 (a draw only event, nothing of the simulation changes)
		};
		Kind kind = TOPPLE;
		std::uint32_t clientId = 0;
		Coord3D direction{};
		float speed = 0.0f; ///< TerrainLogic + 0x1918 when the tree was toppled (Devastation sets 0.1 for its loop, RW 0x8CC75C)
		std::uint32_t frame = 0;
		Coord3D position{};
		float angle = 0.0f;
		std::vector<ObjectGeometry::Shape> shapes;
		float radius = 0.0f; ///< HIDE_SCORCHES, MORPH_AREA
		int morphType = 0;   ///< MORPH, MORPH_AREA
		const ThingTemplate *from = nullptr, *to = nullptr; ///< MORPH, MORPH_END
		std::int32_t morphTime = 0; ///< MORPH: MorphTime (+ 0x4C) in logic frames
	};
	enum
	{
		BUCKETS = 50 ///< RW 0x67D4C4: `imul eax, eax, 0x32`
	};

	explicit TerrainTrees(GameLogic &logic);

	void reset();
	// the bucket grid's extent (RW 0x4622AC); MapObjectLoop sets it from the map before the trees are added
	void setExtent(float loX, float loY, float hiX, float hiY);
	bool hasExtent() const { return m_hasExtent; }
	// RW 0x683D89; false (nothing added) for a template without a W3DTreeDraw module or before the extent is set
	bool addTree(const ThingTemplate &tmpl, const Coord3D &position, std::uint32_t clientId);
	// RW 0x67F52B: the closest record within `radius` (see the header), null when none. Lane LUMBER1: `accept` (the harvest finder's collector RW 0x67D5A0:
	// the same 2D distance and order, then the worker's quick path and ground line tests) is asked only for a record that would become the closest; a
	// refused one is passed over
	Record *findClosestTree(const Coord3D &where, float radius, bool ignoreToppled, int mode, const std::function<bool(const Record &)> &accept = nullptr);
	const Record *findClosestTree(const Coord3D &where, float radius, bool ignoreToppled, int mode) const;
	// RW 0x6849F8 with the source object (or none: `from` is the location)
	void toppleTree(Record &tree, const Coord3D &from, const Object *source);
	// RW 0x67D6E1: the supply boxes taken
	int harvestTree(Record &tree, int amount);
	void burnTreeLumber(std::uint32_t treeId, int amount); ///< RW 0x67F4C2 -> 0x67D6E1: fire consumes tree lumber
	// lane TREES1: RW 0x6858BE, an object entered a new cell (Object's RW 0x69264D gate): a crusher topples the trees under it; the number toppled
	int unitMoved(const Object &obj);
	// lane TREES1: RW 0x6854FD: the trees whose cylinder (20 high, radius 7) the geometry (its bounding circle `circle`) at `position` / `angle` touches are
	// toppled from `source`; the number toppled
	int toppleTreesInGeometry(const Coord3D &position, float angle, const std::vector<ObjectGeometry::Shape> &shapes, float circle, const Object &source,
	                          bool ignoreToppled, int mode);
	// lane TREES1: RW 0x684BCA, removeTreesForConstruction: the trees the geometry overlaps go (no topple, no lumber); the number removed
	int removeTreesForConstruction(const Coord3D &position, const std::vector<ObjectGeometry::Shape> &shapes, float angle);
	// RW 0x67F63F (from the radius clear RW 0x683393): every record of the list in order with an id whose 2D distance from `position` (RW 0x405482:
	// pos - tree in SSE, then the x87 root of y * y + x * x) is not above `radius` leaves the fire grid, is cleared, goes from the client at once (slot
	// 0x58) and stamps TerrainLogic + 0x1910; the number removed
	int removeTreesInRadius(const Coord3D &position, float radius);
	// RW 0x68449B (TerrainLogic, lane LOCAL-42): the map tree closest to `position` (RW 0x67E078: radius 10.0 at RW 0xBD83D8, every record, no mode) made an
	// object: with a template (+ 0x14) ThingFactory::newObject (RW 0x6D165E) for the neutral player's default team (ThePlayerList + 0x18 = player 0,
	// + 0x30C), no status, teleported onto the record (RW 0x696E63(record, 0)); the record cleared (RW 0x67D3C2) and removed (RW 0x684412 with its id).
	// A record without a template is only cleared and removed (null). No record: ThePartitionManager's closest object (RW 0xA39090, distance type 1, 3D
	// centres) within 10.0 of `position` whose KindOf has SUPPLY_SOURCE (RW 0x684560: RW 0x444D39(0, 0x56) / RW 0x445139) and that is alive (filter
	// RW 0xC10E20), or null
	Object *makeTreeObject(const Coord3D &position);
	// TheTerrainVisual slot 0x5C from the radius clear (RW 0x683498): a client event (HIDE_SCORCHES)
	void hideScorches(const Coord3D &position, float radius);
	// S-921: RW 0x67F6F0 (the trees within `radius` start to morph, `type` 1 Taint / 2 Elven Wood); the number that started
	int morphTreesInRadius(const Coord3D &position, float radius, int type);
	// S-921: RW 0x67D4CE (TheTerrainVisual slot 0x40 with radius + 50: the client's shrubs and props, MORPH_AREA)
	void morphArea(const Coord3D &position, float radius, int type);
	// S-921: TheTerrainLogic's update (RW 0x67D08D) -> TheTerrainVisual slot 0x44: the morphing trees advance a logic frame
	void update();
	// the tree buffer's W3DTreeDraw of a type (its template's; null without one) and the type of its MorphTree (RW 0x4EC7B6; null for -2)
	static const class W3DTreeDrawModuleData *treeDrawOf(const ThingTemplate *tmpl);
	const ThingTemplate *morphTargetOf(const ThingTemplate *tmpl) const;
	int morphing() const { return m_morphing; }

	// TerrainLogic + 0x1918, the topple speed the client is given (Devastation sets it around its loop)
	void setToppleSpeed(float s) { m_toppleSpeed = s; }

	const std::vector<Record> &records() const { return m_records; }

	// ---- lane RAI-POW-3: the AI's tree-cluster index (TerrainTrees + 0x584) ---------------------------------------------------------------
	// TheTerrainLogic + 0x584 holds one position sampler, the class of the AI record's (constructor RW 0x8E4042 at RW 0x684876 with padding 100.0, RW
	// 0xBD88D8): AIPositionSampler. Retail's tree index runs the record copies of its functions (RW 0x683C2D register + insert, RW 0x683807 /
	// 0x6831BE / 0x683055, the copies of RW 0x8E42C8 / 0x8E41EB / 0x8E4082; clear RW 0x682F3D); here the sampler's id is the record's index. Every
	// record joins as the map adds it (RW 0x683F17), a load rebuilds the index (RW 0x683D53 from TerrainLogic::xfer, RW 0x6842F1). The decision reads a
	// group's centre (RW 0x9E7071: group + 8), its member count (RW 0x97D700) and sets its "used" byte (RW 0x9E7095; getter RW 0x9E70A6). RW 0x67D3C2
	// clears a record but does not unlink it from its group (the index is built while the map's trees are added and kept for the game).
	using Cluster = AIPositionSampler::Group;
	static constexpr float kClusterPadding = 100.0f; ///< RW 0xBD88D8 (RW 0x8E4042's padding argument at RW 0x684876)
	const std::vector<Cluster> &clusters() const { return m_clusters.groups(); }
	std::vector<Cluster> &clusters() { return m_clusters.groups(); }
	bool anyClusterUsed() const;

	// the client events in order; the client reads from its last index (the list only grows during a game)
	const std::vector<Event> &events() const { return m_events; }
	bool anyChanged() const { return m_changed > 0; }
	// the changed records (toppled or harvested), in list order: a game where no tree changed keeps its hash
	void crc(StateHasher &h) const;
	// lane SAVE-4d: the block obfme:TerrainTrees (see TerrainTrees.cpp, S-4252): the changed records in retail's record layout; a load goes into a game of the
	// same START (lane SAVE-4e: its records are put back to the map's first, so the game's own changes since need not be undone), and throws an XferException
	// naming a mismatch
	void xfer(class Xfer &x);
	static std::string stopLine(); ///< [S-930]
	static std::string trees1StopLine(); ///< [S-5260] (lane TREES1)

private:
	int bucketOf(float x, float y) const;            ///< RW 0x67D400
	// the query's bucket rectangle around `where` for the radius r (already + 7.0): [x0, x1) x [y0, y1) (RW 0x67E0A3 .. 0x67E1F5, the same in RW 0x6854FD)
	void bucketRange(const Coord3D &where, float r, int &x0, int &y0, int &x1, int &y1) const;
	void clearRecord(Record &tree);                  ///< RW 0x67D3C2
	void markChanged(Record &tree);
	void keepStart();                                ///< lane SAVE-4e: m_start before the first change
	void insertCluster(int recordIndex, const std::vector<Record> &from); ///< RW 0x683C2D (the record copy of RW 0x8E4456)
	void rebuildClusters(const std::vector<Record> &from); ///< clear, then insert every record in list order
	std::vector<Record> m_start;                     ///< lane SAVE-4e: the records as the map made them (empty until the first change)
	GameLogic &m_logic;
	std::vector<Record> m_records;   ///< + 0x578
	AIPositionSampler m_clusters;     ///< + 0x584
	std::vector<std::int16_t> m_heads; ///< + 0x588 (50 * 50)
	std::vector<Event> m_events;
	float m_lo[2] = { 0.0f, 0.0f }, m_hi[2] = { 0.0f, 0.0f };
	bool m_hasExtent = false;
	int m_nextIndex = 0;             ///< + 0x48
	float m_toppleSpeed = 0.0f;      ///< + 0x1918
	std::uint32_t m_changedFrame = 0; ///< + 0x1910
	int m_changed = 0;
	int m_morphing = 0; ///< the records with `morph` set (update's fast path)
	bool startMorph(Record &tree, int type);  ///< RW 0x4EC8EB / 0x4EC40E: true when the tree started to morph
	void endMorph(Record &tree);              ///< RW 0x4EAAA7
	void removeTreeNow(std::uint32_t treeId); ///< RW 0x684412
};
