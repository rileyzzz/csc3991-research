#include "mesh.h"
#include <glad/glad.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <iostream>


MeshPart::MeshPart(const MeshPartData& data)
{
  if (data.vtx.empty() || data.idx.empty())
  {
    VAO = VBO = EBO = numElements = 0;
    return;
  }

  glGenVertexArrays(1, &VAO);
  glBindVertexArray(VAO);

  // init VBO
  glGenBuffers(1, &VBO);
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER, data.vtx.size() * sizeof(MeshVertex), (void*)data.vtx.data(), GL_STATIC_DRAW);

  // init EBO
  glGenBuffers(1, &EBO);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, data.idx.size() * sizeof(unsigned int), (void*)data.idx.data(), GL_STATIC_DRAW);
  numElements = data.idx.size();

  size_t size;
  size_t offset = 0;

  // Setup VAO:
  // position
  size = 3 * sizeof(float);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void*)offset);
  offset += size;

  // normal
  size = 3 * sizeof(float);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void*)offset);
  offset += size;

  // uv
  size = 2 * sizeof(float);
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void*)offset);
  offset += size;

  glBindVertexArray(0);
}

MeshPart::~MeshPart()
{
  if (VAO)
  {
    glDeleteVertexArrays(1, &VAO);
    VAO = 0;
  }

  if (VBO)
  {
    glDeleteBuffers(1, &VBO);
    VBO = 0;
  }

  if (EBO)
  {
    glDeleteBuffers(1, &EBO);
    EBO = 0;
  }
}

void MeshPart::draw() const
{
  glBindVertexArray(VAO);
  glDrawElements(GL_TRIANGLES, numElements, GL_UNSIGNED_INT, 0);
}


static void loadParts(tinyobj::ObjReader& reader, std::vector<MeshPartData>& partData, bool ignoreMaterials = false)
{
  auto& attrib = reader.GetAttrib();
  auto& shapes = reader.GetShapes();
  auto& materials = reader.GetMaterials();

  // Add an extra default material.
  if (!ignoreMaterials)
  {
    partData.resize(1 + materials.size(), MeshPartData());
  }
  else
  {
    partData.resize(1, MeshPartData());
  }

  // Loop over shapes
  for (size_t iShape = 0; iShape < shapes.size(); iShape++)
  {
    const tinyobj::shape_t& shape = shapes[iShape];
    const tinyobj::mesh_t& mesh = shape.mesh;

    // Loop over faces(polygon)
    size_t index_offset = 0;
    for (size_t iFace = 0; iFace < mesh.num_face_vertices.size(); ++iFace)
    {
      int materialId = mesh.material_ids[iFace];
      if (ignoreMaterials)
        materialId = -1;
      
      MeshPartData& part = partData[materialId + 1];

      size_t fv = size_t(mesh.num_face_vertices[iFace]);
      if (fv != 3)
      {
        std::cerr << "Non-triangle meshes not supported!\n";
        index_offset += fv;
        continue;
      }


      for (size_t v = 0; v < fv; v++) {
        // access to vertex
        tinyobj::index_t idx = mesh.indices[index_offset + v];
        MeshIndexKey triple{ idx.vertex_index, idx.normal_index, idx.texcoord_index };
        auto found = part.idxToVtxCache.find(triple);
        unsigned int iVertex;

        if (found != part.idxToVtxCache.end())
        {
          iVertex = found->second;
        }
        else
        {
          // Cache this vertex.
          MeshVertex vertex;

          tinyobj::real_t vx = attrib.vertices[3 * size_t(idx.vertex_index) + 0];
          tinyobj::real_t vy = attrib.vertices[3 * size_t(idx.vertex_index) + 1];
          tinyobj::real_t vz = attrib.vertices[3 * size_t(idx.vertex_index) + 2];
          vertex.position = glm::vec3(vx, vy, vz);

          // Check if `normal_index` is zero or positive. negative = no normal data
          if (idx.normal_index >= 0) {
            tinyobj::real_t nx = attrib.normals[3 * size_t(idx.normal_index) + 0];
            tinyobj::real_t ny = attrib.normals[3 * size_t(idx.normal_index) + 1];
            tinyobj::real_t nz = attrib.normals[3 * size_t(idx.normal_index) + 2];
            vertex.normal = glm::vec3(nx, ny, nz);
          }

          // Check if `texcoord_index` is zero or positive. negative = no texcoord data
          if (idx.texcoord_index >= 0) {
            tinyobj::real_t tx = attrib.texcoords[2 * size_t(idx.texcoord_index) + 0];
            tinyobj::real_t ty = attrib.texcoords[2 * size_t(idx.texcoord_index) + 1];
            vertex.uv = glm::vec2(tx, ty);
          }

          // Optional: vertex colors
          // tinyobj::real_t red   = attrib.colors[3*size_t(idx.vertex_index)+0];
          // tinyobj::real_t green = attrib.colors[3*size_t(idx.vertex_index)+1];
          // tinyobj::real_t blue  = attrib.colors[3*size_t(idx.vertex_index)+2];

          // Only support uint indices.
          if (part.vtx.size() >= std::numeric_limits<unsigned int>::max())
            std::cerr << "Index out of bounds!\n";

          iVertex = part.vtx.size();
          part.vtx.push_back(vertex);
          part.idxToVtxCache[triple] = iVertex;
        }

        // Add vertex to index list.
        part.idx.push_back(iVertex);
      }
      index_offset += fv;
    }
  }
}

TargetGeometryStream::TargetGeometryStream(const MeshPartData& data)
{
  glCreateBuffers(1, &TriangleStream);

  size_t numTriangles = data.idx.size() / 3;
  numElements = numTriangles;

  int tileBase = 0;
  std::vector<Triangle> stream;
  stream.resize(numTriangles);
  for (size_t i = 0; i < numTriangles; i++)
  {
    const MeshVertex& v0 = data.vtx[data.idx[i * 3 + 0]];
    const MeshVertex& v1 = data.vtx[data.idx[i * 3 + 1]];
    const MeshVertex& v2 = data.vtx[data.idx[i * 3 + 2]];
    stream[i].p0 = v0.position;
    stream[i].p1 = v1.position;
    stream[i].p2 = v2.position;

    glm::vec3 normal = glm::normalize(glm::cross(v2.position - v0.position, v1.position - v0.position));
    stream[i].normal = normal;
    // TODO: This is wrong.
    stream[i].tangent = glm::normalize(v1.position - v0.position);

    // TODO: factor in area in compute shader.
    int numTiles = 3;

    stream[i].tileBase = tileBase;
    stream[i].tileNum = numTiles;
    tileBase += numTiles;
  }

  glNamedBufferStorage(TriangleStream, sizeof(Triangle) * numTriangles, stream.data(), 0);
}

TargetGeometryStream::~TargetGeometryStream()
{
  glDeleteBuffers(1, &TriangleStream);
}

void TargetGeometryStream::bind(int target) const
{
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, target, TriangleStream);
}

TileGeometryStreams::TileGeometryStreams(const MeshPartData& data)
{
  glCreateBuffers(1, &VertexStream);
  glCreateBuffers(1, &IndexStream);

  std::vector<Vertex> vertices;

  vertices.resize(data.vtx.size());
  Vertex v;
  for (size_t i = 0; i < data.vtx.size(); i++)
  {
    v.position = data.vtx[i].position;
    v.normal = data.vtx[i].normal;
    vertices[i] = v;
  }

  glNamedBufferStorage(VertexStream, sizeof(Vertex) * vertices.size(), vertices.data(), 0);
  glNamedBufferStorage(IndexStream, sizeof(unsigned int) * data.idx.size(), data.idx.data(), 0);
}

TileGeometryStreams::~TileGeometryStreams()
{
  glDeleteBuffers(1, &VertexStream);
  glDeleteBuffers(1, &IndexStream);
}

void TileGeometryStreams::bind(int vertex, int index) const
{
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, vertex, VertexStream);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, index, IndexStream);
}

GPUMeshStreams::GPUMeshStreams(size_t maxVerts, size_t maxIndices)
{
  glCreateBuffers(1, &VertexStream);
  glCreateBuffers(1, &IndexStream);


  glNamedBufferStorage(VertexStream, 8 * sizeof(float) * maxVerts, nullptr, 0);
  glNamedBufferStorage(IndexStream, sizeof(unsigned int) * maxIndices, nullptr, 0);

  // Setup VAO.
  glGenVertexArrays(1, &VAO);
  glBindVertexArray(VAO);

  // Bind.
  glBindBuffer(GL_ARRAY_BUFFER, VertexStream);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IndexStream);

  size_t size;
  size_t offset = 0;

  // Setup VAO:
  // position
  size = 3 * sizeof(float);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void*)offset);
  offset += size;

  // padding.
  offset += sizeof(float);

  // normal
  size = 3 * sizeof(float);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void*)offset);
  offset += size;

  glBindVertexArray(0);

  // Unbind.
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

GPUMeshStreams::~GPUMeshStreams()
{
  glDeleteVertexArrays(1, &VAO);
  glDeleteBuffers(1, &VertexStream);
  glDeleteBuffers(1, &IndexStream);
}

void GPUMeshStreams::bind(int vertex, int index)
{
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, vertex, VertexStream);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, index, IndexStream);
}

void GPUMeshStreams::draw()
{
  glBindVertexArray(VAO);
  glDrawElements(GL_TRIANGLES, 480 * 3 * 3, GL_UNSIGNED_INT, 0);
}

Mesh::Mesh(const std::string& path)
{
  loadFromFile(path);
}

Mesh::~Mesh()
{
  m_parts.clear();
}

void Mesh::loadFromFile(const std::string& file)
{
  tinyobj::ObjReaderConfig reader_config;
  //reader_config.mtl_search_path = "./"; // Path to material files

  tinyobj::ObjReader reader;

  if (!reader.ParseFromFile(file, reader_config))
  {
    if (!reader.Error().empty())
    {
      std::cerr << "TinyObjReader: " << reader.Error();
    }
    exit(-1);
  }

  if (!reader.Warning().empty())
  {
    std::cout << "TinyObjReader: " << reader.Warning();
  }

  // Create buffers.

  std::vector<MeshPartData> partData;
  loadParts(reader, partData);

  // Finalize parts.
  for (int iPart = 0; iPart < partData.size(); ++iPart)
    m_parts.emplace_back(partData[iPart]);
}

void Mesh::draw() const
{
  for (const MeshPart& part : m_parts)
  {
    if (part.numElements == 0)
      continue;

    part.draw();
  }
}

TargetMesh::TargetMesh(const std::string& file)
{
  loadFromFile(file);
}

TargetMesh::~TargetMesh()
{
}

void TargetMesh::loadFromFile(const std::string& file)
{
  tinyobj::ObjReaderConfig reader_config;
  tinyobj::ObjReader reader;

  if (!reader.ParseFromFile(file, reader_config))
  {
    if (!reader.Error().empty())
    {
      std::cerr << "TinyObjReader: " << reader.Error();
    }
    exit(-1);
  }

  if (!reader.Warning().empty())
  {
    std::cout << "TinyObjReader: " << reader.Warning();
  }

  std::vector<MeshPartData> partData;
  loadParts(reader, partData, true);

  if (partData.size() != 1)
  {
    std::cerr << "Failed to load target mesh.\n";
    return;
  }

  triStream = TargetGeometryStream(partData[0]);
}

TileMesh::TileMesh(const std::string& file)
{
  loadFromFile(file);
}

TileMesh::~TileMesh()
{
}

void TileMesh::loadFromFile(const std::string& file)
{
  tinyobj::ObjReaderConfig reader_config;
  tinyobj::ObjReader reader;

  if (!reader.ParseFromFile(file, reader_config))
  {
    if (!reader.Error().empty())
    {
      std::cerr << "TinyObjReader: " << reader.Error();
    }
    exit(-1);
  }

  if (!reader.Warning().empty())
  {
    std::cout << "TinyObjReader: " << reader.Warning();
  }

  std::vector<MeshPartData> partData;
  loadParts(reader, partData, true);

  if (partData.size() != 1)
  {
    std::cerr << "Failed to load tile mesh.\n";
    return;
  }

  tileStreams = TileGeometryStreams(partData[0]);
}
