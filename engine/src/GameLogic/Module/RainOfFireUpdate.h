// OpenBFME. GPL-3.0.
//
// RainOfFireUpdate (lane MODGAP-2; editions: both RotWK 2.01 and BFME2 1.06):
// Drives the RainOfFire special power effect and acts as the environment destruction actor
// via BehaviorModule slot 31 (RW 0x8AFC5F / BFME2 0x4AC5F5).
//
// TARGET FACTS:
// - Registry: RW 0x64F134 / BFME2 0x64F448 ("RainOfFireUpdate", interface mask 1 = UPDATE).
// - RainOfFireUpdateModuleData:
//   ctor RW 0x8AF700 / BFME2 0x4AC03D,
//   buildFieldParse RW 0x8AF718 / BFME2 0x4AC097:
//   * StartRainTime (INI::parseDurationUnsignedInt, default 150)
//   * DarknessFadeTime (INI::parseDurationUnsignedInt, default 120)
//   * Darkness (INI::parseFloat, default 0.25f)
//   * Height (INI::parseFloat, default 100.0f)
// - RainOfFireUpdate:
//   ctor RW 0x8AF739 / BFME2 0x4AC0A8,
//   dtor RW 0x8AF8C5 / BFME2 0x4AC24D,
//   slot 31 (offset 0x7C in secondary interface, RW 0x8A18E0 / BFME2 0x004AC0A8):
//   * returns EnvironmentActor interface pointer (+0x20)
//   * slot 0 release (RW 0x5015D4 / BFME2 slot 0)
//   * slot 1 advance (RW 0x8AF79F / BFME2 0x4AC18D)

#pragma once

#include "Common/Module.h"
#include "GameLogic/Module/EnvironmentActor.h"
#include "GameLogic/Module/UpdateModule.h"

#include <cstdint>

class ModuleFactory;
class MultiIniFieldParse;

class RainOfFireUpdateModuleData : public ModuleData
{
public:
	std::uint32_t m_startRainTime = 150;     ///< +0x08 StartRainTime
	std::uint32_t m_darknessFadeTime = 120;  ///< +0x0C DarknessFadeTime
	float m_darkness = 0.25f;               ///< +0x14 Darkness
	float m_height = 100.0f;                ///< +0x10 Height

	RainOfFireUpdateModuleData() = default;
	static void buildFieldParse(MultiIniFieldParse &p);
};

class RainOfFireUpdate : public UpdateModule, public EnvironmentActor
{
public:
	OBFME_MODULE_XFER(RainOfFireUpdate)

	RainOfFireUpdate(Thing *thing, const RainOfFireUpdateModuleData *data);

	UpdateSleepTime update() override;

	// EnvironmentActor interface (RW 0x8AFC5F / BFME2 0x4AC5F5)
	void release(float amount) override;
	void advance(float amount) override;
	EnvironmentActor *slot31() override { return this; }

	float darkness() const { return m_darkness; }
	float scale() const { return m_scale; }

	static void registerClass(ModuleFactory &modules);

private:
	const RainOfFireUpdateModuleData *m_data = nullptr;
	std::uint32_t m_frame = 0;
	float m_darkness = 1.0f;
	float m_scale = 1.0f;
};

