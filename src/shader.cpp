#include "shader.h"
#include <iostream>
#include <fstream>
#include <sstream>

static char shaderLog[512];

Shader::Shader(GLuint type, const std::string& path, const DefinesList& defines)
  : id(0)
  , type(type)
{
  loadFromFile(type, path, defines);
}

Shader::~Shader()
{
  glDeleteShader(id);
}

void Shader::loadFromFile(GLuint type, const std::string& path, const DefinesList& defines)
{
  id = glCreateShader(type);

  std::ifstream t(path);
  std::stringstream buffer;

  buffer << "#version 460 core\n";

  for (const auto& [name, value] : defines)
    buffer << "#define " << name << " " << value << "\n";

  buffer << t.rdbuf();

  std::string sourceStr = buffer.str();
  const char* pSourceStr = sourceStr.c_str();
  //std::cout << "Compile from source: " << sourceStr << "\n";

  glShaderSource(id, 1, &pSourceStr, NULL);
  glCompileShader(id);

  // check for errors
  int success;
  glGetShaderiv(id, GL_COMPILE_STATUS, &success);
  if (!success)
  {
    glGetShaderInfoLog(id, sizeof(shaderLog), NULL, shaderLog);
    std::cout << "Shader::loadFromFile> Shader compile:\n" << shaderLog << "\n";
  }

  std::cout << "Compiled shader: " << path << "\n";
}

ShaderProgram::ShaderProgram()
  : prog(0)
{
}

ShaderProgram::ShaderProgram(const std::vector<Shader*>& link)
  : prog(0)
{
  create(link);
}

ShaderProgram::~ShaderProgram()
{
  std::cout << "Deleting program " << prog << "\n";
  glDeleteProgram(prog);
}


int ShaderProgram::getUniformLocation(const char* uniform) const
{
  return glGetUniformLocation(prog, uniform);
}

void ShaderProgram::bind(void) const
{
  glUseProgram(prog);
}

void ShaderProgram::create(const std::vector<Shader*>& link)
{
  prog = glCreateProgram();

  for (const Shader* shader : link)
    glAttachShader(prog, shader->id);

  glLinkProgram(prog);

  // check for errors
  int success;
  glGetProgramiv(prog, GL_LINK_STATUS, &success);
  if (!success) {
    glGetProgramInfoLog(prog, sizeof(shaderLog), NULL, shaderLog);
    std::cout << "ShaderProgram::create> Link error:\n" << shaderLog << "\n";
  }
  
  std::cout << "Linked program " << prog << ".\n";
}