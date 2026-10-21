// Prueba de GPU para el N64 (solo con -DPRUEBA_JIT): GLideN64 dibuja con OpenGL ES y arma sus shaders mientras corre, así
// que hace falta que Piglet (el OpenGL ES de la PS4) compile shaders desde texto. Pasos, cada uno anotado antes por si
// cierra el programa:
//   1) cargar Piglet (y el compilador de shaders del sistema, si está) y buscar sus funciones;
//   2) configurarlo y abrir EGL;
//   3) un contexto con una superficie fuera de pantalla (la app dibuja la pantalla; el N64 dibujaría aparte);
//   4) compilar un shader de vértices y uno de píxeles desde texto;
//   5) dibujar y leer un píxel (que la GPU de verdad dibujó).
// Resultado en el registro y en la biblioteca, como la prueba de memoria ejecutable.
#include "prueba_jit.h"

#ifdef PRUEBA_JIT
#include <stdio.h>
#include <string.h>

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <orbis/Pigletv2VSH.h>
#include <orbis/libkernel.h>

#include "../comun/gl_funciones.h"
#include "../comun/pantalla.h"
#include "../comun/plataforma.h"
#include "registro.h"

#ifdef PIGLET_IMPORTADO
extern "C" bool scePigletSetConfigurationVSH(const OrbisPglConfig* config);
#endif

namespace {

int modPiglet = -1, modShacc = -1;
char resultado[300];

int Cargar(const char* nombre, char* det, int max, bool sistemaPrimero = false)
{
    // La carpeta de librerías del sistema vista desde la app: su nombre (al azar) se pide por nombre a libkernel, así el
    // programa no suma funciones a las que pide para abrir.
    const char* (*palabra)() = NULL;
    if (sceKernelDlsym(0x2001, "sceKernelGetFsSandboxRandomWord", (void**)&palabra) != 0) palabra = NULL;
    char ruta[256];
    int ret = 0, h = -1;
    // Las consolas de venta no traen el compilador de shaders (libSceShaccVSH): va dentro del paquete (pruebas, sacado
    // del de RetroArch) o en /data/allesiu/gpu/, donde cada uno pone los suyos. Después, la carpeta del sistema.
    if (!sistemaPrimero)
    {
        snprintf(ruta, sizeof ruta, "/app0/sce_module/%s", nombre);
        h = (int)sceKernelLoadStartModule(ruta, 0, NULL, 0, NULL, &ret);
        if (h < 0) { snprintf(ruta, sizeof ruta, "%s/gpu/%s", CARPETA_DATOS, nombre); h = (int)sceKernelLoadStartModule(ruta, 0, NULL, 0, NULL, &ret); }
    }
    if (h < 0 && palabra && palabra())
    {
        snprintf(ruta, sizeof ruta, "/%s/common/lib/%s", palabra(), nombre);
        h = (int)sceKernelLoadStartModule(ruta, 0, NULL, 0, NULL, &ret);
    }
    if (h < 0) { snprintf(ruta, sizeof ruta, "/system/common/lib/%s", nombre); h = (int)sceKernelLoadStartModule(ruta, 0, NULL, 0, NULL, &ret); }
    if (h < 0 && sistemaPrimero) return Cargar(nombre, det, max, false);   // no está en el sistema: el del paquete
    if (h >= 0) snprintf(det, max, "%s cargado desde %s", nombre, ruta);
    else snprintf(det, max, "%s no cargó (0x%08X)", nombre, (unsigned)h);
    return h;
}

int faltanAnotadas;
void* BuscarEnPiglet(const char* nombre)
{
    void* f = NULL;
    if (sceKernelDlsym(modPiglet, nombre, &f) != 0) f = NULL;
    if (!f && faltanAnotadas++ < 16) Registrar("Prueba GPU 1: falta %s", nombre);   // cuáles (no solo cuántas)
    return f;
}

const char* SH_V = "attribute vec2 pos;\nvoid main() { gl_Position = vec4(pos, 0.0, 1.0); }\n";
const char* SH_F = "precision mediump float;\nuniform vec4 color;\nvoid main() { gl_FragColor = color; }\n";

GLuint Compilar(GLenum tipo, const char* fuente, char* log, int max)
{
    GLuint s = glCreateShader(tipo);
    if (!s) { snprintf(log, max, "glCreateShader dio 0 (error 0x%X)", glGetError()); return 0; }
    glShaderSource(s, 1, &fuente, NULL);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok)
    {
        GLsizei n = 0;
        glGetShaderInfoLog(s, max - 1, &n, log);
        log[n > 0 ? n : 0] = 0;
        if (!log[0]) snprintf(log, max, "no compiló (sin mensaje; error 0x%X)", glGetError());
        return 0;
    }
    return s;
}

}   // namespace

const char* PruebaGpu()
{
    if (resultado[0]) return resultado;
    char ruta[160], antes[320] = "", det[200] = "";
#ifdef SOLO_PRUEBA_GPU
#ifdef MARCA_GPU_FIRMA
    snprintf(ruta, sizeof ruta, "%s/prueba_gpu_minima8.txt", CARPETA_DIAGNOSTICO);   // la de la firma de Piglet (prueba55)
#elif defined(PIGLET_SISTEMA)
    snprintf(ruta, sizeof ruta, "%s/prueba_gpu_minima9.txt", CARPETA_DIAGNOSTICO);   // la receta de ioQuake3 (prueba57)
#else
    snprintf(ruta, sizeof ruta, "%s/prueba_gpu_minima6.txt", CARPETA_DIAGNOSTICO);
#endif   // la variante mínima, con sus intentos
#else
    snprintf(ruta, sizeof ruta, "%s/prueba_gpu9.txt", CARPETA_DIAGNOSTICO);
#endif
    LeerTexto(ruta, antes, sizeof antes);
    // Se hace una sola vez: Piglet toma la salida de video y la app queda en negro. Las veces siguientes, el resultado
    // guardado ("terminada" y la línea del resultado).
    if (!strncmp(antes, "terminada", 9))
    {
        const char* r = strchr(antes, '\n');
        snprintf(resultado, sizeof resultado, "%s", r && r[1] ? r + 1 : "GPU: ya probada (ver el registro)");
        char* fin = strchr(resultado, '\n');
        if (fin) *fin = 0;
        return resultado;
    }
    // Intentos: "intento N" (los N anteriores fallaron) o "paso P intento N" (el intento N se cortó en el paso P: se colgó
    // o se apagó la consola). En la prueba de PCSX2 la primera vez falló y la siguiente funcionó: se reintenta al abrir.
    int intento = 0, pasoCortado = 0;
    if (!strncmp(antes, "intento", 7)) sscanf(antes, "intento %d", &intento);
    if (!strncmp(antes, "paso", 4) && sscanf(antes, "paso %d intento %d", &pasoCortado, &intento) == 2)
    {
        Registrar("Prueba GPU: el intento %d se cortó en el paso %d (cerró, se colgó o se apagó)", intento + 1, pasoCortado);
        intento++;
    }
    const int MAX_INTENTOS = 4;
    if (intento >= MAX_INTENTOS)
    {
        snprintf(resultado, sizeof resultado, "GPU: no funcionó en %d intentos (ver el registro).", MAX_INTENTOS);
        char t[340];
        snprintf(t, sizeof t, "terminada\n%s\n", resultado);
        EscribirTexto(ruta, t);
        Registrar("%s", resultado);
        return resultado;
    }
    bool pigletSistema = intento % 2 == 1;   // 0 y 2: el de RetroArch (paquete); 1 y 3: el del sistema
#ifdef PIGLET_IMPORTADO
    pigletSistema = false;   // importado: el sistema ya cargó el del paquete al crear el proceso
#endif
#ifdef PIGLET_SISTEMA
    pigletSistema = true;    // sin módulos en el paquete: el del firmware (como ioQuake3-PS4)
#endif
    Registrar("Prueba GPU: intento %d de %d, Piglet %s", intento + 1, MAX_INTENTOS, pigletSistema ? "del sistema" : "del paquete");
    auto Paso = [&](int n) { char t[48]; snprintf(t, sizeof t, "paso %d intento %d\n", n, intento); EscribirTexto(ruta, t); };
    EGLDisplay pantalla = EGL_NO_DISPLAY;
    auto Fin = [&](const char* t) {
        // EGL se suelta siempre: la prueba corre antes de abrir la pantalla de la app (main.cpp), que la necesita.
        if (pantalla != EGL_NO_DISPLAY) { eglMakeCurrent(pantalla, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT); eglTerminate(pantalla); }
        CerrarAccesoCompleto();
        snprintf(resultado, sizeof resultado, "GPU: %s", t);
        char g[340];
        snprintf(g, sizeof g, "terminada\n%s\n", resultado);
        EscribirTexto(ruta, g);
        Registrar("%s", resultado);
        return (const char*)resultado;
    };

    // 1) Piglet y el compilador de shaders.
    Paso(1);
    modPiglet = Cargar("libScePigletv2VSH.sprx", det, sizeof det, pigletSistema);
    Registrar("Prueba GPU 1: %s", det);
    if (modPiglet < 0) return Fin("Piglet no cargó.");
    modShacc = Cargar("libSceShaccVSH.sprx", det, sizeof det);
    Registrar("Prueba GPU 1: %s", det);
    // Los shaders precompilados del sistema: la prueba de PCSX2 (que funcionó) los cargaba antes de abrir EGL, y la
    // pantalla de Piglet ("composite") dibuja con ellos. Sin ellos, eglGetDisplay fallaba sin dar error.
    int modPrecompilados = Cargar("libScePrecompiledShaders.sprx", det, sizeof det, true);
    (void)modPrecompilados;
    Registrar("Prueba GPU 1: %s", det);
    int faltan = GlCargar(BuscarEnPiglet);
    Registrar("Prueba GPU 1: faltan %d funciones de OpenGL ES / EGL", faltan);
    // Con el acceso completo de GoldHEN (eleva los permisos): en la prueba de PCSX2 la variante que lo pedía (C) abrió
    // EGL; sin él, eglGetDisplay falla sin dar error. Los módulos ya están cargados (con el acceso, /app0 puede no verse).
    QuitarPantallaDeCarga();   // como el ejemplo oficial de Piglet: con la pantalla de carga puesta, no hay pantalla para EGL
    int acceso = AbrirAccesoCompleto("gpu", true);
    Registrar("Prueba GPU 1: acceso completo de GoldHEN: %d", acceso);
    bool (*configurar)(const OrbisPglConfig*) = NULL;
#ifdef PIGLET_IMPORTADO
    // Llamada directa: Piglet pasa a ser una biblioteca que el sistema carga al crear el proceso (como en RetroArch y
    // en el ejemplo oficial), no algo que se carga después a mano.
    configurar = scePigletSetConfigurationVSH;
#else
    sceKernelDlsym(modPiglet, "scePigletSetConfigurationVSH", (void**)&configurar);
#endif
    if (!configurar) return Fin("falta scePigletSetConfigurationVSH.");

    // 2) Configurar y abrir EGL (la primera vez puede fallar: se reintenta).
    Paso(2);
    OrbisPglConfig cfg;
    memset(&cfg, 0, sizeof cfg);
    cfg.size = sizeof cfg;
    cfg.flags = ORBIS_PGL_FLAGS_USE_COMPOSITE_EXT | ORBIS_PGL_FLAGS_USE_FLEXIBLE_MEMORY | 0x60;
    cfg.processOrder = 1;
    cfg.systemSharedMemorySize = 250 * 1024 * 1024;   // las de la prueba de PCSX2 (funcionó en esta consola)
    cfg.videoSharedMemorySize = 512 * 1024 * 1024;
    cfg.maxMappedFlexibleMemory = 170 * 1024 * 1024;
    cfg.drawCommandBufferSize = 1 * 1024 * 1024;
    cfg.lcueResourceBufferSize = 1 * 1024 * 1024;
    cfg.dbgPosCmd_0x40 = 640;
    cfg.dbgPosCmd_0x44 = 480;
    cfg.unk_0x5C = 2;
    if (!configurar(&cfg)) return Fin("scePigletSetConfigurationVSH falló.");
    for (int i = 0; i < 10 && pantalla == EGL_NO_DISPLAY; i++) { pantalla = eglGetDisplay(EGL_DEFAULT_DISPLAY); if (pantalla == EGL_NO_DISPLAY) Dormir(200); }
    if (pantalla == EGL_NO_DISPLAY)
    {
        snprintf(resultado, sizeof resultado, "GPU: eglGetDisplay falló (0x%X) en el intento %d; se repite al abrir la app.", eglGetError(),
                 intento + 1);
        Registrar("%s", resultado);
        CerrarAccesoCompleto();
        char t[48];
        snprintf(t, sizeof t, "intento %d\n", intento + 1);
        EscribirTexto(ruta, t);
        return resultado;
    }
    EGLint ma = 0, mi = 0;
    if (!eglInitialize(pantalla, &ma, &mi)) { snprintf(det, sizeof det, "eglInitialize falló (0x%X).", eglGetError()); return Fin(det); }
    Registrar("Prueba GPU 2: EGL %d.%d", ma, mi);

    // 3) Contexto con superficie fuera de pantalla.
    Paso(3);
    eglBindAPI(EGL_OPENGL_ES_API);
    const EGLint atrib[] = {EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_RENDERABLE_TYPE,
                            EGL_OPENGL_ES2_BIT, EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_NONE};
    EGLConfig config;
    EGLint n = 0;
    if (!eglChooseConfig(pantalla, atrib, &config, 1, &n) || n < 1) return Fin("no hay configuración con superficie fuera de pantalla (pbuffer).");
    const EGLint atribPb[] = {EGL_WIDTH, 64, EGL_HEIGHT, 64, EGL_NONE};
    EGLSurface sup = eglCreatePbufferSurface(pantalla, config, atribPb);
    if (sup == EGL_NO_SURFACE) { snprintf(det, sizeof det, "eglCreatePbufferSurface falló (0x%X).", eglGetError()); return Fin(det); }
    const EGLint atribCtx[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
    EGLContext ctx = eglCreateContext(pantalla, config, EGL_NO_CONTEXT, atribCtx);
    if (ctx == EGL_NO_CONTEXT || !eglMakeCurrent(pantalla, sup, sup, ctx)) { snprintf(det, sizeof det, "no se pudo crear el contexto (0x%X).", eglGetError()); return Fin(det); }
    const char* ver = (const char*)glGetString(GL_VERSION);
    const char* sl = (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION);
    Registrar("Prueba GPU 3: %s · GLSL %s", ver ? ver : "?", sl ? sl : "?");

    // 4) Shaders desde texto (lo que necesita GLideN64).
    Paso(4);
    char log[160] = "";
    GLuint v = Compilar(GL_VERTEX_SHADER, SH_V, log, sizeof log);
    if (!v) { Registrar("Prueba GPU 4: vértices: %s", log); snprintf(det, sizeof det, "no compila shaders desde texto (%s).", log); return Fin(det); }
    GLuint f = Compilar(GL_FRAGMENT_SHADER, SH_F, log, sizeof log);
    if (!f) { Registrar("Prueba GPU 4: píxeles: %s", log); snprintf(det, sizeof det, "no compila shaders de píxeles (%s).", log); return Fin(det); }
    GLuint prog = glCreateProgram();
    glAttachShader(prog, v);
    glAttachShader(prog, f);
    glBindAttribLocation(prog, 0, "pos");
    glLinkProgram(prog);
    GLint enlazado = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &enlazado);
    if (!enlazado) return Fin("compila los shaders pero no los enlaza.");

    // 5) Dibujar un triángulo de color y leer un píxel.
    Paso(5);
    glViewport(0, 0, 64, 64);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(prog);
    glUniform4f(glGetUniformLocation(prog, "color"), 0.0f, 1.0f, 0.0f, 1.0f);
    static const float tri[] = {-1, -1, 3, -1, -1, 3};
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, tri);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    unsigned char px[4] = {0, 0, 0, 0};
    glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
    Registrar("Prueba GPU 5: píxel leído %d,%d,%d (debería ser 0,255,0)", px[0], px[1], px[2]);
    if (px[1] > 200 && px[0] < 50) return Fin("FUNCIONA: compila shaders desde texto y dibuja fuera de pantalla.");
    snprintf(det, sizeof det, "compila, pero el dibujo no salió (píxel %d,%d,%d).", px[0], px[1], px[2]);
    return Fin(det);
}
#else
const char* PruebaGpu() { return ""; }
#endif
