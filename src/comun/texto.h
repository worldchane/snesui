// Texto con la letra Inter (licencia OFL, en assets/) suavizada, rasterizada al abrir con stb_truetype.
// Entiende UTF-8: acentos, ñ, ¿¡, y algunos signos (… • – — ← → ↑ ↓ « »).
#pragma once
#include <stdint.h>

enum Fuente
{
    F_PEQUENA,    // Medium 20: notas y etiquetas
    F_CUERPO,     // Regular 26
    F_CUERPO_M,   // Medium 26
    F_ETIQUETA,   // SemiBold 24
    F_SUBTITULO,  // SemiBold 32
    F_TITULO,     // Bold 44
    F_GRANDE,     // Bold 64
    F_INICIAL,    // Bold 92: las iniciales en las carátulas
    F_LOGO,       // Bold 128
    F_BOTON,      // Bold 19: letras dentro de los botones (L1, R1)
    NUM_FUENTES
};

// Carga las letras desde /app0/assets/fuentes. Devuelve false si falta algún archivo.
bool CargarFuentes();
float AnchoTexto(int fuente, const char* s);
float AltoLinea(int fuente);
// al: 0 izquierda, 1 centrado, 2 derecha. y es la parte de arriba de la línea.
void Texto(int fuente, const char* s, float x, float y, uint32_t c, float a = 1, int al = 0);
// Párrafo con cortes de línea automáticos dentro de 'ancho'. Devuelve el alto que ocupó.
float Parrafo(int fuente, const char* s, float x, float y, float ancho, uint32_t c, float a = 1, float interlineado = 1.35f);
// Corta el texto con "…" para que no pase de 'ancho' (en buf).
const char* Acortar(int fuente, const char* s, float ancho, char* buf, int max);

// Para lienzo.cpp: pinta una orden de texto en las filas [y0, y1) de la franja.
struct FranjaTexto { uint32_t* px; int pitch; int y0, y1, x0, x1; };
void PintarTexto(const FranjaTexto& fr, int fuente, const char* s, int n, float x, float y, uint32_t c, int alfa256);
