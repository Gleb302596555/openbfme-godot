// OpenBFME. LOCAL-150 tests: SymbioticStructuresBody (RW 0x008C5304, 0x008C4CF4, 0x008C53AF,
// 0x008C53D0, 0x008C4EA4, 0x008C4F84, 0x008C501C, 0x008C5123, 0x008C519A, ActiveBody +0xEC
// RW 0x8C318F / 0x8C330B / 0x8C44F0 / 0x8C464A / 0x8C3599) and SpawnBehavior (RW 0x00862351,
// 0x00862EFA). All expectations are derived from RotWK 2.01 game.dat disassembly on synthetic INI.

#include "doctest.h"
#include "CombatTestUtil.h"

#include "Common/XferLoad.h"
#include "Common/XferSave.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/Damage.h"
#include "GameLogic/FXEvents.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/ActiveBody.h"
#include "GameLogic/Module/SpawnBehavior.h"
#include "GameLogic/Module/StructureModules.h"
#include "GameLogic/Object/ObjectGeometry.h"

#include <cmath>
#include <vector>

using namespace combattest;

namespace
{
const char *kLocal150SymbioticIni = R"INI(
Armor KeepArmor
  Armor = DEFAULT 100%
End

Object Attacker
  KindOf = INFANTRY SELECTABLE CAN_ATTACK
  Body = ActiveBody ModuleTag_Body
    MaxHealth = 200
  End
End

Object SymbioteKeep
  KindOf = STRUCTURE SELECTABLE
  Geometry = BOX
  GeometryMajorRadius = 30
  GeometryMinorRadius = 30
  GeometryHeight = 60
  StructureRubbleHeight = 15
  ArmorSet
    Conditions = None
    Armor = KeepArmor
  End
  Body = ActiveBody ModuleTag_Body
    MaxHealth = 1000
    MaxHealthDamaged = 500
    MaxHealthReallyDamaged = 200
    HealingBuffFx = FX_KeepHeal
  End
End

Object SymbioteDoor
  KindOf = STRUCTURE SELECTABLE
  Geometry = BOX
  GeometryMajorRadius = 20
  GeometryMinorRadius = 20
  GeometryHeight = 50
  StructureRubbleHeight = 12
  Body = SymbioticStructuresBody ModuleTag_Body
    Symbiote = KeepLeft
    MaxHealth = 500
    HealingBuffFx = FX_DoorHeal
  End
End
)INI";

const char *kLocal150SpawnIni = R"INI(
Object SlavedUnit
  KindOf = INFANTRY SELECTABLE
  Body = ActiveBody ModuleTag_Body
    MaxHealth = 100
  End
  Behavior = AIUpdateInterface ModuleTag_AI
  End
  Behavior = SlavedUpdate ModuleTag_Slaved
    GuardMaxRange = 200
    GuardWanderRange = 50
  End
End

Object TestSpawner
  KindOf = STRUCTURE SELECTABLE
  Body = ActiveBody ModuleTag_Body
    MaxHealth = 500
  End
  Behavior = AIUpdateInterface ModuleTag_AI
  End
  Behavior = DefaultProductionExitUpdate ModuleTag_Exit
    UnitCreatePoint = X:10 Y:0 Z:0
    NaturalRallyPoint = X:30 Y:0 Z:0
  End
  Behavior = SpawnBehavior ModuleTag_Spawn
    SpawnNumber = 2
    InitialBurst = 2
    SpawnReplaceDelay = 1000
    SpawnTemplateName = SlavedUnit
  End
End
)INI";

DamageInfo makeHit(Object *attacker, float amount, int damageType)
{
	DamageInfo d{};
	d.m_input.m_sourceID = attacker ? attacker->getID() : INVALID_ID;
	d.m_input.m_damageType = damageType;
	d.m_input.m_amount = amount;
	return d;
}
} // namespace

TEST_CASE("LOCAL-150 SymbioticStructuresBody loadPostProcess links by name, copies transform, and shares damage/healing/death (RW 0x008C5304, 0x008C4CF4, 0x008C53AF, 0x008C53D0)")
{
	CombatWorld w(kLocal150SymbioticIni);
	Object *attacker = w.unit("Attacker", 'A', 100, 100);
	Object *keep = w.unit("SymbioteKeep", 'B', 200, 300);
	Object *door = w.unit("SymbioteDoor", 'B', 50, 50);
	REQUIRE(attacker != nullptr);
	REQUIRE(keep != nullptr);
	REQUIRE(door != nullptr);

	keep->setName("KeepLeft");
	keep->setOrientation(0.5f);

	ActiveBody *keepBody = dynamic_cast<ActiveBody *>(keep->getBodyModule());
	SymbioticStructuresBody *doorBody = dynamic_cast<SymbioticStructuresBody *>(door->getBodyModule());
	REQUIRE(keepBody != nullptr);
	REQUIRE(doorBody != nullptr);

	// RW 0x008C5304: loadPostProcess finds "KeepLeft", copies transform, stores ID +0x104, calls syncFromSymbiote (RW 0x008C4CF4)
	doorBody->loadPostProcess();
	CHECK(doorBody->symbioteId() == keep->getID());
	CHECK(keepBody->symbiotePartnerId() == door->getID()); // RW 0x008C4EE0 writes ActiveBody +0xEC
	CHECK(door->getPosition()->x == 200.0f);
	CHECK(door->getPosition()->y == 300.0f);
	CHECK(door->getOrientation() == doctest::Approx(0.5f)); // tolerance: atan2(sin, cos) round-trip in Thing::setTransform
	CHECK(doorBody->getHealthPercent() == 1.0f); // RW 0x008C4EA4
	CHECK(doorBody->getPreviousHealth() == 1000.0f); // RW 0x008C4F84

	// RW 0x008C501C: getArmorSetFlags delegates to symbiote; own setArmorSetFlag is a no-op (RW 0x9F3A3C)
	keep->setArmorSetFlag(1, true);
	door->setArmorSetFlag(2, true);
	CHECK((doorBody->getArmorSetFlags() & (1u << 1)) != 0u);
	CHECK((doorBody->getArmorSetFlags() & (1u << 2)) == 0u);

	// Damage keep to DAMAGED -> partner gets DAMAGED model condition (RW 0x8C44F0 -> 0x008C53AF) and delegates getters
	DamageInfo hit1 = makeHit(attacker, 600.0f, DAMAGE_SLASH);
	keep->attemptDamage(hit1);
	CHECK(keepBody->getDamageState() == BODY_DAMAGED);
	CHECK(doorBody->getDamageState() == BODY_DAMAGED);
	CHECK(door->testModelCondition(CombatNames::modelCondition("DAMAGED")));
	CHECK(doorBody->getHealthPercent() == 0.4f);
	CHECK(doorBody->getPreviousHealth() == 1000.0f);
	CHECK(doorBody->takeLastAttacker() == attacker->getID()); // RW 0x008C519A
	CHECK(doorBody->takeLastAttacker() == INVALID_ID);

	// Heal keep back to full -> partner clears DAMAGED and UNDER_CONSTRUCTION/RECONSTRUCTING and emits HealingBuffFx (RW 0x8C330B -> 0x008C53D0)
	const int kUnderConstruction = CombatNames::statuses().underConstruction;
	const int kReconstructing = CombatNames::status("RECONSTRUCTING");
	door->setStatus((unsigned)kUnderConstruction, true);
	door->setStatus((unsigned)kReconstructing, true);
	w.logic->fxEvents().beginFrame();

	DamageInfo heal = makeHit(nullptr, 600.0f, DAMAGE_HEALING);
	keepBody->attemptHealing(heal);
	CHECK(doorBody->getLastHealingFrame() == w.logic->getFrame()); // RW 0x008C5123
	CHECK_FALSE(door->testModelCondition(CombatNames::modelCondition("DAMAGED")));
	CHECK_FALSE(door->testStatus((unsigned)kUnderConstruction));
	CHECK_FALSE(door->testStatus((unsigned)kReconstructing));
	bool sawDoorHealFx = false;
	for (const FXEvent &ev : w.logic->fxEvents().frameEvents())
	{
		if (ev.fxList && *ev.fxList == "FX_DoorHeal" && ev.primary == door->getID())
		{
			sawDoorHealFx = true;
		}
	}
	CHECK(sawDoorHealFx);

	// Lethal hit on keep -> partner dies too (RW 0x8C464A..0x8C4679) and enters RUBBLE with StructureRubbleHeight 12 + NO_COLLISIONS (RW 0x008C4CF4)
	DamageInfo kill = makeHit(attacker, 1500.0f, DAMAGE_UNRESISTABLE);
	keep->attemptDamage(kill);
	CHECK(keep->isEffectivelyDead());
	CHECK(door->isEffectivelyDead());
	CHECK(doorBody->getDamageState() == BODY_RUBBLE);
	CHECK(door->testModelCondition(CombatNames::modelCondition("RUBBLE")));
	CHECK(door->testStatus((unsigned)CombatNames::statuses().noCollisions));
	CHECK(ObjectGeometry::maxHeightAbovePosition(door->geometryShapes()) == 12.0f);
}

TEST_CASE("LOCAL-150 SymbioticStructuresBody unlinked defaults for RW 0x008C4EA4..0x008C519A and ActiveBody partner xfer (RW 0x8C3599)")
{
	CombatWorld w(kLocal150SymbioticIni);
	Object *keep = w.unit("SymbioteKeep", 'B', 200, 300);
	Object *door = w.unit("SymbioteDoor", 'B', 50, 50);
	REQUIRE(keep != nullptr);
	REQUIRE(door != nullptr);

	ActiveBody *keepBody = dynamic_cast<ActiveBody *>(keep->getBodyModule());
	SymbioticStructuresBody *doorBody = dynamic_cast<SymbioticStructuresBody *>(door->getBodyModule());
	REQUIRE(keepBody != nullptr);
	REQUIRE(doorBody != nullptr);

	// Unlinked defaults from RW 0x008C4EA4, 0x008C4F84, 0x008C501C, 0x008C5123, 0x008C519A
	CHECK(doorBody->getHealthPercent() == 0.0f);
	CHECK(doorBody->getPreviousHealth() == 0.0f);
	CHECK(doorBody->getArmorSetFlags() == 0u);
	CHECK(doorBody->getLastHealingFrame() == 0xFFFFFFFFu);
	CHECK(doorBody->takeLastAttacker() == INVALID_ID);

	// Link and verify ActiveBody::xfer preserves m_symbiotePartnerId (RW 0x8C3599)
	doorBody->setSymbiote(keep->getID());
	CHECK(keepBody->symbiotePartnerId() == door->getID());

	XferSave xs;
	keepBody->xfer(xs);

	keepBody->setSymbiotePartner(INVALID_ID);
	CHECK(keepBody->symbiotePartnerId() == INVALID_ID);

	XferLoad xl(xs.data());
	keepBody->xfer(xl);
	CHECK(keepBody->symbiotePartnerId() == door->getID());
}

TEST_CASE("LOCAL-150 SpawnBehavior maySpawnSelfTaskAI (RW 0x00862351) and iterateSlavedSpawns (RW 0x00862EFA)")
{
	CombatWorld w(kLocal150SpawnIni);
	Object *spawner = w.unit("TestSpawner", 'A', 100, 100);
	REQUIRE(spawner != nullptr);

	SpawnBehavior *sb = dynamic_cast<SpawnBehavior *>(spawner->findModule("SpawnBehavior"));
	AIUpdateInterface *ai = spawner->getAIUpdateInterface();
	REQUIRE(sb != nullptr);
	REQUIRE(ai != nullptr);

	// Advance enough frames for SpawnBehavior::update to spawn the 2 InitialBurst slaved units
	for (int i = 0; i < 6; ++i)
	{
		for (int phase = 1; phase <= 6; ++phase)
		{
			w.logic->update(phase);
		}
	}
	REQUIRE(sb->spawns().size() == 2u);
	REQUIRE(sb->spawnCount() == 2);

	// RW 0x00862351: ratio == 0.0f -> false; CMD_FROM_AI with 0 self-tasking / 2 spawns -> true for ratio > 0
	ai->setLastCommandSource(CMD_FROM_AI);
	CHECK_FALSE(sb->maySpawnSelfTaskAI(0.0f));
	CHECK(sb->maySpawnSelfTaskAI(0.5f));
	CHECK_FALSE(sb->maySpawnSelfTaskAI(-0.1f));

	// RW 0x86238B: lastCommandSource != CMD_FROM_AI (2) -> false
	ai->setLastCommandSource(CMD_FROM_PLAYER);
	CHECK_FALSE(sb->maySpawnSelfTaskAI(0.5f));

	// RW 0x00862EFA: iterateSlavedSpawns visits every slaved spawn, stops on first 0 return
	struct VisitCtx
	{
		std::vector<ObjectID> seen;
		int stopAfter = 0;
	};
	auto cb = [](Object *obj, void *data) -> int {
		auto *ctx = static_cast<VisitCtx *>(data);
		ctx->seen.push_back(obj->getID());
		return (ctx->stopAfter > 0 && static_cast<int>(ctx->seen.size()) >= ctx->stopAfter) ? 0 : 1;
	};

	VisitCtx allCtx;
	CHECK(sb->iterateSlavedSpawns(cb, &allCtx));
	CHECK(allCtx.seen.size() == 2u);

	VisitCtx stopCtx;
	stopCtx.stopAfter = 1;
	CHECK_FALSE(sb->iterateSlavedSpawns(cb, &stopCtx));
	CHECK(stopCtx.seen.size() == 1u);
}
