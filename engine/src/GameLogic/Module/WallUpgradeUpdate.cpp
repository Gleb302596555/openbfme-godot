// OpenBFME. GPL-3.0.
// See GameLogic/Module/WallUpgradeUpdate.h (editions: both RotWK 2.01 and BFME2 1.06).

#include "GameLogic/Module/WallUpgradeUpdate.h"
#include "GameLogic/ObjectTemplateInfo.h"

#include "Common/INI.h"
#include "Common/Thing/ModuleFactory.h"
#include "Common/Xfer.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/SnapshotXfer.h"

#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>

namespace
{
constexpr int kRubbleCountdownFrames = 40; // 8 * 5 logic fps (RW 0x8AEAE6 / BFME2 0x4AB3EB)
} // namespace

void WallUpgradeUpdateModuleData::buildFieldParse(MultiIniFieldParse &)
{
}

WallUpgradeUpdate::WallUpgradeUpdate(Thing *thing, const WallUpgradeUpdateModuleData *data)
	: UpdateModule(thing, data)
	, m_data(data)
{
	friend_setNextCallFrame((UnsignedInt)UPDATE_SLEEP_NONE);
}

// RW 0x8AEAA9 / BFME2 0x4AB3AE
bool WallUpgradeUpdate::isWallUpgradeKind() const
{
	if (!getObject())
	{
		return false;
	}
	static const int k1 = ObjectTemplateInfoBuilder::kindOfIndex("UPGRADE_GARRISON");
	static const int k2 = ObjectTemplateInfoBuilder::kindOfIndex("POSTERN_GATE");
	static const int k3 = ObjectTemplateInfoBuilder::kindOfIndex("TREBUCHET");
	return (k1 >= 0 && getObject()->isKindOf((unsigned)k1)) ||
	       (k2 >= 0 && getObject()->isKindOf((unsigned)k2)) ||
	       (k3 >= 0 && getObject()->isKindOf((unsigned)k3));
}

// RW 0x8AEAE6 / BFME2 0x4AB3EB
void WallUpgradeUpdate::onBodyDamageStateChange(const DamageInfo *, BodyDamageType oldState, BodyDamageType newState)
{
	if (newState > oldState && newState == BODY_RUBBLE)
	{
		m_countdown = kRubbleCountdownFrames;
		friend_setNextCallFrame((UnsignedInt)UPDATE_SLEEP_NONE);
	}
}

// RW 0x8AEB10 / BFME2 0x4AB415
void WallUpgradeUpdate::scanForBuildingAndPossess()
{
	if (m_attachedObjectID != 0 || !getObject())
	{
		return;
	}
	GameLogic &logic = getObject()->logic();
	static const int structKind = ObjectTemplateInfoBuilder::kindOfIndex("STRUCTURE");
	static const int wallWalkKind = ObjectTemplateInfoBuilder::kindOfIndex("WALK_ON_TOP_OF_WALL");
	if (structKind < 0 || wallWalkKind < 0)
	{
		return;
	}

	const Coord3D &myPos = *getObject()->getPosition();
	float bestDistSq = 200.0f * 200.0f;
	ObjectID bestId = 0;

	for (Object *o = logic.getFirstObject(); o; o = o->getNextObject())
	{
		if (o == getObject() || o->isDestroyed())
		{
			continue;
		}
		if (o->isKindOf((unsigned)structKind) && o->isKindOf((unsigned)wallWalkKind))
		{
			const Coord3D &p = *o->getPosition();
			float dx = p.x - myPos.x;
			float dy = p.y - myPos.y;
			float d2 = dx * dx + dy * dy;
			if (d2 <= bestDistSq)
			{
				bestDistSq = d2;
				bestId = o->getID();
			}
		}
	}
	m_attachedObjectID = bestId;
}

// RW 0x8AEC79 / BFME2 0x4AB4AB
bool WallUpgradeUpdate::canResetUpgrades() const
{
	if (m_attachedObjectID == 0 || !getObject())
	{
		return false;
	}
	Object *att = getObject()->logic().findObjectByID(m_attachedObjectID);
	if (!att || att->isDestroyed())
	{
		return false;
	}
	static const int wallUpgradeKind = ObjectTemplateInfoBuilder::kindOfIndex("WALL_UPGRADE");
	if (wallUpgradeKind >= 0 && att->isKindOf((unsigned)wallUpgradeKind))
	{
		return false;
	}
	return true;
}

// RW 0x8AEBD3 / BFME2 0x4AB4D8
UpdateSleepTime WallUpgradeUpdate::update()
{
	UpdateSleepTime ret = UPDATE_SLEEP_FOREVER;
	if (m_firstScan)
	{
		m_firstScan = false;
		scanForBuildingAndPossess();
	}
	if (m_countdown > 0)
	{
		if (--m_countdown == 0)
		{
			m_postCountdown = true;
		}
		else
		{
			ret = UPDATE_SLEEP_NONE;
		}
	}
	if (m_postCountdown)
	{
		if (canResetUpgrades())
		{
			if (Object *obj = getObject())
			{
				obj->resetUpgrades(); // RW 0x68E083 / BFME2 0x0028DA28
				if (auto *body = obj->getBodyModule())
				{
					body->resetDamageState(0); // RW 0x8C1BF9 / BFME2 slot 9
				}
				obj->setModelConditionState(62, false); // bit 62: POST_COLLAPSE
				obj->setStatus(0x13, false);
				obj->setStatus(3, false);
				obj->setStatus(0x56, false);
				obj->setStatus(4, false);
			}
			m_postCountdown = false;
		}
		else
		{
			ret = (UpdateSleepTime)5; // BfmeLogicFramesPerSecond
		}
	}
	return ret;
}

void WallUpgradeUpdate::xfer(Xfer &x)
{
	SnapshotXfer::version(x, 1, 1);
	xferUpdateModule(x);
	x.xferObjectID(&m_attachedObjectID);
	x.xferInt(&m_countdown);
	x.xferBool(&m_firstScan);
	x.xferBool(&m_postCountdown);
}

void WallUpgradeUpdate::registerClass(ModuleFactory &modules)
{
	modules.bindTypedData<WallUpgradeUpdateModuleData>("WallUpgradeUpdate", MODULETYPE_BEHAVIOR);
	modules.bindModuleProc("WallUpgradeUpdate", MODULETYPE_BEHAVIOR,
		[](Thing *thing, const ModuleData *data, const ModuleFactory::ModuleTemplate &) -> std::unique_ptr<Module> {
			const auto *typed = dynamic_cast<const WallUpgradeUpdateModuleData *>(data);
			if (!typed)
			{
				throw std::logic_error("WallUpgradeUpdate: module data is not typed");
			}
			return std::make_unique<WallUpgradeUpdate>(thing, typed);
		});
}
