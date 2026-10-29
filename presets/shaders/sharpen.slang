// glide3xvk starter preset: simple 3x3 sharpen (unsharp mask, one neighbor
// ring). Written for the D2LoD-libslang-vk project (MIT). Single pass — the
// mild-look counterpart to the scanline smoke preset: proves the chain while
// staying nearly invisible. SHARP 0 = identity.
#pragma name g3xvk_sharpen

#pragma parameter SHARP "Sharpen strength" 0.60 0.0 2.0 0.05

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
        float SHARP;
} params;

uniform sampler2D Source;

void main() {
        vec2 px = 1.0 / params.SourceSize.xy;
        vec4 c = texture(Source, vTexCoord);
        vec3 sum = texture(Source, vTexCoord - vec2(px.x, 0.0)).rgb
                 + texture(Source, vTexCoord + vec2(px.x, 0.0)).rgb
                 + texture(Source, vTexCoord - vec2(0.0, px.y)).rgb
                 + texture(Source, vTexCoord + vec2(0.0, px.y)).rgb;
        FragColor = vec4(clamp(c.rgb + params.SHARP * (c.rgb * 4.0 - sum), 0.0, 1.0), c.a);
}
