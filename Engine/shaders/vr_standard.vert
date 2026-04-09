#version 330 core

layout (location = 0) in vec3 a_Pos;
layout (location = 1) in vec3 a_Normal;
layout (location = 2) in vec2 a_TexCoord;
layout (location = 3) in vec2 a_LMCoord;
layout (location = 4) in vec4 a_Tangent;

uniform mat4 u_Model;
uniform mat3 u_NormalMatrix;

layout (std140) uniform SceneBlock {
    mat4 u_ViewProj;
    vec4 u_CameraPos;
    vec4 u_SunDirection;
    vec4 u_SunColor;
    vec4 u_FogParams;
    vec4 u_FogColor;
    vec4 u_AmbientCube[6];
    vec4 u_WaterParams;
    vec4 u_WaterColor;
    float u_LMExposure;
    float u_Time;
} scene;

out vec2 v_TexCoord;
out vec2 v_LMCoord;
out vec3 v_Normal;
out vec3 v_FragPos;
out mat3 v_TBN;

void main() {
    vec4 worldPos = u_Model * vec4(a_Pos, 1.0);
    v_FragPos = worldPos.xyz;
    v_TexCoord = a_TexCoord;
    v_LMCoord = a_LMCoord;

    v_Normal = normalize(u_NormalMatrix * a_Normal);
    vec3 T = normalize(u_NormalMatrix * a_Tangent.xyz);
    vec3 B = cross(v_Normal, T) * a_Tangent.w;
    v_TBN = mat3(T, B, v_Normal);

    gl_Position = scene.u_ViewProj * worldPos;
}