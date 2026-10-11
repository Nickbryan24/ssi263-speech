"""The BASIC tones (params mode1_rate15_formants, 0.8): watched in the chip's own state and in its sound.

The Braille Lite's BASIC Sound command plays each note as phoneme M in mode 1 at speech rate 15, articulation 0, with
PA between notes (chime.bas, the Pinball lesson).  Through 0.7 the formants glided into M at every note's start, a
bright +7..+8 dB burst the real chip does not make (Jayson's "crackle"; real starts +0.2..+0.5 dB).  Retuning at once
(Astra's preview C) cleaned the starts but retuned the tract to PA while M was still fading: a click at every note's
END (Tomi's ear).  'pa_wait' (preview F, Tomi: "sounds perfect", 2026-10-11): a sounding phoneme retunes at once; PA
holds the tract until the VA and FA latches are both zero, then retunes.  Mode 1 at rate 15 only.

  STATE, every output sample, Python reference and C core: the first sample after M's load has F1 F2 F3 NAS at M's
  targets; from PA's load until both latches are zero they stay at M's; within a sample after that they are at PA's.
  SOUND: a note's first 25 ms and the 10 ms before to 30 ms after its PA carry no more 3-10 kHz energy per pitch
  period than the note's steady part (+2 dB allowed; preview C's tails reached +12, 0.7's starts +8).
  PARITY: Python and C sample for sample (within check_native_core's 1e-9).
  CHUNKS: the C core rendered one sample per call equals one call for the whole sequence.
  SKIP: ssi263_skip / chip.skip across PA's latch crossing (before it, after it) leaves the formant fields and latches
  where running would, and the next note's start is clean.
  INACTIVE: mode 1 at rate 14, mode 3 at rate 15 and 8, and noise -> PA render identically under all three settings.

    python tools/check_tone_tract.py            exit 1 on any failure

Controls (TONE_TRACT_BREAK), each of which must fail, and for its own reason: slew (0.7's glide into M: the start
checks), immediate (preview C: the PA checks and the tail)."""
import math
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
sys.path.insert(0, os.path.join(REPO, "src"))
from ssi263 import SSI263                      # noqa: E402
from ssi263.native import SSI263C              # noqa: E402
from ssi263.rom import Rom                     # noqa: E402

BREAK = os.environ.get("TONE_TRACT_BREAK", "")
POLICY = BREAK or "pa_wait"
RATE = 44100
M, PA = 0x37, 0x00
FORMANTS = ("F1", "F2", "F3", "NAS")
ROM = Rom.load()
LIMIT_DB = 2.0
# CHIME's four pitch / filter-clock settings (R1, R4)
NOTES = [(0xD1, 0xF8), (0xC4, 0xF8), (0xCB, 0xF8), (0xB0, 0xF7)]
fails = []


def make(core, policy=POLICY):
    p = {"mode1_rate15_formants": policy}
    return SSI263C(p, out_rate=RATE) if core == "C" else SSI263(p, out_rate=RATE, dsp="c")


def observe(chip):
    if isinstance(chip, SSI263C):
        v = chip.state()
        cur, lat = dict(zip(("F1", "F2", "F3", "NAS", "VA", "FA"), v[15:21])), v[21:27]
        return cur, dict(zip(("F1", "F2", "F3", "NAS", "VA", "FA"), lat))
    return dict(chip.cur), dict(chip.latch)


def basic_setup(chip, pitch, ff, mode=1, rate=15):
    """As the firmware's BASIC Sound sets the chip up: mode 1, R2 = FF (rate 15), articulation 0, amplitude 10."""
    for a, v in [(3, 0x80), (0, mode << 6), (1, pitch), (2, (rate << 4) | 15), (3, 0x0A), (4, ff), (0, PA)]:
        chip.write(a, v)


def run_rows(chip, seconds):
    """One output sample per call, the chip's state kept after each."""
    rows = []
    for _ in range(int(round(seconds * RATE))):
        y = chip.run(1.0 / RATE)
        cur, lat = observe(chip)
        rows.append((y[-1] if len(y) else 0.0, cur, lat))
    return rows


def note(core, pitch, ff, phase):
    """PA 50 ms, M for 150 ms plus phase/32 of a pitch period, PA 60 ms.  Returns (rows, i_M, i_PA)."""
    chip = make(core)
    basic_setup(chip, pitch, ff)
    rows = run_rows(chip, 0.05)
    f0 = 1e6 / (8 * (4096 - (0x807 + pitch * 8)))
    i_m = len(rows)
    chip.write(0, M)
    rows += run_rows(chip, 0.15 + phase / 32.0 / f0)
    i_pa = len(rows)
    chip.write(0, PA)
    rows += run_rows(chip, 0.06)
    return rows, i_m, i_pa, f0


# ---- the sound: causal 3-10 kHz, energy per pitch period ---------------------------------------------------------
def biquad(x, kind, f, q=1 / math.sqrt(2)):
    """RBJ cookbook second-order section, causal."""
    w = 2 * math.pi * f / RATE
    a = math.sin(w) / (2 * q)
    cw = math.cos(w)
    if kind == "hp":
        b = [(1 + cw) / 2, -(1 + cw), (1 + cw) / 2]
    else:
        b = [(1 - cw) / 2, 1 - cw, (1 - cw) / 2]
    den = [1 + a, -2 * cw, 1 - a]
    y = np.zeros(len(x))
    x1 = x2 = y1 = y2 = 0.0
    for i, v in enumerate(x):
        o = (b[0] * v + b[1] * x1 + b[2] * x2 - den[1] * y1 - den[2] * y2) / den[0]
        x2, x1, y2, y1 = x1, v, y1, o
        y[i] = o
    return y


def hf_excess(ys, i_m, i_pa, f0):
    x = np.asarray(ys, dtype=float)
    hf = biquad(biquad(biquad(biquad(x, "hp", 3000.0), "hp", 3000.0), "lp", 10000.0), "lp", 10000.0)
    p = int(round(RATE / f0))
    e = np.convolve(hf * hf, np.ones(p), "valid")
    steady = e[i_m + int(0.06 * RATE): i_m + int(0.14 * RATE) - p].mean()
    start = e[i_m: i_m + int(0.025 * RATE)].max()
    tail = e[i_pa - int(0.010 * RATE): i_pa + int(0.030 * RATE) - p].max()
    return 10 * math.log10(start / steady), 10 * math.log10(tail / steady)


# ---- the judgement ---------------------------------------------------------------------------------------------------
def judge_note(core, pitch, ff, phase):
    rows, i_m, i_pa, f0 = note(core, pitch, ff, phase)
    label = "%-2s R1=%02X R4=%02X phase %2d/32" % (core, pitch, ff, phase)
    bad = []
    m_t, pa_t = ROM.entry(M), ROM.entry(PA)
    # the start: the first sample after M's load is at M's tract
    cur = rows[i_m][1]
    off = [f for f in FORMANTS if cur[f] != m_t[f]]
    if off:
        bad.append("start: the tract slewed into M (%s)" % ", ".join("%s %.2f not %d" % (f, cur[f], m_t[f]) for f in off))
    # through PA until both latches are zero: M's tract held; then PA's within a sample
    held = [f for f in FORMANTS if rows[i_pa - 1][1][f] != m_t[f]]
    if held:
        bad.append("M's tract not reached by PA's load (%s)" % ", ".join(held))
    k_zero = next((k for k in range(i_pa, len(rows)) if rows[k][2]["VA"] == 0 and rows[k][2]["FA"] == 0), None)
    if k_zero is None:
        bad.append("PA: the VA/FA latches never reached zero")
    else:
        moved = next((k for k in range(i_pa, k_zero) if any(rows[k][1][f] != m_t[f] for f in FORMANTS)), None)
        if moved is not None:
            bad.append("PA: the tract retuned %.2f ms after PA's load with the VA latch at %d" % (
                (moved - i_pa + 1) * 1e3 / RATE, rows[moved][2]["VA"]))
        if k_zero + 1 < len(rows) and any(rows[k_zero + 1][1][f] != pa_t[f] for f in FORMANTS):
            bad.append("PA: the tract did not retune once the latches were zero")
    start_db, tail_db = hf_excess([r[0] for r in rows], i_m, i_pa, f0)
    if start_db > LIMIT_DB:
        bad.append("start: %+.1f dB of 3-10 kHz over the steady note" % start_db)
    if tail_db > LIMIT_DB:
        bad.append("tail: %+.1f dB of 3-10 kHz over the steady note" % tail_db)
    report(label, bad, "start %+.2f dB, tail %+.2f dB, PA tract held %.2f ms" % (
        start_db, tail_db, ((k_zero or i_pa) - i_pa + 1) * 1e3 / RATE))
    return [r[0] for r in rows]


def report(label, bad, ok_text):
    if bad:
        fails.append(label)
    print("%s  %-36s %s" % ("FAIL" if bad else "ok  ", label, "; ".join(bad) if bad else ok_text))


def check_parity(py, c):
    d = max(abs(a - b) for a, b in zip(py, c)) if len(py) == len(c) else float("inf")
    report("parity: Python = C (tone)", [] if d <= 1e-9 else ["max difference %.3g" % d], "max difference %.1g" % d)


def sequence(chip, chunked):
    """Two notes and a long PA, in one call or one sample per call."""
    basic_setup(chip, 0xD1, 0xF8)
    out = []
    for code, sec in [(PA, 0.03), (M, 0.12), (PA, 0.003), (M, 0.08), (PA, 0.05)]:
        chip.write(0, code)
        if chunked:
            for _ in range(int(round(sec * RATE))):
                out += list(chip.run(1.0 / RATE))
        else:
            out += list(chip.run(sec))
    return out


def check_chunks():
    one = sequence(make("C"), False)
    many = sequence(make("C"), True)
    report("chunks: 1 call = 1 sample per call", [] if one == many else ["the renders differ"], "identical")


def check_skip(core):
    """Run M for 150 ms, load PA, then skip vs run to a time before / after PA's latch crossing.  A skip stops at a
    phoneme's timer (mode 1 at rate 15: PA's 4.1 ms, before the crossing), as a flushing host's loop meets it, so it is
    repeated up to the chip time.  The formant fields must be where the rule puts them for the latches the skip ends
    with, and the latches where running puts them."""
    m_t, pa_t = ROM.entry(M), ROM.entry(PA)
    for t, when in ((0.001, "before the latch crossing"), (0.012, "after it")):
        a, b = make(core), make(core)
        for chip in (a, b):
            basic_setup(chip, 0xD1, 0xF8)
            chip.write(0, M)
            chip.run(0.15)
            chip.write(0, PA)
        t_end, calls = a.time + t, 0
        while a.time < t_end - 1e-12 and calls < 10:
            a.skip(t_end - a.time)
            calls += 1
        b.run(t)
        (ca, la), (cb, lb) = observe(a), observe(b)
        bad = ["latch %s: skip %d, run %d" % (f, la[f], lb[f]) for f in FORMANTS + ("VA", "FA") if la[f] != lb[f]]
        want = pa_t if la["VA"] == 0 and la["FA"] == 0 else m_t
        bad += ["%s %.2f after the skip, the rule says %d" % (f, ca[f], want[f]) for f in FORMANTS if ca[f] != want[f]]
        bad += ["%s: skip %.2f, run %.2f" % (f, ca[f], cb[f]) for f in FORMANTS if ca[f] != cb[f]]
        # the next note after the skip starts clean: at M's tract at once
        a.write(0, M)
        a.run(1.0 / RATE)
        cur = observe(a)[0]
        if any(cur[f] != ROM.entry(M)[f] for f in FORMANTS):
            bad.append("the next note slewed after the skip")
        report("skip %s %.0f ms (%s)" % (core, t * 1e3, when), bad, "formants and latches as running; next start clean")


def check_inactive():
    for mode, rate in [(1, 14), (3, 15), (3, 8)]:
        outs = []
        for policy in ("slew", "immediate", "pa_wait"):
            chip = make("C", policy)
            basic_setup(chip, 0xD1, 0xF8, mode, rate)
            z = []
            for code in [PA, M, PA, 0x01, M, PA, 0x30, PA, 0x2C, PA]:      # ... S PA, HF PA
                chip.write(0, code)
                z += list(chip.run(0.06))
            outs.append(z)
        same = outs[0] == outs[1] == outs[2]
        report("inactive: mode %d rate %d" % (mode, rate), [] if same else ["the settings render differently"],
               "identical under slew / immediate / pa_wait")


def main():
    print("mode1_rate15_formants = %r%s" % (POLICY, " (CONTROL)" if BREAK else ""))
    for pitch, ff in NOTES:
        for phase in (0, 11, 22):
            judge_note("C", pitch, ff, phase)
    py = judge_note("py", 0xD1, 0xF8, 5)
    c = judge_note("C", 0xD1, 0xF8, 5)
    check_parity(py, c)
    check_chunks()
    check_skip("C")
    check_skip("py")
    if not BREAK:
        check_inactive()
    print("BASIC tones: %s" % ("%d FAILED" % len(fails) if fails else "all ok"))
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main()
