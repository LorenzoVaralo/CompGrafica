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
#include <stb_image.h>
#include <json.hpp>

using json = nlohmann::json;

void log(const std::string& message) {
    std::cerr << "[LOG]: " << message << std::endl;
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode);
std::unordered_map<int, bool> keyStates;
const GLuint WIDTH = 1000, HEIGHT = 1000;
int selectedEntityIndex = 0;
class Camera {
public:
    glm::vec3 position;
    glm::vec3 front;
    glm::vec3 up;
    float speed;

    Camera()
        : position(0.0f, 0.0f, 3.0f),
          front(0.0f, 0.0f, -1.0f),
          up(0.0f, 1.0f, 0.0f),
          speed(0.05f) {}

    glm::mat4 getViewMatrix() const {
        return glm::lookAt(position, position + front, up);
    }

    void moveForward() { position += speed * front; }
    void moveBackward() { position -= speed * front; }
    void moveLeft() { position -= glm::normalize(glm::cross(front, up)) * speed; }
    void moveRight() { position += glm::normalize(glm::cross(front, up)) * speed; }
    void rotateHorizontal(float angle) {
        float radians = glm::radians(angle);
        glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), radians, up);
        front = glm::vec3(rotation * glm::vec4(front, 0.0f));
    }
    void rotateVertical(float angle) {
        float radians = glm::radians(angle);
        glm::vec3 right = glm::normalize(glm::cross(front, up));
        glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), radians, right);
        front = glm::vec3(rotation * glm::vec4(front, 0.0f));
    }
};
Camera camera;

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
    } else {
        log("Failed to load texture at path: " + filepath);
    }
    stbi_image_free(data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    return textureId;
}

// Remove hardcoded light colors and positions
std::vector<glm::vec3> lightColors;
std::vector<glm::vec3> lightPositions;

// Remove hardcoded Bézier control points
std::vector<glm::vec3> bezierControlPoints;

// Remove hardcoded entities initialization
// Entities will be initialized dynamically from the config file in loadConfig()

class Entity {
public:
    glm::vec3 position;
    glm::vec3 originalPosition; // Store the original position for Bézier movement
    bool bezierInitialized = false; // Track if Bézier movement is initialized for this entity

    Entity(float x, float y, float z, float initialScale, const std::string& objFilePath, const std::string& mtlFilePath)
        : position(x, y, z), originalPosition(x, y, z), scaleFactor(initialScale), VAO(0), texture(0), nVertices(0),
          rotateX(false), rotateY(false), rotateZ(false) {
        VAO = loadModel(objFilePath, mtlFilePath, nVertices);
        if (VAO == -1) {
            log("Failed to load model from: " + objFilePath);
        }
        setupShaders();
    }

    void resetBezierState() {
        bezierInitialized = false;
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

        glm::mat4 view = camera.getViewMatrix();
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (GLfloat)WIDTH / (GLfloat)HEIGHT, 0.1f, 100.0f);

        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

        for (int i = 0; i < 3; ++i) {
            std::string posName = "lightPos[" + std::to_string(i) + "]";
            std::string colorName = "lightColor[" + std::to_string(i) + "]";
            glUniform3fv(glGetUniformLocation(shaderProgram, posName.c_str()), 1, glm::value_ptr(lightPositions[i]));
            glUniform3fv(glGetUniformLocation(shaderProgram, colorName.c_str()), 1, glm::value_ptr(lightColors[i]));
        }
        
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
                texCoord.y = 1.0f - texCoord.y;
                texCoords.push_back(texCoord);
            } else if (prefix == "vn") {
                glm::vec3 norm;
                ssLine >> norm.x >> norm.y >> norm.z;
                normals.push_back(norm);
            }
            else if (prefix == "f")
            {
                std::string remaining_line;
                std::getline(ssLine, remaining_line); // Read the rest of the line
                std::stringstream face_ss(remaining_line);
                std::string vertex_component;

                struct VertexIndices
                {
                    int v, t, n;
                };
                std::vector<VertexIndices> face_indices;

                // Read all vertex components from the line (handles 3 for triangles, 4 for quads, etc.)
                while (face_ss >> vertex_component)
                {
                    std::stringstream component_ss(vertex_component);
                    VertexIndices indices = {0, 0, 0};
                    char slash;

                    component_ss >> indices.v;
                    if (component_ss.peek() == '/')
                    {
                        component_ss >> slash >> indices.t;
                    }
                    if (component_ss.peek() == '/')
                    {
                        component_ss >> slash >> indices.n;
                    }
                    face_indices.push_back(indices);
                }

                // Now, create triangles from the parsed indices
                if (face_indices.size() >= 3)
                {
                    // Common vertex data for the first triangle (v0, v1, v2)
                    int indices_to_process[] = {0, 1, 2};
                    for (int i = 0; i < 3; ++i)
                    {
                        VertexIndices current_indices = face_indices[indices_to_process[i]];
                        // OBJ is 1-based, arrays are 0-based
                        int vIndex = current_indices.v > 0 ? current_indices.v - 1 : 0;
                        int tIndex = current_indices.t > 0 ? current_indices.t - 1 : 0;
                        int nIndex = current_indices.n > 0 ? current_indices.n - 1 : 0;

                        vBuffer.push_back(vertices[vIndex].x);
                        vBuffer.push_back(vertices[vIndex].y);
                        vBuffer.push_back(vertices[vIndex].z);

                        if (!texCoords.empty())
                            vBuffer.push_back(texCoords[tIndex].x), vBuffer.push_back(texCoords[tIndex].y);
                        else
                            vBuffer.push_back(0.0f), vBuffer.push_back(0.0f);

                        if (!normals.empty())
                            vBuffer.push_back(normals[nIndex].x), vBuffer.push_back(normals[nIndex].y), vBuffer.push_back(normals[nIndex].z);
                        else
                            vBuffer.push_back(0.0f), vBuffer.push_back(0.0f), vBuffer.push_back(0.0f);
                    }

                    // If it's a quad, create a second triangle (v0, v2, v3)
                    if (face_indices.size() == 4)
                    {
                        int quad_indices_to_process[] = {0, 2, 3}; // The second triangle
                        for (int i = 0; i < 3; ++i)
                        {
                            VertexIndices current_indices = face_indices[quad_indices_to_process[i]];
                            // OBJ is 1-based, arrays are 0-based
                            int vIndex = current_indices.v > 0 ? current_indices.v - 1 : 0;
                            int tIndex = current_indices.t > 0 ? current_indices.t - 1 : 0;
                            int nIndex = current_indices.n > 0 ? current_indices.n - 1 : 0;

                            vBuffer.push_back(vertices[vIndex].x);
                            vBuffer.push_back(vertices[vIndex].y);
                            vBuffer.push_back(vertices[vIndex].z);

                            if (!texCoords.empty())
                                vBuffer.push_back(texCoords[tIndex].x), vBuffer.push_back(texCoords[tIndex].y);
                            else
                                vBuffer.push_back(0.0f), vBuffer.push_back(0.0f);

                            if (!normals.empty())
                                vBuffer.push_back(normals[nIndex].x), vBuffer.push_back(normals[nIndex].y), vBuffer.push_back(normals[nIndex].z);
                            else
                                vBuffer.push_back(0.0f), vBuffer.push_back(0.0f), vBuffer.push_back(0.0f);
                        }
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
            uniform vec3 lightPos[3];
            uniform vec3 lightColor[3];
            uniform vec3 camPos;
            
            uniform float ka;
            uniform float kd;
            uniform float ks;
            uniform float q;

            void main() {
                vec3 color = texture(textureSampler, TexCoord).rgb;
                vec3 norm = normalize(Normal);

                vec3 ambient = ka * color;

                vec3 result = ambient;
                for (int i = 0; i < 3; ++i) {
                    vec3 lightDir = normalize(lightPos[i] - FragPos);
                    float diff = max(dot(norm, lightDir), 0.0);
                    vec3 diffuse = kd * diff * lightColor[i] * color;

                    vec3 viewDir = normalize(camPos - FragPos);
                    vec3 reflectDir = reflect(-lightDir, norm);
                    float spec = pow(max(dot(viewDir, reflectDir), 0.0), q);
                    vec3 specular = ks * spec * lightColor[i];

                    result += diffuse + specular;
                }
                
                FragColor = vec4(result, 1.0);
            }
        )glsl";

        GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
        glCompileShader(vertexShader);
        checkCompileErrors(vertexShader, "VERTEX");

        GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
        glCompileShader(fragmentShader);
        checkCompileErrors(fragmentShader, "FRAGMENT");

        shaderProgram = glCreateProgram();
        glAttachShader(shaderProgram, vertexShader);
        glAttachShader(shaderProgram, fragmentShader);
        glLinkProgram(shaderProgram);
        checkCompileErrors(shaderProgram, "PROGRAM");

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
    }

    void checkCompileErrors(GLuint shader, std::string type) {
        GLint success;
        GLchar infoLog[1024];
        if (type != "PROGRAM") {
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
            if (!success) {
                glGetShaderInfoLog(shader, 1024, NULL, infoLog);
                log("Shader compilation error of type: " + type + "\n" + infoLog);
            }
        } else {
            glGetProgramiv(shader, GL_LINK_STATUS, &success);
            if (!success) {
                glGetProgramInfoLog(shader, 1024, NULL, infoLog);
                log("Program linking error of type: " + type + "\n" + infoLog);
            }
        }
    }
};



std::vector<Entity> entities;

// Function to calculate a point on a Bézier curve
glm::vec3 bezierPoint(const std::vector<glm::vec3>& controlPoints, float t) {
    std::vector<glm::vec3> temp = controlPoints;
    while (temp.size() > 1) {
        std::vector<glm::vec3> nextTemp;
        for (size_t i = 0; i < temp.size() - 1; ++i) {
            nextTemp.push_back(glm::mix(temp[i], temp[i + 1], t));
        }
        temp = nextTemp;
    }
    return temp[0];
}

// Update Bézier curve control points for up-and-down movement
float bezierTime = 0.0f;
bool bezierMovementEnabled = false;

void loadConfig(const std::string& configFilePath) {
    std::ifstream configFile(configFilePath);
    if (!configFile.is_open()) {
        log("Failed to open config file: " + configFilePath);
        return;
    }

    json config;
    configFile >> config;

    // Load camera position
    auto cameraPos = config["camera"]["position"];
    camera.position = glm::vec3(cameraPos[0], cameraPos[1], cameraPos[2]);

    // Load lights
    lightColors.clear();
    lightPositions.clear();
    for (const auto& light : config["lights"]) {
        lightPositions.emplace_back(light["position"][0], light["position"][1], light["position"][2]);
        lightColors.emplace_back(light["color"][0], light["color"][1], light["color"][2]);
    }

    // Load entities
    entities.clear();
    for (const auto& entity : config["entities"]) {
        glm::vec3 position(entity["position"][0], entity["position"][1], entity["position"][2]);
        float scale = entity["scale"];
        std::string objFilePath = entity["objFilePath"];
        std::string mtlFilePath = entity["mtlFilePath"];
        entities.emplace_back(position.x, position.y, position.z, scale, objFilePath, mtlFilePath);
    }

    // Load Bézier control points
    bezierControlPoints.clear();
    for (const auto& point : config["bezier"]["controlPoints"]) {
        bezierControlPoints.emplace_back(point[0], point[1], point[2]);
    }
}

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

    // Load configuration
    loadConfig("../config.json");

    log("Entering render loop");
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (keyStates[GLFW_KEY_W]) {
            camera.moveForward();
        }
        if (keyStates[GLFW_KEY_S]) {
            camera.moveBackward();
        }
        if (keyStates[GLFW_KEY_A]) {
            camera.moveLeft();
        }
        if (keyStates[GLFW_KEY_D]) {
            camera.moveRight();
        }
        if (keyStates[GLFW_KEY_LEFT]) {
            camera.rotateHorizontal(1.0f); 
        }
        if (keyStates[GLFW_KEY_RIGHT]) {
            camera.rotateHorizontal(-1.0f);
        }
        if (keyStates[GLFW_KEY_UP]) {
            camera.rotateVertical(1.0f); 
        }
        if (keyStates[GLFW_KEY_DOWN]) {
            camera.rotateVertical(-1.0f);
        }

        // Update Bézier movement in the render loop
        if (bezierMovementEnabled) {
            bezierTime += 0.01f / 3.0f; // Adjust time increment for 5-second loop
            if (bezierTime > 1.0f) bezierTime = 0.0f; // Reset time after one loop

            Entity& selectedEntity = entities[selectedEntityIndex];

            // Ensure the original position is set only when Bézier movement is toggled on
            if (!selectedEntity.bezierInitialized) {
                selectedEntity.originalPosition = selectedEntity.position;
                selectedEntity.bezierInitialized = true;
            }

            // Update the position of the selected entity to start Bézier movement from its original position
            selectedEntity.position = selectedEntity.originalPosition + bezierPoint(bezierControlPoints, bezierTime);
        } else {
            // Reset Bézier state when movement is disabled
            for (auto& entity : entities) {
                entity.resetBezierState();
            }
        }

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
    
    if (action == GLFW_PRESS) {
        keyStates[key] = true;
    } else if (action == GLFW_RELEASE) {
        keyStates[key] = false;
    }

    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        Entity& selectedEntity = entities[selectedEntityIndex];

        float moveStep = 0.1f;
        float intensityStep = 0.1f;

        
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

        if (key == GLFW_KEY_1) {
            if (mode & GLFW_MOD_SHIFT) {
                lightColors[0] -= glm::vec3(intensityStep);
            } else {
                lightColors[0] += glm::vec3(intensityStep);
            }
        }
        if (key == GLFW_KEY_2) {
            if (mode & GLFW_MOD_SHIFT) {
                lightColors[1] -= glm::vec3(intensityStep);
            } else {
                lightColors[1] += glm::vec3(intensityStep);
            }
        }
        if (key == GLFW_KEY_3) {
            if (mode & GLFW_MOD_SHIFT) {
                lightColors[2] -= glm::vec3(intensityStep);
            } else {
                lightColors[2] += glm::vec3(intensityStep);
            }
        }

        for (auto& color : lightColors) {
            color = glm::clamp(color, glm::vec3(0.0f), glm::vec3(3.0f));
        }
    }

    if (key == GLFW_KEY_B && action == GLFW_PRESS) {
        bezierMovementEnabled = !bezierMovementEnabled;
    }
}