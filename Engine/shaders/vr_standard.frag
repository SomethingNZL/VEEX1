#version 330 core

in vec2 v_TexCoord;
in vec2 v_LMCoord;
in vec2 v_DetailTexCoord;
in vec3 v_Normal;
in vec3 v_FragPos;
in mat3 v_TBN;
in vec3 v_GeometricNormal;

uniform sampler2D u_MainTexture;
uniform sampler2D u_LightmapTexture;
uniform sampler2D u_NormalTexture;
uniform sampler2D u_RoughnessTexture;
uniform sampler2D u_DetailTexture;

// ── Reflection Probe Support ────────────────────────────────────────────────
uniform samplerCube u_ReflectionCubemap;
uniform int u_ProbeIndex;
uniform bool u_HasReflectionProbe;

uniform bool u_HasNormalMap;
uniform bool u_HasRoughnessMap;
uniform bool u_HasDetail;

// ── Paper's Lighting Model Parameters ───────────────────────────────────────
// Based on: "A Practical Real-Time Lighting Model for BSP-Based Renderers"
uniform float u_Roughness;                      // Material roughness (0-1)
uniform float u_DiffuseCoefficient;             // k_D in paper (default: 1.0)
uniform float u_LightmapBrightness;             // Lightmap exposure multiplier

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

// ── Reflection Probe UBO ────────────────────────────────────────────────────
#define MAX_REFLECTION_PROBES 16

layout (std140) uniform ReflectionProbeBlock {
    vec4 u_ProbePosition[MAX_REFLECTION_PROBES];
    vec4 u_ProbeData[MAX_REFLECTION_PROBES];       // xyz = radius, w = intensity
    int u_ProbeCount;
    int u_FaceProbeIndex[MAX_REFLECTION_PROBES];
} reflectionProbes;

out vec4 FragColor;

// ──────────────────────────────────────────────────────────────────────────────
// ROUGHNESS SYSTEM
// ──────────────────────────────────────────────────────────────────────────────
// Get effective roughness from texture or scalar uniform

float GetEffectiveRoughness() {
    if (u_HasRoughnessMap) {
        return texture(u_RoughnessTexture, v_TexCoord).r;
    }
    return u_Roughness;
}

// ──────────────────────────────────────────────────────────────────────────────
// FRESNEL (Schlick approximation)
// ──────────────────────────────────────────────────────────────────────────────
// F(N,V) = F_0 + (1 - F_0) * pow(1 - dot(N,V), 5)

vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

// Get F_0 based on metallic (dielectric = 0.04, metal = albedo)
vec3 GetF0(float metallic, vec3 albedo) {
    return mix(vec3(0.04), albedo, metallic);
}

// ──────────────────────────────────────────────────────────────────────────────
// SPECULAR (Blinn-Phong per paper)
// ──────────────────────────────────────────────────────────────────────────────
// s = mix(16, 1024, 1 - r)
// C_specular = F * (N · H)^s

vec3 SpecularBlinnPhong(vec3 N, vec3 H, float roughness, vec3 F, float NdotL) {
    // Paper's specular power mapping: s = mix(16, 1024, 1 - r)
    float shininess = mix(16.0, 1024.0, 1.0 - roughness);
    
    float NdotH = max(dot(N, H), 0.0);
    float specPower = pow(NdotH, shininess);
    
    // Paper: C_specular = F * (N · H)^s
    // Multiply by NdotL to ensure no specular when unlit
    return F * specPower * NdotL;
}

// ──────────────────────────────────────────────────────────────────────────────
// REFLECTION PROBE SAMPLING
// ──────────────────────────────────────────────────────────────────────────────
// Paper: S_r = mix(0.04, 1.0, F_0) * (1 - 0.5r)
// C_reflection = E(R, r) * S_r

vec3 SampleReflectionProbe(vec3 N, vec3 V, float roughness, vec3 F0) {
    if (!u_HasReflectionProbe || u_ProbeIndex < 0 || u_ProbeIndex >= reflectionProbes.u_ProbeCount) {
        return vec3(0.0);
    }
    
    // Calculate reflection vector
    vec3 R = reflect(-V, N);
    
    // Sample the cubemap
    vec3 reflectionColor = texture(u_ReflectionCubemap, R).rgb;
    
    // Get probe intensity
    float intensity = reflectionProbes.u_ProbeData[u_ProbeIndex].w;
    
    // Paper's reflection strength: S_r = mix(0.04, 1.0, F_0) * (1 - 0.5r)
    // Use average of F0 components for scalar F_0
    float F0_scalar = dot(F0, vec3(0.3333));
    float reflectionStrength = mix(0.04, 1.0, F0_scalar) * (1.0 - 0.5 * roughness);
    
    return reflectionColor * intensity * reflectionStrength;
}

// ──────────────────────────────────────────────────────────────────────────────
// MAIN
// ──────────────────────────────────────────────────────────────────────────────
// Paper's final equation:
// C = C_baked + C_direct + C_reflection
// where:
//   C_baked = L_M · A
//   C_direct = [A · k_D (N · L) + F (N · H)^s] · L_sun
//   C_reflection = E(R, r) · S_r

void main() {
    // ── 1. Sample Albedo ────────────────────────────────────────────────────
    vec4 albedo = texture(u_MainTexture, v_TexCoord);
    
    // ── 2. Detail Texture Blending ──────────────────────────────────────────
    if (u_HasDetail) {
        vec4 detail = texture(u_DetailTexture, v_DetailTexCoord);
        float blend = 1.0; // Will be overridden by uniform
        
        // Simple multiply blend for detail (common in Source)
        albedo.rgb *= mix(vec3(1.0), detail.rgb, blend);
    }
    
    // ── 3. Reconstruct Normals (Tangent-Space Normal Mapping) ───────────────
    // Paper: N = normalize(TBN · (2n_map - 1))
    vec3 geometricNormal = normalize(v_GeometricNormal);
    vec3 shadedNormal = geometricNormal;
    
    if (u_HasNormalMap) {
        vec3 nMap = texture(u_NormalTexture, v_TexCoord).rgb;
        nMap = nMap * 2.0 - 1.0;  // Convert [0,1] to [-1,1]
        shadedNormal = normalize(v_TBN * nMap);
    }
    
    // ── 4. Get Effective Roughness ──────────────────────────────────────────
    float roughness = GetEffectiveRoughness();
    
    // ── 5. Compute View/Light Directions ────────────────────────────────────
    vec3 V = normalize(scene.u_CameraPos.xyz - v_FragPos);
    vec3 L = normalize(-scene.u_SunDirection.xyz);
    vec3 H = normalize(L + V);
    
    // Dot products
    float NdotL = max(dot(shadedNormal, L), 0.0);
    float NdotV = max(dot(shadedNormal, V), 0.0);
    
    // ── 6. BAKED COMPONENT (Scalar Lightmap) ────────────────────────────────
    // Paper: C_baked = L_M · A
    vec3 bakedColor = vec3(0.0);
    #ifdef ENABLE_LIGHTMAPS
        vec3 lm = vec3(0.0);
        if (v_LMCoord.x >= 0.0 && v_LMCoord.y >= 0.0 &&
            v_LMCoord.x <= 1.001 && v_LMCoord.y <= 1.001) {
            lm = texture(u_LightmapTexture, clamp(v_LMCoord, 0.0, 1.0)).rgb;
            lm *= scene.u_LMExposure * u_LightmapBrightness;
        }
        
        // Pure scalar irradiance * albedo (no directional modulation)
        bakedColor = lm * albedo.rgb;
    #endif
    
    // ── 7. DIRECT ILLUMINATION ──────────────────────────────────────────────
    // Paper: C_direct = [A · k_D (N · L) + F (N · H)^s] · L_sun
    vec3 F0 = GetF0(0.0, albedo.rgb);  // Non-metallic (metallic = 0)
    vec3 F = FresnelSchlick(NdotV, F0);
    
    // Diffuse: A · k_D · (N · L)
    vec3 diffuseColor = albedo.rgb * u_DiffuseCoefficient * NdotL;
    
    // Specular: F · (N · H)^s
    vec3 specularColor = SpecularBlinnPhong(shadedNormal, H, roughness, F, NdotL);
    
    // Combined direct: (diffuse + specular) · L_sun
    vec3 directColor = (diffuseColor + specularColor) * scene.u_SunColor.rgb;
    
    // ── 8. REFLECTION PROBE ────────────────────────────────────────────────
    // Paper: C_reflection = E(R, r) · S_r
    vec3 reflectionColor = SampleReflectionProbe(shadedNormal, V, roughness, F0);
    
    // ── 9. FINAL COMPOSITION ────────────────────────────────────────────────
    // Paper: C = C_baked + C_direct + C_reflection
    vec3 finalColor = bakedColor + directColor + reflectionColor;
    
    // ── 10. FOG ─────────────────────────────────────────────────────────────
    #ifdef ENABLE_FOG
        float dist = length(scene.u_CameraPos.xyz - v_FragPos);
        float fogFactor = clamp((dist - scene.u_FogParams.x) / 
                               (scene.u_FogParams.y - scene.u_FogParams.x), 0.0, 1.0);
        finalColor = mix(finalColor, scene.u_FogColor.rgb, fogFactor);
    #endif
    
    FragColor = vec4(finalColor, albedo.a);
}