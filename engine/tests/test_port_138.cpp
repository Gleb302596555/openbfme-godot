// OpenBFME. GPL-3.0.
// Port 138 (issue #340, P-79): a destroyed Ennorath Wild economic structure is
// replaced through RebuildHoleBehavior, not through the camp RubbleRiseUpdate.
// Synthetic data only; the expected sequence is RW 0x8868C2 (hole wait/create),
// RW 0x8AD6A4 (worker rebuild construct: percent 0 at 0x8AD86F, health 1 at
// 0x8AD88E, conditions at 0x8AD8B5), RW 0x88DE43 (work: percent += 100/frames at
// 0x88DE99, health += max/frames at 0x88DEB5), and the existing caption path
// DrawableIconUI RW 0x677CF9. Ennorath's named-worker holes wait 120000 ms =
// 600 logic frames (RW 0xD9F608 = 5 fps); the named-worker hole below keeps
// that full mod delay, while the synthetic build is 2 s = 10 frames.
#include "doctest.h"
#include "CombatTestUtil.h"

#include "GameClient/ClientEvents.h"
#include "GameClient/DrawableIconUI.h"
#include "GameClient/DrawableManager.h"
#include "GameClient/GameTextTableSource.h"
#include "GameClient/TacticalView.h"
#include "GameEngineDevice/Win32Device/Common/Win32BIGFileSystem.h"
#include "Libraries/WWVegas/WW3D2/assetmgr.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/Module/RebuildHoleModules.h"
#include "GameLogic/Module/WorkerAIUpdate.h"

#include <string>
#include <vector>

using namespace combattest;

namespace
{
const char kObjects[] =
	"Object Port138Lair\n"
	"  KindOf = STRUCTURE IMMOBILE SELECTABLE\n"
	"  Geometry = CYLINDER\n  GeometryMajorRadius = 10\n  GeometryMinorRadius = 10\n  GeometryHeight = 10\n"
	"  BuildCost = 100\n  BuildTime = 2.0\n"
	"  ArmorSet\n    Conditions = None\n    Armor = NoArmor\n  End\n"
	"  Body = ActiveBody B\n    MaxHealth = 200\n  End\n"
	"  Behavior = GettingBuiltBehavior G\n    WorkerName = Port138Worker\n    SpawnTimer = 100\n  End\n"
	"  Behavior = KeepObjectDie K\n  End\n"
	"  Behavior = StructureCollapseUpdate C\n"
	"    MinCollapseDelay = 0\n    MaxCollapseDelay = 0\n    CollapseDamping = 0.5\n"
	"    MaxShudder = 0\n    MinBurstDelay = 1\n    MaxBurstDelay = 1\n    BigBurstFrequency = 1\n"
	"    DestroyObjectWhenDone = Yes\n    CollapseHeight = 10\n  End\n"
	"  Behavior = RebuildHoleExposeDie E\n    HoleName = Port138Hole\n    HoleMaxHealth = 50\n    TransferAttackers = No\n    FadeInTimeSeconds = 0\n  End\n"
	"  Behavior = DestroyDie D\n  End\n"
	"End\n"
	"Object Port138Hole\n"
	"  KindOf = STRUCTURE IMMOBILE REBUILD_HOLE\n"
	"  Geometry = CYLINDER\n  GeometryMajorRadius = 10\n  GeometryMinorRadius = 10\n  GeometryHeight = 2\n"
	"  ArmorSet\n    Conditions = None\n    Armor = NoArmor\n  End\n"
	"  Body = StructureBody B\n    MaxHealth = 1000\n    InitialHealth = 1000\n  End\n"
	"  Behavior = RebuildHoleBehavior R\n"
	"    WorkerObjectName = Port138Worker\n    WorkerRespawnDelay = 1000\n    HoleHealthRegen%PerSecond = 0.5%\n  End\n"
	"  Behavior = DestroyDie D\n  End\n"
	"End\n"
	"Object Port138DirectHole\n"
	"  KindOf = STRUCTURE IMMOBILE REBUILD_HOLE\n"
	"  Geometry = CYLINDER\n  GeometryMajorRadius = 10\n  GeometryMinorRadius = 10\n  GeometryHeight = 2\n"
	"  ArmorSet\n    Conditions = None\n    Armor = NoArmor\n  End\n"
	"  Body = StructureBody B\n    MaxHealth = 1000\n    InitialHealth = 1000\n  End\n"
	"  Behavior = RebuildHoleBehavior R\n"
	"    WorkerRespawnDelay = 1000\n    HoleHealthRegen%PerSecond = 0\n  End\n"
	"  Behavior = DestroyDie D\n  End\n"
	"End\n"
	"Object Port138Worker\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  Geometry = CYLINDER\n  GeometryMajorRadius = 2\n  GeometryMinorRadius = 2\n  GeometryHeight = 5\n"
	"  ArmorSet\n    Conditions = None\n    Armor = NoArmor\n  End\n"
	"  Body = ActiveBody B\n    MaxHealth = 100\n  End\n"
	"  Behavior = WorkerAIUpdate W\n    MaxBoxes = 0\n  End\n"
	"  LocomotorSet\n    Locomotor = WalkerLoco\n    Condition = SET_NORMAL\n    Speed = 1000\n  End\n"
	"  Behavior = DestroyDie D\n  End\n"
	"End\n"
	"Object Port138Camp\n"
	"  KindOf = STRUCTURE IMMOBILE SELECTABLE CASTLE_KEEP\n"
	"  Geometry = CYLINDER\n  GeometryMajorRadius = 10\n  GeometryMinorRadius = 10\n  GeometryHeight = 10\n"
	"  ArmorSet\n    Conditions = None\n    Armor = NoArmor\n  End\n"
	"  Body = ActiveBody B\n    MaxHealth = 200\n  End\n"
	"  Behavior = KeepObjectDie K\n  End\n"
	"  Behavior = StructureCollapseUpdate C\n"
	"    MinCollapseDelay = 0\n    MaxCollapseDelay = 0\n    CollapseDamping = 0.5\n"
	"    MaxShudder = 0\n    MinBurstDelay = 1\n    MaxBurstDelay = 1\n    BigBurstFrequency = 1\n"
	"    DestroyObjectWhenDone = No\n    CollapseHeight = 0\n  End\n"
	"  Behavior = RubbleRiseUpdate R\n"
	"    MinRubbleRiseDelay = 0\n    MaxRubbleRiseDelay = 0\n    RubbleRiseDamping = 0.5\n"
	"    RubbleHeight = 4\n    MaxShudder = 0\n    MinBurstDelay = 1\n    MaxBurstDelay = 1\n    BigBurstFrequency = 1\n  End\n"
	"  Behavior = DestroyDie D\n  End\n"
	"End\n";

struct Port138World : CombatWorld
{
	Port138World() : CombatWorld(kObjects) {}
	std::vector<Object *> alive(const char *name)
	{
		std::vector<Object *> out;
		for (Object *o = logic->getFirstObject(); o; o = o->getNextObject())
		{
			if (!o->isDestroyed() && !o->isEffectivelyDead() && o->getTemplate()->getName() == name)
				out.push_back(o);
		}
		return out;
	}
};

bool condition(const Object &o, const char *name) { return o.testModelCondition(CombatNames::modelCondition(name)); }
} // namespace

TEST_CASE("PORT-138 Wild hole: waits, then its worker constructs in stages with the RW percentage caption")
{
	Port138World w;
	Object *lair = w.unit("Port138Lair", 'A', 400.0f, 400.0f);
	REQUIRE(lair != nullptr);
	w.frames(1);
	lair->kill(DAMAGE_UNRESISTABLE, DEATH_NORMAL);
	w.frames(1);
	std::vector<Object *> holes = w.alive("Port138Hole");
	REQUIRE(holes.size() == 1);
	Object *hole = holes[0];
	RebuildHoleBehavior *rh = RebuildHoleBehavior::of(*hole);
	REQUIRE(rh != nullptr);
	// Ennorath's 120000 ms at RW's 5 logic fps = 600 frames; one hole update already ran.
	CHECK(rh->workerWaitCounter() == 599);
	CHECK(rh->getWorkerID() == INVALID_ID);
	CHECK(rh->getReconstructedBuildingID() == INVALID_ID);
	w.frames(598);
	CHECK(rh->workerWaitCounter() == 1);
	CHECK(w.alive("Port138Worker").empty());
	CHECK(rh->getReconstructedBuildingID() == INVALID_ID);
	w.frames(1);
	CHECK(rh->workerWaitCounter() == 0);
	std::vector<Object *> workers = w.alive("Port138Worker");
	REQUIRE(workers.size() == 1);
	Object *worker = workers[0];
	WorkerAIUpdate *ai = dynamic_cast<WorkerAIUpdate *>(worker->getAIUpdateInterface());
	REQUIRE(ai != nullptr);
	CHECK(ai->isRebuildConstructWorker());
	Object *rebuilt = w.byId(rh->getReconstructedBuildingID());
	REQUIRE(rebuilt != nullptr);
	CHECK(rebuilt->getTemplate()->getName() == "Port138Lair");
	CHECK(rebuilt->isUnderConstruction());
	CHECK(rebuilt->getConstructionPercent() == 0.0f); // RW 0x8AD86F
	CHECK(rebuilt->getBodyModule()->getHealth() == 1.0f); // RW 0x8AD88E
	CHECK(condition(*rebuilt, "PARTIALLY_CONSTRUCTED"));
	CHECK(condition(*rebuilt, "ACTIVELY_BEING_CONSTRUCTED"));
	CHECK_FALSE(condition(*rebuilt, "AWAITING_CONSTRUCTION"));
	CHECK_FALSE(condition(*rebuilt, "RUBBLE"));
	CHECK_FALSE(condition(*rebuilt, "POST_RUBBLE"));

	GameTextTableSource text;
	text.table.setOverride("CONTROLBAR:UnderConstructionDesc", "Building: %.0f%%");
	const int worked = w.runUntil([&] { return rebuilt->getConstructionPercent() > 0.0f; }, 50);
	REQUIRE(worked < 50); // tolerance: travel/turn frames precede the asserted 10-frame build
	// BuildTime 2 s = 10 frames: RW 0x88DE99 adds 10 percent, 0x88DEB5 adds 20 health.
	CHECK(rebuilt->getConstructionPercent() == 10.0f);
	CHECK(rebuilt->getBodyModule()->getHealth() == 21.0f);
	CHECK(rebuilt->isUnderConstruction());
	CHECK(DrawableIconUI::constructionText(&text, rebuilt->getConstructionPercent()) == "Building: 10%");
	// The actual icon pass, not only its formatting helper: a drawable bound to the rebuilt object emits one caption (RW 0x677CF9).
	Win32BIGFileSystem empty;
	ArchiveW3DFileSource source(empty);
	WW3DAssetManager assets(source);
	DrawableManager drawables(assets, *w.logic);
	ClientEventRecorder recorder(*w.logic);
	recorder.objectCreated(*rebuilt);
	drawables.applyEvents(recorder.take());
	REQUIRE(drawables.findByObject(rebuilt->getID()) != nullptr);
	PinholeView view({ 400.0f, 100.0f, 400.0f }, { 400.0f, 400.0f, 0.0f }, 1024, 768, 0.7f);
	IconUISettings settings;
	settings.text = &text;
	DrawableIconUI ui;
	std::vector<IconUIOp> ops;
	ui.build(*w.logic, drawables, view, settings, {}, 0, ops);
	int captions = 0;
	for (const IconUIOp &op : ops)
	{
		if (op.object == rebuilt->getID() && op.kind == IconUIOp::TEXT)
		{
			++captions;
			CHECK(op.text == "Building: 10%");
		}
	}
	CHECK(captions == 1);
	CHECK(condition(*rebuilt, "PARTIALLY_CONSTRUCTED"));
	CHECK(condition(*rebuilt, "ACTIVELY_BEING_CONSTRUCTED"));
	CHECK_FALSE(condition(*rebuilt, "RUBBLE"));

	const int finished = w.runUntil([&] { return !rebuilt->isUnderConstruction(); }, 100);
	REQUIRE(finished < 100); // tolerance: travel/turn frames precede the asserted 10-frame build
	CHECK(rebuilt->getConstructionPercent() == -1.0f); // RW 0x88DF61
	ops.clear();
	ui.build(*w.logic, drawables, view, settings, {}, 0, ops);
	for (const IconUIOp &op : ops)
	{
		if (op.object == rebuilt->getID())
			CHECK(op.kind != IconUIOp::TEXT);
	}
	CHECK_FALSE(condition(*rebuilt, "PARTIALLY_CONSTRUCTED"));
	CHECK_FALSE(condition(*rebuilt, "ACTIVELY_BEING_CONSTRUCTED"));
	CHECK(w.alive("Port138Hole").empty());
	CHECK(w.alive("Port138Worker").empty());
	CHECK(w.alive("Port138Lair").size() == 1);
}

TEST_CASE("PORT-138 Wild hole: healing the hole during the wait neither starts the rebuild nor restarts the wait")
{
	Port138World w;
	Object *lair = w.unit("Port138Lair", 'A', 420.0f, 420.0f);
	lair->kill(DAMAGE_UNRESISTABLE, DEATH_NORMAL);
	w.frames(1);
	std::vector<Object *> holes = w.alive("Port138Hole");
	REQUIRE(holes.size() == 1);
	Object *hole = holes[0];
	RebuildHoleBehavior *rh = RebuildHoleBehavior::of(*hole);
	REQUIRE(rh != nullptr);
	ActiveBody *body = dynamic_cast<ActiveBody *>(hole->getBodyModule());
	REQUIRE(body != nullptr);
	body->internalChangeHealth(-10.0f);
	const float before = body->getHealth();
	w.frames(1);
	CHECK(rh->workerWaitCounter() == 598);
	CHECK(rh->getReconstructedBuildingID() == INVALID_ID);
	CHECK(w.alive("Port138Worker").empty());
	// RW 0x886BB4/0x886BD0: (0.005 / 5) * 50 = 0.05 health per logic frame.
	CHECK(body->getHealth() == doctest::Approx(before + 0.05f)); // tolerance: x87 PC24 rounding of (0.005 / 5) * 50, RW 0x886BD0
	w.frames(2);
	CHECK(rh->workerWaitCounter() == 596);
	CHECK(rh->getReconstructedBuildingID() == INVALID_ID);
}

TEST_CASE("PORT-138 neighboring hole: no WorkerObjectName keeps the direct, complete build without a caption")
{
	Port138World w;
	Object *lair = w.unit("Port138Lair", 'A', 440.0f, 440.0f);
	Object *hole = w.unit("Port138DirectHole", 'A', 440.0f, 440.0f);
	REQUIRE(lair != nullptr);
	REQUIRE(hole != nullptr);
	RebuildHoleBehavior *rh = RebuildHoleBehavior::of(*hole);
	REQUIRE(rh != nullptr);
	rh->startRebuildProcess(lair->getTemplate(), lair->getID());
	w.frames(4);
	CHECK(rh->getReconstructedBuildingID() == INVALID_ID);
	w.frames(1);
	Object *rebuilt = w.byId(rh->getReconstructedBuildingID());
	REQUIRE(rebuilt != nullptr);
	CHECK(w.alive("Port138Worker").empty());
	CHECK_FALSE(rebuilt->isUnderConstruction());
	CHECK(rebuilt->getConstructionPercent() == -1.0f);
	GameTextTableSource text;
	text.table.setOverride("CONTROLBAR:UnderConstructionDesc", "Building: %.0f%%");
	CHECK(DrawableIconUI::constructionText(&text, rebuilt->getConstructionPercent()).empty());
	w.frames(1);
	CHECK(w.alive("Port138DirectHole").empty());
}

TEST_CASE("PORT-138 neighboring camp: RubbleRiseUpdate keeps its POST_RUBBLE to POST_COLLAPSE sequence, not a hole countdown")
{
	Port138World w;
	Object *camp = w.unit("Port138Camp", 'A', 460.0f, 460.0f);
	REQUIRE(camp != nullptr);
	camp->kill(DAMAGE_UNRESISTABLE, DEATH_NORMAL);
	w.frames(1);
	CHECK(condition(*camp, "POST_RUBBLE"));
	CHECK_FALSE(condition(*camp, "POST_COLLAPSE"));
	w.frames(1);
	CHECK_FALSE(condition(*camp, "POST_RUBBLE"));
	CHECK(condition(*camp, "POST_COLLAPSE"));
	CHECK_FALSE(camp->isUnderConstruction());
	CHECK(camp->getConstructionPercent() == -1.0f);
}
