import sys
from pathlib import Path

import capstone

sys.path.insert(0, str(Path(__file__).resolve().parent))
import modifier_map as mm  # noqa: E402

MD = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)


def run(code: bytes):
    return list(MD.disasm(code + b"\xE8\x00\x00\x00\x00", 0x1000))


def test_the_attribute_is_the_last_push_before_the_call():
    # lea eax, [ebp-8]; push eax; push 7; call   (RW 0x6CA631 .. 0x6CA641: RANGE)
    assert mm.argument_at(run(b"\x8D\x45\xF8\x50\x6A\x07"), 0) == ("imm", 7)
    # push 1; push edi; lea eax, [ebp-8]; push eax; push 0x15; mov ecx, esi; call   (RW 0x6CF037 .. 0x6CF042: RATE_OF_FIRE)
    assert mm.argument_at(run(b"\x6A\x01\x57\x8D\x45\xF8\x50\x6A\x15\x8B\xCE"), 0) == ("imm", 0x15)
    # the k-th push back: the out pointer of the same call
    assert mm.argument_at(run(b"\x50\x6A\x07"), 1)[0] == "?"


def test_a_register_is_followed_to_its_constant_and_an_argument_makes_a_wrapper():
    # mov ecx, 0x15; push ecx; call
    assert mm.argument_at(run(b"\xB9\x15\x00\x00\x00\x51"), 0) == ("imm", 0x15)
    # xor edx, edx; push edx; call
    assert mm.argument_at(run(b"\x33\xD2\x52"), 0) == ("imm", 0)
    # push dword ptr [ebp + 8]; call: the function's own first argument
    assert mm.argument_at(run(b"\xFF\x75\x08"), 0) == ("arg", 0)
    # push eax with eax computed: never guessed
    what, why = mm.argument_at(run(b"\x8B\x06\x50"), 0)
    assert what == "?" and "mov eax" in why
    # a call between the pushes and the call stops the walk
    assert mm.argument_at(run(b"\x6A\x07\xE8\x00\x00\x00\x00"), 0)[0] == "?"


def test_port_reads_resolve_literals_enums_and_wrappers(tmp_path):
    src = tmp_path / "src"
    (src / "a").mkdir(parents=True)
    (src / "a" / "Mods.h").write_text("enum T { ATTRIBUTE_RANGE = 7, ATTRIBUTE_RATE_OF_FIRE = 21 };\n", encoding="utf-8")
    (src / "a" / "Econ.cpp").write_text(
        "// RW 0x6CA641: the range\n"
        "float Economy::attributeModifierProduct(const Object &obj, int type) const\n{\n\tfloat v = 1.0f;\n"
        "\tobj.attributeModifierProduct(type, nullptr, true, v);\n\treturn v;\n}\n"
        "void f(Object &o)\n{\n\tfloat s;\n\to.attributeModifierSum(ATTRIBUTE_RANGE, nullptr, s); // attributeModifierSum(3, in a comment)\n"
        "\tattributeModifierProduct(o, 13);\n\to.attributeModifierSum(0x15, nullptr, s);\n\to.attributeModifierSum(t, nullptr, s);\n}\n",
        encoding="utf-8")
    reads, cites, queries = mm.port_scan(src)
    assert queries[("attributeModifierProduct", 4)] == 0 and queries[("attributeModifierProduct", 2)] == 1  # Economy's: (obj, type)
    got = {(r.attribute, r.call) for r in reads}
    assert (7, "attributeModifierSum") in got and (21, "attributeModifierSum") in got and (13, "attributeModifierProduct") in got
    assert (None, "attributeModifierSum") in got            # `t`: listed as unresolved, not dropped
    assert all(r.attribute != 3 for r in reads)             # the comment is not a read
    assert cites["src/a/Econ.cpp"] == {0x6CA641}


def test_the_verdicts():
    assert mm.verdict(5, 0, 0, 0, True) == "dead in RotWK"
    assert mm.verdict(5, 2, 0, 0, True) == "not read by the port"
    assert mm.verdict(5, 2, 1, 3, True) == "partly ported"
    assert mm.verdict(5, 2, 2, 3, True) == "ported"
    assert mm.verdict(5, 0, 0, 1, True) == "dead in RotWK (port-only reads)"
    assert mm.verdict(1, 0, 0, 0, False) == "unknown to RotWK"


def test_a_computed_attribute_gives_every_value_it_can_take():
    # RW 0x8E1458 .. 0x8E149A (EmotionNugget::canApply): xor ecx, ecx; cmp dword ptr [eax + 4], 6; sete cl; add ecx, 4; mov edi, ecx;
    # push 0; push edi; call -> RESIST_FEAR (4) or RESIST_TERROR (5)
    code = b"\x33\xC9\x83\x78\x04\x06\x0F\x94\xC1\x83\xC1\x04\x8B\xF9\x6A\x00\x57"
    assert mm.argument_at(run(code), 0) == ("imm", {4, 5})
    # a register written by a call is not guessed
    assert mm.argument_at(run(b"\xE8\x00\x00\x00\x00\x8B\xF8\x57"), 0)[0] == "?"
