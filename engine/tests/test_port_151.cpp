// OpenBFME. PORT-151 (fork issue 369): the unit voices an AI order asks for but nothing posted — the enter-state messages 0x7E6 / 0x7E7 / 0x7E8.
//
// TARGET FACTS (RotWK 2.01 game.dat, MD5 daed3668006cd90f01c34e5a7da1901f, caveat S-001), read by rw.py dis/xref; the full map is
// docs/re/rotwk/PORT151_UNIT_VOICES.md:
//   * the picker RW 0x8DEDBB answers 0x7E6 (attack), 0x7E7 (move), 0x7E8 (attack-move) through the low byte table RW 0x8E01CD (selectors 2 / 3 / 5) and the
//     jump table RW 0x8E0191; each body hands the pick info to the move / attack rules, whose enter-state rows are 0x1B .. 0x20 (RW 0x8DE883 .. 0x8DE8AD) and
//     0x16 .. 0x1A (RW 0x8DDF3B).
//   * the only producers of those three messages in the whole image are the AIUpdateInterface voice helpers, each with the owner's drawable list:
//       RW 0x66B1D0 (target object) -> 0x7E6, RW 0x66B24A push 0x7E6;
//       RW 0x66B271 (position)      -> 0x7E6, RW 0x66B2CE push 0x7E6;
//       RW 0x66B2F5 (position)      -> 0x7E8, RW 0x66B352 push 0x7E8;
//       RW 0x66B379 (position)      -> 0x7E7, RW 0x66B3D6 push 0x7E7.
//   * their callers, each behind "[ebp+0x10] == 0 || == 1" (CMD_FROM_PLAYER / CMD_FROM_SCRIPT):
//       privateMoveToPosition RW 0x66B3FD -> RW 0x66B4D2 (only the ordinary branch; the temporary AI state jumps to RW 0x66B4D7);
//       privateAttackObject RW 0x66BD33 -> RW 0x66BEB0 (tail RW 0x66BE77);
//       privateForceAttackObject RW 0x66BF4C -> RW 0x66C02F;
//       privateAttackPosition RW 0x66DA5F -> RW 0x66E060;
//       attack-move (command 0xF body RW 0x66C0B9, vtable slot RW 0xC105C8) -> RW 0x66C136;
//       attack-follow-waypoint (body RW 0x66C1C9, vtable slot RW 0xC10638) -> RW 0x66C1BC with the first waypoint's location (+ 0x38 at RW 0x66C1B6).
//   * the target-object helper also carries the victim's airborne locomotor as PickAndPlayInfo's air flag: RW 0x70E013 (Object::getDrawable) for the victim,
//     victim position + 0x38, RW 0x68BEED (AI + 0x260 -> current locomotor + 0x1F0 -> template surfaces + 0x14 bit 8 = AIR).
// GPL-3.0.

#include "doctest.h"

#include "CombatTestUtil.h"

#include "Common/Audio/AudioEntryPoints.h"
#include "GameClient/UnitVoiceResponse.h"
#include "GameLogic/AI/AIWorld.h"
#include "GameLogic/GameMessage.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Object/Object.h"

#include <string>
#include <vector>

namespace
{
// PORT-151: the sounds the logic's voice events ask for, as the HUD would hand them to the picker (the same wiring InGameHud installs)
struct Port151Voices
{
	struct Call
	{
		int event = 0;
		std::uint32_t objectId = 0;
		std::uint32_t targetId = 0;
		bool hasPosition = false;
		Coord3D position{ 0.0f, 0.0f, 0.0f };
		bool air = false;
	};
	std::vector<Call> calls;
	const void *owner = this;

	Port151Voices()
	{
		AudioApi::installUnitVoiceHandler([this](const AudioApi::UnitVoiceParams &p) {
			Call c;
			c.event = p.voiceEvent;
			c.objectId = p.objectId;
			c.targetId = p.targetId;
			c.hasPosition = p.hasPosition;
			c.position = Coord3D{ p.posX, p.posY, p.posZ };
			c.air = p.air;
			calls.push_back(c);
		}, this);
	}
	~Port151Voices() { AudioApi::uninstallUnitVoiceHandler(this); }
	const Call *last(int event) const
	{
		for (auto it = calls.rbegin(); it != calls.rend(); ++it)
		{
			if (it->event == event)
			{
				return &*it;
			}
		}
		return nullptr;
	}
	int count(int event) const
	{
		int n = 0;
		for (const Call &c : calls)
		{
			if (c.event == event)
			{
				++n;
			}
		}
		return n;
	}
};

// a flying enemy: a WINGS locomotor, the template surface the air flag reads (RW 0x68BEED)
const char kPort151Objects[] =
	"Weapon Port151Sword\n"
	"  AttackRange = 20\n"
	"  MeleeWeapon = Yes\n"
	"  DelayBetweenShots = 1000\n"
	"  DamageNugget\n"
	"    Damage = 5\n"
	"    Radius = 0.0\n"
	"    DamageType = SLASH\n"
	"    DeathType = NORMAL\n"
	"  End\n"
	"End\n"
	"Object Port151Soldier\n"
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK\n"
	"  VisionRange = 300\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = PlainArmor\n"
	"  End\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY Port151Sword\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"    AutoAcquireEnemiesWhenIdle = No\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"End\n"
	"Object Port151Dummy\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = PlainArmor\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"    AutoAcquireEnemiesWhenIdle = No\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"End\n"
	"Object Port151Flyer\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = PlainArmor\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"    AutoAcquireEnemiesWhenIdle = No\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WingsLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"End\n";

const char kPort151WingsLoco[] =
	"Locomotor WingsLoco\n"
	"  Surfaces = AIR\n"
	"  TurnTime = 500\n"
	"  Acceleration = 510\n"
	"  Braking = 510\n"
	"  ZAxisBehavior = NO_Z_MOTIVE_FORCE\n"
	"  Appearance = WINGS\n"
	"  StickToGround = No\n"
	"  CloseEnoughDist = 1\n"
	"End\n";

const std::string kPort151Ini = std::string(kPort151Objects) + kPort151WingsLoco;

struct Port151World : combattest::CombatWorld
{
	Port151World() : combattest::CombatWorld(kPort151Ini.c_str()) { combat().setAutoAcquireEnabled(false); }
};
} // namespace

TEST_CASE("PORT-151 move: a player's or a script's move order asks the unit voice for 0x7E7 with the goal; an AI's own move is silent (RW 0x66B3FD -> RW 0x66B4D2)")
{
	Port151World w;
	Port151Voices voices;
	Object *unit = w.unit("Port151Soldier", 'A', 300, 300);
	REQUIRE(unit);
	w.frames(2);
	AIUpdateInterface *ai = unit->getAIUpdateInterface();
	REQUIRE(ai);
	const Coord3D goal{ 400.0f, 500.0f, 0.0f };
	ai->aiMoveToPosition(goal, CMD_FROM_PLAYER);
	REQUIRE(voices.last(UnitVoiceResponse::VOICE_EVENT_ENTER_STATE_MOVE));
	CHECK(voices.count(UnitVoiceResponse::VOICE_EVENT_ENTER_STATE_MOVE) == 1);
	CHECK(voices.last(UnitVoiceResponse::VOICE_EVENT_ENTER_STATE_MOVE)->objectId == unit->getID());
	CHECK(voices.last(UnitVoiceResponse::VOICE_EVENT_ENTER_STATE_MOVE)->hasPosition);
	// the goal the picker hands the client is the order's position (RW 0x66B4CD pushes [ebp+8], the private command's position)
	CHECK(voices.last(UnitVoiceResponse::VOICE_EVENT_ENTER_STATE_MOVE)->position.x == goal.x);
	CHECK(voices.last(UnitVoiceResponse::VOICE_EVENT_ENTER_STATE_MOVE)->position.y == goal.y);
	// the attack / attack-move helpers stay out of a move order
	CHECK(voices.count(UnitVoiceResponse::VOICE_EVENT_ENTER_STATE_ATTACK) == 0);
	CHECK(voices.count(UnitVoiceResponse::VOICE_EVENT_ENTER_STATE_ATTACKMOVE) == 0);

	// a script's move speaks too (source 1)
	voices.calls.clear();
	ai->aiMoveToPosition(Coord3D{ 350.0f, 350.0f, 0.0f }, CMD_FROM_SCRIPT);
	CHECK(voices.count(UnitVoiceResponse::VOICE_EVENT_ENTER_STATE_MOVE) == 1);

	// the AI's own move (source 2) says nothing: RW 0x66B4C4 tests the source against 0 and 1 only
	voices.calls.clear();
	ai->aiMoveToPosition(Coord3D{ 320.0f, 320.0f, 0.0f }, CMD_FROM_AI);
	CHECK(voices.count(UnitVoiceResponse::VOICE_EVENT_ENTER_STATE_MOVE) == 0);
}

TEST_CASE("PORT-151 attack: a player's attack order asks 0x7E6 with the victim and the victim's air flag; an AI's is silent (RW 0x66BE9F -> RW 0x66BEB0, RW 0x68BEED)")
{
	Port151World w;
	Port151Voices voices;
	Object *soldier = w.unit("Port151Soldier", 'A', 300, 300);
	Object *victim = w.unit("Port151Dummy", 'B', 400, 300);
	REQUIRE(soldier);
	REQUIRE(victim);
	w.frames(2);
	AIUpdateInterface *ai = soldier->getAIUpdateInterface();
	REQUIRE(ai);
	REQUIRE(ai->aiAttackObject(victim, CMD_FROM_PLAYER));
	const Port151Voices::Call *call = voices.last(UnitVoiceResponse::VOICE_EVENT_ENTER_STATE_ATTACK);
	REQUIRE(call);
	CHECK(voices.count(UnitVoiceResponse::VOICE_EVENT_ENTER_STATE_ATTACK) == 1);
	CHECK(call->objectId == soldier->getID());
	CHECK(call->targetId == victim->getID());
	CHECK_FALSE(call->air); // a ground victim: surfaces has no AIR bit

	// a refused order (no victim) says nothing
	voices.calls.clear();
	CHECK_FALSE(ai->aiAttackObject(nullptr, CMD_FROM_PLAYER));
	CHECK(voices.count(UnitVoiceResponse::VOICE_EVENT_ENTER_STATE_ATTACK) == 0);

	// the AI's own attack is silent
	REQUIRE(ai->aiAttackObject(victim, CMD_FROM_AI));
	CHECK(voices.count(UnitVoiceResponse::VOICE_EVENT_ENTER_STATE_ATTACK) == 0);

	// a flying victim puts the air flag in (RW 0x68BEED: template surfaces + 0x14 bit 8)
	Object *flyer = w.unit("Port151Flyer", 'B', 300, 400);
	REQUIRE(flyer);
	w.frames(2);
	voices.calls.clear();
	REQUIRE(ai->aiForceAttackObject(flyer, CMD_FROM_PLAYER)); // the force attack posts the same helper (RW 0x66C02F)
	call = voices.last(UnitVoiceResponse::VOICE_EVENT_ENTER_STATE_ATTACK);
	REQUIRE(call);
	CHECK(call->targetId == flyer->getID());
	CHECK(call->air);
}

TEST_CASE("PORT-151 attack-move: the attack-move of command 0xF asks 0x7E8 (RW 0x66C0B9 -> RW 0x66C136) and the ground attack asks 0x7E6 with its spot (RW 0x66DA5F -> RW 0x66E060)")
{
	Port151World w;
	Port151Voices voices;
	Object *soldier = w.unit("Port151Soldier", 'A', 300, 300);
	REQUIRE(soldier);
	w.frames(2);
	AIUpdateInterface *ai = soldier->getAIUpdateInterface();
	REQUIRE(ai);
	const Coord3D goal{ 450.0f, 450.0f, 0.0f };
	REQUIRE(ai->aiAttackMoveToPosition(goal, CMD_FROM_PLAYER));
	const Port151Voices::Call *call = voices.last(UnitVoiceResponse::VOICE_EVENT_ENTER_STATE_ATTACKMOVE);
	REQUIRE(call);
	CHECK(call->objectId == soldier->getID());
	CHECK(call->hasPosition);
	CHECK(call->position.x == goal.x);
	CHECK(call->position.y == goal.y);
	// command 0xF has one voice: RW 0x66C0B9 posts 0x7E8 only and never reaches the move body's 0x7E7 (RW 0x66B4D2)
	CHECK(voices.count(UnitVoiceResponse::VOICE_EVENT_ENTER_STATE_MOVE) == 0);

	// the AI's own attack-move is silent
	voices.calls.clear();
	REQUIRE(ai->aiAttackMoveToPosition(Coord3D{ 460.0f, 460.0f, 0.0f }, CMD_FROM_AI));
	CHECK(voices.count(UnitVoiceResponse::VOICE_EVENT_ENTER_STATE_ATTACKMOVE) == 0);

	// the 3-argument command 0xF body (RW 0x696266 -> RW 0x66C0B9) is the one the GUI button calls: 0x7E8 with its goal, no move voice
	voices.calls.clear();
	ai->aiAttackMoveToPosition(Coord3D{ 470.0f, 470.0f, 0.0f }, 3, CMD_FROM_PLAYER);
	REQUIRE(voices.last(UnitVoiceResponse::VOICE_EVENT_ENTER_STATE_ATTACKMOVE));
	CHECK(voices.count(UnitVoiceResponse::VOICE_EVENT_ENTER_STATE_MOVE) == 0);
	CHECK(voices.count(UnitVoiceResponse::VOICE_EVENT_ENTER_STATE_ATTACKMOVE) == 1);

	// the attack on the ground: the same picker message 0x7E6 with the spot and no target (RW 0x66B271)
	voices.calls.clear();
	const Coord3D spot{ 500.0f, 500.0f, 0.0f };
	REQUIRE(ai->aiAttackPosition(spot, 1, CMD_FROM_SCRIPT));
	call = voices.last(UnitVoiceResponse::VOICE_EVENT_ENTER_STATE_ATTACK);
	REQUIRE(call);
	CHECK(call->hasPosition);
	CHECK(call->position.x == spot.x);
	CHECK(call->position.y == spot.y);
	CHECK_FALSE(call->air);
}
