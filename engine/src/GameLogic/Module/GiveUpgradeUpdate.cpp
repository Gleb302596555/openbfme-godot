// OpenBFME. GPL-3.0. See GiveUpgradeUpdate.h for edition sources and S-6011.
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif
#include "GameLogic/Module/GiveUpgradeUpdate.h"
#include "Common/AsciiString.h"
#include "Common/Thing/ModuleFactory.h"
#include "Common/Upgrade.h"
#include "GameLogic/FXEvents.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/Object/PartitionManager.h"
#include "GameLogic/SnapshotXfer.h"
#include <cstddef>
#include <stdexcept>

namespace {
using Data = GiveUpgradeUpdateModuleData;
void parseName(INI *ini, void *, void *store, const void *)
{
	const std::string value = ini->getNextToken();
	*static_cast<std::string *>(store) = AsciiStringUtil::compareNoCase(value, "None") == 0 ? std::string() : value;
}
#define GIVE_OFF(m) (int)offsetof(Data, m)
const FieldParse kFields[] = {
	{ "GiveUpgradeEffect", parseName, nullptr, GIVE_OFF(m_giveFX) },
	{ "SpawnOutFX", parseName, nullptr, GIVE_OFF(m_spawnFX) },
	{ "FadeOutSpeed", INI::parseReal, nullptr, GIVE_OFF(m_fadeSpeed) },
	{ "DeliverUpgrade", INI::parseBool, nullptr, GIVE_OFF(m_deliver) },
	{ nullptr, nullptr, nullptr, 0 }
};
#undef GIVE_OFF
}

void Data::buildFieldParse(MultiIniFieldParse &p) { SpecialAbilityUpdateModuleData::buildFieldParse(p); p.add(kFields); }
GiveUpgradeUpdate::GiveUpgradeUpdate(Thing *thing, const Data *data) : SpecialAbilityUpdate(thing, data), m_giveData(data) {}
const char *GiveUpgradeUpdate::stopLine() { return "[S-6011] GiveUpgradeUpdate: automatic search scheduling is unverified; contained-target providers, paired reservations, selection and delivery FX are not ported (RW 0x89FE64 / 0x8A01B2; BFME2 Rva 0x49C6E1 / 0x49CA2F)"; }
const UpgradeTemplate *GiveUpgradeUpdate::upgrade() const
{
	if (!TheUpgradeCenter) return nullptr;
	const UpgradeMaskType &mask = getObject()->getUpgradeMask();
	for (unsigned bit = 0; bit < UpgradeMaskType::BITS; ++bit)
		if (mask.test(bit)) if (const UpgradeTemplate *u = TheUpgradeCenter->findUpgradeByMaskBit((int)bit)) return u;
	return nullptr;
}
void GiveUpgradeUpdate::onObjectCreated()
{
	if (m_giveData->m_deliver) { getObject()->logic().noteStop(stopLine()); m_searching = true; setWakeFrame(getObject(), UPDATE_SLEEP(1)); }
}
void GiveUpgradeUpdate::initiateIntentToDoSpecialPower(const SpecialPowerTemplate *, Object *, const Coord3D *, unsigned, int)
{
	// RW 0x8A006C: find delivery target and schedule delivery
	findDeliveryTarget();
}
void GiveUpgradeUpdate::findDeliveryTarget()
{
	Object *owner = getObject();
	const UpgradeTemplate *u = upgrade();
	if (!u || !m_giveData->m_specialPowerTemplate) return;
	GameLogic &logic = owner->logic();
	PartitionFilterFn eligible([&](Object &o) {
		return &o != owner && owner->getRelationship(o) == ALLIES && !o.isEffectivelyDead() && !o.testStatus(0x42)
			&& !o.isKindOfName("WALL_UPGRADE") && o.affectedByUpgrade(u); // RW 0x89FFAF, KindOf 150
	});
	for (const PartitionHit &hit : logic.partition().iterateObjectsInRange(*owner->getPosition(), 100000.0f, FROM_CENTER_2D, { &eligible }, ITER_SORTED_NEAR_TO_FAR))
	{
		Object *candidate = hit.object;
		Object *producer = logic.findObjectByID(candidate->getProducerID());
		// RW 0x89FFC8: a producer that is not a structure is treated as a
		// linked object; those paired reservations are not yet reproduced.
		if (candidate->getContainedBy() || (producer && !producer->isKindOfName("STRUCTURE"))) { logic.noteStop(stopLine()); continue; }
		candidate->setStatus(0x42, true);
		SpecialAbilityUpdate::initiateIntentToDoSpecialPower(m_giveData->m_specialPowerTemplate, candidate, nullptr, 0, 0);
		return;
	}
}
void GiveUpgradeUpdate::releaseTarget()
{
	if (Object *t = target()) t->setStatus(0x42, false);
}
void GiveUpgradeUpdate::finishAbility(bool ownerDying, bool aborted)
{
	releaseTarget();
	SpecialAbilityUpdate::finishAbility(ownerDying, aborted);
}
void GiveUpgradeUpdate::triggerAbilityEffect()
{
	if (m_fading) return;
	SpecialAbilityUpdate::triggerAbilityEffect();
	Object *owner = getObject();
	Object *recipient = target();
	const UpgradeTemplate *u = upgrade();
	if (!recipient || recipient->isEffectivelyDead() || !u) return;
	Object *producer = owner->logic().findObjectByID(recipient->getProducerID());
	if (recipient->getContainedBy() || (producer && producer->getContain() && producer->getContain()->getHordeContainInterface()))
	{
		owner->logic().noteStop(stopLine());
		return;
	}
	if (recipient->isKindOfName("HORDE") && (!recipient->getContain() || !recipient->getContain()->getHordeContainInterface()))
	{
		owner->logic().noteStop(stopLine());
		return;
	}
	if (!recipient->affectedByUpgrade(u)) return;
	recipient->giveUpgrade(u); // ordinary object RW 0x8A0287; horde slot 0xB8 RW 0x8A0255 / BFME2 Rva 0x46E7E3 (S-486)
	owner->logic().noteStop(stopLine()); // the original also deselects and flashes the recipient
	releaseTarget();
	m_searching = false;
	owner->setStatus(3, true);
	m_fading = true;
	m_opacity = 1.0f;
}
UpdateSleepTime GiveUpgradeUpdate::update()
{
	if (m_searching && !m_active && !m_fading) findDeliveryTarget();
	const UpdateSleepTime result = SpecialAbilityUpdate::update();
	if (m_fading)
	{
		m_opacity -= m_giveData->m_fadeSpeed;
		if (m_opacity < 0.0f) { getObject()->logic().destroyObject(getObject()); m_fading = false; m_opacity = 0.0f; }
		else if (ObjectClientHooks *client = getObject()->clientHooks()) client->setDrawableOpacity(*getObject(), m_opacity);
	}
	return (m_searching || m_fading) ? UPDATE_SLEEP_NONE : result;
}
void GiveUpgradeUpdate::crc(StateHasher &h) const { SpecialAbilityUpdate::crc(h); h.addBool(m_fading); h.addFloat(m_opacity); h.addBool(m_searching); }
void GiveUpgradeUpdate::xfer(Xfer &x) { SpecialAbilityUpdate::xfer(x); SnapshotXfer::version(x, 1, 1); x.xferBool(&m_fading); x.xferReal(&m_opacity); x.xferBool(&m_searching); }
void GiveUpgradeUpdate::registerClass(ModuleFactory &factory)
{
	factory.bindTypedData<Data>("GiveUpgradeUpdate", MODULETYPE_BEHAVIOR);
	factory.bindModuleProc("GiveUpgradeUpdate", MODULETYPE_BEHAVIOR, [](Thing *thing, const ModuleData *data, const ModuleFactory::ModuleTemplate &) -> std::unique_ptr<Module> {
		const auto *typed = dynamic_cast<const Data *>(data);
		if (!typed) throw std::logic_error("GiveUpgradeUpdate: the module data is not typed");
		return std::make_unique<GiveUpgradeUpdate>(thing, typed);
	});
}
