# PORT-303 (fork issue 695): SalvageCrateCollide's four unread chances, its radius, and the three functions the module map
# listed as uncited (RW 0x8C12EC the class-name getter, RW 0x8C130B the lazy name-key accessor, RW 0x8C1350 the scalar
# deleting destructor). The lane changes no behaviour: the three are the port's Module::getModuleClassName,
# Module::getModuleNameKey (filled by ModuleFactory) and the compiler's destructor; the three chances and the radius stay
# parsed and unread (docs/ennorath/port303-salvage-crate-rw.md, the S-7234 row this reading narrows). The engine-side change
# is citations only (GameLogic/Module/SalvageCrateCollide.h/.cpp, Common/Module.h): this lane only adds its tests.

set(OPENBFME_PORT303_TESTS
    tests/test_port_303.cpp)
