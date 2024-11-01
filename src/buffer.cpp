#include "buffer.h"
#include <iostream>

UniformBuffer::UniformBuffer(void* data, size_t len)
{
  glGenBuffers(1, &id);
  setData(data, len);
}

UniformBuffer::~UniformBuffer()
{
  std::cout << "DEBUG: Delete uniform buffer " << id << "\n";
  glDeleteBuffers(1, &id);
}

void UniformBuffer::setData(void* data, size_t len)
{
  glNamedBufferData(id, len, data, GL_STATIC_DRAW);
}

StorageBuffer::StorageBuffer(void* data, size_t len)
{
  glCreateBuffers(1, &id);
  setData(data, len);
}

StorageBuffer::~StorageBuffer()
{
  std::cout << "DEBUG: Delete storage buffer " << id << "\n";
  glDeleteBuffers(1, &id);
}

void StorageBuffer::bind(int target) const
{
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, target, id);
}

void StorageBuffer::setData(void* data, size_t len)
{
  glNamedBufferStorage(id, len, data, 0);
}
