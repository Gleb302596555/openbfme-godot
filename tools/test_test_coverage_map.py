"""tools/test_coverage_map.py: the linker's map and a process's record, on made-up lines."""
from __future__ import annotations

import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_coverage_map as tcm  # noqa: E402

MAP = """
 Preferred load address is 0000000140000000

 0001:00009050       ?used@@YAHH@Z              000000014000a050 f   b.obj
 0001:00009060       ?inl@@YAHH@Z               000000014000a060 f i c.obj
 0002:00000109       __sancov$2$?used@@YAHH@Z   0000000140087109     b.obj
 0002:00000120       __sancov$2$?inl@@YAHH@Z    0000000140087120     c.obj
 0007:00000109       __sancov$3$?used@@YAHH@Z   00000001400ab109     b.obj
"""


def test_the_map_gives_each_functions_first_flag_and_address():
    flags, functions = tcm.parse_map(MAP)
    assert flags == [(0x109, "?used@@YAHH@Z", "b.obj"), (0x120, "?inl@@YAHH@Z", "c.obj")]
    assert functions[("?used@@YAHH@Z", "b.obj")] == 0xa050
    assert functions[("?inl@@YAHH@Z", "c.obj")] == 0xa060


def test_a_flag_belongs_to_the_function_it_follows():
    record = struct.pack("<3I", 0x10a, 0x10b, 0x121)
    assert tcm.files_of_record(record, [0x109, 0x120], ["engine/src/b.cpp", "engine/src/c.h"]) == {"engine/src/b.cpp", "engine/src/c.h"}
    assert tcm.files_of_record(struct.pack("<I", 0x10a), [0x109, 0x120], ["engine/src/b.cpp", "engine/src/c.h"]) == {"engine/src/b.cpp"}


def test_a_flag_before_every_function_and_code_outside_the_repository_count_for_nothing():
    assert tcm.files_of_record(struct.pack("<2I", 0x10, 0x121), [0x109, 0x120], ["engine/src/b.cpp", None]) == set()


def test_a_path_outside_the_repository_has_no_name(tmp_path: Path):
    assert tcm.repo_relative(str(tmp_path / "engine/src/a.cpp"), tmp_path) == "engine/src/a.cpp"
    assert tcm.repo_relative(r"C:\\Program Files\\MSVC\\vector", tmp_path) is None
    assert tcm.repo_relative(None, tmp_path) is None


def test_the_built_map_keeps_the_unmapped_and_the_commit():
    built = tcm.build_map({"engine/tests/test_a.cpp": {"engine/src/b.cpp"}}, ["engine/tests/test_b.cpp"], "abc")
    assert built == {"commit": "abc", "files": ["engine/src/b.cpp"], "tests": {"engine/tests/test_a.cpp": [0]}, "unmapped": ["engine/tests/test_b.cpp"], "startup": []}


def test_a_file_run_only_before_main_is_still_named():
    built = tcm.build_map({"engine/tests/test_a.cpp": set()}, [], "abc", {"engine/src/statics.cpp"})
    assert built["files"] == ["engine/src/statics.cpp"] and built["startup"] == [0]
