// OpenBFME. GPL-3.0.
//
// W3DProjectileStreamDraw: draws projectile stream trails (lane MODGAP-2, stop S-113).
//
// Both editions:
//   RotWK 2.01:
//     - friend_newModuleInstance: RW 0x004648A7
//     - friend_newModuleData:     RW 0x004648DF
//     - buildFieldParse:          RW 0x004D0C00, table RW 0x00BDBAA8
//     - ModuleData ctor:          RW 0x004D0C20, vtable RW 0x00BE3EB8
//     - ctor:                      RW 0x004D0780, vtable RW 0x00BE3EF8
//     - ~W3DProjectileStreamDraw:  RW 0x004D0840
//     - doDrawModule:             RW 0x004D08B0
//     - setFullyObscuredByShroud: RW 0x004D09F0
//     - xfer:                      RW 0x004D0B40
//   BFME2 1.06:
//     - friend_newModuleInstance: BFME2 0x00465063 (or 0x000650B0)
//     - friend_newModuleData:     BFME2 0x0046509A (or 0x000650E8)
//     - buildFieldParse:          BFME2 0x000D125C, table BFME2 0x00BCDFB0
//     - ModuleData ctor:          BFME2 0x000D11D2, vtable BFME2 0x00BCDF18
//     - ctor:                      BFME2 0x004D0F70 (or 0x000D1370), vtable BFME2 0x00BCE010
//     - ~W3DProjectileStreamDraw: BFME2 0x000D146B
//     - doDrawModule:             BFME2 0x000D14F9
//     - setFullyObscuredByShroud: BFME2 0x000D1124
//     - xfer:                      BFME2 0x000D11A6

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
	static void buildFieldParse(MultiIniFieldParse &p);

	std::string m_texture;             ///< +0x08 Texture
	float m_width = 0.0f;              ///< +0x0C Width (default 0.0f)
	float m_tileFactor = 0.0f;         ///< +0x10 TileFactor (default 0.0f)
	float m_scrollRate = 0.0f;         ///< +0x14 ScrollRate (default 0.0f)
	int m_maxSegments = 0;             ///< +0x18 MaxSegments (default 0)
};

namespace W3DProjectileStreamDrawTables
{
const FieldParse *stream(); ///< RW 0x00BDBAA8 / BFME2 0x00BCDFB0
}

class ModuleFactory;

class W3DProjectileStreamDraw : public DrawableModule
{
public:
	OBFME_MODULE_XFER(W3DProjectileStreamDraw)

	W3DProjectileStreamDraw(Thing *thing, const ModuleData *moduleData);
	~W3DProjectileStreamDraw() override = default;

	void setFullyObscuredByShroud(bool fullyObscured);
	bool isFullyObscuredByShroud() const { return m_fullyObscured; }

	void updateStreamPoints();
	const std::vector<Coord3D> &streamPoints() const { return m_streamPoints; }

	const W3DProjectileStreamDrawModuleData *getData() const { return m_data; }

	static void registerClass(ModuleFactory &modules);

private:
	Thing *m_thing = nullptr;
	const W3DProjectileStreamDrawModuleData *m_data = nullptr;
	bool m_fullyObscured = false;
	std::vector<Coord3D> m_streamPoints;
};
