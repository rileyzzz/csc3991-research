#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <filesystem>
#include <memory>
#include "mesh.h"
#include "shader.h"
#include "texture.h"
#include "buffer.h"
#include "statsobject.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/euler_angles.hpp>

// ImGUI.
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "implot.h"

// stbi_write
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

static glm::mat4 proj;
static glm::mat4 view;
static glm::mat4 viewProj;
static glm::vec3 cameraCenterPos;
static glm::vec3 cameraOrbitRot;
static float cameraDist = 5.f;

static glm::vec3 cameraPos;
static glm::vec3 cameraForward;
static glm::vec3 cameraRight;

static bool bDraggingMouse = false;

enum class ClippingMode
{
  Off,
  On,

  Max
};

enum class NormalMode
{
  Flat,
  Smooth,

  Max
};

enum class ThreadgroupSize : int
{
  Threads_64,
  Threads_128,
  Threads_256,
  Threads_512,

  Max
};

enum class SubdivLevel : int
{
  Subdiv_2,
  Subdiv_4,
  Subdiv_8,
  Subdiv_16,
  Subdiv_32,
  Subdiv_64,
  Subdiv_128,

  Count
};

const char* s_subdivLevelNames[(int)SubdivLevel::Count] = {
  "2x",
  "4x",
  "8x",
  "16x",
  "32x",
  "64x",
  "128x",
};

enum class MeshTarget : int
{
  Cube_Simple,
  Cube_1,
  Cube_2,
  Cube_3,
  Cube_4,
  Ico,
  SmoothIco,
  SmoothIco3,
  Cylinder,
  SmoothCylinder,
  Sponza,

  Count
};

const char* s_meshTargetNames[(int)MeshTarget::Count] = {
  "Simple Cube",
  "Subdiv Cube(1)",
  "Subdiv Cube(2)",
  "Subdiv Cube(3)",
  "Subdiv Cube(4)",
  "Icosahedron",
  "Smooth Icosahedron",
  "Smooth Icosahedron(x3)",
  "Cylinder",
  "Smooth Cylinder",
  "Sponza"
};

enum class TileMeshes : int
{
  Brick,
  InsetCube,
  Sponza,

  Count
};

const char* s_tileMeshNames[(int)TileMeshes::Count] = {
  "Brick",
  "Inset Cube",
  "Sponza"
};

static GLuint getThreadgroupSize(ThreadgroupSize size)
{
  if (size == ThreadgroupSize::Threads_64) return 64;
  if (size == ThreadgroupSize::Threads_128) return 128;
  if (size == ThreadgroupSize::Threads_256) return 256;
  if (size == ThreadgroupSize::Threads_512) return 512;
  return 0;
}

static GLuint getSubdivLevel(SubdivLevel level)
{
  if (level == SubdivLevel::Subdiv_2) return 2;
  if (level == SubdivLevel::Subdiv_4) return 4;
  if (level == SubdivLevel::Subdiv_8) return 8;
  if (level == SubdivLevel::Subdiv_16) return 16;
  if (level == SubdivLevel::Subdiv_32) return 32;
  if (level == SubdivLevel::Subdiv_64) return 64;
  if (level == SubdivLevel::Subdiv_128) return 128;
  return 0;
}

static std::unique_ptr<ShaderProgram> tilegen[(int)ClippingMode::Max][(int)NormalMode::Max][(int)ThreadgroupSize::Max];
static std::unique_ptr<ShaderProgram>& getTilegenShader(ClippingMode clip, NormalMode normals, ThreadgroupSize threads)
{
  return tilegen[(int)clip][(int)normals][(int)threads];
}

static std::unique_ptr<ShaderProgram> simpleMaterial;
static std::unique_ptr<ShaderProgram> texturedMaterial;

static int s_subdivLevel = (int)SubdivLevel::Subdiv_64;
static std::unique_ptr<ShaderProgram> subdivMaterials[(int)SubdivLevel::Count];
static std::unique_ptr<ShaderProgram> texturedSubdivMaterials[(int)SubdivLevel::Count];

static std::unique_ptr<ShaderProgram> lineMaterial;
static std::unique_ptr<GPUMeshStreams> generatedMesh;

static std::unique_ptr<Mesh> s_sponza;
static int s_curMeshTarget = (int)MeshTarget::Cube_2;
static int s_curTilemesh = (int)TileMeshes::Brick;
static std::vector<std::unique_ptr<TargetMesh>> s_meshTarget;
static std::vector<std::unique_ptr<Mesh>> s_tessellationTarget;
static std::vector<std::unique_ptr<TileMesh>> s_tileMeshes;
static std::vector<std::unique_ptr<Texture>> s_tileDiffTextures;
static std::vector<std::unique_ptr<Texture>> s_tileDispTextures;

//static std::unique_ptr<StorageBuffer> outputVertices;
//static std::unique_ptr<StorageBuffer> outputIndices;

static bool s_bDrawWireframe = false;
static bool s_bDrawTessellatedMesh = true;
static bool s_bDrawNormalVectors = false;
static bool s_bComputeReferenceImplementation = false;
static bool s_bOneTimeCompute = false;
static bool s_bEnableClipping = true;
static bool s_bSmoothNormals = false;
static ThreadgroupSize s_threadgroupSize = ThreadgroupSize::Threads_256;
static bool s_bDrawReferenceImplementation = false;
static int s_nTrianglesOnScreen = 0;
static bool s_bTakeScreenshot = false;

enum class GLQuery
{
  ComputeTime,
  TessellationTriangles,
  TessellationRenderTime,
  TilemeshRenderTime,

  Max
};
static GLuint s_glQueries[(int)GLQuery::Max] = { 0 };

#define FRAMETIME_WINDOW 1024
static double s_frameTimes[FRAMETIME_WINDOW] = { 0 };

// Stats objects.
static bool s_bRecordingFrametime = false;
static StatsObject s_statsFrametime("frametime.csv", { "Compute", "Tesselation", "Render" });


static std::unique_ptr<Mesh> loadMesh(const std::string& mesh);
static std::unique_ptr<TargetMesh> loadTargetMesh(const std::string& mesh);
static std::unique_ptr<TileMesh> loadTileMesh(const std::string& mesh);
static void loadShaders(void);
static void drawUI(GLFWwindow* window, double dt);
static void updateInput(GLFWwindow* window, float dt);
static void updateCamera(GLFWwindow* window);
static void saveScreenshot(GLFWwindow* window);

static void generateSurfaceGeometry(const TargetMesh& target, const TileMesh& tile);
static void drawScene(void);
static void drawLines(Mesh* mesh);

void glfwErrorCallback(int error, const char* description)
{
  fprintf(stderr, "Error: %s\n", description);
}

void glfwScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
  cameraDist -= yoffset;
  cameraDist = std::clamp(cameraDist, 1.f, 10.f);
}


void glDebugOutput(GLenum source,
  GLenum type,
  unsigned int id,
  GLenum severity,
  GLsizei length,
  const char* message,
  const void* userParam)
{
  if (id == 131169 || id == 131185 || id == 131218 || id == 131204 || id == 131186) return;

  std::cout << "GL DEBUG (" << id << "): " << message << "\n";
}

int main()
{
  if (!glfwInit())
  {
    std::cerr << "Failed to init GLFW.\n";
    return -1;
  }

  glfwSetErrorCallback(glfwErrorCallback);

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#if _DEBUG
  glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, true);
#endif // _DEBUG

  GLFWwindow* window = glfwCreateWindow(640, 480, "Sample", NULL, NULL);
  if (!window)
  {
    std::cerr << "Failed to create window.\n";
    return -1;
  }

  glfwSetScrollCallback(window, glfwScrollCallback);

  // Make the context current.
  glfwMakeContextCurrent(window);

  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
  {
    std::cout << "Failed to initialize GLAD.\n";
    return -1;
  }

  // Setup debug callback.
#if _DEBUG
  int flags;
  glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
  if (flags & GL_CONTEXT_FLAG_DEBUG_BIT)
  {
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(glDebugOutput, nullptr);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
    std::cout << "Registered debug callback.\n";
  }
  else
  {
    std::cerr << "No debug context created!\n";
  }
#endif // _DEBUG

  IMGUI_CHECKVERSION();
  if (!ImGui::CreateContext())
  {
    std::cerr << "Failed to create Dear ImGui context!\n";
    return -1;
  }

  if (!ImPlot::CreateContext())
  {
    std::cerr << "Failed to create ImPlot context!\n";
    return -1;
  }

  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init();

  // Load shaders.
  loadShaders();

  s_tileMeshes.resize((int)TileMeshes::Count);
  s_tileDiffTextures.resize((int)TileMeshes::Count);
  s_tileDispTextures.resize((int)TileMeshes::Count);

  s_tileMeshes[(int)TileMeshes::Brick] = loadTileMesh("tile_brick.obj");
  s_tileDiffTextures[(int)TileMeshes::Brick] = nullptr;
  s_tileDispTextures[(int)TileMeshes::Brick] = std::make_unique<Texture>((std::filesystem::path(SCENE_DIR) / "brick.jpg").string());

  s_tileMeshes[(int)TileMeshes::InsetCube] = loadTileMesh("cube_in_cube.obj");
  s_tileDiffTextures[(int)TileMeshes::InsetCube] = nullptr;
  s_tileDispTextures[(int)TileMeshes::InsetCube] = std::make_unique<Texture>((std::filesystem::path(SCENE_DIR) / "inset_cubes_heights.tga").string());

  //s_tileMeshes[(int)TileMeshes::Sponza] = loadTileMesh("sponza_brick.obj");
  s_tileMeshes[(int)TileMeshes::Sponza] = loadTileMesh("sponza_brick_2.obj");
  s_tileDiffTextures[(int)TileMeshes::Sponza] = std::make_unique<Texture>((std::filesystem::path(SCENE_DIR) / "sponza/textures/spnza_bricks_a_diff.png").string());
  s_tileDispTextures[(int)TileMeshes::Sponza] = std::make_unique<Texture>((std::filesystem::path(SCENE_DIR) / "sponza/textures/spnza_bricks_a_bump.png").string());

  s_sponza = loadMesh("sponza/sponza_no_bricks_scaled.obj");

  s_meshTarget.resize((int)MeshTarget::Count);
  s_meshTarget[(int)MeshTarget::Cube_Simple] = loadTargetMesh("cube_simple.obj");
  s_meshTarget[(int)MeshTarget::Cube_1] = loadTargetMesh("cube_1.obj");
  s_meshTarget[(int)MeshTarget::Cube_2] = loadTargetMesh("cube_2.obj");
  s_meshTarget[(int)MeshTarget::Cube_3] = loadTargetMesh("cube_3.obj");
  s_meshTarget[(int)MeshTarget::Cube_4] = loadTargetMesh("cube_4.obj");
  s_meshTarget[(int)MeshTarget::Ico] = loadTargetMesh("test_ico.obj");
  s_meshTarget[(int)MeshTarget::SmoothIco] = loadTargetMesh("smooth_ico.obj");
  s_meshTarget[(int)MeshTarget::SmoothIco3] = loadTargetMesh("smooth_ico_x3.obj");
  s_meshTarget[(int)MeshTarget::Cylinder] = loadTargetMesh("cylinder.obj");
  s_meshTarget[(int)MeshTarget::SmoothCylinder] = loadTargetMesh("cylinder_smooth.obj");
  s_meshTarget[(int)MeshTarget::Sponza] = loadTargetMesh("sponza/sponza_bricks_scaled.obj");

  s_tessellationTarget.resize((int)MeshTarget::Count);
  s_tessellationTarget[(int)MeshTarget::Cube_Simple] = loadMesh("cube_simple.obj");
  s_tessellationTarget[(int)MeshTarget::Cube_1] = loadMesh("cube_1.obj");
  s_tessellationTarget[(int)MeshTarget::Cube_2] = loadMesh("cube_2.obj");
  s_tessellationTarget[(int)MeshTarget::Cube_3] = loadMesh("cube_3.obj");
  s_tessellationTarget[(int)MeshTarget::Cube_4] = loadMesh("cube_4.obj");
  s_tessellationTarget[(int)MeshTarget::Ico] = loadMesh("test_ico.obj");
  s_tessellationTarget[(int)MeshTarget::SmoothIco] = loadMesh("smooth_ico.obj");
  s_tessellationTarget[(int)MeshTarget::SmoothIco3] = loadMesh("smooth_ico_x3.obj");
  s_tessellationTarget[(int)MeshTarget::Cylinder] = loadMesh("cylinder.obj");
  s_tessellationTarget[(int)MeshTarget::SmoothCylinder] = loadMesh("cylinder_smooth.obj");
  s_tessellationTarget[(int)MeshTarget::Sponza] = loadMesh("sponza/sponza_bricks_scaled.obj");


  const int maxVertices = 1024 * 128 * 128 * 4;
  const int maxIndices = 3 * maxVertices;
  generatedMesh = std::make_unique<GPUMeshStreams>(maxVertices, maxIndices);

  // Setup.
  glEnable(GL_DEPTH_TEST);

  glGenQueries((int)GLQuery::Max, s_glQueries);

  // Main loop.
  double lastTime = glfwGetTime();
  for (;;)
  {
    if (glfwWindowShouldClose(window))
      break;

    double time = glfwGetTime();
    double dt = time - lastTime;
    lastTime = time;

    // UI
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    drawUI(window, dt);

    updateCamera(window);
    updateInput(window, dt);

    glClearColor(0.3f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (s_bDrawWireframe)
    {
      glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }
    else
    {
      glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    TargetMesh* curTarget = nullptr;
    Mesh* tessellateMesh = nullptr;
    TileMesh* curTile = nullptr;
    Texture* tileDiffTex = nullptr;
    Texture* tileDispTex = nullptr;
    if (s_curMeshTarget >= 0 && s_curMeshTarget < (int)MeshTarget::Count)
    {
      curTarget = s_meshTarget[s_curMeshTarget].get();
      tessellateMesh = s_tessellationTarget[s_curMeshTarget].get();
    }

    if (s_curTilemesh >= 0 && s_curTilemesh < (int)TileMeshes::Count)
    {
      curTile = s_tileMeshes[s_curTilemesh].get();
      tileDiffTex = s_tileDiffTextures[s_curTilemesh].get();
      tileDispTex = s_tileDispTextures[s_curTilemesh].get();
    }

    // Compute phase.
    glBeginQuery(GL_TIME_ELAPSED, s_glQueries[(int)GLQuery::ComputeTime]);
    if ((s_bComputeReferenceImplementation || s_bOneTimeCompute) && curTarget && curTile)
    {
      generateSurfaceGeometry(*curTarget, *curTile);
      s_bOneTimeCompute = false;
    }
    glEndQuery(GL_TIME_ELAPSED);

    const ShaderProgram* curSubdivMaterial = tileDiffTex ? texturedSubdivMaterials[s_subdivLevel].get() : subdivMaterials[s_subdivLevel].get();
    if (curSubdivMaterial)
    {
      curSubdivMaterial->bind();

      // Set uniforms.
      glm::mat4 model(1.f);
      glUniformMatrix4fv(curSubdivMaterial->getUniformLocation("viewProj"), 1, GL_FALSE, glm::value_ptr(viewProj));
      glUniformMatrix4fv(curSubdivMaterial->getUniformLocation("model"), 1, GL_FALSE, glm::value_ptr(model));
      glUniform3fv(curSubdivMaterial->getUniformLocation("viewPos"), 1, glm::value_ptr(cameraPos));

      glUniform1i(curSubdivMaterial->getUniformLocation("diffuse"), 0);
      if (tileDiffTex)
      {
        glActiveTexture(GL_TEXTURE0);
        tileDiffTex->bind();
      }

      glUniform1i(curSubdivMaterial->getUniformLocation("displacement"), 1);
      if (tileDispTex)
      {
        glActiveTexture(GL_TEXTURE1);
        tileDispTex->bind();
      }

      glBeginQuery(GL_TIME_ELAPSED, s_glQueries[(int)GLQuery::TessellationRenderTime]);
      glBeginQuery(GL_PRIMITIVES_GENERATED, s_glQueries[(int)GLQuery::TessellationTriangles]);
      if (s_bDrawTessellatedMesh && tessellateMesh)
      {
        tessellateMesh->drawPatches();
      }
      glEndQuery(GL_TIME_ELAPSED);
      glEndQuery(GL_PRIMITIVES_GENERATED);
    }

    drawScene();

    if (s_bDrawNormalVectors && tessellateMesh)
    {
      drawLines(tessellateMesh);
    }

    ShaderProgram* generatedTileMat = tileDiffTex ? texturedMaterial.get() : simpleMaterial.get();

    if (generatedTileMat)
    {
      generatedTileMat->bind();

      // Set uniforms.
      glUniformMatrix4fv(generatedTileMat->getUniformLocation("viewProj"), 1, GL_FALSE, glm::value_ptr(viewProj));
      glUniform3fv(generatedTileMat->getUniformLocation("viewPos"), 1, glm::value_ptr(cameraPos));

      glUniform1i(generatedTileMat->getUniformLocation("tex"), 0);
      if (tileDiffTex)
      {
        glActiveTexture(GL_TEXTURE0);
        tileDiffTex->bind();
      }

      glm::mat4 model = glm::mat4(1.f);
      glUniformMatrix4fv(generatedTileMat->getUniformLocation("model"), 1, GL_FALSE, glm::value_ptr(model));

      // TODO: send this indirectly!
      GLuint numGenerated = generatedMesh->getNumGeneratedElements();

      // Render the generated mesh.
      glBeginQuery(GL_TIME_ELAPSED, s_glQueries[(int)GLQuery::TilemeshRenderTime]);
      if (s_bDrawReferenceImplementation && curTarget && curTile)
      {
        int numSurfaceTris = curTarget->numTriangles();
        int maxSurfaceIndices = curTile->getNumIndices() * 4 * numSurfaceTris;

        //generatedMesh->draw(maxSurfaceIndices);
        generatedMesh->draw(numGenerated);
      }
      glEndQuery(GL_TIME_ELAPSED);
    }

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);


    ImGui::Render();

    if (!s_bTakeScreenshot)
    {
      ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    // Analyze framedata.
    GLuint64 computeTime = 0;
    GLuint64 tesselationTris = 0;
    GLuint64 tesselationRenderTime = 0;
    GLuint64 tilemeshRenderTime = 0;
    glGetQueryObjectui64v(s_glQueries[(int)GLQuery::ComputeTime], GL_QUERY_RESULT, &computeTime);
    glGetQueryObjectui64v(s_glQueries[(int)GLQuery::TessellationTriangles], GL_QUERY_RESULT, &tesselationTris);
    glGetQueryObjectui64v(s_glQueries[(int)GLQuery::TessellationRenderTime], GL_QUERY_RESULT, &tesselationRenderTime);
    glGetQueryObjectui64v(s_glQueries[(int)GLQuery::TilemeshRenderTime], GL_QUERY_RESULT, &tilemeshRenderTime);


    GLuint64 totalTime = computeTime + tesselationRenderTime + tilemeshRenderTime;

    // Shift frametime over.
    for (int i = 0; i < IM_ARRAYSIZE(s_frameTimes) - 1; i++)
      s_frameTimes[i] = s_frameTimes[i + 1];

    // Nanoseconds to seconds.
    s_frameTimes[IM_ARRAYSIZE(s_frameTimes) - 1] = (double)totalTime * 1e-9;
    if (s_bRecordingFrametime)
    {
      s_statsFrametime.AddData({ std::to_string(computeTime), std::to_string(tesselationRenderTime), std::to_string(tilemeshRenderTime) });
    }

    s_nTrianglesOnScreen = 0;
    if (s_bDrawReferenceImplementation)
    {
      s_nTrianglesOnScreen += generatedMesh->getNumGeneratedElements() / 3;
    }
    if (s_bDrawTessellatedMesh)
    {
      s_nTrianglesOnScreen += tesselationTris;
    }


    if (s_bTakeScreenshot)
    {
      s_bTakeScreenshot = false;
      saveScreenshot(window);
    }

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  glDeleteQueries((int)GLQuery::Max, s_glQueries);

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();

  ImPlot::DestroyContext();
  ImGui::DestroyContext();

  glfwDestroyWindow(window);
  glfwTerminate();
}

static void generateSurfaceGeometry(const TargetMesh& target, const TileMesh& tile)
{
  generatedMesh->reset();

  target.bindGeometryStream(0);
  tile.bindGeometryStreams(1, 2);

  generatedMesh->bind(3, 4);

  const int threadgroupSize = getThreadgroupSize(s_threadgroupSize);
  // Run the compute shader.
  int numThreads = target.numTriangles() * (tile.getNumIndices() / 3);
  int numWorkgroupsX = (numThreads + (threadgroupSize - 1)) / threadgroupSize;

  //int numWorkgroupsX = tile.getNumIndices() / 3;
  //int numWorkgroupsY = target.numTriangles();

  getTilegenShader(s_bEnableClipping ? ClippingMode::On : ClippingMode::Off
    , s_bSmoothNormals ? NormalMode::Smooth : NormalMode::Flat
    , s_threadgroupSize)->bind();

  glDispatchCompute(numWorkgroupsX, 1, 1);

  // Unbind mesh streams.
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, 0);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, 0);
}

static void drawScene(void)
{
  if (s_curMeshTarget != (int)MeshTarget::Sponza)
    return;

  texturedMaterial->bind();
  //simpleMaterial->bind();

  // Set uniforms.
  glUniformMatrix4fv(texturedMaterial->getUniformLocation("viewProj"), 1, GL_FALSE, glm::value_ptr(viewProj));
  glUniform3fv(texturedMaterial->getUniformLocation("viewPos"), 1, glm::value_ptr(cameraPos));

  //glm::mat4 model = glm::scale(glm::mat4(1.f), glm::vec3(0.01f, 0.01f, 0.01f));
  glm::mat4 model = glm::mat4(1.f);
  glUniformMatrix4fv(texturedMaterial->getUniformLocation("model"), 1, GL_FALSE, glm::value_ptr(model));

  if (s_sponza)
  {
    s_sponza->draw();
  }
}

static void drawLines(Mesh* mesh)
{
  lineMaterial->bind();

  // Set uniforms.
  glUniformMatrix4fv(lineMaterial->getUniformLocation("viewProj"), 1, GL_FALSE, glm::value_ptr(viewProj));
  glUniform3fv(lineMaterial->getUniformLocation("viewPos"), 1, glm::value_ptr(cameraPos));

  glm::mat4 model = glm::mat4(1.f);
  glUniformMatrix4fv(lineMaterial->getUniformLocation("model"), 1, GL_FALSE, glm::value_ptr(model));


  mesh->drawNormalVectors();
}

static std::unique_ptr<Mesh> loadMesh(const std::string& mesh)
{
  std::filesystem::path meshPath = std::filesystem::path(SCENE_DIR) / mesh;
  return std::make_unique<Mesh>(meshPath.string());
}

static std::unique_ptr<TargetMesh> loadTargetMesh(const std::string& mesh)
{
  std::filesystem::path meshPath = std::filesystem::path(SCENE_DIR) / mesh;
  return std::make_unique<TargetMesh>(meshPath.string());
}

static std::unique_ptr<TileMesh> loadTileMesh(const std::string& mesh)
{
  std::filesystem::path meshPath = std::filesystem::path(SCENE_DIR) / mesh;
  return std::make_unique<TileMesh>(meshPath.string());
}

static void loadShaders(void)
{
  {
    std::filesystem::path vertPath = std::filesystem::path(SHADERS_DIR) / "simple.vs";
    std::filesystem::path fragPath = std::filesystem::path(SHADERS_DIR) / "simple.fs";
    std::filesystem::path texturedFragPath = std::filesystem::path(SHADERS_DIR) / "textured.fs";

    std::filesystem::path subdivVertPath = std::filesystem::path(SHADERS_DIR) / "subdiv.vs";
    std::filesystem::path tcsPath = std::filesystem::path(SHADERS_DIR) / "subdiv.tcs";
    std::filesystem::path tevPath = std::filesystem::path(SHADERS_DIR) / "subdiv.tev";
    std::filesystem::path lineVertPath = std::filesystem::path(SHADERS_DIR) / "line.vs";
    std::filesystem::path lineFragPath = std::filesystem::path(SHADERS_DIR) / "line.fs";

    // these are destructed when the function exits.
    Shader vert(GL_VERTEX_SHADER, vertPath.string());
    Shader frag(GL_FRAGMENT_SHADER, fragPath.string());

    std::vector<Shader*> progs = { &vert, &frag };
    simpleMaterial = std::make_unique<ShaderProgram>(progs);

    Shader subdivVert(GL_VERTEX_SHADER, subdivVertPath.string());
    Shader tev(GL_TESS_EVALUATION_SHADER, tevPath.string());

    Shader lineVs(GL_VERTEX_SHADER, lineVertPath.string());
    Shader lineFs(GL_FRAGMENT_SHADER, lineFragPath.string());

    Shader texturedFrag(GL_FRAGMENT_SHADER, texturedFragPath.string());

    progs = { &vert, &texturedFrag };
    texturedMaterial = std::make_unique<ShaderProgram>(progs);

    progs = { &lineVs, &lineFs };
    lineMaterial = std::make_unique<ShaderProgram>(progs);

    for (int iSubdivLevel = 0; iSubdivLevel < (int)SubdivLevel::Count; ++iSubdivLevel)
    {
      Shader::DefinesList defines;

      defines.push_back({ "TESS_LEVEL", std::to_string(getSubdivLevel((SubdivLevel)iSubdivLevel)) });

      Shader tcs(GL_TESS_CONTROL_SHADER, tcsPath.string(), defines);

      progs = { &subdivVert, &tcs, &tev, &frag };
      subdivMaterials[iSubdivLevel] = std::make_unique<ShaderProgram>(progs);

      progs = { &subdivVert, &tcs, &tev, &texturedFrag };
      texturedSubdivMaterials[iSubdivLevel] = std::make_unique<ShaderProgram>(progs);
    }
  }

  for (int threadgroupSizeEnum = 0; threadgroupSizeEnum < (int)ThreadgroupSize::Max; threadgroupSizeEnum++)
  for (int normalMode = 0; normalMode < (int)NormalMode::Max; normalMode++)
  for (int clipMode = 0; clipMode < (int)ClippingMode::Max; clipMode++)
  {
    std::filesystem::path csPath = std::filesystem::path(SHADERS_DIR) / "tilegen.glsl";

    Shader::DefinesList defines;

    defines.push_back({ "TILE_THREADGROUPS_X", std::to_string(getThreadgroupSize((ThreadgroupSize)threadgroupSizeEnum))});
    defines.push_back({ "ENABLE_CLIPPING", clipMode == 1 ? "1" : "0" });
    defines.push_back({ "SMOOTH_NORMALS", normalMode == 1 ? "1" : "0" });

    // these are destructed when the function exits.
    Shader computeProg(GL_COMPUTE_SHADER, csPath.string(), defines);

    std::vector<Shader*> progs = { &computeProg };
    getTilegenShader((ClippingMode)clipMode, (NormalMode)normalMode, (ThreadgroupSize)threadgroupSizeEnum) = std::make_unique<ShaderProgram>(progs);
  }

}

static void drawUI(GLFWwindow* window, double dt)
{
  int w, h;
  glfwGetWindowSize(window, &w, &h);
  if (w == 0 || h == 0)
    return;

  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSizeConstraints(ImVec2(20, h), ImVec2(w / 2, h));

  ImGui::Begin("Debug", NULL, 0);

  char fpsStr[64];
  //snprintf(fpsStr, sizeof(fpsStr), "FPS %f", 1.0 / dt);
  // snprintf(fpsStr, sizeof(fpsStr), "frame: %.2f ms", dt * 1000);

  const int meanWindow = 128;
  double meanTime = 0;
  for (int i = IM_ARRAYSIZE(s_frameTimes) - meanWindow; i < IM_ARRAYSIZE(s_frameTimes); i++)
    meanTime += s_frameTimes[i];
  meanTime /= meanWindow;
  snprintf(fpsStr, sizeof(fpsStr), "mean frame: %.2f ms (%.2f FPS)", meanTime * 1000, 1.0 / meanTime);
  ImGui::Text(fpsStr);

  snprintf(fpsStr, sizeof(fpsStr), "%d triangles", s_nTrianglesOnScreen);
  ImGui::Text(fpsStr);

  ImGui::Combo("Target Mesh", &s_curMeshTarget, s_meshTargetNames, IM_ARRAYSIZE(s_meshTargetNames));
  ImGui::Combo("Tilemesh", &s_curTilemesh, s_tileMeshNames, IM_ARRAYSIZE(s_tileMeshNames));

  ImGui::Text("Rendering:");
  ImGui::BeginGroup();
  ImGui::Checkbox("Wireframe", &s_bDrawWireframe);
  
  ImGui::Checkbox("Draw Tessellated Mesh", &s_bDrawTessellatedMesh);
  ImGui::Combo("Tessellation Level", &s_subdivLevel, s_subdivLevelNames, IM_ARRAYSIZE(s_subdivLevelNames));

  ImGui::Checkbox("Draw Normal Vectors", &s_bDrawNormalVectors);

  if (ImGui::Button("Generate Tilemesh"))
  {
    s_bOneTimeCompute = true;
  }
  ImGui::Checkbox("Generate per-frame", &s_bComputeReferenceImplementation);

  ImGui::BeginGroup();
  ImGui::Checkbox("Enable Clipping", &s_bEnableClipping);
  ImGui::Checkbox("Interpolate Normals", &s_bSmoothNormals);
  ImGui::Combo("Threadgroup Size", (int*)&s_threadgroupSize, "64\000128\000256\000512\0\0");
  ImGui::EndGroup();

  ImGui::Checkbox("Draw Reference Implementation", &s_bDrawReferenceImplementation);
  ImGui::EndGroup();

  // Recenter the plot every 1000 frames.
  //static int iframe = 0;
  //if ((iframe++ % 1000) == 0)
  //  ImPlot::SetNextAxisToFit(ImAxis_Y1);
  ImPlot::SetNextAxisLimits(ImAxis_Y1, 0.0, 0.01);

  if (ImPlot::BeginPlot("##plot"))
  {
    ImPlot::PlotLine("Frame time:", s_frameTimes, IM_ARRAYSIZE(s_frameTimes), 1.0, 0.0);
    ImPlot::EndPlot();
  }

  if (ImGui::Button(s_bRecordingFrametime ? "Stop Recording" : "Record"))
  {
    s_bRecordingFrametime = !s_bRecordingFrametime;
  }

  if (!s_bRecordingFrametime && s_statsFrametime.HasData())
  {
    if (ImGui::Button("Save Data"))
    {
      s_statsFrametime.Save();
      s_statsFrametime.Clear();
    }
  }

  if (ImGui::Button("Screenshot"))
  {
    s_bTakeScreenshot = true;
  }

  ImGui::End();
}

static void updateInput(GLFWwindow* window, float dt)
{
  const float speed = 10.f * dt;

  glm::vec3 offsetCameraPos = glm::vec3(0, 0, 0);
  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    offsetCameraPos += speed * cameraForward;
  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    offsetCameraPos -= speed * cameraForward;
  if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    offsetCameraPos += speed * cameraRight;
  if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    offsetCameraPos -= speed * cameraRight;
  offsetCameraPos.y = 0;

  if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
    offsetCameraPos.y += speed * 0.5f;

  if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
    offsetCameraPos.y -= speed * 0.5f;

  float scrollSpeed = speed * 0.5f;
  if (glfwGetKey(window, GLFW_KEY_PAGE_UP) == GLFW_PRESS)
    glfwScrollCallback(nullptr, 0, scrollSpeed);
  if (glfwGetKey(window, GLFW_KEY_PAGE_DOWN) == GLFW_PRESS)
    glfwScrollCallback(nullptr, 0, -scrollSpeed);

  cameraCenterPos += offsetCameraPos;

  bool wasDraggingMouse = bDraggingMouse;
  bDraggingMouse = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
  if (bDraggingMouse)
  {
    static double lastx, lasty;
    double mx, my;
    glfwGetCursorPos(window, &mx, &my);

    if (!wasDraggingMouse)
    {
      lastx = mx;
      lasty = my;
    }

    double dx = (mx - lastx) * dt;
    double dy = (my - lasty) * dt;
    cameraOrbitRot += glm::vec3(0, -dx, -dy / 1.5f) * 1.f;
    cameraOrbitRot.z = std::clamp(cameraOrbitRot.z, glm::radians(-89.f), glm::radians(89.f));

    lastx = mx;
    lasty = my;
  }
}

static void updateCamera(GLFWwindow* window)
{
  int w, h;
  glfwGetWindowSize(window, &w, &h);
  if (w == 0 || h == 0)
    return;

  glViewport(0, 0, w, h);

  float aspect = (float)w / (float)h;

  // Camera setup.
  proj = glm::perspective(glm::radians(60.f), aspect, 0.01f, 100.f);

  view = glm::mat4(1.f);
  view = glm::translate(view, glm::vec3(0, 0, cameraDist));
  view = glm::eulerAngleZYX(cameraOrbitRot.x, cameraOrbitRot.y, cameraOrbitRot.z) * view;
  //view = glm::translate(view, cameraCenterPos);
  view[3] += glm::vec4(cameraCenterPos, 0);

  viewProj = proj * glm::inverse(view);

  cameraPos = glm::vec3(view[3]);
  cameraForward = glm::vec3(view * glm::vec4(0, 0, -1, 0));
  cameraRight = glm::vec3(view * glm::vec4(1, 0, 0, 0));
}

static void saveScreenshot(GLFWwindow* window)
{
  int w, h;
  glfwGetWindowSize(window, &w, &h);
  if (w == 0 || h == 0)
    return;

  std::string screenshotDir = (std::filesystem::path(SCENE_DIR) / "../screenshots").string();
  if (!std::filesystem::is_directory(screenshotDir))
  {
    std::filesystem::create_directory(screenshotDir);
  }

  std::string path;
  int index = 0;
  do {
    path = (std::filesystem::path(screenshotDir) / std::format("img_{}.png", index++)).string();
  } while (std::filesystem::exists(path));

  void* pixBuf = malloc(w * h * 4);
  glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixBuf);

  int pitch = w * 4;
  stbi_flip_vertically_on_write(true);
  stbi_write_png(path.c_str(), w, h, 4, pixBuf, pitch);
  free(pixBuf);

  std::cout << "Saved screenshot to: " << path << "\n";
}