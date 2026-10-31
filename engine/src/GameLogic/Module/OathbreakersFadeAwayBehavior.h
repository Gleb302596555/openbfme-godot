// OpenBFME. GPL-3.0.
// Both editions: RW 0x8BCBD0 / 0x8BCC50 / 0x8BCC82, BFME2 0x8B8A8A /
// 0x8B8B0F / 0x8B8B41 (Rva004B89C4Update.cpp, Rva004B8B0F.cpp).
#pragma once
#include "Common/INI.h"
#include "GameLogic/Module/UpdateModule.h"

class ModuleFactory;
class OathbreakersFadeAwayBehaviorModuleData : public ModuleData
{
public:
	unsigned m_fadeOutTime = 1;
	static void buildFieldParse(MultiIniFieldParse &p);
};

class OathbreakersFadeAwayBehavior : public UpdateModule
{
public:
	OBFME_MODULE_XFER(OathbreakersFadeAwayBehavior)
	OathbreakersFadeAwayBehavior(Thing *thing, const OathbreakersFadeAwayBehaviorModuleData *data);
	UpdateSleepTime update() override;
	void crc(StateHasher &h) const override;
	bool fadingStarted() const { return m_started; }
	float opacity() const { return m_fraction; }
	static const char *hordeStop();
	static void registerClass(ModuleFactory &modules);
private:
	const OathbreakersFadeAwayBehaviorModuleData *m_data;
	bool m_started = false;
	float m_fraction = 1.0f;
};
