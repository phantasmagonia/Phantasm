#version 450

layout(location = 0) in vec2 in_uv;
layout(location = 0) out vec4 out_color;

// SDL_GPU binding convention: fragment-stage sampled textures live in set 2.
layout(set = 2, binding = 0) uniform sampler2D sprite_texture;

void main() {
    out_color = texture(sprite_texture, in_uv);
}
