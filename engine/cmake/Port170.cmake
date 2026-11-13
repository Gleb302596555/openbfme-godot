# PORT-170 (issue #399, task #146): the placement seal flickers red/white with a stationary cursor.
#
# RW analysis (workspace/RotWK/game.dat, caveat S-001):
#   * The ghost's legality is judged in InGameUI::handleBuildPlacements (RW 0x6A2AE5) and the tint is
#     applied in the same function (RW 0x6A3932..0x6A3B37) only on odd client frames (TheGameClient
#     vslot 0x7C, `test al, 1` at RW 0x6A393D). The code is TheBuildAssistant vslot 0x44 =
#     isLocationLegalToBuild (RW 0x79681A..0x796D34) at the drawable's site with 0x48F; the thunk
#     RW 0x676711 interleaved in the argument setup is Drawable::getPosition, not a translation.
#   * Code 9 is the WALL_HUB range test (template +0x11B & 0x10, RW 0x796C66): the site must be within
#     the module's range of one of the owning player's points (RW 0x73CDD8 finder, RW 0x6B00F3 /
#     RW 0x6B00CE point collector), else 9 (RW 0x796D0C). 9 tints (0, 0, 0.5), blue 1.7 on frame&4;
#     any other nonzero code tints IllegalBuildColor 0xDA0B28 (1, 0, 0); 0 tints nothing.
#   * No TaintManager call exists in RW 0x79681A..0x796D34: taint does not enter the legality code.
#
# Fixed here: BuildPlacement::isLocationLegalToBuild ended after the terrain restrictions, so the
#   code-9 branch the tint loop paints blue never existed for a building (the wall plan had its own
#   copy). The pass is now ported once, in the function both the ghost and the release call.
# The tests: the code of a parked pointer per frame (the parity gate and the busy frames of the
#   device must not alternate it, on the taint disc and off it - RotWK asks no taint test), and the
#   hub's MaxBuildoutDistance on both sides of the range.
set(OPENBFME_PORT170_TESTS
    tests/test_port_170.cpp
)
