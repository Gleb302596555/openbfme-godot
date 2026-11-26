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
#include "Common/StateHash.h"
#include "Common/GameState.h"
#include "Libraries/WWVegas/WW3D2/assetmgr.h"
#include "GameClient/ClientEvents.h"
#include "GameClient/Drawable.h"
#include "GameClient/DrawableManager.h"
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
	"Object Keep314\n"
	"  KindOf = STRUCTURE IMMOBILE\n"
	"  ShroudClearingRange = 40\n"
	"  VisionRange = 40\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 9\n"
	"End\n"
	"Object Scout314\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  ShroudClearingRange = 100\n"
	"  VisionRange = 100\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 5\n"
	"End\n"
	"Object Props314\n"
	"  KindOf = STRUCTURE IMMOBILE\n"
	"  ShroudClearingRange = 40\n"
	"  VisionRange = 40\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 9\n"
	"  Draw = W3DDefaultDraw ModuleTag_DefaultDraw\n"
	"  End\n"
	"End\n";

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

// RW 0xB4E190's retention test on the record, RW 0xB4F510's false branch and RW 0x691602's eligibility: a keep that a player saw and then lost in the fog keeps
// its record past its destruction; a player that never saw it has no record for it; a scout (not IMMOBILE) and a keep with W3DDefaultDraw never qualify.
TEST_CASE("PORT-314 ghost record: a destroyed immobile object keeps its shroud record for the player that saw it fogged (RW 0xB4E190), not for one that did "
		  "not, and a non-eligible object keeps none (RW 0x691602)")
{
	Fx f;
	const int alice = f.idx("Alice"), bob = f.idx("Bob");
	// cell (5, 5): Bob's keep. Its own look clears Bob's fog only (RW 0x694A45's mask: the owner and its allies); Bob never looks at the place himself
	Object *keep = f.place("Keep314", "Bob", 220.0f, 220.0f);
	// Alice sees the keep: her cell is CLEAR and RW 0xB4E890's clear branch sets her seen byte (RW 0xB4E98A)
	f.reveal(alice, 5, 5, 1);
	f.shroud.update(); // the record's look and its statuses (RW 0xB51970)
	CHECK(f.shroud.hasGhostObject(*keep)); // IMMOBILE, no W3DDefaultDraw: RW 0x691602's ghost
	CHECK(f.shroud.isRegistered(*keep));
	CHECK(f.shroud.getObjectStatus(*keep, alice) == OBJECTSHROUD_CLEAR);
	CHECK(f.shroud.hasSeen(*keep, alice));
	CHECK(f.shroud.getObjectStatus(*keep, bob) == OBJECTSHROUD_CLEAR); // the keep's own look: its owner's fog is clear
	// Alice looks away: the cell goes FOGGED (the unlook is immediate here, RW 0xB50E60) and the record's all-fogged branch stores 3 and keeps `seen` 1; for Bob
	// the cell stays CLEAR (his own look covers it), so only Alice's status can keep the record
	f.unreveal(alice, 5, 5, 1);
	f.shroud.update();
	CHECK(f.shroud.getCellStatus(alice, 5, 5) == CELLSHROUD_FOGGED);
	CHECK(f.shroud.getObjectStatus(*keep, alice) == OBJECTSHROUD_FOGGED);
	CHECK(f.shroud.hasSeen(*keep, alice));

	const size_t before = f.shroud.recordCount();
	f.destroy(keep);
	CHECK(f.logic->findObjectByID(keep->getID()) == nullptr); // the object is gone (GameLogic::processDestroyList's delete)
	CHECK(f.shroud.recordCount() == before);                 // RW 0xB4E190 returned 1: RW 0xB4F510 freed nothing
	CHECK(f.shroud.ghostRecordCount() == 1);

	// a scout is not IMMOBILE and a W3DDefaultDraw keep has the excluded draw module: neither keeps a record
	Object *scout = f.place("Scout314", "Alice", 620.0f, 220.0f);
	Object *props = f.place("Props314", "Alice", 220.0f, 620.0f);
	f.shroud.update(); // Alice's fog over the keep's cell still holds its retained record
	CHECK(f.shroud.isRegistered(*scout));
	CHECK_FALSE(f.shroud.hasGhostObject(*scout)); // RW 0x691626: no IMMOBILE
	CHECK_FALSE(f.shroud.hasGhostObject(*props)); // RW 0x691670: a W3DDefaultDraw draw module
	const size_t beforeScout = f.shroud.recordCount();
	f.destroy(scout);
	f.destroy(props);
	CHECK(f.shroud.recordCount() == beforeScout - 2); // RW 0xB4EAC0 freed both
	CHECK(f.shroud.ghostRecordCount() == 1);          // only the keep's record is kept
}

// RW 0xB4E890's cell branch on the retained record: the record still answers FOGGED for the player that saw it fogged (the status cached at the retention) and
// SHROUDED for a player that never saw it (the branch RW 0xB4E9D5 with `seen` 0)
TEST_CASE("PORT-314 ghost status: after the destruction the record still answers FOGGED for the player that saw it and SHROUDED for one that did not "
		  "(RW 0xB4E890 over the retained record)")
{
	Fx f;
	const int alice = f.idx("Alice"), bob = f.idx("Bob");
	Object *keep = f.place("Keep314", "Bob", 220.0f, 220.0f);
	f.reveal(alice, 5, 5, 1);
	f.shroud.update();
	CHECK(f.shroud.getObjectStatus(*keep, alice) == OBJECTSHROUD_CLEAR);
	f.unreveal(alice, 5, 5, 1);
	f.shroud.update();
	CHECK(f.shroud.getObjectStatus(*keep, alice) == OBJECTSHROUD_FOGGED);
	CHECK(f.shroud.getObjectStatus(*keep, bob) == OBJECTSHROUD_CLEAR);
	f.destroy(keep);
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
	CHECK((int)g.status[(size_t)alice] == (int)OBJECTSHROUD_FOGGED);  // kept: a seen byte and a FOGGED status (RW 0xB4E1B0)
	CHECK((int)g.status[(size_t)bob] == (int)OBJECTSHROUD_CLEAR);     // Bob's own look kept the cell clear
}

// RW 0xB4E100 queues the look off at the destruction (RW 0xB526A0: due at the counter + UnlookPersistDuration) and RW 0xB51690 removes it a frame later; a look
// that clears the boundary cell then makes RW 0xB4E890's clear branch set `seen` and status 1 on the retained record, and the retention test (a seen byte with
// status 3) can no longer hold it: the port's update() pass frees the record the way RW 0xB4EAC0 does
TEST_CASE("PORT-314 ghost release: the look queued off at the destruction ends the ghost's own look, and the next look that clears its cells frees the "
		  "record (RW 0xB4E100 / RW 0xB4E190)")
{
	Fx f;
	const int alice = f.idx("Alice");
	Object *keep = f.place("Keep314", "Bob", 220.0f, 220.0f);
	// Alice sees the whole 3 x 3 block around the keep (her clearing range 100 = cells 4 .. 6): the uncovered boundary cell (7, 5) of the ghost's circle stays
	// hers to fog and later clear
	f.reveal(alice, 5, 5, 1);
	f.shroud.update();
	CHECK(f.shroud.getObjectStatus(*keep, alice) == OBJECTSHROUD_CLEAR);
	f.unreveal(alice, 5, 5, 1);
	f.shroud.update();
	CHECK(f.shroud.getObjectStatus(*keep, alice) == OBJECTSHROUD_FOGGED);
	f.destroy(keep);
	REQUIRE(f.shroud.ghostRecordCount() == 1);
	// the retained record's statuses are the ghost's own; Alice's fog still holds it
	f.shroud.update();
	CHECK(f.shroud.ghostRecordCount() == 1);
	f.shroud.update();
	// Alice looks at the cell the keep's coverage held: the record's re-evaluation (RW 0xB4E890's clear branch) sets `seen` and status 1, and no player has a
	// seen byte with a FOGGED status any more
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
	Object *keep = f.place("Keep314", "Bob", 220.0f, 220.0f);
	f.reveal(alice, 5, 5, 1);
	f.shroud.update();
	CHECK_FALSE(f.shroud.hasGhostObject(*keep));
	f.unreveal(alice, 5, 5, 1);
	f.shroud.update();
	CHECK(f.shroud.getObjectStatus(*keep, alice) == OBJECTSHROUD_FOGGED);
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
	f.shroud.setDisplayed(true); // the client presents the shroud (LogicSnapshot::build's shroudLocal)
	const int alice = f.idx("Alice");
	Object *keep = f.place("Keep314", "Bob", 220.0f, 220.0f);
	f.reveal(alice, 5, 5, 1);
	f.shroud.update();
	f.unreveal(alice, 5, 5, 1);
	f.shroud.update();
	CHECK(f.shroud.getObjectStatus(*keep, alice) == OBJECTSHROUD_FOGGED);
	const ObjectID id = keep->getID(); // the object is deleted by destroy(): keep its id
	const std::shared_ptr<const LogicSnapshot> first = LogicSnapshot::build(*f.logic, nullptr, false, 0, alice);
	REQUIRE(first->find(id) != nullptr);
	CHECK(first->findGhost(id) == nullptr);
	f.destroy(keep);
	const std::shared_ptr<const LogicSnapshot> second = LogicSnapshot::build(*f.logic, first.get(), false, 0, alice);
	CHECK(second->find(id) == nullptr); // the record left the live walk
	const LogicSnapshot::GhostRecord *ghost = second->findGhost(id);
	REQUIRE(ghost != nullptr);
	CHECK(ghost->record.id == id);
	CHECK(ghost->record.position.x == doctest::Approx(220.0f)); // frozen where the building stood
	CHECK(ghost->record.position.y == doctest::Approx(220.0f));
	CHECK(ghost->record.tmpl != nullptr); // and as it was (its template; the model state is the last presented record)
	CHECK(ghost->record.objectShroud == (int)OBJECTSHROUD_FOGGED); // RW 0xB4E890's FOGGED: the ghost is drawn in the fog
	CHECK_FALSE(ghost->record.shroudedForLocal);                   // and not hidden (SHROUDED hides, RW 0xB4E890)
	CHECK(second->findGhost(9999) == nullptr);
	// the ghost outlives the frame after the destruction: the next snapshot freezes the previous ghost
	const std::shared_ptr<const LogicSnapshot> third = LogicSnapshot::build(*f.logic, second.get(), false, 0, alice);
	REQUIRE(third->findGhost(id) != nullptr);
	CHECK(third->findGhost(id)->record.position.x == 220.0f);
	// a look at the cells ends it for the viewer (RW 0xB4E890's clear branch): no ghost is presented, the empty ground shows
	f.reveal(alice, 5, 5, 1);
	f.shroud.update();
	const std::shared_ptr<const LogicSnapshot> fourth = LogicSnapshot::build(*f.logic, third.get(), false, 0, alice);
	CHECK(fourth->findGhost(id) == nullptr);
}

// the drawable of the destroyed object is what the device draws; it stays as the ghost's stand-in while the snapshot holds the ghost and is presented in the fog
// look (Drawable::presentedLook through the synced ghost record), then goes with the ghost. The destroyed event carries the shroud manager's answer (RW 0xB4E190).
TEST_CASE("PORT-314 ghost drawable: a destroyed building's drawable is still presented in the fog while the ghost stands, and goes when the cells are seen again")
{
	Fx f;
	f.shroud.setDisplayed(true);
	Player *alicePlayer = f.players.findPlayerWithName("Alice");
	f.logic->players().setLocalPlayer(alicePlayer);
	const int alice = alicePlayer->getPlayerIndex();
	ClientEventRecorder recorder(*f.logic);
	f.logic->setClientHooks(&recorder);
	struct Unhook
	{
		GameLogic &l;
		~Unhook() { l.setClientHooks(nullptr); }
	} unhook{ *f.logic };
	struct EmptySource : W3DFileSource
	{
		bool Exists(const std::string &) const override { return false; }
		bool Read(const std::string &, std::vector<std::uint8_t> &, std::string *) override { return false; }
	} source;
	WW3DAssetManager assets(source);
	DrawableManager drawables(assets, *f.logic);

	Object *keep = f.place("Keep314", "Bob", 220.0f, 220.0f);
	const ObjectID id = keep->getID();
	drawables.applyEvents(recorder.take());
	REQUIRE(drawables.findByObject(id) != nullptr);
	const DrawableID drawableId = drawables.findByObject(id)->getID();
	f.reveal(alice, 5, 5, 1);
	f.shroud.update();
	f.unreveal(alice, 5, 5, 1);
	f.shroud.update();
	REQUIRE(f.shroud.getObjectStatus(*keep, alice) == OBJECTSHROUD_FOGGED);
	const std::shared_ptr<const LogicSnapshot> first = LogicSnapshot::build(*f.logic, nullptr, false, 0, alice);
	drawables.syncTransforms(*first, 0.0, false);
	CHECK(drawables.ghostDrawableCount() == 0);

	f.destroy(keep);
	const std::vector<ClientEvent> events = recorder.take();
	const auto destroyedEv = std::find_if(events.begin(), events.end(), [&](const ClientEvent &e) { return e.kind == ClientEvent::DESTROYED && e.object == id; });
	REQUIRE(destroyedEv != events.end());
	CHECK(destroyedEv->ghost); // the retained record (RW 0xB4E190 returned 1)
	drawables.applyEvents(events);
	CHECK(drawables.findByObject(id) == nullptr);           // the object is gone: nothing finds the drawable through it
	REQUIRE(drawables.find(drawableId) != nullptr);          // the drawable is the ghost's stand-in
	CHECK(drawables.ghostDrawableCount() == 1);

	const std::shared_ptr<const LogicSnapshot> second = LogicSnapshot::build(*f.logic, first.get(), false, 0, alice);
	CHECK(second->find(id) == nullptr);
	REQUIRE(second->findGhost(id) != nullptr);
	drawables.syncTransforms(*second, 0.0, false);
	const Drawable *d = drawables.find(drawableId);
	REQUIRE(d != nullptr);                                   // still there after the sync: the snapshot holds the ghost
	const Drawable::PresentedLook look = d->presentedLook(second.get(), true);
	CHECK_FALSE(look.hidden);                                // drawn (RW 0xB4E890 FOGGED, not SHROUDED)
	CHECK(look.fogLook);                                     // in the fog look
	// the ghost keeps standing over later frames
	const std::shared_ptr<const LogicSnapshot> third = LogicSnapshot::build(*f.logic, second.get(), false, 0, alice);
	drawables.syncTransforms(*third, 0.0, false);
	REQUIRE(drawables.find(drawableId) != nullptr);
	CHECK_FALSE(drawables.find(drawableId)->presentedLook(third.get(), true).hidden);

	// Alice looks at the cells: the ghost is released (the shroud's update), the snapshot holds none, the drawable goes
	f.reveal(alice, 5, 5, 1);
	f.shroud.update();
	CHECK(f.shroud.ghostRecordCount() == 0);
	const std::shared_ptr<const LogicSnapshot> fourth = LogicSnapshot::build(*f.logic, third.get(), false, 0, alice);
	CHECK(fourth->findGhost(id) == nullptr);
	drawables.syncTransforms(*fourth, 0.0, false);
	CHECK(drawables.find(drawableId) == nullptr);
	CHECK(drawables.ghostDrawableCount() == 0);
}

// a ghost-eligible record that is never kept hashes as it did before PORT-314 (VIS1 without the ghost bytes); a retained record is hashed with them and the
// saved block (version 1.3) carries the retained record through a load that has no object for it (loadPostProcess)
TEST_CASE("PORT-314 ghost hash and save: only a retained record moves the shroud hash, and the retained record survives an xfer round trip without its object")
{
	auto hashOf = [](const ShroudManager &m) {
		StateHasher h;
		m.hash(h);
		return h.value();
	};
	// no ghost kept: the eligible and the not-eligible game hash alike
	{
		Fx a, b;
		b.shroud.setGhostsEnabled(false);
		a.place("Keep314", "Bob", 220.0f, 220.0f);
		b.place("Keep314", "Bob", 220.0f, 220.0f);
		a.shroud.update();
		b.shroud.update();
		CHECK(hashOf(a.shroud) == hashOf(b.shroud));
	}
	auto retainedGame = [](Fx &f) {
		const int alice = f.idx("Alice");
		Object *keep = f.place("Keep314", "Bob", 220.0f, 220.0f);
		f.reveal(alice, 5, 5, 1);
		f.shroud.update();
		f.unreveal(alice, 5, 5, 1);
		f.shroud.update();
		f.destroy(keep);
		f.shroud.update();
	};
	Fx a, b, loaded;
	b.shroud.setGhostsEnabled(false);
	retainedGame(a);
	retainedGame(b);
	REQUIRE(a.shroud.ghostRecordCount() == 1);
	REQUIRE(b.shroud.ghostRecordCount() == 0);
	CHECK(hashOf(a.shroud) != hashOf(b.shroud)); // the kept record (and its ghost / retained bytes) is in the hash
	const std::vector<std::string> tokens = { "obfme:Shroud" };
	std::vector<std::uint8_t> saved, again;
	a.logic->gameState().saveBlocks(tokens, saved);
	loaded.logic->gameState().loadBlocks(tokens, saved); // the record has no object in `loaded`: loadPostProcess keeps it (RW 0xB4E239 detached it)
	loaded.logic->gameState().saveBlocks(tokens, again);
	CHECK(saved == again);
	CHECK(loaded.shroud.ghostRecordCount() == 1);
	CHECK(hashOf(loaded.shroud) == hashOf(a.shroud));
}
