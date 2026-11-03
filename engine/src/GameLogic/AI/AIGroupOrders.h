// OpenBFME. GPL-3.0.
// The group orders AIWorld's AIOrdersManager queues. Both editions.
//
// MoveToGroupOrder, the order MSG_DO_MOVETO / MSG_DO_ATTACKMOVETO hand the group manager (RW 0x75748C -> the constructor RW 0x94E232, vtable RW 0xC81430):
//   * the fields: destination + 0x18, attack flag + 0x24, the flag + 0x25 (the message was 0x442, MSG_DO_SALVAGE here: never set by a live command), the
//     per-object destinations (hash_map ObjectID -> Coord3D, + 0x28) and the stuck counter (+ 0x3C);
//   * activation, slot 0x10 RW 0x94E184 (BFME2 MoveToGroupOrder::rva005478B5, Rva 0x5478B5): the object's cached destination or the one RW 0x94DF97 works out
//     (rva005476C8, AIGroup::planningDestination) and keeps; an object that is gone, contained or without an AI takes nothing; then RW 0x94DF1B (rva0054764C):
//     the attack flag picks the attack-move (RW 0x696266) or aiMoveToPosition (RW 0x66C4CA) from source CMD_FROM_PLAYER;
//   * completion, slot 0x14 RW 0x94DC37 (BFME2 rva00547368, Rva 0x547368): an object that is gone, contained or without an AI is done; with no cached destination it
//     is done and the counter is cleared; at the destination (RW 0x94DB32 -> 0x6CA525: the squared distance from the edge of the object's bounding circle
//     below 20.0, RW 0xBDBC6C; Rva 0x547263) it is done and the counter is cleared; otherwise an idle AI (vslot 0x1B8, isIdle) counts up and is done on
//     the second count, a busy one clears the counter;
//   * member removal, slot 0x18 RW 0x9F3A3C: `ret 4`, nothing.
// The ObjectOrderQueue / AIOrdersManager core is RW 0x94EBD1 .. 0x94F19E and 0x7572FA (BFME2 Rva 0x5482E9 .. 0x548984, 0x3557B7).
//
// NOT PORTED (stop S-1831): the other order classes (attack object, garrison, formation move ...), the patrol start (RW 0x7571A7 / 0x756F76 / 0x756D05:
// clone and preparePatrol are the queue core's contract and no live command reaches them), the SHIP / AMPHIBIOUS destination branch (RW 0x94DA57 ->
// 0x770D27), the object + 0x27C branch (RW 0x754B18, the physics object's move), the pending-command block between the two queue passes of the AI update
// (RW 0x66E5C3 .. 0x66E653), the planning commit (RW 0x6AAC85) and the callers that clear a queue (RW 0x77B0DD, 0x8760F9, 0x88C704; stop is ported).
// INFERENCE: RotWK keeps the queue of an object that left the world; AIWorld drops it (AIOrdersManager::forget), so a save names live objects only.

#pragma once

#include "GameLogic/AI/AIOrdersManager.h"
#include "GameLogic/ObjectTypes.h"

#include "Common/INIDataTypes.h"

#include <map>
#include <vector>

class GameLogic;
class Object;
class StateHasher;
class Xfer;

class MoveToGroupOrder final : public AIOrdersManager::GroupOrder
{
public:
	MoveToGroupOrder(GameLogic &logic, std::vector<ObjectID> objects, const Coord3D &destination, bool attackMove);

	void activate(ObjectID object) override;
	bool complete(ObjectID object) override;
	void removed(ObjectID) override {}
	void preparePatrol(ObjectID) override {}
	std::unique_ptr<GroupOrder> clone() const override;
	bool isNearDestination(const Coord3D &pos) const override;

	const Coord3D &destination() const { return m_destination; }
	bool attackMove() const { return m_attackMove; }
	const std::map<ObjectID, Coord3D> &positions() const { return m_positions; }
	unsigned stuckCount() const { return m_stuck; }

	// RW 0x94DB32 (BFME2 Rva00547263, Rva 0x547263): true for an object that is effectively dead or whose edge is within the 20.0 of the target
	static bool closeEnough(const Object &object, const Coord3D &target);

	// the saved fields of one order (AIGroupOrders::Image)
	struct State
	{
		Coord3D destination{};
		bool attackMove = false;
		std::map<ObjectID, Coord3D> positions;
		unsigned stuck = 0;
	};
	void setState(const State &state);

private:
	GameLogic &m_logic;
	Coord3D m_destination;
	bool m_attackMove;
	std::map<ObjectID, Coord3D> m_positions;
	unsigned m_stuck = 0;
};

// RW 0x94D79D / 0xC813D8 (BFME2 Rva 0x546ECE / 0x00C6A420): group attack on an object
class AttackObjectGroupOrder final : public AIOrdersManager::GroupOrder
{
public:
	AttackObjectGroupOrder(GameLogic &logic, std::vector<ObjectID> objects, ObjectID targetID);

	void activate(ObjectID object) override;
	bool complete(ObjectID object) override;
	void removed(ObjectID) override {}
	void preparePatrol(ObjectID) override {}
	std::unique_ptr<GroupOrder> clone() const override;

	ObjectID targetID() const { return m_targetID; }
	bool completed() const { return m_completed; }
	const Coord3D &lastTargetPos() const { return m_lastTargetPos; }

	struct State
	{
		ObjectID targetID = 0;
		bool completed = false;
		Coord3D lastTargetPos{};
	};
	void setState(const State &state);

private:
	GameLogic &m_logic;
	ObjectID m_targetID = 0;
	bool m_completed = false;
	Coord3D m_lastTargetPos{};
};

// RW 0x94D4F5 / 0xC8137C (BFME2 Rva 0x546E8A / 0x00C6A3EC): group enter/garrison into an object
class GarrisonObjectGroupOrder final : public AIOrdersManager::GroupOrder
{
public:
	GarrisonObjectGroupOrder(GameLogic &logic, std::vector<ObjectID> objects, ObjectID targetID);

	void activate(ObjectID object) override;
	bool complete(ObjectID object) override;
	void removed(ObjectID) override {}
	void preparePatrol(ObjectID) override {}
	std::unique_ptr<GroupOrder> clone() const override;

	ObjectID targetID() const { return m_targetID; }
	bool completed() const { return m_completed; }
	const Coord3D &lastTargetPos() const { return m_lastTargetPos; }

	struct State
	{
		ObjectID targetID = 0;
		bool completed = false;
		Coord3D lastTargetPos{};
	};
	void setState(const State &state);

private:
	GameLogic &m_logic;
	ObjectID m_targetID = 0;
	bool m_completed = false;
	Coord3D m_lastTargetPos{};
};

// RW 0x94E857 / 0xC81484 (BFME2 Rva 0x546EFE / 0x00C6A454): group move in formation
class MoveToFormationGroupOrder final : public AIOrdersManager::GroupOrder
{
public:
	MoveToFormationGroupOrder(GameLogic &logic, std::vector<ObjectID> objects, int formationType, const Coord3D &destination, float angle, bool attackMove, std::map<ObjectID, Coord3D> positions = {});

	void activate(ObjectID object) override;
	bool complete(ObjectID object) override;
	void removed(ObjectID) override {}
	void preparePatrol(ObjectID) override {}
	std::unique_ptr<GroupOrder> clone() const override;
	bool isNearDestination(const Coord3D &pos) const override;

	int formationType() const { return m_formationType; }
	const Coord3D &destination() const { return m_destination; }
	float angle() const { return m_angle; }
	bool attackMove() const { return m_attackMove; }
	const std::map<ObjectID, Coord3D> &positions() const { return m_positions; }
	bool completed() const { return m_completed; }
	unsigned stuckCount() const { return m_stuck; }

	struct State
	{
		int formationType = 0;
		Coord3D destination{};
		float angle = 0.0f;
		bool attackMove = false;
		std::map<ObjectID, Coord3D> positions;
		bool completed = false;
		unsigned stuck = 0;
	};
	void setState(const State &state);

private:
	GameLogic &m_logic;
	int m_formationType = 0;
	Coord3D m_destination{};
	float m_angle = 0.0f;
	bool m_attackMove = false;
	std::map<ObjectID, Coord3D> m_positions;
	bool m_completed = false;
	unsigned m_stuck = 0;
};

// The hash and the native save of the manager's state (stop S-4220: OpenBFME layout, not retail's CHUNK_AiOrdersManager). The save is one image: loading
// reads and checks it whole, install() then replaces the manager's state.
namespace AIGroupOrders
{
enum class OrderKind : std::uint8_t
{
	Move = 1,
	Attack = 2,
	Garrison = 3,
	Formation = 4,
};

struct OrderImage
{
	OrderKind kind = OrderKind::Move;
	AIOrdersManager::OrderId id = 0, clone = 0;
	std::vector<AIOrdersManager::ObjectId> objects;
	MoveToGroupOrder::State move;
	AttackObjectGroupOrder::State attack;
	GarrisonObjectGroupOrder::State garrison;
	MoveToFormationGroupOrder::State formation;
};
struct Image
{
	AIOrdersManager::OrderId nextId = 0;
	std::vector<OrderImage> orders;
	std::vector<ObjectOrderQueue::State> queues;
};
// the manager's state as an image; a group order class this layout does not hold is an error
Image capture(const AIOrdersManager &manager);
void crc(const AIOrdersManager &manager, StateHasher &hasher);
// saving writes `image`, loading fills it (every id checked against the issued ids, every order against the live game); nothing of the manager changes
void xfer(Xfer &x, Image &image, GameLogic &logic);
// replaces the manager's state with a loaded image (the manager refuses an inconsistent one: an XferException naming why)
void install(AIOrdersManager &manager, Image &&image, GameLogic &logic);
} // namespace AIGroupOrders
