// OpenBFME. GPL-3.0.
// LOCAL-107 (fork issue 785): unit AI core — AIGateUpdate, ThreatFinderUpdate, AIUpdateInterface (editions: both RotWK 2.01 and BFME2 1.06).
// Sources:
//   - AIGateUpdate::~AIGateUpdate: RW 0x008B490F (scalar deleting dtor RW 0x008B4A33, PolygonTrigger::getPolygonTriggerByID RW 0x006E4A7F,
//     PolygonTrigger::removePolygonTrigger RW 0x6E4ABE, object trigger cleanup RW 0x6263AF); DONOR BFME2 AIGateUpdate.
//   - AIGateUpdate::update / onTrigger / getGateInterface: RW 0x008B4D34, RW 0x008B4AF4, RW 0x008B4A4F.
//   - AIUpdateInterface::chooseLocomotorSet / LocomotorSet::addLocomotor: RW 0x0073DD59, RW 0x005E80B0.
//   - ThreatFinderManager::~ThreatFinderManager: RW 0x007EE2D1, RW 0x007EE293, RW 0x005B468A.
//   - Ennorath INI: data/ini/object/civilian/amonsulbuildings.ini (AmonSulGate), GondorFighterHorde, GondorFighter.

#include "doctest.h"

#include "CombatTestUtil.h"
#include "EnnorathScenario.h"
#include "HudTestUtil.h"

#include "Common/GameEdition.h"
#include "Common/Player.h"
#include "Common/PlayerTemplate.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/GateModules.h"
#include "GameLogic/Module/ThreatFinderUpdate.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/ScriptEngine/ScriptEngine.h"
#include "GameLogic/SkirmishAI/SkirmishAIData.h"
#include "GameLogic/SkirmishAI/SkirmishAIManager.h"
#include "GameLogic/System/ThreatFinder.h"

#include <string>

namespace
{
const char kGateObjects107[] =
	"SkirmishAIData\n"
	"End\n"
	"Locomotor TestLoco107\n"
	"  Surfaces = GROUND\n"
	"  Speed = 40\n"
	"  Acceleration = 100\n"
	"  Braking = 100\n"
	"End\n"
	"Object TestGate107\n"
	"  KindOf = STRUCTURE IMMOBILE SELECTABLE\n"
	"  Body = ActiveBody BodyTag\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"  Behavior = GateOpenAndCloseBehavior GateTag\n"
	"    ResetTimeInMilliseconds = 100\n"
	"    OpenByDefault = No\n"
	"    PercentOpenForPathing = 50\n"
	"  End\n"
	"  Behavior = AIGateUpdate AIGateTag\n"
	"    TriggerWidthX = 80.0\n"
	"    TriggerWidthY = 80.0\n"
	"  End\n"
	"End\n"
	"Object TestUnit107\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  Body = ActiveBody BodyTag\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = TestLoco107\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 40\n"
	"  End\n"
	"  Behavior = AIUpdateInterface AITag\n"
	"  End\n"
	"End\n"
	"Object TestThreatMarker107\n"
	"  KindOf = IMMOBILE\n"
	"  Body = ActiveBody BodyTag\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = ThreatFinderUpdate TFTag\n"
	"    DefaultRadius = 120.0\n"
	"  End\n"
	"End\n";

struct GateWorld107 : combattest::CombatWorld
{
	SkirmishAIStore aiStore;
	PlayerTemplate aiTemplate;

	GateWorld107() : aiStore(w.keys)
	{
		GateModules::registerAll(w.modules);
		ThreatFinderUpdate::registerModule(w.modules);
		aiStore.registerBlocks(w.fx.env.blocks);
		const std::string err = w.load(kGateObjects107);
		REQUIRE_MESSAGE(err.empty(), err);
		aiTemplate.m_playableSide = true;
		logic->skirmishAI().setStore(&aiStore);
	}
};

bool hasTriggerEntry(const Object &obj, int triggerIdx)
{
	for (int i = 0; i < obj.triggerCount(); ++i)
	{
		if (obj.triggerEntry(i).trigger == triggerIdx && obj.triggerEntry(i).inside)
		{
			return true;
		}
	}
	return false;
}
} // namespace

// RW 0x008B490F (~AIGateUpdate), RW 0x008B4A33 (scalar deleting dtor), RW 0x006E4A7F (PolygonTrigger::getPolygonTriggerByID):
// destroying a gate with an active AIGateUpdate removes its runtime polygon trigger from ScriptEngine (RW 0x6E4ABE)
// and clears the trigger entry from every object that was inside it (RW 0x6263AF).
TEST_CASE("LOCAL-107 AIGateUpdate: destroying a gate removes its runtime polygon trigger and clears unit entries (RW 0x008B490F)")
{
	for (GameEdition edition : { GameEdition::RotWK201, GameEdition::BFME2_106 })
	{
		GateWorld107 world;
		world.logic->setEdition(edition);
		CAPTURE(gameEditionName(edition));

		Object *gate = world.unit("TestGate107", 'A', 300.0f, 300.0f);
		Object *ally = world.unit("TestUnit107", 'A', 305.0f, 300.0f);
		REQUIRE(gate != nullptr);
		REQUIRE(ally != nullptr);

		// Make the gate's controlling player a playable computer player so AIGateUpdate activates (RW 0x8B4D34 -> RW 0x6A950B).
		Player *owner = gate->getControllingPlayer();
		REQUIRE(owner != nullptr);
		owner->init(&world.aiTemplate, 0);
		owner->setPlayerType(PLAYER_COMPUTER, true);
		world.logic->skirmishAI().newGame(true);
		REQUIRE(world.logic->skirmishAI().findAI(owner->getPlayerIndex()) != nullptr);

		auto *aiGate = dynamic_cast<AIGateUpdate *>(gate->findModule("AIGateUpdate"));
		REQUIRE(aiGate != nullptr);
		REQUIRE(GateOpenAndCloseBehavior::findGate(*gate) != nullptr);

		// Advance a frame so AIGateUpdate creates its runtime trigger, then move ally across an integer cell boundary (RW 0x69264D).
		world.frames(1);
		const int trigIdx = aiGate->triggerIndex();
		REQUIRE(trigIdx == 0);
		Coord3D pos = *ally->getPosition();
		pos.x += 3.0f;
		ally->setPosition(&pos);
		world.frames(2);
		CHECK(aiGate->alliesInside() == 1);
		CHECK(world.logic->scriptEngine().triggerAreaAt((size_t)trigIdx).points.size() == 4);
		CHECK(hasTriggerEntry(*ally, trigIdx));

		// Destroy the gate: ~AIGateUpdate (RW 0x008B490F) must remove the trigger and clear ally's trigger entry.
		world.logic->destroyObject(gate);
		world.frames(1);

		CHECK(world.logic->scriptEngine().triggerAreaAt((size_t)trigIdx).points.empty());
		CHECK_FALSE(hasTriggerEntry(*ally, trigIdx));
	}
}

// Neighbour test: a human player's gate never creates a runtime trigger (RW 0x8B4D34 -> RW 0x6A950B) and destroys cleanly;
// AIUpdateInterface chooses its locomotor set (RW 0x0073DD59 / RW 0x005E80B0), and ThreatFinderUpdate / ThreatFinderManager
// construct and tear down cleanly (RW 0x007EE2D1 / RW 0x007EE293 / RW 0x005B468A).
TEST_CASE("LOCAL-107 neighbour: human gate destroys cleanly, AIUpdateInterface locomotor set and ThreatFinderUpdate lifecycle")
{
	GateWorld107 world;

	// Human player A: AIGateUpdate stays disabled (no SkirmishAI) and triggerIndex remains -1.
	Object *gate = world.unit("TestGate107", 'A', 300.0f, 300.0f);
	REQUIRE(gate != nullptr);
	auto *aiGate = dynamic_cast<AIGateUpdate *>(gate->findModule("AIGateUpdate"));
	REQUIRE(aiGate != nullptr);
	world.frames(2);
	CHECK(aiGate->triggerIndex() == -1);
	world.logic->destroyObject(gate);
	world.frames(1);

	// AIUpdateInterface locomotor set selection (RW 0x0073DD59, RW 0x005E80B0) and aiIdle.
	Object *u = world.unit("TestUnit107", 'A', 200.0f, 200.0f);
	REQUIRE(u != nullptr);
	AIUpdateInterface *ai = u->getAIUpdateInterface();
	REQUIRE(ai != nullptr);
	CHECK(ai->curLocomotor() != nullptr);
	ai->aiMoveToPosition(Coord3D{ 260.0f, 200.0f, 0.0f }, CMD_FROM_PLAYER);
	world.frames(2);
	ai->aiIdle(CMD_FROM_PLAYER);
	CHECK(ai->isIdle());

	// ThreatFinderUpdate + ThreatFinderManager lifecycle (RW 0x007EE2D1, RW 0x007EE293, RW 0x005B468A).
	Object *marker = world.unit("TestThreatMarker107", 'A', 250.0f, 250.0f);
	REQUIRE(marker != nullptr);
	auto *tfu = dynamic_cast<ThreatFinderUpdate *>(marker->findModule("ThreatFinderUpdate"));
	REQUIRE(tfu != nullptr);
	world.frames(2);
	world.logic->destroyObject(marker);
	world.frames(1);
}

// Ennorath INI scenario: real Ennorath horde attack/guard, real fortress gate (AmonSulGate, amonsulbuildings.ini)
// opening for its own units, staying shut for enemies, and cleaning up its runtime trigger on destruction (RW 0x008B490F).
TEST_CASE("LOCAL-107 ENNORATH scenario: horde attack/guard, AmonSulGate AIGateUpdate open/enemy/destroy, and target selection")
{
	ennorathscenario::World world;
	if (!world.load("LOCAL-107 Ennorath scenario"))
	{
		return;
	}

	hudtest::Rig game(world.shared, "map mp fall back 8p", "FactionMen");
	const int humanIdx = game.index();
	Player *p2 = game.game->players().findPlayerWithName("Player_2");
	REQUIRE(p2 != nullptr);
	const int aiIdx = p2->getPlayerIndex();
	if (game.game->logic().skirmishAI().findAI(aiIdx) == nullptr)
	{
		if (game.game->logic().skirmishAI().store() == nullptr)
		{
			game.game->logic().skirmishAI().setStore(&world.shared.world->skirmishAI());
		}
		p2->setPlayerType(PLAYER_COMPUTER, true);
		game.game->logic().skirmishAI().newGame(true);
	}
	REQUIRE(game.game->logic().skirmishAI().findAI(aiIdx) != nullptr);

	std::string err;
	const Coord3D gatePos{ 1800.0f, 2300.0f, 0.0f };
	Object *gate = game.game->createObject("AmonSulGate", aiIdx, gatePos, 0.0f, &err);
	REQUIRE_MESSAGE(gate != nullptr, err);
	auto *aiGate = dynamic_cast<AIGateUpdate *>(gate->findModule("AIGateUpdate"));
	auto *gateBeh = GateOpenAndCloseBehavior::findGate(*gate);
	REQUIRE(aiGate != nullptr);
	REQUIRE(gateBeh != nullptr);

	// Step so AIGateUpdate creates its runtime trigger.
	game.game->stepFrame();
	const int trigIdx = aiGate->triggerIndex();
	REQUIRE(trigIdx >= 0); // tolerance: runtime trigger index follows map triggers

	// Enemy (humanIdx) near the AI player's gate: gate stays shut (RW 0x8B4D34 checks m_allies only).
	Object *enemy = game.game->createObject("MenPorter", humanIdx, Coord3D{ 1825.0f, 2300.0f, 0.0f }, 0.0f, &err);
	REQUIRE_MESSAGE(enemy != nullptr, err);
	{
		Coord3D p = *enemy->getPosition();
		p.x += 3.0f;
		enemy->setPosition(&p);
	}
	for (int i = 0; i < 40; ++i)
	{
		game.game->stepFrame();
	}
	CHECK(aiGate->alliesInside() == 0);
	CHECK(aiGate->enemiesInside() >= 1); // tolerance: map AI units may also enter the 450x225 box
	CHECK_FALSE(gateBeh->isOpen());

	// Friendly unit (aiIdx) inside the gate trigger box: gate opens for its own unit.
	Object *ally = game.game->createObject("MenPorter", aiIdx, Coord3D{ 1775.0f, 2300.0f, 0.0f }, 0.0f, &err);
	REQUIRE_MESSAGE(ally != nullptr, err);
	{
		Coord3D p = *ally->getPosition();
		p.x += 3.0f;
		ally->setPosition(&p);
	}
	game.game->stepFrame();
	CHECK(hasTriggerEntry(*ally, trigIdx));
	for (int i = 0; i < 40; ++i)
	{
		game.game->stepFrame();
	}
	CHECK(aiGate->alliesInside() >= 1); // tolerance: map AI units may also enter the 450x225 box
	CHECK(gateBeh->isOpen());

	// Destroy the gate: ~AIGateUpdate (RW 0x008B490F) removes the runtime trigger and clears ally's trigger entry.
	game.game->logic().destroyObject(gate);
	game.game->stepFrame();
	CHECK(game.game->logic().scriptEngine().triggerAreaAt((size_t)trigIdx).points.empty());
	CHECK_FALSE(hasTriggerEntry(*ally, trigIdx));

	// Real Ennorath horde & unit: guard, attack, and target selection via AIUpdateInterface.
	Object *horde = game.game->createObject("GondorFighterHorde", humanIdx, Coord3D{ 1600.0f, 2300.0f, 0.0f }, 0.0f, &err);
	REQUIRE_MESSAGE(horde != nullptr, err);
	Object *fighter = game.game->createObject("GondorFighter", humanIdx, Coord3D{ 1650.0f, 2300.0f, 0.0f }, 0.0f, &err);
	REQUIRE_MESSAGE(fighter != nullptr, err);
	Object *targetNear = game.game->createObject("GondorFighter", aiIdx, Coord3D{ 1700.0f, 2300.0f, 0.0f }, 0.0f, &err);
	Object *targetFar = game.game->createObject("GondorFighter", aiIdx, Coord3D{ 1760.0f, 2300.0f, 0.0f }, 0.0f, &err);
	REQUIRE_MESSAGE(targetNear != nullptr, err);
	REQUIRE_MESSAGE(targetFar != nullptr, err);
	game.game->stepFrame();

	AIUpdateInterface *fighterAI = fighter->getAIUpdateInterface();
	REQUIRE(fighterAI != nullptr);
	CHECK(fighterAI->aiGuardPosition(Coord3D{ 1650.0f, 2300.0f, 0.0f }, 0, CMD_FROM_SCRIPT));
	Object *picked = fighterAI->getNextMoodTarget(false, true);
	CHECK(picked != nullptr);

	AIUpdateInterface *hordeAI = horde->getAIUpdateInterface();
	REQUIRE(hordeAI != nullptr);
	CHECK(hordeAI->aiAttackObject(targetNear, CMD_FROM_PLAYER));
	game.game->stepFrame();
	CHECK(hordeAI->isAttacking());
}
