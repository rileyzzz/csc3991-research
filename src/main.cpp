#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <filesystem>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

static void loadMesh(const std::string& mesh);

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

  loadMesh("monkey.obj");

  // Main loop.
  for (;;)
  {
    if (glfwWindowShouldClose(window))
      break;

  }

  glfwDestroyWindow(window);
  glfwTerminate();
}

static void loadMesh(const std::string& mesh)
{
  std::filesystem::path meshPath = std::filesystem::path(SCENE_DIR) / mesh;

  tinyobj::ObjReaderConfig reader_config;
  //reader_config.mtl_search_path = "./"; // Path to material files

  tinyobj::ObjReader reader;

  if (!reader.ParseFromFile(meshPath.string(), reader_config))
  {
    if (!reader.Error().empty())
    {
      std::cerr << "TinyObjReader: " << reader.Error();
    }
    exit(-1);
  }

  if (!reader.Warning().empty())
  {
    std::cout << "TinyObjReader: " << reader.Warning();
  }
}