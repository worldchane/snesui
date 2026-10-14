// Colores, animación y piezas de la interfaz compartidas por la aplicación y los programas de prueba.
#pragma once
#include <stdint.h>

// Paleta: azul noche con acentos azul y turquesa.
// Paleta estilo Super Nintendo: morado/gris de la consola y los cuatro colores de los botones (A rojo, B amarillo,
// X azul, Y verde) como acentos.
static const uint32_t C_FONDO_ARRIBA = 0x1A1033, C_FONDO_ABAJO = 0x241640;
static const uint32_t C_PANEL = 0x2A1E4D, C_PANEL_CLARO = 0x392A63;
static const uint32_t C_TEXTO = 0xF4F1FB, C_TEXTO_2 = 0xB6A8D8, C_TEXTO_3 = 0x7D6FA3;
static const uint32_t C_ACENTO = 0x9B6DFF, C_ACENTO_2 = 0x00C4B4;
// Los cuatro botones de la Super Famicom.
static const uint32_t C_SNES_A = 0xE8485A, C_SNES_B = 0xF5C542, C_SNES_X = 0x4F8CF5, C_SNES_Y = 0x3FBF6F;
static const uint32_t C_BIEN = 0x4ADE9A, C_AVISO = 0xFFC35C, C_MAL = 0xFF6B7A;
static const float MARGEN = 96;   // margen a los lados de la pantalla (zona segura de la TV)

// Acerca 'v' a 'objetivo' de forma exponencial: el movimiento frena suave al llegar y no depende de los cuadros por
// segundo. rapidez ~ 1/segundos (12 = rápido, 6 = tranquilo).
void Suave(float& v, float objetivo, float rapidez, float dt);
float Facilidad(float t);   // curva de 0 a 1 que arranca y frena suave (para entradas con tiempo fijo)

enum BotonMando { BM_CRUZ, BM_CIRCULO, BM_TRIANGULO, BM_CUADRADO, BM_L1, BM_R1, BM_OPTIONS };
// Dibuja el botón centrado en (cx, cy) y devuelve su ancho.
float DibujarBoton(int boton, float cx, float cy, float a = 1);
// Pista "botón + texto" con la parte izquierda en x; devuelve el ancho.
float Pista(int boton, const char* texto, float x, float cy, float a = 1, bool medir = false);
// Fila de pistas alineada a la derecha, abajo de la pantalla.
void Pistas(const int* botones, const char* const* textos, int n, float a = 1);
// Pastilla de texto (etiqueta redondeada). Devuelve el ancho.
float Pastilla(const char* texto, float x, float y, uint32_t fondo, float alfaFondo, uint32_t color, float a = 1);
// Fondo: degradado y tres luces difusas que se mueven lentamente (si animado = false quedan quietas).
void FondoAnimado(float t, bool animado);
