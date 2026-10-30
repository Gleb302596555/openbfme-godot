# Lane MODGAP-1 (docs/ennorath/MODULE_GAPS.md: the modules Ennorath uses that the port lacked): the tests of the openbfme_tests executable. The runtime of each
# class lives in the files of its family (SalvageCrateCollide in GameLogic/Module/MoneyEventModules.cpp, the static contacts in GameLogic/AI/AIWorld.cpp), which are
# already in the simulation manifest. Included from engine/CMakeLists.txt so the lane's file list stays out of the shared list.
set(OPENBFME_MODGAP1_TESTS
    tests/test_modgap1_salvage.cpp
    tests/test_modgap1_slave.cpp
    tests/test_modgap1_deflect.cpp
    tests/test_modgap1_reflect.cpp
    tests/test_modgap1_evacuate.cpp
    tests/test_modgap1_hole.cpp
)

# simulation sources (sim audit manifest): module state that enters the lockstep hash
list(APPEND OPENBFME_SIM_SOURCES
    src/GameLogic/Module/SlaveWatcherBehavior.cpp
    src/GameLogic/Module/OCLUpdate.cpp
    src/GameLogic/Module/DeflectSpecialPower.cpp
    src/GameLogic/Module/ReflectDamage.cpp
    src/GameLogic/Module/EvacuateDamage.cpp
    src/GameLogic/Module/RebuildHoleModules.cpp
)
