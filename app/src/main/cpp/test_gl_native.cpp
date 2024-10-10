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
void RenderCombinedRectangles(GLuint shaderProgram);
float rectangleOffset = -2.0f;
void SetupCombinedBuffers();

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
layout(location = 0) in vec3 aPos; // Vị trí đỉnh

void main() {
    gl_Position = vec4(aPos, 1.0); // Thiết lập vị trí
}
)";

const char* fragmentShaderSource = R"(
#version 310 es
precision mediump float;

out vec4 FragColor; // Màu đầu ra

void main() {
    FragColor = vec4(0.5, 0.5, 0.5, 1.0); // Áp dụng màu xám (RGB: 0.5, 0.5, 0.5)
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


// Combined vertices for the rectangle (x, y, z)
const GLfloat vertices[] = {
        // Mặt dưới
        -0.5f, -0.5f, -0.5f,  // Góc dưới trái phía sau
        0.5f, -0.5f, -0.5f,  // Góc dưới phải phía sau
        0.5f, -0.5f,  0.5f,  // Góc dưới phải phía trước
        -0.5f, -0.5f,  0.5f,  // Góc dưới trái phía trước

        // Mặt trên
        -0.5f,  0.5f, -0.5f,  // Góc trên trái phía sau
        0.5f,  0.5f, -0.5f,  // Góc trên phải phía sau
        0.5f,  0.5f,  0.5f,  // Góc trên phải phía trước
        -0.5f,  0.5f,  0.5f   // Góc trên trái phía trước
};

// Chỉ số cho hình hộp chữ nhật (12 tam giác)
const GLuint indices[] = {
        // Mặt dưới
        0, 1, 2,
        0, 2, 3,
        // Mặt trên
        4, 5, 6,
        4, 6, 7,
        // Mặt phía trước
        3, 2, 6,
        3, 6, 7,
        // Mặt phía sau
        0, 4, 5,
        0, 5, 1,
        // Mặt bên trái
        0, 3, 7,
        0, 7, 4,
        // Mặt bên phải
        1, 5, 6,
        1, 6, 2
};

// Setup buffers for combined vertices and indices
void SetupBuffers() {
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    // Bind VBO
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Bind EBO
    glGenBuffers(1, &EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

// Hàm render
void Render() {
    // Xóa bộ đệm màu và độ sâu
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Kích hoạt shader
    glUseProgram(shaderProgram);

    // Liên kết VAO
    glBindVertexArray(VAO);

    // Bật độ sâu
    glEnable(GL_DEPTH_TEST);

    // Vẽ hình hộp chữ nhật
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0); // 36 = 6 mặt * 2 tam giác/mặt * 3 chỉ số/tam giác

    // Liên kết lại VAO
    glBindVertexArray(0);
}


extern "C" JNIEXPORT void JNICALL
Java_com_vinai_testglkotlin_MainActivity_initSurface(JNIEnv *env, jobject instance, jobject j_surface, jstring picturesDir) {
    LOGI("Java_com_vinai_testglkotlin_MainActivity_initSurface");

    // Get native window from the surface
    window = ANativeWindow_fromSurface(env, j_surface);
    running = true;

    // Start the render thread
    renderThread = std::thread([] {
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

        // Thiết lập buffers
        SetupBuffers();

        // Main render loop
        while (running) {
            // Clear buffers
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glClearColor(0,0,0,0);

            // Render both rectangles
            Render();

            // Swap EGL buffers
            eglSwapBuffers(display, surface);
        }

        // Cleanup OpenGL resources
        deinit_gl(width, height);
        GLHelper_releaseSurface(surface);
        GLHelper_releaseContext(context);
    });
}