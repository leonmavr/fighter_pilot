#version 330

in vec3 fragNormal;
in vec3 fragWorldPos;
in vec4 fragColor;

uniform vec3 lightDir;
uniform vec3 lightColor;
uniform vec3 ambientColor;
uniform float renderUnitsToMeters;
uniform float waterSurfaceMeters;

out vec4 finalColor;

float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float smoothNoise(vec2 p) {
    vec2 cell = floor(p);
    vec2 local = fract(p);
    vec2 fade = local * local * (3.0 - 2.0 * local);

    float v00 = hash12(cell);
    float v10 = hash12(cell + vec2(1.0, 0.0));
    float v01 = hash12(cell + vec2(0.0, 1.0));
    float v11 = hash12(cell + vec2(1.0, 1.0));

    float vx0 = mix(v00, v10, fade.x);
    float vx1 = mix(v01, v11, fade.x);
    return mix(vx0, vx1, fade.y);
}

vec3 terrainPalette(float heightMeters, float slope, vec2 worldMeters) {
    vec3 beach = vec3(0.76, 0.73, 0.56);
    vec3 grass = vec3(0.35, 0.52, 0.25);
    vec3 forest = vec3(0.22, 0.36, 0.18);
    vec3 rock = vec3(0.43, 0.36, 0.29);
    vec3 highRock = vec3(0.62, 0.58, 0.52);
    vec3 snow = vec3(0.95, 0.96, 0.98);

    vec2 macroCoords = worldMeters * 0.0025;
    vec2 detailCoords = worldMeters * 0.008;
    float macroFilter = clamp(1.0 - max(fwidth(macroCoords.x), fwidth(macroCoords.y)) * 1.6, 0.0, 1.0);
    float detailFilter = clamp(1.0 - max(fwidth(detailCoords.x), fwidth(detailCoords.y)) * 2.2, 0.0, 1.0);
    float macroNoise = (smoothNoise(macroCoords) * 2.0 - 1.0) * macroFilter;
    float detailNoise = (smoothNoise(detailCoords) * 2.0 - 1.0) * detailFilter;
    float shapedHeight = heightMeters + macroNoise * 35.0 + detailNoise * 8.0;

    float shorelineAa = max(fwidth(shapedHeight) * 2.5, 3.0);
    float shoreBlend = smoothstep(
        waterSurfaceMeters + 5.0 - shorelineAa,
        waterSurfaceMeters + 30.0 + shorelineAa,
        shapedHeight);
    float forestBlend = smoothstep(110.0, 300.0, shapedHeight);
    float rockBlend = smoothstep(900.0, 1250.0, shapedHeight);
    float snowBlend = smoothstep(2050.0, 2350.0, shapedHeight);
    float slopeRock = smoothstep(0.22, 0.5, slope) * smoothstep(120.0, 850.0, shapedHeight);

    vec3 lowland = mix(beach, grass, shoreBlend);
    vec3 upland = mix(lowland, forest, forestBlend);
    vec3 mountainous = mix(upland, rock, max(rockBlend, slopeRock));
    vec3 alpine = mix(mountainous, highRock, smoothstep(1500.0, 2000.0, shapedHeight));
    return mix(alpine, snow, snowBlend);
}

void main() {
    vec3 normal = normalize(fragNormal);
    vec3 worldMeters = fragWorldPos * renderUnitsToMeters;
    float heightMeters = worldMeters.y;
    float slope = clamp(1.0 - normal.y, 0.0, 1.0);

    vec3 albedo = terrainPalette(heightMeters, slope, worldMeters.xz);
    float seamSurface = 1.0 - smoothstep(0.18, 0.5, normal.y);
    albedo = mix(albedo, fragColor.rgb, seamSurface);
    vec3 toLight = normalize(-lightDir);
    float diffuse = max(dot(normal, toLight), 0.0);
    float rim = pow(1.0 - max(normal.y, 0.0), 2.0) * 0.06;
    vec3 lighting = ambientColor + lightColor * diffuse + vec3(rim);

    finalColor = vec4(albedo * lighting, 1.0);
}