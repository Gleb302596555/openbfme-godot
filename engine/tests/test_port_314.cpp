// OpenBFME unit tests. GPL-3.0.
//
// PORT-314 (issue #750, S-561 / S-4970): the ghost objects. A destroyed IMMOBILE object keeps its shroud record (RW 0xB4E190, the retention branch of RW 0xB4F510)
// for the players that saw it while it was FOGGED; a player that never saw it gets nothing. The record is freed when a look makes the keeping condition false.
//
// Sources of every expectation (PLAN rule 10: RW + mod data, never this engine's own output):
//   RW 0xB4F510: the unregister path. RW 0xB4F547 `call 0xB4E190`; on 1 the record and the object are not freed (the call at RW 0xB4F549 and the free at RW
//                0xB4F554 / RW 0xB4F55A are the false branch).
//   RW 0xB4E190: the retention test. RW 0xB4E198 `mov eax, [esi + 8]` (the record's ghost), the unrolled loop RW 0xB4E1A3 .. 0xB4E21B: for each of the 20
//                players, RW 0xB4E1B0 loads the seen byte (+ 0xC4); a nonzero seen byte with a status (+ 0x24) of 3 (folded FOGGED, the `mov edx, 3` at RW 0xB4E1A8)
//                keeps the record. RW 0xB4E239: record + 4 = 0, the object's provider slot 0xC with 0, the ghost's slot 4. RW 0xB4E221: no ghost -> false.
//   RW 0x691602: where the ghost comes from. RW 0x69160C GlobalData + 0xC69; RW 0x691626 `test byte ptr [esi + 0x108], 4` (KindOf IMMOBILE, bit 2); RW 0x691634
//                the draw-module vector (stride 0x14 at + 0x2F0) and RW 0x691670 strcmp against "W3DDefaultDraw" (RW 0x691689: a hit -> no ghost); RW 0x6916AA
//                TheGhostObjectManager (RW 0xDE4BD0) vtable slot 0x1C with the object.
//   RW 0xB4E890: the status from the record's cells (+ 0x1C / + 0x20 the covered cells, + 0x24 the status), the branch at RW 0xB4E914 / 0xB4E92F: no cell or
//                all shrouded -> 4 SHROUDED and seen 0 (RW 0xB4E9D5); all fogged/shrouded -> 3 FOGGED, the ghost's slot 0x14 checkVisible may make it 4; a clear
//                cell -> 1 CLEAR and seen 1 (RW 0xB4E98A). The unlook that RW 0xB4E100 queues at the destruction reaches the cells one UnlookPersistDuration later
//                (RW 0xB526A0 / RW 0xB51690).
//   RW 0x62F8ED / the port's own Fx: the shroud is a 25 x 20 grid of 40-unit cells over (0,0)..(1000,800); a look of a 100-unit clearing range covers the
//                cells within its integer circle (RW 0xB50100, test_shroud.cpp's hand trace).
//
// The INI below is synthetic: no retail file is read (the port's tests run without game data), and the two facts the objects need (IMMOBILE -> the ghost, a
// W3DDefaultDraw draw module -> no ghost) are the only template rows the test uses.

#include "doctest.h"

#include "LogicTestUtil.h"

#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/Thing/ThingTemplate.h"
#include "GameClient/Drawable.h"
#include "GameClient/LogicSnapshot.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/System/ShroudManager.h"

#include <algorithm>
#include <string>
#include <vector>

using namespace logictest;

namespace
{
// the objects: a keep (IMMOBILE, no draw module row: eligible for a ghost), a walking scout (no IMMOBILE), and two keeps whose draw-module list the test names:
// `Draw = W3DDefaultDraw` is RW 0x691670's exclusion; the empty `Draw =` list is the object without a draw module of that class
const char kObjects[] =
	"Object Keep314\\n"
	"  KindOf = STRUCTURE IMMOBILE\\n"
	"  ShroudClearingRange = 40\\n"
	"  VisionRange = 40\\n"
	"  Geometry = CYLINDER\\n"
	"  GeometryMajorRadius = 9\\n"
	"End\\n"
	"Object Scout314\\n"
	"  KindOf = INFANTRY SELECTABLE\\n"
	"  ShroudClearingRange = 100\\n"
	"  VisionRange = 100\\n"
	"  Geometry = CYLINDER\\n"
	"  GeometryMajorRadius = 5\\n"
	"End\\n"
	"Object Props314\\n"
	"  KindOf = STRUCTURE IMMOBILE\\n"
	"  ShroudClearingRange = 40\\n"
	"  VisionRange = 40\\n"
	"  Geometry = CYLINDER\\n"
	"  GeometryMajorRadius = 9\\n"
	"  Draw = W3DDefaultDraw ModuleTag_DefaultDraw\\n"
	"  End\\n"
	"End\\n";

struct Fx : LogicWorld
{
	ShroudManager shroud;
	Fx()
		: shroud(*logic)
	{
		REQUIRE_MESSAGE(loadError.empty(), loadError);
		const std::string err = w.load(kObjects);
		REQUIRE_MESSAGE(err.empty(), err);
		shroud.init(40.0f, 1);
		shroud.setExtent(0.0f, 0.0f, 1000.0f, 800.0f);
		shroud.attach();
	}
	int idx(const char *name) { return players.findPlayerWithName(name)->getPlayerIndex(); }
	// a look of `player` that clears the cells around (x, y): the shroud's own low level entry (RW 0xB50C90), used to give a player sight of a place without
	// placing an object of that player's (the same call the object source makes)
	void reveal(int player, int cx, int cy, int radius)
	{
		ShroudManager::Radii r;
		r.forward = r.side = r.rear = radius;
		shroud.lookAt(cx, cy, r, 0.0f, 1u << player);
	}
	void unreveal(int player, int cx, int cy, int radius)
	{
		ShroudManager::Radii r;
		r.forward = r.side = r.rear = radius;
		shroud.unlookAt(cx, cy, r, 0.0f, 1u << player);
	}
	// destroy the object the way the simulation does: GameLogic::destroyObject -> the destroy list -> ~Object (friend_objectLeftWorld -> the shroud's hooks)
	void destroy(Object *o)
	{
		logic->destroyObject(o);
		logic->processDestroyList();
	}
	Object *place(const char *tmpl, const char *owner, float x, float y)
	{
		Object *o = make(tmpl, teamOf(owner));
		Coord3D p{ x, y, 0.0f };
		o->setPosition(&p);
		return o;
	}
};
} // namespace

// RW 0xB4E190's retention test on the record, RW 0xB4F510's false branch and RW 0x691602's eligibility: a keep that a player saw fogged keeps its record past
// its destruction; a player that never saw it has no record for it; a scout (not IMMOBILE) and a keep with W3DDefaultDraw never qualify.
TEST_CASE("PORT-314 ghost record: a destroyed immobile object keeps its shroud record for the player that saw it fogged (RW 0xB4E190), not for one that did "
		  "not, and a non-eligible object keeps none (RW 0x691602)")
{
	Fx f;
	const int alice = f.idx("Alice"), bob = f.idx("Bob");
	// Alice looks at the keep's place and away again: her cells are FOGGED (seen once), Bob never saw them
	f.reveal(alice, 5, 5, 1);
	f.unreveal(alice, 5, 5, 1);
	CHECK(f.shroud.getCellStatus(alice, 5, 5) == CELLSHROUD_FOGGED);
	CHECK(f.shroud.getCellStatus(bob, 5, 5) == CELLSHROUD_SHROUDED);

	Object *keep = f.place("Keep314", "Alice", 220.0f, 220.0f); // cell (5, 5)
	f.shroud.update();                                          // the record gets its look and its statuses (RW 0xB51970)
	CHECK(f.shroud.hasGhostObject(*keep));                      // IMMOBILE, no W3DDefaultDraw: RW 0x691602's ghost
	// Alice's status is FOGGED with the seen byte set (RW 0xB4E890's all-fogged branch stores 3 and leaves `seen` 1); Bob's is SHROUDED with seen 0
	CHECK(f.shroud.isRegistered(*keep));
	CHECK(f.shroud.getObjectStatus(*keep, alice) == OBJECTSHROUD_FOGGED);
	CHECK(f.shroud.hasSeen(*keep, alice));
	CHECK(f.shroud.getObjectStatus(*keep, bob) == OBJECTSHROUD_SHROUDED);
	CHECK_FALSE(f.shroud.hasSeen(*keep, bob));

	const size_t before = f.shroud.recordCount();
	f.destroy(keep);
	CHECK(f.logic->findObjectByID(keep->getID()) == nullptr); // the object is gone (GameLogic::processDestroyList's delete)
	CHECK(f.shroud.recordCount() == before);                // RW 0xB4E190 returned 1: RW 0xB4F510 freed nothing
	CHECK(f.shroud.ghostRecordCount() == 1);

	// a scout is not IMMOBILE and a W3DDefaultDraw keep has the excluded draw module: neither keeps a record
	Object *scout = f.place("Scout314", "Alice", 620.0f, 220.0f);
	Object *props = f.place("Props314", "Alice", 220.0f, 620.0f);
	f.shroud.update();
	CHECK(f.shroud.isRegistered(*scout));
	CHECK_FALSE(f.shroud.hasGhostObject(*scout)); // RW 0x691626: no IMMOBILE
	CHECK_FALSE(f.shroud.hasGhostObject(*props)); // RW 0x691670: a W3DDefaultDraw draw module
	const size_t beforeScout = f.shroud.recordCount();
	f.destroy(scout);
	f.destroy(props);
	CHECK(f.shroud.recordCount() == beforeScout - 2); // RW 0xB4EAC0 freed both
	CHECK(f.shroud.ghostRecordCount() == 1);          // only the keep's record is kept
}

// RW 0xB4E890's cell branch on the retained record: the cells it covered are still its statuses, so the player that saw it fogged keeps seeing a FOGGED
// record where the building stood, and the player that never saw it gets SHROUDED (RW 0xB4E914's no-cell branch clears `seen`)
TEST_CASE("PORT-314 ghost status: after the destruction the record still answers FOGGED for the player that saw it and SHROUDED for one that did not "
		  "(RW 0xB4E890 over the retained record)")
{
	Fx f;
	const int alice = f.idx("Alice");
	f.reveal(alice, 5, 5, 1);
	f.unreveal(alice, 5, 5, 1);
	Object *keep = f.place("Keep314", "Alice", 220.0f, 220.0f);
	f.shroud.update();
	f.destroy(keep);
	// the retained record has no object: the status comes from the record and the cells it still covers
	const ShroudManager::GhostRecordView g = [&] {
		for (size_t i = 0; i < f.shroud.recordSlotCount(); ++i)
		{
			const ShroudManager::GhostRecordView v = f.shroud.ghostRecordView(i);
			if (v.valid && v.id == keep->getID())
			{
				return v;
			}
		}
		return ShroudManager::GhostRecordView();
	}();
	REQUIRE(g.valid);
	CHECK((int)g.status[(size_t)f.idx("Alice")] == (int)OBJECTSHROUD_FOGGED);
	CHECK((int)g.status[(size_t)f.idx("Bob")] == (int)OBJECTSHROUD_SHROUDED);
}

// RW 0xB4E100 queues the look off at the destruction (RW 0xB526A0: due at the counter + UnlookPersistDuration) and RW 0xB51690 removes it a frame later; the
// cells Alice looked at go back to SHROUDED and the retention test (RW 0xB4E190: a nonzero seen byte and status 3) fails, so the record goes
TEST_CASE("PORT-314 ghost release: the look queued off at the destruction ends the ghost's coverage, and the next look that clears the cells frees the record "
		  "(RW 0xB4E100 / RW 0xB4E190)")
{
	Fx f;
	const int alice = f.idx("Alice");
	// Alice sees the keep for now (CLEAR), Bob nothing
	f.reveal(alice, 5, 5, 1);
	Object *keep = f.place("Keep314", "Alice", 220.0f, 220.0f);
	f.shroud.update();
	CHECK(f.shroud.getObjectStatus(*keep, alice) == OBJECTSHROUD_CLEAR);
	CHECK(f.shroud.hasSeen(*keep, alice));
	// a fogged status with the seen byte is what keeps the record: Alice still looks, so the keep would be CLEAR — take her look away first
	f.unreveal(alice, 5, 5, 1);
	f.shroud.update(); // FOGGED with seen 1 (RW 0xB4E890 writes 3 and leaves `seen`)
	CHECK(f.shroud.getObjectStatus(*keep, alice) == OBJECTSHROUD_FOGGED);
	CHECK(f.shroud.hasSeen(*keep, alice));
	f.destroy(keep);
	CHECK(f.shroud.ghostRecordCount() == 1);
	// the destroyed object's look is queued off (RW 0xB4E100): the counter passes its due frame, the unlook returns the cells to SHROUDED
	f.shroud.update();
	f.shroud.update();
	CHECK(f.shroud.getCellStatus(alice, 5, 5) == CELLSHROUD_SHROUDED);
	// a look clears the cells: RW 0xB4E890's clear branch sets `seen` and status 1, and the retention test can no longer hold the record
	f.reveal(alice, 5, 5, 1);
	f.shroud.update();
	CHECK(f.shroud.getCellStatus(alice, 5, 5) == CELLSHROUD_CLEAR);
	CHECK(f.shroud.ghostRecordCount() == 0); // RW 0xB4EAC0 through the port's release pass
}

// RW 0xB4E190's ghost gate: the record is kept only with ghost data at + 8. With GlobalData + 0xC69 off (RW 0x69160C; the port's switch setGhostsEnabled, the
// gate the constructor RW 0x643402 stores 1 in) a new object gets no ghost, so its destruction frees the record as before
TEST_CASE("PORT-314 ghost gate: with ghosts disabled (GlobalData + 0xC69) no record is kept (RW 0x69160C / RW 0xB4E198)")
{
	Fx f;
	const int alice = f.idx("Alice");
	f.shroud.setGhostsEnabled(false);
	f.reveal(alice, 5, 5, 1);
	f.unreveal(alice, 5, 5, 1);
	Object *keep = f.place("Keep314", "Alice", 220.0f, 220.0f);
	f.shroud.update();
	CHECK_FALSE(f.shroud.hasGhostObject(*keep));
	const size_t before = f.shroud.recordCount();
	f.destroy(keep);
	CHECK(f.shroud.recordCount() == before - 1);
	CHECK(f.shroud.ghostRecordCount() == 0);
}

// the client stand-in: the frozen ObjectSnapshot (DONOR BFME2 W3DGhostObjectScene.cpp snapShot keeps a clone of the render objects; the port's snaphot carries
// the record) is drawn where the building stood while the local player's status is FOGGED, and a drawable whose object is gone asks presentedLook for it
TEST_CASE("PORT-314 client ghost record: the snapshot freezes the destroyed object's last record for the local player's fog (RW 0xB4E190's kept record)")
{
	Fx f;
	const int alice = f.idx("Alice");
	f.reveal(alice, 5, 5, 1);
	f.unreveal(alice, 5, 5, 1);
	Object *keep = f.place("Keep314", "Alice", 220.0f, 220.0f);
	f.shroud.update();
	const std::shared_ptr<const LogicSnapshot> first = LogicSnapshot::build(*f.logic, nullptr, false, 0, alice);
	REQUIRE(first->find(keep->getID()) != nullptr);
	CHECK(first->findGhost(keep->getID()) == nullptr);
	f.destroy(keep);
	const std::shared_ptr<const LogicSnapshot> second = LogicSnapshot::build(*f.logic, first.get(), false, 0, alice);
	CHECK(second->find(keep->getID()) == nullptr); // the record left the live walk
	const LogicSnapshot::GhostRecord *ghost = second->findGhost(keep->getID());
	REQUIRE(ghost != nullptr);
	CHECK(ghost->record.id == keep->getID());
	CHECK(ghost->record.position.x == doctest::Approx(220.0f)); // frozen where the building stood
	CHECK(ghost->record.position.y == doctest::Approx(220.0f));
	CHECK(ghost->record.tmpl == keep->getTemplate()); // and as it was (its template; the model state is the last presented record)
	CHECK(second->findGhost(9999) == nullptr);
}
