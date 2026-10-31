// OpenBFME. GPL-3.0. See ClearanceTestingSlowDeathBehavior.h.
#include "GameLogic/Module/ClearanceTestingSlowDeathBehavior.h"
#include "GameLogic/Module/ClearanceTestingSlowDeathGeometry.h"

#include "Common/Thing/ModuleFactory.h"
#include "GameLogic/ConstructionSite.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/SquishCollide.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/SimMath.h"
#include "GameLogic/SnapshotXfer.h"

#include <cstddef>
#include <stdexcept>

namespace {
using Data = ClearanceTestingSlowDeathBehaviorModuleData;
const char *const kGeometryNames[] = { "SPHERE", "CYLINDER", "BOX", nullptr };
void parseGeometry(INI *ini, void *instance, void *, const void *) {
    auto *d = static_cast<Data *>(instance);
    const int type = INI::scanIndexList(ini->getNextToken(), kGeometryNames);
    if (!d->hasGeometry) d->geometry = ObjectGeometry::Shape{};
    d->geometry.type = type;
    d->hasGeometry = true;
}
void parseDimension(INI *ini, void *instance, void *, const void *which) {
    auto *d = static_cast<Data *>(instance);
    float value = 0.0f;
    INI::parseReal(ini, instance, &value, nullptr);
    if (!d->hasGeometry) return; // geometry setters act on the last shape only
    const int n = *static_cast<const int *>(which);
    if (n == 0) d->geometry.majorRadius = value;
    else if (n == 1) d->geometry.minorRadius = value;
    else d->geometry.height = value;
}
void parseNonNegative(INI *ini, void *instance, void *store, const void *userData) {
    INI::parseReal(ini, instance, store, userData);
    if (*static_cast<float *>(store) < 0.0f) throw std::logic_error("ClearanceTestingSlowDeathBehavior: expected a nonnegative value");
}
void parseNonPositive(INI *ini, void *instance, void *store, const void *userData) {
    INI::parseReal(ini, instance, store, userData);
    if (*static_cast<float *>(store) > 0.0f) throw std::logic_error("ClearanceTestingSlowDeathBehavior: expected a nonpositive value");
}
const int kMajor = 0, kMinor = 1, kHeight = 2;
const FieldParse kFields[] = {
    { "ClearanceGeometry", parseGeometry, nullptr, 0 },
    { "ClearanceGeometryMajorRadius", parseDimension, &kMajor, 0 },
    { "ClearanceGeometryMinorRadius", parseDimension, &kMinor, 0 },
    { "ClearanceGeometryHeight", parseDimension, &kHeight, 0 },
    { "ClearanceGeometryRotationAnchorOffset", INI::parseCoord2D, nullptr, (int)offsetof(Data, rotationAnchor) },
    { "ClearanceGeometryIsSmall", INI::parseBool, nullptr, (int)offsetof(Data, isSmall) },
    { "ClearanceGeometryOffset", INI::parseCoord3D, nullptr, (int)offsetof(Data, offset) },
    { "ClearanceMaxHeight", parseNonNegative, nullptr, (int)offsetof(Data, maxHeight) },
    { "ClearanceMaxHeightFraction", parseNonNegative, nullptr, (int)offsetof(Data, maxFraction) },
    { "ClearanceMinHeight", parseNonPositive, nullptr, (int)offsetof(Data, minHeight) },
    { "ClearanceMinHeightFraction", parseNonNegative, nullptr, (int)offsetof(Data, minFraction) },
    { nullptr, nullptr, nullptr, 0 }
};
Coord3D transform(const Object &obj, const Coord3D &p) {
    using namespace SimMath;
    const float *m = obj.getBasis();
    const Coord3D &t = *obj.getPosition();
    return { addf32(addf32(addf32(mulf32(m[2], p.z), mulf32(m[1], p.y)), mulf32(m[0], p.x)), t.x),
             addf32(addf32(addf32(mulf32(m[5], p.z), mulf32(m[3], p.x)), mulf32(m[4], p.y)), t.y),
             addf32(addf32(addf32(mulf32(m[8], p.z), mulf32(m[6], p.x)), mulf32(m[7], p.y)), t.z) };
}
}

void ClearanceTestingSlowDeathBehaviorModuleData::buildFieldParse(MultiIniFieldParse &p) {
    SlowDeathBehaviorModuleData::buildFieldParse(p);
    p.add(kFields);
}
bool ClearanceTestingSlowDeathBehavior::clearanceSatisfied() const {
    Object *obj = getObject();
    if (!obj) return false;
    GameLogic &logic = obj->logic();
    // The parsed anchor is for MapObject::getLocation (RW 0x70EE4F), not
    // GeometryInfo::CollidesWith or the clearance sampler; neither reads it.
    const Coord3D center = transform(*obj, m_clearance->offset);
    const float angle = obj->getOrientation();
    std::vector<ObjectGeometry::Shape> shapes;
    if (m_clearance->hasGeometry) shapes.push_back(m_clearance->geometry);
    PathfindGeometry bounds;
    if (!shapes.empty()) ObjectGeometry::fillPathfindGeometry(shapes, bounds);
    const float radius = shapes.empty() ? 0.0f : SimMath::mulf32(bounds.boundingSphereRadius(), 1.1f);
    ConstructionSite::GeometryFilter overlap(center, shapes, angle, true);
    const int crusher = ObjectCrush::crusherLevel(*obj);
    PartitionFilterFn valid([&](Object &candidate) {
        if (&candidate == obj || (crusher != 0 && crusher > ObjectCrush::crushableLevel(candidate))) return false;
        const AIUpdateInterface *ai = candidate.getAIUpdateInterface();
        if (ai && ai->curLocomotor() && (ai->curLocomotor()->getTemplate().m_surfaces & (unsigned)LOCOMOTORSURFACE_AIR)) return false;
        const BodyModuleInterface *body = candidate.getBodyModule();
        return !body || body->getDamageState() != BODY_RUBBLE;
    });
    if (logic.partition().getClosestObject(center, radius, FROM_BOUNDINGSPHERE_3D, { &valid, &overlap })) return false;
    if (logic.hasTerrain() && (m_clearance->maxFraction <= 1.0f || m_clearance->minFraction <= 1.0f)) {
        ObjectGeometry::Shape footprint = m_clearance->geometry;
        if (!m_clearance->hasGeometry) { footprint.type = ObjectGeometry::SHAPE_SPHERE; footprint.majorRadius = 0.01f; }
        const auto points = ClearanceDeathGeometry::sample(footprint, angle, center);
        unsigned high = 0, low = 0;
        const float z = obj->getPosition()->z;
        for (const Coord2D &point : points) {
            const float terrain = logic.getLayerHeightForDestination(point.x, point.y, z);
            if ((double)terrain > SimMath::pc24AddW((double)z, (double)m_clearance->maxHeight)) ++high;
            else if (terrain < SimMath::addf32(z, m_clearance->minHeight)) ++low;
        }
        if (!points.empty()) {
            const float count = SimMath::sseFromInt32((int)points.size());
            if (SimMath::divf32(SimMath::sseFromInt32((int)high), count) >= m_clearance->maxFraction ||
                SimMath::divf32(SimMath::sseFromInt32((int)low), count) >= m_clearance->minFraction) return false;
        }
    }
    return true;
}
void ClearanceTestingSlowDeathBehavior::crc(StateHasher &h) const { SlowDeathBehavior::crc(h); }
void ClearanceTestingSlowDeathBehavior::xfer(Xfer &x) { SnapshotXfer::version(x, 1, 1); SlowDeathBehavior::xfer(x); }
void ClearanceTestingSlowDeathBehavior::registerClass(ModuleFactory &factory) {
    factory.bindTypedData<Data>("ClearanceTestingSlowDeathBehavior", MODULETYPE_BEHAVIOR);
    factory.bindModuleProc("ClearanceTestingSlowDeathBehavior", MODULETYPE_BEHAVIOR, [](Thing *thing, const ModuleData *data, const ModuleFactory::ModuleTemplate &) -> std::unique_ptr<Module> {
        const auto *typed = dynamic_cast<const Data *>(data);
        if (!typed) throw std::logic_error("ClearanceTestingSlowDeathBehavior: the module data is not typed");
        return std::make_unique<ClearanceTestingSlowDeathBehavior>(thing, typed);
    });
}
