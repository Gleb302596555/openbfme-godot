"""Pins of the RotWK facts the secondary texture (DetailColor) modes of a classic W3D material rest on: what defaultw3d.fxo's pixel shaders compute when retail
converts the material to DefaultW3D.fx, against what the fixed-function pipeline computes for the surfaces it does not convert. The binary facts run only when
RW_GAME_DAT is set (the path of the RotWK game.dat; caveat S-001: the community-modified image); the retail-file facts only when ROTWK_INSTALL is set
(Shaders.big). Each assertion is read from the files, so a different binary or a wrong citation fails loudly.

The conversion (BFME2 decomp MeshMatDescFXShaderConversion.cpp:416-433, strings at RW 0x7EC...) maps the post-detail colour function of the shader to the
effect's SecondaryTextureBlendMode (STBM): Scale (2) 0, Scale2X (11) 1, DetailBlend (8) 2, ModAlphaAddColor (12) 3, HouseColor (13) 3 + HouseColorEnable;
every other function, and a DetailAlpha function, leave the surface to the fixed-function pipeline. The pixel shader of technique Default is
PS_Array[NT' + 3 STBM' + ...] (NT = NumTextures; the FXLC expression is pinned in test_house_color_binary_facts.py), so the unshroud / unfogged / unshadowed
permutations with two textures are PS_Array[2], [5], [8], [11].

Fixed function (BFME2 ShaderClassApply.cpp:1325-1445): stage 0 MODULATE2X(texture, diffuse); stage 1 = SCALE: MODULATE, SCALE2X: MODULATE2X, DETAILBLEND:
BLENDCURRENTALPHA(Arg1 = TEXTURE, Arg2 = CURRENT) = TEXTURE * CURRENT.a + CURRENT * (1 - CURRENT.a), MODALPHAADDCOLOR: MODULATEALPHA_ADDCOLOR(Arg1 = CURRENT,
Arg2 = SPECULAR). The texture of stage 1 is NOT doubled there.
"""
from __future__ import annotations

import os
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "rw_object_model"))
sys.path.insert(0, str(ROOT / "fx"))
sys.path.insert(0, str(ROOT / "render"))

needs_binary = pytest.mark.skipif(not os.environ.get("RW_GAME_DAT"), reason="RW_GAME_DAT not set (path of the RotWK game.dat)")
needs_install = pytest.mark.skipif(not os.environ.get("ROTWK_INSTALL"), reason="ROTWK_INSTALL not set")

from fxo_effect import code, expected_index, ps_array, ps_l_array, run_pixel_shader  # noqa: E402


@pytest.fixture(scope="module")
def img():
    from rwimage import Image

    return Image()


@pytest.fixture(scope="module")
def effect():
    import bigfs
    import fxo_effect

    return fxo_effect.Effect(bigfs.Mount().read("shaders\\compiled\\defaultw3d.fxo"))


SAMPLES = [
    # t0, t1, v0 (oD0), v1 (oD1 rgb), cloud
    ((0.8, 0.4, 0.2, 0.5), (0.1, 0.9, 0.3, 0.7), (0.3, 0.2, 0.4, 0.6), (0.05, 0.1, 0.02), (0.9, 0.8, 0.7, 1.0)),
    ((0.25, 0.5, 0.75, 1.0), (0.6, 0.3, 0.9, 0.2), (0.5, 0.5, 0.5, 1.0), (0.0, 0.0, 0.0), (1.0, 1.0, 1.0, 1.0)),
    ((1.0, 0.1, 0.4, 0.3), (0.9, 0.9, 0.1, 1.0), (0.45, 0.35, 0.25, 0.8), (0.1, 0.2, 0.3), (0.5, 0.6, 0.7, 1.0)),
]


@needs_binary
@needs_install
def test_the_index_expression_puts_the_four_modes_at_2_5_8_11(effect):
    # the FXLC expression of technique Default (0xB64B8), evaluated: index = NT' + 3 STBM' + 12 [Shroud == 2] + 24 [Fog] (1 + [Blend == 2]) + 72 min(NumShadows, 1) + 144 HCE
    import fxo_effect

    expression = effect.selector_expressions()[0xB64B8]
    for nt in range(3):
        for stbm in range(4):
            for shroud in (0, 2):
                for fog in (0, 1):
                    for blend in range(3):
                        for shadows in (0, 1, 3):
                            v = dict(NumTextures=nt, SecondaryTextureBlendMode=stbm, HouseColorEnable=0, ObjectShroudStatus=shroud, Fog=fog, BlendMode=blend,
                                     NumShadows=shadows)
                            assert fxo_effect.evaluate_selector(expression, v) == expected_index(nt, stbm, 0, shroud, fog, blend, shadows)
    base = dict(HouseColorEnable=0, ObjectShroudStatus=0, Fog=0, BlendMode=0, NumShadows=0, NumTextures=2)
    assert [fxo_effect.evaluate_selector(expression, dict(base, SecondaryTextureBlendMode=m)) for m in range(4)] == [2, 5, 8, 11]


@needs_binary
@needs_install
def test_the_four_secondary_modes_with_two_textures(effect):
    ps, _ids = ps_array(effect)
    index = {mode: expected_index(2, mode, 0, 0, 0, 0, 0) for mode in range(4)}
    assert index == {0: 2, 1: 5, 2: 8, 3: 11}
    # Scale and Scale2X: a product, so the order of the lighting and the texture is immaterial
    assert code(ps[index[0]]) == [
        "texld r3, t0, s0", "texld r2, t1, s1", "texld r1, t2, s2",
        "mov r0.xyz, v0", "add r0.xyz, r0, v1", "mov r0.w, v0.w",
        "mul r0, r3, r0", "mul r0.xyz, r2, r0", "mul r0.xyz, r1, r0", "add r0.xyz, r0, r0", "mov oC0, r0",
    ]
    assert code(ps[index[1]]) == [
        "texld r3, t0, s0", "texld r2, t1, s1", "texld r1, t2, s2",
        "mov r0.xyz, v0", "add r0.xyz, r0, v1", "mov r0.w, v0.w",
        "mul r0, r3, r0", "mul r0.xyz, r2, r0", "add r0.xyz, r0, r0", "mul r0.xyz, r1, r0", "add r0.xyz, r0, r0", "mov oC0, r0",
    ]
    # DetailBlend (blob 195): r3 = t1 - t0 * (v0 + v1); r0 = t0 * v0 (alpha t0.a * v0.a); r0.rgb = r0.rgb + r0.a * r3; * cloud; * 2
    assert code(ps[index[2]]) == [
        "texld r3, t1, s1", "texld r2, t0, s0", "texld r1, t2, s2",
        "mov r0.xyz, v0", "add r0.xyz, r0, v1", "mad r3.xyz, r0, -r2, r3", "mov r0.w, v0.w",
        "mul r0, r2, r0", "mad r0.xyz, r0.w, r3, r0", "mul r0.xyz, r1, r0", "add r0.xyz, r0, r0", "mov oC0, r0",
    ]
    # ModAlphaAddColor (blob 198) does not sample Texture_1 at all: r0 = t0 * v0; r0.rgb += v1 * r0.a; * cloud; * 2
    assert code(ps[index[3]]) == [
        "texld r0, t0, s0", "texld r1, t2, s2",
        "mul r0, r0, v0", "mad r0.xyz, v1, r0.w, r0", "mul r0.xyz, r1, r0", "add r0.xyz, r0, r0", "mov oC0, r0",
    ]


@needs_binary
@needs_install
def test_the_blobs_compute_the_formulas_the_generated_shader_uses(effect):
    ps, _ids = ps_array(effect)
    for t0, t1, v0, v1, cloud in SAMPLES:
        alpha = t0[3] * v0[3]
        light = [v0[i] + v1[i] for i in range(3)]
        # Scale: 2 * t0 * t1 * light; Scale2X: 4 * t0 * t1 * light (times the cloud)
        for mode, factor in ((0, 2), (1, 4)):
            out = run_pixel_shader(ps[expected_index(2, mode, 0, 0, 0, 0, 0)], v0, v1, t0, t1, cloud)
            assert out[:3] == pytest.approx([factor * cloud[i] * t0[i] * t1[i] * light[i] for i in range(3)], abs=1e-6)  # tolerance: float arithmetic
        # DetailBlend: 2 * cloud * lerp(t0 * light, t1, alpha): the second texture is doubled with the rest, NOT blended into the doubled colour
        out = run_pixel_shader(ps[expected_index(2, 2, 0, 0, 0, 0, 0)], v0, v1, t0, t1, cloud)
        assert out[:3] == pytest.approx([2 * cloud[i] * ((1 - alpha) * t0[i] * light[i] + alpha * t1[i]) for i in range(3)], abs=1e-6)  # tolerance: float arithmetic
        assert out[3] == pytest.approx(alpha, abs=1e-6)  # tolerance: float arithmetic
        # ... which differs from the fixed-function order lerp(2 * t0 * light, t1, alpha) wherever t1 is not black
        fixed_function = [(1 - alpha) * 2 * t0[i] * light[i] + alpha * t1[i] for i in range(3)]
        assert out[:3] != pytest.approx([cloud[i] * fixed_function[i] for i in range(3)], abs=1e-3)
        # ModAlphaAddColor: 2 * cloud * (t0 * v0 + v1 * alpha), t1 unused
        out = run_pixel_shader(ps[expected_index(2, 3, 0, 0, 0, 0, 0)], v0, v1, t0, (9, 9, 9, 9), cloud)
        assert out[:3] == pytest.approx([2 * cloud[i] * (t0[i] * v0[i] + v1[i] * alpha) for i in range(3)], abs=1e-6)  # tolerance: float arithmetic


@needs_binary
@needs_install
def test_the_one_texture_and_no_texture_permutations_ignore_the_mode(effect):
    ps, _ids = ps_array(effect)
    # NumTextures 1 (the conversion's count when DetailColor is Disable, or the file has one texture) is the same shader for every mode, NumTextures 0 too
    for nt in (0, 1):
        reference = code(ps[expected_index(nt, 0, 0, 0, 0, 0, 0)])
        assert reference
        for mode in range(1, 4):
            assert code(ps[expected_index(nt, mode, 0, 0, 0, 0, 0)]) == reference


@needs_binary
@needs_install
def test_the_blend_order_holds_in_every_fog_shroud_and_shadow_variant(effect):
    import re

    ps, _ids = ps_array(effect)
    sub = re.compile(r"mad (r\d)\.xyz, (r\d), -(r\d), (r\d)")   # r3.rgb = t1 - t0 * light: the light term subtracted from the second texture
    lerp = re.compile(r"mad (r\d)\.xyz, (r\d)\.w, (r\d), (r\d)")  # rgb = rgb + alpha * that
    for shroud in (0, 2):
        for fog in (0, 1):
            for blend in range(3):
                for shadows in (0, 1):
                    lines = code(ps[expected_index(2, 2, 0, shroud, fog, blend, shadows)])
                    steps = [i for i, x in enumerate(lines) if sub.fullmatch(x)]
                    mixes = [i for i, x in enumerate(lines) if lerp.fullmatch(x)]
                    assert len(steps) == 1 and len(mixes) == 1 and steps[0] < mixes[0]
                    # the final x2 (outside fog / shroud variants it is `add r, r, r` after the cloud) comes after the blend, never between the texels
                    assert any(x.startswith("texld") and x.endswith("t1, s1") for x in lines)


@needs_binary
@needs_install
def test_the_low_shader_level_blend_also_doubles_the_second_texture(effect):
    # PS_L_Array (ps_1_1, techniques Default_L / Default_M) at NT 2, STBM 2: r1 = lerp(r1.w, t1, light * t0) then the final mad r1 * (1 + 0.999)
    psl, _ids = ps_l_array(effect)
    assert code(psl[8]) == [
        "def c0, 0.999, 0.999, 0.999, 0",
        "tex t0",
        "tex t1",
        "add r0.xyz, v0, v1",
        "mov r0.w, v0.w",
        "mul r0, t0, r0",
        "mov_sat r1.w, r0.w",
        "lrp r1.xyz, r1.w, t1, r0",
        "mov r0.w, r0.w",
        "mad r0.xyz, r1, c0, r1",
    ]


@needs_binary
def test_the_conversion_strings_name_the_supported_detail_colour_functions(img):
    assert b"Unsupported DetailColor mode. Supported: 'Disable', 'Scale', 'Scale2X', 'DetailBlend', 'ModAlphaAddColor' or 'HouseColor'" in img.data
    assert b"DetailAlpha is not 'Disable'" in img.data
