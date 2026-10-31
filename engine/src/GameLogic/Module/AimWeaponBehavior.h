// OpenBFME. GPL-3.0.
//
// AimWeaponBehavior (lane MODGAP-1, MODULE_GAPS.md group 11): while the object attacks with an aiming weapon, the model conditions AIM_HIGH / STRAIGHT / LOW and AIM_NEAR / FAR follow the target, so
// the drawable plays the right aim. Ported from the RotWK binary (caveat S-001); the BFME2 decompile Object/Behavior/AimWeaponBehavior.cpp has the data and the xfer.
//
// TARGET FACTS (RotWK game.dat):
//   create RW 0x64ADAC, 0x24 bytes, constructor RW 0x85E29A (+ 0x20 = 0, wake next frame RW 0x850C32(1)); table RW 0xC58178: AimLowThreshold + 0x08, AimHighThreshold + 0x0C, AimNearDistance
//   + 0x10, AimFarDistance + 0x14. The BFME2 constructor's defaults: -0.05, 0.05, 0, FLT_MAX. xfer: version 1, UpdateModule, the bool at + 0x20.
//   * update RW 0x85E30B: no AI, drawable (RW 0x70E013) or current weapon (RW 0x68B58C(0)): sleeps for ever. Attacking = the AI's slot 0x1BC (RW 0x662B3C: the state's isAttack) or the
//     weapon in its window (RW 0x6CA2C3: when pre-attack finished <= frame < follow through end). A weapon with IsAimingWeapon (template + 0x156) and attacking: delta = the fire target
//     (Object + 0x3A8, RW 0x69213E's record) - the position; d = sqrt(dx * dx + dy * dy) in x87, at least 10 (RW 0xBD83D8); slope = dz / d: above AimHighThreshold AIM_HIGH (0x40000),
//     below AimLowThreshold AIM_LOW (0x100000), else AIM_STRAIGHT (0x80000), that bit set (nothing cleared here: INFERENCE below) with + 0x20 = 1; d below AimNearDistance: AIM_FAR (0xB6)
//     cleared and AIM_NEAR (0xB5) set; above AimFarDistance: the reverse (RW 0x68D607); the drawable is flushed (RW 0x67449C(0)).
//     Then with + 0x20 set and not attacking: + 0x20 = 0, AIM_HIGH, STRAIGHT, LOW (0xB2, 0xB3, 0xB4), NEAR and FAR cleared (RW 0x4B5AA9 / 0x5E3B79), flushed. Answers 1 frame.
// INFERENCE: a new slope sets its bit while the old one stays until the attack ends: RW 0x85E43D only ors (read as written). The drawable test is the client's: a headless logic has one for
// every object in RW, so the port does not ask it (S-5607).

#pragma once

#include "Common/INI.h"
#include "GameLogic/Module/UpdateModule.h"

class ModuleFactory;
class StateHasher;

class AimWeaponBehaviorModuleData : public ModuleData
{
public:
	float m_aimLowThreshold = -0.05f;   ///< + 0x08
	float m_aimHighThreshold = 0.05f;   ///< + 0x0C
	float m_aimNearDistance = 0.0f;     ///< + 0x10
	float m_aimFarDistance = 3.40282347e+38f; ///< + 0x14 (FLT_MAX)
	static void buildFieldParse(MultiIniFieldParse &p);
};

class AimWeaponBehavior : public UpdateModule
{
public:
	OBFME_MODULE_XFER(AimWeaponBehavior)
	AimWeaponBehavior(Thing *thing, const AimWeaponBehaviorModuleData *data); // RW 0x85E29A
	UpdateSleepTime update() override;                                        // RW 0x85E30B
	void crc(StateHasher &hasher) const override;

	bool aiming() const { return m_aiming; }
	static void registerClass(ModuleFactory &modules);

private:
	const AimWeaponBehaviorModuleData *m_data;
	bool m_aiming = false; ///< + 0x20
};
