#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/executor.h"
#include "ui/window.h"

static int run_cli(const char *command, const char *hint)
{
    char out[65536];
    int code = 1;
    size_t len;

    if (winx_executor_run_once(command, hint, out, sizeof(out), &code) != 0) {
        fprintf(stderr, "winlinux: command failed to execute\n");
        return 1;
    }

    len = strlen(out);
    if (len > 0) {
        fwrite(out, 1, len, stdout);
    }
    return code;
}

static const char *get_git_hint(void)
{
    return getenv("WINLINUX_GIT_BASH");
}

int main(int argc, char **argv)
{
    const char *hint = get_git_hint();

    if (argc >= 3 && strcmp(argv[1], "-c") == 0) {
        return run_cli(argv[2], hint);
    }

    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        printf("winlinux - Git Bash command shell\n\n");
        printf("GUI  : winlinux\n");
        printf("CLI  : winlinux -c \"<command>\"\n");
        printf("Env  : WINLINUX_GIT_BASH=<git root>\n");
        return 0;
    }

    {
        winx_app *app = winx_app_create(hint);
        int rc;
        if (app == NULL) {
            return 1;
        }
        rc = winx_app_run(app, SW_SHOWDEFAULT);
        winx_app_destroy(app);
        return rc;
    }
}
