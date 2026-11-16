// OpenBFME unit tests for PORT-205 (fork issue 472): the two inferences stop S-1582 carried for the weapon-choice lane, replaced by the bodies RotWK 2.01
// runs.
//
// TARGET FACTS (RotWK 2.01 game.dat, MD5 daed3668006cd90f01c34e5a7da1901f, caveat S-001), every address read linearly with tools/mod/rw.py:
//   * HordeAttackNugget's isApplicable, RW 0x9119CB: the base test RW 0x90D77C(weapon, victim) first (RW 0x9119D5 .. 0x9119DC); then the weapon's owner
//     (RW 0x449681 on the source id at RW 0x9119E5), its contain (Object + 0x258, RW 0x9119F3) and that contain's horde interface (slot 0x7C, RW 0x68C866:
//     `mov ecx, [ecx + 0x258]` .. `jmp [eax + 0x7c]`). RW 0x911A09 `mov eax, [esi + 4]; test byte ptr [eax + 0x115], 0x40` asks the owner template's
//     KindOf word 3 bit 6; the name table of BitFlagNames.cpp puts bit 0x6E (110) on COMBO_HORDE: set, the interface's slot 0x50 (RW 0x86FEEC) picks, clear,
//     the interface's slot 0x114 with argument 0xA4 (RW 0x8705A6). Both picks walk the contain list (slot 0x118) and then the members on their way into a
//     garrison (interface + 0x54) and never ask whether a member is dead.
//   * RW 0x8705A6: the first member whose template is not KindOf bit 0xA4 (BANNER) (RW 0x8705CB .. 0x8705D4: the entry's object, `mov ecx, [eax + 4]` its
//     template, RW 0x444FA2 the test), else the list's first member (RW 0x8705E3 .. 0x8705EC); with an empty list the same pick over the members on their
//     way (RW 0x8705F3 .. 0x87064C).
//   * RW 0x86FEEC: over the same two lists, `mov ecx, [edi]; call 0x68B58C` (the member's current weapon; RW 0x86FF2B / 0x86FF32 pass over a member
//     without one) and `call 0x6CA8BD` (the member's attack range at dz 0, RW 0x86FF3D). The best starts at -1.0 (RW 0x86FEFA zeroes the slot, RW 0x86FF4B
//     `fcompi` / 0x86FF4F `jbe`), so a later member replaces the best only with a strictly greater range.
//   * RW 0x911A27 .. 0x911A4A: a member found, its current weapon (RW 0x68B58C again, RW 0x911A31) and RW 0x6CDBF3(member weapon, victim); no member at
//     all: true (RW 0x911A51 `mov al, 1`).
//   * contain slot 0x1C, the gate DamageContainedNugget passes at RW 0x9110E4 (`test al, al; jne` skips the nugget) and RW 0x9111F3 (slot 5, the kill), is
//     the GarrisonContain family's ImmuneToClearBuildingAttacks: RW 0x87B308 `mov eax, [ecx - 0x1C]; mov al, [eax + 0xA1]; ret` (module data byte + 0xA1,
//     the row RW 0xC09630 "ImmuneToClearBuildingAttacks"). RW 0x87B308 sits at + 0x1C of the garrison tables RW 0xC5C698 (GarrisonContain), RW 0xC5C9F0
//     (HordeGarrisonContain), RW 0xC5F0D0 (SlaughterHordeContain) and RW 0xC5F2E8 (CitadelSlaughterHordeContain) only; every other contain table of 2.01
//     holds the true body RW 0x8BD372 there (OpenContain RW 0xC59AF0, HordeContain RW 0xC5B480, TransportContain RW 0xC5A690, SiegeEngineContain
//     RW 0xC5D668, TunnelContain RW 0xC5DCE0, ...).
//
// Synthetic INI only: no RotWK or Ennorath file is mounted.

#include "doctest.h"
#include "CombatTestUtil.h"

#include "GameLogic/Combat/ObjectWeapons.h"
#include "GameLogic/Combat/WeaponDelivery.h"
#include "GameLogic/Module/BehaviorModule.h"
#include "GameLogic/Object/Contain/HordeContainRuntime.h"
#include "GameLogic/Weapon.h"

#include <string>
#include <vector>

using namespace combattest;

namespace
{
// A hit of 5 for anything (a DamageNugget's slot 1, RW 0x90E855: the base test, the owner, LostLeadershipUselessAgainst and the victim's estimate) and the
// same reach with nothing that can damage (a FireLogicNugget alone answers false: its slot 1 is RW 0x8F8014). Reach does NOT enter isApplicable; only the
// member's weapon can decide the pick, so the fixtures differ by "can damage", not by range.
const char kPort205Weapons[] =
	"Weapon P205ShortHit\n"
	"  AttackRange = 100\n"
	"  DamageNugget\n"
	"    Damage = 5\n"
	"    DamageType = SLASH\n"
	"    DeathType = NORMAL\n"
	"  End\n"
	"End\n"
	"Weapon P205LongHit\n"
	"  AttackRange = 400\n"
	"  DamageNugget\n"
	"    Damage = 5\n"
	"    DamageType = SLASH\n"
	"    DeathType = NORMAL\n"
	"  End\n"
	"End\n"
	"Weapon P205ShortWaste\n"
	"  AttackRange = 100\n"
	"  FireLogicNugget\n"
	"    LogicType = INCREASE_BURN_RATE\n"
	"  End\n"
	"End\n"
	"Weapon P205LongWaste\n"
	"  AttackRange = 400\n"
	"  FireLogicNugget\n"
	"    LogicType = INCREASE_BURN_RATE\n"
	"  End\n"
	"End\n"
	// the horde's own weapon: a HordeAttackNugget and nothing else
	"Weapon P205HordeWeapon\n"
	"  AttackRange = 150\n"
	"  DelayBetweenShots = 1000\n"
	"  LeechRangeWeapon = Yes\n"
	"  HordeAttackNugget\n"
	"  End\n"
	"End\n"
	// a trebuchet-like launcher whose warhead carries the DamageContainedNugget (the fire drake's shape)
	"Weapon P205ContainedWarhead\n"
	"  DamageContainedNugget\n"
	"    KillCount = 5\n"
	"    KillKindof = INFANTRY\n"
	"    DeathType = BURNED\n"
	"  End\n"
	"End\n"
	"Weapon P205ContainedLauncher\n"
	"  AttackRange = 300\n"
	"  ProjectileNugget\n"
	"    ProjectileTemplateName = Arrow\n"
	"    WarheadTemplateName = P205ContainedWarhead\n"
	"  End\n"
	"End\n";

// P205Archer carries a weapon that can damage (P205ShortHit), P205WasteArcher one that cannot (P205ShortWaste); both reach 100, so a horde's answer follows
// the member the pick names and not the range. P205Banner (KindOf BANNER) cannot damage either, P205LongBanner can (P205LongHit, reach 400). P205Rider has
// no weapon at all.
const char kPort205Objects[] =
	"Object P205Archer\n"
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY P205ShortHit\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"End\n"
	"Object P205WasteArcher\n"
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY P205ShortWaste\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"End\n"
	"Object P205Banner\n"
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK BANNER\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY P205ShortWaste\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"End\n"
	"Object P205LongBanner\n"
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK BANNER\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY P205LongHit\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"End\n"
	// four hordes, each with a fixed member list (InitialPayload keeps its order, so no member has to be moved at run time):
	//   P205HordeBannerFirst  [P205Banner (cannot damage), P205Archer (can)]
	//   P205HordeHitFirst     [P205Archer (can), P205WasteArcher (cannot)]
	//   P205WasteHorde        [P205WasteArcher] alone
	//   P205ComboBannerFirst  COMBO_HORDE [P205WasteArcher (reach 100), P205LongBanner (reach 400, can)]
	"Object P205HordeBannerFirst\n"
	"  KindOf = HORDE LARGE_RECTANGLE_PATHFIND SELECTABLE CAN_ATTACK\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 30\n"
	"  GeometryMinorRadius = 45\n"
	"  GeometryHeight = 20\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY P205HordeWeapon\n"
	"  End\n"
	"  Body = ImmortalBody ModuleTag_Body\n"
	"    MaxHealth = 1\n"
	"  End\n"
	"  Behavior = HordeAIUpdate ModuleTag_AI\n"
	"  End\n"
	"  Behavior = HordeContain ModuleTag_Contain\n"
	"    RankInfo = RankNumber:1 UnitType:P205Banner Position:X:50 Y:0\n"
	"    RankInfo = RankNumber:1 UnitType:P205Archer Position:X:50 Y:20\n"
	"    InitialPayload = P205Banner 1\n"
	"    InitialPayload = P205Archer 1\n"
	"  End\n"
	"End\n"
	"Object P205HordeHitFirst\n"
	"  KindOf = HORDE LARGE_RECTANGLE_PATHFIND SELECTABLE CAN_ATTACK\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 30\n"
	"  GeometryMinorRadius = 45\n"
	"  GeometryHeight = 20\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY P205HordeWeapon\n"
	"  End\n"
	"  Body = ImmortalBody ModuleTag_Body\n"
	"    MaxHealth = 1\n"
	"  End\n"
	"  Behavior = HordeAIUpdate ModuleTag_AI\n"
	"  End\n"
	"  Behavior = HordeContain ModuleTag_Contain\n"
	"    RankInfo = RankNumber:1 UnitType:P205Archer Position:X:50 Y:0\n"
	"    RankInfo = RankNumber:1 UnitType:P205WasteArcher Position:X:50 Y:20\n"
	"    InitialPayload = P205Archer 1\n"
	"    InitialPayload = P205WasteArcher 1\n"
	"  End\n"
	"End\n"
	"Object P205WasteHorde\n"
	"  KindOf = HORDE LARGE_RECTANGLE_PATHFIND SELECTABLE CAN_ATTACK\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 30\n"
	"  GeometryMinorRadius = 45\n"
	"  GeometryHeight = 20\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY P205HordeWeapon\n"
	"  End\n"
	"  Body = ImmortalBody ModuleTag_Body\n"
	"    MaxHealth = 1\n"
	"  End\n"
	"  Behavior = HordeAIUpdate ModuleTag_AI\n"
	"  End\n"
	"  Behavior = HordeContain ModuleTag_Contain\n"
	"    RankInfo = RankNumber:1 UnitType:P205WasteArcher Position:X:50 Y:0\n"
	"    InitialPayload = P205WasteArcher 1\n"
	"  End\n"
	"End\n"
	"Object P205ComboBannerFirst\n"
	"  KindOf = HORDE COMBO_HORDE LARGE_RECTANGLE_PATHFIND SELECTABLE CAN_ATTACK\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 30\n"
	"  GeometryMinorRadius = 45\n"
	"  GeometryHeight = 20\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY P205HordeWeapon\n"
	"  End\n"
	"  Body = ImmortalBody ModuleTag_Body\n"
	"    MaxHealth = 1\n"
	"  End\n"
	"  Behavior = HordeAIUpdate ModuleTag_AI\n"
	"  End\n"
	"  Behavior = HordeContain ModuleTag_Contain\n"
	"    RankInfo = RankNumber:1 UnitType:P205WasteArcher Position:X:50 Y:0\n"
	"    RankInfo = RankNumber:1 UnitType:P205LongBanner Position:X:50 Y:20\n"
	"    InitialPayload = P205WasteArcher 1\n"
	"    InitialPayload = P205LongBanner 1\n"
	"  End\n"
	"End\n"
	"Object P205Victim\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"End\n"
	"Object P205Tower\n"
	"  KindOf = STRUCTURE SELECTABLE IMMOBILE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 20\n"
	"  GeometryMinorRadius = 20\n"
	"  GeometryHeight = 40\n"
	"  Body = ImmortalBody ModuleTag_Body\n"
	"    MaxHealth = 1\n"
	"  End\n"
	"  Behavior = GarrisonContain ModuleTag_Contain\n"
	"    ContainMax = 10\n"
	"  End\n"
	"End\n"
	"Object P205ImmuneTower\n"
	"  KindOf = STRUCTURE SELECTABLE IMMOBILE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 20\n"
	"  GeometryMinorRadius = 20\n"
	"  GeometryHeight = 40\n"
	"  Body = ImmortalBody ModuleTag_Body\n"
	"    MaxHealth = 1\n"
	"  End\n"
	"  Behavior = GarrisonContain ModuleTag_Contain\n"
	"    ContainMax = 10\n"
	"    ImmuneToClearBuildingAttacks = Yes\n"
	"  End\n"
	"End\n"
	"Object P205Rider\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 6\n"
	"  GeometryMinorRadius = 6\n"
	"  GeometryHeight = 15\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"End\n";

// a world with the synthetic weapons and objects above (the fixture's own armors, projectile and warhead stay as they are)
struct Port205World : CombatWorld
{
	explicit Port205World()
		: CombatWorld((std::string(kPort205Weapons) + kPort205Objects).c_str())
	{
	}
};

// the members of `horde` in the contain list's order (the order RotWK walks at RW 0x8705BC / 0x86FF0F)
std::vector<Object *> membersOf(Object *horde)
{
	std::vector<Object *> out;
	ContainModuleInterface *c = horde ? horde->getContain() : nullptr;
	const ContainModuleInterface::ContainedItemsList *items = c ? c->getContainedItemsList() : nullptr;
	if (items)
	{
		for (Object *m : *items)
		{
			out.push_back(m);
		}
	}
	return out;
}

HordeContainInterface *hordeInterface(Object *horde)
{
	ContainModuleInterface *c = horde ? horde->getContain() : nullptr;
	return c ? c->getHordeContainInterface() : nullptr;
}

// RW 0x6CB779 on the horde's own weapon: does its HordeAttackNugget consider `victim` a target of this horde (slot 1, RW 0x9119CB)?
bool hordeWeaponApplies(CombatWorld &w, Object *horde, Object *victim)
{
	ObjectWeapons *weapons = horde->getWeapons();
	REQUIRE(weapons);
	const Weapon *rangefinder = weapons->weaponInSlot(0);
	REQUIRE(rangefinder);
	REQUIRE(rangefinder->getTemplate());
	REQUIRE(rangefinder->getTemplate()->getName() == "P205HordeWeapon");
	return WeaponTemplateAnyNuggetApplicable(*w.logic, *rangefinder->getTemplate(), horde->getID(), victim);
}

void layAt(Object *o, float x, float y)
{
	Coord3D at{ x, y, 0.0f };
	o->setPosition(&at);
}

// empties `horde` and refills it in the given order (the contain list keeps insertion order)
void reorderContain(Object *horde, const std::vector<Object *> &order)
{
	ContainModuleInterface *c = horde->getContain();
	REQUIRE(c);
	for (Object *m : membersOf(horde))
	{
		c->removeFromContain(m);
	}
	for (Object *m : order)
	{
		REQUIRE(c->addToContain(m));
	}
}
} // namespace

// RW 0x8705A6, the pick of a horde that is not a COMBO_HORDE (RW 0x911A09: KindOf word 3 bit 6 clear): the first contained member whose template is not
// KindOf BANNER (0xA4), dead or alive -- the body is never asked (RW 0x8705CB .. 0x8705D4 read the template only). The nugget then asks THAT member's
// current weapon (RW 0x68B58C) whether it can damage the victim (RW 0x6CDBF3, RW 0x911A3A .. 0x911A4A).
TEST_CASE("PORT-205: HordeAttackNugget asks the first member that is not a banner, not the first member at all (RW 0x9119CB, RW 0x8705A6)")
{
	Port205World w;
	// the bit RotWK passes: 0xA4 = 164, the index of BANNER in the name table (BitFlagNames.cpp)
	const int kBanner = ObjectTemplateInfoBuilder::kindOfIndex("BANNER");
	REQUIRE(kBanner == 164);
	Object *victim = w.unit("P205Victim", 'B', 600, 300);

	// a banner whose weapon cannot damage sits FIRST, a fighter that can behind it: the pick passes the banner over and the nugget applies
	Object *bannerFirst = w.unit("P205HordeBannerFirst", 'A', 300, 300);
	HordeContainInterface *hi = hordeInterface(bannerFirst);
	REQUIRE(hi);
	std::vector<Object *> members = membersOf(bannerFirst);
	REQUIRE(members.size() == 2u);
	CHECK(members[0]->getTemplate()->getName() == "P205Banner");
	CHECK(members[1]->getTemplate()->getName() == "P205Archer");
	CHECK(hi->firstMemberNotKindOf((unsigned)kBanner) == members[1]);
	CHECK(hordeWeaponApplies(w, bannerFirst, victim)); // the fighter's P205ShortHit, not the banner's P205ShortWaste

	// a horde of one fighter that cannot damage: the pick is that member and the nugget does not apply
	Object *waste = w.unit("P205WasteHorde", 'A', 700, 300);
	HordeContainInterface *wasteHi = hordeInterface(waste);
	REQUIRE(wasteHi);
	std::vector<Object *> wasteMembers = membersOf(waste);
	REQUIRE(wasteMembers.size() == 1u);
	CHECK(wasteHi->firstMemberNotKindOf((unsigned)kBanner) == wasteMembers[0]);
	CHECK_FALSE(hordeWeaponApplies(w, waste, victim));

	// the pick reads the list, so a fighter first in it decides: a horde [P205Archer, P205WasteArcher] answers true, and the same horde after the archer
	// died answers false -- the dead member has left the contain (HordeContain's own removal), so the second member is the pick
	Object *hitFirst = w.unit("P205HordeHitFirst", 'A', 900, 300);
	HordeContainInterface *hitHi = hordeInterface(hitFirst);
	REQUIRE(hitHi);
	std::vector<Object *> hitMembers = membersOf(hitFirst);
	REQUIRE(hitMembers.size() == 2u);
	CHECK(hitMembers[0]->getTemplate()->getName() == "P205Archer");
	CHECK(hitMembers[1]->getTemplate()->getName() == "P205WasteArcher");
	CHECK(hordeWeaponApplies(w, hitFirst, victim));
	hitMembers[0]->kill(DAMAGE_UNRESISTABLE, DEATH_NORMAL);
	REQUIRE(hitMembers[0]->isEffectivelyDead());
	std::vector<Object *> left = membersOf(hitFirst);
	REQUIRE(left.size() == 1u); // the dead member left the contain list (Object::kill -> the horde's removal)
	CHECK(hitHi->firstMemberNotKindOf((unsigned)kBanner) == left[0]);
	CHECK_FALSE(hordeWeaponApplies(w, hitFirst, victim));
}

// RW 0x86FEEC, the pick of a COMBO_HORDE (RW 0x911A09: KindOf word 3 bit 6 set): the member whose current weapon has the greatest attack range
// (RW 0x6CA8BD at dz 0), the first of two equals keeping the place (RW 0x86FF4B / 0x86FF4F replace only on a strictly greater range), a member without a
// current weapon passed over (RW 0x68B58C then RW 0x86FF32). The nugget then asks that member's weapon (RW 0x911A3A .. 0x911A4A).
TEST_CASE("PORT-205: a COMBO_HORDE asks the longest-reaching member, not the first (RW 0x9119CB, RW 0x86FEEC)")
{
	Port205World w;
	Object *victim = w.unit("P205Victim", 'B', 600, 300);
	// a fighter of 100 of reach first, a banner of 400 that can damage behind it
	Object *combo = w.unit("P205ComboBannerFirst", 'A', 300, 300);
	HordeContainInterface *hi = hordeInterface(combo);
	REQUIRE(hi);
	std::vector<Object *> members = membersOf(combo);
	REQUIRE(members.size() == 2u);
	CHECK(members[0]->getTemplate()->getName() == "P205WasteArcher");
	CHECK(members[1]->getTemplate()->getName() == "P205LongBanner");
	// the longest reach is the last member's weapon (RW 0x6CA8BD on both: 100 against 400) even though it is a BANNER (only the non-combo branch skips those)
	CHECK(hi->memberWithLongestAttackRange() == members[1]);
	CHECK(hordeWeaponApplies(w, combo, victim));

	// the same two members on a horde that is NOT a COMBO_HORDE: the pick is the first member that is not a banner, whose weapon cannot damage
	Object *bannerFirst = w.unit("P205HordeBannerFirst", 'A', 700, 300);
	HordeContainInterface *bannerHi = hordeInterface(bannerFirst);
	REQUIRE(bannerHi);
	std::vector<Object *> bannerMembers = membersOf(bannerFirst);
	REQUIRE(bannerMembers.size() == 2u);
	CHECK(bannerHi->firstMemberNotKindOf(164u) == bannerMembers[1]);
	// both members of that horde hold a P205Short* weapon: their reaches are equal, so the first scored candidate keeps the place
	CHECK(bannerHi->memberWithLongestAttackRange() == bannerMembers[0]);
}

// RW 0x87B308 (contain slot 0x1C of the GarrisonContain family: tables RW 0xC5C698 / RW 0xC5C9F0 / RW 0xC5F0D0 / RW 0xC5F2E8) answers the module data's
// ImmuneToClearBuildingAttacks (byte + 0xA1, its field row is RW 0xC09630). DamageContainedNugget's slot 1 skips a contain that answers true
// (RW 0x9110E4 .. 0x9110ED) and so does its slot 5 (RW 0x9111F3 .. 0x9111F8).
TEST_CASE("PORT-205: DamageContainedNugget spares a garrison immune to the clearing of the building it attacks (RW 0x911086, RW 0x87B308)")
{
	Port205World w;
	Object *plainTower = w.unit("P205Tower", 'B', 400, 300);
	Object *immuneTower = w.unit("P205ImmuneTower", 'B', 700, 300);
	Object *riderA = w.unit("P205Rider", 'B', 405, 300);
	Object *riderB = w.unit("P205Rider", 'B', 705, 300);
	REQUIRE(plainTower->getContain());
	REQUIRE(immuneTower->getContain());
	REQUIRE(plainTower->getContain()->addToContain(riderA));
	REQUIRE(immuneTower->getContain()->addToContain(riderB));

	// the parsed byte is behind the slot (RW 0x87B308 reads module data + 0xA1)
	CHECK_FALSE(plainTower->getContain()->isImmuneToClearBuildingAttacks());
	CHECK(immuneTower->getContain()->isImmuneToClearBuildingAttacks());

	// the trebuchet's weapon is the launcher; its ProjectileNugget asks the warhead, whose DamageContainedNugget is the slot under test
	const WeaponTemplate *launcher = TheWeaponStore->findWeaponTemplate("P205ContainedLauncher");
	REQUIRE(launcher);
	const WeaponTemplate *warhead = TheWeaponStore->findWeaponTemplate("P205ContainedWarhead");
	REQUIRE(warhead);
	Object *trebuchet = w.unit("P205Archer", 'A', 300, 300);
	REQUIRE(trebuchet);
	CHECK(WeaponTemplateAnyNuggetApplicable(*w.logic, *launcher, trebuchet->getID(), plainTower));
	CHECK_FALSE(WeaponTemplateAnyNuggetApplicable(*w.logic, *launcher, trebuchet->getID(), immuneTower));
	// and an empty immune garrison is refused by the count test before the slot is asked (RW 0x9110CF .. 0x9110D7)
	immuneTower->getContain()->removeFromContain(riderB);
	CHECK_FALSE(WeaponTemplateAnyNuggetApplicable(*w.logic, *launcher, trebuchet->getID(), immuneTower));
}

// the other half of the same slot: every other contain class of 2.01 answers TRUE at + 0x1C (RW 0x8BD372: OpenContain RW 0xC59AF0, HordeContain RW 0xC5B480,
// TransportContain RW 0xC5A690, SiegeEngineContain RW 0xC5D668, TunnelContain RW 0xC5DCE0, ...), so the port's default keeps the nugget for them.
TEST_CASE("PORT-205: contain slot 0x1C answers true for a container that is not a garrison (RW 0x8BD372)")
{
	Port205World w;
	Object *horde = w.unit("P205WasteHorde", 'A', 300, 300);
	REQUIRE(horde->getContain());
	CHECK(horde->getContain()->isImmuneToClearBuildingAttacks());
	Object *plainTower = w.unit("P205Tower", 'B', 400, 300);
	REQUIRE(plainTower->getContain());
	CHECK_FALSE(plainTower->getContain()->isImmuneToClearBuildingAttacks());
	Object *immuneTower = w.unit("P205ImmuneTower", 'B', 700, 300);
	REQUIRE(immuneTower->getContain());
	CHECK(immuneTower->getContain()->isImmuneToClearBuildingAttacks());
}
