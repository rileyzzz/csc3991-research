
layout (location = 0) in vec3 vPos;

out vec4 fragPos;

uniform mat4 viewProj;
uniform mat4 model;

void main()
{
	fragPos = vec4(vPos.x, vPos.y, vPos.z, 1.0);

	gl_Position = viewProj * model * fragPos;
}