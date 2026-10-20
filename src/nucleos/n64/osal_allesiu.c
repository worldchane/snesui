/* Reemplazo de GLideN64/src/osal/osal_files_unix.c para la consola: esa parte solo sirve para la caché de texturas en
 * alta resolución (carpetas en disco), que ALLESIU no usa, y pedía stat y opendir al sistema. Acá no toca el disco. */
#include <wchar.h>

int osal_is_directory(const wchar_t* name) { (void)name; return 0; }
int osal_is_absolute_path(const wchar_t* name) { return name && name[0] == L'/'; }
int osal_path_existsA(const char* path) { (void)path; return 0; }
int osal_path_existsW(const wchar_t* path) { (void)path; return 0; }
int osal_mkdirp(const wchar_t* dirpath) { (void)dirpath; return -1; }
void* osal_search_dir_open(const wchar_t* pathname) { (void)pathname; return 0; }
const wchar_t* osal_search_dir_read_next(void* dir_handle) { (void)dir_handle; return 0; }
void osal_search_dir_close(void* dir_handle) { (void)dir_handle; }
