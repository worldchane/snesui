/* Se incluye antes de cada archivo de un núcleo libretro (-include) con -DNUCLEO=<prefijo>: renombra las funciones
 * retro_* a <prefijo>retro_* para que varios núcleos convivan en el mismo programa. api.h arma la tabla con esos
 * nombres. */
#ifndef NUCLEO_PREFIJO_H
#define NUCLEO_PREFIJO_H
#define NUCLEO_JUNTAR2(a, b) a##b
#define NUCLEO_JUNTAR(a, b) NUCLEO_JUNTAR2(a, b)
#define retro_set_environment NUCLEO_JUNTAR(NUCLEO, retro_set_environment)
#define retro_set_video_refresh NUCLEO_JUNTAR(NUCLEO, retro_set_video_refresh)
#define retro_set_audio_sample NUCLEO_JUNTAR(NUCLEO, retro_set_audio_sample)
#define retro_set_audio_sample_batch NUCLEO_JUNTAR(NUCLEO, retro_set_audio_sample_batch)
#define retro_set_input_poll NUCLEO_JUNTAR(NUCLEO, retro_set_input_poll)
#define retro_set_input_state NUCLEO_JUNTAR(NUCLEO, retro_set_input_state)
#define retro_init NUCLEO_JUNTAR(NUCLEO, retro_init)
#define retro_deinit NUCLEO_JUNTAR(NUCLEO, retro_deinit)
#define retro_api_version NUCLEO_JUNTAR(NUCLEO, retro_api_version)
#define retro_get_system_info NUCLEO_JUNTAR(NUCLEO, retro_get_system_info)
#define retro_get_system_av_info NUCLEO_JUNTAR(NUCLEO, retro_get_system_av_info)
#define retro_set_controller_port_device NUCLEO_JUNTAR(NUCLEO, retro_set_controller_port_device)
#define retro_reset NUCLEO_JUNTAR(NUCLEO, retro_reset)
#define retro_run NUCLEO_JUNTAR(NUCLEO, retro_run)
#define retro_serialize_size NUCLEO_JUNTAR(NUCLEO, retro_serialize_size)
#define retro_serialize NUCLEO_JUNTAR(NUCLEO, retro_serialize)
#define retro_unserialize NUCLEO_JUNTAR(NUCLEO, retro_unserialize)
#define retro_cheat_reset NUCLEO_JUNTAR(NUCLEO, retro_cheat_reset)
#define retro_cheat_set NUCLEO_JUNTAR(NUCLEO, retro_cheat_set)
#define retro_load_game NUCLEO_JUNTAR(NUCLEO, retro_load_game)
#define retro_load_game_special NUCLEO_JUNTAR(NUCLEO, retro_load_game_special)
#define retro_unload_game NUCLEO_JUNTAR(NUCLEO, retro_unload_game)
#define retro_get_region NUCLEO_JUNTAR(NUCLEO, retro_get_region)
#define retro_get_memory_data NUCLEO_JUNTAR(NUCLEO, retro_get_memory_data)
#define retro_get_memory_size NUCLEO_JUNTAR(NUCLEO, retro_get_memory_size)
/* Las tablas de opciones que casi todos los núcleos definen sin static (libretro_core_options.h). */
#define options_intl NUCLEO_JUNTAR(NUCLEO, options_intl)
#define option_defs_us NUCLEO_JUNTAR(NUCLEO, option_defs_us)
#define options_us NUCLEO_JUNTAR(NUCLEO, options_us)
#define option_cats_us NUCLEO_JUNTAR(NUCLEO, option_cats_us)
/* fopen de los núcleos (BIOS, roms de arranque...) va por la función de la app, con sus rutas (nucleo_libretro.cpp). */
#define fopen NucleoAbrirArchivo
/* En la consola, la hora local y los "locale" de la libc leen archivos de zona horaria y de idioma (piden fstat y
 * mmap al sistema). Los núcleos usan versiones simples (hora UTC, locale "C"), en archivos.c. */
#if defined(__FreeBSD__)
#define fstat nucleo_fstat
#define localtime nucleo_localtime
#define localtime_r nucleo_localtime_r
#define gmtime nucleo_gmtime
#define gmtime_r nucleo_gmtime_r
#define mktime nucleo_mktime
#define timegm nucleo_timegm
#define strftime nucleo_strftime
#define newlocale nucleo_newlocale
#define uselocale nucleo_uselocale
#define freelocale nucleo_freelocale
#define setlocale nucleo_setlocale
#endif
#endif
