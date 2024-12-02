#ifndef _SHADER_H
#define _SHADER_H
#include <glad/glad.h>
#include <string>
#include <vector>

struct Shader
{
public:
  typedef std::vector<std::pair<std::string, std::string>> DefinesList;

  GLuint id;
  GLuint type;

  Shader(GLuint type, const std::string& path, const DefinesList& defines = DefinesList());
  ~Shader();
  
  // delete copy constructor
  Shader(const Shader&) = delete;
  Shader& operator=(const Shader&) = delete;

protected:
  void loadFromFile(GLuint type, const std::string& path, const DefinesList& defines);
};


struct ShaderProgram
{
protected:
  GLuint prog;

public:
  ShaderProgram();
  ShaderProgram(const std::vector<Shader*>& link);
  ~ShaderProgram();

  // delete copy constructor
  ShaderProgram(const ShaderProgram&) = delete;
  ShaderProgram& operator=(const ShaderProgram&) = delete;

  int getUniformLocation(const char* uniform) const;
  void bind(void) const;

protected:
  void create(const std::vector<Shader*>& link);
};

#endif // _SHADER_H