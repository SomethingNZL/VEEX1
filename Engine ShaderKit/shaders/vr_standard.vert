#version 330 core

layout (location = 0) in vec3 a_Pos;
layout (location = 1) in vec3 a_Normal;
layout (location = 2) in vec2 a_TexCoord;
layout (location = 3) in vec2 a_LmCoord;
layout (location = 4) in vec4 a_Tangent;

uniform mat4 u_Model;
uniform mat3 u_NormalMatrix;

layout (std140) uniform SceneBlock {
    mat4 viewProj;
    vec4 cameraPos;
    vec4 sunDir;
    vec4 sunColor;
    vec4 fogParams;
    vec4 fogColor;
    vec4 ambientCube[6];
} scene;

out vec2 v_TexCoord;
out vec2 v_LmCoord;
out vec3 v_Normal;
out vec3 v_FragPos;
out mat3 v_TBN;

void main() {
    vec4 worldPos = u_Model * vec4(a_Pos, 1.0);
    v_FragPos = worldPos.xyz;
    v_TexCoord = a_TexCoord;
    v_LmCoord = a_LmCoord;

    // Normal and TBN Calculation
    v_Normal = normalize(u_NormalMatrix * a_Normal);
    
    vec3 T = normalize(u_NormalMatrix * a_Tangent.xyz);
    vec3 N = v_Normal;
    // MikkTSpace bitangent reconstruction (tangent.w is the sign)
    vec3 B = cross(N, T) * a_Tangent.w;
    v_TBN = mat3(T, B, N);

    gl_Position = scene.viewProj * worldPos;
}