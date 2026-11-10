// OpenBFME. GPL-3.0.
//
// DamageFieldUpdate (lane LOCAL-36, issue #286): Easterling fortress damage field, etc.
//
// TARGET FACTS (RotWK game.dat, caveat S-001):
//   * data (field table RW 0xC64018, after FireWeaponUpdateModuleData base):
//     Radius (+0x10, parseReal RW 0x42EC5E),
//     ObjectFilter (+0x14, parseObjectFilter RW 0x76392F),
//     RequiredUpgrade (+0x18, parseAsciiString RW 0x42EE5E).
//     Ctor RW 0x89478E (calls FireWeaponUpdateModuleData ctor RW 0x88F26F). Factory RW 0x659082.
//   * update (RW 0x8949A2; DONOR BFME2 0x891316 / DamageFieldUpdateRva004911AE.cpp:195):
//     - If RequiredUpgrade is set and owner lacks it: returns UPDATE_SLEEP_NONE (RW 0x8949FD).
//     - Scans partition within Radius (2D) for alive, non-self objects passing ObjectFilter
//       with controlling player (RW 0x894A05..0x894A93);
//     - Newly entering victims (in range but not in m_victims) are fired upon via
//       weaponHost().fireExtraWeapon(*w, *other) (RW 0x89483A) and added to m_victims;
//     - Exited objects are removed from m_victims;
//     - Returns nextSleep() (RW 0x88F0AA / RW 0x894BF5).
//   * xfer (RW 0x894C52; DONOR BFME2 0x89157D):
//     Version (1, 1), xferSTLObjectIDList (m_victims), FireWeaponUpdate::xfer(x).

#pragma once

#include "GameLogic/Module/SpellEffectModules.h"
#include "GameLogic/ObjectFilter.h"

#include <string>
#include <vector>

class ModuleFactory;

class DamageFieldUpdateModuleData : public FireWeaponUpdateModuleData
{
public:
	float m_radius = 0.0f;         ///< +0x10 Radius
	ObjectFilter m_objectFilter;   ///< +0x14 ObjectFilter
	std::string m_requiredUpgrade; ///< +0x18 RequiredUpgrade
	static void buildFieldParse(MultiIniFieldParse &p);
};

class DamageFieldUpdate : public FireWeaponUpdate
{
public:
	OBFME_MODULE_XFER(DamageFieldUpdate) // lane SAVE-4b: version 1.1 + victims list + FireWeaponUpdate
	DamageFieldUpdate(Thing *thing, const DamageFieldUpdateModuleData *data);
	~DamageFieldUpdate() override;

	UpdateSleepTime update() override; // RW 0x8949A2
	void crc(StateHasher &hasher) const override;

	const std::vector<ObjectID> &victims() const { return m_victims; }

	static void registerClass(ModuleFactory &modules);

private:
	void fireExtraWeapon(Object *other); // RW 0x89483A

	const DamageFieldUpdateModuleData *m_data = nullptr;
	std::vector<ObjectID> m_victims; // +0x2C in retail
};
