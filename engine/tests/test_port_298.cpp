// OpenBFME unit tests: PORT-298 (fork issue 690): RespawnUpdate's unread DeathAnimationTime and its four
// own RotWK functions the function map had uncited (xfer RW 0x8B319D, getModuleNameKey RW 0x8B32C4, name
// RW 0x8B3309, the deleting destructor RW 0x8B332D, the last being no behaviour).
//
// RotWK 2.01 facts (game.dat md5 daed3668..., each read with tools/mod/rw.py from the first instruction):
//   DeathAnimationTime: the parse row RW 0xC6CEA0 (table RW 0xC6CE80) stores frames through
//   parseDurationUnsignedInt (RW 0x73A429) at module data +0xFC; the only instruction of the whole unit
//   RW 0x8B307E..0x8B4271 that addresses +0xFC is the data constructor's zero write RW 0x8B3CC4; none of the
//   ten references of the name string RW 0xC0B098 (factory RW 0x65A002, Object RW 0x691806, the hero list
//   RW 0x780ED1 / RW 0x7815E7, ProductionUpdate RW 0x8A16B9 / 0x8A1BCE, ActiveBody RW 0x8C45DC, RespawnBody
//   RW 0x8C55FA, the module itself) reads it; BFME2 1.06 agrees (m_fieldFC: the parse row and the constructor
//   only). Verdict S-7323 (PORT-129): parsed, never read — nothing waits for it, not the revive record, not
//   the body, not the respawn clock. These tests lock that behaviourally.
//   The timing that DOES rule the respawn (RW 0x8B3744 onDeath: no wake-frame change on the rule path, the
//   module sleeps since the ctor RW 0x8B3244's setWakeFrame(obj, UPDATE_SLEEP_FOREVER) at RW 0x8B32AE; the
//   revive record joins the player's hero list in the same call, RW 0x8B385F..0x8B3897):
//   onRevived (RW 0x8B38A2) sets state 4 (STATE_SPAWN_ANIMATION) and wakes after RespawnAnimationTime
//   (+0x100, the wake at RW 0x8B3A26..0x8B3A2E) or InitialSpawnAnimationTime (+0x104) for an initial spawn;
//   update (RW 0x8B3A35) at that frame: state 4 -> 0 (RW 0x8B3A93), RespawnAnim and InitialSpawnAnim cleared,
//   UNSELECTABLE and DISABLED_PARALYZED cleared (RW 0x62684D / 0x692443 at RW 0x8B3A7E..0x8B3A92).
//   xfer (RW 0x8B319D, primary vtable 0xC6C360 slot 3): version (1, 1), the UpdateModule base (RW 0x850C60),
//   +0x2C state (labelled int RW 0x7078CF), +0x30 ObjectID (RW 0x707887), +0x34 / +0x38 int, +0x40 bool,
//   +0x3C int, +0x20 real, +0x41 bool; the port keeps its native crc order with the same eight fields (S-4190).
//   getRespawnProgress (RW 0x8B310D, slot 13): *out = +0x3C when out != null (RW 0x8B3118); state 0, 2 and 4
//   return 0.0f ([0xC1B594]), state 1 returns 1.0f ([0xBD1908], RW 0x8B3163), state 3 with +0x38 > 0 returns
//   (frame - +0x34) / +0x38 (RW 0x8B3139..0x8B3161), else 0.0f.

#include "doctest.h"

#include "ProdTestUtil.h"

#include "Common/PlayerHeroList.h"
#include "Common/Xfer.h"
#include "Common/XferLoad.h"
#include "Common/XferSave.h"
#include "GameLogic/BitFlags.h"
#include "GameLogic/Damage.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/HeroModules.h"
#include "GameLogic/Object/Object.h"

namespace
{

// RespawnAnimationTime 2000 ms = 10 frames, InitialSpawnAnimationTime 400 ms = 2 frames,
// DeathAnimationTime 3400 ms = 17 frames (parseDurationUnsignedInt RW 0x73A429: ms * 0.005, RW 0x73A429's
// scale 5/1000); Port298NoDeathAnimTime omits the row (the constructor's zero, RW 0x8B3CC4).
const char kPort298Objects[] =
	"Object Port298Hero\n"
	"  BuildCost = 1000\n"
	"  BuildTime = 20.0\n"
	"  KindOf = SELECTABLE INFANTRY HERO\n"
	"  Body = RespawnBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"  Behavior = RespawnUpdate ModuleTag_Respawn\n"
	"    DeathAnim = DYING\n"
	"    DeathFX = None\n"
	"    DeathAnimationTime = 3400\n"
	"    RespawnAnim = LEVELED\n"
	"    RespawnFX = None\n"
	"    RespawnAnimationTime = 2000\n"
	"    InitialSpawnAnim = LEVELED\n"
	"    InitialSpawnFX = None\n"
	"    InitialSpawnAnimationTime = 400\n"
	"    ButtonImage = HIPort298Dead\n"
	"    RespawnRules = AutoSpawn:No Cost:550 Time:40000 Health:75%\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"End\n"
	"Object Port298NoDeathAnimTimeHero\n"
	"  BuildCost = 1000\n"
	"  BuildTime = 20.0\n"
	"  KindOf = SELECTABLE INFANTRY HERO\n"
	"  Body = RespawnBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"  Behavior = RespawnUpdate ModuleTag_Respawn\n"
	"    DeathAnim = DYING\n"
	"    DeathFX = None\n"
	"    RespawnAnim = LEVELED\n"
	"    RespawnFX = None\n"
	"    RespawnAnimationTime = 2000\n"
	"    RespawnRules = AutoSpawn:No Cost:550 Time:40000 Health:75%\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"End\n"
	"Object Port298Fortress\n"
	"  KindOf = STRUCTURE SELECTABLE CASTLE_KEEP\n"
	"  Behavior = QueueProductionExitUpdate ModuleTag_Exit\n"
	"    UnitCreatePoint = X:0.0 Y:-20.0 Z:0.0\n"
	"    NaturalRallyPoint = X:30.0 Y:-50.0 Z:0.0\n"
	"  End\n"
	"End\n";

int port298McBit(const char *name)
{
	for (int i = 0; TheModelConditionNames[i]; ++i)
	{
		if (std::string(TheModelConditionNames[i]) == name)
		{
			return i;
		}
	}
	return -1;
}

void port298Kill(Object *victim)
{
	DamageInfo d;
	d.m_input.m_sourceID = 0;
	d.m_input.m_amount = 100000.0f;
	d.m_input.m_damageType = DAMAGE_UNRESISTABLE;
	victim->attemptDamage(d);
}

} // namespace

TEST_CASE("PORT-298 RespawnUpdate DeathAnimationTime parsed (+0xFC) but never read (S-7323): the revive record "
	"appears in the death frame, the module never wakes by itself, the spawn animation ends exactly at "
	"RespawnAnimationTime (RW 0xC6CEA0 / 0x8B3744 / 0x8B38A2 / 0x8B3A35)")
{
	prodtest::ProdWorld f;
	f.logic->settings().bodyThresholdsLoaded = true;
	f.logic->settings().unitDamagedThreshold = 0.7f;
	f.logic->settings().unitReallyDamagedThreshold = 0.35f;
	f.load(kPort298Objects);

	Player *alice = f.alice();
	REQUIRE(alice != nullptr);
	Object *fortress = f.make("Port298Fortress", f.teamOf("Alice"));
	REQUIRE(fortress != nullptr);

	// The row is parsed into the module data (RW 0xC6CEA0 -> +0xFC): 3400 ms = 17 frames, and the omitted row is 0
	Object *hero = f.make("Port298Hero", f.teamOf("Alice"));
	REQUIRE(hero != nullptr);
	RespawnUpdate *ru = dynamic_cast<RespawnUpdate *>(hero->findModule("RespawnUpdate"));
	REQUIRE(ru != nullptr);
	CHECK(ru->data()->m_deathAnimationTime == 17u);
	CHECK(ru->data()->m_respawnAnimationTime == 10u);
	CHECK(ru->data()->m_initialSpawnAnimationTime == 2u);
	CHECK(ru->state() == RespawnUpdate::STATE_ALIVE);

	// onDeath (RW 0x8B3744): the revive record joins the hero list in the same call — no DeathAnimationTime wait
	port298Kill(hero);
	CHECK(ru->state() == RespawnUpdate::STATE_ALIVE); // onDeath writes no +0x2C
	CHECK(hero->isEffectivelyDead());                 // RW 0x8B382D -> 0x68D950(1)
	CHECK((hero->getDisabledMask() & (1u << 4)) == (1u << 4)); // DISABLED_PARALYZED, RW 0x8B3824 -> 0x692432(4)
	CHECK(hero->testStatus(3));                        // UNSELECTABLE, RW 0x8B3836 -> 0x62684D(3, 1)
	REQUIRE(alice->heroes().size() == 1u);
	const HeroRecord *dead = alice->heroes().at(0);
	REQUIRE(dead != nullptr);
	CHECK(dead->dead);
	CHECK(dead->cost == 550u);    // RW 0x8B3637: the rule of the rank
	CHECK(dead->seconds == 40);   // RW 0x8B36B2: Time 40000 / 1000
	CHECK(dead->deadImage == "HIPort298Dead");

	// 40 frames later (DeathAnimationTime's 17 long past): nothing moved — the module sleeps since the ctor
	// (RW 0x8B32AE) and onDeath never re-arms it; the record waits unchanged for a revive purchase
	f.frames(40);
	CHECK(ru->state() == RespawnUpdate::STATE_ALIVE);
	CHECK(hero->isEffectivelyDead());
	REQUIRE(alice->heroes().size() == 1u);
	CHECK(alice->heroes().at(0)->dead);
	CHECK(alice->heroes().at(0)->cost == 550u);
	CHECK(alice->heroes().at(0)->seconds == 40);

	// The revived hero (a new object, the record not a revive: initialSpawn false) enters state 4 and wakes
	// after RespawnAnimationTime (+0x100) — 10 frames, not DeathAnimationTime (RW 0x8B38A2 -> RW 0x8B3A26)
	Object *hero2 = f.make("Port298Hero", f.teamOf("Alice"));
	REQUIRE(hero2 != nullptr);
	RespawnUpdate *ru2 = dynamic_cast<RespawnUpdate *>(hero2->findModule("RespawnUpdate"));
	REQUIRE(ru2 != nullptr);
	ru2->onRevived(fortress);
	CHECK(ru2->state() == RespawnUpdate::STATE_SPAWN_ANIMATION);           // RW 0x8B396D
	CHECK(hero2->testModelCondition(port298McBit("LEVELED")));             // RespawnAnim, RW 0x8B392A -> 0x5E3BA5
	CHECK(hero2->getBodyModule()->getHealth() == 375.0f);                  // Health 75% of 500, body vslot 0x58
	CHECK(ru2->getRespawnProgress() == 0.0f);                              // state 4 -> [0xC1B594]
	f.frames(9);                                                           // frame F+9 < F+10: still animating
	CHECK(ru2->state() == RespawnUpdate::STATE_SPAWN_ANIMATION);
	f.frames(1);                                                           // frame F+10: update (RW 0x8B3A35) ends it
	CHECK(ru2->state() == RespawnUpdate::STATE_ALIVE);
	CHECK_FALSE(hero2->testModelCondition(port298McBit("LEVELED")));

	// An initial spawn (+0x41) takes InitialSpawnAnimationTime (+0x104): 2 frames (RW 0x8B38EA .. 0x8B390D)
	Object *hero3 = f.make("Port298Hero", f.teamOf("Alice"));
	REQUIRE(hero3 != nullptr);
	RespawnUpdate *ru3 = dynamic_cast<RespawnUpdate *>(hero3->findModule("RespawnUpdate"));
	REQUIRE(ru3 != nullptr);
	ru3->setInitialSpawn(true); // RW 0x78142F: a purchase record
	ru3->onRevived(fortress);
	CHECK(ru3->state() == RespawnUpdate::STATE_SPAWN_ANIMATION);
	f.frames(1);
	CHECK(ru3->state() == RespawnUpdate::STATE_SPAWN_ANIMATION);
	f.frames(1);
	CHECK(ru3->state() == RespawnUpdate::STATE_ALIVE);

	// The omitted row (the constructor's zero, RW 0x8B3CC4) plays the identical timeline
	Object *hero4 = f.make("Port298NoDeathAnimTimeHero", f.teamOf("Alice"));
	REQUIRE(hero4 != nullptr);
	RespawnUpdate *ru4 = dynamic_cast<RespawnUpdate *>(hero4->findModule("RespawnUpdate"));
	REQUIRE(ru4 != nullptr);
	CHECK(ru4->data()->m_deathAnimationTime == 0u);
	port298Kill(hero4);
	REQUIRE(alice->heroes().size() == 2u);
	CHECK(alice->heroes().at(1)->dead);
	Object *hero5 = f.make("Port298NoDeathAnimTimeHero", f.teamOf("Alice"));
	REQUIRE(hero5 != nullptr);
	RespawnUpdate *ru5 = dynamic_cast<RespawnUpdate *>(hero5->findModule("RespawnUpdate"));
	REQUIRE(ru5 != nullptr);
	ru5->onRevived(fortress);
	CHECK(ru5->state() == RespawnUpdate::STATE_SPAWN_ANIMATION);
	f.frames(9);
	CHECK(ru5->state() == RespawnUpdate::STATE_SPAWN_ANIMATION);
	f.frames(1);
	CHECK(ru5->state() == RespawnUpdate::STATE_ALIVE);
}

TEST_CASE("PORT-298 RespawnUpdate xfer (RW 0x8B319D): the eight state fields round-trip; getRespawnProgress "
	"(RW 0x8B310D) holds the retail state switch")
{
	prodtest::ProdWorld f;
	f.logic->settings().bodyThresholdsLoaded = true;
	f.logic->settings().unitDamagedThreshold = 0.7f;
	f.logic->settings().unitReallyDamagedThreshold = 0.35f;
	f.load(kPort298Objects);

	Player *alice = f.alice();
	REQUIRE(alice != nullptr);
	Object *hero = f.make("Port298Hero", f.teamOf("Alice"));
	REQUIRE(hero != nullptr);
	RespawnUpdate *ru = dynamic_cast<RespawnUpdate *>(hero->findModule("RespawnUpdate"));
	REQUIRE(ru != nullptr);

	// getRespawnProgress before a death: state 0, *out = +0x3C (the ctor's -1, RW 0x8B327B), returns 0.0f
	int outCost = 0;
	CHECK(ru->getRespawnProgress(&outCost) == 0.0f);
	CHECK(outCost == -1);

	// onDeath (RW 0x8B3744) fills +0x38 time, +0x3C cost, +0x20 health, +0x40 !AutoSpawn
	port298Kill(hero);
	CHECK(ru->getRespawnProgress(&outCost) == 0.0f); // state stays 0
	CHECK(outCost == 550);
	CHECK(ru->healthFraction() == 0.75f);

	// xfer (RW 0x8B319D): the dead-hero state round-trips through a save/load pair
	{
		XferSave saver;
		ru->xfer(saver);
		XferLoad loader(saver.data());
		ru->xfer(loader);
		CHECK(ru->state() == RespawnUpdate::STATE_ALIVE);
		CHECK(ru->healthFraction() == 0.75f);
		CHECK(ru->getRespawnProgress(&outCost) == 0.0f);
		CHECK(outCost == 550);
		CHECK(ru->autoRespawnDuration() == 40000);
	}

	// the RW 0x8B310D state switch: 1 -> 1.0f; 2 -> 0.0f; 3 with +0x38 > 0 -> (frame - +0x34) / +0x38; 4 -> 0.0f
	ru->setState(RespawnUpdate::STATE_REFUNDED);
	CHECK(ru->getRespawnProgress() == 1.0f); // RW 0x8B3125 .. 0x8B3128 -> [0xBD1908]
	ru->setState(RespawnUpdate::STATE_PERMANENTLY_DEAD);
	CHECK(ru->getRespawnProgress() == 0.0f); // RW 0x8B3128 .. 0x8B312B -> [0xC1B594]
	const UnsignedInt start = f.logic->getFrame();
	ru->setState(RespawnUpdate::STATE_AUTO_RESPAWN);
	ru->setAutoRespawnTimer((std::int32_t)start, 50);
	ru->setRespawnAt(hero->getID());
	CHECK(ru->getRespawnProgress() == 0.0f); // frame - start == 0
	f.frames(25);
	CHECK(ru->getRespawnProgress() == 0.5f); // (F+25 - F) / 50, RW 0x8B3139 .. 0x8B3161

	// xfer round-trip of the auto-respawn state: all eight fields (state, respawnAt, +0x34, +0x38, +0x40, +0x3C, +0x20, +0x41)
	{
		XferSave saver;
		ru->xfer(saver);
		XferLoad loader(saver.data());
		ru->xfer(loader);
		CHECK(ru->state() == RespawnUpdate::STATE_AUTO_RESPAWN);
		CHECK(ru->respawnAt() == hero->getID());
		CHECK(ru->autoRespawnStartFrame() == (std::int32_t)start);
		CHECK(ru->autoRespawnDuration() == 50);
		CHECK(ru->healthFraction() == 0.75f);
		CHECK(ru->getRespawnProgress(&outCost) == 0.5f); // (F+25 - F) / 50 exactly (fild/fisub/fidiv)
		CHECK(outCost == 550);
		CHECK(ru->isInitialSpawn() == false);
	}

	// +0x38 <= 0 in state 3 returns 0.0f (RW 0x8B313C .. 0x8B3141)
	ru->setAutoRespawnTimer((std::int32_t)start, -1);
	CHECK(ru->getRespawnProgress() == 0.0f);
}
