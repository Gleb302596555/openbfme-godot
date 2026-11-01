// OpenBFME. GPL-3.0.
//
// Both editions: RW 0x004D0780, BFME2 0x004D0F70. See W3DProjectileStreamDraw.h.

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif

#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DProjectileStreamDraw.h"

#include "Common/Thing/ModuleFactory.h"
#include "GameClient/Drawable.h"
#include "GameLogic/Module/ProjectileStreamUpdate.h"
#include "GameLogic/Object/Object.h"

#include <cstddef>
#include <memory>

namespace
{
#define STREAM_ROW(name, parse, field) { name, parse, nullptr, (int)offsetof(W3DProjectileStreamDrawModuleData, field) }
const FieldParse kW3DProjectileStreamDrawFields[] = {
	STREAM_ROW("Texture", INI::parseAsciiString, m_texture),
	STREAM_ROW("Width", INI::parseReal, m_width),
	STREAM_ROW("TileFactor", INI::parseReal, m_tileFactor),
	STREAM_ROW("ScrollRate", INI::parseReal, m_scrollRate),
	STREAM_ROW("MaxSegments", INI::parseInt, m_maxSegments),
	{ nullptr, nullptr, nullptr, 0 }
};
#undef STREAM_ROW
} // namespace

namespace W3DProjectileStreamDrawTables
{
const FieldParse *stream()
{
	return kW3DProjectileStreamDrawFields;
}
} // namespace W3DProjectileStreamDrawTables

W3DProjectileStreamDrawModuleData::W3DProjectileStreamDrawModuleData()
	: m_texture(""), m_width(0.0f), m_tileFactor(0.0f), m_scrollRate(0.0f), m_maxSegments(0)
{
}

void W3DProjectileStreamDrawModuleData::buildFieldParse(MultiIniFieldParse &p)
{
	p.add(kW3DProjectileStreamDrawFields);
}

W3DProjectileStreamDraw::W3DProjectileStreamDraw(Thing *thing, const ModuleData *moduleData)
	: DrawableModule(thing, moduleData), m_thing(thing), m_data(dynamic_cast<const W3DProjectileStreamDrawModuleData *>(moduleData))
{
}

void W3DProjectileStreamDraw::setFullyObscuredByShroud(bool fullyObscured)
{
	m_fullyObscured = fullyObscured;
}

void W3DProjectileStreamDraw::updateStreamPoints()
{
	m_streamPoints.clear();
	if (!m_thing)
	{
		return;
	}
	Object *obj = dynamic_cast<Object *>(m_thing);
	if (!obj)
	{
		return;
	}
	auto *update = dynamic_cast<const ProjectileStreamUpdate *>(obj->findModule("ProjectileStreamUpdate"));
	if (!update)
	{
		return;
	}
	Coord3D pts[ProjectileStreamUpdate::MAX_PROJECTILE_STREAM];
	int count = 0;
	update->getAllPoints(pts, &count);
	m_streamPoints.assign(pts, pts + count);
}

void W3DProjectileStreamDraw::xfer(Xfer &x)
{
	SnapshotXfer::version(x, 1, 1);
	x.xferBool(&m_fullyObscured);
}

void W3DProjectileStreamDraw::registerClass(ModuleFactory &modules)
{
	modules.bindTypedData<W3DProjectileStreamDrawModuleData>("W3DProjectileStreamDraw", MODULETYPE_DRAW);
	modules.bindModuleProc("W3DProjectileStreamDraw", MODULETYPE_DRAW, [](Thing *thing, const ModuleData *data, const ModuleFactory::ModuleTemplate &) -> std::unique_ptr<Module> {
		const auto *typed = dynamic_cast<const W3DProjectileStreamDrawModuleData *>(data);
		return std::make_unique<W3DProjectileStreamDraw>(thing, typed);
	});
}
