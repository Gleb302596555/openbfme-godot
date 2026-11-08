// OpenBFME. GPL-3.0.
// The order lines (see OrderLines.h for the RotWK addresses).
#include "GameClient/OrderLines.h"

#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "GameLogic/AI/AIGroupOrders.h"
#include "GameLogic/AI/AIOrdersManager.h"
#include "GameLogic/AI/AIWorld.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object/Object.h"

#include <cmath>
#include <iterator>

namespace
{
bool ignoredInGui(const Object &obj)
{
	return obj.isKindOfName("IGNORED_IN_GUI"); // template + 0x10D bit 7 (RW 0x4E00E4, 0x4E0293)
}

// RW 0x693A1A(0): the object when it is a HORDE, else its container when that is a HORDE, else none
const Object *hordeOf(const Object &obj)
{
	if (obj.isKindOfName("HORDE")) // template + 0x114 bit 0x2000 (RW 0x693A23)
	{
		return &obj;
	}
	const Object *c = obj.getContainedBy(); // + 0x27C (RW 0x693A2F)
	if (c && c->isKindOfName("HORDE"))
	{
		return c;
	}
	return nullptr; // the argument 0 skips the third branch (RW 0x693A44)
}
} // namespace

int OrderLines::modeFor(const GameLogic &logic)
{
	// RW 0x69B1C3: ThePlayerList's local player (+ 0x10); its + 0x770 is 0, 1 or 2; anything else leaves the mode as it was (RW 0x69B1DF jne -> ret),
	// which the order mode never is (Player::setOrderMode takes MSG_CHANGE_ORDERMODE's 0 .. 2, RW 0x6AAC78)
	const Player *local = logic.players().getLocalPlayer();
	if (!local)
	{
		return 0;
	}
	const int m = local->orderMode();
	return (m == 1 || m == 2) ? m : 0;
}

void OrderLines::build(GameLogic &logic, const std::vector<ObjectID> &selected, ObjectID mousedOver, std::uint32_t milliseconds, OrderLinesFrame &out,
	std::uint32_t sinceLastDrawMs)
{
	out.strips.clear();
	out.markers.clear();
	// the cache lives with the orders: a new game starts it empty, and an order the manager no longer holds leaves it (order IDs are reused)
	if (m_logic != &logic)
	{
		m_targetCache.clear();
		m_logic = &logic;
	}
	const AIWorld *world = logic.aiWorld();
	for (auto it = m_targetCache.begin(); it != m_targetCache.end();)
	{
		it = (world && world->ordersManager().order(it->first)) ? std::next(it) : m_targetCache.erase(it);
	}
	out.mode = modeFor(logic);
	// RW 0x4E0175 .. 0x4E01B2: (ms % 1000) * 0.001 subtracted from 1 (RW 0xBD1908)
	// then seglinerenderer Render: + the rate * the ms since its last draw, each kept to its fraction (floorf)
	const float advance = kUVRatePerMs * static_cast<float>(sinceLastDrawMs);
	const float u = 0.0f + advance;
	const float v = (1.0f - static_cast<float>(milliseconds % 1000u) * 0.001f) + advance;
	out.runningU = u - std::floor(u);
	out.runningV = v - std::floor(v);
	if (out.mode == 0) // RW 0x4E015B
	{
		return;
	}
	Pass pass;
	pass.out = &out;
	const Player *local = logic.players().getLocalPlayer();
	// RW 0x4E0266 .. 0x4E02F3: the moused-over object the local player controls, its horde in its place, at width 3
	if (mousedOver != INVALID_ID)
	{
		Object *obj = logic.findObjectByID(mousedOver);
		if (obj && !ignoredInGui(*obj) && local && obj->getControllingPlayer() == local)
		{
			const Object *horde = hordeOf(*obj);
			pass.width = kHoverWidth;
			pass.opacity = 1.0f; // RW 0x4E02D2: fld1
			drawObject(logic, horde ? *horde : *obj, pass);
			pass.width = kWidth; // RW 0x4E02E7
		}
	}
	// RW 0x4E0318 .. 0x4E0352: every selected object at opacity 1 (RW 0x4E00D9 skips IGNORED_IN_GUI)
	pass.opacity = 1.0f;
	for (ObjectID id : selected)
	{
		const Object *obj = logic.findObjectByID(id);
		if (obj && !ignoredInGui(*obj))
		{
			drawObject(logic, *obj, pass);
		}
	}
	// RW 0x4E0354 .. 0x4E037F: mode 2 (planning; GameData's PlanningModeEnabled is never set in retail, S-1831) also every object of the local player
	if (out.mode == 2 && local)
	{
		pass.opacity = kOthersOpacity;
		for (Object *o = logic.getFirstObject(); o; o = o->getNextObject())
		{
			if (o->getControllingPlayer() == local && !ignoredInGui(*o))
			{
				drawObject(logic, *o, pass);
			}
		}
	}
}

void OrderLines::flush(Pass &pass, int style, std::vector<Coord3D> &points)
{
	// RW 0x4DFDB3: two points or more: tile factor 4, Set_Points, Render; then all but the last are dropped (RW 0x4DFE02 .. 0x4DFE10)
	if (points.size() <= 1)
	{
		return;
	}
	OrderLineStrip s;
	s.style = style;
	s.width = pass.width;
	s.opacity = pass.opacity;
	s.tileFactor = kTileFactor;
	s.points = points;
	pass.out->strips.push_back(std::move(s));
	points.erase(points.begin(), points.end() - 1);
}

bool OrderLines::orderPoint(GameLogic &logic, std::uint32_t orderId, Coord3D &out)
{
	const AIWorld *world = logic.aiWorld();
	const AIOrdersManager::GroupOrder *order = world ? world->ordersManager().order(orderId) : nullptr; // RW 0x756C59
	if (!order)
	{
		return false;
	}
	if (const auto *m = dynamic_cast<const MoveToGroupOrder *>(order))
	{
		out = m->destination(); // RW 0x94DA6C: + 0x18
		return true;
	}
	if (const auto *f = dynamic_cast<const MoveToFormationGroupOrder *>(order))
	{
		out = f->destination(); // RW 0x94E37D: + 0x1C
		return true;
	}
	ObjectID target = INVALID_ID;
	Coord3D seed{};
	if (const auto *a = dynamic_cast<const AttackObjectGroupOrder *>(order))
	{
		target = a->targetID(); // RW 0x94D93F: + 0x18 -> the cache + 0x20
		seed = a->lastTargetPos();
	}
	else if (const auto *g = dynamic_cast<const GarrisonObjectGroupOrder *>(order))
	{
		target = g->targetID(); // RW 0x94D64B: + 0x18 -> the cache + 0x1C
		seed = g->lastTargetPos();
	}
	else
	{
		return false; // every class AIGroupOrders saves is above; a new one must give its point here
	}
	auto it = m_targetCache.find(orderId);
	if (it == m_targetCache.end())
	{
		it = m_targetCache.emplace(orderId, seed).first;
	}
	// RW 0x94D945 .. 0x94D95E: TheGameLogic->findObjectByID (RW 0x449681); found -> its position (+ 0x38) into the cache
	if (const Object *t = logic.findObjectByID(target))
	{
		it->second = *t->getPosition();
	}
	out = it->second;
	return true;
}

void OrderLines::drawObject(GameLogic &logic, const Object &obj, Pass &pass)
{
	const AIWorld *world = logic.aiWorld();
	const ObjectOrderQueue *q = world ? world->ordersManager().queue(obj.getID()) : nullptr; // RW 0x756C70: obj + 0x74
	if (!q || q->orders().empty()) // RW 0x4DFE9D
	{
		return;
	}
	std::uint32_t prev = 0;                        // + 0x248 (RW 0x4DFEA5)
	std::vector<Coord3D> points{ *obj.getPosition() }; // RW 0x4DFEAC .. 0x4DFF00: + 0x38
	int style = 0;                                 // the line + 4 (RW 0x4DFF05)
	std::uint32_t loopStart = 0;                   // [ebp - 4]
	const std::uint32_t planned = q->planned(), patrolStart = q->patrolStart(), nextPatrolStart = q->nextPatrolStart(); // queue + 8 / + 0xC / + 0x10
	for (std::uint32_t id : q->orders())
	{
		if (id == planned) // RW 0x4DFF33
		{
			Coord3D p;
			if (loopStart && orderPoint(logic, loopStart, p)) // RW 0x4DFF38 .. 0x4DFF82: the loop closes on the patrol start's point
			{
				points.push_back(p);
			}
			loopStart = 0;
			flush(pass, style, points); // RW 0x4DFF90
			style = 1;
		}
		if (id == patrolStart) // RW 0x4DFF9E
		{
			loopStart = patrolStart;
			flush(pass, style, points); // RW 0x4DFFAB
			style = 2;
		}
		if (id == nextPatrolStart) // RW 0x4DFFB9
		{
			loopStart = patrolStart; // RW 0x4DFFBE .. 0x4DFFC6: [ebp - 0xC]
			flush(pass, style, points); // RW 0x4DFFC9
			style = 3;
		}
		Coord3D p;
		if (orderPoint(logic, id, p)) // RW 0x4DFFDD .. 0x4DFFF2
		{
			if (prev) // RW 0x4DFFF7
			{
				const auto key = std::make_pair(prev, id);
				if (pass.pairs.count(key)) // RW 0x4E0016: met in this frame
				{
					flush(pass, style, points); // RW 0x4E0035
					points.clear();             // RW 0x4E0045
				}
				else
				{
					pass.pairs.insert(key); // RW 0x4E0029
				}
			}
			points.push_back(p);                            // RW 0x4E0071
			pass.out->markers.push_back({ p, pass.opacity }); // RW 0x4E007E .. 0x4E0086: SCMoveHintSml at the point, the lines' opacity (RW 0x4DF983)
		}
		prev = id; // RW 0x4E009E
	}
	flush(pass, style, points); // RW 0x4E00B2 .. 0x4E00CD
}
