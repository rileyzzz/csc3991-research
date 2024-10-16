#include "mesh.h"
#include <glad/glad.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <iostream>


MeshPart::MeshPart(const MeshPartData& data)
{
  glGenVertexArrays(1, &VAO);
  glBindVertexArray(VAO);

  // init VBO
  glGenBuffers(1, &VBO);
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER, data.vtx.size() * sizeof(MeshVertex), (void*)data.vtx.data(), GL_STATIC_DRAW);

  // init EBO
  glGenBuffers(1, &EBO);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, data.idx.size() * sizeof(unsigned short), (void*)data.idx.data(), GL_STATIC_DRAW);
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

void MeshPart::draw() const
{
  glBindVertexArray(VAO);
  glDrawElements(GL_TRIANGLES, numElements, GL_UNSIGNED_SHORT, 0);
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

  auto& attrib = reader.GetAttrib();
  auto& shapes = reader.GetShapes();
  auto& materials = reader.GetMaterials();

  std::vector<MeshPartData> partData;
  // Add an extra default material.
  partData.resize(1 + materials.size(), MeshPartData());

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
        unsigned short iVertex;
        
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

          // Only support ushort indices.
          if (part.vtx.size() >= std::numeric_limits<unsigned short>::max())
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

  // Finalize parts.
  for (int iPart = 0; iPart < partData.size(); ++iPart)
    m_parts.emplace_back(partData[iPart]);
}

void Mesh::draw() const
{
  for (const MeshPart& part : m_parts)
    part.draw();
}