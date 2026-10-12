/* bl_firmware.h -- the Braille Lite firmware a user brings, found by its content and written the way bl_create reads
 * it: a .BNS, the ROM image from file offset 3000h.
 *
 * The image starts F3 C3 xx xx FF "COPYRIGHT" (di; jp ...; then Blazie's notice).  A .BNS update file carries it at
 * 3000h (the 1998 ones at 4000h or 5000h); an update program may carry it anywhere, so the whole file is scanned.
 * Only the releases on the list (bl_firmware.c's KNOWN, by the sha256 of their image) are accepted, each one booted
 * and heard through its state recipe before it was listed (Tomi, 2026-09-30): another image -- another country's
 * release, whose addressing may differ -- is refused, never guessed at.  Blazie's other units (Braille 'n Speak, Type
 * 'n Speak, the Braille Lite 18 and 40) carry the same notice and are refused: by bl_create's firmware sites, and the
 * Braille 'n Speak 2000 (which bl_create runs, for the emulator) by bl_model.  Only hashes are here, never bytes.
 */
#ifndef BL_FIRMWARE_H
#define BL_FIRMWARE_H

#include "bl_host.h"
#include "bl_state.h"                      /* blv_state_progress */

#ifdef __cplusplus
extern "C" {
#endif

#define BLV_FW_ENGLISH 0           /* an English release on the list (BL2ENG.BNS) */
#define BLV_FW_SPANISH 1           /* a Spanish release on the list (BL2SPA.BNS) */
#define BLV_FW_GERMAN 2            /* 0.8: Blazie's September 2000 German Braille Lite 2000 (Lite2000.BNS) */
#define BLV_FW_ITALIAN 3           /* ... Italian */
#define BLV_FW_SLOVAK 4            /* the Slovak Braille 'n Speak 2000 (BS2SLL.BNS): the one Braille 'n Speak voice */
#define BLV_FW_NONE (-1)           /* no Braille Lite firmware in these bytes */
#define BLV_FW_REFUSED (-2)        /* an image, but not one the voice can run (the sites are missing: another unit) */
#define BLV_FW_WRITE (-3)          /* out_bns could not be written */
#define BLV_FW_UNKNOWN (-4)        /* a Braille Lite 2000 image the voice could start, but not a release on the list */
#define BLV_FW_NO_VOICE (-8)       /* a release on the list the emulator runs, but no voice speaks it yet (-5 to -7 are
                                      the phones' own: ssa_import.h) */

/* The offset of the first ROM image in data, or -1. */
BL_API long blv_find_image(const unsigned char *data, long n);

/* Finds the image in data (a .BNS, an update program, any file), and when it is a release on the list writes it to
   out_bns as a .BNS -- a .BNS with the image at 3000h exactly as it came, anything else behind 3000h zero bytes --
   and checks that the voice can run it.  Returns BLV_FW_ENGLISH or _SPANISH with the release's label in msg; or
   BLV_FW_NONE, _REFUSED, _UNKNOWN, _NO_VOICE, _WRITE with the reason in msg and out_bns removed. */
BL_API int blv_import_firmware(const unsigned char *data, long n, const char *out_bns, char *msg, int msglen);

/* The list: how many releases, and each one's label ("Braille Lite English: the June 5, 2003 revision") and
   language (BLV_FW_ENGLISH / _SPANISH); NULL / -1 past the end. */
BL_API int blv_firmware_count(void);
BL_API const char *blv_firmware_label(int release);
BL_API int blv_firmware_language(int release);
/* A language's state recipe (bl_state.h BLV_STATE_*) for blv_make_state: the Slovak unit takes the English one;
   -1 for an unknown language.  A language is not a voice index: each platform maps it to its own voice. */
BL_API int blv_firmware_recipe(int language);

/* 1 when the state at state_path is the one blv_make_state makes from the .BNS at bns_path (a release on the list,
   as blv_import_firmware wrote it), else 0. */
BL_API int blv_state_check(const char *bns_path, const char *state_path);

/* The list by release (bl_languages.txt, one row per firmware image; 0 .. blv_firmware_count() - 1), for the voices
   and the emulator alike: the release a .BNS holds (its image from 3000h) or -1; its stable id ("bl2_de_2000_09"),
   name in the emulators' menus ("Braille Lite 2000 (German)"), language (BCP 47), model (bl_board.h BL_MODEL_*) and DOS code page (850, 852; 0 not yet read) -- NULL / -1 past the
   end; and its state, made by its own recipe (the shape and the answer its row names), as blv_make_state. */
BL_API int blv_release_of(const char *bns_path);
BL_API const char *blv_release_id(int release);
BL_API const char *blv_release_name(int release);
BL_API const char *blv_release_tag(int release);
BL_API int blv_release_model(int release);
BL_API int blv_release_code_page(int release);
BL_API int blv_make_release_state(const char *firmware, int release, const char *out_path,
                                  blv_state_progress progress, void *ctx, char *err, int errlen);

/* sha256 of n bytes into out[32]; exported for the tests. */
BL_API void blv_sha256(const unsigned char *data, long n, unsigned char *out);

/* The tests' controls (an app never sets it): 1 drops the list, so an image the voice can start that is not on it
   is taken as English -- test_import_native.py's unknown-release case must then fail; 2 hands a release with no voice
   (the emulator's only) out as English -- test_releases.py's no-voice case must then fail. */
BL_API extern int blv_firmware_break;

#ifdef __cplusplus
}
#endif
#endif
