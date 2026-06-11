#include "gl_es3.h"
#include "engine/core/log.h"

#if defined(__ANDROID__)
#  include <EGL/egl.h>
#  include <dlfcn.h>
#elif defined(__APPLE__)
#  include <TargetConditionals.h>
#  if TARGET_OS_IOS
#    include <dlfcn.h>
#  endif
#else
#  include <SDL.h>
#endif

// ---------------------------------------------------------------------------
// Function pointer definitions
// ---------------------------------------------------------------------------
#define PINO_GL_FUNC(ret, name, params) PFN_##name name = nullptr

// -- State & getters
PINO_GL_FUNC(void,         glGetIntegerv,           );
PINO_GL_FUNC(GLenum,       glGetError,              );
PINO_GL_FUNC(const GLubyte*, glGetString,           );

// -- Viewport & clear
PINO_GL_FUNC(void,         glViewport,              );
PINO_GL_FUNC(void,         glClearColor,            );
PINO_GL_FUNC(void,         glClearDepthf,           );
PINO_GL_FUNC(void,         glClearStencil,          );
PINO_GL_FUNC(void,         glClear,                 );
PINO_GL_FUNC(void,         glColorMask,             );
PINO_GL_FUNC(void,         glDepthMask,             );

// -- Enable / disable
PINO_GL_FUNC(void,         glEnable,                );
PINO_GL_FUNC(void,         glDisable,               );
PINO_GL_FUNC(GLboolean,    glIsEnabled,             );

// -- Depth / stencil
PINO_GL_FUNC(void,         glDepthFunc,             );
PINO_GL_FUNC(void,         glDepthRangef,           );
PINO_GL_FUNC(void,         glStencilFunc,           );
PINO_GL_FUNC(void,         glStencilMask,           );
PINO_GL_FUNC(void,         glStencilOp,             );

// -- Blending
PINO_GL_FUNC(void,         glBlendFunc,             );
PINO_GL_FUNC(void,         glBlendFuncSeparate,     );
PINO_GL_FUNC(void,         glBlendEquation,         );
PINO_GL_FUNC(void,         glBlendEquationSeparate, );
PINO_GL_FUNC(void,         glBlendColor,            );

// -- Culling
PINO_GL_FUNC(void,         glCullFace,              );
PINO_GL_FUNC(void,         glFrontFace,             );

// -- Scissor
PINO_GL_FUNC(void,         glScissor,               );

// -- Line width
PINO_GL_FUNC(void,         glLineWidth,             );

// -- Pixel storage
PINO_GL_FUNC(void,         glPixelStorei,           );

// -- VAOs
PINO_GL_FUNC(void,         glGenVertexArrays,       );
PINO_GL_FUNC(void,         glDeleteVertexArrays,    );
PINO_GL_FUNC(void,         glBindVertexArray,       );

// -- Buffer Objects
PINO_GL_FUNC(void,         glGenBuffers,            );
PINO_GL_FUNC(void,         glDeleteBuffers,         );
PINO_GL_FUNC(void,         glBindBuffer,            );
PINO_GL_FUNC(void,         glBufferData,            );
PINO_GL_FUNC(void,         glBufferSubData,         );
PINO_GL_FUNC(void*,        glMapBufferRange,        );
PINO_GL_FUNC(GLboolean,    glUnmapBuffer,           );

// -- Vertex Attributes
PINO_GL_FUNC(void,         glEnableVertexAttribArray,);
PINO_GL_FUNC(void,         glDisableVertexAttribArray,);
PINO_GL_FUNC(void,         glVertexAttribPointer,   );
PINO_GL_FUNC(void,         glVertexAttribDivisor,   );

// -- Shaders
PINO_GL_FUNC(GLuint,       glCreateShader,          );
PINO_GL_FUNC(void,         glShaderSource,          );
PINO_GL_FUNC(void,         glCompileShader,         );
PINO_GL_FUNC(void,         glGetShaderiv,           );
PINO_GL_FUNC(void,         glGetShaderInfoLog,      );
PINO_GL_FUNC(void,         glDeleteShader,          );

// -- Programs
PINO_GL_FUNC(GLuint,       glCreateProgram,         );
PINO_GL_FUNC(void,         glAttachShader,          );
PINO_GL_FUNC(void,         glDetachShader,          );
PINO_GL_FUNC(void,         glLinkProgram,           );
PINO_GL_FUNC(void,         glGetProgramiv,          );
PINO_GL_FUNC(void,         glGetProgramInfoLog,     );
PINO_GL_FUNC(void,         glUseProgram,            );
PINO_GL_FUNC(void,         glDeleteProgram,         );

// -- Uniforms
PINO_GL_FUNC(GLint,        glGetUniformLocation,    );
PINO_GL_FUNC(void,         glUniform1f,             );
PINO_GL_FUNC(void,         glUniform2f,             );
PINO_GL_FUNC(void,         glUniform3f,             );
PINO_GL_FUNC(void,         glUniform4f,             );
PINO_GL_FUNC(void,         glUniform1i,             );
PINO_GL_FUNC(void,         glUniform2i,             );
PINO_GL_FUNC(void,         glUniform3i,             );
PINO_GL_FUNC(void,         glUniform4i,             );
PINO_GL_FUNC(void,         glUniformMatrix3fv,      );
PINO_GL_FUNC(void,         glUniformMatrix4fv,      );
PINO_GL_FUNC(void,         glUniform1fv,            );
PINO_GL_FUNC(void,         glUniform2fv,            );
PINO_GL_FUNC(void,         glUniform3fv,            );
PINO_GL_FUNC(void,         glUniform4fv,            );

// -- Textures
PINO_GL_FUNC(void,         glGenTextures,           );
PINO_GL_FUNC(void,         glDeleteTextures,        );
PINO_GL_FUNC(void,         glBindTexture,           );
PINO_GL_FUNC(void,         glTexImage2D,            );
PINO_GL_FUNC(void,         glTexParameteri,         );
PINO_GL_FUNC(void,         glTexParameterf,         );
PINO_GL_FUNC(void,         glGenerateMipmap,        );
PINO_GL_FUNC(void,         glActiveTexture,         );
PINO_GL_FUNC(void,         glTexSubImage2D,         );
PINO_GL_FUNC(void,         glCopyTexImage2D,        );

// -- FBOs
PINO_GL_FUNC(void,         glGenFramebuffers,       );
PINO_GL_FUNC(void,         glDeleteFramebuffers,    );
PINO_GL_FUNC(void,         glBindFramebuffer,       );
PINO_GL_FUNC(GLenum,       glCheckFramebufferStatus,);
PINO_GL_FUNC(void,         glFramebufferTexture2D,  );
PINO_GL_FUNC(void,         glFramebufferRenderbuffer,);

// -- RBOs
PINO_GL_FUNC(void,         glGenRenderbuffers,      );
PINO_GL_FUNC(void,         glDeleteRenderbuffers,   );
PINO_GL_FUNC(void,         glBindRenderbuffer,      );
PINO_GL_FUNC(void,         glRenderbufferStorage,   );

// -- Uniform Buffer Objects
PINO_GL_FUNC(GLuint,       glGetUniformBlockIndex,   );
PINO_GL_FUNC(void,         glUniformBlockBinding,    );
PINO_GL_FUNC(void,         glBindBufferBase,         );
PINO_GL_FUNC(void,         glBindBufferRange,        );
PINO_GL_FUNC(void,         glGetActiveUniformBlockiv,);

// -- Drawing
PINO_GL_FUNC(void,         glDrawArrays,            );
PINO_GL_FUNC(void,         glDrawElements,          );
PINO_GL_FUNC(void,         glDrawArraysInstanced,   );
PINO_GL_FUNC(void,         glDrawElementsInstanced, );

// -- Debug
PINO_GL_FUNC(void,         glDebugMessageCallback,  );
PINO_GL_FUNC(void,         glDebugMessageControl,   );

#undef PINO_GL_FUNC

// ---------------------------------------------------------------------------
// Loader
// ---------------------------------------------------------------------------
namespace pino {
namespace gl {

#if defined(__ANDROID__)
static void* load_gl_func(const char* name) {
    void* p = reinterpret_cast<void*>(eglGetProcAddress(name));
    if (!p) p = dlsym(RTLD_DEFAULT, name);
    return p;
}
#define PINO_LOAD(func) \
    do { \
        func = reinterpret_cast<PFN_##func>(load_gl_func(#func)); \
        if (!func) { \
            PINO_WARN("GL function not loaded: %s", #func); \
        } \
    } while (0)
#elif defined(__APPLE__) && TARGET_OS_IOS
static void* load_gl_func(const char* name) {
    return dlsym(RTLD_DEFAULT, name);
}
#define PINO_LOAD(func) \
    do { \
        func = reinterpret_cast<PFN_##func>(load_gl_func(#func)); \
        if (!func) { \
            PINO_WARN("GL function not loaded: %s", #func); \
        } \
    } while (0)
#else
#define PINO_LOAD(func) \
    do { \
        func = reinterpret_cast<PFN_##func>(SDL_GL_GetProcAddress(#func)); \
        if (!func) { \
            PINO_WARN("GL function not loaded: %s", #func); \
        } \
    } while (0)
#endif

bool init() {
    PINO_LOAD(glGetIntegerv);
    PINO_LOAD(glGetError);
    PINO_LOAD(glGetString);

    PINO_LOAD(glViewport);
    PINO_LOAD(glClearColor);
    PINO_LOAD(glClearDepthf);
    PINO_LOAD(glClearStencil);
    PINO_LOAD(glClear);
    PINO_LOAD(glColorMask);
    PINO_LOAD(glDepthMask);

    PINO_LOAD(glEnable);
    PINO_LOAD(glDisable);
    PINO_LOAD(glIsEnabled);

    PINO_LOAD(glDepthFunc);
    PINO_LOAD(glDepthRangef);
    PINO_LOAD(glStencilFunc);
    PINO_LOAD(glStencilMask);
    PINO_LOAD(glStencilOp);

    PINO_LOAD(glBlendFunc);
    PINO_LOAD(glBlendFuncSeparate);
    PINO_LOAD(glBlendEquation);
    PINO_LOAD(glBlendEquationSeparate);
    PINO_LOAD(glBlendColor);

    PINO_LOAD(glCullFace);
    PINO_LOAD(glFrontFace);
    PINO_LOAD(glScissor);
    PINO_LOAD(glLineWidth);

    PINO_LOAD(glPixelStorei);

    PINO_LOAD(glGenVertexArrays);
    PINO_LOAD(glDeleteVertexArrays);
    PINO_LOAD(glBindVertexArray);

    PINO_LOAD(glGenBuffers);
    PINO_LOAD(glDeleteBuffers);
    PINO_LOAD(glBindBuffer);
    PINO_LOAD(glBufferData);
    PINO_LOAD(glBufferSubData);
    PINO_LOAD(glMapBufferRange);
    PINO_LOAD(glUnmapBuffer);

    PINO_LOAD(glEnableVertexAttribArray);
    PINO_LOAD(glDisableVertexAttribArray);
    PINO_LOAD(glVertexAttribPointer);
    PINO_LOAD(glVertexAttribDivisor);

    PINO_LOAD(glCreateShader);
    PINO_LOAD(glShaderSource);
    PINO_LOAD(glCompileShader);
    PINO_LOAD(glGetShaderiv);
    PINO_LOAD(glGetShaderInfoLog);
    PINO_LOAD(glDeleteShader);

    PINO_LOAD(glCreateProgram);
    PINO_LOAD(glAttachShader);
    PINO_LOAD(glDetachShader);
    PINO_LOAD(glLinkProgram);
    PINO_LOAD(glGetProgramiv);
    PINO_LOAD(glGetProgramInfoLog);
    PINO_LOAD(glUseProgram);
    PINO_LOAD(glDeleteProgram);

    PINO_LOAD(glGetUniformLocation);
    PINO_LOAD(glUniform1f);
    PINO_LOAD(glUniform2f);
    PINO_LOAD(glUniform3f);
    PINO_LOAD(glUniform4f);
    PINO_LOAD(glUniform1i);
    PINO_LOAD(glUniform2i);
    PINO_LOAD(glUniform3i);
    PINO_LOAD(glUniform4i);
    PINO_LOAD(glUniformMatrix3fv);
    PINO_LOAD(glUniformMatrix4fv);
    PINO_LOAD(glUniform1fv);
    PINO_LOAD(glUniform2fv);
    PINO_LOAD(glUniform3fv);
    PINO_LOAD(glUniform4fv);

    PINO_LOAD(glGenTextures);
    PINO_LOAD(glDeleteTextures);
    PINO_LOAD(glBindTexture);
    PINO_LOAD(glTexImage2D);
    PINO_LOAD(glTexParameteri);
    PINO_LOAD(glTexParameterf);
    PINO_LOAD(glGenerateMipmap);
    PINO_LOAD(glActiveTexture);
    PINO_LOAD(glTexSubImage2D);
    PINO_LOAD(glCopyTexImage2D);

    PINO_LOAD(glGenFramebuffers);
    PINO_LOAD(glDeleteFramebuffers);
    PINO_LOAD(glBindFramebuffer);
    PINO_LOAD(glCheckFramebufferStatus);
    PINO_LOAD(glFramebufferTexture2D);
    PINO_LOAD(glFramebufferRenderbuffer);

    PINO_LOAD(glGenRenderbuffers);
    PINO_LOAD(glDeleteRenderbuffers);
    PINO_LOAD(glBindRenderbuffer);
    PINO_LOAD(glRenderbufferStorage);

    PINO_LOAD(glGetUniformBlockIndex);
    PINO_LOAD(glUniformBlockBinding);
    PINO_LOAD(glBindBufferBase);
    PINO_LOAD(glBindBufferRange);
    PINO_LOAD(glGetActiveUniformBlockiv);

    PINO_LOAD(glDrawArrays);
    PINO_LOAD(glDrawElements);
    PINO_LOAD(glDrawArraysInstanced);
    PINO_LOAD(glDrawElementsInstanced);

    PINO_LOAD(glDebugMessageCallback);
    PINO_LOAD(glDebugMessageControl);

    const GLubyte* renderer = glGetString(0x1F01 /*GL_RENDERER*/);
    const GLubyte* version  = glGetString(0x1F02 /*GL_VERSION*/);
    PINO_INFO("OpenGL renderer: %s", renderer ? (const char*)renderer : "?");
    PINO_INFO("OpenGL version:  %s", version  ? (const char*)version  : "?");

    return true;
}

#undef PINO_LOAD

} // namespace gl
} // namespace pino
