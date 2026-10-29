# Arnor stones: sparks animate too fast (handoff)

Status: open, not reproduced here. Written by a cloud session that has the source but no game files, so nothing below was checked
against retail data or a retail capture. A session with `workspace/` (game files, decomps, notes) should confirm the cause, fix it
and drop this file in the same commit.

## Report

The sparks over the Arnor stones animate too fast compared with retail. The report came from a run with the Ennorath mod mounted
(`--mod-install=<Ennorath install>`); whether vanilla RotWK 2.01 shows it too is not known.

## Ruled out (by reading the code)

- Particle simulation speed. `FXPlayer::advance` (engine/src/GodotDevice/GodotFXPlayer.cpp) steps the playback at a fixed 30 Hz
  whatever the render rate, and `ParticleSystem::gpuNowFrames` (engine/src/GameClient/ParticleSys.cpp) is built from that client
  frame counter (33 ms per frame). A high render frame rate does not speed particles up.
- Texture mappers on models (W3D Grid / LinearOffset / Rotate / Random mappers in engine/src/Libraries/WWVegas/WW3D2/mapper.cpp,
  simple.fx `TexCoordTransform_0` in engine/src/GodotDevice/GodotW3DMaterial.cpp). The clocks are seconds turned into ms once
  (`W3DInstancer::update_mappers`), and every instancer creates its own mapper objects, so no mapper is advanced by two clocks.
- W3D emitters embedded in models (`e_firesparks_sm` and the like). `FXPlayback::playW3DEmitter` is only reached from the FX viewer;
  the live game does not spawn them, so the sparks in a match come from an FXParticleSystem.

## Main suspect: GpuDraw frame animation

GPU particles with an animated texture (`Draw = GpuDraw` with `FramesPerRow`, `TotalFrames`, `SpeedMultiplier`) pick their frame in
the GpuDraw vertex shader (`gpuShader` in engine/src/GodotDevice/GodotFXPlayer.cpp, "frame animation" block):

```glsl
float f = floor(rnd_k48 * tf) + floor(age * speed_mult);
...
lerp_t = fract(age * speed_mult) * multi;
```

`age` is `now - birth` in client frames (30 per second), so with `SpeedMultiplier = 1` the texture changes frame 30 times a
second. The shader is a port of gpuparticle.fxo (notes fx-draw-gpu.md 2.1, not in the repository), and no test covers this
formula: `test_fx_draw.cpp` only checks that `FramesPerRow` / `TotalFrames` reach `GpuDrawParams`.

Also worth checking while there: the frame-to-cell mapping (`g_cur = fract(f / tf) * (tf - 1)`, then `frame_uv`). With
`FramesPerRow = 4`, `TotalFrames = 16` it never selects column 3 of the atlas, which looks wrong unless the bytecode does the same.

## To do

1. Find the particle system: the stone object's INI (`ParticleSysBone` / FXList) in the mod, then its FXParticleSystem block. Note
   its Draw module and, for GpuDraw, `FramesPerRow`, `TotalFrames` and `SpeedMultiplier`.
2. Compare the system alone in the FX viewer (`godot/scripts/fx_viewer.gd`, `-- --ps=<name>`) with retail, and estimate the factor
   (2x? 30x?).
3. If it is GpuDraw: check the frame expression and the cell mapping against gpuparticle.fxo's vs_2_0 (the SSA listing behind
   fx-draw-gpu.md). Add a test that pins the frame for a given age and `SpeedMultiplier`.
4. If it is not GpuDraw: the suspect is wrong; look at the Draw module the system uses.
