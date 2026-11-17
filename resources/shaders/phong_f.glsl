#version 460 core
in vec2 v_tex_coord;
in vec3 v_pos;
in vec3 v_normal;

uniform sampler2D tex;
uniform vec3 u_tint;
uniform vec3 u_light_pos;
uniform vec3 u_light_color;
uniform vec3 u_view_pos;
uniform float u_time;
uniform int u_is_water;

out vec4 o_color;

void main()
{
    vec3 normal = normalize(v_normal);
    vec3 light_dir = normalize(u_light_pos - v_pos);
    float diff = max(dot(normal, light_dir), 0.0);
    // Keep the original Phong lighting for houses/trees/land.
    vec3 ambient = 0.30 * u_light_color;
    vec3 diffuse = diff * u_light_color;

    vec3 view_dir = normalize(u_view_pos - v_pos);
    vec3 reflected = reflect(-light_dir, normal);
    float shininess = u_is_water != 0 ? 80.0 : 32.0;
    float spec = pow(max(dot(view_dir, reflected), 0.0), shininess);
    vec3 specular = (u_is_water != 0 ? 0.35 : 0.50) * spec * u_light_color;

    vec4 surface;
    if (u_is_water != 0)
    {
        // Two moving layers and gentle ripples suggest steady river flow.
        // The ribbon UV's Y direction follows world Z, along the river.
        vec2 distortion = vec2(
            sin(v_tex_coord.y * 12.0 + u_time * 1.3),
            cos(v_tex_coord.x * 8.0 - u_time * 1.7)) * 0.010;
        vec2 uv_a = v_tex_coord + distortion + vec2(0.017 * u_time, -0.105 * u_time);
        vec2 uv_b = v_tex_coord * 1.55 - distortion + vec2(-0.012 * u_time, -0.155 * u_time);
        vec3 layer_a = texture(tex, uv_a).rgb;
        vec3 layer_b = texture(tex, uv_b).rgb;
        vec3 water = mix(layer_a, layer_b, 0.28);
        float glint = pow(max(0.0, sin(v_tex_coord.y * 33.0 - u_time * 4.2
                           + sin(v_tex_coord.x * 17.0))), 12.0) * 0.085;
        water += vec3(glint);
        surface = vec4(water, 0.84);
    }
    else
    {
        surface = texture(tex, v_tex_coord);
    }

    if (surface.a < 0.12) discard;

    vec3 lit = (ambient + diffuse + specular) * surface.rgb;
    if (u_is_water != 0)
        lit = (ambient + diffuse * 0.66) * surface.rgb * u_tint +
              specular * 0.50;
    // Smooth edge haze: no fog in the town, but the square terrain fades
    // into the same color as the clear buffer before its +/-16 borders.
    float edge = max(abs(v_pos.x), abs(v_pos.z));
    float fog = smoothstep(13.0, 16.0, edge);
    vec3 fog_color = vec3(0.45, 0.55, 0.60);
    o_color = vec4(mix(lit, fog_color, fog), mix(surface.a, 1.0, fog));
}
