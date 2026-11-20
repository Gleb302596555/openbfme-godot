// OpenBFME. GPL-3.0.
//
// Narrow tests for Local 88 (issue #714): Client draw modules other than W3DScriptedModelDraw:
// 15 classes, 98 uncited own functions (debris, floors, streaks, tanks, trucks, supply,
// lights, props, tornado, buffs, quadrupeds, sails).

#include "doctest.h"
#include "ObjectTestUtil.h"
#include "W3DDrawTestUtil.h"
#include "W3dSynth.h"

#include "Common/ModelState.h"
#include "Common/Thing/ModuleFactory.h"
#include "Common/Thing/Thing.h"
#include "Common/Thing/ThingTemplate.h"
#include "Common/Xfer.h"
#include "Common/XferLoad.h"
#include "Common/XferSave.h"
#include "GameClient/BuffDraw.h"
#include "GameClient/ClientEffects.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DDebrisDraw.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DDrawModules.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DLaserDraw.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DLightDraw.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DModelDrawVariants.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DProjectileStreamDraw.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DQuadrupedDraw.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DSailModelDraw.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DStreakDraw.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DSupplyDraw.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DTornadoDraw.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DTreeDraw.h"
#include "Libraries/WWVegas/WW3D2/assetmgr.h"
#include "Libraries/WWVegas/WWMath/matrix3d.h"

#include <cmath>
#include <vector>

namespace
{
class FixedTestRandom : public W3DDrawRandom
{
public:
	int value(int lo, int hi) override { return lo == hi ? lo : (lo + hi) / 2; }
	float real(float lo, float hi) override { return (lo + hi) * 0.5f; }
};
} // namespace

TEST_CASE("LOCAL-88: W3DProjectileStreamDraw MaxSegments windowing, zero-hole splitting, >1 point filter, shroud visibility, and line properties (RW 0x004D0D05 / RW 0x004D0A7C / RW 0x004D0933)")
{
	W3DProjectileStreamDrawModuleData data;
	data.m_texture = "EXFireProjectile03.tga";
	data.m_width = 2.5f;
	data.m_tileFactor = 4.0f;
	data.m_scrollRate = 1.75f;
	data.m_maxSegments = 6;

	W3DProjectileStreamDraw draw(nullptr, &data);

	// 8 points total; with MaxSegments = 6, currentMasterPoint starts at max(0, 8 - 6) = 2.
	// Points 0 and 1 are outside the MaxSegments window.
	// Indices 2..3 form a 2-point segment.
	// Index 4 is a (0,0,0) hole.
	// Index 5 is an isolated single point before another hole at index 6 (must be skipped because stagingPoint <= 1).
	// Wait: let's test both an isolated 1-point run and a valid 2-point run in the window!
	const Coord3D pts[9] = {
		{ 1.0f, 0.0f, 0.0f },  // 0 (windowed out when pointsUsed = 8, MaxSegments = 6 -> start = 2)
		{ 2.0f, 0.0f, 0.0f },  // 1 (windowed out)
		{ 10.0f, 1.0f, 2.0f }, // 2 (run 1 pt 0)
		{ 20.0f, 2.0f, 3.0f }, // 3 (run 1 pt 1)
		{ 0.0f, 0.0f, 0.0f },  // 4 (hole)
		{ 99.0f, 0.0f, 0.0f }, // 5 (isolated 1-point run -> ignored by RW 0x004D0E05 cmp > 1)
		{ 0.0f, 0.0f, 0.0f },  // 6 (hole)
		{ 30.0f, 3.0f, 4.0f }, // 7 (run 2 pt 0)
		{ 40.0f, 4.0f, 5.0f }, // 8 (run 2 pt 1)
	};
	// With 9 points and MaxSegments = 7, window starts at 9 - 7 = 2:
	data.m_maxSegments = 7;
	draw.buildLinesFromPoints(pts, 9);

	REQUIRE(draw.linesValid() == 2);
	REQUIRE(draw.lines().size() == 2u);

	const W3DProjectileStreamLine &l0 = draw.lines()[0];
	REQUIRE(l0.points.size() == 2u);
	CHECK(l0.points[0].x == 10.0f);
	CHECK(l0.points[1].x == 20.0f);
	CHECK(l0.texture == "EXFireProjectile03.tga");
	CHECK(l0.width == 2.5f);
	CHECK(l0.tileFactor == 4.0f);
	CHECK(l0.scrollRate == 1.75f);
	CHECK(l0.inScene == true);

	const W3DProjectileStreamLine &l1 = draw.lines()[1];
	REQUIRE(l1.points.size() == 2u);
	CHECK(l1.points[0].x == 30.0f);
	CHECK(l1.points[1].x == 40.0f);

	// Shroud toggle (RW 0x004D0933) removes/re-adds active lines
	draw.setFullyObscuredByShroud(true);
	CHECK(draw.isFullyObscuredByShroud());
	CHECK_FALSE(draw.lines()[0].inScene);
	CHECK_FALSE(draw.lines()[1].inScene);

	draw.setFullyObscuredByShroud(false);
	CHECK(draw.lines()[0].inScene);
	CHECK(draw.lines()[1].inScene);

	// Shrinking to 1 segment trims leftover lines (RW 0x004D0E30..0x004D0E75)
	draw.buildLinesFromPoints(pts + 2, 2);
	CHECK(draw.linesValid() == 1);
}

TEST_CASE("LOCAL-88: W3DLightDraw random flicker drift and setHidden (RW 0x004CF2B5 / RW 0x004CF29D)")
{
	W3DLightDrawModuleData data;
	data.m_diffuse = RGBColor{ 1.0f, 0.5f, 0.25f };
	data.m_ambient = RGBColor{ 0.2f, 0.1f, 0.05f };
	data.m_radius = 35.0f;
	data.m_intensity = 2.0f;
	data.m_flickerAmplitude = 0.8f;
	data.m_flickerFrequency = 5.0f;
	data.m_pulseAmplitude = 0.0f;

	LightPulsePool pool;
	W3DLightDraw light(nullptr, &data);
	light.setLightPool(&pool);

	CHECK(light.lightRadius() == 35.0f);
	CHECK(light.currentHeight() == 2.0f);

	// Previously targetOffset was hardcoded to (minVal + maxVal) * 0.5f == 0.0f, so m_angle was always 0.0f
	// and m_height never changed when pulseAmplitude == 0. With RW 0x004CF2B5 Random_Float, m_angle != 0 and
	// currentHeight drifts within [intensity - amp, intensity + amp].
	light.stepFlicker(0.1f);
	CHECK(light.currentAngle() != 0.0f); // tolerance: MSVCR71 rand() flicker slope is non-zero
	CHECK(light.currentHeight() != 2.0f);
	CHECK(light.currentHeight() >= 2.0f - 0.8f); // tolerance: random flicker bounded by [intensity - amp, intensity + amp]
	CHECK(light.currentHeight() <= 2.0f + 0.8f); // tolerance: random flicker bounded by [intensity - amp, intensity + amp]

	// RW 0x004CF29D: setHidden(true) disables the light in the pool; setHidden(false) re-enables it
	light.setHidden(true);
	CHECK(light.isHidden());
	CHECK_FALSE(light.isEnabled());
	REQUIRE(pool.lights().size() == 1u);
	CHECK_FALSE(pool.lights()[0].enabled);

	light.setHidden(false);
	CHECK_FALSE(light.isHidden());
	CHECK(light.isEnabled());
	CHECK(pool.lights()[0].enabled);
}

TEST_CASE("LOCAL-88: W3DDebrisDraw setAnimNames STOP fallback, shadow/shroud setters, reactToTransformChange, and xfer load re-init (RW 0x004B12FE / RW 0x004B130F / RW 0x004B1339 / RW 0x004B16E8 / RW 0x004B19A4)")
{
	W3DDebrisDrawModuleData data;
	W3DDebrisDraw debris(nullptr, &data);

	debris.setModelName("DebrisChunk", 0x112233, 1);
	debris.setAnimNames("Chunk_Init", "Chunk_Fly", "STOP");
	CHECK(debris.finalStop());
	// RW 0x004B19A4 copies flying into final when final is "STOP"
	CHECK(debris.animFinal() == "Chunk_Fly");

	debris.setShadowsEnabled(false);
	CHECK_FALSE(debris.shadowsEnabled());
	debris.setFullyObscuredByShroud(true);
	CHECK(debris.isFullyObscuredByShroud());

	debris.reactToTransformChange(nullptr, 0.0f);
	CHECK(debris.transformChangeCount() == 1u);

	// Advance into FLYING then FINAL
	debris.advance(true, true);
	CHECK(debris.state() == 1);
	W3DDebrisDrawStep finalStep = debris.advance(true, true);
	CHECK(debris.state() == 2);
	CHECK(finalStep.mode == W3DDebrisDrawStep::MODE_MANUAL);

	// Round-trip through xfer (RW 0x004B16E8)
	XferSave save;
	debris.xfer(save);

	W3DDebrisDraw loaded(nullptr, &data);
	XferLoad load(save.data().data(), save.data().size());
	loaded.xfer(load);
	CHECK(loaded.modelName() == "DebrisChunk");
	CHECK(loaded.modelColor() == 0x112233);
	CHECK(loaded.animInitial() == "Chunk_Init");
	CHECK(loaded.animFlying() == "Chunk_Fly");
	CHECK(loaded.animFinal() == "Chunk_Fly");
	CHECK(loaded.state() == 2);
	CHECK(loaded.frames() == 2);
	CHECK(loaded.finalStop() == true);
}

TEST_CASE("LOCAL-88: W3DPropDrawState and W3DFloorDrawState triple visibility gate, weather texture, and v3 xfer (RW 0x004CE793 / RW 0x004CE868 / RW 0x004CEA6F / RW 0x004CEB80 / RW 0x004CECA1 / RW 0x004CECB5)")
{
	W3DPropDrawModuleData propData;
	propData.m_modelName = "PMBarrel";
	propData.m_distanceFog = false;

	W3DPropDrawState prop(&propData);
	CHECK(prop.getModelName() == "PMBarrel");
	CHECK_FALSE(prop.distanceFog());
	// RW 0x004CE793 ignores (0, 0) position and registers once posX != 0 || posY != 0
	CHECK_FALSE(prop.reactToTransformChange(0.0f, 0.0f));
	CHECK_FALSE(prop.propCreated());
	CHECK(prop.reactToTransformChange(12.0f, 0.0f));
	CHECK(prop.propCreated());
	CHECK_FALSE(prop.reactToTransformChange(15.0f, 20.0f)); // one-shot

	W3DFloorDrawModuleData floorData;
	floorData.m_modelName = "GBBarracks_Bib";
	floorData.m_startHidden = true;
	floorData.m_weatherTextures.emplace_back(1, "GBBarracks_Bib_Snow.tga");
	ModelConditionFlags hideMask;
	hideMask.set(3);
	floorData.m_hideIfModelConditions.push_back(hideMask);

	W3DFloorDrawState floor(&floorData);
	// RW 0x004CE91B seeds +0x15 (m_hiddenBySubObject) from StartHidden (+0x1E)
	CHECK(floor.hiddenBySubObject());
	CHECK_FALSE(floor.isFloorVisible());

	CHECK(floor.reactToTransformChange(1));
	CHECK(floor.registered());
	CHECK(floor.activeWeatherTexture() == "GBBarracks_Bib_Snow.tga");

	// Reveal via setSubObjectHidden(false) (RW 0x004CECA1)
	floor.setSubObjectHidden(false);
	CHECK(floor.isFloorVisible());

	// Hide via model condition (RW 0x004CEB80)
	ModelConditionFlags activeFlags;
	activeFlags.set(3);
	CHECK(floor.replaceModelConditionState(activeFlags));
	CHECK(floor.hiddenByModelCondition());
	CHECK_FALSE(floor.isFloorVisible());

	// Clear model condition, hide via module (RW 0x004CECB5)
	CHECK(floor.replaceModelConditionState(ModelConditionFlags()));
	CHECK(floor.isFloorVisible());
	floor.setHidden(true);
	CHECK(floor.hiddenByModule());
	CHECK_FALSE(floor.isFloorVisible());

	// Round-trip v3 xfer (RW 0x004CE868)
	XferSave save;
	floor.xfer(save);

	W3DFloorDrawState restored(&floorData);
	XferLoad load(save.data().data(), save.data().size());
	restored.xfer(load);
	CHECK(restored.hiddenByModule() == true);
	CHECK(restored.hiddenByModelCondition() == false);
	CHECK(restored.hiddenBySubObject() == false);
}

TEST_CASE("LOCAL-88: W3DTankDrawState tread discovery, pivot vs drive UV scrolling, and debris emitters (RW 0x004CD7BC / RW 0x004CD91D / RW 0x004CD93B / RW 0x004CD95E / RW 0x004CDBD7)")
{
	W3DTankDrawModuleData data;
	data.m_treadAnimationRate = 0.25f;
	data.m_treadPivotSpeedFraction = 0.6f;
	data.m_treadDriveSpeedFraction = 0.3f;

	W3DTankDrawState tank(&data);
	tank.onRenderObjRecreated({ "TANK.BODY", "TANK.TREADSL", "TANK.TREADSR", "TANK.TREADSMID" });
	REQUIRE(tank.treadCount() == 3);
	CHECK(tank.treads()[0].type == W3DTankDrawState::TREAD_LEFT);
	CHECK(tank.treads()[1].type == W3DTankDrawState::TREAD_RIGHT);
	CHECK(tank.treads()[2].type == W3DTankDrawState::TREAD_MIDDLE);

	// Driving forward at speed 5 / maxSpeed 10 (fraction 0.5 >= 0.3): starts debris emitters and advances all treads equally
	tank.doDrawModule(5.0f, 10.0f, 0, true, false, 1.0f, 0.0f, false);
	CHECK(tank.leftDebrisActive());
	CHECK(tank.rightDebrisActive());
	CHECK(tank.debrisVelocityMultiplier().x == 1.0f);
	CHECK(tank.treads()[0].u == 0.25f);
	CHECK(tank.treads()[1].u == 0.25f);

	// Pivot turning in place (speed 1 / 10 = 0.1 < 0.6, turnBits = -1, direction changed): left and right scroll oppositely
	tank.doDrawModule(1.0f, 10.0f, -1, true, false, 0.0f, 1.0f, false);
	CHECK(tank.treads()[0].u == 0.50f); // 0.25 + 0.25
	CHECK(tank.treads()[1].u == 0.00f); // 0.25 - 0.25

	// Hiding or shrouding stops debris emitters (RW 0x004CD91D / RW 0x004CD93B)
	tank.setHidden(true);
	CHECK_FALSE(tank.leftDebrisActive());
	CHECK_FALSE(tank.rightDebrisActive());
	tank.setHidden(false);
	tank.startMoveDebris();
	CHECK(tank.leftDebrisActive());
	tank.setFullyObscuredByShroud(true);
	CHECK_FALSE(tank.leftDebrisActive());
}

TEST_CASE("LOCAL-88: W3DTruckDrawState, W3DSupplyDraw, W3DSailModelDraw, W3DQuadrupedDraw, W3DStreakTrail, and ClientBuffs xfer / state helpers")
{
	// 1. W3DTruckDrawState (RW 0x004CA936 / RW 0x004CBF6E / RW 0x004CBF8E / RW 0x004CD634 / RW 0x004CD6F7)
	W3DTruckDrawModuleData truckData;
	W3DTruckDrawState truck(&truckData);
	truck.enableEmitters(true);
	CHECK(truck.emittersEnabled());
	truck.setHidden(true);
	CHECK_FALSE(truck.emittersEnabled());
	truck.setFullyObscuredByShroud(true);
	CHECK_FALSE(truck.emittersCreated());
	truck.setFullyObscuredByShroud(false);
	CHECK(truck.emittersCreated());
	truck.onRenderObjRecreated(6);
	CHECK(truck.resolvedBoneCount() == 6);
	CHECK(truck.boneUpdateCount() == 1u);
	truck.setEffectsState(true, true);
	{
		XferSave save;
		truck.xfer(save);
		W3DTruckDrawState truckLoaded(&truckData);
		XferLoad load(save.data().data(), save.data().size());
		truckLoaded.xfer(load);
		CHECK(truckLoaded.effectsInitialized());
		CHECK(truckLoaded.wasAirborne());
	}

	// 2. W3DSupplyDraw::xfer (RW 0x004CA6B8)
	w3dsynth::MemoryFileSource fs;
	WW3DAssetManager mgr(fs);
	WW3DDrawAssets assets(mgr);
	ThingTemplate tmpl;
	W3DSupplyDrawModuleData supplyData;
	supplyData.m_supplyBonePrefix = "BONE";
	W3DSupplyDraw supply(tmpl, supplyData, assets);
	supply.setTotalAndVisibleBones(12, 7);
	{
		XferSave save;
		supply.xfer(save);
		W3DSupplyDraw supplyLoaded(tmpl, supplyData, assets);
		XferLoad load(save.data().data(), save.data().size());
		supplyLoaded.xfer(load);
		CHECK(supplyLoaded.totalBones() == 12);
		CHECK(supplyLoaded.visibleBones() == 7);
	}

	// 3. W3DSailModelDraw::xfer (RW 0x004D0009)
	W3DSailModelDrawModuleData sailData;
	sailData.m_conditionStates.resize(1);
	sailData.m_defaultState = 0;
	sailData.m_maxRotation = 1.0f;
	sailData.m_aboutDamping = 0.05f;
	FixedTestRandom rng;
	W3DSailModelDraw sail(sailData, assets, rng);
	Matrix3D m;
	sail.adjustAttachedTransform(m);
	REQUIRE(sail.hasLastAngle());
	const float savedAngle = sail.lastAngle();
	{
		XferSave save;
		sail.xfer(save);
		W3DSailModelDraw sailLoaded(sailData, assets, rng);
		XferLoad load(save.data().data(), save.data().size());
		sailLoaded.xfer(load);
		CHECK(sailLoaded.hasLastAngle());
		CHECK(sailLoaded.lastAngle() == savedAngle);
	}

	// 4. W3DStreakTrail setHidden / reactToTransformChange / doDrawModule (RW 0x004CF73F / RW 0x004CFC4C / RW 0x004CFC54)
	W3DStreakDrawModuleData streakData;
	W3DStreakTrail trail;
	trail.setHidden(true);
	CHECK(trail.isHidden());
	trail.setHidden(false);
	const float p0[3] = { 0.0f, 0.0f, 0.0f };
	const float p1[3] = { 10.0f, 0.0f, 0.0f };
	trail.reactToTransformChange(streakData, p0);
	trail.doDrawModule(streakData, p1);
	CHECK(trail.points().size() == 2u);

	// 5. ClientBuffs::xferBuff (RW 0x004D18E9)
	ClientBuffs::Buff b;
	b.thing = "BuffTemplate1";
	b.state = ClientBuffs::ON;
	b.opacity = 0.75f;
	b.visible = true;
	b.position = Coord3D{ 4.0f, 5.0f, 6.0f };
	b.scale = 1.25f;
	{
		XferSave save;
		ClientBuffs::xferBuff(save, b);
		ClientBuffs::Buff bLoaded;
		XferLoad load(save.data().data(), save.data().size());
		ClientBuffs::xferBuff(load, bLoaded);
		CHECK(bLoaded.thing == "BuffTemplate1");
		CHECK(bLoaded.state == ClientBuffs::ON);
		CHECK(bLoaded.opacity == 0.75f);
		CHECK(bLoaded.visible == true);
		CHECK(bLoaded.position.x == 4.0f);
		CHECK(bLoaded.scale == 1.25f);
	}
}
