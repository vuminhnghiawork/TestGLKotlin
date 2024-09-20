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
GLuint movingRectangleProgram;
int width = 0;
int height = 0;
bool running = false;
std::thread renderThread;
bool triggerUpdateSize = true;
std::mutex mMutex;

GLuint textureID = 0;
GLuint CreateShaderProgram();
GLuint CreateMovingRectangleShaderProgram();
GLuint vbo[2];
GLuint vao[2];
GLuint ebo[2];
GLuint VBO, VAO, EBO;
float rectangleOffset = 0.0f;
void SetupBuffers();
void DrawRectangle(GLuint shaderProgram);
void DrawMovingRectangle(GLuint movingRectangleProgram);
void loadTextureFromFile(const char* pictureDir);
void renderLoop();

void glUniform2f(GLuint i, float d);

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
        movingRectangleProgram = CreateMovingRectangleShaderProgram();

        // Thiết lập buffers và VAO
        SetupBuffers();
        glClearColor(0,0,0,0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        loadTextureFromFile(path);

        while (running) {
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // Vẽ hình chữ nhật
            DrawRectangle(shaderProgram);

            // Cập nhật vị trí hình chữ nhật di chuyển
            rectangleOffset += 0.005f;
            if (rectangleOffset > 2.0f) {
                rectangleOffset = 0.0f;
            }

            // Vẽ đường di chuyển
            DrawMovingRectangle(movingRectangleProgram);

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

// Moving Rectangle shaders source code
const char* movingRectangleVertexShaderSource = R"(
#version 310 es
precision mediump float;
layout(location = 0) in vec3 aPosition;  // Đổi tên từ vPosition thành aPosition
out vec2 vPos;  // Truyền giá trị vị trí cho fragment shader
uniform float rectangleOffset;

void main() {
    gl_Position = vec4(aPosition.x + rectangleOffset - 1.0, aPosition.y, aPosition.z, 1.0);
    vPos = gl_Position.xy;  // Lấy giá trị vị trí x, y của đỉnh
}
)";



const char* movingRectangleFragmentShaderSource = R"(
#version 310 es
precision mediump float;
out vec4 fragColor;
in vec2 vPos;  // Nhận giá trị vị trí từ vertex shader

// Giới hạn vùng vẽ
uniform float leftLimit;
uniform float rightLimit;
uniform float bottomLimit;
uniform float topLimit;

void main() {

    float distanceFromCenter = abs(vPos.x);  // Tính khoảng cách từ tâm (vPos.x = 0)
    float fadeFactor = 1.0 - distanceFromCenter;  // Giảm độ đậm dần khi đi xa tâm
    fadeFactor = max(fadeFactor, 0.0);  // Đảm bảo giá trị không âm


    // Kiểm tra nếu pixel nằm ngoài vùng được xác định
    if (vPos.x < leftLimit || vPos.x > rightLimit || vPos.y < bottomLimit || vPos.y > topLimit) {
        fadeFactor = 0.0;  // Bỏ qua pixel nếu nằm ngoài vùng giới hạn


    }
    fragColor = vec4(1.0, 1.0, 0.0, fadeFactor);  // Màu vàng với alpha dựa trên fadeFactor
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

GLuint CreateMovingRectangleShaderProgram() {
    GLuint vertexShader = CompileShader(GL_VERTEX_SHADER, movingRectangleVertexShaderSource);
    GLuint fragmentShader = CompileShader(GL_FRAGMENT_SHADER, movingRectangleFragmentShaderSource);

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    // Kiểm tra lỗi
    GLint success;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
        LOGE("Moving Rectangle shader program linking error: %s", infoLog);
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}

// Vertices coordinates
const GLfloat vertices[] = {
        // COORDINATES     /   TexCoord
        -0.5, -0.5f, 0.0f,   0.0f, 0.0f,  // Lower left corner
        -0.5,  0.5f, 0.0f,   0.0f, 1.0f,  // Upper left corner
        0.5,  0.5f, 0.0f,    1.0f, 1.0f,  // Upper right corner
        0.5, -0.5f, 0.0f,    1.0f, 0.0f   // Lower right corner
};

const GLuint indices[] = {
        0, 1, 2,
        0, 2, 3
};

const GLfloat movingRectangleVertices[] = {
        -0.5f, -0.5f, 0.0f,   // Góc dưới trái
        0.5f, -0.5f, 0.0f,    // Góc dưới phải
        0.5f,  0.5f, 0.0f,    // Góc trên phải
        -0.5f,  0.5f, 0.0f    // Góc trên trái
};

const GLuint movingRectangleIndices[] = {
        0, 1, 2,
        0, 2, 3
};

void SetupBuffers() {
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(0);

    // TexCoord attribute
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    glGenVertexArrays(1, &vao[1]);
    glGenBuffers(1, &vbo[2]);
    glGenBuffers(1, &ebo[1]);

    glBindVertexArray(vao[1]);

    glBindBuffer(GL_ARRAY_BUFFER, vbo[2]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(movingRectangleVertices), movingRectangleVertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo[1]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(movingRectangleIndices), movingRectangleIndices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (GLvoid*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

void DrawRectangle(GLuint shaderProgram) {
    glUseProgram(shaderProgram);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void DrawMovingRectangle(GLuint movingRectangleProgram) {
    // Sử dụng chương trình shader
    glUseProgram(movingRectangleProgram);

    // Truyền giá trị cho uniform rectangleOffset
    GLuint offsetLocation = glGetUniformLocation(movingRectangleProgram, "rectangleOffset");


    // Lấy vị trí của các biến uniform giới hạn vùng vẽ
    GLint leftLimitLoc = glGetUniformLocation(movingRectangleProgram, "leftLimit");
    GLint rightLimitLoc = glGetUniformLocation(movingRectangleProgram, "rightLimit");
    GLint bottomLimitLoc = glGetUniformLocation(movingRectangleProgram, "bottomLimit");
    GLint topLimitLoc = glGetUniformLocation(movingRectangleProgram, "topLimit");

    // Thiết lập giá trị cho các uniform giới hạn vùng vẽ
    glUniform1f(leftLimitLoc, -0.5f);
    glUniform1f(rightLimitLoc, 0.5f);
    glUniform1f(bottomLimitLoc, -0.5f);
    glUniform1f(topLimitLoc, 0.5f);
    glUniform1f(offsetLocation, rectangleOffset);

    // Bật chế độ blending trước khi vẽ để đảm bảo hiệu ứng alpha hoạt động
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Vẽ hình chữ nhật di chuyển
    glBindVertexArray(vao[1]);  // Sử dụng VAO đã lưu
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);  // Vẽ các phần tử
    glBindVertexArray(0);  // Hủy liên kết VAO

    // Tắt chế độ blending sau khi vẽ (nếu cần)
    glDisable(GL_BLEND);
}


void loadTextureFromFile(const char* pictureDir) {
    LOGD("Loading texture from: %s", pictureDir);

    // Tải hình ảnh bằng stb_image
    int width, height, nrChannels;
    unsigned char* data = stbi_load(pictureDir, &width, &height, &nrChannels, 0);
    if (data == nullptr) {
        LOGE("Failed to load image: %s", pictureDir);
        return;
    }

    // Tạo texture OpenGL
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    // Cài đặt các thông số cho texture
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Tải dữ liệu hình ảnh vào texture
    GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);

    // Giải phóng dữ liệu hình ảnh
    stbi_image_free(data);
    glBindTexture(GL_TEXTURE_2D, 0);
}
