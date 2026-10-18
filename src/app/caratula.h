// Carátulas de los juegos: se guardan en /data/snes-ps4/caratulas/<nombre>.png y se bajan de internet una sola vez.
// El módulo las carga a memoria cuando hacen falta (con un caché chico, porque cada una ocupa bastante) y las entrega
// como imagen para dibujar. La descarga usa la base pública de carátulas de libretro.
#pragma once
#include <stdint.h>

// px = NULL si todavía no hay carátula (no existe, o se está cargando). aparece: de 0 a 1 en el cuarto de segundo
// después de cargarse (para mostrarla con un fundido).
struct Caratula { const uint32_t* px; int w, h; float aparece; };

// Devuelve la carátula del juego (por su clave: el nombre del archivo sin extensión) si ya está cargada; si no, la pide
// a un hilo aparte (que la lee de /data y la achica) y devuelve px = NULL. No se traba nunca: llamarla cada cuadro.
Caratula ObtenerCaratula(const char* nombre);
// ¿Existe el archivo de la carátula en /data? (sin cargarla a memoria)
bool HayCaratula(const char* nombre);
// Descarga de las carátulas que falten, en un hilo aparte, desde la base de carátulas de RetroArch (libretro). 'claves'
// son los nombres de archivo de las roms sin extensión (con roms No-Intro coinciden con los de libretro). Devuelve false
// si ya había una descarga en curso.
bool EmpezarDescarga(const char* const* claves, int n);
struct ProgresoDescarga
{
    bool activa, terminada, fallo;   // fallo: no hubo red
    int hechas, total, bajadas, sinCaratula;
    char detalle[160];
};
ProgresoDescarga Progreso();
void OlvidarCaratulas();
// Borra el registro de las carátulas que no estaban en el servidor (la próxima descarga las vuelve a buscar).
void OlvidarNoEncontradas();
// Carga un PNG cualquiera a 0xRRGGBB (malloc; liberar con free). NULL si no se pudo.
uint32_t* CargarImagen(const char* ruta, int* w, int* h);   // libera el caché de memoria (al salir de la biblioteca)
