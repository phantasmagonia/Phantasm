#version 450

// SDL_GPU binding convention: vertex-stage uniform buffers live in set 1.
layout(set = 1, binding = 0) uniform SpriteUniforms {
    vec4 clip_rect; // x, y = top-left in clip space; z, w = size (w negative)
};

layout(location = 0) out vec2 out_uv;

// Two triangles forming a quad; generated from the vertex index, so no vertex buffer.
const vec2 kCorners[6] = vec2[6](
    vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(0.0, 1.0),
    vec2(1.0, 0.0), vec2(1.0, 1.0), vec2(0.0, 1.0));

void main() {
    vec2 corner = kCorners[gl_VertexIndex];
    out_uv = corner;
    gl_Position = vec4(clip_rect.xy + corner * clip_rect.zw, 0.0, 1.0);
}
