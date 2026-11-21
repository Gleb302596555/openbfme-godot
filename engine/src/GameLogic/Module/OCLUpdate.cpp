// OpenBFME. GPL-3.0.
// See GameLogic/Module/OCLUpdate.h for the target facts and the addresses.

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif

#include "GameLogic/Module/OCLUpdate.h"
#include "GameLogic/SnapshotXfer.h"

#include "Common/StateHash.h"
#include "Common/Thing/ModuleFactory.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Map/TerrainLogic.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/ObjectCreationList.h"
#include "GameLogic/SimMath.h"

#include <cstddef>
#include <stdexcept>

namespace
{
// RW 0x73A368: an ObjectCreationList name, "None" stores none
void parseOCLName(INI *ini, void *, void *store, const void *)
{
	const std::string name = ini->getNextToken();
	*static_cast<std::string *>(store) = name == "None" ? std::string() : name;
}

#define OU_OFF(member) (int)offsetof(OCLUpdateModuleData, member)
const FieldParse kOCLUpdateFieldParse[] = {
	{ "OCL", parseOCLName, nullptr, OU_OFF(m_ocl) },
	{ "MinDelay", INI::parseDurationUnsignedInt, nullptr, OU_OFF(m_minDelay) },
	{ "MaxDelay", INI::parseDurationUnsignedInt, nullptr, OU_OFF(m_maxDelay) },
	{ "CreateAtEdge", INI::parseBool, nullptr, OU_OFF(m_createAtEdge) },
	{ "Amount", INI::parseInt, nullptr, OU_OFF(m_amount) },
	{ nullptr, nullptr, nullptr, 0 }
};
#undef OU_OFF

const char *const kStop = "";
} // namespace

void OCLUpdateModuleData::buildFieldParse(MultiIniFieldParse &p)
{
	p.add(kOCLUpdateFieldParse);
}

OCLUpdate::OCLUpdate(Thing *thing, const OCLUpdateModuleData *data)
	: UpdateModule(thing, data)
	, m_data(data)
{
}

const char *OCLUpdate::stopLine()
{
	return kStop;
}

float OCLUpdate::getCountdownPercent() const
{
	// RW 0x89EA88: 1.0f - (float)(m_nextFrame - now) / (float)(m_nextFrame - m_scheduledAt)
	if (m_nextFrame == 0 || m_nextFrame <= m_scheduledAt)
	{
		return 0.0f;
	}
	const UnsignedInt now = getObject()->logic().getFrame();
	if (now >= m_nextFrame)
	{
		return 1.0f;
	}
	return SimMath::subf32(1.0f, SimMath::divf32((float)(m_nextFrame - now), (float)(m_nextFrame - m_scheduledAt)));
}

// RW 0x89EBBA
void OCLUpdate::schedule()
{
	GameLogic &logic = getObject()->logic();
	const int delay = logic.random().getValue((int)m_data->m_minDelay, (int)m_data->m_maxDelay, "OCLUpdate.cpp", 0x7B); // GetGameLogicRandomValue
	m_scheduledAt = logic.getFrame();
	m_nextFrame = m_scheduledAt + (unsigned)delay;
}

// RW 0x89EC43
UpdateSleepTime OCLUpdate::update()
{
	Object *obj = getObject();
	GameLogic &logic = obj->logic();
	if (logic.getFrame() < m_nextFrame) // RW 0x89EB99 (the creation frame is not reached)
	{
		return UPDATE_SLEEP_NONE;
	}
	if (m_nextFrame == 0)
	{
		schedule(); // RW 0x89EC63: the first call only schedules
		return UPDATE_SLEEP_NONE;
	}
	schedule();
	Coord3D where = *obj->getPosition();
	if (m_data->m_createAtEdge && logic.terrain())
	{
		// RW 0x89EC81: TerrainLogic::findClosestEdgePoint (RW 0x67D949, vslot 0x34)
		where = logic.terrain()->findClosestEdgePoint(where);
	}
	const Coord3D secondary = *obj->getPosition();
	const ObjectCreationList *ocl = TheObjectCreationListStore && !m_data->m_ocl.empty() ? TheObjectCreationListStore->findObjectCreationList(m_data->m_ocl) : nullptr;
	if (ocl)
	{
		ocl->create(logic, obj, where, &secondary, 0); // RW 0x89ECBA (RW 0x5F00CA)
	}
	if (m_data->m_amount > 0)
	{
		++m_created;
		if (m_created >= m_data->m_amount)
		{
			return UPDATE_SLEEP_FOREVER; // RW 0x89ECD0
		}
	}
	return UPDATE_SLEEP_NONE;
}

void OCLUpdate::crc(StateHasher &h) const
{
	UpdateModule::crc(h);
	h.addU32(m_nextFrame);
	h.addU32(m_scheduledAt);
	h.addI32(m_created);
}

void OCLUpdate::registerClass(ModuleFactory &modules)
{
	const char *name = "OCLUpdate";
	modules.bindTypedData<OCLUpdateModuleData>(name, MODULETYPE_BEHAVIOR);
	modules.bindModuleProc(name, MODULETYPE_BEHAVIOR, [name](Thing *thing, const ModuleData *data, const ModuleFactory::ModuleTemplate &) -> std::unique_ptr<Module> {
		const OCLUpdateModuleData *typed = dynamic_cast<const OCLUpdateModuleData *>(data);
		if (!typed)
		{
			throw std::logic_error(std::string(name) + ": the module data is not typed");
		}
		return std::make_unique<OCLUpdate>(thing, typed);
	});
}

// RW 0x0089EBE8 (version block RW 0x89EC00 is (1, 2)): UpdateModule, the two frames, from version 2 the count. OpenBFME native layout (S-4190): one version with all three
void OCLUpdate::xfer(Xfer &x)
{
	SnapshotXfer::version(x, 1, 1);
	xferUpdateModule(x);
	x.xferUnsignedInt(&m_nextFrame);
	x.xferUnsignedInt(&m_scheduledAt);
	x.xferInt(&m_created);
	SnapshotXfer::checkRange(m_created, 0, 0x7FFFFFFF, "OCLUpdate m_created");
}
