# PORT-144 (issue #361, P-79: rebuilding from ruins): the tests of the goblin hole's rebuild on the module set the Wild structures and WildStructureHole
# carry (RebuildHoleExposeDie + KeepObjectDie + StructureCollapseUpdate + GettingBuiltBehavior with a worker; RebuildHoleBehavior + SlowDeathBehavior).
# The engine files are in lists other lanes already carry (GameLogic/Module/RebuildHoleModules.cpp, ConstructionModules.cpp, WorkerAIUpdate.cpp).

set(OPENBFME_PORT144_TESTS
    tests/test_port_144.cpp)
