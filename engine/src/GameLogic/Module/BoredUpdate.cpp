// OpenBFME. GPL-3.0. See BoredUpdate.h.
#include "GameLogic/Module/BoredUpdate.h"
#include "Common/SpecialPower.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/StateHash.h"
#include "Common/Thing/ModuleFactory.h"
#include "GameLogic/ActionManager.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/Object/PartitionManager.h"
#include "GameLogic/ObjectFilterMatch.h"
#include "GameLogic/SnapshotXfer.h"
#include <cstddef>
#include <stdexcept>

namespace {
struct BoredRules {
    bool targetlessValidationAvailable;
};
// The scan is paired above; ActionManager's BFME2 validation is not yet paired.
const BoredRules &boredRules(GameEdition edition) {
    static const BoredRules rotwk{true}, bfme2{false};
    return edition == GameEdition::BFME2_106 ? bfme2 : rotwk;
}
// RW 0xC11DC0 (mask 4, forward relationship), including its player-layer
// mask RW 0x660B3E; BFME2 BoredUpdateUpdate.cpp Rva0049680F.
class BoredAlliesFilter : public PartitionFilter {
public:
    explicit BoredAlliesFilter(Object &owner) : m_owner(owner) {
        const Player *player = owner.getControllingPlayer();
        if (!player) return;
        m_mask = 0;
        const PlayerList &players = owner.logic().players();
        for (int i = 0; i < players.getPlayerCount(); ++i) {
            const Player *other = players.getNthPlayer(i);
            if (other && other->getPlayerIndex() >= 0 && other->getPlayerIndex() < 32 &&
                (other == player || player->getRelationship(other->getDefaultTeam()) == ALLIES))
                m_mask |= 1u << other->getPlayerIndex();
        }
    }
    bool allow(Object &candidate) override { return m_owner.getRelationship(candidate) == ALLIES; }
    std::uint32_t playerLayerMask() const override { return m_mask; }
private:
    Object &m_owner;
    std::uint32_t m_mask = 0xFFFFFFFFu;
};
void parsePower(INI *ini, void *instance, void *, const void *) {
    auto *d = static_cast<BoredUpdateModuleData *>(instance);
    d->m_powerName = ini->getNextToken();
    d->m_power = TheSpecialPowerStore ? TheSpecialPowerStore->findSpecialPowerTemplate(d->m_powerName) : nullptr;
}
void parseFilter(INI *ini, void *instance, void *store, const void *) {
    ParseObjectFilter(ini, instance, static_cast<ObjectFilter *>(store), nullptr);
}
#define BORED_OFF(m) (int)offsetof(BoredUpdateModuleData, m)
const FieldParse kFields[] = {
    { "ScanDelayTime", INI::parseDurationUnsignedInt, nullptr, BORED_OFF(m_scanDelay) },
    { "ScanDistance", INI::parseReal, nullptr, BORED_OFF(m_scanDistance) },
    { "BoredFilter", parseFilter, nullptr, BORED_OFF(m_filter) },
    { "CanScanWhileAttackingOrMoving", INI::parseBool, nullptr, BORED_OFF(m_canScanWhileAttackingOrMoving) },
    { "SpecialPowerTemplate", parsePower, nullptr, 0 },
    { nullptr, nullptr, nullptr, 0 }
};
#undef BORED_OFF
}

void BoredUpdateModuleData::buildFieldParse(MultiIniFieldParse &p) { p.add(kFields); }

BoredUpdate::BoredUpdate(Thing *thing, const BoredUpdateModuleData *data)
    : UpdateModule(thing, data), m_data(data) {
    // RW 0x899F1F; BFME2 0x896791: initial stagger uses the simulation RNG.
    m_timer = (unsigned)getObject()->logic().random().getValue(0, (int)data->m_scanDelay, "BoredUpdate.cpp", 0x46);
}

UpdateSleepTime BoredUpdate::update() {
    Object *obj = getObject();
    if (!m_data->m_power || obj->isEffectivelyDead()) return UPDATE_SLEEP(1);
    const bool allowed = m_data->m_canScanWhileAttackingOrMoving ||
        (!obj->testModelCondition(0x25) && !obj->testModelCondition(0x3D));
    const bool scan = m_timer == 0;
    if (scan) m_timer = m_data->m_scanDelay; else --m_timer; // RW 0x8B66B4; BFME2 0x8B25AF
    if (!scan || !allowed) return UPDATE_SLEEP(1);
    if (ObjectFilterMatch::isValid(&m_data->m_filter)) {
        GameLogic &logic = obj->logic();
        BoredAlliesFilter allies(*obj);
        PartitionFilterFn filter([&](Object &candidate) {
            return ObjectFilterMatch::allows(logic, m_data->m_filter, candidate, obj->getControllingPlayer());
        });
        PartitionFilterFn other([&](Object &candidate) { return &candidate != obj; });
        // RW 0x89A085 / BFME2 0x8968F7: closest in 2D, with the filter,
        // allied relationship and not-self, in this order.
        Object *target = logic.partition().getClosestObject(*obj->getPosition(), m_data->m_scanDistance,
            FROM_CENTER_2D, { &filter, &allies, &other });
        if (target) logic.noteStop(stopLine()); // target validation is S-6000
    } else if (!boredRules(obj->logic().edition()).targetlessValidationAvailable) {
        obj->logic().noteStop(stopLine());
    } else if (ActionManager::canDoSpecialPower(*obj, m_data->m_power)) {
        auto *power = SpecialPowerModules::findModule(*obj, m_data->m_power);
        // RW 0x82D679: source requirements include a fully recharged module.
        // The missing object power-availability mask is reported globally by S-142.
        if (power && power->getPercentReady() >= 1.0f && power->requirementsMet())
            SpecialPowerModules::doSpecialPower(*obj, m_data->m_power, 2, false);
    }
    return UPDATE_SLEEP(1);
}

const char *BoredUpdate::stopLine() {
    return "[S-6000] BoredUpdate: at-object validation (RW 0x82D925; BFME2 0x81CFCC) and BFME2 targetless validation (0x81CCD5) are not ported";
}
void BoredUpdate::crc(StateHasher &h) const { UpdateModule::crc(h); h.addU32(m_timer); }
void BoredUpdate::xfer(Xfer &x) { SnapshotXfer::version(x, 1, 1); xferUpdateModule(x); x.xferUnsignedInt(&m_timer); }
void BoredUpdate::registerClass(ModuleFactory &factory) {
    factory.bindTypedData<BoredUpdateModuleData>("BoredUpdate", MODULETYPE_BEHAVIOR);
    factory.bindModuleProc("BoredUpdate", MODULETYPE_BEHAVIOR, [](Thing *thing, const ModuleData *data, const ModuleFactory::ModuleTemplate &) -> std::unique_ptr<Module> {
        auto *typed = dynamic_cast<const BoredUpdateModuleData *>(data);
        if (!typed) throw std::logic_error("BoredUpdate: the module data is not typed");
        return std::make_unique<BoredUpdate>(thing, typed);
    });
}
