#version 300 es
precision highp float;

layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_uv;

layout(location = 3) in vec4 a_inst_m0;
layout(location = 4) in vec4 a_inst_m1;
layout(location = 5) in vec4 a_inst_m2;
layout(location = 6) in vec4 a_inst_m3;

uniform mat4 u_model;
uniform mat4 u_view_proj;
uniform mat3 u_normal_matrix;
uniform mat4 u_light_vp;
uniform int  u_instanced;

out vec3 v_world_pos;
out vec3 v_normal;
out vec2 v_uv;
out vec4 v_shadow_coord;

void main() {
    mat4 model = u_instanced != 0
        ? mat4(a_inst_m0, a_inst_m1, a_inst_m2, a_inst_m3)
        : u_model;
    vec4 wp = model * vec4(a_pos, 1.0);
    gl_Position = u_view_proj * wp;
    v_world_pos = wp.xyz;
    v_normal    = normalize(u_normal_matrix * a_normal);
    v_uv        = a_uv;
    v_shadow_coord = u_light_vp * wp;
}
