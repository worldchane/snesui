// Colores, animación y piezas de la interfaz compartidas por la aplicación y los programas de prueba.
#pragma once
#include <stdint.h>

// Paleta: la del estilo de interfaz elegido (tema.h). Los cuatro colores de la SNES y los de estado son fijos.
#include "tema.h"
#define C_FONDO_ARRIBA (temaActual->fondoArriba)
#define C_FONDO_ABAJO (temaActual->fondoAbajo)
#define C_PANEL (temaActual->panel)
#define C_PANEL_CLARO (temaActual->panelClaro)
#define C_TEXTO (temaActual->texto)
#define C_TEXTO_2 (temaActual->texto2)
#define C_TEXTO_3 (temaActual->texto3)
#define C_ACENTO (temaActual->acento)
#define C_ACENTO_2 (temaActual->acento2)
#define C_INSIGNIA (temaActual->insignia)
// Los cuatro botones de la Super Famicom.
static const uint32_t C_SNES_A = 0xE8485A, C_SNES_B = 0xF5C542, C_SNES_X = 0x4F8CF5, C_SNES_Y = 0x3FBF6F;
static const uint32_t C_BIEN = 0x4ADE9A, C_AVISO = 0xFFC35C, C_MAL = 0xFF6B7A;
static const float MARGEN = 96;   // margen a los lados de la pantalla (zona segura de la TV)

// Acerca 'v' a 'objetivo' de forma exponencial: el movimiento frena suave al llegar y no depende de los cuadros por
// segundo. rapidez ~ 1/segundos (12 = rápido, 6 = tranquilo).
void Suave(float& v, float objetivo, float rapidez, float dt);
float Facilidad(float t);   // curva de 0 a 1 que arranca y frena suave (para entradas con tiempo fijo)

enum BotonMando { BM_CRUZ, BM_CIRCULO, BM_TRIANGULO, BM_CUADRADO, BM_L1, BM_R1, BM_OPTIONS, BM_TACTIL };
// Estilos de los botones (50: forma, relleno, color de los símbolos, grosor y tamaño).
int NumEstilosBoton();
void BotonesElegir(int i);
int BotonesElegido();
const char* NombreEstiloBoton(int i);
// La cápsula de lo elegido (la pestaña de arriba) con la forma, el relleno y el brillo del estilo de los botones.
void Capsula(float x, float y, float w, float h, float a);
// El color de un texto encima de una Capsula (oscuro si el estilo la rellena de un color claro).
uint32_t TextoCapsula();
// Dibuja el botón centrado en (cx, cy) y devuelve su ancho.
float DibujarBoton(int boton, float cx, float cy, float a = 1);
// Pista "botón + texto" con la parte izquierda en x; devuelve el ancho.
float Pista(int boton, const char* texto, float x, float cy, float a = 1, bool medir = false);
// Fila de pistas alineada a la derecha, abajo de la pantalla.
void Pistas(const int* botones, const char* const* textos, int n, float a = 1);
// Lo mismo a otra altura (cy: el centro de la fila).
// derecha: dónde terminan (por defecto, el margen derecho de la pantalla; dentro de una ventana, su borde).
void PistasEn(const int* botones, const char* const* textos, int n, float a, float cy, float derecha = -1);
// Pastilla de texto (etiqueta redondeada). Devuelve el ancho.
float Pastilla(const char* texto, float x, float y, uint32_t fondo, float alfaFondo, uint32_t color, float a = 1);
// Panel (ventanas, avisos, menús) con el material del tema: sólido, vidrio, neón, papel... radio se ajusta al tema.
void Panel(float x, float y, float w, float h, float radio, float a = 1);
// Radio de esquina ajustado al tema (los temas "rectos" redondean menos).
float Radio(float r);
// Fondo: degradado y tres luces difusas que se mueven lentamente (si animado = false quedan quietas).
void FondoAnimado(float t, bool animado);
