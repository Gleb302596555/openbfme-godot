# PORT-285 (issue #660): the closed-set audit of the snapshot code (tools/save/closed_sets.py) is green for the transfers the issue
# lists: every closed value and id among them has its check on load (SnapshotXfer::checkRange / enumIn, or a refusal naming the value),
# and every transfer whose bits are all legal has its entry in tools/save/closed_sets.json. The test saves each checked field, loads it
# back and arms the SnapshotProbe on that check: an out-of-set value must be refused by name, an in-set one must load. Synthetic
# data only, no game files.
set(OPENBFME_PORT285_TESTS
    tests/test_port_285.cpp)
