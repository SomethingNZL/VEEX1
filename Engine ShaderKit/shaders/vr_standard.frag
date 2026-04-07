#version 330 core

in vec2 v_TexCoord;
in vec2 v_LmCoord;
in vec3 v_Normal;
in vec3 v_FragPos;
in mat3 v_TBN;

uniform sampler2D u_MainTexture;
uniform sampler2D u_LightmapTexture;
uniform sampler2D u_NormalTexture;
uniform bool u_HasNormal;

layout (std140) uniform SceneBlock {
    mat4 viewProj;
    vec4 cameraPos;
    vec4 sunDir;
    vec4 sunColor;
    vec4 fogParams;
    vec4 fogColor;
    vec4 ambientCube[6];
} scene;

out vec4 FragColor;

void main() {
    // 1. Albedo / Diffuse
    vec4 albedo = texture(u_MainTexture, v_TexCoord);
    if(albedo.a < 0.5) discard; // Simple alpha test

    // 2. Normal Mapping
    vec3 norm = normalize(v_Normal);
    if (u_HasNormal) {
        vec3 nMap = texture(u_NormalTexture, v_TexCoord).rgb;
        nMap = nMap * 2.0 - 1.0; 
        norm = normalize(v_TBN * nMap);
    }

    // 3. Lighting (Lightmap vs Directional)
    vec3 lighting = vec3(0.0);

    #ifdef ENABLE_LIGHTMAPS
        // Scale lightmap (Source lightmaps are often overbrightened by 4.0 or 2.0)
        vec3 lm = texture(u_LightmapTexture, v_LmCoord).rgb;
        lighting = lm * 2.0; 
    #else
        // Simple fallback NdotL if no lightmaps
        float diff = max(dot(norm, -scene.sunDir.xyz), 0.0);
        lighting = scene.ambientCube[2].rgb + (diff * scene.sunColor.rgb * scene.sunColor.w);
    #endif

    // 4. Fog
    vec3 finalColor = albedo.rgb * lighting;

    #ifdef ENABLE_FOG
        float dist = length(scene.cameraPos.xyz - v_FragPos);
        float fogFactor = clamp((scene.fogParams.y - dist) / (scene.fogParams.y - scene.fogParams.x), 0.0, 1.0);
        finalColor = mix(scene.fogColor.rgb, finalColor, fogFactor);
    #endif

    FragColor = vec4(finalColor, albedo.a);
}