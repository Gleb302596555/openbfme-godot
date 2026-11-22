// OpenBFME unit tests for PORT-324 (issue #841): client drawing, Drawable, W3D, HUD, and audio stops.
// Synthetic INI, synthetic W3D meshes, and synthetic snapshots only; no retail files required.
// RW citations:
//   S-5204: RW 0x4B7C33 (W3DModelDraw::getInstanceTransform NoRotate identity rotation after world * m_instanceMatrix)
//   S-830:  RW 0x73501F -> Drawable::showSubObject RW 0x672823 -> vslot 0x88 RW 0x4C3B25 (CurDrawableHide/ShowSubObject across sibling draws)
//   S-484:  RW 0x8B9247 / 0x8B935D -> Drawable::clearRecolorHouse RW 0x672713 -> RW 0x4B86F2 (per-mesh recolour clear on grant/removal)
//   S-6013 / S-4612: RW 0x4C455B (OverrideTooltip) / RW 0x8BBBCC / 0x8BBC0A -> RW 0x6796DA / 0x674FAE -> UnitHelpSource RW 0x8EBCEC / 0x8EC0C7
//   S-1950: RW 0x67646C -> RW 0x8AE8F0 -> WallUpgradeUpdate::isWallUpgradeKind RW 0x8AEAA9
//   S-1951: RW 0x6A6FA0..0x6A7192 / RW 0x69ED62..0x69EDA1 (InGameUI/Language DrawableCaptionFont/PointSize/Bold/Color -> RW 0x677CF9)
//   S-706:  RW 0x676A8A rows 37..41 (SoundSelectors), RW 0x6778A9 -> 0x6760DA (under-construction pristine fallback), RW 0x678816 / 0x6717C4 (SoundAmbientBattle)
//   S-1001: RW 0x85EE0F..0x85EE7A / RW 0x6AAC52 (CAMOUFLAGE launcher stealth fade branch)
//   S-4974: RW 0x4E5631 / RW 0x4E44FB (W3DFloorBuffer::Draw -> terrain.fxo Default RenderingMode 3 with tod.terrain)

#include "doctest.h"

#include "CombatTestUtil.h"
#include "EconTestUtil.h"
#include "W3dSynth.h"

#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/Upgrade.h"
#include "GameClient/ClientEvents.h"
#include "GameClient/CommandButtonHelp.h"
#include "GameClient/Drawable.h"
#include "GameClient/DrawableIconUI.h"
#include "GameClient/DrawableManager.h"
#include "GameClient/GameTextTableSource.h"
#include "GameClient/InGameUIMessages.h"
#include "GameClient/LiveGameAudio.h"
#include "GameClient/LogicSnapshot.h"
#include "GameClient/MapChunks.h"
#include "GameClient/SoundSelectorClientBehavior.h"
#include "GameClient/TacticalView.h"
#include "GameClient/UnitHelpSource.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DDrawModules.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DScriptedModelDraw.h"
#include "GameEngineDevice/W3DDevice/GameClient/W3DObjectLighting.h"
#include "GameLogic/ContainParseHooks.h"
#include "GameLogic/Module/ActiveBody.h"
#include "GameLogic/Module/WallUpgradeUpdate.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/System/ShroudManager.h"
#include "Libraries/WWVegas/WW3D2/assetmgr.h"
#include "Libraries/WWVegas/WW3D2/w3dshadergen.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>
#include <vector>

namespace
{
void buildSimpleW3d(w3dsynth::MemoryFileSource &fs, const std::string &path, const std::string &container, const std::vector<std::string> &meshes)
{
	using namespace w3dsynth;
	std::vector<std::uint8_t> bytes;
	std::vector<SynthSub> subs;
	for (const std::string &name : meshes)
	{
		SynthMesh mesh;
		mesh.Container = container;
		mesh.Name = name;
		mesh.Vertices = { { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 } };
		mesh.Triangles = { 0, 1, 2 };
		append(bytes, meshChunk(mesh));
		subs.push_back({ container + "." + name, 0 });
	}
	append(bytes, hlodChunk(container.c_str(), "UnusedSkel", subs));
	fs.Add(path, bytes);
}

int countVaryingLines(const std::string &shader)
{
	int count = 0;
	std::istringstream in(shader);
	std::string ln;
	while (std::getline(in, ln))
	{
		if (ln.rfind("varying ", 0) == 0)
		{
			++count;
		}
	}
	return count;
}

std::u16string asciiU16(const std::string &s)
{
	return std::u16string(s.begin(), s.end());
}
} // namespace

TEST_CASE("PORT-324 S-5204: NoRotate resets full rotation after world * m_instanceMatrix while keeping scale and translation (RW 0x4B7C33)")
{
	const char *kIni =
		"Object P324NoRotateObj\n"
		"  KindOf = INFANTRY SELECTABLE\n"
		"  Scale = 2.0\n"
		"  Draw = W3DScriptedModelDraw DrawFixed\n"
		"    NoRotate = Yes\n"
		"    DefaultModelConditionState\n"
		"      Model = P324A\n"
		"    End\n"
		"  End\n"
		"  Draw = W3DScriptedModelDraw DrawRotated\n"
		"    DefaultModelConditionState\n"
		"      Model = P324A\n"
		"    End\n"
		"  End\n"
		"  Body = ActiveBody BodyTag\n"
		"    MaxHealth = 100\n"
		"  End\n"
		"End\n";
	econtest::Fx f("");
	w3dsynth::MemoryFileSource fs;
	buildSimpleW3d(fs, "art\\w3d\\p3\\p324a.w3d", "P324A", { "Body" });
	W3DDrawModules::registerTypedDrawModuleData(f.w.modules);
	REQUIRE_MESSAGE(f.w.load(kIni).empty(), "load");
	WW3DAssetManager assets(fs);
	DrawableManager dm(assets, *f.logic);
	ClientEventRecorder rec(*f.logic);
	f.logic->setClientHooks(&rec);
	Object *obj = f.makeAt("P324NoRotateObj", "Alice", 10.0f, 20.0f);
	obj->setOrientation(1.1f);
	rec.take();
	// Re-create via event at rotated orientation + non-identity m_instanceMatrix (sway / pitch tilt)
	Drawable *d = dm.findByObject(obj->getID());
	if (!d)
	{
		ClientEvent e;
		e.kind = ClientEvent::CREATED;
		e.object = obj->getID();
		e.tmpl = obj->getTemplate();
		e.position = *obj->getPosition();
		e.angle = 1.1f;
		dm.applyEvents({ e });
		d = dm.findByObject(obj->getID());
	}
	REQUIRE(d != nullptr);
	REQUIRE(d->entries().size() == 2);

	Matrix3D inst;
	// 90 deg rotation about Z + local translation (3, 4, 5)
	inst.Row[0][0] = 0.0f; inst.Row[0][1] = -1.0f; inst.Row[0][2] = 0.0f; inst.Row[0][3] = 3.0f;
	inst.Row[1][0] = 1.0f; inst.Row[1][1] = 0.0f;  inst.Row[1][2] = 0.0f; inst.Row[1][3] = 4.0f;
	inst.Row[2][0] = 0.0f; inst.Row[2][1] = 0.0f;  inst.Row[2][2] = 1.0f; inst.Row[2][3] = 5.0f;
	d->setInstanceMatrix(inst);

	Matrix3D fixedXf, rotXf;
	CHECK_FALSE(d->entryTransform(d->entries()[0], fixedXf));
	CHECK_FALSE(d->entryTransform(d->entries()[1], rotXf));

	// DrawFixed: identity rotation * scale (2.0), non-trivial translated origin preserved from world * instanceMatrix
	CHECK(fixedXf.Row[0][0] == doctest::Approx(2.0f).epsilon(1e-5)); // tolerance: float32 matrix scale
	CHECK(fixedXf.Row[0][1] == doctest::Approx(0.0f).epsilon(1e-5)); // tolerance: zero off-diagonal rotation
	CHECK(fixedXf.Row[1][0] == doctest::Approx(0.0f).epsilon(1e-5)); // tolerance: zero off-diagonal rotation
	CHECK(fixedXf.Row[1][1] == doctest::Approx(2.0f).epsilon(1e-5)); // tolerance: float32 matrix scale
	CHECK(fixedXf.Row[2][2] == doctest::Approx(2.0f).epsilon(1e-5)); // tolerance: float32 matrix scale
	CHECK(fixedXf.Row[0][3] == doctest::Approx(13.0f).epsilon(1e-4)); // tolerance: 10 + 3 (identity world * inst translation)
	CHECK(fixedXf.Row[1][3] == doctest::Approx(24.0f).epsilon(1e-4)); // tolerance: 20 + 4

	// DrawRotated: rotated by obj->angle + inst rotation
	CHECK(std::fabs(rotXf.Row[0][0] - 2.0f) > 0.1f); // tolerance: rotated basis diverges from identity * scale
	f.logic->setClientHooks(nullptr);
}

TEST_CASE("PORT-324 S-830: state scripts CurDrawableHide/ShowSubObject[Permanently] forward to sibling draws via Drawable (RW 0x73501F -> 0x672823)")
{
	const char *kIni =
		"Object P324ScriptSubObj\n"
		"  KindOf = STRUCTURE SELECTABLE\n"
		"  Draw = W3DScriptedModelDraw DrawMain\n"
		"    DefaultModelConditionState\n"
		"      Model = P324M\n"
		"    End\n"
		"    IdleAnimationState\n"
		"      BeginScript\n"
		"        CurDrawableHideSubObjectPermanently(\"SHIELD\")\n"
		"      ENDSCRIPT\n"
		"    End\n"
		"    AnimationState = USER_1\n"
		"      BeginScript\n"
		"        CurDrawableShowSubObjectPermanently(\"SHIELD\")\n"
		"      ENDSCRIPT\n"
		"    End\n"
		"  End\n"
		"  Draw = W3DScriptedModelDraw DrawExtra\n"
		"    DefaultModelConditionState\n"
		"      Model = P324E\n"
		"    End\n"
		"  End\n"
		"  Body = ActiveBody BodyTag\n"
		"    MaxHealth = 100\n"
		"  End\n"
		"End\n";
	econtest::Fx f("");
	w3dsynth::MemoryFileSource fs;
	buildSimpleW3d(fs, "art\\w3d\\p3\\p324m.w3d", "P324M", { "BODY" });
	buildSimpleW3d(fs, "art\\w3d\\p3\\p324e.w3d", "P324E", { "SHIELD", "BANNER" });
	W3DDrawModules::registerTypedDrawModuleData(f.w.modules);
	REQUIRE_MESSAGE(f.w.load(kIni).empty(), "load");
	WW3DAssetManager assets(fs);
	DrawableManager dm(assets, *f.logic);
	ClientEventRecorder rec(*f.logic);
	f.logic->setClientHooks(&rec);

	Object *obj = f.makeAt("P324ScriptSubObj", "Alice", 0.0f, 0.0f);
	dm.applyEvents(rec.take());
	Drawable *d = dm.findByObject(obj->getID());
	REQUIRE(d != nullptr);
	REQUIRE(d->entries().size() == 2);
	auto *mainDraw = d->entries()[0].draw.get();
	auto *extraDraw = d->entries()[1].draw.get();
	REQUIRE(mainDraw != nullptr);
	REQUIRE(extraDraw != nullptr);

	// SHIELD on DrawExtra must be hidden by DrawMain's Default BeginScript (flushed after entries finish building)
	CHECK(extraDraw->hiddenSubObjects().size() == 1);
	for (const W3DStopHit &h : mainDraw->stops())
	{
		CHECK(h.Id != "S-830");
	}

	// USER_1 BeginScript calls CurDrawableShowSubObjectPermanently("SHIELD"), revealing SHIELD on DrawExtra
	const int kUser1 = ModelCondition::indexOf("USER_1");
	REQUIRE(kUser1 >= 0); // tolerance: valid bit index
	obj->setModelConditionState(kUser1, true);
	f.logic->runLogicFrame();
	dm.applyEvents(rec.take());
	CHECK(extraDraw->hiddenSubObjects().empty());
	f.logic->setClientHooks(nullptr);
}

TEST_CASE("PORT-324 S-484: SubObjectsUpgrade calls clearRecolorHouse on grant and removal and bumps materialStateGeneration (RW 0x8B9247 / 0x8B935D -> 0x672713)")
{
	const char *kIni =
		"Upgrade Upgrade_P324Recolor\n"
		"  Type = OBJECT\n"
		"End\n"
		"Object P324UpgradeUnit\n"
		"  KindOf = INFANTRY SELECTABLE\n"
		"  Draw = W3DScriptedModelDraw DrawTag\n"
		"    DefaultModelConditionState\n"
		"      Model = P324U\n"
		"    End\n"
		"  End\n"
		"  Body = ActiveBody BodyTag\n"
		"    MaxHealth = 100\n"
		"  End\n"
		"  Behavior = SubObjectsUpgrade UpgradeTag\n"
		"    TriggeredBy = Upgrade_P324Recolor\n"
		"    HideSubObjectsOnRemove = Yes\n"
		"    UpgradeTexture = Orig.tga 0 New.tga\n"
		"    RecolorHouse = Yes\n"
		"  End\n"
		"End\n";
	econtest::Fx f("");
	w3dsynth::MemoryFileSource fs;
	buildSimpleW3d(fs, "art\\w3d\\p3\\p324u.w3d", "P324U", { "Body" });
	W3DDrawModules::registerTypedDrawModuleData(f.w.modules);
	REQUIRE_MESSAGE(f.w.load(kIni).empty(), "load");
	WW3DAssetManager assets(fs);
	DrawableManager dm(assets, *f.logic);
	ClientEventRecorder rec(*f.logic);
	f.logic->setClientHooks(&rec);
	f.player("Alice")->setPlayerColor(0xFF204060u);

	Object *obj = f.makeAt("P324UpgradeUnit", "Alice", 0.0f, 0.0f);
	dm.applyEvents(rec.take());
	Drawable *d = dm.findByObject(obj->getID());
	REQUIRE(d != nullptr);
	auto *draw = d->entries()[0].draw.get();
	REQUIRE(draw != nullptr);
	const std::uint32_t gen0 = draw->materialStateGeneration();

	const UpgradeTemplate *up = TheUpgradeCenter->findUpgrade("Upgrade_P324Recolor");
	REQUIRE(up != nullptr);
	obj->giveUpgrade(up);
	std::vector<ClientEvent> grantEvents = rec.take();
	int clearIdx = -1, texIdx = -1, recolorIdx = -1;
	for (size_t i = 0; i < grantEvents.size(); ++i)
	{
		if (grantEvents[i].kind == ClientEvent::HOUSE_RECOLOR_CLEAR) clearIdx = (int)i;
		if (grantEvents[i].kind == ClientEvent::UPGRADE_TEXTURES) texIdx = (int)i;
		if (grantEvents[i].kind == ClientEvent::HOUSE_RECOLOR) recolorIdx = (int)i;
	}
	CHECK(clearIdx >= 0); // tolerance: event emitted on grant
	CHECK(texIdx == clearIdx + 1);
	CHECK(recolorIdx == texIdx + 1);
	dm.applyEvents(grantEvents);
	CHECK(draw->meshHouseColors().size() == 1);
	const std::uint32_t gen1 = draw->materialStateGeneration();
	CHECK(gen1 != gen0); // tolerance: material generation changed for GodotGameWorld static fast-path

	obj->removeUpgrade(up);
	std::vector<ClientEvent> remEvents = rec.take();
	bool sawClearOnRemove = false;
	for (const ClientEvent &e : remEvents)
	{
		if (e.kind == ClientEvent::HOUSE_RECOLOR_CLEAR) sawClearOnRemove = true;
	}
	CHECK(sawClearOnRemove);
	dm.applyEvents(remEvents);
	CHECK(draw->meshHouseColors().empty());
	CHECK(draw->materialStateGeneration() != gen1); // tolerance: clear bumped generation again
	f.logic->setClientHooks(nullptr);
}

TEST_CASE("PORT-324 S-6013 / S-4612: OverrideTooltip and TooltipUpgrade sync via Drawable (+0x348/+0x34C) to UnitHelpSource::compose (RW 0x4C455B / 0x6796DA / 0x8EBCEC)")
{
	const char *kIni =
		"Upgrade Upgrade_P324Tooltip\n"
		"  Type = OBJECT\n"
		"End\n"
		"Object P324TooltipObj\n"
		"  KindOf = INFANTRY SELECTABLE\n"
		"  DisplayName = OBJECT:BaseName\n"
		"  Description = CONTROLBAR:BaseDesc\n"
		"  Draw = W3DScriptedModelDraw DrawTag\n"
		"    DefaultModelConditionState\n"
		"      Model = P324T\n"
		"    End\n"
		"    ModelConditionState = MOUNTED\n"
		"      Model = P324T\n"
		"      OverrideTooltip = OBJECT:MountedName\n"
		"    End\n"
		"    ModelConditionState = USER_2\n"
		"      Model = P324T\n"
		"      OverrideTooltip = ReferenceDisplayName\n"
		"    End\n"
		"  End\n"
		"  Body = ActiveBody BodyTag\n"
		"    MaxHealth = 100\n"
		"  End\n"
		"  Behavior = TooltipUpgrade TooltipTag\n"
		"    TriggeredBy = Upgrade_P324Tooltip\n"
		"    DisplayName = OBJECT:UpgradedName\n"
		"    Description = CONTROLBAR:UpgradedDesc\n"
		"  End\n"
		"End\n";
	econtest::Fx f("");
	w3dsynth::MemoryFileSource fs;
	buildSimpleW3d(fs, "art\\w3d\\p3\\p324t.w3d", "P324T", { "Body" });
	W3DDrawModules::registerTypedDrawModuleData(f.w.modules);
	REQUIRE_MESSAGE(f.w.load(kIni).empty(), "load");
	WW3DAssetManager assets(fs);
	DrawableManager dm(assets, *f.logic);
	ClientEventRecorder rec(*f.logic);
	f.logic->setClientHooks(&rec);

	GameTextTableSource text;
	text.table.setOverride("OBJECT:BaseName", "Base Knight");
	text.table.setOverride("CONTROLBAR:BaseDesc", "Base Desc");
	text.table.setOverride("OBJECT:MountedName", "Mounted Knight");
	text.table.setOverride("OBJECT:UpgradedName", "Royal Guard");
	text.table.setOverride("CONTROLBAR:UpgradedDesc", "Armoured Desc");

	Object *obj = f.makeAt("P324TooltipObj", "Alice", 0.0f, 0.0f);
	dm.applyEvents(rec.take());
	Drawable *d = dm.findByObject(obj->getID());
	REQUIRE(d != nullptr);
	Player *alice = f.player("Alice");

	// Baseline: template DisplayName and Description
	UnitHelpSource::Text h0 = UnitHelpSource::compose(*obj, &text, *f.logic, alice, d);
	CHECK(h0.title == asciiU16("Base Knight"));
	CHECK(h0.text.find(asciiU16("Base Desc")) != std::u16string::npos);

	// ModelConditionState MOUNTED sets OverrideTooltip -> Drawable::setCustomTooltipDisplayName
	const int kMounted = ModelCondition::indexOf("MOUNTED");
	REQUIRE(kMounted >= 0); // tolerance: valid bit index
	obj->setModelConditionState(kMounted, true);
	f.logic->runLogicFrame();
	dm.applyEvents(rec.take());
	CHECK(d->customTooltipDisplayName() == "OBJECT:MountedName");
	UnitHelpSource::Text hMounted = UnitHelpSource::compose(*obj, &text, *f.logic, alice, d);
	CHECK(hMounted.title == asciiU16("Mounted Knight"));

	// USER_2 OverrideTooltip = ReferenceDisplayName resets custom display name back to template DisplayName
	const int kUser2 = ModelCondition::indexOf("USER_2");
	REQUIRE(kUser2 >= 0); // tolerance: valid bit index
	obj->setModelConditionState(kMounted, false);
	obj->setModelConditionState(kUser2, true);
	f.logic->runLogicFrame();
	dm.applyEvents(rec.take());
	CHECK(d->customTooltipDisplayName().empty());
	CHECK(UnitHelpSource::compose(*obj, &text, *f.logic, alice, d).title == asciiU16("Base Knight"));

	// TooltipUpgrade grant sets both display name and description via SET_CUSTOM_TOOLTIP, removal clears both
	const UpgradeTemplate *up = TheUpgradeCenter->findUpgrade("Upgrade_P324Tooltip");
	REQUIRE(up != nullptr);
	obj->giveUpgrade(up);
	dm.applyEvents(rec.take());
	CHECK(d->customTooltipDisplayName() == "OBJECT:UpgradedName");
	CHECK(d->customTooltipDescription() == "CONTROLBAR:UpgradedDesc");
	UnitHelpSource::Text hUp = UnitHelpSource::compose(*obj, &text, *f.logic, alice, d);
	CHECK(hUp.title == asciiU16("Royal Guard"));
	CHECK(hUp.text.find(asciiU16("Armoured Desc")) != std::u16string::npos);

	obj->removeUpgrade(up);
	dm.applyEvents(rec.take());
	CHECK(d->customTooltipDisplayName().empty());
	CHECK(d->customTooltipDescription().empty());
	UnitHelpSource::Text hRem = UnitHelpSource::compose(*obj, &text, *f.logic, alice, d);
	CHECK(hRem.title == asciiU16("Base Knight"));
	CHECK(hRem.text.find(asciiU16("Base Desc")) != std::u16string::npos);
	f.logic->setClientHooks(nullptr);
}

TEST_CASE("PORT-324 S-1950: WALL_UPGRADE health bar requires WallUpgradeUpdate::isWallUpgradeKind() (RW 0x67646C -> 0x8AEAA9)")
{
	const char *kIni =
		"Object P324WallGate\n"
		"  KindOf = STRUCTURE SELECTABLE WALL_UPGRADE\n"
		"  Body = ActiveBody BodyTag\n"
		"    MaxHealth = 500\n"
		"  End\n"
		"  Behavior = WallUpgradeUpdate ModuleTag_Wall\n"
		"  End\n"
		"End\n"
		"Object P324WallBanner\n"
		"  KindOf = STRUCTURE SELECTABLE WALL_UPGRADE\n"
		"  Body = ActiveBody BodyTag\n"
		"    MaxHealth = 500\n"
		"  End\n"
		"  Behavior = WallUpgradeUpdate ModuleTag_Wall\n"
		"  End\n"
		"End\n"
		"Object P324WallNoModule\n"
		"  KindOf = STRUCTURE SELECTABLE WALL_UPGRADE\n"
		"  Body = ActiveBody BodyTag\n"
		"    MaxHealth = 500\n"
		"  End\n"
		"End\n";
	econtest::Fx f("");
	REQUIRE_MESSAGE(f.w.load(kIni).empty(), "load");
	Object *gate = f.makeAt("P324WallGate", "Alice", 0.0f, 0.0f);
	Object *banner = f.makeAt("P324WallBanner", "Alice", 20.0f, 0.0f);
	Object *noMod = f.makeAt("P324WallNoModule", "Alice", 40.0f, 0.0f);

	const int kPostern = ModelCondition::indexOf("UPGRADE_POSTERN_GATE");
	REQUIRE(kPostern >= 0); // tolerance: valid model condition index
	gate->setModelConditionState(kPostern, true);

	IconUISettings s;
	s.showObjectHealth = true;
	const std::set<ObjectID> selected = { gate->getID(), banner->getID(), noMod->getID() };
	const Player *alice = f.player("Alice");

	// WallUpgradeUpdate with UPGRADE_POSTERN_GATE shows health bar; plain WallUpgradeUpdate without kind condition hides it;
	// absent WallUpgradeUpdate falls through (RW 0x67647D: test eax, eax; je 0x67648E)
	CHECK(wantsHealthBar(*gate, s, selected, INVALID_ID, alice));
	CHECK_FALSE(wantsHealthBar(*banner, s, selected, INVALID_ID, alice));
	CHECK(wantsHealthBar(*noMod, s, selected, INVALID_ID, alice));
}

TEST_CASE("PORT-324 S-1951: DrawableCaptionFont / PointSize / Bold / Color parse from InGameUI and Language.ini (RW 0x6A6FA0 / 0x69ED62)")
{
	const std::string inGameUiNoOverride =
		"InGameUI\n"
		"  DrawableCaptionFont = Times\n"
		"  DrawableCaptionPointSize = 14\n"
		"  DrawableCaptionBold = Yes\n"
		"  DrawableCaptionColor = R:200 G:180 B:160\n"
		"End\n";
	const std::string langPlain =
		"Language\n"
		"End\n";
	InGameUIMessageSettings ms;
	std::string err;
	REQUIRE_MESSAGE(InGameUIMessageSettings::scan(inGameUiNoOverride, langPlain, ms, &err), err);
	CHECK(ms.captionFont == "Times");
	CHECK(ms.captionPointSize == 14);
	CHECK(ms.captionBold == true);
	CHECK(ms.captionColor == 0xFFC8B4A0u);

	const std::string langOverride =
		"Language\n"
		"  DrawableCaptionFont = \"Albertus MT\" 12 No\n"
		"End\n";
	REQUIRE_MESSAGE(InGameUIMessageSettings::scan(inGameUiNoOverride, langOverride, ms, &err), err);
	CHECK(ms.captionFont == "Albertus MT");
	CHECK(ms.captionPointSize == 12);
	CHECK(ms.captionBold == false);
	CHECK(ms.captionColor == 0xFFC8B4A0u);

	// DrawableIconUI emits construction text with IconUISettings::captionColor (RW 0x677CF9)
	const char *kConsIni =
		"Object P324BuildSite\n"
		"  KindOf = STRUCTURE SELECTABLE\n"
		"  Geometry = BOX\n"
		"  GeometryMajorRadius = 20\n"
		"  GeometryMinorRadius = 20\n"
		"  GeometryHeight = 20\n"
		"  Body = ActiveBody BodyTag\n"
		"    MaxHealth = 200\n"
		"  End\n"
		"End\n";
	econtest::Fx f("");
	W3DDrawModules::registerTypedDrawModuleData(f.w.modules);
	REQUIRE_MESSAGE(f.w.load(kConsIni).empty(), "load");
	w3dsynth::MemoryFileSource mfs;
	WW3DAssetManager assets(mfs);
	DrawableManager dm(assets, *f.logic);
	ClientEventRecorder rec(*f.logic);
	f.logic->setClientHooks(&rec);
	Object *site = f.makeAt("P324BuildSite", "Alice", 0.0f, 0.0f);
	const int kUnderCons = ObjectTemplateInfoBuilder::objectStatusIndex("UNDER_CONSTRUCTION");
	REQUIRE(kUnderCons >= 0); // tolerance: status exists
	site->setStatus((unsigned)kUnderCons, true);
	site->setConstructionPercent(45.0f);
	dm.applyEvents(rec.take());

	GameTextTableSource text;
	text.table.setOverride("CONTROLBAR:UnderConstructionDesc", "Building: %.0f%%");
	IconUISettings uiSet;
	uiSet.showObjectHealth = true;
	uiSet.text = &text;
	uiSet.captionColor = ms.captionColor;
	PinholeView view(Coord3D{ 0.0f, -150.0f, 150.0f }, Coord3D{ 0.0f, 0.0f, 0.0f }, 1024, 768, 0.7f);
	DrawableIconUI iconUI;
	std::vector<IconUIOp> ops;
	iconUI.build(*f.logic, dm, view, uiSet, {}, INVALID_ID, ops);
	bool sawCaption = false;
	for (const IconUIOp &op : ops)
	{
		if (op.kind == IconUIOp::TEXT && op.object == site->getID())
		{
			sawCaption = true;
			CHECK(op.color == 0xFFC8B4A0u);
		}
	}
	CHECK(sawCaption);
	f.logic->setClientHooks(nullptr);
}

TEST_CASE("PORT-324 S-706: SoundSelectors ambient rows 37..41, under-construction pristine fallback (RW 0x6778A9), and SoundAmbientBattle (RW 0x678816)")
{
	econtest::Fx f("");
	SoundSelectorClientBehavior::registerClasses(f.w.modules);
	const std::function<bool(const std::string &)> savedHook = TheContainParseHooks().audioEventExists;
	struct RestoreHook
	{
		std::function<bool(const std::string &)> prev;
		~RestoreHook() { TheContainParseHooks().audioEventExists = prev; }
	} restore{ savedHook };
	TheContainParseHooks().audioEventExists = [](const std::string &) { return true; };

	const char *kIni =
		"Object P324SelectorAmbient\n"
		"  KindOf = STRUCTURE SELECTABLE\n"
		"  SoundAmbient = BaseLoop\n"
		"  SoundAmbientDamaged = BaseDamagedLoop\n"
		"  SoundAmbientBattle = BaseBattleLoop\n"
		"  Body = ActiveBody BodyTag\n"
		"    MaxHealth = 100\n"
		"  End\n"
		"  ClientBehavior = ModelConditionSoundSelectorClientBehavior ModuleTag_MCS\n"
		"    SoundState = USER_1\n"
		"      SoundAmbient = SelLoop\n"
		"      SoundAmbientDamaged = SelDamagedLoop\n"
		"      SoundAmbientBattle = SelBattleLoop\n"
		"    End\n"
		"  End\n"
		"End\n";
	REQUIRE_MESSAGE(f.w.load(kIni).empty(), "load");

	Object *obj = f.makeAt("P324SelectorAmbient", "Alice", 0.0f, 0.0f);
	CHECK(LiveGameAudio::ambientSoundFor(*obj) == "BaseLoop");
	CHECK(LiveGameAudio::battleAmbientSoundFor(*obj) == "BaseBattleLoop");

	// Damage to DAMAGED state -> BaseDamagedLoop; AWAITING_CONSTRUCTION overrides back to pristine BaseLoop (RW 0x6778A9)
	DamageInfo dmg;
	dmg.m_input.m_amount = 45.0f;
	dmg.m_input.m_damageType = DAMAGE_UNRESISTABLE;
	obj->attemptDamage(dmg);
	REQUIRE(obj->getBodyModule()->getDamageState() == BODY_DAMAGED);
	CHECK(LiveGameAudio::ambientSoundFor(*obj) == "BaseDamagedLoop");

	const int kAwaiting = ModelCondition::indexOf("AWAITING_CONSTRUCTION");
	REQUIRE(kAwaiting >= 0); // tolerance: valid bit index
	obj->setModelConditionState(kAwaiting, true);
	CHECK(LiveGameAudio::ambientSoundFor(*obj) == "BaseLoop");
	obj->setModelConditionState(kAwaiting, false);

	// ModelConditionSoundSelectorClientBehavior overrides ambient + damaged + battle rows (RW 0x676A8A)
	const int kUser1 = ModelCondition::indexOf("USER_1");
	REQUIRE(kUser1 >= 0); // tolerance: valid bit index
	obj->setModelConditionState(kUser1, true);
	CHECK(LiveGameAudio::ambientSoundFor(*obj) == "SelDamagedLoop");
	CHECK(LiveGameAudio::battleAmbientSoundFor(*obj) == "SelBattleLoop");

	const std::vector<std::string> stops = LiveGameAudio::acceptanceStops();
	CHECK(stops.size() == 9);
}

TEST_CASE("PORT-324 S-1001: CAMOUFLAGE launcher stealth hides projectile launch from active enemy (RW 0x85EE0F..0x85EE7A / 0x6AAC52)")
{
	const char *kIni =
		"Object P324Launcher\n"
		"  KindOf = INFANTRY SELECTABLE\n"
		"  Body = ActiveBody BodyTag\n"
		"    MaxHealth = 100\n"
		"  End\n"
		"End\n"
		"Object P324Stone\n"
		"  KindOf = PROJECTILE\n"
		"  Behavior = BezierProjectileBehavior ProjTag\n"
		"    FirstHeight = 10\n"
		"    SecondHeight = 10\n"
		"    FirstPercentIndent = 30%\n"
		"    SecondPercentIndent = 70%\n"
		"    InvisibleFrames = 100\n"
		"    FadeInTime = 100\n"
		"  End\n"
		"End\n";
	econtest::Fx f("");
	w3dsynth::MemoryFileSource fs;
	W3DDrawModules::registerTypedDrawModuleData(f.w.modules);
	REQUIRE_MESSAGE(f.w.load(kIni).empty(), "load");
	WW3DAssetManager assets(fs);
	DrawableManager dm(assets, *f.logic);
	ClientEventRecorder rec(*f.logic);
	f.logic->setClientHooks(&rec);

	Object *enemyLauncher = f.makeAt("P324Launcher", "Bob", 50.0f, 50.0f);
	Object *stone1 = f.makeAt("P324Stone", "Bob", 60.0f, 50.0f);
	Object *stone2 = f.makeAt("P324Stone", "Bob", 60.0f, 50.0f);
	Object *stone3 = f.makeAt("P324Stone", "Bob", 60.0f, 50.0f);
	dm.applyEvents(rec.take());
	Drawable *d1 = dm.findByObject(stone1->getID());
	Drawable *d2 = dm.findByObject(stone2->getID());
	Drawable *d3 = dm.findByObject(stone3->getID());
	REQUIRE((d1 && d2 && d3));

	const int kCamo = ModelCondition::indexOf("INVISIBLE_CAMOUFLAGE");
	const int kStealth = ModelCondition::indexOf("INVISIBLE_STEALTH");
	REQUIRE(kCamo >= 0); // tolerance: valid model condition index
	REQUIRE(kStealth >= 0); // tolerance: valid model condition index

	// Local player is Alice (index 1); Bob (index 2) is enemy
	Player *alice = f.player("Alice");
	REQUIRE(alice != nullptr);
	f.players.setLocalPlayer(alice);
	enemyLauncher->setModelConditionState(kCamo, true);
	std::shared_ptr<const LogicSnapshot> snapCamo = LogicSnapshot::build(*f.logic, nullptr, false, 0, -1, false);
	REQUIRE(snapCamo != nullptr);
	REQUIRE(snapCamo->players != nullptr);
	CHECK(snapCamo->players->localActive == true);
	const ObjectSnapshot *lSnap = snapCamo->find(enemyLauncher->getID());
	REQUIRE(lSnap != nullptr);
	CHECK(lSnap->camouflaged == true);

	auto makeShroudClear = []() {
		auto sh = std::make_shared<ShroudView>();
		sh->displayed = true;
		sh->localPlayer = 0;
		sh->countX = 4;
		sh->countY = 1;
		sh->cellSize = 100.0f;
		sh->status.assign(4, (std::uint8_t)CELLSHROUD_CLEAR);
		return sh;
	};
	// Snapshot with clear shroud + camouflaged enemy launcher in OBJECTSHROUD_CLEAR + clear target cell:
	// targetSeen = true, launcherSeen = false -> fade in from fog (mode 1)
	auto snapProbe = std::make_shared<LogicSnapshot>(*snapCamo);
	snapProbe->shroud = makeShroudClear();
	for (ObjectSnapshot &os : snapProbe->objects)
	{
		if (os.id == enemyLauncher->getID()) os.objectShroud = (int)OBJECTSHROUD_CLEAR;
	}
	ObjectSnapshot projRec;
	projRec.id = stone1->getID();
	projRec.projectile = true;
	projRec.projectileFireFrame = 10;
	projRec.projectileLauncher = enemyLauncher->getID();
	projRec.projectileEnd = Coord3D{ 350.0f, 50.0f, 0.0f };
	projRec.projectileSegments = 6;

	d1->startProjectileFade(projRec, *snapProbe);
	CHECK(d1->fade().mode == 1);

	// Inactive/observer local player (!localActive, RW 0x6AAC52) ignores launcher stealth -> mode 5 (both seen)
	auto snapObserver = std::make_shared<LogicSnapshot>(*snapProbe);
	auto obsPlayers = std::make_shared<PlayerView>(*snapProbe->players);
	obsPlayers->localActive = false;
	snapObserver->players = obsPlayers;
	projRec.id = stone3->getID();
	projRec.projectileFireFrame = 12;
	d3->startProjectileFade(projRec, *snapObserver);
	CHECK(d3->fade().mode == 5);

	// INVISIBLE_STEALTH (type 0, not CAMOUFLAGE 1) keeps launcherSeen = true in OBJECTSHROUD_CLEAR -> mode 5 (both seen)
	enemyLauncher->setModelConditionState(kCamo, false);
	enemyLauncher->setModelConditionState(kStealth, true);
	std::shared_ptr<const LogicSnapshot> snapStealth = LogicSnapshot::build(*f.logic, nullptr, false, 0, -1, false);
	auto probe2 = std::make_shared<LogicSnapshot>(*snapStealth);
	probe2->shroud = makeShroudClear();
	for (ObjectSnapshot &os : probe2->objects)
	{
		if (os.id == enemyLauncher->getID()) os.objectShroud = (int)OBJECTSHROUD_CLEAR;
	}
	projRec.id = stone2->getID();
	projRec.projectileFireFrame = 11;
	d2->startProjectileFade(projRec, *probe2);
	CHECK(d2->fade().mode == 5);
	f.logic->setClientHooks(nullptr);
}

TEST_CASE("PORT-324 S-4974: W3DFloorDraw uses tod.terrain via terrain.fxo RenderingMode 3 without changing varyings (RW 0x4E5631 / 0x4E44FB)")
{
	GlobalLightingData g;
	g.version = 8;
	g.timeOfDay = 1;
	g.terrainLightingMultiplier = 2.0f;
	TimeOfDayLights &tod = g.tod[0];
	// Warm red objects lighting
	tod.objects[0].ambient[0] = 0.8f; tod.objects[0].ambient[1] = 0.1f; tod.objects[0].ambient[2] = 0.1f;
	tod.objects[0].diffuse[0] = 0.9f; tod.objects[0].diffuse[1] = 0.2f; tod.objects[0].diffuse[2] = 0.2f;
	tod.objects[0].lightPos[0] = 0.0f; tod.objects[0].lightPos[1] = 0.0f; tod.objects[0].lightPos[2] = -1.0f;
	// Cool blue terrain lighting
	tod.terrain[0].ambient[0] = 0.05f; tod.terrain[0].ambient[1] = 0.15f; tod.terrain[0].ambient[2] = 0.45f;
	tod.terrain[0].diffuse[0] = 0.10f; tod.terrain[0].diffuse[1] = 0.25f; tod.terrain[0].diffuse[2] = 0.70f;
	tod.terrain[0].lightPos[0] = 0.0f; tod.terrain[0].lightPos[1] = 0.0f; tod.terrain[0].lightPos[2] = -1.0f;
	tod.terrain[1].diffuse[0] = 0.02f; tod.terrain[1].diffuse[1] = 0.10f; tod.terrain[1].diffuse[2] = 0.20f;
	tod.terrain[1].lightPos[0] = 0.0f; tod.terrain[1].lightPos[1] = 0.6f; tod.terrain[1].lightPos[2] = -0.5f; // |-lightPos.z| = 0.5

	W3DObjectLighting out;
	std::string err;
	REQUIRE_MESSAGE(W3DObjectLightingUtil::fromMap(g, out, &err), err);
	CHECK(out.colorScale == doctest::Approx(2.0f).epsilon(1e-5)); // tolerance: overbright multiplier
	CHECK(out.floor.colorScale == doctest::Approx(2.0f).epsilon(1e-5)); // tolerance: overbright multiplier
	CHECK(out.floor.baseAmbient[0] == doctest::Approx(0.05f).epsilon(1e-5)); // tolerance: uses terrain ambient, not objects 0.8
	CHECK(out.floor.baseAmbient[2] == doctest::Approx(0.45f).epsilon(1e-5)); // tolerance: float32 copy
	CHECK(out.floor.sunColor[2] == doctest::Approx(0.70f).epsilon(1e-5)); // tolerance: uses terrain sun
	CHECK(out.floor.accentDiffuse[2] == doctest::Approx(0.10f).epsilon(1e-5)); // tolerance: 0.5 * 0.20 (RW 0x4E45C9)

	W3DShaderKey plain;
	plain.Blend = W3D_GODOT_BLEND_MIX;
	plain.DepthWrite = false;
	plain.Texturing = true;
	plain.Tex[0] = true;
	W3DShaderKey flr = plain;
	flr.Floor = true;

	const std::string plainGlsl = Generate_W3D_Shader_Code(plain, W3DSamplerPolicy::Anisotropic);
	const std::string flrGlsl = Generate_W3D_Shader_Code(flr, W3DSamplerPolicy::Anisotropic);
	CHECK(countVaryingLines(flrGlsl) == countVaryingLines(plainGlsl));
	CHECK(flrGlsl.find("w3d_flr_amb") != std::string::npos);
	CHECK(flrGlsl.find("w3d_flr_sun_col") != std::string::npos);
	CHECK(flrGlsl.find("flr_lit") != std::string::npos);
}
