// OpenBFME. GPL-3.0.
// See GameLogic/Module/MoneyEventModules.h for the target facts and the addresses.

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif

#include "GameLogic/Module/MoneyEventModules.h"
#include "GameLogic/SnapshotXfer.h"

#include "Common/AsciiString.h"
#include "Common/NumericState.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/PlayerTemplate.h"
#include "Common/RandomValue.h"
#include "Common/Science.h"
#include "Common/StateHash.h"
#include "Common/Thing/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "GameLogic/AI/AIGarrisonStates.h"
#include "GameLogic/Economy.h"
#include "GameLogic/FXEvents.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/ObjectFilter.h"
#include "GameLogic/ObjectFilterMatch.h"
#include "GameLogic/ObjectTemplateInfo.h"
#include "GameLogic/Object/ExperienceTracker.h"
#include "GameLogic/Object/Object.h"

#include <cstddef>

namespace
{
void parseOptionalObjectFilter(INI *ini, void *instance, void *store, const void *)
{
	ObjectFilter f;
	ParseObjectFilter(ini, instance, &f, nullptr);
	*static_cast<std::shared_ptr<const ObjectFilter> *>(store) = std::make_shared<const ObjectFilter>(std::move(f));
}
void parseUpgradeName(INI *ini, void *, void *store, const void *)
{
	const std::string name = ini->getNextAsciiString();
	*static_cast<std::string *>(store) = (name == "None" || name == "NONE" || name == "none") ? std::string() : name;
}

#define RD_OFF(member) (int)offsetof(RefundDieModuleData, member)
const FieldParse kRefundDieFieldParse[] = {
	{ "DeathTypes", ParseDeathTypeFlags, nullptr, RD_OFF(m_dieMux.m_deathTypes) },
	{ "ExemptStatus", ParseObjectStatusMask, nullptr, RD_OFF(m_dieMux.m_exemptStatus) },
	{ "RequiredStatus", ParseObjectStatusMask, nullptr, RD_OFF(m_dieMux.m_requiredStatus) },
	{ "DamageAmountRequired", INI::parseReal, nullptr, RD_OFF(m_dieMux.m_damageAmountRequired) },
	{ "MinKillerAngle", INI::parseAngleReal, nullptr, RD_OFF(m_dieMux.m_minKillerAngle) },
	{ "MaxKillerAngle", INI::parseAngleReal, nullptr, RD_OFF(m_dieMux.m_maxKillerAngle) },
	{ "UpgradeRequired", parseUpgradeName, nullptr, RD_OFF(m_upgradeRequired) },
	{ "BuildingRequired", parseOptionalObjectFilter, nullptr, RD_OFF(m_buildingRequired) },
	{ "RefundPercent", INI::parsePercentToReal, nullptr, RD_OFF(m_refundPercent) },
	{ nullptr, nullptr, nullptr, 0 }
};
#undef RD_OFF

#define PM_OFF(member) (int)offsetof(PillageModuleData, member)
const FieldParse kPillageFieldParse[] = {
	{ "PillageAmount", INI::parseUnsignedInt, nullptr, PM_OFF(m_pillageAmount) },
	{ "NumDamageEventsPerPillage", INI::parseUnsignedInt, nullptr, PM_OFF(m_numDamageEventsPerPillage) },
	{ "PillageFilter", parseOptionalObjectFilter, nullptr, PM_OFF(m_pillageFilter) },
	{ nullptr, nullptr, nullptr, 0 }
};
#undef PM_OFF

#define SC_OFF(member) (int)offsetof(SalvageCrateCollideModuleData, member)
const FieldParse kSalvageCrateFieldParse[] = {
	{ "RequiredKindOf", ParseKindOfMask, nullptr, SC_OFF(m_requiredKindOf) },
	{ "ForbiddenKindOf", ParseKindOfMask, nullptr, SC_OFF(m_forbiddenKindOf) },
	{ "ForbidOwnerPlayer", INI::parseBool, nullptr, SC_OFF(m_forbidOwnerPlayer) },
	{ "BuildingPickup", INI::parseBool, nullptr, SC_OFF(m_buildingPickup) },
	{ "HumanOnly", INI::parseBool, nullptr, SC_OFF(m_humanOnly) },
	{ "PickupScience", INI::parseAsciiString, nullptr, SC_OFF(m_pickupScience) },
	{ "ExecuteFX", INI::parseAsciiString, nullptr, SC_OFF(m_executeFX) },
	{ "ExecuteAnimation", INI::parseAsciiString, nullptr, SC_OFF(m_executeAnimation) },
	{ "ExecuteAnimationTime", INI::parseReal, nullptr, SC_OFF(m_executeAnimationTime) },
	{ "ExecuteAnimationZRise", INI::parseReal, nullptr, SC_OFF(m_executeAnimationZRise) },
	{ "ExecuteAnimationFades", INI::parseBool, nullptr, SC_OFF(m_executeAnimationFades) },
	{ "PorterChance", INI::parsePercentToReal, nullptr, SC_OFF(m_porterChance) },
	{ "BannerChance", INI::parsePercentToReal, nullptr, SC_OFF(m_bannerChance) },
	{ "LevelUpChance", INI::parsePercentToReal, nullptr, SC_OFF(m_levelUpChance) },
	{ "LevelUpRadius", INI::parsePercentToReal, nullptr, SC_OFF(m_levelUpRadius) },
	{ "ResourceChance", INI::parsePercentToReal, nullptr, SC_OFF(m_resourceChance) },
	{ "Upgrade", INI::parseAsciiString, nullptr, SC_OFF(m_upgrade) },
	{ "MinResource", INI::parseInt, nullptr, SC_OFF(m_minResource) },
	{ "MaxResource", INI::parseInt, nullptr, SC_OFF(m_maxResource) },
	{ "AllowAIPickup", INI::parseBool, nullptr, SC_OFF(m_allowAIPickup) },
	{ nullptr, nullptr, nullptr, 0 }
};
#undef SC_OFF

// RW 0x661317: A has every bit of `required` and none of `exempt`
bool statusTest(const ObjectStatusMaskType &a, const ObjectStatusMaskType &required, const ObjectStatusMaskType &exempt)
{
	for (size_t i = 0; i < a.size(); ++i)
	{
		if ((exempt[i] & a[i]) != 0 || (required[i] & a[i]) != required[i])
		{
			return false;
		}
	}
	return true;
}
} // namespace

void RefundDieModuleData::buildFieldParse(MultiIniFieldParse &p)
{
	p.add(kRefundDieFieldParse);
}

void PillageModuleData::buildFieldParse(MultiIniFieldParse &p)
{
	p.add(kPillageFieldParse);
}

void SalvageCrateCollideModuleData::buildFieldParse(MultiIniFieldParse &p)
{
	p.add(kSalvageCrateFieldParse);
}

bool DieMuxData::isDieApplicable(const Object &obj, const DieModuleInterface::Event &event, bool *angleNotPorted) const
{
	// RW 0x8D29B6 .. 0x8D29C1: `shl eax, cl` takes the count modulo 32
	if (!(m_deathTypes & (1u << ((unsigned)(event.deathType - 1) & 31u))))
	{
		return false;
	}
	if (!statusTest(obj.getStatusBits(), m_requiredStatus, m_exemptStatus))
	{
		return false;
	}
	if (m_damageAmountRequired >= 0.0f && m_damageAmountRequired > event.damageAmount)
	{
		return false; // RW 0x8D29EC .. 0x8D29FE
	}
	if (m_maxKillerAngle > m_minKillerAngle)
	{
		if (angleNotPorted)
		{
			*angleNotPorted = true; // the killer's bearing test (RW 0x8D2A0F .. 0x8D2AAC) is not ported
		}
		return true;
	}
	return true;
}

// ---- RefundDie -----------------------------------------------------------------------------------------------------------------------
RefundDie::RefundDie(Thing *thing, const RefundDieModuleData *data)
	: BehaviorModule(thing, data)
	, m_data(data)
{
}

int RefundDie::refundAmount(const DieModuleInterface::Event &event) const
{
	const Object *obj = getObject();
	bool angle = false;
	if (!m_data->m_dieMux.isDieApplicable(*obj, event, &angle))
	{
		return 0;
	}
	if (angle)
	{
		obj->logic().reportError("RefundDie of " + obj->getTemplate()->getName() + ": MinKillerAngle / MaxKillerAngle are not ported (the die applies as if the killer were in the window)");
	}
	const int underConstruction = ObjectTemplateInfoBuilder::objectStatusIndex("UNDER_CONSTRUCTION");
	const int sold = ObjectTemplateInfoBuilder::objectStatusIndex("SOLD");
	if ((underConstruction >= 0 && obj->testStatus((unsigned)underConstruction)) || (sold >= 0 && obj->testStatus((unsigned)sold)))
	{
		return 0; // RW 0x888548 / 0x888559
	}
	Player *owner = obj->getControllingPlayer();
	if (!owner)
	{
		return 0;
	}
	if (!m_data->m_upgradeRequired.empty() && !owner->hasUpgradeComplete(m_data->m_upgradeRequired))
	{
		return 0;
	}
	if (ObjectFilterMatch::isValid(m_data->m_buildingRequired.get()) && !owner->hasObjectMatching(*m_data->m_buildingRequired, false))
	{
		return 0;
	}
	// RW 0x8885BF .. 0x8885E4: fld paid; fld percent; fmul; fstp qword; ceil; fstp dword; fld; fistp
	const double product = NumericState::pc24MulW((double)obj->getBuildCostPaid(), (double)m_data->m_refundPercent);
	return NumericState::fistp32((double)NumericState::fstpDword(NumericState::ceilD(product)));
}

void RefundDie::onDie(const DieModuleInterface::Event &event)
{
	const int refund = refundAmount(event);
	if (refund == 0)
	{
		return;
	}
	Object *obj = getObject();
	Player *owner = obj->getControllingPlayer();
	owner->depositMoney((std::uint32_t)refund, true); // RW 0x8885FF
	obj->logic().economy().noteIncome(*obj, *owner, refund);
}

// ---- PillageModule -------------------------------------------------------------------------------------------------------------------
PillageModule::PillageModule(Thing *thing, const PillageModuleData *data)
	: BehaviorModule(thing, data)
	, m_data(data)
{
}

void PillageModule::onDamageDealt(Object &victim)
{
	Object *self = getObject();
	const ObjectFilter filter = m_data->m_pillageFilter ? *m_data->m_pillageFilter : ObjectFilter();
	if (!ObjectFilterMatch::allows(self->logic(), filter, victim, nullptr))
	{
		return;
	}
	++m_counter;
	if (m_counter < m_data->m_numDamageEventsPerPillage)
	{
		return;
	}
	m_counter = 0;
	Player *victimOwner = victim.getControllingPlayer();
	Player *attackerOwner = self->getControllingPlayer();
	if (!victimOwner || !attackerOwner)
	{
		return;
	}
	const std::uint32_t cash = victimOwner->getMoney()->countMoney();
	const std::uint32_t amount = m_data->m_pillageAmount < cash ? m_data->m_pillageAmount : cash; // cmovb
	const std::uint32_t taken = victimOwner->withdrawMoney(amount, true);                         // RW 0x8882F5
	if (taken == 0)
	{
		return;
	}
	attackerOwner->depositMoney(taken, true); // RW 0x888314
	self->logic().economy().noteIncome(*self, *attackerOwner, (int)taken, Economy::IncomeEvent::TEXT_RAISED_10, 1.3f); // 1.3: the owner's size for this text (P-19), not retail
}

void PillageModule::crc(StateHasher &h) const
{
	h.addU32(m_counter);
}

// ---- SalvageCrateCollide (lane MODGAP-1, P-10, stop S-260; the facts are in the header) ------------------------------------------------------------------
namespace
{
// RW 0x70C4FE Object::isKindOfMulti(must, mustNot): every bit of `must` and none of `mustNot`
bool kindOfMulti(const Object &o, const KindOfMaskType &must, const KindOfMaskType &mustNot)
{
	const KindOfMaskType &k = o.getKindOf();
	for (size_t i = 0; i < k.size(); ++i)
	{
		if ((k[i] & must[i]) != must[i] || (k[i] & mustNot[i]) != 0)
		{
			return false;
		}
	}
	return true;
}

const char *const kSalvageStop =
	"[S-260] SalvageCrateCollide (RW 0x8C06B6 onCollide, 0x8C080B / 0x8C145D validity, 0x8C136C outcome, 0x8C158B execute, 0x8C148B money) pays money, gives its player upgrade "
	"and a rank, plays ExecuteFX and removes the crate; NOT ported: the client's GUI:GainRank text and ExecuteAnimation (RW 0x8C13BB / 0x6A1875), the units-in-range branch of "
	"RW 0x8C168D (the outcome function never answers 0 or 1: INFERENCE); INFERENCE: the touches are AIWorld::processStaticContacts' planar circles of the crate and of each "
	"mobile AI unit, not RW's partition contacts (S-581, S-220)";
} // namespace

SalvageCrateCollide::SalvageCrateCollide(Thing *thing, const SalvageCrateCollideModuleData *data)
	: BehaviorModule(thing, data)
	, m_data(data)
{
}

const char *SalvageCrateCollide::stopLine()
{
	return kSalvageStop;
}

// RW 0x8C080B: CrateCollide::isValidToExecute
bool SalvageCrateCollide::crateIsValid(const Object &other) const
{
	static const unsigned kStructure = (unsigned)ObjectTemplateInfoBuilder::kindOfIndex("STRUCTURE"); // template + 0x108 bit 7
	static const unsigned kCreep = (unsigned)ObjectTemplateInfoBuilder::kindOfIndex("CREEP");         // template + 0x11D bit 6
	if (m_executed)
	{
		return false; // RW 0x8C081E
	}
	const Object *crate = getObject();
	Player *otherOwner = other.getControllingPlayer();
	if (otherOwner == crate->logic().players().getNeutralPlayer())
	{
		return false; // RW 0x8C0826 (Object::isOwnedByNeutral, RW 0x68B760)
	}
	const bool buildingAttempt = m_data->m_buildingPickup && other.isKindOf(kStructure); // RW 0x8C0834 .. 0x8C0849
	if (!other.getAIUpdateInterface() && !buildingAttempt)
	{
		return false; // RW 0x8C084B
	}
	if (other.isKindOf(kCreep))
	{
		const PlayerTemplate *pt = otherOwner ? otherOwner->getPlayerTemplate() : nullptr;
		if (!pt || !pt->m_playableSide)
		{
			return false; // RW 0x8C085C .. 0x8C0880 (RW 0x6AAC66: the player's template is a playable side)
		}
	}
	if (!kindOfMulti(other, m_data->m_requiredKindOf, m_data->m_forbiddenKindOf))
	{
		return false; // RW 0x8C0886
	}
	if (other.isEffectivelyDead())
	{
		return false; // RW 0x8C0899
	}
	if (GarrisonRules::isAboveTerrain(*crate) && !buildingAttempt)
	{
		return false; // RW 0x8C08A2
	}
	if (m_data->m_forbidOwnerPlayer && crate->getControllingPlayer() == otherOwner)
	{
		return false; // RW 0x8C08B2
	}
	if (m_data->m_humanOnly && otherOwner && otherOwner->getPlayerType() == PLAYER_COMPUTER)
	{
		return false; // RW 0x8C08CD (player + 0x5C)
	}
	if (!m_data->m_pickupScience.empty() && AsciiStringUtil::compareNoCase(m_data->m_pickupScience, "None") != 0)
	{
		const ScienceType science = TheScienceStore ? TheScienceStore->getScienceFromInternalName(m_data->m_pickupScience) : SCIENCE_INVALID;
		if (science == SCIENCE_INVALID)
		{
			crate->logic().reportError("SalvageCrateCollide of " + crate->getTemplate()->getName() + ": PickupScience " + m_data->m_pickupScience + " is not a science");
			return false;
		}
		if (otherOwner && !otherOwner->hasScience(science))
		{
			return false; // RW 0x8C08EB .. 0x8C090F
		}
	}
	return true; // RW 0x8C0915: the crate's undetected-defector bit (+ 0x458 bit 1) is never set (S-141)
}

// RW 0x8C145D
bool SalvageCrateCollide::isValidToExecute(const Object *other) const
{
	static const unsigned kImmobile = (unsigned)ObjectTemplateInfoBuilder::kindOfIndex("IMMOBILE"); // template + 0x108 bit 2
	if (!other || !crateIsValid(*other))
	{
		return false;
	}
	return other->getAIUpdateInterface() != nullptr && !other->isKindOf(kImmobile); // RW 0x8C146C .. 0x8C1481
}

// RW 0x8C136C: GameLogicRandomValueReal(0, 1) always draws
int SalvageCrateCollide::chooseOutcome() const
{
	const float roll = getObject()->logic().random().getValueReal(0.0f, 1.0f, "SalvageCrateCollide.cpp", 0x45); // GetGameLogicRandomValueReal
	if (m_data->m_levelUpChance > roll)
	{
		return 2; // a rank
	}
	return m_data->m_upgrade.empty() ? 3 : 4;
}

// RW 0x8C148B
void SalvageCrateCollide::doMoney(Object &other)
{
	const int lo = m_data->m_minResource, hi = m_data->m_maxResource;
	Object *crate = getObject();
	GameLogic &logic = crate->logic();
	int amount = lo == hi ? lo : logic.random().getValue(lo, hi, "SalvageCrateCollide.cpp", 0xDB); // GetGameLogicRandomValue
	if (amount <= 0)
	{
		return; // RW 0x8C14C5
	}
	Economy &economy = logic.economy();
	if (economy.isMultiplayerGame())
	{
		// RW 0x8C14CE .. 0x8C1508: fild amount; fmul MultiPlayMoneyMult[live playable - 1] (PC24); _ftol
		const float mult = economy.multiPlayMoneyMult(economy.livePlayableCount(false));
		amount = (int)NumericState::ftol2Low32(NumericState::pc24MulW((double)amount, (double)mult));
	}
	Player *owner = other.getControllingPlayer();
	if (!owner)
	{
		return;
	}
	amount = economy.applyHandicapMoney(*owner, amount); // RW 0x8C151C (Player::ScaleMoney)
	owner->depositMoney((std::uint32_t)amount, true);     // RW 0x8C1533 (RW 0x7B18B8: the player's score keeper)
	economy.noteIncome(*crate, *owner, amount);           // RW 0x8C156A: GUI:AddCash, 10 above the crate (RW 0xBD83D8) in the owner's colour
}

// RW 0x8C158B
bool SalvageCrateCollide::executeCrateBehavior(Object &other)
{
	if (!m_data->m_allowAIPickup)
	{
		Player *p = other.getControllingPlayer();
		if (p && p->getPlayerType() == PLAYER_COMPUTER)
		{
			return false; // RW 0x8C159F .. 0x8C15C0
		}
	}
	switch (chooseOutcome())
	{
	case 2:
		if (ExperienceTracker *tracker = other.getExperienceTracker())
		{
			tracker->gainExpForLevel(1, true, false); // RW 0x8C1640 (RW 0x79DA0A); the client text GUI:GainRank (RW 0x8C167C) is not ported (stop line)
		}
		break;
	case 4:
	{
		const UpgradeTemplate *upgrade = TheUpgradeCenter ? TheUpgradeCenter->findUpgrade(m_data->m_upgrade) : nullptr; // RW 0x8C15F8 (RW 0x66F5E5)
		if (upgrade && upgrade->getUpgradeType() == UPGRADE_TYPE_PLAYER)
		{
			if (Player *owner = other.getControllingPlayer())
			{
				owner->addUpgrade(upgrade, Player::UPGRADE_STATUS_COMPLETE, false); // RW 0x8C161B (RW 0x6AEE22(upgrade, 2, 0))
			}
		}
		break;
	}
	default:
		doMoney(other);
		break;
	}
	return true;
}

// RW 0x8C06B6: CrateCollide::onCollide
void SalvageCrateCollide::onCollide(Object *other, const Coord3D *, const Coord3D *)
{
	if (!other || !isValidToExecute(other))
	{
		return;
	}
	if (!executeCrateBehavior(*other))
	{
		return;
	}
	Object *crate = getObject();
	GameLogic &logic = crate->logic();
	if (!m_data->m_executeFX.empty())
	{
		// RW 0x8C06E2 .. 0x8C06ED: FXList::doFXObj(ExecuteFX, other, null)
		logic.fxEvents().emit(FXEventLog::objectEvent(FXEvent::OBJECT_FX, "SalvageCrateCollide ExecuteFX", logic.getFrame(), m_data->m_executeFX, *other));
	}
	m_executed = true;         // RW 0x8C06F8
	logic.destroyObject(crate); // RW 0x8C0702 (RW 0x62BBAB)
	logic.noteStop(kSalvageStop);
}

void SalvageCrateCollide::crc(StateHasher &h) const
{
	h.addBool(m_executed);
}

// ---- lane SAVE-4b: the snapshots (OpenBFME native, S-4190) ------------------------------------------------------------------------------------------------
void RefundDie::xfer(Xfer &x)
{
	SnapshotXfer::version(x, 1, 1);
	xferBehaviorModule(x);
}

void PillageModule::xfer(Xfer &x)
{
	SnapshotXfer::version(x, 1, 1);
	xferBehaviorModule(x);
	x.xferUnsignedInt(&m_counter);
}

// RW 0x8C0760: version 2, the base module, the executed byte (OpenBFME native layout: version 1 holds it from the start)
void SalvageCrateCollide::xfer(Xfer &x)
{
	SnapshotXfer::version(x, 1, 1);
	xferBehaviorModule(x);
	x.xferBool(&m_executed);
}
