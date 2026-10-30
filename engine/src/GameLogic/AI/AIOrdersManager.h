// OpenBFME. GPL-3.0.
// Shared group orders and their per-object queues, for both editions.
// TARGET: RW 0x7572FA (addOrderToObjectQueues) and 0x94F19E (remove member),
// caveat S-001. BFME2 1.06: Rva 0x3557B7 (AiOrdersManager.cpp) and
// Rva 0x548984 (Rva00548984Order.cpp), Open-BFME-2 byte-matched rows.
// This core is not wired into live commands or native saves (S-281).
#pragma once

#include "GameLogic/AI/ObjectOrderQueue.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <utility>
#include <vector>

class AIOrdersManager final : private ObjectOrderQueue::Owner
{
public:
	using ObjectId = ObjectOrderQueue::ObjectId;
	using OrderId = ObjectOrderQueue::OrderId;

	class GroupOrder
	{
	public:
		explicit GroupOrder(std::vector<ObjectId> objects) : m_objects(std::move(objects)) {}
		virtual ~GroupOrder() = default;
		virtual void activate(ObjectId object) = 0;
		virtual bool complete(ObjectId object) = 0;
		virtual void removed(ObjectId object) = 0;
		virtual void preparePatrol(ObjectId object) = 0;
		virtual std::unique_ptr<GroupOrder> clone() const = 0;
		const std::vector<ObjectId> &objects() const { return m_objects; }

	private:
		friend class AIOrdersManager;
		std::vector<ObjectId> m_objects;
		OrderId m_clone = 0;
	};

	// Mode 0 replaces immediate orders, mode 1 queues and mode 2 plans.
	// The caller constructs the order with the current selection, not a later one.
	OrderId submit(int mode, std::unique_ptr<GroupOrder> order)
	{
		if (!order || mode < 0 || mode > 2) return 0;
		const OrderId id = allocateId();
		const std::vector<ObjectId> members = order->objects();
		m_orders.emplace(id, std::move(order));
		for (ObjectId object : members)
		{
			auto it = m_queues.try_emplace(object, object).first;
			it->second.clear(mode == 0 ? 1u : mode == 1 ? 2u : 0u, *this);
			it->second.insert(mode == 2 ? 2u : 1u, id, *this);
		}
		return id;
	}

	void process(ObjectId object)
	{
		auto it = m_queues.find(object);
		if (it != m_queues.end()) it->second.process(*this);
	}
	void clear(ObjectId object, unsigned flags)
	{
		auto it = m_queues.find(object);
		if (it != m_queues.end()) it->second.clear(flags, *this);
	}
	void commitPlanned(ObjectId object)
	{
		auto it = m_queues.find(object);
		if (it != m_queues.end()) it->second.commitPlanned(*this);
	}
	void setPatrolStart(ObjectId object, OrderId orderId, unsigned flags)
	{
		auto it = m_queues.find(object);
		if (it != m_queues.end()) it->second.setPatrolStart(orderId, flags, *this);
	}
	void discardLastPlanned(ObjectId object)
	{
		auto it = m_queues.find(object);
		if (it != m_queues.end()) it->second.discardLastPlanned();
	}
	const ObjectOrderQueue *queue(ObjectId object) const
	{
		auto it = m_queues.find(object);
		return it == m_queues.end() ? nullptr : &it->second;
	}
	const GroupOrder *order(OrderId id) const
	{
		auto it = m_orders.find(id);
		return it == m_orders.end() ? nullptr : it->second.get();
	}

private:
	OrderId allocateId()
	{
		if (++m_nextId == 0) ++m_nextId; // GroupOrder ctor skips the zero id
		return m_nextId;
	}
	void activate(OrderId id, ObjectId object) override
	{
		auto it = m_orders.find(id);
		if (it != m_orders.end()) it->second->activate(object);
	}
	bool complete(OrderId id, ObjectId object) override
	{
		auto it = m_orders.find(id);
		return it != m_orders.end() && it->second->complete(object);
	}
	void removed(OrderId id, ObjectId object, bool cancelled) override
	{
		auto it = m_orders.find(id);
		if (it == m_orders.end()) return;
		GroupOrder &order = *it->second;
		order.removed(object);
		order.m_objects.erase(std::remove(order.m_objects.begin(), order.m_objects.end(), object), order.m_objects.end());
		const OrderId clone = cancelled ? order.m_clone : 0;
		if (clone) removed(clone, object, true);
		if (order.m_objects.empty()) m_orders.erase(it);
	}
	OrderId cloneForPatrol(OrderId id) override
	{
		auto it = m_orders.find(id);
		if (it == m_orders.end()) return 0;
		if (it->second->m_clone) return it->second->m_clone;
		auto clone = it->second->clone();
		if (!clone) return 0;
		const OrderId cloneId = allocateId();
		clone->m_clone = 0;
		it->second->m_clone = cloneId;
		m_orders.emplace(cloneId, std::move(clone));
		return cloneId;
	}
	void preparePatrol(OrderId id, ObjectId object) override
	{
		auto it = m_orders.find(id);
		if (it != m_orders.end()) it->second->preparePatrol(object);
	}

	OrderId m_nextId = 0;
	std::map<OrderId, std::unique_ptr<GroupOrder>> m_orders;
	std::map<ObjectId, ObjectOrderQueue> m_queues;
};
