// Ver juego.h. La emulación corre en su propio hilo (otro núcleo) mientras el hilo principal dibuja el cuadro anterior:
// así el tiempo de un cuadro es el mayor de los dos y no la suma. La imagen se agranda con la proporción de la consola.
// El juego corre en la consola que le toca por la extensión de la rom (consola.h): la SNES con el núcleo propio, las
// demás con núcleos libretro. El mando del DualShock 4 se traduce a sus botones. L3 + R3 juntos abre el menú de pausa.
//
// Guardado, como en snes9x y bsnes:
//   - Batería (SRAM del cartucho): /data/allesiu/guardados/<rom>.srm (las otras consolas, en guardados/<carpeta>/), el contenido tal cual (se puede usar en esos
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
#include "ajustes.h"
#include "arcade.h"
#include "consola.h"
#include "rebobinar.h"
#include "caratula.h"
#include "trucos.h"
#include "registro.h"

static bool abierto;
static float vis;
static char nombre[64], error[256];
static bool mostrarAyuda = true;
static float tAyuda;

static const Consola* con;   // la consola del juego abierto (JuegoAbrir la elige)

// Dos cuadros: el hilo de emulación escribe en uno mientras se muestra el otro. Cada uno con su tamaño (la imagen de
// algunas consolas cambia de tamaño durante el juego).
static uint32_t marcos[2][CONSOLA_MAX_ANCHO * CONSOLA_MAX_ALTO];
static int marcoW[2] = {SNES_ANCHO, SNES_ANCHO}, marcoH[2] = {SNES_ALTO, SNES_ALTO};
static int16_t audio[4096];

// Corre un cuadro de la consola en el marco 'm'. Devuelve las muestras de sonido.
// Run-ahead de un cuadro (menos retraso de los controles): se corre el cuadro de verdad sin mostrarlo (su sonido sí
// se usa), se guarda el estado, se corre uno más con la misma entrada y ese es el que se ve, y se vuelve al estado
// guardado. Cuesta el doble: solo en las consolas con estados chicos (no el arcade ni la N64).
static uint32_t descarte[CONSOLA_MAX_ANCHO * CONSOLA_MAX_ALTO];
static int16_t audioDescarte[4096];
static uint8_t* estadoAdelanto;
static uint32_t capEstadoAdelanto;

static bool RunAhead()
{
    if (!General().runahead || EsArcade(con->sistema) || con->sistema == SIS_N64) return false;
    uint32_t tam = con->EstadoTam();
    if (!tam || tam > (1u << 20)) return false;
    if (tam > capEstadoAdelanto)
    {
        uint8_t* nuevo = (uint8_t*)realloc(estadoAdelanto, tam);
        if (!nuevo) return false;
        estadoAdelanto = nuevo;
        capEstadoAdelanto = tam;
    }
    return true;
}

static int Correr(int m)
{
    int w = 0, h = 0;
    if (RunAhead())
    {
        int n = con->Cuadro(descarte, &w, &h, audio, sizeof audio / 4);
        uint32_t guardado = con->GuardarEstado(estadoAdelanto, capEstadoAdelanto);
        if (guardado)
        {
            con->Cuadro(marcos[m], &w, &h, audioDescarte, sizeof audioDescarte / 4);
            con->CargarEstado(estadoAdelanto, guardado);
            marcoW[m] = w > 0 && w <= CONSOLA_MAX_ANCHO ? w : 1;
            marcoH[m] = h > 0 && h <= CONSOLA_MAX_ALTO ? h : 1;
            return n;
        }
        memcpy(marcos[m], descarte, sizeof descarte);   // no se pudo guardar: el cuadro normal
        marcoW[m] = w > 0 && w <= CONSOLA_MAX_ANCHO ? w : 1;
        marcoH[m] = h > 0 && h <= CONSOLA_MAX_ALTO ? h : 1;
        return n;
    }
    int n = con->Cuadro(marcos[m], &w, &h, audio, sizeof audio / 4);
    marcoW[m] = w > 0 && w <= CONSOLA_MAX_ANCHO ? w : 1;
    marcoH[m] = h > 0 && h <= CONSOLA_MAX_ALTO ? h : 1;
    return n;
}

// Achica (o agranda) una imagen al tamaño de destino promediando 2 × 2 puntos por píxel (miniaturas e informes).
static void Reducir(const uint32_t* src, int sw, int sh, uint32_t* dst, int dw, int dh)
{
    for (int y = 0; y < dh; y++)
        for (int x = 0; x < dw; x++)
        {
            uint32_t r = 0, g = 0, b = 0;
            for (int k = 0; k < 4; k++)
            {
                int sx = (int)(((x * 2 + (k & 1)) * 2 + 1) * (int64_t)sw / (dw * 4));
                int sy = (int)(((y * 2 + (k >> 1)) * 2 + 1) * (int64_t)sh / (dh * 4));
                uint32_t c = src[sy * sw + sx];
                r += (c >> 16) & 255; g += (c >> 8) & 255; b += c & 255;
            }
            dst[y * dw + x] = (r / 4) << 16 | (g / 4) << 8 | (b / 4);
        }
}
static std::atomic<int> pedidos{0}, hechos{0};   // cuadros pedidos al hilo y cuadros terminados
static std::atomic<int> listo{-1};               // el último cuadro terminado (0 o 1)
static std::atomic<uint16_t> botones{0}, botones2{0};   // jugador 1 y jugador 2
static std::atomic<bool> hayJugador2{false};            // hay un segundo mando (el puerto 2 de la SNES está conectado)
static std::atomic<int> velocidad{1};                   // cuadros por cuadro mostrado (R2: avance rápido)
static std::atomic<bool> rebobinando{false};            // L2 mantenido
static bool hayHistoria;                                // se pudo reservar la memoria para rebobinar
static std::atomic<bool> hiloCreado{false};

// ---------- archivos de guardado ----------
static char base[256];   // /data/allesiu/guardados/<nombre de la rom sin extensión>
static const int RANURAS = 4;
static const int MINI_W = SNES_ANCHO / 2, MINI_H = SNES_ALTO / 2;
static uint32_t miniaturas[RANURAS][MINI_W * MINI_H];
static bool hayEstado[RANURAS];
static int contadorSram;   // cuadros que faltan para escribir la batería (hilo de emulación)
static bool carpetaLista;  // /data/allesiu/guardados existe y se puede escribir

// La carpeta de las partidas: guardados/<carpeta de la consola> (dos roms de consolas distintas pueden llamarse igual).
static void CarpetaGuardados(char* c, int max) { snprintf(c, max, "%s/guardados/%s", CARPETA_DATOS, con->carpeta); }

static void ArmarBase(const char* ruta)
{
    const char* n = strrchr(ruta, '/');
    n = n ? n + 1 : ruta;
    char limpio[160];
    snprintf(limpio, sizeof limpio, "%s", n);
    char* punto = strrchr(limpio, '.');
    if (punto) *punto = 0;
    char carpeta[128];
    CarpetaGuardados(carpeta, sizeof carpeta);
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
    if (!con->SramTam() || !base[0]) return true;
    char r[300];
    snprintf(r, sizeof r, "%s.srm", base);
    return EscribirArchivo(r, con->SramDatos(), con->SramTam());
}

// ---------- diagnóstico automático ----------
// El hilo de emulación vigila el juego: cada segundo revisa si la imagen quedó de un solo color, si va lento y si el
// núcleo detectó algo raro; cada 10 s deja una foto del estado (por si el programa se cuelga). Cuando hay un problema
// arma un informe completo (texto, imagen y estado) y el hilo principal lo escribe en el USB.
static const int INFORME_MAX = 24 * 1024;
static char fotoTexto[INFORME_MAX], informeTexto[INFORME_MAX], informeMotivo[200];
static uint32_t informeImagen[SNES_ANCHO * SNES_ALTO];   // el informe lleva la imagen a 256 × 224
static uint8_t* informeEstado;
static uint32_t informeEstadoN;
static std::atomic<bool> fotoLista{false}, informeListo{false};
static int informesAuto;                    // informes automáticos en esta partida (a lo más 3)
static int cuadrosDiag, segNegro;
static bool negroAvisado;
static double tiempoSuma, tiempoMax, tiempoMaxTotal, tiempoSumaTotal, ultimoLento = -999;
static uint32_t cuadrosMedidos, cuadrosTotal, segundosLentos;
static double inicioPartida;

static void ArmarInforme(const char* motivo, int m)
{
    snprintf(informeMotivo, sizeof informeMotivo, "%s", motivo);
    con->Diagnostico(informeTexto, sizeof informeTexto);
    if (marcoW[m] == SNES_ANCHO && marcoH[m] == SNES_ALTO) memcpy(informeImagen, marcos[m], sizeof informeImagen);
    else Reducir(marcos[m], marcoW[m], marcoH[m], informeImagen, SNES_ANCHO, SNES_ALTO);
    free(informeEstado);
    informeEstado = (uint8_t*)malloc(con->EstadoTam());
    informeEstadoN = informeEstado ? con->GuardarEstado(informeEstado, con->EstadoTam()) : 0;
}

// Después de cada cuadro normal (hilo de emulación). 'ms' es lo que tardó en emularse.
static void Vigilar(int m, double ms)
{
    tiempoSuma += ms;
    tiempoSumaTotal += ms;
    if (ms > tiempoMax) tiempoMax = ms;
    if (ms > tiempoMaxTotal) tiempoMaxTotal = ms;
    cuadrosMedidos++;
    cuadrosTotal++;

    char motivo[200];
    bool problema = con->Anomalia(motivo, sizeof motivo);
    const uint32_t* imagen = marcos[m];

    if (++cuadrosDiag % 60 == 0)
    {
        // Imagen de un solo color durante 10 s seguidos (la pantalla negra con sonido, por ejemplo).
        bool plana = true;
        for (int i = 1; i < marcoW[m] * marcoH[m] && plana; i++) plana = imagen[i] == imagen[0];
        segNegro = plana ? segNegro + 1 : 0;
        if (segNegro == 10 && !negroAvisado && !problema)
        {
            negroAvisado = true;
            snprintf(motivo, sizeof motivo, "La imagen lleva 10 segundos de un solo color (%06X): pantalla negra o vacía",
                     imagen[0]);
            problema = true;
        }
        // Velocidad: más de 16,6 ms por cuadro en promedio = el juego va más lento que la consola real.
        double prom = tiempoSuma / cuadrosMedidos;
        if (prom > 16.6)
        {
            segundosLentos++;
            if (Segundos() - ultimoLento > 60)
            {
                ultimoLento = Segundos();
                Registrar("Lento: %.1f ms por cuadro en promedio (máximo %.1f ms); la consola real usa 16,6 ms", prom, tiempoMax);
            }
        }
        tiempoSuma = tiempoMax = 0;
        cuadrosMedidos = 0;
        // Foto del estado cada 10 s.
        if (cuadrosDiag % 600 == 0 && !fotoLista.load(std::memory_order_acquire))
        {
            con->Diagnostico(fotoTexto, sizeof fotoTexto);
            fotoLista.store(true, std::memory_order_release);
        }
    }

    if (!problema) return;
    Registrar("PROBLEMA DETECTADO: %s", motivo);
    if (informesAuto < 3 && !informeListo.load(std::memory_order_acquire))
    {
        informesAuto++;
        ArmarInforme(motivo, m);
        informeListo.store(true, std::memory_order_release);
    }
}

// Hilo principal: escribe lo que dejó el hilo de emulación.
static void DiagnosticoPendiente()
{
    if (fotoLista.load(std::memory_order_acquire))
    {
        RegistroFoto(fotoTexto);
        fotoLista.store(false, std::memory_order_release);
    }
    if (informeListo.load(std::memory_order_acquire))
    {
        RegistroInforme(informeMotivo, informeTexto, informeImagen, informeEstado, informeEstadoN, NULL, 0);
        informeListo.store(false, std::memory_order_release);
    }
}

// Si escribir en /data falla dentro del encierro del sistema, se reintenta con el acceso completo de GoldHEN (solo desde
// el hilo principal, con la emulación en pausa).
static bool ConAccesoSiHaceFalta(bool (*f)(void* u), void* u)
{
    if (f(u)) return true;
    AbrirAccesoCompleto("snes", true);
    char carpeta[128];
    CarpetaGuardados(carpeta, sizeof carpeta);
    carpetaLista = CrearCarpetas(carpeta);
    bool ok = f(u);
    CerrarAccesoCompleto();
    return ok;
}

static void CargarSram()
{
    if (!con->SramTam()) return;
    char r[300];
    snprintf(r, sizeof r, "%s.srm", base);
    uint32_t n = 0;
    uint8_t* d = LeerArchivo(r, &n);
    if (!d) return;
    memcpy(con->SramDatos(), d, n < con->SramTam() ? n : con->SramTam());
    free(d);
    con->SramCambiada();   // lo recién cargado no cuenta como cambio
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
static unsigned cuadrosHistoria;

static void* HiloEmulacion(void*)
{
    int visto = 0, escribir = 0;
    for (;;)
    {
        int p = pedidos.load(std::memory_order_acquire);
        if (p == visto) { Dormir(1); continue; }
        visto = p;
        con->Mando(0, botones.load(std::memory_order_relaxed));
        con->Mando(1, botones2.load(std::memory_order_relaxed));
        con->MandoConectado(1, hayJugador2.load(std::memory_order_relaxed));
        if (rebobinando.load(std::memory_order_relaxed) && hayHistoria)
        {
            // Rebobinar: un paso atrás (2 cuadros) por cuadro mostrado, sin sonido. Se corre un cuadro para tener la
            // imagen de ese momento.
            RebobinarAtras();
            Correr(escribir);
        }
        else
        {
            // Normal: 1 cuadro. Avance rápido: hasta 'velocidad' cuadros mientras entren en el tiempo de uno (12 ms),
            // sin sonido (se oiría entrecortado).
            int vel = velocidad.load(std::memory_order_relaxed);
            double t0 = Segundos();
            for (int k = 0; k < vel; k++)
            {
                double tc = Segundos();
                int n = Correr(escribir);
                if (vel == 1) { AudioJuego(audio, n); Vigilar(escribir, (Segundos() - tc) * 1000); }
                // Batería: se escribe 1,5 s después del último cambio (los juegos guardan varios bytes seguidos).
                if (con->SramCambiada()) contadorSram = 90;
                else if (contadorSram > 0 && --contadorSram == 0) GuardarSram();
                // Historia para rebobinar: un estado cada 2 cuadros.
                if (hayHistoria && (++cuadrosHistoria & 1) == 0) RebobinarCapturar();
                if (k + 1 < vel && Segundos() - t0 > .012) break;
            }
        }
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

static uint16_t ABotonesSnes(uint32_t a)
{
    uint16_t b = 0;
    if (a & B_CRUZ) b |= SB_B;
    if (a & B_CIRCULO) b |= SB_A;
    if (a & B_CUADRADO) b |= SB_Y;
    if (a & B_TRIANGULO) b |= SB_X;
    if (a & B_L1) b |= SB_L;
    if (a & B_R1) b |= SB_R;
    if (a & B_ARRIBA) b |= SB_ARRIBA;
    if (a & B_ABAJO) b |= SB_ABAJO;
    if (a & B_IZQ) b |= SB_IZQ;
    if (a & B_DER) b |= SB_DER;
    if (a & B_OPTIONS) b |= SB_START;
    if (a & B_TACTIL) b |= SB_SELECT;
    // Consolas de dos botones (NES, Game Boy...): cuadrado y triángulo también sirven de B y A.
    if (con->sistema != SIS_SNES)
    {
        if (a & B_CUADRADO) b |= SB_B;
        if (a & B_TRIANGULO) b |= SB_A;
    }
    return b;
}

// La historia de rebobinado se reinicia cuando el juego salta a otro momento (abrir, cargar estado, reiniciar).
static void NuevaHistoria()
{
    hayHistoria = General().rebobinar && RebobinarIniciar();
    cuadrosHistoria = 0;
}

// ---------- imagen: proporción y filtros ----------
// La proporción y el filtro que se usan: los propios del juego si los tiene, si no los generales (ver ajustes.h).
// Los del juego se guardan en <rom>.cfg junto a sus partidas (-1 = usar el general).
static int proporcion = PR_4_3, filtro = FI_SUAVE;
static int propJuego = -1, filtroJuego = -1;

static void AplicarAjustes()
{
    // Lo del juego; si no, lo de su consola; si no, lo general.
    const AjustesConsola& c = DeConsola(con ? con->sistema : SIS_SNES);
    proporcion = propJuego >= 0 ? propJuego : c.proporcion >= 0 ? c.proporcion : General().proporcion;
    filtro = filtroJuego >= 0 ? filtroJuego : c.filtro >= 0 ? c.filtro : General().filtro;
}

static void LeerAjustesJuego()
{
    propJuego = filtroJuego = -1;
    char ruta[300], txt[96] = "";
    snprintf(ruta, sizeof ruta, "%s.cfg", base);
    if (LeerTexto(ruta, txt, sizeof txt) > 0) sscanf(txt, "proporcion=%d filtro=%d", &propJuego, &filtroJuego);
    if (propJuego >= NUM_PROPORCIONES) propJuego = -1;
    if (filtroJuego >= NUM_FILTROS) filtroJuego = -1;
    AplicarAjustes();
}

static void GuardarAjustesJuego()
{
    char ruta[300], txt[64];
    snprintf(ruta, sizeof ruta, "%s.cfg", base);
    snprintf(txt, sizeof txt, "proporcion=%d filtro=%d\n", propJuego, filtroJuego);
    EscribirTexto(ruta, txt);
    AplicarAjustes();
}

// El rectángulo de la imagen en la pantalla según la proporción.
// 'iw' × 'ih' es el tamaño de la imagen de la consola. "Como la TV" usa la proporción de la consola (4:3 en las de
// televisor, la de la pantalla en las portátiles); "píxeles cuadrados", la de la imagen tal cual.
static void Rectangulo(int iw, int ih, float& x, float& y, float& w, float& h)
{
    switch (proporcion)
    {
    case PR_8_7: h = ALTO; w = ALTO * (float)iw / ih; break;
    case PR_ENTERO:
    {
        int k = ALTO / ih;
        if (k * iw > ANCHO) k = ANCHO / iw;
        if (k < 1) k = 1;
        w = (float)iw * k; h = (float)ih * k;
        break;
    }
    case PR_16_9: w = ANCHO; h = ALTO; break;
    default:
    {
        float asp = con->Aspecto ? con->Aspecto() : 0;
        if (asp <= 0 || !EsArcade(con->sistema)) asp = con->aspecto;   // la tabla de consolas, salvo en el arcade
        h = ALTO; w = ALTO * asp;
        break;
    }
    }
    if (w > ANCHO) { h = h * ANCHO / w; w = ANCHO; }
    x = (ANCHO - w) / 2;
    y = (ALTO - h) / 2;
}

// Filtros: la imagen de 256 × 224 se prepara en un búfer más grande y el dibujo la agranda con filtro suave.
//   Nítido suavizado: agrandada ×4 al píxel más cercano (bordes nítidos, sin el temblor del filtro "nítido").
//   Líneas de TV: el doble de filas, con las impares oscuras (como las líneas de un televisor de tubo).
//   CRT: además, cada píxel en 3 columnas que resaltan el rojo, el verde y el azul (la máscara del tubo).
static const int PROCESADO_MAX = 640 * 480 * 6;   // la imagen más grande (640 × 480) con el CRT (× 3 × 2)
static uint32_t procesado[PROCESADO_MAX];
static int procesadoDe = -1, procesadoFiltro = -1;

static inline uint32_t Escalar(uint32_t c, int num)   // c × num / 256 por componente
{
    uint32_t r = ((c >> 16) & 255) * num >> 8, g = ((c >> 8) & 255) * num >> 8, b = (c & 255) * num >> 8;
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    return r << 16 | g << 8 | b;
}

// HD: agrandada ×2 con un escalado de bordes al estilo xBR (el "HD" de otros emuladores): en cada esquina de cada píxel
// compara las dos diagonales posibles con sus vecinos (una ventana de 5 × 5) y, si el borde va en diagonal, mezcla la
// esquina con el color del otro lado. Las zonas planas y los bordes rectos quedan igual; las diagonales y las curvas de
// los sprites se redondean. Después el dibujo la agranda con filtro suave. Se reparte por filas entre los hilos.
static inline int Distancia(uint32_t a, uint32_t b)
{
    int dr = (int)((a >> 16) & 255) - (int)((b >> 16) & 255), dg = (int)((a >> 8) & 255) - (int)((b >> 8) & 255),
        db = (int)(a & 255) - (int)(b & 255);
    return (dr < 0 ? -dr : dr) * 2 + (dg < 0 ? -dg : dg) * 4 + (db < 0 ? -db : db);
}

static inline uint32_t Mitad(uint32_t a, uint32_t b) { return ((a & 0xFEFEFE) >> 1) + ((b & 0xFEFEFE) >> 1); }

struct TrabajoHD { const uint32_t* src; int sw, sh; uint32_t* dst; };

static void FilasHD(int parte, int partes, void* arg)
{
    const TrabajoHD& t = *(const TrabajoHD*)arg;
    const int sw = t.sw, sh = t.sh, dw = sw * 2;
    auto P = [&](int x, int y) {
        x = x < 0 ? 0 : x >= sw ? sw - 1 : x;
        y = y < 0 ? 0 : y >= sh ? sh - 1 : y;
        return t.src[y * sw + x];
    };
    for (int y = sh * parte / partes; y < sh * (parte + 1) / partes; y++)
        for (int x = 0; x < sw; x++)
        {
            uint32_t e = P(x, y);
            for (int sy = -1; sy <= 1; sy += 2)
                for (int sx = -1; sx <= 1; sx += 2)
                {
                    // Hacia la esquina (sx, sy): f al costado, h arriba o abajo, i en diagonal; el resto alrededor.
                    uint32_t f = P(x + sx, y), hh = P(x, y + sy), i = P(x + sx, y + sy), c = P(x + sx, y - sy),
                             g = P(x - sx, y + sy), b = P(x, y - sy), d = P(x - sx, y), f4 = P(x + 2 * sx, y),
                             h5 = P(x, y + 2 * sy), i4 = P(x + 2 * sx, y + sy), i5 = P(x + sx, y + 2 * sy);
                    uint32_t out = e;
                    if (e != f && e != hh)
                    {
                        int wd1 = Distancia(e, c) + Distancia(e, g) + Distancia(i, f4) + Distancia(i, h5) + 4 * Distancia(hh, f);
                        int wd2 = Distancia(hh, d) + Distancia(hh, i5) + Distancia(f, i4) + Distancia(f, b) + 4 * Distancia(e, i);
                        if (wd1 < wd2) out = Mitad(e, Distancia(e, f) <= Distancia(e, hh) ? f : hh);
                    }
                    t.dst[(y * 2 + (sy > 0)) * dw + x * 2 + (sx > 0)] = out;
                }
        }
}

static void Filtrar(const uint32_t* src, int sw, int sh, int cual, int& w, int& h)
{
    if (filtro == FI_HD)
    {
        w = sw * 2; h = sh * 2;
        if (procesadoDe == cual && procesadoFiltro == filtro) return;
        TrabajoHD t = {src, sw, sh, procesado};
        EnParalelo(FilasHD, &t);
    }
    else if (filtro == FI_NITIDO_SUAVE)
    {
        // Agrandada ×4, o menos si la imagen es grande.
        int e = 4;
        while (e > 1 && sw * e * sh * e > PROCESADO_MAX) e--;
        w = sw * e; h = sh * e;
        if (procesadoDe == cual && procesadoFiltro == filtro) return;
        for (int y = 0; y < sh; y++)
        {
            uint32_t* fila = procesado + (y * e) * w;
            for (int x = 0; x < sw; x++)
            {
                uint32_t c = src[y * sw + x];
                for (int k = 0; k < e; k++) fila[x * e + k] = c;
            }
            for (int k = 1; k < e; k++) memcpy(fila + k * w, fila, w * 4);
        }
    }
    else if (filtro == FI_LINEAS)
    {
        w = sw; h = sh * 2;
        if (procesadoDe == cual && procesadoFiltro == filtro) return;
        for (int y = 0; y < sh; y++)
            for (int x = 0; x < sw; x++)
            {
                uint32_t c = src[y * sw + x];
                procesado[(y * 2) * w + x] = Escalar(c, 280);
                procesado[(y * 2 + 1) * w + x] = Escalar(c, 110);
            }
    }
    else
    {
        w = sw * 3; h = sh * 2;
        if (procesadoDe == cual && procesadoFiltro == filtro) return;
        for (int y = 0; y < sh; y++)
            for (int x = 0; x < sw; x++)
            {
                uint32_t c = src[y * sw + x];
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

// ---------- marco decorativo (bezel) ----------
// Una imagen del tamaño de la pantalla que se ve en las franjas que deja libres la imagen del juego (en lugar del
// negro). Se busca al abrir el juego: bezels/<consola>/<juego>.png, bezels/<consola>/default.png o bezels/<consola>.png,
// en /data/allesiu y en los USB (también .jpg).
static uint32_t* bezel;
static int bezelW, bezelH;

static void BuscarBezel(const char* ruta)
{
    free(bezel);
    bezel = NULL;
    if (!General().bezels) return;
    const char* n = strrchr(ruta, '/');
    n = n ? n + 1 : ruta;
    char nombre[200];
    snprintf(nombre, sizeof nombre, "%s", n);
    char* e = strrchr(nombre, '.');
    if (e) *e = 0;
    static const char* const RAICES[] = {CARPETA_DATOS, "/mnt/usb0", "/mnt/usb1", "/mnt/usb2", "/mnt/usb3"};
    static const char* const EXT[] = {"png", "jpg"};
    AbrirAccesoCompleto("snes");
    for (int forma = 0; forma < 3 && !bezel; forma++)
        for (const char* r : RAICES)
            for (const char* x : EXT)
            {
                if (bezel) break;
                char p[600];
                if (forma == 0) snprintf(p, sizeof p, "%s/bezels/%s/%s.%s", r, con->carpeta, nombre, x);
                else if (forma == 1) snprintf(p, sizeof p, "%s/bezels/%s/default.%s", r, con->carpeta, x);
                else snprintf(p, sizeof p, "%s/bezels/%s.%s", r, con->carpeta, x);
                if (ExisteArchivo(p)) bezel = CargarImagen(p, &bezelW, &bezelH);
            }
    CerrarAccesoCompleto();
}

static void DibujarImagen()
{
    int l = listo.load(std::memory_order_acquire);
    int iw = l >= 0 ? marcoW[l] : SNES_ANCHO, ih = l >= 0 ? marcoH[l] : SNES_ALTO;
    float x, y, w, h;
    Rectangulo(iw, ih, x, y, w, h);
    if (bezel && (x > 1 || y > 1)) Imagen(bezel, bezelW, bezelH, 0, 0, ANCHO, ALTO, 1, true);   // el marco en las franjas
    else
    {
        if (x > 0) { Caja(0, 0, x + 1, ALTO, 0, 0x000000, 1); Caja(x + w - 1, 0, ANCHO - x - w + 1, ALTO, 0, 0x000000, 1); }
        if (y > 0) { Caja(0, 0, ANCHO, y + 1, 0, 0x000000, 1); Caja(0, y + h - 1, ANCHO, ALTO - y - h + 1, 0, 0x000000, 1); }
    }
    if (l < 0) { Caja(x, y, w, h, 0, 0x000000, 1); return; }
    if (filtro == FI_SUAVE || filtro == FI_NITIDO)
    {
        Imagen(marcos[l], iw, ih, x, y, w, h, 1, filtro == FI_SUAVE);
        return;
    }
    // El búfer procesado se rehace solo cuando hay un cuadro nuevo (cuenta de cuadros terminados).
    int pw, ph;
    Filtrar(marcos[l], iw, ih, hechos.load(std::memory_order_acquire), pw, ph);
    Imagen(procesado, pw, ph, x, y, w, h, 1, true);
}

// ---------- menú de pausa ----------
enum { OP_CONTINUAR, OP_GUARDAR, OP_CARGAR, OP_PROPORCION, OP_FILTRO, OP_GENERAL, OP_TRUCOS, OP_REINICIAR, OP_SALIR, NUM_OPCIONES };
static const char* const OPCIONES[NUM_OPCIONES] = {"Continuar", "Guardar estado", "Cargar estado", "Proporción", "Filtro",
                                                   "Usar la configuración general", "Trucos",
                                                   "Reiniciar", "Volver a la biblioteca"};
static bool viendoTrucos;   // la lista de trucos del juego (desde el menú de pausa)
static int selTruco;
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

// Ranuras bloqueadas (no se pueden sobrescribir): <base>.bloqueos con un 0 o un 1 por ranura.
static bool bloqueada[RANURAS];
static void LeerBloqueos()
{
    char ruta[300], t[16] = "";
    snprintf(ruta, sizeof ruta, "%s.bloqueos", base);
    LeerTexto(ruta, t, sizeof t);
    for (int i = 0; i < RANURAS; i++) bloqueada[i] = t[i] == '1';
}
static void CambiarBloqueo(int r)
{
    bloqueada[r] = !bloqueada[r];
    char ruta[300], t[RANURAS + 2];
    for (int i = 0; i < RANURAS; i++) t[i] = bloqueada[i] ? '1' : '0';
    t[RANURAS] = '\n';
    t[RANURAS + 1] = 0;
    snprintf(ruta, sizeof ruta, "%s.bloqueos", base);
    EscribirTexto(ruta, t);
}

static void GuardarEstado(int r)
{
    if (bloqueada[r]) { Avisar("Esa ranura está bloqueada (□ la desbloquea)", C_AVISO); return; }
    uint32_t tam = con->EstadoTam();
    uint8_t* d = (uint8_t*)malloc(tam);
    uint32_t n = d ? con->GuardarEstado(d, tam) : 0;
    if (!n) { free(d); Avisar("No se pudo armar el estado (sin memoria)", C_MAL); Registrar("ERROR: sin memoria para el estado (%u bytes)", tam); return; }
    char ruta[300];
    snprintf(ruta, sizeof ruta, "%s.st%d", base, r);
    EscrituraEstado e = {ruta, d, n};
    bool ok = ConAccesoSiHaceFalta(EscribirEstado, &e);
    free(d);
    if (ok)
    {
        // Miniatura: la imagen actual achicada a 128 × 112.
        int l = listo.load(std::memory_order_acquire);
        if (l >= 0)
        {
            Reducir(marcos[l], marcoW[l], marcoH[l], miniaturas[r], MINI_W, MINI_H);
            snprintf(ruta, sizeof ruta, "%s.st%d.img", base, r);
            EscribirArchivo(ruta, miniaturas[r], sizeof miniaturas[r]);
        }
        hayEstado[r] = true;
        char t[64];
        snprintf(t, sizeof t, "Estado guardado en la ranura %d", r + 1);
        Avisar(t, C_BIEN);
    }
    else if (!carpetaLista) { Avisar("No se pudo crear la carpeta de guardados en /data/allesiu", C_MAL); Registrar("ERROR: no se pudo crear la carpeta de guardados"); }
    else { Avisar("No se pudo escribir el archivo del estado en /data", C_MAL); Registrar("ERROR: no se pudo escribir %s", ruta); }
}

// Continuar donde quedé: un estado aparte (<base>.continuar, no ocupa las ranuras) que se guarda al salir del juego y se
// carga al abrirlo (si está activado en la configuración).
static void GuardarContinuar()
{
    uint32_t tam = con->EstadoTam();
    uint8_t* d = tam ? (uint8_t*)malloc(tam) : NULL;
    uint32_t n = d ? con->GuardarEstado(d, tam) : 0;
    if (n)
    {
        char ruta[300];
        snprintf(ruta, sizeof ruta, "%s.continuar", base);
        EscrituraEstado e = {ruta, d, n};
        if (!ConAccesoSiHaceFalta(EscribirEstado, &e)) Registrar("ERROR: no se pudo guardar el estado para continuar");
    }
    free(d);
}

static void CargarContinuar()
{
    char ruta[300];
    snprintf(ruta, sizeof ruta, "%s.continuar", base);
    uint32_t n = 0;
    uint8_t* d = LeerArchivo(ruta, &n);
    if (!d) return;
    bool ok = con->CargarEstado(d, n);
    free(d);
    if (ok) { AudioJuegoVaciar(); NuevaHistoria(); Avisar("Continuando donde quedaste", C_BIEN); }
}

static bool CargarEstado(int r)
{
    char ruta[300];
    snprintf(ruta, sizeof ruta, "%s.st%d", base, r);
    uint32_t n = 0;
    uint8_t* d = LeerArchivo(ruta, &n);
    if (!d) { Avisar("La ranura está vacía", C_AVISO); return false; }
    Registrar("Estado cargado de la ranura %d", r + 1);
    bool ok = con->CargarEstado(d, n);
    free(d);
    if (!ok) { Avisar("El estado es de otra versión o está dañado", C_MAL); Registrar("ERROR: el estado %s no se pudo cargar (%u bytes)", ruta, n); return false; }
    AudioJuegoVaciar();
    NuevaHistoria();
    char t[64];
    snprintf(t, sizeof t, "Estado de la ranura %d cargado", r + 1);
    Avisar(t, C_BIEN);
    return true;
}

static void Salir()
{
    // Cuánto tarda cada paso (al registro si la salida se nota: más de medio segundo).
    double t0 = Segundos(), t[4];
    abierto = false;
    menu = false;
    EsperarHilo();
    t[0] = Segundos();
    // Si el juego no llegó a cargar (la pantalla de error), no hay partida que guardar ni núcleo abierto.
    if (!error[0] && !ConAccesoSiHaceFalta([](void*) { return GuardarSram(); }, NULL)) Registrar("ERROR: no se pudo guardar la partida (batería) al salir");
    if (!error[0] && General().continuar) GuardarContinuar();
    contadorSram = 0;
    t[1] = Segundos();
    DiagnosticoPendiente();
    if (cuadrosTotal)
        Registrar("Juego cerrado: %.0f s jugados, %u cuadros, %.2f ms por cuadro en promedio (máximo %.1f), %u segundos lentos",
                  Segundos() - inicioPartida, cuadrosTotal, tiempoSumaTotal / cuadrosTotal, tiempoMaxTotal, segundosLentos);
    RegistroFinJuego();
    t[2] = Segundos();
    con->Descargar();
    free(bezel);
    bezel = NULL;
    t[3] = Segundos();
    if (t[3] - t0 > .5)
        Registrar("Salir del juego tardó %.1f s: esperar la emulación %.1f, guardar la partida %.1f, diagnóstico %.1f, cerrar el "
                  "emulador %.1f", t[3] - t0, t[0] - t0, t[1] - t[0], t[2] - t[1], t[3] - t[2]);
    Sonar(SON_ATRAS);
}

static void MenuEntrada(const Mando& m)
{
    // La lista de trucos: ↑ ↓ eligen, X activa o desactiva (con la emulación quieta), ○ vuelve al menú.
    if (viendoTrucos)
    {
        int n = TrucosCuantos();
        if (m.pulsados & (B_CIRCULO | B_OPTIONS)) { viendoTrucos = false; Sonar(SON_ATRAS); return; }
        if (n && (m.pulsados & B_ARRIBA)) { selTruco = (selTruco + n - 1) % n; Sonar(SON_MOVER); }
        if (n && (m.pulsados & B_ABAJO)) { selTruco = (selTruco + 1) % n; Sonar(SON_MOVER); }
        if (n && (m.pulsados & B_CRUZ))
        {
            EsperarHilo();
            TrucoCambiar(selTruco);
            Sonar(TrucoDe(selTruco)->activo ? SON_ACEPTAR : SON_ATRAS);
        }
        return;
    }
    if (m.pulsados & B_ARRIBA) { sel = (sel + NUM_OPCIONES - 1) % NUM_OPCIONES; Sonar(SON_MOVER); }
    if (m.pulsados & B_ABAJO) { sel = (sel + 1) % NUM_OPCIONES; Sonar(SON_MOVER); }
    // ← → cambia el valor de la opción elegida: la ranura, la proporción o el filtro.
    int d = (m.pulsados & B_DER) ? 1 : (m.pulsados & B_IZQ) ? -1 : 0;
    if (d && (sel == OP_GUARDAR || sel == OP_CARGAR)) { ranura = (ranura + RANURAS + d) % RANURAS; Sonar(SON_MOVER); }
    // Proporción y filtro cambiados acá valen solo para este juego.
    if (d && sel == OP_PROPORCION) { propJuego = (proporcion + NUM_PROPORCIONES + d) % NUM_PROPORCIONES; GuardarAjustesJuego(); Sonar(SON_MOVER); }
    if (d && sel == OP_FILTRO) { filtroJuego = (filtro + NUM_FILTROS + d) % NUM_FILTROS; GuardarAjustesJuego(); Sonar(SON_MOVER); }
    if ((m.pulsados & B_CUADRADO) && (sel == OP_GUARDAR || sel == OP_CARGAR))
    {
        CambiarBloqueo(ranura);
        Avisar(bloqueada[ranura] ? "Ranura bloqueada: no se puede sobrescribir" : "Ranura desbloqueada", C_TEXTO);
        Sonar(SON_MOVER);
        return;
    }
    if (m.pulsados & (B_CIRCULO | B_OPTIONS)) { menu = false; Sonar(SON_ATRAS); return; }
    if (!(m.pulsados & B_CRUZ)) return;
    switch (sel)
    {
    case OP_CONTINUAR: menu = false; Sonar(SON_ACEPTAR); break;
    case OP_PROPORCION: propJuego = (proporcion + 1) % NUM_PROPORCIONES; GuardarAjustesJuego(); Sonar(SON_MOVER); break;
    case OP_FILTRO: filtroJuego = (filtro + 1) % NUM_FILTROS; GuardarAjustesJuego(); Sonar(SON_MOVER); break;
    case OP_GENERAL:
        if (propJuego < 0 && filtroJuego < 0) { Sonar(SON_ERROR); break; }
        propJuego = filtroJuego = -1;
        GuardarAjustesJuego();
        Avisar("Este juego usa ahora la configuración general", C_BIEN);
        Sonar(SON_ACEPTAR);
        break;
    case OP_GUARDAR: GuardarEstado(ranura); Sonar(SON_ACEPTAR); menu = false; break;
    case OP_CARGAR: if (CargarEstado(ranura)) { Sonar(SON_ACEPTAR); menu = false; } else Sonar(SON_ERROR); break;
    case OP_TRUCOS:
        if (TrucosCuantos()) { viendoTrucos = true; selTruco = 0; Sonar(SON_ACEPTAR); }
        else { Avisar("Sin trucos: pon su archivo .cht en trucos/<consola>/ del USB", C_TEXTO_2); Sonar(SON_ERROR); }
        break;
    case OP_REINICIAR: Registrar("Juego reiniciado desde el menú"); con->Reiniciar(); TrucosAplicar(); AudioJuegoVaciar(); NuevaHistoria(); Avisar("Juego reiniciado", C_TEXTO); Sonar(SON_ACEPTAR); menu = false; break;
    case OP_SALIR: Salir(); break;
    }
}

static void MenuDibujar(float a)
{
    if (a <= .01f) return;
    // Mientras se elige proporción o filtro, el juego se ve casi sin oscurecer a la derecha (vista previa en vivo).
    bool vistaPrevia = sel == OP_PROPORCION || sel == OP_FILTRO || sel == OP_GENERAL;
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
        if (i == OP_GUARDAR || i == OP_CARGAR) snprintf(t, sizeof t, "%s   « %d »%s", OPCIONES[i], ranura + 1, bloqueada[ranura] ? "  (bloqueada)" : "");
        else snprintf(t, sizeof t, "%s", OPCIONES[i]);
        if (i == OP_PROPORCION || i == OP_FILTRO)
        {
            // El valor que usa el juego, y debajo de dónde sale: la configuración general o la propia del juego.
            Texto(F_SUBTITULO, OPCIONES[i], x, y0 + i * paso, i == sel ? C_TEXTO : C_TEXTO_2, a);
            char v[64];
            snprintf(v, sizeof v, "« %s »", i == OP_PROPORCION ? PROPORCIONES[proporcion] : FILTROS[filtro]);
            Texto(F_CUERPO_M, v, x + 580, y0 + i * paso - 6, i == sel ? C_ACENTO : C_TEXTO_3, a, 2);
            bool propio = i == OP_PROPORCION ? propJuego >= 0 : filtroJuego >= 0;
            Texto(F_PEQUENA, propio ? "Este juego" : "General", x + 580, y0 + i * paso + 24, propio ? C_SNES_B : C_TEXTO_3, a, 2);
            continue;
        }
        if (i == OP_TRUCOS)
        {
            int nt = TrucosCuantos(), act = 0;
            for (int k = 0; k < nt; k++) act += TrucoDe(k)->activo;
            Texto(F_SUBTITULO, OPCIONES[i], x, y0 + i * paso, nt ? (i == sel ? C_TEXTO : C_TEXTO_2) : C_TEXTO_3, nt ? a : a * .6f);
            char v[48];
            if (nt) snprintf(v, sizeof v, act ? "%d de %d activos" : "%d", act ? act : nt, nt);
            else snprintf(v, sizeof v, "No hay");
            Texto(F_PEQUENA, v, x + 580, y0 + i * paso + 8, act ? C_SNES_B : C_TEXTO_3, a, 2);
            continue;
        }
        if (i == OP_GENERAL)
        {
            bool hay = propJuego >= 0 || filtroJuego >= 0;   // solo tiene efecto si el juego tiene configuración propia
            Texto(F_SUBTITULO, OPCIONES[i], x, y0 + i * paso, hay ? (i == sel ? C_TEXTO : C_TEXTO_2) : C_TEXTO_3, hay ? a : a * .6f);
            continue;
        }
        Texto(F_SUBTITULO, t, x, y0 + i * paso, i == sel ? C_TEXTO : C_TEXTO_2, a);
    }

    // Ranura elegida: miniatura (o vacía) y las 4 ranuras abajo (se oculta durante la vista previa de la imagen).
    a *= 1 - transparencia;
    if (a <= .01f) return;
    float pw = 768, ph = 750, px = ANCHO - MARGEN - pw - 40 + (1 - a) * 60, py = 120;
    Panel(px, py, pw, ph, 24, a);
    if (viendoTrucos)
    {
        // Los trucos del juego (de su archivo .cht), con su estado; la lista se desplaza con la selección.
        Texto(F_SUBTITULO, "Trucos", px + 32, py + 30, C_TEXTO, a);
        int nt = TrucosCuantos();
        const int filas = 13;
        int primero = selTruco - filas / 2;
        if (primero > nt - filas) primero = nt - filas;
        if (primero < 0) primero = 0;
        for (int k = primero; k < nt && k < primero + filas; k++)
        {
            const Truco* tr = TrucoDe(k);
            float fy = py + 90 + (k - primero) * 46;
            bool el = k == selTruco;
            if (el) Caja(px + 16, fy - 6, pw - 32, 42, 12, C_ACENTO, .22f * a);
            Caja(px + 32, fy + 4, 44, 24, 12, tr->activo ? C_BIEN : 0xFFFFFF, (tr->activo ? .9f : .15f) * a);
            Circulo(px + (tr->activo ? 64 : 44), fy + 16, 9, 0xFFFFFF, a);
            char corto[96];
            Texto(F_CUERPO, Acortar(F_CUERPO, tr->nombre, pw - 140, corto, sizeof corto), px + 92, fy, el ? C_TEXTO : C_TEXTO_2, a);
        }
        int b2[2] = {BM_CRUZ, BM_CIRCULO};
        const char* tx2[2] = {"Activar o desactivar", "Volver"};
        Pistas(b2, tx2, 2, a);
        return;
    }
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
        if (bloqueada[i]) Aro(cx, cy, i == ranura ? 17 : 13, 2.5f, C_SNES_B, a);   // bloqueada: un aro dorado
    }

    int b[2] = {BM_CRUZ, BM_CIRCULO};
    const char* tx[2] = {"Aceptar", "Volver al juego"};
    Pistas(b, tx, 2, a);
    Texto(F_PEQUENA, "← → cambia la ranura, la proporción o el filtro (solo para este juego)   ·   □ bloquea la ranura", MARGEN, ALTO - 44, C_TEXTO_3, a);
}

// ---------- pantalla ----------
bool JuegoAbrir(const char* ruta, const char* nom, int sistema)
{
    snprintf(nombre, sizeof nombre, "%s", nom);
    error[0] = 0;

    procesadoDe = -1;
    if (!hiloCreado) hiloCreado = Lanzar(HiloEmulacion, NULL, "emulacion");
    EsperarHilo();
    // Los cuadros en negro: si el núcleo nuevo empieza con cuadros repetidos (N64 lo hace), se repetiría la imagen del
    // juego anterior.
    memset(marcos, 0, sizeof marcos);
    // La rom está en el USB: solo se ve afuera del encierro del sistema (igual que al buscar). Se carga entera a memoria,
    // así que después se puede volver a entrar.
    // Una rom comprimida se saca a memoria y la consola sale del archivo de adentro.
    bool ok = false;
    int acceso = AbrirAccesoCompleto("snes", true);
    if (EsArcade(sistema))
    {
        // Arcade: FinalBurn Neo abre el zip por su ruta. Si falla, la explicación de qué le falta al set es más útil que
        // el error genérico.
        con = &ConsolaDe(sistema);
        char explicacion[160], rutaNucleo[600];
        int estado = ArcadeRevisar(ruta, explicacion);
        // Si el zip (o su juego principal, o la BIOS) tiene otro nombre, FinalBurn Neo recibe el nombre corto y la capa
        // de archivos abre el real (ver ArcadePrepararAlias).
        ArcadePrepararAlias(ruta, rutaNucleo, sizeof rutaNucleo);
        if (!con->presente) snprintf(error, sizeof error, "Esta versión de ALLESIU todavía no trae el emulador arcade.");
        // Si al set le faltan archivos propios, es de otra versión o falta la BIOS, ni se intenta: FinalBurn Neo abriría
        // igual, pero con su pantalla de error en inglés. (Un padre que falta sí se intenta: el núcleo lo busca también
        // en la carpeta de BIOS.)
        else if (estado == ARC_FALTAN_ARCHIVOS || estado == ARC_OTRA_VERSION || estado == ARC_FALTA_BIOS) snprintf(error, sizeof error, "%s", explicacion);
        else ok = con->Cargar(rutaNucleo, error);
        if (!ok && estado != ARC_COMPLETO && explicacion[0]) snprintf(error, sizeof error, "%s", explicacion);
    }
    else if (EsComprimido(ruta))
    {
        char interno[256];
        uint32_t tam = 0;
        uint8_t* datos = SacarDeComprimido(ruta, interno, sizeof interno, &tam, error);
        const Consola* c = datos ? ConsolaParaRuta(interno) : NULL;
        con = c ? c : &ConsolaDe(SIS_SNES);
        if (datos) ok = con->CargarMemoria(ruta, interno, datos, tam, error);
    }
    else
    {
        const Consola* c = ConsolaParaRuta(ruta);
        con = c ? c : &ConsolaDe(SIS_SNES);
        if (!con->presente) snprintf(error, sizeof error, "Esta versión de ALLESIU todavía no trae el emulador de %s.", con->nombreLargo);
        else ok = con->Cargar(ruta, error);
    }
    // Un juego del USB sin el acceso de GoldHEN no se ve: eso explica el error mejor que lo que dijo el emulador.
    if (!ok && acceso <= 0 && !strncmp(ruta, "/mnt/", 5))
        snprintf(error, sizeof error, "La aplicación no pudo ver el USB. %s", AccesoDetalle());
    CerrarAccesoCompleto();
    ConsolaActivar(con->sistema);
    ArmarBase(ruta);
    LeerAjustesJuego();
    viendoTrucos = false;
    LeerBloqueos();
    if (ok)
    {
        AbrirAccesoCompleto("snes");   // los trucos pueden estar en el USB
        TrucosAbrir(ruta, con->sistema, base);
        CerrarAccesoCompleto();
        TrucosAplicar();
        BuscarBezel(ruta);
    }
    // Diagnóstico: todo en cero para esta partida, y al registro qué se abrió.
    informesAuto = cuadrosDiag = segNegro = 0;
    negroAvisado = false;
    tiempoSuma = tiempoMax = tiempoMaxTotal = tiempoSumaTotal = 0;
    cuadrosMedidos = cuadrosTotal = segundosLentos = 0;
    inicioPartida = Segundos();
    fotoLista = false;
    informeListo = false;
    if (ok)
    {
        // Las primeras líneas del informe: la cabecera de la rom y sus chips.
        static char cab[1024];
        con->Diagnostico(cab, sizeof cab);
        char* fin = strstr(cab, "Cuadros emulados");
        if (fin) *fin = 0;
        int l = (int)strlen(cab);
        while (l > 0 && cab[l - 1] == '\n') cab[--l] = 0;
        Registrar("Juego abierto: %s\n%s", ruta, cab);
    }
    else Registrar("ERROR al abrir %s: %s", ruta, error);
    char memoria[160];
    MemoriaResumen(memoria, sizeof memoria);
    Registrar("Al abrir: %s", memoria);
    if (ok)
    {
        CargarSram();
        if (General().continuar) CargarContinuar();
        LeerMiniaturas();
        NuevaHistoria();
    }
    contadorSram = 0;
    if (con->sistema == SIS_SNES) SnesSuperFxVelocidad(General().superfx);
    AudioJuegoFrecuencia(ok ? con->Frecuencia() : 32000);
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

static bool jugador2;

// Indicador arriba a la izquierda mientras se acelera o rebobina.
static void DibujarVelocidad(const Mando& m)
{
    const char* t = NULL;
    char buf[48];
    if ((m.apretados & B_L2) && hayHistoria)
    {
        snprintf(buf, sizeof buf, "«« Rebobinando   %.0f s", RebobinarSegundos());
        t = buf;
    }
    else if (m.apretados & B_R2) t = "»» Avance rápido";
    if (!t || menu) return;
    float w = AnchoTexto(F_CUERPO_M, t) + 48;
    Caja(MARGEN - 40, 40, w, 52, 26, 0x0A0614, .78f);
    Texto(F_CUERPO_M, t, MARGEN - 16, 52, C_TEXTO, 1);
}

void JuegoCuadro(float t, float dt, const Mando& m)
{
    (void)t;
    bool combo = (m.apretados & B_L3) && (m.apretados & B_R3) && (m.pulsados & (B_L3 | B_R3));
    // La pantalla de "no se pudo cargar": se vuelve también con ○ o ✕ (no hay juego que pausar).
    if (abierto && error[0] && (combo || (m.pulsados & (B_CIRCULO | B_CRUZ)))) Salir();
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
    if (abierto) DiagnosticoPendiente();
    Suave(vis, abierto ? 1 : 0, 12, dt);
    Suave(visMenu, menu ? 1 : 0, 14, dt);
    Suave(selSuave, (float)sel, 18, dt);
    if (vis <= .01f) return;

    // Pedir el cuadro siguiente si el hilo ya terminó el anterior (si va atrasado, se muestra el último que tenga).
    // Jugador 2 (se busca su mando aunque el juego esté en pausa, para avisar cuando se conecta).
    bool con2 = false;
    uint32_t a2 = abierto ? LeerMando2(&con2) : 0;
    if (abierto && con2 != jugador2)
    {
        jugador2 = con2;
        hayJugador2.store(con2, std::memory_order_relaxed);
        Avisar(con2 ? "Jugador 2 conectado" : "Jugador 2 desconectado", con2 ? C_BIEN : C_AVISO);
    }
    if (abierto && !error[0] && !menu)
    {
        botones.store(ABotonesSnes(m.apretados), std::memory_order_relaxed);
        botones2.store(ABotonesSnes(a2), std::memory_order_relaxed);
        velocidad.store((m.apretados & B_R2) ? General().avance : 1, std::memory_order_relaxed);
        rebobinando.store((m.apretados & B_L2) != 0, std::memory_order_relaxed);
        if (hechos.load(std::memory_order_acquire) == pedidos.load(std::memory_order_acquire))
            pedidos.fetch_add(1, std::memory_order_release);
    }

    Opacidad(vis);
    if (error[0])
    {
        Caja(0, 0, ANCHO, ALTO, 0, 0x000000, 1);
        Brillo(ANCHO / 2, ALTO / 2 - 40, 200, C_MAL, .3f);
        Texto(F_TITULO, "No se pudo cargar el juego", ANCHO / 2, ALTO / 2 - 80, C_TEXTO, 1, 1);
        // Centrado si entra en una línea; si no, en un párrafo de 800 de ancho.
        float w = AnchoTexto(F_CUERPO, error);
        if (w <= 800) Texto(F_CUERPO, error, ANCHO / 2, ALTO / 2 + 10, C_TEXTO_2, 1, 1);
        else Parrafo(F_CUERPO, error, ANCHO / 2 - 400, ALTO / 2, 800, C_TEXTO_2);
        static const int boton[1] = {BM_CIRCULO};
        static const char* const textoBoton[1] = {"Volver"};
        PistasEn(boton, textoBoton, 1, 1, ALTO - 120);
        Opacidad(1);
        return;
    }

    // Imagen de la consola con la proporción y el filtro elegidos (bandas negras alrededor si no llena la pantalla).
    DibujarImagen();

    // Ayuda al abrir (se va sola a los 4 s).
    tAyuda += dt;
    if (mostrarAyuda && tAyuda > 4) mostrarAyuda = false;
    if (mostrarAyuda)
    {
        float a = tAyuda < 3 ? 1 : (4 - tAyuda);
        Velo(0, ALTO - 70, ANCHO, 70, 0, 0x000000, 0, .6f * a);
        char txt[128];
        snprintf(txt, sizeof txt, "%s   ·   L3 + R3: menú   ·   R2: acelerar   ·   L2: rebobinar", nombre);
        Texto(F_CUERPO, txt, ANCHO / 2, ALTO - 50, C_TEXTO_2, a, 1);
    }

    DibujarVelocidad(m);
    MenuDibujar(visMenu);

    // Aviso breve (estado guardado, cargado...).
    tAviso += dt;
    if (tAviso < 2.5f)
    {
        float a = tAviso < .2f ? tAviso / .2f : tAviso > 2 ? (2.5f - tAviso) / .5f : 1;
        float w = AnchoTexto(F_CUERPO_M, aviso) + 64;
        Panel(ANCHO / 2 - w / 2, 36, w, 60, 30, a);
        Circulo(ANCHO / 2 - w / 2 + 30, 66, 7, avisoColor, a);
        Texto(F_CUERPO_M, aviso, ANCHO / 2 + 10, 50, C_TEXTO, a, 1);
    }
    Opacidad(1);
}
