#version 450

// SDL_GPU convention: vertex uniforms in set 1.
layout(set = 1, binding = 0) uniform CameraUniforms {
    vec4 clip_transform; // xy = scale, zw = offset
};

// Per-instance attributes (one SpriteInstance per sprite). An instanced vertex
// buffer instead of a storage buffer: spirv-cross would otherwise give the
// storage buffer Metal slot 0, which SDL reserves for uniforms.
layout(location = 0) in vec4 in_rect; // world x, y, w, h
layout(location = 1) in vec4 in_uv;   // u, v, w, h
layout(location = 2) in vec4 in_tint;

layout(location = 0) out vec2 out_uv;
layout(location = 1) out vec4 out_tint;

// Two triangles per quad, generated from the vertex index.
const vec2 kCorners[6] = vec2[6](
    vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(0.0, 1.0),
    vec2(1.0, 0.0), vec2(1.0, 1.0), vec2(0.0, 1.0));

void main() {
    vec2 corner = kCorners[gl_VertexIndex];
    vec2 world = in_rect.xy + corner * in_rect.zw;
    gl_Position = vec4(world * clip_transform.xy + clip_transform.zw, 0.0, 1.0);
    out_uv = in_uv.xy + corner * in_uv.zw;
    out_tint = in_tint;
}
