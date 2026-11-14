# PORT-167 (issue #396, problem P-72): Lurtz is not tinted red under the Fury ability. The hero special power's
# model condition selects the hero AnimationState whose EnteringStateFX (RW 0x4BF222) sets the drawable's custom
# tint envelope (Drawable::setCustomTintEnvelope RW 0x671A73 / TINT_STATUS_CUSTOM RW 0x675E9A), read by the render
# (Drawable::evaluateRenderTint RW 0x46F4E7). Synthetic hero: the tint is red while the ability lasts, after a model
# state change in between, and none when it ends.
set(OPENBFME_PORT167_TESTS
    tests/test_port_167.cpp)
