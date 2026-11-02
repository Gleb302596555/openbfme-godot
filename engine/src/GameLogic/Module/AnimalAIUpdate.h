// OpenBFME. GPL-3.0.
//
// AnimalAIUpdate (lane MODGAP-2; editions: both RotWK 2.01 and BFME2 1.06):
// AI update for ambient animals (e.g. horses, wolves, sheep, deer). Animals wander
// around an origin point or salt lick, or flee when enemies enter FleeRange.
//
// TARGET FACTS:
// - Registry: RW 0x64EC65 / BFME2 0x6524BF ("AnimalAIUpdate",
//   type MODULETYPE_BEHAVIOR = 1, interface mask 0x11 = UPDATE | DRAWABLE_CAPABLE).
// - AnimalAIUpdateModuleData:
//   ctor RW 0x882310 / BFME2 0x47EC40,
//   buildFieldParse RW 0x882350 / BFME2 0x24BE7A:
//   * FleeRange (default 20.0f)
//   * FleeDistance (default 100.0f)
//   * WanderPercentage (default 50)
//   * MaxWanderDistance (default 10.0f)
//   * MaxWanderRadius (default 10.0f)
//   * AfraidOfCastles (default true)
//   * UpdateTimer (default 10)
// - AnimalAIUpdate:
//   ctor RW 0x881DAA / BFME2 0x47ECA8,
//   dtor RW 0x881E19 / BFME2 0x47ECE0,
//   update RW 0x882058 / BFME2 0x47EDC9,
//   xfer RW 0x881E3E / BFME2 0x47EBAF.

#pragma once

#include "Common/INIDataTypes.h"
#include "GameLogic/Module/AIUpdate.h"

class MultiIniFieldParse;
class ModuleFactory;

class AnimalAIUpdateModuleData : public ModuleData
{
public:
	float m_fleeRange = 20.0f;
	float m_fleeDistance = 100.0f;
	int m_wanderPercentage = 50;
	float m_maxWanderDistance = 10.0f;
	float m_maxWanderRadius = 10.0f;
	bool m_afraidOfCastles = true;
	unsigned int m_updateTimer = 10;

	static void buildFieldParse(MultiIniFieldParse &p);
};

class AnimalAIUpdate : public AIUpdateInterface
{
public:
	AnimalAIUpdate(Thing *thing, const ModuleData *data);
	~AnimalAIUpdate() override = default;

	const AnimalAIUpdateModuleData *animalData() const { return m_data; }

	UpdateSleepTime update() override;

	void crc(StateHasher &h) const override;
	OBFME_MODULE_XFER(AnimalAIUpdate)

	static void registerClass(ModuleFactory &modules);

private:
	const AnimalAIUpdateModuleData *m_data = nullptr;
	ObjectID m_threatID = 0;
	Coord3D m_origin{ 0.0f, 0.0f, 0.0f };
	bool m_processed = false;
	bool m_returning = false;
};

