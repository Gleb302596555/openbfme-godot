// OpenBFME. GPL-3.0.
//
// EvacuateDamage (lane MODGAP-1, group 6, stops S-1995 / S-5602): a garrison that runs out of its building when a named siege weapon hits it or when it takes too much
// damage in a short time (the Mordor catapult's human heads, the walls of the citadels and expansions). Ported from the RotWK binary (caveat S-001); the BFME2
// decompilation (Open-BFME-2 Damage/EvacuateDamage*.cpp: summaries, the bodies are not byte-matched) names the pieces.
//
// TARGET FACTS (RotWK game.dat, each read from the disassembly):
//   * data (ctor RW 0x8BEE59, table RW 0xC70A88): WeaponThatCausesEvacuation + 0x08 (parse RW 0x8BEDF8: an AsciiString, set only when TheWeaponStore knows the name,
//     RW 0x6CC5DF; empty otherwise), DamageTypeToTrack + 0x0C (RW 0x42E956 over the damage names, default 3 = PIERCE), DamageToPanicThreshold + 0x10 (a percent as a real,
//     default 0.1 = RW 0xBD83D4), TrackingTimeSpan + 0x14 (RW 0x73A429, frames, default 6);
//   * module (ctor RW 0x8BEF66 over DamageModule; vtables RW 0xC70AE4, damage interface RW 0xC70AD8): + 0x14 a list of {amount, frame};
//   * onDamage (RW 0x8BF12E): the source object (DamageInfo + 0x08) must exist and have a current weapon (RW 0x68B58C(0)). The data's weapon name equal to the weapon
//     template's name (RW 0x4065AA, a compare that answers 0 for equal): the object's contain (Object + 0x258) takes every rider out at once (the contain's slot 0x74 is the
//     exit interface, never null in the contain tables read: RW 0x867A94): a HORDE rider aiHordeExit (RW 0x775AFB), any other aiExit (RW 0x7716C1), both with the
//     command source 2 (CMD_FROM_AI), the riders' AIs unchecked. Another weapon: when the damage type (DamageInfo + 0x10) is DamageTypeToTrack the damage is tracked;
//   * tracking (RW 0x8BF09E): a body (Object + 0x25C) is needed; {the INPUT amount (DamageInfo + 0x20), the logic frame} is appended to the list; the list is summed
//     (RW 0x8BEF0A, SSE adds from 0) over the entries whose frame is above now - TrackingTimeSpan, older ones are erased there; the sum below the body's max health
//     (body slot 0x1C, RW 0x6AA274: the float at + 0x10; INFERENCE: ActiveBody's max health) times DamageToPanicThreshold ends it; otherwise, with the source object still
//     alive, the panic (RW 0x8BF076): with an AI the list is cleared and AI command 0x2E is given with the source object and the command source 2 (RW 0x881FF3);
//   * onHealing and onBodyDamageStateChange do nothing. xfer: OpenBFME native layout (S-4190): the module's version, BehaviorModule's, the list.
// NOT PORTED (stop S-5602): AI command 0x2E is not identified in the port (INFERENCE: ZH's AICMD_PANIC, which in BFME takes the attacker); the panic clears the list and
// notes the stop, the garrison stays inside.

#pragma once

#include "Common/INI.h"
#include "GameLogic/Module/DamageModule.h"

#include <vector>

class ModuleFactory;
class StateHasher;

class EvacuateDamageModuleData : public ModuleData
{
public:
	std::string m_weaponName;         // + 0x08
	int m_damageTypeToTrack = 3;      // + 0x0C
	float m_panicThreshold = 0.1f;    // + 0x10
	unsigned m_trackingTimeSpan = 6;  // + 0x14 frames
	static void buildFieldParse(MultiIniFieldParse &p); // RW 0x8BEEAD
};

class EvacuateDamage : public BehaviorModule, public DamageModuleInterface
{
public:
	struct Entry
	{
		float amount = 0.0f;
		int frame = 0;
	};
	OBFME_MODULE_XFER(EvacuateDamage)
	EvacuateDamage(Thing *thing, const EvacuateDamageModuleData *data); // RW 0x8BEF66
	DamageModuleInterface *getDamage() override { return this; }
	void crc(StateHasher &h) const override;
	void onDamage(const DamageInfo &info) override;                                               // RW 0x8BF12E
	void onBodyDamageStateChange(const DamageInfo *, BodyDamageType, BodyDamageType) override {} // a stub there
	const std::vector<Entry> &tracked() const { return m_tracked; }
	static const char *stopLine();
	static void registerClass(ModuleFactory &modules);

private:
	void evacuateAll();                    // RW 0x8BF178
	void trackDamage(const DamageInfo &);  // RW 0x8BF09E
	float windowedSum();                   // RW 0x8BEF0A
	void panic(Object &source);            // RW 0x8BF076
	const EvacuateDamageModuleData *m_data;
	std::vector<Entry> m_tracked;          // + 0x14
};
