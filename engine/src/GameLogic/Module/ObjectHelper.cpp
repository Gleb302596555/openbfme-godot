// OpenBFME. GPL-3.0.
// See GameLogic/Module/ObjectHelper.h.

#include "GameLogic/Module/ObjectHelper.h"

#include "Common/StateHash.h"
#include "Common/Xfer.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/SnapshotXfer.h"

ObjectHelperShell::ObjectHelperShell(Thing *thing, std::string helperName, std::string tag)
	: UpdateModule(thing, nullptr)
	, m_name(std::move(helperName))
	, m_tag(std::move(tag))
{
	// ZH ObjectHelper::ObjectHelper: setWakeFrame(getObject(), UPDATE_SLEEP_FOREVER). The object is not in the list yet, so
	// friend_awakenUpdateModule just stores the frame (the module index is -1): the same state, set directly.
	friend_setNextCallFrame((UnsignedInt)UPDATE_SLEEP_FOREVER);
	friend_setModuleClass(m_name, NAMEKEY_INVALID);
}

UpdateSleepTime ObjectHelperShell::update()
{
	++m_calls;
	return UPDATE_SLEEP_FOREVER;
}

void ObjectHelperShell::xfer(Xfer &x)
{
	XferVersion v{ 1, 1 };
	x.xferVersion(v);
	xferUpdateModule(x);
	x.xferUnsignedInt(&m_calls);
}

DefectionHelper::DefectionHelper(Thing *thing)
	: ObjectHelperShell(thing, "DefectionHelper", "ModuleTag_DefectionHelper")
{
}

// RW 0x8E3ABB
void DefectionHelper::startDefectionTimer(UnsignedInt frames, bool flash)
{
	Object *obj = getObject();
	if (!obj)
	{
		return;
	}
	if (!obj->isUndetectable())
	{
		setWakeFrame(obj, UPDATE_SLEEP_FOREVER);
		return;
	}
	const UnsignedInt now = obj->logic().getFrame();
	m_startFrame = now;
	m_expireFrame = now + frames;
	m_flash = flash;
	setWakeFrame(obj, UPDATE_SLEEP_NONE);
}

// RW 0x8E38DD
UpdateSleepTime DefectionHelper::update()
{
	Object *obj = getObject();
	if (!obj || !obj->isUndetectable())
	{
		return UPDATE_SLEEP_FOREVER;
	}
	const UnsignedInt now = obj->logic().getFrame();
	if (now >= m_expireFrame || obj->isEffectivelyDead() || obj->isDisabledByType(DISABLED_UNMANNED))
	{
		obj->setTemporarilyUndetectable(false); // RW 0x68B303(false)
		return UPDATE_SLEEP_FOREVER;
	}
	return UPDATE_SLEEP_NONE;
}

void DefectionHelper::crc(StateHasher &hasher) const
{
	UpdateModule::crc(hasher);
	if (m_startFrame != 0 || m_expireFrame != 0 || m_flash)
	{
		hasher.addU32(m_startFrame);
		hasher.addU32(m_expireFrame);
		hasher.addBool(m_flash);
	}
}

void DefectionHelper::xfer(Xfer &x)
{
	SnapshotXfer::version(x, 1, 1);
	ObjectHelperShell::xfer(x);
	x.xferUnsignedInt(&m_startFrame);
	x.xferUnsignedInt(&m_expireFrame);
	x.xferBool(&m_flash);
}
