// OpenBFME. GPL-3.0.
// See GameLogic/Module/AnimalAIUpdate.h (lane MODGAP-2; editions: both RotWK 2.01 and BFME2 1.06).

#include "GameLogic/Module/AnimalAIUpdate.h"

#include "Common/INI.h"
#include "Common/RandomValue.h"
#include "Common/StateHash.h"
#include "Common/Thing/ModuleFactory.h"
#include "Common/Xfer.h"
#include "GameLogic/AI/AIXfer.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/Object/PartitionManager.h"
#include "GameLogic/ObjectTemplateInfo.h"
#include "Common/Player.h"
#include "GameLogic/SimMath.h"
#include "GameLogic/Module/UnportedModule.h"

namespace
{
// Coord3D::GetLengthEstimate, RW 0x4035AC
float lengthEstimate3D(float x, float y, float z)
{
	const auto absf = [](float a) { return a < 0.0f ? SimMath::subf32(0.0f, a) : a; };
	const auto estimate = [](float a, float b) { return a > b ? SimMath::addf32(a, SimMath::mulf32(b, 0.25f)) : SimMath::addf32(b, SimMath::mulf32(a, 0.25f)); };
	return estimate(estimate(absf(x), absf(y)), absf(z));
}

#define AIU_OFF(m) (int)offsetof(AnimalAIUpdateModuleData, m)
const FieldParse kAnimalAIUpdateFieldParse[] = {
	{ "FleeRange", &INI::parseReal, nullptr, AIU_OFF(m_fleeRange) },
	{ "FleeDistance", &INI::parseReal, nullptr, AIU_OFF(m_fleeDistance) },
	{ "WanderPercentage", &INI::parseInt, nullptr, AIU_OFF(m_wanderPercentage) },
	{ "MaxWanderDistance", &INI::parseReal, nullptr, AIU_OFF(m_maxWanderDistance) },
	{ "MaxWanderRadius", &INI::parseReal, nullptr, AIU_OFF(m_maxWanderRadius) },
	{ "AfraidOfCastles", &INI::parseBool, nullptr, AIU_OFF(m_afraidOfCastles) },
	{ "UpdateTimer", &INI::parseUnsignedInt, nullptr, AIU_OFF(m_updateTimer) },
	{ nullptr, nullptr, nullptr, 0 }
};
#undef AIU_OFF
} // namespace

void AnimalAIUpdateModuleData::buildFieldParse(MultiIniFieldParse &p)
{
	p.add(kAnimalAIUpdateFieldParse);
}

// RW 0x881DAA / BFME2 0x47ECA8
AnimalAIUpdate::AnimalAIUpdate(Thing *thing, const ModuleData *data)
	: AIUpdateInterface(thing, data)
	, m_data(dynamic_cast<const AnimalAIUpdateModuleData *>(data))
{
}

// RW 0x882058 / BFME2 0x47EDC9
UpdateSleepTime AnimalAIUpdate::update()
{
	const unsigned state = currentStateId();
	AIUpdateInterface::update();
	Object &obj = *getObject();
	if (!m_processed)
	{
		m_origin = *obj.getPosition();
		m_processed = true;
	}
	if (state != AI_IDLE)
	{
		return UPDATE_SLEEP_NONE;
	}

	GameLogic &logic = obj.logic();
	Coord3D dest = *obj.getPosition();

	// Enemy flee check (RW 0x882058)
	if (m_data && m_data->m_fleeRange > 0.0f)
	{
		PartitionFilterFn enemyFilter([&](Object &other) -> bool {
			if (&other == &obj || other.isEffectivelyDead())
			{
				return false;
			}
			Player *myOwner = obj.getControllingPlayer();
			if (myOwner)
			{
				return myOwner->getRelationship(other.getTeam()) == ENEMIES;
			}
			return other.getControllingPlayer() != nullptr;
		});
		Object *enemy = logic.partition().getClosestObject(dest, m_data->m_fleeRange, FROM_CENTER_3D, { &enemyFilter });
		if (enemy)
		{
			m_threatID = enemy->getID();
			m_returning = false;
			Coord3D fleeDir;
			fleeDir.x = SimMath::subf32(dest.x, enemy->getPosition()->x);
			fleeDir.y = SimMath::subf32(dest.y, enemy->getPosition()->y);
			fleeDir.z = 0.0f;
			float len = SimMath::length2d(fleeDir.x, fleeDir.y);
			if (len > 0.001f)
			{
				fleeDir.x = SimMath::divf32(fleeDir.x, len);
				fleeDir.y = SimMath::divf32(fleeDir.y, len);
			}
			else
			{
				fleeDir.x = 1.0f;
				fleeDir.y = 0.0f;
			}
			dest.x = SimMath::addf32(dest.x, SimMath::mulf32(fleeDir.x, m_data->m_fleeDistance));
			dest.y = SimMath::addf32(dest.y, SimMath::mulf32(fleeDir.y, m_data->m_fleeDistance));
			dest.z = 0.0f;
			chooseLocomotorSet(LOCOMOTORSET_PANIC);
			aiMoveToPosition(dest, CMD_FROM_AI);
			return UPDATE_SLEEP_NONE;
		}
	}
	m_threatID = 0;

	const int wanderPct = m_data ? m_data->m_wanderPercentage : 50;
	if (logic.random().getValue(0, 100, "AnimalAIUpdate.cpp", 0x9a) >= wanderPct)
	{
		return UPDATE_SLEEP_NONE;
	}

	static const int emitterKind = ObjectTemplateInfoBuilder::kindOfIndex("SALT_LICK");
	PartitionFilterFn emitterFilter([](Object &o) { return emitterKind >= 0 && o.isKindOf((unsigned)emitterKind); });
	const float scanRange = m_data ? m_data->m_fleeRange : 20.0f;
	Object *emitter = logic.partition().getClosestObject(dest, scanRange, FROM_CENTER_3D, { &emitterFilter });
	const Coord3D center = emitter ? *emitter->getPosition() : m_origin;
	const float dx = SimMath::subf32(center.x, dest.x), dy = SimMath::subf32(center.y, dest.y), dz = SimMath::subf32(center.z, dest.z);
	const float radius = m_data ? m_data->m_maxWanderRadius : 10.0f;
	const bool returnToCenter = emitter ? SimMath::sumSquares2(dx, dy) > SimMath::mulf32(radius, radius) : lengthEstimate3D(dx, dy, dz) > radius;
	chooseLocomotorSet(LOCOMOTORSET_NORMAL);
	if (returnToCenter)
	{
		dest = center;
		m_returning = true;
	}
	else
	{
		m_returning = false;
		const int angleLine = emitter ? 0xbc : 0xf2, distanceLine = emitter ? 0xbf : 0xf5;
		const float angle = (float)logic.random().getValue(-15, 15, "AnimalAIUpdate.cpp", angleLine);
		const float maxDist = m_data ? m_data->m_maxWanderDistance : 10.0f;
		const float distance = (float)logic.random().getValue(0, (int)maxDist, "AnimalAIUpdate.cpp", distanceLine);
		const Coord2D dir = locomotorHost().getUnitDirectionVector2D();
		dest.x = SimMath::pc24Add(SimMath::pc24Mul(SimMath::pc24Add(dir.x, SimMath::cosDet(angle)), distance), dest.x);
		dest.y = SimMath::pc24Add(SimMath::pc24Mul(SimMath::pc24Add(dir.y, SimMath::sinDet(angle)), distance), dest.y);
		dest.z = 0.0f;
	}
	aiMoveToPosition(dest, CMD_FROM_AI);
	return UPDATE_SLEEP_NONE;
}

// RW 0x881E3E / BFME2 0x47EBAF
void AnimalAIUpdate::xfer(Xfer &x)
{
	XferVersion v{ 1, 1 };
	x.xferVersion(v);
	x.xferObjectID(&m_threatID);
	x.xferBool(&m_processed);
	AIXfer::coord(x, m_origin);
	x.xferBool(&m_returning);
	AIUpdateInterface::xfer(x);
}

void AnimalAIUpdate::crc(StateHasher &h) const
{
	AIUpdateInterface::crc(h);
	h.addU32(m_threatID);
	h.addBool(m_processed);
	h.addFloat(m_origin.x);
	h.addFloat(m_origin.y);
	h.addFloat(m_origin.z);
	h.addBool(m_returning);
}

void AnimalAIUpdate::registerClass(ModuleFactory &modules)
{
	modules.bindTypedData<AnimalAIUpdateModuleData>("AnimalAIUpdate", MODULETYPE_BEHAVIOR);
	modules.bindModuleProc("AnimalAIUpdate", MODULETYPE_BEHAVIOR, [](Thing *thing, const ModuleData *data, const ModuleFactory::ModuleTemplate &info) -> std::unique_ptr<Module> {
		if (!thing->asObject() || !thing->asObject()->logic().aiWorld())
		{
			return makeUnportedModule(thing, data, info.name, info.type, info.interfaceMask);
		}
		return std::make_unique<AnimalAIUpdate>(thing, data);
	});
}

