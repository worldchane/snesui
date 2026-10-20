/* ALLESIU: la máquina de N64 (struct device g_dev, 60 MB: la memoria, las tablas de bloques del procesador...) no va
   fija en el programa: es un puntero y se pide al abrir el núcleo (retro_init) y se suelta al cerrarlo (retro_deinit).
   Fija ocupaba memoria de la app aunque no se jugara a N64 y la dejaba sin espacio para las demás consolas. Va con
   -include después de renombrar/n64.h, que la llamaba n64_g_dev. */
#undef g_dev
#define g_dev (*n64_dispositivo)
