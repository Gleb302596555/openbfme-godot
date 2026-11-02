// OpenBFME. GPL-3.0.
// SpawnUnitBehavior: both editions (RW 0x8BC9B0 / BFME2 0x89ECC0), stop S-207.
// Building spawns unit on production / command button trigger when command points allow.

#pragma once

#include "Common/INI.h"
#include "Common/StateHash.h"
#include "GameLogic/Module/BehaviorModule.h"
#include "GameLogic/Module/UpdateModule.h"

class ModuleFactory;

class SpawnUnitBehaviorModuleData : public ModuleData
{
public:
	std::string m_unitName;    ///< RW data +0x08, default empty
	std::string m_unitCommand; ///< RW data +0x0C, default empty
	bool m_spawnOnce = false;  ///< RW data +0x1C
	static void buildFieldParse(MultiIniFieldParse &p);
};

class SpawnUnitBehavior : public UpdateModule, public SpawnUnitBehaviorInterface
{
public:
	SpawnUnitBehavior(Thing *thing, const SpawnUnitBehaviorModuleData *data);

	UpdateSleepTime update() override;

	OBFME_MODULE_XFER(SpawnUnitBehavior)
	void crc(StateHasher &hasher) const override;

	SpawnUnitBehaviorInterface *getSpawnUnitBehaviorInterface() override { return this; }
	const std::string &getUnitName() const override
	{
		return m_data ? m_data->m_unitName : s_empty;
	}

	bool spawned() const { return m_spawned; }
	void setSpawned(bool s) { m_spawned = s; }

	static void registerClass(ModuleFactory &modules);

private:
	const SpawnUnitBehaviorModuleData *m_data = nullptr;
	bool m_spawned = false; // RW 0x8BC9B0 [esi + 0x14], RW 0x4B87A3
	inline static const std::string s_empty{};
};
