# PORT-253: Upgrade modules: SubObjectsUpgrade (logic side), GeometryUpgrade, ReplaceSelfUpgrade, CostModifierUpgrade, InheritUpgradeCreate, SpellRechargeModifierUpgrade (fork issue 601)
# The lane's engine changes are in files other lists already carry (Upgrade, Structure, Economy); this file adds the lane's tests only.

set(OPENBFME_PORT253_TESTS
    tests/test_port_253.cpp)
