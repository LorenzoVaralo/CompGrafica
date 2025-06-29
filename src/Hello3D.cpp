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
#include <cmath> // For sin()

using json = nlohmann::json;

// Function Prototypes
void log(const std::string& message);
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode);
glm::vec3 bezierPoint(const std::vector<glm::vec3>& controlPoints, float t);

// Global State
std::unordered_map<int, bool> keyStates;
const GLuint WIDTH = 1000, HEIGHT = 1000;
int selectedEntityIndex = 0;
std::vector<glm::vec3> lightColors;
std::vector<glm::vec3> lightPositions;

// --- Camera Class ---
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
          speed(2.5f) {} // Adjusted speed for deltaTime

    glm::mat4 getViewMatrix() const {
        return glm::lookAt(position, position + front, up);
    }

    void moveForward(float deltaTime) { position += speed * deltaTime * front; }
    void moveBackward(float deltaTime) { position -= speed * deltaTime * front; }
    void moveLeft(float deltaTime) { position -= glm::normalize(glm::cross(front, up)) * speed * deltaTime; }
    void moveRight(float deltaTime) { position += glm::normalize(glm::cross(front, up)) * speed * deltaTime; }

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

// --- Helper Structs and Classes ---

// Struct for configurable rotation animation
struct RotationAnimation {
    float amplitude = 0.0f; // How far to rotate in degrees
    float speed = 0.0f;     // How fast to oscillate
    bool fullAmplitude = false; // Whether to perform a full rotation
};

GLuint loadTexture(const std::string& filepath) {
    GLuint textureId;
    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);

    int width, height, nrChannels;
    unsigned char* data = stbi_load(filepath.c_str(), &width, &height, &nrChannels, 0);
    if (data) {
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

// --- Entity Class (Final Version) ---
class Entity {
public:
    // Positional and animation data
    glm::vec3 position;
    glm::vec3 originalPosition;
    std::vector<glm::vec3> bezierControlPoints;
    float bezierTime;

    // Rotational animation data
    glm::vec3 rotationAngles;
    std::vector<RotationAnimation> rotAnimX;
    std::vector<RotationAnimation> rotAnimY;
    std::vector<RotationAnimation> rotAnimZ;

    Entity(float x, float y, float z, float initialScale, const std::string& objFilePath, const std::string& mtlFilePath, const std::vector<glm::vec3>& ctrlPoints, const std::vector<RotationAnimation>& rX, const std::vector<RotationAnimation>& rY, const std::vector<RotationAnimation>& rZ)
        : position(x, y, z),
          originalPosition(x, y, z),
          scaleFactor(initialScale),
          rotationAngles(0.0f),
          VAO(0),
          texture(0),
          nVertices(0),
          bezierControlPoints(ctrlPoints),
          rotAnimX(rX), rotAnimY(rY), rotAnimZ(rZ)
    {
        bezierTime = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
        VAO = loadModel(objFilePath, mtlFilePath, nVertices);
        if (VAO == -1) {
            log("Failed to load model from: " + objFilePath);
        }
        setupShaders();
    }

    void update(float deltaTime) {
        // --- Bézier position movement ---
        if (!bezierControlPoints.empty()) {
            float animationSpeed = 0.2f;
            bezierTime += deltaTime * animationSpeed;
            if (bezierTime > 1.0f) {
                bezierTime -= 1.0f;
            }
            glm::vec3 bezierOffset = bezierPoint(bezierControlPoints, bezierTime);
            position = originalPosition + bezierOffset;
        }

        // --- Rotation animation logic ---
        float currentTime = static_cast<float>(glfwGetTime());
        rotationAngles.x = calculateRotation(rotAnimX, currentTime);
        rotationAngles.y = calculateRotation(rotAnimY, currentTime);
        rotationAngles.z = calculateRotation(rotAnimZ, currentTime);
    }

    float calculateRotation(const std::vector<RotationAnimation>& animations, float currentTime) {
        float totalRotation = 0.0f;
        for (const auto& anim : animations) {
            if (anim.fullAmplitude) {
                totalRotation += glm::radians(anim.amplitude * fmod(anim.speed * currentTime, 360.0f));
            } else {
                totalRotation += glm::radians(anim.amplitude * sin(anim.speed * currentTime));
            }
        }
        return totalRotation;
    }

    void draw() {
        glUseProgram(shaderProgram);

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, position);

        // Apply calculated rotations (Yaw, Pitch, Roll)
        model = glm::rotate(model, rotationAngles.y, glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::rotate(model, rotationAngles.x, glm::vec3(1.0f, 0.0f, 0.0f));
        model = glm::rotate(model, rotationAngles.z, glm::vec3(0.0f, 0.0f, 1.0f));

        model = glm::scale(model, glm::vec3(scaleFactor));

        glm::mat4 view = camera.getViewMatrix();
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (GLfloat)WIDTH / (GLfloat)HEIGHT, 0.1f, 100.0f);

        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

        for (size_t i = 0; i < lightPositions.size(); ++i) {
            std::string posName = "lightPos[" + std::to_string(i) + "]";
            std::string colorName = "lightColor[" + std::to_string(i) + "]";
            glUniform3fv(glGetUniformLocation(shaderProgram, posName.c_str()), 1, glm::value_ptr(lightPositions[i]));
            glUniform3fv(glGetUniformLocation(shaderProgram, colorName.c_str()), 1, glm::value_ptr(lightColors[i]));
        }

        glUniform3fv(glGetUniformLocation(shaderProgram, "camPos"), 1, glm::value_ptr(camera.position));
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

    void scaleUp() { scaleFactor = std::min(scaleFactor + 0.1f, 10.0f); }
    void scaleDown() { scaleFactor = std::max(0.1f, scaleFactor - 0.1f); }

private:
    GLuint VAO, texture, shaderProgram;
    int nVertices;
    float scaleFactor;

    int loadModel(const std::string& objFilePath, const std::string& mtlFilePath, int& nVerts);
    void setupShaders();
    void checkCompileErrors(GLuint shader, std::string type);
};

std::vector<Entity> entities;

// --- Function Implementations ---

void log(const std::string& message) {
    std::cerr << "[LOG]: " << message << std::endl;
}

glm::vec3 bezierPoint(const std::vector<glm::vec3>& controlPoints, float t) {
    if (controlPoints.empty()) return glm::vec3(0.0f);
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

void loadConfig(const std::string& configFilePath) {
    std::ifstream configFile(configFilePath);
    if (!configFile.is_open()) {
        log("Failed to open config file: " + configFilePath);
        return;
    }
    json config;
    configFile >> config;

    auto cameraPos = config["camera"]["position"];
    camera.position = glm::vec3(cameraPos[0], cameraPos[1], cameraPos[2]);

    lightColors.clear();
    lightPositions.clear();
    for (const auto& light : config["lights"]) {
        lightPositions.emplace_back(light["position"][0], light["position"][1], light["position"][2]);
        lightColors.emplace_back(light["color"][0], light["color"][1], light["color"][2]);
    }

    entities.clear();
    for (const auto& entityConfig : config["entities"]) {
        glm::vec3 position(entityConfig["position"][0], entityConfig["position"][1], entityConfig["position"][2]);
        float scale = entityConfig["scale"];
        std::string objFilePath = entityConfig["objFilePath"];
        std::string mtlFilePath = entityConfig["mtlFilePath"];

        std::vector<glm::vec3> entityControlPoints;
        if (entityConfig.contains("bezierControlPoints")) {
            for (const auto& point : entityConfig["bezierControlPoints"]) {
                entityControlPoints.emplace_back(point[0], point[1], point[2]);
            }
        }
        
        std::vector<RotationAnimation> rX, rY, rZ;
        if (entityConfig.contains("rotationAnimation")) {
            const auto& rotConfig = entityConfig["rotationAnimation"];
            if (rotConfig.contains("x")) {
                for (const auto& animConfig : rotConfig["x"]) {
                    RotationAnimation anim;
                    anim.amplitude = animConfig.value("amplitude", 0.0f);
                    anim.speed = animConfig.value("speed", 0.0f);
                    anim.fullAmplitude = animConfig.value("fullAmplitude", false);
                    rX.push_back(anim);
                }
            }
            if (rotConfig.contains("y")) {
                for (const auto& animConfig : rotConfig["y"]) {
                    RotationAnimation anim;
                    anim.amplitude = animConfig.value("amplitude", 0.0f);
                    anim.speed = animConfig.value("speed", 0.0f);
                    anim.fullAmplitude = animConfig.value("fullAmplitude", false);
                    rY.push_back(anim);
                }
            }
            if (rotConfig.contains("z")) {
                for (const auto& animConfig : rotConfig["z"]) {
                    RotationAnimation anim;
                    anim.amplitude = animConfig.value("amplitude", 0.0f);
                    anim.speed = animConfig.value("speed", 0.0f);
                    anim.fullAmplitude = animConfig.value("fullAmplitude", false);
                    rZ.push_back(anim);
                }
            }
        }
        entities.emplace_back(position.x, position.y, position.z, scale, objFilePath, mtlFilePath, entityControlPoints, rX, rY, rZ);
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
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
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
    loadConfig("../config.json");
    
    float lastFrame = 0.0f;
    log("Entering render loop");
    while (!glfwWindowShouldClose(window)) {
        float currentFrame = static_cast<float>(glfwGetTime());
        float deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        glfwPollEvents();
        if (keyStates[GLFW_KEY_W]) camera.moveForward(deltaTime);
        if (keyStates[GLFW_KEY_S]) camera.moveBackward(deltaTime);
        if (keyStates[GLFW_KEY_A]) camera.moveLeft(deltaTime);
        if (keyStates[GLFW_KEY_D]) camera.moveRight(deltaTime);
        if (keyStates[GLFW_KEY_LEFT]) camera.rotateHorizontal(1.0f);
        if (keyStates[GLFW_KEY_RIGHT]) camera.rotateHorizontal(-1.0f);
        if (keyStates[GLFW_KEY_UP]) camera.rotateVertical(1.0f);
        if (keyStates[GLFW_KEY_DOWN]) camera.rotateVertical(-1.0f);

        for (auto& entity : entities) {
            entity.update(deltaTime);
        }

        glClearColor(0.05f, 0.2f, 0.4f, 1.0f); // Deep blue background
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

    if (action == GLFW_PRESS) keyStates[key] = true;
    else if (action == GLFW_RELEASE) keyStates[key] = false;

    if (action == GLFW_PRESS) {
        if (key == GLFW_KEY_LEFT_BRACKET) entities[selectedEntityIndex].scaleDown();
        if (key == GLFW_KEY_RIGHT_BRACKET) entities[selectedEntityIndex].scaleUp();
        if (key == GLFW_KEY_N) selectedEntityIndex = (selectedEntityIndex + 1) % entities.size();
        if (key == GLFW_KEY_P) selectedEntityIndex = (selectedEntityIndex == 0) ? entities.size() - 1 : selectedEntityIndex - 1;

        float intensityStep = 0.1f;
        if (key == GLFW_KEY_1) lightColors[0] += (mode & GLFW_MOD_SHIFT) ? -glm::vec3(intensityStep) : glm::vec3(intensityStep);
        if (key == GLFW_KEY_2) lightColors[1] += (mode & GLFW_MOD_SHIFT) ? -glm::vec3(intensityStep) : glm::vec3(intensityStep);
        if (key == GLFW_KEY_3) lightColors[2] += (mode & GLFW_MOD_SHIFT) ? -glm::vec3(intensityStep) : glm::vec3(intensityStep);
        for (auto& color : lightColors) color = glm::clamp(color, glm::vec3(0.0f), glm::vec3(3.0f));
    }
}


// --- Entity Method Implementations ---

void Entity::checkCompileErrors(GLuint shader, std::string type) {
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

void Entity::setupShaders() {
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
        uniform float ka, kd, ks, q;
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

int Entity::loadModel(const std::string& objFilePath, const std::string& mtlFilePath, int& nVerts) {
    std::vector<glm::vec3> vertices;
    std::vector<glm::vec2> texCoords;
    std::vector<glm::vec3> normals;
    std::vector<GLfloat> vBuffer;
    std::string currentMtlTexturePath;

    std::ifstream mtlFile(mtlFilePath);
    if (mtlFile.is_open()) {
        std::string line;
        while (std::getline(mtlFile, line)) {
            std::istringstream ssLine(line);
            std::string keyword;
            ssLine >> keyword;
            if (keyword == "map_Kd") {
                ssLine >> currentMtlTexturePath;
            }
        }
        mtlFile.close();
        if (!currentMtlTexturePath.empty()) {
            this->texture = loadTexture(currentMtlTexturePath);
        }
    } else {
        log("Error opening MTL file: " + mtlFilePath);
    }

    std::ifstream objFile(objFilePath);
    if (!objFile.is_open()) {
        log("Error opening OBJ file: " + objFilePath);
        return -1;
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
            texCoords.push_back(texCoord);
        } else if (prefix == "vn") {
            glm::vec3 norm;
            ssLine >> norm.x >> norm.y >> norm.z;
            normals.push_back(norm);
        } else if (prefix == "f") {
            std::string remaining_line;
            std::getline(ssLine, remaining_line);
            std::stringstream face_ss(remaining_line);
            std::string vertex_component;
            struct VertexIndices { int v, t, n; };
            std::vector<VertexIndices> face_indices;
            while (face_ss >> vertex_component) {
                std::stringstream component_ss(vertex_component);
                VertexIndices indices = {0, 0, 0};
                char slash;
                component_ss >> indices.v;
                if (component_ss.peek() == '/') {
                    component_ss >> slash;
                    if (component_ss.peek() != '/') component_ss >> indices.t;
                }
                if (component_ss.peek() == '/') {
                    component_ss >> slash >> indices.n;
                }
                face_indices.push_back(indices);
            }
            if (face_indices.size() >= 3) {
                for (size_t i = 0; i < face_indices.size() - 2; ++i) {
                    int indices_to_process[] = {0, (int)i + 1, (int)i + 2};
                    for (int j = 0; j < 3; ++j) {
                        VertexIndices current_indices = face_indices[indices_to_process[j]];
                        int vIndex = current_indices.v > 0 ? current_indices.v - 1 : -1;
                        int tIndex = current_indices.t > 0 ? current_indices.t - 1 : -1;
                        int nIndex = current_indices.n > 0 ? current_indices.n - 1 : -1;
                        vBuffer.push_back(vertices[vIndex].x);
                        vBuffer.push_back(vertices[vIndex].y);
                        vBuffer.push_back(vertices[vIndex].z);
                        if (tIndex != -1 && !texCoords.empty()) { vBuffer.push_back(texCoords[tIndex].x); vBuffer.push_back(1.0f - texCoords[tIndex].y); }
                        else { vBuffer.push_back(0.0f); vBuffer.push_back(0.0f); }
                        if (nIndex != -1 && !normals.empty()) { vBuffer.push_back(normals[nIndex].x); vBuffer.push_back(normals[nIndex].y); vBuffer.push_back(normals[nIndex].z); }
                        else { vBuffer.push_back(0.0f); vBuffer.push_back(0.0f); vBuffer.push_back(0.0f); }
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

    nVerts = vBuffer.size() / 8;
    return VAO;
}