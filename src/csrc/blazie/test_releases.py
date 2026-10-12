"""The release table through the library (bl_languages.txt -> bl_languages_table.h -> bl_firmware.c's blv_release_*):
every row as the library reports it, and for every row whose firmware is in the firmware folder, blv_release_of
finding it and blv_make_release_state making its listed state, byte for byte (its recipe's shape and answer are the
row's).  The states are made in parallel, one process each.

    python test_releases.py [--firmware DIR] [--lib PATH] [--chip PATH]

--firmware (default $SSI263_FIRMWARE, else firmware/blazie) is searched with its subfolders for each row's image; a
row whose firmware is not there is skipped, and said so.  The control (SSI263_RELEASES_TEST_BREAK=1) answers every
setup with n instead of the row's letter (blv_make_state_yes): every state made must then differ from its row's.
"""
import argparse
import ctypes
import hashlib
import os
import struct
import sys
import tempfile
from concurrent.futures import ProcessPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(REPO, "src", "platforms", "android", "test"))
import gen_languages  # noqa: E402

WINDOWS = sys.platform == "win32"
BREAK = bool(os.environ.get("SSI263_RELEASES_TEST_BREAK"))
FILE_ROM_OFFSET = 0x3000
PROGRESS = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p, ctypes.c_double)
VOICE = {"english": 0, "spanish": 1, "german": 2, "italian": 3, "slovak": 4, "-": -1}
MODEL = {"bl": 0, "bns": 1}
SHAPE = {"english": 0, "slow": 1}
N_CHORD = 0x1D                                   # n in computer braille

lib = None


def load(lib_path, chip_path):
    global lib
    import test_android_native as TAN
    lib = TAN.load_reference(lib_path, chip_path)
    for name in ("blv_firmware_label", "blv_release_id", "blv_release_tag"):
        getattr(lib, name).restype = ctypes.c_char_p
        getattr(lib, name).argtypes = [ctypes.c_int]
    for name in ("blv_firmware_language", "blv_release_model", "blv_release_code_page"):
        getattr(lib, name).argtypes = [ctypes.c_int]
    lib.blv_release_of.argtypes = [ctypes.c_char_p]
    lib.blv_make_release_state.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p, PROGRESS,
                                           ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int]
    lib.blv_make_state_yes.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_ubyte, ctypes.c_char_p, PROGRESS,
                                       ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int]
    return lib


def make(job):
    """In a worker: (release, shape, firmware, out) -> (release, ok, the state's sha256 or the error)."""
    release, shape, firmware, out = job
    err = ctypes.create_string_buffer(256)
    if BREAK:
        ok = lib.blv_make_state_yes(firmware.encode(), shape, N_CHORD, out.encode(), PROGRESS(0), None, err, 256)
    else:
        ok = lib.blv_make_release_state(firmware.encode(), release, out.encode(), PROGRESS(0), None, err, 256)
    if not ok:
        return release, False, err.value.decode(errors="replace")
    with open(out, "rb") as f:
        digest = hashlib.sha256(f.read()).hexdigest()
    os.remove(out)
    return release, True, digest


def find_images(folder, table):
    """{row index: a file in folder holding its image at 3000h}."""
    want = {(r["length"], r["image"]): k for k, r in enumerate(table)}
    lengths = sorted({r["length"] for r in table})
    found = {}
    for root, _, files in os.walk(folder):
        for name in sorted(files):
            if not name.upper().endswith(".BNS"):
                continue
            path = os.path.join(root, name)
            if os.path.getsize(path) > FILE_ROM_OFFSET + 0x40000:
                continue
            data = open(path, "rb").read()[FILE_ROM_OFFSET:]
            for n in lengths:
                k = want.get((n, hashlib.sha256(data[:n]).hexdigest()))
                if k is not None and k not in found:
                    found[k] = path
    return found


def main():
    arch = "x64" if struct.calcsize("P") == 8 else "x86"
    ap = argparse.ArgumentParser()
    ap.add_argument("--firmware", default=os.environ.get("SSI263_FIRMWARE") or os.path.join(REPO, "firmware", "blazie"))
    ap.add_argument("--lib", default=os.path.join(REPO, "nvda", "dist", "blazie-lib", arch, "bl.dll") if WINDOWS
                    else os.path.join(REPO, "build", "linux", "libssi263speech.so"))
    ap.add_argument("--chip", default=os.path.join(REPO, "src", "ssi263", "_bin", arch, "ssi263.dll") if WINDOWS
                    else None)
    a = ap.parse_args()
    table, problems = gen_languages.rows()
    if problems:
        sys.exit("bl_languages.txt: " + "; ".join(problems))
    load(a.lib, a.chip)
    bad = 0

    def result(ok, what, detail=""):
        nonlocal bad
        print("%-4s %s%s" % ("ok" if ok else "FAIL", what, (": " + detail) if detail else ""))
        bad += not ok

    if not BREAK:
        got = [(lib.blv_release_id(k), lib.blv_release_tag(k), lib.blv_firmware_label(k),
                lib.blv_firmware_language(k), lib.blv_release_model(k), lib.blv_release_code_page(k))
               for k in range(lib.blv_firmware_count())]
        want = [(r["id"].encode(), r["tag"].encode(), r["label"].encode(), VOICE[r["voice"]], MODEL[r["model"]],
                 int(r["cp"]) if r["cp"] != "-" else 0) for r in table]
        result(got == want, "the library's list = bl_languages.txt (%d releases: id, language, label, voice, model, "
               "code page)" % len(want), "" if got == want else "first difference at row %d" %
               next(k for k in range(max(len(got), len(want))) if k >= len(got) or k >= len(want) or got[k] != want[k]))
        n = len(table)
        result(lib.blv_release_id(n) is None and lib.blv_release_tag(-1) is None and lib.blv_release_model(n) == -1
               and lib.blv_release_code_page(-1) == -1, "past the list: none")
    found = find_images(a.firmware, table)
    for k, r in enumerate(table):
        if k not in found:
            print("skip %s: its firmware is not in the firmware folder" % r["id"])
    if not BREAK:
        located = {k: lib.blv_release_of(p.encode()) for k, p in found.items()}
        wrong = [table[k]["id"] for k, got in located.items() if got != k]
        result(not wrong, "blv_release_of: each of %d .BNS files found as its own release" % len(found),
               ", ".join(wrong))
    tmp = tempfile.mkdtemp(prefix="ssi263-releases-test-")
    jobs = [(k, SHAPE[table[k]["shape"]], found[k], os.path.join(tmp, "%s.state" % table[k]["id"]))
            for k in sorted(found)]
    with ProcessPoolExecutor(max_workers=min(len(jobs), os.cpu_count() or 1) or 1, initializer=load,
                             initargs=(a.lib, a.chip)) as ex:
        for k, ok, got in ex.map(make, jobs):
            r = table[k]
            how = "answered n (the control)" if BREAK else "%s recipe, %s" % (r["shape"], r["yes"])
            result(ok and got == r["state"], "%s: its state (%s) = the row's" % (r["id"], how),
                   "" if ok and got == r["state"] else (got if not ok else "a different state, %s..." % got[:16]))
    try:
        os.rmdir(tmp)
    except OSError:
        pass
    print("releases: every case as expected" if not bad else "releases: %d FAILED" % bad)
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
