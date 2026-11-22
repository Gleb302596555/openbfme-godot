# PORT-237 (issue #560): ObjectCreationList nuggets/fields (ApplyRandomForce, FireWeapon, Attack, SkipIfSignificantlyAirborne,
# OffsetInLocalSpace, WaypointSpawnPoints, InvulnerableTime via DefectionHelper, OCL::create(primary, secondary)),
# CreateObjectDie (S-980), AODCrushCollide (S-6008), TerrainLogic::findClosestEdgePoint / findFarthestEdgePoint,
# OCLUpdate::CreateAtEdge & getCountdownPercent (S-5501), and Player BattlePlan bonuses / ActiveBody::applyDamageScalar (S-1712).

set(OPENBFME_PORT237_TESTS
    tests/test_port_237.cpp)
