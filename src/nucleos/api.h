// Las funciones de un núcleo libretro enlazado con un prefijo (ver prefijo.h), en una tabla.
#pragma once
#include "libretro.h"

struct ApiLibretro
{
    void (*set_environment)(retro_environment_t);
    void (*set_video_refresh)(retro_video_refresh_t);
    void (*set_audio_sample)(retro_audio_sample_t);
    void (*set_audio_sample_batch)(retro_audio_sample_batch_t);
    void (*set_input_poll)(retro_input_poll_t);
    void (*set_input_state)(retro_input_state_t);
    void (*init)(void);
    void (*deinit)(void);
    void (*get_system_info)(struct retro_system_info*);
    void (*get_system_av_info)(struct retro_system_av_info*);
    void (*set_controller_port_device)(unsigned, unsigned);
    void (*reset)(void);
    void (*run)(void);
    size_t (*serialize_size)(void);
    bool (*serialize)(void*, size_t);
    bool (*unserialize)(const void*, size_t);
    bool (*load_game)(const struct retro_game_info*);
    void (*unload_game)(void);
    void* (*get_memory_data)(unsigned);
    size_t (*get_memory_size)(unsigned);
    void (*cheat_reset)(void);
    void (*cheat_set)(unsigned, bool, const char*);
};

#define API_LIBRETRO_DECLARAR(p)                                                                                        \
    extern "C" {                                                                                                       \
    void p##retro_set_environment(retro_environment_t);                                                                \
    void p##retro_set_video_refresh(retro_video_refresh_t);                                                            \
    void p##retro_set_audio_sample(retro_audio_sample_t);                                                              \
    void p##retro_set_audio_sample_batch(retro_audio_sample_batch_t);                                                  \
    void p##retro_set_input_poll(retro_input_poll_t);                                                                  \
    void p##retro_set_input_state(retro_input_state_t);                                                                \
    void p##retro_init(void);                                                                                          \
    void p##retro_deinit(void);                                                                                        \
    void p##retro_get_system_info(struct retro_system_info*);                                                          \
    void p##retro_get_system_av_info(struct retro_system_av_info*);                                                    \
    void p##retro_set_controller_port_device(unsigned, unsigned);                                                      \
    void p##retro_reset(void);                                                                                         \
    void p##retro_run(void);                                                                                           \
    size_t p##retro_serialize_size(void);                                                                              \
    bool p##retro_serialize(void*, size_t);                                                                            \
    bool p##retro_unserialize(const void*, size_t);                                                                    \
    bool p##retro_load_game(const struct retro_game_info*);                                                            \
    void p##retro_unload_game(void);                                                                                   \
    void* p##retro_get_memory_data(unsigned);                                                                          \
    size_t p##retro_get_memory_size(unsigned);                                                                         \
    void p##retro_cheat_reset(void);                                                                                   \
    void p##retro_cheat_set(unsigned, bool, const char*);                                                              \
    }

#define API_LIBRETRO_TABLA(p)                                                                                           \
    {                                                                                                                  \
        p##retro_set_environment, p##retro_set_video_refresh, p##retro_set_audio_sample,                               \
        p##retro_set_audio_sample_batch, p##retro_set_input_poll, p##retro_set_input_state, p##retro_init,             \
        p##retro_deinit, p##retro_get_system_info, p##retro_get_system_av_info, p##retro_set_controller_port_device,   \
        p##retro_reset, p##retro_run, p##retro_serialize_size, p##retro_serialize, p##retro_unserialize,               \
        p##retro_load_game, p##retro_unload_game, p##retro_get_memory_data, p##retro_get_memory_size,                  \
        p##retro_cheat_reset, p##retro_cheat_set                                                                       \
    }
