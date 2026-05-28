#version 460 core

in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vUV;

layout(std140, binding = 0) uniform CameraBlock {
    mat4 uView;
    mat4 uProjection;
    vec4 uCameraPos;
};

layout(binding = 0) uniform sampler2D uBaseColorMap;
layout(binding = 1) uniform sampler2D uMetallicRoughnessMap;
layout(binding = 2) uniform sampler2D uNormalMap;

uniform int   uUseBaseColorMap = 0;
uniform int   uUseMrMap        = 0;
uniform int   uUseNormalMap    = 0;
uniform vec3  uAlbedo          = vec3(0.75, 0.55, 0.35);
uniform float uMetallicFactor  = 0.0;
uniform float uRoughnessFactor = 0.7;
uniform float uAlpha           = 1.0;
uniform float uNormalIntensity = 0.5;  // reduzido: normal map estava ruidoso demais
uniform int   uDebugMode       = 0;    // 0=lit, 1=albedo, 2=normal, 3=roughness, 4=metallic, 5=uv
uniform int   uApplyGamma      = 1;    // se 1: pow(albedo, 2.2) ao samplear (sRGB → linear)

// Key light (sol)
uniform vec3  uLightDir   = vec3(-0.4, -1.0, -0.3);
uniform vec3  uLightColor = vec3(1.30, 1.20, 1.08);
// Fill light (oposta, mais fria)
uniform vec3  uFillLightDir   = vec3(0.6, -0.4, 0.7);
uniform vec3  uFillLightColor = vec3(0.50, 0.55, 0.65);
// Rim light (frente do jogador, baixa intensidade) — destaca silhueta
uniform vec3  uRimLightDir   = vec3(0.0, -0.2, -1.0);
uniform vec3  uRimLightColor = vec3(0.30, 0.34, 0.40);
// Ambient hemisférico (mais forte agora)
uniform vec3  uAmbientSky    = vec3(0.55, 0.58, 0.68);
uniform vec3  uAmbientGround = vec3(0.20, 0.18, 0.14);
uniform float uAmbientWeight = 0.65;

out vec4 fragColor;

// TBN derivada (sem tangentes per-vertex) — Mikkelsen aproximado.
vec3 perturbNormal(vec3 N, vec3 worldPos, vec2 uv) {
    vec3 dp1  = dFdx(worldPos);
    vec3 dp2  = dFdy(worldPos);
    vec2 duv1 = dFdx(uv);
    vec2 duv2 = dFdy(uv);
    vec3 dp2perp = cross(dp2, N);
    vec3 dp1perp = cross(N, dp1);
    vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;
    float invMax = inversesqrt(max(dot(T, T), dot(B, B)));
    mat3 TBN = mat3(T * invMax, B * invMax, N);
    vec3 sN = texture(uNormalMap, uv).xyz * 2.0 - 1.0;
    sN.xy *= uNormalIntensity;
    // Reconstrói sN.z mantendo o vetor unitário — evita normal degenerado quando
    // intensity=0 (caso em que TBN * (0,0,~0) daria magnitude zero).
    sN.z = sqrt(max(1.0 - dot(sN.xy, sN.xy), 0.0));
    return normalize(TBN * sN);
}

vec3 evaluateLight(vec3 N, vec3 V, vec3 lightDir, vec3 lightColor,
                   vec3 albedo, float metallic, float roughness) {
    vec3 L = normalize(-lightDir);
    vec3 H = normalize(L + V);
    float ndl = max(dot(N, L), 0.0);
    if (ndl <= 0.0) return vec3(0.0);

    // Rede de proteção contra specular aliasing: roughness mínima clamped.
    // Mesmo se a MR texture for reativada no futuro, shininess nunca passa de ~55.
    float effectiveRoughness = max(roughness, 0.4);
    float shininess = mix(96.0, 6.0, clamp(effectiveRoughness, 0.05, 0.99));
    float spec = pow(max(dot(N, H), 0.0), shininess);

    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 F = F0 + (vec3(1.0) - F0) * pow(1.0 - max(dot(H, V), 0.0), 5.0);

    vec3 diffuse  = (1.0 - metallic) * albedo * ndl;
    vec3 specular = F * spec;
    return (diffuse + specular) * lightColor;
}

void main() {
    // Albedo: textura ou cor uniforme. Degamma é toggleable.
    vec3 albedo = uAlbedo;
    if (uUseBaseColorMap > 0) {
        vec4 sc = texture(uBaseColorMap, vUV);
        albedo = (uApplyGamma > 0) ? pow(sc.rgb, vec3(2.2)) : sc.rgb;
    }

    // Metallic/roughness do canal padrão glTF: G=roughness, B=metallic
    float metallic  = uMetallicFactor;
    float roughness = uRoughnessFactor;
    if (uUseMrMap > 0) {
        vec3 mr = texture(uMetallicRoughnessMap, vUV).rgb;
        roughness = clamp(mr.g * uRoughnessFactor, 0.04, 1.0);
        metallic  = clamp(mr.b * uMetallicFactor,  0.0,  1.0);
    }

    vec3 N = normalize(vNormal);
    if (uUseNormalMap > 0) N = perturbNormal(N, vWorldPos, vUV);
    vec3 V = normalize(uCameraPos.xyz - vWorldPos);

    // Ambient hemisférico (mais forte) + boost leve de Fresnel
    float hemi = 0.5 + 0.5 * N.y;
    vec3 ambient = mix(uAmbientGround, uAmbientSky, hemi) * uAmbientWeight;
    float ndv = max(dot(N, V), 0.0);
    ambient *= (1.0 + (1.0 - ndv) * 0.25);

    vec3 lit = ambient * albedo;
    lit += evaluateLight(N, V, uLightDir,     uLightColor,     albedo, metallic, roughness);
    lit += evaluateLight(N, V, uFillLightDir, uFillLightColor, albedo, metallic, roughness);
    lit += evaluateLight(N, V, uRimLightDir,  uRimLightColor,  albedo, metallic, roughness);

    vec3 color;
    if (uDebugMode == 1) {
        // Albedo bruto (sem lighting) — mostra a textura como está depois da degamma.
        color = albedo;
    } else if (uDebugMode == 2) {
        color = N * 0.5 + 0.5;  // normal visualizada como cor
    } else if (uDebugMode == 3) {
        color = vec3(roughness);
    } else if (uDebugMode == 4) {
        color = vec3(metallic);
    } else if (uDebugMode == 5) {
        color = vec3(fract(vUV), 0.0);  // UV em verde/vermelho
    } else {
        color = clamp(lit, 0.0, 1.0);
    }
    color = pow(color, vec3(1.0/2.2));
    fragColor = vec4(color, uAlpha);
}
