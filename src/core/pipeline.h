#ifndef WINLINUX_CORE_PIPELINE_H
#define WINLINUX_CORE_PIPELINE_H

#include <windows.h>
#include <stddef.h>

typedef struct winx_pipeline winx_pipeline;

typedef void (*winx_pipe_data_cb)(const char *bytes, size_t len, void *ud);

typedef struct winx_pipe_spec {
    HANDLE child_stdin_write;
    HANDLE child_stdout_read;
    HANDLE child_stderr_read;
} winx_pipe_spec;

winx_pipeline *winx_pipeline_create(void);

int winx_pipeline_open(
    winx_pipeline *p,
    const winx_pipe_spec *spec,
    winx_pipe_data_cb on_output,
    winx_pipe_data_cb on_error,
    void *ud);

void winx_pipeline_close(winx_pipeline *p);

HANDLE winx_pipeline_stdin(const winx_pipeline *p);

#endif
