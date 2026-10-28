"""VRITRA cross-file consistency verifier + base-36 codec test.

    python verify_schema.py

Run after ANY change to vritra_gcs.py's SCHEMA, Base36.h, vritra_gs.ino or
DataLogger.cpp.

There is no C++ toolchain on this machine, so a wire-format mistake cannot be
caught by a compiler. This is the substitute. It reads SCHEMA out of
vritra_gcs.py and checks every hardcoded index in the firmware against it BY
NAME -- N_FIELDS, GCS_ATT_MAP, B36_OPT_MAP, every literal rec[]
write in the gateway, and the CSV mirror header in DataLogger.cpp.

That matters because the GCS parser is POSITIONAL: it maps columns by
enumerate(ALL_FIELDS) with no name lookup and no error on a short row. A stale
index does not crash anything -- it decodes one field into another field's slot
and the value looks plausible on screen. This script is what turns that into a
loud failure on the bench.

It also exercises the base-36 codec itself: round-trip exactness across the
int32 boundaries, every rejection path, the dp == log10(div) invariant that
round-trip exactness depends on, and full-scale range safety for every
transmitted field.

Derives every schema index BY NAME from vritra_gcs.py's SCHEMA, then checks
every hardcoded index in Base36.h, vritra_gs.ino and DataLogger.cpp against it.
"""
import re, sys, os, random

ROOT = os.path.dirname(os.path.abspath(__file__))
os.chdir(ROOT)
fails, warns = [], []


def rd(p):
    return open(p, encoding='utf-8', errors='replace').read()


def bad(m):
    fails.append(m)


def warn(m):
    warns.append(m)


# ---------------------------------------------------------------- SCHEMA
gcs = rd('vritra_gcs.py')
m = re.search(r'^SCHEMA\s*=\s*OrderedDict\(\[(.*?)^\]\)', gcs, re.S | re.M)
if not m:
    print('FATAL: SCHEMA block not found'); sys.exit(1)
names = re.findall(r'^\s*\("([A-Z0-9_]+)",\s*_f\(', m.group(1), re.M)
IDX = {n: i for i, n in enumerate(names)}
N = len(names)
print('SCHEMA: %d fields, first=%s last=%s' % (N, names[0], names[-1]))
if len(set(names)) != N:
    bad('duplicate names in SCHEMA')

# mandated core order (guideline 6.3.i)
MANDATED = ["TIME_S", "PKT", "ALT_M", "PRES_PA", "TEMP_C", "BUS_V", "GNSS_TIME",
            "LAT", "LON", "GPS_ALT_M", "SATS", "ACC_X", "ACC_Y", "ACC_Z",
            "SPIN_RATE", "STATE"]
if names[:16] != MANDATED:
    bad('mandated core 0..15 order changed: %s' % names[:16])
else:
    print('mandated core 0..15 : OK')

# ---------------------------------------------------------------- Base36.h
b36 = rd('vritra_fc/Base36.h')
if rd('vritra_gs/Base36.h') != b36:
    bad('Base36.h NOT byte-identical between vritra_fc and vritra_gs')
else:
    print('Base36.h byte-identical : OK')

opt_n = int(re.search(r'#define\s+B36_OPT_N\s+(\d+)', b36).group(1))
mm = re.search(r'B36_OPT_MAP\[B36_OPT_N\]\s*=\s*\{(.*?)\}', b36, re.S)
omap = [int(x) for x in re.findall(r'\d+', mm.group(1))]
if len(omap) != opt_n:
    bad('B36_OPT_MAP has %d entries, B36_OPT_N=%d' % (len(omap), opt_n))

# the comment on each B36_OPT row names the schema field; check value vs name
rows = re.findall(r'/\*\s*F(\d+)\s+(\d+)\s+([A-Z0-9_]+)', b36)
seen = {}
for f_, i_, nm in rows:
    f_, i_ = int(f_), int(i_)
    seen[f_] = (i_, nm)
    if nm not in IDX:
        bad('B36_OPT F%d comment names %s which is not in SCHEMA' % (f_, nm))
    elif IDX[nm] != i_:
        bad('B36_OPT F%d comment says index %d for %s, SCHEMA says %d'
            % (f_, i_, nm, IDX[nm]))
    elif f_ < len(omap) and omap[f_] != i_:
        bad('B36_OPT_MAP[%d]=%d but comment/SCHEMA say %d (%s)'
            % (f_, omap[f_], i_, nm))
if len(seen) == opt_n and not [x for x in fails if 'B36_OPT' in x]:
    print('B36_OPT_MAP vs SCHEMA (%d frames) : OK' % opt_n)

for f_ in range(opt_n):
    if f_ not in seen:
        warn('B36_OPT F%d has no "F<n> <idx> <NAME>" comment to check' % f_)

# every optional destination must be a real, non-core schema slot
for f_, dst in enumerate(omap):
    if dst >= N:
        bad('B36_OPT_MAP[%d]=%d is past the end of SCHEMA (%d)' % (f_, dst, N))
    elif dst < 16:
        bad('B36_OPT_MAP[%d]=%d targets a MANDATED core slot' % (f_, dst))

# ---------------------------------------------------------------- gateway
gs = rd('vritra_gs/vritra_gs.ino')
mn = re.search(r'#define\s+N_FIELDS\s+(\d+)', gs)
if mn:
    if int(mn.group(1)) != N:
        bad('gateway N_FIELDS=%s but SCHEMA has %d' % (mn.group(1), N))
    else:
        print('gateway N_FIELDS=%d : OK' % N)
else:
    warn('gateway N_FIELDS define not found')


def arr(pat):
    a = re.search(pat + r'\s*=\s*\{([^}]*)\}', gs)
    return [int(x) for x in re.findall(r'\d+', a.group(1))] if a else None


for pat, want in ((r'GCS_ATT_MAP\[2\]', ["ATT_ROLL", "ATT_PITCH"]),):
    got = arr(pat)
    exp = [IDX[w] for w in want]
    if got is None:
        warn('%s not found' % pat)
    elif got != exp:
        bad('%s = %s but SCHEMA says %s for %s' % (pat, got, exp, want))
    else:
        print('%-18s = %s : OK' % (pat.split('[')[0], got))

# ATT_YAW and QUAT_X/Y/Z left the wire at layout 3. If any of them reappears in
# SCHEMA without the firmware being updated to send it, the GCS silently reads
# an always-empty column and nobody notices until the flight data is examined.
for gone in ("ATT_YAW", "QUAT_X", "QUAT_Y", "QUAT_Z"):
    if gone in IDX:
        bad('%s is back in SCHEMA but nothing transmits it — wire layout 3 '
            'removed it from buildPacket() and from the gateway' % gone)
if 'GCS_QUAT_MAP' in gs:
    bad('gateway still has GCS_QUAT_MAP — the quaternion left the wire at '
        'layout 3')
if re.search(r'^\s*static const B36Scale\s+B36_QUAT', b36, re.M):
    bad('Base36.h still defines B36_QUAT — the quaternion left the wire at '
        'layout 3')
print('retired wire fields (ATT_YAW, QUAT_X/Y/Z) stay retired : OK')

ma = re.search(r'#define\s+GCS_ATT_ROLL\s+(\d+)', gs)
if ma and int(ma.group(1)) != IDX['ATT_ROLL']:
    bad('GCS_ATT_ROLL=%s but ATT_ROLL is %d' % (ma.group(1), IDX['ATT_ROLL']))

# Every LITERAL rec[<n>] in the gateway must correspond, by name, to exactly
# the set of fields the gateway is supposed to touch directly. Set equality —
# so a stale index left behind by a schema edit fails loudly instead of
# silently writing into whatever field now occupies that slot.
REC_NAMES = ['PKT', 'GNSS_TIME', 'RSSI', 'SNR', 'FREQ_MHZ',
             'MCU_ACTIVE', 'WDOG_OK', 'BEACON_ON', 'UTC']
want = {IDX[n]: n for n in REC_NAMES}
got = set(int(x) for x in re.findall(r'rec\[(\d+)\]', gs))
for lit in sorted(got):
    if lit >= N:
        bad('gateway rec[%d] is past end of SCHEMA (%d)' % (lit, N))
missing = set(want) - got
extra = got - set(want)
if missing:
    bad('gateway never writes/reads rec[] for: %s'
        % ', '.join('%s(%d)' % (want[i], i) for i in sorted(missing)))
if extra:
    bad('gateway uses unexpected literal rec[%s] — stale index after a '
        'schema edit?' % ', '.join(str(i) for i in sorted(extra)))
if not missing and not extra:
    print('gateway literal rec[] set matches SCHEMA by name : OK (%d)' % len(want))
    for i in sorted(want):
        print('    rec[%-2d] = %s' % (i, want[i]))

# ---------------------------------------------------------------- CSV mirror
dl = rd('vritra_fc/DataLogger.cpp')
# Strip /* */ comments FIRST. The header is located by a non-greedy match up to
# the first ';', so one semicolon inside a comment sitting between two of the
# concatenated string literals truncates it — and the failure is silent in the
# worst way: the build is correct, this check just quietly stops covering the
# columns after the comment.
dl_nc = re.sub(r'/\*.*?\*/', '', dl, flags=re.S)
mh = re.search(r'CSV_HEADER\[\]\s*=(.*?);', dl_nc, re.S)
hdr = ''.join(re.findall(r'"([^"]*)"', mh.group(1)))
cols = [c for c in hdr.split(',') if c]
if cols[:N] != names:
    for i in range(min(len(cols), N)):
        if cols[i] != names[i]:
            bad('CSV_HEADER col %d = %s, SCHEMA = %s' % (i, cols[i], names[i]))
            break
    else:
        bad('CSV_HEADER has %d schema cols, SCHEMA has %d' % (len(cols), N))
else:
    print('CSV_HEADER cols 0..%d match SCHEMA : OK (+%d diagnostics = %d)'
          % (N - 1, len(cols) - N, len(cols)))

# ---------------------------------------------------------------- base-36
DIG = '0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ'
INT32_MIN, INT32_MAX = -2147483648, 2147483647


def enc(v):
    neg, u = v < 0, abs(v)
    s = ''
    if u == 0:
        s = '0'
    while u:
        s = DIG[u % 36] + s
        u //= 36
    return ('-' if neg else '') + s


def dec(s):
    """Faithful port of b36dec, including its reject rules."""
    if not s:
        return None
    p, neg = s, False
    if p.startswith('-'):
        neg, p = True, p[1:]
    if not p:
        return None
    acc, digits = 0, 0
    for c in p:
        if c.isdigit():
            d = ord(c) - 48
        elif 'A' <= c <= 'Z':
            d = ord(c) - 55
        elif 'a' <= c <= 'z':
            d = ord(c) - 87
        else:
            return None
        digits += 1
        if digits > 7:
            return None
        acc = acc * 36 + d
        if acc > 2147483648:
            return None
    v = -acc if neg else acc
    return v if INT32_MIN <= v <= INT32_MAX else None


print()
print('--- base-36 codec ---')
# 1. exhaustive round-trip over boundaries + fuzz
edges = [0, 1, -1, 35, 36, 1295, 1296, INT32_MAX, INT32_MIN, INT32_MIN + 1,
         2147483646, -2147483647]
vals = edges + [random.randint(INT32_MIN, INT32_MAX) for _ in range(200000)]
rt = 0
for v in vals:
    if dec(enc(v)) != v:
        bad('round-trip FAILED for %d -> %s -> %s' % (v, enc(v), dec(enc(v))))
        break
    rt += 1
else:
    print('round-trip exact over %d values incl. int32 boundaries : OK' % rt)

# 2. every rejection path
REJECT = {'empty': '', 'lone minus': '-', 'double minus': '--1',
          'punctuation': '1,2', 'space': '1 2', 'plus': '+1',
          'out of range +': 'ZIK0ZK', '8 digits': '11111111',
          'overflow acc': 'ZZZZZZZ', 'null-ish': None}
for why, tok in REJECT.items():
    if tok is None:
        continue
    if dec(tok) is not None:
        bad('b36dec ACCEPTED bad token (%s): %r -> %s' % (why, tok, dec(tok)))
print('rejects empty/lone-minus/punctuation/overflow/over-long : OK')

# INT32_MIN must survive: encoder path uses int64 negate
if dec(enc(INT32_MIN)) != INT32_MIN:
    bad('INT32_MIN does not round-trip')
# lowercase accepted (decoder is case-insensitive)
if dec('zik0zj') != INT32_MAX:
    bad('lowercase not accepted')
print('INT32_MIN round-trip + lowercase accept : OK')

# 3. capacity / width analysis
print()
print('--- base-36 capacity ---')
print('bits/char: log2(36)=5.170 vs decimal 3.322  -> %.1f%% fewer chars'
      % (100 * (1 - 3.3219 / 5.1699)))
print('%-12s %-9s %-9s %s' % ('MAGNITUDE', 'B36', 'DEC', 'SAVED'))
for v in (10 ** 2, 10 ** 4, 10 ** 6, 10 ** 8, INT32_MAX):
    b, d = len(enc(v)), len(str(v))
    print('%-12d %-9d %-9d %+d' % (v, b, d, b - d))
mx = max(len(enc(v)) for v in (INT32_MAX, INT32_MIN))
mc = re.search(r'#define\s+B36_MAX_CHARS\s+(\d+)', b36)
need = mx + 1
print('widest token %d chars ("%s"), B36_MAX_CHARS=%s, need >=%d incl NUL'
      % (mx, enc(INT32_MIN), mc.group(1), need))
if int(mc.group(1)) < need:
    bad('B36_MAX_CHARS=%s too small, need %d' % (mc.group(1), need))

# 4. scale-table invariant: dp == log10(div) for EVERY row
print()
print('--- scale tables: dp must equal log10(div) ---')
tbl = 0
for tname in ('B36_CORE', 'B36_OPT'):
    blk = re.search(tname + r'\[[A-Z0-9_]+\]\s*=\s*\{(.*?)\n\};', b36, re.S)
    for div, dp in re.findall(r'\{\s*(-?\d+)\s*,\s*(\d+)\s*\}', blk.group(1)):
        div, dp = int(div), int(dp)
        tbl += 1
        if 10 ** dp != div:
            bad('%s row div=%d dp=%d violates dp==log10(div)' % (tname, div, dp))
for nm in ('B36_ATT',):        # B36_QUAT was removed at wire layout 3
    s = re.search(nm + r'\s*=\s*\{\s*(\d+)\s*,\s*(\d+)\s*\}', b36)
    div, dp = int(s.group(1)), int(s.group(2))
    tbl += 1
    if 10 ** dp != div:
        bad('%s div=%d dp=%d violates dp==log10(div)' % (nm, div, dp))
print('%d scale rows checked' % tbl)

# 5. no transmitted field can overflow int32 at its scale
print()
print('--- range safety: value*div must fit int32 ---')
RANGES = {'ALT_M': 20000, 'PRES_PA': 110000, 'TEMP_C': 125, 'BUS_V': 20,
          'GNSS_TIME': 235959, 'LAT': 90, 'LON': 180, 'GPS_ALT_M': 20000,
          'ACC_X': 16 * 9.81, 'ACC_Y': 16 * 9.81, 'ACC_Z': 16 * 9.81,
          'SPIN_RATE': 2000}
core_blk = re.search(r'B36_CORE\[B36_CORE_N\]\s*=\s*\{(.*?)\n\};', b36, re.S)
core_rows = [(int(a), int(b)) for a, b in
             re.findall(r'\{\s*(-?\d+)\s*,\s*(\d+)\s*\}', core_blk.group(1))]
for nm, hi in RANGES.items():
    div = core_rows[IDX[nm]][0]
    if hi * div > INT32_MAX:
        bad('%s full-scale %g * div %d = %g overflows int32' % (nm, hi, div, hi * div))
print('%d transmitted core fields within int32 at full scale' % len(RANGES))

# ---------------------------------------------------------------- report
print()
print('=' * 62)
for w in warns:
    print('WARN  ' + w)
if fails:
    for f_ in fails:
        print('FAIL  ' + f_)
    print('%d FAILURE(S)' % len(fails))
    sys.exit(1)
print('ALL CHECKS PASSED  (%d warnings)' % len(warns))
