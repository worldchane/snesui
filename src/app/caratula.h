// Carátulas de los juegos: se guardan en /data/allesiu/caratulas/<nombre>.png y se bajan de internet una sola vez.
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
// Dónde va (o está) la carátula de esa clave: /data/allesiu/caratulas/<consola>/<nombre>.png.
void CaratulaRuta(const char* clave, char* ruta, int max);
// Copia del USB las carátulas que el usuario ya tenga (ver caratulas_usb.cpp) para los juegos que no tengan una.
// Llamar con el acceso completo abierto. Devuelve cuántas copió.
struct Juego;
int CaratulasDelUsb(const Juego* juegos, int n);
// Descarga de las carátulas que falten, en un hilo aparte, desde la base de carátulas de RetroArch (libretro). 'claves'
// son los nombres de archivo de las roms sin extensión (con roms No-Intro coinciden con los de libretro). Devuelve false
// si ya había una descarga en curso.
// automatica: la pidió la app sola al terminar una búsqueda (sin aviso si no hay red o no había nada nuevo).
bool EmpezarDescarga(const char* const* claves, int n, bool automatica = false);
struct ProgresoDescarga
{
    bool activa, terminada, fallo;   // fallo: no hubo red
    bool automatica;                 // la pidió la app sola (ver EmpezarDescarga)
    bool leyendoLista;               // bajando la lista de nombres del servidor de una consola (antes de las carátulas)
    int hechas, total, bajadas, sinCaratula;
    char detalle[160];
};
ProgresoDescarga Progreso();
void OlvidarCaratulas();
// Borra el registro de las carátulas que no estaban en el servidor (la próxima descarga las vuelve a buscar).
void OlvidarNoEncontradas();
// Carga un PNG cualquiera a 0xRRGGBB (malloc; liberar con free). NULL si no se pudo.
uint32_t* CargarImagen(const char* ruta, int* w, int* h);   // libera el caché de memoria (al salir de la biblioteca)

// Elegir la carátula a mano: busca en el servidor las más parecidas al nombre del juego y las baja (en un hilo).
struct OpcionCaratula { char nombre[112]; uint32_t* px; int w, h; };
bool CaratulaManualEmpezar(const char* clave);   // false si hay una descarga de carátulas en curso
int CaratulaManualEstado(int* total, bool* terminado, bool* sinRed);   // cuántas ya están listas
const OpcionCaratula* CaratulaManualOpcion(int i);
bool CaratulaManualElegir(int i);                // la guarda como carátula del juego
void CaratulaManualCerrar();                     // suelta las imágenes
bool CaratulaQuitar(const char* clave);          // borra la carátula (y no se vuelve a bajar sola)
