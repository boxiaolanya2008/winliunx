#include "core/executor.h"

#include <stdio.h>
#include <stdlib.h>
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

static int winx_read_until_eof(HANDLE proc, HANDLE pipe,
                               char *out, size_t outcap,
                               size_t *olen, DWORD timeout_ms)
{
    DWORD ws;
    DWORD code = 0;
    int started = 0;

    for (;;) {
        DWORD n;
        BOOL ok = ReadFile(pipe, out + *olen,
                           (DWORD)(outcap - *olen - 1), &n, NULL);
        if (!ok || n == 0) {
            break;
        }
        *olen += n;
        started = 1;
        if (*olen + 1 >= outcap) {
            break;
        }
    }

    ws = WaitForSingleObject(proc, started ? timeout_ms : 5000);
    if (ws != WAIT_OBJECT_0) {
        WaitForSingleObject(proc, timeout_ms);
    }
    GetExitCodeProcess(proc, &code);
    if (outcap > 0 && olen != NULL) {
        out[*olen] = '\0';
    }
    return (int)code;
}

int winx_executor_run_once(const char *command, const char *git_hint,
                           char *out, size_t outcap, int *exit_code)
{
    winx_backend b;
    STARTUPINFOA si;
    SECURITY_ATTRIBUTES sa;
    PROCESS_INFORMATION pi;
    char bash_path[WINX_BACKEND_PATH_MAX];
    char *cmd;
    HANDLE sin_r, sin_w, sout_r, sout_w;
    size_t olen = 0;
    int child_rc = -1;
    size_t cmd_len;

    if (out == NULL || command == NULL) {
        return -1;
    }
    memset(&pi, 0, sizeof(pi));

    if (winx_backend_resolve(git_hint, &b) != 0) {
        return -1;
    }
    if (winx_backend_build_env(&b) != 0) {
        winx_backend_close(&b);
        return -1;
    }
    snprintf(bash_path, sizeof(bash_path), "%s", b.bash_path);

    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    if (!CreatePipe(&sin_r, &sin_w, &sa, 4096) ||
        !CreatePipe(&sout_r, &sout_w, &sa, 4096)) {
        winx_backend_close(&b);
        return -1;
    }
    SetHandleInformation(sin_w, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(sout_r, HANDLE_FLAG_INHERIT, 0);

    cmd_len = strlen(command);
    if (cmd_len > 60000) {
        CloseHandle(sin_r); CloseHandle(sin_w);
        CloseHandle(sout_r); CloseHandle(sout_w);
        winx_backend_close(&b);
        return -1;
    }
    cmd = (char *)malloc(cmd_len + WINX_BACKEND_PATH_MAX + 32);
    if (cmd == NULL) {
        CloseHandle(sin_r); CloseHandle(sin_w);
        CloseHandle(sout_r); CloseHandle(sout_w);
        winx_backend_close(&b);
        return -1;
    }
    snprintf(cmd, cmd_len + WINX_BACKEND_PATH_MAX + 32,
             "\"%s\" -s", bash_path);

    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdInput = sin_r;
    si.hStdOutput = sout_w;
    si.hStdError = sout_w;
    si.wShowWindow = SW_HIDE;

    if (!CreateProcessA(NULL, cmd, NULL, NULL, TRUE,
                        CREATE_NO_WINDOW, b.env, NULL, &si, &pi)) {
        free(cmd);
        CloseHandle(sin_r); CloseHandle(sin_w);
        CloseHandle(sout_r); CloseHandle(sout_w);
        winx_backend_close(&b);
        return -1;
    }
    free(cmd);

    CloseHandle(sin_r);
    CloseHandle(sout_w);

    {
        DWORD w;
        WriteFile(sin_w, command, (DWORD)cmd_len, &w, NULL);
        WriteFile(sin_w, "\n", 1, &w, NULL);
    }
    CloseHandle(sin_w);

    child_rc = winx_read_until_eof(pi.hProcess, sout_r, out, outcap, &olen,
                                   45000);

    CloseHandle(sout_r);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    winx_backend_close(&b);

    if (exit_code != NULL && child_rc >= 0) {
        *exit_code = child_rc;
    }
    return (child_rc >= 0) ? 0 : -1;
}
