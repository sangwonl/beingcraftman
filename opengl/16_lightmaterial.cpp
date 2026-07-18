#define GLFW_INCLUDE_NONE
#define STB_IMAGE_IMPLEMENTATION

#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <stb_image.h>

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

    float orbitRadius = 1.5f;
    float orbitSpeed = 0.8f;
    float angle = currentFrame * orbitSpeed;
    lightPos = glm::vec3(
        std::cos(angle) * orbitRadius, 0.0f, std::sin(angle) * orbitRadius);

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
    glm::vec3 lightColor;
    lightColor.x = std::sin(currentFrame * 2.0f);
    lightColor.y = std::sin(currentFrame * 0.7f);
    lightColor.z = std::sin(currentFrame * 1.3f);
    glm::vec3 diffuseColor =
        lightColor * glm::vec3(0.5f);  // decrease the influence
    glm::vec3 ambientColor = diffuseColor * glm::vec3(0.2f);  // low influence
    lightShader.setVec3("light.ambient", ambientColor);
    lightShader.setVec3("light.diffuse", diffuseColor);
    lightShader.setVec3("light.specular", glm::vec3(1.0f, 1.0f, 1.0f));

    // material properties
    lightShader.setVec3("material.ambient", 1.0f, 0.5f, 0.31f);
    lightShader.setVec3("material.diffuse", 1.0f, 0.5f, 0.31f);
    lightShader.setVec3(
        "material.specular", 0.5f, 0.5f,
        0.5f);  // specular lighting doesn't have full effect on this object's
                // material
    lightShader.setFloat("material.shininess", 32.0f);

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
