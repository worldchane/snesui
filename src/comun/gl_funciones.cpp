// Generado por herramientas/generar_gl.py desde GLES2/gl2.h y EGL/egl.h: no editar.
// Cada función de OpenGL ES 2 y de EGL llama a un puntero que llena GlCargar (ver gl_funciones.h).
#include "gl_funciones.h"

#include <EGL/egl.h>
#include <GLES2/gl2.h>

// Con Piglet importado (PIGLET_IMPORTADO, como ioQuake3-PS4) estas funciones no se definen: el programa usa las de
// Piglet directamente (con las nuestras con el mismo nombre, el sistema podía conectar a Piglet con ellas).
#ifndef PIGLET_IMPORTADO
static void (GL_APIENTRY* p_glActiveTexture)(GLenum texture);
extern "C" void GL_APIENTRY glActiveTexture(GLenum texture) { p_glActiveTexture(texture); }
static void (GL_APIENTRY* p_glAttachShader)(GLuint program, GLuint shader);
extern "C" void GL_APIENTRY glAttachShader(GLuint program, GLuint shader) { p_glAttachShader(program, shader); }
static void (GL_APIENTRY* p_glBindAttribLocation)(GLuint program, GLuint index, const GLchar *name);
extern "C" void GL_APIENTRY glBindAttribLocation(GLuint program, GLuint index, const GLchar *name) { p_glBindAttribLocation(program, index, name); }
static void (GL_APIENTRY* p_glBindBuffer)(GLenum target, GLuint buffer);
extern "C" void GL_APIENTRY glBindBuffer(GLenum target, GLuint buffer) { p_glBindBuffer(target, buffer); }
static void (GL_APIENTRY* p_glBindFramebuffer)(GLenum target, GLuint framebuffer);
extern "C" void GL_APIENTRY glBindFramebuffer(GLenum target, GLuint framebuffer) { p_glBindFramebuffer(target, framebuffer); }
static void (GL_APIENTRY* p_glBindRenderbuffer)(GLenum target, GLuint renderbuffer);
extern "C" void GL_APIENTRY glBindRenderbuffer(GLenum target, GLuint renderbuffer) { p_glBindRenderbuffer(target, renderbuffer); }
static void (GL_APIENTRY* p_glBindTexture)(GLenum target, GLuint texture);
extern "C" void GL_APIENTRY glBindTexture(GLenum target, GLuint texture) { p_glBindTexture(target, texture); }
static void (GL_APIENTRY* p_glBlendColor)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
extern "C" void GL_APIENTRY glBlendColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) { p_glBlendColor(red, green, blue, alpha); }
static void (GL_APIENTRY* p_glBlendEquation)(GLenum mode);
extern "C" void GL_APIENTRY glBlendEquation(GLenum mode) { p_glBlendEquation(mode); }
static void (GL_APIENTRY* p_glBlendEquationSeparate)(GLenum modeRGB, GLenum modeAlpha);
extern "C" void GL_APIENTRY glBlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha) { p_glBlendEquationSeparate(modeRGB, modeAlpha); }
static void (GL_APIENTRY* p_glBlendFunc)(GLenum sfactor, GLenum dfactor);
extern "C" void GL_APIENTRY glBlendFunc(GLenum sfactor, GLenum dfactor) { p_glBlendFunc(sfactor, dfactor); }
static void (GL_APIENTRY* p_glBlendFuncSeparate)(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha);
extern "C" void GL_APIENTRY glBlendFuncSeparate(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha) { p_glBlendFuncSeparate(sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha); }
static void (GL_APIENTRY* p_glBufferData)(GLenum target, GLsizeiptr size, const void *data, GLenum usage);
extern "C" void GL_APIENTRY glBufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage) { p_glBufferData(target, size, data, usage); }
static void (GL_APIENTRY* p_glBufferSubData)(GLenum target, GLintptr offset, GLsizeiptr size, const void *data);
extern "C" void GL_APIENTRY glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void *data) { p_glBufferSubData(target, offset, size, data); }
static GLenum (GL_APIENTRY* p_glCheckFramebufferStatus)(GLenum target);
extern "C" GLenum GL_APIENTRY glCheckFramebufferStatus(GLenum target) { return p_glCheckFramebufferStatus(target); }
static void (GL_APIENTRY* p_glClear)(GLbitfield mask);
extern "C" void GL_APIENTRY glClear(GLbitfield mask) { p_glClear(mask); }
static void (GL_APIENTRY* p_glClearColor)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
extern "C" void GL_APIENTRY glClearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) { p_glClearColor(red, green, blue, alpha); }
static void (GL_APIENTRY* p_glClearDepthf)(GLfloat d);
extern "C" void GL_APIENTRY glClearDepthf(GLfloat d) { p_glClearDepthf(d); }
static void (GL_APIENTRY* p_glClearStencil)(GLint s);
extern "C" void GL_APIENTRY glClearStencil(GLint s) { p_glClearStencil(s); }
static void (GL_APIENTRY* p_glColorMask)(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
extern "C" void GL_APIENTRY glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha) { p_glColorMask(red, green, blue, alpha); }
static void (GL_APIENTRY* p_glCompileShader)(GLuint shader);
extern "C" void GL_APIENTRY glCompileShader(GLuint shader) { p_glCompileShader(shader); }
static void (GL_APIENTRY* p_glCompressedTexImage2D)(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void *data);
extern "C" void GL_APIENTRY glCompressedTexImage2D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void *data) { p_glCompressedTexImage2D(target, level, internalformat, width, height, border, imageSize, data); }
static void (GL_APIENTRY* p_glCompressedTexSubImage2D)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *data);
extern "C" void GL_APIENTRY glCompressedTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *data) { p_glCompressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format, imageSize, data); }
static void (GL_APIENTRY* p_glCopyTexImage2D)(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border);
extern "C" void GL_APIENTRY glCopyTexImage2D(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border) { p_glCopyTexImage2D(target, level, internalformat, x, y, width, height, border); }
static void (GL_APIENTRY* p_glCopyTexSubImage2D)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);
extern "C" void GL_APIENTRY glCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height) { p_glCopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height); }
static GLuint (GL_APIENTRY* p_glCreateProgram)(void);
extern "C" GLuint GL_APIENTRY glCreateProgram(void) { return p_glCreateProgram(); }
static GLuint (GL_APIENTRY* p_glCreateShader)(GLenum type);
extern "C" GLuint GL_APIENTRY glCreateShader(GLenum type) { return p_glCreateShader(type); }
static void (GL_APIENTRY* p_glCullFace)(GLenum mode);
extern "C" void GL_APIENTRY glCullFace(GLenum mode) { p_glCullFace(mode); }
static void (GL_APIENTRY* p_glDeleteBuffers)(GLsizei n, const GLuint *buffers);
extern "C" void GL_APIENTRY glDeleteBuffers(GLsizei n, const GLuint *buffers) { p_glDeleteBuffers(n, buffers); }
static void (GL_APIENTRY* p_glDeleteFramebuffers)(GLsizei n, const GLuint *framebuffers);
extern "C" void GL_APIENTRY glDeleteFramebuffers(GLsizei n, const GLuint *framebuffers) { p_glDeleteFramebuffers(n, framebuffers); }
static void (GL_APIENTRY* p_glDeleteProgram)(GLuint program);
extern "C" void GL_APIENTRY glDeleteProgram(GLuint program) { p_glDeleteProgram(program); }
static void (GL_APIENTRY* p_glDeleteRenderbuffers)(GLsizei n, const GLuint *renderbuffers);
extern "C" void GL_APIENTRY glDeleteRenderbuffers(GLsizei n, const GLuint *renderbuffers) { p_glDeleteRenderbuffers(n, renderbuffers); }
static void (GL_APIENTRY* p_glDeleteShader)(GLuint shader);
extern "C" void GL_APIENTRY glDeleteShader(GLuint shader) { p_glDeleteShader(shader); }
static void (GL_APIENTRY* p_glDeleteTextures)(GLsizei n, const GLuint *textures);
extern "C" void GL_APIENTRY glDeleteTextures(GLsizei n, const GLuint *textures) { p_glDeleteTextures(n, textures); }
static void (GL_APIENTRY* p_glDepthFunc)(GLenum func);
extern "C" void GL_APIENTRY glDepthFunc(GLenum func) { p_glDepthFunc(func); }
static void (GL_APIENTRY* p_glDepthMask)(GLboolean flag);
extern "C" void GL_APIENTRY glDepthMask(GLboolean flag) { p_glDepthMask(flag); }
static void (GL_APIENTRY* p_glDepthRangef)(GLfloat n, GLfloat f);
extern "C" void GL_APIENTRY glDepthRangef(GLfloat n, GLfloat f) { p_glDepthRangef(n, f); }
static void (GL_APIENTRY* p_glDetachShader)(GLuint program, GLuint shader);
extern "C" void GL_APIENTRY glDetachShader(GLuint program, GLuint shader) { p_glDetachShader(program, shader); }
static void (GL_APIENTRY* p_glDisable)(GLenum cap);
extern "C" void GL_APIENTRY glDisable(GLenum cap) { p_glDisable(cap); }
static void (GL_APIENTRY* p_glDisableVertexAttribArray)(GLuint index);
extern "C" void GL_APIENTRY glDisableVertexAttribArray(GLuint index) { p_glDisableVertexAttribArray(index); }
static void (GL_APIENTRY* p_glDrawArrays)(GLenum mode, GLint first, GLsizei count);
extern "C" void GL_APIENTRY glDrawArrays(GLenum mode, GLint first, GLsizei count) { p_glDrawArrays(mode, first, count); }
static void (GL_APIENTRY* p_glDrawElements)(GLenum mode, GLsizei count, GLenum type, const void *indices);
extern "C" void GL_APIENTRY glDrawElements(GLenum mode, GLsizei count, GLenum type, const void *indices) { p_glDrawElements(mode, count, type, indices); }
static void (GL_APIENTRY* p_glEnable)(GLenum cap);
extern "C" void GL_APIENTRY glEnable(GLenum cap) { p_glEnable(cap); }
static void (GL_APIENTRY* p_glEnableVertexAttribArray)(GLuint index);
extern "C" void GL_APIENTRY glEnableVertexAttribArray(GLuint index) { p_glEnableVertexAttribArray(index); }
static void (GL_APIENTRY* p_glFinish)(void);
extern "C" void GL_APIENTRY glFinish(void) { p_glFinish(); }
static void (GL_APIENTRY* p_glFlush)(void);
extern "C" void GL_APIENTRY glFlush(void) { p_glFlush(); }
static void (GL_APIENTRY* p_glFramebufferRenderbuffer)(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
extern "C" void GL_APIENTRY glFramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer) { p_glFramebufferRenderbuffer(target, attachment, renderbuffertarget, renderbuffer); }
static void (GL_APIENTRY* p_glFramebufferTexture2D)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
extern "C" void GL_APIENTRY glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) { p_glFramebufferTexture2D(target, attachment, textarget, texture, level); }
static void (GL_APIENTRY* p_glFrontFace)(GLenum mode);
extern "C" void GL_APIENTRY glFrontFace(GLenum mode) { p_glFrontFace(mode); }
static void (GL_APIENTRY* p_glGenBuffers)(GLsizei n, GLuint *buffers);
extern "C" void GL_APIENTRY glGenBuffers(GLsizei n, GLuint *buffers) { p_glGenBuffers(n, buffers); }
static void (GL_APIENTRY* p_glGenerateMipmap)(GLenum target);
extern "C" void GL_APIENTRY glGenerateMipmap(GLenum target) { p_glGenerateMipmap(target); }
static void (GL_APIENTRY* p_glGenFramebuffers)(GLsizei n, GLuint *framebuffers);
extern "C" void GL_APIENTRY glGenFramebuffers(GLsizei n, GLuint *framebuffers) { p_glGenFramebuffers(n, framebuffers); }
static void (GL_APIENTRY* p_glGenRenderbuffers)(GLsizei n, GLuint *renderbuffers);
extern "C" void GL_APIENTRY glGenRenderbuffers(GLsizei n, GLuint *renderbuffers) { p_glGenRenderbuffers(n, renderbuffers); }
static void (GL_APIENTRY* p_glGenTextures)(GLsizei n, GLuint *textures);
extern "C" void GL_APIENTRY glGenTextures(GLsizei n, GLuint *textures) { p_glGenTextures(n, textures); }
static void (GL_APIENTRY* p_glGetActiveAttrib)(GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name);
extern "C" void GL_APIENTRY glGetActiveAttrib(GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name) { p_glGetActiveAttrib(program, index, bufSize, length, size, type, name); }
static void (GL_APIENTRY* p_glGetActiveUniform)(GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name);
extern "C" void GL_APIENTRY glGetActiveUniform(GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name) { p_glGetActiveUniform(program, index, bufSize, length, size, type, name); }
static void (GL_APIENTRY* p_glGetAttachedShaders)(GLuint program, GLsizei maxCount, GLsizei *count, GLuint *shaders);
extern "C" void GL_APIENTRY glGetAttachedShaders(GLuint program, GLsizei maxCount, GLsizei *count, GLuint *shaders) { p_glGetAttachedShaders(program, maxCount, count, shaders); }
static GLint (GL_APIENTRY* p_glGetAttribLocation)(GLuint program, const GLchar *name);
extern "C" GLint GL_APIENTRY glGetAttribLocation(GLuint program, const GLchar *name) { return p_glGetAttribLocation(program, name); }
static void (GL_APIENTRY* p_glGetBooleanv)(GLenum pname, GLboolean *data);
extern "C" void GL_APIENTRY glGetBooleanv(GLenum pname, GLboolean *data) { p_glGetBooleanv(pname, data); }
static void (GL_APIENTRY* p_glGetBufferParameteriv)(GLenum target, GLenum pname, GLint *params);
extern "C" void GL_APIENTRY glGetBufferParameteriv(GLenum target, GLenum pname, GLint *params) { p_glGetBufferParameteriv(target, pname, params); }
static GLenum (GL_APIENTRY* p_glGetError)(void);
extern "C" GLenum GL_APIENTRY glGetError(void) { return p_glGetError(); }
static void (GL_APIENTRY* p_glGetFloatv)(GLenum pname, GLfloat *data);
extern "C" void GL_APIENTRY glGetFloatv(GLenum pname, GLfloat *data) { p_glGetFloatv(pname, data); }
static void (GL_APIENTRY* p_glGetFramebufferAttachmentParameteriv)(GLenum target, GLenum attachment, GLenum pname, GLint *params);
extern "C" void GL_APIENTRY glGetFramebufferAttachmentParameteriv(GLenum target, GLenum attachment, GLenum pname, GLint *params) { p_glGetFramebufferAttachmentParameteriv(target, attachment, pname, params); }
static void (GL_APIENTRY* p_glGetIntegerv)(GLenum pname, GLint *data);
extern "C" void GL_APIENTRY glGetIntegerv(GLenum pname, GLint *data) { p_glGetIntegerv(pname, data); }
static void (GL_APIENTRY* p_glGetProgramiv)(GLuint program, GLenum pname, GLint *params);
extern "C" void GL_APIENTRY glGetProgramiv(GLuint program, GLenum pname, GLint *params) { p_glGetProgramiv(program, pname, params); }
static void (GL_APIENTRY* p_glGetProgramInfoLog)(GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
extern "C" void GL_APIENTRY glGetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog) { p_glGetProgramInfoLog(program, bufSize, length, infoLog); }
static void (GL_APIENTRY* p_glGetRenderbufferParameteriv)(GLenum target, GLenum pname, GLint *params);
extern "C" void GL_APIENTRY glGetRenderbufferParameteriv(GLenum target, GLenum pname, GLint *params) { p_glGetRenderbufferParameteriv(target, pname, params); }
static void (GL_APIENTRY* p_glGetShaderiv)(GLuint shader, GLenum pname, GLint *params);
extern "C" void GL_APIENTRY glGetShaderiv(GLuint shader, GLenum pname, GLint *params) { p_glGetShaderiv(shader, pname, params); }
static void (GL_APIENTRY* p_glGetShaderInfoLog)(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
extern "C" void GL_APIENTRY glGetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog) { p_glGetShaderInfoLog(shader, bufSize, length, infoLog); }
static void (GL_APIENTRY* p_glGetShaderPrecisionFormat)(GLenum shadertype, GLenum precisiontype, GLint *range, GLint *precision);
extern "C" void GL_APIENTRY glGetShaderPrecisionFormat(GLenum shadertype, GLenum precisiontype, GLint *range, GLint *precision) { p_glGetShaderPrecisionFormat(shadertype, precisiontype, range, precision); }
static void (GL_APIENTRY* p_glGetShaderSource)(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *source);
extern "C" void GL_APIENTRY glGetShaderSource(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *source) { p_glGetShaderSource(shader, bufSize, length, source); }
static const GLubyte * (GL_APIENTRY* p_glGetString)(GLenum name);
extern "C" const GLubyte * GL_APIENTRY glGetString(GLenum name) { return p_glGetString(name); }
static void (GL_APIENTRY* p_glGetTexParameterfv)(GLenum target, GLenum pname, GLfloat *params);
extern "C" void GL_APIENTRY glGetTexParameterfv(GLenum target, GLenum pname, GLfloat *params) { p_glGetTexParameterfv(target, pname, params); }
static void (GL_APIENTRY* p_glGetTexParameteriv)(GLenum target, GLenum pname, GLint *params);
extern "C" void GL_APIENTRY glGetTexParameteriv(GLenum target, GLenum pname, GLint *params) { p_glGetTexParameteriv(target, pname, params); }
static void (GL_APIENTRY* p_glGetUniformfv)(GLuint program, GLint location, GLfloat *params);
extern "C" void GL_APIENTRY glGetUniformfv(GLuint program, GLint location, GLfloat *params) { p_glGetUniformfv(program, location, params); }
static void (GL_APIENTRY* p_glGetUniformiv)(GLuint program, GLint location, GLint *params);
extern "C" void GL_APIENTRY glGetUniformiv(GLuint program, GLint location, GLint *params) { p_glGetUniformiv(program, location, params); }
static GLint (GL_APIENTRY* p_glGetUniformLocation)(GLuint program, const GLchar *name);
extern "C" GLint GL_APIENTRY glGetUniformLocation(GLuint program, const GLchar *name) { return p_glGetUniformLocation(program, name); }
static void (GL_APIENTRY* p_glGetVertexAttribfv)(GLuint index, GLenum pname, GLfloat *params);
extern "C" void GL_APIENTRY glGetVertexAttribfv(GLuint index, GLenum pname, GLfloat *params) { p_glGetVertexAttribfv(index, pname, params); }
static void (GL_APIENTRY* p_glGetVertexAttribiv)(GLuint index, GLenum pname, GLint *params);
extern "C" void GL_APIENTRY glGetVertexAttribiv(GLuint index, GLenum pname, GLint *params) { p_glGetVertexAttribiv(index, pname, params); }
static void (GL_APIENTRY* p_glGetVertexAttribPointerv)(GLuint index, GLenum pname, void **pointer);
extern "C" void GL_APIENTRY glGetVertexAttribPointerv(GLuint index, GLenum pname, void **pointer) { p_glGetVertexAttribPointerv(index, pname, pointer); }
static void (GL_APIENTRY* p_glHint)(GLenum target, GLenum mode);
extern "C" void GL_APIENTRY glHint(GLenum target, GLenum mode) { p_glHint(target, mode); }
static GLboolean (GL_APIENTRY* p_glIsBuffer)(GLuint buffer);
extern "C" GLboolean GL_APIENTRY glIsBuffer(GLuint buffer) { return p_glIsBuffer(buffer); }
static GLboolean (GL_APIENTRY* p_glIsEnabled)(GLenum cap);
extern "C" GLboolean GL_APIENTRY glIsEnabled(GLenum cap) { return p_glIsEnabled(cap); }
static GLboolean (GL_APIENTRY* p_glIsFramebuffer)(GLuint framebuffer);
extern "C" GLboolean GL_APIENTRY glIsFramebuffer(GLuint framebuffer) { return p_glIsFramebuffer(framebuffer); }
static GLboolean (GL_APIENTRY* p_glIsProgram)(GLuint program);
extern "C" GLboolean GL_APIENTRY glIsProgram(GLuint program) { return p_glIsProgram(program); }
static GLboolean (GL_APIENTRY* p_glIsRenderbuffer)(GLuint renderbuffer);
extern "C" GLboolean GL_APIENTRY glIsRenderbuffer(GLuint renderbuffer) { return p_glIsRenderbuffer(renderbuffer); }
static GLboolean (GL_APIENTRY* p_glIsShader)(GLuint shader);
extern "C" GLboolean GL_APIENTRY glIsShader(GLuint shader) { return p_glIsShader(shader); }
static GLboolean (GL_APIENTRY* p_glIsTexture)(GLuint texture);
extern "C" GLboolean GL_APIENTRY glIsTexture(GLuint texture) { return p_glIsTexture(texture); }
static void (GL_APIENTRY* p_glLineWidth)(GLfloat width);
extern "C" void GL_APIENTRY glLineWidth(GLfloat width) { p_glLineWidth(width); }
static void (GL_APIENTRY* p_glLinkProgram)(GLuint program);
extern "C" void GL_APIENTRY glLinkProgram(GLuint program) { p_glLinkProgram(program); }
static void (GL_APIENTRY* p_glPixelStorei)(GLenum pname, GLint param);
extern "C" void GL_APIENTRY glPixelStorei(GLenum pname, GLint param) { p_glPixelStorei(pname, param); }
static void (GL_APIENTRY* p_glPolygonOffset)(GLfloat factor, GLfloat units);
extern "C" void GL_APIENTRY glPolygonOffset(GLfloat factor, GLfloat units) { p_glPolygonOffset(factor, units); }
static void (GL_APIENTRY* p_glReadPixels)(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void *pixels);
extern "C" void GL_APIENTRY glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void *pixels) { p_glReadPixels(x, y, width, height, format, type, pixels); }
static void (GL_APIENTRY* p_glReleaseShaderCompiler)(void);
extern "C" void GL_APIENTRY glReleaseShaderCompiler(void) { p_glReleaseShaderCompiler(); }
static void (GL_APIENTRY* p_glRenderbufferStorage)(GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
extern "C" void GL_APIENTRY glRenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height) { p_glRenderbufferStorage(target, internalformat, width, height); }
static void (GL_APIENTRY* p_glSampleCoverage)(GLfloat value, GLboolean invert);
extern "C" void GL_APIENTRY glSampleCoverage(GLfloat value, GLboolean invert) { p_glSampleCoverage(value, invert); }
static void (GL_APIENTRY* p_glScissor)(GLint x, GLint y, GLsizei width, GLsizei height);
extern "C" void GL_APIENTRY glScissor(GLint x, GLint y, GLsizei width, GLsizei height) { p_glScissor(x, y, width, height); }
static void (GL_APIENTRY* p_glShaderBinary)(GLsizei count, const GLuint *shaders, GLenum binaryFormat, const void *binary, GLsizei length);
extern "C" void GL_APIENTRY glShaderBinary(GLsizei count, const GLuint *shaders, GLenum binaryFormat, const void *binary, GLsizei length) { p_glShaderBinary(count, shaders, binaryFormat, binary, length); }
static void (GL_APIENTRY* p_glShaderSource)(GLuint shader, GLsizei count, const GLchar *const*string, const GLint *length);
extern "C" void GL_APIENTRY glShaderSource(GLuint shader, GLsizei count, const GLchar *const*string, const GLint *length) { p_glShaderSource(shader, count, string, length); }
static void (GL_APIENTRY* p_glStencilFunc)(GLenum func, GLint ref, GLuint mask);
extern "C" void GL_APIENTRY glStencilFunc(GLenum func, GLint ref, GLuint mask) { p_glStencilFunc(func, ref, mask); }
static void (GL_APIENTRY* p_glStencilFuncSeparate)(GLenum face, GLenum func, GLint ref, GLuint mask);
extern "C" void GL_APIENTRY glStencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask) { p_glStencilFuncSeparate(face, func, ref, mask); }
static void (GL_APIENTRY* p_glStencilMask)(GLuint mask);
extern "C" void GL_APIENTRY glStencilMask(GLuint mask) { p_glStencilMask(mask); }
static void (GL_APIENTRY* p_glStencilMaskSeparate)(GLenum face, GLuint mask);
extern "C" void GL_APIENTRY glStencilMaskSeparate(GLenum face, GLuint mask) { p_glStencilMaskSeparate(face, mask); }
static void (GL_APIENTRY* p_glStencilOp)(GLenum fail, GLenum zfail, GLenum zpass);
extern "C" void GL_APIENTRY glStencilOp(GLenum fail, GLenum zfail, GLenum zpass) { p_glStencilOp(fail, zfail, zpass); }
static void (GL_APIENTRY* p_glStencilOpSeparate)(GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass);
extern "C" void GL_APIENTRY glStencilOpSeparate(GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass) { p_glStencilOpSeparate(face, sfail, dpfail, dppass); }
static void (GL_APIENTRY* p_glTexImage2D)(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels);
extern "C" void GL_APIENTRY glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels) { p_glTexImage2D(target, level, internalformat, width, height, border, format, type, pixels); }
static void (GL_APIENTRY* p_glTexParameterf)(GLenum target, GLenum pname, GLfloat param);
extern "C" void GL_APIENTRY glTexParameterf(GLenum target, GLenum pname, GLfloat param) { p_glTexParameterf(target, pname, param); }
static void (GL_APIENTRY* p_glTexParameterfv)(GLenum target, GLenum pname, const GLfloat *params);
extern "C" void GL_APIENTRY glTexParameterfv(GLenum target, GLenum pname, const GLfloat *params) { p_glTexParameterfv(target, pname, params); }
static void (GL_APIENTRY* p_glTexParameteri)(GLenum target, GLenum pname, GLint param);
extern "C" void GL_APIENTRY glTexParameteri(GLenum target, GLenum pname, GLint param) { p_glTexParameteri(target, pname, param); }
static void (GL_APIENTRY* p_glTexParameteriv)(GLenum target, GLenum pname, const GLint *params);
extern "C" void GL_APIENTRY glTexParameteriv(GLenum target, GLenum pname, const GLint *params) { p_glTexParameteriv(target, pname, params); }
static void (GL_APIENTRY* p_glTexSubImage2D)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels);
extern "C" void GL_APIENTRY glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels) { p_glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels); }
static void (GL_APIENTRY* p_glUniform1f)(GLint location, GLfloat v0);
extern "C" void GL_APIENTRY glUniform1f(GLint location, GLfloat v0) { p_glUniform1f(location, v0); }
static void (GL_APIENTRY* p_glUniform1fv)(GLint location, GLsizei count, const GLfloat *value);
extern "C" void GL_APIENTRY glUniform1fv(GLint location, GLsizei count, const GLfloat *value) { p_glUniform1fv(location, count, value); }
static void (GL_APIENTRY* p_glUniform1i)(GLint location, GLint v0);
extern "C" void GL_APIENTRY glUniform1i(GLint location, GLint v0) { p_glUniform1i(location, v0); }
static void (GL_APIENTRY* p_glUniform1iv)(GLint location, GLsizei count, const GLint *value);
extern "C" void GL_APIENTRY glUniform1iv(GLint location, GLsizei count, const GLint *value) { p_glUniform1iv(location, count, value); }
static void (GL_APIENTRY* p_glUniform2f)(GLint location, GLfloat v0, GLfloat v1);
extern "C" void GL_APIENTRY glUniform2f(GLint location, GLfloat v0, GLfloat v1) { p_glUniform2f(location, v0, v1); }
static void (GL_APIENTRY* p_glUniform2fv)(GLint location, GLsizei count, const GLfloat *value);
extern "C" void GL_APIENTRY glUniform2fv(GLint location, GLsizei count, const GLfloat *value) { p_glUniform2fv(location, count, value); }
static void (GL_APIENTRY* p_glUniform2i)(GLint location, GLint v0, GLint v1);
extern "C" void GL_APIENTRY glUniform2i(GLint location, GLint v0, GLint v1) { p_glUniform2i(location, v0, v1); }
static void (GL_APIENTRY* p_glUniform2iv)(GLint location, GLsizei count, const GLint *value);
extern "C" void GL_APIENTRY glUniform2iv(GLint location, GLsizei count, const GLint *value) { p_glUniform2iv(location, count, value); }
static void (GL_APIENTRY* p_glUniform3f)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
extern "C" void GL_APIENTRY glUniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2) { p_glUniform3f(location, v0, v1, v2); }
static void (GL_APIENTRY* p_glUniform3fv)(GLint location, GLsizei count, const GLfloat *value);
extern "C" void GL_APIENTRY glUniform3fv(GLint location, GLsizei count, const GLfloat *value) { p_glUniform3fv(location, count, value); }
static void (GL_APIENTRY* p_glUniform3i)(GLint location, GLint v0, GLint v1, GLint v2);
extern "C" void GL_APIENTRY glUniform3i(GLint location, GLint v0, GLint v1, GLint v2) { p_glUniform3i(location, v0, v1, v2); }
static void (GL_APIENTRY* p_glUniform3iv)(GLint location, GLsizei count, const GLint *value);
extern "C" void GL_APIENTRY glUniform3iv(GLint location, GLsizei count, const GLint *value) { p_glUniform3iv(location, count, value); }
static void (GL_APIENTRY* p_glUniform4f)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
extern "C" void GL_APIENTRY glUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) { p_glUniform4f(location, v0, v1, v2, v3); }
static void (GL_APIENTRY* p_glUniform4fv)(GLint location, GLsizei count, const GLfloat *value);
extern "C" void GL_APIENTRY glUniform4fv(GLint location, GLsizei count, const GLfloat *value) { p_glUniform4fv(location, count, value); }
static void (GL_APIENTRY* p_glUniform4i)(GLint location, GLint v0, GLint v1, GLint v2, GLint v3);
extern "C" void GL_APIENTRY glUniform4i(GLint location, GLint v0, GLint v1, GLint v2, GLint v3) { p_glUniform4i(location, v0, v1, v2, v3); }
static void (GL_APIENTRY* p_glUniform4iv)(GLint location, GLsizei count, const GLint *value);
extern "C" void GL_APIENTRY glUniform4iv(GLint location, GLsizei count, const GLint *value) { p_glUniform4iv(location, count, value); }
static void (GL_APIENTRY* p_glUniformMatrix2fv)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
extern "C" void GL_APIENTRY glUniformMatrix2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) { p_glUniformMatrix2fv(location, count, transpose, value); }
static void (GL_APIENTRY* p_glUniformMatrix3fv)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
extern "C" void GL_APIENTRY glUniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) { p_glUniformMatrix3fv(location, count, transpose, value); }
static void (GL_APIENTRY* p_glUniformMatrix4fv)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
extern "C" void GL_APIENTRY glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) { p_glUniformMatrix4fv(location, count, transpose, value); }
static void (GL_APIENTRY* p_glUseProgram)(GLuint program);
extern "C" void GL_APIENTRY glUseProgram(GLuint program) { p_glUseProgram(program); }
static void (GL_APIENTRY* p_glValidateProgram)(GLuint program);
extern "C" void GL_APIENTRY glValidateProgram(GLuint program) { p_glValidateProgram(program); }
static void (GL_APIENTRY* p_glVertexAttrib1f)(GLuint index, GLfloat x);
extern "C" void GL_APIENTRY glVertexAttrib1f(GLuint index, GLfloat x) { p_glVertexAttrib1f(index, x); }
static void (GL_APIENTRY* p_glVertexAttrib1fv)(GLuint index, const GLfloat *v);
extern "C" void GL_APIENTRY glVertexAttrib1fv(GLuint index, const GLfloat *v) { p_glVertexAttrib1fv(index, v); }
static void (GL_APIENTRY* p_glVertexAttrib2f)(GLuint index, GLfloat x, GLfloat y);
extern "C" void GL_APIENTRY glVertexAttrib2f(GLuint index, GLfloat x, GLfloat y) { p_glVertexAttrib2f(index, x, y); }
static void (GL_APIENTRY* p_glVertexAttrib2fv)(GLuint index, const GLfloat *v);
extern "C" void GL_APIENTRY glVertexAttrib2fv(GLuint index, const GLfloat *v) { p_glVertexAttrib2fv(index, v); }
static void (GL_APIENTRY* p_glVertexAttrib3f)(GLuint index, GLfloat x, GLfloat y, GLfloat z);
extern "C" void GL_APIENTRY glVertexAttrib3f(GLuint index, GLfloat x, GLfloat y, GLfloat z) { p_glVertexAttrib3f(index, x, y, z); }
static void (GL_APIENTRY* p_glVertexAttrib3fv)(GLuint index, const GLfloat *v);
extern "C" void GL_APIENTRY glVertexAttrib3fv(GLuint index, const GLfloat *v) { p_glVertexAttrib3fv(index, v); }
static void (GL_APIENTRY* p_glVertexAttrib4f)(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
extern "C" void GL_APIENTRY glVertexAttrib4f(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w) { p_glVertexAttrib4f(index, x, y, z, w); }
static void (GL_APIENTRY* p_glVertexAttrib4fv)(GLuint index, const GLfloat *v);
extern "C" void GL_APIENTRY glVertexAttrib4fv(GLuint index, const GLfloat *v) { p_glVertexAttrib4fv(index, v); }
static void (GL_APIENTRY* p_glVertexAttribPointer)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer);
extern "C" void GL_APIENTRY glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer) { p_glVertexAttribPointer(index, size, type, normalized, stride, pointer); }
static void (GL_APIENTRY* p_glViewport)(GLint x, GLint y, GLsizei width, GLsizei height);
extern "C" void GL_APIENTRY glViewport(GLint x, GLint y, GLsizei width, GLsizei height) { p_glViewport(x, y, width, height); }
static EGLBoolean (EGLAPIENTRY* p_eglChooseConfig)(EGLDisplay dpy, const EGLint *attrib_list, EGLConfig *configs, EGLint config_size, EGLint *num_config);
extern "C" EGLBoolean EGLAPIENTRY eglChooseConfig(EGLDisplay dpy, const EGLint *attrib_list, EGLConfig *configs, EGLint config_size, EGLint *num_config) { return p_eglChooseConfig(dpy, attrib_list, configs, config_size, num_config); }
static EGLBoolean (EGLAPIENTRY* p_eglCopyBuffers)(EGLDisplay dpy, EGLSurface surface, EGLNativePixmapType target);
extern "C" EGLBoolean EGLAPIENTRY eglCopyBuffers(EGLDisplay dpy, EGLSurface surface, EGLNativePixmapType target) { return p_eglCopyBuffers(dpy, surface, target); }
static EGLContext (EGLAPIENTRY* p_eglCreateContext)(EGLDisplay dpy, EGLConfig config, EGLContext share_context, const EGLint *attrib_list);
extern "C" EGLContext EGLAPIENTRY eglCreateContext(EGLDisplay dpy, EGLConfig config, EGLContext share_context, const EGLint *attrib_list) { return p_eglCreateContext(dpy, config, share_context, attrib_list); }
static EGLSurface (EGLAPIENTRY* p_eglCreatePbufferSurface)(EGLDisplay dpy, EGLConfig config, const EGLint *attrib_list);
extern "C" EGLSurface EGLAPIENTRY eglCreatePbufferSurface(EGLDisplay dpy, EGLConfig config, const EGLint *attrib_list) { return p_eglCreatePbufferSurface(dpy, config, attrib_list); }
static EGLSurface (EGLAPIENTRY* p_eglCreatePixmapSurface)(EGLDisplay dpy, EGLConfig config, EGLNativePixmapType pixmap, const EGLint *attrib_list);
extern "C" EGLSurface EGLAPIENTRY eglCreatePixmapSurface(EGLDisplay dpy, EGLConfig config, EGLNativePixmapType pixmap, const EGLint *attrib_list) { return p_eglCreatePixmapSurface(dpy, config, pixmap, attrib_list); }
static EGLSurface (EGLAPIENTRY* p_eglCreateWindowSurface)(EGLDisplay dpy, EGLConfig config, EGLNativeWindowType win, const EGLint *attrib_list);
extern "C" EGLSurface EGLAPIENTRY eglCreateWindowSurface(EGLDisplay dpy, EGLConfig config, EGLNativeWindowType win, const EGLint *attrib_list) { return p_eglCreateWindowSurface(dpy, config, win, attrib_list); }
static EGLBoolean (EGLAPIENTRY* p_eglDestroyContext)(EGLDisplay dpy, EGLContext ctx);
extern "C" EGLBoolean EGLAPIENTRY eglDestroyContext(EGLDisplay dpy, EGLContext ctx) { return p_eglDestroyContext(dpy, ctx); }
static EGLBoolean (EGLAPIENTRY* p_eglDestroySurface)(EGLDisplay dpy, EGLSurface surface);
extern "C" EGLBoolean EGLAPIENTRY eglDestroySurface(EGLDisplay dpy, EGLSurface surface) { return p_eglDestroySurface(dpy, surface); }
static EGLBoolean (EGLAPIENTRY* p_eglGetConfigAttrib)(EGLDisplay dpy, EGLConfig config, EGLint attribute, EGLint *value);
extern "C" EGLBoolean EGLAPIENTRY eglGetConfigAttrib(EGLDisplay dpy, EGLConfig config, EGLint attribute, EGLint *value) { return p_eglGetConfigAttrib(dpy, config, attribute, value); }
static EGLBoolean (EGLAPIENTRY* p_eglGetConfigs)(EGLDisplay dpy, EGLConfig *configs, EGLint config_size, EGLint *num_config);
extern "C" EGLBoolean EGLAPIENTRY eglGetConfigs(EGLDisplay dpy, EGLConfig *configs, EGLint config_size, EGLint *num_config) { return p_eglGetConfigs(dpy, configs, config_size, num_config); }
static EGLDisplay (EGLAPIENTRY* p_eglGetCurrentDisplay)(void);
extern "C" EGLDisplay EGLAPIENTRY eglGetCurrentDisplay(void) { return p_eglGetCurrentDisplay(); }
static EGLSurface (EGLAPIENTRY* p_eglGetCurrentSurface)(EGLint readdraw);
extern "C" EGLSurface EGLAPIENTRY eglGetCurrentSurface(EGLint readdraw) { return p_eglGetCurrentSurface(readdraw); }
static EGLDisplay (EGLAPIENTRY* p_eglGetDisplay)(EGLNativeDisplayType display_id);
extern "C" EGLDisplay EGLAPIENTRY eglGetDisplay(EGLNativeDisplayType display_id) { return p_eglGetDisplay(display_id); }
static EGLint (EGLAPIENTRY* p_eglGetError)(void);
extern "C" EGLint EGLAPIENTRY eglGetError(void) { return p_eglGetError(); }
static __eglMustCastToProperFunctionPointerType (EGLAPIENTRY* p_eglGetProcAddress)(const char *procname);
extern "C" __eglMustCastToProperFunctionPointerType EGLAPIENTRY eglGetProcAddress(const char *procname) { return p_eglGetProcAddress(procname); }
static EGLBoolean (EGLAPIENTRY* p_eglInitialize)(EGLDisplay dpy, EGLint *major, EGLint *minor);
extern "C" EGLBoolean EGLAPIENTRY eglInitialize(EGLDisplay dpy, EGLint *major, EGLint *minor) { return p_eglInitialize(dpy, major, minor); }
static EGLBoolean (EGLAPIENTRY* p_eglMakeCurrent)(EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx);
extern "C" EGLBoolean EGLAPIENTRY eglMakeCurrent(EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx) { return p_eglMakeCurrent(dpy, draw, read, ctx); }
static EGLBoolean (EGLAPIENTRY* p_eglQueryContext)(EGLDisplay dpy, EGLContext ctx, EGLint attribute, EGLint *value);
extern "C" EGLBoolean EGLAPIENTRY eglQueryContext(EGLDisplay dpy, EGLContext ctx, EGLint attribute, EGLint *value) { return p_eglQueryContext(dpy, ctx, attribute, value); }
static const char * (EGLAPIENTRY* p_eglQueryString)(EGLDisplay dpy, EGLint name);
extern "C" const char * EGLAPIENTRY eglQueryString(EGLDisplay dpy, EGLint name) { return p_eglQueryString(dpy, name); }
static EGLBoolean (EGLAPIENTRY* p_eglQuerySurface)(EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint *value);
extern "C" EGLBoolean EGLAPIENTRY eglQuerySurface(EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint *value) { return p_eglQuerySurface(dpy, surface, attribute, value); }
static EGLBoolean (EGLAPIENTRY* p_eglSwapBuffers)(EGLDisplay dpy, EGLSurface surface);
extern "C" EGLBoolean EGLAPIENTRY eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) { return p_eglSwapBuffers(dpy, surface); }
static EGLBoolean (EGLAPIENTRY* p_eglTerminate)(EGLDisplay dpy);
extern "C" EGLBoolean EGLAPIENTRY eglTerminate(EGLDisplay dpy) { return p_eglTerminate(dpy); }
static EGLBoolean (EGLAPIENTRY* p_eglWaitGL)(void);
extern "C" EGLBoolean EGLAPIENTRY eglWaitGL(void) { return p_eglWaitGL(); }
static EGLBoolean (EGLAPIENTRY* p_eglWaitNative)(EGLint engine);
extern "C" EGLBoolean EGLAPIENTRY eglWaitNative(EGLint engine) { return p_eglWaitNative(engine); }
static EGLBoolean (EGLAPIENTRY* p_eglBindTexImage)(EGLDisplay dpy, EGLSurface surface, EGLint buffer);
extern "C" EGLBoolean EGLAPIENTRY eglBindTexImage(EGLDisplay dpy, EGLSurface surface, EGLint buffer) { return p_eglBindTexImage(dpy, surface, buffer); }
static EGLBoolean (EGLAPIENTRY* p_eglReleaseTexImage)(EGLDisplay dpy, EGLSurface surface, EGLint buffer);
extern "C" EGLBoolean EGLAPIENTRY eglReleaseTexImage(EGLDisplay dpy, EGLSurface surface, EGLint buffer) { return p_eglReleaseTexImage(dpy, surface, buffer); }
static EGLBoolean (EGLAPIENTRY* p_eglSurfaceAttrib)(EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint value);
extern "C" EGLBoolean EGLAPIENTRY eglSurfaceAttrib(EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint value) { return p_eglSurfaceAttrib(dpy, surface, attribute, value); }
static EGLBoolean (EGLAPIENTRY* p_eglSwapInterval)(EGLDisplay dpy, EGLint interval);
extern "C" EGLBoolean EGLAPIENTRY eglSwapInterval(EGLDisplay dpy, EGLint interval) { return p_eglSwapInterval(dpy, interval); }
static EGLBoolean (EGLAPIENTRY* p_eglBindAPI)(EGLenum api);
extern "C" EGLBoolean EGLAPIENTRY eglBindAPI(EGLenum api) { return p_eglBindAPI(api); }
static EGLenum (EGLAPIENTRY* p_eglQueryAPI)(void);
extern "C" EGLenum EGLAPIENTRY eglQueryAPI(void) { return p_eglQueryAPI(); }
static EGLSurface (EGLAPIENTRY* p_eglCreatePbufferFromClientBuffer)(EGLDisplay dpy, EGLenum buftype, EGLClientBuffer buffer, EGLConfig config, const EGLint *attrib_list);
extern "C" EGLSurface EGLAPIENTRY eglCreatePbufferFromClientBuffer(EGLDisplay dpy, EGLenum buftype, EGLClientBuffer buffer, EGLConfig config, const EGLint *attrib_list) { return p_eglCreatePbufferFromClientBuffer(dpy, buftype, buffer, config, attrib_list); }
static EGLBoolean (EGLAPIENTRY* p_eglReleaseThread)(void);
extern "C" EGLBoolean EGLAPIENTRY eglReleaseThread(void) { return p_eglReleaseThread(); }
static EGLBoolean (EGLAPIENTRY* p_eglWaitClient)(void);
extern "C" EGLBoolean EGLAPIENTRY eglWaitClient(void) { return p_eglWaitClient(); }
static EGLContext (EGLAPIENTRY* p_eglGetCurrentContext)(void);
extern "C" EGLContext EGLAPIENTRY eglGetCurrentContext(void) { return p_eglGetCurrentContext(); }
static EGLSync (EGLAPIENTRY* p_eglCreateSync)(EGLDisplay dpy, EGLenum type, const EGLAttrib *attrib_list);
extern "C" EGLSync EGLAPIENTRY eglCreateSync(EGLDisplay dpy, EGLenum type, const EGLAttrib *attrib_list) { return p_eglCreateSync(dpy, type, attrib_list); }
static EGLBoolean (EGLAPIENTRY* p_eglDestroySync)(EGLDisplay dpy, EGLSync sync);
extern "C" EGLBoolean EGLAPIENTRY eglDestroySync(EGLDisplay dpy, EGLSync sync) { return p_eglDestroySync(dpy, sync); }
static EGLint (EGLAPIENTRY* p_eglClientWaitSync)(EGLDisplay dpy, EGLSync sync, EGLint flags, EGLTime timeout);
extern "C" EGLint EGLAPIENTRY eglClientWaitSync(EGLDisplay dpy, EGLSync sync, EGLint flags, EGLTime timeout) { return p_eglClientWaitSync(dpy, sync, flags, timeout); }
static EGLBoolean (EGLAPIENTRY* p_eglGetSyncAttrib)(EGLDisplay dpy, EGLSync sync, EGLint attribute, EGLAttrib *value);
extern "C" EGLBoolean EGLAPIENTRY eglGetSyncAttrib(EGLDisplay dpy, EGLSync sync, EGLint attribute, EGLAttrib *value) { return p_eglGetSyncAttrib(dpy, sync, attribute, value); }
static EGLImage (EGLAPIENTRY* p_eglCreateImage)(EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLAttrib *attrib_list);
extern "C" EGLImage EGLAPIENTRY eglCreateImage(EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLAttrib *attrib_list) { return p_eglCreateImage(dpy, ctx, target, buffer, attrib_list); }
static EGLBoolean (EGLAPIENTRY* p_eglDestroyImage)(EGLDisplay dpy, EGLImage image);
extern "C" EGLBoolean EGLAPIENTRY eglDestroyImage(EGLDisplay dpy, EGLImage image) { return p_eglDestroyImage(dpy, image); }
static EGLDisplay (EGLAPIENTRY* p_eglGetPlatformDisplay)(EGLenum platform, void *native_display, const EGLAttrib *attrib_list);
extern "C" EGLDisplay EGLAPIENTRY eglGetPlatformDisplay(EGLenum platform, void *native_display, const EGLAttrib *attrib_list) { return p_eglGetPlatformDisplay(platform, native_display, attrib_list); }
static EGLSurface (EGLAPIENTRY* p_eglCreatePlatformWindowSurface)(EGLDisplay dpy, EGLConfig config, void *native_window, const EGLAttrib *attrib_list);
extern "C" EGLSurface EGLAPIENTRY eglCreatePlatformWindowSurface(EGLDisplay dpy, EGLConfig config, void *native_window, const EGLAttrib *attrib_list) { return p_eglCreatePlatformWindowSurface(dpy, config, native_window, attrib_list); }
static EGLSurface (EGLAPIENTRY* p_eglCreatePlatformPixmapSurface)(EGLDisplay dpy, EGLConfig config, void *native_pixmap, const EGLAttrib *attrib_list);
extern "C" EGLSurface EGLAPIENTRY eglCreatePlatformPixmapSurface(EGLDisplay dpy, EGLConfig config, void *native_pixmap, const EGLAttrib *attrib_list) { return p_eglCreatePlatformPixmapSurface(dpy, config, native_pixmap, attrib_list); }
static EGLBoolean (EGLAPIENTRY* p_eglWaitSync)(EGLDisplay dpy, EGLSync sync, EGLint flags);
extern "C" EGLBoolean EGLAPIENTRY eglWaitSync(EGLDisplay dpy, EGLSync sync, EGLint flags) { return p_eglWaitSync(dpy, sync, flags); }

int GlCargar(void* (*buscar)(const char* nombre))
{
    int faltan = 0;
    p_glActiveTexture = (void (GL_APIENTRY*)(GLenum texture))buscar("glActiveTexture");
    if (!p_glActiveTexture) faltan++;
    p_glAttachShader = (void (GL_APIENTRY*)(GLuint program, GLuint shader))buscar("glAttachShader");
    if (!p_glAttachShader) faltan++;
    p_glBindAttribLocation = (void (GL_APIENTRY*)(GLuint program, GLuint index, const GLchar *name))buscar("glBindAttribLocation");
    if (!p_glBindAttribLocation) faltan++;
    p_glBindBuffer = (void (GL_APIENTRY*)(GLenum target, GLuint buffer))buscar("glBindBuffer");
    if (!p_glBindBuffer) faltan++;
    p_glBindFramebuffer = (void (GL_APIENTRY*)(GLenum target, GLuint framebuffer))buscar("glBindFramebuffer");
    if (!p_glBindFramebuffer) faltan++;
    p_glBindRenderbuffer = (void (GL_APIENTRY*)(GLenum target, GLuint renderbuffer))buscar("glBindRenderbuffer");
    if (!p_glBindRenderbuffer) faltan++;
    p_glBindTexture = (void (GL_APIENTRY*)(GLenum target, GLuint texture))buscar("glBindTexture");
    if (!p_glBindTexture) faltan++;
    p_glBlendColor = (void (GL_APIENTRY*)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha))buscar("glBlendColor");
    if (!p_glBlendColor) faltan++;
    p_glBlendEquation = (void (GL_APIENTRY*)(GLenum mode))buscar("glBlendEquation");
    if (!p_glBlendEquation) faltan++;
    p_glBlendEquationSeparate = (void (GL_APIENTRY*)(GLenum modeRGB, GLenum modeAlpha))buscar("glBlendEquationSeparate");
    if (!p_glBlendEquationSeparate) faltan++;
    p_glBlendFunc = (void (GL_APIENTRY*)(GLenum sfactor, GLenum dfactor))buscar("glBlendFunc");
    if (!p_glBlendFunc) faltan++;
    p_glBlendFuncSeparate = (void (GL_APIENTRY*)(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha))buscar("glBlendFuncSeparate");
    if (!p_glBlendFuncSeparate) faltan++;
    p_glBufferData = (void (GL_APIENTRY*)(GLenum target, GLsizeiptr size, const void *data, GLenum usage))buscar("glBufferData");
    if (!p_glBufferData) faltan++;
    p_glBufferSubData = (void (GL_APIENTRY*)(GLenum target, GLintptr offset, GLsizeiptr size, const void *data))buscar("glBufferSubData");
    if (!p_glBufferSubData) faltan++;
    p_glCheckFramebufferStatus = (GLenum (GL_APIENTRY*)(GLenum target))buscar("glCheckFramebufferStatus");
    if (!p_glCheckFramebufferStatus) faltan++;
    p_glClear = (void (GL_APIENTRY*)(GLbitfield mask))buscar("glClear");
    if (!p_glClear) faltan++;
    p_glClearColor = (void (GL_APIENTRY*)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha))buscar("glClearColor");
    if (!p_glClearColor) faltan++;
    p_glClearDepthf = (void (GL_APIENTRY*)(GLfloat d))buscar("glClearDepthf");
    if (!p_glClearDepthf) faltan++;
    p_glClearStencil = (void (GL_APIENTRY*)(GLint s))buscar("glClearStencil");
    if (!p_glClearStencil) faltan++;
    p_glColorMask = (void (GL_APIENTRY*)(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha))buscar("glColorMask");
    if (!p_glColorMask) faltan++;
    p_glCompileShader = (void (GL_APIENTRY*)(GLuint shader))buscar("glCompileShader");
    if (!p_glCompileShader) faltan++;
    p_glCompressedTexImage2D = (void (GL_APIENTRY*)(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void *data))buscar("glCompressedTexImage2D");
    if (!p_glCompressedTexImage2D) faltan++;
    p_glCompressedTexSubImage2D = (void (GL_APIENTRY*)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *data))buscar("glCompressedTexSubImage2D");
    if (!p_glCompressedTexSubImage2D) faltan++;
    p_glCopyTexImage2D = (void (GL_APIENTRY*)(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border))buscar("glCopyTexImage2D");
    if (!p_glCopyTexImage2D) faltan++;
    p_glCopyTexSubImage2D = (void (GL_APIENTRY*)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height))buscar("glCopyTexSubImage2D");
    if (!p_glCopyTexSubImage2D) faltan++;
    p_glCreateProgram = (GLuint (GL_APIENTRY*)(void))buscar("glCreateProgram");
    if (!p_glCreateProgram) faltan++;
    p_glCreateShader = (GLuint (GL_APIENTRY*)(GLenum type))buscar("glCreateShader");
    if (!p_glCreateShader) faltan++;
    p_glCullFace = (void (GL_APIENTRY*)(GLenum mode))buscar("glCullFace");
    if (!p_glCullFace) faltan++;
    p_glDeleteBuffers = (void (GL_APIENTRY*)(GLsizei n, const GLuint *buffers))buscar("glDeleteBuffers");
    if (!p_glDeleteBuffers) faltan++;
    p_glDeleteFramebuffers = (void (GL_APIENTRY*)(GLsizei n, const GLuint *framebuffers))buscar("glDeleteFramebuffers");
    if (!p_glDeleteFramebuffers) faltan++;
    p_glDeleteProgram = (void (GL_APIENTRY*)(GLuint program))buscar("glDeleteProgram");
    if (!p_glDeleteProgram) faltan++;
    p_glDeleteRenderbuffers = (void (GL_APIENTRY*)(GLsizei n, const GLuint *renderbuffers))buscar("glDeleteRenderbuffers");
    if (!p_glDeleteRenderbuffers) faltan++;
    p_glDeleteShader = (void (GL_APIENTRY*)(GLuint shader))buscar("glDeleteShader");
    if (!p_glDeleteShader) faltan++;
    p_glDeleteTextures = (void (GL_APIENTRY*)(GLsizei n, const GLuint *textures))buscar("glDeleteTextures");
    if (!p_glDeleteTextures) faltan++;
    p_glDepthFunc = (void (GL_APIENTRY*)(GLenum func))buscar("glDepthFunc");
    if (!p_glDepthFunc) faltan++;
    p_glDepthMask = (void (GL_APIENTRY*)(GLboolean flag))buscar("glDepthMask");
    if (!p_glDepthMask) faltan++;
    p_glDepthRangef = (void (GL_APIENTRY*)(GLfloat n, GLfloat f))buscar("glDepthRangef");
    if (!p_glDepthRangef) faltan++;
    p_glDetachShader = (void (GL_APIENTRY*)(GLuint program, GLuint shader))buscar("glDetachShader");
    if (!p_glDetachShader) faltan++;
    p_glDisable = (void (GL_APIENTRY*)(GLenum cap))buscar("glDisable");
    if (!p_glDisable) faltan++;
    p_glDisableVertexAttribArray = (void (GL_APIENTRY*)(GLuint index))buscar("glDisableVertexAttribArray");
    if (!p_glDisableVertexAttribArray) faltan++;
    p_glDrawArrays = (void (GL_APIENTRY*)(GLenum mode, GLint first, GLsizei count))buscar("glDrawArrays");
    if (!p_glDrawArrays) faltan++;
    p_glDrawElements = (void (GL_APIENTRY*)(GLenum mode, GLsizei count, GLenum type, const void *indices))buscar("glDrawElements");
    if (!p_glDrawElements) faltan++;
    p_glEnable = (void (GL_APIENTRY*)(GLenum cap))buscar("glEnable");
    if (!p_glEnable) faltan++;
    p_glEnableVertexAttribArray = (void (GL_APIENTRY*)(GLuint index))buscar("glEnableVertexAttribArray");
    if (!p_glEnableVertexAttribArray) faltan++;
    p_glFinish = (void (GL_APIENTRY*)(void))buscar("glFinish");
    if (!p_glFinish) faltan++;
    p_glFlush = (void (GL_APIENTRY*)(void))buscar("glFlush");
    if (!p_glFlush) faltan++;
    p_glFramebufferRenderbuffer = (void (GL_APIENTRY*)(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer))buscar("glFramebufferRenderbuffer");
    if (!p_glFramebufferRenderbuffer) faltan++;
    p_glFramebufferTexture2D = (void (GL_APIENTRY*)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level))buscar("glFramebufferTexture2D");
    if (!p_glFramebufferTexture2D) faltan++;
    p_glFrontFace = (void (GL_APIENTRY*)(GLenum mode))buscar("glFrontFace");
    if (!p_glFrontFace) faltan++;
    p_glGenBuffers = (void (GL_APIENTRY*)(GLsizei n, GLuint *buffers))buscar("glGenBuffers");
    if (!p_glGenBuffers) faltan++;
    p_glGenerateMipmap = (void (GL_APIENTRY*)(GLenum target))buscar("glGenerateMipmap");
    if (!p_glGenerateMipmap) faltan++;
    p_glGenFramebuffers = (void (GL_APIENTRY*)(GLsizei n, GLuint *framebuffers))buscar("glGenFramebuffers");
    if (!p_glGenFramebuffers) faltan++;
    p_glGenRenderbuffers = (void (GL_APIENTRY*)(GLsizei n, GLuint *renderbuffers))buscar("glGenRenderbuffers");
    if (!p_glGenRenderbuffers) faltan++;
    p_glGenTextures = (void (GL_APIENTRY*)(GLsizei n, GLuint *textures))buscar("glGenTextures");
    if (!p_glGenTextures) faltan++;
    p_glGetActiveAttrib = (void (GL_APIENTRY*)(GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name))buscar("glGetActiveAttrib");
    if (!p_glGetActiveAttrib) faltan++;
    p_glGetActiveUniform = (void (GL_APIENTRY*)(GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name))buscar("glGetActiveUniform");
    if (!p_glGetActiveUniform) faltan++;
    p_glGetAttachedShaders = (void (GL_APIENTRY*)(GLuint program, GLsizei maxCount, GLsizei *count, GLuint *shaders))buscar("glGetAttachedShaders");
    if (!p_glGetAttachedShaders) faltan++;
    p_glGetAttribLocation = (GLint (GL_APIENTRY*)(GLuint program, const GLchar *name))buscar("glGetAttribLocation");
    if (!p_glGetAttribLocation) faltan++;
    p_glGetBooleanv = (void (GL_APIENTRY*)(GLenum pname, GLboolean *data))buscar("glGetBooleanv");
    if (!p_glGetBooleanv) faltan++;
    p_glGetBufferParameteriv = (void (GL_APIENTRY*)(GLenum target, GLenum pname, GLint *params))buscar("glGetBufferParameteriv");
    if (!p_glGetBufferParameteriv) faltan++;
    p_glGetError = (GLenum (GL_APIENTRY*)(void))buscar("glGetError");
    if (!p_glGetError) faltan++;
    p_glGetFloatv = (void (GL_APIENTRY*)(GLenum pname, GLfloat *data))buscar("glGetFloatv");
    if (!p_glGetFloatv) faltan++;
    p_glGetFramebufferAttachmentParameteriv = (void (GL_APIENTRY*)(GLenum target, GLenum attachment, GLenum pname, GLint *params))buscar("glGetFramebufferAttachmentParameteriv");
    if (!p_glGetFramebufferAttachmentParameteriv) faltan++;
    p_glGetIntegerv = (void (GL_APIENTRY*)(GLenum pname, GLint *data))buscar("glGetIntegerv");
    if (!p_glGetIntegerv) faltan++;
    p_glGetProgramiv = (void (GL_APIENTRY*)(GLuint program, GLenum pname, GLint *params))buscar("glGetProgramiv");
    if (!p_glGetProgramiv) faltan++;
    p_glGetProgramInfoLog = (void (GL_APIENTRY*)(GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog))buscar("glGetProgramInfoLog");
    if (!p_glGetProgramInfoLog) faltan++;
    p_glGetRenderbufferParameteriv = (void (GL_APIENTRY*)(GLenum target, GLenum pname, GLint *params))buscar("glGetRenderbufferParameteriv");
    if (!p_glGetRenderbufferParameteriv) faltan++;
    p_glGetShaderiv = (void (GL_APIENTRY*)(GLuint shader, GLenum pname, GLint *params))buscar("glGetShaderiv");
    if (!p_glGetShaderiv) faltan++;
    p_glGetShaderInfoLog = (void (GL_APIENTRY*)(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog))buscar("glGetShaderInfoLog");
    if (!p_glGetShaderInfoLog) faltan++;
    p_glGetShaderPrecisionFormat = (void (GL_APIENTRY*)(GLenum shadertype, GLenum precisiontype, GLint *range, GLint *precision))buscar("glGetShaderPrecisionFormat");
    if (!p_glGetShaderPrecisionFormat) faltan++;
    p_glGetShaderSource = (void (GL_APIENTRY*)(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *source))buscar("glGetShaderSource");
    if (!p_glGetShaderSource) faltan++;
    p_glGetString = (const GLubyte * (GL_APIENTRY*)(GLenum name))buscar("glGetString");
    if (!p_glGetString) faltan++;
    p_glGetTexParameterfv = (void (GL_APIENTRY*)(GLenum target, GLenum pname, GLfloat *params))buscar("glGetTexParameterfv");
    if (!p_glGetTexParameterfv) faltan++;
    p_glGetTexParameteriv = (void (GL_APIENTRY*)(GLenum target, GLenum pname, GLint *params))buscar("glGetTexParameteriv");
    if (!p_glGetTexParameteriv) faltan++;
    p_glGetUniformfv = (void (GL_APIENTRY*)(GLuint program, GLint location, GLfloat *params))buscar("glGetUniformfv");
    if (!p_glGetUniformfv) faltan++;
    p_glGetUniformiv = (void (GL_APIENTRY*)(GLuint program, GLint location, GLint *params))buscar("glGetUniformiv");
    if (!p_glGetUniformiv) faltan++;
    p_glGetUniformLocation = (GLint (GL_APIENTRY*)(GLuint program, const GLchar *name))buscar("glGetUniformLocation");
    if (!p_glGetUniformLocation) faltan++;
    p_glGetVertexAttribfv = (void (GL_APIENTRY*)(GLuint index, GLenum pname, GLfloat *params))buscar("glGetVertexAttribfv");
    if (!p_glGetVertexAttribfv) faltan++;
    p_glGetVertexAttribiv = (void (GL_APIENTRY*)(GLuint index, GLenum pname, GLint *params))buscar("glGetVertexAttribiv");
    if (!p_glGetVertexAttribiv) faltan++;
    p_glGetVertexAttribPointerv = (void (GL_APIENTRY*)(GLuint index, GLenum pname, void **pointer))buscar("glGetVertexAttribPointerv");
    if (!p_glGetVertexAttribPointerv) faltan++;
    p_glHint = (void (GL_APIENTRY*)(GLenum target, GLenum mode))buscar("glHint");
    if (!p_glHint) faltan++;
    p_glIsBuffer = (GLboolean (GL_APIENTRY*)(GLuint buffer))buscar("glIsBuffer");
    if (!p_glIsBuffer) faltan++;
    p_glIsEnabled = (GLboolean (GL_APIENTRY*)(GLenum cap))buscar("glIsEnabled");
    if (!p_glIsEnabled) faltan++;
    p_glIsFramebuffer = (GLboolean (GL_APIENTRY*)(GLuint framebuffer))buscar("glIsFramebuffer");
    if (!p_glIsFramebuffer) faltan++;
    p_glIsProgram = (GLboolean (GL_APIENTRY*)(GLuint program))buscar("glIsProgram");
    if (!p_glIsProgram) faltan++;
    p_glIsRenderbuffer = (GLboolean (GL_APIENTRY*)(GLuint renderbuffer))buscar("glIsRenderbuffer");
    if (!p_glIsRenderbuffer) faltan++;
    p_glIsShader = (GLboolean (GL_APIENTRY*)(GLuint shader))buscar("glIsShader");
    if (!p_glIsShader) faltan++;
    p_glIsTexture = (GLboolean (GL_APIENTRY*)(GLuint texture))buscar("glIsTexture");
    if (!p_glIsTexture) faltan++;
    p_glLineWidth = (void (GL_APIENTRY*)(GLfloat width))buscar("glLineWidth");
    if (!p_glLineWidth) faltan++;
    p_glLinkProgram = (void (GL_APIENTRY*)(GLuint program))buscar("glLinkProgram");
    if (!p_glLinkProgram) faltan++;
    p_glPixelStorei = (void (GL_APIENTRY*)(GLenum pname, GLint param))buscar("glPixelStorei");
    if (!p_glPixelStorei) faltan++;
    p_glPolygonOffset = (void (GL_APIENTRY*)(GLfloat factor, GLfloat units))buscar("glPolygonOffset");
    if (!p_glPolygonOffset) faltan++;
    p_glReadPixels = (void (GL_APIENTRY*)(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void *pixels))buscar("glReadPixels");
    if (!p_glReadPixels) faltan++;
    p_glReleaseShaderCompiler = (void (GL_APIENTRY*)(void))buscar("glReleaseShaderCompiler");
    if (!p_glReleaseShaderCompiler) faltan++;
    p_glRenderbufferStorage = (void (GL_APIENTRY*)(GLenum target, GLenum internalformat, GLsizei width, GLsizei height))buscar("glRenderbufferStorage");
    if (!p_glRenderbufferStorage) faltan++;
    p_glSampleCoverage = (void (GL_APIENTRY*)(GLfloat value, GLboolean invert))buscar("glSampleCoverage");
    if (!p_glSampleCoverage) faltan++;
    p_glScissor = (void (GL_APIENTRY*)(GLint x, GLint y, GLsizei width, GLsizei height))buscar("glScissor");
    if (!p_glScissor) faltan++;
    p_glShaderBinary = (void (GL_APIENTRY*)(GLsizei count, const GLuint *shaders, GLenum binaryFormat, const void *binary, GLsizei length))buscar("glShaderBinary");
    if (!p_glShaderBinary) faltan++;
    p_glShaderSource = (void (GL_APIENTRY*)(GLuint shader, GLsizei count, const GLchar *const*string, const GLint *length))buscar("glShaderSource");
    if (!p_glShaderSource) faltan++;
    p_glStencilFunc = (void (GL_APIENTRY*)(GLenum func, GLint ref, GLuint mask))buscar("glStencilFunc");
    if (!p_glStencilFunc) faltan++;
    p_glStencilFuncSeparate = (void (GL_APIENTRY*)(GLenum face, GLenum func, GLint ref, GLuint mask))buscar("glStencilFuncSeparate");
    if (!p_glStencilFuncSeparate) faltan++;
    p_glStencilMask = (void (GL_APIENTRY*)(GLuint mask))buscar("glStencilMask");
    if (!p_glStencilMask) faltan++;
    p_glStencilMaskSeparate = (void (GL_APIENTRY*)(GLenum face, GLuint mask))buscar("glStencilMaskSeparate");
    if (!p_glStencilMaskSeparate) faltan++;
    p_glStencilOp = (void (GL_APIENTRY*)(GLenum fail, GLenum zfail, GLenum zpass))buscar("glStencilOp");
    if (!p_glStencilOp) faltan++;
    p_glStencilOpSeparate = (void (GL_APIENTRY*)(GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass))buscar("glStencilOpSeparate");
    if (!p_glStencilOpSeparate) faltan++;
    p_glTexImage2D = (void (GL_APIENTRY*)(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels))buscar("glTexImage2D");
    if (!p_glTexImage2D) faltan++;
    p_glTexParameterf = (void (GL_APIENTRY*)(GLenum target, GLenum pname, GLfloat param))buscar("glTexParameterf");
    if (!p_glTexParameterf) faltan++;
    p_glTexParameterfv = (void (GL_APIENTRY*)(GLenum target, GLenum pname, const GLfloat *params))buscar("glTexParameterfv");
    if (!p_glTexParameterfv) faltan++;
    p_glTexParameteri = (void (GL_APIENTRY*)(GLenum target, GLenum pname, GLint param))buscar("glTexParameteri");
    if (!p_glTexParameteri) faltan++;
    p_glTexParameteriv = (void (GL_APIENTRY*)(GLenum target, GLenum pname, const GLint *params))buscar("glTexParameteriv");
    if (!p_glTexParameteriv) faltan++;
    p_glTexSubImage2D = (void (GL_APIENTRY*)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels))buscar("glTexSubImage2D");
    if (!p_glTexSubImage2D) faltan++;
    p_glUniform1f = (void (GL_APIENTRY*)(GLint location, GLfloat v0))buscar("glUniform1f");
    if (!p_glUniform1f) faltan++;
    p_glUniform1fv = (void (GL_APIENTRY*)(GLint location, GLsizei count, const GLfloat *value))buscar("glUniform1fv");
    if (!p_glUniform1fv) faltan++;
    p_glUniform1i = (void (GL_APIENTRY*)(GLint location, GLint v0))buscar("glUniform1i");
    if (!p_glUniform1i) faltan++;
    p_glUniform1iv = (void (GL_APIENTRY*)(GLint location, GLsizei count, const GLint *value))buscar("glUniform1iv");
    if (!p_glUniform1iv) faltan++;
    p_glUniform2f = (void (GL_APIENTRY*)(GLint location, GLfloat v0, GLfloat v1))buscar("glUniform2f");
    if (!p_glUniform2f) faltan++;
    p_glUniform2fv = (void (GL_APIENTRY*)(GLint location, GLsizei count, const GLfloat *value))buscar("glUniform2fv");
    if (!p_glUniform2fv) faltan++;
    p_glUniform2i = (void (GL_APIENTRY*)(GLint location, GLint v0, GLint v1))buscar("glUniform2i");
    if (!p_glUniform2i) faltan++;
    p_glUniform2iv = (void (GL_APIENTRY*)(GLint location, GLsizei count, const GLint *value))buscar("glUniform2iv");
    if (!p_glUniform2iv) faltan++;
    p_glUniform3f = (void (GL_APIENTRY*)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2))buscar("glUniform3f");
    if (!p_glUniform3f) faltan++;
    p_glUniform3fv = (void (GL_APIENTRY*)(GLint location, GLsizei count, const GLfloat *value))buscar("glUniform3fv");
    if (!p_glUniform3fv) faltan++;
    p_glUniform3i = (void (GL_APIENTRY*)(GLint location, GLint v0, GLint v1, GLint v2))buscar("glUniform3i");
    if (!p_glUniform3i) faltan++;
    p_glUniform3iv = (void (GL_APIENTRY*)(GLint location, GLsizei count, const GLint *value))buscar("glUniform3iv");
    if (!p_glUniform3iv) faltan++;
    p_glUniform4f = (void (GL_APIENTRY*)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3))buscar("glUniform4f");
    if (!p_glUniform4f) faltan++;
    p_glUniform4fv = (void (GL_APIENTRY*)(GLint location, GLsizei count, const GLfloat *value))buscar("glUniform4fv");
    if (!p_glUniform4fv) faltan++;
    p_glUniform4i = (void (GL_APIENTRY*)(GLint location, GLint v0, GLint v1, GLint v2, GLint v3))buscar("glUniform4i");
    if (!p_glUniform4i) faltan++;
    p_glUniform4iv = (void (GL_APIENTRY*)(GLint location, GLsizei count, const GLint *value))buscar("glUniform4iv");
    if (!p_glUniform4iv) faltan++;
    p_glUniformMatrix2fv = (void (GL_APIENTRY*)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value))buscar("glUniformMatrix2fv");
    if (!p_glUniformMatrix2fv) faltan++;
    p_glUniformMatrix3fv = (void (GL_APIENTRY*)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value))buscar("glUniformMatrix3fv");
    if (!p_glUniformMatrix3fv) faltan++;
    p_glUniformMatrix4fv = (void (GL_APIENTRY*)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value))buscar("glUniformMatrix4fv");
    if (!p_glUniformMatrix4fv) faltan++;
    p_glUseProgram = (void (GL_APIENTRY*)(GLuint program))buscar("glUseProgram");
    if (!p_glUseProgram) faltan++;
    p_glValidateProgram = (void (GL_APIENTRY*)(GLuint program))buscar("glValidateProgram");
    if (!p_glValidateProgram) faltan++;
    p_glVertexAttrib1f = (void (GL_APIENTRY*)(GLuint index, GLfloat x))buscar("glVertexAttrib1f");
    if (!p_glVertexAttrib1f) faltan++;
    p_glVertexAttrib1fv = (void (GL_APIENTRY*)(GLuint index, const GLfloat *v))buscar("glVertexAttrib1fv");
    if (!p_glVertexAttrib1fv) faltan++;
    p_glVertexAttrib2f = (void (GL_APIENTRY*)(GLuint index, GLfloat x, GLfloat y))buscar("glVertexAttrib2f");
    if (!p_glVertexAttrib2f) faltan++;
    p_glVertexAttrib2fv = (void (GL_APIENTRY*)(GLuint index, const GLfloat *v))buscar("glVertexAttrib2fv");
    if (!p_glVertexAttrib2fv) faltan++;
    p_glVertexAttrib3f = (void (GL_APIENTRY*)(GLuint index, GLfloat x, GLfloat y, GLfloat z))buscar("glVertexAttrib3f");
    if (!p_glVertexAttrib3f) faltan++;
    p_glVertexAttrib3fv = (void (GL_APIENTRY*)(GLuint index, const GLfloat *v))buscar("glVertexAttrib3fv");
    if (!p_glVertexAttrib3fv) faltan++;
    p_glVertexAttrib4f = (void (GL_APIENTRY*)(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w))buscar("glVertexAttrib4f");
    if (!p_glVertexAttrib4f) faltan++;
    p_glVertexAttrib4fv = (void (GL_APIENTRY*)(GLuint index, const GLfloat *v))buscar("glVertexAttrib4fv");
    if (!p_glVertexAttrib4fv) faltan++;
    p_glVertexAttribPointer = (void (GL_APIENTRY*)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer))buscar("glVertexAttribPointer");
    if (!p_glVertexAttribPointer) faltan++;
    p_glViewport = (void (GL_APIENTRY*)(GLint x, GLint y, GLsizei width, GLsizei height))buscar("glViewport");
    if (!p_glViewport) faltan++;
    p_eglChooseConfig = (EGLBoolean (EGLAPIENTRY*)(EGLDisplay dpy, const EGLint *attrib_list, EGLConfig *configs, EGLint config_size, EGLint *num_config))buscar("eglChooseConfig");
    if (!p_eglChooseConfig) faltan++;
    p_eglCopyBuffers = (EGLBoolean (EGLAPIENTRY*)(EGLDisplay dpy, EGLSurface surface, EGLNativePixmapType target))buscar("eglCopyBuffers");
    if (!p_eglCopyBuffers) faltan++;
    p_eglCreateContext = (EGLContext (EGLAPIENTRY*)(EGLDisplay dpy, EGLConfig config, EGLContext share_context, const EGLint *attrib_list))buscar("eglCreateContext");
    if (!p_eglCreateContext) faltan++;
    p_eglCreatePbufferSurface = (EGLSurface (EGLAPIENTRY*)(EGLDisplay dpy, EGLConfig config, const EGLint *attrib_list))buscar("eglCreatePbufferSurface");
    if (!p_eglCreatePbufferSurface) faltan++;
    p_eglCreatePixmapSurface = (EGLSurface (EGLAPIENTRY*)(EGLDisplay dpy, EGLConfig config, EGLNativePixmapType pixmap, const EGLint *attrib_list))buscar("eglCreatePixmapSurface");
    if (!p_eglCreatePixmapSurface) faltan++;
    p_eglCreateWindowSurface = (EGLSurface (EGLAPIENTRY*)(EGLDisplay dpy, EGLConfig config, EGLNativeWindowType win, const EGLint *attrib_list))buscar("eglCreateWindowSurface");
    if (!p_eglCreateWindowSurface) faltan++;
    p_eglDestroyContext = (EGLBoolean (EGLAPIENTRY*)(EGLDisplay dpy, EGLContext ctx))buscar("eglDestroyContext");
    if (!p_eglDestroyContext) faltan++;
    p_eglDestroySurface = (EGLBoolean (EGLAPIENTRY*)(EGLDisplay dpy, EGLSurface surface))buscar("eglDestroySurface");
    if (!p_eglDestroySurface) faltan++;
    p_eglGetConfigAttrib = (EGLBoolean (EGLAPIENTRY*)(EGLDisplay dpy, EGLConfig config, EGLint attribute, EGLint *value))buscar("eglGetConfigAttrib");
    if (!p_eglGetConfigAttrib) faltan++;
    p_eglGetConfigs = (EGLBoolean (EGLAPIENTRY*)(EGLDisplay dpy, EGLConfig *configs, EGLint config_size, EGLint *num_config))buscar("eglGetConfigs");
    if (!p_eglGetConfigs) faltan++;
    p_eglGetCurrentDisplay = (EGLDisplay (EGLAPIENTRY*)(void))buscar("eglGetCurrentDisplay");
    if (!p_eglGetCurrentDisplay) faltan++;
    p_eglGetCurrentSurface = (EGLSurface (EGLAPIENTRY*)(EGLint readdraw))buscar("eglGetCurrentSurface");
    if (!p_eglGetCurrentSurface) faltan++;
    p_eglGetDisplay = (EGLDisplay (EGLAPIENTRY*)(EGLNativeDisplayType display_id))buscar("eglGetDisplay");
    if (!p_eglGetDisplay) faltan++;
    p_eglGetError = (EGLint (EGLAPIENTRY*)(void))buscar("eglGetError");
    if (!p_eglGetError) faltan++;
    p_eglGetProcAddress = (__eglMustCastToProperFunctionPointerType (EGLAPIENTRY*)(const char *procname))buscar("eglGetProcAddress");
    if (!p_eglGetProcAddress) faltan++;
    p_eglInitialize = (EGLBoolean (EGLAPIENTRY*)(EGLDisplay dpy, EGLint *major, EGLint *minor))buscar("eglInitialize");
    if (!p_eglInitialize) faltan++;
    p_eglMakeCurrent = (EGLBoolean (EGLAPIENTRY*)(EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx))buscar("eglMakeCurrent");
    if (!p_eglMakeCurrent) faltan++;
    p_eglQueryContext = (EGLBoolean (EGLAPIENTRY*)(EGLDisplay dpy, EGLContext ctx, EGLint attribute, EGLint *value))buscar("eglQueryContext");
    if (!p_eglQueryContext) faltan++;
    p_eglQueryString = (const char * (EGLAPIENTRY*)(EGLDisplay dpy, EGLint name))buscar("eglQueryString");
    if (!p_eglQueryString) faltan++;
    p_eglQuerySurface = (EGLBoolean (EGLAPIENTRY*)(EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint *value))buscar("eglQuerySurface");
    if (!p_eglQuerySurface) faltan++;
    p_eglSwapBuffers = (EGLBoolean (EGLAPIENTRY*)(EGLDisplay dpy, EGLSurface surface))buscar("eglSwapBuffers");
    if (!p_eglSwapBuffers) faltan++;
    p_eglTerminate = (EGLBoolean (EGLAPIENTRY*)(EGLDisplay dpy))buscar("eglTerminate");
    if (!p_eglTerminate) faltan++;
    p_eglWaitGL = (EGLBoolean (EGLAPIENTRY*)(void))buscar("eglWaitGL");
    if (!p_eglWaitGL) faltan++;
    p_eglWaitNative = (EGLBoolean (EGLAPIENTRY*)(EGLint engine))buscar("eglWaitNative");
    if (!p_eglWaitNative) faltan++;
    p_eglBindTexImage = (EGLBoolean (EGLAPIENTRY*)(EGLDisplay dpy, EGLSurface surface, EGLint buffer))buscar("eglBindTexImage");
    if (!p_eglBindTexImage) faltan++;
    p_eglReleaseTexImage = (EGLBoolean (EGLAPIENTRY*)(EGLDisplay dpy, EGLSurface surface, EGLint buffer))buscar("eglReleaseTexImage");
    if (!p_eglReleaseTexImage) faltan++;
    p_eglSurfaceAttrib = (EGLBoolean (EGLAPIENTRY*)(EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint value))buscar("eglSurfaceAttrib");
    if (!p_eglSurfaceAttrib) faltan++;
    p_eglSwapInterval = (EGLBoolean (EGLAPIENTRY*)(EGLDisplay dpy, EGLint interval))buscar("eglSwapInterval");
    if (!p_eglSwapInterval) faltan++;
    p_eglBindAPI = (EGLBoolean (EGLAPIENTRY*)(EGLenum api))buscar("eglBindAPI");
    if (!p_eglBindAPI) faltan++;
    p_eglQueryAPI = (EGLenum (EGLAPIENTRY*)(void))buscar("eglQueryAPI");
    if (!p_eglQueryAPI) faltan++;
    p_eglCreatePbufferFromClientBuffer = (EGLSurface (EGLAPIENTRY*)(EGLDisplay dpy, EGLenum buftype, EGLClientBuffer buffer, EGLConfig config, const EGLint *attrib_list))buscar("eglCreatePbufferFromClientBuffer");
    if (!p_eglCreatePbufferFromClientBuffer) faltan++;
    p_eglReleaseThread = (EGLBoolean (EGLAPIENTRY*)(void))buscar("eglReleaseThread");
    if (!p_eglReleaseThread) faltan++;
    p_eglWaitClient = (EGLBoolean (EGLAPIENTRY*)(void))buscar("eglWaitClient");
    if (!p_eglWaitClient) faltan++;
    p_eglGetCurrentContext = (EGLContext (EGLAPIENTRY*)(void))buscar("eglGetCurrentContext");
    if (!p_eglGetCurrentContext) faltan++;
    p_eglCreateSync = (EGLSync (EGLAPIENTRY*)(EGLDisplay dpy, EGLenum type, const EGLAttrib *attrib_list))buscar("eglCreateSync");
    if (!p_eglCreateSync) faltan++;
    p_eglDestroySync = (EGLBoolean (EGLAPIENTRY*)(EGLDisplay dpy, EGLSync sync))buscar("eglDestroySync");
    if (!p_eglDestroySync) faltan++;
    p_eglClientWaitSync = (EGLint (EGLAPIENTRY*)(EGLDisplay dpy, EGLSync sync, EGLint flags, EGLTime timeout))buscar("eglClientWaitSync");
    if (!p_eglClientWaitSync) faltan++;
    p_eglGetSyncAttrib = (EGLBoolean (EGLAPIENTRY*)(EGLDisplay dpy, EGLSync sync, EGLint attribute, EGLAttrib *value))buscar("eglGetSyncAttrib");
    if (!p_eglGetSyncAttrib) faltan++;
    p_eglCreateImage = (EGLImage (EGLAPIENTRY*)(EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLAttrib *attrib_list))buscar("eglCreateImage");
    if (!p_eglCreateImage) faltan++;
    p_eglDestroyImage = (EGLBoolean (EGLAPIENTRY*)(EGLDisplay dpy, EGLImage image))buscar("eglDestroyImage");
    if (!p_eglDestroyImage) faltan++;
    p_eglGetPlatformDisplay = (EGLDisplay (EGLAPIENTRY*)(EGLenum platform, void *native_display, const EGLAttrib *attrib_list))buscar("eglGetPlatformDisplay");
    if (!p_eglGetPlatformDisplay) faltan++;
    p_eglCreatePlatformWindowSurface = (EGLSurface (EGLAPIENTRY*)(EGLDisplay dpy, EGLConfig config, void *native_window, const EGLAttrib *attrib_list))buscar("eglCreatePlatformWindowSurface");
    if (!p_eglCreatePlatformWindowSurface) faltan++;
    p_eglCreatePlatformPixmapSurface = (EGLSurface (EGLAPIENTRY*)(EGLDisplay dpy, EGLConfig config, void *native_pixmap, const EGLAttrib *attrib_list))buscar("eglCreatePlatformPixmapSurface");
    if (!p_eglCreatePlatformPixmapSurface) faltan++;
    p_eglWaitSync = (EGLBoolean (EGLAPIENTRY*)(EGLDisplay dpy, EGLSync sync, EGLint flags))buscar("eglWaitSync");
    if (!p_eglWaitSync) faltan++;
    return faltan;
}
#else
int GlCargar(void* (*)(const char*)) { return 0; }   // las de Piglet ya están conectadas por el sistema
#endif
