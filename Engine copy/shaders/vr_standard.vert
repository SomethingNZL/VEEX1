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

// Probe data UBO - will be bound programmatically
layout (std140) uniform ProbeBlock {
    uint probeCount;
    uint _pad0, _pad1, _pad2;
    struct {
        vec4 position;       // xyz = position, w = radius
        vec4 intensity;      // xyz = intensity tint, w = blend mode
        vec4 boxExtents;     // AABB min
        vec4 boxExtentsMax;  // AABB max
    } probes[16];
} probeData;

out vec2 v_TexCoord;
out vec2 v_LMCoord;
out vec3 v_Normal;
out vec3 v_FragPos;
out mat3 v_TBN;
out vec3 v_TangentViewDir;

void main() {
    vec4 worldPos = u_Model * vec4(a_Pos, 1.0);
    v_FragPos = worldPos.xyz;
    v_TexCoord = a_TexCoord;
    v_LMCoord = a_LMCoord;

    // Transform normal to world space for proper lighting calculations
    v_Normal = normalize(u_NormalMatrix * a_Normal);
    
    // Compute tangent space matrix for normal mapping
    // Handle case where tangent might be zero (no normal mapping)
    vec3 tangent = a_Tangent.xyz;
    float tangentSign = a_Tangent.w;
    
    // If tangent is zero, create an orthogonal basis
    if (length(tangent) < 0.1) {
        // Create tangent from cross product with an arbitrary vector
        if (abs(v_Normal.z) < 0.99) {
            tangent = normalize(cross(v_Normal, vec3(0.0, 0.0, 1.0)));
        } else {
            tangent = normalize(cross(v_Normal, vec3(1.0, 0.0, 0.0)));
        }
        tangentSign = 1.0;
    }
    
    vec3 T = normalize(u_NormalMatrix * tangent);
    vec3 B = cross(v_Normal, T) * tangentSign;
    v_TBN = mat3(T, B, v_Normal);

    // Tangent-space view direction for parallax mapping
    vec3 worldViewDir = normalize(scene.u_CameraPos.xyz - worldPos.xyz);
    v_TangentViewDir = normalize(vec3(
        dot(worldViewDir, T),
        dot(worldViewDir, B),
        dot(worldViewDir, v_Normal)
    ));

    gl_Position = scene.u_ViewProj * worldPos;
}
