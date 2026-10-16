// El chip OBC1 de algunos cartuchos: ayuda a armar la tabla de sprites en la RAM del cartucho (8 KB). La SNES elige un
// sprite ($7FF6) y lee o escribe sus 4 bytes por $7FF0-$7FF3, y sus 2 bits extra por $7FF4 (el chip calcula dónde
// van). $7FF5 elige cuál de las dos tablas de la RAM se usa. Todo lo demás es la RAM tal cual.
#include "interno.h"
#include "rom.h"

bool obc1Presente;

static struct { uint16_t base; uint8_t sprite, desplazamiento; } o;

static inline uint8_t Ram(uint16_t a) { return sram[a & 0x1fff]; }
static inline void Ram(uint16_t a, uint8_t v) { sram[a & 0x1fff] = v; sramCambiada = true; }

void Obc1Reiniciar()
{
    o.base = (Ram(0x1ff5) & 1) ? 0x1800 : 0x1c00;
    o.sprite = Ram(0x1ff6) & 0x7f;
    o.desplazamiento = (Ram(0x1ff6) & 3) << 1;
}

uint8_t Obc1Leer(uint16_t dir)
{
    dir &= 0x1fff;
    if (dir >= 0x1ff0 && dir <= 0x1ff3) return Ram((uint16_t)(o.base + (o.sprite << 2) + (dir & 3)));
    if (dir == 0x1ff4) return Ram((uint16_t)(o.base + (o.sprite >> 2) + 0x200));
    return Ram(dir);
}

void Obc1Escribir(uint16_t dir, uint8_t v)
{
    dir &= 0x1fff;
    if (dir >= 0x1ff0 && dir <= 0x1ff3) { Ram((uint16_t)(o.base + (o.sprite << 2) + (dir & 3)), v); return; }
    if (dir == 0x1ff4)
    {
        uint16_t a = (uint16_t)(o.base + (o.sprite >> 2) + 0x200);
        Ram(a, (uint8_t)((Ram(a) & ~(3 << o.desplazamiento)) | ((v & 3) << o.desplazamiento)));
        return;
    }
    if (dir == 0x1ff5) o.base = (v & 1) ? 0x1800 : 0x1c00;
    if (dir == 0x1ff6) { o.sprite = v & 0x7f; o.desplazamiento = (v & 3) << 1; }
    Ram(dir, v);
}

void Obc1Estado(Serial& s) { s(o); }
