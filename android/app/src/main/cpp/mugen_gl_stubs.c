/*
 * TEMPORARY (P3b M2, D-003/D-013): link stubs for the desktop OpenGL renderer, so that the whole engine (mugen_core) links into
 * libmugen.so before the renderer is rewritten (P6 abstraction, P7 GLES). Generated from the prototypes of
 * src/platform/stubs/GL/gl.h, which the compiler checks against these definitions.
 *
 * What they do: nothing. The GL entry points are empty, glGetString returns "", glGetError returns 0, glGetIntegerv and
 * glGenTextures write zeros; the GLEW entry-point pointers (__glew*) are NULL, the extension flags (__GLEW_*) are 0 and glewInit
 * reports failure. The first boot (P5) uses the software renderer (RENDER_SYSTEM), which never reaches this code; an OpenGL
 * render mode would draw nothing. They disappear in P6/P7.
 */
#include "platform/platform.h"

#include <GL/gl.h>
#include <GL/glu.h>

/* GLEW 1.5.4: GLEW_ERROR_NO_GL_VERSION */
int glewInit(void) { return 1; }
void gluOrtho2D(double left, double right, double bottom, double top) { (void)left; (void)right; (void)bottom; (void)top; }

void glBegin(GLenum mode) { }
void glBindTexture(GLenum target, GLuint texture) { }
void glBlendFunc(GLenum sfactor, GLenum dfactor) { }
void glClear(GLbitfield mask) { }
void glClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) { }
void glClearDepth(GLclampd depth) { }
void glColor3f(GLfloat red, GLfloat green, GLfloat blue) { }
void glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha) { }
void glCullFace(GLenum mode) { }
void glDeleteTextures(GLsizei n, const GLuint *textures) { }
void glDepthFunc(GLenum func) { }
void glDepthMask(GLboolean flag) { }
void glDisable(GLenum cap) { }
void glDrawArrays(GLenum mode, GLint first, GLsizei count) { }
void glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices) { }
void glEnable(GLenum cap) { }
void glEnd(void) { }
void glFlush(void) { }
void glFrontFace(GLenum mode) { }
void glFrustum(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar) { }
void glGenTextures(GLsizei n, GLuint *textures) { GLsizei i; for (i = 0; textures != NULL && i < n; ++i) { textures[i] = 0; } }
GLenum glGetError(void) { return 0; }
void glGetIntegerv(GLenum pname, GLint *params) { (void)pname; if (params != NULL) { *params = 0; } }
const GLubyte * glGetString(GLenum name) { (void)name; return (const GLubyte *)""; }
void glHint(GLenum target, GLenum mode) { }
void glLoadIdentity(void) { }
void glLogicOp(GLenum opcode) { }
void glMatrixMode(GLenum mode) { }
void glMultMatrixf(const GLfloat *m) { }
void glOrtho(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar) { }
void glPixelStorei(GLenum pname, GLint param) { }
void glPopAttrib(void) { }
void glPushAttrib(GLbitfield mask) { }
void glReadBuffer(GLenum mode) { }
void glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels) { }
void glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z) { }
void glScaled(GLdouble x, GLdouble y, GLdouble z) { }
void glScalef(GLfloat x, GLfloat y, GLfloat z) { }
void glScissor(GLint x, GLint y, GLsizei width, GLsizei height) { }
void glTexCoord2f(GLfloat s, GLfloat t) { }
void glTexEnvf(GLenum target, GLenum pname, GLfloat param) { }
void glTexImage1D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const GLvoid *pixels) { }
void glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels) { }
void glTexParameteri(GLenum target, GLenum pname, GLint param) { }
void glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels) { }
void glTranslated(GLdouble x, GLdouble y, GLdouble z) { }
void glTranslatef(GLfloat x, GLfloat y, GLfloat z) { }
void glVertex2f(GLfloat x, GLfloat y) { }
void glVertex3f(GLfloat x, GLfloat y, GLfloat z) { }
void glViewport(GLint x, GLint y, GLsizei width, GLsizei height) { }

/* GLEW entry-point pointers and extension flags: only the names the engine refers to */
void *__glewActiveTexture;
void *__glewAttachShader;
void *__glewBindAttribLocation;
void *__glewBindBuffer;
void *__glewBindFramebufferEXT;
void *__glewBindRenderbufferEXT;
void *__glewBlendColor;
void *__glewBlendEquation;
void *__glewBufferData;
void *__glewBufferSubData;
void *__glewCheckFramebufferStatusEXT;
void *__glewCompileShader;
void *__glewCreateProgram;
void *__glewCreateShader;
void *__glewDeleteBuffers;
void *__glewDeleteFramebuffersEXT;
void *__glewDeleteProgram;
void *__glewDeleteRenderbuffersEXT;
void *__glewDeleteShader;
void *__glewDisableVertexAttribArray;
void *__glewEnableVertexAttribArray;
void *__glewFramebufferRenderbufferEXT;
void *__glewFramebufferTexture2DEXT;
void *__glewGenBuffers;
void *__glewGenFramebuffersEXT;
void *__glewGenRenderbuffersEXT;
void *__glewGenerateMipmapEXT;
void *__glewGetProgramInfoLog;
void *__glewGetProgramiv;
void *__glewGetShaderInfoLog;
void *__glewGetShaderiv;
void *__glewGetUniformLocation;
void *__glewIsShader;
void *__glewLinkProgram;
void *__glewMultiTexCoord2f;
void *__glewMultiTexCoord4f;
void *__glewRenderbufferStorageEXT;
void *__glewShaderSource;
void *__glewUniform1f;
void *__glewUniform1i;
void *__glewUniform2f;
void *__glewUniform3f;
void *__glewUniform3fv;
void *__glewUniform4fv;
void *__glewUniformMatrix3fv;
void *__glewUniformMatrix4fv;
void *__glewUseProgram;
void *__glewVertexAttrib4f;
void *__glewVertexAttribPointer;
unsigned char __GLEW_ARB_fragment_program;
unsigned char __GLEW_ARB_texture_float;
unsigned char __GLEW_ARB_texture_non_power_of_two;
unsigned char __GLEW_ARB_texture_rg;
unsigned char __GLEW_EXT_blend_logic_op;
unsigned char __GLEW_EXT_framebuffer_object;
unsigned char __GLEW_VERSION_2_0;
unsigned char __GLEW_VERSION_3_0;
