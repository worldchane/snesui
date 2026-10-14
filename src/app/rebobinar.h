// Rebobinar: una historia de estados de la consola para volver atrás en el tiempo. Cada estado se guarda como la
// diferencia con el siguiente (XOR), comprimida: casi todo es igual entre dos cuadros, así cada paso ocupa pocos KB y en
// 16 MB entra bastante más de medio minuto. Se usa desde el hilo de emulación.
#pragma once

bool RebobinarIniciar();     // vacía la historia (al abrir un juego, cargar un estado o reiniciar); false sin memoria
void RebobinarCapturar();    // agrega el estado actual de la consola a la historia
bool RebobinarAtras();       // carga el estado anterior de la historia; false si ya no queda
float RebobinarSegundos();   // cuántos segundos de historia hay (aproximado)
