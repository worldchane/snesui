// La pantalla de 1920 × 1080, directo con la salida de video de la consola (la consola la escala a la resolución de la
// tele). Cada cuadro: PantallaComenzar → órdenes de dibujo (lienzo.h, texto.h) → PantallaMostrar.
#pragma once

bool PantallaIniciar();
void PantallaComenzar();
void PantallaMostrar();
// Milisegundos del último cuadro: dibujando (todos los núcleos) y entregándolo a la consola.
float MsDibujo();
float MsEntrega();
