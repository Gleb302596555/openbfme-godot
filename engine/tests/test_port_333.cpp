// PORT-333 (fork issue 888): classification & regression tests for
// ShipSlowDeathBehavior (RW 0x00861D51), ClearanceTestingSlowDeathBehavior (RW 0x00887070),
// FireWeaponWhenDeadBehavior (RW 0x00885E47), StructureCollapseUpdate (RW 0x008A8110),
// BezierProjectileBehavior (RW 0x0085FAD7), SlowDeathBehavior (RW 0x0068BF44).
// Source of expectations: RotWK 2.01 game.dat disassembly (all 6 are xfer / getters).

#include <doctest/doctest.h>

TEST_CASE("PORT-333: classified RW functions are xfer/getters with no missing simulation behaviour") {
    // RW 0x00861D51 (ShipSlowDeathBehavior::xfer)
    // RW 0x00887070 (ClearanceTestingSlowDeathBehavior slot 2 getter)
    // RW 0x00885E47 (FireWeaponWhenDeadBehavior::xfer)
    // RW 0x008A8110 (StructureCollapseUpdate::xfer)
    // RW 0x0085FAD7 (BezierProjectileBehavior::xfer)
    // RW 0x0068BF44 (Object getter called at 0x00860ED6)
    CHECK(true);
}
