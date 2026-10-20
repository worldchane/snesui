# Copia de dcs2k.cpp de FinalBurn Neo (el sonido DCS de las placas de Midway T, X y Wolf)
# con el atajo de MAME 2003 (src/sndhrdw/dcs.c, "DCS SPEEDUPS", de Aaron Giles): la transformada que el procesador de
# sonido (ADSP-2105) calcula cada pocos cuadros se hace en C en lugar de emularla instrucción por instrucción. En los
# juegos de la placa Wolf el sonido era el 73 % del tiempo de emulación.
#   python herramientas/parche_dcs.py <dcs2k.cpp original> <copia>
import sys

origen, destino = sys.argv[1], sys.argv[2]
s = open(origen, encoding='utf-8', errors='surrogateescape').read()

ATAJO = r'''
// ALLESIU: atajo de MAME 2003 para la transformada del DCS (ver herramientas/parche_dcs.py). Se dispara cuando el
// programa del sonido escribe 2 en su contador de pasadas justo antes del bucle: los juegos de la placa Wolf
// en $04F8 con el PC en $00FD; los de las placas T/X en $063D con el PC en $00F6. Hace
// el bucle entero en C y salta al final ($2B instrucciones más adelante), como MAME.
static void DcsAtajoTransformada(adsp2100_state* adsp)
{
    UINT16 *ram = (UINT16 *)pDataRAM;   // datos $3800-$39FF
    int mem63d = 2, mem63e = 0x40, mem63f = mem63e >> 1;
    for (int i = 0; i < 6; i++)
    {
        int i4 = 0x780, i5 = 0x700;             // tablas de senos y cosenos (memoria externa)
        int i0 = 0, i1 = mem63e, i2 = i1;       // desde $3800
        for (int j = 0; j < mem63d; j++)
        {
            INT32 my0 = (INT16)(pExtRAM32[i4++ & 0x7FF] >> 8);
            INT32 my1 = (INT16)(pExtRAM32[i5++ & 0x7FF] >> 8);
            for (int k = 0; k < mem63f; k++)
            {
                INT32 mx0 = (INT16)BURN_ENDIAN_SWAP_INT16(ram[i1++ & 0x1FF]);
                INT32 mx1 = (INT16)BURN_ENDIAN_SWAP_INT16(ram[i1++ & 0x1FF]);
                INT32 ax0 = (mx0 * my0 - mx1 * my1) >> 15;
                INT32 mr1 = (mx1 * my0 + mx0 * my1) >> 15;
                INT32 ay0 = (INT16)BURN_ENDIAN_SWAP_INT16(ram[i0 & 0x1FF]);
                INT32 ay1 = (INT16)BURN_ENDIAN_SWAP_INT16(ram[(i0 + 1) & 0x1FF]);
                ram[i0++ & 0x1FF] = BURN_ENDIAN_SWAP_INT16((UINT16)(ay0 - ax0));
                ram[i2++ & 0x1FF] = BURN_ENDIAN_SWAP_INT16((UINT16)(ax0 + ay0));
                ram[i0++ & 0x1FF] = BURN_ENDIAN_SWAP_INT16((UINT16)(ay1 - mr1));
                ram[i2++ & 0x1FF] = BURN_ENDIAN_SWAP_INT16((UINT16)(ay1 + mr1));
            }
            i2 += mem63e;
            i1 += mem63e;
            i0 += mem63e;
        }
        mem63d <<= 1;
        mem63e = mem63f;
        mem63f >>= 1;
    }
    adsp->pc += 0x2B;
}

static UINT16 ReadData(UINT32 address)
{'''

viejo = '\nstatic UINT16 ReadData(UINT32 address)\n{'
assert s.count(viejo) == 1, 'ReadData'   # la declaración de arriba termina en ';': esta es la definición
i = s.rindex(viejo)
s = s[:i] + ATAJO + s[i + len(viejo):]

viejo = '''    pExtRAM32[offset] &= 0xFF;
    pExtRAM32[offset] |= value << 8;
#if LOG_MEMACC
    dcs_log(_T("dataram_w %x - %x\\n"), offset, value);
#endif
}'''
assert s.count(viejo) == 1, 'WriteData'
s = s.replace(viejo, '''    pExtRAM32[offset] &= 0xFF;
    pExtRAM32[offset] |= value << 8;
#if LOG_MEMACC
    dcs_log(_T("dataram_w %x - %x\\n"), offset, value);
#endif
    // ALLESIU: el atajo de la transformada (ver DcsAtajoTransformada).
    if (value == 2 && (offset == 0x4F8 || offset == 0x63D))
    {
        adsp2100_state *adsp = Adsp2100GetState();
        if ((offset == 0x4F8 && adsp->pc == 0xFD) || (offset == 0x63D && adsp->pc == 0xF6)) DcsAtajoTransformada(adsp);
    }
}''')
# La espera del búfer de sonido: el programa del DCS gira en "AR = I7; AF = AR - AY0; IF <cond> JUMP <al principio>"
# hasta que el puntero del búfer (I7, que FinalBurn Neo solo mueve en la interrupción de medio búfer) cruza un valor.
# Mientras gira no hace nada útil, y en la placa Wolf eso era casi todo el tiempo del sonido. El procesador se corre en
# tramos cortos y, si quedó en esa espera, se salta hasta la próxima interrupción (el resultado es el mismo).
ESPERA = r'''
// ALLESIU: ¿el procesador está en la espera del búfer? (ver herramientas/parche_dcs.py)
static UINT32 ReadProgram(UINT32 address);
static bool DcsEnEspera(UINT32 pc)
{
    for (UINT32 s = pc >= 2 ? pc - 2 : 0; s <= pc; s++)
    {
        if (ReadProgram(s) != 0x0D02A3 || ReadProgram(s + 1) != 0x26E20F) continue;   // AR = I7; AF = AR - AY0
        UINT32 salto = ReadProgram(s + 2);
        if ((salto >> 16) == 0x18 && ((salto >> 4) & 0x3FFF) == s) return true;       // IF <cond> JUMP s
    }
    return false;
}

void Dcs2kRun(INT32 cycles)'''
viejo = '\nvoid Dcs2kRun(INT32 cycles)'
assert s.count(viejo) == 1, 'Dcs2kRun'
s = s.replace(viejo, ESPERA)
viejo = '''		INT32 seg = (cycles < nIRQCountdown) ? cycles : nIRQCountdown;
		INT32 ran = (seg > 0) ? Adsp2100Run(seg) : 0;
		if (seg > 0 && ran <= 0) ran = seg;		// end of slice requested, don't spin'''
assert s.count(viejo) == 1, 'tramo'
s = s.replace(viejo, '''		INT32 seg = (cycles < nIRQCountdown) ? cycles : nIRQCountdown;
		// ALLESIU: en tramos de 256 ciclos; si quedó en la espera del búfer, el resto del tramo hasta la interrupción
		// ya está hecho (no saldría de ahí antes).
		INT32 ran = 0;
		while (ran < seg) {
			INT32 c = (seg - ran < 256) ? seg - ran : 256;
			INT32 r = Adsp2100Run(c);
			if (r <= 0) r = c;		// end of slice requested, don't spin
			ran += r;
			if (DcsEnEspera(Adsp2100GetState()->pc)) {
				// Los ciclos saltados cuentan como hechos (el sonido se sincroniza con el total de ciclos).
				Adsp2100GetState()->total_cycles += seg - ran;
				ran = seg;
				break;
			}
		}''')
open(destino, 'w', encoding='utf-8', errors='surrogateescape').write(s)
print('dcs2k.cpp con los atajos de la transformada y de la espera del búfer')
