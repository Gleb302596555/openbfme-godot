// OpenBFME. MODGAP-1 tests, group 6 (MODULE_GAPS.md): EvacuateDamage (RW 0x8BF12E onDamage, 0x8BF09E tracking, 0x8BEF0A the windowed sum) on synthetic data.
// The expected values are the data's own: the named weapon empties the garrison, other hits of the tracked type are summed over TrackingTimeSpan.
#include "doctest.h"
#include "CombatTestUtil.h"

#include "GameLogic/Damage.h"
#include "GameLogic/Module/EvacuateDamage.h"

using namespace combattest;

namespace
{
std::string tower(const char *name, const char *fields)
{
	return std::string("Object ") + name + "\n"
		"  KindOf = STRUCTURE\n"
		"  Geometry = CYLINDER\n"
		"  GeometryMajorRadius = 20\n"
		"  GeometryMinorRadius = 20\n"
		"  GeometryHeight = 20\n"
		"  ArmorSet\n"
		"    Conditions = None\n"
		"    Armor = NoArmor\n"
		"  End\n"
		"  Body = ActiveBody ModuleTag_Body\n"
		"    MaxHealth = 1000\n"
		"  End\n"
		"  Behavior = AIUpdateInterface ModuleTag_AI\n"
		"  End\n"
		"  Behavior = TransportContain ModuleTag_Contain\n"
		"    Slots = 5\n"
		"  End\n"
		"  Behavior = EvacuateDamage ModuleTag_Evac\n" + fields + "  End\n"
		"End\n";
}

// SwordWeapon is the attacker's (the Swordsman of CombatTestUtil); SlowSword is the weapon the second tower names, so for it the sword is "another weapon"
const std::string kEvacObjects =
	tower("EvacBySword", "    WeaponThatCausesEvacuation = SwordWeapon\n") +
	tower("EvacByTracking",
		"    WeaponThatCausesEvacuation = SlowSword\n"
		"    DamageTypeToTrack = SLASH\n"
		"    DamageToPanicThreshold = 10%\n"
		"    TrackingTimeSpan = 1200\n");

void hit(Object *victim, Object *attacker, int type, float amount)
{
	DamageInfo info;
	info.m_input.m_sourceID = attacker->getID();
	info.m_input.m_damageType = type;
	info.m_input.m_deathType = DEATH_NORMAL;
	info.m_input.m_amount = amount;
	victim->attemptDamage(info);
}

EvacuateDamage *evacOf(Object *o)
{
	return dynamic_cast<EvacuateDamage *>(o->findModule("EvacuateDamage"));
}

size_t riders(Object *tower)
{
	const ContainModuleInterface::ContainedItemsList *items = tower->getContain() ? tower->getContain()->getContainedItemsList() : nullptr;
	return items ? items->size() : 0;
}
} // namespace

TEST_CASE("modgap1 EvacuateDamage: a hit by the named weapon sends the whole garrison out, another weapon's hit does not (RW 0x8BF12E, 0x8BF178)")
{
	SUBCASE("the named weapon")
	{
		CombatWorld w(kEvacObjects.c_str());
		Object *t = w.unit("EvacBySword", 'A', 400, 400);
		Object *a = w.unit("Swordsman", 'A', 420, 400);
		Object *b = w.unit("Swordsman", 'A', 430, 400);
		Object *enemy = w.unit("Swordsman", 'B', 600, 600);
		REQUIRE(t->getContain()->addToContain(a));
		REQUIRE(t->getContain()->addToContain(b));
		CHECK(riders(t) == 2);
		hit(t, enemy, DAMAGE_SLASH, 10.0f);
		w.frames(40);
		CHECK(riders(t) == 0);
		CHECK(a->getContainedBy() == nullptr);
		CHECK(b->getContainedBy() == nullptr);
	}
	SUBCASE("another weapon")
	{
		CombatWorld w(kEvacObjects.c_str());
		Object *t = w.unit("EvacByTracking", 'A', 400, 400);
		Object *a = w.unit("Swordsman", 'A', 420, 400);
		Object *enemy = w.unit("Swordsman", 'B', 600, 600);
		REQUIRE(t->getContain()->addToContain(a));
		hit(t, enemy, DAMAGE_PIERCE, 10.0f); // not the tracked type either
		w.frames(40);
		CHECK(riders(t) == 1);
	}
}

TEST_CASE("modgap1 EvacuateDamage: damage of the tracked type is kept for TrackingTimeSpan and summed against DamageToPanicThreshold of the max health (RW 0x8BF09E, 0x8BEF0A)")
{
	CombatWorld w(kEvacObjects.c_str());
	Object *t = w.unit("EvacByTracking", 'A', 400, 400);
	Object *enemy = w.unit("Swordsman", 'B', 600, 600);
	EvacuateDamage *evac = evacOf(t);
	REQUIRE(evac != nullptr);
	hit(t, enemy, DAMAGE_PIERCE, 90.0f);
	CHECK(evac->tracked().size() == 0); // not DamageTypeToTrack
	hit(t, enemy, DAMAGE_SLASH, 60.0f);
	REQUIRE(evac->tracked().size() == 1);
	CHECK(evac->tracked()[0].amount == doctest::Approx(60.0f)); // the INPUT amount
	w.frames(10);                                                // TrackingTimeSpan 1200 ms = 6 frames
	hit(t, enemy, DAMAGE_SLASH, 60.0f);
	CHECK(evac->tracked().size() == 1); // the first entry is erased by the sum, 60 < 0.1 * 1000
	hit(t, enemy, DAMAGE_SLASH, 60.0f);
	CHECK(evac->tracked().size() == 0); // 120 >= 100: the panic, the list is cleared (RW 0x862535); the AI command 0x2E is S-5602
}

TEST_CASE("modgap1 EvacuateDamage: the stop line names S-5602 and the AI command")
{
	const std::string line = EvacuateDamage::stopLine();
	CHECK(line.compare(0, 8, "[S-5602]") == 0);
	CHECK(line.find("0x2E") != std::string::npos);
}
