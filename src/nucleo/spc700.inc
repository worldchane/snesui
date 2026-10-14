// Intérprete del SPC700 (el procesador del APU). Se incluye dentro de apu.cpp (usa spcLeer/spcEscribir y 'spc').
// Es un procesador de 8 bits parecido al 6502. YA es el par de 16 bits (Y la parte alta, A la baja). La bandera P
// elige si la "página directa" está en 0x0000 o en 0x0100.

static inline uint8_t rd(uint16_t a) { spc.ciclos++; return spcLeer(a); }
static inline void wr(uint16_t a, uint8_t v) { spc.ciclos++; spcEscribir(a, v); }
static inline uint8_t fetch() { return rd(spc.pc++); }
static inline uint16_t dpa(uint8_t o) { return ((spc.psw & P_P) ? 0x100 : 0) | o; }
static inline void push(uint8_t v) { wr(0x100 | spc.sp, v); spc.sp--; }
static inline uint8_t pop() { spc.sp++; return rd(0x100 | spc.sp); }

#define SZ(v) do { uint8_t _t = (uint8_t)(v); spc.psw = (spc.psw & ~(P_N | P_Z)) | ((_t) & 0x80 ? P_N : 0) | ((_t) ? 0 : P_Z); } while (0)

static uint8_t adc8(uint8_t x, uint8_t y)
{
    int c = spc.psw & P_C;
    int r = x + y + c;
    spc.psw &= ~(P_C | P_H | P_V | P_N | P_Z);
    if (((x ^ y ^ r) & 0x10)) spc.psw |= P_H;
    if ((~(x ^ y) & (x ^ r) & 0x80)) spc.psw |= P_V;
    if (r > 0xFF) spc.psw |= P_C;
    SZ(r);
    return (uint8_t)r;
}
static uint8_t sbc8(uint8_t x, uint8_t y) { return adc8(x, ~y); }
static void cmp8(uint8_t x, uint8_t y) { int r = x - y; spc.psw &= ~P_C; if (x >= y) spc.psw |= P_C; SZ(r); }
static uint8_t and8(uint8_t x, uint8_t y) { x &= y; SZ(x); return x; }
static uint8_t or8(uint8_t x, uint8_t y) { x |= y; SZ(x); return x; }
static uint8_t eor8(uint8_t x, uint8_t y) { x ^= y; SZ(x); return x; }
static uint8_t asl8(uint8_t v) { spc.psw = (spc.psw & ~P_C) | (v & 0x80 ? P_C : 0); v <<= 1; SZ(v); return v; }
static uint8_t lsr8(uint8_t v) { spc.psw = (spc.psw & ~P_C) | (v & 1 ? P_C : 0); v >>= 1; SZ(v); return v; }
static uint8_t rol8(uint8_t v) { int c = spc.psw & P_C; spc.psw = (spc.psw & ~P_C) | (v & 0x80 ? P_C : 0); v = v << 1 | c; SZ(v); return v; }
static uint8_t ror8(uint8_t v) { int c = spc.psw & P_C; spc.psw = (spc.psw & ~P_C) | (v & 1 ? P_C : 0); v = v >> 1 | (c ? 0x80 : 0); SZ(v); return v; }
static uint8_t inc8(uint8_t v) { v++; SZ(v); return v; }
static uint8_t dec8(uint8_t v) { v--; SZ(v); return v; }

static void rama(bool c) { int8_t o = (int8_t)fetch(); if (c) spc.pc += o; }

// 16 bits (MOVW/INCW/DECW/ADDW/SUBW/CMPW y MUL/DIV).
static uint16_t rd16dp(uint8_t o) { return rd(dpa(o)) | rd(dpa(o + 1)) << 8; }
static void wr16dp(uint8_t o, uint16_t v) { wr(dpa(o), v & 0xFF); wr(dpa(o + 1), v >> 8); }

void Spc700Paso()
{
    uint8_t op = fetch();
    switch (op)
    {
    // --- MOV A ---
    case 0xE8: spc.a = fetch(); SZ(spc.a); break;                                 // MOV A,#imm
    case 0xE6: spc.a = rd(((spc.psw & P_P) ? 0x100 : 0) | spc.x); SZ(spc.a); break;   // MOV A,(X)
    case 0xBF: spc.a = rd(((spc.psw & P_P) ? 0x100 : 0) | spc.x); SZ(spc.a); spc.x++; break;   // MOV A,(X)+
    case 0xE4: spc.a = rd(dpa(fetch())); SZ(spc.a); break;                        // MOV A,dp
    case 0xF4: spc.a = rd(dpa(fetch() + spc.x)); SZ(spc.a); break;                // MOV A,dp+X
    case 0xE5: { uint16_t a = fetch() | fetch() << 8; spc.a = rd(a); SZ(spc.a); break; }   // MOV A,!abs
    case 0xF5: { uint16_t a = (fetch() | fetch() << 8) + spc.x; spc.a = rd(a); SZ(spc.a); break; }   // MOV A,!abs+X
    case 0xF6: { uint16_t a = (fetch() | fetch() << 8) + spc.y; spc.a = rd(a); SZ(spc.a); break; }   // MOV A,!abs+Y
    case 0xE7: { uint8_t o = fetch() + spc.x; uint16_t a = rd(dpa(o)) | rd(dpa(o + 1)) << 8; spc.a = rd(a); SZ(spc.a); break; }   // MOV A,[dp+X]
    case 0xF7: { uint8_t o = fetch(); uint16_t a = (rd(dpa(o)) | rd(dpa(o + 1)) << 8) + spc.y; spc.a = rd(a); SZ(spc.a); break; } // MOV A,[dp]+Y
    // --- MOV X/Y ---
    case 0xCD: spc.x = fetch(); SZ(spc.x); break;                                 // MOV X,#imm
    case 0xF8: spc.x = rd(dpa(fetch())); SZ(spc.x); break;                        // MOV X,dp
    case 0xF9: spc.x = rd(dpa(fetch() + spc.y)); SZ(spc.x); break;                // MOV X,dp+Y
    case 0xE9: { uint16_t a = fetch() | fetch() << 8; spc.x = rd(a); SZ(spc.x); break; }   // MOV X,!abs
    case 0x8D: spc.y = fetch(); SZ(spc.y); break;                                 // MOV Y,#imm
    case 0xEB: spc.y = rd(dpa(fetch())); SZ(spc.y); break;                        // MOV Y,dp
    case 0xFB: spc.y = rd(dpa(fetch() + spc.x)); SZ(spc.y); break;                // MOV Y,dp+X
    case 0xEC: { uint16_t a = fetch() | fetch() << 8; spc.y = rd(a); SZ(spc.y); break; }   // MOV Y,!abs
    // --- MOV a memoria ---
    case 0xC6: wr(((spc.psw & P_P) ? 0x100 : 0) | spc.x, spc.a); break;           // MOV (X),A
    case 0xAF: wr(((spc.psw & P_P) ? 0x100 : 0) | spc.x, spc.a); spc.x++; break;  // MOV (X)+,A
    case 0xC4: wr(dpa(fetch()), spc.a); break;                                    // MOV dp,A
    case 0xD4: wr(dpa(fetch() + spc.x), spc.a); break;                            // MOV dp+X,A
    case 0xC5: { uint16_t a = fetch() | fetch() << 8; wr(a, spc.a); break; }      // MOV !abs,A
    case 0xD5: { uint16_t a = (fetch() | fetch() << 8) + spc.x; wr(a, spc.a); break; }   // MOV !abs+X,A
    case 0xD6: { uint16_t a = (fetch() | fetch() << 8) + spc.y; wr(a, spc.a); break; }   // MOV !abs+Y,A
    case 0xC7: { uint8_t o = fetch() + spc.x; uint16_t a = rd(dpa(o)) | rd(dpa(o + 1)) << 8; wr(a, spc.a); break; }   // MOV [dp+X],A
    case 0xD7: { uint8_t o = fetch(); uint16_t a = (rd(dpa(o)) | rd(dpa(o + 1)) << 8) + spc.y; wr(a, spc.a); break; } // MOV [dp]+Y,A
    case 0xD8: wr(dpa(fetch()), spc.x); break;                                    // MOV dp,X
    case 0xD9: wr(dpa(fetch() + spc.y), spc.x); break;                            // MOV dp+Y,X
    case 0xC9: { uint16_t a = fetch() | fetch() << 8; wr(a, spc.x); break; }      // MOV !abs,X
    case 0xCB: wr(dpa(fetch()), spc.y); break;                                    // MOV dp,Y
    case 0xDB: wr(dpa(fetch() + spc.x), spc.y); break;                            // MOV dp+X,Y
    case 0xCC: { uint16_t a = fetch() | fetch() << 8; wr(a, spc.y); break; }      // MOV !abs,Y
    case 0xFA: { uint8_t s = fetch(); uint8_t d = fetch(); wr(dpa(d), rd(dpa(s))); break; }   // MOV dp,dp
    case 0x8F: { uint8_t v = fetch(); wr(dpa(fetch()), v); break; }               // MOV dp,#imm
    // --- transferencias de registros ---
    case 0x7D: spc.a = spc.x; SZ(spc.a); break;   // MOV A,X
    case 0xDD: spc.a = spc.y; SZ(spc.a); break;   // MOV A,Y
    case 0x5D: spc.x = spc.a; SZ(spc.x); break;   // MOV X,A
    case 0xFD: spc.y = spc.a; SZ(spc.y); break;   // MOV Y,A
    case 0x9D: spc.x = spc.sp; SZ(spc.x); break;  // MOV X,SP
    case 0xBD: spc.sp = spc.x; break;             // MOV SP,X
    // --- aritmética con A ---
    case 0x88: spc.a = adc8(spc.a, fetch()); break;
    case 0x86: spc.a = adc8(spc.a, rd(((spc.psw & P_P) ? 0x100 : 0) | spc.x)); break;
    case 0x84: spc.a = adc8(spc.a, rd(dpa(fetch()))); break;
    case 0x94: spc.a = adc8(spc.a, rd(dpa(fetch() + spc.x))); break;
    case 0x85: { uint16_t a = fetch() | fetch() << 8; spc.a = adc8(spc.a, rd(a)); break; }
    case 0x95: { uint16_t a = (fetch() | fetch() << 8) + spc.x; spc.a = adc8(spc.a, rd(a)); break; }
    case 0x96: { uint16_t a = (fetch() | fetch() << 8) + spc.y; spc.a = adc8(spc.a, rd(a)); break; }
    case 0x87: { uint8_t o = fetch() + spc.x; uint16_t a = rd(dpa(o)) | rd(dpa(o + 1)) << 8; spc.a = adc8(spc.a, rd(a)); break; }
    case 0x97: { uint8_t o = fetch(); uint16_t a = (rd(dpa(o)) | rd(dpa(o + 1)) << 8) + spc.y; spc.a = adc8(spc.a, rd(a)); break; }
    case 0x99: { uint16_t ax = ((spc.psw & P_P) ? 0x100 : 0) | spc.x, ay = ((spc.psw & P_P) ? 0x100 : 0) | spc.y; wr(ax, adc8(rd(ax), rd(ay))); break; }   // ADC (X),(Y)
    case 0x89: { uint8_t s = fetch(); uint16_t d = dpa(fetch()); wr(d, adc8(rd(d), rd(dpa(s)))); break; }   // ADC dp,dp
    case 0x98: { uint8_t im = fetch(); uint16_t d = dpa(fetch()); wr(d, adc8(rd(d), im)); break; }          // ADC dp,#imm
    case 0xA8: spc.a = sbc8(spc.a, fetch()); break;
    case 0xA6: spc.a = sbc8(spc.a, rd(((spc.psw & P_P) ? 0x100 : 0) | spc.x)); break;
    case 0xA4: spc.a = sbc8(spc.a, rd(dpa(fetch()))); break;
    case 0xB4: spc.a = sbc8(spc.a, rd(dpa(fetch() + spc.x))); break;
    case 0xA5: { uint16_t a = fetch() | fetch() << 8; spc.a = sbc8(spc.a, rd(a)); break; }
    case 0xB5: { uint16_t a = (fetch() | fetch() << 8) + spc.x; spc.a = sbc8(spc.a, rd(a)); break; }
    case 0xB6: { uint16_t a = (fetch() | fetch() << 8) + spc.y; spc.a = sbc8(spc.a, rd(a)); break; }
    case 0xA7: { uint8_t o = fetch() + spc.x; uint16_t a = rd(dpa(o)) | rd(dpa(o + 1)) << 8; spc.a = sbc8(spc.a, rd(a)); break; }
    case 0xB7: { uint8_t o = fetch(); uint16_t a = (rd(dpa(o)) | rd(dpa(o + 1)) << 8) + spc.y; spc.a = sbc8(spc.a, rd(a)); break; }
    case 0xB9: { uint16_t ax = ((spc.psw & P_P) ? 0x100 : 0) | spc.x, ay = ((spc.psw & P_P) ? 0x100 : 0) | spc.y; wr(ax, sbc8(rd(ax), rd(ay))); break; }
    case 0xA9: { uint8_t s = fetch(); uint16_t d = dpa(fetch()); wr(d, sbc8(rd(d), rd(dpa(s)))); break; }
    case 0xB8: { uint8_t im = fetch(); uint16_t d = dpa(fetch()); wr(d, sbc8(rd(d), im)); break; }
    // --- CMP ---
    case 0x68: cmp8(spc.a, fetch()); break;
    case 0x66: cmp8(spc.a, rd(((spc.psw & P_P) ? 0x100 : 0) | spc.x)); break;
    case 0x64: cmp8(spc.a, rd(dpa(fetch()))); break;
    case 0x74: cmp8(spc.a, rd(dpa(fetch() + spc.x))); break;
    case 0x65: { uint16_t a = fetch() | fetch() << 8; cmp8(spc.a, rd(a)); break; }
    case 0x75: { uint16_t a = (fetch() | fetch() << 8) + spc.x; cmp8(spc.a, rd(a)); break; }
    case 0x76: { uint16_t a = (fetch() | fetch() << 8) + spc.y; cmp8(spc.a, rd(a)); break; }
    case 0x67: { uint8_t o = fetch() + spc.x; uint16_t a = rd(dpa(o)) | rd(dpa(o + 1)) << 8; cmp8(spc.a, rd(a)); break; }
    case 0x77: { uint8_t o = fetch(); uint16_t a = (rd(dpa(o)) | rd(dpa(o + 1)) << 8) + spc.y; cmp8(spc.a, rd(a)); break; }
    case 0x79: { uint16_t ax = ((spc.psw & P_P) ? 0x100 : 0) | spc.x, ay = ((spc.psw & P_P) ? 0x100 : 0) | spc.y; cmp8(rd(ax), rd(ay)); break; }
    case 0x69: { uint8_t s = fetch(); uint16_t d = dpa(fetch()); cmp8(rd(d), rd(dpa(s))); break; }
    case 0x78: { uint8_t im = fetch(); uint16_t d = dpa(fetch()); cmp8(rd(d), im); break; }
    case 0xC8: cmp8(spc.x, fetch()); break;                     // CMP X,#imm
    case 0x3E: cmp8(spc.x, rd(dpa(fetch()))); break;            // CMP X,dp
    case 0x1E: { uint16_t a = fetch() | fetch() << 8; cmp8(spc.x, rd(a)); break; }   // CMP X,!abs
    case 0xAD: cmp8(spc.y, fetch()); break;                     // CMP Y,#imm
    case 0x7E: cmp8(spc.y, rd(dpa(fetch()))); break;            // CMP Y,dp
    case 0x5E: { uint16_t a = fetch() | fetch() << 8; cmp8(spc.y, rd(a)); break; }   // CMP Y,!abs
    // --- AND/OR/EOR ---
    case 0x28: spc.a = and8(spc.a, fetch()); break;
    case 0x26: spc.a = and8(spc.a, rd(((spc.psw & P_P) ? 0x100 : 0) | spc.x)); break;
    case 0x24: spc.a = and8(spc.a, rd(dpa(fetch()))); break;
    case 0x34: spc.a = and8(spc.a, rd(dpa(fetch() + spc.x))); break;
    case 0x25: { uint16_t a = fetch() | fetch() << 8; spc.a = and8(spc.a, rd(a)); break; }
    case 0x35: { uint16_t a = (fetch() | fetch() << 8) + spc.x; spc.a = and8(spc.a, rd(a)); break; }
    case 0x36: { uint16_t a = (fetch() | fetch() << 8) + spc.y; spc.a = and8(spc.a, rd(a)); break; }
    case 0x27: { uint8_t o = fetch() + spc.x; uint16_t a = rd(dpa(o)) | rd(dpa(o + 1)) << 8; spc.a = and8(spc.a, rd(a)); break; }
    case 0x37: { uint8_t o = fetch(); uint16_t a = (rd(dpa(o)) | rd(dpa(o + 1)) << 8) + spc.y; spc.a = and8(spc.a, rd(a)); break; }
    case 0x29: { uint8_t s = fetch(); uint16_t d = dpa(fetch()); wr(d, and8(rd(d), rd(dpa(s)))); break; }
    case 0x38: { uint8_t im = fetch(); uint16_t d = dpa(fetch()); wr(d, and8(rd(d), im)); break; }
    case 0x08: spc.a = or8(spc.a, fetch()); break;
    case 0x06: spc.a = or8(spc.a, rd(((spc.psw & P_P) ? 0x100 : 0) | spc.x)); break;
    case 0x04: spc.a = or8(spc.a, rd(dpa(fetch()))); break;
    case 0x14: spc.a = or8(spc.a, rd(dpa(fetch() + spc.x))); break;
    case 0x05: { uint16_t a = fetch() | fetch() << 8; spc.a = or8(spc.a, rd(a)); break; }
    case 0x15: { uint16_t a = (fetch() | fetch() << 8) + spc.x; spc.a = or8(spc.a, rd(a)); break; }
    case 0x16: { uint16_t a = (fetch() | fetch() << 8) + spc.y; spc.a = or8(spc.a, rd(a)); break; }
    case 0x07: { uint8_t o = fetch() + spc.x; uint16_t a = rd(dpa(o)) | rd(dpa(o + 1)) << 8; spc.a = or8(spc.a, rd(a)); break; }
    case 0x17: { uint8_t o = fetch(); uint16_t a = (rd(dpa(o)) | rd(dpa(o + 1)) << 8) + spc.y; spc.a = or8(spc.a, rd(a)); break; }
    case 0x09: { uint8_t s = fetch(); uint16_t d = dpa(fetch()); wr(d, or8(rd(d), rd(dpa(s)))); break; }
    case 0x18: { uint8_t im = fetch(); uint16_t d = dpa(fetch()); wr(d, or8(rd(d), im)); break; }
    case 0x48: spc.a = eor8(spc.a, fetch()); break;
    case 0x46: spc.a = eor8(spc.a, rd(((spc.psw & P_P) ? 0x100 : 0) | spc.x)); break;
    case 0x44: spc.a = eor8(spc.a, rd(dpa(fetch()))); break;
    case 0x54: spc.a = eor8(spc.a, rd(dpa(fetch() + spc.x))); break;
    case 0x45: { uint16_t a = fetch() | fetch() << 8; spc.a = eor8(spc.a, rd(a)); break; }
    case 0x55: { uint16_t a = (fetch() | fetch() << 8) + spc.x; spc.a = eor8(spc.a, rd(a)); break; }
    case 0x56: { uint16_t a = (fetch() | fetch() << 8) + spc.y; spc.a = eor8(spc.a, rd(a)); break; }
    case 0x47: { uint8_t o = fetch() + spc.x; uint16_t a = rd(dpa(o)) | rd(dpa(o + 1)) << 8; spc.a = eor8(spc.a, rd(a)); break; }
    case 0x57: { uint8_t o = fetch(); uint16_t a = (rd(dpa(o)) | rd(dpa(o + 1)) << 8) + spc.y; spc.a = eor8(spc.a, rd(a)); break; }
    case 0x49: { uint8_t s = fetch(); uint16_t d = dpa(fetch()); wr(d, eor8(rd(d), rd(dpa(s)))); break; }
    case 0x58: { uint8_t im = fetch(); uint16_t d = dpa(fetch()); wr(d, eor8(rd(d), im)); break; }
    // --- INC/DEC ---
    case 0xBC: spc.a = inc8(spc.a); break; case 0x3D: spc.x = inc8(spc.x); break; case 0xFC: spc.y = inc8(spc.y); break;
    case 0x9C: spc.a = dec8(spc.a); break; case 0x1D: spc.x = dec8(spc.x); break; case 0xDC: spc.y = dec8(spc.y); break;
    case 0xAB: { uint16_t d = dpa(fetch()); wr(d, inc8(rd(d))); break; }           // INC dp
    case 0xBB: { uint16_t d = dpa(fetch() + spc.x); wr(d, inc8(rd(d))); break; }   // INC dp+X
    case 0xAC: { uint16_t a = fetch() | fetch() << 8; wr(a, inc8(rd(a))); break; } // INC !abs
    case 0x8B: { uint16_t d = dpa(fetch()); wr(d, dec8(rd(d))); break; }           // DEC dp
    case 0x9B: { uint16_t d = dpa(fetch() + spc.x); wr(d, dec8(rd(d))); break; }   // DEC dp+X
    case 0x8C: { uint16_t a = fetch() | fetch() << 8; wr(a, dec8(rd(a))); break; } // DEC !abs
    // --- desplazamientos ---
    case 0x1C: spc.a = asl8(spc.a); break; case 0x0B: { uint16_t d = dpa(fetch()); wr(d, asl8(rd(d))); break; }
    case 0x1B: { uint16_t d = dpa(fetch() + spc.x); wr(d, asl8(rd(d))); break; } case 0x0C: { uint16_t a = fetch() | fetch() << 8; wr(a, asl8(rd(a))); break; }
    case 0x5C: spc.a = lsr8(spc.a); break; case 0x4B: { uint16_t d = dpa(fetch()); wr(d, lsr8(rd(d))); break; }
    case 0x5B: { uint16_t d = dpa(fetch() + spc.x); wr(d, lsr8(rd(d))); break; } case 0x4C: { uint16_t a = fetch() | fetch() << 8; wr(a, lsr8(rd(a))); break; }
    case 0x3C: spc.a = rol8(spc.a); break; case 0x2B: { uint16_t d = dpa(fetch()); wr(d, rol8(rd(d))); break; }
    case 0x3B: { uint16_t d = dpa(fetch() + spc.x); wr(d, rol8(rd(d))); break; } case 0x2C: { uint16_t a = fetch() | fetch() << 8; wr(a, rol8(rd(a))); break; }
    case 0x7C: spc.a = ror8(spc.a); break; case 0x6B: { uint16_t d = dpa(fetch()); wr(d, ror8(rd(d))); break; }
    case 0x7B: { uint16_t d = dpa(fetch() + spc.x); wr(d, ror8(rd(d))); break; } case 0x6C: { uint16_t a = fetch() | fetch() << 8; wr(a, ror8(rd(a))); break; }
    case 0x9F: spc.a = (spc.a >> 4) | (spc.a << 4); SZ(spc.a); break;              // XCN
    // --- 16 bits ---
    case 0xBA: { uint8_t o = fetch(); uint16_t v = rd16dp(o); spc.a = v; spc.y = v >> 8; spc.psw = (spc.psw & ~(P_N | P_Z)) | (v & 0x8000 ? P_N : 0) | (v ? 0 : P_Z); break; }   // MOVW YA,dp
    case 0xDA: { uint8_t o = fetch(); wr16dp(o, spc.a | spc.y << 8); break; }      // MOVW dp,YA
    case 0x3A: { uint8_t o = fetch(); uint16_t v = rd16dp(o) + 1; wr16dp(o, v); spc.psw = (spc.psw & ~(P_N | P_Z)) | (v & 0x8000 ? P_N : 0) | (v ? 0 : P_Z); break; }   // INCW
    case 0x1A: { uint8_t o = fetch(); uint16_t v = rd16dp(o) - 1; wr16dp(o, v); spc.psw = (spc.psw & ~(P_N | P_Z)) | (v & 0x8000 ? P_N : 0) | (v ? 0 : P_Z); break; }   // DECW
    case 0x7A: { uint8_t o = fetch(); uint32_t ya = spc.a | spc.y << 8; uint32_t m = rd16dp(o); uint32_t r = ya + m; spc.psw &= ~(P_C | P_H | P_V | P_N | P_Z); if (((ya ^ m ^ r) & 0x1000)) spc.psw |= P_H; if ((~(ya ^ m) & (ya ^ r) & 0x8000)) spc.psw |= P_V; if (r > 0xFFFF) spc.psw |= P_C; if (r & 0x8000) spc.psw |= P_N; if (!(r & 0xFFFF)) spc.psw |= P_Z; spc.a = r; spc.y = r >> 8; break; }   // ADDW
    case 0x9A: { uint8_t o = fetch(); int32_t ya = spc.a | spc.y << 8; int32_t m = rd16dp(o); int32_t r = ya - m; spc.psw &= ~(P_C | P_H | P_V | P_N | P_Z); if (ya >= m) spc.psw |= P_C; if (((ya ^ m) & (ya ^ r) & 0x8000)) spc.psw |= P_V; if (((ya ^ m ^ r) & 0x1000) == 0) spc.psw |= P_H; if (r & 0x8000) spc.psw |= P_N; if (!(r & 0xFFFF)) spc.psw |= P_Z; spc.a = r; spc.y = r >> 8; break; }   // SUBW
    case 0x5A: { uint8_t o = fetch(); uint16_t ya = spc.a | spc.y << 8, m = rd16dp(o); uint16_t r = ya - m; spc.psw &= ~(P_C | P_N | P_Z); if (ya >= m) spc.psw |= P_C; if (r & 0x8000) spc.psw |= P_N; if (!r) spc.psw |= P_Z; break; }   // CMPW
    case 0xCF: { uint16_t r = spc.y * spc.a; spc.a = r & 0xFF; spc.y = r >> 8; spc.psw = (spc.psw & ~(P_N | P_Z)) | (spc.y & 0x80 ? P_N : 0) | (spc.y ? 0 : P_Z); break; }   // MUL YA
    case 0x9E: {   // DIV YA,X
        uint16_t ya = spc.a | spc.y << 8;
        if (spc.x == 0) { spc.a = 0xFF; spc.y = 0xFF; }
        else { spc.a = ya / spc.x > 0xFF ? 0xFF : ya / spc.x; spc.y = ya % spc.x; }
        spc.psw = (spc.psw & ~(P_N | P_Z)) | (spc.a & 0x80 ? P_N : 0) | (spc.a ? 0 : P_Z);
        break; }
    // --- DAA/DAS ---
    case 0xDF: { if ((spc.psw & P_H) || (spc.a & 0xF) > 9) { if ((int)spc.a + 6 > 0xFF) spc.psw |= P_C; spc.a += 6; } if ((spc.psw & P_C) || spc.a > 0x9F) { spc.a += 0x60; spc.psw |= P_C; } SZ(spc.a); break; }   // DAA
    case 0xBE: { if (!(spc.psw & P_H) || (spc.a & 0xF) > 9) spc.a -= 6; if (!(spc.psw & P_C) || spc.a > 0x9F) { spc.a -= 0x60; spc.psw &= ~P_C; } SZ(spc.a); break; }   // DAS
    // --- ramas ---
    case 0x2F: rama(true); break;                               // BRA
    case 0xF0: rama(spc.psw & P_Z); break;                      // BEQ
    case 0xD0: rama(!(spc.psw & P_Z)); break;                   // BNE
    case 0xB0: rama(spc.psw & P_C); break;                      // BCS
    case 0x90: rama(!(spc.psw & P_C)); break;                   // BCC
    case 0x70: rama(spc.psw & P_V); break;                      // BVS
    case 0x50: rama(!(spc.psw & P_V)); break;                   // BVC
    case 0x30: rama(spc.psw & P_N); break;                      // BMI
    case 0x10: rama(!(spc.psw & P_N)); break;                   // BPL
    case 0x2E: { uint16_t d = dpa(fetch()); uint8_t v = rd(d); int8_t o = (int8_t)fetch(); if (spc.a != v) spc.pc += o; break; }   // CBNE dp
    case 0xDE: { uint16_t d = dpa(fetch() + spc.x); uint8_t v = rd(d); int8_t o = (int8_t)fetch(); if (spc.a != v) spc.pc += o; break; }   // CBNE dp+X
    case 0x6E: { uint16_t d = dpa(fetch()); uint8_t v = rd(d) - 1; wr(d, v); int8_t o = (int8_t)fetch(); if (v) spc.pc += o; break; }   // DBNZ dp
    case 0xFE: { int8_t o = (int8_t)fetch(); spc.y--; if (spc.y) spc.pc += o; break; }   // DBNZ Y
    // --- CALL/RET ---
    case 0x3F: { uint16_t a = fetch() | fetch() << 8; push(spc.pc >> 8); push(spc.pc & 0xFF); spc.pc = a; break; }   // CALL
    case 0x4F: { uint8_t o = fetch(); push(spc.pc >> 8); push(spc.pc & 0xFF); spc.pc = 0xFF00 | o; break; }          // PCALL
    case 0x6F: { spc.pc = pop(); spc.pc |= pop() << 8; break; }   // RET
    case 0x7F: { spc.psw = pop(); spc.pc = pop(); spc.pc |= pop() << 8; break; }   // RETI
    // --- pila ---
    case 0x2D: push(spc.a); break; case 0x4D: push(spc.x); break; case 0x6D: push(spc.y); break; case 0x0D: push(spc.psw); break;
    case 0xAE: spc.a = pop(); break; case 0xCE: spc.x = pop(); break; case 0xEE: spc.y = pop(); break; case 0x8E: spc.psw = pop(); break;
    // --- banderas ---
    case 0x60: spc.psw &= ~P_C; break; case 0x80: spc.psw |= P_C; break; case 0xED: spc.psw ^= P_C; break;
    case 0xE0: spc.psw &= ~(P_V | P_H); break;
    case 0x20: spc.psw &= ~P_P; break; case 0x40: spc.psw |= P_P; break;
    case 0xA0: spc.psw |= P_I; break; case 0xC0: spc.psw &= ~P_I; break;
    // --- bits en memoria ---
    case 0x02: case 0x22: case 0x42: case 0x62: case 0x82: case 0xA2: case 0xC2: case 0xE2:   // SET1 dp.bit
        { uint16_t d = dpa(fetch()); wr(d, rd(d) | (1 << (op >> 5))); break; }
    case 0x12: case 0x32: case 0x52: case 0x72: case 0x92: case 0xB2: case 0xD2: case 0xF2:   // CLR1 dp.bit
        { uint16_t d = dpa(fetch()); wr(d, rd(d) & ~(1 << (op >> 5))); break; }
    case 0x03: case 0x23: case 0x43: case 0x63: case 0x83: case 0xA3: case 0xC3: case 0xE3:   // BBS dp.bit,rel
        { uint16_t d = dpa(fetch()); uint8_t v = rd(d); int8_t o = (int8_t)fetch(); if (v & (1 << (op >> 5))) spc.pc += o; break; }
    case 0x13: case 0x33: case 0x53: case 0x73: case 0x93: case 0xB3: case 0xD3: case 0xF3:   // BBC dp.bit,rel
        { uint16_t d = dpa(fetch()); uint8_t v = rd(d); int8_t o = (int8_t)fetch(); if (!(v & (1 << (op >> 5)))) spc.pc += o; break; }
    case 0x0E: { uint16_t a = fetch() | fetch() << 8; uint8_t v = rd(a); spc.psw = (spc.psw & ~(P_N | P_Z)) | ((spc.a & v) ? 0 : P_Z) | ((spc.a - v) & 0x80 ? P_N : 0); wr(a, v | spc.a); break; }   // TSET1
    case 0x4E: { uint16_t a = fetch() | fetch() << 8; uint8_t v = rd(a); spc.psw = (spc.psw & ~(P_N | P_Z)) | ((spc.a & v) ? 0 : P_Z) | ((spc.a - v) & 0x80 ? P_N : 0); wr(a, v & ~spc.a); break; }   // TCLR1
    // --- bits de memoria + carry (AND1/OR1/EOR1/NOT1/MOV1) ---
    case 0x0A: { uint16_t w = fetch() | fetch() << 8; int bit = (rd(w & 0x1FFF) >> (w >> 13)) & 1; if (bit) spc.psw |= P_C; break; }    // OR1 C,mem.bit
    case 0x2A: { uint16_t w = fetch() | fetch() << 8; int bit = (rd(w & 0x1FFF) >> (w >> 13)) & 1; if (!bit) spc.psw |= P_C; break; }   // OR1 C,/mem.bit
    case 0x4A: { uint16_t w = fetch() | fetch() << 8; int bit = (rd(w & 0x1FFF) >> (w >> 13)) & 1; if (!bit) spc.psw &= ~P_C; break; } // AND1 C,mem.bit
    case 0x6A: { uint16_t w = fetch() | fetch() << 8; int bit = (rd(w & 0x1FFF) >> (w >> 13)) & 1; if (bit) spc.psw &= ~P_C; break; }  // AND1 C,/mem.bit
    case 0x8A: { uint16_t w = fetch() | fetch() << 8; uint16_t a = w & 0x1FFF; int b = w >> 13; int bit = (rd(a) >> b) & 1; if (bit) spc.psw ^= P_C; break; }   // EOR1
    case 0xEA: { uint16_t w = fetch() | fetch() << 8; uint16_t a = w & 0x1FFF; int b = w >> 13; wr(a, rd(a) ^ (1 << b)); break; }   // NOT1
    case 0xAA: { uint16_t w = fetch() | fetch() << 8; uint16_t a = w & 0x1FFF; int b = w >> 13; spc.psw = (spc.psw & ~P_C) | (((rd(a) >> b) & 1) ? P_C : 0); break; }   // MOV1 C,mem.bit
    case 0xCA: { uint16_t w = fetch() | fetch() << 8; uint16_t a = w & 0x1FFF; int b = w >> 13; uint8_t v = rd(a); v = (v & ~(1 << b)) | ((spc.psw & P_C ? 1 : 0) << b); wr(a, v); break; }   // MOV1 mem.bit,C
    // --- TCALL n ---
    case 0x01: case 0x11: case 0x21: case 0x31: case 0x41: case 0x51: case 0x61: case 0x71:
    case 0x81: case 0x91: case 0xA1: case 0xB1: case 0xC1: case 0xD1: case 0xE1: case 0xF1:
        { int n = op >> 4; uint16_t v = 0xFFDE - n * 2; push(spc.pc >> 8); push(spc.pc & 0xFF); spc.pc = rd(v) | rd(v + 1) << 8; break; }
    // --- saltos que faltaban ---
    case 0x5F: { uint16_t a = fetch() | fetch() << 8; spc.pc = a; break; }                                  // JMP !abs
    case 0x1F: { uint16_t a = (fetch() | fetch() << 8) + spc.x; spc.pc = rd(a) | rd(a + 1) << 8; break; }   // JMP [!abs+X]
    case 0x0F: { push(spc.pc >> 8); push(spc.pc & 0xFF); push(spc.psw); spc.psw = (spc.psw | P_B) & ~P_I; spc.pc = rd(0xFFDE) | rd(0xFFDF) << 8; break; }   // BRK
    // --- (X),(Y) que faltaban ---
    case 0x19: { uint16_t ax = ((spc.psw & P_P) ? 0x100 : 0) | spc.x, ay = ((spc.psw & P_P) ? 0x100 : 0) | spc.y; wr(ax, or8(rd(ax), rd(ay))); break; }    // OR (X),(Y)
    case 0x39: { uint16_t ax = ((spc.psw & P_P) ? 0x100 : 0) | spc.x, ay = ((spc.psw & P_P) ? 0x100 : 0) | spc.y; wr(ax, and8(rd(ax), rd(ay))); break; }   // AND (X),(Y)
    case 0x59: { uint16_t ax = ((spc.psw & P_P) ? 0x100 : 0) | spc.x, ay = ((spc.psw & P_P) ? 0x100 : 0) | spc.y; wr(ax, eor8(rd(ax), rd(ay))); break; }   // EOR (X),(Y)
    // --- control ---
    case 0x00: break;                   // NOP
    case 0xEF: spc.ciclos += 2; break;  // SLEEP: se queda esperando
    case 0xFF: spc.ciclos += 2; break;  // STOP
    default: break;
    }
}
