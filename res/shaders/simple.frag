#version 430 core

#define NUM_LIGHTS 1
#define IS_ENABLED(f) ((features & f) != 0)

struct Light {
    vec3 position;
    vec3 color;
};

in layout(location = 0) vec3 normal;
in layout(location = 1) vec2 textureCoordinates;
in layout(location = 2) vec3 fragPos;
in layout(location = 3) mat3 tbn;

uniform layout(location = 6) vec3 cameraPos;
uniform layout(location = 7) vec3 ballPos;
uniform layout(location = 8) uint features;
uniform Light lights[NUM_LIGHTS];

layout(binding = 0) uniform sampler2D diffuseSampler;
layout(binding = 1) uniform sampler2D normalSampler;

out vec4 color;

float rand(vec2 co) { return fract(sin(dot(co.xy, vec2(12.9898,78.233))) * 43758.5453); }
float dither(vec2 uv) { return (rand(uv)*2.0-1.0) / 256.0; }

vec3 reject(vec3 from, vec3 onto) {
    return from - onto * dot(from, onto) / dot(onto, onto);
}

// Shader feature flags
const uint PhongLighting = 1 << 0;
const uint Text = 1 << 1;
const uint DiffuseMap = 1 << 2;
const uint NormalMap = 1 << 3;

float ballRadius = 3.0;
float softShadowRadius = 4.0;

void main()
{
    vec4 objectColor = vec4(normal, 1.0);
    vec3 norm = normalize(normal);

    if (IS_ENABLED(DiffuseMap)) {
        objectColor = texture(diffuseSampler, textureCoordinates);
    } else if (IS_ENABLED(PhongLighting)) {
        objectColor = vec4(1.0);
    }

    if (IS_ENABLED(NormalMap)) {
        norm = tbn * (texture(normalSampler, textureCoordinates).xyz * 2 - 1);
    }
    
    if (IS_ENABLED(PhongLighting)) {
        vec3 ambient = vec3(0.0);
        vec3 diffuse = vec3(0.0);
        vec3 specular = vec3(0.0);
        for (int i = 0; i < NUM_LIGHTS; i++) {
            vec3 relativeLightPos = lights[i].position - fragPos;
            vec3 relativeBallPos = ballPos - fragPos;
            vec3 rejection = reject(relativeBallPos, relativeLightPos);
            float softShadowFactor = 1.0;
            if (length(relativeLightPos) > length(relativeBallPos) && dot(relativeLightPos, relativeBallPos) >= 0.0) {
                if (length(rejection) < ballRadius) {
                    continue;
                } else if (length(rejection) < softShadowRadius) {
                    softShadowFactor = (length(rejection) - ballRadius) / (softShadowRadius - ballRadius);
                }
            }

            float lightDistance = length(relativeLightPos);
            float attenuation = 1.0 / (0.01 + lightDistance * 0.04 + pow(lightDistance, 2) * 0.0001);

            vec3 lightDir = normalize(relativeLightPos);
            float diffuseIntensity = max(dot(norm, lightDir), 0.0);
            diffuse += diffuseIntensity * lights[i].color * attenuation * softShadowFactor;

            vec3 reflectDir = reflect(-lightDir, norm);
            vec3 viewDir = normalize(cameraPos - fragPos);
            float specularIntensity = pow(max(dot(viewDir, reflectDir), 0.0), 32);
            specular += specularIntensity * lights[i].color * attenuation * softShadowFactor;
        }

        color = vec4(ambient + diffuse + specular + dither(textureCoordinates), 1.0) * objectColor;
    } else if (IS_ENABLED(Text)) {
        color = texture(diffuseSampler, textureCoordinates);
    } else {
        color = objectColor;
    }
}