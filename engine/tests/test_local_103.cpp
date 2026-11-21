// OpenBFME. LOCAL-103 tests (fork issue 773): client draw modules
// (SwayClientUpdate, W3DLaserDraw, W3DDebrisDraw, W3DStreakDraw, W3DTruckDraw, W3DProjectileStreamDraw).
// Expectations from RotWK 2.01 game.dat (RW 0x008CD514..0x008CD9CD, RW 0x00740A56) and Ennorath 1.9.8.3 INI.

#include "doctest.h"
#include "CombatTestUtil.h"
#include "EnnorathScenario.h"

#include "Common/NameKeyGenerator.h"
#include "Common/XferCRC.h"
#include "Common/XferLoad.h"
#include "Common/XferSave.h"
#include "GameClient/ClientBehaviorModules.h"
#include "GameClient/ClientEvents.h"
#include "GameClient/Drawable.h"
#include "GameClient/DrawableManager.h"
#include "GameClient/SwayClientUpdate.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DDrawModules.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DProjectileStreamDraw.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DStreakDraw.h"
#include "GameEngineDevice/W3DDevice/GameClient/W3DVegetationSway.h"
#include "GameLogic/Module/ProjectileStreamUpdate.h"
#include "GameLogic/ScriptEngine/ScriptEngine.h"

#include <cmath>
#include <cstring>
#include <string>
#include <vector>

namespace
{
const char *kSwaySyntheticIni =
	"Object Local103SwayTree\n"
	"  Draw = W3DDefaultDraw ModuleTag_Draw\n"
	"  End\n"
	"  ClientUpdate = SwayClientUpdate ModuleTag_Sway\n"
	"  End\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 5\n"
	"  GeometryHeight = 20\n"
	"End\n";

const char *kStreamSyntheticIni =
	"Object Local103Stream\n"
	"  Draw = W3DProjectileStreamDraw ModuleTag_Draw\n"
	"    Width = 2.0\n"
	"    TileFactor = 1.0\n"
	"    ScrollRate = 0.5\n"
	"    MaxSegments = 10\n"
	"  End\n"
	"  Behavior = ProjectileStreamUpdate ModuleTag_Update\n"
	"  End\n"
	"End\n"
	"Object Local103Shot\n"
	"  Geometry = SPHERE\n"
	"  GeometryMajorRadius = 1\n"
	"End\n";

SwayClientUpdate *findSway(const Drawable *d)
{
	if (!d)
	{
		return nullptr;
	}
	for (const auto &m : d->clientModules())
	{
		if (auto *s = dynamic_cast<SwayClientUpdate *>(m.get()))
		{
			return s;
		}
	}
	return nullptr;
}
} // namespace

// Expectations from RW 0x008CD514, RW 0x008CD51A, RW 0x008CD55F (DONOR BFME2 0x004C90E0 / 0x004C9125).
TEST_CASE("LOCAL-103 SwayClientUpdate: getModuleName, getModuleNameKey, updateSway and zero randomness reset (RW 0x008CD514 / 0x008CD51A / 0x008CD55F)")
{
	CHECK(std::strcmp(SwayClientUpdate::getModuleName(), "SwayClientUpdate") == 0);
	NameKeyGenerator gen;
	gen.init();
	const NameKeyType k1 = SwayClientUpdate::getModuleNameKey(gen);
	const NameKeyType k2 = gen.nameToKey("SwayClientUpdate");
	CHECK(k1 != NAMEKEY_INVALID);
	CHECK(k1 == k2);

	combattest::CombatWorld cw;
	ClientBehaviorModules::registerAll(cw.w.modules);
	W3DDrawModules::registerTypedDrawModuleData(cw.w.modules);
	REQUIRE(cw.w.load(kSwaySyntheticIni).empty());

	ArchiveW3DFileSource source(cw.w.fx.fsys);
	WW3DAssetManager assets(source);
	DrawableManager dm(assets, (*cw.logic));
	ClientEventRecorder rec((*cw.logic));
	cw.logic->setClientHooks(&rec);

	const ThingTemplate *tt = cw.w.things.findTemplate("Local103SwayTree");
	REQUIRE(tt != nullptr);
	Object *o = cw.logic->newObject(tt, cw.players.getNeutralPlayer()->getDefaultTeam(), ObjectStatusMaskType{});
	REQUIRE(o != nullptr);
	const float objYaw = 0.5f;
	{ Coord3D p{ 100.0f, 100.0f, 0.0f }; o->setPosition(&p); }
	o->setOrientation(objYaw);
	dm.applyEvents(rec.take());

	Drawable *d = dm.findByObject(o->getID());
	REQUIRE(d != nullptr);
	d->setOrientation(objYaw);

	SwayClientUpdate *sway = findSway(d);
	REQUIRE(sway != nullptr);
	CHECK(!sway->isUnported());
	CHECK(sway->curVersion() == -1);
	CHECK(sway->isSwaying());

	W3DClientRandom rng(RandomAlgorithm::ZH_CarryChain);
	rng.seed(42);

	W3DVegetationSway::Breeze b;
	b.direction = 1.25f;
	b.directionX = std::sin(b.direction);
	b.directionY = std::cos(b.direction);
	b.intensity = 0.08f;
	b.lean = 0.03f;
	b.randomness = 0.0f;
	b.breezePeriod = 50;
	b.breezeVersion = 3;

	sway->updateSway(b, rng, 6.0f);
	CHECK(sway->curValue() == 0.0f);
	CHECK(sway->curAngleLimit() == 0.08f);
	CHECK(sway->curDelta() == (1.0f / (6.0f * 50.0f)) * 6.2831854820251465f);
	CHECK(sway->leanAngle() == 0.03f);
	CHECK(sway->curVersion() == 3);
	CHECK(sway->directionSin() == doctest::Approx(std::sin(b.direction - objYaw)).epsilon(1e-4)); // tolerance: atan2/sin rounding from drawable basis
	CHECK(sway->directionCos() == doctest::Approx(std::cos(b.direction - objYaw)).epsilon(1e-4)); // tolerance: atan2/cos rounding from drawable basis

	// Non-zero randomness scales within [1 - r/2, 1 + r/2] (RW 0x008CD593..0x008CD637)
	b.randomness = 0.4f;
	b.breezeVersion = 4;
	sway->updateSway(b, rng, 6.0f);
	CHECK(sway->curVersion() == 4);
	CHECK(sway->curAngleLimit() >= 0.08f * 0.8f - 1e-6f); // tolerance: random draw in [1 - r/2, 1 + r/2]
	CHECK(sway->curAngleLimit() <= 0.08f * 1.2f + 1e-6f); // tolerance: random draw in [1 - r/2, 1 + r/2]
	CHECK(sway->leanAngle() >= 0.03f * 0.8f - 1e-6f); // tolerance: random draw in [1 - r/2, 1 + r/2]
	CHECK(sway->leanAngle() <= 0.03f * 1.2f + 1e-6f); // tolerance: random draw in [1 - r/2, 1 + r/2]

	cw.logic->setClientHooks(nullptr);
}

// Expectations from RW 0x008CD675 (DONOR BFME2 0x004C923B).
TEST_CASE("LOCAL-103 SwayClientUpdate: clientUpdate visibility gate, breezeVersion update, instanceMatrix rotation, and burned/dead stop (RW 0x008CD675)")
{
	combattest::CombatWorld cw;
	ClientBehaviorModules::registerAll(cw.w.modules);
	W3DDrawModules::registerTypedDrawModuleData(cw.w.modules);
	REQUIRE(cw.w.load(kSwaySyntheticIni).empty());

	ArchiveW3DFileSource source(cw.w.fx.fsys);
	WW3DAssetManager assets(source);
	DrawableManager dm(assets, (*cw.logic));
	ClientEventRecorder rec((*cw.logic));
	cw.logic->setClientHooks(&rec);

	const ThingTemplate *tt = cw.w.things.findTemplate("Local103SwayTree");
	REQUIRE(tt != nullptr);
	Object *o = cw.logic->newObject(tt, cw.players.getNeutralPlayer()->getDefaultTeam(), ObjectStatusMaskType{});
	REQUIRE(o != nullptr);
	{ Coord3D p{ 50.0f, 50.0f, 0.0f }; o->setPosition(&p); }
	dm.applyEvents(rec.take());

	Drawable *d = dm.findByObject(o->getID());
	REQUIRE(d != nullptr);
	SwayClientUpdate *sway = findSway(d);
	REQUIRE(sway != nullptr);

	W3DClientRandom rng(RandomAlgorithm::ZH_CarryChain);
	rng.seed(7);

	W3DVegetationSway::Breeze b;
	b.direction = 0.7f;
	b.directionX = std::sin(b.direction);
	b.directionY = std::cos(b.direction);
	b.intensity = 0.1f;
	b.lean = 0.05f;
	b.randomness = 0.0f;
	b.breezePeriod = 10;
	b.breezeVersion = 1;

	// Version changed (-1 -> 1): updates even when visible == false (RW 0x008CD688..0x008CD69E)
	sway->clientUpdate(b, rng, /*visible=*/false, /*burnedOrDead=*/false, 6.0f);
	CHECK(sway->curVersion() == 1);
	CHECK(d->hasInstanceMatrix());
	const float angleAfterFirst = sway->curAngle();
	CHECK(angleAfterFirst == doctest::Approx(std::sin(sway->curDelta()) * 0.1f + 0.05f)); // tolerance: sin float rounding

	// Same version and visible == false: skips update (RW 0x008CD6A5..0x008CD6B4)
	const float valBeforeHidden = sway->curValue();
	sway->clientUpdate(b, rng, /*visible=*/false, /*burnedOrDead=*/false, 6.0f);
	CHECK(sway->curValue() == valBeforeHidden);
	CHECK(sway->curAngle() == angleAfterFirst);

	// Same version and visible == true: advances phase and rotates instanceMatrix
	sway->clientUpdate(b, rng, /*visible=*/true, /*burnedOrDead=*/false, 6.0f);
	CHECK(sway->curValue() == valBeforeHidden + sway->curDelta());
	CHECK(sway->curAngle() == doctest::Approx(std::sin(sway->curValue()) * 0.1f + 0.05f)); // tolerance: sin float rounding
	// entryTransform reflects the tilted instanceMatrix
	REQUIRE(!d->entries().empty());
	Matrix3D xf;
	d->entryTransform(d->entries()[0], xf);
	CHECK(std::fabs(xf.Row[0][2]) + std::fabs(xf.Row[1][2]) > 1e-4f); // tolerance: non-zero tilt from pre-rotate X/Y

	// Burned or dead stops sway (RW 0x008CD917..0x008CD937)
	sway->clientUpdate(b, rng, /*visible=*/true, /*burnedOrDead=*/true, 6.0f);
	CHECK(!sway->isSwaying());
	const float stoppedVal = sway->curValue();
	sway->clientUpdate(b, rng, /*visible=*/true, /*burnedOrDead=*/false, 6.0f);
	CHECK(sway->curValue() == stoppedVal);

	cw.logic->setClientHooks(nullptr);
}

// Expectations from RW 0x008CD93F and RW 0x008CD9CD (DONOR BFME2 0x004C9505 / 0x004C9593).
TEST_CASE("LOCAL-103 SwayClientUpdate: xfer round-trip, XFER_CRC early-out, and loadPostProcess (RW 0x008CD93F / 0x008CD9CD)")
{
	SwayClientUpdateModuleData data;
	SwayClientUpdate src(nullptr, &data);

	W3DClientRandom rng(RandomAlgorithm::ZH_CarryChain);
	rng.seed(11);
	W3DVegetationSway::Breeze b;
	b.direction = 1.0f;
	b.intensity = 0.06f;
	b.lean = 0.02f;
	b.randomness = 0.0f;
	b.breezePeriod = 20;
	b.breezeVersion = 5;
	src.updateSway(b, rng, 6.0f);

	// XFER_CRC early-out (RW 0x008CD94D..0x008CD956)
	XferCRC crcXfer;
	const std::uint32_t crcBefore = crcXfer.getCRC();
	src.xfer(crcXfer);
	CHECK(crcXfer.getCRC() == crcBefore);

	// Save -> Load round-trip (RW 0x008CD958..0x008CD9C5)
	XferSave save;
	src.xfer(save);
	REQUIRE(!save.data().empty());

	SwayClientUpdate dst(nullptr, &data);
	XferLoad load(save.data());
	dst.xfer(load);
	CHECK(dst.curValue() == src.curValue());
	CHECK(dst.curAngle() == src.curAngle());
	CHECK(dst.curDelta() == src.curDelta());
	CHECK(dst.curAngleLimit() == src.curAngleLimit());
	CHECK(dst.leanAngle() == src.leanAngle());
	CHECK(dst.curVersion() == src.curVersion());
	CHECK(dst.isSwaying() == src.isSwaying());
	CHECK(dst.directionSin() == src.directionSin());
	CHECK(dst.directionCos() == src.directionCos());

	// loadPostProcess re-runs updateSway (RW 0x008CD9CD -> jmp 0x008CD55F)
	b.breezeVersion = 9;
	b.intensity = 0.12f;
	dst.loadPostProcess(b, rng, 6.0f);
	CHECK(dst.curVersion() == 9);
	CHECK(dst.curAngleLimit() == 0.12f);
}

// Expectations from RW 0x00740A56 (ProjectileStreamUpdate::getAllPoints) and RW 0x004D0D05 (W3DProjectileStreamDraw::doDrawModule).
TEST_CASE("LOCAL-103 ProjectileStreamUpdate::getAllPoints: circular buffer and dead-projectile zero holes (RW 0x00740A56)")
{
	combattest::CombatWorld cw;
	ProjectileStreamUpdate::registerClass(cw.w.modules);
	W3DDrawModules::registerTypedDrawModuleData(cw.w.modules);
	REQUIRE(cw.w.load(kStreamSyntheticIni).empty());

	const ThingTemplate *streamTt = cw.w.things.findTemplate("Local103Stream");
	const ThingTemplate *shotTt = cw.w.things.findTemplate("Local103Shot");
	REQUIRE(streamTt != nullptr);
	REQUIRE(shotTt != nullptr);

	Object *streamObj = cw.logic->newObject(streamTt, cw.players.getNeutralPlayer()->getDefaultTeam(), ObjectStatusMaskType{});
	Object *p1 = cw.logic->newObject(shotTt, cw.players.getNeutralPlayer()->getDefaultTeam(), ObjectStatusMaskType{});
	Object *p2 = cw.logic->newObject(shotTt, cw.players.getNeutralPlayer()->getDefaultTeam(), ObjectStatusMaskType{});
	Object *p3 = cw.logic->newObject(shotTt, cw.players.getNeutralPlayer()->getDefaultTeam(), ObjectStatusMaskType{});
	REQUIRE(streamObj != nullptr);
	{ Coord3D p{ 10.0f, 0.0f, 5.0f }; p1->setPosition(&p); }
	{ Coord3D p{ 20.0f, 0.0f, 5.0f }; p2->setPosition(&p); }
	{ Coord3D p{ 30.0f, 0.0f, 5.0f }; p3->setPosition(&p); }

	auto *psu = dynamic_cast<ProjectileStreamUpdate *>(streamObj->findModule("ProjectileStreamUpdate"));
	REQUIRE(psu != nullptr);
	psu->addProjectile(streamObj->getID(), p1->getID(), 0, nullptr);
	psu->addProjectile(streamObj->getID(), p2->getID(), 0, nullptr);
	psu->addProjectile(streamObj->getID(), p3->getID(), 0, nullptr);

	// Destroy middle projectile: RW 0x00740AAF outputs (0,0,0) hole
	cw.logic->destroyObject(p2);

	Coord3D pts[ProjectileStreamUpdate::MAX_PROJECTILE_STREAM];
	int count = 0;
	psu->getAllPoints(pts, &count);
	REQUIRE(count == 3);
	CHECK(pts[0].x == 10.0f);
	CHECK(pts[1].x == 0.0f);
	CHECK(pts[1].y == 0.0f);
	CHECK(pts[1].z == 0.0f);
	CHECK(pts[2].x == 30.0f);
}

// Expectations from Ennorath 1.9.8.3 INI (cinematicobjects.ini, goodfactionsubobjects.ini, system.ini,
// evilfactionsubobjects.ini, angmarporter.ini, firedrake.ini) and RW 0x008CD675.
TEST_CASE("LOCAL-103 ENNORATH scenario: SwayClientUpdate and client draw modules on real Ennorath objects")
{
	ennorathscenario::World world;
	if (!world.load("LOCAL-103 ENNORATH scenario client draw modules"))
	{
		return;
	}
	hudtest::Rig game(world.shared);
	std::string error;

	// 1. SwayClientUpdate on PTree_cine (data/ini/object/modcinematic/cinematicobjects.ini)
	Object *tree = game.game->createObject("PTree_cine", game.index(), Coord3D{ 1100.0f, 1100.0f, 0.0f }, 0.0f, &error);
	REQUIRE_MESSAGE(tree != nullptr, error);

	// 2. Real Ennorath objects for the other 5 draw module classes
	Object *laser = game.game->createObject("GandalfLaserBeam", game.index(), Coord3D{ 1120.0f, 1100.0f, 0.0f }, 0.0f, &error);
	REQUIRE_MESSAGE(laser != nullptr, error);
	Object *debris = game.game->createObject("GenericDebris", game.index(), Coord3D{ 1140.0f, 1100.0f, 0.0f }, 0.0f, &error);
	REQUIRE_MESSAGE(debris != nullptr, error);
	Object *arrow = game.game->createObject("BarbedArrowProjectile", game.index(), Coord3D{ 1160.0f, 1100.0f, 0.0f }, 0.0f, &error);
	REQUIRE_MESSAGE(arrow != nullptr, error);
	Object *cart = game.game->createObject("AngmarPorter", game.index(), Coord3D{ 1180.0f, 1100.0f, 0.0f }, 0.0f, &error);
	REQUIRE_MESSAGE(cart != nullptr, error);
	Object *stream = game.game->createObject("FlamethrowerProjectileStream", game.index(), Coord3D{ 1200.0f, 1100.0f, 0.0f }, 0.0f, &error);
	REQUIRE_MESSAGE(stream != nullptr, error);

	// Set breeze on ScriptEngine (SET_TREE_SWAY RW 0x6031A9) and step client frames
	game.logic().scriptEngine().setSway(0.8f, 0.09f, 0.03f, 25, 0.0f);
	game.logic().runLogicFrame();
	game.game->refreshClient(1000.0 / 30.0, 0.0);

	Drawable *treeDraw = game.game->drawables().findByObject(tree->getID());
	REQUIRE(treeDraw != nullptr);
	SwayClientUpdate *sway = findSway(treeDraw);
	REQUIRE(sway != nullptr);
	CHECK(!sway->isUnported());
	CHECK(sway->isSwaying());
	CHECK(treeDraw->hasInstanceMatrix());
	CHECK(sway->curAngle() == doctest::Approx(std::sin(sway->curValue()) * 0.09f + 0.03f)); // tolerance: sin float rounding

	// Check drawables exist for the other 5 classes too
	CHECK(game.game->drawables().findByObject(laser->getID()) != nullptr);
	CHECK(game.game->drawables().findByObject(debris->getID()) != nullptr);
	CHECK(game.game->drawables().findByObject(arrow->getID()) != nullptr);
	CHECK(game.game->drawables().findByObject(cart->getID()) != nullptr);
	CHECK(game.game->drawables().findByObject(stream->getID()) != nullptr);
}
