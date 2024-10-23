#ifndef _MESH_H
#define _MESH_H
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <tuple>

struct MeshVertex
{
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec2 uv;
};

struct MeshIndexKey
{
  int posIdx;
  int normalIdx;
  int uvIdx;

  bool operator==(const MeshIndexKey&) const = default;
};

template <>
struct std::hash<MeshIndexKey>
{
  std::size_t operator()(const MeshIndexKey& k) const noexcept
  {
    return hash<int>()(k.posIdx)
      ^ (hash<int>()(k.normalIdx) << 1)
      ^ (hash<int>()(k.uvIdx) << 2);
  }
};

struct MeshPartData
{
  std::vector<MeshVertex> vtx;
  std::vector<unsigned int> idx;

  std::unordered_map<MeshIndexKey, int> idxToVtxCache;
};

class MeshPart
{
public:
  GLuint VAO;
  GLuint VBO;
  GLuint EBO;
  GLuint numElements;

  MeshPart(const MeshPartData& data);
  ~MeshPart();

  MeshPart(MeshPart&& rhs) noexcept;
  MeshPart& operator=(MeshPart&& rhs) noexcept;

  // delete copy constructor
  MeshPart(const MeshPart&) = delete;
  MeshPart& operator=(const MeshPart&) = delete;

  void draw() const;
};

class BufferMesh
{
public:
  BufferMesh();
  ~BufferMesh();

  void loadFromFile(const std::string& file);

};

class Mesh
{
protected:
  std::vector<MeshPart> m_parts;

public:
  Mesh(const std::string& file);
  ~Mesh();


  void loadFromFile(const std::string& file);

  void draw() const;
};

#endif // _MESH_H