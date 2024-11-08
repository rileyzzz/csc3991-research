#version 460 core

layout (local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

struct Vertex {
    vec3 position;
    vec3 normal;
};

struct Triangle {
    vec3 p0;
    int tileBase;

    vec3 p1;
    int tileNum;

    vec3 p2;
    float padding0;

    vec3 normal;
    float padding1;

    // vec3 tangent;
    // float padding2;

    vec3 uvToBary0; float padding2;
    vec3 uvToBary1; float padding3;
    vec3 uvToBary2; float padding4;
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
    // vec3 bitangent = normalize(cross(tri.normal, tri.tangent));
    // mat3 tangentBasis = mat3(tri.tangent, tri.normal, bitangent);
    v.position.xz = v.position.xz * 0.5 + 0.5;

    // Tile test.
    v.position.xz *= 0.5;
    if (iTile == 0) v.position.x += 0.5;
    if (iTile == 1) v.position.z += 0.5;
    if (iTile == 2) v.position.xz += 0.5;

    mat3 uvToBary = mat3(tri.uvToBary0, tri.uvToBary1, tri.uvToBary2);

    vec3 uvCoord = vec3(v.position.xz, 1.0);
    vec3 baryCoord = uvToBary * uvCoord;

    vec3 finalCoord =
        tri.p0 * baryCoord.x +
        tri.p1 * baryCoord.y +
        tri.p2 * baryCoord.z;

    v.position = finalCoord + v.position.y * tri.normal;

    // vec3 triCenter = (tri.p0 + tri.p1 + tri.p2) / 3.0;
    // vec3 srcpos = tri.p0;
    // if (iTile == 1) srcpos = tri.p1;
    // if (iTile == 2) srcpos = tri.p2;

    // srcpos = triCenter * 0.8 + srcpos * 0.2;
    // srcpos += tri.normal;

    // scale down a bit.
    // v.position *= 0.4;

    // v.position = srcpos + tangentBasis * (v.position + vec3(iTile, 0, 0));
    // v.position = srcpos + tangentBasis * v.position;
    // v.position = srcpos + v.position;
    // v.position = triCenter + v.position + iTile * tri.normal * 0.1;
}

bool planeSide(vec3 v, vec4 plane) {
    return dot(v, plane.xyz) >= plane.w;
}

// Clip results.
// const int CLIP_ALL_OUTSIDE = 0;
// const int CLIP_ALL_INSIDE = 1;

// int baseVertex = 0;
// Vertex generated_Vertices[1024];

// int baseIndex = 0;
// uint generated_Indices[1024];

// Transfer a vertex from the source buffer to the destination, or use its index if it already exists.
// int transferVertex(int src) {
// }

// int outputVertex(Vertex v) {
//     generated_Vertices[baseVertex++] = v;
// }

// void outputIndex(int i) {
//     generated_Indices[baseIndex++] = i;
//     generated_VertexUsed[i] = true;
// }

// void clearVertexUsed() {
//     for (int i = 0; i < generated_VertexUsed.length(); i++)
//         generated_VertexUsed[i] = false;
// }

// void clipTileTriangleToPlane(inout int v0, inout int v1, inout int v2, out int v3, vec4 plane) {
//     v3 = 0;
// }

// void clipMeshToPlane(Vertex[] vertices, uint[] indices, vec4 plane) {
//     for (int i = 0; i < indices.length(); i += 3) {
//         uint i0 = indices[i + 0];
//         uint i1 = indices[i + 1];
//         uint i2 = indices[i + 2];

//         Vertex v0 = vertices[i0];
//         Vertex v1 = vertices[i1];
//         Vertex v2 = vertices[i2];
        
//         bool v0Inside = planeSide(v0.position, plane);
//         bool v1Inside = planeSide(v1.position, plane);
//         bool v2Inside = planeSide(v2.position, plane);

//         int nInside = int(v0Inside) + int(v1Inside) + int(v2Inside);

//         // Simple cases.
//         if (nInside == 0)
//         {
//             // All outside, skip this triangle.
//             continue;
//         }
        
//         if (nInside == 3)
//         {
//             // All inside. Transfer geometry.
//             i0 = transferVertex(i0);
//             i1 = transferVertex(i1);
//             i2 = transferVertex(i2);

//             outputIndex(i0);
//             outputIndex(i1);
//             outputIndex(i2);
//         }
        
//         if (nInside == 1)
//         {
//             // Triangle-triangle case.
//         }
//         else // (nInside == 2)
//         {
//             // Triangle-quad case.
//         }
//     }
// }

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

        // Step 1: Push the transformed tile geometry.
        /*
        baseVertex = 0;
        baseIndex = 0;
        clearVertexUsed();

        for (int iVert = 0; iVert < in_TileVertices.length(); iVert++) {
            Vertex v = in_TileVertices[iVert];
            projectOntoTriangle(v, in_Triangles[iTriangle], iTile);
            outputVertex(v);
        }

        for (int iIndex = 0; iIndex < in_TileIndices.length(); iIndex++) {
            outputIndex(in_TileIndices[iIndex]);
        }

        // Step 2: Clip geometry to target edges.
        for (int iPlane = 0; iPlane < 3; iPlane++) {
            int clipResult = clipTileTriangleToPlane(v0, v1, v2, v3);

        }
        */

        baseVertex += in_TileVertices.length();
        baseIndex += in_TileIndices.length();
    }
}