#version 460 core

in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vUV;

layout(std140, binding = 0) uniform CameraBlock {
    mat4 uView;
    mat4 uProjection;
    vec4 uCameraPos;
};

uniform vec3 uAlbedo = vec3(0.75, 0.55, 0.35);
uniform vec3 uLightDir = vec3(-0.4, -1.0, -0.3);
uniform vec3 uLightColor = vec3(1.0, 0.97, 0.9);
uniform float uAmbient = 0.18;
uniform float uShininess = 48.0;

out vec4 fragColor;

void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(-uLightDir);
    vec3 V = normalize(uCameraPos.xyz - vWorldPos);
    vec3 H = normalize(L + V);

    float diff = max(dot(N, L), 0.0);
    float spec = pow(max(dot(N, H), 0.0), uShininess) * step(0.0001, diff);

    vec3 color = uAlbedo * (uAmbient + diff * uLightColor) + spec * uLightColor * 0.4;
    fragColor = vec4(color, 1.0);
}
