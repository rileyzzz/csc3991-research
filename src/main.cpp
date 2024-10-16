#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <filesystem>
#include <memory>
#include "mesh.h"
#include "shader.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/euler_angles.hpp>

static glm::mat4 proj;
static glm::mat4 viewProj;
static glm::vec3 cameraPos;
static glm::vec3 cameraRot;
static float cameraDist = 2.f;

static std::unique_ptr<ShaderProgram> simpleMaterial;

static std::unique_ptr<Mesh> loadMesh(const std::string& mesh);
static void loadShaders();
static void updateCamera(GLFWwindow* window);

void glfwErrorCallback(int error, const char* description)
{
  fprintf(stderr, "Error: %s\n", description);
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

  // Make the context current.
  glfwMakeContextCurrent(window);

  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
  {
    std::cout << "Failed to initialize GLAD.\n";
    return -1;
  }

  // Load shaders.
  loadShaders();

  auto monkey = loadMesh("monkey.obj");

  // Setup.
  glEnable(GL_DEPTH_TEST);

  // Main loop.
  for (;;)
  {
    if (glfwWindowShouldClose(window))
      break;

    updateCamera(window);

    glClearColor(0.3f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    simpleMaterial->bind();

    // Set uniforms.
    glUniformMatrix4fv(simpleMaterial->getUniformLocation("viewProj"), 1, GL_FALSE, glm::value_ptr(viewProj));

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

static void loadShaders()
{
  std::filesystem::path vertPath = std::filesystem::path(SHADERS_DIR) / "simple.vs";
  std::filesystem::path fragPath = std::filesystem::path(SHADERS_DIR) / "simple.fs";

  // these are destructed when the function exits.
  Shader vert(GL_VERTEX_SHADER, vertPath.string());
  Shader frag(GL_FRAGMENT_SHADER, fragPath.string());

  std::vector<Shader*> progs = { &vert, &frag };
  simpleMaterial = std::make_unique<ShaderProgram>(progs);
}

static void updateCamera(GLFWwindow* window)
{
  int w, h;
  glfwGetWindowSize(window, &w, &h);
  glViewport(0, 0, w, h);

  float aspect = (float)w / (float)h;

  // Camera setup.
  proj = glm::perspective(80.f, aspect, 0.1f, 100.f);

  glm::mat4 view = glm::mat4(1.f);
  view = glm::translate(view, glm::vec3(0, 0, cameraDist));
  view = glm::eulerAngleXYZ(cameraRot.x, cameraRot.y, cameraRot.z) * view;
  view = glm::translate(view, cameraPos);

  viewProj = proj * glm::inverse(view);
}