// OpenBFME. GPL-3.0.
//
// HordeWorkerAIUpdate (lane MODGAP-2; editions: both RotWK 2.01 and BFME2 1.06):
// AI update for worker hordes (e.g. Rohan Peasant hordes that can build and repair structures).
// Overrides HordeAIUpdate to handle repair commands (0x13, 0x42) and direct contained workers.
//
// TARGET FACTS:
// - Registry: RW 0x64E248 / BFME2 0x24E560 ("HordeWorkerAIUpdate",
//   type MODULETYPE_BEHAVIOR = 1, interface mask 17 = 0x11).
// - HordeWorkerAIUpdateModuleData:
//   shares AIUpdateModuleData (RW 0x653F70 / BFME2 0x2541A3, table 0xC0F530).
// - HordeWorkerAIUpdate:
//   ctor RW 0x64EE0E / BFME2 0x49AD15,
//   dtor RW 0x49AC94 / BFME2 0x49AC94,
//   update RW 0x89EC9A / BFME2 0x49AEA4,
//   xfer RW 0x89EB64 / BFME2 0x49AE07,
//   privateRepair RW 0x89ECA0 / BFME2 0x49AFFC,
//   bfmePrivateCommand42 RW 0x89ED20 / BFME2 0x49B1CB.

#pragma once

#include "GameLogic/Module/HordeAIUpdate.h"

class ModuleFactory;

class HordeWorkerAIUpdate : public HordeAIUpdate
{
public:
	HordeWorkerAIUpdate(Thing *thing, const ModuleData *data);
	~HordeWorkerAIUpdate() override = default;

	UpdateSleepTime update() override;

	void privateRepair(Object *obj, CommandSourceType cmdSource);
	void bfmePrivateCommand42(Object *obj, CommandSourceType cmdSource);
	void commandAccepted(CommandSourceType source, int command, Object *target) override;

	void crc(StateHasher &h) const override;
	void xfer(Xfer &x) override;

	static void registerClass(ModuleFactory &modules);

private:
	ObjectID m_pendingRepairTarget = 0; // +0x3E8
	ObjectID m_activeRepairTarget = 0;  // +0x3EC
	ObjectID m_command42Target = 0;     // +0x3F0
	bool m_flag = false;                // +0x3F4
};

