// OpenBFME. GPL-3.0.
// See GameLogic/Module/EvacuateDamage.h for the target facts and the addresses.

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif

#include "GameLogic/Module/EvacuateDamage.h"
#include "GameLogic/SnapshotXfer.h"

#include "Common/StateHash.h"
#include "Common/Thing/ModuleFactory.h"
#include "Common/Thing/ThingTemplate.h"
#include "Common/Xfer.h"
#include "GameLogic/AI/AICommandSink.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/Combat/ObjectWeapons.h"
#include "GameLogic/Damage.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/SimMath.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/WeaponState.h"

#include <cstddef>
#include <stdexcept>

namespace
{
typedef EvacuateDamageModuleData Data;

// RW 0x8BEDF8: the weapon template's name, kept only when TheWeaponStore knows it (RW 0x6CC5DF)
void parseWeaponName(INI *ini, void *, void *store, const void *)
{
	const std::string name = ini->getNextToken();
	if (TheWeaponStore && TheWeaponStore->findWeaponTemplate(name))
	{
		*static_cast<std::string *>(store) = name;
	}
}

#define ED_OFF(member) (int)offsetof(Data, member)
const FieldParse kEvacuateDamageFieldParse[] = { // RW 0xC70A88
	{ "WeaponThatCausesEvacuation", parseWeaponName, nullptr, ED_OFF(m_weaponName) },
	{ "DamageTypeToTrack", INI::parseIndexList, TheDamageNames, ED_OFF(m_damageTypeToTrack) },
	{ "DamageToPanicThreshold", INI::parsePercentToReal, nullptr, ED_OFF(m_panicThreshold) },
	{ "TrackingTimeSpan", INI::parseDurationUnsignedInt, nullptr, ED_OFF(m_trackingTimeSpan) },
	{ nullptr, nullptr, nullptr, 0 }
};
#undef ED_OFF

const char *const kStop =
	"[S-5602] EvacuateDamage (RW 0x8BF12E onDamage, 0x8BF09E tracking, 0x8BF076 panic): the named weapon's hit empties the garrison and the tracked damage is summed as in "
	"RotWK; NOT ported: the panic's AI command 0x2E (RW 0x881FF3, the attacker as its object, source 2) is not identified in the port, so a garrison that took too much damage "
	"stays inside (the tracked list is cleared). INFERENCE: ZH's AICMD_PANIC";
} // namespace

void EvacuateDamageModuleData::buildFieldParse(MultiIniFieldParse &p)
{
	p.add(kEvacuateDamageFieldParse);
}

EvacuateDamage::EvacuateDamage(Thing *thing, const EvacuateDamageModuleData *data)
	: BehaviorModule(thing, data)
	, m_data(data)
{
}

const char *EvacuateDamage::stopLine()
{
	return kStop;
}

// RW 0x8BF12E
void EvacuateDamage::onDamage(const DamageInfo &info)
{
	Object *self = getObject();
	Object *source = self->logic().findObjectByID(info.m_input.m_sourceID);
	if (!source)
	{
		return;
	}
	const ObjectWeapons *weapons = source->getWeapons();
	const Weapon *weapon = weapons ? weapons->currentWeapon() : nullptr; // RW 0x68B58C(0)
	if (!weapon || !weapon->getTemplate())
	{
		return;
	}
	if (m_data->m_weaponName == weapon->getTemplate()->getName()) // RW 0x4065AA == 0
	{
		evacuateAll();
		return;
	}
	if (m_data->m_damageTypeToTrack == info.m_input.m_damageType)
	{
		trackDamage(info);
	}
}

// RW 0x8BF178 .. 0x8BF1E8: every rider of the object's contain leaves it, command source 2
void EvacuateDamage::evacuateAll()
{
	Object *self = getObject();
	ContainModuleInterface *contain = self->getContain();
	if (!contain)
	{
		return;
	}
	static const unsigned kHorde = (unsigned)CombatNames::kindOf("HORDE"); // template + 0x115 bit 5
	std::vector<ObjectID> riders;
	if (const ContainModuleInterface::ContainedItemsList *items = contain->getContainedItemsList()) // slot 0x118
	{
		for (Object *o : *items)
		{
			riders.push_back(o->getID());
		}
	}
	for (ObjectID id : riders)
	{
		Object *rider = self->logic().findObjectByID(id);
		AIUpdateInterface *ai = rider ? rider->getAIUpdateInterface() : nullptr;
		if (!ai)
		{
			continue; // RW reads the AI unchecked; a rider without one is not a case in the data
		}
		if (rider->isKindOf(kHorde))
		{
			ai->aiHordeExit(self, CMD_FROM_AI); // RW 0x775AFB
		}
		else
		{
			ai->aiExit(self, CMD_FROM_AI); // RW 0x7716C1
		}
	}
}

// RW 0x8BEF0A: the entries above now - TrackingTimeSpan are summed in list order, the others are erased
float EvacuateDamage::windowedSum()
{
	const int now = (int)getObject()->logic().getFrame();
	const int oldest = now - (int)m_data->m_trackingTimeSpan;
	float sum = 0.0f;
	for (size_t i = 0; i < m_tracked.size();)
	{
		if (m_tracked[i].frame > oldest)
		{
			sum = SimMath::addf32(m_tracked[i].amount, sum);
			++i;
		}
		else
		{
			m_tracked.erase(m_tracked.begin() + (std::ptrdiff_t)i);
		}
	}
	return sum;
}

// RW 0x8BF09E
void EvacuateDamage::trackDamage(const DamageInfo &info)
{
	Object *self = getObject();
	const BodyModuleInterface *body = self->getBodyModule(); // Object + 0x25C
	if (!body)
	{
		return;
	}
	Entry e;
	e.amount = info.m_input.m_amount;
	e.frame = (int)self->logic().getFrame();
	m_tracked.push_back(e);
	const float sum = windowedSum();
	const float threshold = SimMath::mulf32(body->getMaxHealth(), m_data->m_panicThreshold);
	if (sum < threshold) // fcompi; jb
	{
		return;
	}
	if (Object *source = self->logic().findObjectByID(info.m_input.m_sourceID))
	{
		panic(*source);
	}
}

// RW 0x8BF076
void EvacuateDamage::panic(Object &source)
{
	(void)source;
	Object *self = getObject();
	if (!self->getAIUpdateInterface())
	{
		return;
	}
	m_tracked.clear();                    // RW 0x862535
	self->logic().noteStop(kStop);        // AI command 0x2E (RW 0x881FF3) is not ported
}

void EvacuateDamage::crc(StateHasher &h) const
{
	BehaviorModule::crc(h);
	h.addU32((std::uint32_t)m_tracked.size());
	for (const Entry &e : m_tracked)
	{
		h.addFloat(e.amount);
		h.addI32(e.frame);
	}
}

void EvacuateDamage::xfer(Xfer &x)
{
	SnapshotXfer::version(x, 1, 1);
	xferBehaviorModule(x);
	std::uint32_t count = (std::uint32_t)m_tracked.size();
	x.xferUnsignedInt(&count);
	SnapshotXfer::checkRange((int)count, 0, 100000, "an EvacuateDamage's tracked hits");
	m_tracked.resize(count);
	for (Entry &e : m_tracked)
	{
		x.xferReal(&e.amount);
		x.xferInt(&e.frame);
	}
}

void EvacuateDamage::registerClass(ModuleFactory &modules)
{
	const char *name = "EvacuateDamage";
	modules.bindTypedData<Data>(name, MODULETYPE_BEHAVIOR);
	modules.bindModuleProc(name, MODULETYPE_BEHAVIOR, [name](Thing *thing, const ModuleData *data, const ModuleFactory::ModuleTemplate &) -> std::unique_ptr<Module> {
		const Data *typed = dynamic_cast<const Data *>(data);
		if (!typed)
		{
			throw std::logic_error(std::string(name) + ": the module data is not typed");
		}
		return std::make_unique<EvacuateDamage>(thing, typed);
	});
}
