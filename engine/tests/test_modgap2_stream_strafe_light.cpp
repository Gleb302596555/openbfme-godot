// Both editions: RotWK 2.01 and BFME2 1.06.
#include "doctest.h"
#include "CombatTestUtil.h"
#include "ObjectTestUtil.h"

#include "Common/BfmeGame.h"
#include "Common/GameEdition.h"
#include "Common/INI.h"
#include "Common/StateHash.h"
#include "Common/XferLoad.h"
#include "Common/XferSave.h"
#include "Common/Thing/Thing.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DLightDraw.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DProjectileStreamDraw.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/ProjectileStreamUpdate.h"
#include "GameLogic/Module/StrafeAreaUpdate.h"
#include "GameLogic/Object/Object.h"

#include <cmath>
#include <cstring>
#include <string>

namespace
{
const char kStreamIni[] =
    "Object TestStreamObj\n"
    "  KindOf = SELECTABLE\n"
    "  Body = ActiveBody BodyTag\n"
    "    MaxHealth = 100\n"
    "  End\n"
    "  Behavior = ProjectileStreamUpdate ModuleTag_StreamUpdate\n"
    "  End\n"
    "  Draw = W3DProjectileStreamDraw ModuleTag_StreamDraw\n"
    "    Texture = EXFireProjectile03.tga\n"
    "    Width = 1.5\n"
    "    TileFactor = 3.0\n"
    "    ScrollRate = 2.0\n"
    "    MaxSegments = 4\n"
    "  End\n"
    "End\n";

const char kStrafeIni[] =
    "Weapon TestStrafeWeapon\n"
    "  AttackRange = 200.0\n"
    "  DamageNugget\n"
    "    Damage = 50.0\n"
    "    DamageType = MAGIC\n"
    "    DeathType = NORMAL\n"
    "  End\n"
    "End\n"
    "Object TestStrafeObj\n"
    "  KindOf = SELECTABLE\n"
    "  Body = ActiveBody BodyTag\n"
    "    MaxHealth = 1000\n"
    "  End\n"
    "  Behavior = AIUpdateInterface ModuleTag_AI\n"
    "  End\n"
    "  Behavior = StrafeAreaUpdate ModuleTag_StrafeUpdate\n"
    "    WeaponName = TestStrafeWeapon\n"
    "    StrafeAreaRadius = 180.0\n"
    "    Sweepfrequency = 0.5\n"
    "    SweepAmplitude = 120.0\n"
    "    Slope = 60.0\n"
    "    InitialSweepPhase = 0.1\n"
    "  End\n"
    "End\n";

const char kLightIni[] =
    "Object TestLightObj\n"
    "  KindOf = SELECTABLE\n"
    "  Draw = W3DLightDraw ModuleTag_LightDraw\n"
    "    Ambient = R:20 G:30 B:40\n"
    "    Diffuse = R:240 G:240 B:100\n"
    "    Specular = R:10 G:10 B:10\n"
    "    Radius = 45.0\n"
    "    Intensity = 2.5\n"
    "    FlickerAmplitude = 0.8\n"
    "    FlickerFrequency = 8.0\n"
    "    PulseAmplitude = 0.3\n"
    "    PulseFrequency = 2.0\n"
    "    AttachToBoneInAnotherModule = TorchBone\n"
    "  End\n"
    "End\n";
} // namespace

// ==============================================================================================
// 1. ProjectileStreamUpdate Tests
// ==============================================================================================

TEST_CASE("modgap2 stream: module data defaults and registration (both editions: RW 0x00890AE0, BFME2 0x0088D370)")
{
	for (GameEdition edition : { GameEdition::RotWK201, GameEdition::BFME2_106 })
	{
		CAPTURE(gameEditionName(edition));
		combattest::CombatWorld w(kStreamIni);
		w.logic->setEdition(edition);

		Object *obj = w.unit("TestStreamObj", 'A', 50.0f, 50.0f);
		REQUIRE(obj != nullptr);

		auto *stream = dynamic_cast<ProjectileStreamUpdate *>(obj->findModule("ProjectileStreamUpdate"));
		REQUIRE(stream != nullptr);
		CHECK(stream->owningObject() == 0u);
		CHECK(stream->nextFreeIndex() == 0);
		CHECK(stream->firstValidIndex() == 0);
	}
}

TEST_CASE("modgap2 stream: circular buffer, hole insertion on target change, and point extraction (both editions)")
{
	for (GameEdition edition : { GameEdition::RotWK201, GameEdition::BFME2_106 })
	{
		CAPTURE(gameEditionName(edition));
		combattest::CombatWorld w(kStreamIni);
		w.logic->setEdition(edition);

		Object *streamObj = w.unit("TestStreamObj", 'A', 0.0f, 0.0f);
		REQUIRE(streamObj != nullptr);
		auto *stream = dynamic_cast<ProjectileStreamUpdate *>(streamObj->findModule("ProjectileStreamUpdate"));
		REQUIRE(stream != nullptr);

		Object *p1 = w.unit("TestStreamObj", 'A', 10.0f, 0.0f);
		Object *p2 = w.unit("TestStreamObj", 'A', 20.0f, 0.0f);
		REQUIRE(p1 != nullptr);
		REQUIRE(p2 != nullptr);

		ObjectID ownerID = 100;
		ObjectID victimA = 200;
		ObjectID victimB = 300;

		// Add projectile 1 targeting victimA
		stream->addProjectile(ownerID, p1->getID(), victimA, nullptr);
		CHECK(stream->owningObject() == ownerID);
		CHECK(stream->targetObject() == victimA);
		// In retail (RW 0x890B60 / BFME2 0x88D3F0), victimID != m_targetObject inserts a 0-hole before the first projectile
		CHECK(stream->nextFreeIndex() == 2);
		CHECK(stream->projectileID(0) == 0u);
		CHECK(stream->projectileID(1) == p1->getID());

		// Add projectile 2 targeting victimB: changes target, so a 0-hole is inserted before p2
		stream->addProjectile(ownerID, p2->getID(), victimB, nullptr);
		CHECK(stream->targetObject() == victimB);
		// Hole at index 2, p2 at index 3 -> nextFreeIndex = 4
		CHECK(stream->nextFreeIndex() == 4);
		CHECK(stream->projectileID(2) == 0u);
		CHECK(stream->projectileID(3) == p2->getID());

		// Extract points
		Coord3D points[ProjectileStreamUpdate::MAX_PROJECTILE_STREAM];
		int count = 0;
		stream->getAllPoints(points, &count);
		CHECK(count == 4);
		CHECK(points[0].x == doctest::Approx(0.0f)); // initial hole // tolerance: floating-point evaluation
		CHECK(points[1].x == doctest::Approx(10.0f)); // p1 // tolerance: floating-point evaluation
		CHECK(points[2].x == doctest::Approx(0.0f)); // hole // tolerance: floating-point evaluation
		CHECK(points[3].x == doctest::Approx(20.0f)); // p2 // tolerance: floating-point evaluation
	}
}

TEST_CASE("modgap2 stream: cullFrontOfList and considerDying (both editions)")
{
	for (GameEdition edition : { GameEdition::RotWK201, GameEdition::BFME2_106 })
	{
		CAPTURE(gameEditionName(edition));
		combattest::CombatWorld w(kStreamIni);
		w.logic->setEdition(edition);

		Object *streamObj = w.unit("TestStreamObj", 'A', 0.0f, 0.0f);
		REQUIRE(streamObj != nullptr);
		auto *stream = dynamic_cast<ProjectileStreamUpdate *>(streamObj->findModule("ProjectileStreamUpdate"));
		REQUIRE(stream != nullptr);

		Object *proj = w.unit("TestStreamObj", 'A', 10.0f, 0.0f);
		REQUIRE(proj != nullptr);
		ObjectID projID = proj->getID();

		Object *owner = w.unit("TestStreamObj", 'A', 0.0f, 0.0f);
		REQUIRE(owner != nullptr);
		ObjectID ownerID = owner->getID();

		Coord3D targetPos{ 50.0f, 50.0f, 0.0f };
		stream->addProjectile(ownerID, projID, 0, &targetPos);
		CHECK(stream->firstValidIndex() == 0);
		CHECK(stream->nextFreeIndex() == 2);
		CHECK(stream->considerDying() == false);

		// Destroy the projectile; cullFrontOfList should advance firstValidIndex past hole (0) and proj (1) -> 2
		w.logic->destroyObject(proj);
		stream->cullFrontOfList();
		CHECK(stream->firstValidIndex() == 2);

		// Now no active projectiles, but owner is still alive: considerDying is false
		CHECK(stream->considerDying() == false);

		// Destroy owner: considerDying is now true
		w.logic->destroyObject(owner);
		CHECK(stream->considerDying() == true);
	}
}

TEST_CASE("modgap2 stream: xfer snapshot and crc state hashing (both editions)")
{
	for (GameEdition edition : { GameEdition::RotWK201, GameEdition::BFME2_106 })
	{
		CAPTURE(gameEditionName(edition));
		combattest::CombatWorld w(kStreamIni);
		w.logic->setEdition(edition);

		Object *obj = w.unit("TestStreamObj", 'A', 0.0f, 0.0f);
		REQUIRE(obj != nullptr);
		auto *stream = dynamic_cast<ProjectileStreamUpdate *>(obj->findModule("ProjectileStreamUpdate"));
		REQUIRE(stream != nullptr);

		Coord3D tpos{ 123.0f, 456.0f, 7.0f };
		stream->addProjectile(10, 20, 0, &tpos);

		StateHasher h1;
		stream->crc(h1);
		StateHasher h1_repeat;
		stream->crc(h1_repeat);
		CHECK(h1.value() == h1_repeat.value());

		XferSave save;
		stream->xfer(save);

		Object *obj2 = w.unit("TestStreamObj", 'A', 0.0f, 0.0f);
		auto *stream2 = dynamic_cast<ProjectileStreamUpdate *>(obj2->findModule("ProjectileStreamUpdate"));
		REQUIRE(stream2 != nullptr);

		const auto &buf = save.data();
		XferLoad load(buf.data(), buf.size());
		stream2->xfer(load);

		CHECK(stream2->owningObject() == 10u);
		CHECK(stream2->nextFreeIndex() == stream->nextFreeIndex());
		CHECK(stream2->targetPosition().x == doctest::Approx(123.0f)); // tolerance: floating-point evaluation
		CHECK(stream2->targetPosition().y == doctest::Approx(456.0f)); // tolerance: floating-point evaluation
	}
}

// ==============================================================================================
// 2. W3DProjectileStreamDraw Tests
// ==============================================================================================

TEST_CASE("modgap2 stream draw: module data defaults and field table (both editions: RW 0x004D0C20 / 0x00BDBAA8, BFME2 0x000D11D2 / 0x00BCDFB0)")
{
	W3DProjectileStreamDrawModuleData d;
	CHECK(d.m_texture == "");
	CHECK(d.m_width == 0.0f);
	CHECK(d.m_tileFactor == 0.0f);
	CHECK(d.m_scrollRate == 0.0f);
	CHECK(d.m_maxSegments == 0);

	const FieldParse *t = W3DProjectileStreamDrawTables::stream();
	const char *const expected[] = { "Texture", "Width", "TileFactor", "ScrollRate", "MaxSegments" };
	int n = 0;
	for (; t[n].token; ++n)
	{
		REQUIRE(n < 5); // tolerance: floating-point evaluation
		CHECK(std::strcmp(t[n].token, expected[n]) == 0);
	}
	CHECK(n == 5);
}

TEST_CASE("modgap2 stream draw: INI parsing, point sync, and shroud toggle (both editions)")
{
	for (GameEdition edition : { GameEdition::RotWK201, GameEdition::BFME2_106 })
	{
		CAPTURE(gameEditionName(edition));
		objtest::World w;
		W3DProjectileStreamDraw::registerClass(w.modules);
		ProjectileStreamUpdate::registerClass(w.modules);
		REQUIRE(w.load(kStreamIni).empty());

		const ThingTemplate *tmpl = w.get("TestStreamObj");
		REQUIRE(tmpl != nullptr);
		const auto &n = tmpl->drawModules().nuggets();
		REQUIRE(n.size() == 1u);
		const auto *data = dynamic_cast<const W3DProjectileStreamDrawModuleData *>(n[0].data.get());
		REQUIRE(data != nullptr);
		CHECK(data->m_texture == "EXFireProjectile03.tga");
		CHECK(data->m_width == doctest::Approx(1.5f)); // tolerance: floating-point evaluation
		CHECK(data->m_tileFactor == doctest::Approx(3.0f)); // tolerance: floating-point evaluation
		CHECK(data->m_scrollRate == doctest::Approx(2.0f)); // tolerance: floating-point evaluation
		CHECK(data->m_maxSegments == 4);

		Thing thing(tmpl);
		W3DProjectileStreamDraw draw(&thing, data);
		CHECK(draw.isFullyObscuredByShroud() == false);
		draw.setFullyObscuredByShroud(true);
		CHECK(draw.isFullyObscuredByShroud() == true);

		XferSave save;
		draw.xfer(save);

		W3DProjectileStreamDraw restored(&thing, data);
		const auto &buf = save.data();
		XferLoad load(buf.data(), buf.size());
		restored.xfer(load);
		CHECK(restored.isFullyObscuredByShroud() == true);
	}
}

// ==============================================================================================
// 3. StrafeAreaUpdate Tests
// ==============================================================================================

TEST_CASE("modgap2 strafe: module data defaults and field table (both editions: RW 0x008A38F0 / 0x00BF7394, BFME2 0x003A4E5A / 0x00C1B1F8)")
{
	StrafeAreaUpdateModuleData d;
	CHECK(d.m_weaponName == "");
	CHECK(d.m_strafeAreaRadius == doctest::Approx(150.0f)); // tolerance: floating-point evaluation
	CHECK(d.m_sweepFrequency == doctest::Approx(0.4f)); // tolerance: floating-point evaluation
	CHECK(d.m_sweepAmplitude == doctest::Approx(100.0f)); // tolerance: floating-point evaluation
	CHECK(d.m_slope == doctest::Approx(50.0f)); // tolerance: floating-point evaluation
	CHECK(d.m_initialSweepPhase == doctest::Approx(0.0f)); // tolerance: floating-point evaluation

	const FieldParse *t = StrafeAreaUpdateTables::strafe();
	const char *const expected[] = { "WeaponName", "StrafeAreaRadius", "Sweepfrequency", "SweepAmplitude", "Slope", "InitialSweepPhase" };
	int n = 0;
	for (; t[n].token; ++n)
	{
		REQUIRE(n < 6); // tolerance: floating-point evaluation
		CHECK(std::strcmp(t[n].token, expected[n]) == 0);
	}
	CHECK(n == 6);
}

TEST_CASE("modgap2 strafe: INI parsing with WeaponName, Radius, Sweep parameters (both editions)")
{
	for (GameEdition edition : { GameEdition::RotWK201, GameEdition::BFME2_106 })
	{
		CAPTURE(gameEditionName(edition));
		combattest::CombatWorld w(kStrafeIni);
		w.logic->setEdition(edition);

		Object *obj = w.unit("TestStrafeObj", 'A', 100.0f, 100.0f);
		REQUIRE(obj != nullptr);

		auto *strafe = dynamic_cast<StrafeAreaUpdate *>(obj->findModule("StrafeAreaUpdate"));
		REQUIRE(strafe != nullptr);
		const StrafeAreaUpdateModuleData *data = strafe->getData();
		REQUIRE(data != nullptr);
		CHECK(data->m_weaponName == "TestStrafeWeapon");
		CHECK(data->m_strafeAreaRadius == doctest::Approx(180.0f)); // tolerance: floating-point evaluation
		CHECK(data->m_sweepFrequency == doctest::Approx(0.5f)); // tolerance: floating-point evaluation
		CHECK(data->m_sweepAmplitude == doctest::Approx(120.0f)); // tolerance: floating-point evaluation
		CHECK(data->m_slope == doctest::Approx(60.0f)); // tolerance: floating-point evaluation
		CHECK(data->m_initialSweepPhase == doctest::Approx(0.1f)); // tolerance: floating-point evaluation
	}
}

TEST_CASE("modgap2 strafe: initiateStrafe, state transitions, and stop line S-6026 (both editions)")
{
	for (GameEdition edition : { GameEdition::RotWK201, GameEdition::BFME2_106 })
	{
		CAPTURE(gameEditionName(edition));
		combattest::CombatWorld w(kStrafeIni);
		w.logic->setEdition(edition);

		Object *obj = w.unit("TestStrafeObj", 'A', 0.0f, 0.0f);
		REQUIRE(obj != nullptr);
		auto *strafe = dynamic_cast<StrafeAreaUpdate *>(obj->findModule("StrafeAreaUpdate"));
		REQUIRE(strafe != nullptr);

		Coord3D from{ 0.0f, 0.0f, 100.0f };
		Coord3D to{ 300.0f, 0.0f, 100.0f };
		bool ok = strafe->initiateStrafe(&from, &to);
		CHECK(ok == true);
		CHECK(strafe->state() == 0);
		CHECK(strafe->strafeTarget().x == doctest::Approx(300.0f)); // tolerance: floating-point evaluation

		// Position far away: state 0 approach
		strafe->update();
		CHECK(strafe->state() == 0);

		// Teleport closer to target: distance <= radius + slope (180 + 60 = 240)
		Coord3D nearPos{ 150.0f, 0.0f, 100.0f };
		obj->setPosition(&nearPos);
		strafe->update();
		// Should advance to state 1
		CHECK(strafe->state() == 1);
		CHECK(strafe->nextFireFrame() == obj->logic().getFrame() + 12);

		// Check stop line
		CHECK(std::string(StrafeAreaUpdate::stopLine()).find("S-6026") != std::string::npos);
	}
}

TEST_CASE("modgap2 strafe: xfer snapshot and crc state hashing (both editions)")
{
	for (GameEdition edition : { GameEdition::RotWK201, GameEdition::BFME2_106 })
	{
		CAPTURE(gameEditionName(edition));
		combattest::CombatWorld w(kStrafeIni);
		w.logic->setEdition(edition);

		Object *obj = w.unit("TestStrafeObj", 'A', 0.0f, 0.0f);
		REQUIRE(obj != nullptr);
		auto *strafe = dynamic_cast<StrafeAreaUpdate *>(obj->findModule("StrafeAreaUpdate"));
		REQUIRE(strafe != nullptr);

		Coord3D from{ 10.0f, 20.0f, 30.0f };
		Coord3D to{ 200.0f, 150.0f, 50.0f };
		strafe->initiateStrafe(&from, &to);

		StateHasher h1;
		strafe->crc(h1);

		XferSave save;
		strafe->xfer(save);

		Object *obj2 = w.unit("TestStrafeObj", 'A', 0.0f, 0.0f);
		auto *strafe2 = dynamic_cast<StrafeAreaUpdate *>(obj2->findModule("StrafeAreaUpdate"));
		REQUIRE(strafe2 != nullptr);

		const auto &buf = save.data();
		XferLoad load(buf.data(), buf.size());
		strafe2->xfer(load);

		CHECK(strafe2->strafeTarget().x == doctest::Approx(200.0f)); // tolerance: floating-point evaluation
		CHECK(strafe2->strafeTarget().y == doctest::Approx(150.0f)); // tolerance: floating-point evaluation
		CHECK(strafe2->strafeTarget().z == doctest::Approx(50.0f)); // tolerance: floating-point evaluation
		CHECK(strafe2->strafeStart().x == doctest::Approx(10.0f)); // tolerance: floating-point evaluation
		CHECK(strafe2->strafeStart().y == doctest::Approx(20.0f)); // tolerance: floating-point evaluation
		CHECK(strafe2->strafeStart().z == doctest::Approx(30.0f)); // tolerance: floating-point evaluation
		CHECK(strafe2->sweepPhase() == doctest::Approx(strafe->sweepPhase()));
		CHECK(strafe2->state() == strafe->state());
		CHECK(strafe2->started() == strafe->started());
		CHECK(strafe2->nextFireFrame() == strafe->nextFireFrame());
		CHECK(strafe2->friend_getNextCallFrame() == strafe->friend_getNextCallFrame());

		// strafe2 belongs to obj2, a distinct object in the world whose UpdateModule has a different
		// scheduler slot (m_indexInLogic: 1 vs 0). Per retail RW 0x850C60, xferUpdateModule transfers
		// only m_nextCallFrame; scheduler slots are world state restored by GameLogic's scheduler block.
		// Therefore strafe2->crc() legitimately differs from strafe->crc() across distinct objects:
		StateHasher h2;
		strafe2->crc(h2);
		CHECK(h2.value() != h1.value());

		// Aligning the scheduler slot proves all module-specific CRC state restored identically:
		strafe2->friend_setIndexInLogic(strafe->friend_getIndexInLogic(), strafe->friend_getPhaseInLogic());
		StateHasher h2Aligned;
		strafe2->crc(h2Aligned);
		CHECK(h2Aligned.value() == h1.value());
	}
}

// ==============================================================================================
// 4. W3DLightDraw Tests
// ==============================================================================================

TEST_CASE("modgap2 light: module data defaults and field table (both editions: RW 0x004CF510 / 0x00BDBB08, BFME2 0x000CFB3E / 0x00BCD918)")
{
	W3DLightDrawModuleData d;
	CHECK(d.m_ambient.red == 0.0f);
	CHECK(d.m_diffuse.red == 0.0f);
	CHECK(d.m_specular.red == 0.0f);
	CHECK(d.m_radius == doctest::Approx(20.0f)); // tolerance: floating-point evaluation
	CHECK(d.m_intensity == doctest::Approx(1.0f)); // tolerance: floating-point evaluation
	CHECK(d.m_flickerAmplitude == 0.0f);
	CHECK(d.m_flickerFrequency == 0.0f);
	CHECK(d.m_pulseAmplitude == 0.0f);
	CHECK(d.m_pulseFrequency == 0.0f);
	CHECK(d.m_attachToBone == "");

	const FieldParse *t = W3DLightDrawTables::light();
	const char *const expected[] = {
		"Ambient", "Diffuse", "Specular", "Radius", "Intensity",
		"FlickerAmplitude", "FlickerFrequency", "PulseAmplitude", "PulseFrequency",
		"AttachToBoneInAnotherModule"
	};
	int n = 0;
	for (; t[n].token; ++n)
	{
		REQUIRE(n < 10); // tolerance: floating-point evaluation
		CHECK(std::strcmp(t[n].token, expected[n]) == 0);
	}
	CHECK(n == 10);
}

TEST_CASE("modgap2 light: INI parsing, flicker stepping, and xfer (both editions)")
{
	for (GameEdition edition : { GameEdition::RotWK201, GameEdition::BFME2_106 })
	{
		CAPTURE(gameEditionName(edition));
		objtest::World w;
		W3DLightDraw::registerClass(w.modules);
		REQUIRE(w.load(kLightIni).empty());

		const ThingTemplate *tmpl = w.get("TestLightObj");
		REQUIRE(tmpl != nullptr);
		const auto &n = tmpl->drawModules().nuggets();
		REQUIRE(n.size() == 1u);
		const auto *data = dynamic_cast<const W3DLightDrawModuleData *>(n[0].data.get());
		REQUIRE(data != nullptr);

		CHECK(data->m_radius == doctest::Approx(45.0f)); // tolerance: floating-point evaluation
		CHECK(data->m_intensity == doctest::Approx(2.5f)); // tolerance: floating-point evaluation
		CHECK(data->m_flickerAmplitude == doctest::Approx(0.8f)); // tolerance: floating-point evaluation
		CHECK(data->m_flickerFrequency == doctest::Approx(8.0f)); // tolerance: floating-point evaluation
		CHECK(data->m_pulseAmplitude == doctest::Approx(0.3f)); // tolerance: floating-point evaluation
		CHECK(data->m_pulseFrequency == doctest::Approx(2.0f)); // tolerance: floating-point evaluation
		CHECK(data->m_attachToBone == "TorchBone");

		Thing thing(tmpl);
		W3DLightDraw light(&thing, data);
		CHECK(light.isEnabled() == true);
		CHECK(light.currentHeight() == doctest::Approx(2.5f)); // tolerance: floating-point evaluation

		// Step flicker simulation
		light.stepFlicker(0.1f);
		CHECK(light.phase() == doctest::Approx(0.1f)); // tolerance: floating-point evaluation

		// Shroud / disable toggle
		light.setEnabled(false);
		CHECK(light.isEnabled() == false);

		// Snapshot save / load
		XferSave save;
		light.xfer(save);

		W3DLightDraw restored(&thing, data);
		const auto &buf = save.data();
		XferLoad load(buf.data(), buf.size());
		restored.xfer(load);
		CHECK(restored.phase() == doctest::Approx(0.1f)); // tolerance: floating-point evaluation
	}
}
