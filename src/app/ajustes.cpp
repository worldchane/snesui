// Ver ajustes.h. El archivo es una línea de texto: "proporcion=0 filtro=0 avance=4 rebobinar=1 copiar=0 superfx=1 musica=1". Los archivos de
// versiones anteriores solo traen los dos primeros valores: el resto queda con lo de fábrica.
#include "ajustes.h"

#include <string.h>

#include "consola.h"

#include <stdio.h>

#include "../comun/plataforma.h"

const char* const PROPORCIONES[NUM_PROPORCIONES] = {"Como la consola", "Píxeles cuadrados", "Píxel exacto", "16:9 (estirada)"};
const char* const FILTROS[NUM_FILTROS] = {"Suave", "Nítido", "Nítido suavizado", "Líneas de TV", "CRT", "HD"};

// Por defecto: interfaz Aero clásico (tema 0), menú Clásico (0) y botones "Tecla vidrio" (18).
static AjustesGenerales ajustes = {PR_4_3, FI_SUAVE, 4, 1, 0, 1, 1, 1, 0, 0, 18, 0, 1, 0};

AjustesGenerales& General() { return ajustes; }

void AjustesLeer()
{
    char ruta[160], txt[260] = "";
    snprintf(ruta, sizeof ruta, "%s/ajustes.txt", CARPETA_DATOS);
    if (LeerTexto(ruta, txt, sizeof txt) <= 0) return;
    int p = ajustes.proporcion, f = ajustes.filtro, v = ajustes.avance, r = ajustes.rebobinar, c = ajustes.copiarAuto;
    int x = ajustes.superfx, m = ajustes.musica, ca = ajustes.caratulasAuto, te = ajustes.tema, me = ajustes.menu, bo = ajustes.botones, cn = ajustes.continuar, bz = ajustes.bezels, ra = ajustes.runahead, ev = 0;
    sscanf(txt, "proporcion=%d filtro=%d avance=%d rebobinar=%d copiar=%d superfx=%d musica=%d caratulas=%d tema=%d menu=%d botones=%d continuar=%d bezels=%d runahead=%d estilos=%d",
           &p, &f, &v, &r, &c, &x, &m, &ca, &te, &me, &bo, &cn, &bz, &ra, &ev);
    if (p >= 0 && p < NUM_PROPORCIONES) ajustes.proporcion = p;
    if (f >= 0 && f < NUM_FILTROS) ajustes.filtro = f;
    if (v >= 2 && v <= 4) ajustes.avance = v;
    ajustes.rebobinar = r ? 1 : 0;
    ajustes.copiarAuto = c ? 1 : 0;
    if (x >= 1 && x <= 4) ajustes.superfx = x;
    ajustes.musica = m ? 1 : 0;
    ajustes.caratulasAuto = ca ? 1 : 0;
    // Los estilos guardados antes de la lista definitiva (estilos=2) apuntan a otros: quedan los de por defecto.
    if (ev >= 2)
    {
        ajustes.tema = te >= 0 && te < 1000 ? te : 0;   // los estilos revisan su propio rango al elegirse
        ajustes.menu = me >= 0 && me < 1000 ? me : 0;
        ajustes.botones = bo >= 0 && bo < 1000 ? bo : 0;
    }
    ajustes.continuar = cn ? 1 : 0;
    ajustes.bezels = bz ? 1 : 0;
    ajustes.runahead = ra ? 1 : 0;
}

static AjustesConsola consolas[NUM_SISTEMAS];
static bool consolasLeidas;

static void RutaConsolas(char* r, int max) { snprintf(r, max, "%s/ajustes_consolas.txt", CARPETA_DATOS); }

// Una línea por consola con algo propio: "<carpeta> <proporción> <filtro>".
static void LeerConsolas()
{
    consolasLeidas = true;
    for (AjustesConsola& c : consolas) c = {-1, -1};
    char r[160], txt[2048] = "";
    RutaConsolas(r, sizeof r);
    if (LeerTexto(r, txt, sizeof txt) <= 0) return;
    for (char* p = txt; *p;)
    {
        char carpeta[32];
        int pr = -1, fi = -1;
        if (sscanf(p, "%31s %d %d", carpeta, &pr, &fi) == 3)
            for (int s = 0; s < NUM_SISTEMAS; s++)
                if (!strcmp(ConsolaDe(s).carpeta, carpeta))
                {
                    consolas[s].proporcion = pr >= 0 && pr < NUM_PROPORCIONES ? pr : -1;
                    consolas[s].filtro = fi >= 0 && fi < NUM_FILTROS ? fi : -1;
                }
        char* fin = strchr(p, '\n');
        if (!fin) break;
        p = fin + 1;
    }
}

AjustesConsola& DeConsola(int sistema)
{
    if (!consolasLeidas) LeerConsolas();
    return consolas[sistema >= 0 && sistema < NUM_SISTEMAS ? sistema : 0];
}

void AjustesConsolasGuardar()
{
    char r[160], txt[2048];
    int n = 0;
    for (int s = 0; s < NUM_SISTEMAS; s++)
        if (consolas[s].proporcion >= 0 || consolas[s].filtro >= 0)
            n += snprintf(txt + n, sizeof txt - n, "%s %d %d\n", ConsolaDe(s).carpeta, consolas[s].proporcion, consolas[s].filtro);
    txt[n] = 0;
    RutaConsolas(r, sizeof r);
    EscribirTexto(r, txt);
}

void AjustesGuardar()
{
    char ruta[160], txt[260];
    CrearCarpetas(CARPETA_DATOS);
    snprintf(ruta, sizeof ruta, "%s/ajustes.txt", CARPETA_DATOS);
    snprintf(txt, sizeof txt, "proporcion=%d filtro=%d avance=%d rebobinar=%d copiar=%d superfx=%d musica=%d caratulas=%d tema=%d menu=%d botones=%d continuar=%d bezels=%d runahead=%d estilos=2\n",
             ajustes.proporcion, ajustes.filtro, ajustes.avance, ajustes.rebobinar, ajustes.copiarAuto, ajustes.superfx, ajustes.musica,
             ajustes.caratulasAuto, ajustes.tema, ajustes.menu, ajustes.botones, ajustes.continuar, ajustes.bezels, ajustes.runahead);
    EscribirTexto(ruta, txt);
}
