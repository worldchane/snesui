// OpenGL ES 2 sin pedirle funciones al sistema para abrir: cada función de gl2.h está definida en gl_funciones.cpp
// (generado por herramientas/generar_gl.py) y llama a un puntero. GlCargar los llena con 'buscar' (en la consola,
// sceKernelDlsym sobre el módulo Piglet) y devuelve cuántas no encontró.
#pragma once
int GlCargar(void* (*buscar)(const char* nombre));
