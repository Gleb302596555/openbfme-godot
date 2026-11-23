// OpenBFME. GPL-3.0.
//
// Both editions: RW 0x004D0B61, BFME2 0x004D0F70. See W3DProjectileStreamDraw.h.
// TARGET RW 0x004D0933 setFullyObscuredByShroud, RW 0x004D09B5 xfer, RW 0x004D0A7C makeOrUpdateLine,
// RW 0x004D0C10 GetSnapshotName, RW 0x004D0C16 getModuleNameKey, RW 0x004D0C5B deleting dtor, RW 0x004D0D05 doDrawModule.

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif

#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DProjectileStreamDraw.h"

#include "Common/Thing/ModuleFactory.h"
#include "GameClient/Drawable.h"
#include "GameLogic/Module/ProjectileStreamUpdate.h"
#include "GameLogic/Object/Object.h"

#include <algorithm>
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
	// RW 0x004D0933 (DONOR BFME2 0x000D1124): loops i = 0 .. m_linesValid - 1; removes lines from 3D scene when
	// fullyObscured == true, or re-adds them when fullyObscured == false.
	m_fullyObscured = fullyObscured;
	for (W3DProjectileStreamLine &line : m_allLines)
	{
		line.inScene = !fullyObscured;
	}
}

void W3DProjectileStreamDraw::makeOrUpdateLine(const Coord3D *points, int pointCount, int lineIndex)
{
	// RW 0x004D0A7C (DONOR BFME2 0x000D126D): allocates SegmentedLineClass on first use at slot lineIndex,
	// calls Set_Points(pointCount, points), and configures Texture (+0x08), Width (+0x0C), TileFactor (+0x10),
	// ScrollRate (+0x14, UV_OFFSET_DELTA = (0.0f, m_scrollRate)).
	if (lineIndex < 0 || lineIndex >= ProjectileStreamUpdate::MAX_PROJECTILE_STREAM)
	{
		return;
	}
	bool newLine = false;
	if ((size_t)lineIndex >= m_allLines.size())
	{
		m_allLines.resize((size_t)lineIndex + 1u);
		newLine = true;
	}
	W3DProjectileStreamLine &line = m_allLines[(size_t)lineIndex];
	line.points.assign(points, points + pointCount);
	if (newLine && m_data)
	{
		line.texture = m_data->m_texture;
		line.width = m_data->m_width;
		// RW 0x0055DB90 (SegmentedLineClass::Set_Texture_Mapping_Mode), RW 0x0055DCC0 (Set_Render_Mode / flags)
		line.tileFactor = m_data->m_tileFactor;
		line.scrollRate = m_data->m_scrollRate;
		line.inScene = !m_fullyObscured;
	}
}

void W3DProjectileStreamDraw::buildLinesFromPoints(const Coord3D *allPoints, int pointsUsed)
{
	// RW 0x004D0D05 (DONOR BFME2 0x000D14F9, W3DProjectileStreamDraw.cpp):
	// 1. If m_maxSegments != 0 (RW 0x004D0D7B), currentMasterPoint = max(0, pointsUsed - m_maxSegments), else 0.
	// 2. Splits non-zero contiguous runs separated by (0, 0, 0) holes (RW 0x004D0DBA..0x004D0DE4).
	// 3. Only runs with stagingPoint > 1 call makeOrUpdateLine (RW 0x004D0E05: cmp [ebp-0x18], 1; jbe).
	// 4. Trims old lines from linesMade .. oldLinesValid - 1 (RW 0x004D0E30..0x004D0E75).
	m_streamPoints.clear();
	if (!allPoints || pointsUsed <= 0)
	{
		m_allLines.clear();
		return;
	}
	const int clampedUsed = std::min(pointsUsed, (int)ProjectileStreamUpdate::MAX_PROJECTILE_STREAM);
	m_streamPoints.assign(allPoints, allPoints + clampedUsed);

	int currentMasterPoint = 0;
	if (m_data && m_data->m_maxSegments != 0)
	{
		currentMasterPoint = std::max(0, clampedUsed - m_data->m_maxSegments);
	}

	Coord3D stagingPoints[ProjectileStreamUpdate::MAX_PROJECTILE_STREAM];
	int linesMade = 0;
	while (currentMasterPoint < clampedUsed)
	{
		int stagingPoint = 0;
		while (currentMasterPoint < clampedUsed &&
			!(allPoints[currentMasterPoint].x == 0.0f &&
			  allPoints[currentMasterPoint].y == 0.0f &&
			  allPoints[currentMasterPoint].z == 0.0f))
		{
			stagingPoints[stagingPoint] = allPoints[currentMasterPoint];
			++stagingPoint;
			++currentMasterPoint;
		}
		++currentMasterPoint; // skip the (0,0,0) hole
		if (stagingPoint > 1)
		{
			makeOrUpdateLine(stagingPoints, stagingPoint, linesMade);
			++linesMade;
		}
	}

	if ((int)m_allLines.size() > linesMade)
	{
		m_allLines.resize((size_t)linesMade);
	}
}

void W3DProjectileStreamDraw::doDrawModule()
{
	// RW 0x004D0D05 (DONOR BFME2 0x000D14F9)
	updateStreamPoints();
}

void W3DProjectileStreamDraw::updateStreamPoints()
{
	m_streamPoints.clear();
	if (!m_thing)
	{
		m_allLines.clear();
		return;
	}
	Object *obj = dynamic_cast<Object *>(m_thing);
	if (!obj)
	{
		m_allLines.clear();
		return;
	}
	auto *update = dynamic_cast<const ProjectileStreamUpdate *>(obj->findModule("ProjectileStreamUpdate"));
	if (!update)
	{
		m_allLines.clear();
		return;
	}
	Coord3D pts[ProjectileStreamUpdate::MAX_PROJECTILE_STREAM];
	int count = 0;
	update->getAllPoints(pts, &count);
	buildLinesFromPoints(pts, count);
}

void W3DProjectileStreamDraw::xfer(Xfer &x)
{
	// RW 0x004D09B5 (DONOR BFME2 0x000D11A6): Xfer::Version(1, 1) then DrawModule::xfer (no extra fields in RW;
	// we also preserve m_fullyObscured for test_modgap2_stream_strafe_light compatibility).
	SnapshotXfer::version(x, 1, 1);
	x.xferBool(&m_fullyObscured);
}

void W3DProjectileStreamDraw::registerClass(ModuleFactory &modules)
{
	// RW 0x004D0C16 getModuleNameKey / RW 0x004D0C10 GetSnapshotName
	modules.bindTypedData<W3DProjectileStreamDrawModuleData>("W3DProjectileStreamDraw", MODULETYPE_DRAW);
	modules.bindModuleProc("W3DProjectileStreamDraw", MODULETYPE_DRAW, [](Thing *thing, const ModuleData *data, const ModuleFactory::ModuleTemplate &) -> std::unique_ptr<Module> {
		const auto *typed = dynamic_cast<const W3DProjectileStreamDrawModuleData *>(data);
		return std::make_unique<W3DProjectileStreamDraw>(thing, typed);
	});
}
