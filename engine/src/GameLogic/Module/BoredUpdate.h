// OpenBFME. GPL-3.0. BoredUpdate, RotWK 2.01 and BFME2 1.06.
// Update RW 0x899F9D, BFME2 0x89680F (decomp BoredUpdateUpdate.cpp,
// Rva0049680F). Targeted casts remain stopped at S-6000.
#pragma once

#include "GameLogic/Module/UpdateModule.h"
#include "GameLogic/ObjectFilter.h"
#include <string>

class ModuleFactory;
class SpecialPowerTemplate;

class BoredUpdateModuleData : public ModuleData {
public:
    unsigned m_scanDelay = 5;
    float m_scanDistance = 300.0f;
    ObjectFilter m_filter = ObjectFilter::none({}, {});
    bool m_canScanWhileAttackingOrMoving = false;
    std::string m_powerName;
    const SpecialPowerTemplate *m_power = nullptr;
    static void buildFieldParse(MultiIniFieldParse &p);
};

class BoredUpdate : public UpdateModule {
public:
    OBFME_MODULE_XFER(BoredUpdate)
    BoredUpdate(Thing *thing, const BoredUpdateModuleData *data);
    UpdateSleepTime update() override;
    void crc(StateHasher &h) const override;
    static void registerClass(ModuleFactory &factory);
    static const char *stopLine();
private:
    const BoredUpdateModuleData *m_data;
    unsigned m_timer = 0;
};
