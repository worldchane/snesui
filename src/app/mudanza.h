// La mudanza de los datos de versiones anteriores (ver mudanza.cpp).
#pragma once

void Mudanza();                 // al abrir, antes de crear carpetas o leer ajustes
// Los juegos sueltos en /data/allesiu/roms, a la carpeta de su consola (después de ArcadeIniciar: usa la tabla arcade).
void MudanzaOrdenarRoms();
const char* MudanzaResumen();   // qué se movió, para el registro ("" si nada)
