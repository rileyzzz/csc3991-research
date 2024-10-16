#ifndef _TEXTURE_H
#define _TEXTURE_H
#include <glad/glad.h>
#include <string>

class Texture
{
protected:
  GLuint id;

public:
  Texture(const std::string& path);
  ~Texture();

  // Delete copy constructors
  Texture(const Texture&) = delete;
  Texture& operator=(const Texture&) = delete;

  void bind(void) const;

protected:
  void loadFromFile(const std::string& path);
};
#endif // _TEXTURE_H