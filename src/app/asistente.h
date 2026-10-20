// Asistente de primera vez: unas pantallas que explican cómo usar ALLESIU y dejan elegir lo básico (jugar desde el
// USB o copiar a la consola, carátulas, música). Sale solo la primera vez (o al pedirlo en Configuración). También la
// pantalla de novedades, la primera vez que se abre una versión nueva.
#pragma once
#include "../comun/plataforma.h"

void AsistenteIniciar(const char* version);   // decide si hay que mostrar el asistente o las novedades
void AsistenteAbrir();                        // desde Configuración
bool AsistenteActivo();                       // el asistente o las novedades están en pantalla
// Si el asistente o las novedades están abiertos, los dibuja y atiende el mando (true: se llevan el cuadro).
bool AsistenteCuadro(float dt, const Mando& m);
