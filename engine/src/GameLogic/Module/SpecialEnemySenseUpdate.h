// OpenBFME. GPL-3.0.
// Both editions: RW 0x89D758 / 0x89D7E6; BFME2 0x899FA9 / 0x89A037
// (Object/Update/SpecialEnemySenseUpdate.cpp, RVAs 0x499FA9 / 0x49A037).
#pragma once

#include "GameLogic/Module/UpdateModule.h"
#include "GameLogic/ObjectFilter.h"

class ModuleFactory;

class SpecialEnemySenseUpdateModuleData : public ModuleData
{
public:
	ObjectFilter m_filter;
	float m_scanRange = 0.0f;
	unsigned m_scanInterval = 1;
	static void buildFieldParse(MultiIniFieldParse &p);
};

class SpecialEnemySenseUpdate : public UpdateModule
{
public:
	OBFME_MODULE_XFER(SpecialEnemySenseUpdate)
	SpecialEnemySenseUpdate(Thing *thing, const SpecialEnemySenseUpdateModuleData *data);
	UpdateSleepTime update() override;
	static void registerClass(ModuleFactory &modules);
private:
	const SpecialEnemySenseUpdateModuleData *m_data;
};
