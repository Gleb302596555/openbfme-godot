// OpenBFME. GPL-3.0. MODGAP-2, both RotWK 2.01 and BFME2 1.06.
// RW 0x8D1EA7; BFME2 0x8CDA49 (StoreObjectsSpecialPower::triggerAbilityEffect
// in Open-BFME-2 MissileLauncherBuildingUpdate.cpp). RW binary caveat S-001.
#pragma once

#include "GameLogic/Module/SpecialAbilityModules.h"

class ModuleFactory;

class StoreObjectsSpecialPowerModuleData : public SpecialAbilityUpdateModuleData {
public:
    float radius = 0.0f; // RW data +0xD0; BFME2 +0xC8
    static void buildFieldParse(MultiIniFieldParse &p);
};

class StoreObjectsSpecialPower : public SpecialAbilityUpdate {
public:
    OBFME_MODULE_XFER(StoreObjectsSpecialPower)
    StoreObjectsSpecialPower(Thing *thing, const StoreObjectsSpecialPowerModuleData *data)
        : SpecialAbilityUpdate(thing, data), m_store(data) {}
    void crc(StateHasher &h) const override;
    static void registerClass(ModuleFactory &factory);
    const std::vector<ObjectID> &storedObjects() const { return m_objects; }
    void clearStoredObjects() { m_objects.clear(); }
protected:
    void triggerAbilityEffect() override;
private:
    const StoreObjectsSpecialPowerModuleData *m_store;
    std::vector<ObjectID> m_objects;
};
