// OpenBFME. GPL-3.0.
// See GameLogic/Module/AimWeaponBehavior.h for the target facts and the addresses.

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif

#include "GameLogic/Module/AimWeaponBehavior.h"

#include "Common/StateHash.h"
#include "Common/Thing/ModuleFactory.h"
#include "GameLogic/Combat/ObjectWeapons.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/SimMath.h"
#include "GameLogic/SnapshotXfer.h"

#include <cmath>
#include <cstddef>
#include <memory>
#include <stdexcept>

namespace
{
typedef AimWeaponBehaviorModuleData Data;

const int kAimHigh = 0xB2;     // bit 178: + 0x120 mask 0x40000
const int kAimStraight = 0xB3; // bit 179: mask 0x80000
const int kAimLow = 0xB4;      // bit 180: mask 0x100000
const int kAimNear = 0xB5;
const int kAimFar = 0xB6;

const FieldParse kFields[] = { // RW 0xC58178
	{ "AimLowThreshold", INI::parseReal, nullptr, (int)offsetof(Data, m_aimLowThreshold) },
	{ "AimHighThreshold", INI::parseReal, nullptr, (int)offsetof(Data, m_aimHighThreshold) },
	{ "AimNearDistance", INI::parseReal, nullptr, (int)offsetof(Data, m_aimNearDistance) },
	{ "AimFarDistance", INI::parseReal, nullptr, (int)offsetof(Data, m_aimFarDistance) },
	{ nullptr, nullptr, nullptr, 0 }
};

Object::ModelConditionBits bit(int index)
{
	Object::ModelConditionBits b{};
	b[(size_t)index >> 5] |= 1u << (index & 31);
	return b;
}
} // namespace

void AimWeaponBehaviorModuleData::buildFieldParse(MultiIniFieldParse &p)
{
	p.add(kFields);
}

AimWeaponBehavior::AimWeaponBehavior(Thing *thing, const Data *data)
	: UpdateModule(thing, data)
	, m_data(data)
{
	setWakeFrame(getObject(), UPDATE_SLEEP(1)); // RW 0x85E2D9
}

// RW 0x85E30B
UpdateSleepTime AimWeaponBehavior::update()
{
	Object *obj = getObject();
	AIUpdateInterface *ai = obj->getAIUpdateInterface();
	ObjectWeapons *weapons = obj->getWeapons();
	const Weapon *weapon = weapons ? weapons->currentWeapon() : nullptr; // RW 0x68B58C(0)
	if (!ai || !weapon)
	{
		return UPDATE_SLEEP_FOREVER; // RW 0x85E505
	}
	const std::uint32_t frame = obj->logic().getFrame();
	const bool attacking = ai->isAttacking() || (weapon->whenPreAttackFinished() <= frame && frame < weapon->followThruEnd()); // slot 0x1BC, RW 0x6CA2C3
	bool changed = false;
	if (weapon->getTemplate()->m_isAimingWeapon && attacking)
	{
		const Coord3D &target = weapons->drawTargetPosition(); // Object + 0x3A8
		const Coord3D &me = *obj->getPosition();
		const float dx = SimMath::subf32(target.x, me.x), dy = SimMath::subf32(target.y, me.y), dz = SimMath::subf32(target.z, me.z);
		// RW 0x85E3B7 .. 0x85E3D4: x87, sqrt from msvcr71, stored as a float
		float distance = (float)std::sqrt((double)dx * (double)dx + (double)dy * (double)dy);
		if (10.0f > distance) // RW 0xBD83D8
		{
			distance = 10.0f;
		}
		const float slope = SimMath::divf32(dz, distance);
		int aim = kAimStraight;
		if (slope > m_data->m_aimHighThreshold)
		{
			aim = kAimHigh;
		}
		else if (m_data->m_aimLowThreshold > slope)
		{
			aim = kAimLow;
		}
		m_aiming = true;
		if (!obj->testModelCondition(aim))
		{
			obj->setModelConditionState(aim, true); // RW 0x85E445: only ors the bit
		}
		if (m_data->m_aimNearDistance > distance)
		{
			obj->clearAndSetModelConditionFlags(bit(kAimFar), bit(kAimNear)); // RW 0x85E4AC
		}
		else if (distance > m_data->m_aimFarDistance)
		{
			obj->clearAndSetModelConditionFlags(bit(kAimNear), bit(kAimFar));
		}
		changed = true; // RW 0x85E4B1
	}
	if (m_aiming && !attacking)
	{
		m_aiming = false;
		Object::ModelConditionBits clear{};
		for (int b : { kAimHigh, kAimStraight, kAimLow, kAimFar, kAimNear })
		{
			clear[(size_t)b >> 5] |= 1u << (b & 31);
		}
		obj->clearAndSetModelConditionFlags(clear, Object::ModelConditionBits{}); // RW 0x85E4E7
		changed = true;
	}
	if (changed)
	{
		obj->flushDrawableModelConditions(); // RW 0x85E4FB
	}
	return UPDATE_SLEEP_NONE;
}

void AimWeaponBehavior::crc(StateHasher &h) const
{
	UpdateModule::crc(h);
	h.addBool(m_aiming);
}

void AimWeaponBehavior::xfer(Xfer &x)
{
	SnapshotXfer::version(x, 1, 1);
	xferUpdateModule(x);
	x.xferBool(&m_aiming);
}

void AimWeaponBehavior::registerClass(ModuleFactory &modules)
{
	modules.bindTypedData<Data>("AimWeaponBehavior", MODULETYPE_BEHAVIOR);
	modules.bindModuleProc("AimWeaponBehavior", MODULETYPE_BEHAVIOR, [](Thing *thing, const ModuleData *data, const ModuleFactory::ModuleTemplate &) -> std::unique_ptr<Module> {
		const Data *typed = dynamic_cast<const Data *>(data);
		if (!typed)
		{
			throw std::logic_error("AimWeaponBehavior: the module data is not typed");
		}
		return std::make_unique<AimWeaponBehavior>(thing, typed);
	});
}
