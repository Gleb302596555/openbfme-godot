"""tools/mod/rw_library_functions.py: what counts as library code the port never ports one by one (owner 2026-11-21).
A wrong `library` hides a real gap, so the rule is pinned here: the names are the BFME2 decomp's (the map's `decomp` column)."""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import rw_library_functions as lib  # noqa: E402

LUA = (0x00B5B180, 0x00B64CA0)  # the span of the `_lua*` functions in the 2026-11-21 map


def test_library_kinds():
    assert lib.classify("UBfmePod12::list<>::push_front (tier A)") == "C++ standard library container (STLport)"
    assert lib.classify("IVRva004D971D::_Rb_tree<>::insert_unique (tier A)") == "C++ standard library container (STLport)"
    assert lib.classify("D::StringBase<>::compare (tier A)") == "string class (AsciiString / UnicodeString)"
    assert lib.classify("AODCrushCollide::`scalar deleting destructor' (tier A)") == "compiler-generated deleting destructor"
    assert lib.classify("_strncat (tier A)") == "C runtime / compiler helper"
    assert lib.classify("__SEH_prolog (tier A)") == "C runtime / compiler helper"
    assert lib.classify("_bfme_debugRecordCallsite (tier D)") == "debug hook of the game"
    assert lib.classify("_lua_pushstring (tier A)", 0x00B5B830, LUA) == "Lua virtual machine (the port runs its own Lua)"
    assert lib.classify("_rehash (tier A)", 0x00B64A80, LUA) == "Lua virtual machine (the port runs its own Lua)"


def test_game_code_is_not_library():
    # game templates, game functions with a leading underscore, the Apt player, and a name inside the Lua block that is the game's glue
    assert lib.classify("UModelConditionInfo::SparseMatchFinder<>::findBest (tier A)") is None
    assert lib.classify("Object::isInList (tier A)") is None
    assert lib.classify("_appendMessage (tier A)") is None
    assert lib.classify("_Rva00054E23CopyDispatch (tier A)") is None
    assert lib.classify("_AptGetAnimationAtLevel (tier A)") is None
    assert lib.classify("_bfmeGoUPC (tier A)", 0x00B5BB30, LUA) is None
    assert lib.classify("_rehash (tier A)", 0x00C00000, LUA) is None  # outside the VM's span: not proven Lua
    assert lib.classify("") is None  # no decomp name: stays `none` until a task classifies it


def test_load_reads_the_written_list(tmp_path):
    p = tmp_path / "lib.tsv"
    p.write_text("# header\naddress\tname\treason\n0x00A3D01A\t_strncat\tC runtime / compiler helper\n", encoding="utf-8")
    assert lib.load(p) == {0x00A3D01A: "C runtime / compiler helper"}
    assert lib.load(tmp_path / "missing.tsv") == {}
