
layout (local_size_x = TILE_THREADGROUPS_X, local_size_y = 1, local_size_z = 1) in;

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

    vec3 n0; float padding5;
    vec3 n1; float padding6;
    vec3 n2; float padding7;
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
    uint out_baseVertex;
    Vertex out_Vertices[];
};

layout(std430, binding = 4) buffer outputIndexStream
{
    uint out_baseIndex;
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

    #if SMOOTH_NORMALS
    vec3 interpNormal = normalize(
      tri.n0 * baryCoord.x +
      tri.n1 * baryCoord.y +
      tri.n2 * baryCoord.z);
    #else // !SMOOTH_NORMALS
    vec3 interpNormal = tri.normal;
    #endif // !SMOOTH_NORMALS
    
    // Temp: match the displacement output
    interpNormal *= 0.5;

    v.position = finalCoord + v.position.y * interpNormal;

    vec3 normalCoord = vec3(v.normal.xz, 0.0);
    vec3 baryNormal = uvToBary * normalCoord;
    vec3 triCenter = (tri.p0 + tri.p1 + tri.p2) / 3.0;

    vec3 finalNorm = normalize(
        (tri.p0 - triCenter) * baryNormal.x +
        (tri.p1 - triCenter) * baryNormal.y +
        (tri.p2 - triCenter) * baryNormal.z +
        v.normal.y * interpNormal);
    v.normal = finalNorm;
}

float planeSide(vec3 v, vec4 plane) {
    return dot(v, plane.xyz) - plane.w;
}

// Clip results.
// const int CLIP_ALL_OUTSIDE = 0;
// const int CLIP_ALL_INSIDE = 1;

#define SCRATCH_VERTEX_COUNT 16
#define SCRATCH_INDEX_COUNT 32

int generated_baseVertex = 0;
Vertex generated_Vertices[SCRATCH_VERTEX_COUNT];

int generated_baseIndex = 0;
uint generated_Indices[SCRATCH_INDEX_COUNT];

// int out_baseVertex;
// int out_baseIndex;


// int addVertexToOutput(Vertex v) {
//     int idx = generated_baseVertex++;
//     out_Vertices[out_baseVertex + idx] = v;
//     return idx;
// }

int outputScratchVertex(Vertex v) {
    int idx = generated_baseVertex++;
    generated_Vertices[idx] = v;
    return idx;
}

void outputScratchIndex(uint i) {
    generated_Indices[generated_baseIndex++] = i;
}


void clipMeshToPlane(/*Vertex[SCRATCH_VERTEX_COUNT] vertices, uint numVertices, */ uint[SCRATCH_INDEX_COUNT] indices, uint numIndices, vec4 plane) {
    for (int i = 0; i < numIndices; i += 3) {
        uint origIndex[3];
        origIndex[0] = indices[i + 0];
        origIndex[1] = indices[i + 1];
        origIndex[2] = indices[i + 2];

        Vertex v[3];
        v[0] = generated_Vertices[origIndex[0]];
        v[1] = generated_Vertices[origIndex[1]];
        v[2] = generated_Vertices[origIndex[2]];
        
        int nInside = 0;
        float dist[3];
        for (int i = 0; i < 3; i++)
        {
            dist[i] = planeSide(v[i].position, plane);
            if (dist[i] <= 0.0)
                nInside++;
        }

        // Simple cases.
        if (nInside == 0)
        {
            // All outside, skip this triangle.
            continue;
        }
        
        if (nInside == 3)
        {
            // All inside. Transfer geometry.
            outputScratchIndex(origIndex[0]);
            outputScratchIndex(origIndex[1]);
            outputScratchIndex(origIndex[2]);
            continue;
        }
        
        if (nInside == 1)
        {
            // Triangle-triangle case.
            int insideVert;
            for (insideVert = 0; insideVert < 3; insideVert++)
            {
                if (dist[insideVert] <= 0.0)
                    break;
            }

            float split = -dist[insideVert];

            int outside0 = (insideVert + 1) % 3;
            int outside1 = (insideVert + 2) % 3;

            // Create two new vertices.
            Vertex p0 = lerpVertex(v[insideVert], v[outside0], split / (split + dist[outside0]));
            Vertex p1 = lerpVertex(v[insideVert], v[outside1], split / (split + dist[outside1]));

            int i0 = outputScratchVertex(p0);
            int i1 = outputScratchVertex(p1);

            outputScratchIndex(origIndex[insideVert]);
            outputScratchIndex(i0);
            outputScratchIndex(i1);
        }
        else // (nInside == 2)
        {
            // Triangle-quad case.
            int outsideVert;
            for (outsideVert = 0; outsideVert < 3; outsideVert++)
            {
                if (dist[outsideVert] > 0.0)
                    break;
            }

            int inside0 = (outsideVert + 1) % 3;
            int inside1 = (outsideVert + 2) % 3;

            float split0 = -dist[inside0];
            float split1 = -dist[inside1];

            // Create two new vertices.
            Vertex p0 = lerpVertex(v[inside0], v[outsideVert], split0 / (split0 + dist[outsideVert]));
            Vertex p1 = lerpVertex(v[inside1], v[outsideVert], split1 / (split1 + dist[outsideVert]));

            int i0 = outputScratchVertex(p0);
            int i1 = outputScratchVertex(p1);

            outputScratchIndex(origIndex[inside0]);
            outputScratchIndex(origIndex[inside1]);
            outputScratchIndex(i0);

            outputScratchIndex(i0);
            outputScratchIndex(origIndex[inside1]);
            outputScratchIndex(i1);
        }
    }
}

void main() {
    uint tileTriangles = (in_TileIndices.length() / 3);
    uint iTargetTriangle = gl_GlobalInvocationID.x / tileTriangles;
    uint iTileTriangle = gl_GlobalInvocationID.x - (iTargetTriangle * tileTriangles);
    if (iTargetTriangle > in_Triangles.length())
        return;
    
    vec3 triVertex[3];
    triVertex[0] = in_Triangles[iTargetTriangle].p0;
    triVertex[1] = in_Triangles[iTargetTriangle].p1;
    triVertex[2] = in_Triangles[iTargetTriangle].p2;

    vec3 triNormal[3];
    triNormal[0] = in_Triangles[iTargetTriangle].n0;
    triNormal[1] = in_Triangles[iTargetTriangle].n1;
    triNormal[2] = in_Triangles[iTargetTriangle].n2;

    #if !ENABLE_CLIPPING
    for (int iTile = 0; iTile < 4; iTile++) {
        uint outBase = atomicAdd(out_baseVertex, 3);
        uint indexBase = atomicAdd(out_baseIndex, 3);
        for (int iVert = 0; iVert < 3; iVert++)
        {
            uint tileIndex = in_TileIndices[iTileTriangle * 3 + iVert];
            Vertex v = in_TileVertices[tileIndex];
            projectOntoTriangle(v, in_Triangles[iTargetTriangle], iTile);
            out_Vertices[outBase + iVert] = v;
            out_TileIndices[indexBase + iVert] = outBase + iVert;
        }
    }

    // int out_baseVertex = in_Triangles[iTargetTriangle].tileBase * in_TileVertices.length();
    // int out_baseIndex = in_Triangles[iTargetTriangle].tileBase * in_TileIndices.length();

    #else // ENABLE_CLIPPING
    for (int iTile = 0; iTile < 4; iTile++) {
        uint srcIdx[SCRATCH_INDEX_COUNT];
        int numIdx = 3;
        srcIdx[0] = in_TileIndices[iTileTriangle * 3 + 0];
        srcIdx[1] = in_TileIndices[iTileTriangle * 3 + 1];
        srcIdx[2] = in_TileIndices[iTileTriangle * 3 + 2];

        Vertex srcVtx[SCRATCH_VERTEX_COUNT];
        int numVtx = 3;
        srcVtx[0] = in_TileVertices[srcIdx[0]];
        srcVtx[1] = in_TileVertices[srcIdx[1]];
        srcVtx[2] = in_TileVertices[srcIdx[2]];

        // Project the initial vertices.
        // int iTile = 0;
        for (int i = 0; i < numVtx; i++) {
            projectOntoTriangle(srcVtx[i], in_Triangles[iTargetTriangle], iTile);
        }

        generated_baseVertex = numVtx;
        generated_Vertices = srcVtx;

        numIdx = 3;
        for (int i = 0; i < 3; i++)
            srcIdx[i] = i;
        
        // Clip the index list, adding vertices where needed.
        for (int iPlane = 0; iPlane < 3; iPlane++) {
            generated_baseIndex = 0;
            // generated_baseVertex = 0;
            
            vec3 planeStart = triVertex[iPlane];
            vec3 planeEnd = triVertex[(iPlane + 1) % 3];
            vec3 planeVec = normalize(planeEnd - planeStart);
            #if SMOOTH_NORMALS
            vec3 planeNorm = triNormal[(iPlane + 1) % 3];
            #else // !SMOOTH_NORMALS
            // vec3 planeNorm = in_Triangles[iTargetTriangle].normal;
            vec3 planeNorm = triNormal[(iPlane + 1) % 3];
            #endif // !SMOOTH_NORMALS
            vec3 planeNormal = normalize(cross(planeVec, planeNorm));
            float planeDist = dot(planeStart, planeNormal);
            vec4 plane = vec4(planeNormal, planeDist);

            // vec4 plane = vec4(0, 1, 0, -0.75);
            clipMeshToPlane(srcIdx, numIdx, plane);

            // Copy new geometry.
            srcIdx = generated_Indices;
            numIdx = generated_baseIndex;
        }

        // Push new geometry.
        uint outBase = atomicAdd(out_baseVertex, generated_baseVertex);
        for (int i = 0; i < generated_baseVertex; i++)
        {
            // out_Vertices[outBase + i] = generated_Vertices[i];

            Vertex test = generated_Vertices[i];
            // test.position += in_Triangles[iTargetTriangle].normal * 0.1;
            // test.position += normalize(in_Triangles[iTargetTriangle].p1 - in_Triangles[iTargetTriangle].p0) * 0.1;
            out_Vertices[outBase + i] = test;
        }
        
        uint indexBase = atomicAdd(out_baseIndex, generated_baseIndex);
        for (int i = 0; i < generated_baseIndex; i++)
            out_TileIndices[indexBase + i] = outBase + generated_Indices[i];

    }
    #endif // ENABLE_CLIPPING
}