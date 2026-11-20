// OpenBFME. GPL-3.0.
//
// Both editions: RW 0x004CF121, BFME2 0x004CEF80. See W3DLightDraw.h.
// TARGET RW 0x004B612F getModelName, RW 0x004CF240 GetSnapshotName, RW 0x004CF29D setHidden,
// RW 0x004CF2B5 doDrawModule, RW 0x004CF4D0 getModuleNameKey, RW 0x004CF5B6 / RW 0x004CF5D2 deleting destructors.

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif

#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DLightDraw.h"

#include "Common/Thing/ModuleFactory.h"
#include "GameClient/ClientEffects.h"
#include "GameClient/Drawable.h"
#include "GameLogic/SnapshotXfer.h"

#include <cmath>
#include <cstddef>
#include <memory>

namespace
{
#define LIGHT_ROW(name, parse, field) { name, parse, nullptr, (int)offsetof(W3DLightDrawModuleData, field) }
const FieldParse kW3DLightDrawFields[] = {
	LIGHT_ROW("Ambient", INI::parseRGBColor, m_ambient),
	LIGHT_ROW("Diffuse", INI::parseRGBColor, m_diffuse),
	LIGHT_ROW("Specular", INI::parseRGBColor, m_specular),
	LIGHT_ROW("Radius", INI::parseReal, m_radius),
	LIGHT_ROW("Intensity", INI::parseReal, m_intensity),
	LIGHT_ROW("FlickerAmplitude", INI::parseReal, m_flickerAmplitude),
	LIGHT_ROW("FlickerFrequency", INI::parseReal, m_flickerFrequency),
	LIGHT_ROW("PulseAmplitude", INI::parseReal, m_pulseAmplitude),
	LIGHT_ROW("PulseFrequency", INI::parseReal, m_pulseFrequency),
	LIGHT_ROW("AttachToBoneInAnotherModule", INI::parseAsciiString, m_attachToBone),
	{ nullptr, nullptr, nullptr, 0 }
};
#undef LIGHT_ROW
} // namespace

namespace W3DLightDrawTables
{
const FieldParse *light()
{
	return kW3DLightDrawFields;
}
} // namespace W3DLightDrawTables

W3DLightDrawModuleData::W3DLightDrawModuleData()
	: m_ambient{ 0.0f, 0.0f, 0.0f }
	, m_diffuse{ 0.0f, 0.0f, 0.0f }
	, m_specular{ 0.0f, 0.0f, 0.0f }
	, m_radius(20.0f)
	, m_intensity(1.0f)
	, m_flickerAmplitude(0.0f)
	, m_flickerFrequency(0.0f)
	, m_pulseAmplitude(0.0f)
	, m_pulseFrequency(0.0f)
	, m_attachToBone("")
{
}

void W3DLightDrawModuleData::buildFieldParse(MultiIniFieldParse &p)
{
	p.add(kW3DLightDrawFields);
}

W3DLightDraw::W3DLightDraw(Thing *thing, const ModuleData *moduleData)
	: DrawableModule(thing, moduleData), m_thing(thing), m_data(dynamic_cast<const W3DLightDrawModuleData *>(moduleData))
{
	m_flickerRemaining = 0.0f;
	m_angle = 0.0f;
	m_height = m_data ? m_data->m_intensity : 1.0f;
	m_phase = 0.0f;
	m_enabled = true;
	if (m_data)
	{
		updateLight();
	}
}

W3DLightDraw::~W3DLightDraw()
{
	// RW 0x004CF246 / scalar deleting destructor RW 0x004CF5B6
	LightPulsePool *pool = getPool();
	if (pool && m_lightHandle >= 0)
	{
		pool->removePersistent(m_lightHandle);
		m_lightHandle = -1;
	}
}

float W3DLightDraw::nextRandomUnit()
{
	// RW 0x00B25D60 WWMath::Random_Float: (rand() & 0xFFF) * (1.0f / 4095.0f)
	m_rngState = m_rngState * 214013u + 2531011u;
	const int r = (int)((m_rngState >> 16) & 0xFFFu);
	return (float)r * (1.0f / 4095.0f);
}

LightPulsePool *W3DLightDraw::getPool() const
{
	if (m_pool)
	{
		return m_pool;
	}
	if (m_thing)
	{
		if (const Drawable *d = m_thing->asDrawable())
		{
			return d->drawServices().lightPulses;
		}
	}
	return nullptr;
}

void W3DLightDraw::setLightPool(LightPulsePool *pool)
{
	if (m_pool != pool)
	{
		if (m_pool && m_lightHandle >= 0)
		{
			m_pool->removePersistent(m_lightHandle);
			m_lightHandle = -1;
		}
		m_pool = pool;
		updateLight();
	}
}

float W3DLightDraw::lightRadius() const
{
	return m_data ? m_data->m_radius : 0.0f;
}

RGBColor W3DLightDraw::lightColor() const
{
	// RW 0x004CF18E sets m_ambient (+0xD4) and m_diffuse (+0xE0) separately.
	// The dynamic point light diffuse is Diffuse * Intensity (no fallback to Ambient).
	if (!m_data)
	{
		return RGBColor{ 0.0f, 0.0f, 0.0f };
	}
	const RGBColor &base = m_data->m_diffuse;
	return RGBColor{ base.red * m_height, base.green * m_height, base.blue * m_height };
}

RGBColor W3DLightDraw::lightAmbient() const
{
	// lane LOCAL-38: Ambient (+0xD4, RW 0x4CF1A0) times the current Intensity (+0xD0, written by RW 0x4CF479), as Init_From_Point RW 0x53F90C
	if (!m_data)
	{
		return RGBColor{ 0.0f, 0.0f, 0.0f };
	}
	const RGBColor &base = m_data->m_ambient;
	return RGBColor{ base.red * m_height, base.green * m_height, base.blue * m_height };
}

Coord3D W3DLightDraw::lightPosition() const
{
	if (m_data && !m_data->m_attachToBone.empty() && m_thing)
	{
		if (const Drawable *d = m_thing->asDrawable())
		{
			Coord3D bonePos;
			if (d->boneWorldPosition(m_data->m_attachToBone, bonePos))
			{
				return bonePos;
			}
		}
	}
	return m_thing ? *m_thing->getPosition() : Coord3D{ 0.0f, 0.0f, 0.0f };
}

void W3DLightDraw::updateLight()
{
	if (!m_data)
	{
		return;
	}
	LightPulsePool *pool = getPool();
	if (!pool)
	{
		return;
	}
	const Coord3D pos = lightPosition();
	const RGBColor col = lightColor();
	const float rad = lightRadius();
	// lane LOCAL-38: LightEnvironmentClass::Add_Light reads the raw Diffuse (+0xE0) for its black filter (RW 0x540261) and adds
	// Ambient (+0xD4) * Intensity (+0xD0, RW 0x53F90C) to the object's ambient; the ctor set both from the module data (RW 0x4CF18E .. 0x4CF1DC)
	const RGBColor amb = lightAmbient();
	const RGBColor raw = m_data->m_diffuse;
	if (m_lightHandle < 0 || (size_t)m_lightHandle >= pool->lights().size())
	{
		if (m_enabled)
		{
			m_lightHandle = pool->addPersistent(pos, col, rad, &amb, &raw);
		}
	}
	else
	{
		pool->updatePersistent(m_lightHandle, pos, col, rad, m_enabled, &amb, &raw);
	}
}

void W3DLightDraw::reactToTransformChange(const Coord3D * /*oldPos*/, float /*oldAngle*/)
{
	// RW 0x004CF5EE: updates bone or drawable position to dynamic light
	updateLight();
}

void W3DLightDraw::setEnabled(bool enabled)
{
	m_enabled = enabled;
	updateLight();
}

void W3DLightDraw::setHidden(bool hidden)
{
	// RW 0x004CF29D (DONOR BFME2 0x000CFA99): m_light->setEnabled(!hidden)
	setEnabled(!hidden);
}

void W3DLightDraw::stepFlicker(float dt)
{
	// RW 0x004CF2B5 doDrawModule (DONOR BFME2 0x000CFC0C, W3DLightDrawDoDrawModule.cpp)
	m_phase += dt;
	if (!m_data)
	{
		return;
	}

	float intensity = m_data->m_intensity;

	if (m_data->m_flickerAmplitude > 0.0f)
	{
		if (m_flickerRemaining <= 0.0f)
		{
			if (m_data->m_flickerFrequency > 0.0001f)
			{
				m_flickerRemaining = 1.0f / m_data->m_flickerFrequency;
			}
			else
			{
				m_flickerRemaining = 1.0f;
			}
			const float minVal = -m_data->m_flickerAmplitude;
			const float maxVal = m_data->m_flickerAmplitude;
			// RW 0x004CF3E1..0x004CF40B: Random_Float(-m_flickerAmplitude, +m_flickerAmplitude)
			const float targetOffset = minVal + nextRandomUnit() * (maxVal - minVal);
			m_angle = (targetOffset + intensity - m_height) / (m_flickerRemaining > 0.0f ? m_flickerRemaining : 1.0f);
		}
		m_flickerRemaining -= dt;
		intensity = m_angle * dt + m_height;
		m_height = intensity;
	}

	if (m_data->m_pulseAmplitude > 0.0f)
	{
		intensity += std::sin(m_phase * m_data->m_pulseFrequency * 6.2831853f) * m_data->m_pulseAmplitude;
	}

	m_height = intensity;
	updateLight();
}

void W3DLightDraw::xfer(Xfer &x)
{
	// RW 0x004CF488 (DONOR BFME2 0x000CFAB1, W3DLightDrawXfer.cpp)
	SnapshotXfer::version(x, 1, 1);
	x.xferReal(&m_phase);
	x.xferReal(&m_flickerRemaining);
	x.xferReal(&m_angle);
	x.xferReal(&m_height);
}

void W3DLightDraw::registerClass(ModuleFactory &modules)
{
	// RW 0x004CF4D0 getModuleNameKey / RW 0x004CF240 GetSnapshotName
	modules.bindTypedData<W3DLightDrawModuleData>("W3DLightDraw", MODULETYPE_DRAW);
	modules.bindModuleProc("W3DLightDraw", MODULETYPE_DRAW, [](Thing *thing, const ModuleData *data, const ModuleFactory::ModuleTemplate &) -> std::unique_ptr<Module> {
		const auto *typed = dynamic_cast<const W3DLightDrawModuleData *>(data);
		return std::make_unique<W3DLightDraw>(thing, typed);
	});
}
