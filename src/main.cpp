#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <filesystem>
#include <memory>
#include "mesh.h"

static std::unique_ptr<Mesh> loadMesh(const std::string& mesh);

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

  auto monkey = loadMesh("monkey.obj");

  // Main loop.
  for (;;)
  {
    if (glfwWindowShouldClose(window))
      break;

    glClearColor(0.3f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // monkey->draw();

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