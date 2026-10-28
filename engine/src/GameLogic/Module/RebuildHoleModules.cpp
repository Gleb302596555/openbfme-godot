// OpenBFME. GPL-3.0.
// Derived from Command & Conquer Generals Zero Hour, (c) 2001-2003 Electronic Arts Inc., GPL-3.0.
// See GameLogic/Module/RebuildHoleModules.h.

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif

#include "GameLogic/Module/RebuildHoleModules.h"

#include "Common/INI.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/StateHash.h"
#include "Common/Team.h"
#include "Common/Thing/ModuleFactory.h"
#include "Common/Thing/ThingFactory.h"
#include "Common/Thing/ThingTemplate.h"
#include "GameLogic/AI/AIWorld.h"
#include "GameLogic/BitFlags.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/Construction.h"
#include "GameLogic/Damage.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/ConstructionModules.h"
#include "GameLogic/Module/DozerAIUpdate.h"
#include "GameLogic/Module/SpawnBehavior.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/ScriptEngine/ScriptEngine.h"
#include "GameLogic/SimMath.h"
#include "GameLogic/SnapshotXfer.h"

#include <cstddef>
#include <memory>
#include <stdexcept>

namespace
{
const FieldParse kExposeDieMux[] = {
	{ "DeathTypes", ParseDeathTypeFlags, nullptr, (int)offsetof(RebuildHoleExposeDieModuleData, m_dieMux.m_deathTypes) },
	{ "ExemptStatus", ParseObjectStatusMask, nullptr, (int)offsetof(RebuildHoleExposeDieModuleData, m_dieMux.m_exemptStatus) },
	{ "RequiredStatus", ParseObjectStatusMask, nullptr, (int)offsetof(RebuildHoleExposeDieModuleData, m_dieMux.m_requiredStatus) },
	{ "DamageAmountRequired", INI::parseReal, nullptr, (int)offsetof(RebuildHoleExposeDieModuleData, m_dieMux.m_damageAmountRequired) },
	{ "MinKillerAngle", INI::parseAngleReal, nullptr, (int)offsetof(RebuildHoleExposeDieModuleData, m_dieMux.m_minKillerAngle) },
	{ "MaxKillerAngle", INI::parseAngleReal, nullptr, (int)offsetof(RebuildHoleExposeDieModuleData, m_dieMux.m_maxKillerAngle) },
	{ nullptr, nullptr, nullptr, 0 }
};

// RW 0xC616B8
const FieldParse kExposeDie[] = {
	{ "HoleName", INI::parseAsciiString, nullptr, (int)offsetof(RebuildHoleExposeDieModuleData, m_holeName) },
	{ "HoleMaxHealth", INI::parseReal, nullptr, (int)offsetof(RebuildHoleExposeDieModuleData, m_holeMaxHealth) },
	{ "TransferAttackers", INI::parseBool, nullptr, (int)offsetof(RebuildHoleExposeDieModuleData, m_transferAttackers) },
	{ "FadeInTimeSeconds", INI::parseReal, nullptr, (int)offsetof(RebuildHoleExposeDieModuleData, m_fadeInTimeSeconds) },
	{ nullptr, nullptr, nullptr, 0 }
};

// RW 0xC60230
const FieldParse kRebuildHole[] = {
	{ "WorkerObjectName", INI::parseAsciiString, nullptr, (int)offsetof(RebuildHoleBehaviorModuleData, m_workerObjectName) },
	{ "WorkerRespawnDelay", INI::parseDurationReal, nullptr, (int)offsetof(RebuildHoleBehaviorModuleData, m_workerRespawnDelay) },
	{ "HoleHealthRegen%PerSecond", INI::parsePercentToReal, nullptr, (int)offsetof(RebuildHoleBehaviorModuleData, m_holeHealthRegen) },
	{ nullptr, nullptr, nullptr, 0 }
};

const int kUnselectable = 3;   // TheObjectStatusNames (RW 0xD8AFF0)[3]
const int kUnattackable = 60;  // [60]
const int kBuildCanceled = 86; // [86] BUILD_BEING_CANCELED

// every AI of the game moves its attack from `from` to `to` (RW 0x66826E per object with an AI, in the object list's order)
void transferAttacks(GameLogic &logic, ObjectID from, ObjectID to)
{
	for (Object *o = logic.getFirstObject(); o; o = o->getNextObject())
	{
		if (AIUpdateInterface *ai = o->getAIUpdateInterface())
		{
			ai->transferAttack(from, to);
		}
	}
}

GettingBuiltBehavior *gettingBuiltOf(Object &obj)
{
	return dynamic_cast<GettingBuiltBehavior *>(obj.findModule("GettingBuiltBehavior")); // RW 0x68C3E6
}

template <class Runtime, class Data>
void bind(ModuleFactory &modules, const char *name)
{
	modules.bindTypedData<Data>(name, MODULETYPE_BEHAVIOR);
	modules.bindModuleProc(name, MODULETYPE_BEHAVIOR, [name](Thing *thing, const ModuleData *data, const ModuleFactory::ModuleTemplate &) -> std::unique_ptr<Module> {
		const Data *typed = dynamic_cast<const Data *>(data);
		if (!typed)
		{
			throw std::logic_error(std::string(name) + ": the module data is not typed");
		}
		return std::make_unique<Runtime>(thing, typed);
	});
}
} // namespace

void RebuildHoleExposeDieModuleData::buildFieldParse(MultiIniFieldParse &p)
{
	p.add(kExposeDieMux); // RW 0xC76BD8
	p.add(kExposeDie);
}

void RebuildHoleBehaviorModuleData::buildFieldParse(MultiIniFieldParse &p)
{
	p.add(kRebuildHole);
}

// ---- RebuildHoleExposeDie ----------------------------------------------------------------------------------------------------------------------------

void RebuildHoleExposeDie::onDie(const DieModuleInterface::Event &event)
{
	Object *us = getObject();
	if (!m_data->m_dieMux.isDieApplicable(*us, event))
	{
		return;
	}
	GameLogic &logic = us->logic();
	Player *owner = us->getControllingPlayer();
	if (owner == logic.players().getNeutralPlayer())
	{
		return; // RW 0x889AE5
	}
	if (!owner || owner->isObserver() || owner->isDefeated() || us->isUnderConstruction())
	{
		return; // RW 0x889AF6: RW 0x6AAC52; RW 0x889B03 .. 0x889B0E: Object + 0x94 bit 2
	}
	const ThingTemplate *holeTemplate = logic.things().findTemplate(m_data->m_holeName); // RW 0x6D1305
	if (!holeTemplate)
	{
		logic.reportError("RebuildHoleExposeDie of " + us->getTemplate()->getName() + ": no HoleName template '" + m_data->m_holeName + "'");
		return;
	}
	Object *hole = logic.newObject(holeTemplate, us->getTeam(), ObjectStatusMaskType{}); // RW 0x6D165E
	if (!hole)
	{
		return;
	}
	hole->setPosition(us->getPosition());      // RW 0x70C201
	hole->setOrientation(us->getOrientation()); // RW 0x70C31E
	logic.scriptEngine().transferObjectName(us->getName(), *hole); // RW 0x759467
	if (AIWorld *ai = logic.aiWorld())
	{
		ai->removeObjectFromPathfindMap(*us); // RW 0x6E85FB
		ai->addObjectToPathfindMap(*hole);    // RW 0x6E85E9
	}
	if (BodyModuleInterface *body = hole->getBodyModule())
	{
		body->setMaxHealth(m_data->m_holeMaxHealth, 0); // body slot 0x5C
	}
	if (RebuildHoleBehavior *rh = RebuildHoleBehavior::of(*hole)) // RW 0x886448
	{
		if (SpawnBehaviorInterface *spawn = SpawnBehaviorInterface::of(*us)) // RW 0x68C3A3
		{
			rh->setSpawnerActive(spawn->isSpawnerActive()); // spawn slot 11 -> rebuild slot 4
		}
		rh->startRebuildProcess(us->getTemplate(), us->getID()); // rebuild slot 0
	}
	if (m_data->m_transferAttackers) // + 0x44
	{
		transferAttacks(logic, us->getID(), hole->getID());
	}
	if (m_data->m_fadeInTimeSeconds > 0.0f) // RW 0x889C34: comiss with 0.0 (RW 0xC1B594)
	{
		// RW 0x889C3D .. 0x889C55: _ftol2(fild 30 * seconds) client frames, the hole's drawable (client)
		if (ObjectClientHooks *client = logic.clientHooks())
		{
			client->fadeIn(*hole, (UnsignedInt)SimMath::ftol2Low32(SimMath::pc24MulW(30.0, (double)m_data->m_fadeInTimeSeconds)));
		}
	}
}

void RebuildHoleExposeDie::xfer(Xfer &x)
{
	SnapshotXfer::version(x, 1, 1); // RW 0x8899A4
	xferBehaviorModule(x);
}

// ---- RebuildHoleBehavior -----------------------------------------------------------------------------------------------------------------------------

RebuildHoleBehavior::RebuildHoleBehavior(Thing *thing, const RebuildHoleBehaviorModuleData *data)
	: UpdateModule(thing, data)
	, m_data(data)
{
}

RebuildHoleBehavior *RebuildHoleBehavior::of(Object &obj)
{
	return dynamic_cast<RebuildHoleBehavior *>(obj.findModule("RebuildHoleBehavior"));
}

void RebuildHoleBehavior::startRebuildProcess(const ThingTemplate *rebuild, ObjectID spawnerId)
{
	// RW 0x886831
	GameLogic &logic = getObject()->logic();
	if (const Object *spawner = logic.findObjectByID(spawnerId))
	{
		m_spawnerCostPaid = spawner->getBuildCostPaid(); // + 0x40 = Object + 0x33C
	}
	m_rebuildTemplate = rebuild; // + 0x3C
	m_spawnerId = spawnerId;     // + 0x30
	newWorkerRespawnProcess(logic.findObjectByID(m_workerId));
}

void RebuildHoleBehavior::newWorkerRespawnProcess(Object *existingWorker)
{
	// RW 0x8865F7
	Object *hole = getObject();
	GameLogic &logic = hole->logic();
	if (existingWorker)
	{
		logic.destroyObject(existingWorker); // RW 0x62BBAB
	}
	m_workerId = INVALID_ID;                                                 // + 0x28
	m_workerWaitCounter = (unsigned)SimMath::ftol2Low32((double)m_data->m_workerRespawnDelay); // + 0x34: fld, _ftol2
	hole->setStatus((unsigned)kUnselectable, false); // RW 0x68D440(mask(3, 60), 0)
	hole->setStatus((unsigned)kUnattackable, false);
	hole->setScriptSelectable(true); // RW 0x68BE3C(1)
	if (AIWorld *ai = logic.aiWorld())
	{
		ai->addObjectToPathfindMap(*hole); // RW 0x6E85E9
	}
}

UpdateSleepTime RebuildHoleBehavior::update()
{
	// RW 0x8868C2
	Object *hole = getObject();
	GameLogic &logic = hole->logic();
	Object *worker = nullptr;
	if (m_workerId != INVALID_ID)
	{
		worker = logic.findObjectByID(m_workerId); // RW 0x8868EE
		if (!worker)
		{
			newWorkerRespawnProcess(nullptr);
		}
	}
	Object *reconstructing = nullptr;
	if (m_reconstructingId != INVALID_ID)
	{
		reconstructing = logic.findObjectByID(m_reconstructingId); // RW 0x886911
		if (!reconstructing)
		{
			newWorkerRespawnProcess(worker);
			m_reconstructingId = INVALID_ID;
		}
	}
	if (!worker && m_workerWaitCounter > 0 && --m_workerWaitCounter == 0) // RW 0x88692A .. 0x886943
	{
		Player *owner = hole->getControllingPlayer();
		if (m_data->m_workerObjectName.empty()) // RW 0x88694F: RW 0x401E64
		{
			// RW 0x88695C .. 0x8869FC: the hole leaves the pathfind map, TheBuildAssistant builds the structure now (slot 0x38)
			if (AIWorld *ai = logic.aiWorld())
			{
				ai->removeObjectFromPathfindMap(*hole);
			}
			reconstructing = (owner && m_rebuildTemplate) ? Construction::buildObjectNow(logic, hole, *m_rebuildTemplate, *hole->getPosition(), hole->getOrientation(), *owner) : nullptr;
			if (!reconstructing)
			{
				return UPDATE_SLEEP_FOREVER; // RW 0x886AB9
			}
			if (GettingBuiltBehavior *gb = gettingBuiltOf(*reconstructing)) // RW 0x8869A3
			{
				if (BodyModuleInterface *body = reconstructing->getBodyModule())
				{
					body->setInitialHealthPercent(1.0f); // body slot 0x58 (1.0, 0)
				}
				gb->setForceComplete(true);  // slot 0x28 (10)
				gb->startConstruction(true); // slot 0x10 (4)
			}
			Construction::setModelConditions(*reconstructing, {}, { "PARTIALLY_CONSTRUCTED", "ACTIVELY_BEING_CONSTRUCTED" }); // RW 0x8869D6 .. 0x8869E7: 0x44 / 0x45
		}
		else
		{
			// RW 0x886A01 .. 0x886AE0: the worker
			if (!m_workerTemplate)
			{
				m_workerTemplate = logic.things().findTemplate(m_data->m_workerObjectName); // RW 0x6D1305
				if (!m_workerTemplate)
				{
					// retail makes no worker and the hole waits for ever; the port reports the missing data (lane BFME2CAMP1 review: no silent defaults)
					logic.reportError("RebuildHoleBehavior of " + hole->getTemplate()->getName() + ": no WorkerObjectName template '" + m_data->m_workerObjectName + "'");
				}
			}
			worker = m_workerTemplate ? logic.newObject(m_workerTemplate, hole->getTeam(), ObjectStatusMaskType{}) : nullptr; // RW 0x6D165E
			if (worker)
			{
				worker->setPosition(hole->getPosition()); // RW 0x70C201
				m_workerId = worker->getID();
				if (worker->getAIUpdateInterface())
				{
					if (!reconstructing)
					{
						if (AIWorld *ai = logic.aiWorld())
						{
							ai->removeObjectFromPathfindMap(*hole); // RW 0x886A80
						}
						// AI slot 0x1F8 (construct(template, pos, angle, player, 1)): the dozer's (RW 0x88C217, a rebuild: no tests, nothing paid); another AI's
						// slot answers null
						DozerAIUpdate *dozer = dynamic_cast<DozerAIUpdate *>(worker->getAIUpdateInterface());
						reconstructing = (owner && m_rebuildTemplate && dozer) ? dozer->construct(*m_rebuildTemplate, *hole->getPosition(), hole->getOrientation(), *owner, true) : nullptr;
						if (!reconstructing)
						{
							return UPDATE_SLEEP_FOREVER; // RW 0x886AB9
						}
					}
					else if (DozerAIUpdate *dozer = dynamic_cast<DozerAIUpdate *>(worker->getAIUpdateInterface()))
					{
						dozer->resumeConstruction(*reconstructing); // RW 0x771526(structure, CMD_FROM_AI)
					}
				}
				worker->setStatus((unsigned)kUnselectable, true); // RW 0x886ACE .. 0x886AE0
			}
		}
		if (reconstructing) // RW 0x886AE5
		{
			transferAttacks(logic, hole->getID(), reconstructing->getID()); // RW 0x886AEF .. 0x886B1C
			m_reconstructingId = reconstructing->getID();                    // + 0x2C
			reconstructing->setProducer(hole);                               // RW 0x68B6A1
			reconstructing->setBuildCostPaid(m_spawnerCostPaid);             // Object + 0x33C = + 0x40
			if (!m_spawnerActive)
			{
				if (SpawnBehaviorInterface *spawn = SpawnBehaviorInterface::of(*reconstructing))
				{
					spawn->stopSpawning(); // spawn slot 9
				}
			}
			// RW 0x886B4D .. 0x886B72: the hole is masked (Object + 0x459 bit 1: S-1427), UNSELECTABLE and UNATTACKABLE, not selectable
			hole->setStatus((unsigned)kUnselectable, true);
			hole->setStatus((unsigned)kUnattackable, true);
			hole->setScriptSelectable(false);
		}
	}
	// RW 0x886B77 .. 0x886BD5: the hole heals while below its max health
	if (BodyModuleInterface *body = hole->getBodyModule())
	{
		const float health = body->getHealth();
		const float maxHealth = body->getMaxHealth();
		if (maxHealth > health)
		{
			DamageInfo heal;
			heal.m_input.m_sourceID = hole->getID();
			heal.m_input.m_damageType = DAMAGE_HEALING; // 7
			heal.m_input.m_deathType = DEATH_NONE;      // 1
			// x87: fild 5 (LOGICFRAMES_PER_SECOND), fdivr regen, fmulp max, fstp
			heal.m_input.m_amount = SimMath::fstpDword(SimMath::pc24MulW(SimMath::pc24DivW((double)m_data->m_holeHealthRegen, 5.0), (double)maxHealth));
			body->attemptHealing(heal); // body slot 4
		}
	}
	// RW 0x886BD8 .. 0x886C40: the rebuilt structure stands
	if (reconstructing)
	{
		const bool canceled = reconstructing->testStatus((unsigned)kBuildCanceled);
		if (!canceled && reconstructing->isUnderConstruction())
		{
			return UPDATE_SLEEP_NONE;
		}
		if (!canceled && gettingBuiltOf(*reconstructing) && reconstructing->isEffectivelyDead())
		{
			return UPDATE_SLEEP_NONE;
		}
		logic.scriptEngine().transferObjectName(hole->getName(), *reconstructing); // RW 0x886C21
		if (worker)
		{
			logic.destroyObject(worker); // RW 0x886C35
		}
		hole->kill(DAMAGE_UNRESISTABLE, 0x16); // RW 0x698EC3(8, 0x16)
	}
	return UPDATE_SLEEP_NONE; // RW 0x886C45: 1
}

void RebuildHoleBehavior::crc(StateHasher &h) const
{
	h.addU32(m_workerId);
	h.addU32(m_reconstructingId);
	h.addU32(m_spawnerId);
	h.addU32(m_workerWaitCounter);
	h.addString(m_workerTemplate ? m_workerTemplate->getName() : std::string());
	h.addString(m_rebuildTemplate ? m_rebuildTemplate->getName() : std::string());
	h.addFloat(m_spawnerCostPaid);
	h.addBool(m_spawnerActive);
}

void RebuildHoleBehavior::xfer(Xfer &x)
{
	// RW 0x886669 (version 3): the update module, the ids, the counter, the templates by name, the flag, then the cost
	SnapshotXfer::version(x, 1, 1);
	xferUpdateModule(x);
	x.xferObjectID(&m_workerId);
	x.xferObjectID(&m_reconstructingId);
	x.xferObjectID(&m_spawnerId);
	x.xferUnsignedInt(&m_workerWaitCounter);
	const ThingFactory &things = getObject()->logic().things();
	for (const ThingTemplate **t : { &m_workerTemplate, &m_rebuildTemplate })
	{
		std::string name = *t ? (*t)->getName() : std::string();
		x.xferAsciiString(&name);
		if (x.isLoading())
		{
			*t = name.empty() ? nullptr : things.findTemplate(name);
			if (!name.empty() && !*t)
			{
				throw XferException("RebuildHoleBehavior: no template '" + name + "'");
			}
		}
	}
	x.xferBool(&m_spawnerActive);
	x.xferReal(&m_spawnerCostPaid);
}

void RebuildHoleModules::registerAll(ModuleFactory &modules)
{
	bind<RebuildHoleExposeDie, RebuildHoleExposeDieModuleData>(modules, "RebuildHoleExposeDie");
	bind<RebuildHoleBehavior, RebuildHoleBehaviorModuleData>(modules, "RebuildHoleBehavior");
}
