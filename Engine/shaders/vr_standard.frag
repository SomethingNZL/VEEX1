#version 330 core

out vec4 FragColor;

in vec3 v_WorldPos;
in vec2 v_TexCoord;
in vec2 v_LMCoord;
in vec3 v_Normal;
in vec3 v_Tangent;
in vec3 v_Bitangent;

// Scene UBO
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

// Material Uniforms
uniform float u_Roughness;
uniform float u_Metallic;
uniform float u_AlphaRef;
uniform float u_EmissiveScale;
uniform float u_EnvmapTint;

uniform bool u_HasNormalMap;
uniform bool u_HasEmissiveMap;

// Samplers
uniform sampler2D u_MainTexture;     // Unit 0
uniform sampler2D u_LightmapTexture; // Unit 1
uniform sampler2D u_NormalTexture;   // Unit 4
uniform sampler2D u_EmissiveTexture; // Unit 5

void main() {
    // 1. Albedo & Alpha Test
    vec4 albedo = texture(u_MainTexture, v_TexCoord);
    
    #ifdef SHADER_FEATURE_ALPHA_TEST
        if (albedo.a < u_AlphaRef) discard;
    #endif

    // 2. Normal Setup
    vec3 N = normalize(v_Normal);
    #ifdef SHADER_FEATURE_NORMAL_MAP
        if (u_HasNormalMap) {
            // Reconstruct TBN matrix in fragment shader
            mat3 TBN = mat3(normalize(v_Tangent), normalize(v_Bitangent), N);
            vec3 tangentNormal = texture(u_NormalTexture, v_TexCoord).rgb * 2.0 - 1.0;
            N = normalize(TBN * tangentNormal);
        }
    #endif

    // 3. Lighting (Lightmap + Modulated Ambient)
    vec3 lightmap = texture(u_LightmapTexture, v_LMCoord).rgb * scene.u_LMExposure;
    
    // Calculate local lightmap brightness (Luminance)
    float luma = dot(lightmap, vec3(0.2126, 0.7152, 0.0722));
    
    // 6-Axis Ambient Cube Lookup (Source Engine Style)
    vec3 nSquared = N * N;
    vec3 ambient = 
        nSquared.x * (N.x > 0.0 ? scene.u_AmbientCube[0].rgb : scene.u_AmbientCube[1].rgb) +
        nSquared.y * (N.y > 0.0 ? scene.u_AmbientCube[2].rgb : scene.u_AmbientCube[3].rgb) +
        nSquared.z * (N.z > 0.0 ? scene.u_AmbientCube[4].rgb : scene.u_AmbientCube[5].rgb);
    
    // MODULATION: Use luma to mask ambient light so corners stay pitch black
    vec3 finalLighting = lightmap + (ambient * clamp(luma, 0.0, 1.0));
    
    vec3 finalColor = albedo.rgb * finalLighting;

    // 4. Emissive
    if (u_HasEmissiveMap) {
        finalColor += texture(u_EmissiveTexture, v_TexCoord).rgb * u_EmissiveScale;
    }

    // 5. Fog
    #ifdef SHADER_FEATURE_FOG
        float dist = length(scene.u_CameraPos.xyz - v_WorldPos);
        float fogFactor = clamp((scene.u_FogParams.y - dist) / (scene.u_FogParams.y - scene.u_FogParams.x), 0.0, 1.0);
        finalColor = mix(scene.u_FogColor.rgb, finalColor, fogFactor);
    #endif

    // 6. Gamma Correction (Converts Linear to sRGB)
    finalColor = pow(finalColor, vec3(1.0 / 2.2));

    FragColor = vec4(finalColor, albedo.a);
}