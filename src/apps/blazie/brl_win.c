/* brl_win.c -- the emulated Braille Lite's display as a read-only field in the Windows window (brl_win.h) */
#include "brl_win.h"

#include <wchar.h>

static WNDPROC g_edit_proc;                 /* the edit control's own procedure */
static WNDPROC g_keys;                      /* the window's: the keys go there */
static HWND g_label;

/* the field: its keys to the window, everything else the edit control's (read-only: it never changes the text) */
static LRESULT CALLBACK field_proc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_GETDLGCODE:
        return DLGC_WANTALLKEYS | DLGC_WANTARROWS | DLGC_WANTCHARS | DLGC_WANTTAB;
#ifndef BRLW_NO_FORWARD                     /* test_brl_win's control: the keys stay in the field */
    case WM_KEYDOWN: case WM_KEYUP: case WM_SYSKEYDOWN: case WM_SYSKEYUP:
    case WM_CHAR: case WM_SYSCHAR: case WM_DEADCHAR: case WM_SYSDEADCHAR:
        return g_keys(GetParent(w), msg, wp, lp);
    case WM_KILLFOCUS:                      /* the window lets go of a half-pressed chord and held keys */
        g_keys(GetParent(w), msg, wp, lp);
        break;
#endif
    case WM_CONTEXTMENU:
        return 0;
    }
    return CallWindowProcW(g_edit_proc, w, msg, wp, lp);
}

HWND brlw_create(HWND parent, HINSTANCE inst, WNDPROC keys)
{
    HWND field;
    HFONT font = CreateFontW(-24, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 0, 0, L"Segoe UI Symbol");
    g_keys = keys;
    /* the label first: a screen reader names the field from the label before it */
    g_label = CreateWindowExW(0, L"STATIC", L"Braille display", WS_CHILD, 8, 8, 300, 20, parent, NULL, inst, NULL);
    field = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", BRLW_NONE, WS_CHILD | WS_TABSTOP | ES_READONLY | ES_AUTOHSCROLL,
                            8, 32, 400, 36, parent, NULL, inst, NULL);
    if (font)
        SendMessageW(field, WM_SETFONT, (WPARAM)font, 0);
    g_edit_proc = (WNDPROC)SetWindowLongPtrW(field, GWLP_WNDPROC, (LONG_PTR)field_proc);
    return field;
}

void brlw_visible(HWND field, int on)
{
    ShowWindow(g_label, on ? SW_SHOW : SW_HIDE);
    ShowWindow(field, on ? SW_SHOW : SW_HIDE);
}

void brlw_layout(HWND field, int width, int height)
{
    (void)height;
    MoveWindow(g_label, 8, 8, width - 16 > 0 ? width - 16 : 1, 20, TRUE);
    MoveWindow(field, 8, 32, width - 16 > 0 ? width - 16 : 1, 36, TRUE);
}

void brlw_text(const unsigned char *cells, int n, wchar_t *out, int cap)
{
    int i;
    if (n <= 0) {
        wcsncpy(out, BRLW_NONE, (size_t)cap - 1);
        out[cap - 1] = 0;
        return;
    }
    for (i = 0; i < n && i < cap - 1; i++)
        out[i] = (wchar_t)(0x2800 + cells[i]);
    out[i] = 0;
}

int brlw_show(HWND field, const unsigned char *cells, int n)
{
    wchar_t want[64], have[64];
    brlw_text(cells, n, want, 64);
    GetWindowTextW(field, have, 64);
    if (!wcscmp(want, have))
        return 0;
    SetWindowTextW(field, want);           /* the caret stays at the start: the field never moves */
    return 1;
}
