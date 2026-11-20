// OpenBFME tests for PORT-293 (issue #685): ActiveBody's last-healing timestamp -- module + 0xC0, the body interface's slot 0x48 -- and the one rule that reads
// it, AutoHealBehavior's NonStackable test.
//
// RotWK 2.01 facts (game.dat, verified by hand from the disassembly; caveat S-001):
//   * The object's constructor ORs + 0xC0 with -1 (RW 0x8C3897), so a body that was never healed answers 0xFFFFFFFF.
//   * attemptHealing stamps the current logic frame (RW 0x8C30A9 .. 0x8C30BA: `mov eax, [0xDE412C]` = TheGameLogic, `[eax + 0x40]` the frame, `mov [esi + 0xB0], eax`)
//     after the health change and its two output stores, before the health-up test of RW 0x8C30B1 -- a hit the caller clipped to zero never reaches the store.
//   * The getter is one shared stub for every body class: RW 0x5DC48C `mov eax, [ecx + 0xB0]; ret` (body slot 0x48 of ActiveBody's table RW 0xC720F0 and of the
//     classes that reuse it); InactiveBody answers the shared `or eax, -1; ret` (RW 0x48DACD, its table RW 0xC718C8 + 0x48), i.e. never healed.
//   * AutoHealBehavior::healOne refuses a NonStackable heal (data + 0x174) while the target's stamp plus HealingDelay (data + 0x140) is still in the future
//     (RW 0x855777 .. 0x8557A2: vslot 0x48, `add eax, [esi + 0x140]`, `cmp eax, ebx`, `ja`). The sum is unsigned, so a never-healed body refuses until
//     HealingDelay - 1 frames of the game have passed. A refusal skips everything after it, including the next-heal-frame write of RW 0x855826.

#include "doctest.h"

#include "CombatTestUtil.h"

#include "Common/GameEdition.h"
#include "GameLogic/Damage.h"
#include "GameLogic/Module/BehaviorModule.h"
#include "GameLogic/Module/HeroModules.h"
#include "GameLogic/Object/Object.h"

using namespace combattest;

// AutoHealBehavior's friend (HeroModules.h): the private step update() drives (RW 0x855761), called directly so a test controls the healer's cadence
struct Port293HealAccess
{
	static void healOne(AutoHealBehavior &heal, Object &target, bool fx) { heal.healOne(target, fx); }
};

namespace
{

const char kObjects[] =
	"Object Port293Healer\n"
	"  KindOf = SELECTABLE INFANTRY HERO\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"  Behavior = AutoHealBehavior ModuleTag_Heal\n"
	"    StartsActive = Yes\n"
	"    HealingAmount = 10\n"
	"    HealingDelay = 1000\n" // 30 logic frames
	"    NonStackable = Yes\n"
	"  End\n"
	"End\n"
	"Object Port293StackHealer\n"
	"  KindOf = SELECTABLE INFANTRY HERO\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"  Behavior = AutoHealBehavior ModuleTag_Heal\n"
	"    StartsActive = Yes\n"
	"    HealingAmount = 10\n"
	"    HealingDelay = 1000\n"
	"    NonStackable = No\n"
	"  End\n"
	"End\n"
	"Object Port293Target\n"
	"  KindOf = SELECTABLE INFANTRY\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"End\n"
	"Object Port293Prop\n"
	"  KindOf = IMMOBILE\n"
	"  Body = InactiveBody ModuleTag_Body\n"
	"  End\n"
	"End\n";

struct Port293World : CombatWorld
{
	Port293World()
		: CombatWorld(kObjects)
	{
		logic->setEdition(GameEdition::RotWK201);
	}
	AutoHealBehavior *healOf(Object *o) const
	{
		AutoHealBehavior *h = o ? dynamic_cast<AutoHealBehavior *>(o->findModule("AutoHealBehavior")) : nullptr;
		REQUIRE(h);
		return h;
	}
};

// the hundred points of damage every test starts from: 500 -> 400
void wound(Object &target)
{
	DamageInfo hit;
	hit.m_input.m_amount = 100.0f;
	hit.m_input.m_damageType = DAMAGE_UNRESISTABLE;
	target.attemptDamage(hit);
	REQUIRE(target.getBodyModule()->getHealth() == 400.0f);
}

float health(Object &o)
{
	return o.getBodyModule()->getHealth();
}

// the module's HealingDelay in logic frames as the INI parse made it (data + 0x140; INI parseDurationUnsignedInt = ceil(ms * 5 / 1000), so HealingDelay 1000 is
// exactly 5 frames: LOGICFRAMES_PER_SECOND is 5, not 30)
const AutoHealBehaviorModuleData *dataOf(Object &o)
{
	const AutoHealBehaviorModuleData *data = nullptr;
	for (const auto &nugget : static_cast<const ThingTemplate *>(o.getTemplate())->behaviorModules().nuggets())
	{
		if (const auto *healData = dynamic_cast<const AutoHealBehaviorModuleData *>(nugget.data.get()))
		{
			data = healData;
		}
	}
	REQUIRE(data);
	return data;
}

} // namespace

TEST_SUITE_BEGIN("PORT-293");

// RW 0x8C3897 / 0x8C30A9 .. 0x8C30BA / 0x5DC48C: the stamp is written by a healing hit only, with the hit's frame, and slot 0x48 answers it
TEST_CASE("PORT-293 body: a healing hit stamps the body's last-healing frame (RW 0x8C30BA) and body slot 0x48 answers it (RW 0x5DC48C)")
{
	Port293World f;
	Object *target = f.unit("Port293Target", 'A', 0.0f, 0.0f);
	REQUIRE(target);
	BodyModuleInterface *body = target->getBodyModule();
	REQUIRE(body);
	CHECK(body->getLastHealingFrame() == 0xFFFFFFFFu); // RW 0x8C3897: never healed
	wound(*target);
	CHECK(body->getLastHealingFrame() == 0xFFFFFFFFu); // the damaging record writes + 0xAC, never + 0xB0
	f.frames(3);
	const UnsignedInt hitFrame = f.logic->getFrame();
	REQUIRE(hitFrame != 0xFFFFFFFFu);
	DamageInfo heal;
	heal.m_input.m_amount = 10.0f;
	heal.m_input.m_damageType = DAMAGE_HEALING;
	body->attemptHealing(heal);
	CHECK(health(*target) == 410.0f);
	CHECK(body->getLastHealingFrame() == hitFrame); // RW 0x8C30BA stores TheGameLogic's frame
	CHECK(heal.m_output.m_actualDamageDealt == 10.0f);
	f.frames(4);
	const UnsignedInt laterFrame = f.logic->getFrame();
	CHECK(laterFrame == hitFrame + 4);
	body->attemptHealing(heal);
	CHECK(health(*target) == 420.0f);
	CHECK(body->getLastHealingFrame() == laterFrame); // every applied hit moves it
	// a hit the body clips to zero stamps nothing (the amount > 0 gate of RW 0x8C3044 comes before the store of RW 0x8C30BA)
	const UnsignedInt beforeZero = body->getLastHealingFrame();
	DamageInfo zero;
	zero.m_input.m_amount = 0.0f;
	zero.m_input.m_damageType = DAMAGE_HEALING;
	body->attemptHealing(zero);
	CHECK(health(*target) == 420.0f);
	CHECK(body->getLastHealingFrame() == beforeZero);
}

// RW 0x48DACD (InactiveBody's table RW 0xC718C8 + 0x48): a body class without the writer answers the never-healed -1, and a damaging hit is routed to
// attemptDamage by RW 0x8C2FF8 rather than stamped
TEST_CASE("PORT-293 body: a body class without the timestamp answers -1 (RW 0x48DACD) and a damaging hit never stamps one (RW 0x8C2FF8)")
{
	Port293World f;
	Object *prop = f.unit("Port293Prop", 'A', 0.0f, 0.0f);
	Object *target = f.unit("Port293Target", 'A', 60.0f, 0.0f);
	REQUIRE(prop);
	REQUIRE(prop->getBodyModule());
	REQUIRE(target);
	CHECK(prop->getBodyModule()->getLastHealingFrame() == 0xFFFFFFFFu);
	DamageInfo hit;
	hit.m_input.m_amount = 100.0f;
	hit.m_input.m_damageType = DAMAGE_UNRESISTABLE;
	target->getBodyModule()->attemptHealing(hit); // a non-HEALING hit goes to attemptDamage (RW 0x8C2FF0..0x8C2FF8)
	CHECK(health(*target) == 400.0f);
	CHECK(target->getBodyModule()->getLastHealingFrame() == 0xFFFFFFFFu);
}

// RW 0x855777 .. 0x8557A2: the NonStackable rule -- the target's stamp plus HealingDelay guard the second hit, a module without NonStackable has no gate
TEST_CASE("PORT-293 AutoHealBehavior: NonStackable refuses a heal while the target's stamp + HealingDelay is in the future (RW 0x855797 .. 0x8557A2)")
{
	Port293World f;
	Object *stackHealer = f.unit("Port293StackHealer", 'A', 0.0f, 0.0f);
	Object *healer = f.unit("Port293Healer", 'A', 0.0f, 60.0f);
	Object *target = f.unit("Port293Target", 'A', 60.0f, 0.0f);
	REQUIRE(stackHealer);
	REQUIRE(healer);
	REQUIRE(target);
	AutoHealBehavior *stack = f.healOf(stackHealer);
	AutoHealBehavior *guarded = f.healOf(healer);
	wound(*target);
	// no NonStackable: two hits in one frame both land (RW 0x855777 is the only reader of the flag in healOne)
	Port293HealAccess::healOne(*stack, *target, false);
	CHECK(health(*target) == 410.0f);
	Port293HealAccess::healOne(*stack, *target, false);
	CHECK(health(*target) == 420.0f);
	// NonStackable: the target's stamp is the one the other module left (stamp + delay > frame), so the hit is refused and nothing is written
	const UnsignedInt delay = dataOf(*healer)->m_healingDelay;
	CHECK(delay == 5); // HealingDelay 1000
	const UnsignedInt healedAt = target->getBodyModule()->getLastHealingFrame();
	CHECK(healedAt == 0); // the two hits above landed in the world's first frame
	CHECK(f.logic->getFrame() == 0);
	Port293HealAccess::healOne(*guarded, *target, false);
	CHECK(health(*target) == 420.0f);
	CHECK(target->getBodyModule()->getLastHealingFrame() == healedAt); // a refusal writes nothing
	f.frames((int)(healedAt + delay - 1 - f.logic->getFrame()));       // one frame short of the delay: still refused
	Port293HealAccess::healOne(*guarded, *target, false);
	CHECK(health(*target) == 420.0f);
	f.frames(1); // healedAt + delay: the sum is no longer greater than the frame
	Port293HealAccess::healOne(*guarded, *target, false);
	CHECK(health(*target) == 430.0f);
	CHECK(target->getBodyModule()->getLastHealingFrame() == healedAt + delay);
}

// RW 0x85579A / 0x8557A2: the sum is unsigned, so a never-healed body (-1, RW 0x8C3897) refuses a NonStackable heal until HealingDelay - 1 frames of the
// game have passed -- the first heal of a 1000 ms module at frame 0 needs frame 29. Holds the arithmetic of the guard (a signed sum would allow it at once)
TEST_CASE("PORT-293 AutoHealBehavior: a never-healed target refuses NonStackable until HealingDelay - 1 frames (RW 0x85579A, unsigned sum)")
{
	Port293World f;
	Object *healer = f.unit("Port293Healer", 'A', 0.0f, 0.0f);
	Object *target = f.unit("Port293Target", 'A', 60.0f, 0.0f);
	REQUIRE(healer);
	REQUIRE(target);
	REQUIRE(f.logic->getFrame() == 0);
	AutoHealBehavior *guarded = f.healOf(healer);
	wound(*target);
	REQUIRE(target->getBodyModule()->getLastHealingFrame() == 0xFFFFFFFFu);
	const UnsignedInt delay = dataOf(*healer)->m_healingDelay;
	CHECK(delay == 5); // HealingDelay 1000
	Port293HealAccess::healOne(*guarded, *target, false);
	CHECK(health(*target) == 400.0f); // 0xFFFFFFFF + delay = delay - 1 > 0
	f.frames((int)(delay - 2));
	Port293HealAccess::healOne(*guarded, *target, false);
	CHECK(health(*target) == 400.0f); // delay - 1 > delay - 2
	f.frames(1);
	Port293HealAccess::healOne(*guarded, *target, false);
	CHECK(health(*target) == 410.0f); // delay - 1 > delay - 1 is false
	CHECK(target->getBodyModule()->getLastHealingFrame() == delay - 1);
}

TEST_SUITE_END();
