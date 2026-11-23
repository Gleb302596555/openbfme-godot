// OpenBFME. GPL-3.0. See AptImageMode.h.

#include "GameClient/GUI/AptScreens/AptImageMode.h"

#include "Common/AsciiString.h"

AptImageMode parseAptImageMode(const std::string &mode)
{
	// RW 0x815814 (BFME2 0x412AF3): the six (mode, name) pairs with _strcmpi; no match or no value: ALPHA.
	// Verified in RotWK 2.01 (Port 331, S-5016 closed): W3DDisplay::drawImageCore (RW 0x44704F) maps 0->0 (OPAQUE),
	// 1->3 (ALPHA+GRAY), 2->2 (ALPHA), 3->1 (ADDITIVE), 4->4 (OPAQUE+GRAY), 5->6 (SOLID_TEXTURE); Render2DClass::Render
	// (RW 0x51A44D .. 0x51A5EA) applies Dot3 grey (TFACTOR 0x8092A587) for modes 3/4 and SOLID_TEXTURE alpha blend at RW 0x51A4D5.
	static const struct
	{
		AptImageMode mode;
		const char *name;
	} kModes[] = { { APT_IMAGE_SOLID, "SOLID" }, { APT_IMAGE_GRAYSCALE, "GRAYSCALE" }, { APT_IMAGE_ALPHA, "ALPHA" }, { APT_IMAGE_ADDITIVE, "ADDITIVE" },
		{ APT_IMAGE_SOLID_GRAYSCALE, "SOLID_GRAYSCALE" }, { APT_IMAGE_SOLID_TEXTURE, "SOLID_TEXTURE" } };
	for (const auto &m : kModes)
	{
		if (!mode.empty() && AsciiStringUtil::compareNoCase(mode, m.name) == 0)
		{
			return m.mode;
		}
	}
	return APT_IMAGE_ALPHA;
}

std::string aptImageModeShader(AptImageMode mode)
{
	if (mode == APT_IMAGE_ALPHA)
	{
		return std::string();
	}
	const bool solid = mode == APT_IMAGE_SOLID || mode == APT_IMAGE_SOLID_TEXTURE || mode == APT_IMAGE_SOLID_GRAYSCALE;
	const bool grayscale = mode == APT_IMAGE_GRAYSCALE || mode == APT_IMAGE_SOLID_GRAYSCALE;
	std::string code = "shader_type canvas_item;\n";
	if (mode == APT_IMAGE_ADDITIVE)
	{
		code += "render_mode blend_add;\n";
	}
	code += "varying vec4 v_tint;\n";
	code += "void vertex() { v_tint = COLOR; }\n";
	code += "void fragment() {\n\tvec4 pixel = texture(TEXTURE, UV);\n";
	if (solid)
	{
		code += "\tpixel.a = 1.0;\n";
	}
	if (grayscale)
	{
		// Render2DClass's grey (kRender2DGrayWeights, RW 0x51A5DC)
		code += "\tpixel.rgb = vec3(dot(pixel.rgb, vec3(" + std::to_string(kRender2DGrayWeights[0]) + ", " + std::to_string(kRender2DGrayWeights[1]) + ", " +
			std::to_string(kRender2DGrayWeights[2]) + ")));\n";
	}
	code += "\tCOLOR = pixel * v_tint;\n}\n";
	return code;
}

std::vector<std::string> aptImageModeStops()
{
	return { "[S-5016] HUD images and the farm placement feedback (community PR #37): a RenderImage clip's `_imageMap` that Palantir's dynamic image map "
			 "does not hold is looked up in the MappedImageCollection (BFME2 AptPlayer::FindRenderImage 0x223AC4: its body and so the order of the two "
			 "lookups was not read); the `_mode` render states (SOLID* without texture alpha, *GRAYSCALE by Render2DClass's Dot3 grey RW 0x51A5DC, ADDITIVE as "
			 "Godot's blend_add) are inferred (W3DDisplay's image draw was not read); the farm feedback (BFME2 PlaceTerrainResourceClaimantFeedback) is "
			 "shown for a template whose first TerrainResourceBehavior has MaxIncome > 0 (which templates retail gives it to was not read)" };
}
