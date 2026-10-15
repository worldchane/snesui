// El núcleo del emulador de Super Nintendo. A diferencia de la PS2, la SNES corre a 3,58 MHz: un intérprete simple la
// emula a velocidad completa sin problemas, así que no hace falta recompilador (JIT).
// Partes: cpu (el procesador 65816), ppu (gráficos), apu (sonido: SPC700 + DSP), bus (la memoria y el hardware).
#pragma once
#include <stdint.h>

static const int SNES_ANCHO = 256, SNES_ALTO = 224;

// Carga la rom (.smc/.sfc). Si falla, explica por qué en 'error' (160 bytes).
bool SnesCargar(const char* ruta, char* error);
void SnesDescargar();
void SnesReiniciar();

// Corre un cuadro entero (un fotograma de la consola) y deja la imagen en 'pantalla' (256 × 224, 0xRRGGBB) y el sonido
// del cuadro en 'audio' (muestras estéreo de 16 bits a 32000 Hz). Devuelve cuántas muestras de audio generó.
int SnesCuadro(uint32_t* pantalla, int16_t* audio, int maxAudio);

// Estado de los botones del mando (un bit por botón), antes de correr el cuadro.
enum
{
    SB_B = 1 << 0, SB_Y = 1 << 1, SB_SELECT = 1 << 2, SB_START = 1 << 3,
    SB_ARRIBA = 1 << 4, SB_ABAJO = 1 << 5, SB_IZQ = 1 << 6, SB_DER = 1 << 7,
    SB_A = 1 << 8, SB_X = 1 << 9, SB_L = 1 << 10, SB_R = 1 << 11,
};
void SnesMando(int jugador, uint16_t botones);

struct InfoRom
{
    bool valida;
    char titulo[24];       // el nombre del juego que trae la rom
    char mapeo[8];         // "LoROM", "HiROM"
    int tamKB;             // tamaño de la rom
    bool pal;              // región PAL (50 Hz) o NTSC (60 Hz)
    bool tieneRam;         // la rom guarda partida (SRAM)
    char error[160];
};
const InfoRom& SnesInfo();

// Guardado de batería (SRAM del cartucho): el contenido tal cual, igual que los .srm de snes9x y bsnes (se pueden
// pasar de uno a otro). SnesSramCambiada dice si el juego escribió desde la última consulta.
uint32_t SnesSramTam();
uint8_t* SnesSramDatos();
bool SnesSramCambiada();

// Estados rápidos: la consola entera (procesador, memoria, gráficos, sonido y SRAM) en un bloque de bytes.
uint32_t SnesEstadoTam();
uint32_t SnesGuardarEstado(uint8_t* buf, uint32_t max);   // bytes escritos, 0 si no entró
bool SnesCargarEstado(const uint8_t* buf, uint32_t n);    // false si el estado es de otro juego o está dañado

// Informe de diagnóstico (texto): la rom, el estado del procesador, la imagen y el Super FX, y un historial de los
// últimos 30 segundos. Devuelve los bytes escritos.
int SnesDiagnostico(char* out, int max);
// Problemas que el núcleo detecta solo (juego trabado, Super FX perdido...). Devuelve true una vez por cada uno, con la
// explicación en 'txt'. Llamar desde el mismo hilo que SnesCuadro.
bool SnesAnomalia(char* txt, int max);
// Revisa la rom sin cargarla del todo (para la biblioteca).
void SnesAnalizar(const char* ruta, InfoRom& info);
