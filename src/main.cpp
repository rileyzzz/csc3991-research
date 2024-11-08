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
static std::unique_ptr<ShaderProgram> tilegen;
static std::unique_ptr<Texture> dispTex;
static std::unique_ptr<GPUMeshStreams> generatedMesh;
//static std::unique_ptr<StorageBuffer> outputVertices;
//static std::unique_ptr<StorageBuffer> outputIndices;

static std::unique_ptr<Mesh> loadMesh(const std::string& mesh);
static std::unique_ptr<TargetMesh> loadTargetMesh(const std::string& mesh);
static std::unique_ptr<TileMesh> loadTileMesh(const std::string& mesh);
static void loadShaders(void);
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

  // Load shaders.
  loadShaders();

  auto target = loadTargetMesh("cube_simple.obj");
  auto tile = loadTileMesh("tile_brick.obj");
  auto monkey = loadMesh("cube.obj");

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

    updateCamera(window);
    updateInput(window, dt);

    glClearColor(0.3f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Compute phase.
    generateSurfaceGeometry(*target, *tile);

    simpleMaterial->bind();

    // Set uniforms.
    glUniformMatrix4fv(simpleMaterial->getUniformLocation("viewProj"), 1, GL_FALSE, glm::value_ptr(viewProj));
    glUniform3fv(simpleMaterial->getUniformLocation("viewPos"), 1, glm::value_ptr(cameraPos));
    
    //glUniform1i(simpleMaterial->getUniformLocation("displacement"), 0);

    glActiveTexture(GL_TEXTURE0);
    dispTex->bind();

    // monkey->draw();

    // Render the generated mesh.
    int numSurfaceTris = target->numTriangles();
    generatedMesh->draw(tile->getNumIndices() * 4 * numSurfaceTris);

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  glfwDestroyWindow(window);
  glfwTerminate();
}

static void generateSurfaceGeometry(const TargetMesh& target, const TileMesh& tile)
{
  target.bindGeometryStream(0);
  tile.bindGeometryStreams(1, 2);

  generatedMesh->bind(3, 4);

  const int threadgroupSize = 64;
  // Run the compute shader.
  int numWorkgroupsX = (target.numTriangles() + (threadgroupSize - 1)) / threadgroupSize;

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

    // these are destructed when the function exits.
    Shader vert(GL_VERTEX_SHADER, vertPath.string());
    Shader frag(GL_FRAGMENT_SHADER, fragPath.string());

    std::vector<Shader*> progs = { &vert, &frag };
    simpleMaterial = std::make_unique<ShaderProgram>(progs);
  }

  {
    std::filesystem::path csPath = std::filesystem::path(SHADERS_DIR) / "tilegen.glsl";

    // these are destructed when the function exits.
    Shader computeProg(GL_COMPUTE_SHADER, csPath.string());

    std::vector<Shader*> progs = { &computeProg };
    tilegen = std::make_unique<ShaderProgram>(progs);
  }

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