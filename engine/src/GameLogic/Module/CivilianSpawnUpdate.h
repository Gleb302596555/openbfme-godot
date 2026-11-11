// OpenBFME. GPL-3.0.
//
// CivilianSpawnUpdate (lane LOCAL-36, issue #286): civilians leaving buildings and running toward destinations.
//
// TARGET FACTS (RotWK game.dat, caveat S-001):
//   * data (field table RW 0xC5EC68, after ModuleData base):
//     SpawnDelayTime (+0x8, parseDurationUnsignedInt RW 0x73A429),
//     RunToFilter (+0xC, parseObjectFilter RW 0x76392F),
//     MaximumDistance (+0x10, parseReal RW 0x42EC5E),
//     Civilian (+0x14, parseAsciiStringList RW 0x42EED6).
//     Ctor RW 0x882C31. Factory RW 0x6593F7.
//   * update (RW 0x882D5B; DONOR BFME2 0x47FAFE / CivilianSpawnUpdate.cpp:306):
//     - Selects random template from Civilian list;
//     - Finds alive non-self objects in MaximumDistance matching RunToFilter for controlling player;
//     - Selects random target object;
//     - Spawns civilian at obj->pos + dir * (radius * 0.5f);
//     - If civilian has AI: chooses locomotor set 4 (LOCOMOTORSET_PANIC), sets MODELCONDITION_PANICKING;
//     - Sets destination = target->pos + targetDir * (targetRadius * 0.5f);
//     - Ignores target as obstacle;
//     - Checks QuickDoesPathExist: if path exists, issues aiMoveToPosition(dest, CMD_FROM_AI);
//       otherwise destroys civilian;
//     - Clears ignored obstacle;
//     - Returns m_spawnFrame.
//   * xfer (RW 0x89A1AC):
//     Version (1, 2); if version >= 2, xferUpdateModule(x).

#pragma once

#include "GameLogic/Module/UpdateModule.h"
#include "GameLogic/ObjectFilter.h"

#include <string>
#include <vector>

class ModuleFactory;

class CivilianSpawnUpdateModuleData : public ModuleData
{
public:
	unsigned int m_spawnDelay = 0;        ///< +0x8 SpawnDelayTime (frames)
	ObjectFilter m_runToFilter;           ///< +0xC RunToFilter
	float m_maximumDistance = 0.0f;       ///< +0x10 MaximumDistance
	std::vector<std::string> m_civilians; ///< +0x14 Civilian template names
	static void buildFieldParse(MultiIniFieldParse &p);
};

class CivilianSpawnUpdate : public UpdateModule
{
public:
	OBFME_MODULE_XFER(CivilianSpawnUpdate) // lane SAVE-4b: version 1.2
	CivilianSpawnUpdate(Thing *thing, const CivilianSpawnUpdateModuleData *data);

	UpdateSleepTime update() override; // RW 0x882D5B
	void crc(StateHasher &hasher) const override;

	unsigned int spawnFrame() const { return m_spawnFrame; }

	static void registerClass(ModuleFactory &modules);

private:
	const CivilianSpawnUpdateModuleData *m_data = nullptr;
	unsigned int m_spawnFrame = 0; // +0x20 in retail
};
