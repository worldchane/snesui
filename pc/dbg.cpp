#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "../src/nucleo/snes.h"
void SnesVolcado(char* out, int max);
void PpuVolcado(char* o, int m);
int PpuColoresNoNegros();
void PpuVolcadoVram(char* o, int m);
bool ExisteArchivo(const char* r, uint64_t* t){ FILE*f=fopen(r,"rb"); if(!f)return false; if(t){_fseeki64(f,0,SEEK_END);*t=(uint64_t)_ftelli64(f);} fclose(f); return true; }
extern "C" {}
FILE* AbrirArchivo(const char* r, const char* m){ return fopen(r,m); }
int main(int c, char** v){
  extern bool apuLogOn; apuLogOn=false; char e[160]; if(!SnesCargar(v[1], e)){ printf("no cargo: %s\n", e); return 1; }
  static unsigned int fb[256*224]; static short au[2000]; char d[400];
  for(int f=0; f<900; f++){ SnesCuadro(fb,au,2000); if(f%60==0){ SnesVolcado(d,sizeof d); printf("cuadro %d: %s\n", f, d); PpuVolcado(d, sizeof d); printf("   %s  colores=%d\n", d, PpuColoresNoNegros()); } }
  return 0;
}
int LeerTexto(const char* r, char* buf, int max){ FILE*f=fopen(r,"rb"); if(!f)return -1; int n=(int)fread(buf,1,max-1,f); fclose(f); if(n<0)n=0; buf[n]=0; return n; }
