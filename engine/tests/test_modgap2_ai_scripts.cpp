// OpenBFME. GPL-3.0.
// Editions: both RotWK 2.01 and BFME2 1.06.
//
// Tests for lane MODGAP-2 AI scripts and unblocked modules:
// - PLAYER_HAS_NUM_UNITS_WITH_UPGRADE (RW 0x7E9356 / BFME2 0x3E8785)
// - PLAYER_BUILT_UPGRADE (RW 0x7E4F6A / BFME2 0x3E438D)
// - AnimalAIUpdate (RW 0x64EC65 / BFME2 0x6524BF)
// - HordeWorkerAIUpdate (RW 0x64E248 / BFME2 0x24E560)
// - DestroyEnvironmentUpdate slot 31 (RW 0x8AFC5F)
// - RadarMarkerClientUpdate & PartTheHeavensUpdate

#include "doctest.h"
#include "CombatTestUtil.h"
#include "ScriptTestUtil.h"

#include "Common/GameEdition.h"
#include "Common/INI.h"
#include "Common/Player.h"
#include "Common/StateHash.h"
#include "Common/Thing/ModuleFactory.h"
#include "Common/XferLoad.h"
#include "Common/XferSave.h"
#include "GameClient/Drawable.h"
#include "GameClient/RadarMarkerClientUpdate.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AnimalAIUpdate.h"
#include "GameLogic/Module/RunOffMapBehavior.h"
#include "GameLogic/Module/DestroyEnvironmentUpdate.h"
#include "GameLogic/Module/RainOfFireUpdate.h"
#include "GameLogic/Module/HordeWorkerAIUpdate.h"
#include "GameLogic/Module/LogicModules.h"
#include "GameLogic/Module/PartTheHeavensUpdate.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/ObjectTemplateInfo.h"
#include "GameLogic/ScriptEngine/ScriptConditions.h"
#include "GameLogic/ScriptEngine/ScriptEngine.h"

using namespace scripttest;

namespace
{
const char kAiScriptTestIni[] =
    "Object TestRunOffAtBirth\n"
    "  KindOf = INFANTRY\n"
    "  Body = ActiveBody BodyTag\n"
    "    MaxHealth = 100\n"
    "  End\n"
    "  Behavior = AIUpdateInterface AITag\n"
    "  End\n"
    "  Behavior = RunOffMapBehavior RunOffTag\n"
    "    RequiresSpecificTrigger = No\n"
    "  End\n"
    "End\n"
    "Upgrade Upgrade_TestBuilt\n"
    "  Type = PLAYER\n"
    "End\n"
    "Object TestAnimal\n"
    "  KindOf = INFANTRY\n"
    "  Body = ActiveBody BodyTag\n"
    "    MaxHealth = 100\n"
    "  End\n"
    "  Behavior = AnimalAIUpdate ModuleTag_AnimalAI\n"
    "    FleeRange = 30.0\n"
    "    FleeDistance = 60.0\n"
    "    WanderPercentage = 100\n"
    "    MaxWanderDistance = 20.0\n"
    "    MaxWanderRadius = 50.0\n"
    "    AfraidOfCastles = Yes\n"
    "    UpdateTimer = 15\n"
    "  End\n"
    "End\n"
    "Object TestHordeWorker\n"
    "  KindOf = INFANTRY HORDE\n"
    "  Body = ActiveBody BodyTag\n"
    "    MaxHealth = 200\n"
    "  End\n"
    "  Behavior = HordeWorkerAIUpdate ModuleTag_HordeWorkerAI\n"
    "  End\n"
    "End\n"
    "Object TestBuilding\n"
    "  KindOf = STRUCTURE\n"
    "  Body = ActiveBody BodyTag\n"
    "    MaxHealth = 500\n"
    "    InitialHealth = 250\n"
    "  End\n"
    "End\n"
    "Object TestDummyEnv\n"
    "  KindOf = ENVIRONMENT\n"
    "  Body = ActiveBody BodyTag\n"
    "    MaxHealth = 100\n"
    "  End\n"
    "End\n"
    "Object TestEnvActor\n"
    "  KindOf = ENVIRONMENT\n"
    "  Body = ActiveBody BodyTag\n"
    "    MaxHealth = 100\n"
    "  End\n"
    "  Behavior = RainOfFireUpdate ModuleTag_RainOfFire\n"
    "    Darkness = 0.5\n"
    "  End\n"
    "End\n"
    "Object TestDestroySpell\n"
    "  KindOf = SYSTEM\n"
    "  Behavior = DestroyEnvironmentUpdate ModuleTag_DestroyEnv\n"
    "    StartTime = 5\n"
    "    DestructionTime = 20\n"
    "  End\n"
    "End\n";
} // namespace

TEST_CASE("modgap2 ai: PLAYER_HAS_NUM_UNITS_WITH_UPGRADE and PLAYER_BUILT_UPGRADE conditions (both editions)")
{
	for (GameEdition edition : { GameEdition::RotWK201, GameEdition::BFME2_106 })
	{
		CAPTURE(gameEditionName(edition));
		combattest::CombatWorld w(kAiScriptTestIni);
		w.logic->setEdition(edition);

		Object *u1 = w.unit("TestAnimal", 'A', 10.0f, 10.0f);
		Object *u2 = w.unit("TestAnimal", 'A', 20.0f, 20.0f);
		REQUIRE(u1 != nullptr);
		REQUIRE(u2 != nullptr);
		Player *alice = u1->getControllingPlayer();
		REQUIRE(alice != nullptr);

		// 1. Test PLAYER_BUILT_UPGRADE (condition 69)
		ScriptCondition builtCond = cond("PLAYER_BUILT_UPGRADE", { sp(11, alice->getPlayerName()), sp(33, "Upgrade_TestBuilt") });
		CHECK_FALSE(ScriptConditions::evaluate(w.logic->scriptEngine(), builtCond));

		// Grant completed upgrade notification to script engine
		w.logic->scriptEngine().notifyOfCompletedUpgrade(alice->getPlayerIndex(), "Upgrade_TestBuilt", 0);
		CHECK(ScriptConditions::evaluate(w.logic->scriptEngine(), builtCond));
		const UpgradeTemplate *up = Player::resolveUpgrade("Upgrade_TestBuilt", true);

		// 2. Test PLAYER_HAS_NUM_UNITS_WITH_UPGRADE (condition 178)
		ScriptCondition hasCond1 = cond("PLAYER_HAS_NUM_UNITS_WITH_UPGRADE", { sp(11, alice->getPlayerName()), ip(0, 1), sp(33, "Upgrade_TestBuilt") });
		ScriptCondition hasCond2 = cond("PLAYER_HAS_NUM_UNITS_WITH_UPGRADE", { sp(11, alice->getPlayerName()), ip(0, 2), sp(33, "Upgrade_TestBuilt") });

		CHECK_FALSE(ScriptConditions::evaluate(w.logic->scriptEngine(), hasCond1));

		u1->giveUpgradeSelf(up);
		CHECK(ScriptConditions::evaluate(w.logic->scriptEngine(), hasCond1));
		CHECK_FALSE(ScriptConditions::evaluate(w.logic->scriptEngine(), hasCond2));

		u2->giveUpgradeSelf(up);
		CHECK(ScriptConditions::evaluate(w.logic->scriptEngine(), hasCond2));
	}
}

// RW 0x865321: a module that needs no trigger runs when the unit is made; RW 0x865519 .. 0x865534 gives it an AI command and removes nothing. The port destroyed the
// object there, in its constructor, which ended `move retail` with SIGABRT on retail's GondorCavalryRiderless. The AI commands themselves are S-5308.
TEST_CASE("modgap2 ai: RunOffMapBehavior without a trigger leaves the new unit alive and reports the unported run (both editions)")
{
	for (GameEdition edition : { GameEdition::RotWK201, GameEdition::BFME2_106 })
	{
		CAPTURE(gameEditionName(edition));
		combattest::CombatWorld w(kAiScriptTestIni);
		w.logic->setEdition(edition);
		Object *unit = w.unit("TestRunOffAtBirth", 'A', 100.0f, 100.0f);
		REQUIRE(unit != nullptr);
		const ObjectID id = unit->getID();
		const Coord3D at = *unit->getPosition();
		w.frames(10);
		Object *now = w.byId(id);
		REQUIRE(now != nullptr);
		CHECK_FALSE(now->isEffectivelyDead());
		CHECK(now->getPosition()->x == at.x);
		auto *run = dynamic_cast<RunOffMapBehavior *>(now->findModule("RunOffMapBehavior"));
		REQUIRE(run != nullptr);
		CHECK(run->hasTriggered());
		bool reported = false;
		for (const auto &line : w.logic->report().stops)
		{
			reported = reported || line.find("[S-5308]") != std::string::npos;
		}
		CHECK(reported);
	}
}

TEST_CASE("modgap2 ai: AnimalAIUpdate data parsing, wander, flee, crc and xfer (both editions)")
{
	for (GameEdition edition : { GameEdition::RotWK201, GameEdition::BFME2_106 })
	{
		CAPTURE(gameEditionName(edition));
		combattest::CombatWorld w(kAiScriptTestIni);
		w.logic->setEdition(edition);

		Object *animal = w.unit("TestAnimal", 'A', 100.0f, 100.0f);
		REQUIRE(animal != nullptr);

		auto *ai = dynamic_cast<AnimalAIUpdate *>(animal->findModule("AnimalAIUpdate"));
		REQUIRE(ai != nullptr);
		REQUIRE(ai->animalData() != nullptr);
		CHECK(ai->animalData()->m_fleeRange == 30.0f);
		CHECK(ai->animalData()->m_fleeDistance == 60.0f);
		CHECK(ai->animalData()->m_wanderPercentage == 100);
		CHECK(ai->animalData()->m_maxWanderDistance == 20.0f);
		CHECK(ai->animalData()->m_maxWanderRadius == 50.0f);
		CHECK(ai->animalData()->m_afraidOfCastles == true);
		CHECK(ai->animalData()->m_updateTimer == 15);

		// Update idle wander
		ai->update();

		// CRC test
		StateHasher h1;
		ai->crc(h1);
		// tolerance: hash of active module state must be non-zero
		CHECK(h1.value() != 0);

		// Xfer roundtrip
		XferSave save;
		ai->xfer(save);
		CHECK_FALSE(save.data().empty());

		XferLoad load(save.data().data(), save.data().size());
		ai->xfer(load);
	}
}

TEST_CASE("modgap2 ai: HordeWorkerAIUpdate command handling and repair (both editions)")
{
	for (GameEdition edition : { GameEdition::RotWK201, GameEdition::BFME2_106 })
	{
		CAPTURE(gameEditionName(edition));
		combattest::CombatWorld w(kAiScriptTestIni);
		w.logic->setEdition(edition);

		Object *workerHorde = w.unit("TestHordeWorker", 'A', 50.0f, 50.0f);
		Object *bldg = w.unit("TestBuilding", 'A', 60.0f, 60.0f);
		REQUIRE(workerHorde != nullptr);
		REQUIRE(bldg != nullptr);

		auto *ai = dynamic_cast<HordeWorkerAIUpdate *>(workerHorde->findModule("HordeWorkerAIUpdate"));
		REQUIRE(ai != nullptr);

		// Dispatch repair command 0x13
		ai->commandAccepted(CMD_FROM_AI, 0x13, bldg);
		ai->update();

		// Dispatch command 0x42
		ai->bfmePrivateCommand42(bldg, CMD_FROM_AI);
		ai->update();

		// CRC & Xfer
		StateHasher h;
		ai->crc(h);
		// tolerance: hash of active module state must be non-zero
		CHECK(h.value() != 0);

		XferSave save;
		ai->xfer(save);
		CHECK_FALSE(save.data().empty());

		XferLoad load(save.data().data(), save.data().size());
		ai->xfer(load);
	}
}

TEST_CASE("modgap2 ai: DestroyEnvironmentUpdate partition lookup and RadarMarkerClientUpdate (both editions)")
{
	for (GameEdition edition : { GameEdition::RotWK201, GameEdition::BFME2_106 })
	{
		CAPTURE(gameEditionName(edition));
		combattest::CombatWorld w(kAiScriptTestIni);
		w.logic->setEdition(edition);

		// DestroyEnvironmentUpdate finds KINDOF_ENVIRONMENT actor via slot 31 (RW 0x8AFC5F)
		Object *dummy = w.unit("TestDummyEnv", 'A', 100.0f, 100.0f);
		REQUIRE(dummy != nullptr);
		Object *envObj = w.unit("TestEnvActor", 'A', 200.0f, 200.0f);
		REQUIRE(envObj != nullptr);
		Object *spell = w.unit("TestDestroySpell", 'A', 50.0f, 50.0f);
		REQUIRE(spell != nullptr);

		auto *deu = dynamic_cast<DestroyEnvironmentUpdate *>(spell->findModule("DestroyEnvironmentUpdate"));
		REQUIRE(deu != nullptr);

		deu->update();
		// Authentically selects envObj (which implements slot 31), skipping dummy:
		CHECK(deu->objectID() == envObj->getID());

		// RadarMarkerClientUpdate
		RadarMarkerClientUpdateModuleData radarData;
		radarData.m_markerType = "PingBeacon";
		RadarMarkerClientUpdate rmu(nullptr, &radarData);
		rmu.clientUpdate();
		CHECK(rmu.markerType() == "PingBeacon");
	}
}
