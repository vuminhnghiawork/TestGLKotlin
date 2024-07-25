#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>
#include <android/native_window.h>
#include <android/log.h>
#include "gl_helper.h"
#include "gl_error.h"
#include <stdio.h>

#if ANDROID_LOG
#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO, "gl_helper", __VA_ARGS__);
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, "gl_helper", __VA_ARGS__);
#else
#define ALOGI(...) { printf(__VA_ARGS__); printf("\r\n"); }
#define ALOGE(...) { printf(__VA_ARGS__); printf("\r\n"); }
#endif

extern "C" bool GLHelper_initGL(EGLContext sharedContext, ANativeWindow* window, EGLContext* outContext, EGLSurface* outSurface) {
    // Hardcoded to RGBx output display
    const EGLint config_attribs[] = {
            // Tag                  Value
            EGL_RENDERABLE_TYPE,    EGL_OPENGL_ES2_BIT,
            EGL_RED_SIZE,           8,
            EGL_GREEN_SIZE,         8,
            EGL_BLUE_SIZE,          8,
            EGL_ALPHA_SIZE,         8,
            EGL_DEPTH_SIZE,         8,
            EGL_NONE
    };

    // Select OpenGL ES v 3
    const EGLint context_attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};

    // Set up our OpenGL ES context associated with the default display (though we won't be visible)
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY) {
        ALOGE("SharedGLResource glinit Failed to get egl display");
        return false;
    }

    EGLint major = 0;
    EGLint minor = 0;
    if (!eglInitialize(display, &major, &minor)) {
        ALOGE("SharedGLResource glinit Failed to initialize EGL: %s", getEGLError());
        return false;
    } else {
        ALOGI("SharedGLResource glinit Intiialized EGL at %d.%d", major, minor);
    }

    // Select the configuration that "best" matches our desired characteristics
    EGLConfig egl_config;
    EGLint num_configs;
    if (!eglChooseConfig(display, config_attribs, &egl_config, 1, &num_configs)) {
        ALOGE("SharedGLResource glinit eglChooseConfig() failed with error: %s", getEGLError());
        return false;
    }

    // Create the EGL context
    EGLContext context = eglCreateContext(display, egl_config, sharedContext, context_attribs);
    if (context == EGL_NO_CONTEXT) {
        ALOGE("SharedGLResource glinit Failed to create OpenGL ES Context: %s", getEGLError());
        return false;
    }

    EGLSurface surface = EGL_NO_SURFACE;

    //get the format for surface
    EGLint format;
    if (!eglGetConfigAttrib(display, egl_config, EGL_NATIVE_VISUAL_ID, &format)) {
        ALOGE("ThreadGLResource eglGetConfigAttrib() returned error %d", eglGetError());
        ALOGE("%s", getEGLError());
        return false;
    }

    if (window != nullptr) {
        //create surface by using nativewindow
        ANativeWindow_setBuffersGeometry(window, 0, 0, format);
        if (!(surface = eglCreateWindowSurface(display, egl_config, window, 0))) {
            ALOGE("ThreadGLResource eglCreateWindowSurface() returned error %d", eglGetError());
            ALOGE("%s", getEGLError());
            return false;
        }
    }
    else {
        EGLint attibs[] = {
                EGL_WIDTH, 1,
                EGL_HEIGHT, 1,
                EGL_TEXTURE_FORMAT, EGL_TEXTURE_RGBA,
                EGL_TEXTURE_TARGET, EGL_TEXTURE_2D,
                EGL_NONE
        };
        if (!(surface = eglCreatePbufferSurface(display, egl_config, attibs))) {
            ALOGE("ThreadGLResource eglCreatePbufferSurface() returned error %d", eglGetError());
            ALOGE("%s", getEGLError());
            return false;
        }
    }

    *outContext = context;
    *outSurface = surface;
    return true;
}

extern "C" bool GLHelper_createSurface(ANativeWindow* window, EGLSurface* outSurface) {
    // Hardcoded to RGBx output display
    const EGLint config_attribs[] = {
            // Tag                  Value
            EGL_RENDERABLE_TYPE,    EGL_OPENGL_ES2_BIT,
            EGL_RED_SIZE,           8,
            EGL_GREEN_SIZE,         8,
            EGL_BLUE_SIZE,          8,
            EGL_ALPHA_SIZE,         8,
            EGL_DEPTH_SIZE,         8,
            EGL_NONE
    };

    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

    // Select the configuration that "best" matches our desired characteristics
    EGLConfig egl_config;
    EGLint num_configs;
    if (!eglChooseConfig(display, config_attribs, &egl_config, 1, &num_configs)) {
        ALOGE("SharedGLResource glinit eglChooseConfig() failed with error: %s", getEGLError());
        return false;
    }

    EGLSurface surface = EGL_NO_SURFACE;

    //get the format for surface
    EGLint format;
    if (!eglGetConfigAttrib(display, egl_config, EGL_NATIVE_VISUAL_ID, &format)) {
        ALOGE("ThreadGLResource eglGetConfigAttrib() returned error %d", eglGetError());
        return false;
    }

    if (window != nullptr) {
        //create surface by using nativewindow
        ANativeWindow_setBuffersGeometry(window, 0, 0, format);
        if (!(surface = eglCreateWindowSurface(display, egl_config, window, 0))) {
            ALOGE("ThreadGLResource eglCreateWindowSurface() returned error %d", eglGetError());
            return false;
        }
    }
    else {
        EGLint attibs[] = {
                EGL_WIDTH, 1,
                EGL_HEIGHT, 1,
                EGL_TEXTURE_FORMAT, EGL_TEXTURE_RGBA,
                EGL_TEXTURE_TARGET, EGL_TEXTURE_2D,
                EGL_NONE
        };
        if (!(surface = eglCreatePbufferSurface(display, egl_config, attibs))) {
            ALOGE("ThreadGLResource eglCreatePbufferSurface() returned error %d", eglGetError());
            return false;
        }
    }

    *outSurface = surface;
    return true;
}

extern "C" void GLHelper_releaseSurface(EGLSurface surface) {
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroySurface(display, surface);
}

extern "C" void GLHelper_releaseContext(EGLContext context) {
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglDestroyContext(display, context);
}

extern "C" void GLHelper_getSurfaceSize(EGLSurface surface, int& width, int& height) {
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglQuerySurface(display, surface, EGL_WIDTH, &width);
    eglQuerySurface(display, surface, EGL_HEIGHT, &height);
}

extern "C" void GLHelper_releaseGL() {}

