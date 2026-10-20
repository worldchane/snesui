// Registro de diagnóstico automático: todo lo que pasa (programa abierto, juegos, errores, problemas detectados) queda en
// <USB>/SNESUI-diagnostico/registro.txt (o /data/allesiu/diagnostico si no hay USB), para pasárselo al desarrollador.
// Los informes completos (texto + captura .bmp + estado .st) van a la misma carpeta como informe-NNN.*.
#pragma once
#include <stdint.h>

// Al abrir el programa (hilo principal). Si la vez anterior el programa se cerró en medio de un juego, copia al registro
// la última foto del estado que quedó guardada.
void RegistroIniciar(const char* version);
// Agrega una línea (desde cualquier hilo). Se escribe en el archivo al rato, desde el hilo principal.
void Registrar(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
// Cada cuadro, desde el hilo principal: escribe en el archivo lo pendiente.
void RegistroCuadro();
// Las últimas líneas del registro (para meterlas en un informe).
int RegistroUltimas(char* out, int max);

// Guarda un informe completo (hilo principal). 'imagen' es la pantalla de la SNES (256 × 224, 0xRRGGBB) o NULL.
// Devuelve true y deja en 'donde' la ruta del texto.
bool RegistroInforme(const char* motivo, const char* texto, const uint32_t* imagen, const uint8_t* estado,
                     uint32_t nEstado, char* donde, int max);

// Mientras se juega: la foto más reciente del estado (si el programa se cuelga, queda para la próxima vez).
void RegistroFoto(const char* texto);
void RegistroFinJuego();   // se salió del juego normalmente: ya no hace falta la foto
