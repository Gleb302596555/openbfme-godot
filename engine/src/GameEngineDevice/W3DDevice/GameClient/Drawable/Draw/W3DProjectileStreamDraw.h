// OpenBFME. GPL-3.0.
//
// W3DProjectileStreamDraw: draws projectile stream trails (lane MODGAP-2, stop S-113; Local 88 #714).
//
// Both editions:
//   RotWK 2.01:
//     - friend_newModuleInstance: RW 0x004648A7
//     - friend_newModuleData:     RW 0x004648DF
//     - setFullyObscuredByShroud: RW 0x004D0933
//     - xfer:                     RW 0x004D09B5
//     - ModuleData ctor:          RW 0x004D09E1, vtable RW 0x00BE3FD8
//     - ~ModuleData:              RW 0x004D0A35
//     - buildFieldParse:          RW 0x004D0A6B, table RW 0x00BE3F98, name RW 0x00BDBAA8
//     - makeOrUpdateLine:         RW 0x004D0A7C
//     - ctor:                     RW 0x004D0B61, vtable RW 0x00BE3FF8
//     - GetSnapshotName / name:   RW 0x004D0C10
//     - getModuleNameKey:         RW 0x004D0C16
//     - scalar deleting dtor:     RW 0x004D0C5B
//     - ~W3DProjectileStreamDraw: RW 0x004D0C77
//     - doDrawModule:             RW 0x004D0D05
//   BFME2 1.06:
//     - friend_newModuleInstance: BFME2 0x00465063 (or 0x000650B0)
//     - friend_newModuleData:     BFME2 0x0046509A (or 0x000650E8)
//     - setFullyObscuredByShroud: BFME2 0x000D1124
//     - xfer:                     BFME2 0x000D11A6
//     - ModuleData ctor:          BFME2 0x000D11D2, vtable BFME2 0x00BCDF18
//     - buildFieldParse:          BFME2 0x000D125C, table BFME2 0x00BCDFB0
//     - makeOrUpdateLine:         BFME2 0x000D126D
//     - ctor:                     BFME2 0x004D0F70 (or 0x000D1370), vtable BFME2 0x00BCE010
//     - GetSnapshotName / name:   BFME2 0x000D1401
//     - getModuleNameKey:         BFME2 0x000D140A
//     - scalar deleting dtor:     BFME2 0x000D144F
//     - ~W3DProjectileStreamDraw: BFME2 0x000D146B
//     - doDrawModule:             BFME2 0x000D14F9

#pragma once

#include "Common/INI.h"
#include "Common/INIDataTypes.h"
#include "Common/Module.h"
#include "GameLogic/SnapshotXfer.h"

#include <string>
#include <vector>

class W3DProjectileStreamDrawModuleData : public ModuleData
{
public:
	W3DProjectileStreamDrawModuleData();
	~W3DProjectileStreamDrawModuleData() override = default; // RW 0x004D0A35
	static void buildFieldParse(MultiIniFieldParse &p);

	std::string m_texture;             ///< +0x08 Texture
	float m_width = 0.0f;              ///< +0x0C Width (default 0.0f)
	float m_tileFactor = 0.0f;         ///< +0x10 TileFactor (default 0.0f)
	float m_scrollRate = 0.0f;         ///< +0x14 ScrollRate (default 0.0f)
	int m_maxSegments = 0;             ///< +0x18 MaxSegments (default 0)
};

namespace W3DProjectileStreamDrawTables
{
const FieldParse *stream(); ///< RW 0x00BE3F98 / BFME2 0x00BCDFB0
}

class ModuleFactory;

// One segmented line created/updated by W3DProjectileStreamDraw::makeOrUpdateLine (RW 0x004D0A7C).
struct W3DProjectileStreamLine
{
	std::vector<Coord3D> points;
	std::string texture;
	float width = 0.0f;
	float tileFactor = 0.0f;
	float scrollRate = 0.0f;
	bool inScene = true; ///< toggled by setFullyObscuredByShroud (RW 0x004D0933)
};

class W3DProjectileStreamDraw : public DrawableModule
{
public:
	OBFME_MODULE_XFER(W3DProjectileStreamDraw)

	W3DProjectileStreamDraw(Thing *thing, const ModuleData *moduleData);
	~W3DProjectileStreamDraw() override = default; // RW 0x004D0C77 / scalar deleting dtor RW 0x004D0C5B

	// RW 0x004D0933 (DONOR BFME2 0x000D1124): removes/re-adds m_allLines[0..m_linesValid) from/to the 3D scene
	void setFullyObscuredByShroud(bool fullyObscured);
	bool isFullyObscuredByShroud() const { return m_fullyObscured; }

	// RW 0x004D0D05 doDrawModule (DONOR BFME2 0x000D14F9): queries ProjectileStreamUpdate::getAllPoints and builds segment lines
	void doDrawModule();
	void updateStreamPoints();
	// Direct point-array entry for unit tests of the RW 0x004D0D05 / RW 0x004D0A7C segment builder
	void buildLinesFromPoints(const Coord3D *allPoints, int pointsUsed);

	const std::vector<Coord3D> &streamPoints() const { return m_streamPoints; }
	const std::vector<W3DProjectileStreamLine> &lines() const { return m_allLines; }
	int linesValid() const { return (int)m_allLines.size(); }

	const W3DProjectileStreamDrawModuleData *getData() const { return m_data; }
	float streamWidth() const { return m_data && m_data->m_width > 0.0f ? m_data->m_width : 0.0f; }
	float streamTileFactor() const { return m_data && m_data->m_tileFactor != 0.0f ? m_data->m_tileFactor : 0.0f; }
	float streamScrollRate() const { return m_data && m_data->m_scrollRate != 0.0f ? m_data->m_scrollRate : 0.0f; }

	static void registerClass(ModuleFactory &modules);

private:
	// RW 0x004D0A7C (DONOR BFME2 0x000D126D): creates or updates m_allLines[lineIndex]
	void makeOrUpdateLine(const Coord3D *points, int pointCount, int lineIndex);

	Thing *m_thing = nullptr;
	const W3DProjectileStreamDrawModuleData *m_data = nullptr;
	bool m_fullyObscured = false;
	std::vector<Coord3D> m_streamPoints;
	std::vector<W3DProjectileStreamLine> m_allLines; ///< +0x10..+0x5C, m_linesValid at +0x60
};
