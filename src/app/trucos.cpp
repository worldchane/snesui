// Ver trucos.h.
#include "trucos.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "../comun/plataforma.h"
#include "consola.h"
#include "registro.h"

static const int MAX_TRUCOS = 128;
static Truco trucos[MAX_TRUCOS];
static int nTrucos, sistemaActual = -1;
static char archivo[600], rutaEstado[400];

int TrucosCuantos() { return nTrucos; }
const Truco* TrucoDe(int i) { return i >= 0 && i < nTrucos ? &trucos[i] : NULL; }
const char* TrucosArchivo() { return archivo; }

static void Normalizar(const char* s, char* out, int max, bool sinMarcas)
{
    int n = 0;
    for (const char* p = s; *p && n < max - 1; p++)
    {
        if (sinMarcas && (*p == '(' || *p == '[')) break;   // sin región ni versión
        unsigned char c = (unsigned char)*p;
        if (isalnum(c)) out[n++] = (char)tolower(c);
    }
    out[n] = 0;
}

// ¿Es el archivo de trucos de este juego? Igual nombre (sin extensión), o el mismo título sin región.
struct Busca { char nombre[128], titulo[128]; char hallado[600]; bool porTitulo; const char* carpeta; };
static void AlListar(const char* n, bool esCarpeta, void* u)
{
    Busca& b = *(Busca*)u;
    const char* e = strrchr(n, '.');
    if (esCarpeta || !e || strcasecmp(e, ".cht")) return;
    char base[256];
    snprintf(base, sizeof base, "%.*s", (int)(e - n), n);
    char a[128], t[128];
    Normalizar(base, a, sizeof a, false);
    Normalizar(base, t, sizeof t, true);
    if (!strcmp(a, b.nombre)) { snprintf(b.hallado, sizeof b.hallado, "%s/%s", b.carpeta, n); b.porTitulo = false; }
    else if (!b.hallado[0] && t[0] && !strcmp(t, b.titulo)) { snprintf(b.hallado, sizeof b.hallado, "%s/%s", b.carpeta, n); b.porTitulo = true; }
}

static bool BuscarEn(const char* carpeta, Busca& b)
{
    b.carpeta = carpeta;
    ListarCarpeta(carpeta, AlListar, &b);
    return b.hallado[0] && !b.porTitulo;
}

// El valor de "clave = valor" (con o sin comillas) en la línea, o NULL.
static const char* Valor(char* linea, const char* clave)
{
    size_t l = strlen(clave);
    char* p = linea;
    while (*p == ' ' || *p == '\t') p++;
    if (strncmp(p, clave, l)) return NULL;
    p += l;
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '=') return NULL;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '"') { p++; char* f = strchr(p, '"'); if (f) *f = 0; }
    else { char* f = p + strlen(p); while (f > p && (f[-1] == ' ' || f[-1] == '\r')) *--f = 0; }
    return p;
}

static void Leer(const char* ruta)
{
    uint64_t tam = 0;
    if (!ExisteArchivo(ruta, &tam) || !tam || tam > 512 * 1024) return;
    char* txt = (char*)malloc((size_t)tam + 1);
    if (!txt) return;
    if (LeerTexto(ruta, txt, (int)tam + 1) <= 0) { free(txt); return; }
    for (char* p = txt; *p;)
    {
        char* fin = strchr(p, '\n');
        if (fin) *fin = 0;
        int k;
        char clave[32];
        if (sscanf(p, " cheat%d_", &k) == 1 && k >= 0 && k < MAX_TRUCOS)
        {
            const char* v;
            snprintf(clave, sizeof clave, "cheat%d_desc", k);
            if ((v = Valor(p, clave))) snprintf(trucos[k].nombre, sizeof trucos[k].nombre, "%s", v);
            snprintf(clave, sizeof clave, "cheat%d_code", k);
            if ((v = Valor(p, clave))) snprintf(trucos[k].codigo, sizeof trucos[k].codigo, "%s", v);
            snprintf(clave, sizeof clave, "cheat%d_enable", k);
            if ((v = Valor(p, clave))) trucos[k].activo = !strcmp(v, "true");
            if (k + 1 > nTrucos) nTrucos = k + 1;
        }
        if (!fin) break;
        p = fin + 1;
    }
    free(txt);
    // Sin nombre: el código.
    for (int i = 0; i < nTrucos; i++)
        if (!trucos[i].nombre[0]) snprintf(trucos[i].nombre, sizeof trucos[i].nombre, "Truco %d", i + 1);
}

// Cuáles estaban activos la última vez: una línea con los números de los activos.
static void LeerEstado()
{
    char txt[1024] = "";
    if (LeerTexto(rutaEstado, txt, sizeof txt) <= 0) return;
    for (int i = 0; i < nTrucos; i++) trucos[i].activo = false;
    for (char* p = txt; *p;)
    {
        int k = (int)strtol(p, &p, 10);
        if (k >= 0 && k < nTrucos) trucos[k].activo = true;
        while (*p && !isdigit((unsigned char)*p)) p++;
    }
}

static void GuardarEstado()
{
    char txt[1024];
    int n = 0;
    for (int i = 0; i < nTrucos && n < (int)sizeof txt - 8; i++)
        if (trucos[i].activo) n += snprintf(txt + n, sizeof txt - n, "%d ", i);
    txt[n] = 0;
    EscribirTexto(rutaEstado, txt);
}

void TrucosAbrir(const char* rutaJuego, int sistema, const char* base)
{
    nTrucos = 0;
    archivo[0] = 0;
    memset(trucos, 0, sizeof trucos);
    sistemaActual = sistema;
    snprintf(rutaEstado, sizeof rutaEstado, "%s.trucos", base);
    const Consola& con = ConsolaDe(sistema);
    Busca b;
    memset(&b, 0, sizeof b);
    const char* n = strrchr(rutaJuego, '/');
    n = n ? n + 1 : rutaJuego;
    char nombre[256];
    snprintf(nombre, sizeof nombre, "%s", n);
    char* e = strrchr(nombre, '.');
    if (e) *e = 0;
    Normalizar(nombre, b.nombre, sizeof b.nombre, false);
    Normalizar(nombre, b.titulo, sizeof b.titulo, true);
    // RetroArch nombra las carpetas de trucos como sus carátulas: "Nintendo - Super Nintendo Entertainment System".
    char ra[96];
    int k = 0;
    for (const char* p = con.caratulas; *p && k < 95; p++) ra[k++] = *p == '_' ? ' ' : *p;
    ra[k] = 0;
    char c[300];
    static const char* const USB[] = {"/mnt/usb0", "/mnt/usb1", "/mnt/usb2", "/mnt/usb3"};
    snprintf(c, sizeof c, "%s/trucos/%s", CARPETA_DATOS, con.carpeta);
    bool listo = BuscarEn(c, b);
    for (const char* u : USB)
    {
        if (listo) break;
        snprintf(c, sizeof c, "%s/trucos/%s", u, con.carpeta);
        if ((listo = BuscarEn(c, b))) break;
        snprintf(c, sizeof c, "%s/cheats/%s", u, ra);
        if ((listo = BuscarEn(c, b))) break;
        snprintf(c, sizeof c, "%s/cheats/%s", u, con.carpeta);
        listo = BuscarEn(c, b);
    }
    if (!b.hallado[0]) return;
    snprintf(archivo, sizeof archivo, "%s", b.hallado);
    Leer(archivo);
    LeerEstado();
    if (nTrucos) Registrar("Trucos: %d de %s", nTrucos, archivo);
}

void TrucosAplicar()
{
    if (sistemaActual < 0) return;
    const Consola& con = ConsolaDe(sistemaActual);
    if (!con.TrucosBorrar || !con.Truco) return;
    con.TrucosBorrar();
    for (int i = 0; i < nTrucos; i++)
        if (trucos[i].activo && trucos[i].codigo[0]) con.Truco((unsigned)i, true, trucos[i].codigo);
}

void TrucoCambiar(int i)
{
    if (i < 0 || i >= nTrucos) return;
    trucos[i].activo = !trucos[i].activo;
    GuardarEstado();
    TrucosAplicar();
}
