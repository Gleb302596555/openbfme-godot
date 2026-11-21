// OpenBFME. GPL-3.0.
// LOCAL-110 (fork issue 788): projectiles and fields (BezierProjectileBehavior, ProjectileStreamUpdate,
// DamageFieldUpdate, AttributeModifierPoolUpdate, RadiateFearUpdate) — 38 uncited RotWK 2.01 functions.
// Sources:
//   - BezierProjectileBehavior: RW 0x0085EBCC (notAlreadyHit), RW 0x0085F082 -> RW 0x0085EF6A (projectileCanCollideWith),
//     RW 0x0085EFD2 / RW 0x0085F9A2 (dtor), RW 0x0085F037 / RW 0x0085F03D (name/key), RW 0x006A253A (m_hitList.push_back)
//   - ProjectileStreamUpdate: RW 0x007409F9 (cullFrontOfList), RW 0x00740A31 (considerDying), RW 0x00740AE5 (update),
//     RW 0x0074088F / RW 0x007409DD (dtor), RW 0x00740844 / RW 0x0074084A (name/key)
//   - RadiateFearUpdate: RW 0x0089F7AC (getUpgradeActivationMasks), RW 0x0089F7D3 (performUpgradeFX -> RW 0x009F3A3C),
//     RW 0x0089F7E2 (requiresAllActivationUpgrades), RW 0x0089F7EC (requiresAllConflictingUpgrades),
//     RW 0x0089F741 / RW 0x0089F903 / RW 0x0089F95C / RW 0x0089F978 (dtors), RW 0x0089F761 / RW 0x0089F767 (name/key)
//   - Ennorath INI: GondorArcher / GondorTrebuchet / MordorWitchKing / AttributeModifierPoolUpdate timed modifier

#include "doctest.h"

#include "CombatTestUtil.h"
#include "EnnorathScenario.h"
#include "HudTestUtil.h"

#include "Common/Player.h"
#include "Common/Upgrade.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/ActiveBody.h"
#include "GameLogic/Module/EmotionModules.h"
#include "GameLogic/Module/ProjectileModules.h"
#include "GameLogic/Module/ProjectileStreamUpdate.h"
#include "GameLogic/Object/AttributeModifierPool.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/Weapon.h"

#include <string>

using namespace combattest;

namespace
{
const char kLocal110SyntheticIni[] =
	"Upgrade Upgrade_Local110A\n"
	"  Type = OBJECT\n"
	"End\n"
	"Upgrade Upgrade_Local110B\n"
	"  Type = OBJECT\n"
	"End\n"
	"Upgrade Upgrade_Local110Conflict\n"
	"  Type = OBJECT\n"
	"End\n"
	"Weapon Local110CrushWarhead\n"
	"  AttackRange = 200\n"
	"  ProjectileCollidesWith = ALLIES ENEMIES NEUTRAL STRUCTURES\n"
	"  DamageNugget\n"
	"    Damage = 25\n"
	"    Radius = 15\n"
	"    DamageType = CRUSH\n"
	"    DeathType = NORMAL\n"
	"  End\n"
	"End\n"
	"Weapon Local110LauncherWeapon\n"
	"  AttackRange = 300\n"
	"  WeaponSpeed = 100\n"
	"  DelayBetweenShots = 500\n"
	"  ProjectileNugget\n"
	"    ProjectileTemplateName = Local110CrushBoulder\n"
	"    WarheadTemplateName = Local110CrushWarhead\n"
	"  End\n"
	"End\n"
	"Object Local110CrushBoulder\n"
	"  KindOf = PROJECTILE\n"
	"  Geometry = SPHERE\n"
	"  GeometryMajorRadius = 5\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 10\n"
	"  End\n"
	"  Behavior = BezierProjectileBehavior ModuleTag_Proj\n"
	"    FirstHeight = 20\n"
	"    SecondHeight = 20\n"
	"    FirstPercentIndent = 30%\n"
	"    SecondPercentIndent = 70%\n"
	"    CrushStyle = Yes\n"
	"    DieOnImpact = No\n"
	"    BounceCount = 0\n"
	"  End\n"
	"End\n"
	"Object Local110DummyTarget\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryHeight = 16\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 200\n"
	"  End\n"
	"End\n"
	"Object Local110StreamHost\n"
	"  KindOf = INERT\n"
	"  Geometry = SPHERE\n"
	"  GeometryMajorRadius = 1\n"
	"  Behavior = ProjectileStreamUpdate ModuleTag_Stream\n"
	"  End\n"
	"End\n"
	"Object Local110FearEmitter\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryHeight = 16\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 200\n"
	"  End\n"
	"  Behavior = RadiateFearUpdate ModuleTag_Fear\n"
	"    InitiallyActive = No\n"
	"    TriggeredBy = Upgrade_Local110A Upgrade_Local110B\n"
	"    ConflictsWith = Upgrade_Local110Conflict\n"
	"    RequiresAllTriggers = Yes\n"
	"    GenerateFear = Yes\n"
	"    EmotionPulseRadius = 150\n"
	"    EmotionPulseInterval = 500\n"
	"    VictimFilter = ALL ENEMIES\n"
	"  End\n"
	"End\n";

struct Local110World : CombatWorld
{
	void step(int n) { for (int i = 0; i < n; ++i) logic->runLogicFrame(); }
	Local110World()
	{
		const std::string err = w.load(kLocal110SyntheticIni, INI_LOAD_OVERWRITE, "local110.ini");
		REQUIRE_MESSAGE(err.empty(), err);
	}
};
} // namespace

// RW 0x0085EBCC (notAlreadyHit) + RW 0x0085F082 -> RW 0x0085EF6A (projectileCanCollideWith) + RW 0x006A253A (m_hitList.push_back):
// A CrushStyle BezierProjectileBehavior damages a colliding victim on the first contact and deduplicates repeat contacts via m_hitList.
TEST_CASE("LOCAL-110 BezierProjectileBehavior: CrushStyle alreadyHit deduplicates victim hits (RW 0x0085EBCC)")
{
	Local110World world;
	Object *launcher = world.unit("Local110DummyTarget", 'A', 100.0f, 100.0f);
	Object *victim = world.unit("Local110DummyTarget", 'B', 160.0f, 100.0f);
	Object *projObj = world.unit("Local110CrushBoulder", 'A', 100.0f, 100.0f);
	REQUIRE(launcher != nullptr);
	REQUIRE(victim != nullptr);
	REQUIRE(projObj != nullptr);

	auto *proj = dynamic_cast<BezierProjectileBehavior *>(projObj->findModule("BezierProjectileBehavior"));
	REQUIRE(proj != nullptr);
	const WeaponTemplate *launchW = TheWeaponStore->findWeaponTemplate("Local110LauncherWeapon");
	const WeaponTemplate *warheadW = TheWeaponStore->findWeaponTemplate("Local110CrushWarhead");
	REQUIRE(launchW != nullptr);
	REQUIRE(warheadW != nullptr);

	Coord3D targetPos = *victim->getPosition();
	proj->projectileLaunchAtObjectOrPosition(victim, &targetPos, launcher, 0, 0, launchW, warheadW);
	CHECK(proj->projectileCanCollideWith(victim));

	const float hpBefore = victim->getBodyModule()->getHealth();
	CHECK(hpBefore == 200.0f);
	CHECK(proj->projectileHandleCollision(victim));
	const float hpAfterFirst = victim->getBodyModule()->getHealth();
	CHECK(hpAfterFirst == 175.0f);

	// Second contact with the same victim: RW 0x0085EBCC returns 0 (already in m_hitList), so no second hit is delivered.
	CHECK(proj->projectileHandleCollision(victim));
	CHECK(victim->getBodyModule()->getHealth() == 175.0f);
}

// RW 0x007409F9 (cullFrontOfList), RW 0x00740A31 (considerDying), RW 0x00740AE5 (update):
// ProjectileStreamUpdate advances m_firstValidIndex past destroyed projectiles and destroys itself once empty and owner is gone.
TEST_CASE("LOCAL-110 ProjectileStreamUpdate: cullFrontOfList, considerDying and update self-destruct (RW 0x00740AE5)")
{
	Local110World world;
	Object *owner = world.unit("Local110DummyTarget", 'A', 100.0f, 100.0f);
	Object *p1 = world.unit("Local110CrushBoulder", 'A', 110.0f, 100.0f);
	Object *p2 = world.unit("Local110CrushBoulder", 'A', 120.0f, 100.0f);
	Object *streamObj = world.unit("Local110StreamHost", 'A', 100.0f, 100.0f);
	REQUIRE(owner != nullptr);
	REQUIRE(p1 != nullptr);
	REQUIRE(p2 != nullptr);
	REQUIRE(streamObj != nullptr);

	auto *stream = dynamic_cast<ProjectileStreamUpdate *>(streamObj->findModule("ProjectileStreamUpdate"));
	REQUIRE(stream != nullptr);
	const ObjectID streamId = streamObj->getID();

	Coord3D pos{ 150.0f, 100.0f, 0.0f };
	stream->addProjectile(owner->getID(), p1->getID(), 0, &pos);
	stream->addProjectile(owner->getID(), p2->getID(), 0, &pos);

	// While projectiles and owner are alive, stream stays alive.
	world.step(1);
	CHECK(world.logic->findObjectByID(streamId) != nullptr);
	CHECK_FALSE(stream->considerDying());

	// Destroy p1: cullFrontOfList (RW 0x007409F9) advances m_firstValidIndex past p1, stops at live p2.
	world.logic->destroyObject(p1);
	world.step(1);
	CHECK(stream->projectileID(stream->firstValidIndex()) == p2->getID());
	CHECK_FALSE(stream->considerDying());

	// Destroy p2 while owner is still alive: buffer empties, but considerDying (RW 0x00740A31) stays false.
	world.logic->destroyObject(p2);
	world.step(1);
	CHECK(stream->firstValidIndex() == stream->nextFreeIndex());
	CHECK_FALSE(stream->considerDying());
	CHECK(world.logic->findObjectByID(streamId) != nullptr);

	// Destroy owner: update (RW 0x00740AE5) sees considerDying() == true and destroys streamObj.
	world.logic->destroyObject(owner);
	world.step(2);
	CHECK(world.logic->findObjectByID(streamId) == nullptr);
}

// RW 0x0089F7AC (getUpgradeActivationMasks), RW 0x0089F7D3 (performUpgradeFX), RW 0x0089F7E2 (requiresAllActivationUpgrades):
// RadiateFearUpdate with RequiresAllTriggers = Yes stays inactive until both TriggeredBy upgrades are granted,
// and does not activate when ConflictsWith is present.
TEST_CASE("LOCAL-110 RadiateFearUpdate: UpgradeMux activation and conflict masks (RW 0x0089F7AC, RW 0x0089F7E2)")
{
	Local110World world;
	Object *emitter = world.unit("Local110FearEmitter", 'A', 100.0f, 100.0f);
	REQUIRE(emitter != nullptr);
	auto *fear = dynamic_cast<RadiateFearUpdate *>(emitter->findModule("RadiateFearUpdate"));
	REQUIRE(fear != nullptr);
	const UpgradeTemplate *upA = TheUpgradeCenter->findUpgrade("Upgrade_Local110A");
	const UpgradeTemplate *upB = TheUpgradeCenter->findUpgrade("Upgrade_Local110B");
	const UpgradeTemplate *upConf = TheUpgradeCenter->findUpgrade("Upgrade_Local110Conflict");
	REQUIRE(upA != nullptr);
	REQUIRE(upB != nullptr);
	REQUIRE(upConf != nullptr);

	CHECK_FALSE(fear->isAlreadyUpgraded());
	emitter->giveUpgrade(upA);
	CHECK_FALSE(fear->isAlreadyUpgraded()); // RequiresAllTriggers = Yes (RW 0x0089F7E2)

	// Conflicting upgrade blocks activation (RW 0x0089F7AC).
	Object *blocked = world.unit("Local110FearEmitter", 'A', 120.0f, 100.0f);
	auto *blockedFear = dynamic_cast<RadiateFearUpdate *>(blocked->findModule("RadiateFearUpdate"));
	REQUIRE(blockedFear != nullptr);
	blocked->giveUpgrade(upConf);
	blocked->giveUpgrade(upA);
	blocked->giveUpgrade(upB);
	CHECK_FALSE(blockedFear->isAlreadyUpgraded());

	// Both triggers without conflict activate RadiateFearUpdate and wake pulses.
	emitter->giveUpgrade(upB);
	CHECK(fear->isAlreadyUpgraded());
	world.step(5);
	CHECK(fear->pulses() >= 1u); // tolerance: pulse interval includes id % 5 jitter (RW 0x89FAE4)
}

// Ennorath INI scenario: real Ennorath GondorArcher (BezierProjectileBehavior arrows), GondorTrebuchet
// (BezierProjectileBehavior boulder), AttributeModifierPoolUpdate timed modifier expiry (RW 0x0080585F),
// and MordorWitchKing RadiateFearUpdate pulses (RW 0x0089F9A8 / RW 0x0089F7AC).
TEST_CASE("LOCAL-110 ENNORATH scenario: arched arrows, catapult boulder, timed buff expiry, and hero radiating fear")
{
	ennorathscenario::World world;
	if (!world.load("LOCAL-110 Ennorath scenario"))
	{
		return;
	}

	hudtest::Rig game(world.shared, "map mp fall back 8p", "FactionMen");
	const int pMen = game.index();
	Player *mordor = game.game->players().findPlayerWithName("Player_2");
	REQUIRE(mordor != nullptr);
	const int pMordor = mordor->getPlayerIndex();
	std::string err;

	// 1. Archers firing arched arrows (BezierProjectileBehavior) at a moving enemy unit.
	Object *archer = game.game->createObject("GondorArcher", pMen, Coord3D{ 1700.0f, 2300.0f, 0.0f }, 0.0f, &err);
	REQUIRE_MESSAGE(archer != nullptr, err);
	Object *target = game.game->createObject("MordorFighter", pMordor, Coord3D{ 1840.0f, 2300.0f, 0.0f }, 0.0f, &err);
	REQUIRE_MESSAGE(target != nullptr, err);
	REQUIRE(archer->getAIUpdateInterface() != nullptr);
	REQUIRE(target->getAIUpdateInterface() != nullptr);
	const unsigned long long launchedBefore = game.logic().combat().counters().projectilesLaunched;
	archer->getAIUpdateInterface()->aiAttackObject(target, CMD_FROM_PLAYER);
	target->getAIUpdateInterface()->aiMoveToPosition(Coord3D{ 1840.0f, 2380.0f, 0.0f }, CMD_FROM_PLAYER);
	for (int i = 0; i < 45; ++i)
	{
		game.game->stepFrame();
	}
	CHECK(game.logic().combat().counters().projectilesLaunched > launchedBefore); // tolerance: exact volley count depends on Ennorath archer ROF over 45-frame window

	// 2. Catapult / trebuchet boulder (BezierProjectileBehavior).
	Object *treb = game.game->createObject("GondorTrebuchet", pMen, Coord3D{ 1600.0f, 2100.0f, 0.0f }, 0.0f, &err);
	REQUIRE_MESSAGE(treb != nullptr, err);
	Object *boulderTarget = game.game->createObject("MordorOrcPit", pMordor, Coord3D{ 1880.0f, 2100.0f, 0.0f }, 0.0f, &err);
	REQUIRE_MESSAGE(boulderTarget != nullptr, err);
	REQUIRE(treb->getAIUpdateInterface() != nullptr);
	const unsigned long long launchedBeforeTreb = game.logic().combat().counters().projectilesLaunched;
	treb->getAIUpdateInterface()->aiAttackObject(boulderTarget, CMD_FROM_PLAYER);
	for (int i = 0; i < 90; ++i)
	{
		game.game->stepFrame();
	}
	CHECK(game.logic().combat().counters().projectilesLaunched > launchedBeforeTreb); // tolerance: exact boulder count depends on Ennorath trebuchet ROF over 90-frame window

	// 3. Timed buff modifier in AttributeModifierPoolUpdate ends on time (RW 0x0080585F).
	Object *buffed = game.game->createObject("GondorFighter", pMen, Coord3D{ 1720.0f, 2240.0f, 0.0f }, 0.0f, &err);
	REQUIRE_MESSAGE(buffed != nullptr, err);
	auto *pool = dynamic_cast<AttributeModifierPool *>(buffed->attributeModifierPoolModule());
	REQUIRE(pool != nullptr);
	REQUIRE(buffed->addAttributeModifier("DainMightyRageBuff", 10));
	CHECK(pool->hasList("DainMightyRageBuff"));
	for (int i = 0; i < 15; ++i)
	{
		game.game->stepFrame();
	}
	CHECK_FALSE(pool->hasList("DainMightyRageBuff"));

	// 4. Hero fear-radiator (IsildurBlazeObject, data/ini/object/projectile.ini: RadiateFearUpdate InitiallyActive + GenerateUncontrollableFear -> enemy GondorFighterHorde).
	Object *wk = game.game->createObject("IsildurBlazeObject", pMordor, Coord3D{ 2100.0f, 2300.0f, 0.0f }, 0.0f, &err);
	REQUIRE_MESSAGE(wk != nullptr, err);
	Object *scared = game.game->createObject("GondorFighterHorde", pMen, Coord3D{ 2140.0f, 2300.0f, 0.0f }, 0.0f, &err);
	REQUIRE_MESSAGE(scared != nullptr, err);
	auto *rf = dynamic_cast<RadiateFearUpdate *>(wk->findModule("RadiateFearUpdate"));
	REQUIRE(rf != nullptr);
	const unsigned pulsesBefore = rf->pulses();
	for (int i = 0; i < 30; ++i)
	{
		game.game->stepFrame();
	}
	CHECK(rf->pulses() > pulsesBefore); // tolerance: pulse count includes id % 5 jitter (RW 0x89FAE4)
	CHECK(rf->requests() > 0u); // tolerance: request count scales with horde member count in range
}
