// Lo que el programa necesita de la PS4, con solo funciones del sistema ya probadas en la consola del usuario (ver LEEME.md):
// tiempo, hilos, reparto de trabajo entre los núcleos, mando, sonidos de la interfaz y archivos en /data o el USB.
#pragma once
#include <stdint.h>
#include <stdio.h>

// ---------- tiempo ----------
double Segundos();          // desde que abrió el programa, con precisión de microsegundos

// ---------- hilos ----------
// Siempre hilos de la consola (scePthreadCreate): std::thread cierra el programa.
bool Lanzar(void* (*f)(void*), void* arg, const char* nombre);

// Reparte un trabajo entre el hilo principal y los trabajadores; vuelve cuando todas las partes terminaron.
// f(parte, partes, arg) con parte en [0, partes).
void IniciarTrabajadores(int n);
int Partes();
void EnParalelo(void (*f)(int parte, int partes, void* arg), void* arg);

// ---------- mando ----------
enum
{
    B_ARRIBA = 1 << 0, B_ABAJO = 1 << 1, B_IZQ = 1 << 2, B_DER = 1 << 3,
    B_CRUZ = 1 << 4, B_CIRCULO = 1 << 5, B_CUADRADO = 1 << 6, B_TRIANGULO = 1 << 7,
    B_L1 = 1 << 8, B_R1 = 1 << 9, B_L2 = 1 << 10, B_R2 = 1 << 11,
    B_OPTIONS = 1 << 12, B_L3 = 1 << 13, B_R3 = 1 << 14, B_TACTIL = 1 << 15,
};
struct Mando
{
    uint32_t apretados;   // en este momento (las direcciones incluyen el stick izquierdo)
    uint32_t pulsados;    // recién apretados, con repetición automática en las direcciones si se mantienen
    float lx, ly, rx, ry; // sticks en [-1, 1], ya sin la zona muerta
    bool conectado;
};
void IniciarMando();
void LeerMando(Mando& m, float dt);
// Mando del jugador 2 (el de otro usuario con la sesión iniciada en la consola): los botones apretados (bits B_*).
uint32_t LeerMando2(bool* conectado);

// ---------- sonidos de la interfaz (un hilo de la consola los mezcla) ----------
enum Sonido { SON_MOVER, SON_ACEPTAR, SON_ATRAS, SON_SECCION, SON_ERROR, SON_INICIO };
void IniciarAudio();
bool AudioListo();
void Sonar(Sonido s);
void SonidosActivos(bool si);
// Sonido del juego: muestras estéreo de 16 bits a 32 kHz (se pasan a 48 kHz y se mezclan con los de la interfaz).
void AudioJuego(const int16_t* estereo, int n);
void AudioJuegoVaciar();

// ---------- archivos ----------
bool CrearCarpeta(const char* ruta);
// Crea la carpeta y las que falten antes (mkdir de a una, como la consola lo pide). true si al final existe.
bool CrearCarpetas(const char* ruta);
// Llama a f por cada entrada de la carpeta (sin "." ni ".."). Devuelve cuántas hubo, o -1 si no se pudo abrir.
// No mide los archivos (eso es lento en un USB con miles): quien necesite el tamaño usa ExisteArchivo.
// Compilado con SIN_LISTADO (variante "segura" que solo usa funciones probadas) siempre devuelve -1.
int ListarCarpeta(const char* ruta, void (*f)(const char* nombre, bool carpeta, void* u), void* u);
int ErrorListado();
// Saca a la aplicación del encierro del sistema (sandbox) con GoldHEN, para que vea el USB (/mnt/usb0...). Sin esto la
// aplicación solo ve sus propias carpetas y /data. Devuelve 1 si quedó con acceso, 0 si GoldHEN lo negó y -1 si se
// saltó porque una vez cerró el programa. El detalle queda en AccesoDetalle().
// 'quien' nombra el anotador de este programa (/data/snes-ps4/acceso_<quien>.txt): cada programa lleva el suyo, así
// lo que pasa en uno (por ejemplo una prueba) no frena a otro. insistir = true lo intenta aunque una vez haya cerrado el
// programa (cuando el usuario lo pide a propósito).
// Afuera del encierro /app0 puede no existir: usar solo para recorrer el USB y volver con CerrarAccesoCompleto().
int AbrirAccesoCompleto(const char* quien, bool insistir = false);
void CerrarAccesoCompleto();
const char* AccesoDetalle();   // errno de la última carpeta que ListarCarpeta no pudo abrir (2 = no existe, 13 = sin permiso)
void Dormir(int ms);
bool ExisteArchivo(const char* ruta, uint64_t* tam = 0);
// fopen (en el simulador de PC cambia /data y /mnt/usb0 por carpetas de pc/).
FILE* AbrirArchivo(const char* ruta, const char* modo);
// Lee hasta max-1 bytes y termina con 0; -1 si no existe.
int LeerTexto(const char* ruta, char* buf, int max);
bool EscribirTexto(const char* ruta, const char* texto);

static const char* const CARPETA_DATOS = "/data/snes-ps4";

// ---------- red (para bajar carátulas) ----------
// La red de la consola se carga recién la primera vez que se usa (no al abrir el programa): si la consola no la permite,
// solo falla esto y el resto sigue andando. En la variante "segura" (SIN_RED) no hay red.
bool RedIniciar(char* detalle, int max);
int RedConectar(const char* host, int puerto);   // socket conectado, o -1
int RedEnviar(int s, const void* datos, int n);
int RedRecibir(int s, void* datos, int n);        // bytes recibidos; 0 = se cerró; < 0 = error
void RedCerrar(int s);
