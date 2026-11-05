// OpenBFME. GPL-3.0.
//
// The terrain's scorch marks (GodotTerrainScorches.h), drawn as RotWK's drawScorches does (RW 0x468BCB): the fixed-function shader at
// RW 0xD9B2F0 (BFME2 decomp ShaderClass::_PresetAlphaShader: texture times the vertex diffuse, alpha = texture alpha times the vertex alpha,
// blended src alpha / inverse src alpha, no depth write) with the hardware fog when fog is on (the fog blends the colour toward the fog colour by
// the eye distance, the alpha untouched), the texture's U and V clamped (RW 0x468C56 .. 0x468C8E: SetSamplerState ADDRESSU / ADDRESSV = CLAMP).
// The colour hand-over is the terrain's (terrain_common.gdshaderinc retail_output): the map root composites the transparent pass in gamma space,
// so output_gamma 1.0 hands the gamma-space value over as it is.
// The projection depth bias of RW 0x51EB60 (1.0 while the marks draw, RW 0x468C43): retail grows the projection's _43 by the factor
// (1 + 1.0 * 0.000625), which gives every point the depth of the point at d / (1 + 0.000625); the vertex shader draws the eye-space vertex scaled
// by depth_bias_scale (TerrainScorchBuffer::depthBiasViewScale), so the screen position stays and only the depth moves. The fog keeps the unbiased
// eye distance (retail's hardware fog reads the eye-space vertex, not the projection). The marks' vertices lie 1.0 above the height map (RW 0x4674C1).
shader_type spatial;
render_mode unshaded, blend_mix, depth_draw_never, cull_disabled, fog_disabled;

uniform sampler2D scorch_tex : filter_linear_mipmap, repeat_disable, hint_default_white;
uniform float fog_enabled = 0.0;
uniform vec3 fog_color = vec3(0.5);
uniform float fog_range_start = 0.0;
uniform float fog_range_end = 1.0;
global uniform float w3d_fog_shift; // lane PLAY-1: the free camera's extra eye distance (TacticalCamera::fogShift; 0 = retail)
uniform float output_gamma = 1.0;
uniform float depth_bias_scale = 1.0; // TerrainScorchBuffer::depthBiasViewScale(DEPTH_BIAS), set by the terrain builder

varying float v_fog;

void vertex() {
	vec3 world = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	float dist = length(CAMERA_POSITION_WORLD - world) - w3d_fog_shift;
	v_fog = 1.0 - fog_enabled * clamp((dist - fog_range_start) / max(fog_range_end - fog_range_start, 0.0001), 0.0, 1.0);
	vec4 eye = MODELVIEW_MATRIX * vec4(VERTEX, 1.0);
	POSITION = PROJECTION_MATRIX * vec4(eye.xyz * depth_bias_scale, 1.0);
}

vec3 retail_output(vec3 c) {
	c = max(c, vec3(0.0));
	if (output_gamma == 1.0) {
		return c;
	}
	return mix(c / 12.92, pow((c + 0.055) / 1.055, vec3(2.4)), step(vec3(0.04045), c));
}

void fragment() {
	vec4 t = texture(scorch_tex, UV);
	vec3 c = t.rgb * COLOR.rgb;
	c = mix(fog_color, c, v_fog);
	ALBEDO = retail_output(c);
	ALPHA = t.a * COLOR.a;
}
