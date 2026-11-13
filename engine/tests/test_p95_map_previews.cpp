// OpenBFME. GPL-3.0.
//
// P-95 (docs/ennorath/PROBLEMS.md, owner 2026-11-12): some maps show no picture in the skirmish / LAN menu and on the loading screen. The menu's two
// pictures: AptMapPreview::Picture takes "<map without .map>_pic.tga" (RW 0x97640C -> 0x975F23), the CurrentMap window "<map>_art.tga", else "<map>.tga"
// (getMapPreviewImage RW 0x702689 .. 0x7028B9; neither: no image, RW 0x7028B9). The archives (checked 2026-11-13): map mp harlindon, map mp grey mountains
// and map mp withered heath have their .map in RotWK's Maps.big and their _art / _pic only in BFME2's maps.big. Lists every map of the
// Ennorath setup source whose pictures the mounted file system does not find. Skips without the three installs.
#include "EnnorathScenario.h"
#include "GameClient/GUI/Skirmish/IniSkirmishSetupSource.h"

#include <algorithm>
#include <cstdio>

TEST_CASE("P-95 ennorath: the skirmish menu finds the _pic and _art pictures of every shipped multiplayer map, BFME2-only ones included")
{
	ennorathscenario::World world;
	if (!world.load("P-95 map previews"))
	{
		return;
	}
	IniSkirmishSetupSource source;
	std::string error;
	REQUIRE_MESSAGE(source.load(*world.merged.fs, &error), error);
	int missingPic = 0, missingArt = 0;
	for (const MapCacheEntry &m : source.maps())
	{
		// the shipped multiplayer maps ("maps/map mp ..."); the three developer test maps have no _pic in any archive (retail shows MissingMap too)
		if (!m.isMultiplayer || m.name.size() <= 4 || m.name.rfind("maps/map mp ", 0) != 0)
		{
			continue;
		}
		std::string base = m.name.substr(0, m.name.size() - 4);
		std::replace(base.begin(), base.end(), '/', '\\');
		const bool pic = source.fileExists(base + "_pic.tga");
		const bool art = source.fileExists(base + "_art.tga") || source.fileExists(base + ".tga");
		const bool picFs = world.merged.fs->doesFileExist(base + "_pic.tga");
		if (!pic || !art)
		{
			std::printf("P-95 %s pic=%d art=%d fsPic=%d\n", m.name.c_str(), pic ? 1 : 0, art ? 1 : 0, picFs ? 1 : 0);
		}
		missingPic += pic ? 0 : 1;
		missingArt += art ? 0 : 1;
	}
	std::printf("P-95 %zu maps, %d without _pic, %d without _art/.tga\n", source.maps().size(), missingPic, missingArt);
	CHECK(missingPic == 0);
	CHECK(missingArt == 0);
}
