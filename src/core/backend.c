#include "core/backend.h"

#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

enum { WINX_PIPE_BUF = 4096 };

static int path_exists(const char *p)
{
    return GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES;
}

static void path_join(char *dst, size_t cap, const char *base, const char *sub)
{
    if (sub == NULL || sub[0] == '\0') {
        snprintf(dst, cap, "%s", base);
        return;
    }
    snprintf(dst, cap, "%s\\%s", base, sub);
}

static int try_git_root(winx_backend *b, const char *root)
{
    char bash[WINX_BACKEND_PATH_MAX];
    char probe[WINX_BACKEND_PATH_MAX];

    path_join(bash, sizeof(bash), root, "usr\\bin\\bash.exe");
    if (!path_exists(bash)) {
        return 0;
    }
    path_join(probe, sizeof(probe), root, "usr\\bin\\ls.exe");
    if (!path_exists(probe)) {
        return 0;
    }

    snprintf(b->bash_path, sizeof(b->bash_path), "%s", bash);
    snprintf(b->git_root, sizeof(b->git_root), "%s", root);
    return 1;
}

int winx_backend_resolve(const char *hint, winx_backend *out)
{
    const char *candidates[] = {
        "C:\\Program Files\\Git",
        "C:\\Program Files (x86)\\Git",
        "D:\\Git",
        "E:\\Git",
        "C:\\Git",
    };
    size_t i;

    if (out == NULL) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    out->stdin_read = INVALID_HANDLE_VALUE;
    out->stdin_write = INVALID_HANDLE_VALUE;
    out->stdout_read = INVALID_HANDLE_VALUE;
    out->stdout_write = INVALID_HANDLE_VALUE;
    out->stderr_read = INVALID_HANDLE_VALUE;
    out->stderr_write = INVALID_HANDLE_VALUE;

    if (hint != NULL && hint[0] != '\0' && path_exists(hint)) {
        if (try_git_root(out, hint)) {
            return 0;
        }
    }

    for (i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
        if (try_git_root(out, candidates[i])) {
            return 0;
        }
    }
    return -1;
}

static char *build_env_block(const char *git_root)
{
    DWORD sys_len;
    char *win_path;
    size_t off = 0;
    char *buf;
    int n;

    sys_len = GetEnvironmentVariableA("PATH", NULL, 0);
    win_path = (char *)malloc(sys_len + 2);
    if (win_path == NULL) {
        return NULL;
    }
    GetEnvironmentVariableA("PATH", win_path, sys_len + 1);

    buf = (char *)malloc(32768);
    if (buf == NULL) {
        free(win_path);
        return NULL;
    }

    n = snprintf(buf, 32768,
                 "PATH=%s\\usr\\bin;%s\\usr\\sbin;%s\\bin;%s;MSYSTEM=MSYS;"
                 "MSYSTEM_CHOST=x86_64-w64-mingw32;CHERE_INVOKING=1;",
                 git_root, git_root, git_root, win_path);
    if (n > 0) {
        off = (size_t)n;
    }
    if (off >= 32768) {
        off = 32767;
    }
    free(win_path);
    buf[off++] = '\0';
    buf[off] = '\0';
    return buf;
}

int winx_backend_spawn(winx_backend *b)
{
    STARTUPINFOA si;
    char cmdline[WINX_BACKEND_PATH_MAX + 40];
    SECURITY_ATTRIBUTES sa;
    BOOL ok;

    if (b == NULL || b->bash_path[0] == '\0') {
        return -1;
    }

    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    if (!CreatePipe(&b->stdin_read, &b->stdin_write, &sa, WINX_PIPE_BUF)) {
        return -1;
    }
    if (!CreatePipe(&b->stdout_read, &b->stdout_write, &sa, WINX_PIPE_BUF)) {
        return -1;
    }
    if (!CreatePipe(&b->stderr_read, &b->stderr_write, &sa, WINX_PIPE_BUF)) {
        return -1;
    }

    SetHandleInformation(b->stdin_write, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(b->stdout_read, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(b->stderr_read, HANDLE_FLAG_INHERIT, 0);

    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdInput = b->stdin_read;
    si.hStdOutput = b->stdout_write;
    si.hStdError = b->stderr_write;
    si.wShowWindow = SW_HIDE;

    snprintf(cmdline, sizeof(cmdline),
             "\"%s\" --noprofile --norc -s", b->bash_path);

    b->env = build_env_block(b->git_root);

    ok = CreateProcessA(
        b->bash_path,
        cmdline,
        NULL,
        NULL,
        TRUE,
        CREATE_NO_WINDOW,
        b->env,
        NULL,
        &si,
        &b->proc);
    if (!ok) {
        return -1;
    }

    CloseHandle(b->stdin_read);
    CloseHandle(b->stdout_write);
    CloseHandle(b->stderr_write);
    b->stdin_read = INVALID_HANDLE_VALUE;
    b->stdout_write = INVALID_HANDLE_VALUE;
    b->stderr_write = INVALID_HANDLE_VALUE;
    b->alive = 1;
    return 0;
}

void winx_backend_spec(const winx_backend *b, winx_pipe_spec *out)
{
    out->child_stdin_write = b->stdin_write;
    out->child_stdout_read = b->stdout_read;
    out->child_stderr_read = b->stderr_read;
}

BOOL winx_backend_poll(winx_backend *b, DWORD *exit_code)
{
    DWORD code;

    if (b == NULL || !b->alive) {
        return FALSE;
    }
    if (GetExitCodeProcess(b->proc.hProcess, &code)) {
        if (code != STILL_ACTIVE) {
            b->alive = 0;
            if (exit_code != NULL) {
                *exit_code = code;
            }
            return TRUE;
        }
    }
    return FALSE;
}

void winx_backend_close(winx_backend *b)
{
    if (b == NULL) {
        return;
    }
    free(b->env);
    b->env = NULL;
    if (b->proc.hThread != NULL) {
        CloseHandle(b->proc.hThread);
    }
    if (b->proc.hProcess != NULL) {
        CloseHandle(b->proc.hProcess);
    }
    b->proc.hThread = NULL;
    b->proc.hProcess = NULL;
}
