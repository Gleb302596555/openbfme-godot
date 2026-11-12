# PORT-143 (issue #345, the owner's P-25): Rohan's battering ram — the tactic the
# player chose sounds but the palantir keeps showing the defensive stance. RotWK's
# client keeps the stance button's picture in CommandButton::m_currentIndex (+0xFC,
# the ctor RW 0x75D64A): SetStance (RW 0x96F53C -> setCurrentIndex RW 0x75D082, the
# image of getButtonImage RW 0x75CDFC into GadgetButtonSetEnabledImage RW 0x6C106C)
# writes it for a stance the object's logic never answers, and the widget's ctor
# (RW 0x96FDFD, 0x96FF55 skips it when StancesBehavior is missing) and its stance
# listener (RW 0x96F5D8) keep it there. Ported as CommandButton::setCurrentIndex /
# currentIndex / getButtonImage / getTextLabel / getDescriptLabel,
# ControlBar::stanceSlotOf(button, stance) (RW 0x96F2DA) and ControlBar::applyToggleStance.
# The voice and GameLogic's stance are untouched (the owner's option 2); the mod's
# missing StancesBehavior on RohanBatteringRam stays a mod-data bug (stop S-7319).
# Tests pin the field, the raw compare, the gray button's picture and the class
# follow of a stanced unit. No game files needed.
set(OPENBFME_PORT143_TESTS
    tests/test_port_143.cpp)
