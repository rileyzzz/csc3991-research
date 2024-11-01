#include "texture.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <iostream>

Texture::Texture(const std::string& path)
  : id(0)
{
  loadFromFile(path);
}

Texture::~Texture()
{
  std::cout << "DEBUG: Delete texture " << id << "\n";
  glDeleteTextures(1, &id);
}

void Texture::bind(void) const
{
  glBindTexture(GL_TEXTURE_2D, id);
}

void Texture::loadFromFile(const std::string& path)
{
  int w, h, nComponents;
  stbi_uc* data = stbi_load(path.c_str(), &w, &h, &nComponents, 0);
  if (!data)
  {
    std::cerr << "Failed to load image: " << path << "\n";
    return;
  }

  glGenTextures(1, &id);
  glBindTexture(GL_TEXTURE_2D, id);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  // nComponents == 3 ? GL_RGB : 
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, nComponents == 3 ? GL_RGB : GL_RGBA, GL_UNSIGNED_BYTE, data);
  glGenerateMipmap(GL_TEXTURE_2D);

  glBindTexture(GL_TEXTURE_2D, 0);
  stbi_image_free(data);
}