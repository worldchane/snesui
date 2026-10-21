// La pantalla de 1920 × 1080, directo con la salida de video de la consola (la consola la escala a la resolución de la
// tele). Cada cuadro: PantallaComenzar → órdenes de dibujo (lienzo.h, texto.h) → PantallaMostrar.
#pragma once

bool PantallaIniciar();
void PantallaComenzar();
void PantallaMostrar();
// Milisegundos del último cuadro: dibujando (todos los núcleos) y entregándolo a la consola.
float MsDibujo();
float MsEntrega();
// Quita la pantalla de carga del sistema (la que se ve al abrir la app). La GPU (Piglet) no abre su pantalla mientras
// está puesta: el ejemplo oficial de Piglet la quita antes de iniciar EGL.
void QuitarPantallaDeCarga();
