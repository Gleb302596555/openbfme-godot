// OpenBFME. GPL-3.0.
//
// SlaveWatcherBehavior (lane MODGAP-1, group 4, stop S-1995): the master of a structure's slave (the wall trebuchet, the ballista) watches it. Ported from the RotWK
// binary (caveat S-001); the BFME2 decomp (SlaveWatcherBehavior.cpp, bodies are summaries) names the data and the layout.
//
// TARGET FACTS (RotWK game.dat, each read from the disassembly):
//   * data (ctor RW 0x887953, vtable RW 0xC60A88): GrantUpgrade (+8) and RemoveUpgrade (+0xC) names, ShareUpgrades (+0x10) and LetSlaveLive (+0x11) bytes; the table
//     is BFME2's (INI table 0xC4A208 there); INFERENCE: RotWK's names are the same (the Ennorath corpus loads with no error);
//   * module (ctor RW 0x887860, vtables RW 0xC60A54 / 0xC67300 / 0xC60A48): UpdateModule, + 0x20 the slave's id (0), + 0x24 the last mask shared (0x24 dwords,
//     cleared), woken in 1 frame;
//   * setSlave RW 0x887AF5 (called by ObjectCreationUpgrade's ThingToSpawn path, RW 0x8B86AB: the object it made): + 0x20 = the id; with ShareUpgrades the slave
//     object is found and the owner's whole upgrade mask (object + 0x28C) is copied to + 0x24 and OR-ed into the slave's (RW 0x68CC6C), then the slave's upgrade
//     modules run (Object::updateUpgradeModules, RW 0x6936FE); then the module wakes (setWakeFrame 1, RW 0x850C32);
//   * update RW 0x887A35: no slave: stay awake (sleep NONE). The slave exists and is alive (not effectively dead, + 0x458 bit 0): with ShareUpgrades, a mask of the
//     owner that differs from the one shared before (RW 0x862520) is copied, OR-ed into the slave's and the slave's upgrade modules run; stay awake. The slave
//     gone or dead: RemoveUpgrade is taken from the owner (Object::removeUpgrade RW 0x691438) and GrantUpgrade given (RW 0x69388B), the owner's upgrade modules run
//     (RW 0x6936FE), the slave id is cleared and the module sleeps for ever (RW 0x887AEA);
//   * the destructor RW 0x8879C5: without LetSlaveLive and with a slave id, the slave is found and killed (RW 0x698EC3 with damage type 0, death type 13).
//     INFERENCE: the port runs it from Module::onDelete (the object's destruction), the retail one from the module's destructor;
//   * xfer: not read (OpenBFME native layout: the id and the shared mask).
// NOT PORTED: nothing of the module itself; ObjectCreationUpgrade's DestroyWhenSold reads slave() (RW 0x8B87C5).

#pragma once

#include "Common/INI.h"
#include "Common/Upgrade.h"
#include "GameLogic/Module/UpdateModule.h"
#include "GameLogic/ObjectTypes.h"

#include <string>

class ModuleFactory;
class StateHasher;

class SlaveWatcherBehaviorModuleData : public ModuleData
{
public:
	std::string m_grantUpgrade;       // +8
	std::string m_removeUpgrade;      // +0xC
	bool m_shareUpgrades = false;     // +0x10
	bool m_letSlaveLive = false;      // +0x11
	static void buildFieldParse(MultiIniFieldParse &p);
};

class SlaveWatcherBehavior : public UpdateModule
{
public:
	OBFME_MODULE_XFER(SlaveWatcherBehavior)
	SlaveWatcherBehavior(Thing *thing, const SlaveWatcherBehaviorModuleData *data); // RW 0x887860
	UpdateSleepTime update() override;                                              // RW 0x887A35
	void onDelete() override;                                                       // RW 0x8879C5 (the destructor's kill)
	void crc(StateHasher &h) const override;

	void setSlave(ObjectID id); // RW 0x887AF5
	ObjectID slave() const { return m_slave; }
	const SlaveWatcherBehaviorModuleData *data() const { return m_data; }
	static void registerClass(ModuleFactory &modules);

private:
	const SlaveWatcherBehaviorModuleData *m_data;
	ObjectID m_slave = INVALID_ID;   // + 0x20
	UpgradeMaskType m_shared;        // + 0x24
};
