#include "texture.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

Texture::Texture(const std::string& path)
  : id(0)
{
  loadFromFile(path);
}

Texture::~Texture()
{
  glDeleteTextures(1, &id);
}

void Texture::bind(void) const
{
  glBindTexture(GL_TEXTURE_2D, id);
}

void Texture::loadFromFile(const std::string& path)
{
  int w, h, nComponents;
  stbi_uc* data = stbi_load(path.c_str(), &w, &h, &nComponents, 4);

  glGenTextures(1, &id);
  glBindTexture(GL_TEXTURE_2D, id);

  glTexImage2D(GL_TEXTURE_2D, 0, nComponents == 3 ? GL_RGB : GL_RGBA, w, h, 0, nComponents == 3 ? GL_RGB : GL_RGBA, GL_UNSIGNED_BYTE, data);
  glGenerateMipmap(GL_TEXTURE_2D);

  glBindTexture(GL_TEXTURE_2D, 0);
  stbi_image_free(data);
}