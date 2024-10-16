#version 460 core

layout (location = 0) in vec3 vPos;
layout (location = 1) in vec3 vNormal;
layout (location = 2) in vec2 vUV;

uniform mat4 viewProj;

void main()
{
	gl_Position = viewProj * vec4(vPos.x, vPos.y, vPos.z, 1.0);
}