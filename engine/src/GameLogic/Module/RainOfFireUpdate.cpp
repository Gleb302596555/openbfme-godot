// OpenBFME. GPL-3.0.
// See GameLogic/Module/RainOfFireUpdate.h (lane MODGAP-2; editions: both RotWK 2.01 and BFME2 1.06).

#include "GameLogic/Module/RainOfFireUpdate.h"

#include "Common/INI.h"
#include "Common/Thing/ModuleFactory.h"
#include "Common/Xfer.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/SnapshotXfer.h"

#include <cstddef>
#include <memory>
#include <stdexcept>

namespace
{
#define ROFU_OFF(m) (int)offsetof(RainOfFireUpdateModuleData, m)
// RW 0xC6B6F0 / BFME2 0xC54BA8
const FieldParse kRainOfFireUpdateFieldParse[] = {
	{ "StartRainTime", INI::parseDurationUnsignedInt, nullptr, ROFU_OFF(m_startRainTime) },
	{ "DarknessFadeTime", INI::parseDurationUnsignedInt, nullptr, ROFU_OFF(m_darknessFadeTime) },
	{ "Darkness", INI::parseReal, nullptr, ROFU_OFF(m_darkness) },
	{ "Height", INI::parseReal, nullptr, ROFU_OFF(m_height) },
	{ nullptr, nullptr, nullptr, 0 }
};
#undef ROFU_OFF
} // namespace

void RainOfFireUpdateModuleData::buildFieldParse(MultiIniFieldParse &p)
{
	p.add(kRainOfFireUpdateFieldParse);
}

RainOfFireUpdate::RainOfFireUpdate(Thing *thing, const RainOfFireUpdateModuleData *data)
	: UpdateModule(thing, data)
	, m_data(data)
{
	if (getObject())
	{
		m_frame = getObject()->logic().getFrame();
	}
}

// RW 0x5015D4 / BFME2 slot 0
void RainOfFireUpdate::release(float amount)
{
	m_scale = amount;
}

// RW 0x8AF79F / BFME2 0x4AC18D
void RainOfFireUpdate::advance(float amount)
{
	m_darkness += amount;
}

UpdateSleepTime RainOfFireUpdate::update()
{
	if (!m_data)
	{
		return UPDATE_SLEEP_FOREVER;
	}
	return UPDATE_SLEEP_NONE;
}

void RainOfFireUpdate::xfer(Xfer &x)
{
	SnapshotXfer::version(x, 1, 1);
	xferUpdateModule(x);
	x.xferUnsignedInt(&m_frame);
	x.xferReal(&m_darkness);
	x.xferReal(&m_scale);
}

void RainOfFireUpdate::registerClass(ModuleFactory &modules)
{
	modules.bindTypedData<RainOfFireUpdateModuleData>("RainOfFireUpdate", MODULETYPE_BEHAVIOR);
	modules.bindModuleProc("RainOfFireUpdate", MODULETYPE_BEHAVIOR,
		[](Thing *thing, const ModuleData *data, const ModuleFactory::ModuleTemplate &) -> std::unique_ptr<Module> {
			const auto *typed = dynamic_cast<const RainOfFireUpdateModuleData *>(data);
			if (!typed)
			{
				throw std::logic_error("RainOfFireUpdate: module data is not typed");
			}
			return std::make_unique<RainOfFireUpdate>(thing, typed);
		});
}

