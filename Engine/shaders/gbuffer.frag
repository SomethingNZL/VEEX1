#version 330 core

// G-buffer attachments
layout (location = 0) out vec4 g_Position;     // World position
layout (location = 1) out vec4 g_Normal;       // World normal + material ID
layout (location = 2) out vec4 g_Albedo;       // Albedo color
layout (location = 3) out vec4 g_Material;     // Roughness, Metallic, AO, Lightmap exposure
layout (location = 4) out vec4 g_Lightmap;     // Lightmap coordinates and data

// Input from vertex shader
in vec3 v_FragPos;
in vec3 v_Normal;
in vec3 v_Tangent;
in vec3 v_Bitangent;
in vec3 v_FaceNormal;
in vec2 v_TexCoord;
in vec2 v_LMCoord;
in vec2 v_LMCoord1;
in vec2 v_LMCoord2;
in vec4 v_Tangent4;
flat in int v_MaterialID;

// Material textures
uniform sampler2D u_MainTexture;
uniform sampler2D u_NormalTexture;
uniform sampler2D u_LightmapTexture;
uniform sampler2D u_RoughnessTexture;
uniform sampler2D u_MetallicTexture;
uniform sampler2D u_AmbientOcclusionTexture;

// Material parameters
uniform bool u_HasNormalMap;
uniform bool u_HasRoughnessMap;
uniform bool u_HasMetallicMap;
uniform bool u_HasAOMap;
uniform bool u_HasLightmap;
uniform float u_Roughness;
uniform float u_Metallic;
uniform float u_AO;
uniform float u_LMExposure;
uniform float u_BumpScale;

// Scene uniforms
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

void main() {
    // Sample albedo
    vec4 albedo = texture(u_MainTexture, v_TexCoord);
    if (albedo.a < 0.5) discard;
    
    // Sample normal map and compute tangent space normal
    vec3 normal = normalize(v_Normal);
    if (u_HasNormalMap) {
        vec3 nMap = texture(u_NormalTexture, v_TexCoord).rgb;
        nMap = nMap * 2.0 - 1.0;
        
        // Apply bump scale
        nMap.z = mix(nMap.z, 1.0, 1.0 - u_BumpScale);
        nMap = normalize(nMap);
        
        // Transform from tangent space to world space
        mat3 TBN = mat3(v_Tangent, v_Bitangent, v_Normal);
        normal = normalize(TBN * nMap);
    }
    
    // Sample material properties
    float roughness = u_Roughness;
    float metallic = u_Metallic;
    float ao = u_AO;
    
    if (u_HasRoughnessMap) {
        roughness = texture(u_RoughnessTexture, v_TexCoord).r;
    }
    
    if (u_HasMetallicMap) {
        metallic = texture(u_MetallicTexture, v_TexCoord).r;
    }
    
    if (u_HasAOMap) {
        ao = texture(u_AmbientOcclusionTexture, v_TexCoord).r;
    }
    
    // Sample lightmap if available
    vec3 lightmap = vec3(0.0);
    if (u_HasLightmap && v_LMCoord.x >= 0.0 && v_LMCoord.y >= 0.0) {
        lightmap = texture(u_LightmapTexture, clamp(v_LMCoord, 0.0, 1.0)).rgb;
    }
    
    // Write to G-buffer
    g_Position = vec4(v_FragPos, 1.0);
    g_Normal = vec4(normal, float(v_MaterialID) / 255.0);
    g_Albedo = albedo;
    g_Material = vec4(roughness, metallic, ao, u_LMExposure);
    g_Lightmap = vec4(v_LMCoord, lightmap.r, lightmap.g);
}