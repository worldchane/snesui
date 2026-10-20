// Música de la biblioteca: canciones originales de ALLESIU, compuestas por reglas y sintetizadas en el momento como un
// chip de sonido de consola (melodía, arpegio, bajo y batería), sin archivos de audio. La mezcla la llama el hilo de
// sonido (plataforma.cpp); la interfaz elige la canción y la apaga mientras se juega.
#pragma once

static const int NUM_CANCIONES = 10;

const char* MusicaNombre(int cancion);
void MusicaElegir(int cancion);   // empieza esa canción desde el principio; -1 = sin música
int MusicaElegida();
void MusicaActiva(bool si);       // false: baja hasta silencio (por ejemplo, durante un juego); true: vuelve
// Encendida: suenan todas en orden aleatorio (cada una dos vueltas, con un fundido entre una y otra). 'semilla' cambia
// el orden de una vez a otra (por ejemplo, el reloj al abrir la app).
void MusicaEncender(bool si, unsigned semilla);
// La canción que sigue a la que suena (en orden; después de la última, la primera). Devuelve cuál quedó.
int MusicaSiguiente();
// Suma n muestras estéreo de 48 kHz a L y R (las del hilo de sonido).
void MusicaMezclar(float* L, float* R, int n);
