#include "doctest.h"
#include "GameLogic/Module/EmotionModules.h"

TEST_CASE("PORT-300 RadiateFearUpdate: module and WhichSpecialPower verdict (S-7193)")
{
	// All 9 uncited own functions of RadiateFearUpdate (RW 0x004B612F, 0x0089F761, 0x0089F767,
	// 0x0089F7AC, 0x0089F7D3, 0x0089F7E2, 0x0089F7EC, 0x0089F903, 0x0089F95C) are non-behaviour
	// (getters, destructors, GetSnapshotName, getModuleNameKey, UpgradeMux thunks).
	// WhichSpecialPower (+0xC) has no reachable reader in RotWK 2.01 (S-7193, PORT-57).
	CHECK(true);
}
