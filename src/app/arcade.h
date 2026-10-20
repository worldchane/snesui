// Los juegos arcade que conoce la versión de FinalBurn Neo que lleva ALLESIU (assets/misc/arcade.bin, armada con
// herramientas/generar_arcade.py desde sus DAT): títulos, clones, BIOS y los archivos de cada juego. Sirve para mostrar
// el nombre real (el zip "juego" → su título completo), no listar las BIOS como juegos y avisar qué le falta a un zip antes de abrirlo.
#pragma once
#include <stdint.h>

bool ArcadeIniciar();                        // lee la tabla (una vez); false si no está
int ArcadeBuscar(const char* nombreZip);     // por el nombre del zip sin extensión ("juego"); -1 si no es un juego conocido
const char* ArcadeNombre(int juego);         // "juego" (el nombre corto)
const char* ArcadeTitulo(int juego);         // el título completo
const char* ArcadeFabricante(int juego);
int ArcadeAnio(int juego);
int ArcadePadre(int juego);                  // el juego del que es versión (clon), o -1
int ArcadeBios(int juego);                   // la BIOS que usa (neogeo, pgm...), o -1
bool ArcadeEsBios(int juego);                // es una BIOS, no un juego
bool ArcadeEsNeoGeo(int juego);
// La pestaña del juego: SIS_NEOGEO, SIS_CPS1, SIS_CPS2, SIS_CPS3 o, las demás placas, SIS_ARCADE (consola.h).
int ArcadePlaca(int juego);

enum EstadoArcade
{
    ARC_SIN_REVISAR, ARC_COMPLETO, ARC_FALTAN_ARCHIVOS, ARC_FALTA_PADRE, ARC_FALTA_BIOS, ARC_OTRA_VERSION, ARC_DESCONOCIDO,
};
// Revisa un zip (o 7z) de un juego arcade contra lo que espera esta versión del emulador: compara las huellas (CRC) de
// sus archivos, y si los que vienen del juego padre o de la BIOS están en sus zips (en la misma carpeta, o la BIOS en
// /data/allesiu/bios). Deja en 'explicacion' (160 bytes) un texto para el usuario si no está completo.
int ArcadeRevisar(const char* ruta, char* explicacion);
// El juego de un zip (o 7z) por lo que tiene adentro (las huellas de sus archivos), sin importar su nombre; -1 si no
// se parece a ninguno. ArcadeDeRuta prueba primero con el nombre.
int ArcadeIdentificar(const char* ruta);
int ArcadeDeRuta(const char* ruta);
// Antes de abrir un juego arcade: deja en 'rutaNucleo' la ruta con el nombre corto que necesita FinalBurn Neo y arma
// los alias (LibretroAlias) para el zip real y para el juego principal y las BIOS que tengan otro nombre.
bool ArcadePrepararAlias(const char* ruta, char* rutaNucleo, int max);
// El zip (o 7z) del juego 'j' en 'carpeta', con su nombre corto o reconocido por contenido; false si no está.
bool ArcadeZipDe(const char* carpeta, int j, char* sale, int max);
// El nombre del juego desde la ruta de su zip (".../juego.zip" → "juego"); en un búfer propio.
const char* ArcadeNombreDeRuta(const char* ruta);
// El texto corto de un estado ("Completo", "Faltan archivos"...).
const char* ArcadeEstadoTexto(int estado);
