#define GLFW_INCLUDE_NONE
#define STB_IMAGE_IMPLEMENTATION

#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <stb_image.h>

#include <array>
#include <cstddef>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

#include "module/Camera.hpp"
#include "module/Shader.hpp"

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);

// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

// camera
Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

glm::vec3 lightPos(1.2f, 1.0f, 2.0f);

struct MaterialPreset {
  const char* name;
  glm::vec3 ambient;
  glm::vec3 diffuse;
  glm::vec3 specular;
  float shininess;
};

const std::array<MaterialPreset, 24> materials = {{
    {"emerald",
     {0.0215f, 0.1745f, 0.0215f},
     {0.07568f, 0.61424f, 0.07568f},
     {0.633f, 0.727811f, 0.633f},
     0.6f},
    {"jade",
     {0.135f, 0.2225f, 0.1575f},
     {0.54f, 0.89f, 0.63f},
     {0.316228f, 0.316228f, 0.316228f},
     0.1f},
    {"obsidian",
     {0.05375f, 0.05f, 0.06625f},
     {0.18275f, 0.17f, 0.22525f},
     {0.332741f, 0.328634f, 0.346435f},
     0.3f},
    {"pearl",
     {0.25f, 0.20725f, 0.20725f},
     {1.0f, 0.829f, 0.829f},
     {0.296648f, 0.296648f, 0.296648f},
     0.088f},
    {"ruby",
     {0.1745f, 0.01175f, 0.01175f},
     {0.61424f, 0.04136f, 0.04136f},
     {0.727811f, 0.626959f, 0.626959f},
     0.6f},
    {"turquoise",
     {0.1f, 0.18725f, 0.1745f},
     {0.396f, 0.74151f, 0.69102f},
     {0.297254f, 0.30829f, 0.306678f},
     0.1f},
    {"brass",
     {0.329412f, 0.223529f, 0.027451f},
     {0.780392f, 0.568627f, 0.113725f},
     {0.992157f, 0.941176f, 0.807843f},
     0.21794872f},
    {"bronze",
     {0.2125f, 0.1275f, 0.054f},
     {0.714f, 0.4284f, 0.18144f},
     {0.393548f, 0.271906f, 0.166721f},
     0.2f},
    {"chrome",
     {0.25f, 0.25f, 0.25f},
     {0.4f, 0.4f, 0.4f},
     {0.774597f, 0.774597f, 0.774597f},
     0.6f},
    {"copper",
     {0.19125f, 0.0735f, 0.0225f},
     {0.7038f, 0.27048f, 0.0828f},
     {0.256777f, 0.137622f, 0.086014f},
     0.1f},
    {"gold",
     {0.24725f, 0.1995f, 0.0745f},
     {0.75164f, 0.60648f, 0.22648f},
     {0.628281f, 0.555802f, 0.366065f},
     0.4f},
    {"silver",
     {0.19225f, 0.19225f, 0.19225f},
     {0.50754f, 0.50754f, 0.50754f},
     {0.508273f, 0.508273f, 0.508273f},
     0.4f},
    {"black plastic",
     {0.0f, 0.0f, 0.0f},
     {0.01f, 0.01f, 0.01f},
     {0.5f, 0.5f, 0.5f},
     0.25f},
    {"cyan plastic",
     {0.0f, 0.1f, 0.06f},
     {0.0f, 0.50980392f, 0.50980392f},
     {0.50196078f, 0.50196078f, 0.50196078f},
     0.25f},
    {"green plastic",
     {0.0f, 0.0f, 0.0f},
     {0.1f, 0.35f, 0.1f},
     {0.45f, 0.55f, 0.45f},
     0.25f},
    {"red plastic",
     {0.0f, 0.0f, 0.0f},
     {0.5f, 0.0f, 0.0f},
     {0.7f, 0.6f, 0.6f},
     0.25f},
    {"white plastic",
     {0.0f, 0.0f, 0.0f},
     {0.55f, 0.55f, 0.55f},
     {0.7f, 0.7f, 0.7f},
     0.25f},
    {"yellow plastic",
     {0.0f, 0.0f, 0.0f},
     {0.5f, 0.5f, 0.0f},
     {0.6f, 0.6f, 0.5f},
     0.25f},
    {"black rubber",
     {0.02f, 0.02f, 0.02f},
     {0.01f, 0.01f, 0.01f},
     {0.4f, 0.4f, 0.4f},
     0.078125f},
    {"cyan rubber",
     {0.0f, 0.05f, 0.05f},
     {0.4f, 0.5f, 0.5f},
     {0.04f, 0.7f, 0.7f},
     0.078125f},
    {"green rubber",
     {0.0f, 0.05f, 0.0f},
     {0.4f, 0.5f, 0.4f},
     {0.04f, 0.7f, 0.04f},
     0.078125f},
    {"red rubber",
     {0.05f, 0.0f, 0.0f},
     {0.5f, 0.4f, 0.4f},
     {0.7f, 0.04f, 0.04f},
     0.078125f},
    {"white rubber",
     {0.05f, 0.05f, 0.05f},
     {0.5f, 0.5f, 0.5f},
     {0.7f, 0.7f, 0.7f},
     0.078125f},
    {"yellow rubber",
     {0.05f, 0.05f, 0.0f},
     {0.5f, 0.5f, 0.4f},
     {0.7f, 0.7f, 0.04f},
     0.078125f},
}};

std::size_t selectedMaterial = 0;
bool wasLeftMousePressed = false;

int main() {
  // glfw: initialize and configure
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

#ifdef __APPLE__
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

  // glfw window creation
  // --------------------
  GLFWwindow* window =
      glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL", NULL, NULL);
  if (window == NULL) {
    std::cout << "Failed to create GLFW window" << std::endl;
    glfwTerminate();
    return -1;
  }
  glfwMakeContextCurrent(window);
  glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
  glfwSetCursorPosCallback(window, mouse_callback);
  glfwSetScrollCallback(window, scroll_callback);

  // tell GLFW to capture our mouse
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

  // glad: load all OpenGL function pointers
  // ---------------------------------------
  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    std::cout << "Failed to initialize GLAD" << std::endl;
    return -1;
  }

  // configure global opengl state
  glEnable(GL_DEPTH_TEST);

  std::cout << "Material: " << materials[selectedMaterial].name << std::endl;

  // build and compile our shader program
  Shader lightShader(
      "shaders/shader-light-material.vs", "shaders/shader-light-material.fs");

  Shader lightCubeShader(
      "shaders/shader-light-cube.vs", "shaders/shader-light-cube.fs");

  // clang-format off
  // set up vertex data (and buffers) and configure vertex attributes
  float vertices[] = {
      -0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f,
       0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f,
       0.5f,  0.5f, -0.5f, 0.0f, 0.0f, -1.0f,
       0.5f,  0.5f, -0.5f, 0.0f, 0.0f, -1.0f,
      -0.5f,  0.5f, -0.5f, 0.0f, 0.0f, -1.0f,
      -0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f,

      -0.5f, -0.5f,  0.5f, 0.0f, 0.0f, 1.0f,
       0.5f, -0.5f,  0.5f, 0.0f, 0.0f, 1.0f,
       0.5f,  0.5f,  0.5f, 0.0f, 0.0f, 1.0f,
       0.5f,  0.5f,  0.5f, 0.0f, 0.0f, 1.0f,
      -0.5f,  0.5f,  0.5f, 0.0f, 0.0f, 1.0f,
      -0.5f, -0.5f,  0.5f, 0.0f, 0.0f, 1.0f,

      -0.5f,  0.5f,  0.5f, -1.0f, 0.0f, 0.0f,
      -0.5f,  0.5f, -0.5f, -1.0f, 0.0f, 0.0f,
      -0.5f, -0.5f, -0.5f, -1.0f, 0.0f, 0.0f,
      -0.5f, -0.5f, -0.5f, -1.0f, 0.0f, 0.0f,
      -0.5f, -0.5f,  0.5f, -1.0f, 0.0f, 0.0f,
      -0.5f , 0.5f,  0.5f, -1.0f, 0.0f, 0.0f,

       0.5f,  0.5f,  0.5f, 1.0f, 0.0f, 0.0f,
       0.5f,  0.5f, -0.5f, 1.0f, 0.0f, 0.0f,
       0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f,
       0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f,
       0.5f, -0.5f,  0.5f, 1.0f, 0.0f, 0.0f,
       0.5f,  0.5f,  0.5f, 1.0f, 0.0f, 0.0f,

      -0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f,
       0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f,
       0.5f, -0.5f,  0.5f, 0.0f, -1.0f, 0.0f,
       0.5f, -0.5f,  0.5f, 0.0f, -1.0f, 0.0f,
      -0.5f, -0.5f,  0.5f, 0.0f, -1.0f, 0.0f,
      -0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f,

      -0.5f,  0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
       0.5f,  0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
       0.5f,  0.5f,  0.5f, 0.0f, 1.0f, 0.0f,
       0.5f,  0.5f,  0.5f, 0.0f, 1.0f, 0.0f,
      -0.5f,  0.5f,  0.5f, 0.0f, 1.0f, 0.0f,
      -0.5f,  0.5f, -0.5f, 0.0f, 1.0f, 0.0f
  };
  // clang-format on

  unsigned int VBO;
  glGenBuffers(1, &VBO);
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  unsigned int cubeVAO;
  glGenVertexArrays(1, &cubeVAO);
  glBindVertexArray(cubeVAO);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(
      1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);

  unsigned int lightCubeVAO;
  glGenVertexArrays(1, &lightCubeVAO);
  glBindVertexArray(lightCubeVAO);

  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(0);

  while (!glfwWindowShouldClose(window)) {
    // per-frame time logic
    float currentFrame = static_cast<float>(glfwGetTime());
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;

    // float orbitRadius = 1.5f;
    // float orbitSpeed = 0.8f;
    // float angle = currentFrame * orbitSpeed;
    // lightPos = glm::vec3(
    //     std::cos(angle) * orbitRadius, 0.0f, std::sin(angle) * orbitRadius);

    // input
    processInput(window);

    // render
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(
        GL_COLOR_BUFFER_BIT |
        GL_DEPTH_BUFFER_BIT);  // also clear the depth buffer now!

    lightShader.use();
    lightShader.setVec3("light.position", lightPos);
    lightShader.setVec3("viewPos", camera.Position);

    // light properties
    glm::vec3 ambientColor(0.35f);
    glm::vec3 diffuseColor(0.8f);
    lightShader.setVec3("light.ambient", ambientColor);
    lightShader.setVec3("light.diffuse", diffuseColor);
    lightShader.setVec3("light.specular", glm::vec3(1.0f));

    // material properties
    const MaterialPreset& material = materials[selectedMaterial];
    lightShader.setVec3("material.ambient", material.ambient);
    lightShader.setVec3("material.diffuse", material.diffuse);
    lightShader.setVec3("material.specular", material.specular);
    lightShader.setFloat("material.shininess", material.shininess * 128.0f);

    // projection matrix
    glm::mat4 projection = glm::perspective(
        glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f,
        100.0f);
    lightShader.setMat4("projection", projection);

    // view matrix
    glm::mat4 view = camera.GetViewMatrix();
    lightShader.setMat4("view", view);

    // model matrix
    glm::mat4 model = glm::mat4(1.0f);
    lightShader.setMat4("model", model);

    // render boxes
    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    lightCubeShader.use();
    lightCubeShader.setMat4("projection", projection);
    lightCubeShader.setMat4("view", view);
    model = glm::mat4(1.0f);
    model = glm::translate(model, lightPos);
    model = glm::scale(model, glm::vec3(0.2f));
    lightCubeShader.setMat4("model", model);

    glBindVertexArray(lightCubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  glDeleteVertexArrays(1, &cubeVAO);
  glDeleteVertexArrays(1, &lightCubeVAO);
  glDeleteBuffers(1, &VBO);

  glfwTerminate();
  return 0;
}

void processInput(GLFWwindow* window) {
  if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    glfwSetWindowShouldClose(window, true);

  float cameraSpeed = static_cast<float>(2.5 * deltaTime);
  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    camera.ProcessKeyboard(FORWARD, deltaTime);
  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    camera.ProcessKeyboard(BACKWARD, deltaTime);
  if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    camera.ProcessKeyboard(LEFT, deltaTime);
  if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    camera.ProcessKeyboard(RIGHT, deltaTime);

  bool isLeftMousePressed =
      glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
  if (isLeftMousePressed && !wasLeftMousePressed) {
    selectedMaterial = (selectedMaterial + 1) % materials.size();
    std::cout << "Material: " << materials[selectedMaterial].name << std::endl;
  }
  wasLeftMousePressed = isLeftMousePressed;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
  glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn) {
  float xpos = static_cast<float>(xposIn);
  float ypos = static_cast<float>(yposIn);

  if (firstMouse) {
    lastX = xpos;
    lastY = ypos;
    firstMouse = false;
  }

  float xoffset = xpos - lastX;
  float yoffset =
      lastY - ypos;  // reversed since y-coordinates go from bottom to top
  lastX = xpos;
  lastY = ypos;

  camera.ProcessMouseMovement(xoffset, yoffset);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
  camera.ProcessMouseScroll(static_cast<float>(yoffset));
}
