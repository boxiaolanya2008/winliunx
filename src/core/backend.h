#ifndef WINLINUX_CORE_BACKEND_H
#define WINLINUX_CORE_BACKEND_H

#include <windows.h>

#include "core/pipeline.h"

#define WINX_BACKEND_PATH_MAX 1024

typedef struct winx_backend {
    PROCESS_INFORMATION proc;
    HANDLE stdin_read;
    HANDLE stdin_write;
    HANDLE stdout_read;
    HANDLE stdout_write;
    HANDLE stderr_read;
    HANDLE stderr_write;
    char bash_path[WINX_BACKEND_PATH_MAX];
    char git_root[WINX_BACKEND_PATH_MAX];
    char *env;
    int alive;
} winx_backend;

int winx_backend_resolve(const char *hint, winx_backend *out);

int winx_backend_build_env(winx_backend *b);

int winx_backend_spawn(winx_backend *b);

void winx_backend_spec(const winx_backend *b, winx_pipe_spec *out);

void winx_backend_close(winx_backend *b);

BOOL winx_backend_poll(winx_backend *b, DWORD *exit_code);

#endif
