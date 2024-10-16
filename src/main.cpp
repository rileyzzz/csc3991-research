#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <filesystem>
#include <memory>
#include "mesh.h"
#include "shader.h"
#include "texture.h"

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
static std::unique_ptr<Texture> dispTex;

static std::unique_ptr<Mesh> loadMesh(const std::string& mesh);
static void loadShaders(void);
static void updateInput(GLFWwindow* window, float dt);
static void updateCamera(GLFWwindow* window);

void glfwErrorCallback(int error, const char* description)
{
  fprintf(stderr, "Error: %s\n", description);
}

void glfwScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
  cameraDist -= yoffset;
  cameraDist = std::clamp(cameraDist, 1.f, 10.f);
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

  // Load shaders.
  loadShaders();

  auto monkey = loadMesh("cube_high.obj");
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

    float time = glfwGetTime();
    float dt = time - lastTime;
    lastTime = time;

    updateCamera(window);
    updateInput(window, dt);

    glClearColor(0.3f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    simpleMaterial->bind();

    // Set uniforms.
    glUniformMatrix4fv(simpleMaterial->getUniformLocation("viewProj"), 1, GL_FALSE, glm::value_ptr(viewProj));
    glUniform3fv(simpleMaterial->getUniformLocation("viewPos"), 1, glm::value_ptr(cameraPos));
    
    //glUniform1i(simpleMaterial->getUniformLocation("displacement"), 0);

    glActiveTexture(GL_TEXTURE0);
    dispTex->bind();

    monkey->draw();

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  glfwDestroyWindow(window);
  glfwTerminate();
}

static std::unique_ptr<Mesh> loadMesh(const std::string& mesh)
{
  std::filesystem::path meshPath = std::filesystem::path(SCENE_DIR) / mesh;

  return std::make_unique<Mesh>(meshPath.string());
}

static void loadShaders(void)
{
  std::filesystem::path vertPath = std::filesystem::path(SHADERS_DIR) / "simple.vs";
  std::filesystem::path fragPath = std::filesystem::path(SHADERS_DIR) / "simple.fs";

  // these are destructed when the function exits.
  Shader vert(GL_VERTEX_SHADER, vertPath.string());
  Shader frag(GL_FRAGMENT_SHADER, fragPath.string());

  std::vector<Shader*> progs = { &vert, &frag };
  simpleMaterial = std::make_unique<ShaderProgram>(progs);
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

    double dx = mx - lastx;
    double dy = my - lasty;
    cameraOrbitRot += glm::vec3(0, -dx, -dy / 1.5f) * 10.f * dt;
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