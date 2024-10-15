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

layout(location = 0) in vec3 aPos;       // Vertex position
layout(location = 1) in vec2 aTexCoord;  // Texture coordinates
layout(location = 2) in vec3 aNormal;    // Vertex normal

uniform mat4 projection;  // Projection matrix
uniform mat4 view;        // View (camera) matrix
uniform mat4 model;       // Model matrix for transforming normals

out vec2 TexCoord;        // Pass texture coordinates to fragment shader
out vec3 FragPos;         // Fragment position in world space
out vec3 Normal;          // Normal vector in world space

void main() {
    // Combine projection, view, and model to get final position
    gl_Position = projection * view * model * vec4(aPos, 1.0);

    // Pass texture coordinates
    TexCoord = aTexCoord;

    // Transform normal into world space using the model matrix
    Normal = (model * vec4(aNormal, 1.0)).xyz;

    // Pass the fragment position (in world space)
    FragPos = vec3(model * vec4(aPos, 1.0));
}
)";


const char* fragmentShaderSource = R"(
#version 310 es
precision mediump float;

in vec2 TexCoord;        // Interpolated texture coordinates
in vec3 FragPos;         // Fragment position in world space
in vec3 Normal;          // Interpolated normal vector

out vec4 FragColor;      // Output fragment color

// Uniforms for lighting
uniform vec3 lightPos;   // Light position in world space
uniform vec3 lightColor; // Light color
uniform vec3 objectColor;// Object color

// Texture sampler
uniform sampler2D textureSampler;

void main() {
    // Normalize the normal vector
    vec3 norm = normalize(Normal);

    // Calculate light direction (from fragment to light)
    vec3 lightDir = normalize(lightPos - FragPos);

    // Calculate the diffuse intensity (Lambertian reflection)
    float diff = max(dot(norm, lightDir), 0.0);

    // Apply the diffuse lighting to the object color
    vec3 diffuse = diff * lightColor;

    // Sample the texture color
    vec4 texColor = texture(textureSampler, TexCoord);

    // Output final fragment color, combining diffuse lighting and texture color
    FragColor = vec4(diffuse, 1.0) * texColor;
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
// Positions          // Texture Coords // Normals
        // Front face
        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,  0.0f,  0.0f,  1.0f,
        0.5f, -0.5f,  0.5f,  1.0f, 0.0f,  0.0f,  0.0f,  1.0f,
        0.5f,  0.5f,  0.5f,  1.0f, 1.0f,  0.0f,  0.0f,  1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f, 1.0f,  0.0f,  0.0f,  1.0f,

        // Back face
        -0.5f, -0.5f, -0.5f,  0.0f, 0.0f,  0.0f,  0.0f, -1.0f,
        0.5f, -0.5f, -0.5f,  1.0f, 0.0f,  0.0f,  0.0f, -1.0f,
        0.5f,  0.5f, -0.5f,  1.0f, 1.0f,  0.0f,  0.0f, -1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,  0.0f,  0.0f, -1.0f,

        // Left face
        -0.5f, -0.5f, -0.5f,  0.0f, 0.0f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f,  0.5f,  1.0f, 0.0f, -1.0f,  0.0f,  0.0f,
        -0.5f,  0.5f,  0.5f,  1.0f, 1.0f, -1.0f,  0.0f,  0.0f,
        -0.5f,  0.5f, -0.5f,  0.0f, 1.0f, -1.0f,  0.0f,  0.0f,

        // Right face
        0.5f, -0.5f, -0.5f,  0.0f, 0.0f,  1.0f,  0.0f,  0.0f,
        0.5f, -0.5f,  0.5f,  1.0f, 0.0f,  1.0f,  0.0f,  0.0f,
        0.5f,  0.5f,  0.5f,  1.0f, 1.0f,  1.0f,  0.0f,  0.0f,
        0.5f,  0.5f, -0.5f,  0.0f, 1.0f,  1.0f,  0.0f,  0.0f,

        // Bottom face
        -0.5f, -0.5f, -0.5f,  0.0f, 0.0f,  0.0f, 1.0f,  0.0f,
        0.5f, -0.5f, -0.5f,  1.0f, 0.0f,  0.0f, 1.0f,  0.0f,
        0.5f, -0.5f,  0.5f,  1.0f, 1.0f,  0.0f, 1.0f,  0.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, 1.0f,  0.0f, 1.0f,  0.0f,

        // Top face
        -0.5f,  0.5f, -0.5f,  0.0f, 0.0f,  0.0f,  1.0f,  0.0f,
        0.5f,  0.5f, -0.5f,  1.0f, 0.0f,  0.0f,  1.0f,  0.0f,
        0.5f,  0.5f,  0.5f,  1.0f, 1.0f,  0.0f,  1.0f,  0.0f,
        -0.5f,  0.5f,  0.5f,  0.0f, 1.0f,  0.0f,  1.0f,  0.0f,
};

const GLuint indices[] = {
        // Front face
        0, 1, 2,
        2, 3, 0,

        // Back face
        4, 5, 6,
        6, 7, 4,

        // Left face
        8, 9, 10,
        10, 11, 8,

        // Right face
        12, 13, 14,
        14, 15, 12,

        // Bottom face
        16, 17, 18,
        18, 19, 16,

        // Top face
        20, 21, 22,
        22, 23, 20,
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

// Position attribute (location = 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

// Texture coordinate attribute (location = 1)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

// Normal attribute (location = 2)
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(5 * sizeof(float)));
    glEnableVertexAttribArray(2);

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
void Render(GLuint shaderProgram, GLuint VAO, GLuint textureID, float angle, glm::vec3 lightPos, glm::vec3 viewPos) {

    // Clear the color and depth buffers
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Use the shader program
    glUseProgram(shaderProgram);

    // Create the projection matrix (Perspective projection)
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), 800.0f / 600.0f, 0.1f, 100.0f);

    // Create the view matrix (camera view)
    glm::mat4 view = glm::lookAt(viewPos, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    // Create the model matrix with rotation around the Y-axis
    glm::mat4 model = glm::rotate(glm::mat4(1.0f), glm::radians(angle), glm::vec3(0.0f, 1.0f, 0.0f));

    // Send the individual projection, view, and model matrices to the shader
    GLint projLocation = glGetUniformLocation(shaderProgram, "projection");
    glUniformMatrix4fv(projLocation, 1, GL_FALSE, glm::value_ptr(projection));

    GLint viewLocation = glGetUniformLocation(shaderProgram, "view");
    glUniformMatrix4fv(viewLocation, 1, GL_FALSE, glm::value_ptr(view));

    GLint modelLocation = glGetUniformLocation(shaderProgram, "model");
    glUniformMatrix4fv(modelLocation, 1, GL_FALSE, glm::value_ptr(model));

    // Send light position and camera (view) position to the shader
    glUniform3fv(glGetUniformLocation(shaderProgram, "lightPos"), 1, glm::value_ptr(lightPos));
    glUniform3fv(glGetUniformLocation(shaderProgram, "viewPos"), 1, glm::value_ptr(viewPos));

    // Set the light color (white) and object color (gray)
    glUniform3f(glGetUniformLocation(shaderProgram, "lightColor"), 1.0f, 1.0f, 1.0f);
    glUniform3f(glGetUniformLocation(shaderProgram, "objectColor"), 0.5f, 0.5f, 0.5f);

    // Bind texture unit 0 to the texture sampler and send it to the shader
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glUniform1i(glGetUniformLocation(shaderProgram, "textureSampler"), 0); // Texture unit 0

    // Bind the VAO and draw the object (assuming indexed drawing with glDrawElements)
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);

    // Unbind VAO after drawing
    glBindVertexArray(0);
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
        glm::vec3 viewPos = glm::vec3(2.0f, 2.0f, 2.0f); // Camera position

        // Setup buffers
        SetupBuffers(VAO, VBO, EBO);

        // Load texture from file using the copied path
        loadTextureFromFile(pathStr.c_str());

        // Main render loop
        while (running) {
            // Clear buffers
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glClearColor(0,1,1,0);
            // Update the rotation angle faster
            angle += 2.0f; // Increase the angle faster to rotate the object faster

            // Render
            Render(shaderProgram, VAO, textureID, angle, lightPos, viewPos);

            // Swap EGL buffers
            eglSwapBuffers(display, surface);
        }

        // Cleanup OpenGL resources
        deinit_gl(width, height);
        GLHelper_releaseSurface(surface);
        GLHelper_releaseContext(context);
    });
}