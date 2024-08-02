#include <jni.h>
#include <string>
#include <iostream>
#include <GLES3/gl3.h>
#include "gl_helper.h"
#include <android/native_window.h>
#include <android/bitmap.h>
#include <android/native_window_jni.h>
#include <thread>
#include <mutex>
#include "renderer.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <android/log.h>
#include <EGL/eglext.h>

#define LOG_TAG "Native"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

ANativeWindow* window = nullptr;
EGLContext context;
EGLSurface surface;
EGLDisplay display;
GLuint shaderProgram;
int width = 0;
int height = 0;
bool running = false;
std::thread renderThread;
bool triggerUpdateSize = true;
std::mutex mMutex;

GLuint CreateShaderProgram();
void SetupBuffers();
void DrawRectangle(GLuint shaderProgram);

extern "C" JNIEXPORT jstring JNICALL
Java_com_vinai_testglkotlin_MainActivity_stringFromJNI(JNIEnv *env, jobject instance) {
    return env->NewStringUTF("Xin chao");
}

extern "C" JNIEXPORT void JNICALL
Java_com_vinai_testglkotlin_MainActivity_initSurface(JNIEnv *env, jobject instance, jobject j_surface) {
    LOGI("Java_com_vinai_testglkotlin_MainActivity_initSurface");
    window = ANativeWindow_fromSurface(env, j_surface);
    running = true;
    renderThread = std::thread([]{
        LOGI("Start render thread");
        display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if(!GLHelper_initGL(EGL_NO_CONTEXT, window, &context, &surface)) {
            LOGE("InitGL failed");
            return;
        }
        eglMakeCurrent(display, surface, surface, context);
        GLHelper_getSurfaceSize(surface, width, height);
        init_gl(width, height);
        // Tạo shader program
        shaderProgram = CreateShaderProgram();

        // Thiết lập buffers và VAO
        SetupBuffers();
        while (running) {
            bool willUpdateSize = false;
            {
                std::lock_guard<std::mutex> lk(mMutex);
                willUpdateSize = triggerUpdateSize;
                triggerUpdateSize = false;
            }
            if (willUpdateSize) {
                GLHelper_getSurfaceSize(surface, width, height);
                resize_gl(width, height);
            }
//            render_gl(width, height);
            DrawRectangle(shaderProgram);
            eglSwapBuffers(display, surface);

            glClearColor(1,1,1,1);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }
        deinit_gl(width, height);
        GLHelper_releaseSurface(surface);
        GLHelper_releaseContext(context);
    });
}

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

// Vertex shader source code
const char* vertexShaderSource = R"(
#version 310 es
precision mediump float;
layout(location = 0) in vec3 vPosition;
layout(location = 1) in vec3 color;
out vec2 texCoord;
out vec3 _color;
void main() {
    gl_Position = vec4(vPosition, 1.0);
    _color = color;
}
)";

// Fragment shader source code
const char* fragmentShaderSource = R"(
#version 310 es
precision mediump float;
in vec2 texCoord;
in vec3 _color;
out vec3 fragColor;
uniform sampler2D textureSampler;
void main() {
    fragColor = _color;
}
)";

// Compile shader and check for errors
GLuint CompileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

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

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    GLint success;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
        LOGE("Program linking error: %s", infoLog);
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}

// Vertices coordinates
const GLfloat vertices[] =
        { //     COORDINATES     /        COLORS      /   TexCoord  //
                -0.5f, -0.5f, 0.0f,     1.0f, 0.0f, 0.0f,   0.0f, 0.0f, // Lower left corner
                -0.5f,  0.5f, 0.0f,     0.0f, 1.0f, 0.0f,   0.0f, 1.0f, // Upper left corner
                0.5f,  0.5f, 0.0f,     0.0f, 0.0f, 1.0f,    1.0f, 1.0f, // Upper right corner
                0.5f, -0.5f, 0.0f,     1.0f, 1.0f, 1.0f,    1.0f, 0.0f  // Lower right corner
        };

const GLuint indices[] = {
        0, 1, 2,
        0, 2, 3
};

GLuint VBO, VAO, EBO;

void SetupBuffers() {
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // coord
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(0);

    // color
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

//    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)(2 * sizeof(GLfloat)));
//    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
}

void DrawRectangle(GLuint shaderProgram) {
    eglMakeCurrent(display, surface, surface, context);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(shaderProgram);
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

extern "C" JNIEXPORT void JNICALL
Java_com_vinai_testglkotlin_MainActivity_drawRectangle(JNIEnv *env, jobject thiz) {
    // Vẽ hình chữ nhật
    DrawRectangle(shaderProgram);

    eglSwapBuffers(display, surface);
}
//extern "C" JNIEXPORT void JNICALL
//Java_com_vinai_testglkotlin_MainActivity_loadTextureFromFile(JNIEnv *env, jobject thiz, jobject _surface, jstring picturesDir) {
//
//    // Chuyển đổi jstring thành chuỗi C
//    const char* path = env->GetStringUTFChars(picturesDir, nullptr);
//    window = ANativeWindow_fromSurface(env, _surface);
//
//    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
//    GLHelper_initGL(EGL_NO_CONTEXT, window, &context, &surface);
//    eglMakeCurrent(display, surface, surface, context);
//    int surfaceWidth = 0;
//    int surfaceHeight = 0;
//    GLHelper_getSurfaceSize(surface, surfaceWidth, surfaceHeight);
//
//    // Tải hình ảnh bằng stb_image
//    int width, height, nrChannels;
//    unsigned char* data = stbi_load("/storage/emulated/0/Download/1045-2.jpg", &width, &height, &nrChannels, 0);
//    if (data == nullptr) {
//        LOGE("Failed to load image: %s", "/storage/emulated/0/Download/1045-2.jpg");
//        env->ReleaseStringUTFChars(picturesDir, "/storage/emulated/0/Download/1045-2.jpg");
//        return;
//    }
//
//    glClearColor(0, 0, 1, 1);
//    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
//
//    // Tạo texture OpenGL
//    GLuint textureID;
//    glGenTextures(1, &textureID);
//    glBindTexture(GL_TEXTURE_2D, textureID);
//
//    // Cài đặt các thông số cho texture
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
//
//    // Tải dữ liệu hình ảnh vào texture
//    GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
//    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
//
//    GLuint readFramebuffer = 0;
//    glGenFramebuffers(1, &readFramebuffer);
//
//    glBindFramebuffer(GL_READ_FRAMEBUFFER, readFramebuffer);
//    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureID, 0);
//    glBlitFramebuffer(0, 0, width, height, (surfaceWidth - width) / 2, (surfaceHeight - height) / 2, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
//
////    // Clean up resources
////    glDeleteVertexArrays(1, &VAO);
////    glDeleteBuffers(1, &VBO);
////    glDeleteBuffers(1, &EBO);
//
//    eglSwapBuffers(display, surface);
//
//    // Giải phóng OpenGL
//    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
//    GLHelper_releaseSurface(surface);
//    GLHelper_releaseContext(context);
//
//    // Giải phóng dữ liệu hình ảnh sau khi tải vào texture
//    stbi_image_free(data);
//    env->ReleaseStringUTFChars(picturesDir, path);
//}