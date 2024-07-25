#include <GLES3/gl31.h>
#include "renderer.h"

#if ANDROID_LOG
#include <android/log.h>
#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO, "RENDERER", __VA_ARGS__);
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, "RENDERER", __VA_ARGS__);
#else
#include <stdio.h>
#define ALOGI(...) { printf(__VA_ARGS__); printf("\r\n"); }
#define ALOGE(...) { printf(__VA_ARGS__); printf("\r\n"); }
#endif

void init_gl(int width, int height) {
    ALOGI("init_gl %d x %d", width, height);
}

void resize_gl(int new_width, int new_height) {
    ALOGI("init_gl %d x %d", new_width, new_height);
}

void render_gl(int width, int height) {
    ALOGI("render_gl %d x %d", width, height);

    glClearColor(1,1,1,1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glFinish();
}

void deinit_gl(int width, int height) {
    ALOGI("deinit_gl %d x %d", width, height);
}

