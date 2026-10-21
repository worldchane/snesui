// Ver gpu.h.
#include "gpu.h"

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gl_funciones.h"
#include "plataforma.h"

void Registrar(const char* formato, ...);

#ifndef GL_DEPTH24_STENCIL8_OES
#define GL_DEPTH24_STENCIL8_OES 0x88F0
#endif

static bool cargadas;
static EGLDisplay pantalla = EGL_NO_DISPLAY;
static EGLContext contexto = EGL_NO_CONTEXT;
static EGLSurface superficie = EGL_NO_SURFACE;
static GLuint fbo, textura, profundidad;
static int fw, fh;
static bool es3;           // OpenGL ES 3 (tiene buffers de lectura, PBO)
static uint8_t* lectura;   // la imagen leída de la GPU (RGBA)

static bool PrepararEgl(char* error, int max)
{
    if (contexto != EGL_NO_CONTEXT) return true;
    if (!cargadas)
    {
        if (!GpuCargarModulos(error, max)) return false;
        int faltan = GlCargar(GpuBuscar);
        if (faltan) Registrar("GPU: faltan %d funciones de OpenGL ES / EGL", faltan);
        cargadas = true;
    }
    // La primera vez eglGetDisplay puede fallar (en la consola): se reintenta.
    for (int i = 0; i < 10 && pantalla == EGL_NO_DISPLAY; i++)
    {
        pantalla = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (pantalla == EGL_NO_DISPLAY) Dormir(200);
    }
    if (pantalla == EGL_NO_DISPLAY) { snprintf(error, max, "eglGetDisplay falló."); return false; }
    EGLint ma = 0, mi = 0;
    if (!eglInitialize(pantalla, &ma, &mi)) { snprintf(error, max, "eglInitialize falló (0x%X).", eglGetError()); return false; }
    eglBindAPI(EGL_OPENGL_ES_API);
    const EGLint atrib[] = {EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_DEPTH_SIZE, 16,
                            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_NONE};
    EGLConfig config;
    EGLint n = 0;
    if (!eglChooseConfig(pantalla, atrib, &config, 1, &n) || n < 1) { snprintf(error, max, "no hay configuración EGL con pbuffer."); return false; }
    // Una superficie chica: se dibuja en el framebuffer propio, no en ella.
    const EGLint atribPb[] = {EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE};
    superficie = eglCreatePbufferSurface(pantalla, config, atribPb);
    if (superficie == EGL_NO_SURFACE) { snprintf(error, max, "eglCreatePbufferSurface falló (0x%X).", eglGetError()); return false; }
    const EGLint atribCtx[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
    contexto = eglCreateContext(pantalla, config, EGL_NO_CONTEXT, atribCtx);
    if (contexto == EGL_NO_CONTEXT) { snprintf(error, max, "eglCreateContext falló (0x%X).", eglGetError()); return false; }
    Registrar("GPU: EGL %d.%d", ma, mi);
    return true;
}

bool GpuActivar()
{
    bool ok = contexto != EGL_NO_CONTEXT && eglMakeCurrent(pantalla, superficie, superficie, contexto);
    if (ok && fbo && getenv("GPU_ROJO"))   // prueba: ¿el núcleo dibuja en el framebuffer? (si no, queda rojo)
    {
        GLint antes = 0;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &antes);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glClearColor(1, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)antes);
    }
    return ok;
}
void GpuSoltar() { if (contexto != EGL_NO_CONTEXT) eglMakeCurrent(pantalla, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT); }

bool GpuAbrir(int w, int h, char* error, int max)
{
    if (!PrepararEgl(error, max)) return false;
    if (!GpuActivar()) { snprintf(error, max, "eglMakeCurrent falló (0x%X).", eglGetError()); return false; }
    if (fbo && (fw != w || fh != h)) GpuCerrar();
    if (!fbo)
    {
        glGenTextures(1, &textura);
        glBindTexture(GL_TEXTURE_2D, textura);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glGenRenderbuffers(1, &profundidad);
        glBindRenderbuffer(GL_RENDERBUFFER, profundidad);
        const char* ext = (const char*)glGetString(GL_EXTENSIONS);
        bool juntos = ext && strstr(ext, "GL_OES_packed_depth_stencil");
        glRenderbufferStorage(GL_RENDERBUFFER, juntos ? GL_DEPTH24_STENCIL8_OES : GL_DEPTH_COMPONENT16, w, h);
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textura, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, profundidad);
        if (juntos) glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, profundidad);
        GLenum st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (st != GL_FRAMEBUFFER_COMPLETE) { snprintf(error, max, "el framebuffer no quedó completo (0x%X).", st); GpuCerrar(); return false; }
        fw = w;
        fh = h;
        free(lectura);
        lectura = (uint8_t*)malloc((size_t)w * h * 4);
        const char* ver = (const char*)glGetString(GL_VERSION);
        es3 = ver && strstr(ver, "OpenGL ES 3");
        Registrar("GPU: %s, framebuffer %d x %d%s", ver ? ver : "?", w, h, juntos ? " con stencil" : "");
    }
    return true;
}

unsigned GpuFramebuffer() { return fbo; }
void GpuAnotarError(const char* momento) { GLenum e = glGetError(); if (e) Registrar("GPU: error de OpenGL 0x%X %s", e, momento); }
void* GpuFuncion(const char* nombre) { return GpuBuscar(nombre); }

bool GpuLeer(uint32_t* sale, int w, int h)
{
    if (!fbo || !lectura || w > fw || h > fh) return false;
    // GLideN64 usa buffers de lectura (PBO) para copiar imágenes a la memoria de la N64: si dejó uno conectado,
    // glReadPixels escribiría ahí. Se desconecta solo para leer y después todo queda como estaba (guarda lo que conectó).
    GLenum antes = glGetError();   // un error que dejó el núcleo (para el registro de abajo)
    GLint fboAntes = 0, pboAntes = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fboAntes);
    if (es3) { glGetIntegerv(0x88ED /* GL_PIXEL_PACK_BUFFER_BINDING */, &pboAntes); if (pboAntes) glBindBuffer(0x88EB /* GL_PIXEL_PACK_BUFFER */, 0); }
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, lectura);
    if (pboAntes) glBindBuffer(0x88EB, (GLuint)pboAntes);
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)fboAntes);
    static int vistas;
    if (vistas < 400 && (++vistas % 50) == 1)
    {
        const uint8_t* c = lectura + ((size_t)(h / 2) * w + w / 2) * 4;
        Registrar("GPU: cuadro %d de %d x %d, error del núcleo 0x%X, al leer 0x%X, centro %02x%02x%02x", vistas, w, h, antes,
                  glGetError(), c[0], c[1], c[2]);
    }
    for (int y = 0; y < h; y++)
    {
        const uint8_t* f = lectura + (size_t)(h - 1 - y) * w * 4;   // OpenGL guarda de abajo hacia arriba
        uint32_t* s = sale + (size_t)y * w;
        for (int x = 0; x < w; x++, f += 4) s[x] = (uint32_t)f[0] << 16 | (uint32_t)f[1] << 8 | f[2];
    }
    return true;
}

void GpuCerrar()
{
    if (fbo) glDeleteFramebuffers(1, &fbo);
    if (profundidad) glDeleteRenderbuffers(1, &profundidad);
    if (textura) glDeleteTextures(1, &textura);
    fbo = profundidad = textura = 0;
    fw = fh = 0;
}
