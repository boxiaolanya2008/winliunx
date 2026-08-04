#include "ui/window.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "core/executor.h"

#define WINX_WM_OUTPUT (WM_APP + 1)

static const TCHAR *g_class_name = TEXT("WinXTermClass");
static const int CTRL_INPUT = 1001;
static const int CTRL_RUN = 1002;
static const int CTRL_OUTPUT = 1003;

typedef struct ui_buffer {
    char *data;
    size_t len;
    size_t cap;
} ui_buffer;

struct winx_app {
    HINSTANCE hinst;
    HWND hwnd;
    HWND input;
    HWND output;
    HWND run_btn;
    HFONT font;
    winx_executor exec;
    CRITICAL_SECTION lock;
    ui_buffer pending;
    int dirty;
    const char *git_hint;
};

static void ui_buffer_append(ui_buffer *b, const char *bytes, size_t len)
{
    size_t need;
    char *tmp;

    if (b == NULL || (bytes == NULL && len > 0)) {
        return;
    }
    need = b->len + len + 1;
    if (need > b->cap) {
        size_t cap = b->cap ? b->cap : 256;
        while (cap < need) {
            cap *= 2;
        }
        tmp = (char *)realloc(b->data, cap);
        if (tmp == NULL) {
            return;
        }
        b->data = tmp;
        b->cap = cap;
    }
    if (len > 0) {
        memcpy(b->data + b->len, bytes, len);
        b->len += len;
    }
    b->data[b->len] = '\0';
}

static int append_edit_text(HWND edit, const char *text)
{
    int text_len;
    int edit_len;
    int sel_start;
    int sel_end;

    text_len = (int)strlen(text);
    if (text_len <= 0) {
        return 0;
    }
    edit_len = GetWindowTextLengthA(edit);
    sel_start = edit_len;
    sel_end = edit_len;
    SendMessageA(edit, EM_SETSEL, (WPARAM)sel_start, (LPARAM)sel_end);
    SendMessageA(edit, EM_REPLACESEL, FALSE, (LPARAM)text);
    return 0;
}

static void do_run_impl(winx_app *app)
{
    char buf[8192];
    int len;
    char echo[8200];
    int echo_len;

    len = GetWindowTextA(app->input, buf, (int)sizeof(buf));
    if (len <= 0) {
        return;
    }

    echo_len = snprintf(echo, sizeof(echo), "\r\n$ %s\r\n", buf);
    if (echo_len > 0) {
        append_edit_text(app->output, echo);
    }
    winx_executor_run(&app->exec, buf);

    SendMessageA(app->input, EM_SETSEL, 0, -1);
    SetFocus(app->input);
}

static void do_run(HWND hwnd)
{
    winx_app *app = (winx_app *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (app != NULL) {
        do_run_impl(app);
    }
}

static void flush_pending_locked(winx_app *app)
{
    if (app->pending.len > 0) {
        append_edit_text(app->output, app->pending.data);
        app->pending.len = 0;
        if (app->pending.data != NULL) {
            app->pending.data[0] = '\0';
        }
    }
    app->dirty = 0;
}

static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    winx_app *app = (winx_app *)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_SIZE:
        if (app != NULL && IsWindow(app->output) &&
            IsWindow(app->input) && IsWindow(app->run_btn)) {
            int w = LOWORD(lparam);
            int h = HIWORD(lparam);
            const int input_h = 28;
            const int gap = 6;
            const int btn_w = 64;
            const int margin = 8;
            MoveWindow(app->input, margin, margin, w - 2 * margin - btn_w - gap,
                       input_h, TRUE);
            MoveWindow(app->run_btn, w - margin - btn_w, margin, btn_w,
                       input_h, TRUE);
            MoveWindow(app->output, margin, margin + input_h + gap,
                       w - 2 * margin, h - 2 * margin - input_h - gap, TRUE);
        }
        return 0;

    case WINX_WM_OUTPUT:
        if (app != NULL) {
            EnterCriticalSection(&app->lock);
            flush_pending_locked(app);
            LeaveCriticalSection(&app->lock);
        }
        return 0;

    case WM_COMMAND:
        if (LOWORD(wparam) == CTRL_RUN) {
            do_run(hwnd);
            return 0;
        }
        break;

    case WM_CLOSE:
        if (app != NULL) {
            winx_executor_terminate(&app->exec);
        }
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }
    return DefWindowProcA(hwnd, msg, wparam, lparam);
}

static void relay_output(const char *bytes, size_t len, void *ud)
{
    winx_app *app = ud;
    int post;

    EnterCriticalSection(&app->lock);
    ui_buffer_append(&app->pending, bytes, len);
    app->dirty = 1;
    post = (app->hwnd != NULL);
    LeaveCriticalSection(&app->lock);

    if (post) {
        PostMessage(app->hwnd, WINX_WM_OUTPUT, 0, 0);
    }
}

winx_app *winx_app_create(const char *git_hint)
{
    winx_app *app = (winx_app *)calloc(1, sizeof(winx_app));
    if (app != NULL) {
        app->git_hint = git_hint;
        InitializeCriticalSection(&app->lock);
    }
    return app;
}

int winx_app_run(winx_app *app, int ncmdshow)
{
    WNDCLASSA wc;
    RECT rect;
    MSG msg;
    int ret;
    DWORD style;
    int screen_w;
    int screen_h;
    int win_w;
    int win_h;
    HMODULE user32;

    if (app == NULL) {
        return 1;
    }
    app->hinst = GetModuleHandleA(NULL);

    user32 = GetModuleHandleA("user32.dll");
    if (user32 != NULL) {
        union {
            FARPROC raw;
            BOOL(WINAPI *dpi)(void *);
        } fn;
        fn.raw = GetProcAddress(user32, "SetProcessDpiAwarenessContext");
        if (fn.raw != NULL) {
            fn.dpi((void *)(LONG_PTR)-4);
        } else {
            SetProcessDPIAware();
        }
    }

    screen_w = GetSystemMetrics(SM_CXSCREEN);
    screen_h = GetSystemMetrics(SM_CYSCREEN);
    win_w = (screen_w * 65) / 100;
    win_h = (screen_h * 75) / 100;
    if (win_w < 1000) {
        win_w = 1000;
    }
    if (win_h < 620) {
        win_h = 620;
    }

    if (winx_executor_open(&app->exec, app->git_hint) != 0) {
        MessageBoxA(NULL,
                    "Could not start Git Bash. Install Git for Windows, or "
                    "set WINLINUX_GIT_BASH to the bash.exe path.",
                    "winlinux", MB_ICONERROR | MB_OK);
        return 1;
    }
    winx_executor_on_output(&app->exec, relay_output, app);

    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = window_proc;
    wc.hInstance = app->hinst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = g_class_name;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    if (!RegisterClassA(&wc)) {
        winx_executor_close(&app->exec);
        return 1;
    }

    rect.left = 0;
    rect.top = 0;
    rect.right = win_w;
    rect.bottom = win_h;
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    app->hwnd = CreateWindowExA(
        0, g_class_name, "winlinux - Git Bash terminal",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left, rect.bottom - rect.top,
        NULL, NULL, app->hinst, NULL);
    if (app->hwnd == NULL) {
        winx_executor_close(&app->exec);
        return 1;
    }
    SetWindowLongPtr(app->hwnd, GWLP_USERDATA, (LONG_PTR)app);
    {
        int x;
        int y;
        RECT rc;
        GetWindowRect(app->hwnd, &rc);
        x = (screen_w - (rc.right - rc.left)) / 2;
        y = (screen_h - (rc.bottom - rc.top)) / 2;
        if (x < 0) {
            x = 0;
        }
        if (y < 0) {
            y = 0;
        }
        SetWindowPos(app->hwnd, NULL, x, y, 0, 0,
                     SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW);
    }

    app->font = CreateFontA(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                            CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                            FIXED_PITCH | FF_MODERN, "Consolas");

    style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_LEFT | ES_AUTOHSCROLL;
    app->input = CreateWindowExA(0, "EDIT", "", style,
                                 0, 0, 0, 0, app->hwnd, (HMENU)(INT_PTR)CTRL_INPUT,
                                 app->hinst, NULL);
    app->run_btn = CreateWindowExA(0, "BUTTON", "Run",
                                   WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                       BS_PUSHBUTTON,
                                   0, 0, 0, 0, app->hwnd, (HMENU)(INT_PTR)CTRL_RUN,
                                   app->hinst, NULL);
    app->output = CreateWindowExA(0, "EDIT", "",
                                  WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                      ES_MULTILINE | ES_READONLY | WS_VSCROLL |
                                      WS_HSCROLL | ES_AUTOVSCROLL,
                                  0, 0, 0, 0, app->hwnd, (HMENU)(INT_PTR)CTRL_OUTPUT,
                                  app->hinst, NULL);
    if (app->font != NULL) {
        SendMessage(app->input, WM_SETFONT, (WPARAM)app->font, TRUE);
        SendMessage(app->output, WM_SETFONT, (WPARAM)app->font, TRUE);
    }

    ShowWindow(app->hwnd, ncmdshow);
    UpdateWindow(app->hwnd);

    SendMessage(app->input, WM_SETTEXT, 0, (LPARAM)"ls -la");

    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN &&
            msg.hwnd == app->input) {
            do_run(app->hwnd);
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    ret = (int)msg.wParam;
    winx_executor_close(&app->exec);
    if (app->font != NULL) {
        DeleteObject(app->font);
    }
    return ret;
}

int winx_app_append_output(winx_app *app, const char *bytes, size_t len)
{
    if (app == NULL) {
        return -1;
    }
    EnterCriticalSection(&app->lock);
    ui_buffer_append(&app->pending, bytes, len);
    app->dirty = 1;
    LeaveCriticalSection(&app->lock);
    return 0;
}

void winx_app_destroy(winx_app *app)
{
    if (app == NULL) {
        return;
    }
    DeleteCriticalSection(&app->lock);
    free(app->pending.data);
    free(app);
}
