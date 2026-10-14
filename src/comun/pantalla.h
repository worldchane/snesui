// La pantalla de 1920 × 1080 a través de SDL2 (el mismo camino que ya funcionó en la consola con Blue Chalk y la
// plantilla). Cada cuadro: PantallaComenzar → órdenes de dibujo (lienzo.h, texto.h) → PantallaMostrar.
#pragma once

bool PantallaIniciar();
void PantallaComenzar();
void PantallaMostrar();
// Milisegundos del último cuadro: dibujando (todos los núcleos) y entregándolo a la consola.
float MsDibujo();
float MsEntrega();
