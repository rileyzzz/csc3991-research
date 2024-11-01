#ifndef _TEXTURE_H
#define _TEXTURE_H
#include <glad/glad.h>
#include <string>

class Texture
{
protected:
  GLuint id;

public:
  inline Texture() : id(0) { }
  Texture(const std::string& path);
  ~Texture();

  inline Texture(Texture&& rhs) noexcept;
  inline Texture& operator=(Texture&& rhs) noexcept;

  // Delete copy constructors
  Texture(const Texture&) = delete;
  Texture& operator=(const Texture&) = delete;

  void bind(void) const;

protected:
  void loadFromFile(const std::string& path);
};


Texture::Texture(Texture&& rhs) noexcept
  : id(rhs.id)
{
  rhs.id = 0;
}

Texture& Texture::operator=(Texture&& rhs) noexcept
{
  id = rhs.id;
  rhs.id = 0;
}

#endif // _TEXTURE_H