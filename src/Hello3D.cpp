#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm> // Include algorithm for std::min and std::max
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode);
const GLuint WIDTH = 1000, HEIGHT = 1000;
int selectedEntityIndex = 0;

// Calculate the normal vector of the triangle
glm::vec3 calculateNormal(const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& v3) {
    glm::vec3 edge1 = v2 - v1;
    glm::vec3 edge2 = v3 - v1;
    return glm::normalize(glm::cross(edge1, edge2));
}

// Determine color based on the principal direction of the normal vector
glm::vec3 colorFromDirection(const glm::vec3& normal) {
    // Define colors corresponding to +x, -x, +y, -y, +z, -z
    glm::vec3 colors[6] = {
        glm::vec3(1.0f, 0.0f, 0.0f), // +x
        glm::vec3(0.6f, 0.0f, 0.0f), // -x
        glm::vec3(0.0f, 1.0f, 0.0f), // +y
        glm::vec3(0.0f, 0.6f, 0.0f), // -y
        glm::vec3(0.0f, 0.0f, 1.0f), // +z
        glm::vec3(0.0f, 0.0f, 0.6f)  // -z
    };

    // Compute the weights based on proximity
    float weights[6];
    weights[0] = std::max(0.0f, normal.x); // +x
    weights[1] = std::max(0.0f, -normal.x); // -x
    weights[2] = std::max(0.0f, normal.y); // +y
    weights[3] = std::max(0.0f, -normal.y); // -y
    weights[4] = std::max(0.0f, normal.z); // +z
    weights[5] = std::max(0.0f, -normal.z); // -z

    // Normalize weights
    float sumWeights = weights[0] + weights[1] + weights[2] + weights[3] + weights[4] + weights[5];
    for (int i = 0; i < 6; ++i) {
        weights[i] /= sumWeights;
    }

    // Interpolate colors based on weights
    glm::vec3 mixedColor = glm::vec3(0.0f);
    for (int i = 0; i < 6; ++i) {
        mixedColor += weights[i] * colors[i];
    }

    return mixedColor;
}

class Entity {
public:
    Entity(float x, float y, float z, float initialScale, const std::string& objFilePath)
        : position(x, y, z), scaleFactor(initialScale), rotateX(false), rotateY(false), rotateZ(false) {
        VAO = loadSimpleOBJ(objFilePath, nVertices);
        if (VAO == -1) {
            std::cerr << "Failed to load model from " << objFilePath << std::endl;
        }
        setupShaders();
    }

    void draw() {
        glUseProgram(shaderProgram);

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, position);
        model = glm::scale(model, glm::vec3(scaleFactor));

        float angle = static_cast<GLfloat>(glfwGetTime());
        if (rotateX) {
            model = glm::rotate(model, angle, glm::vec3(1.0f, 0.0f, 0.0f));
        }
        if (rotateY) {
            model = glm::rotate(model, angle, glm::vec3(0.0f, 1.0f, 0.0f));
        }
        if (rotateZ) {
            model = glm::rotate(model, angle, glm::vec3(0.0f, 0.0f, 1.0f));
        }

        GLint modelLoc = glGetUniformLocation(shaderProgram, "model");
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, nVertices);
        glBindVertexArray(0);
    }

    void toggleRotateX() { rotateX = !rotateX; rotateY = false; rotateZ = false; }
    void toggleRotateY() { rotateX = false; rotateY = !rotateY; rotateZ = false; }
    void toggleRotateZ() { rotateX = false; rotateY = false; rotateZ = !rotateZ; }
    void scaleUp() { scaleFactor = std::min(scaleFactor + 0.1f, 0.8f); }
    void scaleDown() { scaleFactor = std::max(0.1f, scaleFactor - 0.1f); }

private:
    GLuint VAO;
    int nVertices;
    glm::vec3 position;
    float scaleFactor;
    bool rotateX, rotateY, rotateZ;
    GLuint shaderProgram;

    int loadSimpleOBJ(const std::string& filePath, int& nVertices) {
        std::vector<glm::vec3> vertices;
        std::vector<GLfloat> vBuffer;

        std::ifstream arqEntrada(filePath.c_str());
        if (!arqEntrada.is_open()) {
            std::cerr << "Erro ao tentar ler o arquivo " << filePath << std::endl;
            return -1;
        }

        std::string line;
        std::vector<glm::vec3> faceVertices;

        while (std::getline(arqEntrada, line)) {
            std::istringstream ssline(line);
            std::string word;
            ssline >> word;

            if (word == "v") {
                glm::vec3 vertice;
                ssline >> vertice.x >> vertice.y >> vertice.z;
                vertices.push_back(vertice);
            } else if (word == "f") {
                faceVertices.clear();
                while (ssline >> word) {
                    int vi = 0;
                    std::istringstream ss(word);
                    std::string index;

                    if (std::getline(ss, index, '/')) vi = !index.empty() ? std::stoi(index) - 1 : 0;

                    faceVertices.push_back(vertices[vi]);
                }

                if (faceVertices.size() == 3) {
                    glm::vec3 normal = calculateNormal(faceVertices[0], faceVertices[1], faceVertices[2]);
                    glm::vec3 color = colorFromDirection(normal);
                    for (auto& vert : faceVertices) {
                        vBuffer.push_back(vert.x);
                        vBuffer.push_back(vert.y);
                        vBuffer.push_back(vert.z);

                        vBuffer.push_back(color.r);
                        vBuffer.push_back(color.g);
                        vBuffer.push_back(color.b);
                    }
                }
            }
        }

        arqEntrada.close();

        GLuint VBO, VAO;
        glGenBuffers(1, &VBO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vBuffer.size() * sizeof(GLfloat), vBuffer.data(), GL_STATIC_DRAW);

        glGenVertexArrays(1, &VAO);
        glBindVertexArray(VAO);

        // Position attribute
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (GLvoid*)0);
        glEnableVertexAttribArray(0);

        // Color attribute
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
        glEnableVertexAttribArray(1);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        nVertices = vBuffer.size() / 6;
        return VAO;
    }

    void setupShaders() {
        const GLchar* vertexShaderSource = "#version 410\n"
            "layout (location = 0) in vec3 position;\n"
            "layout (location = 1) in vec3 color;\n"
            "out vec3 vertexColor;\n"
            "uniform mat4 model;\n"
            "void main()\n"
            "{\n"
            "gl_Position = model * vec4(position, 1.0);\n"
            "vertexColor = color;\n"
            "}\0";

        const GLchar* fragmentShaderSource = "#version 410\n"
            "in vec3 vertexColor;\n"
            "out vec4 color;\n"
            "void main()\n"
            "{\n"
            "color = vec4(vertexColor, 1.0);\n"
            "}\n\0";

        GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
        glCompileShader(vertexShader);

        GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
        glCompileShader(fragmentShader);

        shaderProgram = glCreateProgram();
        glAttachShader(shaderProgram, vertexShader);
        glAttachShader(shaderProgram, fragmentShader);
        glLinkProgram(shaderProgram);

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
    }
};

std::vector<Entity> entities;

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Atividade Vivencial 1", nullptr, nullptr);
    glfwMakeContextCurrent(window);

    glfwSetKeyCallback(window, key_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glEnable(GL_DEPTH_TEST);

    entities.emplace_back(-0.5f, 0.0f, 0.0f, 0.3f, "../assets/Modelos3D/Cube.obj");
    entities.emplace_back(0.5f, 0.0f, 0.5f, 0.3f, "../assets/Modelos3D/Cube.obj");

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        for (auto& entity : entities) {
            entity.draw();
        }

        glfwSwapBuffers(window);
    }

    glfwTerminate();
    return 0;
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GL_TRUE);
    }

    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        Entity& selectedEntity = entities[selectedEntityIndex];

        if (key == GLFW_KEY_X) {
            selectedEntity.toggleRotateX();
        }
        if (key == GLFW_KEY_Y) {
            selectedEntity.toggleRotateY();
        }
        if (key == GLFW_KEY_Z) {
            selectedEntity.toggleRotateZ();
        }
        if (key == GLFW_KEY_LEFT_BRACKET) {
            selectedEntity.scaleDown();
        }
        if (key == GLFW_KEY_RIGHT_BRACKET) {
            selectedEntity.scaleUp();
        }
        if (key == GLFW_KEY_D) {
            selectedEntityIndex = (selectedEntityIndex + 1) % entities.size();
        }
        if (key == GLFW_KEY_A) {
            selectedEntityIndex = (selectedEntityIndex - 1 + entities.size()) % entities.size();
        }
    }
}