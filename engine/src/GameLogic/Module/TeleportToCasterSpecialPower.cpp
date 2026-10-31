// OpenBFME. GPL-3.0. See TeleportToCasterSpecialPower.h.
#include "GameLogic/Module/TeleportToCasterSpecialPower.h"

#include "Common/SpecialPower.h"
#include "Common/Thing/ModuleFactory.h"
#include "Common/Thing/ThingTemplate.h"
#include "GameLogic/FXEvents.h"
#include "GameLogic/FindPositionAround.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/StoreObjectsSpecialPower.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/Object/PartitionManager.h"
#include "GameLogic/ObjectFilterMatch.h"
#include "GameLogic/ObjectTemplateInfo.h"
#include "GameLogic/SimMath.h"
#include "GameLogic/SnapshotXfer.h"

#include <cstddef>
#include <memory>
#include <stdexcept>

namespace {
typedef TeleportToCasterSpecialPowerModuleData Data;

void parseFXName(INI *ini, void *, void *store, const void *) {
    *static_cast<std::string *>(store) = ini->getNextToken(); // RW 0x73A302
}

const FieldParse kFields[] = { // RW 0xC76710
    { "Radius", INI::parseReal, nullptr, (int)offsetof(Data, m_radius) },
    { "MinDestinationRadius", INI::parseReal, nullptr, (int)offsetof(Data, m_minDestinationRadius) },
    { "MaxDestinationRadius", INI::parseReal, nullptr, (int)offsetof(Data, m_maxDestinationRadius) },
    { "TriggerFX", parseFXName, nullptr, (int)offsetof(Data, m_triggerFX) },
    { "TargetFX", parseFXName, nullptr, (int)offsetof(Data, m_targetFX) },
    { nullptr, nullptr, nullptr, 0 }
};

const char kStop[] =
    "[S-6003] TeleportToCasterSpecialPower (RW 0x8D1AEB, 0x8D1979): TargetFX (the drawable's slot 0x1EC, RW 0x8D193A) and TriggerFX (RW 0x494615 at the target point) are logged as object FX";
}

void TeleportToCasterSpecialPowerModuleData::buildFieldParse(MultiIniFieldParse &p) {
    SpecialAbilityUpdateModuleData::buildFieldParse(p);
    p.add(kFields);
}

const char *TeleportToCasterSpecialPower::stopLine() { return kStop; }

// RW 0x8D1979
void TeleportToCasterSpecialPower::teleportList(const std::vector<ObjectID> &ids, const Coord3D &point) {
    if (ids.empty()) return;
    GameLogic &logic = getObject()->logic();
    const float step = SimMath::divf32(6.28318548f, (float)ids.size()); // RW 0xBDD38C, cvtsi2ss
    float angle = 0.0f;
    for (ObjectID id : ids) {
        if (Object *o = logic.findObjectByID(id)) {
            logic.fxEvents().emit(FXEventLog::objectEvent(FXEvent::OBJECT_FX, "TeleportToCasterSpecialPower TargetFX", logic.getFrame(), m_teleport->m_targetFX, *o));
            FindPositionOptions options;
            options.minRadius = m_teleport->m_minDestinationRadius;
            options.maxRadius = m_teleport->m_maxDestinationRadius;
            options.startAngle = angle;
            float facing = o->getOrientation();
            Coord3D found = point;
            if (FindPosition::findPositionAround(logic, point, options, found)) {
                facing = (float)SimMath::atan2d(SimMath::subf32(found.y, point.y), SimMath::subf32(found.x, point.x)); // RW 0x441BD2
            } else {
                found = point; // RW 0x8D1A80
            }
            if (AIUpdateInterface *ai = o->getAIUpdateInterface()) ai->aiIdle(CMD_FROM_AI); // RW 0x5E821A(2)
            o->setOrientation(facing);    // RW 0x70C31E
            o->teleportTo(&found, false); // RW 0x696E63
            logic.fxEvents().emit(FXEventLog::objectEvent(FXEvent::OBJECT_FX, "TeleportToCasterSpecialPower TargetFX", logic.getFrame(), m_teleport->m_targetFX, *o));
        }
        angle = SimMath::addf32(angle, step);
    }
}

// RW 0x8D1AEB
void TeleportToCasterSpecialPower::triggerAbilityEffect() {
    SpecialAbilityUpdate::triggerAbilityEffect(); // RW 0x853EDF
    Object *source = getObject();
    GameLogic &logic = source->logic();
    StoreObjectsSpecialPower *store = nullptr;
    for (const std::unique_ptr<BehaviorModule> &m : source->modules()) { // RW 0x8D18FF: the first with a list
        StoreObjectsSpecialPower *s = dynamic_cast<StoreObjectsSpecialPower *>(m.get());
        if (s && !s->storedObjects().empty()) {
            store = s;
            break;
        }
    }
    if (store) {
        const std::vector<ObjectID> ids = store->storedObjects();
        teleportList(ids, m_targetPos); // RW 0x8D1B2D
        store->clearStoredObjects();    // RW 0x8D1E93
    } else {
        const SpecialPowerModule *power = mySpecialPowerModule(); // RW 0x851362
        const SpecialPowerTemplate *powerTemplate = power ? power->getSpecialPowerTemplate() : nullptr;
        if (!powerTemplate) throw std::logic_error("TeleportToCasterSpecialPower: special power module or template is missing");
        if (!powerTemplate->m_objectFilter) {
            logic.noteStop("[S-523] TeleportToCasterSpecialPower: the special power's default ObjectFilter is unverified, so no object is searched");
        } else {
            static const int kStructure = ObjectTemplateInfoBuilder::kindOfIndex("STRUCTURE"), kMoveOnly = ObjectTemplateInfoBuilder::kindOfIndex("MOVE_ONLY");
            static const int kHorde = ObjectTemplateInfoBuilder::kindOfIndex("HORDE");
            const ObjectFilter &allowed = *powerTemplate->m_objectFilter;
            PartitionFilterFn filter([&](Object &o) {
                return !o.testStatus(0x26) && ObjectFilterMatch::allows(logic, allowed, o, source->getControllingPlayer())
                    && source->getRelationship(o) == ALLIES && !(o.isKindOf((unsigned)kStructure) && o.isKindOf((unsigned)kMoveOnly))
                    && !o.isEffectivelyDead() && &o != source;
            });
            std::vector<ObjectID> ids;
            for (const PartitionHit &hit : logic.partition().iterateObjectsInRange(m_targetPos, m_teleport->m_radius, FROM_CENTER_2D, { &filter }, ITER_SORTED_NEAR_TO_FAR)) { // RW 0x8D1C10: order 1
                Object *o = hit.object;
                const Object *producer = logic.findObjectByID(o->getProducerID()); // RW 0x8D1CA7
                if ((producer && producer->isKindOf((unsigned)kHorde)) || o->testStatus(3) || o->testStatus(2)) continue;
                ids.push_back(o->getID());
            }
            teleportList(ids, *source->getPosition()); // RW 0x8D1D0A
        }
    }
    logic.fxEvents().emit(FXEventLog::objectEvent(FXEvent::OBJECT_FX, "TeleportToCasterSpecialPower TriggerFX", logic.getFrame(), m_teleport->m_triggerFX, *source)); // RW 0x8D1D42
    logic.noteStop(kStop);
}

void TeleportToCasterSpecialPower::xfer(Xfer &x) {
    SnapshotXfer::version(x, 1, 1);
    SpecialAbilityUpdate::xfer(x);
}

void TeleportToCasterSpecialPower::registerClass(ModuleFactory &factory) {
    factory.bindTypedData<Data>("TeleportToCasterSpecialPower", MODULETYPE_BEHAVIOR);
    factory.bindModuleProc("TeleportToCasterSpecialPower", MODULETYPE_BEHAVIOR, [](Thing *thing, const ModuleData *data, const ModuleFactory::ModuleTemplate &) -> std::unique_ptr<Module> {
        const Data *typed = dynamic_cast<const Data *>(data);
        if (!typed) throw std::logic_error("TeleportToCasterSpecialPower: the module data is not typed");
        return std::make_unique<TeleportToCasterSpecialPower>(thing, typed);
    });
}
