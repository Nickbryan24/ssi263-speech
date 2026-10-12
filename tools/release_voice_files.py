"""The release voices' files, one per line, relative to firmware/blazie/ and to a package's data folder alike:
bl2000/<file> and bl2000/<release id>.state for every row of src/csrc/blazie/bl_languages.txt with a voice key.  For
the packaging scripts (tools/package_linux.sh, tools/release_from_ci.sh), so none keeps its own list.

    python3 tools/release_voice_files.py
"""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "src", "csrc", "blazie"))
import gen_languages  # noqa: E402

table, problems = gen_languages.rows()
if problems:
    sys.exit("bl_languages.txt: " + "; ".join(problems))
for r in table:
    if r["voice"].startswith("blazie_"):
        print("bl2000/" + r["file"])
        print("bl2000/%s.state" % r["id"])
