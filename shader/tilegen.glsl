#version 430 core

layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

struct Vertex {
    vec3 position;
    vec3 normal;
};

struct Triangle {
    vec3 p0;
    vec3 p1;
    vec3 p2;
    vec3 normal;
    vec3 tangent;
    int tileBase;
    int tileNum;
};

layout(std430, binding = 0) buffer inputTriangleStream
{
    Triangle in_Triangles[];
};

layout(std430, binding = 1) buffer inputVertexStream
{
    Vertex in_TileVertices[];
};

layout(std430, binding = 2) buffer inputIndexStream
{
    uint in_TileIndices[];
};

layout(std430, binding = 3) buffer outputVertexStream
{
    Vertex out_Vertices[];
};

void projectOntoTriangle(inout vec3 v, in Triangle tri) {
    vec3 bitangent = cross(normal, tangent);
    mat3 tangentBasis = mat3(tri.tangent, bitangent, tri.normal);

    v = tangentBasis * v;
}

void main() {
    int iTriangle = gl_GlobalInvocationID.x;
    int baseVertex = in_Triangles[iTriangle].tileBase * in_TileVertices.length();
    int baseIndex = in_Triangles[iTriangle].tileBase * in_TileIndices.length();

    for (int iTile = 0; i < in_Triangles[iTriangle].tileNum; i++) {
        for (int iVert = 0; iVert < in_TileVertices.length(); iVert++) {
            Vertex v = in_TileVertices[iVert];
            projectOntoTriangle(v, in_Triangles[iTriangle]);
            out_Vertices[baseVertex + iVert] = v;
        }

        for (int iIndex = 0; iIndex < in_TileIndices.length(); iIndex++) {
            out_Vertices[baseIndex + iIndex] = baseVertex + in_TileIndices[iIndex];
        }

        baseVertex += in_TileVertices.length();
        baseIndex += in_TileIndices.length();
    }
}