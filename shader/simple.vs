#version 460 core

layout (location = 0) in vec3 vPos;
layout (location = 1) in vec3 vNormal;
layout (location = 2) in vec2 vUV;

out vec4 fragPos;
out vec3 fragNormal;

uniform mat4 viewProj;

void main()
{
  // TODO transform by model
	fragPos = vec4(vPos.x, vPos.y, vPos.z, 1.0);
	fragNormal = vNormal;

	gl_Position = viewProj * fragPos;
}