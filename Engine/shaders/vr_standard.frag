#version 330 core

in vec2 v_TexCoord;
in vec2 v_LMCoord;
in vec3 v_Normal;
in vec3 v_FragPos;
in mat3 v_TBN;

uniform sampler2D u_MainTexture;
uniform sampler2D u_LightmapTexture;
uniform sampler2D u_NormalTexture;
uniform bool u_HasNormalMap;

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

void main() {
    vec4 albedo = texture(u_MainTexture, v_TexCoord);
    if (albedo.a < 0.5) discard;

    vec3 norm = normalize(v_Normal);
    if (u_HasNormalMap) {
        vec3 nMap = texture(u_NormalTexture, v_TexCoord).rgb;
        nMap = nMap * 2.0 - 1.0;
        norm = normalize(v_TBN * nMap);
    }

    vec3 lightDir = normalize(-scene.u_SunDirection.xyz);
    vec3 viewDir  = normalize(scene.u_CameraPos.xyz - v_FragPos);
    vec3 halfwayDir = normalize(lightDir + viewDir);

    float spec = pow(max(dot(norm, halfwayDir), 0.0), 128.0);
    vec3 specularHighlight = scene.u_SunColor.rgb * spec * 4.0;

    vec3 diffuseLighting = vec3(0.0);
    #ifdef ENABLE_LIGHTMAPS
        vec3 lm = vec3(0.0);
        if (v_LMCoord.x >= 0.0 && v_LMCoord.y >= 0.0 &&
            v_LMCoord.x <= 1.001 && v_LMCoord.y <= 1.001) {
            lm = texture(u_LightmapTexture, clamp(v_LMCoord, 0.0, 1.0)).rgb * scene.u_LMExposure;
        }
        diffuseLighting = lm * 2.0;
    #else
        float nDotL = max(dot(norm, lightDir), 0.0);
        vec3 ambient = scene.u_AmbientCube[2].rgb * 0.3;
        diffuseLighting = ambient + (nDotL * scene.u_SunColor.rgb);
    #endif

    vec3 finalColor = (albedo.rgb * diffuseLighting) + specularHighlight;

    #ifdef ENABLE_FOG
        float dist = length(scene.u_CameraPos.xyz - v_FragPos);
        float fogFactor = clamp((scene.u_FogParams.y - dist) / (scene.u_FogParams.y - scene.u_FogParams.x), 0.0, 1.0);
        finalColor = mix(scene.u_FogColor.rgb, finalColor, fogFactor);
    #endif

    FragColor = vec4(finalColor, albedo.a);
}
