#version 330 core

in vec2 v_TexCoord;
in vec2 v_LMCoord;
in vec3 v_Normal;
in vec3 v_FragPos;
in mat3 v_TBN;
in vec3 v_TangentViewDir;

uniform sampler2D u_MainTexture;
uniform sampler2D u_LightmapTexture;
uniform sampler2D u_NormalTexture;
uniform samplerCube u_ReflectionCubemap;
uniform bool u_HasNormalMap;
uniform bool u_EnableRNM;  // Enable Radiosity Normal Mapping
uniform bool u_HasReflectionProbe;
uniform float u_BumpScale;  // Normal map depth/bump scale
uniform float u_Roughness;
uniform float u_Metallic;
uniform int u_ProbeIndex;

// Atlas support
uniform bool u_UseAtlas;
uniform vec4 u_AtlasCrop;  // x,y = offset, z,w = scale

// Height map atlas support
uniform bool u_UseHeightAtlas;
uniform vec4 u_HeightAtlasCrop; // x,y = offset, z,w = scale

// Material parameters for advanced lighting
uniform float u_DiffuseCoefficient;
uniform float u_LightmapBrightness;
uniform float u_EmissiveScale;
uniform bool u_HasEmissiveMap;
uniform sampler2D u_EmissiveTexture;

// ──────────────────────────────────────────────────────────────────────────────
// SILHOUETTE PARALLAX MAPPING
// ──────────────────────────────────────────────────────────────────────────────
uniform bool u_EnableSilhouetteParallax;
uniform sampler2D u_HeightTexture;
uniform float u_ParallaxScale;      // Depth/height scale (typically 0.02 - 0.1)
uniform float u_ParallaxMinLayers;  // Min ray-march layers (quality/perf tradeoff)
uniform float u_ParallaxMaxLayers;  // Max ray-march layers

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

out vec4 FragColor;

// ──────────────────────────────────────────────────────────────────────────────
// PARALLAX OCCLUSION MAPPING WITH SILHOUETTE SUPPORT
// Performs ray-marching through a height field to displace texture coordinates.
// Fragments that fall outside the displaced silhouette are discarded.
// ──────────────────────────────────────────────────────────────────────────────
// Helper: transform local [0,1] height coords to atlas coords
vec2 HeightAtlasCoord(vec2 localCoord) {
    if (u_UseHeightAtlas) {
        return u_HeightAtlasCrop.xy + localCoord * u_HeightAtlasCrop.zw;
    }
    return localCoord;
}

vec2 ParallaxOcclusionMapping(vec2 texCoords, vec3 viewDirTangent, out float parallaxHeight) {
    // Determine number of layers based on view angle (more layers at grazing angles)
    float numLayers = mix(u_ParallaxMaxLayers, u_ParallaxMinLayers, abs(viewDirTangent.z));
    
    // Calculate step size
    float layerDepth = 1.0 / numLayers;
    
    // Calculate texture coordinate offset per layer (in tangent space XY plane)
    vec2 P = viewDirTangent.xy * u_ParallaxScale;
    vec2 deltaTexCoords = P / numLayers;
    
    // Initial values
    vec2 currentTexCoords = texCoords;
    float currentDepthMapValue = texture(u_HeightTexture, HeightAtlasCoord(currentTexCoords)).r;
    float currentLayerDepth = 0.0;
    
    // Ray-march until we find an intersection or fall off the height field
    // This loop creates the "silhouette" effect when we exit the height field
    while (currentLayerDepth < currentDepthMapValue) {
        currentTexCoords -= deltaTexCoords;
        
        // If we've marched outside [0,1], the ray has exited the silhouette
        // Return the coords so the caller can decide to discard
        if (currentTexCoords.x < 0.0 || currentTexCoords.x > 1.0 ||
            currentTexCoords.y < 0.0 || currentTexCoords.y > 1.0) {
            parallaxHeight = -1.0; // Signal: outside silhouette
            return currentTexCoords;
        }
        
        currentDepthMapValue = texture(u_HeightTexture, HeightAtlasCoord(currentTexCoords)).r;
        currentLayerDepth += layerDepth;
    }
    
    // Get the depth values before and after intersection for interpolation
    vec2 prevTexCoords = currentTexCoords + deltaTexCoords;
    float afterDepth  = currentDepthMapValue - currentLayerDepth;
    float beforeDepth = texture(u_HeightTexture, HeightAtlasCoord(prevTexCoords)).r - currentLayerDepth + layerDepth;
    
    // Interpolate between the two depth values for smoother result
    float weight = afterDepth / (afterDepth - beforeDepth);
    vec2 finalTexCoords = prevTexCoords * weight + currentTexCoords * (1.0 - weight);
    
    parallaxHeight = currentLayerDepth;
    return finalTexCoords;
}

// ──────────────────────────────────────────────────────────────────────────────
// AMBIENT CUBE LIGHTING (from old working shader)
// Uses squared normal components to weight the 6 ambient cube directions
// This ensures surfaces only receive ambient light from directions they're facing
// ──────────────────────────────────────────────────────────────────────────────
vec3 AmbientCubeLight(vec3 normal) {
    vec3 nSquared = normal * normal;
    vec3 isNegative = step(normal, vec3(0.0));
    
    vec3 linearColor = nSquared.x * scene.u_AmbientCube[int(isNegative.x)].rgb +
                       nSquared.y * scene.u_AmbientCube[int(isNegative.y) + 2].rgb +
                       nSquared.z * scene.u_AmbientCube[int(isNegative.z) + 4].rgb;
    return linearColor;
}

// Oren-Nayar diffuse model
float OrenNayarDiffuse(vec3 normal, vec3 lightDir, vec3 viewDir, float roughness) {
    float LdotN = dot(lightDir, normal);
    float VdotN = dot(viewDir, normal);
    
    float NdotL = max(LdotN, 0.0);
    float NdotV = max(VdotN, 0.0);
    
    if (NdotL <= 0.0) return 0.0;
    
    float theta_i = acos(clamp(LdotN, -1.0, 1.0));
    float theta_r = acos(clamp(VdotN, -1.0, 1.0));
    
    float alpha = max(theta_i, theta_r);
    float beta = min(theta_i, theta_r);
    
    vec3 v_proj = normalize(viewDir - normal * VdotN);
    vec3 l_proj = normalize(lightDir - normal * LdotN);
    float cos_phi_diff = clamp(dot(v_proj, l_proj), 0.0, 1.0);
    
    float sigma2 = roughness * roughness;
    float A = 1.0 - 0.5 * (sigma2 / (sigma2 + 0.33));
    float B = 0.45 * (sigma2 / (sigma2 + 0.09));
    
    return NdotL * (A + B * cos_phi_diff * sin(alpha) * tan(beta));
}

// Fresnel-Schlick approximation
vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Enhanced Blinn-Phong with edge enhancement (rim lighting)
vec3 BlinnPhongSpecular(vec3 normal, vec3 lightDir, vec3 viewDir, float roughness, vec3 albedo) {
    vec3 halfDir = normalize(lightDir + viewDir);
    float NdotH = max(dot(normal, halfDir), 0.0);
    float NdotV = max(dot(normal, viewDir), 0.0);
    
    float shininess = 2.0 / (roughness * roughness + 0.001);
    float specular = pow(NdotH, shininess);
    
    float rim = pow(1.0 - NdotV, 3.0) * roughness;
    
    return specular * scene.u_SunColor.rgb + rim * albedo * 0.2;
}

void main() {
    // ──────────────────────────────────────────────────────────────────────────
    // TEXTURE COORDINATE SETUP (base coords before parallax)
    // ──────────────────────────────────────────────────────────────────────────
    vec2 texCoord = v_TexCoord;
    if (u_UseAtlas) {
        texCoord = u_AtlasCrop.xy + v_TexCoord * u_AtlasCrop.zw;
    }
    
    // ──────────────────────────────────────────────────────────────────────────
    // SILHOUETTE PARALLAX MAPPING
    // Displaces texture coordinates based on height map; discards fragments
    // that would fall outside the displaced silhouette boundary.
    // ──────────────────────────────────────────────────────────────────────────
    float parallaxHeight = 0.0;
    if (u_EnableSilhouetteParallax) {
        // Only apply if height map is available (use normal map as fallback)
        // Compute in local [0,1] texture space
        vec2 localTexCoord = v_TexCoord;
        vec3 viewDirTangent = normalize(v_TangentViewDir);
        
        // Flip Z for OpenGL convention (height points up/out from surface)
        viewDirTangent.z = -viewDirTangent.z;
        
        vec2 displacedCoord = ParallaxOcclusionMapping(localTexCoord, viewDirTangent, parallaxHeight);
        
        // If parallaxHeight < 0, the ray exited the height field silhouette
        if (parallaxHeight < 0.0) {
            discard;
        }
        
        // Apply atlas transform if needed
        if (u_UseAtlas) {
            texCoord = u_AtlasCrop.xy + displacedCoord * u_AtlasCrop.zw;
        } else {
            texCoord = displacedCoord;
        }
    }
    
    vec4 albedo = texture(u_MainTexture, texCoord);
    if (albedo.a < 0.5) discard;

    // ──────────────────────────────────────────────────────────────────────────
    // TANGENT-SPACE NORMAL MAPPING
    // ──────────────────────────────────────────────────────────────────────────
    // The TBN matrix transforms from tangent space to world space
    // u_BumpScale controls the intensity of the normal map effect
    vec3 norm = normalize(v_Normal);
    if (u_HasNormalMap) {
        vec3 nMap = texture(u_NormalTexture, texCoord).rgb;
        
        // Handle missing/invalid normal map gracefully
        if (length(nMap) < 0.1) {
            nMap = vec3(0.5, 0.5, 1.0);
        }
        
        // Convert from [0,1] to [-1,1] range
        nMap = nMap * 2.0 - 1.0;
        
        // Apply bump scale to XY components (controls normal map depth/intensity)
        // This makes the normal map more or less pronounced
        nMap.xy *= u_BumpScale;
        
        // Re-normalize the normal map vector to ensure proper lighting
        nMap = normalize(nMap);
        
        // Transform from tangent space to world space using TBN matrix
        // v_TBN is a 3x3 matrix with Tangent, Bitangent, Normal as columns
        norm = normalize(v_TBN * nMap);
    }

    vec3 viewDir = normalize(scene.u_CameraPos.xyz - v_FragPos);
    vec3 lightDir = normalize(-scene.u_SunDirection.xyz);
    
    // Calculate Fresnel for reflections
    vec3 F0 = mix(vec3(0.04), albedo.rgb, u_Metallic);
    float NdotV = max(dot(norm, viewDir), 0.0);
    vec3 fresnel = FresnelSchlick(NdotV, F0);

    // ──────────────────────────────────────────────────────────────────────────
    // LIGHTING MODEL - Use lightmap for indoor, sun for outdoor
    // ──────────────────────────────────────────────────────────────────────────
    vec3 diffuseLighting = vec3(0.0);
    vec3 specularHighlight = vec3(0.0);
    
    #ifdef ENABLE_LIGHTMAPS
        // INDOOR: Use lightmap as primary light source (contains baked shadows)
        vec3 lm = vec3(0.0);
        if (v_LMCoord.x >= 0.0 && v_LMCoord.y >= 0.0 &&
            v_LMCoord.x <= 1.001 && v_LMCoord.y <= 1.001) {
            lm = texture(u_LightmapTexture, clamp(v_LMCoord, 0.0, 1.0)).rgb * scene.u_LMExposure;
        }
        
        if (u_EnableRNM) {
            // RNM: Combine lightmap with directional ambient cube for normal map interaction
            vec3 ambientFromCube = AmbientCubeLight(norm);
            diffuseLighting = lm * 2.0 + ambientFromCube * u_BumpScale * 8.0;
            
            // Indoor specular from ambient (subtle)
            vec3 ambientDir = normalize(scene.u_AmbientCube[2].rgb);
            vec3 halfwayDir = normalize(ambientDir + viewDir);
            float spec = pow(max(dot(norm, halfwayDir), 0.0), 128.0);
            specularHighlight = ambientFromCube * spec * 0.5;
        } else {
            // Traditional: Just use lightmap
            diffuseLighting = lm * 3.5;
            
            // Subtle indoor specular
            vec3 ambientDir = normalize(scene.u_AmbientCube[2].rgb);
            vec3 halfwayDir = normalize(ambientDir + viewDir);
            float spec = pow(max(dot(norm, halfwayDir), 0.0), 128.0);
            vec3 ambientSpec = AmbientCubeLight(norm);
            specularHighlight = ambientSpec * spec * 0.5;
        }
    #else
        // OUTDOOR: Use sun for direct lighting
        float diffuse = OrenNayarDiffuse(norm, lightDir, viewDir, max(u_Roughness, 0.01));
        diffuseLighting = scene.u_SunColor.rgb * diffuse;
        
        // Directional ambient (not averaged from all directions)
        vec3 ambient = AmbientCubeLight(norm) * 0.3;
        diffuseLighting += ambient;
        
        // Outdoor specular from sun
        specularHighlight = BlinnPhongSpecular(norm, lightDir, viewDir, max(u_Roughness, 0.01), albedo.rgb);
    #endif

    // Lightmap contribution for indoor maps (additive detail on top of base lighting)
    // Only apply when we have valid lightmap coordinates
    vec3 lightmapDetail = vec3(0.0);
    #ifdef ENABLE_LIGHTMAPS
        if (v_LMCoord.x >= 0.0 && v_LMCoord.y >= 0.0 &&
            v_LMCoord.x <= 1.001 && v_LMCoord.y <= 1.001) {
            // Lightmap is already included in diffuseLighting above
            // This section is for any additional detail if needed
        }
    #endif

    // Environment reflections
    vec3 reflection = vec3(0.0);
    if (u_HasReflectionProbe && u_ProbeIndex >= 0 && u_ProbeIndex < 16) {
        vec3 reflectDir = reflect(-viewDir, norm);
        reflection = texture(u_ReflectionCubemap, reflectDir).rgb;
        reflection *= fresnel * 0.5; // Reduce reflection intensity to not overwhelm
    }

    // Emissive contribution
    vec3 emissive = vec3(0.0);
    if (u_HasEmissiveMap) {
        emissive = texture(u_EmissiveTexture, texCoord).rgb * u_EmissiveScale;
    }

    // Combine all lighting components
    vec3 finalColor = albedo.rgb * diffuseLighting * u_DiffuseCoefficient;
    finalColor += specularHighlight;
    finalColor += reflection;
    finalColor += emissive;

    // Apply fog if enabled
    #ifdef ENABLE_FOG
        float dist = length(scene.u_CameraPos.xyz - v_FragPos);
        float fogFactor = clamp((scene.u_FogParams.y - dist) / (scene.u_FogParams.y - scene.u_FogParams.x), 0.0, 1.0);
        finalColor = mix(scene.u_FogColor.rgb, finalColor, fogFactor);
    #endif

    // Gamma correction
    finalColor = pow(finalColor, vec3(1.0/2.2));

    FragColor = vec4(finalColor, albedo.a);
}
