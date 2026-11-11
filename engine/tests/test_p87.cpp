// OpenBFME unit tests. GPL-3.0.
// P-87 (docs/ennorath/PROBLEMS.md): Eva announced the same creep lair again and again ("goblins sighted"). Synthetic world (no game files),
// expectations read from the RotWK binary (caveat S-001), each check names its address:
//   * RW 0x678D03: the sighting routine (RW 0x678C69) announces EvaEnemyObjectSightedEvent only while the sighted drawable's own + 0x445 is 0.
//   * RW 0x678DB5 .. 0x678DDF -> RW 0x6709A2 / 0x670983: after the report a slave with UseSlaverAsControlForEvaObjectSightedEvents sets its own
//     + 0x445, its slaver drawable's and every live sibling's.
//   * RW 0x8A519B .. 0x8A51AE (the end of SlavedUpdate::onEnslave RW 0x8A510B) -> RW 0x6713A8: the new slave's drawable, when the slaved interface's
//     slot 5 (UseSlaverAsControlForEvaObjectSightedEvents, RW 0x6713AF .. 0x6713B6) answers true and the slaver has a drawable, copies the slaver
//     drawable's + 0x445 and + 0x446 (RW 0x6713C5 .. 0x6713D7). That is what keeps a replacement spawn of an announced lair silent; the port did
//     not have it, so every replacement announced the lair again.
//   * RW 0x67561E (Drawable::notifyDead, RW 0x67563E .. 0x67564C): a dying marked drawable clears + 0x445 and sets + 0x446, so the slaves of a lair
//     that died and came back announce once more, with EvaEnemyObjectSightedAfterRespawnEvent (RW 0x678D6C .. 0x678D89).

#include "doctest.h"
#include "EconTestUtil.h"

#include "GameClient/ClientEvents.h"
#include "GameClient/Drawable.h"
#include "GameClient/DrawableManager.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DDrawModules.h"
#include "GameLogic/Damage.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/SlavedUpdate.h"
#include "GameLogic/Module/SpawnBehavior.h"
#include "GameLogic/Object/Object.h"
#include "Libraries/WWVegas/WW3D2/assetmgr.h"

#include <list>
#include <string>
#include <vector>

namespace
{
// the lair's slave: a drawn object with a SlavedUpdate (no AI: SlavedUpdate::update returns before its movement half, RW 0x8A5D00)
std::string creep(const char *name, const char *useSlaver)
{
	return std::string("Object ") + name + "\n" +
		"  KindOf = INFANTRY SELECTABLE\n"
		"  Draw = W3DDefaultDraw ModuleTag_Draw\n  End\n"
		"  Body = ActiveBody ModuleTag_Body\n    MaxHealth = 100\n  End\n"
		"  Behavior = SlavedUpdate ModuleTag_Slave\n"
		"    MarkUnselectable = No\n"
		"    UseSlaverAsControlForEvaObjectSightedEvents = " + useSlaver + "\n"
		"  End\n"
		"End\n";
}

// the lair: drawn (RW 0x6713B8 .. 0x6713C3 needs the slaver's drawable), one spawn, replaced 5 logic frames after its death
std::string lair(const char *name, const char *creepName)
{
	return std::string("Object ") + name + "\n" +
		"  KindOf = STRUCTURE SELECTABLE\n"
		"  Draw = W3DDefaultDraw ModuleTag_Draw\n  End\n"
		"  Behavior = SpawnBehavior ModuleTag_Spawn\n"
		"    SpawnNumber = 1\n"
		"    SpawnReplaceDelay = 1000\n"
		"    SpawnTemplateName = " + creepName + "\n"
		"  End\n"
		"  Behavior = QueueProductionExitUpdate ModuleTag_Exit\n"
		"    UnitCreatePoint = X:20.0 Y:0.0 Z:0.0\n"
		"    NaturalRallyPoint = X:40.0 Y:0.0 Z:0.0\n"
		"  End\n"
		"  Body = ActiveBody ModuleTag_Body\n    MaxHealth = 1000\n  End\n"
		"End\n";
}

struct P87World : econtest::Fx
{
	ArchiveW3DFileSource source;
	WW3DAssetManager assets;
	DrawableManager drawables;
	ClientEventRecorder events;

	explicit P87World(const std::string &objects)
		: econtest::Fx(objects.c_str())
		, source(w.fx.fsys)
		, assets(source)
		, drawables(assets, *logic)
		, events(*logic)
	{
		W3DDrawModules::registerTypedDrawModuleData(w.modules);
		logic->setClientHooks(&events);
	}
	~P87World()
	{
		logic->setClientHooks(nullptr); // before the recorder goes, even when a REQUIRE ends the test
		logic.reset();
	}
	// one logic frame, then the client takes the frame's events in their order (the creation before the enslaving)
	void frame()
	{
		logic->runLogicFrame();
		drawables.applyEvents(events.take());
	}
	SpawnBehavior *spawner(Object *lairObj)
	{
		SpawnBehavior *module = dynamic_cast<SpawnBehavior *>(lairObj->findModule("SpawnBehavior"));
		REQUIRE(module);
		return module;
	}
	// runs frames until the lair has a spawn other than `notThis`
	Object *nextSpawn(SpawnBehavior *module, ObjectID notThis)
	{
		for (int i = 0; i < 20; ++i)
		{
			frame();
			if (!module->spawns().empty() && module->spawns().front() != notThis)
			{
				return logic->findObjectByID(module->spawns().front());
			}
		}
		return nullptr;
	}
};
} // namespace

// RW 0x8A51AE -> RW 0x6713A8: the replacement of an announced lair is born marked, so RW 0x678D03 keeps it silent
TEST_CASE("P-87 SlavedUpdate: a spawn made after its lair was announced takes the lair drawable's sighted mark (RW 0x8A51AE -> RW 0x6713A8)")
{
	P87World f(creep("P87Creep", "Yes") + lair("P87Lair", "P87Creep"));
	Object *lairObj = f.makeAt("P87Lair", "Alice", 200.0f, 200.0f);
	SpawnBehavior *module = f.spawner(lairObj);
	Object *first = f.nextSpawn(module, INVALID_ID);
	REQUIRE(first);
	Drawable *lairD = f.drawables.findByObject(lairObj->getID());
	Drawable *firstD = f.drawables.findByObject(first->getID());
	REQUIRE(lairD);
	REQUIRE(firstD);
	// nothing was announced yet: the slave copied an unmarked lair (RW 0x6713C5: + 0x445 = 0, + 0x446 = 0)
	CHECK_FALSE(firstD->evaEnemySighted());
	CHECK_FALSE(firstD->evaEnemySightedAfterRespawn());

	// the announcement of the first slave marks itself and its lair (RW 0x678DB5, RW 0x6709AD)
	firstD->setEvaEnemySighted(true);
	lairD->setEvaEnemySighted(true);

	const ObjectID firstId = first->getID();
	first->kill(DAMAGE_UNRESISTABLE, 0);
	Object *second = f.nextSpawn(module, firstId);
	REQUIRE(second);
	Drawable *secondD = f.drawables.findByObject(second->getID());
	REQUIRE(secondD);
	CHECK(secondD->evaEnemySighted());                   // RW 0x6713C5 .. 0x6713CB: the lair's + 0x445; RW 0x678D03 then skips the announcement
	CHECK_FALSE(secondD->evaEnemySightedAfterRespawn()); // RW 0x6713D1 .. 0x6713D7: the lair's + 0x446
	CHECK(lairD->evaEnemySighted());                     // the slave's death (RW 0x67561E on its own drawable) does not touch the lair's mark
}

// RW 0x67561E then RW 0x6713A8: a lair drawable that died marked carries + 0x445 = 0, + 0x446 = 1, and its next slave takes both
TEST_CASE("P-87 SlavedUpdate: a spawn takes the AfterRespawn mark of its slaver's drawable (RW 0x67563E .. 0x67564C, RW 0x6713D1 .. 0x6713D7)")
{
	P87World f(creep("P87CreepR", "Yes") + lair("P87LairR", "P87CreepR"));
	Object *lairObj = f.makeAt("P87LairR", "Alice", 200.0f, 200.0f);
	SpawnBehavior *module = f.spawner(lairObj);
	Object *first = f.nextSpawn(module, INVALID_ID);
	REQUIRE(first);
	Drawable *lairD = f.drawables.findByObject(lairObj->getID());
	REQUIRE(lairD);
	lairD->setEvaEnemySighted(true);
	lairD->notifyDead(); // RW 0x67561E: + 0x445 1 -> 0, + 0x446 = 1
	REQUIRE_FALSE(lairD->evaEnemySighted());
	REQUIRE(lairD->evaEnemySightedAfterRespawn());

	const ObjectID firstId = first->getID();
	first->kill(DAMAGE_UNRESISTABLE, 0);
	Object *second = f.nextSpawn(module, firstId);
	REQUIRE(second);
	Drawable *secondD = f.drawables.findByObject(second->getID());
	REQUIRE(secondD);
	CHECK_FALSE(secondD->evaEnemySighted());       // announced again (RW 0x678D03 passes) ...
	CHECK(secondD->evaEnemySightedAfterRespawn()); // ... with EvaEnemyObjectSightedAfterRespawnEvent (RW 0x678D6C .. 0x678D89)
}

// RW 0x6713AF .. 0x6713B6: the copy is only for a slaved interface whose slot 5 answers true
TEST_CASE("P-87 SlavedUpdate: a slave without UseSlaverAsControlForEvaObjectSightedEvents keeps its own marks (RW 0x6713AF .. 0x6713B6)")
{
	P87World f(creep("P87CreepN", "No") + lair("P87LairN", "P87CreepN"));
	Object *lairObj = f.makeAt("P87LairN", "Alice", 200.0f, 200.0f);
	SpawnBehavior *module = f.spawner(lairObj);
	Object *first = f.nextSpawn(module, INVALID_ID);
	REQUIRE(first);
	const SlavedUpdateInterface *slaved = SlavedUpdateInterface::of(*first);
	REQUIRE(slaved);
	CHECK_FALSE(slaved->useSlaverAsControlForEvaObjectSightedEvents());
	CHECK(slaved->getSlaverID() == lairObj->getID()); // enslaved all the same (RW 0x8A510B)
	Drawable *lairD = f.drawables.findByObject(lairObj->getID());
	REQUIRE(lairD);
	lairD->setEvaEnemySighted(true);

	const ObjectID firstId = first->getID();
	first->kill(DAMAGE_UNRESISTABLE, 0);
	Object *second = f.nextSpawn(module, firstId);
	REQUIRE(second);
	Drawable *secondD = f.drawables.findByObject(second->getID());
	REQUIRE(secondD);
	CHECK_FALSE(secondD->evaEnemySighted());
	CHECK_FALSE(secondD->evaEnemySightedAfterRespawn());
}
