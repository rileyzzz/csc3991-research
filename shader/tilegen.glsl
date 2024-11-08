#version 460 core

layout (local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

struct Vertex {
    vec3 position;
    vec3 normal;
};

Vertex lerpVertex(Vertex a, Vertex b, float t) {
    Vertex v;
    v.position = mix(a.position, b.position, t);
    v.normal = normalize(mix(a.normal, b.normal, t));
    return v;
}

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

    vec3 normalCoord = vec3(v.normal.xz, 0.0);
    vec3 baryNormal = uvToBary * normalCoord;
    vec3 triCenter = (tri.p0 + tri.p1 + tri.p2) / 3.0;

    vec3 finalNorm = normalize(
        (tri.p0 - triCenter) * baryNormal.x +
        (tri.p1 - triCenter) * baryNormal.y +
        (tri.p2 - triCenter) * baryNormal.z +
        v.normal.y * tri.normal);
    v.normal = finalNorm;

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

float planeSide(vec3 v, vec4 plane) {
    return dot(v, plane.xyz) - plane.w;
}

// Clip results.
// const int CLIP_ALL_OUTSIDE = 0;
// const int CLIP_ALL_INSIDE = 1;
// #define SCRATCH_VERTEX_COUNT 4096
#define SCRATCH_INDEX_COUNT 1024

int generated_baseVertex = 0;
// Vertex generated_Vertices[SCRATCH_VERTEX_COUNT];

int generated_baseIndex = 0;
// uint generated_Indices[SCRATCH_INDEX_COUNT];
int out_baseVertex;
int out_baseIndex;


// int addVertexToOutput(Vertex v) {
//     // int idx = generated_baseVertex++;
//     // generated_Vertices[idx] = v;
//     // return idx;

//     int idx = generated_baseVertex++;
//     out_Vertices[out_baseVertex + idx] = v;
//     return idx;
// }

// void outputIndex(uint i) {
//     generated_Indices[generated_baseIndex++] = i;
// }


// void clipMeshToPlane(uint[SCRATCH_INDEX_COUNT] indices, uint numIndices, vec4 plane) {
//     for (int i = 0; i < numIndices; i += 3) {
//         uint origIndex[3];
//         origIndex[0] = indices[i + 0];
//         origIndex[1] = indices[i + 1];
//         origIndex[2] = indices[i + 2];

//         Vertex v[3];
//         v[0] = out_Vertices[origIndex[0]];
//         v[1] = out_Vertices[origIndex[1]];
//         v[2] = out_Vertices[origIndex[2]];
        
//         int nInside = 0;
//         float dist[3];
//         for (int i = 0; i < 3; i++)
//         {
//             dist[i] = planeSide(v[i].position, plane);
//             if (dist[i] <= 0.0)
//                 nInside++;
//         }

//         // Simple cases.
//         if (nInside == 0)
//         {
//             // All outside, skip this triangle.
//             continue;
//         }
        
//         if (nInside == 3)
//         {
//             // All inside. Transfer geometry.
//             outputIndex(origIndex[0]);
//             outputIndex(origIndex[1]);
//             outputIndex(origIndex[2]);
//             continue;
//         }
        
//         if (nInside == 1)
//         {
//             // Triangle-triangle case.
//             /*
//             int insideVert;
//             for (insideVert = 0; i < 3; i++)
//             {
//                 if (inside[insideVert])
//                     break;
//             }

//             float split = -dist[insideVert];

//             int outside0 = (insideVert + 1) % 3;
//             int outside1 = (insideVert + 2) % 3;

//             // Create two new vertices.
//             Vertex p0 = lerpVertex(v[insideVert], v[outside0], split / (split + dist[outside0]));
//             Vertex p1 = lerpVertex(v[insideVert], v[outside1], split / (split + dist[outside1]));

//             int i0 = outputVertex(p0);
//             int i1 = outputVertex(p1);

//             outputIndex(origIndex[insideVert]);
//             outputIndex(i0);
//             outputIndex(i1);
//             */
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
    
    int out_baseVertex = in_Triangles[iTriangle].tileBase * in_TileVertices.length();
    int out_baseIndex = in_Triangles[iTriangle].tileBase * in_TileIndices.length();

    for (int iTile = 0; iTile < in_Triangles[iTriangle].tileNum; iTile++) {
        
        for (int iVert = 0; iVert < in_TileVertices.length(); iVert++) {
            Vertex v = in_TileVertices[iVert];
            projectOntoTriangle(v, in_Triangles[iTriangle], iTile);
            out_Vertices[out_baseVertex + iVert] = v;
        }

        for (int iIndex = 0; iIndex < in_TileIndices.length(); iIndex++) {
            out_TileIndices[out_baseIndex + iIndex] = out_baseVertex + in_TileIndices[iIndex];
        }

        out_baseVertex += in_TileVertices.length();
        out_baseIndex += in_TileIndices.length();
        


        // Step 1: Push the transformed tile geometry.
        /*
        generated_baseVertex = 0;
        generated_baseIndex = 0;
        for (int iVert = 0; iVert < in_TileVertices.length(); iVert++) {
            Vertex v = in_TileVertices[iVert];
            projectOntoTriangle(v, in_Triangles[iTriangle], iTile);
            addVertexToOutput(v);
        }

        // Step 2: Clip the index stream to the triangle boundary, adding vertices where needed.
        for (int iIndex = 0; iIndex < in_TileIndices.length() ; iIndex++) {
            outputIndex(in_TileIndices[iIndex]);
        }

        
        // vec4 plane = vec4(0, 1, 0, 0);
        // clipMeshToPlane()

        // for (int iPlane = 0; iPlane < 3; iPlane++) {
        //     int clipResult = clipTileTriangleToPlane(v0, v1, v2, v3);
        // }

        // Step 3: Output clipped geometry.
        for (int iIndex = 0; iIndex < generated_baseIndex; iIndex++) {
            out_TileIndices[out_baseIndex + iIndex] = out_baseVertex + generated_Indices[iIndex];
        }

        out_baseVertex += generated_baseVertex;
        out_baseIndex += generated_baseIndex;
        */
    
    }
}