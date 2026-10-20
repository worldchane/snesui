// Dibujo en alta definición (1920 × 1080) por software, repartido entre los núcleos de la PS4.
// Cada cuadro se arma como una lista de órdenes (cajas redondeadas, sombras, brillos, líneas, texto, imágenes) y después
// cada núcleo pinta su franja de filas de la pantalla con toda la lista: no hay dos núcleos escribiendo el mismo píxel.
// Las formas se calculan con su distancia al borde, así que los bordes salen suavizados (sin escalones) y las sombras,
// difusas. Colores en 0xRRGGBB y opacidad en [0, 1].
#pragma once
#include <stdint.h>

static const int ANCHO = 1920, ALTO = 1080;

void LienzoIniciar();
// Empieza un cuadro que se pintará en 'pixeles' (pitch = píxeles por fila). rojoAzul: la pantalla guarda el azul en
// el byte alto (formato ABGR) y hay que cambiar el orden de los colores.
void LienzoComenzar(uint32_t* pixeles, int pitch, bool rojoAzul);
void LienzoTerminar();   // pinta la lista entera, en paralelo

// Opacidad que multiplica todo lo que se ordene desde ahora (para fundidos de pantallas enteras).
void Opacidad(float a);
float OpacidadActual();
// Recorte: lo que se ordene desde ahora solo se pinta dentro de este rectángulo.
void Recorte(float x, float y, float w, float h);
void SinRecorte();

void Degradado(uint32_t arriba, uint32_t abajo);   // toda la pantalla
// Toda la pantalla con una imagen de (ANCHO/4) × (ALTO/4) agrandada 4 veces con filtro suave. Para fondos de degradados y
// luces difusas: se ven iguales que a resolución completa y cuestan una mezcla por píxel.
static const int FONDO_W = ANCHO / 4, FONDO_H = ALTO / 4;
void FondoReducido(const uint32_t* px);
void Caja(float x, float y, float w, float h, float radio, uint32_t c, float a);
void CajaDegradada(float x, float y, float w, float h, float radio, uint32_t arriba, uint32_t abajo, float a);
void CajaDegradadaH(float x, float y, float w, float h, float radio, uint32_t izq, uint32_t der, float a);
// hueca: no pinta el interior (para sombras debajo de algo opaco que lo tapa; ahorra mucho trabajo).
void Sombra(float x, float y, float w, float h, float radio, float difuminado, float a, bool hueca = false);
void Borde(float x, float y, float w, float h, float radio, float grosor, uint32_t c, float a);
void Circulo(float cx, float cy, float r, uint32_t c, float a);
void Aro(float cx, float cy, float r, float grosor, uint32_t c, float a);
// Vidrio esmerilado (el estilo Aero de macOS): la caja muestra el fondo desenfocado (la imagen de LienzoVidrio, que cubre
// toda la pantalla a baja resolución) teñido con 'tinte' en la proporción 'cuanto'. Sin imagen, es una caja del tinte.
// cambio: la imagen es otra o cambió su contenido (si no, se reusa lo ya agrandado a pantalla completa).
void LienzoVidrio(const uint32_t* px, int w, int h, bool cambio = true);
void CajaVidrio(float x, float y, float w, float h, float radio, uint32_t tinte, float cuanto, float a);
// Rectángulo de un color con la opacidad cambiando de arriba (aArriba) a abajo (aAbajo): sombras de bordes, viñetas.
void Velo(float x, float y, float w, float h, float radio, uint32_t c, float aArriba, float aAbajo);
void Brillo(float cx, float cy, float r, uint32_t c, float a);   // luz radial difusa
void Linea(float x0, float y0, float x1, float y1, float grosor, uint32_t c, float a);
// Imagen de 32 bits (0xRRGGBB) escalada con filtro bilineal (suave) o al píxel más cercano (nítido).
void Imagen(const uint32_t* px, int iw, int ih, float x, float y, float w, float h, float a, bool suave);
// Sumada al fondo (lo negro no tapa nada): para imágenes con fondo negro y brillos, como el logo.
void ImagenSumada(const uint32_t* px, int iw, int ih, float x, float y, float w, float h, float a);

// Uso interno de texto.cpp: una orden de texto ya medida.
void OrdenTexto(int fuente, const char* s, int n, float x, float y, uint32_t c, float a);

// Estadísticas del último cuadro.
int OrdenesUltimoCuadro();
