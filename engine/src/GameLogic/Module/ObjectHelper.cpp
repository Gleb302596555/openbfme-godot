// OpenBFME. GPL-3.0.
// See GameLogic/Module/ObjectHelper.h.

#include "GameLogic/Module/ObjectHelper.h"

#include "Common/StateHash.h"
#include "Common/Audio/AudioRequests.h"
#include "Common/GameCommon.h"
#include "GameLogic/SimMath.h"
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
	m_flashPhase = 0.0f; // RW 0x8E3ABB: a new timer starts on phase zero
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
	ObjectClientHooks *draw = obj->clientHooks();
	if (now >= m_expireFrame)
	{
		obj->setTemporarilyUndetectable(false);
		if (draw && m_flash) // RW 0x8E3927..0x8E39A5
		{
			const RGBColor white{ 1.0f, 1.0f, 1.0f };
			draw->flashAsSelected(*obj, &white);
			AudioApi::playMiscAudioForObject("DefectorTimerDingSound", obj->getID());
		}
		return UPDATE_SLEEP_FOREVER;
	}
	if ((obj->getAIUpdateInterface() && obj->isEffectivelyDead()) || obj->testStatus(13)) // RW 0x8E39B8..0x8E39E2: IS_FIRING_WEAPON, not UNMANNED
	{
		obj->setTemporarilyUndetectable(false);
		return UPDATE_SLEEP_FOREVER;
	}
	// The phase is saved/hashed: advance it on headless peers too; only the visual/audio edge needs a drawable.
	if (m_flash)
	{
		// RW 0x8E39F8..0x8E3A48: cast the OLD and NEW phases, not the remaining frames; a falling edge only.
		const auto before = SimMath::ftol2Low32(m_flashPhase);
		const double fraction = SimMath::pc24DivW(SimMath::fildU32(m_expireFrame - now), 10 * LOGICFRAMES_PER_SECOND);
		const double phase = SimMath::pc24AddW(m_flashPhase, SimMath::pc24MulW(0.5, SimMath::pc24SubW(1.0, fraction)));
		m_flashPhase = SimMath::fstpDword(phase);
		if (draw && (before & 1u) && !(SimMath::ftol2Low32(phase) & 1u))
		{
			draw->flashAsSelected(*obj, nullptr);
			AudioApi::playMiscAudioForObject("DefectorTimerTickSound", obj->getID());
		}
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
		if (m_flashPhase != 0.0f)
		{
			hasher.addFloat(m_flashPhase);
		}
	}
}

void DefectionHelper::xfer(Xfer &x)
{
	XferVersion v{ 1, 2 }; // OpenBFME v1 had no +28; an old window resumes at the initial phase
	x.xferVersion(v);
	ObjectHelperShell::xfer(x);
	x.xferUnsignedInt(&m_startFrame);
	x.xferUnsignedInt(&m_expireFrame);
	x.xferBool(&m_flash);
	if (v.current >= 2)
	{
		x.xferReal(&m_flashPhase);
	}
	else
	{
		m_flashPhase = 0.0f;
	}
}
