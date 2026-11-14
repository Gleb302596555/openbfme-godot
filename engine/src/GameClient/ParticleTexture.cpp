// OpenBFME. GPL-3.0. See ParticleTexture.h.

#include "GameClient/ParticleTexture.h"

#include <cctype>

ParticleTextureResolution ResolveParticleTexture(const std::string &name, const std::function<bool(const std::string &)> &exists)
{
	ParticleTextureResolution out;
	std::string lower = name;
	for (char &c : lower)
	{
		c = (char)std::tolower((unsigned char)c);
	}
	std::string stem = lower;
	const size_t dot = stem.find_last_of('.');
	if (dot != std::string::npos)
	{
		stem = stem.substr(0, dot);
	}
	if (stem.empty())
	{
		return out;
	}
	const bool textures = stem.compare(0, 4, "apt_") == 0 || stem.size() < 2;
	const std::string folder = textures ? "art\\textures\\" : "art\\compiledtextures\\" + stem.substr(0, 2) + "\\";
	struct Candidate
	{
		const char *ext;
		ParticleTextureResolution::Format kind;
	};
	for (const Candidate &c : { Candidate{ "dds", ParticleTextureResolution::DDS }, Candidate{ "tga", ParticleTextureResolution::TGA }, Candidate{ "jpg", ParticleTextureResolution::JPG } })
	{
		const std::string path = folder + stem + "." + c.ext;
		out.Tried.push_back(path);
		if (exists(path))
		{
			out.Found = true;
			out.Kind = c.kind;
			out.Path = path;
			break;
		}
	}
	if (out.Found && out.Kind == ParticleTextureResolution::JPG)
	{
		const std::string png = folder + stem + ".png";
		out.Tried.push_back(png);
		if (exists(png))
		{
			out.AuxPngPath = png;
		}
	}
	return out;
}

bool MergeParticleJpgAlpha(std::uint8_t *rgba, int width, int height, const std::uint8_t *auxPixels, size_t auxSize, ParticleAuxAlphaFormat format)
{
	if (!rgba || !auxPixels || width <= 0 || height <= 0)
	{
		return false;
	}
	size_t stride = 0;
	size_t offset = 0;
	switch (format)
	{
	case ParticleAuxAlphaFormat::L8_OR_A8:
		stride = 1;
		offset = 0;
		break;
	case ParticleAuxAlphaFormat::LA8:
		stride = 2;
		offset = 1;
		break;
	case ParticleAuxAlphaFormat::RGBA8:
		stride = 4;
		offset = 3;
		break;
	default:
		return false;
	}
	const size_t texels = (size_t)width * (size_t)height;
	if (auxSize != texels * stride)
	{
		return false;
	}
	for (size_t i = 0; i < texels; ++i)
	{
		rgba[i * 4u + 3u] = auxPixels[i * stride + offset];
	}
	return true;
}
