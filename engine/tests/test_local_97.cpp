// OpenBFME unit tests: LOCAL-97 (fork issue 762; editions: both):
//   - PoisonedBehavior: periodic poison damage, poisoned tint, cure on healing, xfer
//     (RW 0x00886165, 0x00886193, 0x008861FA, 0x00886145, 0x00886272, 0x0088616B, 0x0088628E,
//      0x00886300, 0x00886326, 0x00886378, 0x0088638E, 0x008863B0, 0x00850BE9, 0x00670913,
//      ActiveBody::attemptHealing onHealing RW 0x008C30C0..0x008C30E9).

#include "doctest.h"
#include "CombatTestUtil.h"
#include "EnnorathScenario.h"

#include "Common/Player.h"
#include "Common/StateHash.h"
#include "Common/XferLoad.h"
#include "Common/XferSave.h"
#include "GameClient/ClientEvents.h"
#include "GameClient/Drawable.h"
#include "GameLogic/Damage.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/ActiveBody.h"
#include "GameLogic/Module/DamageFieldUpdate.h"
#include "GameLogic/Module/PoisonedBehavior.h"
#include "GameLogic/Module/ThreatFinderUpdate.h"
#include "GameLogic/AttributeModifiers.h"
#include "GameLogic/Object/AttributeModifierPool.h"
#include "GameLogic/System/ThreatFinder.h"
#include "GameLogic/Object/Object.h"

#include <utility>
#include <vector>

namespace
{
struct TintCaptureHooks : public ObjectClientHooks
{
	std::vector<std::pair<ObjectID, UnsignedInt>> setEvents;
	std::vector<std::pair<ObjectID, UnsignedInt>> clearEvents;

	void objectCreated(Object &) override {}
	void objectDestroyed(Object &) override {}
	void setDrawableTintStatus(Object &obj, UnsignedInt statusBits) override
	{
		setEvents.push_back({ obj.getID(), statusBits });
	}
	void clearDrawableTintStatus(Object &obj, UnsignedInt statusBits) override
	{
		clearEvents.push_back({ obj.getID(), statusBits });
	}
};

const char *kPoisonIni = R"(
Object Local97PoisonVictim
	Side = Men
	KindOf = SELECTABLE INFANTRY
	Body = ActiveBody ModuleTag_Body
		MaxHealth = 200.0
	End
	Behavior = PoisonedBehavior ModuleTag_Poison
		PoisonDamageInterval = 1000
		PoisonDuration = 3000
	End
End
)";

struct PoisonRig
{
	combattest::CombatWorld world;

	PoisonRig()
	{
		PoisonedBehavior::registerClass(world.w.modules);
		const std::string err = world.w.load(kPoisonIni, INI_LOAD_OVERWRITE, "local97_poison.ini");
		REQUIRE_MESSAGE(err.empty(), err);
	}
};
} // namespace

TEST_CASE("LOCAL-97 PoisonedBehavior: POISON hit ticks damage, sets tint, heals cure poison, and xfer round-trips (RW 0x008863B0, RW 0x0088638E)")
{
	PoisonRig rig;
	TintCaptureHooks hooks;
	rig.world.logic->setClientHooks(&hooks);

	Object *victim = rig.world.unit("Local97PoisonVictim", 'A', 100.0f, 100.0f);
	REQUIRE(victim != nullptr);
	auto *pb = dynamic_cast<PoisonedBehavior *>(victim->findModule("PoisonedBehavior"));
	REQUIRE(pb != nullptr);
	REQUIRE(pb->moduleData() != nullptr);
	// 1000 ms = 5 frames at 5 Hz (LOGICFRAMES_PER_SECOND = 5), 3000 ms = 15 frames
	CHECK(pb->moduleData()->m_poisonDamageInterval == 5u);
	CHECK(pb->moduleData()->m_poisonDuration == 15u);
	CHECK(pb->poisonOverallStopFrame() == 0u);
	CHECK(pb->poisonDeathType() == DEATH_POISONED);

	// Step to frame 2 so frame arithmetic is non-zero.
	rig.world.frames(2);
	const UnsignedInt t0 = rig.world.logic->getFrame();
	REQUIRE(t0 == 2u);

	// Non-POISON hit must not trigger poison (RW 0x0088637C: cmp damageType, 0x1A).
	{
		DamageInfo slash;
		slash.m_input.m_damageType = DAMAGE_SLASH;
		slash.m_input.m_amount = 10.0f;
		victim->attemptDamage(slash);
		CHECK(pb->poisonOverallStopFrame() == 0u);
		CHECK(hooks.setEvents.empty());
		CHECK(victim->getBodyModule()->getHealth() == 190.0f);
	}

	// POISON hit deals 20 damage and starts periodic poison (interval = 5 frames, duration = 15 frames).
	{
		DamageInfo poison;
		poison.m_input.m_damageType = DAMAGE_POISON;
		poison.m_input.m_deathType = DEATH_POISONED;
		poison.m_input.m_amount = 20.0f;
		victim->attemptDamage(poison);
	}
	CHECK(victim->getBodyModule()->getHealth() == 170.0f);
	CHECK(pb->poisonDamageAmount() == 20.0f);
	CHECK(pb->poisonDamageFrame() == t0 + 5u);
	CHECK(pb->poisonOverallStopFrame() == t0 + 15u);
	REQUIRE(hooks.setEvents.size() == 1u);
	CHECK(hooks.setEvents[0].second == Drawable::TINT_STATUS_POISONED);

	// Advance 4 frames (to t0 + 4): no poison tick yet.
	rig.world.frames(4);
	CHECK(victim->getBodyModule()->getHealth() == 170.0f);

	// Advance 1 frame (to t0 + 5): first periodic tick (-20 -> 150).
	rig.world.frames(1);
	CHECK(victim->getBodyModule()->getHealth() == 150.0f);
	CHECK(pb->poisonDamageFrame() == t0 + 10u);

	// Re-poison at t0 + 6: extends overallStopFrame to t0 + 6 + 15 = t0 + 21,
	// while keeping the earlier m_poisonDamageFrame (min(t0 + 10, t0 + 11) == t0 + 10, RW 0x008862BE..0x008862CC).
	rig.world.frames(1);
	{
		DamageInfo repoison;
		repoison.m_input.m_damageType = DAMAGE_POISON;
		repoison.m_input.m_deathType = DEATH_POISONED_BETA;
		repoison.m_input.m_amount = 15.0f;
		victim->attemptDamage(repoison);
	}
	CHECK(victim->getBodyModule()->getHealth() == 135.0f);
	CHECK(pb->poisonDamageAmount() == 15.0f);
	CHECK(pb->poisonDamageFrame() == t0 + 10u);
	CHECK(pb->poisonOverallStopFrame() == t0 + 21u);
	CHECK(pb->poisonDeathType() == DEATH_POISONED_BETA);

	// Xfer round-trip (RW 0x00886326).
	{
		XferSave save;
		pb->xfer(save);
		StateHasher hBefore;
		pb->crc(hBefore);

		// Mutate via healing then reload.
		victim->attemptHealing(10.0f, nullptr);
		CHECK(pb->poisonOverallStopFrame() == 0u);
		CHECK(pb->poisonDamageFrame() == 0u);
		CHECK(pb->poisonDamageAmount() == 0.0f);
		REQUIRE(hooks.clearEvents.size() == 1u);
		CHECK(hooks.clearEvents[0].second == Drawable::TINT_STATUS_POISONED);

		XferLoad load(save.data());
		pb->xfer(load);
		StateHasher hAfter;
		pb->crc(hAfter);
		CHECK(hAfter.value() == hBefore.value());
		CHECK(pb->poisonDamageFrame() == t0 + 10u);
		CHECK(pb->poisonOverallStopFrame() == t0 + 21u);
		CHECK(pb->poisonDamageAmount() == 15.0f);
		CHECK(pb->poisonDeathType() == DEATH_POISONED_BETA);
	}

	// Healing cures poison permanently (RW 0x008C30C0..0x008C30E9 -> RW 0x0088638E): no further ticks after heal.
	victim->attemptHealing(5.0f, nullptr);
	const float healthAfterHeal = victim->getBodyModule()->getHealth();
	CHECK(pb->poisonOverallStopFrame() == 0u);
	rig.world.frames(15);
	CHECK(victim->getBodyModule()->getHealth() == healthAfterHeal);

	// Poison expiry without healing clears tint once m_poisonOverallStopFrame is reached (RW 0x0088641C..0x00886436).
	const std::size_t clearsBefore = hooks.clearEvents.size();
	{
		DamageInfo poison;
		poison.m_input.m_damageType = DAMAGE_POISON;
		poison.m_input.m_deathType = DEATH_POISONED;
		poison.m_input.m_amount = 5.0f;
		victim->attemptDamage(poison);
	}
	rig.world.frames(16);
	CHECK(pb->poisonOverallStopFrame() == 0u);
	CHECK(hooks.clearEvents.size() == clearsBefore + 1u);

	rig.world.logic->setClientHooks(nullptr);
}

TEST_CASE("LOCAL-97 ENNORATH scenario PoisonedBehavior: CINE_Gloin_TEMP carries PoisonedBehavior from cinematicobjects.ini and takes periodic poison damage (RW 0x008863B0)")
{
	ennorathscenario::World world;
	if (!world.load("LOCAL-97 ENNORATH scenario PoisonedBehavior"))
	{
		return;
	}
	structtest::Arena arena(world.shared, "FactionDwarves", "FactionWild");

	Object *gloin = world.place(arena, "CINE_Gloin_TEMP", 0, 400.0f, 400.0f);
	REQUIRE(gloin != nullptr);
	auto *pb = dynamic_cast<PoisonedBehavior *>(gloin->findModule("PoisonedBehavior"));
	REQUIRE(pb != nullptr);
	REQUIRE(pb->moduleData() != nullptr);
	// data/ini/object/modcinematic/cinematicobjects.ini: PoisonDamageInterval = 1000 (5 frames), PoisonDuration = 30000 (150 frames)
	CHECK(pb->moduleData()->m_poisonDamageInterval == 5u);
	CHECK(pb->moduleData()->m_poisonDuration == 150u);

	const float hpBefore = gloin->getBodyModule()->getHealth();
	REQUIRE(hpBefore > 50.0f); // tolerance: CINE_Gloin_TEMP MaxHealth from mod INI

	DamageInfo poison;
	poison.m_input.m_damageType = DAMAGE_POISON;
	poison.m_input.m_deathType = DEATH_POISONED;
	poison.m_input.m_amount = 10.0f;
	gloin->attemptDamage(poison);

	const float hpAfterHit = gloin->getBodyModule()->getHealth();
	REQUIRE(hpAfterHit < hpBefore); // tolerance: poison hit reduced health below initial MaxHealth
	const float dealt = hpBefore - hpAfterHit;
	CHECK(pb->poisonDamageAmount() == dealt);

	// Step 5 frames -> one periodic poison tick.
	for (int i = 0; i < 5; ++i)
	{
		arena.logic.runLogicFrame();
	}
	CHECK(gloin->getBodyModule()->getHealth() == hpAfterHit - dealt);
}

namespace
{
const char *kDamageFieldShortRangeIni = R"(
Weapon Local97ShortRangeFieldWeapon
	AttackRange = 10.0
	DamageNugget
		Damage = 25
		DamageType = MAGIC
		DeathType = NORMAL
	End
End

Object Local97DamageFieldTower
	Side = Men
	KindOf = STRUCTURE SELECTABLE IMMOBILE
	Geometry = BOX
	GeometryMajorRadius = 10
	GeometryMinorRadius = 10
	GeometryHeight = 20
	Body = ActiveBody ModuleTag_Body
		MaxHealth = 500
	End
	Behavior = DamageFieldUpdate ModuleTag_DF
		FireWeaponNugget
			WeaponName = Local97ShortRangeFieldWeapon
			FireDelay = 0
			OneShot = No
		End
		Radius = 150.0
		ObjectFilter = ANY +INFANTRY ENEMIES
	End
End

Object Local97FieldVictim
	Side = Mordor
	KindOf = SELECTABLE INFANTRY
	Geometry = CYLINDER
	GeometryMajorRadius = 5
	GeometryHeight = 15
	Body = ActiveBody ModuleTag_Body
		MaxHealth = 100
	End
End
)";
} // namespace

TEST_CASE("LOCAL-97 DamageFieldUpdate: fires via forceFireWeapon ignoring weapon AttackRange (RW 0x0089483A -> RW 0x006CF407) and xfer round-trips (RW 0x00894C09)")
{
	combattest::CombatWorld world;
	DamageFieldUpdate::registerClass(world.w.modules);
	const std::string err = world.w.load(kDamageFieldShortRangeIni, INI_LOAD_OVERWRITE, "local97_df.ini");
	REQUIRE_MESSAGE(err.empty(), err);

	Object *tower = world.unit("Local97DamageFieldTower", 'A', 200.0f, 200.0f);
	Object *enemy = world.unit("Local97FieldVictim", 'B', 260.0f, 200.0f); // distance 60 > weapon AttackRange 10, but <= Radius 150
	REQUIRE(tower != nullptr);
	REQUIRE(enemy != nullptr);

	auto *df = dynamic_cast<DamageFieldUpdate *>(tower->findModule("DamageFieldUpdate"));
	REQUIRE(df != nullptr);

	// Advance 1 frame: RW 0x006CF407 ignores weapon AttackRange (10) because victim is inside DamageFieldUpdate Radius (150).
	world.frames(1);
	CHECK(enemy->getBodyModule()->getHealth() == 75.0f);
	REQUIRE(df->victims().size() == 1u);
	CHECK(df->victims()[0] == enemy->getID());

	// Xfer round-trip (RW 0x00894C09).
	XferSave save;
	df->xfer(save);
	StateHasher hBefore;
	df->crc(hBefore);

	XferLoad load(save.data());
	df->xfer(load);
	StateHasher hAfter;
	df->crc(hAfter);
	CHECK(hAfter.value() == hBefore.value());
}

namespace
{
const char *kThreatFinderIni = R"(
Object Local97ThreatMarker
	Side = Men
	KindOf = IMMOBILE
	Behavior = ThreatFinderUpdate ModuleTag_TF
		DefaultRadius = 120.0
	End
End
)";
} // namespace

TEST_CASE("LOCAL-97 ThreatFinderUpdate: update tracks owner position and ThreatFinderManager::reset clears registry (RW 0x007EDDB6, RW 0x007EE258)")
{
	combattest::CombatWorld world;
	ThreatFinderUpdate::registerModule(world.w.modules);
	const std::string err = world.w.load(kThreatFinderIni, INI_LOAD_OVERWRITE, "local97_tf.ini");
	REQUIRE_MESSAGE(err.empty(), err);

	Object *marker = world.unit("Local97ThreatMarker", 'A', 150.0f, 250.0f);
	REQUIRE(marker != nullptr);
	auto *tfu = dynamic_cast<ThreatFinderUpdate *>(marker->findModule("ThreatFinderUpdate"));
	REQUIRE(tfu != nullptr);
	REQUIRE(tfu->data() != nullptr);
	CHECK(tfu->data()->m_radius == 120.0f);

	ThreatFinder finder("Local97Area", false);
	world.logic->threatFinders().add(*world.logic, "Local97Area", finder);
	CHECK(world.logic->threatFinders().find(*world.logic, "Local97Area") == &finder);

	// RW 0x007EE258: ThreatFinderManager::reset clears all registered finders.
	world.logic->threatFinders().reset();
	CHECK(world.logic->threatFinders().find(*world.logic, "Local97Area") == nullptr);
}

namespace
{
const char *kAttrPoolIni = R"(
ModifierList Local97DamageBuff
	Category = BUFF
	Modifier = DAMAGE_MULT 150%
	Modifier = ARMOR 25%
	Duration = 1000
End

Object Local97BuffUnit
	Side = Men
	KindOf = SELECTABLE INFANTRY
	Body = ActiveBody ModuleTag_Body
		MaxHealth = 100
	End
	Behavior = AttributeModifierPoolUpdate ModuleTag_Pool
	End
End
)";
} // namespace

static float poolProduct(const AttributeModifierPool &pool, int type)
{
	float out = 1.0f;
	if (!pool.product(type, nullptr, false, out))
	{
		return 1.0f;
	}
	return out;
}

static float poolSum(const AttributeModifierPool &pool, int type)
{
	float out = 0.0f;
	if (!pool.sum(type, nullptr, out))
	{
		return 0.0f;
	}
	return out;
}

TEST_CASE("LOCAL-97 AttributeModifierPoolUpdate: timed ModifierList applies multipliers and expires after Duration (RW 0x0080585F, RW 0x00805A8E)")
{
	combattest::CombatWorld world;
	AttributeModifierStore modifiers;
	modifiers.registerBlock(world.w.fx.env.blocks);
	AttributeModifierStore *savedStore = TheAttributeModifierStore;
	TheAttributeModifierStore = &modifiers;
	struct RestoreStore
	{
		AttributeModifierStore *saved;
		~RestoreStore() { TheAttributeModifierStore = saved; }
	} restore{ savedStore };

	const std::string err = world.w.load(kAttrPoolIni, INI_LOAD_OVERWRITE, "local97_pool.ini");
	REQUIRE_MESSAGE(err.empty(), err);

	Object *u = world.unit("Local97BuffUnit", 'A', 100.0f, 100.0f);
	REQUIRE(u != nullptr);
	auto *pool = dynamic_cast<AttributeModifierPool *>(u->attributeModifierPoolModule());
	REQUIRE(pool != nullptr);

	// Step past frame 0 so frame > m_categoryDisabled[cat] (0).
	world.frames(1);
	CHECK(poolProduct(*pool, ATTRIBUTE_MODIFIER_DAMAGE_MULT) == 1.0f);
	CHECK(poolSum(*pool, ATTRIBUTE_MODIFIER_ARMOR) == 0.0f);

	REQUIRE(u->addAttributeModifier("Local97DamageBuff", -1));
	CHECK(poolProduct(*pool, ATTRIBUTE_MODIFIER_DAMAGE_MULT) == 1.5f);
	CHECK(poolSum(*pool, ATTRIBUTE_MODIFIER_ARMOR) == 0.25f);

	// Duration = 1000 ms = 5 frames at 5 Hz: expires after 5 frames.
	world.frames(5);
	CHECK(poolProduct(*pool, ATTRIBUTE_MODIFIER_DAMAGE_MULT) == 1.0f);
	CHECK(poolSum(*pool, ATTRIBUTE_MODIFIER_ARMOR) == 0.0f);
}

TEST_CASE("LOCAL-97 ENNORATH scenario AttributeModifierPoolUpdate: DainMightyRageBuff applies DAMAGE_MULT 150% and ARMOR 50% through the pool and expires (RW 0x0080585F)")
{
	ennorathscenario::World world;
	if (!world.load("LOCAL-97 ENNORATH scenario AttributeModifierPoolUpdate"))
	{
		return;
	}
	structtest::Arena arena(world.shared, "FactionDwarves", "FactionMordor");

	Object *gloin = world.place(arena, "CINE_Gloin_TEMP", 0, 400.0f, 400.0f);
	REQUIRE(gloin != nullptr);
	auto *pool = dynamic_cast<AttributeModifierPool *>(gloin->attributeModifierPoolModule());
	REQUIRE(pool != nullptr);

	// Step past frame 0 so frame > m_categoryDisabled[cat] (0).
	arena.logic.runLogicFrame();

	// data/ini/attributemodifier.ini: DainMightyRageBuff (ARMOR 50%, DAMAGE_MULT 150%, Duration = 30000 = 150 frames)
	REQUIRE(gloin->addAttributeModifier("DainMightyRageBuff", -1));
	CHECK(poolProduct(*pool, ATTRIBUTE_MODIFIER_DAMAGE_MULT) == 1.5f);
	CHECK(poolSum(*pool, ATTRIBUTE_MODIFIER_ARMOR) == 0.5f);

	for (int i = 0; i < 150; ++i)
	{
		arena.logic.runLogicFrame();
	}
	CHECK(poolProduct(*pool, ATTRIBUTE_MODIFIER_DAMAGE_MULT) == 1.0f);
	CHECK(poolSum(*pool, ATTRIBUTE_MODIFIER_ARMOR) == 0.0f);
}
