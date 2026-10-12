"""The release voices' files in the firmware folder: for every row of src/csrc/blazie/bl_languages.txt with a voice
key (blazie_hu ...), its image as firmware/blazie/bl2000/<file> (found by its content anywhere under
firmware/blazie, copied there when it is elsewhere) and its state as firmware/blazie/bl2000/<release id>.state, made by
the release's own recipe (blv_make_release_state) and checked against the table's hash (blv_state_check).  What the
add-on, SAPI and the Linux package then carry (nvda/build_common.py RELEASE_UNITS); the firmware never goes in git.

    python tools/make_release_states.py [--firmware DIR] [--lib bl.dll|libssi263speech.so] [--chip ssi263.dll]
"""
import argparse
import ctypes
import os
import shutil
import struct
import sys
from concurrent.futures import ProcessPoolExecutor

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BL_SRC = os.path.join(REPO, "src", "csrc", "blazie")
sys.path.insert(0, BL_SRC)
import gen_languages  # noqa: E402
import test_releases  # noqa: E402

PROGRESS = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p, ctypes.c_double)


def make(job):
    k, fw, out = job
    lib = test_releases.lib
    err = ctypes.create_string_buffer(300)
    if not lib.blv_make_release_state(fw.encode(), k, out.encode(), PROGRESS(0), None, err, 300):
        return k, err.value.decode(errors="replace")
    lib.blv_state_check.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
    if lib.blv_state_check(fw.encode(), out.encode()) != 1:
        os.remove(out)
        return k, "the state made is not the table's"
    return k, None


def main():
    win = sys.platform == "win32"
    arch = "x64" if struct.calcsize("P") == 8 else "x86"
    ap = argparse.ArgumentParser()
    ap.add_argument("--firmware", default=os.path.join(REPO, "firmware", "blazie"))
    ap.add_argument("--lib", default=os.path.join(REPO, "nvda", "dist", "blazie-lib", arch, "bl.dll") if win
                    else os.path.join(REPO, "build", "linux", "libssi263speech.so"))
    ap.add_argument("--chip", default=os.path.join(REPO, "src", "ssi263", "_bin", arch, "ssi263.dll") if win else None)
    a = ap.parse_args()
    table, problems = gen_languages.rows()
    if problems:
        sys.exit("bl_languages.txt: " + "; ".join(problems))
    found = test_releases.find_images(a.firmware, table)
    dest = os.path.join(a.firmware, "bl2000")
    os.makedirs(dest, exist_ok=True)
    jobs, missing = [], []
    for k, r in enumerate(table):
        if not r["voice"].startswith("blazie_"):
            continue
        if k not in found:
            missing.append("%s (%s)" % (r["voice"], r["file"]))
            continue
        fw = os.path.join(dest, r["file"])
        if os.path.normcase(os.path.abspath(found[k])) != os.path.normcase(os.path.abspath(fw)):
            shutil.copy2(found[k], fw)
        jobs.append((k, fw, os.path.join(dest, r["id"] + ".state")))
    with ProcessPoolExecutor(max_workers=max(1, min(len(jobs), (os.cpu_count() or 2) // 2)),
                             initializer=test_releases.load, initargs=(a.lib, a.chip)) as ex:
        bad = [(table[k]["voice"], e) for k, e in ex.map(make, jobs) if e]
    for v, e in bad:
        print("FAIL %s: %s" % (v, e))
    for m in missing:
        print("missing %s: its firmware is not under %s" % (m, a.firmware))
    print("%d release voices' files in %s%s" % (len(jobs) - len(bad), dest, "" if not (bad or missing) else
                                                 " (%d failed, %d missing)" % (len(bad), len(missing))))
    sys.exit(1 if bad or missing else 0)


if __name__ == "__main__":
    main()
