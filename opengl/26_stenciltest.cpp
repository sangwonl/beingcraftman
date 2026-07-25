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
unsigned int loadTexture(char const* path);

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

int main() {
  // glfw: initialize and configure
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
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
  glDepthFunc(GL_LESS);
  glEnable(GL_STENCIL_TEST);
  glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
  glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

  // glDepthFunc(GL_ALWAYS);

  // build and compile our shader program
  Shader shader(
      "shaders/shader-stenciltest.vs", "shaders/shader-stenciltest.fs");
  Shader shaderSingleColor(
      "shaders/shader-stenciltest.vs", "shaders/shader-stencilsinglecolor.fs");

  // clang-format off
  // set up vertex data (and buffers) and configure vertex attributes
  float vertices[] = {
      // positions         // texcoords
      -0.5f, -0.5f, -0.5f, 0.0f, 0.0f,
       0.5f, -0.5f, -0.5f, 1.0f, 0.0f,
       0.5f,  0.5f, -0.5f, 1.0f, 1.0f,
       0.5f,  0.5f, -0.5f, 1.0f, 1.0f,
      -0.5f,  0.5f, -0.5f, 0.0f, 1.0f,
      -0.5f, -0.5f, -0.5f, 0.0f, 0.0f,

      -0.5f, -0.5f,  0.5f, 0.0f, 0.0f,
       0.5f, -0.5f,  0.5f, 1.0f, 0.0f,
       0.5f,  0.5f,  0.5f, 1.0f, 1.0f,
       0.5f,  0.5f,  0.5f, 1.0f, 1.0f,
      -0.5f,  0.5f,  0.5f, 0.0f, 1.0f,
      -0.5f, -0.5f,  0.5f, 0.0f, 0.0f,

      -0.5f,  0.5f,  0.5f, 1.0f, 0.0f,
      -0.5f,  0.5f, -0.5f, 1.0f, 1.0f,
      -0.5f, -0.5f, -0.5f, 0.0f, 1.0f,
      -0.5f, -0.5f, -0.5f, 0.0f, 1.0f,
      -0.5f, -0.5f,  0.5f, 0.0f, 0.0f,
      -0.5f , 0.5f,  0.5f, 1.0f, 0.0f,

       0.5f,  0.5f,  0.5f, 1.0f, 0.0f,
       0.5f,  0.5f, -0.5f, 1.0f, 1.0f,
       0.5f, -0.5f, -0.5f, 0.0f, 1.0f,
       0.5f, -0.5f, -0.5f, 0.0f, 1.0f,
       0.5f, -0.5f,  0.5f, 0.0f, 0.0f,
       0.5f,  0.5f,  0.5f, 1.0f, 0.0f,

      -0.5f, -0.5f, -0.5f, 0.0f, 1.0f,
       0.5f, -0.5f, -0.5f, 1.0f, 1.0f,
       0.5f, -0.5f,  0.5f, 1.0f, 0.0f,
       0.5f, -0.5f,  0.5f, 1.0f, 0.0f,
      -0.5f, -0.5f,  0.5f, 0.0f, 0.0f,
      -0.5f, -0.5f, -0.5f, 0.0f, 1.0f,

      -0.5f,  0.5f, -0.5f, 0.0f, 1.0f,
       0.5f,  0.5f, -0.5f, 1.0f, 1.0f,
       0.5f,  0.5f,  0.5f, 1.0f, 0.0f,
       0.5f,  0.5f,  0.5f, 1.0f, 0.0f,
      -0.5f,  0.5f,  0.5f, 0.0f, 0.0f,
      -0.5f,  0.5f, -0.5f, 0.0f, 1.0f,
  };

  float planeVertices[] = {
    // positions         // texture Coords
     5.0f, -0.5f,  5.0f, 2.0f, 0.0f,
    -5.0f, -0.5f,  5.0f, 0.0f, 0.0f,
    -5.0f, -0.5f, -5.0f, 0.0f, 2.0f,

     5.0f, -0.5f,  5.0f, 2.0f, 0.0f,
    -5.0f, -0.5f, -5.0f, 0.0f, 2.0f,
     5.0f, -0.5f, -5.0f, 2.0f, 2.0f
  };
  // clang-format on

  // cube
  unsigned int cubeVBO;
  glGenBuffers(1, &cubeVBO);
  glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  unsigned int cubeVAO;
  glGenVertexArrays(1, &cubeVAO);
  glBindVertexArray(cubeVAO);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(
      1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);
  glBindVertexArray(0);

  // plane
  unsigned int planeVBO;
  glGenBuffers(1, &planeVBO);
  glBindBuffer(GL_ARRAY_BUFFER, planeVBO);
  glBufferData(
      GL_ARRAY_BUFFER, sizeof(planeVertices), planeVertices, GL_STATIC_DRAW);

  unsigned int planeVAO;
  glGenVertexArrays(1, &planeVAO);
  glBindVertexArray(planeVAO);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(
      1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);
  glBindVertexArray(0);

  unsigned int cubeTexture = loadTexture("resources/textures/marble.jpg");
  unsigned int floorTexture = loadTexture("resources/textures/metal.png");

  shader.use();
  shader.setInt("texture1", 0);

  while (!glfwWindowShouldClose(window)) {
    // per-frame time logic
    float currentFrame = static_cast<float>(glfwGetTime());
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;

    // input
    processInput(window);

    // render
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    glm::mat4 model = glm::mat4(1.0f);
    glm::mat4 view = camera.GetViewMatrix();
    glm::mat4 projection = glm::perspective(
        glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f,
        100.0f);

    shaderSingleColor.use();
    shaderSingleColor.setMat4("view", view);
    shaderSingleColor.setMat4("projection", projection);

    shader.use();
    shader.setMat4("view", view);
    shader.setMat4("projection", projection);

    glStencilMask(0x00);

    // floor
    glBindVertexArray(planeVAO);
    glBindTexture(GL_TEXTURE_2D, floorTexture);
    shader.setMat4("model", glm::mat4(1.0f));
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    // 1st render pass: draw objects as normal, writing to the stencil buffer
    glStencilFunc(GL_ALWAYS, 1, 0xFF);
    glStencilMask(0xFF);

    glBindVertexArray(cubeVAO);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, cubeTexture);

    model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(-1.0f, 0.0f, -1.0f));
    shader.setMat4("model", model);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(2.0f, 0.0f, 0.0f));
    shader.setMat4("model", model);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    // 2nd render pass: now draw slightly scaled versions of the objects
    // this time disabling stencil writing
    glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
    glStencilMask(0x00);
    glDisable(GL_DEPTH_TEST);
    shaderSingleColor.use();

    float scale = 1.1f;

    glBindVertexArray(cubeVAO);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, cubeTexture);

    model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(-1.0f, 0.0f, -1.0f));
    model = glm::scale(model, glm::vec3(scale, scale, scale));
    shader.setMat4("model", model);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(2.0f, 0.0f, 0.0f));
    model = glm::scale(model, glm::vec3(scale, scale, scale));
    shader.setMat4("model", model);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    glBindVertexArray(0);
    glStencilMask(0xFF);
    glStencilFunc(GL_ALWAYS, 0, 0xFF);
    glEnable(GL_DEPTH_TEST);

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  glDeleteVertexArrays(1, &cubeVAO);
  glDeleteVertexArrays(1, &planeVAO);
  glDeleteBuffers(1, &cubeVBO);
  glDeleteBuffers(1, &planeVBO);

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

unsigned int loadTexture(char const* path) {
  unsigned int textureID;
  glGenTextures(1, &textureID);

  int width, height, nrComponents;
  unsigned char* data = stbi_load(path, &width, &height, &nrComponents, 0);
  if (data) {
    GLenum format;
    if (nrComponents == 1)
      format = GL_RED;
    else if (nrComponents == 3)
      format = GL_RGB;
    else if (nrComponents == 4)
      format = GL_RGBA;

    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(
        GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE,
        data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(
        GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);
  } else {
    std::cout << "Texture failed to load at path: " << path << std::endl;
    stbi_image_free(data);
  }

  return textureID;
}