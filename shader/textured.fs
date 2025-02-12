
in vec4 fragPos;
in vec3 fragNormal;
in vec2 texCoord;

out vec4 FragColor;

uniform vec3 viewPos;
uniform sampler2D tex;

void main()
{
  vec3 lightColor = vec3(0.8, 0.8, 0.8);
  vec3 lightPos = vec3(3, 3, 3);
  vec3 objectColor = vec3(0.2, 0.2, 1.0);

  float ambientStrength = 0.2;
  float specularStrength = 0.8;
  float specPower = 32.0;

  vec3 vecToLight = normalize(lightPos - fragPos.xyz);
  vec3 vecToView = normalize(viewPos - fragPos.xyz);

  vec3 ambient = ambientStrength * lightColor;

  float diff = max(dot(fragNormal, vecToLight), 0.0);
  vec3 diffuse = diff * lightColor;

  vec3 reflectDir = reflect(-vecToLight, fragNormal); 
  float spec = pow(max(dot(vecToView, reflectDir), 0.0), specPower);
  vec3 specular = specularStrength * spec * lightColor;  

  // flip uvs.
  vec2 uv = texCoord;
  uv.y = 1 - uv.y;

  objectColor = texture(tex, uv).rgb;
  float alpha = texture(tex, uv).a;

  if (alpha < 0.3)
    discard;

  vec3 result = (ambient + diffuse + specular) * objectColor;

  FragColor = vec4(result, 1);
  // FragColor = vec4(fragNormal, 1);
}