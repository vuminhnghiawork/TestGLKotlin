#include <jni.h>
#include <string>
#include <iostream>
#include <GLES3/gl3.h>
#include "gl_helper.h" // Ensure this header includes necessary OpenGL helper functions
#include <android/native_window.h>
#include <android/bitmap.h>
#include <android/native_window_jni.h>
#include <thread>
#include <mutex>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "renderer.h" // Ensure this header includes necessary rendering functions
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <android/log.h>
#include <EGL/eglext.h>

#define LOG_TAG "Native"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Global variables
ANativeWindow* window = nullptr;
EGLContext context = EGL_NO_CONTEXT;
EGLSurface surface = EGL_NO_SURFACE;
EGLDisplay display = EGL_NO_DISPLAY;
GLuint shaderProgram = 0;
int width = 0;
int height = 0;
bool running = false;
std::thread renderThread;
bool triggerUpdateSize = true;
std::mutex mMutex;
GLuint textureID = 0;
GLuint VBO, VAO, EBO;
void loadTextureFromFile(const char* pictureDir);
GLuint textureIDs[6]; // Array to hold texture IDs for each face


extern "C" JNIEXPORT void JNICALL
Java_com_vinai_testglkotlin_MainActivity_surfaceResize(JNIEnv *env, jobject instance) {
    std::lock_guard<std::mutex> lk(mMutex);
    triggerUpdateSize = true;
}

extern "C" JNIEXPORT void JNICALL
Java_com_vinai_testglkotlin_MainActivity_deinitSurface(JNIEnv *env, jobject instance) {
    running = false;
    renderThread.join();
    ANativeWindow_release(window);
}

// Shader sources
const char* vertexShaderSource = R"(
#version 310 es
precision mediump float;

layout(location = 0) in vec3 aPos; // Vertex position
layout(location = 1) in vec2 aTexCoord; // Texture coordinates

uniform mat4 uMVPMatrix;

out vec2 TexCoord; // Pass texture coordinates to fragment shader

void main() {
    gl_Position = uMVPMatrix * vec4(aPos, 1.0);
    TexCoord = aTexCoord; // Pass through the texture coordinates
}
)";

const char* fragmentShaderSource = R"(
#version 310 es
precision mediump float;

in vec2 TexCoord; // Interpolated texture coordinates
out vec4 FragColor; // Output fragment color

uniform sampler2D textureSampler; // The texture sampler

void main() {
    FragColor = texture(textureSampler, TexCoord); // Sample texture
}
)";

// Compile shader and check for errors
GLuint CompileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    if(shader == 0){
        LOGE("Error creating shader of type %d", type);
        return 0;
    }
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    // Check compile status
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        LOGE("Shader compilation error: %s", infoLog);
    }

    return shader;
}

// Create and link shader program
GLuint CreateShaderProgram() {
    GLuint vertexShader = CompileShader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint fragmentShader = CompileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);

    if(vertexShader == 0 || fragmentShader == 0){
        LOGE("Shader compilation failed.");
        return 0;
    }

    GLuint program = glCreateProgram();
    if(program == 0){
        LOGE("Error creating shader program");
        return 0;
    }
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    // Check link status
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if(!success){
        char infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        LOGE("Program linking error: %s", infoLog);
    }

    // Clean up shaders as they're linked into program now and no longer needed
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return program;
}


// vertices for the rectangular (x, y, z)
const GLfloat vertices[] = {
        // Positions          // Texture Coords
        -0.5f, -0.5f, -0.5f,  0.0f, 0.0f,  // Bottom-left
        0.5f, -0.5f, -0.5f,  1.0f, 0.0f,  // Bottom-right
        0.5f,  0.5f, -0.5f,  1.0f, 1.0f,  // Top-right
        -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,  // Top-left

        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,  // Bottom-left
        0.5f, -0.5f,  0.5f,  1.0f, 0.0f,  // Bottom-right
        0.5f,  0.5f,  0.5f,  1.0f, 1.0f,  // Top-right
        -0.5f,  0.5f,  0.5f,  0.0f, 1.0f   // Top-left
};

const GLuint indices[] = {

// Back face
        0, 1, 2,
        2, 3, 0,

        // Front face
        4, 5, 6,
        6, 7, 4,

        // Left face
        0, 4, 7,
        7, 3, 0,

        // Right face
        1, 5, 6,
        6, 2, 1,

        // Bottom face
        0, 1, 5,
        5, 4, 0,

        // Top face
        3, 2, 6,
        6, 7, 3
};

// Setup buffers for combined vertices and indices
void SetupBuffers(GLuint &VAO, GLuint &VBO, GLuint &EBO) {
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Texture coordinate attribute
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0); // Unbind VAO
}


// Load texture from file
void loadTextureFromFile(const char* pictureDir) {
    LOGI("Loading texture from %s", pictureDir);
    int imgWidth, imgHeight, nrChannels;
    unsigned char* data = stbi_load(pictureDir, &imgWidth, &imgHeight, &nrChannels, 0);

    if(data){
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);

        // Set texture parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // or GL_REPEAT
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // or GL_REPEAT

        GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, format, imgWidth, imgHeight, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        stbi_image_free(data);
        glBindTexture(GL_TEXTURE_2D, 0);
        LOGI("Texture loaded successfully");
    }
    else{
        LOGE("Failed to load texture from %s", pictureDir);
    }
}

// Function to render the rectangular prism with rotation around the Y-axis
void Render(GLuint shaderProgram, GLuint VAO, float angle, glm::vec3 lightPos, glm::vec3 viewPos) {

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    // Use the shader program
    glUseProgram(shaderProgram);

    // Create the projection matrix
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), 800.0f / 600.0f, 0.1f, 100.0f); // Perspective projection

    // Create the view matrix (camera)
    glm::mat4 view = glm::lookAt(viewPos, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f)); // Camera at viewPos, looking at origin

    // Create the model matrix with rotation around the Y-axis
    glm::mat4 model = glm::mat4(1.0f); // Initialize to identity matrix
    model = glm::rotate(model, glm::radians(angle), glm::vec3(0.0f, 1.0f, 0.0f)); // Rotate around Y-axis

    // Combine the matrices into the MVP matrix
    glm::mat4 mvp = projection * view * model; // Combine projection, view, and model matrices

    // Send the MVP matrix to the shader
    unsigned int mvpLocation = glGetUniformLocation(shaderProgram, "uMVPMatrix");
    glUniformMatrix4fv(mvpLocation, 1, GL_FALSE, glm::value_ptr(mvp));

    // Send light position and camera position to the shader
    unsigned int lightPosLoc = glGetUniformLocation(shaderProgram, "lightPos");
    glUniform3fv(lightPosLoc, 1, glm::value_ptr(lightPos));

    unsigned int viewPosLoc = glGetUniformLocation(shaderProgram, "viewPos");
    glUniform3fv(viewPosLoc, 1, glm::value_ptr(viewPos));

    // Send light color and object color to the shader
    unsigned int lightColorLoc = glGetUniformLocation(shaderProgram, "lightColor");
    glUniform3f(lightColorLoc, 1.0f, 1.0f, 1.0f); // White light

    unsigned int objectColorLoc = glGetUniformLocation(shaderProgram, "objectColor");
    glUniform3f(objectColorLoc, 0.5f, 0.5f, 0.5f); // Gray object color

    // Bind texture unit 0 to textureSampler
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);
    GLint texLoc = glGetUniformLocation(shaderProgram, "textureSampler");
    glUniform1i(texLoc, 0); // Texture unit 0

    // Bind VAO and draw the rectangular prism
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0); // Unbind VAO
}



extern "C" JNIEXPORT void JNICALL
Java_com_vinai_testglkotlin_MainActivity_initSurface(JNIEnv *env, jobject instance, jobject j_surface, jstring picturesDir) {
    LOGI("Java_com_vinai_testglkotlin_MainActivity_initSurface");

    // Get native window from the surface
    window = ANativeWindow_fromSurface(env, j_surface);
    running = true;

    // Convert jstring to a C++ std::string to ensure it remains valid during the render thread execution
    const char* pathCStr = env->GetStringUTFChars(picturesDir, nullptr);
    std::string pathStr(pathCStr); // Copy the path into a std::string
    env->ReleaseStringUTFChars(picturesDir, pathCStr); // Now it's safe to release the original jstring

    // Start the render thread
    renderThread = std::thread([pathStr] {
        LOGI("Start render thread");

        // Get EGL display
        display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (display == EGL_NO_DISPLAY) {
            LOGE("Unable to get EGL display.");
            return;
        }

        // Initialize EGL
        if (!GLHelper_initGL(EGL_NO_CONTEXT, window, &context, &surface)) {
            LOGE("GLHelper_initGL failed.");
            return;
        }

        // Make the context current
        if (!eglMakeCurrent(display, surface, surface, context)) {
            LOGE("eglMakeCurrent failed.");
            return;
        }

        // Get surface size
        GLHelper_getSurfaceSize(surface, width, height);

        // Initialize OpenGL settings
        init_gl(width, height);

        // Create shader program
        shaderProgram = CreateShaderProgram();
        if (shaderProgram == 0) {
            LOGE("Shader program initialization failed.\n");
            return;
        }

        // Enable depth testing
        glEnable(GL_DEPTH_TEST);

        float angle = 0.0f; // Initial rotation angle
        glm::vec3 lightPos = glm::vec3(2.0f, 2.0f, 2.0f); // Position of the light source
        glm::vec3 viewPos = glm::vec3(2.0f, 2.0f, 3.0f); // Camera position

        // Setup buffers
        SetupBuffers(VAO, VBO, EBO);

        // Load textures for each face
        GLuint textures[6];
        textures[0] = loadTexture("path_to_texture1.jpg");  // Front face
        textures[1] = loadTexture("path_to_texture2.jpg");  // Back face
        // Load other textures for other faces...

        // Load texture from file using the copied path
        loadTextureFromFile(pathStr.c_str());

        // Main render loop
        while (running) {
            // Clear buffers
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glClearColor(1,1,1,1);
            // Update the rotation angle faster
            angle += 2.0f; // Increase the angle faster to rotate the object faster

            // Render
            Render(shaderProgram, VAO, angle, lightPos, viewPos);

            // Swap EGL buffers
            eglSwapBuffers(display, surface);
        }

        // Cleanup OpenGL resources
        deinit_gl(width, height);
        GLHelper_releaseSurface(surface);
        GLHelper_releaseContext(context);
    });
}