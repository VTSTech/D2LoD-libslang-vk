// glide3xvk starter preset: classic scanlines at source-line granularity.
// Written for the D2LoD-libslang-vk project (MIT). Single pass, no
// dependencies — the zero-risk smoke test for the P3 preset chain: enable it
// in d2slangvk.conf and the image should look identical apart from darkened
// every-other-source-line. Raise SCAN_INT via slangvk_chain_set_parameter.
#pragma name g3xvk_scanline

#pragma parameter SCAN_INT "Scanline darkness (0 = off, 1 = black)" 0.30 0.0 1.0 0.05

#pragma stage vertex
layout(location = 0) in vec4 Position;
layout(location = 1) in vec2 TexCoord;
layout(location = 0) out vec2 vTexCoord;
layout(std140, set = 0, binding = 0) uniform UBO
{
	mat4 MVP;
} global;

void main() {
	gl_Position = global.MVP * Position;
	vTexCoord = TexCoord;
}

#pragma stage fragment
layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 FragColor;
layout(std140, set = 0, binding = 1) uniform UBO
{
	vec4 SourceSize;
	float SCAN_INT;
} params;

uniform sampler2D Source;

void main() {
	vec4 c = texture(Source, vTexCoord);
	float line = floor(vTexCoord.y * params.SourceSize.y);
	float shade = 1.0 - (params.SCAN_INT * mod(line, 2.0));
	FragColor = vec4(c.rgb * shade, c.a);
}
