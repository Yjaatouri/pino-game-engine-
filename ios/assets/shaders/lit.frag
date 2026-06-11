#version 300 es
precision highp float;

struct AmbientLight {
    vec3 color;
    float intensity;
};

struct DirectionalLight {
    vec3 direction;
    vec3 color;
};

struct PointLight {
    vec3 position;
    vec3 color;
    float constant;
    float linear;
    float quadratic;
};

#define MAX_POINT_LIGHTS 8

uniform AmbientLight      u_ambient;
uniform DirectionalLight  u_dir_light;
uniform int               u_num_point_lights;
uniform PointLight        u_point_lights[MAX_POINT_LIGHTS];
uniform vec3              u_camera_pos;

uniform vec3    u_mat_ambient;
uniform vec3    u_mat_diffuse;
uniform vec3    u_mat_specular;
uniform vec3    u_mat_emissive;
uniform float   u_mat_shininess;
uniform sampler2D u_mat_diffuse_tex;
uniform int     u_has_diffuse_tex;

uniform sampler2D u_shadow_map;
uniform mat4    u_light_vp;
uniform float   u_shadow_bias;
uniform int     u_shadow_enabled;

in vec3 v_world_pos;
in vec3 v_normal;
in vec2 v_uv;
in vec4 v_shadow_coord;
out vec4 frag_color;

float calc_shadow(vec4 world_pos, vec3 n, vec3 light_dir) {
    vec4 lsp = u_light_vp * world_pos;
    vec3 proj = lsp.xyz / lsp.w;
    proj = proj * 0.5 + 0.5;

    if (proj.x < 0.0 || proj.x > 1.0 ||
        proj.y < 0.0 || proj.y > 1.0 ||
        proj.z < 0.0 || proj.z > 1.0)
        return 0.0;

    float current = proj.z;

    // 3x3 PCF
    vec2 texel = 1.0 / vec2(textureSize(u_shadow_map, 0));
    float shadow = 0.0;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float d = texture(u_shadow_map, proj.xy + vec2(x, y) * texel).r;
            shadow += current - u_shadow_bias > d ? 1.0 : 0.0;
        }
    }
    return shadow / 9.0;
}

vec3 phong_dir(DirectionalLight l, vec3 n, vec3 v) {
    vec3  ld = normalize(-l.direction);
    float d  = max(dot(n, ld), 0.0);
    vec3  h  = normalize(ld + v);
    float s  = pow(max(dot(n, h), 0.0), u_mat_shininess);
    vec3  dt = u_has_diffuse_tex == 1 ? texture(u_mat_diffuse_tex, v_uv).rgb : vec3(1.0);
    return (d * u_mat_diffuse * dt + s * u_mat_specular) * l.color;
}

vec3 phong_point(PointLight l, vec3 n, vec3 p, vec3 v) {
    vec3  ld = l.position - p;
    float dist = length(ld);
    ld = normalize(ld);
    float atten = 1.0 / (l.constant + l.linear * dist + l.quadratic * dist * dist);
    float d  = max(dot(n, ld), 0.0);
    vec3  h  = normalize(ld + v);
    float s  = pow(max(dot(n, h), 0.0), u_mat_shininess);
    vec3  dt = u_has_diffuse_tex == 1 ? texture(u_mat_diffuse_tex, v_uv).rgb : vec3(1.0);
    return (d * u_mat_diffuse * dt + s * u_mat_specular) * l.color * atten;
}

void main() {
    vec3 nrm = normalize(v_normal);
    vec3 view_dir = normalize(u_camera_pos - v_world_pos);

    vec3 diff_tex = u_has_diffuse_tex == 1 ? texture(u_mat_diffuse_tex, v_uv).rgb : vec3(1.0);
    vec3 color = u_mat_emissive * diff_tex;

    color += u_ambient.color * u_ambient.intensity * u_mat_ambient * diff_tex;

    vec3 dir_result = phong_dir(u_dir_light, nrm, view_dir);
    if (u_shadow_enabled != 0) {
        float shadow = calc_shadow(vec4(v_world_pos, 1.0), nrm, normalize(-u_dir_light.direction));
        dir_result *= (1.0 - shadow);
    }
    color += dir_result;

    for (int i = 0; i < u_num_point_lights; ++i)
        color += phong_point(u_point_lights[i], nrm, v_world_pos, view_dir);

    color = clamp(color, 0.0, 1.6);

    frag_color = vec4(color, 1.0);
}
