// OpenBFME. GPL-3.0.
// See GameLogic/AI/AIGroupOrders.h.

#include "GameLogic/AI/AIGroupOrders.h"

#include "Common/StateHash.h"
#include "GameLogic/AI/AICommands.h"
#include "GameLogic/AI/GarrisonCommands.h"
#include "GameLogic/AI/AIGroup.h"
#include "GameLogic/AI/AIWorld.h"
#include "GameLogic/AI/AIXfer.h"
#include "GameLogic/Combat/CombatQueries.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/SimMath.h"
#include "GameClient/FormationAssistant.h"
#include "GameLogic/AI/AIGarrisonStates.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/Object/RetailObjectWorld.h"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace
{
const float kCloseEnoughSquared = 20.0f; // RW 0xBDBC6C
}

MoveToGroupOrder::MoveToGroupOrder(GameLogic &logic, std::vector<ObjectID> objects, const Coord3D &destination, bool attackMove)
	: GroupOrder(std::move(objects))
	, m_logic(logic)
	, m_destination(destination)
	, m_attackMove(attackMove)
{
}

bool MoveToGroupOrder::closeEnough(const Object &object, const Coord3D &target)
{
	if (object.isEffectivelyDead())
	{
		return true; // RW 0x94DB3A: Object + 0x458 bit 0 (taken as the effectively dead flag, as AIUpdateHordeMember does)
	}
	// RW 0x6CA525 is RW 0x6634BF with one radius: the same x87 arithmetic, the object's own bounding circle (+ 0xB8) taken off the distance
	const float d2 = CombatQueries::edgeDistanceSquared2D(*object.getPosition(), CombatQueries::boundingCircleRadius(object), target, 0.0f);
	return kCloseEnoughSquared > d2;
}

bool MoveToGroupOrder::isNearDestination(const Coord3D &pos) const
{
	float dx = pos.x - m_destination.x;
	float dy = pos.y - m_destination.y;
	return (dx * dx + dy * dy) < 200.0f; // RW 0xBE4170
}

void MoveToGroupOrder::activate(ObjectID id)
{
	Object *object = m_logic.findObjectByID(id);
	if (!object || object->isDestroyed())
	{
		return; // RW 0x94E1CB: no object
	}
	AIUpdateInterface *ai = object->getAIUpdateInterface();
	if (!ai || ai->isContained())
	{
		return; // RW 0x94E1D1 / 0x94E1DA: contained (RW 0x6939DF) or no AI (+ 0x260): nothing
	}
	// lane MODULES-3 r2: the order passes aiDoCommand's gate (RW 0x667174) before anything of the unit changes (the destination reserves a goal)
	if (!ai->acceptCommand(CMD_FROM_PLAYER, 0))
	{
		return;
	}
	Coord3D dest;
	const auto cached = m_positions.find(id);
	if (cached != m_positions.end())
	{
		dest = cached->second; // RW 0x94E1B2
	}
	else
	{
		dest = AIGroup::planningDestination(m_logic, *object, m_destination); // RW 0x94DF97 (rva005476C8): kept for the completion test
		m_positions.emplace(id, dest);
	}
	ai->aiMoveToPosition(dest, CMD_FROM_PLAYER); // RW 0x94DF8C: RW 0x66C4CA(pos, 0)
	if (m_attackMove)
	{
		ai->armAttackMove(ai->stateMachine().goalPosition()); // RW 0x94DF82: the attack-move RW 0x696266(pos, 0x7FFFFFFF, 0), the port's attack-move (S-328)
	}
}

bool MoveToGroupOrder::complete(ObjectID id)
{
	Object *object = m_logic.findObjectByID(id);
	if (!object)
	{
		return true; // RW 0x94DC52
	}
	AIUpdateInterface *ai = object->getAIUpdateInterface();
	if (!ai || ai->isContained())
	{
		return true; // RW 0x94DC58 / 0x94DC61
	}
	const auto cached = m_positions.find(id);
	if (cached == m_positions.end())
	{
		m_stuck = 0; // RW 0x94DC95
		return true;
	}
	if (closeEnough(*object, cached->second))
	{
		m_stuck = 0;
		return true;
	}
	if (ai->isIdle()) // RW 0x94DCA6: AI vslot 0x1B8
	{
		return ++m_stuck > 1; // RW 0x94DCB0: done on the second count
	}
	m_stuck = 0; // RW 0x94DCBF
	return false;
}

std::unique_ptr<AIOrdersManager::GroupOrder> MoveToGroupOrder::clone() const
{
	// the C++ copy (BFME2's MoveToGroupOrder copy constructor was not read); only a patrol reaches it and no live command starts one (S-1831)
	auto copy = std::make_unique<MoveToGroupOrder>(m_logic, objects(), m_destination, m_attackMove);
	copy->m_positions = m_positions;
	copy->m_stuck = m_stuck;
	return copy;
}

void MoveToGroupOrder::setState(const State &state)
{
	m_destination = state.destination;
	m_attackMove = state.attackMove;
	m_positions = state.positions;
	m_stuck = state.stuck;
}


AttackObjectGroupOrder::AttackObjectGroupOrder(GameLogic &logic, std::vector<ObjectID> objects, ObjectID targetID)
	: GroupOrder(std::move(objects))
	, m_logic(logic)
	, m_targetID(targetID)
{
}

void AttackObjectGroupOrder::activate(ObjectID id)
{
	Object *victim = m_logic.findObjectByID(m_targetID);
	if (!victim || victim->isEffectivelyDead())
	{
		m_completed = true;
		return;
	}
	if (victim->getPosition())
	{
		m_lastTargetPos = *victim->getPosition();
	}
	Object *obj = m_logic.findObjectByID(id);
	if (!obj || obj->isDestroyed())
	{
		return;
	}
	AIUpdateInterface *ai = obj->getAIUpdateInterface();
	if (!ai || ai->isContained())
	{
		return;
	}
	Object *target = victim;
	if (obj->isKindOf((unsigned)CombatNames::kinds().horde) && victim->getContainedBy() && victim->getContainedBy()->getContain() &&
		victim->getContainedBy()->getContain()->getHordeContainInterface())
	{
		target = victim->getContainedBy();
	}
	if (ai->aiAttackObject(target, CMD_FROM_PLAYER))
	{
		AICommands::recordAttackOrderAccepted();
	}
}

bool AttackObjectGroupOrder::complete(ObjectID id)
{
	(void)id;
	if (m_completed)
	{
		return true;
	}
	Object *victim = m_logic.findObjectByID(m_targetID);
	if (!victim || victim->isEffectivelyDead())
	{
		m_completed = true;
		return true;
	}
	if (victim->getPosition())
	{
		m_lastTargetPos = *victim->getPosition();
	}
	return false;
}

std::unique_ptr<AIOrdersManager::GroupOrder> AttackObjectGroupOrder::clone() const
{
	auto copy = std::make_unique<AttackObjectGroupOrder>(m_logic, objects(), m_targetID);
	copy->m_completed = m_completed;
	copy->m_lastTargetPos = m_lastTargetPos;
	return copy;
}

void AttackObjectGroupOrder::setState(const State &state)
{
	m_targetID = state.targetID;
	m_completed = state.completed;
	m_lastTargetPos = state.lastTargetPos;
}

GarrisonObjectGroupOrder::GarrisonObjectGroupOrder(GameLogic &logic, std::vector<ObjectID> objects, ObjectID targetID)
	: GroupOrder(std::move(objects))
	, m_logic(logic)
	, m_targetID(targetID)
{
}

void GarrisonObjectGroupOrder::activate(ObjectID id)
{
	Object *target = m_logic.findObjectByID(m_targetID);
	if (!target || target->isEffectivelyDead() || target->isDestroyed())
	{
		m_completed = true;
		return;
	}
	if (target->getPosition())
	{
		m_lastTargetPos = *target->getPosition();
	}
	Object *obj = m_logic.findObjectByID(id);
	if (!obj || obj->isDestroyed())
	{
		return;
	}
	AIUpdateInterface *ai = obj->getAIUpdateInterface();
	if (!ai || ai->isContained())
	{
		return;
	}
	if (ai->aiEnter(target, CMD_FROM_PLAYER))
	{
		GarrisonCommands::recordEnterOrderAccepted();
	}
}

bool GarrisonObjectGroupOrder::complete(ObjectID id)
{
	if (m_completed)
	{
		return true;
	}
	Object *target = m_logic.findObjectByID(m_targetID);
	if (!target || target->isEffectivelyDead() || target->isDestroyed())
	{
		m_completed = true;
		return true;
	}
	if (target->getPosition())
	{
		m_lastTargetPos = *target->getPosition();
	}
	Object *obj = m_logic.findObjectByID(id);
	if (!obj || obj->isDestroyed())
	{
		return true;
	}
	for (const Object *c = obj; c; c = c->getContainedBy())
	{
		if (c->getID() == m_targetID)
		{
			return true;
		}
	}
	AIUpdateInterface *ai = obj->getAIUpdateInterface();
	if (!ai)
	{
		return true;
	}
	if (ai->isIdle())
	{
		if (!GarrisonRules::canEnterObject(*obj, target, CMD_FROM_PLAYER, 0, true))
		{
			return true;
		}
		ai->aiEnter(target, CMD_FROM_PLAYER);
	}
	return false;
}
std::unique_ptr<AIOrdersManager::GroupOrder> GarrisonObjectGroupOrder::clone() const
{
	auto copy = std::make_unique<GarrisonObjectGroupOrder>(m_logic, objects(), m_targetID);
	copy->m_completed = m_completed;
	copy->m_lastTargetPos = m_lastTargetPos;
	return copy;
}

void GarrisonObjectGroupOrder::setState(const State &state)
{
	m_targetID = state.targetID;
	m_completed = state.completed;
	m_lastTargetPos = state.lastTargetPos;
}

MoveToFormationGroupOrder::MoveToFormationGroupOrder(GameLogic &logic, std::vector<ObjectID> objects, int formationType, const Coord3D &destination, float angle, bool attackMove, std::map<ObjectID, Coord3D> positions)
	: GroupOrder(std::move(objects))
	, m_logic(logic)
	, m_formationType(formationType)
	, m_destination(destination)
	, m_angle(angle)
	, m_attackMove(attackMove)
	, m_positions(std::move(positions))
{
}

void MoveToFormationGroupOrder::activate(ObjectID id)
{
	if (m_positions.empty())
	{
		const auto *world = RetailObjectWorld::currentContext();
		const FormationAssistant *assistant = world ? &world->formations() : nullptr;
		if (assistant)
		{
			const auto slots = assistant->layout(m_logic, objects(), m_formationType, m_destination);
			const float c = SimMath::cosDet(SimMath::addf32(m_angle, 1.5707964f));
			const float s = SimMath::sinDet(SimMath::addf32(m_angle, 1.5707964f));
			for (const auto &slot : slots)
			{
				Coord3D pos{
					SimMath::subf32(SimMath::addf32(m_destination.x, SimMath::mulf32(slot.offset.x, c)), SimMath::mulf32(slot.offset.y, s)),
					SimMath::addf32(SimMath::addf32(m_destination.y, SimMath::mulf32(slot.offset.x, s)), SimMath::mulf32(slot.offset.y, c)),
					0.0f
				};
				if (const auto *object = m_logic.findObjectByID(slot.object))
				{
					const auto center = FormationAssistant::rankCenter(*object);
					const float facingCos = SimMath::cosDet(m_angle), facingSin = SimMath::sinDet(m_angle);
					pos.x = SimMath::subf32(pos.x, SimMath::subf32(SimMath::mulf32(center.x, facingCos), SimMath::mulf32(center.y, facingSin)));
					pos.y = SimMath::subf32(pos.y, SimMath::addf32(SimMath::mulf32(center.x, facingSin), SimMath::mulf32(center.y, facingCos)));
				}
				pos.z = m_logic.getGroundHeight(pos.x, pos.y);
				m_positions[slot.object] = pos;
			}
		}
		for (ObjectID memberId : objects())
		{
			if (m_positions.find(memberId) == m_positions.end())
			{
				if (Object *obj = m_logic.findObjectByID(memberId))
				{
					m_positions[memberId] = AIGroup::planningDestination(m_logic, *obj, m_destination);
				}
				else
				{
					m_positions[memberId] = m_destination;
				}
			}
		}
	}
	Object *obj = m_logic.findObjectByID(id);
	if (!obj || obj->isDestroyed())
	{
		return;
	}
	AIUpdateInterface *ai = obj->getAIUpdateInterface();
	if (!ai || ai->isContained())
	{
		return;
	}
	auto it = m_positions.find(id);
	Coord3D goal = (it != m_positions.end()) ? it->second : m_destination;
	Coord3D taken;
	if (ai->aiMoveToPositionAndOrientate(goal, m_angle, CMD_FROM_PLAYER, &taken))
	{
		if (m_attackMove)
		{
			ai->armAttackMove(taken, CMD_FROM_PLAYER);
		}
	}
}

bool MoveToFormationGroupOrder::complete(ObjectID id)
{
	if (m_completed)
	{
		return true;
	}
	Object *object = m_logic.findObjectByID(id);
	if (!object || object->isDestroyed())
	{
		return true;
	}
	AIUpdateInterface *ai = object->getAIUpdateInterface();
	if (!ai || ai->isContained())
	{
		return true;
	}
	if (m_positions.find(id) == m_positions.end())
	{
		return true;
	}
	for (const auto &[memberId, pos] : m_positions)
	{
		const Object *m = m_logic.findObjectByID(memberId);
		if (!m || m->isEffectivelyDead())
		{
			continue;
		}
		if (MoveToGroupOrder::closeEnough(*m, pos))
		{
			continue;
		}
		const AIUpdateInterface *mai = m->getAIUpdateInterface();
		if (!mai || mai->isIdle())
		{
			continue;
		}
		return false;
	}
	m_completed = true;
	return true;
}

bool MoveToFormationGroupOrder::isNearDestination(const Coord3D &pos) const
{
	float dx = pos.x - m_destination.x;
	float dy = pos.y - m_destination.y;
	return (dx * dx + dy * dy) < 200.0f; // RW 0xBE4170
}

std::unique_ptr<AIOrdersManager::GroupOrder> MoveToFormationGroupOrder::clone() const
{
	auto copy = std::make_unique<MoveToFormationGroupOrder>(m_logic, objects(), m_formationType, m_destination, m_angle, m_attackMove);
	copy->m_positions = m_positions;
	copy->m_completed = m_completed;
	copy->m_stuck = m_stuck;
	return copy;
}

void MoveToFormationGroupOrder::setState(const State &state)
{
	m_formationType = state.formationType;
	m_destination = state.destination;
	m_angle = state.angle;
	m_attackMove = state.attackMove;
	m_positions = state.positions;
	m_completed = state.completed;
	m_stuck = state.stuck;
}

void AIOrdersManager::setPatrolStartIfLoop(GameLogic &logic, const std::vector<ObjectId> &objects, const Coord3D &dest, unsigned flags)
{
	for (ObjectId id : objects)
	{
		auto qIt = m_queues.find(id);
		if (qIt == m_queues.end() || qIt->second.orders().size() <= 1)
		{
			continue;
		}
		Object *obj = logic.findObjectByID(id);
		if (obj && obj->getPosition())
		{
			float dx = dest.x - obj->getPosition()->x;
			float dy = dest.y - obj->getPosition()->y;
			if ((dx * dx + dy * dy) < 200.0f) // RW 0xBE4170
			{
				setPatrolStart(id, qIt->second.orders().front(), flags);
				continue;
			}
		}
		for (OrderId orderId : qIt->second.orders())
		{
			if (orderId == qIt->second.orders().back())
			{
				break;
			}
			auto oIt = m_orders.find(orderId);
			if (oIt != m_orders.end() && oIt->second->isNearDestination(dest))
			{
				setPatrolStart(id, orderId, flags);
				break;
			}
		}
	}
}

namespace AIGroupOrders
{
namespace
{
const MoveToGroupOrder &moveOrder(const AIOrdersManager::GroupOrder &order)
{
	const auto *move = dynamic_cast<const MoveToGroupOrder *>(&order);
	if (!move)
	{
		throw std::logic_error("AIOrdersManager holds a group order class the native layout does not save (S-1831)");
	}
	return *move;
}

void xferState(Xfer &x, MoveToGroupOrder::State &s)
{
	XferVersion v{ 1, 1 };
	x.xferVersion(v);
	AIXfer::coord(x, s.destination);
	x.xferBool(&s.attackMove);
	AIXfer::map(x, s.positions, 16, [&](std::uint32_t &id, Coord3D &c) {
		x.xferUnsignedInt(&id);
		AIXfer::coord(x, c);
	});
	AIXfer::uns(x, s.stuck);
}

void xferState(Xfer &x, AttackObjectGroupOrder::State &s)
{
	XferVersion v{ 1, 1 };
	x.xferVersion(v);
	AIXfer::objectId(x, s.targetID, "attack order target");
	x.xferBool(&s.completed);
	AIXfer::coord(x, s.lastTargetPos);
}

void xferState(Xfer &x, GarrisonObjectGroupOrder::State &s)
{
	XferVersion v{ 1, 1 };
	x.xferVersion(v);
	AIXfer::objectId(x, s.targetID, "garrison order target");
	x.xferBool(&s.completed);
	AIXfer::coord(x, s.lastTargetPos);
}

void xferState(Xfer &x, MoveToFormationGroupOrder::State &s)
{
	XferVersion v{ 1, 1 };
	x.xferVersion(v);
	x.xferInt(&s.formationType);
	AIXfer::coord(x, s.destination);
	x.xferReal(&s.angle);
	x.xferBool(&s.attackMove);
	AIXfer::map(x, s.positions, 16, [&](std::uint32_t &id, Coord3D &c) {
		x.xferUnsignedInt(&id);
		AIXfer::coord(x, c);
	});
	x.xferBool(&s.completed);
	AIXfer::uns(x, s.stuck);
}
} // namespace

Image capture(const AIOrdersManager &manager)
{
	Image image;
	image.nextId = manager.nextId();
	for (const auto &kv : manager.orders())
	{
		OrderImage o;
		o.id = kv.first;
		o.clone = kv.second->cloneId();
		o.objects = kv.second->objects();
		if (const auto *move = dynamic_cast<const MoveToGroupOrder *>(kv.second.get()))
		{
			o.kind = OrderKind::Move;
			o.move.destination = move->destination();
			o.move.attackMove = move->attackMove();
			o.move.positions = move->positions();
			o.move.stuck = move->stuckCount();
		}
		else if (const auto *atk = dynamic_cast<const AttackObjectGroupOrder *>(kv.second.get()))
		{
			o.kind = OrderKind::Attack;
			o.attack.targetID = atk->targetID();
			o.attack.completed = atk->completed();
			o.attack.lastTargetPos = atk->lastTargetPos();
		}
		else if (const auto *gar = dynamic_cast<const GarrisonObjectGroupOrder *>(kv.second.get()))
		{
			o.kind = OrderKind::Garrison;
			o.garrison.targetID = gar->targetID();
			o.garrison.completed = gar->completed();
			o.garrison.lastTargetPos = gar->lastTargetPos();
		}
		else if (const auto *form = dynamic_cast<const MoveToFormationGroupOrder *>(kv.second.get()))
		{
			o.kind = OrderKind::Formation;
			o.formation.formationType = form->formationType();
			o.formation.destination = form->destination();
			o.formation.angle = form->angle();
			o.formation.attackMove = form->attackMove();
			o.formation.positions = form->positions();
			o.formation.completed = form->completed();
			o.formation.stuck = form->stuckCount();
		}
		else
		{
			throw std::logic_error("AIOrdersManager holds a group order class the native layout does not save (S-1831)");
		}
		image.orders.push_back(std::move(o));
	}
	for (const auto &kv : manager.queues())
	{
		image.queues.push_back(kv.second.state());
	}
	return image;
}

void crc(const AIOrdersManager &manager, StateHasher &h)
{
	h.addU32(manager.nextId());
	h.addU32((std::uint32_t)manager.orders().size());
	for (const auto &kv : manager.orders())
	{
		h.addU32(kv.first);
		h.addU32(kv.second->cloneId());
		h.addU32((std::uint32_t)kv.second->objects().size());
		for (AIOrdersManager::ObjectId id : kv.second->objects())
		{
			h.addU32(id);
		}
		if (const auto *move = dynamic_cast<const MoveToGroupOrder *>(kv.second.get()))
		{
			h.addFloat(move->destination().x);
			h.addFloat(move->destination().y);
			h.addFloat(move->destination().z);
			h.addBool(move->attackMove());
			h.addU32((std::uint32_t)move->positions().size());
			for (const auto &p : move->positions())
			{
				h.addU32(p.first);
				h.addFloat(p.second.x);
				h.addFloat(p.second.y);
				h.addFloat(p.second.z);
			}
			h.addU32(move->stuckCount());
		}
		else if (const auto *atk = dynamic_cast<const AttackObjectGroupOrder *>(kv.second.get()))
		{
			h.addU32(2);
			h.addU32(atk->targetID());
			h.addBool(atk->completed());
			h.addFloat(atk->lastTargetPos().x);
			h.addFloat(atk->lastTargetPos().y);
			h.addFloat(atk->lastTargetPos().z);
		}
		else if (const auto *gar = dynamic_cast<const GarrisonObjectGroupOrder *>(kv.second.get()))
		{
			h.addU32(3);
			h.addU32(gar->targetID());
			h.addBool(gar->completed());
			h.addFloat(gar->lastTargetPos().x);
			h.addFloat(gar->lastTargetPos().y);
			h.addFloat(gar->lastTargetPos().z);
		}
		else if (const auto *form = dynamic_cast<const MoveToFormationGroupOrder *>(kv.second.get()))
		{
			h.addU32(4);
			h.addU32((std::uint32_t)form->formationType());
			h.addFloat(form->destination().x);
			h.addFloat(form->destination().y);
			h.addFloat(form->destination().z);
			h.addFloat(form->angle());
			h.addBool(form->attackMove());
			h.addU32((std::uint32_t)form->positions().size());
			for (const auto &p : form->positions())
			{
				h.addU32(p.first);
				h.addFloat(p.second.x);
				h.addFloat(p.second.y);
				h.addFloat(p.second.z);
			}
			h.addBool(form->completed());
			h.addU32(form->stuckCount());
		}
	}
	h.addU32((std::uint32_t)manager.queues().size());
	for (const auto &kv : manager.queues())
	{
		const ObjectOrderQueue::State s = kv.second.state();
		h.addU32(s.object);
		h.addU32((std::uint32_t)s.orders.size());
		for (AIOrdersManager::OrderId id : s.orders)
		{
			h.addU32(id);
		}
		h.addU32(s.planned);
		h.addU32(s.patrolStart);
		h.addU32(s.nextPatrolStart);
	}
}

void xfer(Xfer &x, Image &image, GameLogic &logic)
{
	(void)logic;
	XferVersion v{ 1, 2 };
	x.xferVersion(v);
	AIXfer::uns(x, image.nextId);
	x.xferVectorSize(image.orders, 12);
	for (OrderImage &o : image.orders)
	{
		AIXfer::uns(x, o.id);
		AIXfer::uns(x, o.clone);
		AIXfer::ids(x, o.objects);
		if (x.isLoading())
		{
			for (std::uint32_t id : o.objects)
			{
				AIXfer::issued(id, "orders: an order's object id");
			}
		}
		if (v.current >= 2)
		{
			std::uint8_t k = static_cast<std::uint8_t>(o.kind);
			x.xferUnsignedByte(&k);
			o.kind = static_cast<OrderKind>(k);
		}
		else
		{
			o.kind = OrderKind::Move;
		}
		if (o.kind == OrderKind::Move)
		{
			xferState(x, o.move);
		}
		else if (o.kind == OrderKind::Attack)
		{
			xferState(x, o.attack);
		}
		else if (o.kind == OrderKind::Garrison)
		{
			xferState(x, o.garrison);
		}
		else if (o.kind == OrderKind::Formation)
		{
			xferState(x, o.formation);
		}
	}
	x.xferVectorSize(image.queues, 16);
	for (ObjectOrderQueue::State &s : image.queues)
	{
		AIXfer::objectId(x, s.object, "orders: a queue's object id");
		AIXfer::ids(x, s.orders);
		AIXfer::uns(x, s.planned);
		AIXfer::uns(x, s.patrolStart);
		AIXfer::uns(x, s.nextPatrolStart);
	}
}

void install(AIOrdersManager &manager, Image &&image, GameLogic &logic)
{
	for (const ObjectOrderQueue::State &s : image.queues)
	{
		if (!logic.findObjectByID(s.object))
		{
			throw XferException("orders: the queue of object " + std::to_string(s.object) + ", which the loaded game lacks");
		}
	}
	std::vector<AIOrdersManager::OrderRecord> records;
	for (OrderImage &o : image.orders)
	{
		for (AIOrdersManager::ObjectId id : o.objects)
		{
			if (!logic.findObjectByID(id))
			{
				throw XferException("orders: order " + std::to_string(o.id) + " names object " + std::to_string(id) + ", which the loaded game lacks");
			}
		}
		std::unique_ptr<AIOrdersManager::GroupOrder> order;
		if (o.kind == OrderKind::Move)
		{
			auto move = std::make_unique<MoveToGroupOrder>(logic, std::move(o.objects), o.move.destination, o.move.attackMove);
			move->setState(o.move);
			order = std::move(move);
		}
		else if (o.kind == OrderKind::Attack)
		{
			auto atk = std::make_unique<AttackObjectGroupOrder>(logic, std::move(o.objects), o.attack.targetID);
			atk->setState(o.attack);
			order = std::move(atk);
		}
		else if (o.kind == OrderKind::Garrison)
		{
			auto gar = std::make_unique<GarrisonObjectGroupOrder>(logic, std::move(o.objects), o.garrison.targetID);
			gar->setState(o.garrison);
			order = std::move(gar);
		}
		else if (o.kind == OrderKind::Formation)
		{
			auto form = std::make_unique<MoveToFormationGroupOrder>(logic, std::move(o.objects), o.formation.formationType, o.formation.destination, o.formation.angle, o.formation.attackMove);
			form->setState(o.formation);
			order = std::move(form);
		}
		else
		{
			throw XferException("orders: unknown order kind");
		}
		AIOrdersManager::OrderRecord r;
		r.id = o.id;
		r.clone = o.clone;
		r.order = std::move(order);
		records.push_back(std::move(r));
	}
	std::string why;
	if (!manager.restore(image.nextId, std::move(records), image.queues, &why))
	{
		throw XferException("orders: " + why);
	}
}
} // namespace AIGroupOrders
