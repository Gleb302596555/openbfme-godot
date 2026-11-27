// OpenBFME tests for PORT-308 (fork issue #700): the three candidate-reader fields of
// LargeGroupBonusUpdate, W3DLightDraw and EvaAnnounceClientCreate. Synthetic INI only.
//
// RW (rotwk 2.01 game.dat, details in docs/ennorath/PORT-308-RW-ANALYSIS.md):
//   * LargeGroupBonusUpdateModuleData ctor RW 0x893871: RW 0x8938B8 writes AlliesOnly = 1 and
//     RW 0x8938BC writes RubOffRadius = 20.0f. update RW 0x8938D9 builds its partition filter
//     at RW 0x89395B..0x89396E and never reads data +0x18 (verdict S-7291); RW 0x893A4A does
//     read RubOffRadius for the rub-off scan.
//   * W3DLightDrawModuleData ctor RW 0x4CF515 writes Radius 20.0f (RW 0x4CF52C); the module
//     ctor RW 0x4CF121 reads data +0x2C at RW 0x4CF208 and writes the light's far attenuation
//     end (+0x104, RW 0x4CF216), start 1.0f (+0x100, RW 0x4CF20E / 0x4CF21C), flag RW 0x4CF227.
//   * EvaAnnounceClientCreate clientUpdate RW 0x8CDF07 reads data +0x08/+0x0C/+0x10 and
//     +0x18/+0x19/+0x1A, never +0x1B (CreateFakeRadarEvent verdict S-7212).
// GPL-3.0.

#include "doctest.h"

#include "ObjectTestUtil.h"

#include "Common/Thing/Thing.h"
#include "Common/Thing/ThingTemplate.h"
#include "GameClient/ClientEffects.h"
#include "GameClient/EvaAnnounceClientCreate.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DLightDraw.h"
#include "GameLogic/Module/AreaScanModules.h"

#include <cstdint>
#include <vector>

namespace
{
const char *const kLgbIni =
	"Object P308HordeDefault\n"
	"  KindOf = SELECTABLE HORDE\n"
	"  Behavior = LargeGroupBonusUpdate ModuleTag_LGB\n"
	"    Count = 10\n"
	"  End\n"
	"End\n"
	"Object P308HordeExplicit\n"
	"  KindOf = SELECTABLE HORDE\n"
	"  Behavior = LargeGroupBonusUpdate ModuleTag_LGB\n"
	"    Count = 10\n"
	"    AlliesOnly = No\n"
	"    RubOffRadius = 200\n"
	"  End\n"
	"End\n";

const char *const kLightIni =
	"Object P308Torch\n"
	"  KindOf = SELECTABLE\n"
	"  Draw = W3DLightDraw ModuleTag_Light\n"
	"    Diffuse = R:255 G:128 B:32\n"
	"    Radius = 90\n"
	"  End\n"
	"End\n"
	"Object P308TorchDefault\n"
	"  KindOf = SELECTABLE\n"
	"  Draw = W3DLightDraw ModuleTag_Light\n"
	"    Diffuse = R:255 G:128 B:32\n"
	"  End\n"
	"End\n";

const LargeGroupBonusUpdateModuleData *lgbData(const ThingTemplate *t)
{
	if (!t)
	{
		return nullptr;
	}
	for (const auto &n : t->behaviorModules().nuggets())
	{
		if (const auto *d = dynamic_cast<const LargeGroupBonusUpdateModuleData *>(n.data.get()))
		{
			return d;
		}
	}
	return nullptr;
}

const W3DLightDrawModuleData *lightData(const ThingTemplate *t)
{
	if (!t)
	{
		return nullptr;
	}
	for (const auto &n : t->drawModules().nuggets())
	{
		if (const auto *d = dynamic_cast<const W3DLightDrawModuleData *>(n.data.get()))
		{
			return d;
		}
	}
	return nullptr;
}

struct AnnounceResult
{
	int event;
	bool firstSight;
};

std::vector<AnnounceResult> announce(EvaAnnounceClientCreateModuleData data, bool alreadySighted)
{
	data.enemy = 10;
	data.ally = 20;
	data.owner = 30;
	data.delay = 4;
	data.firstSight = true;
	EvaAnnounceClientCreate module(nullptr, &data);
	module.onBuildCompleteAt(5); // max(7, 5 + max(2, 4)) = 9, RW 0x8CDED6
	std::vector<AnnounceResult> out;
	for (std::uint32_t frame = 6; frame <= 16; ++frame)
	{
		bool sight = false;
		out.push_back({ module.update(frame, false, true, true, 2, sight, alreadySighted), sight });
	}
	return out;
}
} // namespace

TEST_CASE("PORT-308 LargeGroupBonusUpdate data defaults are Yes / 20.0f in retail (RW 0x8938B8 / RW 0x8938BC)")
{
	const LargeGroupBonusUpdateModuleData d;
	CHECK(d.m_alliesOnly);                 // RW 0x8938B8: mov byte [esi+0x18], 1
	CHECK(d.m_rubOffRadius == 20.0f);      // RW 0x8938BC: 20.0f literal from RW 0xBDBC6C
}

TEST_CASE("PORT-308 AlliesOnly / RubOffRadius are parsed, while omission keeps the retail defaults (RW 0xC63A28 / RW 0xC63A38)")
{
	objtest::World w;
	AreaScanModules::registerAll(w.modules);
	REQUIRE(w.load(kLgbIni).empty());

	const LargeGroupBonusUpdateModuleData *defaults = lgbData(w.get("P308HordeDefault"));
	const LargeGroupBonusUpdateModuleData *explicitData = lgbData(w.get("P308HordeExplicit"));
	REQUIRE(defaults != nullptr);
	REQUIRE(explicitData != nullptr);
	CHECK(defaults->m_alliesOnly);
	CHECK(defaults->m_rubOffRadius == 20.0f);
	CHECK_FALSE(explicitData->m_alliesOnly);
	CHECK(explicitData->m_rubOffRadius == 200.0f); // the value of the mod's horde blocks
}

TEST_CASE("PORT-308 W3DLightDraw Radius reaches the persistent light's far attenuation (RW 0x4CF208 / 0x4CF21C / 0x4CF227)")
{
	objtest::World w;
	W3DLightDraw::registerClass(w.modules);
	REQUIRE(w.load(kLightIni).empty());

	LightPulsePool pool;
	const W3DLightDrawModuleData *d = lightData(w.get("P308Torch"));
	REQUIRE(d != nullptr);
	CHECK(d->m_radius == 90.0f);

	Thing thing(w.get("P308Torch"));
	Coord3D pos{ 12.0f, 34.0f, 0.0f };
	thing.setPosition(&pos);
	W3DLightDraw light(&thing, d);
	light.setLightPool(&pool);

	REQUIRE(light.lightHandle() == 0);
	REQUIRE(pool.lights().size() == 1u);
	CHECK(light.lightRadius() == 90.0f);
	CHECK(pool.lights()[0].farAttenStart == 1.0f); // RW 0x4CF21C writes +0x100 = 1.0
	CHECK(pool.lights()[0].farAttenEnd == 90.0f);  // RW 0x4CF216 writes +0x104 = data +0x2C
	CHECK(pool.lights()[0].farAttenuation);        // RW 0x4CF227

	// Neighbour: the ModuleData default is 20.0f when the INI leaves Radius out (RW 0x4CF52C).
	const W3DLightDrawModuleData *dDefault = lightData(w.get("P308TorchDefault"));
	REQUIRE(dDefault != nullptr);
	CHECK(dDefault->m_radius == 20.0f);
}

TEST_CASE("PORT-308 CreateFakeRadarEvent changes neither the EVA decision nor the first-sight mark (RW 0x8CDF07, S-7212)")
{
	EvaAnnounceClientCreateModuleData flagData;
	flagData.fakeRadar = true;
	const std::vector<AnnounceResult> withFlag = announce(flagData, false);
	const std::vector<AnnounceResult> withoutFlag = announce(EvaAnnounceClientCreateModuleData(), false);
	REQUIRE(withFlag.size() == withoutFlag.size());
	for (size_t i = 0; i < withFlag.size(); ++i)
	{
		CHECK(withFlag[i].event == withoutFlag[i].event);
		CHECK(withFlag[i].firstSight == withoutFlag[i].firstSight);
	}
	// S-7212 / RW 0x8CDF6B .. 0x8CDFC0: due at frame 9, owner event 30, first sight marked exactly once.
	CHECK(withFlag[3].event == 30);
	CHECK(withFlag[3].firstSight);
	for (size_t i = 0; i < withFlag.size(); ++i)
	{
		if (i != 3)
		{
			CHECK(withFlag[i].event == -1);
			CHECK_FALSE(withFlag[i].firstSight);
		}
	}
}
