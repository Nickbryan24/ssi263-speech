"""blazie_files import --keep-exact (the idea of PR #19): text files from an image keep their line ends as they are,
for files edited on Linux (LF) or mostly on Windows (CR LF) that the firmware reads as they come.

  1. --keep-exact: an LF file and a CR LF file in the unit byte for byte;
  2. without it: both made the unit's lone CR, as before;
  3. the options in either order (--dry-run --keep-exact, --keep-exact --dry-run): accepted, nothing saved;
  4. an unknown option: the usage, nothing saved.

  python files_keep.py BLAZIE_FILES_EXE BL_STATE

Control: TEST_FILES_BREAK=8 in the environment (bl_files.h: the import's keep_lines ignored) must fail check 1.
"""
import hashlib
import os
import shutil
import subprocess
import sys
import tempfile

FAILED = []
LF = b"first line\nsecond line\n"
CRLF = b"first line\r\nsecond line\r\n"
CR = b"first line\rsecond line\r"


def check(name, ok, detail):
    print("%-4s %-58s %s" % ("ok" if ok else "FAIL", name, detail))
    if not ok:
        FAILED.append(name)


def run(argv):
    r = subprocess.run(argv, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    return r.returncode, r.stdout.decode("utf-8", "replace").replace("\r\n", "\n")


def sha(path):
    with open(path, "rb") as h:
        return hashlib.sha256(h.read()).hexdigest()


def unit_files(tool, unit, out):
    """{name: bytes} of the unit's files (blazie_files extract: the unit's own bytes)"""
    run([tool, "extract", unit, out])
    files = {}
    for d, _, names in os.walk(out):
        for n in names:
            with open(os.path.join(d, n), "rb") as h:
                files[n] = h.read()
    return files


def show(b):
    return repr(b)[:40] if b is not None else "missing"


def main():
    tool, state = sys.argv[1], sys.argv[2]
    work = tempfile.mkdtemp(prefix="files_keep_")
    try:
        unit = os.path.join(work, "unit.state")
        shutil.copyfile(state, unit)
        img, tree = os.path.join(work, "unit.img"), os.path.join(work, "tree")
        run([tool, "export", unit, img])
        run([tool, "unpack", img, tree])
        ram = sorted(d for d in os.listdir(tree) if "ram" in d.lower())
        if not ram:
            check("the unit's RAM folder in its image", 0, "folders: %s" % os.listdir(tree))
            return
        for name, data in (("lf.txt", LF), ("crlf.txt", CRLF)):
            with open(os.path.join(tree, ram[0], name), "wb") as h:
                h.write(data)
        new = os.path.join(work, "new.img")
        run([tool, "pack", tree, new])

        # 1 and 2: with and without the option
        for tag, args, want_lf, want_crlf in (("1. --keep-exact", ["--keep-exact"], LF, CRLF),
                                              ("2. without it", [], CR, CR)):
            u = os.path.join(work, "u%s.state" % tag[0])
            shutil.copyfile(state, u)
            rc, out = run([tool, "import", u, new] + args)
            got = unit_files(tool, u, os.path.join(work, "x" + tag[0]))
            lf, crlf = got.get("lf.txt"), got.get("crlf.txt")
            check(tag + ": the LF file", rc == 0 and lf == want_lf, "rc %d, %s" % (rc, show(lf)))
            check(tag + ": the CR LF file", rc == 0 and crlf == want_crlf, "rc %d, %s" % (rc, show(crlf)))

        # 3: either order, a dry run -- nothing saved
        for args in (["--dry-run", "--keep-exact"], ["--keep-exact", "--dry-run"]):
            before = sha(unit)
            rc, out = run([tool, "import", unit, new] + args)
            check("3. %s" % " ".join(args), rc == 0 and "dry run" in out and sha(unit) == before,
                  "rc %d, %s" % (rc, "unchanged" if sha(unit) == before else "SAVED"))

        # 4: an unknown option
        before = sha(unit)
        rc, out = run([tool, "import", unit, new, "--keep-exactly"])
        check("4. an unknown option: the usage, nothing saved", rc == 2 and "usage" in out and sha(unit) == before,
              "rc %d, %s" % (rc, "unchanged" if sha(unit) == before else "SAVED"))
    finally:
        shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    main()
    print("FAILED" if FAILED else "all passed")
    sys.exit(1 if FAILED else 0)
