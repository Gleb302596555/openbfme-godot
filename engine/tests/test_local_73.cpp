// OpenBFME tests for Local 73 (issue #535):
//   - P-139: ObjectShadow per-DrawEntry decal shadows (RW 0x4C4EE0..0x4C5175, both editions)
//   - P-141: WeaponModeSpecialPowerUpdate weapon lock (RW 0x898432 -> 0x69121A, RW 0x8983C1 -> 0x68DF11, both editions)
//   - P-144: ObjectWeapons::hasWeaponSetFor / HeroModeSpecialAbilityUpdate (RW 0x691014 -> 0x73F6B9 -> 0x73D89F, both editions)
//   - P-146: W3DTornadoDraw RadiusDecal spiral + bound 1.5x tint envelope (RW 0x4D120A -> 0x4D1109 / 0x4D0FCB -> 0x732DE7, both editions)
//   - P-147: AttachToBoneInAnotherModule ParticleSysBone transform composition (RW 0x4B6A6F..0x4B6B26, both editions)
//   - P-149: Swirly particle physics via Drawable::swirl / B_TORNADO (RW 0x5F2EBC -> 0x678BF2 -> 0x676BD7 -> 0x676C7D, both editions)
//   - MATRIX.tsv reproducers: ElrondPersonalFlood (FireLogicNugget) & GwaihirScreech (RequiredUpgrades)

#include "doctest.h"
#include "CombatTestUtil.h"
#include "EnnorathScenario.h"
#include "FactbRig.h"
#include "FxTestUtil.h"
#include "ObjectTestUtil.h"
#include "W3dSynth.h"

#include "Common/AsciiString.h"
#include "Common/GameEdition.h"
#include "Common/Player.h"
#include "Common/SpecialPower.h"
#include "Common/Thing/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "GameClient/Drawable.h"
#include "GameClient/FXPlayback.h"
#include "GameClient/LiveFX.h"
#include "GameClient/ObjectShadow.h"
#include "GameClient/ParticleBoneFollow.h"
#include "GameClient/ParticleSys.h"
#include "GameClient/SelectionDecals.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DDrawModules.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DScriptedModelDraw.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DTornadoDraw.h"
#include "GameLogic/Combat/ObjectWeapons.h"
#include "GameLogic/Module/BehaviorModule.h"
#include "GameLogic/Object/AttributeModifierPool.h"
#include "GameClient/FXList.h"
#include "GameLogic/Module/HeroAbilityModules.h"
#include "GameLogic/Module/SpecialAbilityModules.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/Object/Contain/HordeContainRuntime.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/ObjectCreationList.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/WeaponNugget.h"
#include "GameLogic/WeaponStores.h"
#include "Libraries/WWVegas/WW3D2/assetmgr.h"

#include <cmath>
#include <string>
#include <vector>

using namespace combattest;
using namespace w3dsynth;

TEST_SUITE_BEGIN("LOCAL-73");

namespace
{
std::vector<std::uint8_t> makeTwoBoneW3d(const char *modelName, const char *bone1, float x1, float y1, float z1,
	const char *bone2 = nullptr, float x2 = 0.0f, float y2 = 0.0f, float z2 = 0.0f)
{
	std::vector<W3dPivotStruct> pivots;
	pivots.push_back(pivot("RootTransform", 0xFFFFFFFFu, 0.0f, 0.0f, 0.0f));
	pivots.push_back(pivot(bone1, 0u, x1, y1, z1));
	if (bone2)
	{
		pivots.push_back(pivot(bone2, 0u, x2, y2, z2));
	}
	SynthMesh mesh;
	mesh.Name = "BoxMesh";
	mesh.Container = modelName;
	mesh.Vertices = { { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 } };
	mesh.Normals = { { 0, 0, 1 }, { 0, 0, 1 }, { 0, 0, 1 } };
	mesh.Triangles = { 0, 1, 2 };
	mesh.Shaders = { opaqueShader() };
	SynthVMat vm;
	mesh.VertexMaterials = { vm };
	SynthPass pass;
	pass.VertexMaterialIds = { 0 };
	pass.ShaderIds = { 0 };
	mesh.Passes = { pass };

	std::vector<std::uint8_t> bytes;
	append(bytes, meshChunk(mesh));
	append(bytes, hierarchyChunk(modelName, pivots));
	append(bytes, hlodChunk(modelName, modelName, { { std::string(modelName) + ".BoxMesh", 0u } }));
	return bytes;
}

const char kSyntheticLocal73Ini[] =
	"SpecialPower SpecialPower_HeroWeaponMode\n"
	"  Enum = SPECIAL_HERO_MODE\n"
	"  ReloadTime = 1000\n"
	"End\n"
	"SpecialPower SpecialPower_HeroAbilityMode\n"
	"  Enum = SPECIAL_ATHELAS\n"
	"  ReloadTime = 1000\n"
	"End\n"
	"Object ModeFighter\n"
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK HERO\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY SlowSword\n"
	"    Weapon = SECONDARY BowWeapon\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 200\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = SpecialPowerModule ModuleTag_WMSP\n"
	"    SpecialPowerTemplate = SpecialPower_HeroWeaponMode\n"
	"  End\n"
	"  Behavior = WeaponModeSpecialPowerUpdate ModuleTag_WMU\n"
	"    SpecialPowerTemplate = SpecialPower_HeroWeaponMode\n"
	"    Duration = 2000\n"
	"    AttributeModifier = None\n"
	"    LockWeaponSlot = SECONDARY\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"End\n"
	"Object DefaultOnlyHero\n"
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK HERO\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY SlowSword\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 200\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = SpecialPowerModule ModuleTag_HMSP\n"
	"    SpecialPowerTemplate = SpecialPower_HeroAbilityMode\n"
	"  End\n"
	"  Behavior = HeroModeSpecialAbilityUpdate ModuleTag_HMU\n"
	"    SpecialPowerTemplate = SpecialPower_HeroAbilityMode\n"
	"    HeroEffectDuration = 2000\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"End\n"
	"Object ModeMember\n"
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY SlowSword\n"
	"    Weapon = SECONDARY BowWeapon\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"End\n"
	"Object ModeHorde\n"
	"  KindOf = HORDE LARGE_RECTANGLE_PATHFIND SELECTABLE CAN_ATTACK\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 30\n"
	"  GeometryMinorRadius = 45\n"
	"  GeometryHeight = 20\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY HordeRangefinder\n"
	"    Weapon = SECONDARY HordeRangefinder\n"
	"  End\n"
	"  WeaponSet\n"
	"    Conditions = WEAPONSET_HERO_MODE\n"
	"    Weapon = PRIMARY HordeRangefinder\n"
	"  End\n"
	"  Body = ImmortalBody ModuleTag_Body\n"
	"    MaxHealth = 1\n"
	"  End\n"
	"  Behavior = HordeAIUpdate ModuleTag_AI\n"
	"  End\n"
	"  Behavior = HordeContain ModuleTag_Contain\n"
	"    RankInfo = RankNumber:1 UnitType:ModeMember Position:X:30 Y:0 Position:X:-30 Y:0\n"
	"    InitialPayload = ModeMember 2\n"
	"  End\n"
	"  Behavior = SpecialPowerModule ModuleTag_HWMSP\n"
	"    SpecialPowerTemplate = SpecialPower_HeroWeaponMode\n"
	"  End\n"
	"  Behavior = WeaponModeSpecialPowerUpdate ModuleTag_HWMU\n"
	"    SpecialPowerTemplate = SpecialPower_HeroWeaponMode\n"
	"    Duration = 2000\n"
	"    LockWeaponSlot = SECONDARY\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = HordeLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 50\n"
	"  End\n"
	"End\n";
} // namespace

TEST_CASE("LOCAL-73 P-139: ObjectShadow::buildDrawableDecalShadows emits per visible non-None entry and handles Model=Invisible shadow vs Model=None")
{
	MemoryFileSource fs;
	fs.Add("art\\w3d\\ho\\hostbox.w3d", makeTwoBoneW3d("HostBox", "BONE_ATTACH", 12.0f, 0.0f, 0.0f));
	fs.Add("art\\w3d\\ch\\childbox.w3d", makeTwoBoneW3d("ChildBox", "BONE_TIP", 0.0f, 4.0f, 0.0f));
	fs.Add("art\\w3d\\in\\invisible.w3d", makeTwoBoneW3d("Invisible", "BONE_X", 0.0f, 0.0f, 0.0f));

	objtest::World w;
	W3DDrawModules::registerTypedDrawModuleData(w.modules);
	const std::string ini =
		"Object MultiDrawDecalObj\n"
		"  Shadow = SHADOW_ALPHA_DECAL\n"
		"  ShadowSizeX = 20\n"
		"  ShadowSizeY = 20\n"
		"  ShadowTexture = maindecal\n"
		"  Draw = W3DScriptedModelDraw ModuleTag_Main\n"
		"    DefaultModelConditionState\n"
		"      Model = HostBox\n"
		"    End\n"
		"  End\n"
		"  Draw = W3DScriptedModelDraw ModuleTag_Attach\n"
		"    AttachToBoneInAnotherModule = BONE_ATTACH\n"
		"    DefaultModelConditionState\n"
		"      Model = ChildBox\n"
		"      Shadow = SHADOW_ADDITIVE_DECAL\n"
		"      ShadowSizeX = 30\n"
		"      ShadowSizeY = 30\n"
		"      ShadowTexture = childdecal\n"
		"    End\n"
		"  End\n"
		"  Draw = W3DScriptedModelDraw ModuleTag_Invis\n"
		"    DefaultModelConditionState\n"
		"      Model = Invisible\n"
		"      Shadow = SHADOW_ALPHA_DECAL_DYNAMIC\n"
		"      ShadowSizeX = 40\n"
		"      ShadowSizeY = 40\n"
		"      ShadowOpacityStart = 100\n"
		"      ShadowOpacityPeak = 200\n"
		"      ShadowOpacityEnd = 100\n"
		"      ShadowOpacityFadeInTime = 1000\n"
		"      ShadowOpacityFadeOutTime = 0\n"
		"      ShadowTexture = invisdecal\n"
		"    End\n"
		"  End\n"
		"  Draw = W3DScriptedModelDraw ModuleTag_None\n"
		"    DefaultModelConditionState\n"
		"      Model = None\n"
		"    End\n"
		"  End\n"
		"End\n";
	REQUIRE(w.load(ini).empty());

	WW3DAssetManager assets(fs);
	W3DLuaDrawScriptHost::Options hostOpt;
	DrawServices services(assets, w.modules, hostOpt);
	std::vector<std::string> problems;
	const ThingTemplate *tpl = w.get("MultiDrawDecalObj");
	REQUIRE(tpl != nullptr);

	Drawable d(services, tpl, (DrawableID)11, (ObjectID)101, {}, problems);
	d.setModelConditionFlags(ModelConditionFlags{});
	const Coord3D pos{ 100.0f, 200.0f, 0.0f };
	d.setPosition(&pos);
	d.setShadowStartFrame(100);

	std::vector<SelectionDecal> out;
	ObjectShadow::buildDrawableDecalShadows(d, out, true, nullptr, 100, nullptr);
	// Main (HostBox), Attach (ChildBox at 100+12=112), Invis (Model=Invisible with condition shadow); None is skipped!
	REQUIRE(out.size() == 3u);
	CHECK(out[0].texture == "maindecal.tga");
	CHECK(out[0].position.x == 100.0f);
	CHECK(out[1].texture == "childdecal.tga");
	CHECK((out[1].style & 0x40u) != 0u); // SHADOW_ADDITIVE_DECAL
	CHECK(out[1].position.x == 112.0f);
	CHECK(out[2].texture == "invisdecal.tga");
	CHECK(((out[2].color >> 24) & 0xFFu) == 100u);

	// Advancing client frame updates dynamic decal on Invis independently
	out.clear();
	ObjectShadow::buildDrawableDecalShadows(d, out, true, nullptr, 130, nullptr);
	REQUIRE(out.size() == 3u);
	CHECK(((out[2].color >> 24) & 0xFFu) == 200u);
}

TEST_CASE("LOCAL-73 P-141 & P-144: WeaponModeSpecialPowerUpdate locks weapons and ObjectWeapons::hasWeaponSetFor matches default None set (both editions)")
{
	struct PowerWorld : CombatWorld
	{
		SpecialPowerStore powers;
		SpecialPowerStore *savedPowers = TheSpecialPowerStore;
		FXListStore fxLists;
		FXListStore *savedFx = TheFXListStore;
		AttributeModifierStore modifiers;
		AttributeModifierStore *savedModifiers = TheAttributeModifierStore;
		explicit PowerWorld(const char *extra) : CombatWorld("")
		{
			TheSpecialPowerStore = &powers;
			TheFXListStore = &fxLists;
			TheAttributeModifierStore = &modifiers;
			modifiers.registerBlock(w.fx.env.blocks);
			w.fx.env.blocks.registerBlock("FXList", [](INI *ini) { ParseFXListDefinitionGlobal(ini); });
			w.fx.env.blocks.registerBlock("SpecialPower", [](INI *ini) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(ini); });
			SpecialAbilityModules::registerAll(w.modules);
			SpecialPowerModules::registerAll(w.modules);
			const std::string loadErr = w.load(extra, INI_LOAD_OVERWRITE, "local73_powers.ini");
			REQUIRE_MESSAGE(loadErr.empty(), loadErr);
		}
		~PowerWorld()
		{
			TheSpecialPowerStore = savedPowers;
			TheFXListStore = savedFx;
			TheAttributeModifierStore = savedModifiers;
		}
	};
	PowerWorld w(kSyntheticLocal73Ini);

	// P-141 on hero + horde container
	Object *hero = w.unit("ModeFighter", 'A', 200, 200);
	REQUIRE(hero->getWeapons() != nullptr);
	CHECK_FALSE(hero->getWeapons()->isCurWeaponLocked());

	SpecialPowerModuleInterface *spHero = nullptr;
	WeaponModeSpecialPowerUpdate *wmHero = nullptr;
	for (const auto &m : hero->modules())
	{
		if (auto *sp = dynamic_cast<SpecialPowerModuleInterface *>(m.get())) spHero = sp;
		if (auto *wm = dynamic_cast<WeaponModeSpecialPowerUpdate *>(m.get())) wmHero = wm;
	}
	REQUIRE(spHero != nullptr);
	REQUIRE(wmHero != nullptr);
	wmHero->initiateIntentToDoSpecialPower(spHero->getSpecialPowerTemplate(), hero, hero->getPosition(), 0u, 0);
	CHECK(wmHero->active());
	CHECK(hero->getWeapons()->isCurWeaponLocked());
	CHECK(hero->getWeapons()->currentWeapon()->getTemplate()->getName() == "BowWeapon");

	Object *horde = w.unit("ModeHorde", 'A', 400, 200);
	const ContainModuleInterface::ContainedItemsList *members = horde->getContain()->getContainedItemsList();
	REQUIRE(members != nullptr);
	REQUIRE(members->size() == 2u);
	WeaponModeSpecialPowerUpdate *wmHorde = nullptr;
	for (const auto &m : horde->modules())
	{
		if (auto *wm = dynamic_cast<WeaponModeSpecialPowerUpdate *>(m.get())) wmHorde = wm;
	}
	REQUIRE(wmHorde != nullptr);
	wmHorde->initiateIntentToDoSpecialPower(wmHorde->getSpecialPowerTemplate(), horde, horde->getPosition(), 0u, 0);
	CHECK(horde->getWeapons()->isCurWeaponLocked());
	for (const Object *m : *members)
	{
		CHECK(m->getWeapons()->isCurWeaponLocked());
		CHECK(m->getWeapons()->currentWeapon()->getTemplate()->getName() == "BowWeapon");
	}
	// Run past 2000 ms (10 logic frames) to verify unlock
	w.runUntil([]() { return false; }, 15);
	CHECK_FALSE(wmHero->active());
	CHECK_FALSE(hero->getWeapons()->isCurWeaponLocked());
	CHECK_FALSE(horde->getWeapons()->isCurWeaponLocked());
	for (const Object *m : *members)
	{
		CHECK_FALSE(m->getWeapons()->isCurWeaponLocked());
	}

	// P-144: Hero with only default (Conditions = None) WeaponSet reports hasWeaponSetFor(WEAPONSET_HERO_MODE == 27) true
	Object *defaultHero = w.unit("DefaultOnlyHero", 'A', 600, 200);
	REQUIRE(defaultHero->getWeapons() != nullptr);
	CHECK(defaultHero->getWeapons()->hasWeaponSetFor(27));
	CHECK(defaultHero->getWeapons()->weaponInSlot(0)->getTemplate()->getName() == "SlowSword");
}

TEST_CASE("LOCAL-73 P-146: W3DTornadoDraw sets (1.5,1.5,1.5) bound custom tint and skips i=0 radius<=0 shadow decal (both editions)")
{
	objtest::World w;
	W3DTornadoDraw::registerClass(w.modules);
	const std::string ini =
		"Object Local73TornadoObj\n"
		"  Draw = W3DTornadoDraw ModuleTag_Tornado\n"
		"    DecalTemplate\n"
		"      Texture = EXWhirlwindground\n"
		"      Style = SHADOW_ALPHA_DECAL\n"
		"      OpacityMin = 10%\n"
		"      OpacityMax = 50%\n"
		"      MaxRadius = 100.0\n"
		"      RotationsPerMinute = 1.0\n"
		"      SpiralAcceleration = 2.0\n"
		"    End\n"
		"    DecalCount = 7\n"
		"    DecalMaxRadius = 300.0\n"
		"  End\n"
		"End\n";
	REQUIRE(w.load(ini).empty());
	w3dsynth::MemoryFileSource fs;
	WW3DAssetManager assets(fs);
	W3DLuaDrawScriptHost::Options hostOpt;
	DrawServices services(assets, w.modules, hostOpt);
	std::vector<std::string> problems;
	const ThingTemplate *tpl = w.get("Local73TornadoObj");
	REQUIRE(tpl != nullptr);

	Drawable d(services, tpl, (DrawableID)21, (ObjectID)201, {}, problems);
	REQUIRE(d.entries().size() == 1u);
	REQUIRE(d.entries()[0].tornadoDraw != nullptr);
	const Coord3D pos{ 300.0f, 400.0f, 0.0f };
	d.setPosition(&pos);
	d.friend_boundToObject();

	CHECK(d.testTintStatus(Drawable::TINT_STATUS_CUSTOM));
	W3DTornadoDraw *td = d.entries()[0].tornadoDraw.get();
	CHECK(td->decalCount() == 7u);
	REQUIRE(td->decals().size() == 7u);
	CHECK_FALSE(td->decals()[0].hasShadow);
	CHECK(td->decals()[1].hasShadow);

	std::vector<SelectionDecal> out;
	td->appendSelectionDecals(201, 21, true, 0xFFFFFFu, 10, true, out);
	CHECK(out.size() == 6u);
	for (const SelectionDecal &sd : out)
	{
		CHECK(sd.size > 0.0f); // tolerance: expanding spiral rings each frame
		CHECK(sd.angle > -3.1416f); // tolerance: normalized into (-pi, pi]
		CHECK(sd.angle <= 3.1416f); // tolerance: normalized into (-pi, pi]
	}
}

TEST_CASE("LOCAL-73 P-149: Drawable::swirlInfluence computes B_TORNADO tangent on MOVE_ONLY drawable and avoids S-193 (both editions)")
{
	MemoryFileSource fs;
	fs.Add("art\\w3d\\sw\\swirlcone.w3d", makeTwoBoneW3d("SwirlCone", "B_TORNADO01", 0.0f, 0.0f, 5.0f));
	objtest::World w;
	W3DDrawModules::registerTypedDrawModuleData(w.modules);
	const std::string ini =
		"Object SwirlMoveOnly\n"
		"  KindOf = IMMOBILE MOVE_ONLY\n"
		"  Draw = W3DScriptedModelDraw ModuleTag_Draw\n"
		"    DefaultModelConditionState\n"
		"      Model = SwirlCone\n"
		"    End\n"
		"  End\n"
		"End\n"
		"Object SwirlPlain\n"
		"  KindOf = IMMOBILE\n"
		"  Draw = W3DScriptedModelDraw ModuleTag_Draw\n"
		"    DefaultModelConditionState\n"
		"      Model = SwirlCone\n"
		"    End\n"
		"  End\n"
		"End\n";
	REQUIRE(w.load(ini).empty());
	WW3DAssetManager assets(fs);
	W3DLuaDrawScriptHost::Options hostOpt;
	DrawServices services(assets, w.modules, hostOpt);
	std::vector<std::string> problems;

	Drawable moveOnlyD(services, w.get("SwirlMoveOnly"), (DrawableID)31, (ObjectID)301, {}, problems);
	moveOnlyD.setModelConditionFlags(ModelConditionFlags{});
	const Coord3D dpos{ 500.0f, 500.0f, 0.0f };
	moveOnlyD.setPosition(&dpos);
	Coord3D tangent{};
	// Particle at (510, 500, 5) => rel (10, 0, 5), bone at (0, 0, 5) => delta (-10, 0, 0) => normal (-1, 0) => tangent (0, +1, 0)
	CHECK(moveOnlyD.swirlInfluence(Coord3D{ 510.0f, 500.0f, 5.0f }, 1u, tangent));
	CHECK(tangent.x == 0.0f);
	CHECK(tangent.y == 1.0f);

	Drawable plainD(services, w.get("SwirlPlain"), (DrawableID)32, (ObjectID)302, {}, problems);
	plainD.setModelConditionFlags(ModelConditionFlags{});
	plainD.setPosition(&dpos);
	CHECK_FALSE(plainD.swirlInfluence(Coord3D{ 510.0f, 500.0f, 5.0f }, 1u, tangent));

	// Attached particle system with Swirly = Yes using drawable ID produces zero S-193 stops
	struct SwirlEnv : fxtest::TestEnv
	{
		Drawable *target = nullptr;
		FXParticleSystem::ParticleAttachInfo attachedDrawable(std::uint32_t did, const std::string &) override
		{
			FXParticleSystem::ParticleAttachInfo info;
			if (target && did == target->getID())
			{
				info.found = true;
				info.position = *target->getPosition();
				info.transform.Set_Translation(Vector3(info.position.x, info.position.y, info.position.z));
			}
			return info;
		}
		FXParticleSystem::ParticleAttachInfo swirlInfluence(std::uint32_t did, const Coord3D &p) override
		{
			FXParticleSystem::ParticleAttachInfo info;
			if (target && did == target->getID())
			{
				info.found = true;
				info.hasInfluence = target->swirlInfluence(p, 1u, info.swirlTangent);
			}
			return info;
		}
	};
	fxtest::Sim sim(
		"FXParticleSystem Swirl73\n"
		"  System\n"
		"    Priority = ALWAYS_RENDER\n"
		"    BurstCount = 1 1\n"
		"    BurstDelay = 100 100\n"
		"    Lifetime = 10 10\n"
		"  End\n"
		"  EmissionVolume = PointEmissionVolume\n"
		"  End\n"
		"  EmissionVelocity = OrthoEmissionVelocity\n"
		"    X = 1 1\n"
		"  End\n"
		"  Physics = DefaultPhysics\n"
		"    Swirly = Yes\n"
		"    VelocityDamping = 1 1\n"
		"  End\n"
		"  Draw = DefaultDraw\n"
		"  End\n"
		"End\n");
	SwirlEnv swirlEnv;
	swirlEnv.target = &moveOnlyD;
	FXParticleSystem::ParticleSystemManager swirlMgr(sim.store, swirlEnv);
	const FXParticleSystem::ParticleSystemTemplate *st = sim.store.findTemplate("Swirl73");
	REQUIRE(st != nullptr);
	const FXParticleSystem::ParticleSystemID sysId = swirlMgr.createParticleSystem(st, true);
	FXParticleSystem::ParticleSystem *sys = swirlMgr.findParticleSystemByID(sysId);
	REQUIRE(sys != nullptr);
	sys->setPosition(Coord3D{ 510.0f, 500.0f, 5.0f });
	sys->attachToDrawable(moveOnlyD.getID());
	++swirlEnv.frame;
	swirlMgr.update(true, 0);
	++swirlEnv.frame;
	swirlMgr.update(true, 0);
	CHECK(swirlMgr.unverified().empty());
	REQUIRE(sys->particleCount() == 1u);
	const FXParticleSystem::Particle &pt = swirlMgr.particle(sys->firstParticle());
	// Emitted at (510,500,5) with vel (1,0,0), swirled along tangent (0,1,0) by approxLength3D(vel)*4 = 4
	CHECK(pt.pos.x > 510.0f); // tolerance: moves in +X along initial emission velocity
	CHECK(pt.pos.y > 503.0f); // tolerance: swirled along +Y by tangent
}

TEST_CASE("LOCAL-73 P-147: entryTransform composes scaled host bone without double scaling")
{
	MemoryFileSource fs;
	fs.Add("art\\w3d\\ho\\hostarm.w3d", makeTwoBoneW3d("HostArm", "BONE_HAND", 10.0f, 0.0f, 0.0f));
	fs.Add("art\\w3d\\sw\\sword.w3d", makeTwoBoneW3d("Sword", "FIREBONE01", 0.0f, 5.0f, 0.0f));
	objtest::World w;
	W3DDrawModules::registerTypedDrawModuleData(w.modules);
	const std::string ini =
		"Object TwoDrawHero\n"
		"  Scale = 2.0\n"
		"  Draw = W3DScriptedModelDraw ModuleTag_Body\n"
		"    DefaultModelConditionState\n"
		"      Model = HostArm\n"
		"    End\n"
		"  End\n"
		"  Draw = W3DScriptedModelDraw ModuleTag_Sword\n"
		"    AttachToBoneInAnotherModule = BONE_HAND\n"
		"    DefaultModelConditionState\n"
		"      Model = Sword\n"
		"    End\n"
		"  End\n"
		"End\n";
	REQUIRE(w.load(ini).empty());
	WW3DAssetManager assets(fs);
	W3DLuaDrawScriptHost::Options hostOpt;
	DrawServices services(assets, w.modules, hostOpt);
	std::vector<std::string> problems;
	Drawable d(services, w.get("TwoDrawHero"), (DrawableID)41, (ObjectID)401, {}, problems);
	d.setModelConditionFlags(ModelConditionFlags{});
	const Coord3D pos{ 100.0f, 200.0f, 0.0f };
	d.setPosition(&pos);
	REQUIRE(d.entries().size() == 2u);

	Matrix3D entryWorld;
	REQUIRE(d.entryTransform(d.entries()[1], entryWorld));
	// BONE_HAND is at x=10 * Scale(2.0) = 20 => world x = 120
	CHECK(entryWorld.Row[0][3] == 120.0f);
	CHECK(entryWorld.Row[1][3] == 200.0f);

	// Evaluating child FIREBONE01 at scale=1.0 inside entryWorld places it at (120, 205, 0) rather than (120, 210, 0)
	Matrix3D childLocal;
	REQUIRE(ParticleBoneFollow::poseBone(*d.entries()[1].draw, 1.0f, d.entries()[1].draw->frame().model->Tree->Get_Bone_Index("firebone01"), childLocal));
	Matrix3D combined;
	Matrix3D::Multiply(entryWorld, childLocal, &combined);
	CHECK(combined.Row[0][3] == 120.0f);
	CHECK(combined.Row[1][3] == 210.0f);
}

TEST_CASE("LOCAL-73 ennorath: Imladris abilities, tornado decals, whirlpool Swirly FX, and MATRIX.tsv reproducers")
{
	ennorathscenario::World world;
	if (!world.load("LOCAL-73 Ennorath Imladris visual & ability reproducers"))
	{
		return;
	}

	hudtest::Rig game(world.shared, "map mp fall back 8p", "FactionImladris");
	game.game->shroud().revealMapForPlayerPermanently(game.index());
	FXPlayback playback(*world.merged.fs, RandomAlgorithm::RotWK_GameDat_LCG);
	const std::vector<std::string> fxErrors = playback.loadRetailData(world.shared.world->report().macrosBeforeFX);
	CHECK(fxErrors.empty());
	LiveFX liveFX(*game.game, playback);
	playback.seedClientRandom(123);

	std::string err;

	// 1) P-146: ElrondWhirlwind W3DTornadoDraw decals + custom tint
	Object *whirlwind = game.game->createObject("ElrondWhirlwind", game.index(), Coord3D{ 1200.0f, 1200.0f, 0.0f }, 0.0f, &err);
	REQUIRE_MESSAGE(whirlwind != nullptr, err);
	game.logic().runLogicFrame();
	game.game->refreshClient(1000.0 / 30.0, 1.0);
	liveFX.flushPending();
	liveFX.updateAttachedSystems();
	Drawable *whirlwindD = game.game->drawables().findByObject(whirlwind->getID());
	REQUIRE(whirlwindD != nullptr);
	CHECK(whirlwindD->testTintStatus(Drawable::TINT_STATUS_CUSTOM));
	bool hasTornadoDraw = false;
	for (const DrawEntry &e : whirlwindD->entries())
	{
		if (e.tornadoDraw)
		{
			hasTornadoDraw = true;
			std::vector<SelectionDecal> decals;
			e.tornadoDraw->appendSelectionDecals(whirlwind->getID(), whirlwindD->getID(), true, 0xFFFFFFu, 5, true, decals);
			CHECK(decals.size() == 6u);
		}
	}
	CHECK(hasTornadoDraw);

	// 2) P-149: CirdanWhirlpool (EXWhirl02, 9 ParticleSysBone, B_TORNADO01) + CirdanWhirlpoolSmall (EXWhirl fallback)
	Object *whirlpool = game.game->createObject("CirdanWhirlpool", game.index(), Coord3D{ 1400.0f, 1200.0f, 0.0f }, 0.0f, &err);
	REQUIRE_MESSAGE(whirlpool != nullptr, err);
	Object *whirlpoolSmall = game.game->createObject("CirdanWhirlpoolSmall", game.index(), Coord3D{ 1600.0f, 1200.0f, 0.0f }, 0.0f, &err);
	REQUIRE_MESSAGE(whirlpoolSmall != nullptr, err);

	game.logic().runLogicFrame();
	game.game->refreshClient(1000.0 / 30.0, 1.0);
	liveFX.flushPending();
	liveFX.updateAttachedSystems();
	const Drawable *whirlpoolD = game.game->drawables().findByObject(whirlpool->getID());
	const Drawable *whirlpoolSmallD = game.game->drawables().findByObject(whirlpoolSmall->getID());
	REQUIRE(whirlpoolD != nullptr);
	REQUIRE(whirlpoolSmallD != nullptr);
	REQUIRE(!whirlpoolD->entries().empty());
	REQUIRE(whirlpoolD->entries()[0].draw != nullptr);
	CHECK(whirlpoolD->entries()[0].draw->frame().model != nullptr);
	CHECK(whirlpoolD->isKindOfName("MOVE_ONLY"));
	CHECK(whirlpoolSmallD->isKindOfName("MOVE_ONLY"));

	int wpSystems = 0;
	int wpSmallSystems = 0;
	playback.particles().forEachSystem([&](const FXParticleSystem::ParticleSystem &sys) {
		if (sys.attachedDrawableID() == whirlpoolD->getID())
		{
			++wpSystems;
		}
		if (sys.attachedDrawableID() == whirlpoolSmallD->getID())
		{
			++wpSmallSystems;
		}
	});
	CHECK(wpSystems == 9);
	CHECK(wpSmallSystems == 4);

	for (int f = 0; f < 10; ++f)
	{
		game.logic().runLogicFrame();
		game.game->refreshClient(1000.0 / 30.0, 1.0);
		liveFX.flushPending();
		liveFX.updateAttachedSystems();
		for (int c = 0; c < 6; ++c)
		{
			playback.step();
		}
	}
	for (const std::string &u : playback.particles().unverified())
	{
		CHECK_MESSAGE(u.find("S-193") == std::string::npos, u);
	}

	// 3) P-144 & P-147: ImladrisGwaihir + ImladrisZwillingeEdain
	Object *gwaihir = game.game->createObject("ImladrisGwaihir", game.index(), Coord3D{ 1100.0f, 1400.0f, 0.0f }, 0.0f, &err);
	REQUIRE_MESSAGE(gwaihir != nullptr, err);
	REQUIRE(gwaihir->getWeapons() != nullptr);
	CHECK(gwaihir->getWeapons()->hasWeaponSetFor(27)); // WEAPONSET_HERO_MODE -> 0x691014 -> 0x73D89F

	Object *twins = game.game->createObject("ImladrisZwillingeEdain", game.index(), Coord3D{ 1300.0f, 1400.0f, 0.0f }, 0.0f, &err);
	REQUIRE_MESSAGE(twins != nullptr, err);
	REQUIRE(twins->getWeapons() != nullptr);
	CHECK(twins->getWeapons()->hasWeaponSetFor(27));
	game.logic().runLogicFrame();
	game.game->refreshClient(1000.0 / 30.0, 1.0);
	const Drawable *twinsD = game.game->drawables().findByObject(twins->getID());
	REQUIRE(twinsD != nullptr);
	bool foundAttachedDraw = false;
	for (const DrawEntry &e : twinsD->entries())
	{
		if (!e.attachToBone.empty())
		{
			foundAttachedDraw = true;
			Matrix3D em;
			CHECK(twinsD->entryTransform(e, em));
		}
	}
	CHECK(foundAttachedDraw);

	// 4) P-139: Ennorath decal objects emit per-entry decals through ObjectShadow::buildDrawableDecalShadows
	Object *elrond = game.game->createObject("ImladrisElrond", game.index(), Coord3D{ 1700.0f, 1400.0f, 0.0f }, 0.0f, &err);
	REQUIRE_MESSAGE(elrond != nullptr, err);
	game.logic().runLogicFrame();
	game.game->refreshClient(1000.0 / 30.0, 1.0);
	const Drawable *elrondD = game.game->drawables().findByObject(elrond->getID());
	REQUIRE(elrondD != nullptr);
	std::vector<SelectionDecal> elrondDecals;
	ObjectShadow::buildDrawableDecalShadows(*elrondD, elrondDecals, true, nullptr, playback.clientFrame(), elrond);
	CHECK(elrondDecals.size() == 2u);

	// 5) MATRIX.tsv:2206 & MATRIX.tsv:2238-2239 reproducers
	const WeaponTemplate *elrondFlood = world.shared.world->weaponStores().weapons().findWeaponTemplate("ElrondPersonalFlood");
	REQUIRE(elrondFlood != nullptr);
	bool hasFireLogic = false;
	bool hasOcl = false;
	for (const std::shared_ptr<WeaponNugget> &n : elrondFlood->m_nuggets)
	{
		if (const auto *fl = dynamic_cast<const FireLogicNugget *>(n.get()))
		{
			hasFireLogic = true;
			CHECK(dynamic_cast<const DamageNugget *>(n.get()) != nullptr);
			CHECK(fl->m_logicType == 1); // DECREASE_BURN_RATE
			CHECK(fl->m_damage == 300.0f);
		}
		if (const auto *on = dynamic_cast<const WeaponOCLNugget *>(n.get()))
		{
			hasOcl = true;
			CHECK(on->m_weaponOCLName == "OCL_ElrondPersonalFlood");
		}
	}
	CHECK(hasFireLogic);
	CHECK(hasOcl);

	// OCL_ManveScreechObject requires Upgrade_TechnologyNoldorArtFirstTome
	REQUIRE(TheObjectCreationListStore != nullptr);
	const ObjectCreationList *screechOcl = TheObjectCreationListStore->findObjectCreationList("OCL_ManveScreechObject");
	REQUIRE(screechOcl != nullptr);
	Object *gwaihirMod = game.game->createObject("ImladrisGwaihirMod", game.index(), Coord3D{ 1500.0f, 1400.0f, 0.0f }, 0.0f, &err);
	REQUIRE_MESSAGE(gwaihirMod != nullptr, err);
	const std::vector<Object *> beforeUpgrade = screechOcl->create(game.logic(), gwaihirMod, *gwaihirMod->getPosition(), gwaihirMod->getPosition(), 0u);
	CHECK(beforeUpgrade.empty()); // gated by Upgrade_TechnologyNoldorArtFirstTome

	game.local->addCompletedUpgrade("Upgrade_TechnologyNoldorArtFirstTome");
	const std::vector<Object *> afterUpgrade = screechOcl->create(game.logic(), gwaihirMod, *gwaihirMod->getPosition(), gwaihirMod->getPosition(), 0u);
	CHECK(!afterUpgrade.empty());
}

TEST_SUITE_END();
