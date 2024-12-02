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

  MeshPart() : VAO(0), VBO(0), EBO(0), numElements(0) { }
  MeshPart(const MeshPartData& data);
  ~MeshPart();

  inline MeshPart(MeshPart&& rhs) noexcept;
  inline MeshPart& operator=(MeshPart&& rhs) noexcept;

  // delete copy constructor
  MeshPart(const MeshPart&) = delete;
  MeshPart& operator=(const MeshPart&) = delete;

  void draw() const;
  void drawPatches() const;
};

class TargetGeometryStream
{
public:
  struct Triangle
  {
    glm::vec3 p0;
    int tileBase;

    glm::vec3 p1;
    int tileNum;

    glm::vec3 p2;
    float padding0;

    glm::vec3 normal;
    float padding1;

    //glm::vec3 tangent;
    //float padding2;

    glm::vec3 uvToBary0;
    float padding2;
    glm::vec3 uvToBary1;
    float padding3;
    glm::vec3 uvToBary2;
    float padding4;

    // glm::mat3 uvToBary;
  };

  GLuint TriangleStream;
  size_t numElements;

  TargetGeometryStream() : TriangleStream(0), numElements(0) { }
  TargetGeometryStream(const MeshPartData& data);
  ~TargetGeometryStream();
  
  void bind(int target) const;

  inline TargetGeometryStream(TargetGeometryStream&& rhs) noexcept;
  inline TargetGeometryStream& operator=(TargetGeometryStream&& rhs) noexcept;

  // delete copy constructor
  TargetGeometryStream(const TargetGeometryStream&) = delete;
  TargetGeometryStream& operator=(const TargetGeometryStream&) = delete;
};

class TileGeometryStreams
{
protected:
  struct Vertex
  {
    glm::vec3 position;
    float padding0;
    glm::vec3 normal;
    float padding1;
  };

  GLuint VertexStream;
  GLuint IndexStream;

public:
  TileGeometryStreams() : VertexStream(0), IndexStream(0) { }
  TileGeometryStreams(const MeshPartData& data);
  ~TileGeometryStreams();

  void bind(int vertex, int index) const;

  inline TileGeometryStreams(TileGeometryStreams&& rhs) noexcept;
  inline TileGeometryStreams& operator=(TileGeometryStreams&& rhs) noexcept;

  // delete copy constructor
  TileGeometryStreams(const TileGeometryStreams&) = delete;
  TileGeometryStreams& operator=(const TileGeometryStreams&) = delete;
};

class GPUMeshStreams
{
protected:
  GLuint VertexStream;
  GLuint IndexStream;
  GLuint VAO;

public:
  GPUMeshStreams() : VertexStream(0), IndexStream(0), VAO(0) { }
  GPUMeshStreams(size_t maxVerts, size_t maxIndices);
  ~GPUMeshStreams();

  void reset();
  void bind(int vertex, int index);
  GLuint getNumGeneratedElements();
  void draw(int numElements);

  // delete copy constructor
  GPUMeshStreams(const GPUMeshStreams&) = delete;
  GPUMeshStreams& operator=(const GPUMeshStreams&) = delete;
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
  void drawPatches() const;

  inline GLuint getTotalElements() const
  {
    GLuint elem = 0;
    for (const MeshPart& part : m_parts)
      elem += part.numElements;
    return elem;
  }
};

class TargetMesh
{
protected:
  TargetGeometryStream triStream;

public:
  TargetMesh(const std::string& file);
  ~TargetMesh();

  void loadFromFile(const std::string& file);

  inline void bindGeometryStream(int target) const { triStream.bind( target ); }
  inline size_t numTriangles() const { return triStream.numElements; }
};

class TileMesh
{
protected:
  unsigned int numVerts;
  unsigned int numIndices;
  TileGeometryStreams tileStreams;

public:
  TileMesh(const std::string& file);
  ~TileMesh();

  void loadFromFile(const std::string& file);

  inline unsigned int getNumVerts() const { return numVerts; }
  inline unsigned int getNumIndices() const { return numIndices; }
  inline void bindGeometryStreams(int vertex, int index) const { tileStreams.bind(vertex, index); }
};

// Inline functions.

MeshPart::MeshPart(MeshPart&& rhs) noexcept
  : VAO(rhs.VAO)
  , VBO(rhs.VBO)
  , EBO(rhs.EBO)
  , numElements(rhs.numElements)
{
  rhs.VAO = 0;
  rhs.VBO = 0;
  rhs.EBO = 0;
  rhs.numElements = 0;
}

MeshPart& MeshPart::operator=(MeshPart&& rhs) noexcept
{
  VAO = rhs.VAO;
  rhs.VAO = 0;

  VBO = rhs.VBO;
  rhs.VBO = 0;

  EBO = rhs.EBO;
  rhs.EBO = 0;

  numElements = rhs.numElements;
  rhs.numElements = 0;

  return *this;
}


TargetGeometryStream::TargetGeometryStream(TargetGeometryStream&& rhs) noexcept
  : TriangleStream(rhs.TriangleStream)
  , numElements(rhs.numElements)
{
  rhs.TriangleStream = 0;
  rhs.numElements = 0;
}

TargetGeometryStream& TargetGeometryStream::operator=(TargetGeometryStream&& rhs) noexcept
{
  TriangleStream = rhs.TriangleStream;
  rhs.TriangleStream = 0;

  numElements = rhs.numElements;
  rhs.numElements = 0;

  return *this;
}

TileGeometryStreams::TileGeometryStreams(TileGeometryStreams&& rhs) noexcept
  : VertexStream(rhs.VertexStream)
  , IndexStream(rhs.IndexStream)
{
  rhs.VertexStream = 0;
  rhs.IndexStream = 0;
}

TileGeometryStreams& TileGeometryStreams::operator=(TileGeometryStreams&& rhs) noexcept
{
  VertexStream = rhs.VertexStream;
  rhs.VertexStream = 0;

  IndexStream = rhs.IndexStream;
  rhs.IndexStream = 0;

  return *this;
}


#endif // _MESH_H