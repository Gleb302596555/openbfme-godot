// OpenBFME. GPL-3.0. See StoreObjectsSpecialPower.h.
#include "GameLogic/Module/StoreObjectsSpecialPower.h"

#include "Common/SpecialPower.h"
#include "Common/StateHash.h"
#include "Common/Thing/ModuleFactory.h"
#include "Common/Thing/ThingTemplate.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/Object/PartitionManager.h"
#include "GameLogic/ObjectFilterMatch.h"
#include "GameLogic/ObjectTemplateInfo.h"
#include "GameLogic/SnapshotXfer.h"

#include <cstddef>
#include <stdexcept>

namespace {
const FieldParse kFields[] = {
    { "Radius", INI::parseReal, nullptr, (int)offsetof(StoreObjectsSpecialPowerModuleData, radius) },
    { nullptr, nullptr, nullptr, 0 }
};
}

void StoreObjectsSpecialPowerModuleData::buildFieldParse(MultiIniFieldParse &p) {
    SpecialAbilityUpdateModuleData::buildFieldParse(p);
    p.add(kFields);
}

void StoreObjectsSpecialPower::triggerAbilityEffect() {
    SpecialAbilityUpdate::triggerAbilityEffect();
    Object *source = getObject();
    GameLogic &logic = source->logic();
    m_objects.clear();
    const SpecialPowerModule *power = mySpecialPowerModule();
    const SpecialPowerTemplate *powerTemplate = power ? power->getSpecialPowerTemplate() : nullptr;
    if (!powerTemplate) throw std::logic_error("StoreObjectsSpecialPower: special power module or template is missing");
    if (!powerTemplate->m_objectFilter) {
        logic.noteStop("[S-523] StoreObjectsSpecialPower: the special power's default ObjectFilter is unverified");
        return;
    }
    const ObjectFilter &allowed = *powerTemplate->m_objectFilter;
    const int horde = ObjectTemplateInfoBuilder::kindOfIndex("HORDE");
    if (horde < 0) throw std::logic_error("StoreObjectsSpecialPower: HORDE kind is unavailable");
    PartitionFilterFn filter([&](Object &candidate) {
        return &candidate != source && !candidate.isEffectivelyDead()
            && !candidate.testStatus(38)
            && ObjectFilterMatch::allows(logic, allowed, candidate, source->getControllingPlayer());
        // RW 0xC0F374: same off-map bit; the port has no off-map objects (S-856).
    });
    const auto hits = logic.partition().iterateObjectsInRange(m_targetPos, m_store->radius,
        FROM_CENTER_2D, { &filter }, ITER_SORTED_NEAR_TO_FAR);
    for (const PartitionHit &hit : hits) {
        Object *candidate = hit.object;
        Object *producer = logic.findObjectByID(candidate->getProducerID()); // RW Object +0x78
        if ((producer && producer->isKindOf((unsigned)horde)) || candidate->testStatus(3)) continue;
        m_objects.push_back(candidate->getID());
    }
}

void StoreObjectsSpecialPower::crc(StateHasher &h) const {
    SpecialAbilityUpdate::crc(h);
    h.addU32((std::uint32_t)m_objects.size());
    for (ObjectID id : m_objects) h.addU32(id);
}

void StoreObjectsSpecialPower::xfer(Xfer &x) {
    SnapshotXfer::version(x, 1, 1);
    SpecialAbilityUpdate::xfer(x);
    // Native OpenBFME snapshots retain the ordered IDs needed by a later teleport.
    SnapshotXfer::ids(x, m_objects);
}

void StoreObjectsSpecialPower::registerClass(ModuleFactory &factory) {
    factory.bindTypedData<StoreObjectsSpecialPowerModuleData>("StoreObjectsSpecialPower", MODULETYPE_BEHAVIOR);
    factory.bindModuleProc("StoreObjectsSpecialPower", MODULETYPE_BEHAVIOR, [](Thing *thing, const ModuleData *data, const ModuleFactory::ModuleTemplate &) -> std::unique_ptr<Module> {
        auto *typed = dynamic_cast<const StoreObjectsSpecialPowerModuleData *>(data);
        if (!typed) throw std::logic_error("StoreObjectsSpecialPower: the module data is not typed");
        return std::make_unique<StoreObjectsSpecialPower>(thing, typed);
    });
}
