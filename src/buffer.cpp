#include "buffer.h"

UniformBuffer::UniformBuffer(void* data, size_t len)
{
  glGenBuffers(1, &id);
  setData(data, len);
}

UniformBuffer::~UniformBuffer()
{
  glDeleteBuffers(1, &id);
}

void UniformBuffer::setData(void* data, size_t len)
{
  glBindBuffer(GL_UNIFORM_BUFFER, id);
  glBufferData(GL_UNIFORM_BUFFER, len, data, GL_STATIC_DRAW);
  glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

StorageBuffer::StorageBuffer(void* data, size_t len)
{
  glCreateBuffers(1, &id);
  setData(data, len);
}

StorageBuffer::~StorageBuffer()
{
  glDeleteBuffers(1, &id);
}

void StorageBuffer::setData(void* data, size_t len)
{
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, id);
  glBufferStorage(GL_SHADER_STORAGE_BUFFER, len, data, 0);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}
