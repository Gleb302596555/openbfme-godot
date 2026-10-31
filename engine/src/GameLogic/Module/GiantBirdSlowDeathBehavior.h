// OpenBFME. GPL-3.0.
//
// GiantBirdSlowDeathBehavior (lane MODGAP-1, MODULE_GAPS.md group 8; no retail 2.01 template uses it, Ennorath's eagles, fell beasts and dragons do): a killed flyer crashes along its flight
// to a spot clear of CrashAvoidKindOfs. Ported from the RotWK binary (caveat S-001); the BFME2 decompile Object/Behavior/GiantBirdSlowDeathBehavior.cpp names the pieces.
//
// TARGET FACTS (RotWK game.dat):
//   create RW 0x64B282, 0x68 bytes, constructor RW 0x864F4F over SlowDeathBehavior RW 0x8604E2; data constructor RW 0x865076: CrashAvoidRadius 800 (RW 0xBE94A8), CrashAvoidStrength 0.1
//   (RW 0xBD83D4). Tables RW 0xC58A98, 0xC76BD8, 0xC59430: FXHitGround + 0x190, OCLHitGround + 0x194, DelayFromGroundToFinalDeath + 0x198, CrashAvoidKindOfs + 0x19C, CrashAvoidRadius
//   + 0x1B8, CrashAvoidStrength + 0x1BC, NeedToMaintainFlailingHeight + 0x1C0.
//   * update RW 0x865009: the physics' bounced flag (+ 0x5E) sets the module's + 0x40 (a HIT_GROUND phase is due); while activated, SlowDeathBehavior's update RW 0x860B39; answers 1;
//   * beginSlowDeath RW 0x86511C: the base's RW 0x860E93. The target starts at the object's position. With an AI: MOVING (0x3D) and a giant bird's flight (AI slot 0x188; none:
//     nothing more happens, RW 0x865186): speed = aerial speed * 5.0 (RW 0xBDAE58); above 50 (RW 0xBD88C4) the target moves along the facing (cos x, sin y, msvcr71) by it and the
//     physics may bounce (+ 0x5D). Then RW 0x66A61A moves it clear of CrashAvoidKindOfs. z = the terrain's height, kept at + 0x54; physics flyTo(target, apex 0, speed 30 (RW 0xBDAE54)).
//   * RW 0x66A61A (AI, radius, in, out): r = max(10, the object's bounding circle radius); the objects within `radius` of `in` (2D, near to far) of every kind of the mask (RW 0x70C4FE);
//     for R = r, 2r, ... up to `radius`, and the angle from -PI to PI by r / R (x87-free SSE: 2PI / (R * 2PI * (1 / r))): the point in + (sin, cos) * R (RW 0x66A704: x takes the sine) with
//     no hit whose circle (its radius + r) strictly contains it is the answer; none: `in` stays.
// NOT READ by these functions: FXHitGround, OCLHitGround, DelayFromGroundToFinalDeath, CrashAvoidStrength, NeedToMaintainFlailingHeight (stop S-5606). The data keeps them.
// The snapshot is OpenBFME native (S-4190).

#pragma once

#include "GameLogic/Module/CombatModules.h"

#include <string>

class ModuleFactory;

class GiantBirdSlowDeathBehaviorModuleData : public SlowDeathBehaviorModuleData
{
public:
	std::string m_fxHitGround;                 ///< + 0x190
	std::string m_oclHitGround;                ///< + 0x194
	float m_delayFromGroundToFinalDeath = 0.0f; ///< + 0x198
	KindOfMaskType m_crashAvoidKindOfs{};      ///< + 0x19C
	float m_crashAvoidRadius = 800.0f;         ///< + 0x1B8
	float m_crashAvoidStrength = 0.1f;         ///< + 0x1BC
	bool m_needToMaintainFlailingHeight = false; ///< + 0x1C0
	static void buildFieldParse(MultiIniFieldParse &p);
};

class GiantBirdSlowDeathBehavior : public SlowDeathBehavior
{
public:
	OBFME_MODULE_XFER(GiantBirdSlowDeathBehavior)
	GiantBirdSlowDeathBehavior(Thing *thing, const GiantBirdSlowDeathBehaviorModuleData *data);
	UpdateSleepTime update() override; // RW 0x865009
	void crc(StateHasher &hasher) const override;

	const Coord3D &crashTarget() const { return m_crashTarget; }
	// RW 0x66A61A: true and `out` when a clear point exists
	static bool findClearPoint(const Object &obj, float radius, const Coord3D &in, Coord3D &out, const KindOfMaskType &kinds);
	static const char *stopLine();
	static void registerClass(ModuleFactory &modules);

protected:
	void beginSlowDeath(const DieModuleInterface::Event &event) override; // RW 0x86511C

private:
	const GiantBirdSlowDeathBehaviorModuleData *m_bird;
	Coord3D m_crashTarget{ 0.0f, 0.0f, 0.0f }; ///< + 0x54
};
