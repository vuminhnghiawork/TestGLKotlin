#ifndef GL_HELPER_H
#define GL_HELPER_H

#include <android/native_window.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

extern "C" bool GLHelper_initGL(EGLContext sharedContext, ANativeWindow* window, EGLContext* outContext, EGLSurface* outSurface);
extern "C" bool GLHelper_createSurface(ANativeWindow* window, EGLSurface* outSurface);
extern "C" void GLHelper_releaseSurface(EGLSurface surface);
extern "C" void GLHelper_releaseContext(EGLContext context);
extern "C" void GLHelper_getSurfaceSize(EGLSurface surface, int& width, int& height);
extern "C" void GLHelper_releaseGL();

#endif
