/* brl_win.h -- the emulated Braille Lite's display in the Windows window: a read-only field holding the unit's cells
 * as Unicode braille (U+2800 + the dots), which the screen reader -- whichever one -- shows on the user's own braille
 * display, whatever its size (the unit's cells at its left; panned by the screen reader if the display is shorter).
 * The same idea as brl_linux.h, through the screen reader instead of BRLTTY.
 *
 * The field keeps the keyboard while it is shown: every key it gets goes to the window's own procedure, so the
 * unit's keys work as before, and the field never edits or moves.  The display's own keys stay the screen reader's;
 * the unit's bars are the PC keyboard's (main_win.c: ; and A, and Alt+Shift+Right and Left).  A unit without a
 * display (the Type 'n Speak, the Braille 'n Speak 2000) shows BRLW_NONE.
 */
#ifndef BLAZIE_BRL_WIN_H
#define BLAZIE_BRL_WIN_H

#include <windows.h>

#define BRLW_NONE L"no braille display"

/* the field and its label ("Braille display", its name to a screen reader), children of parent, hidden; keys: the
   procedure the field's keys go to (the window's own) */
HWND brlw_create(HWND parent, HINSTANCE inst, WNDPROC keys);
/* the field and its label shown or hidden */
void brlw_visible(HWND field, int on);
/* the field across the window's client area (width, height) under its label */
void brlw_layout(HWND field, int width, int height);
/* the unit's cells shown (dot 1 = bit 0 .. dot 8 = bit 7), when they changed: 1 if the field's text changed */
int brlw_show(HWND field, const unsigned char *cells, int n);
/* the field's text for n cells: n characters and a 0 (cap at least n + 1), or BRLW_NONE when n is 0 */
void brlw_text(const unsigned char *cells, int n, wchar_t *out, int cap);

#endif
