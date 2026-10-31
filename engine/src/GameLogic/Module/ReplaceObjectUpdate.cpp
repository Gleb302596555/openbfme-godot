// OpenBFME. GPL-3.0. See ReplaceObjectUpdate.h.
#include "GameLogic/Module/ReplaceObjectUpdate.h"
#include "Common/PlayerList.h"
#include "Common/Thing/ModuleFactory.h"
#include "Common/Thing/ThingFactory.h"
#include "GameLogic/Construction.h"
#include "GameLogic/Damage.h"
#include "GameLogic/FXEvents.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/Object/PartitionManager.h"
#include "GameLogic/ObjectFilterMatch.h"
#include "GameLogic/SnapshotXfer.h"
#include <cstddef>
#include <stdexcept>

namespace {
const FieldParse kEntryFields[] = {
    { "TargetObjectFilter", ParseObjectFilter, nullptr, (int)offsetof(ReplaceObjectEntry, targetFilter) },
    { "ReplacementObjectName", INI::parseAsciiStringVectorAppend, nullptr, (int)offsetof(ReplaceObjectEntry, replacements) },
    { nullptr, nullptr, nullptr, 0 }
};
void parseEntry(INI *ini, void *instance, void *, const void *) {
    ReplaceObjectEntry entry;
    MultiIniFieldParse p;
    p.add(kEntryFields);
    ini->initFromINIMulti(&entry, p);
    static_cast<ReplaceObjectUpdateModuleData *>(instance)->entries.push_back(std::move(entry));
}
const FieldParse kFields[] = {
    { "ReplaceObject", parseEntry, nullptr, 0 },
    { "ReplaceRadius", INI::parseReal, nullptr, (int)offsetof(ReplaceObjectUpdateModuleData, radius) },
    { "ReplaceFX", INI::parseAsciiString, nullptr, (int)offsetof(ReplaceObjectUpdateModuleData, replaceFX) },
    { "Scatter", INI::parseBool, nullptr, (int)offsetof(ReplaceObjectUpdateModuleData, scatter) },
    { nullptr, nullptr, nullptr, 0 }
};
}
void ReplaceObjectUpdateModuleData::buildFieldParse(MultiIniFieldParse &p) {
    SpecialAbilityUpdateModuleData::buildFieldParse(p);
    p.add(kFields);
}
const char *ReplaceObjectUpdate::scatterStop() {
    return "[S-6002] ReplaceObjectUpdate: Scatter requires AI command 0x2E (RW 0x8B6D96; BFME2 0x8B2C00)";
}
void ReplaceObjectUpdate::triggerAbilityEffect() {
    SpecialAbilityUpdate::triggerAbilityEffect();
    Object *source = getObject();
    GameLogic &logic = source->logic();
    Player *owner = source->getControllingPlayer();
    for (const ReplaceObjectEntry &entry : m_replace->entries) {
        PartitionFilterFn filter([&](Object &o) {
            return &o != source && !o.isEffectivelyDead() && ObjectFilterMatch::allows(logic, entry.targetFilter, o, owner);
        });
        const auto hits = logic.partition().iterateObjectsInRange(m_targetPos, m_replace->radius, FROM_CENTER_2D, { &filter }, ITER_FASTEST);
        for (const PartitionHit &hit : hits) {
            Object *victim = hit.object;
            if (Object *outer = victim->getContainedBy()) {
                if (!outer->getContain() || !outer->getContain()->getHordeContainInterface()) continue;
            }
            if (entry.replacements.empty()) throw std::logic_error("ReplaceObjectUpdate: ReplacementObjectName is empty");
            const int index = logic.random().getValue(0, (int)entry.replacements.size() - 1, "ReplaceObjectUpdate.cpp", 0x47);
            const ThingTemplate *replacement = logic.things().findTemplate(entry.replacements[(size_t)index]);
            if (!replacement) throw std::logic_error("ReplaceObjectUpdate: unknown replacement " + entry.replacements[(size_t)index]);
            Player *neutral = logic.players().getNeutralPlayer();
            if (!neutral) throw std::logic_error("ReplaceObjectUpdate: neutral player is missing");
            Object *created = Construction::buildObjectNow(logic, source, *replacement, *victim->getPosition(), victim->getOrientation(), *neutral);
            if (created) {
                if (FXEventLog::isFXName(m_replace->replaceFX))
                    logic.fxEvents().emit(FXEventLog::objectEvent(FXEvent::OBJECT_FX, "ReplaceObjectUpdate ReplaceFX", logic.getFrame(), m_replace->replaceFX, *created));
                if (m_replace->scatter && created->getAIUpdateInterface()) logic.noteStop(scatterStop());
            }
            DamageInfo damage;
            damage.m_input.m_sourceID = source->getID();
            damage.m_input.m_kill = true;
            victim->attemptDamage(damage);
            logic.destroyObject(victim);
        }
    }
}
void ReplaceObjectUpdate::crc(StateHasher &h) const { SpecialAbilityUpdate::crc(h); }
void ReplaceObjectUpdate::xfer(Xfer &x) { SnapshotXfer::version(x, 1, 1); SpecialAbilityUpdate::xfer(x); }
void ReplaceObjectUpdate::registerClass(ModuleFactory &factory) {
    factory.bindTypedData<ReplaceObjectUpdateModuleData>("ReplaceObjectUpdate", MODULETYPE_BEHAVIOR);
    factory.bindModuleProc("ReplaceObjectUpdate", MODULETYPE_BEHAVIOR, [](Thing *thing, const ModuleData *data, const ModuleFactory::ModuleTemplate &) -> std::unique_ptr<Module> {
        auto *typed = dynamic_cast<const ReplaceObjectUpdateModuleData *>(data);
        if (!typed) throw std::logic_error("ReplaceObjectUpdate: the module data is not typed");
        return std::make_unique<ReplaceObjectUpdate>(thing, typed);
    });
}
