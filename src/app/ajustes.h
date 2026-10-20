// Configuración general (para todos los juegos), en /data/allesiu/ajustes.txt. Cada juego puede tener su propia
// proporción y filtro desde el menú de pausa; si no, usa estos.
#pragma once

// PR_4_3 es "como la consola": 4:3 en las de televisor (como la SNES), la de la pantalla en las portátiles.
enum { PR_4_3, PR_8_7, PR_ENTERO, PR_16_9, NUM_PROPORCIONES };
enum { FI_SUAVE, FI_NITIDO, FI_NITIDO_SUAVE, FI_LINEAS, FI_CRT, FI_HD, NUM_FILTROS };
extern const char* const PROPORCIONES[NUM_PROPORCIONES];
extern const char* const FILTROS[NUM_FILTROS];

struct AjustesGenerales
{
    int proporcion, filtro;
    int avance;      // velocidad del avance rápido: 2, 3 o 4
    int rebobinar;   // 1 = guardar historia para rebobinar con L2
    int copiarAuto;  // 1 = copiar a la consola los juegos nuevos que aparezcan en el USB (si hay espacio)
    int superfx;     // velocidad del chip Super FX: 1 (como la consola) a 4
    int musica;      // 1 = música en la biblioteca (todas las canciones en orden aleatorio)
    int caratulasAuto;   // 1 = bajar las carátulas que falten después de cada búsqueda (si hay internet)
    int tema, menu;  // estilo de interfaz (tema.h) y de menú (interfaz.cpp)
    int botones;     // estilo de los botones (estilo.cpp)
    int continuar;   // 1 = al salir de un juego se guarda un estado y al abrirlo sigue desde ahí
    int bezels;      // 1 = mostrar los marcos decorativos (bezels) que haya
    int runahead;    // 1 = run-ahead de un cuadro (menos retraso de los controles; cuesta el doble)
};
AjustesGenerales& General();
void AjustesLeer();
void AjustesGuardar();

// Ajustes de imagen de cada consola (-1 = los generales), en /data/allesiu/ajustes_consolas.txt. El orden: los del juego
// (menú de pausa), si no los de su consola, si no los generales.
struct AjustesConsola { int proporcion, filtro; };
AjustesConsola& DeConsola(int sistema);
void AjustesConsolasGuardar();
