// La GPU para los núcleos que dibujan con OpenGL ES 2 (la N64 con GLideN64): un contexto EGL y un framebuffer fuera de
// pantalla donde el núcleo dibuja; al terminar cada cuadro la imagen se lee a memoria y sigue el camino de siempre
// (filtros, proporción, interfaz). Igual en la consola (Piglet) y en la PC (ANGLE): lo que cambia es cómo se cargan las
// bibliotecas (GpuCargarModulos y GpuBuscar, en plataforma.cpp y plataforma_pc.cpp).
#pragma once
#include <stdint.h>

// Prepara el contexto (la primera vez carga las bibliotecas) y un framebuffer de w × h con profundidad y stencil. Deja
// el contexto activo en el hilo que llama. false y el motivo en 'error' si no se pudo.
bool GpuAbrir(int w, int h, char* error, int max);
unsigned GpuFramebuffer();
void* GpuFuncion(const char* nombre);   // para get_proc_address del núcleo
// El contexto es de un hilo por vez: el hilo de la emulación lo toma al empezar cada cuadro y lo suelta al terminar,
// así el principal puede usarlo al cerrar el juego.
bool GpuActivar();
void GpuSoltar();
// Lee el framebuffer (w × h, el origen de OpenGL abajo) a 'sale' en 0xRRGGBB, con la primera fila arriba.
bool GpuLeer(uint32_t* sale, int w, int h);
// Borra el framebuffer (el contexto queda para la próxima vez).
void GpuCerrar();
// Anota en el registro el error pendiente de OpenGL (para buscar fallos), con el momento.
void GpuAnotarError(const char* momento);

// De cada plataforma: cargar EGL y OpenGL ES 2 (en la consola, Piglet y su configuración) y buscar sus funciones.
bool GpuCargarModulos(char* error, int max);
void* GpuBuscar(const char* nombre);
