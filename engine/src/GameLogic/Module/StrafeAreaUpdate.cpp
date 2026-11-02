// OpenBFME. GPL-3.0.
//
// Both editions: RW 0x008A3950, BFME2 0x0089F9F0. See StrafeAreaUpdate.h.

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif

#include "GameLogic/Module/StrafeAreaUpdate.h"

#include "Common/Thing/ModuleFactory.h"
#include "GameLogic/AI/AICommandSink.h"
#include "GameLogic/Combat/ObjectWeapons.h"
#include "GameLogic/Damage.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/WeaponStores.h"

#include <cmath>
#include <cstddef>
#include <memory>

namespace
{
#define STRAFE_ROW(name, parse, field) { name, parse, nullptr, (int)offsetof(StrafeAreaUpdateModuleData, field) }
const FieldParse kStrafeAreaUpdateFields[] = {
	STRAFE_ROW("WeaponName", INI::parseAsciiString, m_weaponName),
	STRAFE_ROW("StrafeAreaRadius", INI::parseReal, m_strafeAreaRadius),
	STRAFE_ROW("Sweepfrequency", INI::parseReal, m_sweepFrequency), // retail's spelling (RW table 0xC31968)
	STRAFE_ROW("SweepAmplitude", INI::parseReal, m_sweepAmplitude),
	STRAFE_ROW("Slope", INI::parseReal, m_slope),
	STRAFE_ROW("InitialSweepPhase", INI::parseReal, m_initialSweepPhase),
	{ nullptr, nullptr, nullptr, 0 }
};
#undef STRAFE_ROW

constexpr int kFiringConditionBit = 44; // FIRING_A in ModelConditionNames.inc (retail bit 37 / condition 37)
} // namespace

namespace StrafeAreaUpdateTables
{
const FieldParse *strafe()
{
	return kStrafeAreaUpdateFields;
}
} // namespace StrafeAreaUpdateTables

StrafeAreaUpdateModuleData::StrafeAreaUpdateModuleData()
	: m_weaponName("")
	, m_strafeAreaRadius(150.0f)
	, m_sweepFrequency(0.4f)
	, m_sweepAmplitude(100.0f)
	, m_slope(50.0f)
	, m_initialSweepPhase(0.0f)
{
}

void StrafeAreaUpdateModuleData::buildFieldParse(MultiIniFieldParse &p)
{
	p.add(kStrafeAreaUpdateFields);
}

StrafeAreaUpdate::StrafeAreaUpdate(Thing *thing, const ModuleData *moduleData)
	: UpdateModule(thing, moduleData), m_data(dynamic_cast<const StrafeAreaUpdateModuleData *>(moduleData))
{
	m_strafeTarget = Coord3D{};
	m_strafeStart = Coord3D{};
	m_sweepPhase = m_data ? m_data->m_initialSweepPhase : 0.0f;
	m_state = 0;
	m_started = false;
	m_nextFireFrame = 0;
}

const char *StrafeAreaUpdate::stopLine()
{
	return "[S-6026] StrafeAreaUpdate (RW 0x8A3950 / BFME2 0x89F9F0): strafe run AI logic, ground sweep and temp weapon dispatch; aircraft 3D spline trajectory under S-140";
}

bool StrafeAreaUpdate::initiateStrafe(const Coord3D *from, const Coord3D *to)
{
	Object *obj = getObject();
	if (!obj || !from || !to)
	{
		return false;
	}
	AIUpdateInterface *ai = obj->getAIUpdateInterface();
	if (!ai)
	{
		return false;
	}

	m_strafeStart = *from;
	m_strafeTarget = *to;
	m_sweepPhase = m_data ? m_data->m_initialSweepPhase : 0.0f;
	m_state = 0;
	m_started = false;

	Coord3D delta{ m_strafeTarget.x - m_strafeStart.x, m_strafeTarget.y - m_strafeStart.y, m_strafeTarget.z - m_strafeStart.z };
	float angle = std::atan2(delta.y, delta.x);
	obj->setOrientation(angle);

	ai->aiMoveToPosition(m_strafeTarget, CMD_FROM_AI);
	return true;
}

UpdateSleepTime StrafeAreaUpdate::update()
{
	if (!m_data)
	{
		return UPDATE_SLEEP_NONE;
	}
	Object *obj = getObject();
	if (!obj)
	{
		return UPDATE_SLEEP_NONE;
	}
	AIUpdateInterface *ai = obj->getAIUpdateInterface();
	if (!ai)
	{
		return UPDATE_SLEEP_NONE;
	}

	const Coord3D *posPtr = obj->getPosition();
	const Coord3D myPos = posPtr ? *posPtr : Coord3D{};

	Coord3D delta{ m_strafeTarget.x - myPos.x, m_strafeTarget.y - myPos.y, 0.0f };
	float distance = std::sqrt(delta.x * delta.x + delta.y * delta.y);

	switch (m_state)
	{
	case 0:
		ai->chooseLocomotorSet(LOCOMOTORSET_PANIC); // or 6: panic / fast flight
		if (distance <= m_data->m_strafeAreaRadius + m_data->m_slope * 2.0f)
		{
			obj->setModelConditionState(kFiringConditionBit, true);
		}
		if (distance <= m_data->m_strafeAreaRadius + m_data->m_slope)
		{
			ai->chooseLocomotorSet(LOCOMOTORSET_NORMAL);
			m_state = 1;
			m_nextFireFrame = obj->logic().getFrame() + 12;
		}
		break;

	case 1:
	{
		if (!m_data->m_weaponName.empty() && TheWeaponStore)
		{
			const WeaponTemplate *weapon = TheWeaponStore->findWeaponTemplate(m_data->m_weaponName);
			if (weapon && obj->logic().getFrame() >= m_nextFireFrame)
			{
				Coord3D firingPos = myPos;
				float curAngle = obj->getOrientation();
				Coord3D dir{ std::cos(curAngle), std::sin(curAngle), 0.0f };
				Coord3D perp{ dir.y, -dir.x, 0.0f };

				m_sweepPhase += m_data->m_sweepFrequency;
				float offset = std::sin(m_sweepPhase) * m_data->m_sweepAmplitude;

				perp.x *= offset;
				perp.y *= offset;

				dir.x *= m_data->m_slope;
				dir.y *= m_data->m_slope;

				firingPos.x += dir.x + perp.x;
				firingPos.y += dir.y + perp.y;
				firingPos.z = obj->logic().getGroundHeight(firingPos.x, firingPos.y);

				ObjectWeapons::createAndFireTempWeapon(weapon, obj, firingPos);
			}
		}

		obj->setModelConditionState(kFiringConditionBit, true);

		if (m_started && distance > m_data->m_strafeAreaRadius - m_data->m_slope)
		{
			m_state = 2;
			ai->chooseLocomotorSet(LOCOMOTORSET_PANIC);
			obj->setModelConditionState(kFiringConditionBit, false);
			break;
		}

		if (distance < m_data->m_strafeAreaRadius / 12.0f)
		{
			Coord3D onward{ m_strafeTarget.x - m_strafeStart.x, m_strafeTarget.y - m_strafeStart.y, 0.0f };
			float onLen = std::sqrt(onward.x * onward.x + onward.y * onward.y);
			if (onLen > 0.0001f)
			{
				onward.x /= onLen;
				onward.y /= onLen;
			}
			onward.x = onward.x * 500.0f + m_strafeTarget.x;
			onward.y = onward.y * 500.0f + m_strafeTarget.y;
			onward.z = obj->logic().getGroundHeight(onward.x, onward.y);

			ai->aiMoveToPosition(onward, CMD_FROM_AI);
			m_started = true;
		}
		break;
	}

	case 2:
		if (distance > m_data->m_strafeAreaRadius)
		{
			obj->kill(DAMAGE_UNRESISTABLE, DEATH_NORMAL);
		}
		break;
	}

	return UPDATE_SLEEP_NONE;
}

void StrafeAreaUpdate::xfer(Xfer &x)
{
	XferVersion v{ 1, 3 };
	x.xferVersion(v);
	xferUpdateModule(x);

	if (v.current >= 2)
	{
		SnapshotXfer::coord3D(x, m_strafeTarget);
		SnapshotXfer::coord3D(x, m_strafeStart);
		x.xferReal(&m_sweepPhase);
		x.xferInt(&m_state);
	}
	if (v.current >= 3)
	{
		x.xferBool(&m_started);
	}
	if (v.current >= 4)
	{
		x.xferUnsignedInt(&m_nextFireFrame);
	}
}

void StrafeAreaUpdate::crc(StateHasher &hasher) const
{
	UpdateModule::crc(hasher);
	hasher.addFloat(m_strafeTarget.x);
	hasher.addFloat(m_strafeTarget.y);
	hasher.addFloat(m_strafeTarget.z);
	hasher.addFloat(m_strafeStart.x);
	hasher.addFloat(m_strafeStart.y);
	hasher.addFloat(m_strafeStart.z);
	hasher.addFloat(m_sweepPhase);
	hasher.addI32(m_state);
	hasher.addBool(m_started);
	hasher.addU32(m_nextFireFrame);
}

void StrafeAreaUpdate::registerClass(ModuleFactory &modules)
{
	modules.bindTypedData<StrafeAreaUpdateModuleData>("StrafeAreaUpdate", MODULETYPE_BEHAVIOR);
	modules.bindModuleProc("StrafeAreaUpdate", MODULETYPE_BEHAVIOR, [](Thing *thing, const ModuleData *data, const ModuleFactory::ModuleTemplate &) -> std::unique_ptr<Module> {
		const auto *typed = dynamic_cast<const StrafeAreaUpdateModuleData *>(data);
		return std::make_unique<StrafeAreaUpdate>(thing, typed);
	});
}
