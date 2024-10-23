#ifndef _BUFFER_H
#define _BUFFER_H
#include <glad/glad.h>

class UniformBuffer
{
protected:
  GLuint id;

public:
  UniformBuffer(void* data, size_t len);
  ~UniformBuffer();

  // Delete copy constructors
  UniformBuffer(const UniformBuffer&) = delete;
  UniformBuffer& operator=(const UniformBuffer&) = delete;

protected:
  void setData(void* data, size_t len);
};

class StorageBuffer
{
protected:
  GLuint id;

public:
  StorageBuffer(void* data, size_t len);
  ~StorageBuffer();

  // Delete copy constructors
  StorageBuffer(const StorageBuffer&) = delete;
  StorageBuffer& operator=(const StorageBuffer&) = delete;

protected:
  void setData(void* data, size_t len);
};
#endif // _BUFFER_H