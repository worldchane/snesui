// Trucos de los juegos, en el formato de RetroArch (.cht): "cheats = N", "cheatK_desc", "cheatK_code", "cheatK_enable".
// Se buscan al abrir el juego en el USB (trucos/<consola>/ o cheats/<sistema de RetroArch>/) y en /data/allesiu/trucos,
// por el nombre del juego. Cuáles están activos se guarda con las partidas del juego.
#pragma once

struct Truco { char nombre[96]; char codigo[160]; bool activo; };

// Al abrir un juego (con el acceso al USB abierto): busca su archivo de trucos y lee cuáles estaban activos.
// carpetaGuardados/base: dónde se guardan las partidas del juego (el estado de los trucos va al lado, en <base>.trucos).
void TrucosAbrir(const char* rutaJuego, int sistema, const char* base);
int TrucosCuantos();
const Truco* TrucoDe(int i);
void TrucoCambiar(int i);   // activa o desactiva, lo guarda y lo aplica al núcleo
void TrucosAplicar();       // vuelve a poner en el núcleo los activos (al abrir o reiniciar)
const char* TrucosArchivo();   // de dónde salieron ("" si no hay)
