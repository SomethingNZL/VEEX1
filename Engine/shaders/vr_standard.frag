#version 330 core

in vec2 v_TexCoord;
in vec2 v_LMCoord;
in vec2 v_DetailTexCoord;   // Detail texture coordinate (scaled by $detailscale)
in vec3 v_Normal;
in vec3 v_FragPos;
in mat3 v_TBN;
in vec3 v_GeometricNormal;    // Face normal for geometric roughness fallback

uniform sampler2D u_MainTexture;
uniform sampler2D u_LightmapTexture;
uniform sampler2D u_NormalTexture;
uniform sampler2D u_RoughnessTexture;
uniform sampler2D u_DetailTexture;  // $detail texture

uniform bool u_HasNormalMap;
uniform bool u_HasRoughnessMap;
uniform bool u_HasDetail;           // Has detail texture

// ── PBR-lite tuning parameters ───────────────────────────────────────────────
uniform float u_Roughness;                      // Material roughness scalar (0-1)
uniform float u_LightmapSoftness;               // Lightmap directional softness (default: 0.5)
uniform float u_DiffuseFlattening;              // Diffuse flattening for rough surfaces (default: 0.5)
uniform float u_EdgePower;                      // Edge term power (default: 2.0)
uniform float u_GeometricRoughnessPower;        // Curvature sensitivity (default: 4.0)
uniform float u_LightmapBrightness;             // Lightmap brightness multiplier (default: 1.0)

// ── VMT Detail Texture Parameters ──────────────────────────────────────────────
uniform vec2 u_DetailScale;                     // Detail texture tiling scale
uniform float u_DetailBlendFactor;              // Detail blend intensity (0-1)
uniform int u_DetailBlendMode;                  // 0 = multiply, 1 = add, 2 = lerp

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

out vec4 FragColor;

// ──────────────────────────────────────────────────────────────────────────────
// GEOMETRIC ROUGHNESS FALLBACK
// ──────────────────────────────────────────────────────────────────────────────
// When no material roughness is available, derive "effective roughness" from
// the deviation between the shaded normal (normal map or interpolated) and the
// geometric face normal.
//
// flat surface  → dot(N, Ng) ≈ 1 → R_geo ≈ 0 (smooth)
// high deviation → dot(N, Ng) ≈ 0 → R_geo ≈ 1 (rough)

float ComputeGeometricRoughness(vec3 N, vec3 Ng) {
    float dotVal = max(abs(dot(N, Ng)), 0.0);
    return 1.0 - pow(dotVal, u_GeometricRoughnessPower);
}

// ──────────────────────────────────────────────────────────────────────────────
// UNIFIED ROUGHNESS SYSTEM
// ──────────────────────────────────────────────────────────────────────────────
// R_eff = materialRoughness  OR  fallbackRoughness
//
// This guarantees a single shading path regardless of whether the material
// provides a roughness map or scalar.

float GetEffectiveRoughness(vec3 shadedNormal, vec3 geometricNormal) {
    if (u_HasRoughnessMap) {
        // Sample roughness from texture
        float texRough = texture(u_RoughnessTexture, v_TexCoord).r;
        return texRough;
    }
    if (u_Roughness > 0.0) {
        return u_Roughness;
    }
    // Fallback: derive from geometry
    return ComputeGeometricRoughness(shadedNormal, geometricNormal);
}

// ──────────────────────────────────────────────────────────────────────────────
// FRESNEL (Schlick approximation)
// ──────────────────────────────────────────────────────────────────────────────
// F(N,V) = F0 + (1 - F0) * pow(1 - dot(N,V), 5)
// Used strictly for energy redistribution between diffuse and specular.

vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 GetF0(float metallic, vec3 albedo) {
    // Dielectric F0 ≈ 0.04, metallic uses albedo tint
    return mix(vec3(0.04), albedo, metallic);
}

// ──────────────────────────────────────────────────────────────────────────────
// SPECULAR (Normalized Blinn-Phong)
// ──────────────────────────────────────────────────────────────────────────────
// S(N,H,R_eff) = normalizedBlinnPhong(N,H, shininess)
// shininess = mix(16, 8192, pow(1 - R_eff, 4))

vec3 SpecularBlinnPhong(vec3 N, vec3 H, float roughness, vec3 lightColor, float NdotL) {
    // Shininess derived from roughness: smooth(0) → 8192, rough(1) → 16
    float shininess = mix(8192.0, 16.0, pow(roughness, 4.0));
    
    float NdotH = max(dot(N, H), 0.0);
    float specPower = pow(NdotH, shininess);
    
    // Normalize Blinn-Phong: scale by (shininess + 8) / (8 * PI)
    float normalization = (shininess + 8.0) / (8.0 * 3.14159265359);
    
    return lightColor * specPower * normalization * NdotL;
}

// ──────────────────────────────────────────────────────────────────────────────
// EDGE TERM
// ──────────────────────────────────────────────────────────────────────────────
// E(N,V) = 1 - pow(max(dot(N,V), 0), edgePower)
// Used for grazing-angle control.

float EdgeTerm(vec3 N, vec3 V) {
    float NdotV = max(dot(N, V), 0.0);
    return 1.0 - pow(NdotV, u_EdgePower);
}

// ──────────────────────────────────────────────────────────────────────────────
// MAIN
// ──────────────────────────────────────────────────────────────────────────────

void main() {
    // ── 1. Sample Textures ────────────────────────────────────────────────
    vec4 albedo = texture(u_MainTexture, v_TexCoord);
    
    // ── 2. Detail Texture Blending ─────────────────────────────────────────
    if (u_HasDetail) {
        vec4 detail = texture(u_DetailTexture, v_DetailTexCoord);
        
        // Apply detail blend factor
        float blend = u_DetailBlendFactor;
        
        // Blend modes: 0 = multiply, 1 = add, 2 = lerp
        if (u_DetailBlendMode == 0) {
            // Multiply blend (common for detail textures)
            albedo.rgb *= mix(vec3(1.0), detail.rgb, blend);
        } else if (u_DetailBlendMode == 1) {
            // Add blend
            albedo.rgb += detail.rgb * blend;
        } else {
            // Lerp blend (default)
            albedo.rgb = mix(albedo.rgb, detail.rgb, blend);
        }
    }
    
    // ── 3. Reconstruct Normals ────────────────────────────────────────────
    vec3 geometricNormal = normalize(v_GeometricNormal);   // Face normal
    vec3 shadedNormal = geometricNormal;
    
    if (u_HasNormalMap) {
        vec3 nMap = texture(u_NormalTexture, v_TexCoord).rgb;
        nMap = nMap * 2.0 - 1.0;
        shadedNormal = normalize(v_TBN * nMap);
    }
    
    // ── 4. Compute Effective Roughness ────────────────────────────────────
    float R_eff = GetEffectiveRoughness(shadedNormal, geometricNormal);
    
    // ── 5. View/Light Directions ──────────────────────────────────────────
    vec3 V = normalize(scene.u_CameraPos.xyz - v_FragPos);
    vec3 lightDir = normalize(-scene.u_SunDirection.xyz);
    vec3 H = normalize(lightDir + V);
    
    // ── 6. BAKED COMPONENT (Lightmap only) ─────────────────────────────────
    // C_baked = LM * A * brightness * exposure * (1 - R_eff * lightmapSoftness)
    vec3 bakedColor = vec3(0.0);
    #ifdef ENABLE_LIGHTMAPS
        vec3 lm = vec3(0.0);
        if (v_LMCoord.x >= 0.0 && v_LMCoord.y >= 0.0 &&
            v_LMCoord.x <= 1.001 && v_LMCoord.y <= 1.001) {
            lm = texture(u_LightmapTexture, clamp(v_LMCoord, 0.0, 1.0)).rgb;
            // Apply exposure and brightness adjustments
            lm *= scene.u_LMExposure * u_LightmapBrightness;
        }
        
        // Directional softness factor: rough surfaces scatter lightmap more
        float lmDirectional = 1.0 - R_eff * u_LightmapSoftness;
        
        // Pure lightmap contribution
        bakedColor = lm * albedo.rgb * lmDirectional;
    #endif
    
    // ── 7. DYNAMIC COMPONENT ──────────────────────────────────────────────
    vec3 dynamicColor = vec3(0.0);
    vec3 F0 = GetF0(0.0, albedo.rgb); // Non-metallic (metallic = 0)
    float NdotV = max(dot(shadedNormal, V), 0.0);
    vec3 F = FresnelSchlick(NdotV, F0);
    
    // Diffuse term: D(N,L,R_eff) = max(dot(N,L), 0) * (1 - R_eff * diffuseFlattening)
    float NdotL = max(dot(shadedNormal, lightDir), 0.0);
    float diffuseFactor = (1.0 - R_eff * u_DiffuseFlattening);
    
    // Specular term: S(N,H,R_eff)
    vec3 specular = SpecularBlinnPhong(shadedNormal, H, R_eff, scene.u_SunColor.rgb, NdotL);
    
    // Energy conservation: (1 - F) for diffuse, F for specular
    vec3 diffuseContrib = (1.0 - F) * NdotL * diffuseFactor * scene.u_SunColor.rgb;
    vec3 specularContrib = F * specular;
    
    // Edge term for grazing angle control
    float edge = EdgeTerm(shadedNormal, V);
    
    dynamicColor = (diffuseContrib + specularContrib) * edge;
    
    // ── 8. FINAL COMPOSITION ──────────────────────────────────────────────
    // C = BAKED + DYNAMIC
    vec3 finalColor = bakedColor + dynamicColor * albedo.rgb;
    
    // ── 9. FOG ────────────────────────────────────────────────────────────
    #ifdef ENABLE_FOG
        float dist = length(scene.u_CameraPos.xyz - v_FragPos);
        float fogFactor = clamp((scene.u_FogParams.y - dist) / 
                               (scene.u_FogParams.y - scene.u_FogParams.x), 0.0, 1.0);
        finalColor = mix(scene.u_FogColor.rgb, finalColor, fogFactor);
    #endif
    
    FragColor = vec4(finalColor, albedo.a);
}