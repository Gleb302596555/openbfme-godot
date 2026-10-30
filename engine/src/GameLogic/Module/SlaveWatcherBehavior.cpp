// OpenBFME. GPL-3.0.
// See GameLogic/Module/SlaveWatcherBehavior.h for the target facts and the addresses.

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif

#include "GameLogic/Module/SlaveWatcherBehavior.h"
#include "GameLogic/SnapshotXfer.h"

#include "Common/StateHash.h"
#include "Common/Thing/ModuleFactory.h"
#include "GameLogic/Damage.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object/Object.h"

#include <cstddef>
#include <stdexcept>

namespace
{
#define SW_OFF(member) (int)offsetof(SlaveWatcherBehaviorModuleData, member)
const FieldParse kSlaveWatcherFieldParse[] = {
	{ "GrantUpgrade", INI::parseAsciiString, nullptr, SW_OFF(m_grantUpgrade) },
	{ "RemoveUpgrade", INI::parseAsciiString, nullptr, SW_OFF(m_removeUpgrade) },
	{ "ShareUpgrades", INI::parseBool, nullptr, SW_OFF(m_shareUpgrades) },
	{ "LetSlaveLive", INI::parseBool, nullptr, SW_OFF(m_letSlaveLive) },
	{ nullptr, nullptr, nullptr, 0 }
};
#undef SW_OFF

bool sameMask(const UpgradeMaskType &a, const UpgradeMaskType &b)
{
	return a.words == b.words;
}
} // namespace

void SlaveWatcherBehaviorModuleData::buildFieldParse(MultiIniFieldParse &p)
{
	p.add(kSlaveWatcherFieldParse);
}

SlaveWatcherBehavior::SlaveWatcherBehavior(Thing *thing, const SlaveWatcherBehaviorModuleData *data)
	: UpdateModule(thing, data)
	, m_data(data)
{
}

// RW 0x887AF5
void SlaveWatcherBehavior::setSlave(ObjectID id)
{
	Object *owner = getObject();
	m_slave = id;
	if (m_data->m_shareUpgrades)
	{
		if (Object *slave = owner->logic().findObjectByID(id))
		{
			m_shared = owner->getUpgradeMask();       // RW 0x887B1B .. 0x887B2E: 0x24 dwords
			slave->friend_orUpgradeMask(m_shared);    // RW 0x68CC6C
			slave->updateUpgradeModules();            // RW 0x6936FE
		}
	}
	setWakeFrame(owner, UPDATE_SLEEP(1)); // RW 0x887B46 (RW 0x850C32)
}

// RW 0x887A35
UpdateSleepTime SlaveWatcherBehavior::update()
{
	if (m_slave == INVALID_ID)
	{
		return UPDATE_SLEEP_NONE;
	}
	Object *owner = getObject();
	GameLogic &logic = owner->logic();
	Object *slave = logic.findObjectByID(m_slave);
	if (slave && !slave->isEffectivelyDead())
	{
		if (m_data->m_shareUpgrades && !sameMask(m_shared, owner->getUpgradeMask())) // RW 0x887A66 .. 0x887A7C
		{
			m_shared = owner->getUpgradeMask();
			slave->friend_orUpgradeMask(m_shared);
			slave->updateUpgradeModules();
		}
		return UPDATE_SLEEP_NONE;
	}
	// the slave is gone or dead (RW 0x887AA9)
	const UpgradeTemplate *remove = TheUpgradeCenter && !m_data->m_removeUpgrade.empty() ? TheUpgradeCenter->findUpgrade(m_data->m_removeUpgrade) : nullptr; // RW 0x66F5E5
	const UpgradeTemplate *grant = TheUpgradeCenter && !m_data->m_grantUpgrade.empty() ? TheUpgradeCenter->findUpgrade(m_data->m_grantUpgrade) : nullptr;
	if (remove)
	{
		owner->removeUpgrade(remove); // RW 0x887AD0 (RW 0x691438)
	}
	if (grant)
	{
		owner->giveUpgrade(grant); // RW 0x887ADC (RW 0x69388B)
	}
	owner->updateUpgradeModules(); // RW 0x887AE5
	m_slave = INVALID_ID;          // RW 0x887AEA
	return UPDATE_SLEEP_FOREVER;
}

// RW 0x8879C5: the destructor kills a slave that may not live on
void SlaveWatcherBehavior::onDelete()
{
	if (m_data->m_letSlaveLive || m_slave == INVALID_ID)
	{
		return;
	}
	if (Object *slave = getObject()->logic().findObjectByID(m_slave))
	{
		if (!slave->isDestroyed())
		{
			slave->kill(DAMAGE_FORCE, 13); // RW 0x887A12: kill(0, 13)
		}
	}
	m_slave = INVALID_ID;
}

void SlaveWatcherBehavior::crc(StateHasher &h) const
{
	UpdateModule::crc(h);
	h.addU32((std::uint32_t)m_slave);
	for (unsigned i = 0; i < UpgradeMaskType::activeWords(); ++i)
	{
		h.addU32(m_shared.words[i]);
	}
}

void SlaveWatcherBehavior::registerClass(ModuleFactory &modules)
{
	const char *name = "SlaveWatcherBehavior";
	modules.bindTypedData<SlaveWatcherBehaviorModuleData>(name, MODULETYPE_BEHAVIOR);
	modules.bindModuleProc(name, MODULETYPE_BEHAVIOR, [name](Thing *thing, const ModuleData *data, const ModuleFactory::ModuleTemplate &) -> std::unique_ptr<Module> {
		const SlaveWatcherBehaviorModuleData *typed = dynamic_cast<const SlaveWatcherBehaviorModuleData *>(data);
		if (!typed)
		{
			throw std::logic_error(std::string(name) + ": the module data is not typed");
		}
		return std::make_unique<SlaveWatcherBehavior>(thing, typed);
	});
}

// OpenBFME native layout (S-4190)
void SlaveWatcherBehavior::xfer(Xfer &x)
{
	SnapshotXfer::version(x, 1, 1);
	xferUpdateModule(x);
	x.xferObjectID(&m_slave);
	xferUpgradeMask(x, m_shared, "a SlaveWatcherBehavior's shared upgrades");
}
