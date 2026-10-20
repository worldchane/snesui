// Prueba de memoria ejecutable para un recompilador (ver prueba_jit.cpp). Vacía si no se compila con -DPRUEBA_JIT.
#pragma once
const char* PruebaJit();   // hace la prueba (una vez) y devuelve el resultado en una línea
const char* PruebaGpu();   // la de la GPU para el N64 (ver prueba_gpu.cpp): Piglet, shaders desde texto y dibujo
