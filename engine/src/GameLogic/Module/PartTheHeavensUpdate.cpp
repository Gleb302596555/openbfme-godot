// OpenBFME. GPL-3.0.
// See GameLogic/Module/PartTheHeavensUpdate.h (lane MODGAP-2; editions: both RotWK 2.01 and BFME2 1.06).

#include "GameLogic/Module/PartTheHeavensUpdate.h"

#include "Common/INI.h"
#include "Common/Thing/ModuleFactory.h"
#include "Common/Xfer.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/SnapshotXfer.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <stdexcept>

namespace
{
constexpr float kRadsPerDegree = 0.0174532925f;

#define PTH_OFF(m) (int)offsetof(PartTheHeavensUpdateModuleData, m)
// RW 0xC6B8F8 / BFME2 0xC54DC8
const FieldParse kPartTheHeavensUpdateFieldParse[] = {
	{ "Texture", INI::parseAsciiString, nullptr, PTH_OFF(m_texture) },
	{ "Radius", FunctionCurve::parseFunctionCurve, nullptr, PTH_OFF(m_radius) },
	{ "Color", INI::parseColorInt, nullptr, PTH_OFF(m_color) },
	{ "Opacity", FunctionCurve::parseFunctionCurve, nullptr, PTH_OFF(m_opacity) },
	{ "Angle", FunctionCurve::parseFunctionCurve, nullptr, PTH_OFF(m_angle) },
	{ nullptr, nullptr, nullptr, 0 }
};
#undef PTH_OFF
} // namespace

PartTheHeavensUpdateModuleData::PartTheHeavensUpdateModuleData()
	: m_radius(0.0f)
	, m_opacity(1.0f)
	, m_angle(0.0f)
{
}

void PartTheHeavensUpdateModuleData::buildFieldParse(MultiIniFieldParse &p)
{
	p.add(kPartTheHeavensUpdateFieldParse);
}

PartTheHeavensUpdate::PartTheHeavensUpdate(Thing *thing, const PartTheHeavensUpdateModuleData *data)
	: UpdateModule(thing, data)
	, m_data(data)
{
	if (getObject())
	{
		m_startFrame = getObject()->logic().getFrame();
	}
}

UpdateSleepTime PartTheHeavensUpdate::update()
{
	if (!m_data)
	{
		return UPDATE_SLEEP_FOREVER;
	}

	unsigned int elapsed = 0;
	if (getObject())
	{
		unsigned int frame = getObject()->logic().getFrame();
		if (frame >= m_startFrame)
		{
			elapsed = frame - m_startFrame;
		}
	}

	// RW 0x8B0075 / BFME2 0x4AC9FA
	float t = (float)elapsed;
	m_currentRadius = 2.0f * m_data->m_radius(t);
	m_currentAngleRads = m_data->m_angle(t) * kRadsPerDegree;
	int rawOpacity = (int)(m_data->m_opacity(t) * 255.0f);
	m_currentOpacity = (std::uint8_t)std::clamp(rawOpacity, 0, 255);

	// Client projected shadow / decal emission is tracked by stop S-7093.

	return UPDATE_SLEEP_NONE;
}

void PartTheHeavensUpdate::xfer(Xfer &x)
{
	SnapshotXfer::version(x, 1, 1);
	xferUpdateModule(x);
	x.xferUnsignedInt(&m_startFrame);
}

void PartTheHeavensUpdate::registerClass(ModuleFactory &modules)
{
	modules.bindTypedData<PartTheHeavensUpdateModuleData>("PartTheHeavensUpdate", MODULETYPE_BEHAVIOR);
	modules.bindModuleProc("PartTheHeavensUpdate", MODULETYPE_BEHAVIOR,
		[](Thing *thing, const ModuleData *data, const ModuleFactory::ModuleTemplate &) -> std::unique_ptr<Module> {
			const auto *typed = dynamic_cast<const PartTheHeavensUpdateModuleData *>(data);
			if (!typed)
			{
				throw std::logic_error("PartTheHeavensUpdate: module data is not typed");
			}
			return std::make_unique<PartTheHeavensUpdate>(thing, typed);
		});
}
