// OpenBFME. GPL-3.0.
// See GameLogic/Module/RunOffMapBehavior.h (lane FACT-B; editions: both RotWK 2.01 and BFME2 1.06).

#include "GameLogic/Module/RunOffMapBehavior.h"

#include "Common/INI.h"
#include "Common/Thing/ModuleFactory.h"
#include "Common/Xfer.h"
#include "GameLogic/Damage.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/SnapshotXfer.h"

#include <memory>
#include <stdexcept>

namespace
{
#define ROMB_OFF(m) (int)offsetof(RunOffMapBehaviorModuleData, m)
// RW 0xC59698 / BFME2 0xC43028
const FieldParse kRunOffMapBehaviorFieldParse[] = {
	{ "RequiresSpecificTrigger", INI::parseBool, nullptr, ROMB_OFF(m_requiresSpecificTrigger) },
	{ "RunOffMapWaypointName", INI::parseAsciiString, nullptr, ROMB_OFF(m_waypointName) },
	{ "DieOnMap", INI::parseBool, nullptr, ROMB_OFF(m_dieOnMap) },
	{ nullptr, nullptr, nullptr, 0 }
};
#undef ROMB_OFF
} // namespace

void RunOffMapBehaviorModuleData::buildFieldParse(MultiIniFieldParse &p)
{
	p.add(kRunOffMapBehaviorFieldParse);
}

// RW 0x86525F / BFME2 0x46217B
RunOffMapBehavior::RunOffMapBehavior(Thing *thing, const RunOffMapBehaviorModuleData *data)
	: BehaviorModule(thing, data)
	, m_data(data)
{
	// RW 0x865321 / BFME2 0x462213: slot 5 runs on creation if not RequiresSpecificTrigger
	if (m_data && !m_data->m_requiresSpecificTrigger)
	{
		triggerFlee();
	}
}

// RW 0x86540A / BFME2 0x4622FC
void RunOffMapBehavior::triggerFlee()
{
	if (m_triggered || !getObject() || !m_data)
	{
		return;
	}
	m_triggered = true;

	Object *obj = getObject();
	obj->setStatus(0x32, true);                  // STATUS_FLEEING (0x32)
	obj->setModelConditionState(0xFC, true);      // RUNNING_OFF_MAP (0xFC)

	if (m_data->m_dieOnMap)
	{
		obj->kill(DAMAGE_UNRESISTABLE, 0);
	}
	else
	{
		obj->logic().destroyObject(obj);
	}
}

void RunOffMapBehavior::xfer(Xfer &x)
{
	SnapshotXfer::version(x, 1, 1);
	xferBehaviorModule(x);
	x.xferBool(&m_triggered);
}

void RunOffMapBehavior::registerClass(ModuleFactory &modules)
{
	modules.bindTypedData<RunOffMapBehaviorModuleData>("RunOffMapBehavior", MODULETYPE_BEHAVIOR);
	modules.bindModuleProc("RunOffMapBehavior", MODULETYPE_BEHAVIOR,
		[](Thing *thing, const ModuleData *data, const ModuleFactory::ModuleTemplate &) -> std::unique_ptr<Module> {
			const auto *typed = dynamic_cast<const RunOffMapBehaviorModuleData *>(data);
			if (!typed)
			{
				throw std::logic_error("RunOffMapBehavior: module data is not typed");
			}
			return std::make_unique<RunOffMapBehavior>(thing, typed);
		});
}
