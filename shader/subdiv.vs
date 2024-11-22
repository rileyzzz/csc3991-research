#version 460 core

layout (location = 0) in vec3 vPos;
layout (location = 1) in vec3 vNormal;
layout (location = 2) in vec2 vUV;

// out vec4 fragPos;
out vec3 fragNormal;
out vec2 texCoord;

uniform mat4 viewProj;

uniform sampler2D displacement;

void main()
{
  // TODO transform by model
	vec4 fragPos = vec4(vPos.x, vPos.y, vPos.z, 1.0);
	fragNormal = vNormal;
	texCoord = vUV;

	float heightStrength = 0.1;

	// Perturb the normal.
	float h = 0.001;
	float dx = (texture(displacement, vUV + vec2(h, 0)).r - texture(displacement, vUV - vec2(h, 0)).r) / (2*h) * heightStrength;
	float dy = (texture(displacement, vUV + vec2(0, h)).r - texture(displacement, vUV - vec2(0, h)).r) / (2*h) * heightStrength;

	vec3 upVector = vec3(0, 0, 1);

	// vec3 vTangent = normalize(cross(vNormal, upVector));
	// vec3 vBitangent = normalize(cross(vTangent, vNormal));

	vec3 vTangent = normalize(cross(upVector, vNormal));
  	vec3 vBitangent = normalize(cross(vNormal, vTangent));
  	mat3 tbn = mat3(vTangent, vBitangent, vNormal);

	// fragNormal = normalize(tbn * vec3(-dx, -dy, 1));
	// fragNormal = normalize(tbn * vec3(0, 0, 1));
  // fragNormal = vec3(dx, dy, 0);

	// fragPos.xyz += vNormal * texture(displacement, vUV).r * heightStrength;

	// gl_Position = viewProj * fragPos;
	gl_Position = fragPos;
}