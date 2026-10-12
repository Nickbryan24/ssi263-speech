/* bl_files_xfer.h -- a unit's files out to a FAT disk image and back in (Tomi: "dump state file to .img", "use state
 * file from .img"; Jage and Jayson asked for a way without the serial cable).
 *
 * The image (fat_img.h: FAT16, opens in 7-Zip, mounts on Linux): one folder per folder the unit has, named as the unit
 * names it (blx_*_cp: in the unit's code page) -- "ram startup" and "flash startup" on an English unit, "RAM
 * inicial" and "FLASH inicial" on a Spanish Braille Lite, "ram subory" and "fles subory" (with their accents, cp852)
 * on the Slovak Braille 'n Speak 2000, and any the user made -- each holding the unit's files that are in it, under their unit names (as
 * long names), with their exact bytes, their time and date, and the read-only mark when the user protected them.
 * Text files are as the unit keeps them: a lone CR ends a line; a grade 2 file (type B: no extension or .brl on the
 * Braille Lite, .brl/.brf on the Type 'n Speak) holds its braille as ASCII braille, as a .brf file does -- it is not
 * translated.  The help file is not exported (its text is part of the firmware).
 *
 * Import makes the unit's files what the image holds:
 *   - a file whose name, folder and bytes match the unit's is left alone (so export, import, export gives the same
 *     image again);
 *   - other bytes for a file the unit has: the file rewritten where it is, its type and protection kept;
 *   - a file the unit lacks: added to the folder of its image folder (a flash folder: into flash; a RAM folder: into
 *     RAM).  A file at the top of the image goes to the flash startup folder (the RAM one if the flash is not set
 *     up).  An image folder the unit lacks becomes a new folder, a flash folder when the flash is set up.  Deeper
 *     folders are skipped.
 *   - a file the unit has that the image lacks: deleted -- except the clipboard, the datebook and the file the unit
 *     has open, which are kept;
 *   - in a text file (type A or B) a CR LF or a lone LF from a PC editor becomes the unit's CR;
 *   - names are made into names the unit takes (lower case, one dot, an extension of up to 3, 20 characters);
 *     renamed ones are listed.
 * Everything done, kept or refused is listed in the report, one line each.
 *
 * Portable C99.
 */
#ifndef BL_FILES_XFER_H
#define BL_FILES_XFER_H

#include <stddef.h>
#include "bl_files.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int exported, added, replaced, moved, deleted, unchanged, kept, skipped, folders_added;
    char *log;                      /* lines, "\n"-ended; NULL while empty */
    size_t len, cap;
} blx_report;

void blx_report_init(blx_report *r);
void blx_report_free(blx_report *r);

/* the unit's files as a FAT image (malloc'd; free it), its size in *size; NULL with the reason in err */
unsigned char *blx_export(const blf_fs *fs, int model, unsigned long *size, blx_report *r, char *err, int errlen);
/* the image's files into the unit (its images changed in place); 1, or 0 when nothing could be done (the image
   unreadable, the unit's file system not set up), the reason in err.  Files refused one by one are in the report. */
int blx_import(blf_fs *fs, const unsigned char *img, unsigned long size, blx_report *r, char *err, int errlen);

/* A unit name or folder name (its bytes: ASCII, and the units' code page 850 above 7Fh) as UTF-8, and back (a
   character the code page lacks becomes '_'). */
void blx_unit_to_utf8(const char *unit, char *out, size_t cap);
void blx_utf8_to_unit(const char *utf8, char *out, size_t cap);

/* The same with the unit's code page for its names: BLX_CP850 (the functions above: the English and Spanish units)
   or BLX_CP852 (the Slovak Braille 'n Speak 2000, whose own folders are "ram s\xA3bory" and "fle\xE7 s\xA3bory":
   "fles subory" with its s-caron in 852, a thorn in 850).  A file's bytes are never converted, only names. */
#define BLX_CP850 850
#define BLX_CP852 852
unsigned char *blx_export_cp(const blf_fs *fs, int model, int codepage, unsigned long *size, blx_report *r, char *err,
                             int errlen);
int blx_import_cp(blf_fs *fs, int codepage, const unsigned char *img, unsigned long size, blx_report *r, char *err,
                  int errlen);
/* blx_import_cp with its text files' line ends kept as the image has them (keep_lines 1: no CR LF or lone LF made
   CR -- for files edited on Linux or Windows that the firmware reads as they are; the idea of PR #19); 0 is
   blx_import_cp. */
int blx_import_lines(blf_fs *fs, int codepage, const unsigned char *img, unsigned long size, int keep_lines,
                     blx_report *r, char *err, int errlen);

/* Files from the PC added to the unit (the emulator's "Load programs into flash", people's ask: Blazie's programs and
   games without an image or a cable): names[i] the PC file's name (UTF-8, no folder), data[i] / sizes[i] its bytes,
   times[i] / dates[i] its time and date (DOS format).  Each goes into the flash startup folder (the RAM startup one
   when the unit's flash is not set up -- said in the report), under a name the unit takes; a text file's line ends
   made the unit's, as import does (a program, holding NUL bytes, is kept as it is).  One batch, all or nothing:
     1  every file added (r->added) or rewritten where it is (r->replaced: only with `replace`);
     2  (BLX_ADD_EXISTS) some names are the unit's already and `replace` is 0: nothing changed, each "exists NAME"
        line in the report -- ask, then call again with `replace` 1;
     0  nothing could be done, the reason in err.  The images may then be part-changed: the caller drops them (a
        saved state is only written after a 1).
   Refused, the whole batch: a name the unit cannot take, two files with one unit name, an empty file, and replacing
   the unit's own files (clipboard, datebook) or the file it has open. */
#define BLX_ADD_EXISTS 2
int blx_add_cp(blf_fs *fs, int codepage, int n, const char *const *names, const unsigned char *const *data,
               const unsigned long *sizes, const unsigned *times, const unsigned *dates, int replace, blx_report *r,
               char *err, int errlen);
void blx_unit_to_utf8_cp(int codepage, const char *unit, char *out, size_t cap);
void blx_utf8_to_unit_cp(int codepage, const char *utf8, char *out, size_t cap);

#ifdef __cplusplus
}
#endif
#endif
