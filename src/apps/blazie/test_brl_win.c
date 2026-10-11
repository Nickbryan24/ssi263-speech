/* test_brl_win.c -- the braille display field (brl_win.c) in a window that is never shown: its keys go to the
 * window's procedure and leave the field as it was, it asks for every key, and the unit's cells become Unicode
 * braille (changed only when they change; "no braille display" with none).  Built with BRLW_NO_FORWARD (the keys kept
 * in the field) it must fail.  Exit 0 when all pass. */
#include <stdio.h>
#include <wchar.h>
#include "brl_win.h"

static UINT g_msg;
static WPARAM g_wp;
static HWND g_from;
static int g_fail;

static LRESULT CALLBACK window_proc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_KEYDOWN || msg == WM_KEYUP || msg == WM_CHAR || msg == WM_SYSKEYDOWN) {
        g_msg = msg;
        g_wp = wp;
        g_from = w;
        return 0;
    }
    return DefWindowProcW(w, msg, wp, lp);
}

static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok)
        g_fail = 1;
}

int main(void)
{
    WNDCLASSW wc = {0};
    HWND parent, field;
    HINSTANCE inst = GetModuleHandleW(NULL);
    wchar_t text[64];
    const unsigned char cells[3] = {0x01, 0x03, 0x3F}, none[1] = {0};
    wc.lpfnWndProc = window_proc;
    wc.hInstance = inst;
    wc.lpszClassName = L"TestBrlWin";
    RegisterClassW(&wc);
    parent = CreateWindowW(L"TestBrlWin", L"test", WS_OVERLAPPEDWINDOW, 0, 0, 400, 200, NULL, NULL, inst, NULL);
    field = brlw_create(parent, inst, window_proc);
    check(field != NULL, "the field made");

    check(brlw_show(field, cells, 3) == 1, "new cells change the field");
    GetWindowTextW(field, text, 64);
    check(!wcscmp(text, L"\x2801\x2803\x283F"), "dots 1, 1-2, 1-2-3-4-5-6 as Unicode braille");
    check(brlw_show(field, cells, 3) == 0, "the same cells leave it alone");
    brlw_show(field, none, 0);
    GetWindowTextW(field, text, 64);
    check(!wcscmp(text, BRLW_NONE), "no cells: \"no braille display\"");
    brlw_show(field, cells, 3);

    check((SendMessageW(field, WM_GETDLGCODE, 0, 0) & DLGC_WANTALLKEYS) != 0, "the field asks for every key");
    g_msg = 0;
    SendMessageW(field, WM_KEYDOWN, 'F', 0);
    check(g_msg == WM_KEYDOWN && g_wp == 'F' && g_from == parent, "a key down goes to the window");
    g_msg = 0;
    SendMessageW(field, WM_KEYUP, 'F', 0);
    check(g_msg == WM_KEYUP && g_wp == 'F', "its key up too");
    g_msg = 0;
    SendMessageW(field, WM_KEYDOWN, VK_END, 0);
    SendMessageW(field, WM_CHAR, 'f', 0);
    check(g_msg == WM_CHAR, "a character goes to the window");
    g_msg = 0;
    SendMessageW(field, WM_SYSKEYDOWN, VK_RIGHT, 0x20000000);
    check(g_msg == WM_SYSKEYDOWN && g_wp == VK_RIGHT, "Alt keys go to the window (Alt+Shift+Right: a bar)");
    GetWindowTextW(field, text, 64);
    check(!wcscmp(text, L"\x2801\x2803\x283F"), "the field's text unchanged by the keys");
    DestroyWindow(parent);
    printf(g_fail ? "FAILED\n" : "all passed\n");
    return g_fail;
}
