# PORT-435 (fork issue #435, "siege1 gate red on main: Mordor's catapult at Helm's Deep's gate hits the
# gatehouse 16 times and never breaks the gate"): synthetic-INI tests for openbfme_tests.
# The weapon aim of a projectile is the victim's contact point - WeaponTemplate::adjustTargetPositionOnVictim
# RW 0x6CB85A (flag 1) -> Object::getWorldspaceBestContactPoint RW 0x690BD2 (the object's own GeometryInfo,
# +0xA8) -> GeometryInfo::getBestContactPoint RW 0xAD30E0 preference 3: a seed blends two neighbouring contact
# point rows, and a blend outside every ACTIVE shape falls back to the first ACTIVE shape's centre
# (RW 0xAD32BA .. RW 0xAD32EB). A gate switches its active shapes (GeometryInfo::setActive RW 0xAD3520)
# between GeometryForOpen / GeometryForClosed (RW 0xC07D08 field table; the constructor's defaults
# {"OpenLeft", "OpenRight"} / {"Closed"}), so a gate standing open has no aim point in the middle of its
# doorway at all: every stone is aimed at a stood-aside panel, i.e. at the arch's side.
set(OPENBFME_PORT435_TESTS
    tests/test_port_435.cpp)
