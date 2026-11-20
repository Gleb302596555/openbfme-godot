// OpenBFME. GPL-3.0.
// See GameLogic/Module/HordeDispatchSpecialPower.h for the sources and what is inference.

#include "GameLogic/Module/HordeDispatchSpecialPower.h"
#include "GameLogic/SnapshotXfer.h"

#include "Common/StateHash.h"
#include "Common/Thing/ModuleFactory.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/ObjectTemplateInfo.h"
#include "GameLogic/SimMath.h"

#include <stdexcept>
#include <string>

bool HordeDispatchSpecialPower::isHordeWithContain() const
{
	static const int kHorde = ObjectTemplateInfoBuilder::kindOfIndex("HORDE"); // template + 0x115 bit 5
	const Object *obj = getObject();
	return obj && kHorde >= 0 && obj->isKindOf((unsigned)kHorde) && obj->getContain() != nullptr;
}

std::vector<SpecialPowerModuleInterface *> HordeDispatchSpecialPower::memberModules() const
{
	std::vector<SpecialPowerModuleInterface *> out;
	const ContainModuleInterface::ContainedItemsList *items = getObject()->getContain()->getContainedItemsList();
	if (!items)
	{
		return out;
	}
	const ContainModuleInterface::ContainedItemsList copy = *items; // contain vslot 0x10C fills a local list
	for (Object *member : copy)
	{
		if (SpecialPowerModuleInterface *m = member ? SpecialPowerModules::findModule(*member, getSpecialPowerTemplate()) : nullptr) // RW 0x68C26D
		{
			out.push_back(m);
		}
	}
	return out;
}

void HordeDispatchSpecialPower::doSpecialPower(unsigned options)
{
	// RW 0x8CD08C
	if (!isHordeWithContain())
	{
		return;
	}
	SpecialPowerModule::doSpecialPower(options); // RW 0x8980A3
	for (SpecialPowerModuleInterface *m : memberModules())
	{
		m->doSpecialPower(options);
		++m_dispatched;
	}
}

void HordeDispatchSpecialPower::doSpecialPowerAtLocation(const Coord3D &loc, unsigned options)
{
	// RW 0x8CCEC9
	if (!isHordeWithContain())
	{
		return;
	}
	SpecialPowerModule::doSpecialPowerAtLocation(loc, options); // RW 0x89816C
	for (SpecialPowerModuleInterface *m : memberModules())
	{
		m->doSpecialPowerAtLocation(loc, options);
		++m_dispatched;
	}
}

// RW 0x5E3BE4: squared 3D distance between an object (ECX reads its position + 0x38 .. 0x40) and a position; dz*dz + dy*dy, then + dx*dx (x87 at PC24)
static float distanceSquared3(const Coord3D &a, const Coord3D &b)
{
	const float dx = SimMath::subf32(a.x, b.x), dy = SimMath::subf32(a.y, b.y), dz = SimMath::subf32(a.z, b.z);
	return SimMath::addf32(SimMath::addf32(SimMath::mulf32(dz, dz), SimMath::mulf32(dy, dy)), SimMath::mulf32(dx, dx));
}

void HordeDispatchSpecialPower::doSpecialPowerAtObject(Object *target, unsigned options)
{
	// RW 0x8CCF76: the members first (each with the order's target); the member's kept target (the special power interface's slot 0x50, RW 0x7B0F25), found again
	// (RW 0x449681), replaces the horde's own base call's target when it is farther from the horde (squared distance RW 0x5E3BE4) than the best so far, start: the
	// order's target (RW computes its distance before the member loop, and keeps the first only on a strictly greater distance); then
	// SpecialPowerModule::doSpecialPowerAtObject (RW 0x8980EF) on the horde with that target
	if (!isHordeWithContain() || !target)
	{
		return;
	}
	GameLogic &logic = getObject()->logic();
	const Coord3D hpos = *getObject()->getPosition(); // obj + 0x38
	Object *chosen = target;
	float bestDist = distanceSquared3(*target->getPosition(), hpos); // RW 0x8CCFF1 .. 0x8CCFF6 (ECX = the order's target)
	for (SpecialPowerModuleInterface *m : memberModules())
	{
		m->doSpecialPowerAtObject(target, options); // vslot 0x2C (RW 0x8CD018)
		++m_dispatched;
		const ObjectID kept = m->getTargetId();     // vslot 0x50 (RW 0x8CD01F .. 0x8CD022)
		if (Object *t = logic.findObjectByID(kept)) // RW 0x449681 (findObjectByID(0) finds nothing)
		{
			const float d = distanceSquared3(*t->getPosition(), hpos); // RW 0x8CD034 .. 0x8CD03E
			if (d > bestDist)                                          // RW 0x8CD041 .. 0x8CD049: fcompi + jbe (replace only on a strictly greater distance)
			{
				bestDist = d;
				chosen = t;
			}
		}
	}
	SpecialPowerModule::doSpecialPowerAtObject(chosen, options); // RW 0x8CD067 -> RW 0x8980EF
}

void HordeDispatchSpecialPower::crc(StateHasher &h) const
{
	SpecialPowerModule::crc(h);
	h.addU64(m_dispatched);
}

void HordeDispatchSpecialPower::registerClass(ModuleFactory &modules)
{
	const char *name = "HordeDispatchSpecialPower";
	modules.bindTypedData<SpecialPowerModuleData>(name, MODULETYPE_BEHAVIOR); // RW 0xC64DB0 + the empty RW 0xC84858
	modules.bindModuleProc(name, MODULETYPE_BEHAVIOR, [name](Thing *thing, const ModuleData *data, const ModuleFactory::ModuleTemplate &) -> std::unique_ptr<Module> {
		const SpecialPowerModuleData *typed = dynamic_cast<const SpecialPowerModuleData *>(data);
		if (!typed)
		{
			throw std::logic_error(std::string(name) + ": the module data is not typed");
		}
		return std::make_unique<HordeDispatchSpecialPower>(thing, typed);
	});
}

// ---- lane SAVE-4b: the snapshot (OpenBFME native, S-4190) ---------------------------------------------------------------------------------------------------
void HordeDispatchSpecialPower::xfer(Xfer &x)
{
	SnapshotXfer::version(x, 1, 1);
	SpecialPowerModule::xfer(x);
	SnapshotXfer::u64(x, m_dispatched);
}
