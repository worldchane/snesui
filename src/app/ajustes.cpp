// Ver ajustes.h. El archivo es una línea de texto: "proporcion=0 filtro=0 avance=4 rebobinar=1 copiar=0". Los archivos de
// versiones anteriores solo traen los dos primeros valores: el resto queda con lo de fábrica.
#include "ajustes.h"

#include <stdio.h>

#include "../comun/plataforma.h"

const char* const PROPORCIONES[NUM_PROPORCIONES] = {"4:3 (como la TV)", "8:7 (píxeles cuadrados)", "Píxel exacto", "16:9 (estirada)"};
const char* const FILTROS[NUM_FILTROS] = {"Suave", "Nítido", "Nítido suavizado", "Líneas de TV", "CRT"};

static AjustesGenerales ajustes = {PR_4_3, FI_SUAVE, 4, 1, 0};

AjustesGenerales& General() { return ajustes; }

void AjustesLeer()
{
    char ruta[160], txt[160] = "";
    snprintf(ruta, sizeof ruta, "%s/ajustes.txt", CARPETA_DATOS);
    if (LeerTexto(ruta, txt, sizeof txt) <= 0) return;
    int p = ajustes.proporcion, f = ajustes.filtro, v = ajustes.avance, r = ajustes.rebobinar, c = ajustes.copiarAuto;
    sscanf(txt, "proporcion=%d filtro=%d avance=%d rebobinar=%d copiar=%d", &p, &f, &v, &r, &c);
    if (p >= 0 && p < NUM_PROPORCIONES) ajustes.proporcion = p;
    if (f >= 0 && f < NUM_FILTROS) ajustes.filtro = f;
    if (v >= 2 && v <= 4) ajustes.avance = v;
    ajustes.rebobinar = r ? 1 : 0;
    ajustes.copiarAuto = c ? 1 : 0;
}

void AjustesGuardar()
{
    char ruta[160], txt[160];
    CrearCarpetas(CARPETA_DATOS);
    snprintf(ruta, sizeof ruta, "%s/ajustes.txt", CARPETA_DATOS);
    snprintf(txt, sizeof txt, "proporcion=%d filtro=%d avance=%d rebobinar=%d copiar=%d\n", ajustes.proporcion,
             ajustes.filtro, ajustes.avance, ajustes.rebobinar, ajustes.copiarAuto);
    EscribirTexto(ruta, txt);
}
