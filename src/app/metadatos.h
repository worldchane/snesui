// Descripción, año, desarrollador, género y jugadores de los juegos, sacados de los gamelist.xml que el usuario tenga
// junto a sus juegos (los arman Batocera, ES-DE y los programas que bajan datos de internet). La búsqueda de la
// biblioteca los lee y los guarda en /data/allesiu/metadatos.txt; la interfaz los consulta por el nombre del archivo.
#pragma once

struct Metadato
{
    const char* anio;
    const char* desarrollador;
    const char* genero;
    const char* jugadores;
    const char* descripcion;
};

void MetadatosCargar();                    // al abrir: lo guardado de la última búsqueda
void MetadatosEmpezar();                   // al empezar una búsqueda
void MetadatosLeerGamelist(const char* ruta);   // un gamelist.xml que encontró la búsqueda
void MetadatosTerminar();                  // al terminar la búsqueda: guarda y pasa a usar lo nuevo (si encontró algo)
// Los datos del juego por el nombre de su archivo (con o sin extensión); NULL si no hay.
const Metadato* MetadatoDe(const char* archivo);
bool MetadatosHay();                       // ¿hay alguno cargado?
