// OpenBFME. GPL-3.0.
// See GameClient/BeaconClientUpdate.h (lane CHAT-1).

#include "GameClient/BeaconClientUpdate.h"

#include "Common/INI.h"
#include "Common/Thing/ModuleFactory.h"

#include <cstddef>
#include <memory>
#include <stdexcept>

namespace
{
#define BC_OFF(member) (int)offsetof(BeaconClientUpdateModuleData, member)
// RW 0xC757C4
const FieldParse kBeaconClientUpdateFieldParse[] = {
	{ "RadarPulseFrequency", INI::parseDurationUnsignedInt, nullptr, BC_OFF(m_framesBetweenRadarPulses) },
	{ "RadarPulseDuration", INI::parseDurationUnsignedInt, nullptr, BC_OFF(m_radarPulseDuration) },
	{ nullptr, nullptr, nullptr, 0 }
};
#undef BC_OFF
} // namespace

void BeaconClientUpdateModuleData::buildFieldParse(MultiIniFieldParse &p)
{
	p.add(kBeaconClientUpdateFieldParse);
}

BeaconClientUpdate::BeaconClientUpdate(Thing *thing, const BeaconClientUpdateModuleData *data) : DrawableModule(thing, data), m_data(data)
{
}

bool BeaconClientUpdate::pulseDue(std::uint32_t frame)
{
	if (!m_started)
	{
		m_started = true;
		m_lastRadarPulse = frame;
	}
	if (m_hidden || !(m_lastRadarPulse + m_data->m_framesBetweenRadarPulses < frame))
	{
		return false;
	}
	m_lastRadarPulse = frame;
	return true;
}

// RW 0x008CD9FD (getModuleName, DONOR BFME2 0x004C95C3), RW 0x008CDA03 (client DrawableModule xfer, DONOR BFME2 0x004C95C9; client-only, S-4190)
void BeaconClientUpdate::registerClass(ModuleFactory &modules)
{
	// RW 0x008CDA41 (BeaconClientUpdate::getModuleNameKey)
	modules.bindTypedData<BeaconClientUpdateModuleData>("BeaconClientUpdate", MODULETYPE_CLIENT_UPDATE);
	modules.bindModuleProc("BeaconClientUpdate", MODULETYPE_CLIENT_UPDATE,
		[](Thing *thing, const ModuleData *data, const ModuleFactory::ModuleTemplate &) -> std::unique_ptr<Module> {
			const BeaconClientUpdateModuleData *typed = dynamic_cast<const BeaconClientUpdateModuleData *>(data);
			if (!typed)
			{
				throw std::logic_error("BeaconClientUpdate: the module data is not typed");
			}
			return std::make_unique<BeaconClientUpdate>(thing, typed);
		});
}
