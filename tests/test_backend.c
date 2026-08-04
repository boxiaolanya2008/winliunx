#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "test_assert.h"
#include "core/executor.h"
#include "core/backend.h"

typedef struct capture {
    char buf[16384];
    size_t len;
} capture;

static void capture_on_output(const char *bytes, size_t len, void *ud)
{
    capture *c = ud;
    if (c->len + len < sizeof(c->buf) - 1) {
        memcpy(c->buf + c->len, bytes, len);
        c->len += len;
        c->buf[c->len] = '\0';
    }
}

static int wait_contains(capture *c, const char *needle, int ms)
{
    const int step = 25;
    int waited = 0;
    while (waited < ms) {
        if (strstr(c->buf, needle) != NULL) {
            return 1;
        }
        Sleep((DWORD)step);
        waited += step;
    }
    return strstr(c->buf, needle) != NULL;
}

static void test_backend_resolve(void)
{
    winx_backend b;
    int rc = winx_backend_resolve(NULL, &b);
    CHECK(rc == 0);
    if (rc == 0) {
        CHECK(b.bash_path[0] != '\0');
        CHECK(b.git_root[0] != '\0');
    }
    REPORT_SUITE("backend.resolve");
}

static void test_executor_echo(void)
{
    winx_executor e;
    capture cap;
    int rc;

    memset(&cap, 0, sizeof(cap));
    rc = winx_executor_open(&e, NULL);
    CHECK(rc == 0);
    if (rc != 0) {
        return;
    }
    winx_executor_on_output(&e, capture_on_output, &cap);

    CHECK(winx_executor_run(&e, "echo WINX_TEST_TOKEN_42") == 0);
    CHECK(wait_contains(&cap, "WINX_TEST_TOKEN_42", 6000));
    CHECK(strstr(cap.buf, "WINX_TEST_TOKEN_42") != NULL);

    winx_executor_terminate(&e);
    winx_executor_close(&e);
    REPORT_SUITE("executor.echo");
}

static void test_executor_pwd(void)
{
    winx_executor e;
    capture cap;
    int rc;

    memset(&cap, 0, sizeof(cap));
    rc = winx_executor_open(&e, NULL);
    CHECK(rc == 0);
    if (rc != 0) {
        return;
    }
    winx_executor_on_output(&e, capture_on_output, &cap);

    CHECK(winx_executor_run(&e, "pwd") == 0);
    CHECK(wait_contains(&cap, "/", 6000));

    winx_executor_terminate(&e);
    winx_executor_close(&e);
    REPORT_SUITE("executor.pwd");
}

static void test_executor_ls(void)
{
    winx_executor e;
    capture cap;
    int rc;

    memset(&cap, 0, sizeof(cap));
    rc = winx_executor_open(&e, NULL);
    CHECK(rc == 0);
    if (rc != 0) {
        return;
    }
    winx_executor_on_output(&e, capture_on_output, &cap);

    CHECK(winx_executor_run(&e, "ls -la") == 0);
    CHECK(wait_contains(&cap, "total", 8000));
    CHECK(strstr(cap.buf, "drwx") != NULL);

    winx_executor_terminate(&e);
    winx_executor_close(&e);
    REPORT_SUITE("executor.ls");
}

static void test_run_once(void)
{
    char out[65536];
    int code = -1;

    CHECK(winx_executor_run_once("echo CLI_TOKEN_99", NULL,
                                 out, sizeof(out), &code) == 0);
    CHECK(code == 0);
    CHECK(strstr(out, "CLI_TOKEN_99") != NULL);

    code = -1;
    CHECK(winx_executor_run_once("false", NULL, out, sizeof(out), &code) == 0);
    CHECK(code == 1);

    code = -1;
    CHECK(winx_executor_run_once("exit 42", NULL, out, sizeof(out), &code) == 0);
    CHECK(code == 42);

    code = -1;
    CHECK(winx_executor_run_once("echo PIPE_X | grep PIPE_X", NULL,
                                 out, sizeof(out), &code) == 0);
    CHECK(code == 0);
    CHECK(strstr(out, "PIPE_X") != NULL);

    REPORT_SUITE("run_once");
}

int main(void)
{
    test_backend_resolve();
    test_executor_echo();
    test_executor_pwd();
    test_executor_ls();
    test_run_once();
    return g_failures > 0 ? 1 : 0;
}
