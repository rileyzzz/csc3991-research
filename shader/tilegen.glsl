#version 460 core

layout (local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

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

layout(std430, binding = 4) buffer outputIndexStream
{
    uint out_TileIndices[];
};

void projectOntoTriangle(inout Vertex v, in Triangle tri, int iTile) {
    vec3 bitangent = normalize(cross(tri.normal, tri.tangent));
    mat3 tangentBasis = mat3(tri.tangent, bitangent, tri.normal);

    vec3 triCenter = (tri.p0 + tri.p1 + tri.p2) / 3.0;
    vec3 srcpos = tri.p0;
    if (iTile == 1) srcpos = tri.p1;
    if (iTile == 2) srcpos = tri.p2;

    srcpos = triCenter * 0.8 + srcpos * 0.2;

    // v.position = srcpos + tangentBasis * (v.position + vec3(iTile, 0, 0));
    // v.position = srcpos + (v.position * 0.1) * tangentBasis;
    v.position = srcpos + (v.position * 0.4);
}

void main() {
    uint iTriangle = gl_GlobalInvocationID.x;
    if (iTriangle > in_Triangles.length())
        return;
    
    int baseVertex = in_Triangles[iTriangle].tileBase * in_TileVertices.length();
    int baseIndex = in_Triangles[iTriangle].tileBase * in_TileIndices.length();

    for (int iTile = 0; iTile < in_Triangles[iTriangle].tileNum; iTile++) {
        for (int iVert = 0; iVert < in_TileVertices.length(); iVert++) {
            Vertex v = in_TileVertices[iVert];
            projectOntoTriangle(v, in_Triangles[iTriangle], iTile);
            out_Vertices[baseVertex + iVert] = v;
        }

        for (int iIndex = 0; iIndex < in_TileIndices.length(); iIndex++) {
            out_TileIndices[baseIndex + iIndex] = baseVertex + in_TileIndices[iIndex];
        }

        baseVertex += in_TileVertices.length();
        baseIndex += in_TileIndices.length();
    }
}