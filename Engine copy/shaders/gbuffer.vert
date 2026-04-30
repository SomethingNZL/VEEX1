#version 330 core

layout (location = 0) in vec3 a_Pos;
layout (location = 1) in vec3 a_Normal;
layout (location = 2) in vec2 a_TexCoord;
layout (location = 3) in vec2 a_LMCoord;
layout (location = 4) in vec4 a_Tangent;
layout (location = 5) in vec2 a_LMCoord1;
layout (location = 6) in vec2 a_LMCoord2;
layout (location = 7) in vec3 a_FaceNormal;

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

// Output to G-buffer
out vec3 v_FragPos;
out vec3 v_Normal;
out vec3 v_Tangent;
out vec3 v_Bitangent;
out vec3 v_FaceNormal;
out vec2 v_TexCoord;
out vec2 v_LMCoord;
out vec2 v_LMCoord1;
out vec2 v_LMCoord2;
out vec4 v_Tangent4;
flat out int v_MaterialID;

void main() {
    vec4 worldPos = u_Model * vec4(a_Pos, 1.0);
    v_FragPos = worldPos.xyz;
    
    // Transform normal to world space
    v_Normal = normalize(u_NormalMatrix * a_Normal);
    v_FaceNormal = normalize(u_NormalMatrix * a_FaceNormal);
    
    // Tangent space vectors
    v_Tangent = normalize(u_NormalMatrix * a_Tangent.xyz);
    v_Bitangent = cross(v_Normal, v_Tangent) * a_Tangent.w;
    v_Tangent4 = vec4(v_Tangent, a_Tangent.w);
    
    v_TexCoord = a_TexCoord;
    v_LMCoord = a_LMCoord;
    v_LMCoord1 = a_LMCoord1;
    v_LMCoord2 = a_LMCoord2;
    
    // Simple material ID based on texture coordinates for now
    v_MaterialID = int(mod(v_TexCoord.x * 1000.0 + v_TexCoord.y * 1000.0, 255.0));
    
    gl_Position = scene.u_ViewProj * worldPos;
}