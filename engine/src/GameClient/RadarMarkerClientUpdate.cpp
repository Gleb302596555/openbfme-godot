// OpenBFME. GPL-3.0.
// See GameClient/RadarMarkerClientUpdate.h (lane MODGAP-2; editions: both RotWK 2.01 and BFME2 1.06).

#include "GameClient/RadarMarkerClientUpdate.h"
#include "GameClient/Drawable.h"

#include "Common/INI.h"
#include "Common/Thing/ModuleFactory.h"

#include <cstddef>
#include <memory>
#include <stdexcept>

namespace
{
#define RMCU_OFF(m) (int)offsetof(RadarMarkerClientUpdateModuleData, m)
// RW 0x8CE0CA / BFME2 0x4C9BBC
const FieldParse kRadarMarkerClientUpdateFieldParse[] = {
	{ "MarkerType", INI::parseAsciiString, nullptr, RMCU_OFF(m_markerType) },
	{ nullptr, nullptr, nullptr, 0 }
};
#undef RMCU_OFF

const std::string kEmptyString;
} // namespace

void RadarMarkerClientUpdateModuleData::buildFieldParse(MultiIniFieldParse &p)
{
	p.add(kRadarMarkerClientUpdateFieldParse);
}

RadarMarkerClientUpdate::RadarMarkerClientUpdate(Thing *thing, const RadarMarkerClientUpdateModuleData *data)
	: DrawableModule(thing, data)
	, m_data(data)
{
}

const std::string &RadarMarkerClientUpdate::markerType() const
{
	return m_data ? m_data->m_markerType : kEmptyString;
}

// RW 0x8CE116 / BFME2 0x4C9CF7
void RadarMarkerClientUpdate::clientUpdate()
{
	Drawable *d = getDrawable();
	if (!d || !m_data || m_data->m_markerType.empty())
	{
		return;
	}
	m_markerPosition = *d->getPosition();
	m_hasMarker = true;
}

void RadarMarkerClientUpdate::registerClass(ModuleFactory &modules)
{
	modules.bindTypedData<RadarMarkerClientUpdateModuleData>("RadarMarkerClientUpdate", MODULETYPE_CLIENT_UPDATE);
	modules.bindModuleProc("RadarMarkerClientUpdate", MODULETYPE_CLIENT_UPDATE,
		[](Thing *thing, const ModuleData *data, const ModuleFactory::ModuleTemplate &) -> std::unique_ptr<Module> {
			const auto *typed = dynamic_cast<const RadarMarkerClientUpdateModuleData *>(data);
			if (!typed)
			{
				throw std::logic_error("RadarMarkerClientUpdate: module data is not typed");
			}
			return std::make_unique<RadarMarkerClientUpdate>(thing, typed);
		});
}

