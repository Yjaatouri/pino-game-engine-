#pragma once

#include "engine/core/types.h"

// ---------------------------------------------------------------------------
// OpenGL ES 3.0 type definitions (Khronos-compatible subset)
// ---------------------------------------------------------------------------
#if defined(_MSC_VER) && defined(_WIN32)
#  define GL_APIENTRY __stdcall
#else
#  define GL_APIENTRY
#endif

typedef unsigned int   GLenum;
typedef unsigned char  GLboolean;
typedef unsigned int   GLbitfield;
typedef void           GLvoid;
typedef signed char    GLbyte;
typedef short          GLshort;
typedef int            GLint;
typedef unsigned char  GLubyte;
typedef unsigned short GLushort;
typedef unsigned int   GLuint;
typedef int            GLsizei;
typedef float          GLfloat;
typedef float          GLclampf;
typedef double         GLdouble;
typedef double         GLclampd;
typedef int64_t        GLint64;
typedef uint64_t       GLuint64;
typedef intptr_t       GLintptr;
typedef ptrdiff_t      GLsizeiptr;
typedef char           GLchar;

// ---------------------------------------------------------------------------
// Constants (OpenGL ES 3.0)
// ---------------------------------------------------------------------------
#define GL_FALSE                             0
#define GL_TRUE                              1

// Clear bits
#define GL_DEPTH_BUFFER_BIT                  0x00000100
#define GL_STENCIL_BUFFER_BIT                0x00000400
#define GL_COLOR_BUFFER_BIT                  0x00004000

// Boolean
#define GL_ZERO                              0
#define GL_ONE                               1

// BeginMode
#define GL_POINTS                            0x0000
#define GL_LINES                             0x0001
#define GL_LINE_LOOP                         0x0002
#define GL_LINE_STRIP                        0x0003
#define GL_TRIANGLES                         0x0004
#define GL_TRIANGLE_STRIP                    0x0005
#define GL_TRIANGLE_FAN                      0x0006

// BlendingFactorSrc/Dst
#define GL_SRC_ALPHA                         0x0302
#define GL_ONE_MINUS_SRC_ALPHA               0x0303
#define GL_DST_ALPHA                         0x0304
#define GL_ONE_MINUS_DST_ALPHA               0x0305
#define GL_SRC_COLOR                         0x0300
#define GL_ONE_MINUS_SRC_COLOR               0x0301
#define GL_DST_COLOR                         0x0306
#define GL_ONE_MINUS_DST_COLOR               0x0307

// BlendEquation
#define GL_FUNC_ADD                          0x8006
#define GL_FUNC_SUBTRACT                     0x800A
#define GL_FUNC_REVERSE_SUBTRACT             0x800B

// Blend
#define GL_BLEND                             0x0BE2
#define GL_BLEND_SRC_RGB                     0x80C9
#define GL_BLEND_DST_RGB                     0x80CA
#define GL_BLEND_SRC_ALPHA                   0x80CB
#define GL_BLEND_DST_ALPHA                   0x80CC
#define GL_BLEND_EQUATION_RGB                0x8009
#define GL_BLEND_EQUATION_ALPHA              0x883D

// Buffer Objects
#define GL_ARRAY_BUFFER                      0x8892
#define GL_ELEMENT_ARRAY_BUFFER              0x8893
#define GL_STATIC_DRAW                       0x88E4
#define GL_DYNAMIC_DRAW                      0x88E8
#define GL_STREAM_DRAW                       0x88E0

// Vertex Attributes
#define GL_VERTEX_ATTRIB_ARRAY_ENABLED       0x8622
#define GL_VERTEX_ATTRIB_ARRAY_SIZE          0x8623
#define GL_VERTEX_ATTRIB_ARRAY_STRIDE        0x8624
#define GL_VERTEX_ATTRIB_ARRAY_TYPE          0x8625
#define GL_VERTEX_ATTRIB_ARRAY_NORMALIZED    0x886A
#define GL_VERTEX_ATTRIB_ARRAY_POINTER       0x8645
#define GL_VERTEX_ATTRIB_ARRAY_DIVISOR       0x88FE

// Data types
#define GL_BYTE                              0x1400
#define GL_UNSIGNED_BYTE                     0x1401
#define GL_SHORT                             0x1402
#define GL_UNSIGNED_SHORT                    0x1403
#define GL_INT                               0x1404
#define GL_UNSIGNED_INT                      0x1405
#define GL_FLOAT                             0x1406
#define GL_FIXED                             0x140C

// Culling
#define GL_CULL_FACE                         0x0B44
#define GL_CULL_FACE_MODE                    0x0B45
#define GL_FRONT                             0x0404
#define GL_BACK                              0x0405
#define GL_FRONT_AND_BACK                    0x0408
#define GL_CCW                               0x0901
#define GL_CW                                0x0900

// Depth
#define GL_DEPTH_TEST                        0x0B71
#define GL_DEPTH_WRITEMASK                   0x0B72
#define GL_DEPTH_FUNC                        0x0B74
#define GL_DEPTH_RANGE                       0x0B70
#define GL_NEVER                             0x0200
#define GL_LESS                              0x0201
#define GL_EQUAL                             0x0202
#define GL_LEQUAL                            0x0203
#define GL_GREATER                           0x0204
#define GL_NOTEQUAL                          0x0205
#define GL_GEQUAL                            0x0206
#define GL_ALWAYS                            0x0207

// Stencil
#define GL_STENCIL_TEST                      0x0B90
#define GL_STENCIL_WRITEMASK                 0x0B98
#define GL_STENCIL_FUNC                      0x0B92
#define GL_STENCIL_REF                       0x0B97
#define GL_STENCIL_VALUE_MASK                0x0B93
#define GL_STENCIL_FAIL                      0x0B94
#define GL_STENCIL_PASS_DEPTH_FAIL           0x0B95
#define GL_STENCIL_PASS_DEPTH_PASS           0x0B96
#define GL_KEEP                              0x1E00
#define GL_REPLACE                           0x1E01
#define GL_INCR                              0x1E02
#define GL_DECR                              0x1E03
#define GL_INVERT                            0x150A
#define GL_INCR_WRAP                         0x8507
#define GL_DECR_WRAP                         0x8508

// Textures
#define GL_TEXTURE0                           0x84C0
#define GL_TEXTURE1                           0x84C1
#define GL_TEXTURE2                           0x84C2
#define GL_TEXTURE3                           0x84C3
#define GL_TEXTURE_2D                         0x0DE1
#define GL_TEXTURE_CUBE_MAP                  0x8513
#define GL_TEXTURE_CUBE_MAP_POSITIVE_X       0x8515
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_X       0x8516
#define GL_TEXTURE_CUBE_MAP_POSITIVE_Y       0x8517
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Y       0x8518
#define GL_TEXTURE_CUBE_MAP_POSITIVE_Z       0x8519
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Z       0x851A
#define GL_TEXTURE_MIN_FILTER                0x2801
#define GL_TEXTURE_MAG_FILTER                0x2800
#define GL_TEXTURE_WRAP_S                    0x2802
#define GL_TEXTURE_WRAP_T                    0x2803
#define GL_TEXTURE_WRAP_R                    0x8072
#define GL_NEAREST                           0x2600
#define GL_LINEAR                            0x2601
#define GL_NEAREST_MIPMAP_NEAREST            0x2700
#define GL_LINEAR_MIPMAP_NEAREST             0x2701
#define GL_NEAREST_MIPMAP_LINEAR             0x2702
#define GL_LINEAR_MIPMAP_LINEAR              0x2703
#define GL_REPEAT                            0x2901
#define GL_CLAMP_TO_EDGE                     0x812F
#define GL_MIRRORED_REPEAT                   0x8370
#define GL_RGBA                              0x1908
#define GL_RGB                               0x1907
#define GL_RGBA8                             0x8058
#define GL_RGB8                              0x8051
#define GL_DEPTH_COMPONENT                   0x1902
#define GL_DEPTH_COMPONENT16                 0x81A5
#define GL_DEPTH_COMPONENT24                 0x81A6
#define GL_DEPTH_COMPONENT32F                0x8CAC
#define GL_DEPTH24_STENCIL8                  0x88F0
#define GL_DEPTH_STENCIL                     0x84F9
#define GL_UNSIGNED_INT_24_8                 0x84FA
#define GL_RGBA4                             0x8056
#define GL_RGB5_A1                           0x8057
#define GL_TEXTURE_MAX_ANISOTROPY            0x84FE
#define GL_TEXTURE_BASE_LEVEL                0x813C
#define GL_TEXTURE_MAX_LEVEL                 0x813D

// Pixel formats
#define GL_UNPACK_ALIGNMENT                  0x0CF5
#define GL_PACK_ALIGNMENT                    0x0D05

// Framebuffer
#define GL_FRAMEBUFFER                       0x8D40
#define GL_READ_FRAMEBUFFER                  0x8CA8
#define GL_DRAW_FRAMEBUFFER                  0x8CA9
#define GL_RENDERBUFFER                      0x8D41
#define GL_COLOR_ATTACHMENT0                 0x8CE0
#define GL_DEPTH_ATTACHMENT                  0x8D00
#define GL_STENCIL_ATTACHMENT                0x8D20
#define GL_DEPTH_STENCIL_ATTACHMENT          0x821A
#define GL_FRAMEBUFFER_COMPLETE              0x8CD5
#define GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT 0x8CD6
#define GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS 0x8CD9
#define GL_FRAMEBUFFER_UNSUPPORTED           0x8CDD
#define GL_FRAMEBUFFER_BINDING               0x8CA6
#define GL_RENDERBUFFER_BINDING              0x8CA7
#define GL_MAX_TEXTURE_SIZE                  0x0D33
#define GL_MAX_COLOR_ATTACHMENTS             0x8CDF

// Programs / Shaders
#define GL_FRAGMENT_SHADER                   0x8B30
#define GL_VERTEX_SHADER                     0x8B31
#define GL_COMPILE_STATUS                    0x8B81
#define GL_LINK_STATUS                       0x8B82
#define GL_INFO_LOG_LENGTH                   0x8B84
#define GL_CURRENT_PROGRAM                   0x8B87
#define GL_ACTIVE_UNIFORMS                   0x8B86
#define GL_ACTIVE_ATTRIBUTES                 0x8B89
#define GL_SHADER_TYPE                       0x8B4F

// Uniform types
#define GL_FLOAT_VEC2                        0x8B50
#define GL_FLOAT_VEC3                        0x8B51
#define GL_FLOAT_VEC4                        0x8B52
#define GL_INT_VEC2                          0x8B53
#define GL_INT_VEC3                          0x8B54
#define GL_INT_VEC4                          0x8B55
#define GL_BOOL                              0x8B56
#define GL_BOOL_VEC2                         0x8B57
#define GL_BOOL_VEC3                         0x8B58
#define GL_BOOL_VEC4                         0x8B59
#define GL_FLOAT_MAT2                        0x8B5A
#define GL_FLOAT_MAT3                        0x8B5B
#define GL_FLOAT_MAT4                        0x8B5C
#define GL_SAMPLER_2D                        0x8B5E
#define GL_SAMPLER_CUBE                      0x8B60

// Scissor / Viewport
#define GL_SCISSOR_TEST                      0x0C11
#define GL_VIEWPORT                          0x0BA2

// Hints
#define GL_FRAGMENT_SHADER_DERIVATIVE_HINT   0x8B8B
#define GL_DONT_CARE                         0x1100
#define GL_FASTEST                           0x1101
#define GL_NICEST                            0x1102

// Sync objects
#define GL_MAX_SERVER_WAIT_TIMEOUT           0x9111
#define GL_OBJECT_TYPE                       0x9112
#define GL_SYNC_CONDITION                    0x9113
#define GL_SYNC_STATUS                       0x9114
#define GL_SYNC_FLAGS                        0x9115
#define GL_SYNC_FENCE                        0x9116
#define GL_SYNC_GPU_COMMANDS_COMPLETE        0x9117
#define GL_UNSIGNALED                        0x9118
#define GL_SIGNALED                          0x9119
#define GL_ALREADY_SIGNALED                  0x911A
#define GL_TIMEOUT_EXPIRED                   0x911B
#define GL_CONDITION_SATISFIED               0x911C
#define GL_WAIT_FAILED                       0x911D
#define GL_SYNC_FLUSH_COMMANDS_BIT           0x00000001
#define GL_TIMEOUT_IGNORED                   0xFFFFFFFFFFFFFFFFull

// Debug
#define GL_NO_ERROR                          0
#define GL_INVALID_ENUM                      0x0500
#define GL_INVALID_VALUE                     0x0501
#define GL_INVALID_OPERATION                 0x0502
#define GL_OUT_OF_MEMORY                     0x0505
#define GL_INVALID_FRAMEBUFFER_OPERATION     0x0506

// Instanced drawing
#define GL_DRAW_INDIRECT_BUFFER              0x8F3F

// Uniform Buffer Objects (ES 3.0)
#define GL_UNIFORM_BUFFER                    0x8A11
#define GL_UNIFORM_BUFFER_BINDING            0x8A28
#define GL_UNIFORM_BLOCK_BINDING             0x8A3F
#define GL_UNIFORM_BUFFER_START              0x8A29
#define GL_UNIFORM_BUFFER_SIZE               0x8A2A
#define GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT   0x8A34
#define GL_INVALID_INDEX                     0xFFFFFFFFu

// ---------------------------------------------------------------------------
// Function pointer type definitions
// ---------------------------------------------------------------------------
#define PINO_GL_FUNC(ret, name, params) \
    typedef ret (GL_APIENTRY * PFN_##name) params; \
    extern PFN_##name name

// -- State & getters
PINO_GL_FUNC(void,         glGetIntegerv,            (GLenum pname, GLint* data));
PINO_GL_FUNC(GLenum,       glGetError,               (void));
PINO_GL_FUNC(const GLubyte*, glGetString,            (GLenum name));

// -- Viewport & clear
PINO_GL_FUNC(void,         glViewport,               (GLint x, GLint y, GLsizei w, GLsizei h));
PINO_GL_FUNC(void,         glClearColor,             (GLclampf r, GLclampf g, GLclampf b, GLclampf a));
PINO_GL_FUNC(void,         glClearDepthf,            (GLclampf d));
PINO_GL_FUNC(void,         glClearStencil,           (GLint s));
PINO_GL_FUNC(void,         glClear,                  (GLbitfield mask));
PINO_GL_FUNC(void,         glColorMask,              (GLboolean r, GLboolean g, GLboolean b, GLboolean a));
PINO_GL_FUNC(void,         glDepthMask,              (GLboolean flag));

// -- Enable / disable
PINO_GL_FUNC(void,         glEnable,                 (GLenum cap));
PINO_GL_FUNC(void,         glDisable,                (GLenum cap));
PINO_GL_FUNC(GLboolean,    glIsEnabled,              (GLenum cap));

// -- Depth / stencil
PINO_GL_FUNC(void,         glDepthFunc,              (GLenum func));
PINO_GL_FUNC(void,         glDepthRangef,            (GLclampf zNear, GLclampf zFar));
PINO_GL_FUNC(void,         glStencilFunc,            (GLenum func, GLint ref, GLuint mask));
PINO_GL_FUNC(void,         glStencilMask,            (GLuint mask));
PINO_GL_FUNC(void,         glStencilOp,              (GLenum fail, GLenum zfail, GLenum zpass));

// -- Blending
PINO_GL_FUNC(void,         glBlendFunc,              (GLenum sfactor, GLenum dfactor));
PINO_GL_FUNC(void,         glBlendFuncSeparate,      (GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha));
PINO_GL_FUNC(void,         glBlendEquation,          (GLenum mode));
PINO_GL_FUNC(void,         glBlendEquationSeparate,  (GLenum modeRGB, GLenum modeAlpha));
PINO_GL_FUNC(void,         glBlendColor,             (GLclampf r, GLclampf g, GLclampf b, GLclampf a));

// -- Culling
PINO_GL_FUNC(void,         glCullFace,               (GLenum mode));
PINO_GL_FUNC(void,         glFrontFace,              (GLenum mode));

// -- Scissor
PINO_GL_FUNC(void,         glScissor,                (GLint x, GLint y, GLsizei w, GLsizei h));

// -- Line width
PINO_GL_FUNC(void,         glLineWidth,              (GLfloat width));

// -- Pixel storage
PINO_GL_FUNC(void,         glPixelStorei,            (GLenum pname, GLint param));

// -- Vertex Array Objects
PINO_GL_FUNC(void,         glGenVertexArrays,        (GLsizei n, GLuint* arrays));
PINO_GL_FUNC(void,         glDeleteVertexArrays,     (GLsizei n, const GLuint* arrays));
PINO_GL_FUNC(void,         glBindVertexArray,        (GLuint array));

// -- Buffer Objects
PINO_GL_FUNC(void,         glGenBuffers,             (GLsizei n, GLuint* buffers));
PINO_GL_FUNC(void,         glDeleteBuffers,          (GLsizei n, const GLuint* buffers));
PINO_GL_FUNC(void,         glBindBuffer,             (GLenum target, GLuint buffer));
PINO_GL_FUNC(void,         glBufferData,             (GLenum target, GLsizeiptr size, const void* data, GLenum usage));
PINO_GL_FUNC(void,         glBufferSubData,          (GLenum target, GLintptr offset, GLsizeiptr size, const void* data));
PINO_GL_FUNC(void*,        glMapBufferRange,         (GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access));
PINO_GL_FUNC(GLboolean,    glUnmapBuffer,            (GLenum target));

// -- Vertex Attributes
PINO_GL_FUNC(void,         glEnableVertexAttribArray, (GLuint index));
PINO_GL_FUNC(void,         glDisableVertexAttribArray,(GLuint index));
PINO_GL_FUNC(void,         glVertexAttribPointer,    (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer));
PINO_GL_FUNC(void,         glVertexAttribDivisor,    (GLuint index, GLuint divisor));

// -- Shaders
PINO_GL_FUNC(GLuint,       glCreateShader,           (GLenum type));
PINO_GL_FUNC(void,         glShaderSource,           (GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length));
PINO_GL_FUNC(void,         glCompileShader,          (GLuint shader));
PINO_GL_FUNC(void,         glGetShaderiv,            (GLuint shader, GLenum pname, GLint* params));
PINO_GL_FUNC(void,         glGetShaderInfoLog,       (GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog));
PINO_GL_FUNC(void,         glDeleteShader,           (GLuint shader));

// -- Programs
PINO_GL_FUNC(GLuint,       glCreateProgram,          (void));
PINO_GL_FUNC(void,         glAttachShader,           (GLuint program, GLuint shader));
PINO_GL_FUNC(void,         glDetachShader,           (GLuint program, GLuint shader));
PINO_GL_FUNC(void,         glLinkProgram,            (GLuint program));
PINO_GL_FUNC(void,         glGetProgramiv,           (GLuint program, GLenum pname, GLint* params));
PINO_GL_FUNC(void,         glGetProgramInfoLog,      (GLuint program, GLsizei bufSize, GLsizei* length, GLchar* infoLog));
PINO_GL_FUNC(void,         glUseProgram,             (GLuint program));
PINO_GL_FUNC(void,         glDeleteProgram,          (GLuint program));

// -- Uniforms
PINO_GL_FUNC(GLint,        glGetUniformLocation,     (GLuint program, const GLchar* name));
PINO_GL_FUNC(void,         glUniform1f,              (GLint location, GLfloat v0));
PINO_GL_FUNC(void,         glUniform2f,              (GLint location, GLfloat v0, GLfloat v1));
PINO_GL_FUNC(void,         glUniform3f,              (GLint location, GLfloat v0, GLfloat v1, GLfloat v2));
PINO_GL_FUNC(void,         glUniform4f,              (GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3));
PINO_GL_FUNC(void,         glUniform1i,              (GLint location, GLint v0));
PINO_GL_FUNC(void,         glUniform2i,              (GLint location, GLint v0, GLint v1));
PINO_GL_FUNC(void,         glUniform3i,              (GLint location, GLint v0, GLint v1, GLint v2));
PINO_GL_FUNC(void,         glUniform4i,              (GLint location, GLint v0, GLint v1, GLint v2, GLint v3));
PINO_GL_FUNC(void,         glUniformMatrix3fv,       (GLint location, GLsizei count, GLboolean transpose, const GLfloat* value));
PINO_GL_FUNC(void,         glUniformMatrix4fv,       (GLint location, GLsizei count, GLboolean transpose, const GLfloat* value));
PINO_GL_FUNC(void,         glUniform1fv,             (GLint location, GLsizei count, const GLfloat* value));
PINO_GL_FUNC(void,         glUniform2fv,             (GLint location, GLsizei count, const GLfloat* value));
PINO_GL_FUNC(void,         glUniform3fv,             (GLint location, GLsizei count, const GLfloat* value));
PINO_GL_FUNC(void,         glUniform4fv,             (GLint location, GLsizei count, const GLfloat* value));

// -- Textures
PINO_GL_FUNC(void,         glGenTextures,            (GLsizei n, GLuint* textures));
PINO_GL_FUNC(void,         glDeleteTextures,         (GLsizei n, const GLuint* textures));
PINO_GL_FUNC(void,         glBindTexture,            (GLenum target, GLuint texture));
PINO_GL_FUNC(void,         glTexImage2D,             (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* pixels));
PINO_GL_FUNC(void,         glTexParameteri,          (GLenum target, GLenum pname, GLint param));
PINO_GL_FUNC(void,         glTexParameterf,          (GLenum target, GLenum pname, GLfloat param));
PINO_GL_FUNC(void,         glGenerateMipmap,         (GLenum target));
PINO_GL_FUNC(void,         glActiveTexture,          (GLenum texture));
PINO_GL_FUNC(void,         glTexSubImage2D,          (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels));
PINO_GL_FUNC(void,         glCopyTexImage2D,         (GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height));

// -- Framebuffer Objects
PINO_GL_FUNC(void,         glGenFramebuffers,        (GLsizei n, GLuint* framebuffers));
PINO_GL_FUNC(void,         glDeleteFramebuffers,     (GLsizei n, const GLuint* framebuffers));
PINO_GL_FUNC(void,         glBindFramebuffer,        (GLenum target, GLuint framebuffer));
PINO_GL_FUNC(GLenum,       glCheckFramebufferStatus, (GLenum target));
PINO_GL_FUNC(void,         glFramebufferTexture2D,   (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level));
PINO_GL_FUNC(void,         glFramebufferRenderbuffer,(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer));

// -- Renderbuffer Objects
PINO_GL_FUNC(void,         glGenRenderbuffers,       (GLsizei n, GLuint* renderbuffers));
PINO_GL_FUNC(void,         glDeleteRenderbuffers,    (GLsizei n, const GLuint* renderbuffers));
PINO_GL_FUNC(void,         glBindRenderbuffer,       (GLenum target, GLuint renderbuffer));
PINO_GL_FUNC(void,         glRenderbufferStorage,    (GLenum target, GLenum internalformat, GLsizei width, GLsizei height));

// -- Uniform Buffer Objects
PINO_GL_FUNC(void,         glGetUniformBlockIndex,   (GLuint program, const GLchar* name));
PINO_GL_FUNC(void,         glUniformBlockBinding,    (GLuint program, GLuint blockIndex, GLuint bindingPoint));
PINO_GL_FUNC(void,         glBindBufferBase,         (GLenum target, GLuint index, GLuint buffer));
PINO_GL_FUNC(void,         glBindBufferRange,        (GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size));
PINO_GL_FUNC(void,         glGetActiveUniformBlockiv,(GLuint program, GLuint blockIndex, GLenum pname, GLint* params));

// -- Drawing
PINO_GL_FUNC(void,         glDrawArrays,             (GLenum mode, GLint first, GLsizei count));
PINO_GL_FUNC(void,         glDrawElements,           (GLenum mode, GLsizei count, GLenum type, const void* indices));
PINO_GL_FUNC(void,         glDrawArraysInstanced,    (GLenum mode, GLint first, GLsizei count, GLsizei instancecount));
PINO_GL_FUNC(void,         glDrawElementsInstanced,  (GLenum mode, GLsizei count, GLenum type, const void* indices, GLsizei instancecount));

// -- Debug
PINO_GL_FUNC(void,         glDebugMessageCallback,   (void* callback, void* userParam));
PINO_GL_FUNC(void,         glDebugMessageControl,    (GLenum source, GLenum type, GLenum severity, GLsizei count, const GLuint* ids, GLboolean enabled));

#undef PINO_GL_FUNC

// ---------------------------------------------------------------------------
// Loader entry point
// ---------------------------------------------------------------------------
namespace pino {
namespace gl {

// Load all GL function pointers via SDL_GL_GetProcAddress.
// Must be called after a GL context is made current.
bool init();

} // namespace gl
} // namespace pino
