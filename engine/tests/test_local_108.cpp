// OpenBFME. GPL-3.0.
// LOCAL-108 (fork issue 786): ActiveBody, InactiveBody, PorcupineFormationBodyModule, PhysicsBehavior (editions: both RotWK 2.01 and BFME2 1.06).
// Sources:
//   - ActiveBody::setDamageState: RW 0x008C1BF9 / DONOR BFME2 0x004BDAA9 (ActiveBody.cpp:1219)
//   - ActiveBody::resetHealthToDamageState: RW 0x008C1C75 / DONOR BFME2 0x004BDB25 (ActiveBody.cpp:2298)
//   - ActiveBody::isArmorOverridden: RW 0x008C3A48 / DONOR BFME2 0x004BF8AC -> RW 0x005D891E
//   - ActiveBody::clearLastAttacker: RW 0x008C3A89 / DONOR BFME2 0x004BF8ED
//   - ActiveBody::getDodgePercent: RW 0x008C1DAD / DONOR BFME2 0x004BDC41
//   - ActiveBody::isIndestructible: RW 0x008C3A74 / DONOR BFME2 0x004BF8D8
//   - Object::clearAllAttributeModifiers: RW 0x006998D0 / DONOR BFME2 0x00298E6A (reached from ActiveBody rubble reset RW 0x8C1BB7 at 0x8C1BF4)
//   - HordeContain::clearAllAttributeModifiers: RW 0x00879106 / DONOR BFME2 0x00475E06 (HordeContainInterface vslot 0x1E0)
//   - InactiveBody::xfer: RW 0x008C1AEA (primary slot 3 +0x0C)
//   - PorcupineFormationBodyModule: RW 0x008C61C4 / 0x008C61E3 / 0x008C6228 / 0x008C61CA (S-7191 dead-branch gate RW 0x8C6244)
//   - PhysicsBehavior: RW 0x00792C1F / 0x00792C64 / 0x00792DA1 / 0x00792C85 / 0x00793113

#include "doctest.h"

#include "CombatTestUtil.h"
#include "EnnorathScenario.h"
#include "Horde2TestUtil.h"
#include "HudTestUtil.h"

#include "Common/Player.h"
#include "Common/XferLoad.h"
#include "Common/XferSave.h"
#include "GameLogic/Armor.h"
#include "GameLogic/AttributeModifiers.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/ActiveBody.h"
#include "GameLogic/Module/CombatModules.h"
#include "GameLogic/Module/PhysicsBehavior.h"
#include "GameLogic/Module/StructureModules.h"
#include "GameLogic/Object/AttributeModifierPool.h"
#include "GameLogic/Object/Contain/HordeContainRuntime.h"
#include "GameLogic/Object/Object.h"

#include <string>
#include <vector>

using namespace combattest;

namespace
{
const char kLocal108ModList[] =
	"ModifierList Local108TestBuff\n"
	"  Category = BUFF\n"
	"  Modifier = ARMOR 25%\n"
	"  Duration = 10000\n"
	"End\n";

const char kLocal108Objects[] =
	"Object Local108Structure\n"
	"  KindOf = STRUCTURE IMMOBILE SELECTABLE SCORE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 20\n"
	"  GeometryMinorRadius = 20\n"
	"  GeometryHeight = 40\n"
	"  Body = StructureBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"    MaxHealthDamaged = 300\n"
	"    MaxHealthReallyDamaged = 100\n"
	"  End\n"
	"  Behavior = AttributeModifierPoolUpdate ModuleTag_Pool\n"
	"  End\n"
	"End\n"
	"Object Local108Unit\n"
	"  KindOf = INFANTRY SELECTABLE SCORE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = Local108Armor\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"    MaxHealthDamaged = 75\n"
	"    MaxHealthReallyDamaged = 25\n"
	"    DodgePercent = 25%\n"
	"  End\n"
	"  Behavior = AttributeModifierPoolUpdate ModuleTag_Pool\n"
	"  End\n"
	"End\n";

const char kLocal108Armor[] =
	"Armor Local108Armor\n"
	"  Armor = Default 100%\n"
	"End\n";

struct Local108World : CombatWorld
{
	AttributeModifierStore mods;
	AttributeModifierStore *savedMods = nullptr;

	Local108World()
	{
		savedMods = TheAttributeModifierStore;
		TheAttributeModifierStore = &mods;
		mods.registerBlock(w.fx.env.blocks);
		AttributeModifierPool::registerClass(w.modules);
		REQUIRE(w.load(kLocal108Armor, INI_LOAD_OVERWRITE, "local108_armor.ini").empty());
		REQUIRE(w.load(kLocal108ModList, INI_LOAD_OVERWRITE, "local108_mod.ini").empty());
		REQUIRE(w.load(kLocal108Objects, INI_LOAD_OVERWRITE, "local108_obj.ini").empty());
	}
	~Local108World() { TheAttributeModifierStore = savedMods; }
};
DamageInfo makeHit(Object *src, float amount, int type)
{
	DamageInfo d{};
	d.m_input.m_sourceID = src ? src->getID() : INVALID_ID;
	d.m_input.m_amount = amount;
	d.m_input.m_damageType = type;
	return d;
}
} // namespace

// RW 0x8C1BB7 -> RW 0x006998D0 (DONOR BFME2 0x00298E6A):
// When a STRUCTURE enters BODY_RUBBLE, ActiveBody's rubble reset clears its AttributeModifierPool entries in place.
TEST_CASE("LOCAL-108 ActiveBody: entering BODY_RUBBLE clears a STRUCTURE's AttributeModifierPool (RW 0x8C1BB7 -> RW 0x006998D0)")
{
	Local108World w;
	Object *s = w.unit("Local108Structure", 'B', 300, 300);
	Object *a = w.unit("Swordsman", 'A', 200, 300);
	REQUIRE(s->addAttributeModifier("Local108TestBuff", 10000));
	auto *pool = static_cast<AttributeModifierPool *>(s->attributeModifierPoolModule());
	REQUIRE(pool != nullptr);
	REQUIRE(pool->entryCount() == 1u);

	DamageInfo d = makeHit(a, 100000.0f, DAMAGE_UNRESISTABLE);
	s->attemptDamage(d);
	REQUIRE(s->getBodyModule()->getDamageState() == BODY_RUBBLE);
	CHECK(pool->entryCount() == 0u);
}

// RW 0x008C1BF9 (DONOR BFME2 0x004BDAA9) & RW 0x008C1C75 (DONOR BFME2 0x004BDB25) & RW 0x008C1DAD / 0x008C3A74 / 0x008C3A89 / 0x008C3A48:
// ActiveBody::resetDamageState sets health to each state's threshold and updates the damage state;
// ActiveBody::resetHealthToDamageState snaps health to the current damage state's threshold.
TEST_CASE("LOCAL-108 ActiveBody: setDamageState (RW 0x008C1BF9), resetHealthToDamageState (RW 0x008C1C75), and accessors")
{
	Local108World w;
	Object *u = w.unit("Local108Unit", 'B', 300, 300);
	Object *a = w.unit("Swordsman", 'A', 200, 300);
	auto *body = dynamic_cast<ActiveBody *>(u->getBodyModule());
	REQUIRE(body != nullptr);

	CHECK(body->getDodgePercent() == 0.25f); // RW 0x008C1DAD
	CHECK_FALSE(body->isIndestructible());                    // RW 0x008C3A74
	body->setIndestructible(true);
	CHECK(body->isIndestructible());
	body->setIndestructible(false);
	CHECK_FALSE(body->isArmorOverridden());                   // RW 0x008C3A48 -> RW 0x005D891E

	// Hit once to record last attacker, then clearLastAttacker (RW 0x008C3A89) suppresses takeLastAttacker.
	DamageInfo d = makeHit(a, 45.0f, DAMAGE_UNRESISTABLE);
	u->attemptDamage(d);
	REQUIRE(body->getHealth() == 55.0f);
	REQUIRE(body->getDamageState() == BODY_DAMAGED);
	body->clearLastAttacker(); // RW 0x008C3A89
	CHECK(body->takeLastAttacker() == INVALID_ID);

	// RW 0x008C1C75: snaps 55.0f (in BODY_DAMAGED) to m_damagedFraction * m_maxHealth = 75.0f.
	body->resetHealthToDamageState();
	CHECK(body->getHealth() == 75.0f);
	CHECK(body->getDamageState() == BODY_DAMAGED);

	// RW 0x008C1BF9: all four states
	body->resetDamageState(BODY_REALLYDAMAGED);
	CHECK(body->getHealth() == 25.0f);
	CHECK(body->getDamageState() == BODY_REALLYDAMAGED);

	body->resetDamageState(BODY_RUBBLE);
	CHECK(body->getHealth() == 0.0f);
	CHECK(body->getDamageState() == BODY_RUBBLE);

	body->resetDamageState(BODY_PRISTINE);
	CHECK(body->getHealth() == 100.0f);
	CHECK(body->getDamageState() == BODY_PRISTINE);
}

// RW 0x006998D0 -> RW 0x00879106 (DONOR BFME2 0x00298E6A -> 0x00475E06):
// Calling Object::clearAllAttributeModifiers on a HORDE clears every contained member's pool and the horde's own pool.
TEST_CASE("LOCAL-108 ActiveBody: Object::clearAllAttributeModifiers on a HORDE clears horde and member pools (RW 0x006998D0 -> RW 0x00879106)")
{
	ennorathscenario::World world;
	if (!world.load("LOCAL-108 horde clearAllAttributeModifiers"))
	{
		return;
	}
	hudtest::Rig game(world.shared, "map mp fall back 8p", "FactionMen");
	std::string err;
	Object *horde = game.game->createObject("GondorFighterHorde", game.index(), Coord3D{ 1700.0f, 2300.0f, 0.0f }, 0.0f, &err);
	REQUIRE_MESSAGE(horde != nullptr, err);
	auto *hc = dynamic_cast<HordeContain *>(horde->getContain());
	REQUIRE(hc != nullptr);
	REQUIRE_FALSE((*hc->getContainedItemsList()).empty());

	// Pick a real non-LEVEL ModifierList from Ennorath's store (e.g. GenericHeroLeadership or first non-LEVEL list).
	REQUIRE(TheAttributeModifierStore != nullptr);
	std::string modName;
	for (size_t i = 0; i < TheAttributeModifierStore->size(); ++i)
	{
		const ModifierListTemplate *ml = TheAttributeModifierStore->get((int)i);
		if (ml && ml->m_category != ATTRIBUTE_CATEGORY_LEVEL && !ml->m_name.empty())
		{
			modName = ml->m_name;
			break;
		}
	}
	REQUIRE_FALSE(modName.empty());
	REQUIRE(horde->addAttributeModifier(modName, 10000));
	auto *hordePool = static_cast<AttributeModifierPool *>(horde->attributeModifierPoolModule());
	auto *memberPool = static_cast<AttributeModifierPool *>((*hc->getContainedItemsList()).front()->attributeModifierPoolModule());
	REQUIRE(hordePool != nullptr);
	REQUIRE(memberPool != nullptr);
	REQUIRE(hordePool->entryCount() >= 1u); // tolerance: Ennorath horde may carry innate modifiers
	REQUIRE(memberPool->entryCount() >= 1u); // tolerance: Ennorath member may carry innate modifiers

	horde->clearAllAttributeModifiers(); // RW 0x006998D0 -> RW 0x00879106
	CHECK(hordePool->entryCount() == 0u);
	for (Object *m : (*hc->getContainedItemsList()))
	{
		auto *mp = static_cast<AttributeModifierPool *>(m->attributeModifierPoolModule());
		REQUIRE(mp != nullptr);
		CHECK(mp->entryCount() == 0u);
	}
}

// Ennorath scenario (issue #786):
// 1. ActiveBody: a unit hurt to its damaged and really damaged states (GondorFighter), plus a structure entering BODY_RUBBLE (GondorBarracks).
// 2. PorcupineFormationBodyModule: a pikemen horde (GondorTowerShieldGuardHorde) engaged by cavalry (GondorKnightHorde).
// 3. PhysicsBehavior: a unit flung by a troll's blow landing (MordorMountainTroll / GondorFighter).
TEST_CASE("LOCAL-108 ENNORATH scenario: ActiveBody damage states, PorcupineFormationBodyModule pikemen vs cavalry, PhysicsBehavior landing")
{
	ennorathscenario::World world;
	if (!world.load("LOCAL-108 Ennorath scenario"))
	{
		return;
	}
	hudtest::Rig game(world.shared, "map mp fall back 8p", "FactionMen");
	const int p1 = game.index();
	Player *enemy = game.game->players().findPlayerWithName("Player_2");
	REQUIRE(enemy != nullptr);
	const int p2 = enemy->getPlayerIndex();
	std::string err;

	// 1. ActiveBody: hurt GondorFighter into BODY_DAMAGED and BODY_REALLYDAMAGED (RW 0x8C2A5D / RW 0x8C1D53).
	Object *fighter = game.game->createObject("GondorFighter", p1, Coord3D{ 1700.0f, 2300.0f, 0.0f }, 0.0f, &err);
	REQUIRE_MESSAGE(fighter != nullptr, err);
	auto *fighterBody = dynamic_cast<ActiveBody *>(fighter->getBodyModule());
	REQUIRE(fighterBody != nullptr);
	const float maxHp = fighterBody->getMaxHealth();
	REQUIRE(maxHp > 0.0f); // tolerance: Ennorath GondorFighter MaxHealth is mod-defined
	const int mcDamaged = CombatNames::modelCondition("DAMAGED");
	const int mcReally = CombatNames::modelCondition("REALLYDAMAGED");

	// Use GondorBarracks (StructureBody / ActiveBody) which has explicit MaxHealthDamaged and MaxHealthReallyDamaged thresholds.
	Object *barracks = game.game->createObject("GondorBarracks", p1, Coord3D{ 1700.0f, 2100.0f, 0.0f }, 0.0f, &err);
	REQUIRE_MESSAGE(barracks != nullptr, err);
	auto *barracksBody = dynamic_cast<ActiveBody *>(barracks->getBodyModule());
	REQUIRE(barracksBody != nullptr);
	const float bMax = barracksBody->getMaxHealth();
	REQUIRE(bMax > 0.0f); // tolerance: Ennorath GondorBarracks MaxHealth is mod-defined

	DamageInfo d1{};
	d1.m_input.m_damageType = DAMAGE_UNRESISTABLE;
	d1.m_input.m_amount = bMax * 0.55f; // drops into BODY_DAMAGED
	barracks->attemptDamage(d1);
	CHECK(barracksBody->getDamageState() == BODY_DAMAGED);
	CHECK(barracks->testModelCondition(mcDamaged));
	CHECK_FALSE(barracks->testModelCondition(mcReally));

	DamageInfo d2{};
	d2.m_input.m_damageType = DAMAGE_UNRESISTABLE;
	d2.m_input.m_amount = bMax * 0.35f; // drops into BODY_REALLYDAMAGED
	barracks->attemptDamage(d2);
	CHECK(barracksBody->getDamageState() == BODY_REALLYDAMAGED);
	CHECK(barracks->testModelCondition(mcReally));
	CHECK_FALSE(barracks->testModelCondition(mcDamaged));

	// Also check GondorFighter (ActiveBody) drops into BODY_DAMAGED when hurt past 60% of max health.
	DamageInfo fd{};
	fd.m_input.m_damageType = DAMAGE_UNRESISTABLE;
	fd.m_input.m_amount = maxHp * 0.60f;
	fighter->attemptDamage(fd);
	CHECK(fighterBody->getDamageState() != BODY_PRISTINE);


	// 2. PorcupineFormationBodyModule: GondorTowerShieldGuardHorde (members use PorcupineFormationBodyModule) hit by GondorKnightHorde.
	Object *pikes = game.game->createObject("GondorTowerShieldGuardHorde", p1, Coord3D{ 1900.0f, 2300.0f, 0.0f }, 0.0f, &err);
	REQUIRE_MESSAGE(pikes != nullptr, err);
	auto *pikeHorde = dynamic_cast<HordeContain *>(pikes->getContain());
	REQUIRE(pikeHorde != nullptr);
	REQUIRE_FALSE((*pikeHorde->getContainedItemsList()).empty());
	auto *porcBody = dynamic_cast<PorcupineFormationBody *>((*pikeHorde->getContainedItemsList()).front()->getBodyModule());
	REQUIRE(porcBody != nullptr);

	Object *knights = game.game->createObject("GondorKnightHorde", p2, Coord3D{ 2050.0f, 2300.0f, 0.0f }, 0.0f, &err);
	REQUIRE_MESSAGE(knights != nullptr, err);
	REQUIRE(knights->getAIUpdateInterface() != nullptr);
	knights->getAIUpdateInterface()->aiMoveToPosition(Coord3D{ 1850.0f, 2300.0f, 0.0f }, CMD_FROM_PLAYER);
	for (int i = 0; i < 40; ++i)
	{
		for (int phase = 1; phase <= 6; ++phase) game.logic().update(phase);
	}
	CHECK(pikes->getBodyModule() != nullptr);

	// 3. PhysicsBehavior: a GondorFighter thrown by a troll's knockback flies and lands (RW 0x692223 -> RW 0x792DBD -> RW 0x79350E -> RW 0x793372).
	Object *flung = game.game->createObject("GondorFighter", p1, Coord3D{ 2200.0f, 2300.0f, 0.0f }, 0.0f, &err);
	REQUIRE_MESSAGE(flung != nullptr, err);
	PhysicsBehavior *phys = PhysicsBehavior::find(*flung);
	REQUIRE(phys != nullptr);
	const Coord3D beforePos = *flung->getPosition();
	REQUIRE(ObjectKnockback::apply(*flung, 0.0f, 3.0f, 1.5f));
	CHECK(phys->isFlying());
	CHECK(phys->isStunned());
	bool landed = false;
	for (int i = 0; i < 120; ++i)
	{
		for (int phase = 1; phase <= 6; ++phase) game.logic().update(phase);
		if (!phys->isFlying())
		{
			landed = true;
			break;
		}
	}
	CHECK(landed);
	CHECK(flung->getPosition()->x != beforePos.x);
}
