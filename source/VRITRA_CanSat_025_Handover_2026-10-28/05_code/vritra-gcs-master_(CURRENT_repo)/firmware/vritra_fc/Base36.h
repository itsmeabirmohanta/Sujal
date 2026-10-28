/* =============================================================================
 * Base36.h — VRITRA wire codec, shared by the flight computer and the gateway
 * =============================================================================
 * THIS FILE MUST BE BYTE-IDENTICAL IN vritra_fc/ AND vritra_gs/.
 * Arduino only compiles sources inside a sketch folder, so it exists twice.
 * Both sketches print WIRE_SCHEMA_SUM at boot — if the two printed values are
 * not the same number, the copies have drifted and every decoded field is
 * silently wrong. Check that first, before anything else, after any edit here.
 *
 * WHAT THIS ENCODES
 *   Every numeric telemetry field is transmitted as a base-36 integer of the
 *   value multiplied by a fixed scale factor. Base 36 uses 0-9 then A-Z, so it
 *   packs 5.17 bits per character against decimal's 3.32 — about 36 % fewer
 *   characters for the same number, and the result is still plain ASCII in
 *   comma-separated fields, which is what guideline §6.3.i requires.
 *
 * ROUND-TRIP EXACTNESS — the property everything depends on
 *   encode:  raw = llround(value * div)
 *   decode:  text = printf("%.*f", dp, raw / div)
 *   This is exact, with no accumulated drift, precisely because dp is defined
 *   as log10(div) for every entry in the tables below. div=10 -> dp=1,
 *   div=100 -> dp=2, div=1000 -> dp=3, div=1e6 -> dp=6, div=1 -> dp=0.
 *   NEVER add a table row where that relationship does not hold: the decoder
 *   would then print more or fewer digits than the encoder preserved, and the
 *   error would look like sensor noise rather than a codec bug.
 *
 * WHY llround AND double, NOT lroundf AND float
 *   Latitude at 1e-6 degrees is up to 180 000 000. A 32-bit float has a 24-bit
 *   mantissa and stops representing consecutive integers above 16 777 216, so
 *   lroundf(lon * 1e6f) quantises longitude into ~10 m steps near 83 deg — a
 *   silent, plausible-looking position error. All scaling here is done in
 *   double and rounded with llround.
 *
 * SIGN, ZERO, AND EMPTY
 *   Negative values carry a leading '-'. Zero encodes as "0", never as "".
 *   An EMPTY field means "no valid measurement" and decodes to an empty CSV
 *   cell, which vritra_gcs.py renders as an em-dash. That distinction is
 *   deliberate: "the sensor reported zero" must stay separable from "there was
 *   no fix". It is used for the GNSS fields before first lock — encoding a
 *   no-fix position as 0,0 would plot the CanSat in the Gulf of Guinea.
 * ========================================================================== */
#pragma once
#include <Arduino.h>

/* int32 max is 2147483647 = "ZIK0ZJ" in base 36: 6 characters, plus a possible
 * sign, plus the terminator. */
#define B36_MAX_CHARS 9

/* --------------------------------------------------------------------------
 * ENCODE — int32 -> uppercase base-36 text. Returns the character count
 * written, or 0 if the buffer was too small (in which case out is emptied).
 * ------------------------------------------------------------------------ */
static inline int b36enc(char* out, size_t cap, int32_t v) {
    if (!out || cap < 2) { if (out && cap) out[0] = '\0'; return 0; }
    char tmp[12];
    int n = 0;
    const bool neg = (v < 0);
    /* Magnitude via int64: negating INT32_MIN in 32-bit is undefined. */
    uint32_t u = neg ? (uint32_t)(-(int64_t)v) : (uint32_t)v;
    if (u == 0) tmp[n++] = '0';
    while (u) {
        uint32_t r = u % 36u;
        tmp[n++] = (r < 10) ? (char)('0' + r) : (char)('A' + (r - 10));
        u /= 36u;
    }
    const int need = n + (neg ? 1 : 0);
    if ((size_t)need + 1 > cap) { out[0] = '\0'; return 0; }
    int i = 0;
    if (neg) out[i++] = '-';
    while (n) out[i++] = tmp[--n];
    out[i] = '\0';
    return i;
}

/* --------------------------------------------------------------------------
 * DECODE — base-36 text -> int32. Strict: any character outside 0-9/A-Z/a-z,
 * an empty token, a lone '-', an over-long token or an out-of-range magnitude
 * rejects the WHOLE token and returns false. A corrupted field must never
 * decode to a plausible number — silently wrong telemetry is worse than an
 * obviously missing cell.
 * ------------------------------------------------------------------------ */
static inline bool b36dec(const char* s, int32_t& out) {
    if (!s || !*s) return false;
    const char* p = s;
    bool neg = false;
    if (*p == '-') { neg = true; p++; }
    if (!*p) return false;                       /* a lone '-' is not a number */

    uint64_t acc = 0;
    int digits = 0;
    for (; *p; p++) {
        const char c = *p;
        uint32_t d;
        if      (c >= '0' && c <= '9') d = (uint32_t)(c - '0');
        else if (c >= 'A' && c <= 'Z') d = (uint32_t)(c - 'A' + 10);
        else if (c >= 'a' && c <= 'z') d = (uint32_t)(c - 'a' + 10);
        else return false;
        if (++digits > 7) return false;          /* longer than any int32 */
        acc = acc * 36u + d;
        if (acc > 2147483648ULL) return false;   /* early overflow bail */
    }
    const int64_t v = neg ? -(int64_t)acc : (int64_t)acc;
    if (v < INT32_MIN || v > INT32_MAX) return false;
    out = (int32_t)v;
    return true;
}

/* --------------------------------------------------------------------------
 * SCALE TABLE
 * dp MUST equal log10(div). See the round-trip note in the file header.
 * ------------------------------------------------------------------------ */
struct B36Scale { int32_t div; uint8_t dp; };

/* Transmitted core, in wire order after the team id.
 * Indices 0..15 are guideline §6.3.i's mandated fields in the mandated order
 * and map 1:1 onto vritra_gcs.py SCHEMA positions 0..15. Index 16 is the
 * status FLAGS byte, which is team-appended optional data (§6.3.iii.B).
 *
 * Every resolution here meets or beats the §6.3.iii table:
 *   ALTITUDE 0.1 m · PRESSURE 1 Pa · TEMP 0.1 C · VOLTAGE 0.01 V
 *   GNSS LAT/LON 0.0001 deg required, 0.000001 deg sent
 *   GNSS ALTITUDE 0.1 m · GNSS SATS integer · ACCEL m/s^2 · SPIN deg/s     */
#define B36_CORE_N 17
static const B36Scale B36_CORE[B36_CORE_N] = {
    {       1, 0 },   /*  0 TIME_S     s, integer, >= 1 s resolution §6.1.viii */
    {       1, 0 },   /*  1 PKT                                                */
    {      10, 1 },   /*  2 ALT_M      0.1 m                                   */
    {       1, 0 },   /*  3 PRES_PA    1 Pa                                    */
    {      10, 1 },   /*  4 TEMP_C     0.1 C                                   */
    {     100, 2 },   /*  5 BUS_V      0.01 V                                  */
    {       1, 0 },   /*  6 GNSS_TIME  hhmmss as an integer                    */
    { 1000000, 6 },   /*  7 LAT        1e-6 deg  (max 90e6, fits int32)        */
    { 1000000, 6 },   /*  8 LON        1e-6 deg  (max 180e6, fits int32)       */
    {      10, 1 },   /*  9 GPS_ALT_M  0.1 m                                   */
    {       1, 0 },   /* 10 SATS                                               */
    {     100, 2 },   /* 11 ACC_X      0.01 m/s^2 (below the IMU's own LSB)    */
    {     100, 2 },   /* 12 ACC_Y                                              */
    {     100, 2 },   /* 13 ACC_Z                                              */
    {      10, 1 },   /* 14 SPIN_RATE  0.1 deg/s                               */
    {       1, 0 },   /* 15 STATE      0..4                                    */
    {       1, 0 },   /* 16 FLAGS      bit field, not a schema field           */
};

/* Rotating optional value, one per packet, selected by the F<n> tag.
 * B36_OPT_MAP gives the vritra_gcs.py SCHEMA index each frame carries. */
#define B36_OPT_N 16
/* THE INDEX IN EACH COMMENT IS A vritra_gcs.py SCHEMA POSITION.
 *
 * IF YOU ADD A FIELD TO vritra_gcs.py SCHEMA, ADD IT AT THE END. Inserting one
 * in the middle silently re-points every entry below it here, in B36_OPT_MAP,
 * and in the gateway's hardcoded rec[] writes. Nothing catches that at
 * runtime: the schema sum guards the SCALES, not the destinations.
 *
 * When checking these by eye, note that SCHEMA entries are written
 * ("NAME", _f("unit", ...)) — the unit is also a quoted string, so a naive
 * grep for quoted names counts each entry twice and every index after the
 * first unit looks shifted. Count only the key before `_f(`.
 *
 * ---------------------------------------------------------------------------
 * WAS 19 FRAMES. AX / AY / AZ WERE REMOVED BECAUSE THEY WERE DUPLICATES.
 * ---------------------------------------------------------------------------
 * Frames F2/F3/F4 used to carry AX/AY/AZ (schema 20/21/22) in g. But the
 * mandated core already carries ACC_X/ACC_Y/ACC_Z (schema 11/12/13) in every
 * single packet, and buildPacket() fills those from the SAME three variables:
 *     core   ACC_X = d.ax * 9.80665   (m/s^2, §6.3.iii row 13)
 *     opt F2 AX    = d.ax             (g)
 * Identical measurement, different unit. Three of nineteen rotation slots were
 * being spent re-sending, once every 19 packets, a number that was already in
 * front of the receiver 19 times out of 19.
 *
 * The gateway now DERIVES AX/AY/AZ by dividing the core fields by 9.80665, so
 * schema 20/21/22 populate on EVERY packet instead of once per rotation. Two
 * things fall out of that:
 *   - the ground station's INERTIAL panel shows ACCEL X/Y/Z live rather than
 *     after a 19-packet wait (38 s at the 0.5 Hz pad cadence), which is why
 *     those tiles read em-dash on a freshly-started link;
 *   - the rotation drops 19 -> 16, so GX/GY/GZ and MX/MY/MZ — which are NOT
 *     derivable and genuinely have to be transmitted — now come round 16%
 *     sooner.
 * Zero extra airtime: nothing was added to the packet, three redundant things
 * were removed from the rotation. */
static const B36Scale B36_OPT[B36_OPT_N] = {
    {    1, 0 },   /* F0  17 SOC_PCT        %                                */
    {   10, 1 },   /* F1  18 GND_SPEED      0.1 m/s                          */
    {   10, 1 },   /* F2  20 GX             0.1 deg/s                        */
    {   10, 1 },   /* F3  21 GY                                              */
    {   10, 1 },   /* F4  22 GZ                                              */
    {   10, 1 },   /* F5  23 MX             0.1 uT (AK09916 range +-4912 uT) */
    {   10, 1 },   /* F6  24 MY                                              */
    {   10, 1 },   /* F7  25 MZ                                              */
    {    1, 0 },   /* F8  69 SD_PCT         %                                */
    {    1, 0 },   /* F9  74 NAVIC_SATS                                      */
    {  100, 2 },   /* F10 76 HDOP                                            */
    {  100, 2 },   /* F11 77 VDOP                                            */
    {    1, 0 },   /* F12 78 FIX_QUALITY                                     */
    {   10, 1 },   /* F13 79 GEOID_SEP      0.1 m                            */
    {    1, 0 },   /* F14 80 GNSS_SYS       constellation bitmask            */
    {    1, 0 },   /* F15 75 UTC            seconds since midnight, 0..86399;
                            decoded back to HH:MM:SS by the gateway          */
};
/* Destinations shifted down by 3 above index 22 when AX/AY/AZ left SCHEMA.
 * These numbers are checked against vritra_gcs.py's SCHEMA BY NAME, using the
 * "F<n> <index> <NAME>" comments above as the assertion — run the verifier
 * after touching either file rather than trusting a visual scan. */
static const uint8_t B36_OPT_MAP[B36_OPT_N] = {
    17, 18, 20, 21, 22, 23, 24, 25,
    69, 74, 76, 77, 78, 79, 80, 75
};
/* The one frame whose value is a clock, not a scalar. */
#define B36_OPT_UTC_FRAME 15

/* WIRE LAYOUT VERSION — bump this on ANY change to field ORDER or field COUNT.
 *
 * b36SchemaSum() used to fold only the SCALE tables and the frame map, and the
 * comment above B36_OPT says as much: "the schema sum guards the SCALES, not
 * the destinations." That left a real hole. Two builds could agree on every
 * divisor and every map entry while disagreeing about how many fields are on
 * the wire and in what order — and the boot-time sums would MATCH, so the one
 * check that exists to catch drift would stay silent while every field after
 * the changed position decoded into the wrong slot.
 *
 * Layout 1 -> 2: the ",F<n>" rotation tag was removed. It was pure redundancy —
 * the flight computer derives the frame index from the packet counter as
 * frame = (PKT - 1) % B36_OPT_N, and PKT is already in every packet at wire
 * position 2, so the tag re-sent 3-4 bytes of something the receiver could
 * always have computed. The rotating VALUE now sits at wire position 24 and a
 * full packet carries 25 fields instead of 26.
 *
 * Layout 2 -> 3: ATT_YAW AND QUAT_X/Y/Z LEFT THE WIRE. Together they were 12 B
 * of an 86 B packet — 14 % of the airtime budget — carrying attitude the ground
 * station displayed nowhere. No panel ever read ATT_YAW, and the quaternion was
 * explicitly discarded on arrival by vritra_gcs.py's _update_instruments(),
 * which calls set_quaternion(None) because the transmitted quaternion is
 * poisoned by the left-handed board frame. The 3-D model, the ADI and the
 * compass all run off ROLL/PITCH/YAW that the ground station DERIVES from
 * ACC_X/Y/Z + GX/GY/GZ + MX/MY/MZ, so nothing on screen changed.
 *
 * ATT_ROLL and ATT_PITCH STAYED, and the reason is compliance rather than use:
 * guideline table row 13 defines <ACCELEROMETER DATA> as "acceleration and
 * roll & pitch parameters", which makes roll and pitch mandated content for
 * the graded CSV. Yaw and quaternions are named nowhere in that table and fall
 * under row 16 <OPTIONAL DATA>. Both are still logged on the vehicle's own SD
 * card at 20 Hz, which is a better record than the 1 Hz copy the link carried.
 * A full packet now carries 21 fields and the rotating value sits at wire
 * position 20.
 *
 * Folding this constant into the sum means a flight computer on layout 3 and a
 * gateway still on layout 2 print DIFFERENT schema sums at boot and are caught
 * on the bench, instead of producing a silently misparsed downlink in flight. */
#define B36_WIRE_LAYOUT 3

/* ATTITUDE EULER SCALE — 0.1 degree, its own named constant.
 * This used to be written as B36_OPT[5] in BOTH the flight computer and the
 * gateway, borrowing the GX row because it happened to have the same divisor.
 * That is a landmine: the frame table above is a ROTATION SCHEDULE, and any
 * reordering of it silently re-scales the attitude fields at both ends. It has
 * just been reordered — GX moved from index 5 to index 2 — so the borrowed
 * reference would now be pointing at MX. Same numbers today, wrong by
 * construction tomorrow. Named separately so the coupling cannot come back. */
static const B36Scale B36_ATT = { 10, 1 };

/* THE QUATERNION IS NO LONGER TRANSMITTED — B36_QUAT WAS REMOVED AT LAYOUT 3.
 *
 * It rode the wire as X/Y/Z (W reconstructed as sqrt(max(0,1-x^2-y^2-z^2))) to
 * feed the ground station's 3-D model, on the argument that Euler angles have a
 * hard +-90 deg pitch ceiling and cannot show a vehicle tumbling past vertical.
 * That argument was sound; the implementation never delivered on it. The ground
 * station discarded every quaternion it received — _update_instruments() calls
 * set_quaternion(None) because the transmitted values are poisoned by the
 * left-handed physical board frame — and rendered the model from its own
 * derived Euler angles instead. Nine bytes of every packet, every second, for a
 * value that was thrown away on arrival.
 *
 * Sensors.cpp still RUNS quatUpdate() and DataLogger still writes qx/qy/qz to
 * the SD CSV at 20 Hz, where the bytes are free. If the gimbal-lock problem is
 * ever worth solving properly, the fix is to correct the frame handedness first
 * and prove the model renders from it — then put it back on the wire, not
 * before. */

/* --------------------------------------------------------------------------
 * DRIFT GUARD. Folds both tables plus the map into one number. Printed at boot
 * by the flight computer and by the gateway; the two MUST match.
 * ------------------------------------------------------------------------ */
static inline uint16_t b36SchemaSum() {
    uint32_t h = 2166136261u;                    /* FNV-1a */
    for (int i = 0; i < B36_CORE_N; i++) {
        h = (h ^ (uint32_t)B36_CORE[i].div) * 16777619u;
        h = (h ^ (uint32_t)B36_CORE[i].dp)  * 16777619u;
    }
    for (int i = 0; i < B36_OPT_N; i++) {
        h = (h ^ (uint32_t)B36_OPT[i].div)   * 16777619u;
        h = (h ^ (uint32_t)B36_OPT[i].dp)    * 16777619u;
        h = (h ^ (uint32_t)B36_OPT_MAP[i])   * 16777619u;
    }
    h = (h ^ (uint32_t)B36_ATT.div)  * 16777619u;
    h = (h ^ (uint32_t)B36_ATT.dp)   * 16777619u;
    /* Frame COUNT is folded in explicitly. Without it, two builds that agree on
     * every scale but disagree on how many frames the rotation has would
     * produce the same sum — and the F<n> tag would be interpreted against the
     * wrong table on one side. */
    h = (h ^ (uint32_t)B36_OPT_N)    * 16777619u;
    /* Field ORDER and field COUNT, which the scale tables cannot express.
     * See the B36_WIRE_LAYOUT block for what this closes. */
    h = (h ^ (uint32_t)B36_WIRE_LAYOUT) * 16777619u;
    return (uint16_t)((h >> 16) ^ (h & 0xFFFFu));
}

/* --------------------------------------------------------------------------
 * ENCODE ONE FIELD — appends ",<base36>" (or just "," when absent).
 * Scaling is done in double; see the header note on why float is not safe for
 * latitude and longitude. Returns the new write position.
 * ------------------------------------------------------------------------ */
static inline int b36Append(char* buf, int pos, int cap,
                            double value, const B36Scale& sc, bool present) {
    if (pos + 1 >= cap) return pos;
    buf[pos++] = ',';
    buf[pos]   = '\0';
    if (!present) return pos;

    const double scaled = value * (double)sc.div;
    /* Clamp before the cast: an out-of-range sensor reading must not wrap
     * around into a small, believable number. */
    int32_t raw;
    if      (scaled >=  2147483647.0) raw =  2147483647;
    else if (scaled <= -2147483648.0) raw = -2147483647;
    else                              raw = (int32_t)llround(scaled);

    char t[B36_MAX_CHARS];
    const int n = b36enc(t, sizeof(t), raw);
    if (n <= 0 || pos + n >= cap) return pos;
    memcpy(buf + pos, t, (size_t)n);
    pos += n;
    buf[pos] = '\0';
    return pos;
}

/* --------------------------------------------------------------------------
 * DECODE ONE FIELD — base-36 token -> plain decimal text for the GCS.
 * An empty or malformed token yields an empty string, which the GCS renders
 * as an em-dash rather than inventing a value.
 * ------------------------------------------------------------------------ */
static inline void b36Field(const char* tok, const B36Scale& sc,
                            char* out, size_t cap) {
    if (!out || !cap) return;
    out[0] = '\0';
    int32_t raw;
    if (!b36dec(tok, raw)) return;               /* absent or corrupt */
    if (sc.div == 1) snprintf(out, cap, "%ld", (long)raw);
    else             snprintf(out, cap, "%.*f", (int)sc.dp,
                              (double)raw / (double)sc.div);
}
