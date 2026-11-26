// OpenBFME. GPL-3.0.
// FireLogicSystem INI configuration (RW 0x6860E0 and RW 0x6866CE).
#pragma once

class INI;

struct FireTerrainMaterial
{
	unsigned fuel = 0;
	unsigned maxBurnRate = 0;
	unsigned decay = 0;
	unsigned resistance = 0;
};

struct FireLogicConfig
{
	FireTerrainMaterial materials[4];
	int maxCellsBurning = 0x7fffffff;
	int fuelRegenRate = 1;

	void parseINI(INI *ini);
};
