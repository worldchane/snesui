// Copiar juegos del USB a la consola (/data/snes-ps4/juegos), en un hilo aparte. Antes de copiar se mide el espacio
// libre real de la consola y se deja siempre una reserva para el sistema: se copian solo los que entran. Cada copia se
// verifica (mismo tamaño que el original) y si algo falla se borra la copia incompleta.
#pragma once
#include <stdint.h>

static const uint64_t RESERVA_SISTEMA = 1024ull * 1024 * 1024;   // 1 GB que siempre queda libre

struct ProgresoCopia
{
    bool activa, terminada, fallo;
    int total;          // juegos pedidos
    int hechos;         // procesados hasta ahora
    int copiados;       // copiados bien
    int yaEstaban;      // ya había una copia igual en la consola
    int sinEspacio;     // no entraron
    int errores;        // fallaron al copiar
    uint64_t bytesHechos, bytesTotal;
    char actual[96];    // el que se está copiando
    char detalle[200];  // el motivo si falló
};

// Empieza a copiar estos juegos (rutas en el USB). false si ya hay una copia en curso.
bool EmpezarCopia(const char* const* rutas, int n);
ProgresoCopia ProgresoDeCopia();
bool CopiaEnCurso();

// Espacio libre en la consola (bytes). false si no se pudo medir.
bool EspacioConsola(uint64_t* libres);
// Borra un juego copiado a la consola (solo dentro de /data/snes-ps4/juegos).
bool BorrarDeConsola(const char* ruta);
// "350 MB", "1,5 GB".
void TextoTamano(uint64_t bytes, char* out, int max);
