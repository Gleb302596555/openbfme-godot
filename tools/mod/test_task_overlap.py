import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import citation_check as cc  # noqa: E402
import task_overlap as to  # noqa: E402

INV = cc.Inventory([{"address": 0x6AF3C8, "blocks": [[0x6AF3C8, 0x60]]}, {"address": 0x88BD8D, "blocks": [[0x88BD8D, 0x80]]},
                    {"address": 0x68C818, "blocks": [[0x68C818, 0x15]]}])


def task(*addrs):
    return {"title": "", "url": "", "functions": to.functions_of(set(addrs), INV), "prs": []}


def test_two_tasks_on_the_same_function_overlap_and_a_hub_does_not_count():
    # 589 / 662: Player::setProductionSpeedBonus RW 0x6AF3C8 (an inner address names the same function)
    tasks = {"#554": task(0x6AF3C8, 0x68C818), "#647": task(0x6AF3D0, 0x68C818), "#677": task(0x88BD8D, 0x68C818), "#649": task(0x88BDA2, 0x68C818)}
    hub = to.hubs(tasks)
    assert hub == {0x68C818}  # named by every task: the shared query, not a mechanism
    pairs = to.overlaps(tasks, hub)
    assert {(p["a"], p["b"]) for p in pairs} == {("#554", "#647"), ("#649", "#677")}
    assert pairs[0]["shared"] in (["RW 0x6AF3C8"], ["RW 0x88BD8D"])


def test_only_the_added_lines_of_a_diff_count():
    diff = "--- a/x.cpp\n+++ b/x.cpp\n-// RW 0x88BD8D old\n+// RW 0x6AF3C8 new\n context RW 0x68C818\n"
    assert to.addresses(to.added_lines(diff)) == {0x6AF3C8}


def test_a_function_main_already_cites_is_no_new_porting(tmp_path):
    src = tmp_path / "src"
    src.mkdir()
    (src / "a.cpp").write_text("// RW 0x68C818: the query\n", encoding="utf-8")
    ported = to.functions_of(cc.cited(src), INV)
    assert ported == {0x68C818}
    assert to.functions_of({0x6AF3C8, 0x68C818}, INV) - ported == {0x6AF3C8}
