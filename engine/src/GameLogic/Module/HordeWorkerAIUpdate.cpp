// OpenBFME. GPL-3.0.
// See GameLogic/Module/HordeWorkerAIUpdate.h (lane MODGAP-2; editions: both RotWK 2.01 and BFME2 1.06).

#include "GameLogic/Module/HordeWorkerAIUpdate.h"

#include "Common/StateHash.h"
#include "Common/Thing/ModuleFactory.h"
#include "Common/Xfer.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/BehaviorModule.h"
#include "GameLogic/Module/DozerAIUpdate.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Module/UnportedModule.h"
#include "GameLogic/Object/Contain/HordeContainRuntime.h"
#include "GameLogic/Object/Object.h"

// RW 0x64EE0E / BFME2 0x49AD15
HordeWorkerAIUpdate::HordeWorkerAIUpdate(Thing *thing, const ModuleData *data)
	: HordeAIUpdate(thing, data)
{
}

// RW 0x89EC9A / BFME2 0x49AEA4
UpdateSleepTime HordeWorkerAIUpdate::update()
{
	Object *obj = getObject();
	if (!obj)
	{
		return UPDATE_SLEEP_FOREVER;
	}

	GameLogic &logic = obj->logic();

	// Clean up stale IDs
	if (m_pendingRepairTarget != 0 && !logic.findObjectByID(m_pendingRepairTarget))
	{
		m_pendingRepairTarget = 0;
	}
	if (m_activeRepairTarget != 0 && !logic.findObjectByID(m_activeRepairTarget))
	{
		m_activeRepairTarget = 0;
	}
	if (m_command42Target != 0 && !logic.findObjectByID(m_command42Target))
	{
		m_command42Target = 0;
	}

	if (m_pendingRepairTarget != 0)
	{
		Object *target = logic.findObjectByID(m_pendingRepairTarget);
		if (!target || target->isEffectivelyDead())
		{
			aiIdle(CMD_FROM_AI);
			m_pendingRepairTarget = 0;
			return UPDATE_SLEEP_NONE;
		}

		if (isIdle())
		{
			privateRepair(target, CMD_FROM_AI);
			m_pendingRepairTarget = 0;
			return UPDATE_SLEEP_NONE;
		}

		return HordeAIUpdate::update();
	}

	if (m_activeRepairTarget != 0)
	{
		Object *target = logic.findObjectByID(m_activeRepairTarget);
		BodyModuleInterface *body = target ? target->getBodyModule() : nullptr;
		if (!target || target->isEffectivelyDead() || (body && !(body->getHealth() < body->getMaxHealth())))
		{
			if (ContainModuleInterface *c = obj->getContain())
			{
				if (HordeContainInterface *hc = c->getHordeContainInterface())
				{
					hc->reserveMemberGoals();
				}
			}
			m_activeRepairTarget = 0;
		}
		return UPDATE_SLEEP_NONE;
	}

	if (m_command42Target != 0)
	{
		Object *target = logic.findObjectByID(m_command42Target);
		if (target)
		{
			privateRepair(target, CMD_FROM_AI);
		}
		m_command42Target = 0;
		return UPDATE_SLEEP_NONE;
	}

	return HordeAIUpdate::update();
}

// RW 0x89ECA0 / BFME2 0x49AFFC
void HordeWorkerAIUpdate::privateRepair(Object *obj, CommandSourceType cmdSource)
{
	(void)cmdSource;
	Object *me = getObject();
	if (!me || !obj)
	{
		return;
	}

	m_activeRepairTarget = obj->getID();

	if (ContainModuleInterface *c = me->getContain())
	{
		if (const ContainModuleInterface::ContainedItemsList *l = c->getContainedItemsList())
		{
			for (Object *member : *l)
			{
				if (!member || member->isEffectivelyDead())
				{
					continue;
				}
				if (DozerAIUpdate *d = dynamic_cast<DozerAIUpdate *>(member->getAIUpdateInterface()))
				{
					d->repair(*obj);
				}
				else if (AIUpdateInterface *ai = member->getAIUpdateInterface())
				{
					ai->aiMoveToObject(obj, CMD_FROM_AI);
				}
			}
		}
	}
}

// RW 0x89ED20 / BFME2 0x49B1CB
void HordeWorkerAIUpdate::bfmePrivateCommand42(Object *obj, CommandSourceType cmdSource)
{
	BodyModuleInterface *body = obj ? obj->getBodyModule() : nullptr;
	if (obj && body && body->getHealth() < body->getMaxHealth())
	{
		m_command42Target = obj->getID();
		m_pendingRepairTarget = obj->getID();
		m_flag = true;
		privateRepair(obj, cmdSource);
	}
}

void HordeWorkerAIUpdate::commandAccepted(CommandSourceType source, int command, Object *target)
{
	if (command == 0x13 || command == 0x42)
	{
		if (target)
		{
			m_pendingRepairTarget = target->getID();
			m_activeRepairTarget = target->getID();
			privateRepair(target, source);
			return;
		}
	}
	HordeAIUpdate::commandAccepted(source, command, target);
}

// RW 0x89EB64 / BFME2 0x49AE07
void HordeWorkerAIUpdate::xfer(Xfer &x)
{
	XferVersion v{ 1, 1 };
	x.xferVersion(v);
	HordeAIUpdate::xfer(x);
	x.xferObjectID(&m_pendingRepairTarget);
	x.xferObjectID(&m_activeRepairTarget);
	x.xferObjectID(&m_command42Target);
	x.xferBool(&m_flag);
}

void HordeWorkerAIUpdate::crc(StateHasher &h) const
{
	HordeAIUpdate::crc(h);
	h.addU32(m_pendingRepairTarget);
	h.addU32(m_activeRepairTarget);
	h.addU32(m_command42Target);
	h.addBool(m_flag);
}

void HordeWorkerAIUpdate::registerClass(ModuleFactory &modules)
{
	modules.bindModuleProc("HordeWorkerAIUpdate", MODULETYPE_BEHAVIOR, [](Thing *thing, const ModuleData *data, const ModuleFactory::ModuleTemplate &info) -> std::unique_ptr<Module> {
		if (!thing->asObject() || !thing->asObject()->logic().aiWorld())
		{
			return makeUnportedModule(thing, data, info.name, info.type, info.interfaceMask);
		}
		return std::make_unique<HordeWorkerAIUpdate>(thing, data);
	});
}
