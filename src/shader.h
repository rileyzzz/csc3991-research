#ifndef _SHADER_H
#define _SHADER_H
#include <glad/glad.h>
#include <string>
#include <vector>

struct Shader
{
public:
  GLuint id;
  GLuint type;

  Shader(GLuint type, const std::string& path);
  ~Shader();
  
  // delete move constructor
  Shader(const Shader&) = delete;
  Shader& operator=(const Shader&) = delete;

protected:
  void loadFromFile(GLuint type, const std::string& path);
};


struct ShaderProgram
{
protected:
  GLuint prog;

public:
  ShaderProgram();
  ShaderProgram(const std::vector<Shader*>& link);
  ~ShaderProgram();

  // delete move constructor
  ShaderProgram(const ShaderProgram&) = delete;
  ShaderProgram& operator=(const ShaderProgram&) = delete;

  void bind();

protected:
  void create(const std::vector<Shader*>& link);
};

#endif // _SHADER_H