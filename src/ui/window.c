#include "ui/window.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "core/executor.h"

#define WINX_WM_OUTPUT (WM_APP + 1)

static void paint_spinner(HDC hdc, int cx, int cy, int deg);

static const TCHAR *g_class_name = TEXT("WinXTermClass");
static const int CTRL_INPUT = 1001;
static const int CTRL_RUN = 1002;
static const int CTRL_OUTPUT = 1003;

static void app_start_boot_animation(winx_app *app);
static void app_boot_tick(winx_app *app);
static void app_start_spin(winx_app *app);
static void app_stop_spin(winx_app *app);

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

    int running;
    int spin_deg;
    UINT spin_timer;
    int boot_phase;
    UINT boot_timer;
    HWND banner;
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
    winx_executor_run(&app->exec, "printf '\\n__WINX_DONE__\\n'");
    app_start_spin(app);

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
    static const char marker[] = "__WINX_DONE__";
    char *pos;
    char *nl;

    if (app->pending.len > 0) {
        pos = strstr(app->pending.data, marker);
        if (pos != NULL) {
            nl = strrchr(app->pending.data, '\n');
            if (nl != NULL) {
                *nl = '\0';
            } else {
                *pos = '\0';
            }
            app->pending.len = strlen(app->pending.data);
            app_stop_spin(app);
        }
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
            const int status_h = 22;
            MoveWindow(app->input, margin, margin, w - 2 * margin - btn_w - gap,
                       input_h, TRUE);
            MoveWindow(app->run_btn, w - margin - btn_w, margin, btn_w,
                       input_h, TRUE);
            MoveWindow(app->output, margin, margin + input_h + gap,
                       w - 2 * margin, h - 2 * margin - input_h - gap -
                           status_h,
                       TRUE);
        }
        return 0;

    case WM_TIMER:
        if (app != NULL && wparam == 1) {
            app_boot_tick(app);
            return 0;
        }
        if (app != NULL && wparam == 2) {
            RECT s;
            app->spin_deg = (app->spin_deg + 12) % 360;
            GetClientRect(hwnd, &s);
            s.left = s.right - 50;
            s.top = s.bottom - 22;
            InvalidateRect(hwnd, &s, FALSE);
            return 0;
        }
        break;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc;
        if (app != NULL && app->running) {
            RECT c;
            COLORREF bkg;
            HBRUSH bg;
            GetClientRect(hwnd, &c);
            hdc = BeginPaint(hwnd, &ps);
            bkg = GetSysColor(COLOR_WINDOW);
            bg = CreateSolidBrush(bkg);
            FillRect(hdc, &ps.rcPaint, bg);
            DeleteObject(bg);
            paint_spinner(hdc, c.right - 24, c.bottom - 11, app->spin_deg);
            EndPaint(hwnd, &ps);
            return 0;
        }
        break;
    }

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

#define WINX_BANNER_CLASS "WinXBootBanner"
#define WINX_BANNER_LIFE_MS 1400
#define WINX_SPIN_TICK 60

static COLORREF lerp_color(COLORREF a, COLORREF b, int t, int n)
{
    int aa = GetRValue(a) + (GetRValue(b) - GetRValue(a)) * t / n;
    int ab = GetGValue(a) + (GetGValue(b) - GetGValue(a)) * t / n;
    int ac = GetBValue(a) + (GetBValue(b) - GetBValue(a)) * t / n;
    return RGB((BYTE)aa, (BYTE)ab, (BYTE)ac);
}

static void banner_draw_rows(HDC hdc, const RECT *r, COLORREF a, COLORREF b,
                             int y0, int y1)
{
    int y;
    int h = y1 - y0;
    for (y = 0; y < h; ++y) {
        HBRUSH br = CreateSolidBrush(lerp_color(a, b, y, h));
        RECT line;
        line.left = 0;
        line.right = r->right;
        line.top = y0 + y;
        line.bottom = line.top + 1;
        FillRect(hdc, &line, br);
        DeleteObject(br);
    }
}

static LRESULT CALLBACK banner_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    static COLORREF top = RGB(20, 30, 48);
    static COLORREF bottom = RGB(24, 90, 150);

    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc;
        RECT r;
        HFONT big;
        HFONT old;
        char title[64] = "winlinux";
        char sub[64] = "Git Bash command shell";
        RECT tr;
        RECT sr;

        hdc = BeginPaint(hwnd, &ps);
        GetClientRect(hwnd, &r);
        banner_draw_rows(hdc, &r, top, bottom, 0, r.bottom);

        big = CreateFontA(-34, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                          CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                          FIXED_PITCH | FF_MODERN, "Consolas");
        old = (HFONT)SelectObject(hdc, big);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(240, 246, 255));
        tr = r;
        tr.top = (r.bottom - r.top) / 2 - 44;
        DrawTextA(hdc, title, -1, &tr, DT_CENTER | DT_SINGLELINE);
        SelectObject(hdc, old);
        DeleteObject(big);

        big = CreateFontA(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                          CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                          FIXED_PITCH | FF_MODERN, "Consolas");
        old = (HFONT)SelectObject(hdc, big);
        SetTextColor(hdc, RGB(200, 214, 232));
        sr = r;
        sr.top = (r.bottom - r.top) / 2;
        DrawTextA(hdc, sub, -1, &sr, DT_CENTER | DT_SINGLELINE);
        SelectObject(hdc, old);
        DeleteObject(big);

        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    default:
        break;
    }
    return DefWindowProcA(hwnd, msg, wparam, lparam);
}

static int register_banner_class(HINSTANCE hinst)
{
    WNDCLASSA c;
    memset(&c, 0, sizeof(c));
    c.lpfnWndProc = banner_proc;
    c.hInstance = hinst;
    c.hCursor = LoadCursor(NULL, IDC_ARROW);
    c.lpszClassName = WINX_BANNER_CLASS;
    c.hbrBackground = NULL;
    return RegisterClassA(&c) != 0;
}

static void app_start_boot_animation(winx_app *app)
{
    RECT r;
    GetClientRect(app->hwnd, &r);
    app->banner = CreateWindowExA(WS_EX_LAYERED, WINX_BANNER_CLASS, "",
                                  WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
                                  0, 0, r.right - r.left, r.bottom - r.top,
                                  app->hwnd, NULL, app->hinst, NULL);
    SetWindowPos(app->banner, HWND_TOP, 0, 0, 0, 0,
                 SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE);
    AnimateWindow(app->banner, 260, AW_BLEND);
    app->boot_phase = 0;
    app->boot_timer = (UINT)SetTimer(app->hwnd, 1, WINX_BANNER_LIFE_MS / 8, NULL);
}

static void app_boot_tick(winx_app *app)
{
    if (app->banner != NULL) {
        if (app->boot_phase >= 8) {
            KillTimer(app->hwnd, app->boot_timer);
            app->boot_timer = 0;
            AnimateWindow(app->banner, 240, AW_BLEND | AW_HIDE);
            DestroyWindow(app->banner);
            app->banner = NULL;
            RedrawWindow(app->hwnd, NULL, NULL, RDW_INVALIDATE | RDW_ERASE);
            return;
        }
    }
    ++app->boot_phase;
    if (app->banner != NULL && app->boot_timer != 0) {
        KillTimer(app->hwnd, app->boot_timer);
        app->boot_timer = (UINT)SetTimer(app->hwnd, 1,
                                         WINX_BANNER_LIFE_MS / 8, NULL);
    }
}

static void app_start_spin(winx_app *app)
{
    if (app->running) {
        return;
    }
    app->running = 1;
    app->spin_deg = 0;
    app->spin_timer = (UINT)SetTimer(app->hwnd, 2, WINX_SPIN_TICK, NULL);
}

static void app_stop_spin(winx_app *app)
{
    if (!app->running) {
        return;
    }
    app->running = 0;
    if (app->spin_timer != 0) {
        KillTimer(app->hwnd, app->spin_timer);
        app->spin_timer = 0;
    }
}

static void paint_spinner(HDC hdc, int cx, int cy, int deg)
{
    static const COLORREF palette[8] = {
        RGB(0, 160, 120), RGB(0, 150, 180), RGB(40, 110, 220),
        RGB(120, 80, 220), RGB(200, 60, 200), RGB(230, 70, 120),
        RGB(240, 130, 40), RGB(220, 180, 30)
    };
    int i;
    int radius = 7;
    double ang;
    for (i = 0; i < 8; ++i) {
        HBRUSH br;
        RECT rc;
        ang = (double)i * 45.0;
        {
            double rad = (ang + deg) * 3.14159265358979323846 / 180.0;
            rc.left = cx + (int)((double)radius * 0.9 * cos(rad)) - 2;
            rc.top = cy + (int)((double)radius * 0.9 * sin(rad)) - 2;
            rc.right = rc.left + 4;
            rc.bottom = rc.top + 4;
        }
        br = CreateSolidBrush(palette[i]);
        FillRect(hdc, &rc, br);
        DeleteObject(br);
    }
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

    win_w = 720;
    win_h = 480;
    screen_w = GetSystemMetrics(SM_CXSCREEN);
    screen_h = GetSystemMetrics(SM_CYSCREEN);

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
    register_banner_class(app->hinst);

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
    app_start_boot_animation(app);

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
