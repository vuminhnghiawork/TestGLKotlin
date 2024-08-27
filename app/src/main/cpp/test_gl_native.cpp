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

GLuint textureID = 0;
GLuint CreateShaderProgram();
GLuint CreateLineShaderProgram();
GLuint lineProgram;
GLuint vbo[2];
float lineOffset = 0.0f;
void SetupBuffers();
void DrawRectangle(GLuint shaderProgram);
void DrawMovingLine(GLuint lineProgram);
void loadTextureFromFile(const char* pictureDir);
void renderLoop();


extern "C" JNIEXPORT void JNICALL
Java_com_vinai_testglkotlin_MainActivity_initSurface(JNIEnv *env, jobject instance, jobject j_surface, jstring picturesDir) {
    LOGI("Java_com_vinai_testglkotlin_MainActivity_initSurface");
    window = ANativeWindow_fromSurface(env, j_surface);
    running = true;
    const char* path = env->GetStringUTFChars(picturesDir, nullptr);
    renderThread = std::thread([path]{
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
        lineProgram = CreateLineShaderProgram();

        // Thiết lập buffers và VAO
        SetupBuffers();
        glClearColor(1,0,0,0);
//        glClearColor(1, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        loadTextureFromFile(path);

        while (running) {
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // Vẽ hình chữ nhật
            DrawRectangle(shaderProgram);

            // Cập nhật vị trí đường di chuyển
            lineOffset += 0.01f;
            if (lineOffset > 2.0f) {
                lineOffset = -1.0f;
            }

            // Vẽ đường di chuyển
            DrawMovingLine(lineProgram);

            eglSwapBuffers(display, surface);
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

// Rectangle shader source code
const char* vertexShaderSource = R"(
#version 310 es
precision mediump float;
layout(location = 0) in vec3 vPosition;
layout(location = 1) in vec2 vTexCoord;
out vec2 texCoord;
void main() {
    gl_Position = vec4(vPosition, 1.0);
    texCoord = vTexCoord;
}
)";

const char* fragmentShaderSource = R"(
#version 310 es
precision mediump float;
in vec2 texCoord;
out vec4 fragColor;
uniform sampler2D textureSampler;
void main() {
    fragColor = texture(textureSampler, texCoord);
}
)";

// Line shaders source code
const char* lineVertexShaderSource = R"(
    #version 310 es
    layout(location = 0) in vec4 aPosition;
    uniform float uOffset;
    out vec2 vPosition;
    void main() {
        gl_Position = aPosition + vec4(uOffset, 0.0, 0.0, 0.0);
        vPosition = gl_Position.xy;
    }
)";

const char* lineFragmentShaderSource = R"(
    #version 310 es
    precision mediump float;
    in vec2 vPosition;
    out vec4 fragColor;
    void main() {
        float brightness = 1.0 - (vPosition.x + 1.0) / 2.0;
        brightness = pow(brightness, 2.0);  // Tăng cường độ sáng ở bên trái
        fragColor = vec4(1.0, 1.0, 1.0, brightness);+
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

GLuint CreateLineShaderProgram() {
    GLuint vertexShader = CompileShader(GL_VERTEX_SHADER, lineVertexShaderSource);
    GLuint fragmentShader = CompileShader(GL_FRAGMENT_SHADER, lineFragmentShaderSource);

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    GLint success;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
        LOGE("Line shader program linking error: %s", infoLog);
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}


// Vertices coordinates
const GLfloat vertices[] =
        { //     COORDINATES     /        COLORS      /   TexCoord  //
                -0.5, -0.5f, 0.0f,     1.0f, 0.0f, 0.0f,   0.0f, 0.0f, // Lower left corner
                -0.5,  0.5f, 0.0f,     0.0f, 1.0f, 0.0f,   0.0f, 1.0f, // Upper left corner
                0.5,  0.5f, 0.0f,     0.0f, 0.0f, 1.0f,    1.0f, 1.0f, // Upper right corner
                0.5, -0.5f, 0.0f,     1.0f, 1.0f, 1.0f,    1.0f, 0.0f  // Lower right corner
        };

const GLuint indices[] = {
        0, 1, 2,
        0, 2, 3
};

GLfloat lineVertices[] = {
        -1, 0,
        1, 0

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


    //Texture coordinate
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)(6 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    // Setup VBO cho đường di chuyển
    glGenBuffers(1, &vbo[1]);
    glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(lineVertices), lineVertices, GL_STATIC_DRAW);
}

void DrawRectangle(GLuint shaderProgram) {
//    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(shaderProgram);
    glBindVertexArray(VAO);
    //Bind texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glUniform1i(glGetUniformLocation(shaderProgram, "textureSampler"), 0);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindVertexArray(0);
}

void DrawMovingLine (GLuint lineProgram) {
// Draw moving line
glUseProgram(lineProgram);
GLuint offsetLocation = glGetUniformLocation(lineProgram, "uOffset");
glUniform1f(offsetLocation, lineOffset - 1.0f);

glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
glEnableVertexAttribArray(0);
glDrawArrays(GL_LINES, 0, 2);

glFinish();
}

void loadTextureFromFile(const char* picturesDir) {
    LOGD("display=%d, surface=%d, context=%d", display, surface, context);

    // Tải hình ảnh bằng stb_image
    int width, height, nrChannels;
    unsigned char* data = stbi_load(picturesDir, &width, &height, &nrChannels, 0);
    if (data == nullptr) {
        LOGE("Failed to load image: %s", "/storage/emulated/0/Download/1045-2.jpg");

        return;
    } else {
        LOGD("Load texture successfully, Width: %d, Height: %d, nrChannels: %x", width, height, nrChannels);
    }

    // Tạo texture OpenGL
    GLuint tmpTextureID;
    glGenTextures(1, &tmpTextureID);
    glBindTexture(GL_TEXTURE_2D, tmpTextureID);

    // Cài đặt các thông số cho texture
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);


    // Tải dữ liệu hình ảnh vào texture
    GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    // Giải phóng dữ liệu hình ảnh sau khi tải vào texture
    stbi_image_free(data);
    glBindTexture(GL_TEXTURE_2D, 0);

    GLuint readFramebuffer = 0;
    glGenFramebuffers(1, &readFramebuffer);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, readFramebuffer);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tmpTextureID, 0);
    glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    textureID = tmpTextureID;

}