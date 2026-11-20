"""Tests of tools/ci/maps_land.py's merges and checks (no git): pytest tools/ci/test_maps_land.py"""
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import maps_land as M  # noqa: E402

H2 = "date\trev\tbuttons_red"
H3 = "date\trev\tbuttons_red\tmods_open"


def test_progress_takes_the_runs_changed_rows_and_keeps_mains_others():
    base = f"{H2}\n2026-11-18\taaa\t150\n"
    main = f"{H3}\n2026-11-18\taaa\t150\t5\n2026-11-19\tland\t150\t4\n2026-11-20\tland2\t140\t3\n"
    head = f"{H2}\n2026-11-18\taaa\t150\n2026-11-19\trun\t130\n"
    out = M.merge_progress(base, main, head).splitlines()
    assert out[0] == H3
    assert out[1] == "2026-11-18\taaa\t150\t5"            # unchanged by the run: main's row
    assert out[2] == "2026-11-19\trun\t130\t4"            # the run's values over main's row; the column the run lacks stays main's
    assert out[3] == "2026-11-20\tland2\t140\t3"          # a newer landing row stays


def test_progress_refuses_a_column_main_does_not_have():
    with pytest.raises(ValueError):
        M.merge_progress(f"{H2}\n", f"{H2}\n", f"{H3}\n2026-11-19\trun\t1\t2\n")


def test_sanity_refuses_a_short_matrix_and_a_new_header():
    main = "h\n" + "r\n" * 100
    assert M.sanity("docs/ennorath/MATRIX.tsv", main, "h\n" + "r\n" * 95) == []
    assert M.sanity("docs/ennorath/MATRIX.tsv", main, "h\n" + "r\n" * 50)          # an unfinished run
    assert M.sanity("docs/ennorath/FIELD_GAPS.tsv", main, "other\nr\n")            # header changed
    assert M.sanity("docs/ennorath/MATRIX_GAPS.md", None, "") == ["docs/ennorath/MATRIX_GAPS.md: empty"]
