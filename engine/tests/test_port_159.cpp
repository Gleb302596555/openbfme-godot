// OpenBFME unit tests: PORT-159 (issue #377, P-83) — the mumak and the dwarven battle wagon have no shadow on Ultra.
//
// TARGET FACTS (RotWK game.dat 2.01, read with tools/mod/rw.py; docs/ennorath/PORT159_RW_ANALYSIS.md):
//   Shadow type word, names RW 0xD99EB0 (read out of the binary): 0 SHADOW_DECAL 0x1, 1 SHADOW_VOLUME 0x2,
//     2 SHADOW_VOLUME_NEW 0x4, 3 SHADOW_DYNAMIC_PROJECTION 0x8, 4 SHADOW_DIRECTIONAL_PROJECTION 0x10,
//     5 SHADOW_ALPHA_DECAL 0x20, 6 SHADOW_ADDITIVE_DECAL 0x40, 7/8/9 SHADOW_VOLUME_NON_SELF_1/2/3 0x80/0x100/0x200,
//     10/11 SHADOW_ALPHA/ADDITIVE_DECAL_DYNAMIC 0x400/0x800, 12 SHADOW_MERGE_DECAL 0x1000,
//     13 SHADOW_DECAL_TERRAIN_CLAIM 0x2000, 14/15 SHADOW_VOLUME(_NEW)_OR_DECAL 0x4000/0x8000.
//   W3DShadowManager::addShadow RW 0x49A2EB sends the type word to one manager:
//     RW 0x49A302: the type is ShadowTypeInfo + 8 (default 2 with no info);
//     RW 0x49A317..0x49A355: 0x4000 / 0x8000 resolved through GlobalData + 0x60 (UseShadowVolumes);
//     RW 0x49A358..0x49A38E: type 2 and types 0x80 / 0x100 / 0x200 -> W3DVolumetricShadowManager::addShadow RW 0x4F24A8;
//     RW 0x49A39A: type 4 -> the V2 manager RW 0x507CAA;
//     RW 0x49A3B2..0x49A3F6: types 1 / 0x20 / 0x40 / 0x400 / 0x800 / 0x1000 / 0x10000 -> the projected (decal) manager
//       W3DProjectedShadowManager::addShadow RW 0x50BF6A;
//     RW 0x49A3DA: every other type -> NULL, no shadow object.
//   The volumetric manager RW 0x4F24A8 gates on stencil RW 0x51C760, the render object, GlobalData + 0x60
//     (RW 0x4F24D7) and a geometry, allocates a 0x4904-byte W3DVolumetricShadow (RW 0x4F2520), stores the render object
//     at + 0x70 and the type word at + 0x34 (RW 0x4F25A7) — that word is the stencil group, not the "has a shadow" answer.
//   Ultra: the static LOD apply RW 0x601C62 sets UseShadowMapping (+0x62, RW 0x601D4C) and then clears UseShadowVolumes
//     (+0x60, RW 0x601D59) and UseShadowDecals (+0x61, RW 0x601D61) — so a decal type draws nothing on Ultra.
//   W3DScriptedModelDraw::setModelState RW 0x4C4EE0: a condition state's own Shadow (hasShadow, type != 0) wins over the
//     template's (branch A), the template's word is read at + 0x5F2 (branch B, RW 0x4C4FD6).
//
// The port's single caster answer is ObjectShadow::castsMap (MapMask / kDecalMask); its call sites are the map build
// (GodotMapObjectBuilder), the per-frame instance refresh (GodotGameWorld) and the decal builder (ObjectShadow).
// The mumak and the dwarven battle wagon say `Shadow = SHADOW_VOLUME_NON_SELF_2` (0x100) with `ShadowSizeX = 36`.

#include "doctest.h"

#include "ObjectTestUtil.h"
#include "W3dSynth.h"

#include "GameClient/Drawable.h"
#include "GameClient/GameLODManager.h"
#include "GameClient/OptionPreferences.h"
#include "GameClient/ObjectShadow.h"
#include "GameClient/SelectionDecals.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DModelDraw.h"
#include "Libraries/WWVegas/WW3D2/assetmgr.h"

#include <cmath>
#include <string>
#include <vector>

using namespace objtest;

namespace
{
// The rows of the Ennorath data that carry these types (vanilla lines, unchanged by the mod). Kept to the field names only.
const char kPort159Objects[] =
	"Object Port159Mumak\n"
	"  Shadow = SHADOW_VOLUME_NON_SELF_2\n"
	"  ShadowSizeX = 36\n"
	"End\n"
	"Object Port159Wagon\n"
	"  Shadow = SHADOW_VOLUME_NON_SELF_2\n"
	"  ShadowSizeX = 36\n"
	"End\n"
	"Object Port159Rocks\n"
	"  Shadow = SHADOW_VOLUME_NON_SELF_1\n"
	"End\n"
	"Object Port159Third\n"
	"  Shadow = SHADOW_VOLUME_NON_SELF_3\n"
	"End\n"
	"Object Port159Volume\n"
	"  Shadow = SHADOW_VOLUME\n"
	"  ShadowSizeX = 12\n"
	"End\n"
	"Object Port159NewVolume\n"
	"  Shadow = SHADOW_VOLUME_NEW\n"
	"End\n"
	"Object Port159Decal\n"
	"  Shadow = SHADOW_DECAL\n"
	"End\n"
	"Object Port159DynamicProjection\n"
	"  Shadow = SHADOW_DYNAMIC_PROJECTION\n"
	"End\n"
	"Object Port159DirectionalProjection\n"
	"  Shadow = SHADOW_DIRECTIONAL_PROJECTION\n"
	"End\n"
	"Object Port159MergeDecal\n"
	"  Shadow = SHADOW_MERGE_DECAL\n"
	"End\n"
	"Object Port159TerrainClaim\n"
	"  Shadow = SHADOW_DECAL_TERRAIN_CLAIM\n"
	"End\n"
	"Object Port159None\n"
	"End\n";

// The five StaticGameLOD names of RW 0xD9E6BC (GameLODManager::findLevel) with the shadow columns of the level record:
// + 0xC UseShadowVolumes, + 0xD UseShadowDecals, + 0xE UseShadowMapping (RW 0x601D34 / 0x601D40 / 0x601D4C).
const char kPort159Lod[] =
	"StaticGameLOD VeryLow\n"
	"  UseShadowVolumes = No\n"
	"  UseShadowDecals = No\n"
	"  UseShadowMapping = No\n"
	"End\n"
	"StaticGameLOD Low\n"
	"  UseShadowVolumes = No\n"
	"  UseShadowDecals = No\n"
	"  UseShadowMapping = No\n"
	"End\n"
	"StaticGameLOD Medium\n"
	"  UseShadowVolumes = No\n"
	"  UseShadowDecals = Yes\n"
	"  UseShadowMapping = No\n"
	"End\n"
	"StaticGameLOD High\n"
	"  UseShadowVolumes = Yes\n"
	"  UseShadowDecals = Yes\n"
	"  UseShadowMapping = No\n"
	"End\n"
	"StaticGameLOD UltraHigh\n"
	"  UseShadowVolumes = Yes\n"
	"  UseShadowDecals = Yes\n"
	"  UseShadowMapping = Yes\n"
	"End\n";

// An INI environment that also answers StaticGameLOD blocks (GameLODManager::registerBlocks) and the draw modules.
struct LodWorld
{
	World w;
	GameLODManager lod;

	LodWorld()
	{
		lod.registerBlocks(w.fx.env.blocks);
	}

	void load(const std::string &text, const char *name)
	{
		const std::string err = w.load(text, INI_LOAD_OVERWRITE, name);
		REQUIRE(err.empty());
	}
};

} // namespace

TEST_CASE("PORT-159 caster test: SHADOW_VOLUME_NON_SELF_1/2/3 are volume shadows (RW 0x49A2EB, RW 0x4F24A8)")
{
	World w;
	REQUIRE(w.load(kPort159Objects).empty());

	const ThingTemplate *mumak = w.get("Port159Mumak");
	const ThingTemplate *wagon = w.get("Port159Wagon");
	const ThingTemplate *rocks = w.get("Port159Rocks");
	const ThingTemplate *third = w.get("Port159Third");
	const ThingTemplate *volume = w.get("Port159Volume");
	const ThingTemplate *newVolume = w.get("Port159NewVolume");
	const ThingTemplate *decal = w.get("Port159Decal");
	const ThingTemplate *dynamicProjection = w.get("Port159DynamicProjection");
	const ThingTemplate *directionalProjection = w.get("Port159DirectionalProjection");
	const ThingTemplate *mergeDecal = w.get("Port159MergeDecal");
	const ThingTemplate *terrainClaim = w.get("Port159TerrainClaim");
	const ThingTemplate *none = w.get("Port159None");
	REQUIRE(mumak != nullptr);
	REQUIRE(wagon != nullptr);
	REQUIRE(rocks != nullptr);
	REQUIRE(third != nullptr);
	REQUIRE(volume != nullptr);
	REQUIRE(newVolume != nullptr);
	REQUIRE(decal != nullptr);
	REQUIRE(dynamicProjection != nullptr);
	REQUIRE(directionalProjection != nullptr);
	REQUIRE(mergeDecal != nullptr);
	REQUIRE(terrainClaim != nullptr);
	REQUIRE(none != nullptr);

	// The words the data holds: entry 8 of RW 0xD99EB0 is 0x100, entry 7 is 0x80, entry 9 is 0x200.
	REQUIRE(std::get<long long>(*mumak->findField("Shadow")) == 0x100);
	REQUIRE(std::get<long long>(*wagon->findField("Shadow")) == 0x100);
	REQUIRE(std::get<long long>(*rocks->findField("Shadow")) == 0x80);
	REQUIRE(std::get<long long>(*third->findField("Shadow")) == 0x200);

	// Medium (no shadow volumes, decals on): RW 0x49A361 sends 0x80 / 0x100 / 0x200 to the volumetric manager, and the
	// volumetric manager makes a shadow whenever it has stencil, a render object and GlobalData + 0x60 (here: the list decides).
	for (const ThingTemplate *t : { mumak, wagon, rocks, third, volume, newVolume })
	{
		CHECK(ObjectShadow::castsMap(t, nullptr, nullptr, false));
	}
	CHECK_FALSE(ObjectShadow::castsMap(decal, nullptr, nullptr, false));
	CHECK_FALSE(ObjectShadow::castsMap(none, nullptr, nullptr, false));

	// Ultra (UseShadowMapping set, RW 0x601D59 / 0x601D61 clear decals and volumes): the non-self volumes cast the map too.
	for (const ThingTemplate *t : { mumak, wagon, rocks, third, volume, newVolume })
	{
		CHECK(ObjectShadow::castsMap(t, nullptr, nullptr, true));
	}

	// The types RW 0x49A3DA answers NULL for make no shadow object at any LOD: 0x8, 0x10, 0x2000. (0x1000 goes to the
	// projected manager RW 0x49A3CA, so it draws a decal on Medium and nothing on Ultra, like the other decal types.)
	CHECK_FALSE(ObjectShadow::castsMap(dynamicProjection, nullptr, nullptr, false));
	CHECK_FALSE(ObjectShadow::castsMap(dynamicProjection, nullptr, nullptr, true));
	CHECK_FALSE(ObjectShadow::castsMap(directionalProjection, nullptr, nullptr, false));
	CHECK_FALSE(ObjectShadow::castsMap(directionalProjection, nullptr, nullptr, true));
	CHECK_FALSE(ObjectShadow::castsMap(terrainClaim, nullptr, nullptr, false));
	CHECK_FALSE(ObjectShadow::castsMap(terrainClaim, nullptr, nullptr, true));
	CHECK(ObjectShadow::castsMap(mergeDecal, nullptr, nullptr, false));

	// ShadowForceDisable still wins over a non-self volume (RW 0x4C4EF8 / RW 0x4C5013).
	W3DModelDrawModuleData disabled;
	disabled.m_shadowForceDisable = true;
	CHECK_FALSE(ObjectShadow::castsMap(mumak, &disabled, nullptr, true));
	CHECK_FALSE(ObjectShadow::castsMap(mumak, &disabled, nullptr, false));
}

TEST_CASE("PORT-159 a condition state's SHADOW_VOLUME_NON_SELF replaces the template's decal (RW 0x4C4EE0, branch A)")
{
	World w;
	REQUIRE(w.load("Object Port159StateVolume\n  Shadow = SHADOW_DECAL\nEnd\nObject Port159StateDecal\n  Shadow = SHADOW_VOLUME\nEnd\n").empty());
	const ThingTemplate *stateVolume = w.get("Port159StateVolume");
	const ThingTemplate *stateDecal = w.get("Port159StateDecal");
	REQUIRE(stateVolume != nullptr);
	REQUIRE(stateDecal != nullptr);

	ModelConditionInfo state;
	state.hasShadow = true;
	state.shadow.type = 0x100; // SHADOW_VOLUME_NON_SELF_2 in the model condition state
	CHECK(ObjectShadow::castsMap(stateVolume, nullptr, &state, false));
	CHECK(ObjectShadow::castsMap(stateVolume, nullptr, &state, true));
	CHECK(ObjectShadow::castsMap(stateDecal, nullptr, &state, false)); // the state wins over the template either way

	state.shadow.type = 0x1; // SHADOW_DECAL in the state over a volume template: RW 0x49A3EC draws a projected decal
	CHECK_FALSE(ObjectShadow::castsMap(stateDecal, nullptr, &state, false)); // Medium draws the decal, not a map
	CHECK(ObjectShadow::castsMap(stateDecal, nullptr, &state, true));         // Ultra casts the map (decal path off)
}

TEST_CASE("PORT-159 Ultra loses no shadow: the LOD record's flags decide, and the non-self volume survives them (RW 0x601C62)")
{
	OptionPreferences prefs;
	prefs.set("AllHealthBars", "yes"); // RW 0x6E6179 reads this key when the level draws decals; the branch is not what this test asks
	LodWorld lw;
	lw.load(kPort159Objects, "obj.ini");
	lw.load(kPort159Lod, "gamelod.ini");

	REQUIRE(GameLODManager::findLevel("UltraHigh") == 4);
	const int ultra = GameLODManager::findLevel("UltraHigh");
	const int high = GameLODManager::findLevel("High");
	const int medium = GameLODManager::findLevel("Medium");

	lw.lod.setStaticLODLevel(ultra, prefs);
	const StaticLODState &u = lw.lod.state();
	// RW 0x601D4C sets +0x62, RW 0x601D59 / 0x601D61 clear +0x60 / +0x61
	CHECK(u.useShadowMapping);
	CHECK_FALSE(u.useShadowVolumes);
	CHECK_FALSE(u.useShadowDecals);

	lw.lod.setStaticLODLevel(high, prefs);
	const StaticLODState &h = lw.lod.state();
	CHECK_FALSE(h.useShadowMapping);
	CHECK(h.useShadowVolumes);
	CHECK(h.useShadowDecals);

	lw.lod.setStaticLODLevel(medium, prefs);
	const StaticLODState &m = lw.lod.state();
	CHECK_FALSE(m.useShadowMapping);
	CHECK_FALSE(m.useShadowVolumes);
	CHECK(m.useShadowDecals);

	// The one test the client makes: UseShadowMapping on (Ultra) or the volume/decals flags off (Medium) still cast for a
	// NON_SELF volume, because RW 0x49A361 hands it to the volumetric manager, not to the decal one.
	const ThingTemplate *mumak = lw.w.get("Port159Mumak");
	REQUIRE(mumak != nullptr);
	CHECK(ObjectShadow::castsMap(mumak, nullptr, nullptr, u.useShadowMapping));
	CHECK(ObjectShadow::castsMap(mumak, nullptr, nullptr, h.useShadowMapping));
	CHECK(ObjectShadow::castsMap(mumak, nullptr, nullptr, m.useShadowMapping));
}

TEST_CASE("PORT-159 neighbouring behaviour holds: the volume and decal casters of the game are unchanged")
{
	World w;
	REQUIRE(w.load(kPort159Objects).empty());
	const ThingTemplate *volume = w.get("Port159Volume");
	const ThingTemplate *newVolume = w.get("Port159NewVolume");
	const ThingTemplate *decal = w.get("Port159Decal");
	REQUIRE(volume != nullptr);
	REQUIRE(newVolume != nullptr);
	REQUIRE(decal != nullptr);

	// SHADOW_VOLUME (0x2) and SHADOW_VOLUME_NEW (0x4) keep their old answers (RW 0x49A363 / 0x49A36A).
	CHECK(ObjectShadow::castsMap(volume, nullptr, nullptr, false));
	CHECK(ObjectShadow::castsMap(volume, nullptr, nullptr, true));
	CHECK(ObjectShadow::castsMap(newVolume, nullptr, nullptr, false));
	CHECK(ObjectShadow::castsMap(newVolume, nullptr, nullptr, true));

	// SHADOW_DECAL (0x1) is a blob decal on Medium and a map caster on Ultra, exactly as before (RW 0x49A3EC).
	CHECK_FALSE(ObjectShadow::castsMap(decal, nullptr, nullptr, false));
	CHECK(ObjectShadow::castsMap(decal, nullptr, nullptr, true));

	// No template, no shadow; a template without the row, no shadow.
	CHECK_FALSE(ObjectShadow::castsMap(nullptr, nullptr, nullptr, true));
	CHECK_FALSE(ObjectShadow::castsMap(w.get("Port159None"), nullptr, nullptr, true));
}
