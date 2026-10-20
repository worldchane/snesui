// Copiar juegos entre el USB y la consola (/data/allesiu/roms), en las dos direcciones, en un hilo aparte. Antes de
// copiar se mide el espacio libre real del destino y se deja siempre una reserva (1 GB en la consola para el sistema;
// un poco en el USB): se copian solo los que entran. Cada copia se verifica (mismo tamaño que el original) y si algo
// falla se borra la copia incompleta.
#pragma once
#include <stdint.h>

static const uint64_t RESERVA_SISTEMA = 1024ull * 1024 * 1024;   // 1 GB que siempre queda libre en la consola
static const uint64_t RESERVA_USB = 16ull * 1024 * 1024;          // y 16 MB en el USB

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
    bool alUsb;         // la dirección: de la consola al USB (si no, del USB a la consola)
    bool borrando;      // es un borrado (no una copia): 'copiados' cuenta los borrados
    char actual[96];    // el que se está copiando
    char detalle[200];  // el motivo si falló
};

// Empieza a copiar estos juegos a la carpeta 'destino' (CARPETA_JUEGOS_CONSOLA, o una carpeta del USB), dejando libre
// 'reserva' bytes. false si ya hay una copia en curso.
bool EmpezarCopia(const char* const* rutas, int n, const char* destino, uint64_t reserva);
// Busca un USB conectado y devuelve la carpeta donde copiar juegos (por ejemplo /mnt/usb0/SNES). false si no hay.
// También devuelve el espacio libre del USB, si se pide.
bool CarpetaUsb(char* out, int max, uint64_t* libres = 0);
// Borra estos juegos (de la consola o del USB) en segundo plano. Solo borra roms de SNES (.sfc/.smc/.fig/.swc) dentro
// de /data/allesiu/roms o de un USB; nunca otros archivos ni carpetas. false si ya hay una copia o un borrado en curso.
bool EmpezarBorrado(const char* const* rutas, int n);
ProgresoCopia ProgresoDeCopia();
bool CopiaEnCurso();

// Espacio libre en la consola (bytes). false si no se pudo medir.
bool EspacioConsola(uint64_t* libres);
// Borra un juego copiado a la consola (solo dentro de /data/allesiu/roms).
bool BorrarDeConsola(const char* ruta);
// "350 MB", "1,5 GB".
void TextoTamano(uint64_t bytes, char* out, int max);
