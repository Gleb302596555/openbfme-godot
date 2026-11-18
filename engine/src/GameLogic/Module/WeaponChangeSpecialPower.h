// OpenBFME. GPL-3.0.
//
// WeaponChangeSpecialPowerModule (lane FACT-B): a special power that toggles weapon set flags, the Isengard Berserker's Light Torch.
//
// TARGET FACTS (RotWK game.dat, caveat S-001):
//   * data: the SpecialPowerModuleData base (+0x00 .. +0x7B) and the table RW 0xC73D70: FlagsUsedForToggle (+0x7C, the weapon condition bit list
//     RW 0x6C9951, 128 bits), ToggleOnSleepFrames (+0x8C) and ToggleOffSleepFrames (+0x90) (both parseInt RW 0x42EC5E), ToggleOnAttributeModifier
//     (+0x94) and ToggleOffAttributeModifier (+0x98) (both parseAsciiString RW 0x42EE5E). Constructor RW 0x8C833F (tier A).
//   * doSpecialPower (RW 0x8C8420, RotWK's own body: BFME2's 0x4C40BA is the string-anchored counterpart only): a mask of the model conditions
//     MOVING, ATTACKING, FIRING_OR_PREATTACK_A, FIRING_OR_PREATTACK_B is built (static at RW 0xDE9C38); the object is a horde when its template has
//     KindOf HORDE (template + 0x115 bit 5). For every bit 0 .. 0x67 of FlagsUsedForToggle: an object that has the weapon set flag (RW 0x68DEEC) clears
//     it (RW 0x691106), and a horde clears it on each member of its contain (vslot 0x108); else the flag is set (RW 0x691059), the mask's model
//     conditions cleared twice (RW 0x5E3B79 -> clearAndSet RW 0x68D607 with an empty set), "turned on" noted, and a horde does the same on each member.
//     Turned on: the ToggleOn attribute modifier with ToggleOnSleepFrames as its duration (RW 0x68F1A8) when the name is not empty and the frames
//     not 0, then disabled type 4 until now + ToggleOnSleepFrames (RW 0x6907F1) when the frames are > 0, then the special model condition 0x149
//     for ToggleOnSleepFrames frames (RW 0x68B581); a horde repeats each on its members. Turned off: the ToggleOff modifier and the disabled frames
//     the same way with the Off fields, no special model condition. Then the SpecialPowerModule base (RW 0x8980A3).
//   * the target / location forms are the base class's (the button has no target).
// Port 256 (fork issue 604): the class's own map rows that carried no citation, classified by reading them (RW_FUNCTION_MAP depth 0):
//   0x8C82AD / 0x8C83BA  the scalar deleting destructors of the module (RW 0x8C82C9, the three vtable stores, then operator delete RW 0x42F6A0) and of the data
//                       (RW 0x8C83D6) -- the port's implicit destructors;
//   0x8C82A7 / 0x8C82FA  the static type descriptor accessor (returns RW 0xC0A8A0) and its one-shot initializer (RW 0x5487EC): no game code calls them;
//   0x8C6583            jmp RW 0x8972B2, the base module's slot (already cited from the base);
//   0x8C6606            the xfer override: version (1, 1) RW 0x404F15 then the base RW 0x89679D -- the port's xfer();
//   0x8C82E2 / 0x8C82EE  the at-object and at-location forwarders onto slot 0x28 (RW 0x8C8420) -- the port's base class forwarders doSpecialPowerAtObject /
//                       doSpecialPowerAtLocation, which the class does not override;
//   0x4B612F            a static helper of the module data that builds a string from the constant RW 0xDC62B8 (RW 0x435F30) and returns its argument;
//   0x464A09            returns the constant RW 0xBDBA8C, the SAME function in the other ModuleData table RW 0xC651C0: a shared data-class helper, not this class's.

#pragma once

#include "GameLogic/BitFlags.h"
#include "GameLogic/Module/SpecialPowerModules.h"

#include <string>
#include <vector>

class ModuleFactory;

class WeaponChangeSpecialPowerModuleData : public SpecialPowerModuleData
{
public:
	WeaponConditionFlags m_flagsUsedForToggle{}; ///< +0x7C
	int m_toggleOnSleepFrames = 0;              ///< +0x8C
	int m_toggleOffSleepFrames = 0;             ///< +0x90
	std::string m_toggleOnAttributeModifier;    ///< +0x94
	std::string m_toggleOffAttributeModifier;   ///< +0x98
	static void buildFieldParse(MultiIniFieldParse &p);
};

class WeaponChangeSpecialPowerModule : public SpecialPowerModule
{
public:
	OBFME_MODULE_XFER(WeaponChangeSpecialPowerModule) // lane SAVE-4b: SpecialPowerModule's
	WeaponChangeSpecialPowerModule(Thing *thing, const WeaponChangeSpecialPowerModuleData *data) : SpecialPowerModule(thing, data), m_data(data) {}
	void doSpecialPower(unsigned options) override; // RW 0x8C8420

	static void registerClass(ModuleFactory &modules);

private:
	const WeaponChangeSpecialPowerModuleData *m_data;
};
