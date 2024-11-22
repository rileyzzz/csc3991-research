#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <filesystem>
#include <memory>
#include "mesh.h"
#include "shader.h"
#include "texture.h"
#include "buffer.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/euler_angles.hpp>

// ImGUI.
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

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

static std::unique_ptr<ShaderProgram> simpleMaterial;
static std::unique_ptr<ShaderProgram> subdivMaterial;
static std::unique_ptr<ShaderProgram> tilegen;
static std::unique_ptr<Texture> dispTex;
static std::unique_ptr<GPUMeshStreams> generatedMesh;
//static std::unique_ptr<StorageBuffer> outputVertices;
//static std::unique_ptr<StorageBuffer> outputIndices;

static bool s_bDrawWireframe = false;
static bool s_bDrawTessellatedMesh = true;
static bool s_bComputeReferenceImplementation = false;
static bool s_bDrawReferenceImplementation = false;

static std::unique_ptr<Mesh> loadMesh(const std::string& mesh);
static std::unique_ptr<TargetMesh> loadTargetMesh(const std::string& mesh);
static std::unique_ptr<TileMesh> loadTileMesh(const std::string& mesh);
static void loadShaders(void);
static void drawUI(GLFWwindow* window, double dt);
static void updateInput(GLFWwindow* window, float dt);
static void updateCamera(GLFWwindow* window);

static void generateSurfaceGeometry(const TargetMesh& target, const TileMesh& tile);

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
  if (id == 131169 || id == 131185 || id == 131218 || id == 131204) return;

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

  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init();

  // Load shaders.
  loadShaders();

  auto target = loadTargetMesh("cube_simple.obj");
  //auto target = loadTargetMesh("test_ico.obj");
  //auto target = loadTargetMesh("test_torus.obj");
  auto tile = loadTileMesh("tile_brick.obj");
  //auto monkey = loadMesh("cube.obj");
  auto monkey = loadMesh("cube_simple.obj");

  const int maxVertices = 1024 * 128 * 12;
  const int maxIndices = 3 * maxVertices;
  generatedMesh = std::make_unique<GPUMeshStreams>(maxVertices, maxIndices);

  //auto monkey = loadMesh("monkey_high.obj");

  dispTex = std::make_unique<Texture>((std::filesystem::path(SCENE_DIR) / "brick.jpg").string());

  // Setup.
  glEnable(GL_DEPTH_TEST);

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

    // Compute phase.
    if (s_bComputeReferenceImplementation)
    {
      generateSurfaceGeometry(*target, *tile);
    }

    subdivMaterial->bind();

    // Set uniforms.
    glUniformMatrix4fv(subdivMaterial->getUniformLocation("viewProj"), 1, GL_FALSE, glm::value_ptr(viewProj));
    glUniform3fv(subdivMaterial->getUniformLocation("viewPos"), 1, glm::value_ptr(cameraPos));

    glActiveTexture(GL_TEXTURE0);
    dispTex->bind();

    if (s_bDrawTessellatedMesh)
    {
      monkey->drawPatches();
    }

    simpleMaterial->bind();

    // Set uniforms.
    glUniformMatrix4fv(simpleMaterial->getUniformLocation("viewProj"), 1, GL_FALSE, glm::value_ptr(viewProj));
    glUniform3fv(simpleMaterial->getUniformLocation("viewPos"), 1, glm::value_ptr(cameraPos));

    glActiveTexture(GL_TEXTURE0);
    dispTex->bind();

    // Render the generated mesh.
    if (s_bDrawReferenceImplementation)
    {
      int numSurfaceTris = target->numTriangles();
      generatedMesh->draw(tile->getNumIndices() * 4 * numSurfaceTris);
    }

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
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

  const int threadgroupSize = 512;
  // Run the compute shader.
  int numThreads = target.numTriangles() * (tile.getNumIndices() / 3);
  int numWorkgroupsX = (numThreads + (threadgroupSize - 1)) / threadgroupSize;

  //int numWorkgroupsX = tile.getNumIndices() / 3;
  //int numWorkgroupsY = target.numTriangles();

  tilegen->bind();
  glDispatchCompute(numWorkgroupsX, 1, 1);

  // Unbind mesh streams.
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, 0);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, 0);
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

    std::filesystem::path subdivVertPath = std::filesystem::path(SHADERS_DIR) / "subdiv.vs";
    std::filesystem::path tcsPath = std::filesystem::path(SHADERS_DIR) / "subdiv.tcs";
    std::filesystem::path tevPath = std::filesystem::path(SHADERS_DIR) / "subdiv.tev";

    // these are destructed when the function exits.
    Shader vert(GL_VERTEX_SHADER, vertPath.string());
    Shader frag(GL_FRAGMENT_SHADER, fragPath.string());

    std::vector<Shader*> progs = { &vert, &frag };
    simpleMaterial = std::make_unique<ShaderProgram>(progs);

    Shader subdivVert(GL_VERTEX_SHADER, subdivVertPath.string());
    Shader tcs(GL_TESS_CONTROL_SHADER, tcsPath.string());
    Shader tev(GL_TESS_EVALUATION_SHADER, tevPath.string());

    progs = { &subdivVert, &tcs, &tev, &frag };
    subdivMaterial = std::make_unique<ShaderProgram>(progs);
  }

  {
    std::filesystem::path csPath = std::filesystem::path(SHADERS_DIR) / "tilegen.glsl";

    // these are destructed when the function exits.
    Shader computeProg(GL_COMPUTE_SHADER, csPath.string());

    std::vector<Shader*> progs = { &computeProg };
    tilegen = std::make_unique<ShaderProgram>(progs);
  }

}

static void drawUI(GLFWwindow* window, double dt)
{
  int w, h;
  glfwGetWindowSize(window, &w, &h);
  if (w == 0 || h == 0)
    return;

  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(ImVec2(200, h));

  ImGui::Begin("Debug", NULL, 0);

  char fpsStr[32];
  //snprintf(fpsStr, sizeof(fpsStr), "FPS %f", 1.0 / dt);
  snprintf(fpsStr, sizeof(fpsStr), "frame: %.2f ms", dt * 1000);
  ImGui::Text(fpsStr);

  ImGui::Text("Rendering:");
  ImGui::BeginGroup();
  ImGui::Checkbox("Wireframe", &s_bDrawWireframe);
  ImGui::Checkbox("Draw Tessellated Mesh", &s_bDrawTessellatedMesh);
  ImGui::Checkbox("Compute Reference Implementation", &s_bComputeReferenceImplementation);
  ImGui::Checkbox("Draw Reference Implementation", &s_bDrawReferenceImplementation);
  ImGui::EndGroup();

  
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
  proj = glm::perspective(glm::radians(60.f), aspect, 0.01f, 10.f);

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