#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "FastNoiseLite.h"
#include <iostream>
#include <vector>
#include <algorithm>

const int WIDTH = 1024, HEIGHT = 768;
const int TERRAIN_SIZE = 64;
const float CUBE_SIZE = 1.0f;

// Camera
glm::vec3 cameraPos = glm::vec3(0.0f, 20.0f, 10.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
bool wireframe = false;
float speedMultiplier = 1.0f;
float yaw = -90.0f, pitch = 0.0f;
float lastX = WIDTH/2, lastY = HEIGHT/2;
bool firstMouse = true;

struct Cube {
    glm::vec3 position;
};

std::vector<Cube> cubes;

const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
out vec3 ourColor;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    ourColor = aColor;
}
)";

const char* fragmentShaderSource = R"(
#version 330 core
in vec3 ourColor;
out vec4 FragColor;
void main() {
    FragColor = vec4(ourColor, 1.0);
}
)";

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; 
    lastX = xpos;
    lastY = ypos;

    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw += xoffset;
    pitch += yoffset;

    if(pitch > 89.0f) pitch = 89.0f;
    if(pitch < -89.0f) pitch = -89.0f;

    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);
}

void generateTerrain() {
     FastNoiseLite noise;
     noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
     noise.SetFrequency(0.03f);              // Lower = broader features
     noise.SetFractalType(FastNoiseLite::FractalType_FBm);
     noise.SetFractalOctaves(4);             // Fewer octaves = smoother slopes
     noise.SetFractalLacunarity(2.0f);       // Controls roughness
     noise.SetFractalGain(0.4f);             // Lower gain = gentler slopes
     
     std::vector<std::vector<int>> heightMap(TERRAIN_SIZE, std::vector<int>(TERRAIN_SIZE));
 
     for (int x = 0; x < TERRAIN_SIZE; x++) {
         for (int z = 0; z < TERRAIN_SIZE; z++) {
             // Normalized coordinates (-0.5 to 0.5)
             float nx = (x - TERRAIN_SIZE/2) / (float)TERRAIN_SIZE;
             float nz = (z - TERRAIN_SIZE/2) / (float)TERRAIN_SIZE;
             
             // Distance from center (0 to 0.7)
             float distFromCenter = sqrt(nx*nx + nz*nz) * 1.4f;
             
             // Base noise (range: -1 to 1)
             float noiseValue = noise.GetNoise(x * 3.0f, z * 3.0f);
             
             // Bias height toward center (inverted distance)
             float height = (noiseValue + 1.0f) * 30.0f;  // Convert noise to 0-20 range
             height *= (1.0f - distFromCenter);           // Force peak in center
             
             heightMap[x][z] = std::max(0, (int)height);  // Ensure non-negative
         }
     }

    // Create stacked cubes
    for (int x = 0; x < TERRAIN_SIZE; x++) {
        for (int z = 0; z < TERRAIN_SIZE; z++) {
            int height = heightMap[x][z];
            for (int y = 0; y <= height; y++) {
                // Only add cube if it's the top layer or adjacent to air
                bool isSurface = (y == height) || 
                                (x > 0 && y >= heightMap[x-1][z]) ||
                                (x < TERRAIN_SIZE-1 && y >= heightMap[x+1][z]) ||
                                (z > 0 && y >= heightMap[x][z-1]) ||
                                (z < TERRAIN_SIZE-1 && y >= heightMap[x][z+1]);
                
                if(isSurface) {
                    cubes.push_back({glm::vec3(x - TERRAIN_SIZE/2, y, z - TERRAIN_SIZE/2)});
                }
            }
        }
    }
}

void createCubeGeometry(std::vector<float>& vertices) {
    glm::vec3 colors[] = {
        glm::vec3(0.7f), // Top/Bottom
        glm::vec3(0.5f), // Front/Back
        glm::vec3(0.3f)  // Left/Right
    };

    // Cube vertices with colors
    float verticesTemp[] = {
        // Positions          // Colors
        // Top face
        -0.5f, 0.5f, -0.5f, colors[0].x, colors[0].y, colors[0].z,
        0.5f, 0.5f, -0.5f, colors[0].x, colors[0].y, colors[0].z,
        0.5f, 0.5f, 0.5f, colors[0].x, colors[0].y, colors[0].z,
        -0.5f, 0.5f, 0.5f, colors[0].x, colors[0].y, colors[0].z,

        // Bottom face
        -0.5f, -0.5f, -0.5f, colors[0].x, colors[0].y, colors[0].z,
        0.5f, -0.5f, -0.5f, colors[0].x, colors[0].y, colors[0].z,
        0.5f, -0.5f, 0.5f, colors[0].x, colors[0].y, colors[0].z,
        -0.5f, -0.5f, 0.5f, colors[0].x, colors[0].y, colors[0].z,

        // Front face
        -0.5f, -0.5f, 0.5f, colors[1].x, colors[1].y, colors[1].z,
        0.5f, -0.5f, 0.5f, colors[1].x, colors[1].y, colors[1].z,
        0.5f, 0.5f, 0.5f, colors[1].x, colors[1].y, colors[1].z,
        -0.5f, 0.5f, 0.5f, colors[1].x, colors[1].y, colors[1].z,

        // Back face
        -0.5f, -0.5f, -0.5f, colors[1].x, colors[1].y, colors[1].z,
        0.5f, -0.5f, -0.5f, colors[1].x, colors[1].y, colors[1].z,
        0.5f, 0.5f, -0.5f, colors[1].x, colors[1].y, colors[1].z,
        -0.5f, 0.5f, -0.5f, colors[1].x, colors[1].y, colors[1].z,

        // Left face
        -0.5f, -0.5f, -0.5f, colors[2].x, colors[2].y, colors[2].z,
        -0.5f, -0.5f, 0.5f, colors[2].x, colors[2].y, colors[2].z,
        -0.5f, 0.5f, 0.5f, colors[2].x, colors[2].y, colors[2].z,
        -0.5f, 0.5f, -0.5f, colors[2].x, colors[2].y, colors[2].z,

        // Right face
        0.5f, -0.5f, -0.5f, colors[2].x, colors[2].y, colors[2].z,
        0.5f, -0.5f, 0.5f, colors[2].x, colors[2].y, colors[2].z,
        0.5f, 0.5f, 0.5f, colors[2].x, colors[2].y, colors[2].z,
        0.5f, 0.5f, -0.5f, colors[2].x, colors[2].y, colors[2].z,
    };

    vertices.assign(verticesTemp, verticesTemp + sizeof(verticesTemp)/sizeof(float));
}

void processInput(GLFWwindow* window, float deltaTime) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    float cameraSpeed = 5.0f * deltaTime * speedMultiplier;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        cameraPos += cameraSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        cameraPos -= cameraSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        cameraPos += cameraSpeed * cameraUp;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        cameraPos -= cameraSpeed * cameraUp;
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
        speedMultiplier = 3.0f;
    else
        speedMultiplier = 1.0f;
    if (glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS) {
        wireframe = !wireframe;
        glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
        while(glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS) 
            glfwPollEvents();
    }
}

int main() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "3D Terrain", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        return -1;
    }

    generateTerrain();

    // Configure OpenGL
    glEnable(GL_DEPTH_TEST);
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

    // Shader setup
    unsigned int shaderProgram;
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // Cube VAO/VBO
    std::vector<float> cubeVertices;
    createCubeGeometry(cubeVertices);
    unsigned int VBO, VAO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, cubeVertices.size() * sizeof(float), cubeVertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Projection matrix
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)WIDTH/(float)HEIGHT, 0.1f, 100.0f);

    // Main loop
    double lastTime = glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        double currentTime = glfwGetTime();
        float deltaTime = currentTime - lastTime;
        lastTime = currentTime;

        processInput(window, deltaTime);

        // Rendering
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(shaderProgram);

        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

        // Draw cubes
        for (const Cube& cube : cubes) {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, cube.position);
            model = glm::scale(model, glm::vec3(CUBE_SIZE));
            glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
            glBindVertexArray(VAO);
            for (int i = 0; i < 6; i++) {
                glDrawArrays(GL_TRIANGLE_FAN, i*4, 4);
            }
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);
    glfwTerminate();
    return 0;
}