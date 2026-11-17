// PORT-209 (fork issue 491, stops S-7317 / S-6028 / S-587): the banner carrier RotWK's HordeContain makes in onBuildComplete and the banner tie-break of two
// combining hordes. Synthetic data only (the fixtures below, the shape of test_port_117.cpp's and test_port_35.cpp's): the expectations are RotWK's
// instructions, named at each check.
//
// RW 0x8718ED (the create interface's slot 1, onBuildComplete): the payload (RW 0x869ED4), then, when the horde has no producer (Object + 0x78 == 0,
// RW 0x8718F5 .. 0x8718FC) and its BannerCarrierMinLevel is 0 (module data byte + 0x27C, RW 0x8718FE .. 0x871908), the tail-jump RW 0x87190E to the
// carrier spawn RW 0x870204: the carrier exists when the creator's call returns; no level test (that is bannerCheck's, RW 0x871A14). buildObjectNow
// (RW 0x7979EF) completes the horde with its producer cleared (RW 0x7979E8), so a summoned horde of that level has its carrier in the call.
// RW 0x876481's combine branch: both hordes' members leave (slot 0x44, RW 0x876653 / RW 0x87670D), the carriers are set aside (RW 0x876691 / RW 0x87674F)
// and RW 0x8768A0 .. 0x8768FC decides: two carriers -> the levels of their ExperienceTrackers (RW 0x8768AE .. 0x8768BD), this horde's carrier stays unless
// the target's level is higher (`jl` RW 0x8768C6), the loser is destroyed (RW 0x62BBAB); one carrier -> it stays. The survivor joins the new horde through
// slot 0x74 (RW 0x876962 -> RW 0x876406: the id H + 0x26C first, RW 0x8763A9, then the join RW 0x8763C9, then the carrier's level becomes the horde's,
// RW 0x87645E .. 0x876476) under the latch H + 0x2B4 (RW 0x876948: no morph FX).
#include "doctest.h"
#include "CombatTestUtil.h"
#include "Horde2TestUtil.h"
#include "Common/Thing/ThingTemplate.h"
#include "GameLogic/Construction.h"
#include "GameLogic/Object/Contain/HordeContainRuntime.h"
#include "GameLogic/Object/ExperienceTracker.h"
#include "GameLogic/Object/Object.h"

#include <string>
#include <vector>

namespace {

// an object with a horde contain (test_port_117.cpp's shape): `containLines` are the contain's own lines
std::string hordeIni(const char *name, const char *containLines)
{
	return std::string("Object ") + name + "\n"
		"  KindOf = HORDE MELEE_HORDE LARGE_RECTANGLE_PATHFIND SELECTABLE CAN_ATTACK\n"
		"  VisionRange = 200\n"
		"  WeaponSet\n    Conditions = None\n    Weapon = PRIMARY SwordWeapon\n  End\n"
		"  Geometry = BOX\n  GeometryMajorRadius = 30\n  GeometryMinorRadius = 45\n  GeometryHeight = 20\n"
		"  Body = ImmortalBody ModuleTag_Body\n    MaxHealth = 1\n  End\n"
		"  Behavior = HordeAIUpdate ModuleTag_AI\n  End\n"
		"  Behavior = HordeContain ModuleTag_Contain\n" + containLines + "  End\n"
		"  LocomotorSet\n    Locomotor = HordeLoco\n    Condition = SET_NORMAL\n    Speed = 50\n  End\n"
		"End\n";
}

// three swordsmen and the banner carrier due from the start (BannerCarrierMinLevel 0: RW 0x871901's byte is 0). Its ComboHorde entry names the archer
// horde (RW 0x8765CC picks the entry by the target's template)
const char kSwordHorde[] =
	"    RankInfo = RankNumber:1 UnitType:Swordsman Position:X:50 Y:0 Position:X:50 Y:20 Position:X:50 Y:-20\n"
	"    Slots = 3\n"
	"    InitialPayload = Swordsman 3\n"
	"    BannerCarriersAllowed = Port209Bannerman\n"
	"    BannerCarrierPosition = UnitType:Swordsman Pos:X:10.0 Y:0.0\n"
	"    BannerCarrierMinLevel = 0\n"
	"    ComboHorde = Target:Port209ArcherHorde Result:Port209MixedHorde InitiateVoice:NoSound\n";

// the same horde with BannerCarrierMinLevel 1: RW 0x871901 .. 0x871908 skips the spawn at build complete, the carrier comes from the level test of
// bannerCheck (RW 0x871A14) later
const char kSwordHordeMinLevel1[] =
	"    RankInfo = RankNumber:1 UnitType:Swordsman Position:X:50 Y:0 Position:X:50 Y:20 Position:X:50 Y:-20\n"
	"    Slots = 3\n"
	"    InitialPayload = Swordsman 3\n"
	"    BannerCarriersAllowed = Port209Bannerman\n"
	"    BannerCarrierPosition = UnitType:Swordsman Pos:X:10.0 Y:0.0\n"
	"    BannerCarrierMinLevel = 1\n";

// the same horde without a banner carrier at all: RW 0x870204 returns on the empty BannerCarriersAllowed (RW 0x870233)
const char kSwordHordeNoBanner[] =
	"    RankInfo = RankNumber:1 UnitType:Swordsman Position:X:50 Y:0 Position:X:50 Y:20 Position:X:50 Y:-20\n"
	"    Slots = 3\n"
	"    InitialPayload = Swordsman 3\n";

// two archers and a banner carrier due from the start
const char kArcherHorde[] =
	"    RankInfo = RankNumber:1 UnitType:Archer Position:X:30 Y:10 Position:X:30 Y:-10\n"
	"    Slots = 2\n"
	"    InitialPayload = Archer 2\n"
	"    BannerCarriersAllowed = Port209Bannerman\n"
	"    BannerCarrierPosition = UnitType:Archer Pos:X:10.0 Y:0.0\n"
	"    BannerCarrierMinLevel = 0\n";

// the combine's Result: room for both hordes' members, the same carrier template allowed (RW 0x86C86D .. 0x86C89E tests the carrier's template against
// the new horde's BannerCarriersAllowed)
const char kMixedHorde[] =
	"    RankInfo = RankNumber:1 UnitType:Swordsman Position:X:50 Y:0 Position:X:50 Y:20 Position:X:50 Y:-20\n"
	"    RankInfo = RankNumber:2 UnitType:Archer Position:X:30 Y:10 Position:X:30 Y:-10\n"
	"    Slots = 5\n"
	"    BannerCarriersAllowed = Port209Bannerman\n"
	"    BannerCarrierPosition = UnitType:Swordsman Pos:X:10.0 Y:0.0\n"
	"    BannerCarrierMinLevel = 0\n";

// the banner carrier (test_port_117.cpp's): MeleeFreeBannerReSpawnTime 4000 ms = 20 logic frames (RW 0x870414 .. 0x870426 makes it the countdown)
const char kBannerman[] =
	"Object Port209Bannerman\n"
	"  KindOf = INFANTRY SELECTABLE HORDE_BANNER_CARRIER\n"
	"  Geometry = CYLINDER\n  GeometryMajorRadius = 8\n  GeometryMinorRadius = 8\n  GeometryHeight = 20\n"
	"  ArmorSet\n    Conditions = None\n    Armor = PlainArmor\n  End\n"
	"  Body = ActiveBody ModuleTag_Body\n    MaxHealth = 100\n  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n  End\n"
	"  Behavior = BannerCarrierUpdate ModuleTag_Banner\n"
	"    IdleSpawnRate = 2000\n"
	"    MeleeFreeUnitSpawnTime = 4000\n"
	"    DiedRespawnTime = 2000\n"
	"    MeleeFreeBannerReSpawnTime = 4000\n"
	"  End\n"
	"  Behavior = DestroyDie ModuleTag_Destroy\n  End\n"
	"  LocomotorSet\n    Locomotor = WalkerLoco\n    Condition = SET_NORMAL\n    Speed = 55\n  End\n"
	"End\n";

std::string port209Objects()
{
	return std::string(kBannerman) +
		hordeIni("Port209SwordHorde", kSwordHorde) +
		hordeIni("Port209SwordHordeMinLevel1", kSwordHordeMinLevel1) +
		hordeIni("Port209SwordHordeNoBanner", kSwordHordeNoBanner) +
		hordeIni("Port209ArcherHorde", kArcherHorde) +
		hordeIni("Port209MixedHorde", kMixedHorde);
}

struct Port209World : combattest::CombatWorld
{
	Port209World()
	{
		const std::string err = w.load(port209Objects(), INI_LOAD_OVERWRITE, "port209.ini");
		REQUIRE_MESSAGE(err.empty(), err);
		combat().setAutoAcquireEnabled(false); // nobody fights here
	}
	const ThingTemplate &tmpl(const char *name)
	{
		const ThingTemplate *t = w.get(name);
		REQUIRE_MESSAGE(t, name);
		return *t;
	}
	// the horde's members of `name`
	std::vector<Object *> membersNamed(Object *horde, const char *name)
	{
		std::vector<Object *> out;
		for (Object *m : membersOf(horde))
		{
			if (m->getTemplate()->getName() == name)
			{
				out.push_back(m);
			}
		}
		return out;
	}
	// the horde's carrier as an object: the contain's id H + 0x26C, which must be one of its members
	Object *carrierOf(Object *horde)
	{
		HordeContain *contain = hordeOf(horde);
		REQUIRE(contain);
		if (contain->bannerCarrier() == 0)
		{
			return nullptr;
		}
		Object *carrier = logic->findObjectByID(contain->bannerCarrier());
		REQUIRE(carrier);
		return carrier;
	}
	// a complete horde of `name` with its carrier of rank `carrierRank` (the fixtures load no ExperienceLevel data, so the ranks are set by hand, as
	// Horde2TestUtil.h's tests do)
	Object *banneredHorde(const char *name, float x, float y, int carrierRank)
	{
		Object *horde = unit(name, 'A', x, y);
		REQUIRE(horde);
		Object *carrier = carrierOf(horde);
		REQUIRE(carrier);
		REQUIRE(carrier->getExperienceTracker());
		ExperienceTrackerTestAccess::setRank(*carrier->getExperienceTracker(), carrierRank);
		return horde;
	}
};

} // namespace

// RW 0x8718F5 .. 0x87190E: no producer and BannerCarrierMinLevel 0 -> the spawn RW 0x870204 runs inside onBuildComplete, so the carrier is there when
// buildObjectNow returns (the first of BannerCarriersAllowed, RW 0x870239; on the horde's team, RW 0x8702EA; the countdown RW 0x870414 .. 0x870426)
TEST_CASE("PORT-209 buildObjectNow: a horde with BannerCarrierMinLevel 0 has its banner carrier when the call returns (RW 0x87190E -> 0x870204)")
{
	Port209World f;
	Object *builder = f.unit("Swordsman", 'A', 200, 200);
	REQUIRE(builder);
	Player *owner = f.playerOf('A');
	REQUIRE(owner);
	Object *horde = Construction::buildObjectNow(*f.logic, builder, f.tmpl("Port209SwordHorde"), Coord3D{ 500.0f, 500.0f, 0.0f }, 0.0f, *owner);
	REQUIRE(horde);
	// the payload first (RW 0x8718F0 -> RW 0x869ED4)
	CHECK(f.membersNamed(horde, "Swordsman").size() == 3u);
	// then the carrier, in the same call: no frame has run
	Object *carrier = f.carrierOf(horde);
	REQUIRE(carrier);
	CHECK(carrier->getTemplate()->getName() == "Port209Bannerman");
	CHECK(carrier->getContainedBy() == horde);
	CHECK(carrier->getTeam() == horde->getTeam()); // RW 0x8702EA: the horde's team
	CHECK(f.membersNamed(horde, "Port209Bannerman").size() == 1u);
	// RW 0x870414 .. 0x870426: the countdown is the carrier's MeleeFreeBannerReSpawnTime (4000 ms = 20 frames)
	CHECK(f.hordeOf(horde)->bannerCountdown() == 20);
	// the rank test of bannerCheck (RW 0x871A14) was not asked: the horde's tracker is still at this fixture's starting rank 0, below no level
	REQUIRE(horde->getExperienceTracker());
	CHECK(horde->getExperienceTracker()->getRank() == 0);
	// the frames after it make no second carrier (RW 0x870217: a horde with a carrier spawns none)
	f.frames(3);
	CHECK(f.membersNamed(horde, "Port209Bannerman").size() == 1u);
	CHECK(f.carrierOf(horde) == carrier);
}

// the two gates of the tail: a producer (RW 0x8718F5 .. 0x8718FC) or a BannerCarrierMinLevel above 0 (RW 0x8718FE .. 0x871908) leave the carrier to
// bannerCheck (RW 0x8719E4) later; an empty BannerCarriersAllowed makes RW 0x870204 return (RW 0x870233)
TEST_CASE("PORT-209 onBuildComplete: a produced horde, a BannerCarrierMinLevel above 0 or no carrier template gets no carrier in the call")
{
	Port209World f;
	Object *producer = f.unit("Swordsman", 'A', 200, 200);
	REQUIRE(producer);
	SUBCASE("a producer: the tail's first test (Object + 0x78 is not 0)")
	{
		Object *horde = f.logic->newObject(&f.tmpl("Port209SwordHorde"), f.teamA(), ObjectStatusMaskType{});
		REQUIRE(horde);
		const Coord3D at{ 500.0f, 500.0f, 0.0f };
		horde->setPosition(&at);
		horde->setProducer(producer);
		Port209World::completeHorde(horde);
		// a produced horde gets neither its payload (RW 0x871BAB .. 0x871BB3: the producer fills it) nor a carrier in the call
		CHECK(f.membersOf(horde).empty());
		CHECK(f.hordeOf(horde)->bannerCarrier() == 0);
		CHECK(f.membersNamed(horde, "Port209Bannerman").empty());
	}
	SUBCASE("BannerCarrierMinLevel 1: the tail's second test (the byte + 0x27C is not 0)")
	{
		Player *owner = f.playerOf('A');
		REQUIRE(owner);
		Object *horde = Construction::buildObjectNow(*f.logic, producer, f.tmpl("Port209SwordHordeMinLevel1"), Coord3D{ 500.0f, 500.0f, 0.0f }, 0.0f, *owner);
		REQUIRE(horde);
		CHECK(f.membersNamed(horde, "Swordsman").size() == 3u);
		CHECK(f.hordeOf(horde)->bannerCarrier() == 0);
		CHECK(f.membersNamed(horde, "Port209Bannerman").empty());
	}
	SUBCASE("no BannerCarriersAllowed: RW 0x870204 returns at once")
	{
		Player *owner = f.playerOf('A');
		REQUIRE(owner);
		Object *horde = Construction::buildObjectNow(*f.logic, producer, f.tmpl("Port209SwordHordeNoBanner"), Coord3D{ 500.0f, 500.0f, 0.0f }, 0.0f, *owner);
		REQUIRE(horde);
		CHECK(f.membersNamed(horde, "Swordsman").size() == 3u);
		CHECK(f.hordeOf(horde)->bannerCarrier() == 0);
		CHECK(f.membersOf(horde).size() == 3u);
	}
}

// RW 0x8768A0 .. 0x8768FC: two carriers -> the carrier of the higher level stays, this horde's on a tie (`jl` RW 0x8768C6 goes to the target's only when
// this horde's level is lower); the other is destroyed (RW 0x62BBAB at RW 0x8768D4). The survivor is the new horde's carrier (RW 0x876962 -> RW 0x876406,
// the id RW 0x8763A9) and a member of it (the join RW 0x8763C9); every other member of both hordes comes along (RW 0x876990 -> RW 0x876A08)
TEST_CASE("PORT-209 combine: of two bannered hordes the carrier of the higher level stays, this horde's on a tie (RW 0x8768A0 .. 0x8768FC)")
{
	Port209World f;
	int ownRank = 1;
	int otherRank = 1;
	bool ownStays = true;
	SUBCASE("a tie: this horde's carrier (RW 0x8768C6 `jl` not taken)")
	{
		ownRank = 1;
		otherRank = 1;
		ownStays = true;
	}
	SUBCASE("this horde's carrier of the higher level stays (RW 0x8768C8)")
	{
		ownRank = 3;
		otherRank = 2;
		ownStays = true;
	}
	SUBCASE("the target's carrier of the higher level stays (RW 0x8768DB)")
	{
		ownRank = 1;
		otherRank = 2;
		ownStays = false;
	}
	Object *sword = f.banneredHorde("Port209SwordHorde", 300, 300, ownRank);
	Object *archer = f.banneredHorde("Port209ArcherHorde", 340, 300, otherRank);
	f.frames(2);
	Object *ownCarrier = f.carrierOf(sword);
	Object *otherCarrier = f.carrierOf(archer);
	REQUIRE(ownCarrier);
	REQUIRE(otherCarrier);
	REQUIRE(ownCarrier != otherCarrier);
	const std::vector<Object *> swordsmen = f.membersNamed(sword, "Swordsman");
	const std::vector<Object *> archers = f.membersNamed(archer, "Archer");
	REQUIRE(swordsmen.size() == 3u);
	REQUIRE(archers.size() == 2u);
	HordeContain *caster = f.hordeOf(sword);
	REQUIRE(caster);
	Object *made = caster->combineWith(archer);
	REQUIRE(made);
	CHECK(made->getTemplate()->getName() == "Port209MixedHorde");
	CHECK(sword->isDestroyed());
	CHECK(archer->isDestroyed());
	Object *survivor = ownStays ? ownCarrier : otherCarrier;
	Object *loser = ownStays ? otherCarrier : ownCarrier;
	CAPTURE(ownRank);
	CAPTURE(otherRank);
	CHECK_FALSE(survivor->isDestroyed());
	CHECK(loser->isDestroyed()); // RW 0x62BBAB at RW 0x8768D4
	// RW 0x8763A9: the new horde's H + 0x26C is the survivor; RW 0x8763C9: it is a member of the new horde
	CHECK(f.carrierOf(made) == survivor);
	CHECK(survivor->getContainedBy() == made);
	const std::vector<Object *> carriers = f.membersNamed(made, "Port209Bannerman");
	REQUIRE(carriers.size() == 1u);
	CHECK(carriers.front() == survivor);
	// RW 0x876990 -> RW 0x876A08: the other members of both hordes are the new horde's
	for (Object *m : swordsmen)
	{
		CHECK_FALSE(m->isDestroyed());
		CHECK(m->getContainedBy() == made);
	}
	for (Object *m : archers)
	{
		CHECK_FALSE(m->isDestroyed());
		CHECK(m->getContainedBy() == made);
	}
	CHECK(f.membersOf(made).size() == 6u);
	// RW 0x87645E .. 0x876476: the survivor gains the new horde's level minus its own (RW 0x79D7E5 loops `levels` times): the new horde starts at this
	// fixture's rank 0, so a carrier of rank 1 or more gains nothing and keeps its rank
	REQUIRE(survivor->getExperienceTracker());
	CHECK(survivor->getExperienceTracker()->getRank() == (ownStays ? ownRank : otherRank));
	// the new horde's frames keep the one carrier (RW 0x870217: no spawn over an existing carrier)
	f.frames(3);
	CHECK(f.membersNamed(made, "Port209Bannerman").size() == 1u);
	CHECK(f.carrierOf(made) == survivor);
	CHECK(f.membersOf(made).size() == 6u);
}

// RW 0x8768E7 / RW 0x8768EF .. 0x8768FC: one carrier -> it stays, through the same hand-over
TEST_CASE("PORT-209 combine: the only carrier of the two hordes becomes the new horde's (RW 0x8768E7 / 0x8768EF)")
{
	Port209World f;
	Object *sword = f.unit("Port209SwordHorde", 'A', 300, 300);
	Object *archer = f.unit("Port209ArcherHorde", 'A', 340, 300);
	REQUIRE(sword);
	REQUIRE(archer);
	f.frames(2);
	Object *ownCarrier = f.carrierOf(sword);
	Object *otherCarrier = f.carrierOf(archer);
	REQUIRE(ownCarrier);
	REQUIRE(otherCarrier);
	bool keepOwn = true;
	SUBCASE("this horde's carrier alone (the target's was killed before)")
	{
		keepOwn = true;
		f.logic->destroyObject(otherCarrier);
	}
	SUBCASE("the target's carrier alone (this horde's was killed before)")
	{
		keepOwn = false;
		f.logic->destroyObject(ownCarrier);
	}
	Object *survivor = keepOwn ? ownCarrier : otherCarrier;
	REQUIRE(f.carrierOf(keepOwn ? sword : archer) == survivor);
	REQUIRE(f.carrierOf(keepOwn ? archer : sword) == nullptr);
	HordeContain *caster = f.hordeOf(sword);
	REQUIRE(caster);
	Object *made = caster->combineWith(archer);
	REQUIRE(made);
	CHECK_FALSE(survivor->isDestroyed());
	CHECK(f.carrierOf(made) == survivor);
	CHECK(survivor->getContainedBy() == made);
	CHECK(f.membersNamed(made, "Port209Bannerman").size() == 1u);
	CHECK(f.membersOf(made).size() == 6u);
}
