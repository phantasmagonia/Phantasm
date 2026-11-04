#version 450

layout(location = 0) in vec2 in_uv;
layout(location = 1) in vec4 in_tint;
layout(location = 0) out vec4 out_color;

// SDL_GPU convention: fragment samplers in set 2.
layout(set = 2, binding = 0) uniform sampler2D sprite_texture;

void main() {
    out_color = texture(sprite_texture, in_uv) * vec4(in_tint.rgb * in_tint.a, in_tint.a);
}
