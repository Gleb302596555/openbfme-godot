// OpenBFME. GPL-3.0.
// See GameClient/TGAFile.h.

#include "GameClient/TGAFile.h"

#include <algorithm>
#include <cstring>

bool TGAFile::decode(const std::uint8_t *data, size_t size, TGAImage &out, std::string *error, int maxDimension, bool allowRle)
{
	auto fail = [&](const std::string &msg) {
		if (error)
		{
			*error = msg;
		}
		return false;
	};
	out = TGAImage();
	if (size < 18)
	{
		return fail("TGA: shorter than the 18-byte header");
	}
	const std::uint8_t idLength = data[0];
	const std::uint8_t colorMapType = data[1];
	const std::uint8_t imageType = data[2];
	const int width = data[12] | (data[13] << 8);
	const int height = data[14] | (data[15] << 8);
	const int depth = data[16];
	const std::uint8_t flags = data[17];
	if (colorMapType != 0)
	{
		return fail("TGA: colour-mapped images are not supported");
	}
	const bool rle = allowRle && imageType == 10;
	if (imageType != 2 && !rle)
	{
		return fail("TGA: image type " + std::to_string(imageType) + " (only uncompressed true-colour type 2 is read; RLE is rejected like retail)");
	}
	if (depth != 24 && depth != 32)
	{
		return fail("TGA: pixel depth " + std::to_string(depth) + " (24 or 32 expected)");
	}
	if (width <= 0 || height <= 0 || width > maxDimension || height > maxDimension)
	{
		return fail("TGA: dimensions " + std::to_string(width) + "x" + std::to_string(height) + " out of range");
	}
	const size_t bpp = (size_t)depth / 8;
	const size_t pixelsAt = 18 + (size_t)idLength; // colour map length is 0 (colorMapType 0)
	const size_t need = (size_t)width * (size_t)height * bpp;
	if (size < pixelsAt)
	{
		return fail("TGA: shorter than its header and image id");
	}
	std::vector<std::uint8_t> expanded;
	const std::uint8_t *src = data + pixelsAt;
	if (rle)
	{
		// packets: header bit 7 set = one pixel repeated (header & 0x7F) + 1 times, clear = that many raw pixels
		expanded.reserve(need);
		const std::uint8_t *p = src;
		const std::uint8_t *const end = data + size;
		while (expanded.size() < need)
		{
			if (p >= end)
			{
				return fail("TGA: RLE pixel data truncated (" + std::to_string(expanded.size()) + " of " + std::to_string(need) + " bytes)");
			}
			const std::uint8_t header = *p++;
			const size_t count = (size_t)(header & 0x7F) + 1;
			const size_t take = std::min(count * bpp, need - expanded.size());
			if (header & 0x80)
			{
				if ((size_t)(end - p) < bpp)
				{
					return fail("TGA: RLE pixel data truncated (" + std::to_string(expanded.size()) + " of " + std::to_string(need) + " bytes)");
				}
				for (size_t k = 0; k < take; k += bpp)
				{
					expanded.insert(expanded.end(), p, p + bpp);
				}
				p += bpp;
			}
			else
			{
				if ((size_t)(end - p) < take)
				{
					return fail("TGA: RLE pixel data truncated (" + std::to_string(expanded.size()) + " of " + std::to_string(need) + " bytes)");
				}
				expanded.insert(expanded.end(), p, p + take);
				p += take;
			}
		}
		src = expanded.data();
	}
	else if (size < pixelsAt + need)
	{
		return fail("TGA: pixel data truncated (" + std::to_string(size - pixelsAt) + " of " + std::to_string(need) + " bytes)");
	}
	out.width = width;
	out.height = height;
	out.pixelDepth = depth;
	out.topDownFlag = (flags & 0x20) != 0;
	out.rgba.resize((size_t)width * (size_t)height * 4);
	std::uint8_t *dst = out.rgba.data();
	for (size_t i = 0; i < (size_t)width * (size_t)height; ++i)
	{
		dst[0] = src[2]; // r
		dst[1] = src[1]; // g
		dst[2] = src[0]; // b
		dst[3] = bpp == 4 ? src[3] : 255;
		src += bpp;
		dst += 4;
	}
	return true;
}
