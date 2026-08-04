#include "core/pipeline.h"

#include <stdlib.h>

enum { WINX_READ_CHUNK = 4096 };

typedef struct winx_reader {
    HANDLE handle;
    char buf[WINX_READ_CHUNK];
    winx_pipe_data_cb cb;
    void *ud;
    HANDLE thread;
    LONG done;
} winx_reader;

struct winx_pipeline {
    HANDLE in_write;
    winx_reader out;
    winx_reader err;
};

static DWORD WINAPI reader_thread_main(LPVOID arg)
{
    winx_reader *r = arg;
    DWORD nread;
    BOOL cont;

    for (;;) {
        cont = ReadFile(r->handle, r->buf, WINX_READ_CHUNK, &nread, NULL);
        if (!cont || nread == 0) {
            break;
        }
        if (r->cb != NULL) {
            r->cb(r->buf, nread, r->ud);
        }
    }
    InterlockedExchange(&r->done, 1);
    return 0;
}

static int start_reader(winx_reader *r)
{
    r->thread = CreateThread(NULL, 0, reader_thread_main, r, 0, NULL);
    return (r->thread != NULL) ? 0 : -1;
}

winx_pipeline *winx_pipeline_create(void)
{
    winx_pipeline *p = calloc(1, sizeof(winx_pipeline));
    if (p == NULL) {
        return NULL;
    }
    p->in_write = INVALID_HANDLE_VALUE;
    p->out.handle = INVALID_HANDLE_VALUE;
    p->err.handle = INVALID_HANDLE_VALUE;
    return p;
}

int winx_pipeline_open(
    winx_pipeline *p,
    const winx_pipe_spec *spec,
    winx_pipe_data_cb on_output,
    winx_pipe_data_cb on_error,
    void *ud)
{
    if (p == NULL || spec == NULL) {
        return -1;
    }

    p->in_write = spec->child_stdin_write;
    p->out.handle = spec->child_stdout_read;
    p->err.handle = spec->child_stderr_read;
    p->out.cb = on_output;
    p->out.ud = ud;
    p->err.cb = on_error;
    p->err.ud = ud;

    if (start_reader(&p->out) != 0 || start_reader(&p->err) != 0) {
        return -1;
    }
    return 0;
}

void winx_pipeline_close(winx_pipeline *p)
{
    if (p == NULL) {
        return;
    }
    if (p->in_write != INVALID_HANDLE_VALUE) {
        CloseHandle(p->in_write);
    }
    if (p->out.handle != INVALID_HANDLE_VALUE) {
        CloseHandle(p->out.handle);
    }
    if (p->err.handle != INVALID_HANDLE_VALUE) {
        CloseHandle(p->err.handle);
    }
    if (p->out.thread != NULL) {
        WaitForSingleObject(p->out.thread, 2000);
        CloseHandle(p->out.thread);
    }
    if (p->err.thread != NULL) {
        WaitForSingleObject(p->err.thread, 2000);
        CloseHandle(p->err.thread);
    }
    free(p);
}

HANDLE winx_pipeline_stdin(const winx_pipeline *p)
{
    return p->in_write;
}
