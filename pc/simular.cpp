// Simulador de PC: corre la interfaz real (src/app, src/comun) con un mando programado y guarda capturas PNG.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <chrono>
#include <thread>

#include "../src/app/biblioteca.h"
#include "../src/app/interfaz.h"
#include "../src/app/mudanza.h"
#include "../src/app/arcade.h"
#include "../src/comun/lienzo.h"
#include "../src/comun/pantalla.h"
#include "../src/comun/texto.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

extern double tiempoSimulado;
static uint32_t pantalla[ANCHO * ALTO];

bool PantallaIniciar() { LienzoIniciar(); return true; }
void PantallaComenzar() { LienzoComenzar(pantalla, ANCHO, false); }
void PantallaMostrar() { LienzoTerminar(); }
float MsDibujo() { return 0; }
float MsEntrega() { return 0; }

static void Guardar(const char* nombre)
{
    std::vector<unsigned char> rgb(ANCHO * ALTO * 3);
    for (int i = 0; i < ANCHO * ALTO; i++) { rgb[i * 3] = pantalla[i] >> 16; rgb[i * 3 + 1] = pantalla[i] >> 8; rgb[i * 3 + 2] = pantalla[i]; }
    char ruta[128];
    snprintf(ruta, sizeof ruta, "pc/salida/%s.png", nombre);
    stbi_write_png(ruta, ANCHO, ALTO, 3, rgb.data(), ANCHO * 3);
    printf("captura %s\n", ruta);
}

struct Paso { float t; uint32_t botones; const char* captura; };
static const uint32_t STICK_DER = 0x80000000u;   // en el guion: el stick derecho a la derecha durante ese cuadro

// Si se cae: dónde (desplazamiento desde la base, para buscarlo en pc/salida/simular.map).
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static LONG WINAPI AlCaer(EXCEPTION_POINTERS* e)
{
    fprintf(stderr, "CAIDA codigo %08lx desplazamiento %llx\n", e->ExceptionRecord->ExceptionCode,
            (unsigned long long)((char*)e->ExceptionRecord->ExceptionAddress - (char*)GetModuleHandleA(NULL)));
    fflush(stderr);
    return EXCEPTION_EXECUTE_HANDLER;
}

int main()
{
    SetUnhandledExceptionFilter(AlCaer);
    Mudanza();
    ArcadeIniciar();
    MudanzaOrdenarRoms();
    if (MudanzaResumen()[0]) printf("mudanza: %s\n", MudanzaResumen());
    IniciarTrabajadores(4);
    auto inicioReal = std::chrono::steady_clock::now();
    bool avisado = false;
    PantallaIniciar();
    if (!CargarFuentes()) { printf("faltan las fuentes\n"); return 1; }
    InterfazIniciar();
    const Paso guion[] = {
        {0.45f, 0, "0_logo_entra"},
        {2.20f, 0, "0_logo"},
        {3.40f, 0, "0_logo_sale"},
        {4.60f, 0, "1_biblioteca"},
        {4.70f, B_CUADRADO, NULL},
        {5.50f, 0, "2_bajando"},
        {18.00f, 0, "3_caratulas"},
        {18.20f, B_DER, NULL},
        {18.40f, B_DER, NULL},
        {18.70f, B_CRUZ, NULL},
        {24.00f, B_L3 | B_R3, NULL},
        {24.30f, B_ABAJO, NULL},
        {24.50f, B_ABAJO, NULL},
        {24.70f, B_ABAJO, NULL},
        {24.90f, B_DER, NULL},
        {25.50f, 0, "5_proporcion"},
        {25.70f, B_ABAJO, NULL},
        {25.90f, B_DER, NULL},
        {26.10f, B_DER, NULL},
        {26.30f, B_DER, NULL},
        {26.90f, 0, "6_filtro_crt"},
        {27.00f, B_CIRCULO, NULL},
        {27.60f, 0, "7_crt_jugando"},
        {27.80f, B_L3 | B_R3, NULL},
        {28.00f, B_ABAJO, NULL},
        {28.20f, B_CRUZ, NULL},
        {28.60f, 0, "8_guardado"},
        {29.0f, B_L3 | B_R3, NULL},
        {29.2f, B_ABAJO, NULL}, {29.35f, B_ABAJO, NULL}, {29.5f, B_ABAJO, NULL},
        {29.65f, B_ABAJO, NULL}, {29.8f, B_ABAJO, NULL}, {29.95f, B_ABAJO, NULL}, {30.1f, B_ABAJO, NULL},
        {30.3f, B_CRUZ, NULL},
        {31.5f, B_OPTIONS, NULL},
        {32.0f, 0, "9_favorito"},
        {32.2f, B_R1, NULL},
        {32.8f, 0, "10_recientes"},
        {33.0f, B_R1, NULL},
        {33.6f, 0, "11_favoritos"},
        {34.0f, B_R1, NULL},
        {34.3f, B_ABAJO, NULL},
        {34.5f, B_DER, NULL},
        {35.0f, 0, "12_config"},
        {35.2f, B_R1, NULL},
        {35.3f, B_DER, NULL},
        {35.4f, B_DER, NULL},
        {35.6f, B_CRUZ, NULL},
        {37.0f, B_L3 | B_R3, NULL},
        {37.2f, B_ABAJO, NULL}, {37.35f, B_ABAJO, NULL}, {37.5f, B_ABAJO, NULL},
        {37.7f, B_DER, NULL},
        {38.3f, 0, "13_menu_juego"},
        {38.5f, B_CIRCULO, NULL},
        {38.8f, B_L3 | B_R3, NULL},
        {39.0f, B_ABAJO, NULL}, {39.15f, B_ABAJO, NULL}, {39.3f, B_ABAJO, NULL}, {39.45f, B_ABAJO, NULL},
        {39.6f, B_CRUZ, NULL},
        {40.2f, B_L1, NULL},
        {40.4f, B_ABAJO, NULL}, {40.55f, B_ABAJO, NULL}, {40.7f, B_ABAJO, NULL},
        {40.9f, 0, "14_config"},
        {41.0f, B_CRUZ, NULL},
        {41.6f, 0, "15_acerca"},
    };
    // GUION=acerca: un recorrido corto hasta Configuración → Acerca de.
    static const Paso acerca[] = {
        {4.6f, B_R1, NULL}, {4.8f, B_R1, NULL}, {5.0f, B_R1, NULL},
        {5.3f, B_ARRIBA, NULL},   // de la primera fila sube a la última: "Acerca de SNESUI"
        {6.2f, 0, "20_config"},
        {6.3f, B_CRUZ, NULL},
        {6.9f, 0, "21_acerca"},
    };
    // GUION=lista: bajar por una biblioteca grande (para ver el desplazamiento y cuánto tarda cargar carátulas).
    static const Paso lista[] = {
        {4.4f, 0, "30_lista_arriba"},
        {4.6f, B_ABAJO, NULL}, {4.9f, B_ABAJO, NULL}, {5.2f, B_ABAJO, NULL},
        {5.9f, 0, "31_lista_medio"},
        {6.0f, B_R2, NULL},
        {6.6f, 0, "32_lista_pagina"},
        {6.8f, STICK_DER, NULL},
        {7.1f, 0, "33_lista_letra"},
        {7.6f, B_R2, NULL}, {7.9f, B_R2, NULL}, {8.2f, B_R2, NULL},
        {8.9f, 0, "34_lista_abajo"},
    };
    // GUION=mil: medir la búsqueda con muchos juegos (primera vez y con el índice) y buscar por nombre.
    static const Paso mil[] = {
        {0.5f, 0, "40_mil_inicio"},
        {4.5f, 0, "41_mil_lista"},
        {4.6f, B_TACTIL, NULL},
        {5.0f, B_DER, NULL}, {5.15f, B_DER, NULL}, {5.3f, B_ABAJO, NULL},   // la M (fila 2, columna 3)
        {5.45f, B_CRUZ, NULL},
        {5.6f, B_ARRIBA, NULL}, {5.8f, B_CRUZ, NULL},                        // la A
        {6.4f, 0, "42_mil_teclado"},
        {6.5f, B_CIRCULO, NULL},
        {7.2f, 0, "43_mil_filtrado"},
    };
    // GUION=copiar: copiar un juego del USB a la consola, borrarlo, y la ventana de "copiar todos".
    static const Paso copiar[] = {
        {4.4f, B_DER, NULL}, {4.6f, B_DER, NULL},
        {5.0f, 0, "50_usb"},
        {5.2f, B_OPTIONS, NULL},
        {5.7f, 0, "51_menu_juego"},
        {6.1f, B_CRUZ, NULL},                                   // copiar solo este juego a la consola
        {6.3f, 0, "52_copiando"},
        {8.5f, 0, "53_copiado"},
        {8.7f, B_OPTIONS, NULL},
        {9.1f, 0, "54_menu_consola"},
        {9.3f, B_CRUZ, NULL},                                   // borrar de la consola
        {9.7f, 0, "55_confirmar_borrar"},
        {9.9f, B_ARRIBA, NULL}, {10.1f, B_CRUZ, NULL},
        {11.8f, 0, "56_borrado"},
        {12.0f, B_IZQ, NULL}, {12.2f, B_IZQ, NULL},             // el juego que está solo en la consola
        {12.5f, B_OPTIONS, NULL},
        {13.0f, 0, "57_menu_solo_consola"},
        {13.2f, B_CRUZ, NULL},                                  // copiar este juego al USB
        {15.0f, 0, "58_copiado_usb"},
    };
    // GUION=alusb: copiar todos los juegos de la consola al USB.
    static const Paso alusb[] = {
        {4.5f, B_OPTIONS, NULL},
        {4.8f, B_ABAJO, NULL}, {4.95f, B_ABAJO, NULL}, {5.1f, B_ABAJO, NULL}, {5.25f, B_ABAJO, NULL},
        {5.6f, 0, "60_menu_todos_al_usb"},
        {5.8f, B_CRUZ, NULL},
        {6.4f, 0, "61_confirmar_al_usb"},
        {6.6f, B_ARRIBA, NULL}, {6.8f, B_CRUZ, NULL},
        {8.5f, 0, "62_copiados_al_usb"},
        {8.7f, B_L1, NULL},
        {9.0f, B_ARRIBA, NULL},
        {9.5f, 0, "63_config"},
    };
    // GUION=borrar: la ventana de copiar o borrar todos, borrar todos los de la consola y todos los del USB.
    static const Paso borrar[] = {
        {4.5f, B_OPTIONS, NULL},
        {4.8f, B_ABAJO, NULL}, {4.95f, B_ABAJO, NULL}, {5.1f, B_ABAJO, NULL},
        {5.3f, B_CRUZ, NULL},
        {5.8f, 0, "70_administrar"},
        {6.0f, B_ABAJO, NULL}, {6.15f, B_ABAJO, NULL},
        {6.3f, B_CRUZ, NULL},
        {6.8f, 0, "71_seguro_consola"},
        {7.0f, B_ARRIBA, NULL}, {7.2f, B_CRUZ, NULL},
        {8.6f, 0, "72_borrados_consola"},
        {8.8f, B_OPTIONS, NULL},
        {9.0f, B_ABAJO, NULL}, {9.15f, B_ABAJO, NULL},
        {9.3f, B_CRUZ, NULL},
        {9.5f, B_ABAJO, NULL}, {9.65f, B_ABAJO, NULL}, {9.8f, B_ABAJO, NULL},
        {9.95f, B_CRUZ, NULL},
        {10.5f, 0, "73_seguro_usb"},
        {10.7f, B_ARRIBA, NULL}, {10.9f, B_CRUZ, NULL},
        {12.4f, 0, "74_borrados_usb"},
    };
    // GUION=diag: abrir un juego, jugar 12 s (la foto de los 10 s), guardar el informe desde la pausa y salir.
    static const Paso diag[] = {
        {4.5f, B_CRUZ, NULL},
        {16.5f, B_L3 | B_R3, NULL},
        {16.7f, B_ABAJO, NULL}, {16.85f, B_ABAJO, NULL}, {17.0f, B_ABAJO, NULL},
        {17.15f, B_ABAJO, NULL}, {17.3f, B_ABAJO, NULL}, {17.45f, B_ABAJO, NULL},
        {17.7f, 0, "80_menu_informe"},
        {17.8f, B_CRUZ, NULL},
        {18.2f, 0, "81_informe_guardado"},
        {18.5f, B_ABAJO, NULL}, {18.65f, B_ABAJO, NULL}, {18.8f, B_CRUZ, NULL},
        {19.5f, 0, "82_biblioteca"},
    };
    // GUION=multi: juegos de otras consolas (pc/usb/NES y pc/usb/GB, que ordenan antes que los de SNES): abrir el
    // primero (NES), salir, abrir el segundo (GB).
    static const Paso multi[] = {
        {4.4f, 0, "90_biblioteca_multi"},
        {4.45f, B_IZQ, NULL}, {4.6f, B_IZQ, NULL}, {4.75f, B_IZQ, NULL},
        {4.9f, B_CRUZ, NULL},
        {6.0f, B_OPTIONS, NULL}, {6.1f, 0, NULL}, {7.0f, B_OPTIONS, NULL},
        {9.5f, 0, "91_nes"},
        {9.6f, B_L3 | B_R3, NULL},
        {9.8f, B_ARRIBA, NULL},
        {10.0f, B_CRUZ, NULL},
        {11.0f, B_DER, NULL},
        {11.6f, 0, "92_biblioteca_gb"},
        {11.7f, B_CRUZ, NULL},
        {13.0f, B_OPTIONS, NULL}, {14.0f, B_CRUZ, NULL},
        {17.0f, 0, "93_gb"},
    };
    // GUION=menu: la biblioteca con varias consolas: Todos y cada pestaña de consola (R1), un juego de GBA abierto y su
    // menú de pausa.
    static const Paso menu[] = {
        {1.5f, 0, "94_presentacion"},
        {5.0f, 0, "95_todos"},
        {5.2f, B_R1, NULL}, {5.8f, 0, "96_snes"},
        {6.0f, B_R1, NULL}, {6.6f, 0, "97_nes"},
        {6.8f, B_R1, NULL}, {7.4f, 0, "98_gb"},
        {7.6f, B_R1, NULL}, {8.2f, 0, "99_gbc"},
        {8.4f, B_R1, NULL}, {9.0f, 0, "100_gba"},
        {9.1f, B_CRUZ, NULL},
        {16.0f, 0, "101_gba_jugando"},
        {16.1f, B_L3 | B_R3, NULL},
        {16.8f, 0, "102_pausa"},
        {17.0f, B_ARRIBA, NULL}, {17.2f, B_CRUZ, NULL},
        {18.0f, B_R1, NULL}, {18.6f, 0, "103_sms"},
    };
    // GUION=arcade (con USB_PC = una carpeta con solo juegos arcade): cada pestaña, abrir el primero (incompleto: el aviso),
    // el segundo y el tercero (jugar, pausa, salir), y la ficha del juego.
    static const Paso arcade[] = {
        {4.5f, 0, "110_arcade_todos"},
        // De Todos a ARCADE: SNES, NES, GB, GBC, GBA, SMS y ARCADE (pulsaciones separadas: la tira de pestañas se anima).
        {4.6f, B_R1, NULL}, {4.9f, B_R1, NULL}, {5.2f, B_R1, NULL}, {5.5f, B_R1, NULL}, {5.8f, B_R1, NULL}, {6.1f, B_R1, NULL},
        {6.4f, B_R1, NULL},
        {7.0f, 0, "111_arcade"},
        {7.1f, B_R1, NULL}, {7.7f, 0, "112_neogeo"},
        {7.8f, B_R1, NULL}, {8.4f, 0, "113_cps1"},
        {8.5f, B_R1, NULL}, {9.1f, 0, "114_cps2"},
        {9.2f, B_R1, NULL}, {9.8f, 0, "115_cps3"},
        // CPS3: abrir el primero (el más pesado), jugar, pausa, salir.
        {9.9f, B_CRUZ, NULL},
        {16.0f, 0, "116_cps3_jugando"},
        {16.1f, B_L3 | B_R3, NULL}, {16.7f, 0, "117_pausa"},
        {16.9f, B_ARRIBA, NULL}, {17.1f, B_CRUZ, NULL},
        // CPS1: dos juegos seguidos (el segundo con el núcleo recién cerrado).
        {17.8f, B_L1, NULL}, {18.1f, B_L1, NULL},
        {18.6f, B_CRUZ, NULL},
        {23.0f, 0, "118_cps1_primero"},
        {23.1f, B_L3 | B_R3, NULL}, {23.5f, B_ARRIBA, NULL}, {23.7f, B_CRUZ, NULL},
        {24.4f, B_DER, NULL}, {24.7f, B_CRUZ, NULL},
        {29.0f, 0, "119_cps1_segundo"},
        {29.1f, B_L3 | B_R3, NULL}, {29.5f, B_ARRIBA, NULL}, {29.7f, B_CRUZ, NULL},
        // ARCADE: el incompleto (el aviso propio) y después Neo Geo.
        {30.4f, B_L1, NULL}, {30.7f, B_L1, NULL},
        {31.2f, B_CRUZ, NULL},
        {32.5f, 0, "120_incompleto"},
        {32.7f, B_CIRCULO, NULL}, {33.0f, B_CRUZ, NULL},
        {33.6f, B_R1, NULL},
        {34.2f, B_CRUZ, NULL},
        {40.0f, 0, "121_neogeo_jugando"},
        {40.1f, B_L3 | B_R3, NULL}, {40.5f, B_ARRIBA, NULL}, {40.7f, B_CRUZ, NULL},
        {41.4f, B_OPTIONS, NULL},
        {42.0f, 0, "122_ficha"},
    };
    // GUION=refrescar: actualizar la biblioteca (triángulo) varias veces, también en plena búsqueda, cambiar de pestaña
    // y abrir juegos mientras busca.
    static const Paso refrescar[] = {
        {1.0f, B_TRIANGULO, NULL}, {1.5f, B_TRIANGULO, NULL},
        {3.0f, B_R1, NULL}, {3.3f, B_R1, NULL}, {3.6f, B_ABAJO, NULL}, {3.9f, B_CRUZ, NULL},
        {8.0f, B_L3 | B_R3, NULL}, {8.3f, B_ARRIBA, NULL}, {8.5f, B_CRUZ, NULL},
        {10.0f, B_TRIANGULO, NULL}, {10.2f, B_R1, NULL}, {10.4f, B_R1, NULL}, {10.6f, B_R1, NULL},
        {11.0f, 0, "60_refrescar"},
        {20.0f, B_TRIANGULO, NULL}, {20.5f, B_L1, NULL}, {21.0f, B_ABAJO, NULL}, {21.2f, B_CRUZ, NULL},
        {26.0f, B_L3 | B_R3, NULL}, {26.3f, B_ARRIBA, NULL}, {26.5f, B_CRUZ, NULL},
        {40.0f, B_TRIANGULO, NULL}, {70.0f, B_TRIANGULO, NULL},
        {99.0f, 0, "61_refrescar_fin"},
    };
    // GUION=bios: Configuración → BIOS (la penúltima fila).
    static const Paso bios[] = {
        {4.6f, B_L1, NULL},   // de Todos a la última pestaña (Configuración)
        {5.3f, B_ARRIBA, NULL}, {5.5f, B_ARRIBA, NULL},
        {6.2f, 0, "70_config_bios"},
        {6.3f, B_CRUZ, NULL},
        {6.9f, 0, "71_bios"},
    };
    // GUION=ficha: bajar uno y capturar (la ficha del segundo juego en el estilo de menú guardado).
    static const Paso ficha[] = {{4.6f, B_ABAJO, NULL}, {5.6f, 0, "90_ficha"}, {5.8f, B_ABAJO, NULL}, {6.4f, B_ABAJO, NULL}, {7.0f, B_ABAJO, NULL}, {7.6f, B_ABAJO, NULL}, {8.4f, 0, "91_ficha"}};
    // GUION=asistente: el asistente de primera vez, paso por paso (X), con una captura de cada uno.
    static const Paso asistente[] = {
        {4.8f, 0, "a0"}, {5.0f, B_CRUZ, NULL}, {5.8f, 0, "a1"}, {6.0f, B_CRUZ, NULL}, {6.8f, 0, "a2"}, {7.0f, B_CRUZ, NULL},
        {7.8f, 0, "a3"}, {8.0f, B_CRUZ, NULL}, {8.8f, 0, "a4"}, {9.0f, B_CRUZ, NULL}, {9.8f, 0, "a5"}, {10.0f, B_CRUZ, NULL},
        {10.8f, 0, "a6"}, {11.0f, B_CRUZ, NULL}, {11.8f, 0, "a7"}, {12.0f, B_CRUZ, NULL}, {12.8f, 0, "a8"},
    };
    // GUION=caratulas: bajar las carátulas que falten (□) y esperar.
    static const Paso caratulas[] = {
        {4.6f, B_CUADRADO, NULL},
        {7.0f, 0, "79_bajando"},
        {40.0f, 0, "80_caratulas"},
    };
    const char* g = getenv("GUION");
    bool corto = g && !strcmp(g, "acerca"), larga = g && !strcmp(g, "lista"), muchos = g && !strcmp(g, "mil"), copia = g && !strcmp(g, "copiar"), alUsb = g && !strcmp(g, "alusb"), borra = g && !strcmp(g, "borrar"), dg = g && !strcmp(g, "diag"), mu = g && !strcmp(g, "multi"), me = g && !strcmp(g, "menu"), ar = g && !strcmp(g, "arcade"), rf = g && !strcmp(g, "refrescar"), bi = g && !strcmp(g, "bios"), ca = g && !strcmp(g, "caratulas"), fi = g && !strcmp(g, "ficha"), as = g && !strcmp(g, "asistente");
    // GUION=estilos: L2 cuarenta veces (cada interfaz) y después R2 cuarenta veces (cada menú), con una captura de cada uno.
    static Paso estilos[170];
    static char nombresEstilos[80][24];
    int nEstilos = 0;
    bool es = g && !strcmp(g, "estilos");
    if (es)
    {
        for (int i = 0; i < 80; i++)
        {
            float t0 = 4.6f + i * .7f;
            estilos[nEstilos++] = {t0, (uint32_t)(i < 40 ? B_L2 : B_R2), NULL};
            snprintf(nombresEstilos[i], sizeof nombresEstilos[i], i < 40 ? "est_i%02d" : "est_m%02d", (i % 40 + 1) % 40);
            estilos[nEstilos++] = {t0 + .6f, 0, nombresEstilos[i]};
        }
    }
    // GUION=medir: pasa de juego en juego (derecha y abajo) y cuenta cuánto cuesta cada parte del cuadro.
    bool md = g && !strcmp(g, "medir");
    if (md)
        for (int i = 0; i < 40; i++) estilos[nEstilos++] = {4.6f + i * .15f, (uint32_t)(i % 8 == 7 ? B_ABAJO : B_DER), NULL};
    if (md) es = true;
    // GUION=estres: botones al azar en cada cuadro (sin ✕ ni OPTIONS), para buscar cierres al moverse muy rápido.
    bool ets = g && !strcmp(g, "estres");
    const Paso* pasos = es ? estilos : corto ? acerca : larga ? lista : muchos ? mil : copia ? copiar : alUsb ? alusb : borra ? borrar : dg ? diag : mu ? multi : me ? menu : ar ? arcade : rf ? refrescar : bi ? bios : ca ? caratulas : fi ? ficha : as ? asistente : guion;
    const int N = es ? nEstilos : corto ? (int)(sizeof acerca / sizeof acerca[0]) : larga ? (int)(sizeof lista / sizeof lista[0]) : muchos ? (int)(sizeof mil / sizeof mil[0]) : copia ? (int)(sizeof copiar / sizeof copiar[0]) : alUsb ? (int)(sizeof alusb / sizeof alusb[0]) : borra ? (int)(sizeof borrar / sizeof borrar[0]) : dg ? (int)(sizeof diag / sizeof diag[0]) : mu ? (int)(sizeof multi / sizeof multi[0]) : me ? (int)(sizeof menu / sizeof menu[0]) : ar ? (int)(sizeof arcade / sizeof arcade[0]) : rf ? (int)(sizeof refrescar / sizeof refrescar[0]) : bi ? (int)(sizeof bios / sizeof bios[0]) : ca ? (int)(sizeof caratulas / sizeof caratulas[0]) : fi ? (int)(sizeof ficha / sizeof ficha[0]) : as ? (int)(sizeof asistente / sizeof asistente[0]) : (int)(sizeof guion / sizeof guion[0]);
    const double FIN = ets ? 4.5 + atof(getenv("SEGUNDOS") ? getenv("SEGUNDOS") : "60") : md ? 10.6 : es ? 4.6 + 80 * .7 + 1 : corto ? 7.0 : larga ? 9.0 : muchos ? 7.5 : copia ? 15.2 : alUsb ? 9.7 : borra ? 12.6 : dg ? 19.7 : mu ? 17.2 : me ? 18.8 : ar ? 42.3 : rf ? 100.0 : bi ? 7.0 : ca ? 41.0 : fi ? 8.6 : as ? 13.0 : 42.0;
    double peor = 0, medArmar = 0, medPintar = 0;
    int medCuadros = 0;
    int k = 0;
    for (int cuadro = 0; tiempoSimulado < FIN; cuadro++)
    {
        tiempoSimulado = cuadro / 60.0;
        Mando m = {};
        m.conectado = true;
        const char* captura = NULL;
        while (k < N && pasos[k].t <= tiempoSimulado + 1e-6)
        {
            m.pulsados |= pasos[k].botones;
            m.apretados |= pasos[k].botones;
            if (pasos[k].botones & STICK_DER) m.rx = 1;
            if (pasos[k].captura) captura = pasos[k].captura;
            k++;
        }
        if (ets && tiempoSimulado > 4.5)
        {
            static uint32_t az = 12345;
            static const uint32_t lista[] = {B_DER, B_IZQ, B_ARRIBA, B_ABAJO, B_R3, B_L3, B_L1, B_R1, B_L2, B_R2, B_CUADRADO, B_CIRCULO, B_DER, B_ABAJO, B_R3, B_R3};
            for (int b = 0; b < 2; b++)
            {
                az ^= az << 13; az ^= az >> 17; az ^= az << 5;
                uint32_t bot = lista[az % 16];
                if ((bot == B_L2 || bot == B_R2 || bot == B_CIRCULO || bot == B_CUADRADO) && az % 7) continue;   // estos, menos seguido
                m.pulsados |= bot;
                m.apretados |= bot;
            }
            if (cuadro % 600 == 0) { char n[32]; snprintf(n, sizeof n, "estres_%d", cuadro / 600); captura = NULL; printf("estres t=%.0f\n", tiempoSimulado); fflush(stdout); }
        }
        int hallados = 0;
        if (!avisado && cuadro > 2 && !BibliotecaBuscando(&hallados))
        {
            avisado = true;
            double seg = std::chrono::duration<double>(std::chrono::steady_clock::now() - inicioReal).count();
            printf("búsqueda terminada a los %.2f s reales, %d juegos\n", seg, hallados);
        }
        auto t0 = std::chrono::steady_clock::now();
        PantallaComenzar();
        InterfazCuadro((float)tiempoSimulado, 1 / 60.f, m);
        auto t1 = std::chrono::steady_clock::now();
        PantallaMostrar();
        double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        if (md && tiempoSimulado > 4.6)
        {
            medArmar += std::chrono::duration<double, std::milli>(t1 - t0).count();
            medPintar += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t1).count();
            medCuadros++;
        }
        if (tiempoSimulado > 4.3 && ms > peor) peor = ms;
        if (larga && tiempoSimulado > 4.3 && ms > 20) printf("  lento: %.1f ms en t=%.2f\n", ms, tiempoSimulado);
        if (captura) Guardar(captura);
        std::this_thread::sleep_for(std::chrono::milliseconds(16));   // a tiempo real: la descarga y la emulación corren en sus hilos
    }
    if (larga) printf("cuadro más lento en la biblioteca: %.1f ms\n", peor);
    if (md && medCuadros)
    {
        printf("medir: %d cuadros, armar (interfaz + fondo) %.2f ms, pintar (todos los hilos) %.2f ms, peor %.1f ms\n", medCuadros,
               medArmar / medCuadros, medPintar / medCuadros, peor);
#ifdef MEDIR_ORDENES
        extern uint64_t ciclosPorTipo[16];
        extern uint64_t vidrioLentos, vidrioRapidos;
        printf("  vidrio: %.0f px lentos y %.0f rápidos por cuadro\n", (double)vidrioLentos / medCuadros, (double)vidrioRapidos / medCuadros);
        static const char* const nombres[16] = {"degradado", "fondo4", "caja", "sombra", "borde", "brillo", "linea", "texto", "imagen", "velo", "caja-h", "vidrio?"};
        uint64_t tot = 0;
        for (int i = 0; i < 16; i++) tot += ciclosPorTipo[i];
        for (int i = 0; i < 16; i++)
            if (ciclosPorTipo[i]) printf("  %-10s %5.1f %%  %.2f Mciclos/cuadro\n", i == 9 ? "vidrio" : i == 11 ? "velo" : i == 12 ? "agrandar" : i == 13 ? "copiar" : i < 12 ? nombres[i] : "?", 100.0 * ciclosPorTipo[i] / tot, ciclosPorTipo[i] / 1e6 / medCuadros);
#endif
    }
    return 0;
}
