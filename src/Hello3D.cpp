#define STB_IMAGE_IMPLEMENTATION
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <stb_image.h> // Include stb_image for texture loading

void log(const std::string& message) {
    std::cerr << "[LOG]: " << message << std::endl;
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode);
const GLuint WIDTH = 1000, HEIGHT = 1000;
int selectedEntityIndex = 0;

GLuint loadTexture(const std::string& filepath) {
    GLuint textureId;
    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);

    int width, height, nrChannels;
    unsigned char* data = stbi_load(filepath.c_str(), &width, &height, &nrChannels, 0);
    if (data) {
        log("Texture loaded successfully: " + filepath);
        GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        log("Generated mipmaps for texture.");
    } else {
        log("Failed to load texture at path: " + filepath);
        // Handle this case or load a default texture to avoid segmentation faults
    }
    stbi_image_free(data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    return textureId;
}

class Entity {
public:
    glm::vec3 position;

    Entity(float x, float y, float z, float initialScale, const std::string& objFilePath, const std::string& mtlFilePath)
        : position(x, y, z), scaleFactor(initialScale), VAO(0), texture(0), nVertices(0),
          rotateX(false), rotateY(false), rotateZ(false) {
        VAO = loadModel(objFilePath, mtlFilePath, nVertices);
        if (VAO == -1) {
            log("Failed to load model from: " + objFilePath);
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

        glm::mat4 view = glm::lookAt(
            glm::vec3(0.0f, 0.0f, 3.0f),
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (GLfloat)WIDTH / (GLfloat)HEIGHT, 0.1f, 100.0f);

        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

        glUniform3f(glGetUniformLocation(shaderProgram, "lightPos"), 1.2f, 1.0f, 2.0f);
        glUniform3f(glGetUniformLocation(shaderProgram, "lightColor"), 1.0f, 1.0f, 1.0f);
        glUniform3f(glGetUniformLocation(shaderProgram, "camPos"), 0.0f, 0.0f, 3.0f);
        
        glUniform1f(glGetUniformLocation(shaderProgram, "ka"), 0.1f);
        glUniform1f(glGetUniformLocation(shaderProgram, "kd"), 0.8f);
        glUniform1f(glGetUniformLocation(shaderProgram, "ks"), 0.5f);
        glUniform1f(glGetUniformLocation(shaderProgram, "q"), 32.0f);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glUniform1i(glGetUniformLocation(shaderProgram, "textureSampler"), 0);

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
    GLuint VAO, texture;
    int nVertices;
    float scaleFactor;
    bool rotateX, rotateY, rotateZ;
    GLuint shaderProgram;

    int loadModel(const std::string& objFilePath, const std::string& mtlFilePath, int& nVertices) {
        std::vector<glm::vec3> vertices;
        std::vector<glm::vec2> texCoords;
        std::vector<glm::vec3> normals;
        std::vector<GLfloat> vBuffer;

        std::ifstream objFile(objFilePath);
        if (!objFile.is_open()) {
            log("Error opening OBJ file: " + objFilePath);
            return -1;
        }

        std::unordered_map<std::string, GLuint> textureMap;
        std::ifstream mtlFile(mtlFilePath);
        if (mtlFile.is_open()) {
            std::string line;
            std::string textureFilePath;

            while (std::getline(mtlFile, line)) {
                std::istringstream ssLine(line);
                std::string keyword;
                ssLine >> keyword;

                if (keyword == "map_Kd") {
                    ssLine >> textureFilePath;
                    textureMap[textureFilePath] = loadTexture(textureFilePath);
                }
            }
            mtlFile.close();
        } else {
            log("Error opening MTL file: " + mtlFilePath);
        }

        std::string line;
        while (std::getline(objFile, line)) {
            std::istringstream ssLine(line);
            std::string prefix;
            ssLine >> prefix;

            if (prefix == "v") {
                glm::vec3 vertex;
                ssLine >> vertex.x >> vertex.y >> vertex.z;
                vertices.push_back(vertex);
            } else if (prefix == "vt") {
                glm::vec2 texCoord;
                ssLine >> texCoord.x >> texCoord.y;
                texCoord.y = 1.0f - texCoord.y; // Flip the V coordinate!!!
                texCoords.push_back(texCoord);
            } else if (prefix == "vn") {
                glm::vec3 norm;
                ssLine >> norm.x >> norm.y >> norm.z;
                normals.push_back(norm);
            } else if (prefix == "f") {
                int vIndex[3], tIndex[3], nIndex[3];
                for (int i = 0; i < 3; ++i) {
                    std::string vertexData;
                    ssLine >> vertexData;
                    vIndex[i] = tIndex[i] = nIndex[i] = 0;
                    int matches = sscanf(vertexData.c_str(), "%d/%d/%d", &vIndex[i], &tIndex[i], &nIndex[i]);
                    if (matches < 1) {
                        log("Invalid face data: " + vertexData);
                        return -1;
                    } else if (matches == 1) {
                        // Vertex only
                    } else if (matches >= 2) {
                        if (tIndex[i] > 0) tIndex[i]--;
                    }
                    if (vIndex[i] > 0) vIndex[i]--;
                    if (nIndex[i] > 0) nIndex[i]--;
                }

                for (int i = 0; i < 3; ++i) {
                    vBuffer.push_back(vertices[vIndex[i]].x);
                    vBuffer.push_back(vertices[vIndex[i]].y);
                    vBuffer.push_back(vertices[vIndex[i]].z);

                    if (!texCoords.empty() && tIndex[i] >= 0) {
                        vBuffer.push_back(texCoords[tIndex[i]].x);
                        vBuffer.push_back(texCoords[tIndex[i]].y);
                    } else {
                        vBuffer.push_back(0.0f);
                        vBuffer.push_back(0.0f);
                    }

                    if (!normals.empty() && nIndex[i] >= 0) {
                        vBuffer.push_back(normals[nIndex[i]].x);
                        vBuffer.push_back(normals[nIndex[i]].y);
                        vBuffer.push_back(normals[nIndex[i]].z);
                    } else {
                        vBuffer.push_back(0.0f);
                        vBuffer.push_back(0.0f);
                        vBuffer.push_back(0.0f);
                    }
                }
            }
        }
        objFile.close();

        GLuint VBO;
        glGenBuffers(1, &VBO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vBuffer.size() * sizeof(GLfloat), vBuffer.data(), GL_STATIC_DRAW);

        glGenVertexArrays(1, &VAO);
        glBindVertexArray(VAO);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)0);
        glEnableVertexAttribArray(0);

        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
        glEnableVertexAttribArray(1);

        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)(5 * sizeof(GLfloat)));
        glEnableVertexAttribArray(2);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        nVertices = vBuffer.size() / 8;
        if (!textureMap.empty()) {
            texture = textureMap.begin()->second;
        } else {
            log("Warning: No textures loaded for: " + objFilePath);
        }
        return VAO;
    }

    void setupShaders() {
        const GLchar* vertexShaderSource = R"glsl(
            #version 410 core
            layout(location = 0) in vec3 position;
            layout(location = 1) in vec2 texCoord;
            layout(location = 2) in vec3 normal;

            out vec2 TexCoord;
            out vec3 FragPos;
            out vec3 Normal;

            uniform mat4 model;
            uniform mat4 view;
            uniform mat4 projection;

            void main() {
                FragPos = vec3(model * vec4(position, 1.0));
                Normal = mat3(transpose(inverse(model))) * normal;
                TexCoord = texCoord;
                gl_Position = projection * view * vec4(FragPos, 1.0);
            }
        )glsl";

        const GLchar* fragmentShaderSource = R"glsl(
            #version 410 core
            in vec2 TexCoord;
            in vec3 FragPos;
            in vec3 Normal;

            out vec4 FragColor;

            uniform sampler2D textureSampler;
            uniform vec3 lightPos;
            uniform vec3 lightColor;
            uniform vec3 camPos;
            
            uniform float ka;
            uniform float kd;
            uniform float ks;
            uniform float q;

            void main() {
                vec3 color = texture(textureSampler, TexCoord).rgb;
                vec3 norm = normalize(Normal);

                vec3 ambient = ka * color;

                vec3 lightDir = normalize(lightPos - FragPos);
                float diff = max(dot(norm, lightDir), 0.0);
                vec3 diffuse = kd * diff * lightColor * color;

                vec3 viewDir = normalize(camPos - FragPos);
                vec3 reflectDir = reflect(-lightDir, norm);
                float spec = pow(max(dot(viewDir, reflectDir), 0.0), q);
                vec3 specular = ks * spec * lightColor;

                vec3 result = ambient + diffuse + specular;
                FragColor = vec4(result, 1.0);
            }
        )glsl";

        GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
        glCompileShader(vertexShader);

        GLint success;
        GLchar infoLog[512];
        glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
            log(std::string("Vertex shader compilation failed: ") + infoLog);
        }

        GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
        glCompileShader(fragmentShader);

        glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
            log(std::string("Fragment shader compilation failed: ") + infoLog);
        }

        shaderProgram = glCreateProgram();
        glAttachShader(shaderProgram, vertexShader);
        glAttachShader(shaderProgram, fragmentShader);
        glLinkProgram(shaderProgram);

        glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
            log(std::string("Shader program linking failed: ") + infoLog);
        }

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
    }
};

std::vector<Entity> entities;

int main() {
    log("Initializing GLFW");
    if (!glfwInit()) {
        log("Failed to initialize GLFW");
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Atividade Vivencial 1", nullptr, nullptr);
    if (!window) {
        log("Failed to create GLFW window");
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, key_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        log("Failed to initialize GLAD");
        return -1;
    }

    glEnable(GL_DEPTH_TEST);

    log("Creating Entities");
    entities.emplace_back(-0.5f, 0.0f, 0.0f, 0.3f, "../assets/Modelos3D/Suzanne.obj", "../assets/Modelos3D/Suzanne.mtl");
    entities.emplace_back(0.5f, 0.0f, 0.5f, 0.3f, "../assets/Modelos3D/SuzanneSubdiv1.obj", "../assets/Modelos3D/SuzanneSubdiv1.mtl");

    log("Entering render loop");
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        for (auto& entity : entities) {
            entity.draw();
        }

        glfwSwapBuffers(window);
    }

    log("Terminating GLFW");
    glfwTerminate();
    return 0;
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GL_TRUE);
    }

    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        Entity& selectedEntity = entities[selectedEntityIndex];

        float moveStep = 0.1f; // Adjust this value to change movement speed

        if (key == GLFW_KEY_W) {
            selectedEntity.position.y += moveStep;
        }
        if (key == GLFW_KEY_S) {
            selectedEntity.position.y -= moveStep;
        }
        if (key == GLFW_KEY_A) {
            selectedEntity.position.x -= moveStep;
        }
        if (key == GLFW_KEY_D) {
            selectedEntity.position.x += moveStep;
        }

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
        if (key == GLFW_KEY_N) {
            selectedEntityIndex = (selectedEntityIndex + 1) % entities.size();
        }
        if (key == GLFW_KEY_P) {
            selectedEntityIndex = (selectedEntityIndex - 1 + entities.size()) % entities.size();
        }
    }
}