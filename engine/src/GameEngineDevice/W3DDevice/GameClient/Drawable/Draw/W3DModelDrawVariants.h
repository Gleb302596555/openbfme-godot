// OpenBFME. GPL-3.0.
//
// Module data of the ModelDraw variants W3DTruckDraw, W3DSailModelDraw, W3DQuadrupedDraw, W3DTankDraw and W3DSupplyDraw (lane
// MAPOBJ-1): the model draw table (RW 0xBE1320, W3DModelDraw.h) plus the variant's own table. They draw their base model through the
// merged W3DScriptedModelDraw runtime. W3DSailModelDraw's separate runtime now turns the model matrix (PORT-63),
// and truck tires spin (COMBAT-4); treads, feet, dust, cab and trailer rotation remain in stop S-112.
//
// Sources, in PLAN rule 1 priority order (TARGET = RotWK game.dat, caveat S-001):
//   TARGET registry entries (module-registry.json, createData / table adds), object sizes and constructors:
//     W3DTruckDraw      tables [0xBE1320, 0xBE22B8] (26 rows), createData RW 0x4642E7, size 0x1F0, constructor RW 0x4CA9B6, dtor RW 0x004CD660
//                       (the 21 strings empty; CabRotationMultiplier, TrailerRotationMultiplier, RotationDamping,
//                       TireRotationMultiplier 1.0; PowerslideRotationAddition 0.0)
//     W3DSailModelDraw  tables [0xBE1320, 0xBE3B30] (3 rows), createData RW 0x4647CA, size 0x194, constructor RW 0x4CFF2A
//                       (MaxRotationDegrees 0.0, BlowingThresholdDegrees 0.25 (RW 0xBD1904), AboutDamping 0.05 (RW 0xBDD760))
//     W3DQuadrupedDraw  tables [0xBE1320, 0xBE1A80] (4 rows), createData RW 0x464A0F, size 0x198, constructor RW 0x4649B9
//     W3DTankDraw       tables [0xBE1320, 0xBE2968] (5 rows), createData RW 0x464376, size 0x19C, constructor RW 0x4CDEDA
//                       (TreadDebrisLeft "TrackDebrisDirtLeft", TreadDebrisRight "TrackDebrisDirtRight", TreadAnimationRate 0,
//                       TreadPivotSpeedFraction 0.6 (RW 0xBDAD70), TreadDriveSpeedFraction 0.3 (RW 0xBE29D4))
//     W3DSupplyDraw     tables [0xBE1320, 0xBE1E38] (1 row), createData RW 0x464258, size 0x18C, constructor RW 0x4CA6F5
//   TARGET parse functions: 0x42EE5E string, 0x42ED00 real, 0x42EE15 angle, 0x73A4B6 velocity (TreadAnimationRate).
//   DONOR  Open-BFME-2 .../Drawable/Draw/W3DTruckDrawModuleDataCtor.cpp, W3DQuadrupedDrawModuleDataCtor.cpp (member layout and
//          names), ZH W3DTruckDraw.cpp / W3DTankDraw.cpp.
// Retail INI uses Truck (35), Sail (13) and Quadruped (3) draws; Tank and Supply are registered by the binary and parse, no retail
// object declares them (PLAN rule 6: the binary's whole registry).

#pragma once

#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DModelDraw.h"

#include <string>

class W3DTruckDrawModuleData : public W3DModelDrawModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse &p);

	std::string m_dust, m_dirtSpray, m_powerslideSpray;
	std::string m_leftFrontTireBone, m_rightFrontTireBone, m_leftRearTireBone, m_rightRearTireBone;
	std::string m_midLeftFrontTireBone, m_midRightFrontTireBone, m_midLeftRearTireBone, m_midRightRearTireBone;
	std::string m_midLeftMidTireBone, m_midRightMidTireBone;
	std::string m_leftFrontTireBone2, m_rightFrontTireBone2, m_leftRearTireBone2, m_rightRearTireBone2;
	std::string m_midLeftMidTireBone2, m_midRightMidTireBone2;
	std::string m_cabBone, m_trailerBone;
	float m_cabRotationMultiplier = 1.0f, m_trailerRotationMultiplier = 1.0f, m_rotationDamping = 1.0f;
	float m_tireRotationMultiplier = 1.0f, m_powerslideRotationAddition = 0.0f;
};

class W3DSailModelDrawModuleData : public W3DModelDrawModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse &p);

	float m_maxRotation = 0.0f;          ///< MaxRotationDegrees (INI degrees, stored in radians by the angle parser)
	float m_blowingThreshold = 0.25f;    ///< BlowingThresholdDegrees (default stored raw, as the constructor does)
	float m_aboutDamping = 0.05f;        ///< AboutDamping
};

class W3DQuadrupedDrawModuleData : public W3DModelDrawModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse &p);

	std::string m_leftFrontFootBone, m_rightFrontFootBone, m_leftRearFootBone, m_rightRearFootBone;
};

class W3DTankDrawModuleData : public W3DModelDrawModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse &p);

	std::string m_treadDebrisLeft = "TrackDebrisDirtLeft";
	std::string m_treadDebrisRight = "TrackDebrisDirtRight";
	float m_treadAnimationRate = 0.0f;
	float m_treadPivotSpeedFraction = 0.6f;
	float m_treadDriveSpeedFraction = 0.3f;
};

class W3DSupplyDrawModuleData : public W3DModelDrawModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse &p);

	std::string m_supplyBonePrefix;
};

namespace W3DVariantTables
{
const FieldParse *truck();      ///< RW 0xBE22B8, 26 rows
const FieldParse *sail();       ///< RW 0xBE3B30, 3 rows
const FieldParse *quadruped();  ///< RW 0xBE1A80, 4 rows
const FieldParse *tank();       ///< RW 0xBE2968, 5 rows
const FieldParse *supply();     ///< RW 0xBE1E38, 1 row
} // namespace W3DVariantTables

class Xfer;

// Runtime state and logic of W3DTankDraw (Local 88 #714).
// TARGET RW 0x004CD7BC updateTreadPositions, RW 0x004CD830 xfer, RW 0x004CD892 startMoveDebris,
//   RW 0x004CD8DE stopMoveDebris, RW 0x004CD91D setHidden, RW 0x004CD93B setFullyObscuredByShroud,
//   RW 0x004CD95E updateTreadObjects, RW 0x004CDBD2 onRenderObjRecreated, RW 0x004CDBD7 doDrawModule,
//   RW 0x004CDFA8 tossEmitters, RW 0x004CE02B createEmitters, RW 0x004CE1E2 getModuleNameKey,
//   RW 0x004CE227 GetSnapshotName, RW 0x004CE22D loadPostProcess, RW 0x004CE320 ModuleData scalar deleting dtor.
class W3DTankDrawState
{
public:
	enum
	{
		TREAD_LEFT = 0,
		TREAD_RIGHT = 1,
		TREAD_MIDDLE = 2,
		MAX_TREADS_PER_TANK = 4
	};
	struct TreadObjectInfo
	{
		std::string name;
		int type = TREAD_MIDDLE;
		float u = 0.0f; ///< wrapped [0, 1) UV offset (+0x310, stride 0x14)
	};

	explicit W3DTankDrawState(const W3DTankDrawModuleData *data = nullptr);

	// RW 0x004CD95E / RW 0x004CDBD2 onRenderObjRecreated: scans sub-object names for "TREADS" prefix
	// ("TREADSL" -> TREAD_LEFT, "TREADSR" -> TREAD_RIGHT, other "TREADS*" -> TREAD_MIDDLE, up to 4).
	void onRenderObjRecreated(const std::vector<std::string> &subObjectNames);
	void updateTreadObjects(const std::vector<std::string> &subObjectNames);

	// RW 0x004CD7BC: scrolls treads by uvDelta (left += uvDelta, right -= uvDelta), wrapped by u - floor(u)
	void updateTreadPositions(float uvDelta);

	// RW 0x004CD892 / RW 0x004CD8DE
	void startMoveDebris();
	void stopMoveDebris();

	// RW 0x004CD91D: calls W3DModelDraw::setHidden(h) and stopMoveDebris() when h is true
	void setHidden(bool hidden);
	// RW 0x004CD93B: stops debris emitters on transition to fully obscured by shroud
	void setFullyObscuredByShroud(bool fullyObscured);
	// RW 0x004CE22D: loadPostProcess -> tossEmitters + createEmitters
	void loadPostProcess();

	// RW 0x004CDBD7 doDrawModule logic step: updates debris emitter active state, debris velocity multiplier,
	// and tread UV offsets for pivot turning and forward/backward driving.
	void doDrawModule(float currentSpeed, float maxSpeed, int turnBits, bool isMoving, bool isBackingUp,
		float dirX, float dirY, bool airborne = false);

	// RW 0x004CD830: version(1, 1)
	void xfer(Xfer &x);

	bool isHidden() const { return m_hidden; }
	bool isFullyObscuredByShroud() const { return m_fullyObscuredByShroud; }
	bool leftDebrisActive() const { return m_leftDebrisActive; }
	bool rightDebrisActive() const { return m_rightDebrisActive; }
	bool emittersCreated() const { return m_emittersCreated; }
	const Coord3D &debrisVelocityMultiplier() const { return m_debrisVelMult; }
	int treadCount() const { return (int)m_treads.size(); }
	const std::vector<TreadObjectInfo> &treads() const { return m_treads; }

private:
	const W3DTankDrawModuleData *m_data = nullptr;
	bool m_hidden = false;
	bool m_fullyObscuredByShroud = false;
	bool m_emittersCreated = false;
	bool m_leftDebrisActive = false;  ///< +0x2E8
	bool m_rightDebrisActive = false; ///< +0x2F4
	Coord3D m_debrisVelMult{ 1.0f, 1.0f, 1.0f };
	float m_lastDirX = 1.0f;          ///< +0x358
	float m_lastDirY = 0.0f;          ///< +0x35C
	std::vector<TreadObjectInfo> m_treads; ///< +0x304..+0x354
};

// Runtime state and logic of W3DTruckDraw (Local 88 #714).
// TARGET RW 0x004CA936 xfer, RW 0x004CAD65 getModuleNameKey, RW 0x004CADAA GetSnapshotName,
//   RW 0x004CADB0 tossEmitters, RW 0x004CAE6F createEmitters, RW 0x004CB013 enableEmitters,
//   RW 0x004CB0B8 updateBones, RW 0x004CBF6E setHidden, RW 0x004CBF8E onRenderObjRecreated,
//   RW 0x004CD634 loadPostProcess, RW 0x004CD6F7 setFullyObscuredByShroud, RW 0x004CD721 scalar deleting dtor.
class W3DTruckDrawState
{
public:
	explicit W3DTruckDrawState(const W3DTruckDrawModuleData *data = nullptr);

	// RW 0x004CB013
	void enableEmitters(bool enable);
	// RW 0x004CADB0 / RW 0x004CAE6F
	void tossEmitters();
	void createEmitters();

	// RW 0x004CBF6E (DONOR BFME2 0x000CC781, W3DTruckDrawSetHidden.cpp): sets hidden and calls enableEmitters(false) when hidden
	void setHidden(bool hidden);
	// RW 0x004CD6F7: tosses or creates emitters when shroud state changes
	void setFullyObscuredByShroud(bool fullyObscured);
	// RW 0x004CD634: calls W3DModelDraw::loadPostProcess and tossEmitters
	void loadPostProcess();
	// RW 0x004CBF8E: zeroes cached render object and all 16 wheel/cab/trailer bone indices (+0x320..+0x35C), then runs updateBones (RW 0x004CB0B8)
	void onRenderObjRecreated(int foundBoneCount = 0);

	// RW 0x004CA936 (DONOR BFME2 W3DTruckDrawXfer.cpp): version(1, 1), xfers +0x2E9 and +0x2EA
	void xfer(Xfer &x);

	bool isHidden() const { return m_hidden; }
	bool isFullyObscuredByShroud() const { return m_fullyObscuredByShroud; }
	bool emittersEnabled() const { return m_emittersEnabled; }
	bool emittersCreated() const { return m_emittersCreated; }
	bool effectsInitialized() const { return m_effectsInitialized; }
	bool wasAirborne() const { return m_wasAirborne; }
	void setEffectsState(bool initialized, bool airborne) { m_effectsInitialized = initialized; m_wasAirborne = airborne; }
	int resolvedBoneCount() const { return m_resolvedBoneCount; }
	unsigned boneUpdateCount() const { return m_boneUpdateCount; }

private:
	const W3DTruckDrawModuleData *m_data = nullptr;
	bool m_hidden = false;
	bool m_fullyObscuredByShroud = false;
	bool m_emittersCreated = true;
	bool m_emittersEnabled = false;
	bool m_effectsInitialized = false; ///< +0x2E9
	bool m_wasAirborne = false;        ///< +0x2EA
	int m_resolvedBoneCount = 0;       ///< +0x320..+0x35C
	unsigned m_boneUpdateCount = 0;
};
