// Configuración general (para todos los juegos), en /data/snes-ps4/ajustes.txt. Cada juego puede tener su propia
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
};
AjustesGenerales& General();
void AjustesLeer();
void AjustesGuardar();
