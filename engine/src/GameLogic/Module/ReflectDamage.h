// OpenBFME. GPL-3.0.
//
// ReflectDamage (lane MODGAP-1, group 6, stop S-140 -> ported): the Witch-king's and the Nazgul's thorns, a share of the damage they take is dealt back to the attacker.
// Ported from the RotWK binary (caveat S-001); the BFME2 decompilation (Open-BFME-2 Code/GameEngine/Source/GameLogic/Object/Damage/ReflectDamageOnDamage.cpp, a byte-matched
// C++ body) gave the flow and the RotWK body was read in game.dat.
//
// TARGET FACTS (RotWK game.dat, each read from the disassembly):
//   * data (ctor RW 0x8BECE3, parse RW 0x8BEC6E over the table RW 0xC70928): DamageTypesToReflect + 0x08 (RW 0x73A5C9, the damage type flags 1 << (type - 1); starts 0),
//     ReflectDamagePercentage + 0x0C (RW 0x42EEFA, a percent as a real; starts 0), MinimumDamageToReflect + 0x10 (RW 0x42ED00; starts 0). No upgrade fields;
//   * module (constructor RW 0x8BECFD over DamageModule RW 0x8BD869; vtables RW 0xC70974, damage interface + 0xC RW 0xC704B0, + 0x10 RW 0xC70968): no state;
//   * onDamage (RW 0x8BED43, damage interface slot 0), the DamageInfo the body hands it: damage type 13 (REFLECTED, what this module deals) is never reflected; a type
//     whose bit (type - 1, the shift count masked to 5 bits) is not in DamageTypesToReflect is not; the source object (DamageInfo + 0x08, TheGameLogic's lookup RW 0x449681)
//     must still exist. The amount is the INPUT amount (DamageInfo + 0x20, before the armour) times ReflectDamagePercentage (mulss), raised to MinimumDamageToReflect
//     when not above it (comiss; ja). The source takes attemptDamage (RW 0x698E7D) of a fresh DamageInfo (RW 0x66365E) with type 13, + 0x14 = 2, death type (+ 0x1C) = 2
//     and the source id of the thorned object's own id (Object + 0x74);
//   * onHealing and onBodyDamageStateChange do nothing; xfer: DamageModule's (version 1 and BehaviorModule's), no data of its own.
// Not ported: nothing of the module.

#pragma once

#include "Common/INI.h"
#include "GameLogic/Module/DamageModule.h"

class ModuleFactory;

class ReflectDamageModuleData : public ModuleData
{
public:
	unsigned m_damageTypes = 0;        // + 0x08
	float m_reflectPercentage = 0.0f;  // + 0x0C
	float m_reflectMinimum = 0.0f;     // + 0x10
	static void buildFieldParse(MultiIniFieldParse &p); // RW 0x8BEC6E
};

class ReflectDamage : public BehaviorModule, public DamageModuleInterface
{
public:
	OBFME_MODULE_XFER(ReflectDamage)
	ReflectDamage(Thing *thing, const ReflectDamageModuleData *data); // RW 0x8BECFD
	DamageModuleInterface *getDamage() override { return this; }
	void onDamage(const DamageInfo &info) override;                                                        // RW 0x8BED43
	void onBodyDamageStateChange(const DamageInfo *, BodyDamageType, BodyDamageType) override {}          // a stub there
	const ReflectDamageModuleData *data() const { return m_data; }
	static void registerClass(ModuleFactory &modules);

private:
	const ReflectDamageModuleData *m_data;
};
