#include "core/executor.h"

#include <string.h>

static void forward_output(const char *bytes, size_t len, void *ud)
{
    winx_executor *e = ud;
    if (e->on_output != NULL) {
        e->on_output(bytes, len, e->ud);
    }
}

int winx_executor_open(winx_executor *e, const char *git_hint)
{
    winx_pipe_spec spec;
    int rc;

    if (e == NULL) {
        return -1;
    }
    memset(e, 0, sizeof(*e));

    if (winx_backend_resolve(git_hint, &e->backend) != 0) {
        e->state = WINX_EXEC_DEAD;
        return -1;
    }
    if (winx_backend_spawn(&e->backend) != 0) {
        e->state = WINX_EXEC_DEAD;
        return -1;
    }

    e->pipe = winx_pipeline_create();
    if (e->pipe == NULL) {
        winx_backend_close(&e->backend);
        e->state = WINX_EXEC_DEAD;
        return -1;
    }

    winx_backend_spec(&e->backend, &spec);
    rc = winx_pipeline_open(e->pipe, &spec, forward_output, forward_output, e);
    if (rc != 0) {
        winx_pipeline_close(e->pipe);
        winx_backend_close(&e->backend);
        e->state = WINX_EXEC_DEAD;
        return -1;
    }

    e->state = WINX_EXEC_READY;
    return 0;
}

void winx_executor_on_output(winx_executor *e, winx_exec_output_cb cb, void *ud)
{
    if (e != NULL) {
        e->on_output = cb;
        e->ud = ud;
    }
}

int winx_executor_run(winx_executor *e, const char *command)
{
    HANDLE in;
    DWORD written;
    const char *suffix = "\n";
    size_t cmd_len;
    size_t suffix_len;

    if (e == NULL || e->state != WINX_EXEC_READY || command == NULL) {
        return -1;
    }

    cmd_len = strlen(command);
    suffix_len = strlen(suffix);
    if (cmd_len > 65535) {
        return -1;
    }

    in = winx_pipeline_stdin(e->pipe);
    if (in == INVALID_HANDLE_VALUE) {
        return -1;
    }

    if (cmd_len > 0) {
        if (!WriteFile(in, command, (DWORD)cmd_len, &written, NULL) ||
            written != cmd_len) {
            return -1;
        }
    }
    if (!WriteFile(in, suffix, (DWORD)suffix_len, &written, NULL)) {
        return -1;
    }
    FlushFileBuffers(in);
    return 0;
}

int winx_executor_terminate(winx_executor *e)
{
    if (e == NULL) {
        return -1;
    }
    if (e->state == WINX_EXEC_READY) {
        winx_executor_run(e, "exit");
        e->state = WINX_EXEC_IDLE;
    }
    return 0;
}

winx_exec_state winx_executor_state(const winx_executor *e)
{
    return e ? e->state : WINX_EXEC_DEAD;
}

void winx_executor_close(winx_executor *e)
{
    if (e == NULL) {
        return;
    }
    if (e->pipe != NULL) {
        winx_pipeline_close(e->pipe);
        e->pipe = NULL;
    }
    winx_backend_close(&e->backend);
    e->state = WINX_EXEC_DEAD;
}
