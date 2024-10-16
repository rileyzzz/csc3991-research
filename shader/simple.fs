#version 460 core

in vec4 fragPos;
in vec3 fragNormal;

out vec4 FragColor;

uniform vec3 viewPos;

void main()
{
  vec3 lightColor = vec3(0.8, 0.8, 0.8);
  vec3 lightPos = vec3(3, 3, 3);
  vec3 objectColor = vec3(0.2, 0.2, 1.0);

  float ambientStrength = 0.2;
  float specularStrength = 0.8;

  vec3 vecToLight = normalize(lightPos - fragPos.xyz);
  vec3 vecToView = normalize(viewPos - fragPos.xyz);

  vec3 ambient = ambientStrength * lightColor;

  float diff = max(dot(fragNormal, vecToLight), 0.0);
  vec3 diffuse = diff * lightColor;

  vec3 reflectDir = reflect(-vecToLight, fragNormal); 
  float spec = pow(max(dot(vecToView, reflectDir), 0.0), 32);
  vec3 specular = specularStrength * spec * lightColor;  

  vec3 result = (ambient + diffuse + specular) * objectColor;

  FragColor = vec4(result, 1);
}