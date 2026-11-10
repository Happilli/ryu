#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 dims;
    vec4 radii;
    vec4 backgroundColor;
    vec4 borderColor;
    float borderWidth;
};

layout(binding = 1) uniform sampler2D content;

float sdRound(vec2 p, vec2 half_, vec4 r4) {
    float r = p.x < 0.0 ? (p.y < 0.0 ? r4.x : r4.w) : (p.y < 0.0 ? r4.y : r4.z);
    vec2 q = abs(p) - half_ + r;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;
}

void main() {
    vec2 p = (qt_TexCoord0 - 0.5) * dims;
    float d = sdRound(p, dims * 0.5, radii);
    float outer = clamp(0.5 - d, 0.0, 1.0);
    float inner = borderWidth > 0.0 ? clamp(0.5 - (d + borderWidth), 0.0, 1.0) : outer;
    vec4 c = texture(content, qt_TexCoord0);
    vec4 fill = c + backgroundColor * (1.0 - c.a);
    vec4 result = fill * inner + borderColor * (outer - inner);
    fragColor = result * qt_Opacity;
}
