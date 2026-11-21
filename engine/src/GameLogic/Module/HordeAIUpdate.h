// OpenBFME. GPL-3.0.
//
// HordeAIUpdate (lane MOVE-1): the AI of a horde object (horde spec 2.11). The horde object is the thing that paths; its members are ordinary units the
// HordeContain's member pass steers (GameLogic/Object/Contain/HordeMemberPass.cpp).
//
// TARGET FACTS (RotWK game.dat, caveat S-001): RW 0x89E2D0, the twin of B1 HordeAIUpdate_update.cpp (RVA 0x2C4790): when the horde's AI is not moving and the
// formation was last refreshed more than 5 frames ago (BFME1 hard-codes 5; RotWK reads LOGICFRAMES_PER_SECOND), the contain's updateFormation(1) runs, which
// stamps the frame and sends the member pass round again: members that have been displaced walk back to their slots. The base AIUpdateInterface::update
// runs last.
// STANCE-1: the IS_LEAVING_FACTORY retirement (RW 0x89E38A..0x89E3B4) is ported. NOT PORTED (S-222): the saved rally/reform branch, the "ready" notification of idle hordes (iface 0x98) and the per-frame horde tick
// (iface 0xDC: the melee tick of M2).

#pragma once

#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/DockUpdate.h"

#include <vector>

struct MeleeBehaviorModuleData;
class GameLogic;

// COMBAT-1 (horde spec 2.7 / 2.8) + PORT-283 (issue #650): the horde object's AI (create RW 0x64E20D, ctor RW 0x89DFAE, dtor RW 0x89DDF0 / scalar RW 0x89E01F,
// getModuleKey RW 0x89DE21, getClassName RW 0x89DE66, loadPostProcess thunk RW 0x892C33 -> RW 0x667BEF, xfer RW 0x89DF93, vtables RW 0xC66D48 / 0xC66CE8,
// 0x4AC bytes; static slot stubs RW 0x7FEAC1 / 0x8BD372 / 0x8F8014 / 0x9188EB / 0x9B501B / 0x9F3A3C / 0x9F9E80). Also implements DockUpdateInterface
// (RW 0x89DE6C, + 0x4A8, vtable RW 0xC66CE8) so returning or newly assigned members dock onto their reserved formation slots and rejoin the contain list.
//
// WHAT IS INFERENCE (stop S-327): see CombatState::stops().
class HordeAIUpdate : public AIUpdateInterface, public DockUpdateInterface
{
public:
	HordeAIUpdate(Thing *thing, const ModuleData *data)
		: AIUpdateInterface(thing, data)
	{
	}

	// BehaviorModule: RW 0x89DE6C (`lea eax, [ecx+0x4a8]; ret` when `this` is non-null)
	DockUpdateInterface *getDockUpdateInterface() override { return this; }

	// ---- DockUpdateInterface (RW vtable 0xC66CE8, 20 slots, + 0x4A8) ----
	bool isClearToApproach(const Object &docker) const override;                       // + 0x00, RW 0x89E03B
	bool reserveApproachPosition(Object &docker, Coord3D *pos, int *index) override;   // + 0x04, RW 0x89E05C
	bool advanceApproachPosition(Object &, Coord3D *, int *) override { return true; } // + 0x08, RW 0x89DE7C
	bool isClearToEnter(const Object &) const override { return true; }                // + 0x0C, RW 0x9B501B
	bool isClearToAdvance(const Object &, int) const override { return false; }        // + 0x10, RW 0x8F8014
	void getEnterPosition(Object &docker, Coord3D *pos) override;                      // + 0x14, RW 0x89E0B5
	void getDockPosition(Object &docker, Coord3D *pos) override;                       // + 0x18, RW 0x9F9E80 (tail-calls slot + 0x14)
	void getExitPosition(Object &docker, Coord3D *pos) override;                       // + 0x1C, RW 0x9F9E80 (tail-calls slot + 0x14)
	void onApproachReached(Object &) override {}                                       // + 0x20, RW 0x9F3A3C
	void onEnterReached(Object &) override {}                                          // + 0x24, RW 0x9F3A3C
	void onDockReached(Object &) override {}                                           // + 0x28, RW 0x9F3A3C
	void onExitReached(Object &) override {}                                           // + 0x2C, RW 0x9F3A3C
	bool action(Object &docker, Object *drone) override;                               // + 0x30, RW 0x89E133
	void cancelDock(Object &) override {}                                              // + 0x34, RW 0x9F3A3C
	bool isDockOpen() const override { return true; }                                  // + 0x38, RW 0x8BD372
	void setDockOpen(bool) override {}                                                 // + 0x3C, RW 0x9F3A3C
	bool unknownSlot40() const override { return false; }                              // + 0x40, RW 0x7FEAC1
	void setDockCrippled(bool) override {}                                             // + 0x44, RW 0x9F3A3C
	bool isAllowPassthroughType() const override { return true; }                      // + 0x48, RW 0x8BD372
	bool isRallyPointAfterDockType() const override { return false; }                  // + 0x4C, RW 0x9188EB

	AttackMachineKind attackMachineKind() const override { return ATTACK_MACHINE_HORDE; }
	// PORT-283: HordeAIUpdate::aiDoCommand RW 0x89E1C0 .. 0x89E205: while in a porcupine formation (slot 0xF0), auto-toggles out on RW 0x89DE81 when slot 0x5C
	// permits, else drops commands matching RW 0x89DF11 before StancesBehavior and the base switch run
	bool filterAcceptedCommand(CommandSourceType source, int command) override;
	static bool commandTogglesPorcupine(int command, CommandSourceType source); // RW 0x89DE81 (table RW 0x89DEBD, jump RW 0x89DEAD)
	static bool commandBlockedInPorcupine(int command);                         // RW 0x89DF11 (table RW 0x89DF3F, jump RW 0x89DF33)
	// lane MOVE-2 r2: HordeAIUpdate::aiDoCommand RW 0x89E169: after the gate, a horde command of the hand-off list first ends its members' orders (HordeContain
	// slot 0x14, RW 0x87594C) unless the horde stands in a container; then the base command runs
	void commandAccepted(CommandSourceType source, int command, Object *target) override;
	// the command numbers RW 0x89E169's switch sends to slot 0x14 (0, 1, 3, 4, 6 .. 9, 0xB, 0xC, 0xE, 0xF, 0x17, 0x18, 0x24, 0x32 .. 0x34, 0x36, 0x38, 0x41,
	// 0x42, 0x4E) and the port's unidentified move variants (AIUpdateInterface::kCommandUnidentifiedMove, INFERENCE S-1501)
	static bool commandEndsMemberOrders(int command);
	std::unique_ptr<AIStateMachine> makeHordeAttackMachine(AIAttackState *att, int melee = -1, bool *madeMelee = nullptr) override;
	void crc(StateHasher &hasher) const override;
	void xferAIStateBody(class Xfer &x) override; // lane SAVE-4c (AIUpdateSnapshot.cpp)
	OBFME_MODULE_XFER(HordeAIUpdate) // lane SAVE-4e: RW 0x89DF93, version 1 and AIUpdateInterface's (its state: xferAIStateBody)

protected:
	void updateBeforeBase() override;                                                             // RW 0x89E2D0 .. 0x89E413
	void privateMoveToPosition(const Coord3D &pos, CommandSourceType source, bool postMoveVoice) override; // RW 0x89E0F6 (AI vslot 0x34)
	bool privateEnter(Object *container, CommandSourceType source) override;                      // RW 0x89E41F (AI vslot 0x5C)

public:

	// ---- melee ----
	// RW 0x9E6763 / 0x98FA8E (Amoeba isTargetReady, cached for 3 seconds): the fighting can start / goes on
	bool isMeleeTargetReady(Object &target);
	// RW 0x86D75E: the horde engages `target` (does nothing when it is the engaged one); every member's orders are cancelled, the formation freezes
	void beginMelee(Object &target);
	// RW 0x86C0F9: the fight is over; the survivors walk back to their slots
	void endMelee();
	// lane INTEG-1, RW 0x86C40D (HordeContain slot 0x260): the old MeleeBehavior object is destroyed and a new one made from the selected data; the port keeps the
	// behaviour's runtime here, so this resets it the way the replacement starts: its records are rebuilt for the members (runtime slot +4, Amoeba RW 0x990B5B: one
	// fresh record per member through RW 0x98F5A5: no destination, no history, timer 0, not idle, dirty) and the Amoeba constructor (RW 0x98FE62) sets newTarget
	// (+0x14) to 1. The engaged target and the readiness cache are the contain's and stay; no order is given and no random number drawn
	void resetMeleeRuntime(int kind);
	// RW 0x009902A1 Amoeba per-frame update: the members that are not fighting attack what is in reach or step toward the enemy
	void meleeTick(Object &target);
	// lane MOVE-2 r3 (S-1502): RW 0x870A1B, the melee part of the contain's update (RW 0x872FD8): the engaged target gone -> the melee ends (slot 0x138); else the
	// melee behaviour's update (slot 0x14: Amoeba RW 0x9902A1, HoldGround RW 0x98B7CC) and the horde turns to face the target (or its horde)
	void containMeleeUpdate();
	// lane MOVE-2 r3: the melee behaviour's slot 0x34, read by the contain's slot 7 (RW 0x877D89) while the horde melees: Amoeba RW 0x98F819 hands out the member
	// record's destination when its flag (+ 0x39) is set; HoldGround RW 0x8F8014 none (the member keeps its formation slot)
	bool meleeDestination(ObjectID member, Coord3D &out) const;
	// lane MOVE-2 r3: the move hub's melee turn test (RW 0x874FA6 .. with the behaviour's slots 0x1C / 0x20 / 0x24): the Amoeba's 0x1C and 0x24 answer true
	// (RW 0x9B501B), so a member at its melee destination always turns to the formation's facing and is never put on it; HoldGround's 0x1C is false
	bool meleeAlwaysTurns() const;
	ObjectID engagedTarget() const { return m_engagedTarget; }
	// ---- lane HORDE-2: what HordeMemberCollide (RW 0x8C0518) asks the horde of a member (RW HordeContainInterface slots; the port keeps the melee target id and the readiness
	// expiry here: RW +0x16C / +0x170 are the cache this class keeps as m_cacheTarget / m_cacheExpiry) ----
	// slot 0x16C: the melee target id (0 when none)
	ObjectID meleeTargetId() const { return m_cacheTarget; }
	// slot 0x168 RW 0x7BB0B2: + 0x16C = id (lane FLEECHASE-1: the writers are the melee wait state's onEnter / onExit and the approach state's update)
	void setMeleeTargetId(ObjectID id) { m_cacheTarget = id; }
	// slot 0x164 RW 0x86BE9A: `obj` or its horde (RW 0x693A1A(0)) is the melee target -> the readiness holds for 3 * LOGICFRAMES_PER_SECOND more frames
	void refreshMeleeReadiness(Object &obj);
	// slot 0x160 RW 0x870180: the readiness holds and `t` has the melee target's horde (RW 0x693A1A(0), null outside a horde: two nulls agree) or stands within
	// 100 (edge distance) of this horde. The cache lives on the horde object in both: RW in its contain (+ 0x16C / + 0x170), the port in its HordeAIUpdate, with
	// the same writers (setMeleeTargetID, the refresh, the end of the melee RW 0x86C0F9; lane FLEECHASE-1: the ready test RW 0x98FA8E writes none); lane DECOMP-1 closed S-586
	bool isInCurrentMelee(Object *t);
	// slot 0x15C RW 0x86D614: a member is attacking (AI slot 0x1BC) with IS_MELEE_ATTACKING
	bool hasMeleeAttackingMember() const;
	unsigned long long contactRefreshes() const { return m_contactRefreshes; }
	// lane HORDE-2: the stop line of the Squish (crush attack) state, S-583
	static const char *squishStopLine();
	static const char *amoebaStopLine();        // S-588
	static const char *approachStopLine();      // S-590
	// lane ARCHER-1: RW 0x69675C(member, 0), the touch attack of a member holding a melee weapon when its horde's HordeAttackNugget fires (RW 0x875350); the same
	// INFERENCE as HoldGround's (S-588): the enemy of `target` in the member's reach (RW 0x6F2956 rules) is attacked. true: an order was given
	bool touchAttack(Object &member, Object &target);

	struct MeleeStats
	{
		unsigned long long orders = 0;      ///< attack orders given to members
		unsigned long long steps = 0;       ///< one-cell steps ordered
		unsigned long long idleCycles = 0;  ///< members that went idle
	};
	const MeleeStats &meleeStats() const { return m_stats; }

	friend struct Horde2HashAccess; // the HORDE-2 hash mutation tests (test_horde2_hash.cpp)

private:
	struct MemberRecord
	{
		ObjectID id = 0;
		Coord3D dest{ 0.0f, 0.0f, 0.0f };
		bool hasDest = false;
		int historyCount = 0;
		int history[4][2] = {};
		unsigned timer = 0;
		bool idle = false;
		bool dirty = false; ///< RW record + 0xC
	};
	void pushHistory(MemberRecord &rec, int x, int y);    // RW 0x98F3A3
	bool amoebaIdleCycle(MemberRecord &rec, const MeleeBehaviorModuleData &amoeba, GameLogic &logic); // RW 0x990526 .. 0x9905A1
	void recentreOnMembers();                              // RW 0x86F18F
	void holdGroundTick(Object &target);                   // RW 0x98B7CC
	void swarmTick(Object &target);                        // RW 0x98C6A8 (not ported, S-588)
	bool m_newTarget = false;                              ///< RW MeleeBehavior + 0x14
	unsigned long long m_swarmTicks = 0;
	MemberRecord &recordOf(ObjectID id);
	std::vector<Object *> membersOf() const;
	ObjectID m_engagedTarget = 0;
	ObjectID m_cacheTarget = 0;
	unsigned m_cacheExpiry = 0;
	std::vector<MemberRecord> m_records;
	MeleeStats m_stats;
	unsigned long long m_contactRefreshes = 0; ///< HORDE-2: readiness refreshes by member contact

protected:
	const char *machineName() const override { return "HordeAIUpdateMachine"; }
};
