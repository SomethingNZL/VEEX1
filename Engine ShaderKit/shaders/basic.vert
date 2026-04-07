#version 330 core
layout (location = 0) in vec3 a_Pos;
layout (location = 1) in vec3 a_Normal;
layout (location = 4) in vec4 a_Tangent;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;
uniform mat3 u_NormalMatrix;

out VS_OUT {
    vec3 normal;
    vec3 tangent;
    vec3 bitangent;
} vs_out;

void main() {
    gl_Position = view * model * vec4(a_Pos, 1.0); // Output in View Space
    
    mat3 normalMatrix = u_NormalMatrix;
    vs_out.normal = normalize(normalMatrix * a_Normal);
    vs_out.tangent = normalize(normalMatrix * a_Tangent.xyz);
    vs_out.bitangent = normalize(cross(vs_out.normal, vs_out.tangent) * a_Tangent.w);
}