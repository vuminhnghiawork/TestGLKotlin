#include <jni.h>
#include <string>
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
#define LOG_TAG "Native"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)


ANativeWindow* window = nullptr;
EGLContext context;
EGLSurface surface;
int width = 0;
int height = 0;
bool running = false;
std::thread renderThread;
bool triggerUpdateSize = true;
std::mutex mMutex;

extern "C" JNIEXPORT jstring JNICALL
Java_com_vinai_testglkotlin_MainActivity_stringFromJNI(JNIEnv *env, jobject instance) {
    return env->NewStringUTF("Xin chao");
}

extern "C" JNIEXPORT void JNICALL
Java_com_vinai_testglkotlin_MainActivity_initSurface(JNIEnv *env, jobject instance, jobject j_surface) {
    window = ANativeWindow_fromSurface(env, j_surface);
    running = true;
    renderThread = std::thread([]{
        EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        GLHelper_initGL(EGL_NO_CONTEXT, window, &context, &surface);
        eglMakeCurrent(display, surface, surface, context);
        GLHelper_getSurfaceSize(surface, width, height);
        init_gl(width, height);
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
            render_gl(width, height);
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


extern "C" JNIEXPORT jint JNICALL
Java_com_vinai_testglkotlin_MainActivity_loadTextureFromFile(JNIEnv *env, jobject thiz, jstring picturesDir) {
    // Chuyển đổi jstring thành chuỗi C
    const char* path = env->GetStringUTFChars(picturesDir, nullptr);

    // Tải hình ảnh bằng stb_image
    int width, height, nrChannels;
    unsigned char* data = stbi_load(path, &width, &height, &nrChannels, 0);
    if (data == nullptr) {
        LOGE("Failed to load image: %s", path);
        env->ReleaseStringUTFChars(picturesDir, path);
        return 0;
    }

    // Tạo texture OpenGL
    GLuint textureID;
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

    // Giải phóng dữ liệu hình ảnh sau khi tải vào texture
    stbi_image_free(data);
    env->ReleaseStringUTFChars(picturesDir, path);

    return textureID;
}


