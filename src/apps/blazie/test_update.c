/* test_update.c -- the Braille Lite 2000's firmware update on the emulated unit, headless (Fireborn; Tomi: "it
 * rapidly beeps after starting the update").
 *
 *   test_update FIRMWARE STATE UPDATE UPDATE_STATE
 *
 * FIRMWARE + STATE: the English unit (BL2ENG.BNS, bl2_2003_warm.state).  UPDATE: another language's update program
 * (german/Lite2000.BNS, Blazie's September 20, 2000 update), UPDATE_STATE that language's own unit state (the
 * reference for what the updated unit says).  Never in the repo: run_tests passes them, or the test is skipped.
 *
 * Blazie's procedure, as its update guide gives it: the update program goes into the unit's files (here straight into
 * the flash folder as upd.bns, the type X a .bns gets: the emulated unit's RAM has too few free pages for its 67), and
 * runs with o-chord x upd.bns e-chord -- from that folder, so the keys first allow folder mode (the status menu's
 * item 43) and move there.  The update speaks its own language: two questions (j, j) and, on a unit with the super
 * flash, which bank (b: the second language, as Blazie advised, the first left as it was).  It reads the program
 * flash's ID (an Am29F040), erases the bank's four sectors in one multi-sector erase, programs it byte by byte with a
 * chirp every 8 KB, and ends on a steady tone (phoneme 17h left sounding: "a continuous tone means success").
 *
 * Checks: the update finishes on that tone; bank a is the English image byte for byte; bank b the update's image byte
 * for byte, but for byte 4, which the update stamps with the bank's E0h value (10h); chirps while it programs; and
 * after a power-on with that program flash (bl_program_flash), p-chord l restarts the unit into bank b, which then
 * speaks as the update's own unit starting (its first phonemes), not as the English one.
 *
 * Must fail: test_update_break.exe, built with BLAZIE_PROGRAM_FLASH_BREAK (the program flash's writes dropped, as
 * before 0.8: the update fails, bank b stays English); test_update_single.exe, built with FLASH29_NO_MULTI (a sector
 * erase takes no further sectors: three of the bank's sectors are never erased and their bytes cannot be programmed).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#define _getpid getpid
#endif
#include "../../csrc/blazie/bl_board.h"
#include "../../csrc/blazie/bl_files.h"
#include "../../csrc/blazie/bl_files_state.h"

#define STEP_CYCLES 122880ULL                /* 20 ms of the Braille Lite's 6.144 MHz */
#define ROM_OFFSET 0x3000L                   /* the .BNS's program image starts here (bl_board.c FILE_ROM_OFFSET) */
#define BANK 0x40000L
#define GAP 1.6                              /* between keys, in the unit's seconds */
#define PHONES 12                            /* the restart's first phonemes compared */
#define BATTERY_LEVEL 128                    /* emu_unit.c's: the status menu's % reads the gauge (without it: frozen) */
#define HOST_TIME 1790000000LL               /* the clock controller's time, fixed: every run the same */

static int failures;

static void check(const char *name, int ok, const char *detail)
{
    printf("%-4s %-44s %s\n", ok ? "ok" : "FAIL", name, detail);
    failures += !ok;
}

static unsigned char *read_all(const char *path, long *n)
{
    FILE *f = fopen(path, "rb");
    unsigned char *p = NULL;
    if (f && fseek(f, 0, SEEK_END) == 0 && (*n = ftell(f)) > 0 && fseek(f, 0, SEEK_SET) == 0
            && (p = (unsigned char *)malloc((size_t)*n)) != NULL && fread(p, 1, (size_t)*n, f) != (size_t)*n) {
        free(p);
        p = NULL;
    }
    if (f)
        fclose(f);
    return p;
}

/* a bank as the board loads a .BNS: the image from ROM_OFFSET, FFh-padded to 256 KB */
static void bank_of(const unsigned char *bns, long n, unsigned char *out)
{
    long m = n - ROM_OFFSET;
    memset(out, 0xFF, BANK);
    if (m > 0)
        memcpy(out, bns + ROM_OFFSET, (size_t)(m < BANK ? m : BANK));
}

/* the state with the update added to the flash startup folder as upd.bns, saved to out */
static int add_update(const char *state, const unsigned char *data, long n, const char *out)
{
    char err[256];
    bls_unit b;
    blf_fs *fs;
    int ok;
    if (!bls_load(state, &b, err, sizeof err)) { printf("FAIL load %s: %s\n", state, err); return 0; }
    fs = blf_open(b.model, b.ram, b.flash, b.flash_size, err, sizeof err);
    ok = fs && blf_add(fs, 1, "upd.bns", data, (unsigned long)n, 0, (46 << 9) | (6 << 5) | 1, 0, err, sizeof err);
    if (ok && blf_get(fs, blf_find(fs, "upd.bns"))->type != 'X') {
        snprintf(err, sizeof err, "not a program (type %c)", blf_get(fs, blf_find(fs, "upd.bns"))->type);
        ok = 0;
    }
    if (fs)
        blf_close(fs);
    ok = ok && bls_save(out, &b, err, sizeof err);
    if (!ok)
        printf("FAIL the update into the unit: %s\n", err);
    bls_free(&b);
    return ok;
}

/* ---- running the unit ------------------------------------------------------------------------------------------- */
/* a unit as the emulator makes it (emu_unit.c): the battery gauge and the clock controller on */
static bl_unit *unit_new(const char *fw, const char *st)
{
    char err[256];
    blc_time now;
    time_t secs = (time_t)HOST_TIME;
    struct tm *tm = gmtime(&secs);
    bl_unit *u = bl_create(fw, st, 20.0, NULL, NULL, 0, err, sizeof err);
    if (!u) {
        printf("FAIL create: %s\n", err);
        exit(1);
    }
    bl_battery(u, BATTERY_LEVEL);
    memset(&now, 0, sizeof now);
    now.year = tm->tm_year + 1900;
    now.month = tm->tm_mon + 1;
    now.day = tm->tm_mday;
    now.hour = tm->tm_hour;
    now.minute = tm->tm_min;
    now.second = tm->tm_sec;
    if (!bl_clock_on(u, &now, HOST_TIME)) {
        printf("FAIL the clock: out of memory\n");
        exit(1);
    }
    return u;
}

typedef struct {
    double last_write;                       /* the unit's time of its last chip write */
    int last_phone;                          /* its last phoneme load (R0, control bit clear), -1 none */
    int chirps;                              /* phoneme 17h loads while the program flash was being written */
    int phones[PHONES], n_phones;            /* phonemes loaded after `listen_from` */
    double listen_from;
} watch_t;

/* run to `secs` (or until the update's tone has held `tone_s`, when tone_s > 0), pressing keys[i] at at[i] */
static double run(bl_unit *u, const int *keys, const double *at, int n_keys, double secs, double tone_s, watch_t *w)
{
    int i, k, ctl = 0, steps = (int)(secs * 50);
    unsigned long writes = 0, before;
    for (i = 0; i < steps; i++) {
        const bl_event *ev;
        int m;
        double t = i * 0.02;
        for (k = 0; k < n_keys; k++)
            if ((int)(at[k] * 50 + 0.5) == i)
                bl_key(u, keys[k]);
        before = writes;
        bl_run(u, STEP_CYCLES);
        bl_program_flash(u, NULL, &writes);
        m = bl_events(u, &ev);
        for (k = 0; k < m; k++) {
            if (ev[k].type != 'W')
                continue;
            w->last_write = t;
            if (ev[k].a == 3)
                ctl = ev[k].b >> 7;
            else if (ev[k].a == 0 && !ctl) {
                int ph = ev[k].b & 0x3F;
                if (ph == 0x17 && writes > before)
                    w->chirps++;
                if (ph && t >= w->listen_from && w->n_phones < PHONES)
                    w->phones[w->n_phones++] = ph;
                w->last_phone = ph;
            }
        }
        bl_clear_events(u);
        if (tone_s > 0 && writes > 0 && w->last_phone == 0x17 && t - w->last_write >= tone_s)
            return t;
    }
    return secs;
}

/* the keys of the update, from the unit's main menu (computer braille, as test_games.c types it) */
static int script(int *keys, double *at)
{
    static const char *name = "upd.bns";
    static const int letters[] = {0x25, 0x0F, 0x19, 0x28, 0x03, 0x1D, 0x0E};   /* u p d . (dots 4-6) b n s */
    int n = 0, i;
    double t = 4.0;
#define KEY(k) do { keys[n] = (k); at[n++] = t; t += GAP; } while (0)
    KEY(0x4C);                               /* the status menu (dots 3-4 chord) */
    for (i = 0; i < 42; i++)
        KEY(0x48);                           /* on to item 43, "allow folder mode" */
    KEY(0x3D);                               /* y */
    KEY(0x51);                               /* e-chord: out */
    KEY(0x55); KEY(0x0B); KEY(0x40); KEY(0x70); KEY(0x51);   /* o-chord f, space (folder mode), next folder, out */
    KEY(0x55); KEY(0x2D);                    /* o-chord x: "enter program to execute" */
    for (i = 0; i < (int)strlen(name); i++)
        KEY(letters[i]);
    KEY(0x51);                               /* e-chord: run it */
    KEY(0x1A); KEY(0x1A);                    /* j, j (the German update's questions) */
    KEY(0x03);                               /* b: the second bank */
#undef KEY
    return n;
}

int main(int argc, char **argv)
{
    char d[400], path[64];
    long n_en, n_up;
    unsigned char *en, *up, *want_a, *want_b, *pf, *saved;
    int keys[96], n_keys, i, diff_a = 0, diff_b = 0, first_b = -1, same_en = 1, same_up = 1;
    double at[96], done;
    unsigned long writes = 0;
    bl_unit *u;
    watch_t w, wr, we, wu;
    if (argc < 5) {
        printf("usage: test_update FIRMWARE STATE UPDATE UPDATE_STATE\n");
        return 2;
    }
    en = read_all(argv[1], &n_en);
    up = read_all(argv[3], &n_up);
    if (!en || !up) {
        printf("FAIL read the firmware\n");
        return 1;
    }
    want_a = (unsigned char *)malloc(BANK);
    want_b = (unsigned char *)malloc(BANK);
    saved = (unsigned char *)malloc(2 * BANK);
    bank_of(en, n_en, want_a);
    bank_of(up, n_up, want_b);
    snprintf(path, sizeof path, "test_update.%d.state", (int)_getpid());
    if (!add_update(argv[2], up, n_up, path))
        return 1;

    /* the update, with the flash's own timing (the emulator's) */
    u = unit_new(argv[1], path);
    bl_flash_timed(u, 1);
    memset(&w, 0, sizeof w);
    w.last_phone = -1;
    w.listen_from = 1e9;
    n_keys = script(keys, at);
    done = run(u, keys, at, n_keys, 700.0, 10.0, &w);
    bl_program_flash(u, &pf, &writes);
    memcpy(saved, pf, 2 * BANK);
    bl_destroy(u);
    snprintf(d, sizeof d, "the steady tone (phoneme %02Xh, no write for 10 s) at %.0f s; %lu program flash writes",
             w.last_phone < 0 ? 0 : w.last_phone, done, writes);
    check("the update ends on its tone", done < 700.0 && writes > 0, d);
    for (i = 0; i < BANK; i++) {
        diff_a += saved[i] != want_a[i];
        if (saved[BANK + i] != want_b[i] && i != 4) {
            diff_b++;
            if (first_b < 0)
                first_b = i;
        }
    }
    snprintf(d, sizeof d, "%d of %ld bytes differ from the English image", diff_a, BANK);
    check("bank a: the English unit, untouched", diff_a == 0, d);
    snprintf(d, sizeof d, "%d of %ld bytes differ from the update's image%s; byte 4 %02Xh (want 10h, the bank stamp)",
             diff_b, BANK, first_b < 0 ? "" : " (the first at its offset given below)", saved[BANK + 4]);
    if (first_b >= 0)
        printf("     first difference at %05Xh: %02Xh, want %02Xh\n", first_b, saved[BANK + first_b], want_b[first_b]);
    check("bank b: the update's image", diff_b == 0 && saved[BANK + 4] == 0x10, d);
    snprintf(d, sizeof d, "%d chirps (phoneme 17h) while it programmed (want >= 30: one per 8 KB)", w.chirps);
    check("chirps while programming", w.chirps >= 30, d);

    /* power on again with that program flash: the English unit starts (bank a); p-chord l restarts it in bank b */
    {
        static const int pl[] = {0x4F, 0x07};
        static const double pl_at[] = {6.0, 7.6};
        u = unit_new(argv[1], argv[2]);
        bl_program_flash(u, &pf, NULL);
        memcpy(pf, saved, 2 * BANK);
        memset(&wr, 0, sizeof wr);
        wr.last_phone = -1;
        wr.listen_from = 7.6;
        run(u, pl, pl_at, 2, 16.0, 0, &wr);
        bl_destroy(u);
        /* the references: the English unit's start and the update's own unit's start, their first phonemes */
        memset(&we, 0, sizeof we);
        memset(&wu, 0, sizeof wu);
        u = unit_new(argv[1], argv[2]);
        run(u, NULL, NULL, 0, 8.0, 0, &we);
        bl_destroy(u);
        u = unit_new(argv[3], argv[4]);
        run(u, NULL, NULL, 0, 8.0, 0, &wu);
        bl_destroy(u);
        for (i = 0; i < PHONES; i++) {
            same_en &= i < wr.n_phones && i < we.n_phones && wr.phones[i] == we.phones[i];
            same_up &= i < wr.n_phones && i < wu.n_phones && wr.phones[i] == wu.phones[i];
        }
        snprintf(d, sizeof d, "after p-chord l, %d phonemes: %s the update's unit's start, %s the English one's",
                 wr.n_phones, same_up ? "as" : "NOT as", same_en ? "AS" : "not as");
        check("p-chord l starts the updated bank", wr.n_phones == PHONES && same_up && !same_en, d);
        printf("     after p-chord l:");
        for (i = 0; i < wr.n_phones; i++)
            printf(" %02X", wr.phones[i]);
        printf("\n     the update's unit:");
        for (i = 0; i < wu.n_phones; i++)
            printf(" %02X", wu.phones[i]);
        printf("\n");
    }
    remove(path);
    printf("update: %s\n", failures ? "FAILED" : "all passed");
    return failures ? 1 : 0;
}
