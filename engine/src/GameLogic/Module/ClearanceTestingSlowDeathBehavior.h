// OpenBFME. GPL-3.0. MODGAP-2, both RotWK 2.01 and BFME2 1.06.
// RW 0x887191 / 0x886F1C; BFME2 0x883F19 / 0x883C9F
// (Rva00483C35Rva00483F19.cpp, ClearanceTestingSlowDeathBehavior.cpp).
// Rotation anchor is parsed but neither the clearance overlap nor grid sampler
// reads it: RW 0xAD2CE0 / 0x8E047D, BFME2 GeometryInfoCollidesWith.cpp /
// ClearanceTestingSlowDeathGeometrySampler.cpp. RW provenance caveat S-001.
#pragma once

#include "GameLogic/Module/CombatModules.h"
#include "GameLogic/Object/ObjectGeometry.h"

class ClearanceTestingSlowDeathBehaviorModuleData : public SlowDeathBehaviorModuleData {
public:
    ObjectGeometry::Shape geometry;
    bool hasGeometry = false;
    bool isSmall = false;
    Coord2D rotationAnchor{};
    Coord3D offset{};
    float maxHeight = 20.0f, maxFraction = 1.1f;
    float minHeight = -20.0f, minFraction = 1.1f;
    static void buildFieldParse(MultiIniFieldParse &p);
};

class ClearanceTestingSlowDeathBehavior : public SlowDeathBehavior {
public:
    OBFME_MODULE_XFER(ClearanceTestingSlowDeathBehavior)
    ClearanceTestingSlowDeathBehavior(Thing *thing, const ClearanceTestingSlowDeathBehaviorModuleData *data)
        : SlowDeathBehavior(thing, data), m_clearance(data) {}
    void crc(StateHasher &h) const override;
    static void registerClass(ModuleFactory &factory);
    bool clearanceSatisfied() const;
protected:
    bool canBeginSlowDeath() const override { return clearanceSatisfied(); }
private:
    const ClearanceTestingSlowDeathBehaviorModuleData *m_clearance;
};
