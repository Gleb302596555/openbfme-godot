// OpenBFME. GPL-3.0. See LocomotorSetUpgrade.h.
#include "GameLogic/Module/LocomotorSetUpgrade.h"
#include "Common/Thing/ModuleFactory.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/SnapshotXfer.h"
#include <cstddef>
#include <stdexcept>

namespace {
const FieldParse kFields[] = {
    { "KillLocomotorUpgrade", INI::parseBool, nullptr, (int)offsetof(LocomotorSetUpgradeModuleData, killLocomotorUpgrade) },
    { nullptr, nullptr, nullptr, 0 }
};
}
void LocomotorSetUpgradeModuleData::buildFieldParse(MultiIniFieldParse &p) {
    buildBaseFieldParse(p);
    p.add(kFields);
}
void LocomotorSetUpgrade::upgradeImplementation() {
    if (AIUpdateInterface *ai = getObject()->getAIUpdateInterface()) ai->setLocomotorUpgrade(!m_data->killLocomotorUpgrade);
}
void LocomotorSetUpgrade::processUpgradeRemoval() {
    if (AIUpdateInterface *ai = getObject()->getAIUpdateInterface()) ai->setLocomotorUpgrade(false);
}
void LocomotorSetUpgrade::crc(StateHasher &h) const { UpgradeModule::crc(h); }
void LocomotorSetUpgrade::xfer(Xfer &x) { SnapshotXfer::version(x, 1, 1); UpgradeModule::xfer(x); }
void LocomotorSetUpgrade::registerClass(ModuleFactory &factory) {
    factory.bindTypedData<LocomotorSetUpgradeModuleData>("LocomotorSetUpgrade", MODULETYPE_BEHAVIOR);
    factory.bindModuleProc("LocomotorSetUpgrade", MODULETYPE_BEHAVIOR, [](Thing *thing, const ModuleData *data, const ModuleFactory::ModuleTemplate &) -> std::unique_ptr<Module> {
        auto *typed = dynamic_cast<const LocomotorSetUpgradeModuleData *>(data);
        if (!typed) throw std::logic_error("LocomotorSetUpgrade: the module data is not typed");
        return std::make_unique<LocomotorSetUpgrade>(thing, typed);
    });
}
