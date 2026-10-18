// Prueba masiva de las consolas de la app (consola.cpp: la SNES propia y los núcleos libretro), con roms comprimidas o
// no. Por cada rom: la abre como la app, corre N cuadros apretando START de vez en cuando y anota si cargó, el tamaño de
// la imagen, si quedó de un solo color, las muestras de sonido y la velocidad. Guarda la última imagen en PNG.
//   probar_consola <rom> <cuadros>                      una rom: salida/consola.png y el resumen en pantalla
//   probar_consola @lista.txt <cuadros> <carpeta>      varias (una ruta por línea): <carpeta>/<n>.png y resumen.tsv
// Las rutas son de Windows (las de la app, /data y /mnt/usb0, se cambian por pc/data y pc/usb).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <chrono>
#include <string>
#include <vector>

#include "../src/app/consola.h"
#include "../src/comun/plataforma.h"
#include "../src/nucleo/snes.h"   // los bits del mando (SB_*), iguales para todas las consolas

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

extern double tiempoSimulado;

static uint32_t imagen[CONSOLA_MAX_ANCHO * CONSOLA_MAX_ALTO];
static int16_t audio[8192];

struct Resultado
{
    bool cargo;
    char consola[16], error[160];
    int ancho, alto, colores;
    long muestras;
    double cuadrosPorSegundo;
};

static void GuardarPng(const char* ruta, int w, int h)
{
    std::vector<unsigned char> rgb((size_t)w * h * 3);
    for (size_t i = 0; i < (size_t)w * h; i++)
    {
        rgb[i * 3] = (unsigned char)(imagen[i] >> 16);
        rgb[i * 3 + 1] = (unsigned char)(imagen[i] >> 8);
        rgb[i * 3 + 2] = (unsigned char)imagen[i];
    }
    stbi_write_png(ruta, w, h, 3, rgb.data(), w * 3);
}

static Resultado Probar(const char* ruta, int cuadros, const char* png)
{
    Resultado r = {};
    const Consola* c = NULL;
    if (EsComprimido(ruta))
    {
        char interno[256];
        uint32_t tam = 0;
        uint8_t* d = SacarDeComprimido(ruta, interno, sizeof interno, &tam, r.error);
        c = d ? ConsolaParaRuta(interno) : NULL;
        if (c && c->presente) r.cargo = c->CargarMemoria(ruta, interno, d, tam, r.error);
        else if (d) { free(d); snprintf(r.error, sizeof r.error, "sin consola para %s", interno); }
    }
    else
    {
        c = ConsolaParaRuta(ruta);
        // FORZAR=<sistema>: abrir con el núcleo de otra consola (para comparar núcleos, por ejemplo GB con mGBA).
        if (getenv("FORZAR")) c = &ConsolaDe(atoi(getenv("FORZAR")));
        if (!c || !c->presente) snprintf(r.error, sizeof r.error, "sin consola para esta extensión");
        else r.cargo = c->Cargar(ruta, r.error);
    }
    if (c) snprintf(r.consola, sizeof r.consola, "%s", c->nombre);
    if (!r.cargo) return r;
    ConsolaActivar(c->sistema);
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < cuadros; i++)
    {
        tiempoSimulado = i / 60.0;
        // START un momento cada 3 s (para pasar las pantallas de título), y A cada tanto.
        uint16_t b = 0;
        if (i % 180 < 6) b |= SB_START;
        if (i % 180 > 90 && i % 180 < 96) b |= SB_A;
        c->Mando(0, b);
        r.muestras += c->Cuadro(imagen, &r.ancho, &r.alto, audio, sizeof audio / 4);
    }
    double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    r.cuadrosPorSegundo = s > 0 ? cuadros / s : 0;
    // ¿Cuántos colores distintos hay? (1 = imagen plana: negra o vacía)
    std::vector<uint32_t> vistos;
    for (int i = 0; i < r.ancho * r.alto && vistos.size() < 8; i += 7)
    {
        bool esta = false;
        for (uint32_t v : vistos) esta |= v == imagen[i];
        if (!esta) vistos.push_back(imagen[i]);
    }
    r.colores = (int)vistos.size();
    if (png && r.ancho > 0 && r.alto > 0) GuardarPng(png, r.ancho, r.alto);
    c->Descargar();
    return r;
}

int main(int argc, char** argv)
{
    if (argc < 3) { printf("uso: probar_consola <rom | @lista.txt> <cuadros> [carpeta]\n"); return 1; }
    // probar_consola reconocer @lista.txt: cuánto tarda la biblioteca en saber la consola de cada comprimido.
    if (!strcmp(argv[1], "reconocer"))
    {
        FILE* lista = fopen(argv[2] + 1, "rb");
        if (!lista) return 1;
        char linea[1024];
        int n = 0, bien = 0;
        auto t0 = std::chrono::steady_clock::now();
        while (fgets(linea, sizeof linea, lista))
        {
            size_t l = strlen(linea);
            while (l && (linea[l - 1] == '\n' || linea[l - 1] == '\r')) linea[--l] = 0;
            if (!l) continue;
            char interno[256] = "";
            uint64_t tam = 0;
            int s = ConsolaDeComprimido(linea, interno, sizeof interno, &tam);
            n++;
            if (s >= 0) bien++;
            else printf("sin consola: %s\n", linea);
        }
        double seg = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        printf("%d comprimidos, %d reconocidos, %.2f s (%.1f ms cada uno)\n", n, bien, seg, n ? seg * 1000 / n : 0);
        return 0;
    }
    int cuadros = atoi(argv[2]);
    if (argv[1][0] != '@')
    {
        Resultado r = Probar(argv[1], cuadros, "pc/salida/consola.png");
        printf("%s\t%s\t%dx%d\tcolores %d\tsonido %ld\t%.0f cuadros/s\t%s\n", r.cargo ? "OK" : "FALLA", r.consola, r.ancho,
               r.alto, r.colores, r.muestras, r.cuadrosPorSegundo, r.error);
        return r.cargo ? 0 : 2;
    }
    const char* carpeta = argc > 3 ? argv[3] : "pc/salida/lote";
    std::string resumen = std::string(carpeta) + "/resumen.tsv";
    FILE* lista = fopen(argv[1] + 1, "rb");
    FILE* out = fopen(resumen.c_str(), "wb");
    if (!lista || !out) { printf("no se pudo abrir la lista o %s\n", resumen.c_str()); return 1; }
    fprintf(out, "rom\testado\tconsola\tancho\talto\tcolores\tmuestras\tcuadros_s\terror\n");
    char linea[1024];
    int n = 0, ok = 0, planas = 0;
    while (fgets(linea, sizeof linea, lista))
    {
        size_t l = strlen(linea);
        while (l && (linea[l - 1] == '\n' || linea[l - 1] == '\r')) linea[--l] = 0;
        if (!l) continue;
        char png[600];
        snprintf(png, sizeof png, "%s/%04d.png", carpeta, n);
        Resultado r = Probar(linea, cuadros, png);
        fprintf(out, "%s\t%s\t%s\t%d\t%d\t%d\t%ld\t%.0f\t%s\n", linea, r.cargo ? "OK" : "FALLA", r.consola, r.ancho, r.alto,
                r.colores, r.muestras, r.cuadrosPorSegundo, r.error);
        fflush(out);
        n++;
        ok += r.cargo;
        planas += r.cargo && r.colores <= 1;
        if (n % 50 == 0) printf("%d roms, %d abrieron, %d con imagen plana\n", n, ok, planas);
    }
    printf("total: %d roms, %d abrieron, %d con imagen plana al final\n", n, ok, planas);
    fclose(out);
    return 0;
}
