#ifndef WINLINUX_UI_WINDOW_H
#define WINLINUX_UI_WINDOW_H

#include <windows.h>
#include <stddef.h>

typedef struct winx_app winx_app;

winx_app *winx_app_create(const char *git_hint);

int winx_app_run(winx_app *app, int ncmdshow);

int winx_app_append_output(winx_app *app, const char *bytes, size_t len);

void winx_app_destroy(winx_app *app);

#endif
