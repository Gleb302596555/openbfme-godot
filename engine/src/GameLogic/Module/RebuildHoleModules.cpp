// OpenBFME. GPL-3.0.
// See GameLogic/Module/RebuildHoleModules.h for the target facts and the addresses.

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif

#include "GameLogic/Module/RebuildHoleModules.h"
#include "GameLogic/SnapshotXfer.h"

#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/StateHash.h"
#include "Common/Thing/ModuleFactory.h"
#include "Common/Thing/ThingFactory.h"
#include "Common/Thing/ThingTemplate.h"
#include "Common/Xfer.h"
#include "GameLogic/BitFlags.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/Construction.h"
#include "GameLogic/Damage.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/SimMath.h"

#include <cstddef>
#include <stdexcept>

namespace
{
const char *const kStop =
	"[S-5603] RebuildHoleExposeDie / RebuildHoleBehavior (RW 0x889AAF, 0x8868C2) are ported for a hole WITHOUT WorkerObjectName (the creep lairs): the dead structure leaves its "
	"hole, the hole is rebuilt complete at once after WorkerRespawnDelay, a destroyed hole makes its treasure. NOT ported: the worker branch (WorkerObjectName: the Wild and "
	"Imladris holes stay until destroyed), TransferAttackers, the dead object's GeometryInfo on the hole, the spawner's active flag (a rebuilt spawner is active) and the "
	"hole's fade-in";

#define DM_ROWS(T)                                                                                                                                       \
	{ "DeathTypes", ParseDeathTypeFlags, nullptr, (int)offsetof(T, m_dieMux.m_deathTypes) },                                                              \
	{ "ExemptStatus", ParseObjectStatusMask, nullptr, (int)offsetof(T, m_dieMux.m_exemptStatus) },                                                        \
	{ "RequiredStatus", ParseObjectStatusMask, nullptr, (int)offsetof(T, m_dieMux.m_requiredStatus) },                                                    \
	{ "DamageAmountRequired", INI::parseReal, nullptr, (int)offsetof(T, m_dieMux.m_damageAmountRequired) },                                               \
	{ "MinKillerAngle", INI::parseAngleReal, nullptr, (int)offsetof(T, m_dieMux.m_minKillerAngle) },                                                      \
	{ "MaxKillerAngle", INI::parseAngleReal, nullptr, (int)offsetof(T, m_dieMux.m_maxKillerAngle) },                                                      \
	{ nullptr, nullptr, nullptr, 0 }
const FieldParse kExposeDieMux[] = { DM_ROWS(RebuildHoleExposeDieModuleData) };
#undef DM_ROWS

#define RHE_OFF(member) (int)offsetof(RebuildHoleExposeDieModuleData, member)
const FieldParse kExposeDieFieldParse[] = { // RW 0xC616B8
	{ "HoleName", INI::parseAsciiString, nullptr, RHE_OFF(m_holeName) },
	{ "HoleMaxHealth", INI::parseReal, nullptr, RHE_OFF(m_holeMaxHealth) },
	{ "TransferAttackers", INI::parseBool, nullptr, RHE_OFF(m_transferAttackers) },
	{ "FadeInTimeSeconds", INI::parseReal, nullptr, RHE_OFF(m_fadeInTimeSeconds) },
	{ nullptr, nullptr, nullptr, 0 }
};
#undef RHE_OFF

#define RHB_OFF(member) (int)offsetof(RebuildHoleBehaviorModuleData, member)
const FieldParse kHoleFieldParse[] = { // RW 0xC60230
	{ "WorkerObjectName", INI::parseAsciiString, nullptr, RHB_OFF(m_workerName) },
	{ "WorkerRespawnDelay", INI::parseDurationReal, nullptr, RHB_OFF(m_workerRespawnDelay) },
	{ "HoleHealthRegen%PerSecond", INI::parsePercentToReal, nullptr, RHB_OFF(m_regenPercentPerSecond) },
	{ nullptr, nullptr, nullptr, 0 }
};
#undef RHB_OFF

template <class Runtime, class Data>
void bindRuntime(ModuleFactory &modules, const char *name)
{
	modules.bindTypedData<Data>(name, MODULETYPE_BEHAVIOR);
	modules.bindModuleProc(name, MODULETYPE_BEHAVIOR, [name](Thing *thing, const ModuleData *data, const ModuleFactory::ModuleTemplate &) -> std::unique_ptr<Module> {
		const Data *typed = dynamic_cast<const Data *>(data);
		if (!typed)
		{
			throw std::logic_error(std::string(name) + ": the module data is not typed");
		}
		return std::make_unique<Runtime>(thing, typed);
	});
}
} // namespace

// ---- RebuildHoleExposeDie -----------------------------------------------------------------------------------------------------------------------
void RebuildHoleExposeDieModuleData::buildFieldParse(MultiIniFieldParse &p)
{
	p.add(kExposeDieMux); // RW 0xC76BD8
	p.add(kExposeDieFieldParse);
}

RebuildHoleExposeDie::RebuildHoleExposeDie(Thing *thing, const RebuildHoleExposeDieModuleData *data)
	: BehaviorModule(thing, data)
	, m_data(data)
{
}

const char *RebuildHoleExposeDie::stopLine()
{
	return kStop;
}

// RW 0x889AAF
void RebuildHoleExposeDie::onDie(const Event &event)
{
	Object *obj = getObject();
	if (!m_data->m_dieMux.isDieApplicable(*obj, event)) // RW 0x85FED5
	{
		return;
	}
	GameLogic &logic = obj->logic();
	Player *owner = obj->getControllingPlayer();
	if (!owner || owner == logic.players().getNeutralPlayer() || owner->isObserver() || owner->isDefeated()) // RW 0x889AE5, 0x6AAC52
	{
		return;
	}
	if (obj->isUnderConstruction()) // object + 0x94 bit 2
	{
		return;
	}
	const ThingTemplate *holeTemplate = logic.things().findTemplate(m_data->m_holeName); // RW 0x6D1305
	if (!holeTemplate || !obj->getTeam())
	{
		return;
	}
	Object *hole = logic.newObject(holeTemplate, obj->getTeam(), ObjectStatusMaskType{}); // RW 0x6D165E
	if (!hole)
	{
		return;
	}
	hole->setPosition(obj->getPosition());      // RW 0x70C201
	hole->setOrientation(obj->getOrientation()); // RW 0x70C31E
	if (!obj->getName().empty())
	{
		hole->setName(obj->getName()); // RW 0x759467: the script name moves to the hole
		obj->setName("No Name");
	}
	if (BodyModuleInterface *body = hole->getBodyModule())
	{
		body->setMaxHealth(m_data->m_holeMaxHealth, 0); // body slot 0x5C
	}
	if (RebuildHoleBehavior *rebuild = RebuildHoleBehavior::of(*hole)) // RW 0x886448
	{
		rebuild->setRebuildData(obj->getTemplate(), obj->getID(), obj->getBuildCostPaid()); // RW 0x886831
	}
	logic.noteStop(kStop);
}

void RebuildHoleExposeDie::xfer(Xfer &x)
{
	SnapshotXfer::version(x, 1, 1);
	xferBehaviorModule(x);
}

void RebuildHoleExposeDie::registerClass(ModuleFactory &modules)
{
	bindRuntime<RebuildHoleExposeDie, RebuildHoleExposeDieModuleData>(modules, "RebuildHoleExposeDie");
}

// ---- RebuildHoleBehavior --------------------------------------------------------------------------------------------------------------------------
void RebuildHoleBehaviorModuleData::buildFieldParse(MultiIniFieldParse &p)
{
	p.add(kHoleFieldParse);
}

RebuildHoleBehavior::RebuildHoleBehavior(Thing *thing, const RebuildHoleBehaviorModuleData *data)
	: UpdateModule(thing, data)
	, m_data(data)
{
	friend_setNextCallFrame((UnsignedInt)UPDATE_SLEEP_FOREVER); // until the die hands it the structure (setRebuildData wakes it)
}

RebuildHoleBehavior *RebuildHoleBehavior::of(const Object &obj)
{
	return dynamic_cast<RebuildHoleBehavior *>(obj.findModule("RebuildHoleBehavior"));
}

// RW 0x886831 and the reset RW 0x8865F7
void RebuildHoleBehavior::setRebuildData(const ThingTemplate *what, ObjectID spawnerId, float pricePaid)
{
	m_template = what;
	m_spawnerId = spawnerId;
	m_pricePaid = pricePaid;
	reset();
}

void RebuildHoleBehavior::reset()
{
	m_countdown = (unsigned)SimMath::truncToInt32(m_data->m_workerRespawnDelay); // fld; ftol
	m_rebuiltId = INVALID_ID;
	setWakeFrame(getObject(), UPDATE_SLEEP(1));
}

// RW 0x8868C2
UpdateSleepTime RebuildHoleBehavior::update()
{
	static const unsigned kSupplySource = (unsigned)CombatNames::kindOf("SUPPLY_SOURCE");
	Object *hole = getObject();
	GameLogic &logic = hole->logic();
	if (!m_template)
	{
		return UPDATE_SLEEP_FOREVER;
	}
	Object *rebuilt = m_rebuiltId != INVALID_ID ? logic.findObjectByID(m_rebuiltId) : nullptr;
	if (m_rebuiltId != INVALID_ID && !rebuilt)
	{
		reset(); // RW 0x88691C: the rebuilt object is gone, the hole counts down again
		rebuilt = nullptr;
	}
	if (!rebuilt && m_countdown > 0 && --m_countdown == 0) // RW 0x886934
	{
		if (!m_data->m_workerName.empty())
		{
			logic.noteStop(kStop); // the worker branch (RW 0x886A01) is not ported: the hole stays
			return UPDATE_SLEEP_FOREVER;
		}
		Player *owner = hole->getControllingPlayer();
		Object *made = owner ? Construction::buildObjectNow(logic, hole, *m_template, *hole->getPosition(), hole->getOrientation(), *owner) : nullptr; // BuildAssistant slot 0x38
		if (!made)
		{
			return UPDATE_SLEEP_FOREVER; // RW 0x886AB9: the site is blocked, the hole waits for ever
		}
		m_rebuiltId = made->getID();   // RW 0x886B1E
		made->setProducer(hole);       // RW 0x68B6A1
		made->setBuildCostPaid(m_pricePaid); // RW 0x886B2F: object + 0x33C
		rebuilt = made;
	}
	// RW 0x886B77: the hole heals while it waits
	if (BodyModuleInterface *body = hole->getBodyModule())
	{
		if (body->getMaxHealth() > body->getHealth())
		{
			DamageInfo heal;
			heal.m_input.m_damageType = DAMAGE_HEALING;
			heal.m_input.m_deathType = DEATH_NONE;
			heal.m_input.m_sourceID = hole->getID();
			const float perFrame = SimMath::divf32(m_data->m_regenPercentPerSecond, 5.0f); // LOGICFRAMES_PER_SECOND (RW 0xD9F608)
			heal.m_input.m_amount = SimMath::mulf32(perFrame, body->getMaxHealth());
			body->attemptHealing(heal);
		}
	}
	// RW 0x886BD8: a rebuilt structure that stands takes the script name back and the hole fades away (FADED: no treasure)
	if (rebuilt)
	{
		const bool supply = rebuilt->isKindOf(kSupplySource);
		if (!supply && rebuilt->isUnderConstruction())
		{
			return UPDATE_SLEEP_NONE;
		}
		if (!supply && rebuilt->isEffectivelyDead())
		{
			return UPDATE_SLEEP_NONE;
		}
		if (!hole->getName().empty() && hole->getName() != "No Name")
		{
			rebuilt->setName(hole->getName()); // RW 0x759467
			hole->setName("No Name");
		}
		hole->kill(DAMAGE_UNRESISTABLE, DEATH_FADED); // RW 0x698EC3(8, 0x16)
	}
	return UPDATE_SLEEP_NONE;
}

void RebuildHoleBehavior::crc(StateHasher &h) const
{
	UpdateModule::crc(h);
	h.addU32((std::uint32_t)m_spawnerId);
	h.addU32((std::uint32_t)m_rebuiltId);
	h.addU32(m_countdown);
	h.addFloat(m_pricePaid);
}

// OpenBFME native layout (S-4190): UpdateModule's, the template's name, the ids, the countdown and the price
void RebuildHoleBehavior::xfer(Xfer &x)
{
	SnapshotXfer::version(x, 1, 1);
	xferUpdateModule(x);
	std::string name = m_template ? m_template->getName() : std::string();
	x.xferAsciiString(&name);
	if (x.isLoading())
	{
		m_template = nullptr;
		if (!name.empty())
		{
			m_template = getObject()->logic().things().findTemplate(name);
			if (!m_template)
			{
				throw std::runtime_error("a RebuildHoleBehavior's template " + name + " is unknown");
			}
		}
	}
	x.xferObjectID(&m_spawnerId);
	x.xferObjectID(&m_rebuiltId);
	x.xferUnsignedInt(&m_countdown);
	x.xferReal(&m_pricePaid);
}

void RebuildHoleBehavior::registerClass(ModuleFactory &modules)
{
	bindRuntime<RebuildHoleBehavior, RebuildHoleBehaviorModuleData>(modules, "RebuildHoleBehavior");
}
