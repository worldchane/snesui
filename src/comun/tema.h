// Estilos de interfaz: colores, fondo animado y material de los paneles. Los colores de estilo.h (C_TEXTO, C_PANEL...)
// salen del tema elegido, así toda la interfaz cambia junta. Cada tema es una combinación propia de ALLESIU.
#pragma once
#include <stdint.h>

// Fondos animados (fondo.cpp).
enum FondoTema
{
    FT_PAISAJE,      // el paisaje nocturno en pixel art, con sus colores
    FT_PAISAJE_MAPA, // el mismo paisaje con los colores del tema (de oscuro a claro: f[0] → f[3])
    FT_AURORA,       // cielo con bandas de aurora que ondulan
    FT_REJILLA,      // sol y rejilla en perspectiva que avanza (synthwave)
    FT_ESPACIO,      // estrellas que vienen hacia la pantalla y una nebulosa
    FT_OLAS,         // capas de olas
    FT_BURBUJAS,     // círculos suaves que suben
    FT_MANCHAS,      // manchas grandes de color que se mueven despacio (fondos de macOS)
    FT_LLUVIA,       // columnas de puntos que caen
    FT_PLASMA,       // plasma de las demos de los 90
    FT_PUNTOS,       // cuadrícula de puntos con una onda
    FT_CUADROS,      // cuadros en diagonal que se desplazan
    FT_DEGRADADO,    // degradado con dos luces
    NUM_FONDOS_TEMA
};

// Material de los paneles (Panel en estilo.cpp).
enum MaterialTema
{
    MT_SOLIDO,     // sólido con sombra
    MT_VIDRIO,     // vidrio esmerilado: el fondo desenfocado detrás (Aero de macOS)
    MT_NEON,       // oscuro con contorno brillante
    MT_DEGRADADO,  // degradado vertical
    MT_PLANO,      // plano, sin sombra
    MT_CONTORNO,   // translúcido con contorno fino
    MT_PAPEL,      // claro con sombra corrida (retro)
    MT_METAL,      // degradado con brillo arriba
};

struct Tema
{
    const char* nombre;
    uint8_t fondo, material;
    uint32_t f[4];            // colores del fondo, de oscuro a claro
    uint32_t fondoArriba, fondoAbajo;
    uint32_t panel, panelClaro;
    uint32_t texto, texto2, texto3;
    uint32_t acento, acento2;
    uint32_t insignia;        // fondo de las etiquetas sobre las tarjetas (consola, USB)
    float radio;              // multiplica las esquinas redondeadas (0,4 = casi rectas)
};

extern const int NUM_TEMAS;
extern const Tema* temaActual;
const Tema& TemaDe(int i);
void TemaElegir(int i);
int TemaElegido();
// Tema claro (texto oscuro): el logo, los marcos de las tarjetas y la barra de abajo cambian para que se vean.
inline bool TemaClaro() { uint32_t t = temaActual->texto; return ((t >> 16 & 255) * 3 + (t >> 8 & 255) * 6 + (t & 255)) < 1280; }
