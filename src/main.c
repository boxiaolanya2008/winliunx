#include <stdlib.h>

#include "ui/window.h"

int WINAPI WinMain(HINSTANCE hinstance, HINSTANCE hprev, LPSTR lpcmdline,
                   int ncmdshow)
{
    const char *hint;
    winx_app *app;
    int rc;

    (void)hinstance;
    (void)hprev;
    (void)lpcmdline;

    hint = getenv("WINLINUX_GIT_BASH");
    app = winx_app_create(hint);
    if (app == NULL) {
        return 1;
    }
    rc = winx_app_run(app, ncmdshow);
    winx_app_destroy(app);
    return rc;
}
