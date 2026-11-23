# PORT-340: the on-foot black rider horde's Morgul blade (Command_BlackRiderMorgulBladeForHorde). The tests make both hordes of the mod's data
# (mordorschwarzereiter.ini: MordorSchwarzeReiterHorde and its ChildObject MordorSchwarzeReiterHordeFoot, members MordorSchwarzeReiter) out of
# synthetic INI with the module set and the values copied from the mod. The lane's engine change is in files other lists already carry
# (GameLogic/Module/SpecialAbilityModules, GameLogic/Object/Contain/HordeContain*).

set(OPENBFME_PORT340_TESTS
    tests/test_port_340.cpp)
