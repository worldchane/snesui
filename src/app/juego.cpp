// Ver juego.h. La emulación corre en su propio hilo (otro núcleo) mientras el hilo principal dibuja el cuadro anterior:
// así el tiempo de un cuadro es el mayor de los dos y no la suma. La imagen de la SNES se agranda manteniendo 4:3.
// El mando del DualShock 4 se traduce a los botones de la SNES. L3 + R3 juntos abre el menú de pausa.
//
// Guardado, como en snes9x y bsnes:
//   - Batería (SRAM del cartucho): /data/snes-ps4/guardados/<rom>.srm, el contenido tal cual (se puede usar en esos
//     emuladores y al revés). Se escribe sola un momento después de que el juego guarda, y siempre al salir.
//   - Estados rápidos: 4 ranuras por juego, <rom>.st0..st3, cada una con su miniatura (<rom>.st0.img, 128 × 112).
#include "juego.h"

#include <atomic>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../comun/estilo.h"
#include "../comun/lienzo.h"
#include "../comun/texto.h"
#include "../nucleo/snes.h"

static bool abierto;
static float vis;
static char nombre[64], error[160];
static bool mostrarAyuda = true;
static float tAyuda;

// Dos cuadros: el hilo de emulación escribe en uno mientras se muestra el otro.
static uint32_t marcos[2][SNES_ANCHO * SNES_ALTO];
static int16_t audio[2048];
static std::atomic<int> pedidos{0}, hechos{0};   // cuadros pedidos al hilo y cuadros terminados
static std::atomic<int> listo{-1};               // el último cuadro terminado (0 o 1)
static std::atomic<uint16_t> botones{0};
static std::atomic<bool> hiloCreado{false};

// ---------- archivos de guardado ----------
static char base[256];   // /data/snes-ps4/guardados/<nombre de la rom sin extensión>
static const int RANURAS = 4;
static const int MINI_W = SNES_ANCHO / 2, MINI_H = SNES_ALTO / 2;
static uint32_t miniaturas[RANURAS][MINI_W * MINI_H];
static bool hayEstado[RANURAS];
static int contadorSram;   // cuadros que faltan para escribir la batería (hilo de emulación)
static bool carpetaLista;  // /data/snes-ps4/guardados existe y se puede escribir

static void ArmarBase(const char* ruta)
{
    const char* n = strrchr(ruta, '/');
    n = n ? n + 1 : ruta;
    char limpio[160];
    snprintf(limpio, sizeof limpio, "%s", n);
    char* punto = strrchr(limpio, '.');
    if (punto) *punto = 0;
    char carpeta[128];
    snprintf(carpeta, sizeof carpeta, "%s/guardados", CARPETA_DATOS);
    carpetaLista = CrearCarpetas(carpeta);
    snprintf(base, sizeof base, "%s/%s", carpeta, limpio);
}

static bool EscribirArchivo(const char* ruta, const void* datos, size_t n)
{
    FILE* f = AbrirArchivo(ruta, "wb");
    if (!f) return false;
    bool ok = fwrite(datos, 1, n, f) == n;
    fclose(f);
    return ok;
}

// Lee un archivo entero en un búfer nuevo (malloc). Devuelve NULL si no existe.
static uint8_t* LeerArchivo(const char* ruta, uint32_t* n)
{
    uint64_t tam = 0;
    if (!ExisteArchivo(ruta, &tam) || tam == 0 || tam > 16 * 1024 * 1024) return NULL;
    FILE* f = AbrirArchivo(ruta, "rb");
    if (!f) return NULL;
    uint8_t* d = (uint8_t*)malloc((size_t)tam);
    if (d && fread(d, 1, (size_t)tam, f) != tam) { free(d); d = NULL; }
    fclose(f);
    *n = (uint32_t)tam;
    return d;
}

static bool GuardarSram()
{
    if (!SnesSramTam() || !base[0]) return true;
    char r[300];
    snprintf(r, sizeof r, "%s.srm", base);
    return EscribirArchivo(r, SnesSramDatos(), SnesSramTam());
}

// Si escribir en /data falla dentro del encierro del sistema, se reintenta con el acceso completo de GoldHEN (solo desde
// el hilo principal, con la emulación en pausa).
static bool ConAccesoSiHaceFalta(bool (*f)(void* u), void* u)
{
    if (f(u)) return true;
    AbrirAccesoCompleto("snes", true);
    char carpeta[128];
    snprintf(carpeta, sizeof carpeta, "%s/guardados", CARPETA_DATOS);
    carpetaLista = CrearCarpetas(carpeta);
    bool ok = f(u);
    CerrarAccesoCompleto();
    return ok;
}

static void CargarSram()
{
    if (!SnesSramTam()) return;
    char r[300];
    snprintf(r, sizeof r, "%s.srm", base);
    uint32_t n = 0;
    uint8_t* d = LeerArchivo(r, &n);
    if (!d) return;
    memcpy(SnesSramDatos(), d, n < SnesSramTam() ? n : SnesSramTam());
    free(d);
    SnesSramCambiada();   // lo recién cargado no cuenta como cambio
}

static void LeerMiniaturas()
{
    for (int i = 0; i < RANURAS; i++)
    {
        char r[300];
        snprintf(r, sizeof r, "%s.st%d", base, i);
        hayEstado[i] = ExisteArchivo(r);
        memset(miniaturas[i], 0, sizeof miniaturas[i]);
        if (!hayEstado[i]) continue;
        snprintf(r, sizeof r, "%s.st%d.img", base, i);
        uint32_t n = 0;
        uint8_t* d = LeerArchivo(r, &n);
        if (d && n == sizeof miniaturas[i]) memcpy(miniaturas[i], d, n);
        free(d);
    }
}

// ---------- hilo de emulación ----------
static void* HiloEmulacion(void*)
{
    int visto = 0, escribir = 0;
    for (;;)
    {
        int p = pedidos.load(std::memory_order_acquire);
        if (p == visto) { Dormir(1); continue; }
        visto = p;
        SnesMando(0, botones.load(std::memory_order_relaxed));
        int n = SnesCuadro(marcos[escribir], audio, sizeof audio / 4);
        AudioJuego(audio, n);
        // Batería: se escribe 1,5 s después del último cambio (los juegos guardan varios bytes seguidos).
        if (SnesSramCambiada()) contadorSram = 90;
        else if (contadorSram > 0 && --contadorSram == 0) GuardarSram();
        listo.store(escribir, std::memory_order_release);
        escribir ^= 1;
        hechos.store(p, std::memory_order_release);
    }
    return NULL;
}

static void EsperarHilo()
{
    while (hechos.load(std::memory_order_acquire) != pedidos.load(std::memory_order_acquire)) Dormir(1);
}

static uint16_t ABotonesSnes(const Mando& m)
{
    uint16_t b = 0;
    if (m.apretados & B_CRUZ) b |= SB_B;
    if (m.apretados & B_CIRCULO) b |= SB_A;
    if (m.apretados & B_CUADRADO) b |= SB_Y;
    if (m.apretados & B_TRIANGULO) b |= SB_X;
    if (m.apretados & B_L1) b |= SB_L;
    if (m.apretados & B_R1) b |= SB_R;
    if (m.apretados & B_ARRIBA) b |= SB_ARRIBA;
    if (m.apretados & B_ABAJO) b |= SB_ABAJO;
    if (m.apretados & B_IZQ) b |= SB_IZQ;
    if (m.apretados & B_DER) b |= SB_DER;
    if (m.apretados & B_OPTIONS) b |= SB_START;
    if (m.apretados & B_TACTIL) b |= SB_SELECT;
    return b;
}

// ---------- imagen: proporción y filtros ----------
enum { PR_4_3, PR_8_7, PR_ENTERO, PR_16_9, NUM_PROPORCIONES };
static const char* const PROPORCIONES[NUM_PROPORCIONES] = {"4:3 (como la TV)", "8:7 (píxeles cuadrados)", "Píxel exacto", "16:9 (estirada)"};
enum { FI_SUAVE, FI_NITIDO, FI_NITIDO_SUAVE, FI_LINEAS, FI_CRT, NUM_FILTROS };
static const char* const FILTROS[NUM_FILTROS] = {"Suave", "Nítido", "Nítido suavizado", "Líneas de TV", "CRT"};
static int proporcion = PR_4_3, filtro = FI_SUAVE;
static bool ajustesLeidos;

static void LeerAjustes()
{
    char ruta[160], txt[128] = "";
    snprintf(ruta, sizeof ruta, "%s/ajustes.txt", CARPETA_DATOS);
    if (LeerTexto(ruta, txt, sizeof txt) <= 0) return;
    int p = 0, f = 0;
    if (sscanf(txt, "proporcion=%d filtro=%d", &p, &f) == 2)
    {
        if (p >= 0 && p < NUM_PROPORCIONES) proporcion = p;
        if (f >= 0 && f < NUM_FILTROS) filtro = f;
    }
}

static void GuardarAjustes()
{
    char ruta[160], txt[64];
    CrearCarpetas(CARPETA_DATOS);
    snprintf(ruta, sizeof ruta, "%s/ajustes.txt", CARPETA_DATOS);
    snprintf(txt, sizeof txt, "proporcion=%d filtro=%d\n", proporcion, filtro);
    EscribirTexto(ruta, txt);
}

// El rectángulo de la imagen en la pantalla según la proporción.
static void Rectangulo(float& x, float& y, float& w, float& h)
{
    switch (proporcion)
    {
    case PR_8_7: h = ALTO; w = ALTO * (float)SNES_ANCHO / SNES_ALTO; break;
    case PR_ENTERO: { int k = ALTO / SNES_ALTO; w = (float)SNES_ANCHO * k; h = (float)SNES_ALTO * k; break; }
    case PR_16_9: w = ANCHO; h = ALTO; break;
    default: h = ALTO; w = ALTO * 4.f / 3.f; break;
    }
    x = (ANCHO - w) / 2;
    y = (ALTO - h) / 2;
}

// Filtros: la imagen de 256 × 224 se prepara en un búfer más grande y el dibujo la agranda con filtro suave.
//   Nítido suavizado: agrandada ×4 al píxel más cercano (bordes nítidos, sin el temblor del filtro "nítido").
//   Líneas de TV: el doble de filas, con las impares oscuras (como las líneas de un televisor de tubo).
//   CRT: además, cada píxel en 3 columnas que resaltan el rojo, el verde y el azul (la máscara del tubo).
static const int ESCALA_NS = 4;
static uint32_t procesado[SNES_ANCHO * ESCALA_NS * SNES_ALTO * ESCALA_NS];
static int procesadoDe = -1, procesadoFiltro = -1;

static inline uint32_t Escalar(uint32_t c, int num)   // c × num / 256 por componente
{
    uint32_t r = ((c >> 16) & 255) * num >> 8, g = ((c >> 8) & 255) * num >> 8, b = (c & 255) * num >> 8;
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    return r << 16 | g << 8 | b;
}

static void Filtrar(const uint32_t* src, int cual, int& w, int& h)
{
    if (filtro == FI_NITIDO_SUAVE)
    {
        w = SNES_ANCHO * ESCALA_NS; h = SNES_ALTO * ESCALA_NS;
        if (procesadoDe == cual && procesadoFiltro == filtro) return;
        for (int y = 0; y < SNES_ALTO; y++)
        {
            uint32_t* fila = procesado + (y * ESCALA_NS) * w;
            for (int x = 0; x < SNES_ANCHO; x++)
            {
                uint32_t c = src[y * SNES_ANCHO + x];
                for (int k = 0; k < ESCALA_NS; k++) fila[x * ESCALA_NS + k] = c;
            }
            for (int k = 1; k < ESCALA_NS; k++) memcpy(fila + k * w, fila, w * 4);
        }
    }
    else if (filtro == FI_LINEAS)
    {
        w = SNES_ANCHO; h = SNES_ALTO * 2;
        if (procesadoDe == cual && procesadoFiltro == filtro) return;
        for (int y = 0; y < SNES_ALTO; y++)
            for (int x = 0; x < SNES_ANCHO; x++)
            {
                uint32_t c = src[y * SNES_ANCHO + x];
                procesado[(y * 2) * w + x] = Escalar(c, 280);
                procesado[(y * 2 + 1) * w + x] = Escalar(c, 110);
            }
    }
    else
    {
        w = SNES_ANCHO * 3; h = SNES_ALTO * 2;
        if (procesadoDe == cual && procesadoFiltro == filtro) return;
        for (int y = 0; y < SNES_ALTO; y++)
            for (int x = 0; x < SNES_ANCHO; x++)
            {
                uint32_t c = src[y * SNES_ANCHO + x];
                uint32_t r = (c >> 16) & 255, g = (c >> 8) & 255, b = c & 255;
                uint32_t sub[3];
                sub[0] = Escalar(r << 16 | (g * 3 / 4) << 8 | (b * 3 / 4), 300);
                sub[1] = Escalar((r * 3 / 4) << 16 | g << 8 | (b * 3 / 4), 300);
                sub[2] = Escalar((r * 3 / 4) << 16 | (g * 3 / 4) << 8 | b, 300);
                for (int k = 0; k < 3; k++)
                {
                    procesado[(y * 2) * w + x * 3 + k] = sub[k];
                    procesado[(y * 2 + 1) * w + x * 3 + k] = Escalar(sub[k], 120);
                }
            }
    }
    procesadoDe = cual;
    procesadoFiltro = filtro;
}

static void DibujarImagen()
{
    float x, y, w, h;
    Rectangulo(x, y, w, h);
    if (x > 0) { Caja(0, 0, x + 1, ALTO, 0, 0x000000, 1); Caja(x + w - 1, 0, ANCHO - x - w + 1, ALTO, 0, 0x000000, 1); }
    if (y > 0) { Caja(0, 0, ANCHO, y + 1, 0, 0x000000, 1); Caja(0, y + h - 1, ANCHO, ALTO - y - h + 1, 0, 0x000000, 1); }
    int l = listo.load(std::memory_order_acquire);
    if (l < 0) { Caja(x, y, w, h, 0, 0x000000, 1); return; }
    if (filtro == FI_SUAVE || filtro == FI_NITIDO)
    {
        Imagen(marcos[l], SNES_ANCHO, SNES_ALTO, x, y, w, h, 1, filtro == FI_SUAVE);
        return;
    }
    // El búfer procesado se rehace solo cuando hay un cuadro nuevo (cuenta de cuadros terminados).
    int iw, ih;
    Filtrar(marcos[l], hechos.load(std::memory_order_acquire), iw, ih);
    Imagen(procesado, iw, ih, x, y, w, h, 1, true);
}

// ---------- menú de pausa ----------
enum { OP_CONTINUAR, OP_GUARDAR, OP_CARGAR, OP_PROPORCION, OP_FILTRO, OP_REINICIAR, OP_SALIR, NUM_OPCIONES };
static const char* const OPCIONES[NUM_OPCIONES] = {"Continuar", "Guardar estado", "Cargar estado", "Proporción", "Filtro", "Reiniciar", "Volver a la biblioteca"};
static bool menu;
static float visMenu, selSuave, transparencia;
static int sel, ranura;
static char aviso[96];
static float tAviso = 99;
static uint32_t avisoColor;

static void Avisar(const char* txt, uint32_t color)
{
    snprintf(aviso, sizeof aviso, "%s", txt);
    avisoColor = color;
    tAviso = 0;
}

struct EscrituraEstado { const char* ruta; const uint8_t* d; uint32_t n; };
static bool EscribirEstado(void* u)
{
    EscrituraEstado* e = (EscrituraEstado*)u;
    return EscribirArchivo(e->ruta, e->d, e->n);
}

static void GuardarEstado(int r)
{
    uint32_t tam = SnesEstadoTam();
    uint8_t* d = (uint8_t*)malloc(tam);
    uint32_t n = d ? SnesGuardarEstado(d, tam) : 0;
    if (!n) { free(d); Avisar("No se pudo armar el estado (sin memoria)", C_MAL); return; }
    char ruta[300];
    snprintf(ruta, sizeof ruta, "%s.st%d", base, r);
    EscrituraEstado e = {ruta, d, n};
    bool ok = ConAccesoSiHaceFalta(EscribirEstado, &e);
    free(d);
    if (ok)
    {
        // Miniatura: la imagen actual a la mitad (promedio de 2 × 2 píxeles).
        int l = listo.load(std::memory_order_acquire);
        if (l >= 0)
        {
            const uint32_t* src = marcos[l];
            for (int y = 0; y < MINI_H; y++)
                for (int x = 0; x < MINI_W; x++)
                {
                    const uint32_t* p = src + (y * 2) * SNES_ANCHO + x * 2;
                    uint32_t q[4] = {p[0], p[1], p[SNES_ANCHO], p[SNES_ANCHO + 1]};
                    uint32_t r2 = 0, g = 0, b = 0;
                    for (int k = 0; k < 4; k++) { r2 += (q[k] >> 16) & 255; g += (q[k] >> 8) & 255; b += q[k] & 255; }
                    miniaturas[r][y * MINI_W + x] = (r2 / 4) << 16 | (g / 4) << 8 | (b / 4);
                }
            snprintf(ruta, sizeof ruta, "%s.st%d.img", base, r);
            EscribirArchivo(ruta, miniaturas[r], sizeof miniaturas[r]);
        }
        hayEstado[r] = true;
        char t[64];
        snprintf(t, sizeof t, "Estado guardado en la ranura %d", r + 1);
        Avisar(t, C_BIEN);
    }
    else if (!carpetaLista) Avisar("No se pudo crear la carpeta /data/snes-ps4/guardados", C_MAL);
    else Avisar("No se pudo escribir el archivo del estado en /data", C_MAL);
}

static bool CargarEstado(int r)
{
    char ruta[300];
    snprintf(ruta, sizeof ruta, "%s.st%d", base, r);
    uint32_t n = 0;
    uint8_t* d = LeerArchivo(ruta, &n);
    if (!d) { Avisar("La ranura está vacía", C_AVISO); return false; }
    bool ok = SnesCargarEstado(d, n);
    free(d);
    if (!ok) { Avisar("El estado es de otra versión o está dañado", C_MAL); return false; }
    AudioJuegoVaciar();
    char t[64];
    snprintf(t, sizeof t, "Estado de la ranura %d cargado", r + 1);
    Avisar(t, C_BIEN);
    return true;
}

static void Salir()
{
    abierto = false;
    menu = false;
    EsperarHilo();
    ConAccesoSiHaceFalta([](void*) { return GuardarSram(); }, NULL);
    contadorSram = 0;
    SnesDescargar();
    Sonar(SON_ATRAS);
}

static void MenuEntrada(const Mando& m)
{
    if (m.pulsados & B_ARRIBA) { sel = (sel + NUM_OPCIONES - 1) % NUM_OPCIONES; Sonar(SON_MOVER); }
    if (m.pulsados & B_ABAJO) { sel = (sel + 1) % NUM_OPCIONES; Sonar(SON_MOVER); }
    // ← → cambia el valor de la opción elegida: la ranura, la proporción o el filtro.
    int d = (m.pulsados & B_DER) ? 1 : (m.pulsados & B_IZQ) ? -1 : 0;
    if (d && (sel == OP_GUARDAR || sel == OP_CARGAR)) { ranura = (ranura + RANURAS + d) % RANURAS; Sonar(SON_MOVER); }
    if (d && sel == OP_PROPORCION) { proporcion = (proporcion + NUM_PROPORCIONES + d) % NUM_PROPORCIONES; GuardarAjustes(); Sonar(SON_MOVER); }
    if (d && sel == OP_FILTRO) { filtro = (filtro + NUM_FILTROS + d) % NUM_FILTROS; GuardarAjustes(); Sonar(SON_MOVER); }
    if (m.pulsados & (B_CIRCULO | B_OPTIONS)) { menu = false; Sonar(SON_ATRAS); return; }
    if (!(m.pulsados & B_CRUZ)) return;
    switch (sel)
    {
    case OP_CONTINUAR: menu = false; Sonar(SON_ACEPTAR); break;
    case OP_PROPORCION: proporcion = (proporcion + 1) % NUM_PROPORCIONES; GuardarAjustes(); Sonar(SON_MOVER); break;
    case OP_FILTRO: filtro = (filtro + 1) % NUM_FILTROS; GuardarAjustes(); Sonar(SON_MOVER); break;
    case OP_GUARDAR: GuardarEstado(ranura); Sonar(SON_ACEPTAR); menu = false; break;
    case OP_CARGAR: if (CargarEstado(ranura)) { Sonar(SON_ACEPTAR); menu = false; } else Sonar(SON_ERROR); break;
    case OP_REINICIAR: SnesReiniciar(); AudioJuegoVaciar(); Avisar("Juego reiniciado", C_TEXTO); Sonar(SON_ACEPTAR); menu = false; break;
    case OP_SALIR: Salir(); break;
    }
}

static void MenuDibujar(float a)
{
    if (a <= .01f) return;
    // Mientras se elige proporción o filtro, el juego se ve casi sin oscurecer a la derecha (vista previa en vivo).
    bool vistaPrevia = sel == OP_PROPORCION || sel == OP_FILTRO;
    Suave(transparencia, vistaPrevia ? 1 : 0, 12, 1 / 60.f);
    float fondo = .78f - .62f * transparencia;
    Velo(0, 0, ANCHO, ALTO, 0, 0x0A0614, fondo * a, fondo * a);
    if (transparencia > .01f) Caja(0, 0, 780, ALTO, 0, 0x0A0614, .7f * transparencia * a);
    float x = MARGEN + 40 - (1 - a) * 60;
    Texto(F_TITULO, "Pausa", x, 150, C_TEXTO, a);
    Texto(F_CUERPO, nombre, x, 200, C_TEXTO_2, a);

    // Opciones, con la selección que se desliza suave.
    float y0 = 290, paso = 74;
    Caja(x - 24, y0 + selSuave * paso - 14, 620, 64, 16, C_ACENTO, .22f * a);
    Borde(x - 24, y0 + selSuave * paso - 14, 620, 64, 16, 2, C_ACENTO, .7f * a);
    for (int i = 0; i < NUM_OPCIONES; i++)
    {
        char t[64];
        if (i == OP_GUARDAR || i == OP_CARGAR) snprintf(t, sizeof t, "%s   « %d »", OPCIONES[i], ranura + 1);
        else snprintf(t, sizeof t, "%s", OPCIONES[i]);
        if (i == OP_PROPORCION || i == OP_FILTRO)
        {
            Texto(F_SUBTITULO, OPCIONES[i], x, y0 + i * paso, i == sel ? C_TEXTO : C_TEXTO_2, a);
            char v[64];
            snprintf(v, sizeof v, "« %s »", i == OP_PROPORCION ? PROPORCIONES[proporcion] : FILTROS[filtro]);
            Texto(F_CUERPO_M, v, x + 580, y0 + i * paso + 4, i == sel ? C_ACENTO : C_TEXTO_3, a, 2);
            continue;
        }
        Texto(F_SUBTITULO, t, x, y0 + i * paso, i == sel ? C_TEXTO : C_TEXTO_2, a);
    }

    // Ranura elegida: miniatura (o vacía) y las 4 ranuras abajo (se oculta durante la vista previa de la imagen).
    a *= 1 - transparencia;
    if (a <= .01f) return;
    float pw = 768, ph = 750, px = ANCHO - MARGEN - pw - 40 + (1 - a) * 60, py = 120;
    Sombra(px, py, pw, ph, 24, 40, .5f * a);
    Caja(px, py, pw, ph, 24, C_PANEL, a);
    char t[64];
    snprintf(t, sizeof t, "Ranura %d", ranura + 1);
    Texto(F_SUBTITULO, t, px + 32, py + 30, C_TEXTO, a);
    float iw = pw - 64, ih = iw * MINI_H / MINI_W * .875f;
    float ix = px + 32, iy = py + 90;
    if (hayEstado[ranura])
    {
        Imagen(miniaturas[ranura], MINI_W, MINI_H, ix, iy, iw, ih, a, true);
        Texto(F_CUERPO, "Elige Cargar estado para volver a este momento", px + 32, iy + ih + 22, C_TEXTO_2, a);
    }
    else
    {
        Caja(ix, iy, iw, ih, 12, 0x000000, .35f * a);
        Texto(F_SUBTITULO, "Vacía", ix + iw / 2, iy + ih / 2, C_TEXTO_3, a, 1);
        Texto(F_CUERPO, "Guarda un estado para usar esta ranura", px + 32, iy + ih + 22, C_TEXTO_2, a);
    }
    for (int i = 0; i < RANURAS; i++)
    {
        float cx = px + pw / 2 + (i - 1.5f) * 56, cy = py + ph - 34;
        Circulo(cx, cy, i == ranura ? 12 : 8, hayEstado[i] ? C_ACENTO : C_TEXTO_3, a);
    }

    int b[2] = {BM_CRUZ, BM_CIRCULO};
    const char* tx[2] = {"Aceptar", "Volver al juego"};
    Pistas(b, tx, 2, a);
    Texto(F_PEQUENA, "← → cambia la ranura, la proporción o el filtro", MARGEN, ALTO - 44, C_TEXTO_3, a);
}

// ---------- pantalla ----------
bool JuegoAbrir(const char* ruta, const char* nom)
{
    snprintf(nombre, sizeof nombre, "%s", nom);
    error[0] = 0;
    if (!ajustesLeidos) { LeerAjustes(); ajustesLeidos = true; }
    procesadoDe = -1;
    if (!hiloCreado) hiloCreado = Lanzar(HiloEmulacion, NULL, "emulacion");
    EsperarHilo();
    // La rom está en el USB: solo se ve afuera del encierro del sistema (igual que al buscar). Se carga entera a memoria,
    // así que después se puede volver a entrar.
    AbrirAccesoCompleto("snes", true);
    bool ok = SnesCargar(ruta, error);
    CerrarAccesoCompleto();
    ArmarBase(ruta);
    if (ok)
    {
        CargarSram();
        LeerMiniaturas();
    }
    contadorSram = 0;
    AudioJuegoVaciar();
    abierto = true;
    menu = false;
    listo = -1;
    mostrarAyuda = true;
    tAyuda = 0;
    tAviso = 99;
    return ok;
}

bool JuegoActivo() { return abierto || vis > .01f; }

void JuegoCuadro(float t, float dt, const Mando& m)
{
    (void)t;
    bool combo = (m.apretados & B_L3) && (m.apretados & B_R3) && (m.pulsados & (B_L3 | B_R3));
    if (abierto && error[0] && combo) Salir();
    else if (abierto && menu) MenuEntrada(m);
    else if (abierto && combo)
    {
        // Pausa: el hilo termina el cuadro en curso y no se piden más hasta cerrar el menú.
        EsperarHilo();
        menu = true;
        sel = OP_CONTINUAR;
        selSuave = 0;
        mostrarAyuda = false;
        Sonar(SON_SECCION);
    }
    Suave(vis, abierto ? 1 : 0, 12, dt);
    Suave(visMenu, menu ? 1 : 0, 14, dt);
    Suave(selSuave, (float)sel, 18, dt);
    if (vis <= .01f) return;

    // Pedir el cuadro siguiente si el hilo ya terminó el anterior (si va atrasado, se muestra el último que tenga).
    if (abierto && !error[0] && !menu)
    {
        botones.store(ABotonesSnes(m), std::memory_order_relaxed);
        if (hechos.load(std::memory_order_acquire) == pedidos.load(std::memory_order_acquire))
            pedidos.fetch_add(1, std::memory_order_release);
    }

    Opacidad(vis);
    if (error[0])
    {
        Caja(0, 0, ANCHO, ALTO, 0, 0x000000, 1);
        Brillo(ANCHO / 2, ALTO / 2 - 40, 200, C_MAL, .3f);
        Texto(F_TITULO, "No se pudo cargar el juego", ANCHO / 2, ALTO / 2 - 80, C_TEXTO, 1, 1);
        Parrafo(F_CUERPO, error, ANCHO / 2 - 400, ALTO / 2, 800, C_TEXTO_2);
        Texto(F_CUERPO_M, "Pulsa L3 + R3 para volver", ANCHO / 2, ALTO - 120, C_TEXTO_3, 1, 1);
        Opacidad(1);
        return;
    }

    // Imagen de la SNES con la proporción y el filtro elegidos (bandas negras alrededor si no llena la pantalla).
    DibujarImagen();

    // Ayuda al abrir (se va sola a los 4 s).
    tAyuda += dt;
    if (mostrarAyuda && tAyuda > 4) mostrarAyuda = false;
    if (mostrarAyuda)
    {
        float a = tAyuda < 3 ? 1 : (4 - tAyuda);
        Velo(0, ALTO - 70, ANCHO, 70, 0, 0x000000, 0, .6f * a);
        char txt[128];
        snprintf(txt, sizeof txt, "%s   ·   L3 + R3: menú (guardar, cargar, salir)", nombre);
        Texto(F_CUERPO, txt, ANCHO / 2, ALTO - 50, C_TEXTO_2, a, 1);
    }

    MenuDibujar(visMenu);

    // Aviso breve (estado guardado, cargado...).
    tAviso += dt;
    if (tAviso < 2.5f)
    {
        float a = tAviso < .2f ? tAviso / .2f : tAviso > 2 ? (2.5f - tAviso) / .5f : 1;
        float w = AnchoTexto(F_CUERPO_M, aviso) + 64;
        Sombra(ANCHO / 2 - w / 2, 36, w, 60, 30, 20, .4f * a);
        Caja(ANCHO / 2 - w / 2, 36, w, 60, 30, C_PANEL, .95f * a);
        Circulo(ANCHO / 2 - w / 2 + 30, 66, 7, avisoColor, a);
        Texto(F_CUERPO_M, aviso, ANCHO / 2 + 10, 50, C_TEXTO, a, 1);
    }
    Opacidad(1);
}
