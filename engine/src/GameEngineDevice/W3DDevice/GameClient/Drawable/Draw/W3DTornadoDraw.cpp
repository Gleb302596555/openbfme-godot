// OpenBFME. GPL-3.0.
//
// Both editions: RW 0x004D102E, BFME2 0x004D181D. See W3DTornadoDraw.h.

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif

#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DTornadoDraw.h"

#include "Common/Thing/ModuleFactory.h"
#include "GameClient/Drawable.h"

#include <cstddef>
#include <memory>
#include <stdexcept>

namespace
{
#define TORNADO_ROW(name, parse, field) { name, parse, nullptr, (int)offsetof(W3DTornadoDrawModuleData, field) }
const FieldParse kW3DTornadoDrawFields[] = {
	TORNADO_ROW("DecalTemplate", parseRadiusDecalTemplate, m_decalTemplate),
	TORNADO_ROW("DecalCount", INI::parseUnsignedInt, m_decalCount),
	TORNADO_ROW("DecalMaxRadius", INI::parseReal, m_decalMaxRadius),
	{ nullptr, nullptr, nullptr, 0 }
};
#undef TORNADO_ROW
} // namespace

namespace W3DTornadoDrawTables
{
const FieldParse *tornado()
{
	return kW3DTornadoDrawFields;
}
} // namespace W3DTornadoDrawTables

W3DTornadoDrawModuleData::W3DTornadoDrawModuleData()
	: m_decalCount(0), m_decalMaxRadius(200.0f)
{
}

void W3DTornadoDrawModuleData::buildFieldParse(MultiIniFieldParse &p)
{
	p.add(kW3DTornadoDrawFields);
}

W3DTornadoDraw::W3DTornadoDraw(Thing *thing, const ModuleData *moduleData)
	: DrawableModule(thing, moduleData), m_thing(thing), m_data(dynamic_cast<const W3DTornadoDrawModuleData *>(moduleData))
{
	createDecals();
}

W3DTornadoDraw::~W3DTornadoDraw()
{
	clearDecals();
}

void W3DTornadoDraw::clearDecals()
{
	m_decals.clear();
}

void W3DTornadoDraw::createDecals()
{
	clearDecals();
	if (!m_data || m_data->m_decalCount == 0 || !m_thing)
	{
		return;
	}
	const Coord3D *p = m_thing->getPosition();
	const Coord3D pos = p ? *p : Coord3D{};
	float bound = m_data->m_decalTemplate.maxRadius;
	if (bound > m_data->m_decalMaxRadius)
	{
		bound = m_data->m_decalMaxRadius;
	}
	m_decals.reserve(m_data->m_decalCount);
	for (unsigned i = 0; i < m_data->m_decalCount; ++i)
	{
		DecalInstance d;
		d.position = pos;
		d.radius = (float)i * bound / (float)m_data->m_decalCount;
		m_decals.push_back(d);
	}
}

void W3DTornadoDraw::reactToTransformChange(const Coord3D * /*oldPos*/, float /*oldAngle*/)
{
	if (m_thing && m_thing->getPosition())
	{
		const Coord3D pos = *m_thing->getPosition();
		for (auto &d : m_decals)
		{
			d.position = pos;
		}
	}
}

void W3DTornadoDraw::xfer(Xfer &x)
{
	SnapshotXfer::version(x, 1, 1);
	std::uint32_t count = (std::uint32_t)m_decals.size();
	x.xferCount(&count, sizeof(float) * 4, sizeof(DecalInstance));
	if (x.isLoading())
	{
		m_decals.resize(count);
	}
	for (size_t i = 0; i < count; ++i)
	{
		SnapshotXfer::coord3D(x, m_decals[i].position);
		x.xferReal(&m_decals[i].radius);
	}
}

void W3DTornadoDraw::registerClass(ModuleFactory &modules)
{
	modules.bindTypedData<W3DTornadoDrawModuleData>("W3DTornadoDraw", MODULETYPE_DRAW);
	modules.bindModuleProc("W3DTornadoDraw", MODULETYPE_DRAW, [](Thing *thing, const ModuleData *data, const ModuleFactory::ModuleTemplate &) -> std::unique_ptr<Module> {
		const auto *typed = dynamic_cast<const W3DTornadoDrawModuleData *>(data);
		return std::make_unique<W3DTornadoDraw>(thing, typed);
	});
}
