// OpenBFME. GPL-3.0. See SpawnUnitBehavior.h.

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif

#include "GameLogic/Module/SpawnUnitBehavior.h"

#include "Common/Player.h"
#include "Common/Thing/ModuleFactory.h"
#include "Common/Thing/ThingFactory.h"
#include "Common/Thing/ThingTemplate.h"
#include "Common/Xfer.h"
#include "GameClient/ControlBarCommands.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/ProductionUpdate.h"
#include "GameLogic/Object/Object.h"

#include <cstddef>
#include <memory>
#include <stdexcept>

void SpawnUnitBehaviorModuleData::buildFieldParse(MultiIniFieldParse &p)
{
	// RW 0x8BC981 -> 0xC6FEF0; defaults RW 0x65573A.
	static const FieldParse fields[] = {
		{ "UnitName", INI::parseAsciiString, nullptr, (int)offsetof(SpawnUnitBehaviorModuleData, m_unitName) },
		{ "UnitCommand", INI::parseAsciiString, nullptr, (int)offsetof(SpawnUnitBehaviorModuleData, m_unitCommand) },
		{ "SpawnOnce", INI::parseBool, nullptr, (int)offsetof(SpawnUnitBehaviorModuleData, m_spawnOnce) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

SpawnUnitBehavior::SpawnUnitBehavior(Thing *thing, const SpawnUnitBehaviorModuleData *data)
	: UpdateModule(thing, data), m_data(data)
{
	m_spawned = data ? data->m_spawnOnce : false;
}

UpdateSleepTime SpawnUnitBehavior::update()
{
	// RW 0x8BC9B0 / BFME2 0x89ECC0
	Object *obj = getObject();
	if (!obj || !m_data)
	{
		return UPDATE_SLEEP_FOREVER;
	}

	Player *player = obj->getControllingPlayer();
	if (!player || obj->isDestroyed() || obj->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION))
	{
		return UPDATE_SLEEP_NONE;
	}

	ProductionUpdateInterface *prod = obj->getProductionUpdate();
	if (prod && prod->getProductionCount() > 0)
	{
		if (prod->getProductionCount() > 1)
		{
			const ProductionEntry *first = prod->firstProduction();
			if (first && first->objectToProduce && first->objectToProduce->getName() == m_data->m_unitName)
			{
				prod->cancelUnitCreate(first->productionID);
			}
		}
		return UPDATE_SLEEP_NONE;
	}

	if (m_data->m_unitName.empty())
	{
		return UPDATE_SLEEP_NONE;
	}

	GameLogic &logic = obj->logic();
	const ThingTemplate *unitTemplate = logic.things().findTemplate(m_data->m_unitName);
	if (!unitTemplate)
	{
		return UPDATE_SLEEP_NONE;
	}

	if (!player->canAffordCommandPoints(*unitTemplate))
	{
		return UPDATE_SLEEP_NONE;
	}

	if (m_data->m_spawnOnce && !m_spawned)
	{
		return UPDATE_SLEEP_NONE;
	}

	if (!m_data->m_unitCommand.empty() && TheCommandStore)
	{
		const CommandButton *btn = TheCommandStore->findCommandButton(m_data->m_unitCommand);
		if (btn)
		{
			obj->doCommandButton(btn, CMD_FROM_AI, false);
		}
	}

	m_spawned = false;
	return UPDATE_SLEEP_NONE;
}

void SpawnUnitBehavior::xfer(Xfer &x)
{
	XferVersion v{ 1, 2 }; // RW 0x4B87A3
	x.xferVersion(v);
	xferUpdateModule(x);
	x.xferBool(&m_spawned);
}

void SpawnUnitBehavior::crc(StateHasher &hasher) const
{
	UpdateModule::crc(hasher);
	hasher.addBool(m_spawned);
}

void SpawnUnitBehavior::registerClass(ModuleFactory &modules)
{
	modules.bindTypedData<SpawnUnitBehaviorModuleData>("SpawnUnitBehavior", MODULETYPE_BEHAVIOR);
	modules.bindModuleProc("SpawnUnitBehavior", MODULETYPE_BEHAVIOR, [](Thing *thing, const ModuleData *data, const ModuleFactory::ModuleTemplate &) -> std::unique_ptr<Module> {
		const auto *typed = dynamic_cast<const SpawnUnitBehaviorModuleData *>(data);
		return std::make_unique<SpawnUnitBehavior>(thing, typed);
	});
}
