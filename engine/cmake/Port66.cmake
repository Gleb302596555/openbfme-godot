# PORT-66 (issue #180): the GameData AutoFire / AutoSmoke / AutoAflame particle systems, the fires and smoke of a damaged or burning object. The lane's
# engine changes live in files other lists already carry (GameLogicSettings, ActiveBody, BehaviorModule, StructureModules, StructureToppleUpdate,
# FlammableUpdate, FXEvents, LiveFX): only the tests are new.
set(OPENBFME_PORT66_TESTS
    tests/test_port_66.cpp)
