#ifndef _MESH_H
#define _MESH_H
#include <string>

class Mesh
{
public:
  Mesh(const std::string& file);
  ~Mesh();

protected:
  void loadFromFile(const std::string& file);

};

#endif // _MESH_H