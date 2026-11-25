#ifndef MUGEN_GLRENDER_H
#define MUGEN_GLRENDER_H


#include "platform/platform.h"
/*
 * Types of the OpenGL renderer (glrender.c): GLSL programs and the GLEW
 * function pointers it uses from mugen.exe's own GLEW 1.5.4 copy (filled by
 * glewInit).
 */

/* A GLSL program with its uniform locations (Gl_LoadShader). */
typedef struct GlProgram {
    unsigned int vert;          /* 0x00 vertex shader */
    unsigned int frag;          /* 0x04 fragment shader */
    unsigned int program;       /* 0x08 */
    int texunit0;               /* 0x0C uniform locations */
    int texunit1;               /* 0x10 */
    int mask;                   /* 0x14 */
    int add;                    /* 0x18 */
    int mul;                    /* 0x1C */
    int invertall;              /* 0x20 */
    int colorbal;               /* 0x24 */
    int texsize;                /* 0x28 */
    int src_alphamul;           /* 0x2C */
} GlProgram;
MUGEN_ASSERT_SIZE(GlProgram, 0x30);

/*
 * Programs by variant: palfx (0 none, 1 add/mul, 2 colour balance or
 * inversion) + 3 * (0 plain, 2 stretched / tinted quad).
 */
#define GL_PROGRAM_COUNT 9

/* GLEW function pointer types (MUGEN_STDCALL, as the GL entry points). */
typedef void (MUGEN_STDCALL *GlEnumFn)(unsigned int e);                   /* glActiveTexture, glBlendEquation... */
typedef void (MUGEN_STDCALL *GlUintFn)(unsigned int id);                  /* glUseProgram, glCompileShader... */
typedef void (MUGEN_STDCALL *GlMultiTexCoord2fFn)(unsigned int target, float s, float t);
typedef void (MUGEN_STDCALL *GlMultiTexCoord4fFn)(unsigned int target, float s, float t, float r, float q);
typedef void (MUGEN_STDCALL *GlBlendColorFn)(float r, float g, float b, float a);
typedef void (MUGEN_STDCALL *GlAttachShaderFn)(unsigned int program, unsigned int shader);
typedef unsigned int (MUGEN_STDCALL *GlCreateProgramFn)(void);
typedef unsigned int (MUGEN_STDCALL *GlCreateShaderFn)(unsigned int type);
typedef void (MUGEN_STDCALL *GlGetInfoLogFn)(unsigned int obj, int size, int *length, char *log);
typedef void (MUGEN_STDCALL *GlGetObjectivFn)(unsigned int obj, unsigned int pname, int *params);
typedef int (MUGEN_STDCALL *GlGetUniformLocationFn)(unsigned int program, const char *name);
typedef unsigned char (MUGEN_STDCALL *GlIsShaderFn)(unsigned int shader);
typedef void (MUGEN_STDCALL *GlShaderSourceFn)(unsigned int shader, int count, const char **string, const int *length);
typedef void (MUGEN_STDCALL *GlUniform1fFn)(int location, float v0);
typedef void (MUGEN_STDCALL *GlUniform1iFn)(int location, int v0);
typedef void (MUGEN_STDCALL *GlUniform2fFn)(int location, float v0, float v1);
typedef void (MUGEN_STDCALL *GlUniform3fFn)(int location, float v0, float v1, float v2);
typedef void (MUGEN_STDCALL *GlGenDeleteFn)(int n, unsigned int *ids);    /* glGen/DeleteFramebuffers/RenderbuffersEXT */
typedef void (MUGEN_STDCALL *GlDeleteIdsFn)(int n, const unsigned int *ids); /* glDeleteRenderbuffersEXT, const as GLEW declares it */
typedef unsigned int (MUGEN_STDCALL *GlCheckFramebufferStatusFn)(unsigned int target);
typedef void (MUGEN_STDCALL *GlFramebufferRenderbufferFn)(unsigned int target, unsigned int attachment,
                                                       unsigned int renderbuffer_target, unsigned int renderbuffer);
typedef void (MUGEN_STDCALL *GlFramebufferTexture2DFn)(unsigned int target, unsigned int attachment,
                                                    unsigned int texture_target, unsigned int texture, int level);
typedef void (MUGEN_STDCALL *GlRenderbufferStorageFn)(unsigned int target, unsigned int internal_format, int w, int h);

#endif /* MUGEN_GLRENDER_H */
