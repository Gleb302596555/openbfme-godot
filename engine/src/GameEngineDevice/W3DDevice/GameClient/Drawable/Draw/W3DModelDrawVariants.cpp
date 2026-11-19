// OpenBFME. GPL-3.0.
//
// Module data of the W3D model draw variants. See W3DModelDrawVariants.h for the sources.

#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DModelDrawVariants.h"
#include "Common/AsciiString.h"
#include "Common/Xfer.h"
#include "GameLogic/SnapshotXfer.h"
#include <algorithm>
#include <cmath>

#include <cstddef>

#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif

namespace
{
#define STR_ROW(cls, name, field) { name, INI::parseAsciiString, nullptr, (int)offsetof(cls, field) }
#define REAL_ROW(cls, name, field) { name, INI::parseReal, nullptr, (int)offsetof(cls, field) }

// RW 0xBE22B8, 26 rows in the binary's order.
const FieldParse kTruckFieldParse[] = {
	STR_ROW(W3DTruckDrawModuleData, "Dust", m_dust),
	STR_ROW(W3DTruckDrawModuleData, "DirtSpray", m_dirtSpray),
	STR_ROW(W3DTruckDrawModuleData, "PowerslideSpray", m_powerslideSpray),
	STR_ROW(W3DTruckDrawModuleData, "LeftFrontTireBone", m_leftFrontTireBone),
	STR_ROW(W3DTruckDrawModuleData, "RightFrontTireBone", m_rightFrontTireBone),
	STR_ROW(W3DTruckDrawModuleData, "LeftRearTireBone", m_leftRearTireBone),
	STR_ROW(W3DTruckDrawModuleData, "RightRearTireBone", m_rightRearTireBone),
	STR_ROW(W3DTruckDrawModuleData, "MidLeftFrontTireBone", m_midLeftFrontTireBone),
	STR_ROW(W3DTruckDrawModuleData, "MidRightFrontTireBone", m_midRightFrontTireBone),
	STR_ROW(W3DTruckDrawModuleData, "MidLeftRearTireBone", m_midLeftRearTireBone),
	STR_ROW(W3DTruckDrawModuleData, "MidRightRearTireBone", m_midRightRearTireBone),
	STR_ROW(W3DTruckDrawModuleData, "MidLeftMidTireBone", m_midLeftMidTireBone),
	STR_ROW(W3DTruckDrawModuleData, "MidRightMidTireBone", m_midRightMidTireBone),
	STR_ROW(W3DTruckDrawModuleData, "LeftFrontTireBone2", m_leftFrontTireBone2),
	STR_ROW(W3DTruckDrawModuleData, "RightFrontTireBone2", m_rightFrontTireBone2),
	STR_ROW(W3DTruckDrawModuleData, "LeftRearTireBone2", m_leftRearTireBone2),
	STR_ROW(W3DTruckDrawModuleData, "RightRearTireBone2", m_rightRearTireBone2),
	STR_ROW(W3DTruckDrawModuleData, "MidLeftMidTireBone2", m_midLeftMidTireBone2),
	STR_ROW(W3DTruckDrawModuleData, "MidRightMidTireBone2", m_midRightMidTireBone2),
	REAL_ROW(W3DTruckDrawModuleData, "TireRotationMultiplier", m_tireRotationMultiplier),
	REAL_ROW(W3DTruckDrawModuleData, "PowerslideRotationAddition", m_powerslideRotationAddition),
	STR_ROW(W3DTruckDrawModuleData, "CabBone", m_cabBone),
	STR_ROW(W3DTruckDrawModuleData, "TrailerBone", m_trailerBone),
	REAL_ROW(W3DTruckDrawModuleData, "CabRotationMultiplier", m_cabRotationMultiplier),
	REAL_ROW(W3DTruckDrawModuleData, "TrailerRotationMultiplier", m_trailerRotationMultiplier),
	REAL_ROW(W3DTruckDrawModuleData, "RotationDamping", m_rotationDamping),
	{ nullptr, nullptr, nullptr, 0 }
};

// RW 0xBE3B30
const FieldParse kSailFieldParse[] = {
	{ "MaxRotationDegrees", INI::parseAngleReal, nullptr, (int)offsetof(W3DSailModelDrawModuleData, m_maxRotation) },
	{ "BlowingThresholdDegrees", INI::parseAngleReal, nullptr, (int)offsetof(W3DSailModelDrawModuleData, m_blowingThreshold) },
	REAL_ROW(W3DSailModelDrawModuleData, "AboutDamping", m_aboutDamping),
	{ nullptr, nullptr, nullptr, 0 }
};

// RW 0xBE1A80
const FieldParse kQuadrupedFieldParse[] = {
	STR_ROW(W3DQuadrupedDrawModuleData, "LeftFrontFootBone", m_leftFrontFootBone),
	STR_ROW(W3DQuadrupedDrawModuleData, "RightFrontFootBone", m_rightFrontFootBone),
	STR_ROW(W3DQuadrupedDrawModuleData, "LeftRearFootBone", m_leftRearFootBone),
	STR_ROW(W3DQuadrupedDrawModuleData, "RightRearFootBone", m_rightRearFootBone),
	{ nullptr, nullptr, nullptr, 0 }
};

// RW 0xBE2968
const FieldParse kTankFieldParse[] = {
	STR_ROW(W3DTankDrawModuleData, "TreadDebrisLeft", m_treadDebrisLeft),
	STR_ROW(W3DTankDrawModuleData, "TreadDebrisRight", m_treadDebrisRight),
	{ "TreadAnimationRate", INI::parseVelocityReal, nullptr, (int)offsetof(W3DTankDrawModuleData, m_treadAnimationRate) },
	REAL_ROW(W3DTankDrawModuleData, "TreadPivotSpeedFraction", m_treadPivotSpeedFraction),
	REAL_ROW(W3DTankDrawModuleData, "TreadDriveSpeedFraction", m_treadDriveSpeedFraction),
	{ nullptr, nullptr, nullptr, 0 }
};

// RW 0xBE1E38
const FieldParse kSupplyFieldParse[] = {
	STR_ROW(W3DSupplyDrawModuleData, "SupplyBonePrefix", m_supplyBonePrefix),
	{ nullptr, nullptr, nullptr, 0 }
};
#undef STR_ROW
#undef REAL_ROW
} // namespace

void W3DTruckDrawModuleData::buildFieldParse(MultiIniFieldParse &p)
{
	W3DModelDrawModuleData::buildFieldParse(p); // registry: tables [0xBE1320, 0xBE22B8]
	p.add(kTruckFieldParse);
}

void W3DSailModelDrawModuleData::buildFieldParse(MultiIniFieldParse &p)
{
	W3DModelDrawModuleData::buildFieldParse(p);
	p.add(kSailFieldParse);
}

void W3DQuadrupedDrawModuleData::buildFieldParse(MultiIniFieldParse &p)
{
	W3DModelDrawModuleData::buildFieldParse(p);
	p.add(kQuadrupedFieldParse);
}

void W3DTankDrawModuleData::buildFieldParse(MultiIniFieldParse &p)
{
	W3DModelDrawModuleData::buildFieldParse(p);
	p.add(kTankFieldParse);
}

void W3DSupplyDrawModuleData::buildFieldParse(MultiIniFieldParse &p)
{
	W3DModelDrawModuleData::buildFieldParse(p);
	p.add(kSupplyFieldParse);
}

namespace W3DVariantTables
{
const FieldParse *truck() { return kTruckFieldParse; }
const FieldParse *sail() { return kSailFieldParse; }
const FieldParse *quadruped() { return kQuadrupedFieldParse; }
const FieldParse *tank() { return kTankFieldParse; }
const FieldParse *supply() { return kSupplyFieldParse; }
} // namespace W3DVariantTables

// ---------------------------------------------------------------------------------------------------------------------
// W3DTankDrawState & W3DTruckDrawState (Local 88 #714)
// TARGET W3DTankDraw: RW 0x004CD830 xfer, RW 0x004CD91D setHidden, RW 0x004CD93B setFullyObscuredByShroud,
//   RW 0x004CDBD2 onRenderObjRecreated, RW 0x004CDBD7 doDrawModule, RW 0x004CE1E2 getModuleNameKey,
//   RW 0x004CE227 GetSnapshotName, RW 0x004CE22D loadPostProcess, RW 0x004CE320 ModuleData scalar deleting dtor.
// TARGET W3DTruckDraw: RW 0x004CA936 xfer, RW 0x004CAD65 getModuleNameKey, RW 0x004CADAA GetSnapshotName,
//   RW 0x004CBF6E setHidden, RW 0x004CBF8E onRenderObjRecreated, RW 0x004CD634 loadPostProcess,
//   RW 0x004CD6F7 setFullyObscuredByShroud, RW 0x004CD721 scalar deleting dtor.
// ---------------------------------------------------------------------------------------------------------------------

W3DTankDrawState::W3DTankDrawState(const W3DTankDrawModuleData *data)
	: m_data(data)
	, m_emittersCreated(data != nullptr)
{
}

void W3DTankDrawState::onRenderObjRecreated(const std::vector<std::string> &subObjectNames)
{
	// RW 0x004CDBD2 (DONOR BFME2 0x000CE3DE): tail-jumps to updateTreadObjects (RW 0x004CD95E)
	updateTreadObjects(subObjectNames);
}

void W3DTankDrawState::updateTreadObjects(const std::vector<std::string> &subObjectNames)
{
	// RW 0x004CD95E: clears existing treads, returns early if !m_data || m_treadAnimationRate == 0.0f;
	// scans sub-objects for prefix "TREADS" (6 chars, case-insensitive after dot):
	// "TREADSL" -> TREAD_LEFT (0), "TREADSR" -> TREAD_RIGHT (1), else TREAD_MIDDLE (2), max 4 treads.
	m_treads.clear();
	if (!m_data || m_data->m_treadAnimationRate == 0.0f)
	{
		return;
	}
	for (const std::string &fullName : subObjectNames)
	{
		if ((int)m_treads.size() >= MAX_TREADS_PER_TANK)
		{
			break;
		}
		const size_t dot = fullName.find('.');
		const std::string base = (dot != std::string::npos) ? fullName.substr(dot + 1) : fullName;
		if (base.size() >= 6 && AsciiStringUtil::compareNoCase(base.substr(0, 6), "TREADS") == 0)
		{
			TreadObjectInfo info;
			info.name = fullName;
			info.u = 0.0f;
			if (AsciiStringUtil::compareNoCase(base, "TREADSL") == 0)
			{
				info.type = TREAD_LEFT;
			}
			else if (AsciiStringUtil::compareNoCase(base, "TREADSR") == 0)
			{
				info.type = TREAD_RIGHT;
			}
			else
			{
				info.type = TREAD_MIDDLE;
			}
			m_treads.push_back(info);
		}
	}
}

void W3DTankDrawState::updateTreadPositions(float uvDelta)
{
	// RW 0x004CD7BC: type 0 (TREAD_LEFT) adds uvDelta, type 1 (TREAD_RIGHT) subtracts uvDelta, wraps u -= floor(u)
	for (TreadObjectInfo &t : m_treads)
	{
		float offsetU = t.u;
		if (t.type == TREAD_LEFT)
		{
			offsetU += uvDelta;
		}
		else if (t.type == TREAD_RIGHT)
		{
			offsetU -= uvDelta;
		}
		offsetU -= std::floor(offsetU);
		t.u = offsetU;
	}
}

void W3DTankDrawState::startMoveDebris()
{
	// RW 0x004CD892: starts left/right tread debris emitters when drawable is not hidden
	if (m_hidden || !m_emittersCreated)
	{
		return;
	}
	m_leftDebrisActive = m_data && !m_data->m_treadDebrisLeft.empty();
	m_rightDebrisActive = m_data && !m_data->m_treadDebrisRight.empty();
}

void W3DTankDrawState::stopMoveDebris()
{
	// RW 0x004CD8DE: stops left/right tread debris emitters
	m_leftDebrisActive = false;
	m_rightDebrisActive = false;
}

void W3DTankDrawState::setHidden(bool hidden)
{
	// RW 0x004CD91D (DONOR BFME2 0x000CE129)
	m_hidden = hidden;
	if (hidden)
	{
		stopMoveDebris();
	}
}

void W3DTankDrawState::setFullyObscuredByShroud(bool fullyObscured)
{
	// RW 0x004CD93B (DONOR BFME2 0x000CE147)
	if (fullyObscured != m_fullyObscuredByShroud && fullyObscured)
	{
		stopMoveDebris();
	}
	m_fullyObscuredByShroud = fullyObscured;
}

void W3DTankDrawState::loadPostProcess()
{
	// RW 0x004CE22D (DONOR BFME2 0x000CEA39): W3DModelDraw::loadPostProcess, tossEmitters (RW 0x004CDFA8), createEmitters (RW 0x004CE02B)
	stopMoveDebris();
	m_emittersCreated = (m_data != nullptr);
}

void W3DTankDrawState::doDrawModule(float currentSpeed, float maxSpeed, int turnBits, bool isMoving, bool isBackingUp,
	float dirX, float dirY, bool airborne)
{
	// RW 0x004CDBD7 (DONOR BFME2 0x000CE3E3, W3DTankDraw.cpp)
	const float absSpeed = std::fabs(currentSpeed);
	if (absSpeed > 0.0001f && !m_hidden && !m_fullyObscuredByShroud && !airborne)
	{
		startMoveDebris();
	}
	else
	{
		stopMoveDebris();
	}

	// RW 0x004CDCEC..0x004CDD49: debris velocity multiplier
	m_debrisVelMult.x = std::min(1.0f, absSpeed * 0.5f + 0.3f);
	m_debrisVelMult.y = m_debrisVelMult.x;
	m_debrisVelMult.z = std::min(1.0f, absSpeed + 0.3f);

	if (m_data && !m_treads.empty() && maxSpeed > 0.0001f)
	{
		const float speedFrac = absSpeed / maxSpeed;
		const float rate = m_data->m_treadAnimationRate;
		// RW 0x004CDDDF..0x004CDE5A: pivot turn when turnBits != 0 and speedFrac < m_treadPivotSpeedFraction
		if (turnBits != 0 && speedFrac < m_data->m_treadPivotSpeedFraction)
		{
			if (1.0f - (dirX * m_lastDirX + dirY * m_lastDirY) > 0.00001f)
			{
				// turnBits < 0 (TURN_NEGATIVE = -1 or bit 0): +rate; else -rate
				updateTreadPositions((turnBits < 0 || (turnBits & 1)) ? rate : -rate);
			}
		}
		else if (isMoving && speedFrac >= m_data->m_treadDriveSpeedFraction)
		{
			// RW 0x004CDE66..0x004CDEC9: driving scrolls all treads equally (-rate forward, +rate backing up)
			const float delta = isBackingUp ? rate : -rate;
			for (TreadObjectInfo &t : m_treads)
			{
				float offsetU = t.u - delta;
				offsetU -= std::floor(offsetU);
				t.u = offsetU;
			}
		}
	}
	m_lastDirX = dirX;
	m_lastDirY = dirY;
}

void W3DTankDrawState::xfer(Xfer &x)
{
	// RW 0x004CD830 (DONOR BFME2 0x000CE03C): version(1, 1) + W3DScriptedModelDraw::xfer
	SnapshotXfer::version(x, 1, 1);
}

W3DTruckDrawState::W3DTruckDrawState(const W3DTruckDrawModuleData *data)
	: m_data(data)
{
}

void W3DTruckDrawState::enableEmitters(bool enable)
{
	// RW 0x004CB013
	m_emittersEnabled = enable && m_emittersCreated;
}

void W3DTruckDrawState::tossEmitters()
{
	// RW 0x004CADB0
	m_emittersEnabled = false;
	m_emittersCreated = false;
}

void W3DTruckDrawState::createEmitters()
{
	// RW 0x004CAE6F
	m_emittersCreated = true;
}

void W3DTruckDrawState::setHidden(bool hidden)
{
	// RW 0x004CBF6E (DONOR BFME2 0x000CC781, W3DTruckDrawSetHidden.cpp)
	m_hidden = hidden;
	if (hidden)
	{
		enableEmitters(false);
	}
}

void W3DTruckDrawState::setFullyObscuredByShroud(bool fullyObscured)
{
	// RW 0x004CD6F7
	if (fullyObscured != m_fullyObscuredByShroud)
	{
		if (fullyObscured)
		{
			tossEmitters();
		}
		else
		{
			createEmitters();
		}
	}
	m_fullyObscuredByShroud = fullyObscured;
}

void W3DTruckDrawState::loadPostProcess()
{
	// RW 0x004CD634: W3DModelDraw::loadPostProcess + tossEmitters (RW 0x004CADB0)
	tossEmitters();
}

void W3DTruckDrawState::onRenderObjRecreated(int foundBoneCount)
{
	// RW 0x004CBF8E: zeroes m_prevRenderObj (+0x484) and the 16 wheel/cab/trailer bone indices (+0x320..+0x35C),
	// then tail-jumps to updateBones (RW 0x004CB0B8).
	m_resolvedBoneCount = 0;
	m_resolvedBoneCount = foundBoneCount;
	++m_boneUpdateCount;
}

void W3DTruckDrawState::xfer(Xfer &x)
{
	// RW 0x004CA936 (DONOR BFME2 W3DTruckDrawXfer.cpp): version(1, 1), xfers +0x2E9 and +0x2EA
	SnapshotXfer::version(x, 1, 1);
	x.xferBool(&m_effectsInitialized);
	x.xferBool(&m_wasAirborne);
}
