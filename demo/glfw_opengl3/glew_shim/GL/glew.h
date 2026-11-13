/*
 * Minimal GLEW-compatible shim for environments without libglew-dev.
 *
 * On Linux/Mesa the OpenGL entry points used by nuklear_glfw_gl3.h are all
 * exported by libGL, so defining GL_GLEXT_PROTOTYPES before GL/gl.h provides
 * real prototypes and linked symbols directly. This header only supplies the
 * tiny GLEW API surface the backend references. Public domain.
 */
#ifndef GLEW_SHIM_H
#define GLEW_SHIM_H

#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#endif
#include <GL/gl.h>
#include <GL/glext.h>

#ifndef GLEW_OK
#define GLEW_OK 0
#endif

/* Signals nuklear's GLFW backend to emit desktop GLSL (#version 150)
 * rather than the OpenGL ES "#version 300 es" profile. */
#ifndef NK_GLFW_DESKTOP_GL
#define NK_GLFW_DESKTOP_GL 1
#endif

static int glewExperimental __attribute__((unused));

static inline GLenum
glewInit(void)
{
    /* Symbols resolve through libGL at link time; nothing to load. */
    return GLEW_OK;
}

#endif /* GLEW_SHIM_H */
