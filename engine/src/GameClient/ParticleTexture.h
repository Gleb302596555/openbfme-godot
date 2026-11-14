// OpenBFME. GPL-3.0.
//
// Particle texture resolution and packed JPG+PNG alpha merge as RotWK performs them (notes fx-draw.md 1.2 / 3, fx-draw-gpu.md):
//   * RW 0x530D29 resolves a texture lazily (replace the extension, probe .dds, then .tga, then .jpg) through the path builder RW 0x477D1C
//     (Art\CompiledTextures\<first two characters>\<name>; names starting `apt_` or shorter than two characters use Art\Textures\<name>).
//     A PNG is opened only after the JPG candidate succeeded and is stored on the loader task (+0x1C, read into +0x24/+0x20 at RW 0x530E25);
//     it is never a fourth main candidate.
//   * RW 0x530E48 .. 0x530FB8 decodes the JPG into an A8R8G8B8 surface and, when the auxiliary PNG stream is present, decodes the PNG into a
//     same-sized mask surface (requesting D3DFMT_A8 0x1C; accepting D3DFMT_A8 0x1C / D3DFMT_L8 0x32 at stride 1 offset 0 or D3DFMT_A8R8G8B8 0x15
//     at stride 4 offset 3, RW 0x530F11 .. 0x530F42) and copies only the mask byte into each texel's alpha channel (RW 0x530F84 .. 0x530F9F),
//     leaving the JPG RGB untouched.
//   * A name that resolves to nothing draws the default 1x1 white texture (RW 0x532962); the caller reports the miss.
//
// This is separate from the model texture resolver (textureloader.h, DDS then the written extension), which serves W3D meshes.

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

struct ParticleTextureResolution
{
	enum Format
	{
		NONE,
		DDS,
		TGA,
		JPG
	};
	bool Found = false;
	Format Kind = NONE;
	std::string Path;                ///< the file that exists
	std::string AuxPngPath;          ///< the PNG alpha companion that follows a resolved JPG, when it exists (RW +0x1C -> RW 0x530E48)
	std::vector<std::string> Tried;
};

enum class ParticleAuxAlphaFormat
{
	L8_OR_A8, ///< 1 byte/texel, byte 0 (RW 0x530F11 D3DFMT_A8 0x1C / 0x530F16 D3DFMT_L8 0x32: stride 1, offset 0)
	LA8,      ///< 2 bytes/texel, byte 1 (luminance + alpha: stride 2, offset 1)
	RGBA8     ///< 4 bytes/texel, byte 3 (RW 0x530F1B D3DFMT_A8R8G8B8 0x15: stride 4, offset 3)
};

ParticleTextureResolution ResolveParticleTexture(const std::string &name, const std::function<bool(const std::string &)> &exists);

// Copies the auxiliary PNG mask channel into the alpha byte of `rgba` (width * height * 4 RGBA8 bytes), leaving RGB untouched (RW 0x530E48 .. 0x530FB8).
// Returns false if dimensions or buffer sizes do not match.
bool MergeParticleJpgAlpha(std::uint8_t *rgba, int width, int height, const std::uint8_t *auxPixels, size_t auxSize, ParticleAuxAlphaFormat format);
