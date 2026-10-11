"""blazie_files add: PC files into a saved unit's flash startup folder, one batch, all or nothing (the emulator's
Firmware > "Load programs into flash", people's ask: Blazie's programs and games without an image or a cable).

  1. a program (NUL bytes) and a PC text file (CR LF) added together: both in flash startup, the program byte for
     byte, the text with the unit's lone CR, the file system's rules kept, the unit as it was in .before-import;
  2. the same names again: refused, the saved unit unchanged byte for byte; with --replace: rewritten where they are;
  3. --dry-run: nothing saved;
  4. a batch that does not fit (a file larger than the flash, with a small one): refused, the saved unit unchanged,
     the flash's free space named;
  5. two PC files that would be one unit name: refused, unchanged.

  python files_add.py BLAZIE_FILES_EXE BL_STATE

Control: TEST_FILES_BREAK=7 in the environment (bl_files.h: a new flash file's entry written in folder 0, the
deleted mark) must fail: the unit has no such files, and the batch breaks the file system's rules.
"""
import hashlib
import os
import shutil
import subprocess
import sys
import tempfile

FAILED = []
PROGRAM = bytes([0x18, 0x0C]) + b"BNS\x00" + bytes(range(256)) * 3      # a program's kind of bytes: NULs, CRs, LFs
TEXT = b"first line\r\nsecond line\r\n"


def check(name, ok, detail):
    print("%-4s %-58s %s" % ("ok" if ok else "FAIL", name, detail))
    if not ok:
        FAILED.append(name)


def run(argv):
    r = subprocess.run(argv, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    return r.returncode, r.stdout.decode("utf-8", "replace")


def last(out):
    lines = out.strip().splitlines()
    return lines[-1][-150:] if lines else ""


def sha(path):
    with open(path, "rb") as h:
        return hashlib.sha256(h.read()).hexdigest()


def write(path, data):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as h:
        h.write(data)


def extracted(tool, unit, work, tag):
    """{folder/name: bytes} of the unit's files (blazie_files extract)"""
    out = os.path.join(work, "x_" + tag)
    rc, text = run([tool, "extract", unit, out])
    files = {}
    for d, _, names in os.walk(out):
        for n in names:
            with open(os.path.join(d, n), "rb") as h:
                files[os.path.relpath(os.path.join(d, n), out).replace(os.sep, "/")] = h.read()
    return files


def main():
    tool, state = sys.argv[1], sys.argv[2]
    work = tempfile.mkdtemp(prefix="files_add_")
    try:
        unit = os.path.join(work, "unit.state")
        shutil.copyfile(state, unit)
        prog, text = os.path.join(work, "pc", "Pinball.BNS"), os.path.join(work, "pc", "read me.txt")
        write(prog, PROGRAM)
        write(text, TEXT)

        # 1. the batch
        rc, out = run([tool, "add", unit, prog, text])
        files = extracted(tool, unit, work, "1")
        got_p, got_t = files.get("flash startup/pinball.bns"), files.get("flash startup/read me.txt")
        check("1. the batch added", rc == 0 and "2 added, 0 rewritten" in out, "exit %d: %s" % (rc, last(out)))
        check("1. the program in flash startup, byte for byte", got_p == PROGRAM,
              "%s bytes" % (len(got_p) if got_p is not None else "no file"))
        check("1. the text in flash startup, its line ends the unit's",
              got_t == b"first line\rsecond line\r", repr(got_t)[:60])
        rc, out = run([tool, "check", unit])
        check("1. the file system's rules hold", rc == 0, last(out))
        before = unit + ".before-import"
        check("1. the unit as it was kept beside it", os.path.isfile(before) and sha(before) == sha(state),
              "%s.before-import" % os.path.basename(unit))

        # 2. the same names again
        h = sha(unit)
        rc, out = run([tool, "add", unit, prog, text])
        check("2. names the unit has: refused, the unit unchanged", rc != 0 and sha(unit) == h
              and "exists pinball.bns" in out, "exit %d: %s" % (rc, last(out)))
        write(prog, PROGRAM[::-1])
        rc, out = run([tool, "add", unit, prog, text, "--replace"])
        files = extracted(tool, unit, work, "2")
        check("2. --replace: rewritten where they are", rc == 0 and "0 added, 2 rewritten" in out
              and files.get("flash startup/pinball.bns") == PROGRAM[::-1], "exit %d: %s" % (rc, last(out)))

        # 3. a dry run
        h = sha(unit)
        new = os.path.join(work, "pc", "chess.bns")
        write(new, PROGRAM)
        rc, out = run([tool, "add", unit, new, "--dry-run"])
        check("3. --dry-run: nothing saved", rc == 0 and sha(unit) == h and "dry run" in out,
              "exit %d: %s" % (rc, last(out)))

        # 4. a batch that does not fit
        big = os.path.join(work, "pc", "huge.bns")
        write(big, b"\x00" * (3 * 1024 * 1024))
        rc, out = run([tool, "add", unit, new, big])
        check("4. too big for the flash: refused, the unit unchanged, the free space named",
              rc != 0 and sha(unit) == h and "flash free:" in out and "chess.bns" not in extracted(tool, unit, work,
                                                                                                    "4"),
              "exit %d: %s" % (rc, last(out)))

        # 5. two PC files, one unit name
        a, b = os.path.join(work, "pc", "one", "Same.txt"), os.path.join(work, "pc", "two", "same.TXT")
        write(a, b"a\r\n")
        write(b, b"b\r\n")
        rc, out = run([tool, "add", unit, a, b])
        check("5. two files, one unit name: refused, unchanged", rc != 0 and sha(unit) == h and "both be" in out,
              "exit %d: %s" % (rc, last(out)))
    finally:
        shutil.rmtree(work, ignore_errors=True)
    print("FAILED" if FAILED else "all passed")
    sys.exit(1 if FAILED else 0)


if __name__ == "__main__":
    main()
