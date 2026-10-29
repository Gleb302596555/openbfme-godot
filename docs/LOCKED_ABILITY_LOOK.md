# Locked hero abilities look bleached, not dark (handoff)

Status: open, not reproduced here. Written by a cloud session that has the source but no game files, Apt movies or decomps, so the
retail side below is unread. A session with `workspace/` should settle it, fix it and drop this file in the same commit, together with
the matching item in `docs/MODS.md` ("a locked ability's button is drawn in greyscale ... and reads as merely lighter, not locked").

## Report

In vanilla, a hero's locked ability buttons (not unlocked yet) look dark and muted. In the port they look washed out, almost white.

## How the port draws it

- `AptPalantir` (engine/src/GameClient/GUI/AptScreens/AptPalantir.cpp): a `Restricted` button gets the clip state `_disabled`
  (`commandStateName`) and its native image is flagged `grayscale`.
- `HudDevice` (engine/src/GodotDevice/GodotInGameHud.cpp): `RenderImageDisabled` placeholders, and images flagged `grayscale`, are
  drawn from `grayTextureFor`: each texel replaced by luma 0.299 / 0.587 / 0.114, alpha kept, alpha blended. The result is then
  multiplied by the clip's cumulative colour `mul` (the `_disabled` frame's colour transform); the transform's `add` terms are dropped
  (`AptCanvas.cpp`, Placeholder ops copy only `cmd.color.mul`). A clip `_mode` other than ALPHA also gets `aptImageModeShader`
  (luma again for *GRAYSCALE).
- History (stop S-5040, OWNERFIX-3): this used to be a plain 0.45 dimming in colour (an inference). The owner then saw another
  player's disabled buttons in colour, and it was replaced by the greyscale above, also an inference: RotWK's RenderImage callback
  (the image draw 0x412B94) was never read.

## What was checked against Zero Hour (EA's GPL source, GeneralsMD render2d.cpp, Render2DClass::Render)

ZH's `DRAW_IMAGE_GRAYSCALE` (W3DDisplay::drawImage -> Render2DClass::Enable_Grayscale):

- with Dot3: stage 0 `MULTIPLYADD(TFACTOR.a, TEXTURE, TFACTOR.a)`, stage 1 `DOTPRODUCT3(CURRENT, TFACTOR)`, TFACTOR 0x80A5CA8E. Worked
  out, that is about 0.295 R + 0.587 G + 0.114 B: plain luma, so the port's weights match ZH's.
- it draws with `_PresetOpaqueShader` ("grayscale non-alpha blended images"): no blending, so neither the texture alpha nor a vertex
  alpha fades it. The port blends instead.
- without Dot3: `TEXTURE * 0x60606060`, i.e. the image in colour at 0.376, which is dark.

Apt's `add` terms are unsigned bytes (AptCharacterInst.cpp, place object colour transform), so dropping them can only make the port
darker, not lighter: they do not explain the report.

## Likely causes to check

1. The `_disabled` frame's colour transform of the Palantir command button clip: if it fades by alpha (as SpellStore's `_disabled`
   frame does, alpha 0.396), the port draws a 40 % grey icon over the button's backing, which reads as bleached; ZH's opaque
   grayscale path would not fade it.
2. RotWK's own image draw (0x412B94 and the W3DDisplay call it makes) for `RenderImageDisabled` / `_mode` GRAYSCALE: the actual
   operation (luma, luma times a dim factor, the 0x60 modulate) and whether it blends.
3. Whether the button's other layers (frame, highlight, overlay) in the `_disabled` frame are what retail draws darker.

## To do

1. Read the hero button clip's `_disabled` frame in the Palantir Apt movie (its colour transform, which layers it touches).
2. Read 0x412B94's grayscale path in the RotWK binary (or the BFME2 decomp twin).
3. Fix `grayTextureFor` / the Placeholder draw to match, add a test that pins a disabled button's pixel for a known texel, and
   compare against a retail screenshot of a hero with a locked ability.
