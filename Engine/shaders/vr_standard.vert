#version 330 core

// Attributes from Renderer.cpp VAO setup
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in vec2 aLMCoord;
layout (location = 4) in vec4 aTangent; // xyz = Tangent, w = Bitangent sign

// UBO for Scene Data (std140)
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

uniform mat4 u_Model;
uniform mat3 u_NormalMatrix;

// Outputs to Fragment Shader
out vec3 v_WorldPos;
out vec2 v_TexCoord;
out vec2 v_LMCoord;
out vec3 v_Normal;
out vec3 v_Tangent;
out vec3 v_Bitangent;

void main() {
    vec4 worldPos = u_Model * vec4(aPos, 1.0);
    v_WorldPos = worldPos.xyz;
    v_TexCoord = aTexCoord;
    v_LMCoord  = aLMCoord;
    
    // Transform normal to world space
    vec3 N = normalize(u_NormalMatrix * aNormal);
    v_Normal = N;

    // Split TBN into vectors for better interpolation stability on older drivers
    vec3 T = normalize(u_NormalMatrix * aTangent.xyz);
    
    // Gram-Schmidt Orthonormalization: ensures T is perfectly 90 degrees to N
    T = normalize(T - dot(T, N) * N);
    
    vec3 B = cross(N, T) * aTangent.w;
    
    v_Tangent = T;
    v_Bitangent = B;

    gl_Position = scene.u_ViewProj * worldPos;
}