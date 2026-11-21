"""Tests of tools/ci/trusted_pr_relay.py's marker (no network): pytest tools/ci/test_trusted_pr_relay.py"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import trusted_pr_relay as R  # noqa: E402


def test_the_marker_gives_title_and_description():
    assert R.marker("PR READY: Port 308: defaults\n\nCloses #700\nbody") == ("Port 308: defaults", "Closes #700\nbody")
    assert R.marker("Port 308: a normal commit") is None
    assert R.marker("PR READY:   \n\nno title") is None


def test_the_list_has_logins_only():
    assert all(" " not in p and p for p in R.people())
