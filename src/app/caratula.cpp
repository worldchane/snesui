// Ver caratula.h. Las carátulas se decodifican con stb_image en un hilo aparte (otro núcleo): el hilo principal solo
// las pide y las dibuja, así la biblioteca nunca se traba aunque aparezcan varias juntas al desplazarse. Se guardan
// achicadas al tamaño de la tarjeta (unos 250 KB cada una en vez de ~1 MB), y entran muchas en memoria.
#include "caratula.h"

#include <atomic>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../comun/plataforma.h"

#define STB_IMAGE_STATIC
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_ASSERT(x) ((void)0)
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

static void RutaCaratula(const char* nombre, char* ruta, int max)
{
    // El nombre puede traer caracteres que no sirven en un archivo: se cambian por '_'.
    char limpio[128];
    int n = 0;
    for (const char* p = nombre; *p && n < (int)sizeof limpio - 1; p++)
    {
        char c = *p;
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') c = '_';
        limpio[n++] = c;
    }
    limpio[n] = 0;
    snprintf(ruta, max, "%s/caratulas/%s.png", CARPETA_DATOS, limpio);
}

bool HayCaratula(const char* nombre)
{
    char ruta[600];
    RutaCaratula(nombre, ruta, sizeof ruta);
    return ExisteArchivo(ruta);
}

// ---------- caché y carga en segundo plano ----------
// Cada entrada pasa por estos estados. El hilo principal pide (VACIA → PEDIDA) y descarta (→ VACIA, nunca una que se
// está CARGANDO); el hilo de carga toma las pedidas (PEDIDA → CARGANDO) y las deja LISTA o FALTA (no hay archivo).
// Los cambios de estado son con compare_exchange, así nunca los dos tocan la misma entrada a la vez.
enum { E_VACIA, E_PEDIDA, E_CARGANDO, E_LISTA, E_FALTA };
static const int CACHE = 120;
static const int LADO = 300;   // las carátulas se achican para entrar en LADO × LADO (el tamaño de la tarjeta)
struct Entrada
{
    char nombre[128];
    uint32_t* px;
    int w, h;
    volatile uint64_t uso;   // cuándo se pidió por última vez (la más reciente se carga primero)
    double lista;            // cuándo quedó lista (para el fundido)
    double revisada;         // cuándo se vio que faltaba (se vuelve a mirar cada tanto: puede haberse bajado)
    std::atomic<int> estado;
};
static Entrada cache[CACHE];
static uint64_t reloj;
static bool hiloLanzado;

static bool Cambiar(Entrada& e, int de, int a) { return e.estado.compare_exchange_strong(de, a, std::memory_order_acq_rel); }

// Achica 'rgba' (w × h) para que entre en LADO × LADO, promediando los píxeles de cada zona (sin escalones), y lo
// pasa a 0xRRGGBB sobre fondo oscuro (por si tiene transparencia).
static uint32_t* Achicar(const uint8_t* rgba, int w, int h, int* nw, int* nh)
{
    float esc = (float)LADO / (w > h ? w : h);
    if (esc > 1) esc = 1;
    int W = (int)(w * esc + .5f), H = (int)(h * esc + .5f);
    if (W < 1) W = 1;
    if (H < 1) H = 1;
    uint32_t* px = (uint32_t*)malloc((size_t)W * H * 4);
    if (!px) return NULL;
    for (int y = 0; y < H; y++)
    {
        int y0 = y * h / H, y1 = (y + 1) * h / H;
        if (y1 <= y0) y1 = y0 + 1;
        for (int x = 0; x < W; x++)
        {
            int x0 = x * w / W, x1 = (x + 1) * w / W;
            if (x1 <= x0) x1 = x0 + 1;
            uint32_t r = 0, g = 0, b = 0, n = 0;
            for (int yy = y0; yy < y1; yy++)
                for (int xx = x0; xx < x1; xx++)
                {
                    const uint8_t* p = rgba + ((size_t)yy * w + xx) * 4;
                    r += p[0] * p[3] / 255; g += p[1] * p[3] / 255; b += p[2] * p[3] / 255; n++;
                }
            px[y * W + x] = (r / n) << 16 | (g / n) << 8 | (b / n);
        }
    }
    *nw = W;
    *nh = H;
    return px;
}

static void* HiloCaratulas(void*)
{
    uint8_t* buf = (uint8_t*)malloc(4 * 1024 * 1024);
    for (;;)
    {
        // La pedida más reciente primero (la que el usuario está mirando ahora).
        Entrada* elegida = NULL;
        for (Entrada& e : cache)
            if (e.estado.load(std::memory_order_acquire) == E_PEDIDA && (!elegida || e.uso > elegida->uso)) elegida = &e;
        if (!elegida || !Cambiar(*elegida, E_PEDIDA, E_CARGANDO)) { Dormir(8); continue; }

        Entrada& e = *elegida;
        uint32_t* px = NULL;
        int w = 0, h = 0;
        char ruta[600];
        RutaCaratula(e.nombre, ruta, sizeof ruta);
        uint64_t tam = 0;
        if (buf && ExisteArchivo(ruta, &tam) && tam > 0 && tam <= 4u * 1024 * 1024)
        {
            int leido = LeerTexto(ruta, (char*)buf, 4 * 1024 * 1024);
            int iw, ih, canales;
            uint8_t* rgba = leido > 0 ? stbi_load_from_memory(buf, leido, &iw, &ih, &canales, 4) : NULL;
            if (rgba)
            {
                px = Achicar(rgba, iw, ih, &w, &h);
                stbi_image_free(rgba);
            }
        }
        e.px = px;
        e.w = w;
        e.h = h;
        e.lista = Segundos();
        e.revisada = e.lista;
        e.estado.store(px ? E_LISTA : E_FALTA, std::memory_order_release);
    }
    return NULL;
}

// Libera una entrada que no se está cargando. Devuelve false si el hilo de carga la tiene.
static bool Descartar(Entrada& e)
{
    int st = e.estado.load(std::memory_order_acquire);
    if (st == E_VACIA) return true;
    if (st == E_CARGANDO || !Cambiar(e, st, E_VACIA)) return false;
    free(e.px);
    e.px = NULL;
    return true;
}

Caratula ObtenerCaratula(const char* nombre)
{
    if (!hiloLanzado) hiloLanzado = Lanzar(HiloCaratulas, NULL, "caratulas_carga");
    double ahora = Segundos();
    for (Entrada& e : cache)
    {
        int st = e.estado.load(std::memory_order_acquire);
        if (st == E_VACIA || strcmp(e.nombre, nombre)) continue;
        e.uso = ++reloj;
        if (st == E_LISTA)
        {
            float f = (float)((ahora - e.lista) / .25);   // aparece en un cuarto de segundo
            return {e.px, e.w, e.h, f > 1 ? 1.f : f};
        }
        // Faltaba: cada 3 s se vuelve a mirar (la descarga de carátulas puede haberla traído).
        if (st == E_FALTA && ahora - e.revisada > 3) { e.revisada = ahora; Cambiar(e, E_FALTA, E_PEDIDA); }
        return {NULL, 0, 0, 0};
    }
    // No está: ocupar una entrada libre o la que hace más tiempo no se pide.
    Entrada* lugar = NULL;
    for (Entrada& e : cache)
    {
        int st = e.estado.load(std::memory_order_acquire);
        if (st == E_VACIA) { lugar = &e; break; }
        if (st != E_CARGANDO && (!lugar || e.uso < lugar->uso)) lugar = &e;
    }
    if (!lugar || !Descartar(*lugar)) return {NULL, 0, 0, 0};
    snprintf(lugar->nombre, sizeof lugar->nombre, "%s", nombre);
    lugar->uso = ++reloj;
    lugar->estado.store(E_PEDIDA, std::memory_order_release);
    return {NULL, 0, 0, 0};
}

// ---------- descarga ----------
// Las carátulas de RetroArch: http://thumbnails.libretro.com/<sistema>/Named_Boxarts/<nombre>.png. Se pide por HTTP
// simple (sin cifrado: la consola no necesita módulos extra) con HTTP/1.0, así la respuesta llega entera y sin trozos.
static const char* const SERVIDOR = "thumbnails.libretro.com";
static const char* const SISTEMA = "Nintendo - Super Nintendo Entertainment System";
static const int MAX_DESCARGAS = 10000;
static char claves[MAX_DESCARGAS][96];
static int nClaves;
static ProgresoDescarga progreso;
static volatile bool enCurso;

ProgresoDescarga Progreso() { return progreso; }

// Agrega 'txt' a la URL con los caracteres especiales en %XX. libretro cambia &*/:`<>?\|" por '_' en sus nombres.
static void AgregarCodificado(char* url, int max, const char* txt)
{
    int n = (int)strlen(url);
    for (const unsigned char* p = (const unsigned char*)txt; *p && n < max - 4; p++)
    {
        unsigned char c = *p;
        if (strchr("&*/:`<>?\\|\"", c)) c = '_';
        bool libre = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || strchr("-_.~", c);
        if (libre) url[n++] = (char)c;
        else n += snprintf(url + n, max - n, "%%%02X", c);
    }
    url[n] = 0;
}

// GET por HTTP/1.0. Devuelve el código de la respuesta (200, 404...) o -1 si falló la conexión; el cuerpo queda en
// 'cuerpo' / '*largo'.
static int PedirHttp(const char* ruta, uint8_t* buf, int max, const uint8_t** cuerpo, int* largo)
{
    int s = RedConectar(SERVIDOR, 80);
    if (s < 0) return -1;
    char pedido[1024];
    int n = snprintf(pedido, sizeof pedido, "GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: snes-ps4\r\nConnection: close\r\n\r\n", ruta, SERVIDOR);
    if (RedEnviar(s, pedido, n) != n) { RedCerrar(s); return -1; }
    int total = 0;
    while (total < max)
    {
        int r = RedRecibir(s, buf + total, max - total);
        if (r <= 0) break;
        total += r;
    }
    RedCerrar(s);
    int codigo = 0;
    buf[total < max ? total : max - 1] = 0;
    if (total < 12 || sscanf((const char*)buf, "HTTP/%*d.%*d %d", &codigo) != 1) return -1;
    for (int i = 0; i + 3 < total; i++)
        if (buf[i] == '\r' && buf[i + 1] == '\n' && buf[i + 2] == '\r' && buf[i + 3] == '\n')
        {
            *cuerpo = buf + i + 4;
            *largo = total - (i + 4);
            return codigo;
        }
    return -1;
}

// Nombres a probar para una rom, del más exacto al más general: el nombre tal cual; sin las marcas entre corchetes
// ([!], [h1]...); solo con la primera región; y sin región, con las regiones más comunes.
static int Candidatos(const char* clave, char out[][96], int max)
{
    int n = 0;
    auto agregar = [&](const char* s) {
        if (!s[0] || n >= max) return;
        for (int i = 0; i < n; i++) if (!strcmp(out[i], s)) return;
        snprintf(out[n++], 96, "%s", s);
    };
    agregar(clave);
    char t[96];
    snprintf(t, sizeof t, "%s", clave);
    char* c = strstr(t, " [");
    if (c) *c = 0;
    agregar(t);
    char r[96];
    snprintf(r, sizeof r, "%s", t);
    char* p = strstr(r, " (");
    if (p) { char* f = strchr(p, ')'); if (f) f[1] = 0; agregar(r); }
    char sin[96];
    snprintf(sin, sizeof sin, "%s", t);
    p = strstr(sin, " (");
    if (p) *p = 0;
    static const char* const REGIONES[] = {"USA", "USA, Europe", "Europe", "Japan", "World", "Japan, USA"};
    for (const char* reg : REGIONES)
    {
        char x[120];
        snprintf(x, sizeof x, "%s (%s)", sin, reg);
        x[95] = 0;
        agregar(x);
    }
    agregar(sin);
    return n;
}

// ---------- índice de carátulas ----------
// La carpeta de carátulas del servidor tiene una página con la lista de todas (unas 3.700, ~1 MB). Se baja una vez y
// se busca en ella el nombre más parecido al de cada rom: así se encuentran aunque el archivo no tenga el nombre exacto
// (mayúsculas distintas, la región que falta, o el subtítulo que el archivo no trae).
static char (*indice)[112];
static int nIndice;

// Comparaciones sin distinguir mayúsculas (propias: así no se piden más funciones a la consola).
static inline int Minus(int c) { return c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c; }
static bool IgualesMinus(const char* a, const char* b) { for (; *a && Minus(*a) == Minus(*b); a++, b++) {} return *a == *b || Minus(*a) == Minus(*b); }
static bool EmpiezaMinus(const char* s, const char* pre) { for (; *pre; s++, pre++) if (Minus(*s) != Minus(*pre)) return false; return true; }

static int Hex(int c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; }

static void LeerIndice(const uint8_t* html, int largo)
{
    static const int MAX_INDICE = 6000;
    if (!indice) indice = (char(*)[112])malloc(sizeof(*indice) * MAX_INDICE);
    nIndice = 0;
    if (!indice) return;
    const char* p = (const char*)html;
    const char* fin = p + largo;
    while (p < fin && nIndice < MAX_INDICE)
    {
        const char* h = strstr(p, "href=\"");
        if (!h || h >= fin) break;
        h += 6;
        const char* q = strchr(h, '"');
        if (!q || q >= fin) break;
        p = q + 1;
        if (q - h < 5 || strncmp(q - 4, ".png", 4)) continue;
        // Decodificar %XX y quitar ".png".
        char* o = indice[nIndice];
        int n = 0;
        for (const char* c = h; c < q - 4 && n < 111; c++)
        {
            if (*c == '%' && c + 2 < q && Hex(c[1]) >= 0 && Hex(c[2]) >= 0) { o[n++] = (char)(Hex(c[1]) * 16 + Hex(c[2])); c += 2; }
            else if (*c == '&' && !strncmp(c, "&amp;", 5)) { o[n++] = '&'; c += 4; }
            else o[n++] = *c;
        }
        o[n] = 0;
        nIndice++;
    }
}

// Título normalizado para comparar: solo letras y números, en minúsculas, sin "the" al principio ni ", the" (libretro
// escribe "Título, The - ..."). Corta en el primer " (" o " [" (las marcas de región y versión).
static void Normalizar(const char* s, char* out, int max, bool cortarSubtitulo)
{
    char t[128];
    snprintf(t, sizeof t, "%s", s);
    char* c = strstr(t, " (");
    if (c) *c = 0;
    c = strstr(t, " [");
    if (c) *c = 0;
    if (cortarSubtitulo) { c = strstr(t, " - "); if (c) *c = 0; }
    int n = 0;
    const char* p = t;
    if (EmpiezaMinus(p, "the ")) p += 4;
    for (; *p && n < max - 1; p++)
    {
        if (EmpiezaMinus(p, ", the") && (p[5] == 0 || p[5] == ' ')) { p += 4; continue; }
        unsigned char ch = (unsigned char)*p;
        if (ch >= 'A' && ch <= 'Z') ch = ch - 'A' + 'a';
        if ((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9')) out[n++] = (char)ch;
    }
    out[n] = 0;
}

// Elige del índice la mejor carátula para la rom. Devuelve -1 si no hay ninguna parecida.
static int BuscarEnIndice(const char* clave)
{
    // 1) El nombre exacto, sin distinguir mayúsculas.
    for (int i = 0; i < nIndice; i++) if (IgualesMinus(indice[i], clave)) return i;
    char tit[128], titCorto[128];
    Normalizar(clave, tit, sizeof tit, false);
    Normalizar(clave, titCorto, sizeof titCorto, true);
    if (!tit[0]) return -1;
    // Región de la rom (si la trae): se prefiere la misma; si no, USA.
    const char* region = strstr(clave, "(") ? strstr(clave, "(") : "(USA";
    int mejor = -1, mejorPuntos = -1000;
    for (int i = 0; i < nIndice; i++)
    {
        char e[128], eCorto[128];
        Normalizar(indice[i], e, sizeof e, false);
        Normalizar(indice[i], eCorto, sizeof eCorto, true);
        int puntos;
        if (!strcmp(e, tit)) puntos = 100;                      // mismo título completo
        else if (!strcmp(eCorto, tit)) puntos = 80;             // la rom no trae el subtítulo
        else if (titCorto[0] && !strcmp(eCorto, titCorto)) puntos = 60;   // mismo título, otro subtítulo
        else
        {
            // La rom se llama como el subtítulo ("Subtítulo" → "Título 2 - Subtítulo").
            const char* guion = strstr(indice[i], " - ");
            char sub[128];
            if (!guion) continue;
            Normalizar(guion + 3, sub, sizeof sub, false);
            if (strcmp(sub, tit)) continue;
            puntos = 50;
        }
        const char* er = strstr(indice[i], "(");
        if (er && !strncmp(er, region, 4)) puntos += 20;
        else if (er && strstr(er, "USA")) puntos += 12;
        else if (er && (strstr(er, "World") || strstr(er, "Europe"))) puntos += 6;
        static const char* const MALAS[] = {"(Beta", "(Proto", "(Demo", "(Sample", "(Virtual Console", "(Pirate", "(Unl", "Collection)"};
        for (const char* m : MALAS) if (strstr(indice[i], m)) puntos -= 30;
        puntos -= (int)strlen(indice[i]) / 16;   // a igualdad, el nombre más corto (menos marcas)
        if (puntos > mejorPuntos) { mejorPuntos = puntos; mejor = i; }
    }
    return mejor;
}

static void* HiloDescarga(void*)
{
    static const int MAXIMO = 4 * 1024 * 1024;
    uint8_t* buf = (uint8_t*)malloc(MAXIMO + 1);
    char detalle[160] = "";
    if (!buf || !RedIniciar(detalle, sizeof detalle))
    {
        snprintf(progreso.detalle, sizeof progreso.detalle, "%s", buf ? detalle : "Sin memoria para bajar carátulas.");
        progreso.fallo = true;
        progreso.activa = false;
        progreso.terminada = true;
        free(buf);
        enCurso = false;
        return NULL;
    }
    char carpeta[128];
    snprintf(carpeta, sizeof carpeta, "%s/caratulas", CARPETA_DATOS);
    CrearCarpetas(carpeta);
    {
        snprintf(progreso.detalle, sizeof progreso.detalle, "Leyendo la lista de carátulas...");
        char ruta[300] = "/";
        AgregarCodificado(ruta, sizeof ruta, SISTEMA);
        snprintf(ruta + strlen(ruta), sizeof ruta - strlen(ruta), "/Named_Boxarts/");
        const uint8_t* cuerpo = NULL;
        int largo = 0;
        nIndice = 0;
        if (PedirHttp(ruta, buf, MAXIMO, &cuerpo, &largo) == 200) LeerIndice(cuerpo, largo);
    }
    int seguidasSinRed = 0;
    for (int i = 0; i < nClaves; i++)
    {
        progreso.hechas = i;
        if (HayCaratula(claves[i])) { progreso.hechas = i + 1; continue; }
        snprintf(progreso.detalle, sizeof progreso.detalle, "%s", claves[i]);
        char cand[12][96];
        int nc;
        int k0 = nIndice ? BuscarEnIndice(claves[i]) : -1;
        if (nIndice) { nc = k0 >= 0 ? 1 : 0; if (k0 >= 0) snprintf(cand[0], 96, "%s", indice[k0]); }   // con índice: solo la elegida
        else nc = Candidatos(claves[i], cand, 12);                                                       // sin índice: probar nombres
        bool hecha = false, sinRed = false;
        for (int k = 0; k < nc && !hecha; k++)
        {
            char ruta[700] = "/";
            AgregarCodificado(ruta, sizeof ruta, SISTEMA);
            snprintf(ruta + strlen(ruta), sizeof ruta - strlen(ruta), "/Named_Boxarts/");
            AgregarCodificado(ruta, sizeof ruta, cand[k]);
            snprintf(ruta + strlen(ruta), sizeof ruta - strlen(ruta), ".png");
            const uint8_t* cuerpo = NULL;
            int largo = 0;
            int codigo = PedirHttp(ruta, buf, MAXIMO, &cuerpo, &largo);
            if (codigo < 0) { sinRed = true; break; }
            if (codigo == 200 && largo > 8 && cuerpo[0] == 0x89 && cuerpo[1] == 'P' && cuerpo[2] == 'N' && cuerpo[3] == 'G')
            {
                char destino[600];
                RutaCaratula(claves[i], destino, sizeof destino);
                FILE* f = AbrirArchivo(destino, "wb");
                if (f) { hecha = fwrite(cuerpo, 1, largo, f) == (size_t)largo; fclose(f); }
            }
        }
        if (hecha) progreso.bajadas++;
        else progreso.sinCaratula++;
        progreso.hechas = i + 1;
        // Sin conexión: se corta después de 2 seguidas (no tiene sentido esperar el tiempo de cada una).
        seguidasSinRed = sinRed ? seguidasSinRed + 1 : 0;
        if (seguidasSinRed >= 2)
        {
            progreso.fallo = true;
            snprintf(progreso.detalle, sizeof progreso.detalle, "No hay conexión con el servidor de carátulas.");
            break;
        }
    }
    if (!progreso.fallo) snprintf(progreso.detalle, sizeof progreso.detalle, "Listo.");
    free(buf);
    progreso.activa = false;
    progreso.terminada = true;
    enCurso = false;
    return NULL;
}

bool EmpezarDescarga(const char* const* lista, int n)
{
    if (enCurso) return false;
    nClaves = n < MAX_DESCARGAS ? n : MAX_DESCARGAS;
    for (int i = 0; i < nClaves; i++) snprintf(claves[i], sizeof claves[i], "%s", lista[i]);
    memset(&progreso, 0, sizeof progreso);
    progreso.total = nClaves;
    progreso.activa = true;
    enCurso = true;
    if (!Lanzar(HiloDescarga, NULL, "caratulas"))
    {
        enCurso = false;
        progreso.activa = false;
        return false;
    }
    return true;
}

uint32_t* CargarImagen(const char* ruta, int* w, int* h)
{
    uint64_t tam = 0;
    if (!ExisteArchivo(ruta, &tam) || tam == 0 || tam > 8u * 1024 * 1024) return NULL;
    uint8_t* buf = (uint8_t*)malloc((size_t)tam + 1);
    if (!buf) return NULL;
    int leido = LeerTexto(ruta, (char*)buf, (int)tam + 1);
    int canales;
    uint8_t* rgba = leido > 0 ? stbi_load_from_memory(buf, leido, w, h, &canales, 4) : NULL;
    free(buf);
    if (!rgba) return NULL;
    uint32_t* px = (uint32_t*)malloc((size_t)*w * *h * 4);
    if (px)
        for (int i = 0; i < *w * *h; i++)
        {
            uint8_t r = rgba[i * 4], g = rgba[i * 4 + 1], b = rgba[i * 4 + 2], a = rgba[i * 4 + 3];
            px[i] = (uint32_t)(r * a / 255) << 16 | (g * a / 255) << 8 | (b * a / 255);
        }
    stbi_image_free(rgba);
    return px;
}

void OlvidarCaratulas()
{
    for (Entrada& e : cache) Descartar(e);
}
