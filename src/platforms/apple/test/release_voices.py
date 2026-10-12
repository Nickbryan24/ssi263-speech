"""The release voices' files for the Apple tests (ssa_engine.h's SSA_RELEASE_FIRST on: Hungarian, Turkish, ...), as the
app keeps them in its unit folder: each voice's .BNS under the name a person brings (blv_release_file: BL2HUN.BNS),
found by content in the firmware folder, and its state (<release id>.state) made by the release's own recipe.

A state takes the desktop library seconds to make (the slow recipe's twelve answers), so each is made once, in
parallel, and kept in build/apple/test/release-states -- used only while its sha256 is still the table's (the state
column of bl_languages.txt), so a stale one is made again, never trusted.
"""
import ctypes
import hashlib
import os
import shutil
import sys
from concurrent.futures import ProcessPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", "..", "..", ".."))
BLAZIE = os.path.join(REPO, "src", "csrc", "blazie")
sys.path.insert(0, BLAZIE)
import gen_languages  # noqa: E402
import test_releases  # noqa: E402

LIB = os.path.join(REPO, "build", "linux", "libssi263speech.so")      # build_linux.sh's, on the Mac too
STATES = os.path.join(REPO, "build", "apple", "test", "release-states")


def _sha(path):
    with open(path, "rb") as f:
        return hashlib.sha256(f.read()).hexdigest()


def _make(job):
    """In a worker: (row, firmware, out) -> (row, error or "")."""
    k, firmware, out = job
    err = ctypes.create_string_buffer(256)
    ok = test_releases.lib.blv_make_release_state(firmware.encode(), k, out.encode(), test_releases.PROGRESS(0), None,
                                                  err, 256)
    return k, "" if ok else err.value.decode(errors="replace")


def voices(firmware):
    """[(row, the table's row dict, the .BNS path)] for every release voice whose firmware is in the folder."""
    table, problems = gen_languages.rows()
    if problems:
        sys.exit("bl_languages.txt: " + "; ".join(problems))
    found = test_releases.find_images(firmware, table)
    return [(k, table[k], found[k]) for k in sorted(found) if table[k]["voice"].startswith("blazie_")]


def add(data, firmware):
    """Copies each release voice found into the data folder (its .BNS and state, the app's names); returns
    ([(voice key, release id)] of those added, failures), and says which have no firmware here."""
    rows = voices(firmware)
    table, _ = gen_languages.rows()
    have = {k for k, _, _ in rows}
    for k, r in enumerate(table):
        if r["voice"].startswith("blazie_") and k not in have:
            print("skip  the %s voice: no %s in the firmware folder" % (r["voice"], r["file"]))
    if not rows:
        return [], 0
    if not os.path.isfile(LIB):
        print("skip  the release voices: no %s (run sh build_linux.sh)" % os.path.relpath(LIB, REPO))
        return [], 0
    os.makedirs(STATES, exist_ok=True)
    jobs = [(k, bns, os.path.join(STATES, r["id"] + ".state")) for k, r, bns in rows
            if not (os.path.isfile(os.path.join(STATES, r["id"] + ".state"))
                    and _sha(os.path.join(STATES, r["id"] + ".state")) == r["state"])]
    if jobs:
        print("      making %d release state(s) once (kept in %s)" % (len(jobs), os.path.relpath(STATES, REPO)))
        with ProcessPoolExecutor(max_workers=max(1, min(len(jobs), (os.cpu_count() or 2) // 2)),
                                 initializer=test_releases.load, initargs=(LIB, None)) as ex:
            for k, err in ex.map(_make, jobs):
                if err:
                    print("      the %s state: %s" % (table[k]["id"], err))
    added, bad = [], 0
    for k, r, bns in rows:
        state = os.path.join(STATES, r["id"] + ".state")
        if not os.path.isfile(state) or _sha(state) != r["state"]:
            print("FAIL  the %s state is not the table's" % r["id"])
            bad += 1
            continue
        shutil.copyfile(bns, os.path.join(data, r["file"]))
        shutil.copyfile(state, os.path.join(data, r["id"] + ".state"))
        added.append((r["voice"], r["id"]))
    print("%-5s %d release voices' files, each state the table's" % ("ok" if not bad else "FAIL", len(added)))
    return added, bad


def unit_only(firmware):
    """(the table's row index, row, .BNS path) of one release no voice speaks (the emulator's only), or None."""
    table, _ = gen_languages.rows()
    found = test_releases.find_images(firmware, table)
    for k in sorted(found):
        if table[k]["voice"] == "-":
            return k, table[k], found[k]
    return None
