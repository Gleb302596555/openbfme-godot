# PORT-70 (issue #184, "GameData: sort the 209 rows the port does not read"): a verdict table, no engine behaviour change. The page is
# docs/ennorath/gamedata-rows.md (82 rows: 23 gameplay, 16 presentation, 22 debug / editor, 21 not read); the fixture is the page's
# table (engine/tests/data/port70/gamedata-rows.txt), and the tests pin that each row is a real row of RotWK's GameData table
# (RW 0xBFF580 + 16 * index; the index and the member offset from BFME2 1.06's s_GlobalDataFieldParseTable, whose rows the
# field-triage batches confirmed against the image) and that the port takes every one of them with the S-152 catch-all
# (noteUnapplied: the field is accepted, recorded as GameData.<field>, and nothing is applied). No source file is added:
# a later port that gives one of these rows a kGameDataFields row is meant to fail the third test and move the row off the page.
set(OPENBFME_PORT70_TESTS
    tests/test_port_70.cpp)
