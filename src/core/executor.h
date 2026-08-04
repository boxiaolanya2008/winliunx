#ifndef WINLINUX_CORE_EXECUTOR_H
#define WINLINUX_CORE_EXECUTOR_H

#include <windows.h>

#include "core/backend.h"
#include "core/pipeline.h"

typedef enum winx_exec_state {
    WINX_EXEC_IDLE,
    WINX_EXEC_READY,
    WINX_EXEC_DEAD
} winx_exec_state;

typedef void (*winx_exec_output_cb)(const char *bytes, size_t len, void *ud);

typedef struct winx_executor {
    winx_backend backend;
    winx_pipeline *pipe;
    winx_exec_state state;
    winx_exec_output_cb on_output;
    void *ud;
} winx_executor;

int winx_executor_open(winx_executor *e, const char *git_hint);

void winx_executor_on_output(winx_executor *e, winx_exec_output_cb cb, void *ud);

void winx_executor_close(winx_executor *e);

int winx_executor_run(winx_executor *e, const char *command);

int winx_executor_run_once(const char *command, const char *git_hint,
                           char *out, size_t outcap, int *exit_code);

int winx_executor_terminate(winx_executor *e);

winx_exec_state winx_executor_state(const winx_executor *e);

#endif
