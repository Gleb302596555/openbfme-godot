// P-77 (docs/ennorath/PROBLEMS.md): Ennorath's Isengard Uruk pit vanishes when its construction finishes. Ennorath 1.9.8.2 data:
// includes/commandset.inc IsengardPorterCommandSet (the builder IsengardPorter, object/evilfaction/units/isengard/porter.ini) has
// Command_PorterConstructIsengardUrukPit: DOZER_CONSTRUCT of IsengardUrukPitUnpack (commandbutton.ini). IsengardUrukPitUnpack
// (object/evilfaction/structures/isengard/urukpit.ini) has CastleBehavior: CastleToUnpackForFaction Isengard UrukPit_Isengard, InstantUnpack = Yes,
// FilterValidOwnedEntries = ANY +STRUCTURE +WALK_ON_TOP_OF_WALL +BASE_FOUNDATION +TACTICAL_MARKER; its layout (Bases urukpit_isengard .bse) holds
// IsengardUrukPitExpansionPad and IsengardUrukPit. GPL-3.0.

#include "EnnorathScenario.h"
#include "HudTestUtil.h"
#include "Common/PlayerList.h"
#include "GameClient/LiveGame.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/CastleModules.h"
#include "GameLogic/Module/DozerAIUpdate.h"

#include <cstdio>
#include <set>
#include <string>

namespace
{
GameMessage selectMsg(int player, ObjectID id)
{
	GameMessage m(MSG_CREATE_SELECTED_GROUP, player);
	m.appendBooleanArgument(true);
	m.appendObjectIDArgument(id);
	return m;
}
GameMessage dozerMsg(int player, const ThingTemplate &t, const Coord3D &loc, float angle)
{
	GameMessage m(MSG_DOZER_CONSTRUCT, player);
	m.appendIntegerArgument((int)t.getTemplateID());
	m.appendLocationArgument(loc);
	m.appendRealArgument(angle);
	return m;
}
void dumpNear(GameLogic &logic, const Coord3D &at, const char *tag)
{
	for (Object *o = logic.getFirstObject(); o; o = o->getNextObject())
	{
		const float dx = o->getPosition()->x - at.x, dy = o->getPosition()->y - at.y;
		if (dx * dx + dy * dy < 150.0f * 150.0f)
		{
			std::printf("P-77 %s object %u %s constr=%d inWorld=%d destroyed=%d pos=(%.0f %.0f)\n", tag, (unsigned)o->getID(), o->getTemplate()->getName().c_str(),
			            o->isUnderConstruction() ? 1 : 0, o->isInWorld() ? 1 : 0, o->isDestroyed() ? 1 : 0, o->getPosition()->x, o->getPosition()->y);
		}
	}
}
} // namespace

TEST_CASE("P-77 ennorath: Isengard's porter builds the Uruk pit; when it finishes the castle unpacks IsengardUrukPit and it stays")
{
	ennorathscenario::World world;
	if (!world.load("P-77 uruk pit"))
	{
		return;
	}
	hudtest::Rig game(world.shared, "map mp fall back 4p", "FactionIsengard");
	GameLogic &logic = game.logic();
	Player *me = game.local;
	me->getMoney()->deposit(20000u, nullptr, false);
	std::string err;
	const Coord3D site = game.freeSpot(1800, 1800, 200.0f);
	Object *porter = game.game->createObject("IsengardPorter", game.index(), Coord3D{ site.x - 120.0f, site.y, site.z }, 0.0f, &err);
	REQUIRE_MESSAGE(porter != nullptr, err);
	REQUIRE(dynamic_cast<DozerAIUpdate *>(porter->getAIUpdateInterface()) != nullptr);
	const ThingTemplate *tt = logic.things().findTemplate("IsengardUrukPitUnpack");
	REQUIRE(tt != nullptr);
	tt = tt->getFinalOverride();
	logic.runLogicFrame();

	std::set<ObjectID> before;
	for (Object *o = logic.getFirstObject(); o; o = o->getNextObject())
	{
		before.insert(o->getID());
	}
	game.game->commands().append(selectMsg(game.index(), porter->getID()));
	game.game->commands().append(dozerMsg(game.index(), *tt, site, 0.0f));
	logic.runLogicFrame();
	for (const std::string &r : game.game->buildCommands().refusals())
	{
		std::printf("P-77 refused: %s\n", r.c_str());
	}
	Object *pit = nullptr;
	for (int i = 0; i < 300 && !pit; ++i)
	{
		for (Object *o = logic.getFirstObject(); o; o = o->getNextObject())
		{
			if (!before.count(o->getID()) && o->getTemplate()->getFinalOverride() == tt)
			{
				pit = o;
			}
		}
		if (!pit)
		{
			logic.runLogicFrame();
		}
	}
	REQUIRE_MESSAGE(pit != nullptr, "the porter made the foundation");
	const ObjectID pitId = pit->getID();
	std::printf("P-77 pit %u frame %u constr=%d\n", (unsigned)pitId, (unsigned)logic.getFrame(), pit->isUnderConstruction() ? 1 : 0);
	int frames = 0;
	for (int guard = 3000; guard > 0; --guard)
	{
		pit = logic.findObjectByID(pitId);
		if (!pit || pit->isDestroyed() || (!pit->isUnderConstruction() && pit->getConstructionPercent() < 0.0f && frames > 0))
		{
			break;
		}
		if (frames % 150 == 0)
		{
			std::printf("P-77 build +%d constr=%d pct=%.1f\n", frames, pit->isUnderConstruction() ? 1 : 0, pit->getConstructionPercent());
		}
		logic.runLogicFrame();
		++frames;
	}
	pit = logic.findObjectByID(pitId);
	std::printf("P-77 build ended after %d frames: pit=%d destroyed=%d constr=%d\n", frames, pit ? 1 : 0, pit ? (pit->isDestroyed() ? 1 : 0) : -1,
	            pit ? (pit->isUnderConstruction() ? 1 : 0) : -1);
	REQUIRE_MESSAGE(pit != nullptr, "the pit object vanished while rising");
	CHECK_FALSE(pit->isUnderConstruction());
	// the castle's first update unpacks the layout (InstantUnpack, RW 0x79C265 -> 0x79BE6A): the pad first, then the keep at the castle's own spot. The keep's
	// legality check (RW 0x7987EE -> thunk 0x797A96, bit 8 added) does not count the pad, a member of this BASE_SITE castle (RW 0x796E9A .. 0x796EC7)
	logic.runLogicFrame();
	pit = logic.findObjectByID(pitId);
	REQUIRE(pit != nullptr);
	CastleBehavior *cb = dynamic_cast<CastleBehavior *>(pit->findModule("CastleBehavior"));
	REQUIRE(cb != nullptr);
	std::printf("P-77 unpack: state=%d owned=%zu keep=%u err=%s\n", cb->state(), cb->ownedObjects().size(), (unsigned)cb->keepId(), cb->lastError().c_str());
	CHECK(cb->state() == CastleBehavior::STATE_UNPACKED);
	CHECK(cb->lastError().empty());
	CHECK(cb->ownedObjects().size() == 2); // urukpit_isengard.bse: IsengardUrukPitExpansionPad, IsengardUrukPit
	const ObjectID keepId = cb->keepId();
	Object *keep = keepId != INVALID_ID ? logic.findObjectByID(keepId) : nullptr;
	REQUIRE_MESSAGE(keep != nullptr, "the layout's IsengardUrukPit became the castle's keep");
	CHECK(keep->getTemplate()->getName() == "IsengardUrukPit");
	CHECK(keep->getControllingPlayer() == me);
	// the owner's symptom: a few frames later everything was gone (no keep: KeepDeathKillsEverything). It stands
	for (int i = 0; i < 300; ++i)
	{
		logic.runLogicFrame();
	}
	keep = logic.findObjectByID(keepId);
	std::printf("P-77 300 frames later: keep=%d\n", keep && !keep->isDestroyed() ? 1 : 0);
	dumpNear(logic, site, "end");
	CHECK((keep != nullptr && !keep->isDestroyed() && !keep->isEffectivelyDead()));
}
