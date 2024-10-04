//#include <jni.h>
//#include <string>
//#include <iostream>
//#include <GLES3/gl3.h>
//#include "gl_helper.h"
//#include <android/native_window.h>
//#include <android/bitmap.h>
//#include <android/native_window_jni.h>
//#include <thread>
//#include <mutex>
//#include "renderer.h"
//#define STB_IMAGE_IMPLEMENTATION
//#include "stb_image.h"
//#include <android/log.h>
//#include <EGL/eglext.h>
//
//#define LOG_TAG "Native"
//#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
//#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
//#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
//
//ANativeWindow* window = nullptr;
//EGLContext context;
//EGLSurface surface;
//EGLDisplay display;
//GLuint shaderProgram;
//GLuint movingRectangleProgram;
//int width = 0;
//int height = 0;
//bool running = false;
//std::thread renderThread;
//bool triggerUpdateSize = true;
//std::mutex mMutex;
//
//GLuint textureID = 0;
//GLuint CreateShaderProgram();
//GLuint CreateMovingRectangleShaderProgram();
//GLuint vbo[2];
//GLuint vao[2];
//GLuint ebo[2];
//GLuint VBO, VAO, EBO;
//float rectangleOffset = -2.0f;
//void SetupBuffers();
//void DrawRectangle(GLuint shaderProgram);
//void DrawMovingRectangle(GLuint movingRectangleProgram);
//void loadTextureFromFile(const char* pictureDir);
//void Draw();
//void renderLoop();
//
//extern "C" JNIEXPORT void JNICALL
//Java_com_vinai_testglkotlin_MainActivity_initSurface(JNIEnv *env, jobject instance, jobject j_surface, jstring picturesDir) {
//    LOGI("Java_com_vinai_testglkotlin_MainActivity_initSurface");
//    window = ANativeWindow_fromSurface(env, j_surface);
//    running = true;
//    const char* path = env->GetStringUTFChars(picturesDir, nullptr);
//    renderThread = std::thread([path]{
//        LOGI("Start render thread");
//        display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
//        if(!GLHelper_initGL(EGL_NO_CONTEXT, window, &context, &surface)) {
//            LOGE("InitGL failed");
//            return;
//        }
//        eglMakeCurrent(display, surface, surface, context);
//        GLHelper_getSurfaceSize(surface, width, height);
//        init_gl(width, height);
//
//        // Tạo shader program
//        shaderProgram = CreateShaderProgram();
//        movingRectangleProgram = CreateMovingRectangleShaderProgram();
//
//        // Thiết lập buffers và VAO
//        SetupBuffers();
//        glClearColor(0,0,0,0);
//        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
//        loadTextureFromFile(path);
//
//        while (running) {
//            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
//
//            Draw();
//
//            // Cập nhật vị trí hình chữ nhật di chuyển
//            rectangleOffset += 0.005f;
//            if (rectangleOffset > 2.0f) {
//                rectangleOffset = -2.0f;
//            }
//
//            eglSwapBuffers(display, surface);
//        }
//
//        deinit_gl(width, height);
//        GLHelper_releaseSurface(surface);
//        GLHelper_releaseContext(context);
//    });
//}
//
//extern "C" JNIEXPORT void JNICALL
//Java_com_vinai_testglkotlin_MainActivity_surfaceResize(JNIEnv *env, jobject instance) {
//    std::lock_guard<std::mutex> lk(mMutex);
//    triggerUpdateSize = true;
//}
//
//extern "C" JNIEXPORT void JNICALL
//Java_com_vinai_testglkotlin_MainActivity_deinitSurface(JNIEnv *env, jobject instance) {
//    running = false;
//    renderThread.join();
//    ANativeWindow_release(window);
//}
//
//// Rectangle shader source code
//const char* vertexShaderSource = R"(
//#version 300 es
//precision mediump float;
//layout(location = 0) in vec3 vPosition;
//layout(location = 1) in vec2 vTexCoord;
//out vec2 texCoord;
//void main() {
//    gl_Position = vec4(vPosition, 1.0);
//    texCoord = vTexCoord;
//}
//)";
//
//const char* fragmentShaderSource = R"(
//#version 300 es
//precision mediump float;
//in vec2 texCoord;
//out vec4 fragColor;
//uniform sampler2D textureSampler;
//void main() {
//    fragColor = texture(textureSampler, texCoord);
//}
//)";
//
//// Moving Rectangle shaders source code
//const char* movingRectangleVertexShaderSource = R"(
//#version 300 es
//precision mediump float;
//layout(location = 0) in vec3 aPosition;  // Đổi tên từ vPosition thành aPosition
//out vec2 vPos;  // Truyền giá trị vị trí cho fragment shader
//out vec2 vPos1;
//uniform float rectangleOffset;
//
//void main() {
//    gl_Position = vec4(aPosition.x + rectangleOffset, aPosition.y, aPosition.z, 1.0);
//    vPos = aPosition.xy;  // Lấy giá trị vị trí x, y của đỉnh
//    vPos1 = gl_Position.xy;
//}
//)";
//
//
//
//const char* movingRectangleFragmentShaderSource = R"(
//#version 300 es
//precision mediump float;
//out vec4 fragColor;
//in vec2 vPos;  // Nhận giá trị vị trí từ vertex shader
//in vec2 vPos1;
//
//// Giới hạn vùng vẽ
//uniform float leftLimit;
//uniform float rightLimit;
//uniform float bottomLimit;
//uniform float topLimit;
//
//void main() {
//
//    // Kiểm tra nếu pixel nằm ngoài vùng được xác định
//    if (vPos1.x < leftLimit || vPos1.x > rightLimit || vPos1.y < bottomLimit || vPos1.y > topLimit) {
//        discard;  // Bỏ qua pixel nếu nằm ngoài vùng giới hạn
//    }
//    else {
//        float distanceFromCenter = abs(vPos.x);  // Tính khoảng cách từ tâm (vPos.x = 0)
//        float fadeFactor = 1.0 - distanceFromCenter;  // Giảm độ đậm dần khi đi xa tâm
//        fadeFactor = max(fadeFactor, 0.0);  // Đảm bảo giá trị không âm
//        fragColor = vec4(1.0, 1.0, 0.0, fadeFactor);  // Màu vàng với alpha dựa trên fadeFactor
//    }
//}
//)";
//
//
//
//// Compile shader and check for errors
//GLuint CompileShader(GLenum type, const char* source) {
//    GLuint shader = glCreateShader(type);
//    glShaderSource(shader, 1, &source, nullptr);
//    glCompileShader(shader);
//
//    GLint success;
//    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
//    if (!success) {
//        char infoLog[512];
//        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
//        LOGE("Shader compilation error: %s", infoLog);
//    }
//
//    return shader;
//}
//
//// Create and link shader program
//GLuint CreateShaderProgram() {
//    GLuint vertexShader = CompileShader(GL_VERTEX_SHADER, vertexShaderSource);
//    GLuint fragmentShader = CompileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
//
//    GLuint shaderProgram = glCreateProgram();
//    glAttachShader(shaderProgram, vertexShader);
//    glAttachShader(shaderProgram, fragmentShader);
//    glLinkProgram(shaderProgram);
//
//    GLint success;
//    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
//    if (!success) {
//        char infoLog[512];
//        glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
//        LOGE("Program linking error: %s", infoLog);
//    }
//
//    glDeleteShader(vertexShader);
//    glDeleteShader(fragmentShader);
//
//    return shaderProgram;
//}
//
//GLuint CreateMovingRectangleShaderProgram() {
//    GLuint vertexShader = CompileShader(GL_VERTEX_SHADER, movingRectangleVertexShaderSource);
//    GLuint fragmentShader = CompileShader(GL_FRAGMENT_SHADER, movingRectangleFragmentShaderSource);
//
//    GLuint shaderProgram = glCreateProgram();
//    glAttachShader(shaderProgram, vertexShader);
//    glAttachShader(shaderProgram, fragmentShader);
//    glLinkProgram(shaderProgram);
//
//    // Kiểm tra lỗi
//    GLint success;
//    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
//    if (!success) {
//        char infoLog[512];
//        glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
//        LOGE("Moving Rectangle shader program linking error: %s", infoLog);
//    }
//
//    glDeleteShader(vertexShader);
//    glDeleteShader(fragmentShader);
//
//    return shaderProgram;
//}
//
//// Vertices coordinates
//const GLfloat vertices[] = {
//        // COORDINATES     /   TexCoord
//        -0.5f, -0.5f, 0.0f,   1.0f, 1.0f,  // Lower left corner
//        -0.5f,  0.5f, 0.0f,   1.0f, -1.0f,  // Upper left corner
//        0.5f,  0.5f, 0.0f,    -1.0f, -1.0f,  // Upper right corner
//        0.5f, -0.5f, 0.0f,    -1.0f, 1.0f   // Lower right corner
//};
//
//const GLuint indices[] = {
//        0, 1, 2,
//        0, 2, 3
//};
//
//const GLfloat movingRectangleVertices[] = {
//        -0.5f, -0.5f, 0.0f,   // Góc dưới trái
//        0.5f, -0.5f, 0.0f,    // Góc dưới phải
//        0.5f,  0.5f, 0.0f,    // Góc trên phải
//        -0.5f,  0.5f, 0.0f    // Góc trên trái
//};
//
//const GLuint movingRectangleIndices[] = {
//        0, 1, 2,
//        0, 2, 3
//};
//
//void SetupBuffers() {
//    glGenVertexArrays(1, &VAO);
//    glGenBuffers(1, &VBO);
//    glGenBuffers(1, &EBO);
//
//    glBindVertexArray(VAO);
//
//    glBindBuffer(GL_ARRAY_BUFFER, VBO);
//    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
//
//    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
//    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
//
//    // Position attribute
//    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (GLvoid*)0);
//    glEnableVertexAttribArray(0);
//
//    // TexCoord attribute
//    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
//    glEnableVertexAttribArray(1);
//
//    glBindVertexArray(0);
//
//    glGenVertexArrays(1, &vao[1]);
//    glGenBuffers(1, &vbo[2]);
//    glGenBuffers(1, &ebo[1]);
//
//    glBindVertexArray(vao[1]);
//
//    glBindBuffer(GL_ARRAY_BUFFER, vbo[2]);
//    glBufferData(GL_ARRAY_BUFFER, sizeof(movingRectangleVertices), movingRectangleVertices, GL_STATIC_DRAW);
//
//    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo[1]);
//    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(movingRectangleIndices), movingRectangleIndices, GL_STATIC_DRAW);
//
//    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (GLvoid*)0);
//    glEnableVertexAttribArray(0);
//
//    glBindVertexArray(0);
//}
//
//void Draw() {
//    // Xóa bộ đệm màu và độ sâu
//    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
//
//    // Vẽ hình chữ nhật tĩnh
//    glUseProgram(shaderProgram);
//    glActiveTexture(GL_TEXTURE0);
//    glBindTexture(GL_TEXTURE_2D, textureID);
//    glBindVertexArray(VAO);
//    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
//    glBindVertexArray(0);
//
//    // Vẽ hình chữ nhật di chuyển
//    glUseProgram(movingRectangleProgram);
//
//    // Truyền giá trị cho uniform rectangleOffset
//    GLuint offsetLocation = glGetUniformLocation(movingRectangleProgram, "rectangleOffset");
//
//    // Lấy vị trí của các biến uniform giới hạn vùng vẽ
//    GLint leftLimitLoc = glGetUniformLocation(movingRectangleProgram, "leftLimit");
//    GLint rightLimitLoc = glGetUniformLocation(movingRectangleProgram, "rightLimit");
//    GLint bottomLimitLoc = glGetUniformLocation(movingRectangleProgram, "bottomLimit");
//    GLint topLimitLoc = glGetUniformLocation(movingRectangleProgram, "topLimit");
//
//    // Thiết lập giá trị cho các uniform giới hạn vùng vẽ
//    glUniform1f(leftLimitLoc, -0.5f);
//    glUniform1f(rightLimitLoc, 0.5f);
//    glUniform1f(bottomLimitLoc, -0.5f);
//    glUniform1f(topLimitLoc, 0.5f);
//    glUniform1f(offsetLocation, rectangleOffset);
//
//    // Bật chế độ blending
//    glEnable(GL_BLEND);
//    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
//
//    // Vẽ hình chữ nhật di chuyển
//    glBindVertexArray(vao[1]);
//    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
//    glBindVertexArray(0);
//
//    // Tắt chế độ blending sau khi vẽ
//    glDisable(GL_BLEND);
//}
//
////void DrawRectangle(GLuint shaderProgram) {
////    glUseProgram(shaderProgram);
////
////    glActiveTexture(GL_TEXTURE0);
////    glBindTexture(GL_TEXTURE_2D, textureID);
////
////    glBindVertexArray(VAO);
////    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
////    glBindVertexArray(0);
////}
////
////void DrawMovingRectangle(GLuint movingRectangleProgram) {
////    // Sử dụng chương trình shader
////    glUseProgram(movingRectangleProgram);
////
////    // Truyền giá trị cho uniform rectangleOffset
////    GLuint offsetLocation = glGetUniformLocation(movingRectangleProgram, "rectangleOffset");
////
////
////    // Lấy vị trí của các biến uniform giới hạn vùng vẽ
////    GLint leftLimitLoc = glGetUniformLocation(movingRectangleProgram, "leftLimit");
////    GLint rightLimitLoc = glGetUniformLocation(movingRectangleProgram, "rightLimit");
////    GLint bottomLimitLoc = glGetUniformLocation(movingRectangleProgram, "bottomLimit");
////    GLint topLimitLoc = glGetUniformLocation(movingRectangleProgram, "topLimit");
////
////    // Thiết lập giá trị cho các uniform giới hạn vùng vẽ
////    glUniform1f(leftLimitLoc, -0.5f);
////    glUniform1f(rightLimitLoc, 0.5f);
////    glUniform1f(bottomLimitLoc, -0.5f);
////    glUniform1f(topLimitLoc, 0.5f);
////    glUniform1f(offsetLocation, rectangleOffset);
////
////    // Bật chế độ blending trước khi vẽ để đảm bảo hiệu ứng alpha hoạt động
////    glEnable(GL_BLEND);
////    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
////
////    // Vẽ hình chữ nhật di chuyển
////    glBindVertexArray(vao[1]);  // Sử dụng VAO đã lưu
////    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);  // Vẽ các phần tử
////    glBindVertexArray(0);  // Hủy liên kết VAO
////
////    // Tắt chế độ blending sau khi vẽ (nếu cần)
////    glDisable(GL_BLEND);
////}
//
//
//void loadTextureFromFile(const char* pictureDir) {
//    LOGD("Loading texture from: %s", pictureDir);
//
//    // Tải hình ảnh bằng stb_image
//    int width, height, nrChannels;
//    unsigned char* data = stbi_load(pictureDir, &width, &height, &nrChannels, 0);
//    if (data == nullptr) {
//        LOGE("Failed to load image: %s", pictureDir);
//        return;
//    }
//
//    // Tạo texture OpenGL
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
//    // Giải phóng dữ liệu hình ảnh
//    stbi_image_free(data);
//    glBindTexture(GL_TEXTURE_2D, 0);
//}


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
GLuint CreateShaderProgram();
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
            LOGE("Failed to create shader program.");
            return;
        }

        // Setup combined buffers
        SetupCombinedBuffers();

        // Load texture from file using the copied path
        loadTextureFromFile(pathStr.c_str());

        // Main render loop
        while (running) {
            // Clear buffers
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // Render both rectangles
            RenderCombinedRectangles(shaderProgram);

            // Update the moving rectangle's position
            rectangleOffset += 0.005f;
            if (rectangleOffset > 2.0f) {
                rectangleOffset = -2.0f;
            }

            // Swap EGL buffers
            eglSwapBuffers(display, surface);
        }

        // Cleanup OpenGL resources
        deinit_gl(width, height);
        GLHelper_releaseSurface(surface);
        GLHelper_releaseContext(context);
    });
}



// Shader sources
const char* combinedVertexShaderSource = R"(
#version 310 es
precision mediump float;
layout(location = 0) in vec3 vPosition;
layout(location = 1) in vec2 vTexCoord;
layout(location = 2) in float vType; // 0 for static, 1 for moving rectangle

out vec2 texCoord;
out vec2 vPos;
flat out float fragType;

uniform float rectangleOffset;

void main() {
    if (vType == 1.0) {  // Moving rectangle
        gl_Position = vec4(vPosition.x + rectangleOffset, vPosition.y, vPosition.z, 1.0);
    } else {  // Static rectangle
        gl_Position = vec4(vPosition, 1.0);
    }
    texCoord = vTexCoord;
    vPos = vPosition.xy;
    fragType = vType;
}
)";

const char* combinedFragmentShaderSource = R"(
#version 310 es
precision mediump float;
in vec2 texCoord;
in vec2 vPos;
flat in float fragType;

out vec4 fragColor;

uniform sampler2D textureSampler;  // For static rectangle texture
uniform float leftLimit;
uniform float rightLimit;
uniform float bottomLimit;
uniform float topLimit;

void main() {
    if (fragType == 1.0) {  // Moving rectangle
        if (gl_FragCoord.x < leftLimit || gl_FragCoord.x > rightLimit ||
            gl_FragCoord.y < bottomLimit || gl_FragCoord.y > topLimit) {
            discard;
        } else {
            float distanceFromCenter = abs(vPos.x);
            float fadeFactor = 1.0 - distanceFromCenter;
            fadeFactor = max(fadeFactor, 0.0);
            fragColor = vec4(1.0, 1.0, 0.0, fadeFactor);  // Yellow color with fade effect
        }
    } else {  // Static rectangle
        fragColor = texture(textureSampler, texCoord);
    }
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
    GLuint vertexShader = CompileShader(GL_VERTEX_SHADER, combinedVertexShaderSource);
    GLuint fragmentShader = CompileShader(GL_FRAGMENT_SHADER, combinedFragmentShaderSource);

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

// Combined vertices for both static and moving rectangles
const GLfloat combinedVertices[] = {
        // Static rectangle (includes texture coordinates)
        -0.5f, -0.5f, 0.0f,  0.0f, 0.0f,  0.0f,  // vType = 0.0 for static
        -0.5f,  0.5f, 0.0f,  0.0f, 1.0f,  0.0f,
        0.5f,  0.5f, 0.0f,  1.0f, 1.0f,  0.0f,
        0.5f, -0.5f, 0.0f,  1.0f, 0.0f,  0.0f,

        // Moving rectangle (no texture coordinates, vTexCoord can be dummy)
        -0.5f, -0.5f, 0.0f,  0.0f, 0.0f,  1.0f,  // vType = 1.0 for moving
        0.5f, -0.5f, 0.0f,  0.0f, 0.0f,  1.0f,
        0.5f,  0.5f, 0.0f,  0.0f, 0.0f,  1.0f,
        -0.5f,  0.5f, 0.0f,  0.0f, 0.0f,  1.0f,
};

// Combined indices for both rectangles
const GLuint combinedIndices[] = {
        // Static rectangle
        0, 1, 2,
        0, 2, 3,

        // Moving rectangle
        4, 5, 6,
        4, 6, 7
};

// Setup buffers for combined vertices and indices
void SetupCombinedBuffers() {
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    // Bind VBO
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(combinedVertices), combinedVertices, GL_STATIC_DRAW);

    // Bind EBO
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(combinedIndices), combinedIndices, GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Texture coordinate attribute
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Type attribute
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(5 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
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

// Render both rectangles in one call
void RenderCombinedRectangles(GLuint shaderProgram) {
    glUseProgram(shaderProgram);

    // Set uniforms
    GLint offsetLoc = glGetUniformLocation(shaderProgram, "rectangleOffset");
    glUniform1f(offsetLoc, rectangleOffset);

    GLint leftLimitLoc = glGetUniformLocation(shaderProgram, "leftLimit");
    GLint rightLimitLoc = glGetUniformLocation(shaderProgram, "rightLimit");
    GLint bottomLimitLoc = glGetUniformLocation(shaderProgram, "bottomLimit");
    GLint topLimitLoc = glGetUniformLocation(shaderProgram, "topLimit");

    glUniform1f(leftLimitLoc, -0.5f);
    glUniform1f(rightLimitLoc, 0.5f);
    glUniform1f(bottomLimitLoc, -0.5f);
    glUniform1f(topLimitLoc, 0.5f);

    // Bind texture unit 0 to textureSampler
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);
    GLint texLoc = glGetUniformLocation(shaderProgram, "textureSampler");
    glUniform1i(texLoc, 0); // Texture unit 0

    // Bind VAO and draw
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, 12, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    // Unbind texture
    glBindTexture(GL_TEXTURE_2D, 0);
}

