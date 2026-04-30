#version 330 core

// G-buffer inputs
uniform sampler2D g_Position;
uniform sampler2D g_Normal;
uniform sampler2D g_Albedo;
uniform sampler2D g_Material;
uniform sampler2D g_Lightmap;

// Other textures
uniform sampler2D u_EnvironmentMap;
uniform sampler2D u_EnvironmentMapMip0;
uniform sampler2D u_EnvironmentMapMip1;
uniform sampler2D u_EnvironmentMapMip2;
uniform sampler2D u_EnvironmentMapMip3;
uniform sampler2D u_EnvironmentMapMip4;
uniform sampler2D u_EnvironmentMapMip5;

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

in vec2 v_TexCoord;
out vec4 FragColor;

// Constants
const float PI = 3.14159265359;

// Utility functions
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    
    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    
    return nom / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    
    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    
    return nom / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    
    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

// Environment map sampling
vec3 SampleEnvironmentMap(vec3 R, float roughness) {
    float mip = roughness * 5.0;
    
    if (mip < 1.0) {
        return texture(u_EnvironmentMapMip0, R.xy).rgb;
    } else if (mip < 2.0) {
        return texture(u_EnvironmentMapMip1, R.xy).rgb;
    } else if (mip < 3.0) {
        return texture(u_EnvironmentMapMip2, R.xy).rgb;
    } else if (mip < 4.0) {
        return texture(u_EnvironmentMapMip3, R.xy).rgb;
    } else if (mip < 5.0) {
        return texture(u_EnvironmentMapMip4, R.xy).rgb;
    } else {
        return texture(u_EnvironmentMapMip5, R.xy).rgb;
    }
}

// Ambient cube lighting function for RNM
vec3 AmbientCubeLight(vec3 normal) {
    vec3 nSquared = normal * normal;
    vec3 isNegative = step(normal, vec3(0.0));
    
    vec3 linearColor = nSquared.x * scene.u_AmbientCube[int(isNegative.x)].rgb +
                       nSquared.y * scene.u_AmbientCube[int(isNegative.y) + 2].rgb +
                       nSquared.z * scene.u_AmbientCube[int(isNegative.z) + 4].rgb;
    return linearColor;
}

void main() {
    // Read from G-buffer
    vec3 FragPos = texture(g_Position, v_TexCoord).rgb;
    vec4 normalData = texture(g_Normal, v_TexCoord);
    vec3 Normal = normalData.rgb;
    float materialID = normalData.a;
    
    vec4 albedoData = texture(g_Albedo, v_TexCoord);
    vec3 Albedo = albedoData.rgb;
    float alpha = albedoData.a;
    
    vec4 materialData = texture(g_Material, v_TexCoord);
    float Roughness = materialData.r;
    float Metallic = materialData.g;
    float AO = materialData.b;
    float LMExposure = materialData.a;
    
    vec4 lightmapData = texture(g_Lightmap, v_TexCoord);
    vec2 LMCoord = lightmapData.xy;
    vec3 Lightmap = vec3(lightmapData.zw, 0.0);
    
    // Discard transparent pixels
    if (alpha < 0.5) discard;
    
    // View direction
    vec3 ViewDir = normalize(scene.u_CameraPos.xyz - FragPos);
    
    // Basic material properties
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, Albedo, Metallic);
    
    // Lighting calculations
    vec3 Lo = vec3(0.0); // Outgoing light
    
    // 1. Direct lighting (Sun)
    vec3 LightDir = normalize(-scene.u_SunDirection.xyz);
    vec3 HalfDir = normalize(ViewDir + LightDir);
    
    float NDF = DistributionGGX(Normal, HalfDir, Roughness);
    float G = GeometrySmith(Normal, ViewDir, LightDir, Roughness);
    vec3 F = fresnelSchlick(max(dot(HalfDir, ViewDir), 0.0), F0);
    
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - Metallic;
    
    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(Normal, ViewDir), 0.0) * max(dot(Normal, LightDir), 0.0) + 0.001;
    vec3 specular = numerator / denominator;
    
    float NdotL = max(dot(Normal, LightDir), 0.0);
    Lo += (kD * Albedo / PI + specular) * scene.u_SunColor.rgb * NdotL;
    
    // 2. Ambient lighting (IBL)
    vec3 R = reflect(-ViewDir, Normal);
    
    // Diffuse irradiance from ambient cube
    vec3 irradiance = AmbientCubeLight(Normal);
    vec3 diffuse = irradiance * Albedo;
    
    // Specular IBL
    vec3 prefilteredColor = SampleEnvironmentMap(R, Roughness);
    vec3 envBRDF = vec3(0.0);
    
    // Simple approximation for BRDF integration
    float NoV = max(dot(Normal, ViewDir), 0.0);
    envBRDF.r = 0.16 * Roughness * Roughness + (1.0 - Roughness) * 0.04;
    envBRDF.g = 0.16 * Roughness * Roughness + (1.0 - Roughness) * 0.04;
    envBRDF.b = 0.16 * Roughness * Roughness + (1.0 - Roughness) * 0.04;
    
    vec3 specularIBL = prefilteredColor * (F0 * envBRDF.x + (1.0 - F0) * envBRDF.y);
    
    // Combine ambient
    vec3 ambient = (diffuse + specularIBL) * AO;
    
    // 3. Lightmap contribution
    vec3 lightmapContribution = vec3(0.0);
    if (LMCoord.x >= 0.0 && LMCoord.y >= 0.0) {
        lightmapContribution = Lightmap * LMExposure;
    }
    
    // Final color
    vec3 color = ambient + Lo + lightmapContribution;
    
    // Apply fog
    float dist = length(scene.u_CameraPos.xyz - FragPos);
    float fogFactor = clamp((scene.u_FogParams.y - dist) / (scene.u_FogParams.y - scene.u_FogParams.x), 0.0, 1.0);
    color = mix(scene.u_FogColor.rgb, color, fogFactor);
    
    FragColor = vec4(color, alpha);
}