#version 330

in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;
in vec3 fragWorldPos;

uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec3 lightDir;
uniform vec3 lightColor;
uniform vec3 ambientColor;

out vec4 finalColor;

void main() {
    vec3 normal = normalize(fragNormal);
    vec3 toLight = normalize(-lightDir);
    float diffuse = max(dot(normal, toLight), 0.0);
    float rim = pow(1.0 - max(normal.z, 0.0), 2.0) * 0.08;

    vec4 texel = texture(texture0, fragTexCoord);
    vec3 albedo = texel.rgb * colDiffuse.rgb * fragColor.rgb;
    vec3 lighting = ambientColor + lightColor * diffuse + vec3(rim);

    finalColor = vec4(albedo * lighting, texel.a * colDiffuse.a * fragColor.a);
}