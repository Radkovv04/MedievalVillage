
#version 460 core

in vec2 v_tex_coord;

uniform sampler2D tex;
uniform vec3 u_tint;

out vec4 o_color;

void main()
{
    vec4 texture_color = texture(tex, v_tex_coord);
    o_color = vec4(texture_color.rgb * u_tint, texture_color.a);
}
