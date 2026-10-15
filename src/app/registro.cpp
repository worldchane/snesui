// Ver registro.h. Las líneas se juntan en memoria (cualquier hilo, con un candado simple) y el hilo principal las
// agrega al archivo: el USB solo se ve con el acceso completo de GoldHEN, que se abre un momento para escribir.
#include "registro.h"

#include <atomic>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "../comun/plataforma.h"
#include "copiar.h"

static std::atomic_flag candado = ATOMIC_FLAG_INIT;
static char pendiente[16 * 1024];
static int nPendiente;
static char ultimas[6 * 1024];   // las líneas más recientes (anillo de texto), para los informes
static int nUltimas;
static int sesion;
static double ultimaEscritura;
static char carpeta[160];        // donde se escribe; vacío = buscarla de nuevo

static const char* const FOTO = "/data/snes-ps4/foto-en-juego.txt";
static const uint64_t MAX_REGISTRO = 2 * 1024 * 1024;

static void Bloquear() { while (candado.test_and_set(std::memory_order_acquire)) {} }
static void Soltar() { candado.clear(std::memory_order_release); }

void Registrar(const char* fmt, ...)
{
    char l[600];
    double t = Segundos();
    int n = snprintf(l, sizeof l, "[%d %02d:%02d:%02d] ", sesion, (int)(t / 3600), (int)(t / 60) % 60, (int)t % 60);
    va_list a;
    va_start(a, fmt);
    int k = vsnprintf(l + n, sizeof l - n - 1, fmt, a);
    va_end(a);
    n += k > 0 ? k : 0;
    if (n > (int)sizeof l - 2) n = sizeof l - 2;
    l[n++] = '\n';
    l[n] = 0;
    Bloquear();
    if (nPendiente + n < (int)sizeof pendiente) { memcpy(pendiente + nPendiente, l, n); nPendiente += n; }
    if (nUltimas + n >= (int)sizeof ultimas)
    {
        // Se descarta la mitad más vieja, cortando en un fin de línea.
        int corte = nUltimas / 2;
        while (corte < nUltimas && ultimas[corte - 1] != '\n') corte++;
        memmove(ultimas, ultimas + corte, nUltimas - corte);
        nUltimas -= corte;
    }
    if (n < (int)sizeof ultimas) { memcpy(ultimas + nUltimas, l, n); nUltimas += n; }
    Soltar();
}

int RegistroUltimas(char* out, int max)
{
    Bloquear();
    int n = nUltimas < max - 1 ? nUltimas : max - 1;
    memcpy(out, ultimas + nUltimas - n, n);
    Soltar();
    out[n] = 0;
    return n;
}

// La carpeta de diagnóstico: en el primer USB que haya, si no en /data. Llamar con el acceso completo abierto.
static bool BuscarCarpeta()
{
    char usb[160];
    if (CarpetaUsb(usb, sizeof usb))
    {
        char* barra = strrchr(usb, '/');   // CarpetaUsb devuelve <usb>/SNES
        if (barra) *barra = 0;
        snprintf(carpeta, sizeof carpeta, "%s/SNESUI-diagnostico", usb);
        if (CrearCarpetas(carpeta)) return true;
    }
    snprintf(carpeta, sizeof carpeta, "%s/diagnostico", CARPETA_DATOS);
    if (CrearCarpetas(carpeta)) return true;
    carpeta[0] = 0;
    return false;
}

static bool Agregar(const char* texto, int n)
{
    char ruta[200];
    snprintf(ruta, sizeof ruta, "%s/registro.txt", carpeta);
    uint64_t tam = 0;
    bool nuevo = !ExisteArchivo(ruta, &tam) || tam > MAX_REGISTRO;   // muy grande: se empieza de nuevo
    FILE* f = AbrirArchivo(ruta, nuevo ? "wb" : "ab");
    if (!f) return false;
    if (nuevo) fputs("Registro de diagnóstico de SNESUI. Pásale este archivo (y los informe-*) al desarrollador.\n\n", f);
    bool ok = fwrite(texto, 1, n, f) == (size_t)n;
    fclose(f);
    return ok;
}

static void Vaciar()
{
    static char copia[sizeof pendiente];
    Bloquear();
    int n = nPendiente;
    memcpy(copia, pendiente, n);
    nPendiente = 0;
    Soltar();
    if (!n) return;
    AbrirAccesoCompleto("snes");
    if (!carpeta[0] || !Agregar(copia, n))
        if (BuscarCarpeta()) Agregar(copia, n);   // el USB se pudo sacar o poner: se busca de nuevo
    CerrarAccesoCompleto();
}

void RegistroCuadro()
{
    // A lo más una escritura por segundo (pocas líneas por vez).
    double t = Segundos();
    if (t - ultimaEscritura < 1) return;
    Bloquear();
    bool hay = nPendiente > 0;
    Soltar();
    if (!hay) return;
    ultimaEscritura = t;
    Vaciar();
}

void RegistroIniciar(const char* version)
{
    // Número de sesión (cuántas veces se abrió el programa), para separar las líneas de cada vez.
    char t[32] = "";
    char ruta[160];
    snprintf(ruta, sizeof ruta, "%s/sesiones.txt", CARPETA_DATOS);
    if (LeerTexto(ruta, t, sizeof t) > 0) sscanf(t, "%d", &sesion);
    sesion++;
    snprintf(t, sizeof t, "%d\n", sesion);
    EscribirTexto(ruta, t);

    AbrirAccesoCompleto("snes");
    bool hay = BuscarCarpeta();
    CerrarAccesoCompleto();
    Registrar("===== SNESUI %s abierto (sesión %d). Registro en %s =====", version, sesion, hay ? carpeta : "(ninguna carpeta)");
    uint64_t libres = 0;
    if (EspacioLibre(CARPETA_DATOS, &libres)) Registrar("Espacio libre en la consola: %llu MB", (unsigned long long)(libres >> 20));

    // ¿Quedó la foto de un juego? Entonces el programa se cerró sin volver a la biblioteca (cuelgue, cierre del sistema
    // o el botón PS → cerrar aplicación).
    static char foto[16 * 1024];
    if (LeerTexto(FOTO, foto, sizeof foto) > 0 && foto[0])
    {
        Registrar("AVISO: la vez anterior el programa se cerró en medio de un juego (se colgó, se cerró desde el menú de la"
                  " PS4 o se apagó la consola). Último estado guardado:");
        // La foto va entera, en trozos (Registrar corta las líneas largas).
        for (char* p = foto; *p;)
        {
            char* fin = strchr(p, '\n');
            if (fin) *fin = 0;
            Registrar("    %s", p);
            if (!fin) break;
            p = fin + 1;
        }
        EscribirTexto(FOTO, "");
    }
    Vaciar();
}

void RegistroFoto(const char* texto) { EscribirTexto(FOTO, texto); }
void RegistroFinJuego() { EscribirTexto(FOTO, ""); }

// Captura en .bmp (24 bits, filas de abajo hacia arriba), sin ninguna biblioteca.
static bool EscribirBmp(const char* ruta, const uint32_t* img, int w, int h)
{
    FILE* f = AbrirArchivo(ruta, "wb");
    if (!f) return false;
    int fila = (w * 3 + 3) & ~3;
    uint32_t datos = fila * h, total = 54 + datos;
    uint8_t c[54] = {'B', 'M'};
    auto pon32 = [&](int o, uint32_t v) { c[o] = v; c[o + 1] = v >> 8; c[o + 2] = v >> 16; c[o + 3] = v >> 24; };
    pon32(2, total);
    pon32(10, 54);
    pon32(14, 40);
    pon32(18, w);
    pon32(22, h);
    c[26] = 1;
    c[28] = 24;
    pon32(34, datos);
    bool ok = fwrite(c, 1, 54, f) == 54;
    static uint8_t linea[4096];
    for (int y = h - 1; y >= 0 && ok; y--)
    {
        memset(linea, 0, fila);
        for (int x = 0; x < w; x++)
        {
            uint32_t p = img[y * w + x];
            linea[x * 3] = p;
            linea[x * 3 + 1] = p >> 8;
            linea[x * 3 + 2] = p >> 16;
        }
        ok = fwrite(linea, 1, fila, f) == (size_t)fila;
    }
    fclose(f);
    return ok;
}

bool RegistroInforme(const char* motivo, const char* texto, const uint32_t* imagen, const uint8_t* estado,
                     uint32_t nEstado, char* donde, int max)
{
    Vaciar();   // que el registro quede al día antes del informe
    AbrirAccesoCompleto("snes");
    bool ok = carpeta[0] || BuscarCarpeta();
    char ruta[220] = "";
    int num = 1;
    if (ok)
    {
        for (; num < 1000; num++)
        {
            snprintf(ruta, sizeof ruta, "%s/informe-%03d.txt", carpeta, num);
            if (!ExisteArchivo(ruta)) break;
        }
        FILE* f = AbrirArchivo(ruta, "wb");
        if (!f && BuscarCarpeta())
        {
            snprintf(ruta, sizeof ruta, "%s/informe-%03d.txt", carpeta, num);
            f = AbrirArchivo(ruta, "wb");
        }
        ok = f != NULL;
        if (f)
        {
            static char ult[sizeof ultimas];
            RegistroUltimas(ult, sizeof ult);
            fprintf(f, "Informe de diagnóstico de SNESUI (sesión %d, %.0f s desde que se abrió)\nMotivo: %s\n\n", sesion,
                    Segundos(), motivo);
            fputs(texto, f);
            fputs("\n\nÚltimas líneas del registro:\n", f);
            fputs(ult, f);
            fclose(f);
            char r2[220];
            if (imagen)
            {
                snprintf(r2, sizeof r2, "%s/informe-%03d.bmp", carpeta, num);
                EscribirBmp(r2, imagen, 256, 224);
            }
            if (estado && nEstado)
            {
                snprintf(r2, sizeof r2, "%s/informe-%03d.st", carpeta, num);
                FILE* e = AbrirArchivo(r2, "wb");
                if (e) { fwrite(estado, 1, nEstado, e); fclose(e); }
            }
        }
    }
    CerrarAccesoCompleto();
    if (donde) snprintf(donde, max, "%s", ok ? ruta : "");
    if (ok) Registrar("Informe guardado: %s (%s)", ruta, motivo);
    else Registrar("ERROR: no se pudo guardar el informe (%s)", motivo);
    return ok;
}
