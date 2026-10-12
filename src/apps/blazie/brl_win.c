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
#ifdef BRLW_LABEL_FIRST                     /* test_brl_win's control: the label names the field, as 0.8's first preview */
    g_label = CreateWindowExW(0, L"STATIC", L"Braille display", WS_CHILD, 8, 8, 300, 20, parent, NULL, inst, NULL);
#endif
    field = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", BRLW_NONE, WS_CHILD | WS_TABSTOP | ES_READONLY | ES_AUTOHSCROLL,
                            8, 32, 400, 36, parent, NULL, inst, NULL);
#ifndef BRLW_LABEL_FIRST
    /* the caption after the field, so it never becomes the field's name: a screen reader would put that name before
       the cells on the user's display ("Braille display ro edt" took 23 of 40 cells, and the unit's last ones fell off
       -- Pitermach, 0.8's first preview); with no name NVDA shows "ro edt" and all of the unit's cells */
    g_label = CreateWindowExW(0, L"STATIC", L"Braille display", WS_CHILD, 8, 8, 300, 20, parent, NULL, inst, NULL);
#endif
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
#ifndef BRLW_NO_EVENT                       /* test_brl_win's control: the screen reader never told */
    /* and the screen reader told.  NVDA redraws an edit field's braille on its caret moving, which this one never
       does: the text alone changing left the user's display on the old cells until a key moved the caret
       (Pitermach, 0.8's first preview).  Of the events an edit field can send, the name change is the one NVDA's
       braille follows for it (a value change only marks the text changed; a caret event without a move is dropped);
       it speaks nothing, the name being the same (checked with NVDA 2026.3 through its own braille output) */
    NotifyWinEvent(EVENT_OBJECT_NAMECHANGE, field, OBJID_CLIENT, CHILDID_SELF);
#endif
    return 1;
}
