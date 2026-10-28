/* =============================================================================
 * VRITRA FLIGHT COMPUTER  ·  2026-INSPACe-CAN-7USAT-025
 * Teensy 4.1 @ 600 MHz  ·  firmware 6.1.0
 * =============================================================================
 * MODULE MAP
 *   Config.h      pins, RF plan, wire format, every tunable
 *   Sensors.*     BMP585, ICM-20948, AK09916, DS3231, L89 GNSS, bus sense
 *   Radio.*       LR1121 half-duplex transceiver state machine
 *   DataLogger.*  SdFat/SDIO block-aligned flight recorder
 *   Motor.*       TB6612FNG gyro wheel with soft start
 *   this file     mission logic: flight FSM, packet assembly, commands
 *
 * COMPETITION COMPLIANCE — the clauses this file is directly responsible for:
 *   §6.3.i   telemetry transmitted as ASCII comma-separated fields + CR, in the
 *            mandated field order, at >= 1 Hz            -> buildPacket()
 *   §6.1.v   shall NOT transmit until commanded          -> TX_ENABLED_AT_BOOT
 *   §6.1.ix  mission clock AND system state maintained
 *            across a processor reset                    -> Persist / missionS()
 *   §6.1.vi  ground station can command gyro / baro /
 *            accelerometer calibration to zero           -> CMD_CALIBRATE
 *   §6.2.v   telemetry on/off by RF command              -> CMD_TX_ON/OFF
 *   §5.1.xi  record to SD against telemetry loss         -> DataLogger
 *   §5.4.vi  secondary descent control at 600 m (+/-10)  -> FSM_SEC_EJECT_AGL_M
 *
 * BLOCKING POLICY
 *   delay() appears nowhere in this project. setup() arms non-blocking init
 *   state machines and returns; loop() is a flat pump that never waits on any
 *   subsystem. The only busy-wait anywhere is 90 us of I2C bus-recovery clock
 *   edges in Sensors.cpp, which cannot be expressed any other way.
 *   A delay() in setup desynchronises the LR1121's BUSY handshake and its DIO9
 *   interrupt; a delay() in loop drops NMEA bytes out of the UART FIFO and
 *   stalls the 25 Hz barometer cadence.
 *
 * SUBSYSTEM INDEPENDENCE
 *   No subsystem can stall another. A missing SD card, an absent barometer, a
 *   wedged I2C bus, a GNSS with no fix and a radio that will not initialise all
 *   degrade to their own failure mode and everything else keeps running.
 * ========================================================================== */
#include <Arduino.h>
#include <EEPROM.h>
#include "Config.h"
#include "Base36.h"
#include "Sensors.h"
#include "Radio.h"
#include "DataLogger.h"
#include "Motor.h"
#include "Beacon.h"

#if __has_include(<Watchdog_t4.h>)
  #include <Watchdog_t4.h>
  static WDT_T4<WDT1> wdt;
  #define HAVE_WDT 1
#else
  #define HAVE_WDT 0
#endif

/* ===========================================================================
 * PERSISTENT STATE   (guideline §6.1.ix)
 * ========================================================================
 * "In case of processor reset, the mission clock/time stamp & system state
 * shall be maintained." Mission elapsed seconds are therefore checkpointed and
 * resumed, NOT restarted from millis(). Written on every state transition and
 * every MISSION_CLOCK_SAVE_MS, not per packet — Teensy 4.1's EEPROM is
 * flash-emulated and has a finite erase budget.
 */
struct Persist {
    uint32_t magic;
    uint8_t  state;
    uint8_t  armed;
    uint8_t  beacon;
    uint8_t  txEnabled;
    uint32_t pktBase;
    uint32_t missionS;          /* mission clock checkpoint, seconds */
    uint32_t launchS;           /* mission time of launch detection, 0 = none */
    float    padPresPa;
    /* Pad air temperature, captured with padPresPa. The two are only meaningful
     * together — padTempC is the T0 of the hypsometric formula that converts
     * padPresPa into an altitude, so restoring one without the other silently
     * reverts to the ISA 15 C assumption and costs ~4.8% of every altitude.
     * Adding this field is why EEPROM_MAGIC moved to layout 8. */
    float    padTempC;
    float    gbx, gby, gbz;
    float    mbx, mby, mbz;
    float    apogeeM;
};

static Persist   pers;
static uint8_t   state       = FS_BOOT;
static bool      armed       = false;
static bool      beaconOn    = false;
static float     apogeeM     = 0;
static uint32_t  pktBase     = 0;
static uint32_t  missionBase = 0;      /* seconds carried over from EEPROM */
static uint32_t  bootMs      = 0;
static uint32_t  launchS     = 0;      /* 0 = launch not yet detected */
static bool      autoTxOff   = false;  /* 90-min RF cut-off has fired */

/* FSM persistence counters — one noisy sample must never move the state. */
static uint8_t   cLaunch = 0, cApogee = 0, cSecEject = 0, cLanded = 0;
static bool      everAscended = false;

static uint32_t  lastLogMs = 0, lastCsvMs = 0, lastPrintMs = 0;
static uint32_t  lastBuildMs = 0, lastClockSaveMs = 0;
static bool      wdogOk = true;

/* Mission elapsed time in seconds, continuous across a processor reset. */
static inline uint32_t missionS() {
    return missionBase + (millis() - bootMs) / 1000UL;
}

/* ===========================================================================
 * PERSISTENCE
 * ======================================================================== */
static void saveState() {
    pers.magic     = EEPROM_MAGIC;
    pers.state     = state;
    pers.armed     = armed ? 1 : 0;
    pers.beacon    = beaconOn ? 1 : 0;
    pers.txEnabled = Radio.txEnabled() ? 1 : 0;
    pers.pktBase   = pktBase + Radio.txCount();
    pers.missionS  = missionS();
    pers.launchS   = launchS;
    pers.apogeeM   = apogeeM;
    Sensors.exportCalibration(pers.padPresPa, pers.padTempC,
                              pers.gbx, pers.gby, pers.gbz,
                              pers.mbx, pers.mby, pers.mbz);
    EEPROM.put(EEPROM_ADDR, pers);
}

static void loadState() {
    EEPROM.get(EEPROM_ADDR, pers);
    if (pers.magic != EEPROM_MAGIC) {
        memset(&pers, 0, sizeof(pers));
        Serial.println(F("// [EE] no valid record — fresh mission, booting SILENT"));
        return;
    }
    /* pers.state is a uint8_t in the EEPROM struct; the cast makes the two
     * arms of the conditional agree in type. The bound check is what keeps a
     * corrupt EEPROM byte from becoming an out-of-range FlightState. */
    state       = (pers.state <= FS_TOUCHDOWN) ? (FlightState)pers.state
                                               : FS_BOOT;
    armed       = pers.armed != 0;
    beaconOn    = pers.beacon != 0;
    apogeeM     = pers.apogeeM;
    pktBase     = pers.pktBase;
    missionBase = pers.missionS;
    launchS     = pers.launchS;
    Sensors.restoreCalibration(pers.padPresPa, pers.padTempC,
                               pers.gbx, pers.gby, pers.gbz,
                               pers.mbx, pers.mby, pers.mbz);

    /* §6.1.v vs §6.1.ix, reconciled: "shall not transmit until commanded"
     * governs a fresh power-up on the pad. Once the vehicle has left FS_BOOT it
     * is airborne and a mid-flight brownout must restore the commanded state,
     * because §6.1.ix requires system state to survive a processor reset — and
     * because a vehicle that goes permanently silent after a reset at 900 m is
     * a lost vehicle. On the pad the boot is always silent. */
    if (state != FS_BOOT && pers.txEnabled) Radio.setTxEnabled(true);

    Serial.printf("// [EE] resumed state=%u pkt=%lu t=%lus pad=%.1f Pa apogee=%.1f m tx=%s\n",
                  state, (unsigned long)pktBase, (unsigned long)missionBase,
                  pers.padPresPa, apogeeM,
                  (state != FS_BOOT && pers.txEnabled) ? "ON" : "SILENT");
}

/* ===========================================================================
 * DOWNLINK PACKET  —  plain ASCII CSV, guideline §6.3.i
 * ========================================================================
 * EXACT WIRE LAYOUT — every position is fixed. vritra_gs.ino ingest() indexes
 * these by number, so a field added or removed here MUST be mirrored there in
 * the same commit or everything downstream misparses silently.
 *
 *    0 <TEAM_ID>    6 <BUS_V>      12 <ACC_X>   18 <ATT_ROLL>
 *    1 <TIME_S>     7 <GNSS_TIME>  13 <ACC_Y>   19 <ATT_PITCH>
 *    2 <PKT>        8 <LAT>        14 <ACC_Z>   20 <rotating value>
 *    3 <ALT_M>      9 <LON>        15 <SPIN>
 *    4 <PRES_PA>   10 <GPS_ALT>    16 <STATE>
 *    5 <TEMP_C>    11 <SATS>       17 <FLAGS>
 * terminated by \r
 *
 * WIRE LAYOUT 3 DROPPED ATT_YAW AND QUAT_X/Y/Z — 12 B of an 86 B packet that
 * the ground station displayed nowhere (no panel read ATT_YAW; the quaternion
 * was discarded on arrival by set_quaternion(None)). ATT_ROLL and ATT_PITCH
 * stayed because guideline table row 13 defines <ACCELEROMETER DATA> as
 * "acceleration and roll & pitch parameters" — that makes them mandated
 * content, not optional. All four removed values are still recorded on the SD
 * card at 20 Hz. See B36_WIRE_LAYOUT in Base36.h.
 *
 * Positions 0..17 are the mandated fields in the mandated order. Everything
 * from position 18 on is "<ANY OPTIONAL DATA>", which §6.3.iii.B explicitly
 * allows teams to append. Resolutions meet or beat the §6.3.iii table.
 *
 * Positions 18..19 are UNCONDITIONAL: the separating comma is always written
 * even when the value is absent, so field COUNT never depends on field VALUE.
 * That is what makes position 20 unambiguous by POSITION alone, with nothing
 * to distinguish by inspecting the value.
 *
 * Only position 20 rotates. N_FRAMES optional values take turns there so the
 * packet stays inside its airtime budget while every GCS field still refreshes.
 * Which slot the value belongs to is NOT tagged — the gateway derives it as
 * frame = (PKT - 1) % N_FRAMES from the packet counter already at position 2.
 * See B36_WIRE_LAYOUT in Base36.h for why the old ",F<n>" tag was removed.
 * A field with no fitted sensor is sent empty, so the GCS renders it as an
 * em-dash and "reported zero" stays distinguishable from "not fitted".
 */
static char     txline[DL_PACKET_MAX];
static uint16_t txlen = 0;

/* THE ROTATION LENGTH IS AGREED IN TWO PLACES AND WAS NEVER CHECKED.
 * The flight computer selects the frame with N_FRAMES (Config.h); the gateway
 * bounds and maps it with B36_OPT_N (Base36.h). Both are 16, and nothing said
 * so. Now that the gateway reconstructs the frame index arithmetically from
 * PKT rather than reading a transmitted tag, a disagreement between these two
 * would not merely bound-check differently — it would phase-shift the entire
 * rotation and write every optional value into the wrong schema field.
 * Caught at compile time, on the board where the mismatch originates. */
static_assert(N_FRAMES == B36_OPT_N,
              "N_FRAMES (Config.h) must equal B36_OPT_N (Base36.h): the "
              "gateway derives the frame index as (PKT-1) % B36_OPT_N");

static uint8_t currentFlags() {
    const SensorData& d = Sensors.data();
    uint8_t f = 0;
    if (armed)                f |= FLAG_ARMED;
    if (Radio.txEnabled())    f |= FLAG_TX_ENABLED;
    if (beaconOn)             f |= FLAG_BEACON;
    if (wdogOk)               f |= FLAG_WDOG_OK;
    if (Radio.onBackupFreq()) f |= FLAG_MCU_BACKUP;
    if (d.baroOk)             f |= FLAG_BARO_OK;
    if (d.imuOk)              f |= FLAG_IMU_OK;
    if (Logger.ok())          f |= FLAG_SD_OK;
    return f;
}

static void buildPacket(uint32_t pkt, uint8_t frame) {
    const SensorData& d = Sensors.data();
    const int cap = (int)sizeof(txline);

    /* Team id is the only field that is not a number, so it is the only one
     * that is not base-36 encoded. */
    int n = snprintf(txline, cap, "%s", TEAM_ID_AIR);
    if (n < 0 || n >= cap) { txlen = 0; return; }

    /* GNSS fields are marked ABSENT until there is a fix. Encoding a no-fix
     * position as 0,0 would put the CanSat in the Gulf of Guinea on the ground
     * station map and in the graded CSV; an empty cell renders as an em-dash
     * and is the truth. The field itself is still present, so the mandated
     * comma-separated field order in §6.3.i is preserved either way. */
    const bool fix = d.gnssFix;

    /* --- mandated core, §6.3.i field order, §6.3.iii resolutions --------- *
     * Index comments are vritra_gcs.py SCHEMA positions. Scale factors and
     * decimal places live in Base36.h and are shared with the gateway. */
    n = b36Append(txline, n, cap, (double)missionS(),        B36_CORE[0],  true);
    n = b36Append(txline, n, cap, (double)pkt,               B36_CORE[1],  true);
    n = b36Append(txline, n, cap, d.altM,                    B36_CORE[2],  d.baroOk);
    n = b36Append(txline, n, cap, d.presPa,                  B36_CORE[3],  d.baroOk);
    n = b36Append(txline, n, cap, d.tempC,                   B36_CORE[4],  d.baroOk);
    n = b36Append(txline, n, cap, d.busV,                    B36_CORE[5],  true);
    /* TRUNCATE THE CENTISECONDS, DO NOT ROUND THEM.
     * d.gnssTime is hhmmsscc. B36_CORE[6] has div 1, so b36Append llrounds
     * whatever it is handed — and `/ 100.0` hands it hhmmss.cc, which for
     * cc >= 50 rounds the SECONDS field up. At ss = 59 that produces hhmm60:
     * not a time. The gateway's ss < 60 guard then refuses to build UTC from
     * it, and the GNSS_TIME cell itself shows an impossible clock. Integer
     * division discards the centiseconds the field cannot carry anyway, so the
     * value is always a real hhmmss. */
    n = b36Append(txline, n, cap, (double)(d.gnssTime / 100UL), B36_CORE[6], fix);
    n = b36Append(txline, n, cap, d.lat,                     B36_CORE[7],  fix);
    n = b36Append(txline, n, cap, d.lon,                     B36_CORE[8],  fix);
    n = b36Append(txline, n, cap, d.gpsAltM,                 B36_CORE[9],  fix);
    n = b36Append(txline, n, cap, (double)d.sats,            B36_CORE[10], true);
    /* §6.3.iii row 13 specifies m/s^2; the ICM reports g. */
    n = b36Append(txline, n, cap, d.ax * 9.80665,            B36_CORE[11], d.imuOk);
    n = b36Append(txline, n, cap, d.ay * 9.80665,            B36_CORE[12], d.imuOk);
    n = b36Append(txline, n, cap, d.az * 9.80665,            B36_CORE[13], d.imuOk);
    n = b36Append(txline, n, cap, d.spinDps,                 B36_CORE[14], d.imuOk);
    n = b36Append(txline, n, cap, (double)state,             B36_CORE[15], true);
    n = b36Append(txline, n, cap, (double)currentFlags(),    B36_CORE[16], true);
    if (n >= cap - 12) { txlen = 0; return; }

    /* --- ROLL AND PITCH, IN EVERY PACKET --------------------------------- *
     *   wire 18,19 = ATT_ROLL, ATT_PITCH   (degrees, B36_ATT)
     *   wire 20    = <value>               (rotating optional; layout 2 dropped
     *                the F<n> tag, the receiver derives the slot as
     *                (PKT - 1) % N_FRAMES)
     *
     * THESE TWO ARE MANDATED CONTENT, WHICH IS WHY THEY SURVIVED LAYOUT 3.
     * Guideline table row 13 defines <ACCELEROMETER DATA> as "acceleration and
     * roll & pitch parameters", so they belong in the graded CSV whether or not
     * a panel happens to read them. ATT_YAW and QUAT_X/Y/Z went the other way:
     * named nowhere in that table, and 12 B per packet for attitude the ground
     * station threw away on arrival. See B36_WIRE_LAYOUT in Base36.h for the
     * full reasoning and where those values still live (SD, 20 Hz).
     *
     * Source is atan2 on the accelerometer — anchored to gravity, cannot drift,
     * and hard-limited to +-90 deg of pitch. Config.h's ATTITUDE ESTIMATOR
     * block records the bench failures from trying to do better with a Mahony
     * filter. The +-90 deg ceiling is real but costs nothing here: these two
     * feed a number in a CSV, not a tumbling 3-D render.
     *
     * WHY THE GROUND STATION MUST NOT DERIVE THESE ITSELF. It sees 1 Hz
     * packets. Attitude rebuilt from one accelerometer sample per second shows
     * a static tilt and nothing else: rotation between samples is invisible,
     * and a sample taken during motion has linear acceleration mixed into it
     * that cannot be separated from gravity.
     *
     * THE COMMA IS ALWAYS RESERVED, for both fields. When the IMU has not
     * settled (imuOk false) b36Append emits the separator and no digits, so the
     * field COUNT never depends on a field VALUE. A "drop the block when
     * airtime is tight" variant was tried and rejected: dropping it shifts the
     * rotating value off position 20 and the receiver has no way to tell a
     * short packet from a full one. Under wire layout 1 this was already fatal
     * (a base-36 value can itself begin with 'F', so a dropped block made the
     * F<n> tag ambiguous); from layout 2 the rotating value is located purely
     * by position, so the invariant now carries the whole load.
     *
     * MEASURED BYTE COST against the 99 B SF10 wall (Config.h §5). These are
     * computed, not estimated — full-deflection base-36 widths:
     *   core (worst, every field at its limit)                 81 B
     *   Euler  -180.0 / -90.0                                   9 B
     *   CR                                                      1 B
     *   worst-case total                                       91 B  OK
     *   realistic in-flight total (India lat/lon, nominal)     74 B  OK
     *
     * Layout 2 carried a 107 B worst case that was OVER the wall and was argued
     * away as unreachable (it needed lat -90 AND lon -180 AND all three accel
     * axes at -16 g AND spin -2000 dps at once). Dropping ATT_YAW and the
     * quaternion removed the corner instead of arguing about it: the worst case
     * now fits with 8 B to spare. */
    n = b36Append(txline, n, cap, d.rollDeg,  B36_ATT, d.imuOk);
    n = b36Append(txline, n, cap, d.pitchDeg, B36_ATT, d.imuOk);
    if (n >= cap - 12) { txlen = 0; return; }

    /* --- ONE rotating optional value, and only if it fits ---------------- *
     * The 99-byte wall (Config.h §5) is a hard airtime limit. Base-36 leaves
     * comfortable room for the optional value on a typical packet, but the
     * check stays: graded data never competes with non-graded data for
     * airtime. A skipped value is picked up on the next pass of the rotation. */
    double  ov = 0;
    bool    op = true;
    /* MUST MATCH B36_OPT / B36_OPT_MAP IN Base36.h, INDEX FOR INDEX.
     * AX/AY/AZ are deliberately absent: they are the same measurement as the
     * mandated ACC_X/ACC_Y/ACC_Z already in every packet, just in g instead of
     * m/s^2, and the gateway now derives them. See the note above B36_OPT. */
    switch (frame) {
    case  0: ov = d.socPct;                       break;  /* 17 SOC_PCT     */
    case  1: ov = d.gndSpeedMs;                   break;  /* 18 GND_SPEED   */
    case  2: ov = d.gx;  op = d.imuOk;            break;  /* 20 GX          */
    case  3: ov = d.gy;  op = d.imuOk;            break;  /* 21 GY          */
    case  4: ov = d.gz;  op = d.imuOk;            break;  /* 22 GZ          */
    case  5: ov = d.mx;  op = d.magOk;            break;  /* 23 MX          */
    case  6: ov = d.my;  op = d.magOk;            break;  /* 24 MY          */
    case  7: ov = d.mz;  op = d.magOk;            break;  /* 25 MZ          */
    case  8: ov = Logger.usedPct(); op = Logger.ok(); break;/* 69 SD_PCT    */
    case  9: ov = d.navicSats;                    break;  /* 74 NAVIC_SATS  */
    case 10: ov = d.hdop; op = fix;               break;  /* 76 HDOP        */
    case 11: ov = d.vdop; op = fix;               break;  /* 77 VDOP        */
    case 12: ov = d.fixQuality;                   break;  /* 78 FIX_QUALITY */
    case 13: ov = d.geoidSepM; op = fix;          break;  /* 79 GEOID_SEP   */
    case 14: ov = d.gnssSysMask;                  break;  /* 80 GNSS_SYS    */
    default: /* 75 UTC, sent as seconds since midnight and rebuilt as a clock
              * by the gateway — 4 base-36 characters instead of 8. */
             ov = (double)d.utcHour * 3600.0 + (double)d.utcMin * 60.0
                + (double)d.utcSec;
             op = d.rtcOk;                        break;
    }

    /* MEASURE THE REAL WIDTH, DO NOT RESERVE THE WORST CASE.
     *
     * This used to read:
     *     if (n + 13 + 1 <= DL_AIRTIME_LIMIT) { ...append in place... }
     * reserving ",F<nn>" + ",<value>" at their theoretical maximum of 13 B
     * before knowing what the value actually was. That guard can only pass
     * when n <= 85, and a realistic in-flight packet reaches n = 87 by the
     * end of the quaternion block:
     *
     *   team+core 66 B · Euler 11 B · quat 10 B  = 87 B
     *   guard:  87 + 13 + 1 = 101 > 99  -> REJECTED
     *   truth:  ",F5,-YA" is 7 B, 87 + 7 + 1 = 95 <= 99  -> FITS, 4 B spare
     *
     * So the optional slot was being dropped on essentially every packet in
     * flight, and all sixteen fields that reach the ground ONLY through the
     * rotation never arrived at all: SOC_PCT, GND_SPEED, GX/GY/GZ, MX/MY/MZ,
     * SD_PCT, NAVIC_SATS, HDOP, VDOP, FIX_QUALITY, GEOID_SEP, GNSS_SYS, UTC.
     * On the ground that looks exactly like sixteen dead sensors on a link
     * that is otherwise reporting perfectly — the same failure signature the
     * AX/AY/AZ note above describes, but permanent rather than slow.
     *
     * A worst-case reservation is the wrong tool when the real width is
     * cheap to compute: encode into a scratch buffer, measure, then commit
     * only if it genuinely fits.
     *
     * THE ",F<n>" TAG IS GONE — wire layout 2, see B36_WIRE_LAYOUT in Base36.h.
     * `frame` is computed in loop() as (pktBase + Radio.txCount()) % N_FRAMES
     * while `pkt` is that same expression + 1, so frame is EXACTLY
     * (PKT - 1) % N_FRAMES and PKT already rides in every packet at wire
     * position 2. The tag was re-sending 3-4 bytes the gateway could always
     * have derived. It needs no receiver state either: the packet carries its
     * own PKT, so a lost packet costs nothing and resynchronisation is free.
     *
     * The ambiguity argument that justified the tag still holds and is still
     * satisfied — it was about never letting field COUNT depend on a field
     * VALUE, which is why positions 18..23 keep their commas unconditionally.
     * With those fixed, position 24 is the rotating value and nothing has to
     * be distinguished by inspection. */
    char opt[20];
    int  ol = b36Append(opt, 0, (int)sizeof(opt), ov, B36_OPT[frame], op);
    /* +1 for the mandated carriage return that still has to fit. */
    if (ol > 0 && n + ol + 1 <= DL_AIRTIME_LIMIT && n + ol + 1 < cap) {
        memcpy(txline + n, opt, (size_t)ol);
        n += ol;
        txline[n] = '\0';
    }

    /* §6.3.i: "followed by a carriage return". */
    if (n + 1 >= cap) { txlen = 0; return; }
    txline[n++] = '\r';

    txlen = (uint16_t)n;
    Radio.queueDownlink(txline, txlen);
}

/* ===========================================================================
 * COMMANDS
 * ======================================================================== */
static void execCmd(uint8_t id, int16_t arg) {
    switch (id) {
    /* CMD_TX_ON OVERRIDES THE 90-MINUTE CUT-OFF, AND ARMS THE STATE MACHINE.
     *
     * Mission Control has final authority over the link, but the cut-off
     * override has to be deliberate — the timer never re-arms itself silently.
     *
     * CMD_ARM was retired along with the rest of the mechanism commands (see
     * the CmdId note in Config.h), and this is where its job went. It belongs
     * here: guideline §6.1.v already forces this command to be the last
     * deliberate operator action before flight, so folding arming into it means
     * there is no separate step that can be forgotten on the pad, and no silent
     * self-arming at power-on either. The operator's intent — "we are going
     * flying now" — is expressed exactly once.
     *
     * updateFsm() still gates FS_BOOT -> FS_LAUNCH on `armed` unchanged, so the
     * state machine behaves identically to before; only what sets the flag
     * moved. FLAG_ARMED still rides in every packet, so the ground station can
     * confirm it took. */
    case CMD_TX_ON:
        if (autoTxOff) {
            autoTxOff = false;
            launchS = 0;
            Serial.println(F("// [RF] 90-min cut-off OVERRIDDEN by ground command"));
        }
        Radio.setTxEnabled(true);
        if (!armed) {
            armed = true;
            Serial.println(F("// [FSM] ARMED by CMD_TX_ON — launch detection live"));
        }
        saveState(); break;
    /* §6.2.v. TX_OFF silences the downlink only — Radio.setTxEnabled(false)
     * drops the LR1121 into permanent receive. It is never put in standby,
     * never slept and never powered down, so TX_ON is always heard.
     *
     * IT DOES NOT DISARM. Silencing the downlink mid-flight — to clear the
     * channel, or under Mission Control's direction — must never reset the
     * state machine that is tracking the descent. Disarming at 800 m would
     * strand the vehicle in FS_LAUNCH and lose the secondary-deployment
     * transition at 600 m entirely. The two concerns are unrelated and are
     * kept that way. */
    case CMD_TX_OFF:       Radio.setTxEnabled(false); saveState(); break;
    case CMD_BEACON_ON:    beaconOn = true;  saveState(); break;
    case CMD_BEACON_OFF:   beaconOn = false; saveState(); break;
    /* §6.1.vi — zero gyro, barometric altitude and accelerometer on command.
     * All three plus the attitude datum are now armed inside startCalibration()
     * itself, so this is one call and there is one place that decides what a
     * calibration means. It used to be three calls from here, one of which
     * (calibrateBarometer) blocked for 320 ms inside a command handler and
     * another of which (zeroAttitude) then waited a further 3 s before doing
     * anything visible. See Config.h CMD_CALIBRATE GATING. */
    case CMD_CALIBRATE:    Sensors.startCalibration(); break;
    case CMD_MAG_CAL:      arg ? Sensors.startMagCal() : Sensors.finishMagCal(); break;
    case CMD_GYRO_ON:      Wheel.set(GYRO_WHEEL_PCT); break;
    case CMD_GYRO_OFF:     Wheel.stop(); break;
    case CMD_GYRO_SPD:     Wheel.set(arg); break;
    case CMD_FAILOVER:     Radio.useBackupFreq(!Radio.onBackupFreq()); break;
    case CMD_GPS_RESET:    Sensors.gnssHardReset(); break;
    case CMD_NEW_MISSION: {
        Persist blank; memset(&blank, 0, sizeof(blank));
        EEPROM.put(EEPROM_ADDR, blank);
        Logger.flushNow();
        SCB_AIRCR = 0x05FA0004;                    /* system reset */
        break; }
    case CMD_RESET:
        saveState();
        Logger.flushNow();
        SCB_AIRCR = 0x05FA0004;
        break;
    default:
        Serial.printf("// [CMD] unknown id=%u\n", id);
        return;
    }
    Serial.printf("// [CMD] id=%u arg=%d applied\n", id, arg);
}

/* Bench/USB command path, so the flight computer is fully testable with no
 * radio link at all. Type e.g. "CMD_CALIBRATE" or "CMD_GYRO_SPD 40". */
static void pollUsb() {
    static char buf[48];
    static uint8_t n = 0;
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\r' || c == '\n') {
            if (!n) continue;
            buf[n] = '\0'; n = 0;
            /* Mirrors the ground station's command list exactly. ARM, DISARM,
             * FORCE_EJECT, FORCE_SEC_EJ and BRAKE_DEPLOY are absent here for
             * the same reason they are absent there — see Config.h CmdId. */
            static const struct { const char* s; uint8_t id; } MAP[] = {
                {"CMD_TX_ON",CMD_TX_ON},{"CMD_TX_OFF",CMD_TX_OFF},
                {"CMD_CALIBRATE",CMD_CALIBRATE},
                {"CMD_BEACON_ON",CMD_BEACON_ON},{"CMD_BEACON_OFF",CMD_BEACON_OFF},
                {"CMD_FAILOVER",CMD_FAILOVER},{"CMD_RESET",CMD_RESET},
                {"CMD_GYRO_ON",CMD_GYRO_ON},{"CMD_GYRO_OFF",CMD_GYRO_OFF},
                {"CMD_GYRO_SPD",CMD_GYRO_SPD},{"CMD_MAG_CAL",CMD_MAG_CAL},
                {"CMD_GPS_RESET",CMD_GPS_RESET},{"CMD_NEW_MISSION",CMD_NEW_MISSION},
            };
            char* sp = strchr(buf, ' ');
            int16_t arg = 0;
            if (sp) { *sp = '\0'; arg = (int16_t)atoi(sp + 1); }
            for (auto& mm : MAP)
                if (!strcmp(buf, mm.s)) { execCmd(mm.id, arg); break; }
            continue;
        }
        if (n < sizeof(buf) - 1) buf[n++] = c;
        else n = 0;
    }
}

/* ===========================================================================
 * FLIGHT STATE MACHINE
 * ========================================================================
 * Every transition is gated on consecutive qualifying samples. The
 * barometer's own failure mode is a mechanical-stress artifact — a fingertip
 * on the lid produced 1214 Pa pp on the bench, about 10 m of apparent
 * altitude — so a single-sample threshold crossing is exactly what a vibration
 * transient looks like. Persistence is what separates them.
 */
static void updateFsm() {
    const SensorData& d = Sensors.data();
    if (!d.baroOk) return;                    /* no altitude, no FSM */

    if (d.altM > apogeeM) apogeeM = d.altM;
    if (d.vspeedMs > FSM_LAUNCH_VS_MS) everAscended = true;

    switch (state) {
    case FS_BOOT:
        if (armed && d.altM > FSM_LAUNCH_AGL_M && d.vspeedMs > FSM_LAUNCH_VS_MS) {
            if (++cLaunch >= FSM_PERSIST_N) {
                state = FS_LAUNCH;
                /* Starts the mandatory 90-minute RF cut-off clock. Persisted,
                 * so a mid-flight reset cannot restart it. */
                launchS = missionS();
                saveState();
            }
        } else cLaunch = 0;
        break;

    case FS_LAUNCH:
        /* Apogee: descending, having genuinely ascended first. Requiring
         * everAscended stops a pad-side pressure transient from declaring
         * apogee at 3 m altitude. */
        if (everAscended && d.vspeedMs < FSM_APOGEE_VS_MS) {
            if (++cApogee >= FSM_PERSIST_N) { state = FS_EJECT; saveState(); }
        } else cApogee = 0;
        break;

    case FS_EJECT:
        /* §5.4.vi — secondary descent control activates at 600 m (+/-10 m).
         * The +/-10 m tolerance is why this is persistence-gated on a filtered
         * altitude rather than a raw sample: 10 m is only ~120 Pa, and a single
         * torn barometer read is worth ~1024 Pa. */
        if (d.altM < FSM_SEC_EJECT_AGL_M && d.vspeedMs < 0) {
            if (++cSecEject >= FSM_PERSIST_N) { state = FS_SEC_EJECT; saveState(); }
        } else cSecEject = 0;
        break;

    case FS_SEC_EJECT:
        if (d.altM < FSM_LANDED_AGL_M && fabsf(d.vspeedMs) < FSM_LANDED_VS_MS) {
            /* Landing needs a much longer proof than any other transition: a
             * momentary near-zero vertical speed happens on every chute
             * oscillation. */
            if (++cLanded >= FSM_LANDED_PERSIST_N) {
                state = FS_TOUCHDOWN;
                beaconOn = true;                  /* §5.2.v recovery aid */
                Wheel.stop();
                Logger.flushNow();
                saveState();
            }
        } else cLanded = 0;
        break;

    case FS_TOUCHDOWN:
    default:
        break;
    }
}

/* ===========================================================================
 * SETUP
 * ======================================================================== */
void setup() {
    bootMs = millis();
    Serial.begin(115200);
    /* No wait-for-Serial loop. On a flight boot there is no host attached and
     * blocking here would delay the radio and the recorder for no benefit. */

    Serial.println();
    Serial.println(F("// ============================================================"));
    Serial.println(F("// VRITRA FLIGHT COMPUTER  " TEAM_ID_FULL));
    Serial.println(F("// firmware " FW_VERSION "   Teensy 4.1"));
    Serial.println(F("// ============================================================"));
    /* Harness rev 2026-08 reused three pins for different signals (24, 30, 22).
     * A board wired to the previous revision compiles and boots perfectly and
     * then fails in two unrelated-looking ways — radio -2, GNSS no data. This
     * banner is the fastest way to notice you are holding the older harness:
     * read it against the silkscreen before debugging anything else. */
    Serial.printf("// [PINS] harness rev 2026-08 | LoRa CS=%d BUSY=%d RST=%d IRQ=%d "
                  "SPI1 MOSI=%d SCK=%d MISO=%d\n",
                  PIN_LORA_CS, PIN_LORA_BUSY, PIN_LORA_RST, PIN_LORA_IRQ,
                  PIN_SPI1_MOSI, PIN_SPI1_SCK, PIN_SPI1_MISO);
    Serial.printf("// [PINS] GNSS Serial2 RX=%d TX=%d RST=%d | I2C SDA=%d SCL=%d | "
                  "INT bmp=%d icm=%d sqw=%d | MOT %d/%d/%d/%d\n",
                  PIN_GNSS_RX, PIN_GNSS_TX, PIN_GNSS_RST, PIN_SDA, PIN_SCL,
                  PIN_BMP_INT, PIN_ICM_INT, PIN_RTC_SQW,
                  PIN_MOT_PWMA, PIN_MOT_AIN1, PIN_MOT_AIN2, PIN_MOT_STBY);

    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, LOW);

    Beacon.begin();         /* amplifier SD low first, for the same reason the
                               wheel's STBY is: a recovery beacon that shrieks
                               through every power-up is a beacon nobody leaves
                               connected during integration */
    Wheel.begin();          /* STBY low first — the wheel must never spin up
                               while the rest of the system is still coming up */
    Sensors.begin();        /* arms the staged init machine, returns at once  */
    Radio.begin();
    Logger.begin();
    /* GPS assist injection happens in DataLogger::service() once Sensors.ready()
     * is true — the GNSS serial port is not yet open here so any PMTK command
     * sent now would be silently discarded. */
    loadState();

#if HAVE_WDT
    WDT_timings_t cfg;
    cfg.trigger = WDT_TIMEOUT_S - 1.0f;
    cfg.timeout = WDT_TIMEOUT_S;
    wdt.begin(cfg);
    Serial.printf("// [WDT] armed, %.1f s\n", WDT_TIMEOUT_S);
#else
    Serial.println(F("// [WDT] Watchdog_t4.h not found — watchdog DISABLED"));
    wdogOk = false;
#endif

    Serial.printf("// [RF] %.2f MHz SF%d BW%.0fk sync 0x%02X — %s\n",
                  FREQ_MAIN_MHZ, LORA_SF, LORA_BW_KHZ, LORA_SYNC,
                  Radio.txEnabled() ? "TRANSMITTING" : "SILENT until CMD_TX_ON (guideline 6.1.v)");
    /* The gateway prints the same number. If they differ, the two copies of
     * Base36.h have drifted and every decoded field will be silently wrong —
     * check this before trusting a single byte of telemetry. */
    Serial.printf("// [WIRE] base-36, schema sum 0x%04X\n", b36SchemaSum());

    /* AUDIBLE PROOF OF LIFE, EVERY BOOT.
     * The recovery beacon is the one subsystem whose first real use is after
     * the vehicle has landed somewhere you cannot see it — the worst possible
     * moment to discover a dead amplifier, an unpowered 5 V rail or LRCLK and
     * BCLK swapped. A quarter-second chirp at power-up exercises the entire
     * chain (I2S clocks, DMA, amplifier enable, speaker) and costs nothing.
     * Deliberately far too short to be the "shrieks on every power-up"
     * behaviour the SD pull-down exists to prevent. */
    Beacon.chirp(250);

    lastLogMs = lastCsvMs = lastPrintMs = lastBuildMs = lastClockSaveMs = millis();
}

/* ===========================================================================
 * LOOP
 * ======================================================================== */
void loop() {
#if HAVE_WDT
    wdt.feed();
#endif

    Sensors.service();      /* staged init, NMEA pump, sampling, bus recovery */
    Radio.service();        /* TX slot, uplink window, or continuous receive  */
    Wheel.service();        /* soft-start ramp                                */
    Logger.service();       /* self-paced 2000 ms sync                        */
    pollUsb();

    const uint32_t now = millis();
    const SensorData& d = Sensors.data();

    /* ---- uplink commands ------------------------------------------------ */
    uint8_t cid; int16_t carg;
    while (Radio.takeCommand(cid, carg)) execCmd(cid, carg);

    if (!Sensors.ready()) return;      /* still probing devices */

    /* ---- flight logic --------------------------------------------------- */
    updateFsm();

    /* ---- RF plan compliance, re-asserted every loop --------------------- */
    const bool onPad = (state == FS_BOOT);
    /* Downlink cadence: slower on the pad so commanding is responsive, 1 Hz
     * once the vehicle is moving and telemetry rate is what is graded. */
    Radio.setPeriod(onPad ? TX_PERIOD_PAD_MS : TX_PERIOD_MS);
    /* Assigned power: +10 dBm on pad, ~+14 dBm in flight. Stepped by the
     * flight computer so it cannot be forgotten on the pad. */
    Radio.setTxPower(onPad ? TX_POWER_PAD_DBM : TX_POWER_FLIGHT_DBM);
    /* A 400 ms uplink window fits every cycle at the 0.5 Hz pad cadence; in
     * flight a 944 ms downlink leaves no room, so it is decimated. */
    Radio.setRxEveryN(onPad ? 1 : RX_EVERY_N_CYCLES);

    /* Mandatory RF cut-off: transmission stops 90 minutes after launch
     * detection so we cannot interfere with a later team's slot. The receiver
     * stays armed, so Mission Control can still reach the vehicle afterwards. */
    if (!autoTxOff && launchS && (missionS() - launchS) >= (TX_AUTO_OFF_MS / 1000UL)) {
        autoTxOff = true;
        Radio.setTxEnabled(false);
        saveState();
        Serial.println(F("// [RF] 90 min since launch detection — transmitter OFF "
                         "(receiver still armed)"));
    }

    /* ---- mission clock checkpoint (§6.1.ix) ----------------------------- */
    if (now - lastClockSaveMs >= MISSION_CLOCK_SAVE_MS) {
        lastClockSaveMs = now;
        saveState();
    }

    /* ---- packet assembly ------------------------------------------------ */
    /* Rebuilt at 10 Hz and handed to the radio, which transmits the freshest
     * copy at its own slot. PKT and the frame index derive from the radio's
     * transmit count, so they advance exactly once per transmission and
     * survive a reboot through pktBase. */
    if (now - lastBuildMs >= 100) {
        lastBuildMs = now;
        uint32_t pkt   = pktBase + Radio.txCount() + 1;
        uint8_t  frame = (uint8_t)((pktBase + Radio.txCount()) % N_FRAMES);
        buildPacket(pkt, frame);
    }

    /* ---- recorders (§5.1.xi) -------------------------------------------- */
    if (now - lastLogMs >= (1000UL / LOG_RATE_HZ)) {
        lastLogMs = now;
        Logger.logSample(d, state, currentFlags(),
                         pktBase + Radio.txCount(),
                         (int16_t)lroundf(Radio.lastRssi() * 10.0f));
    }
    if (now - lastCsvMs >= (1000UL / LOG_CSV_RATE_HZ)) {
        lastCsvMs = now;
        Logger.logCsv(d, state, pktBase + Radio.txCount(),
                      currentFlags(), missionS());
    }

    /* Refresh the stored GNSS position for the NEXT boot's warm start. The
     * whole date/time reconciliation that used to live here is gone: the
     * DS3231 is now synced from GNSS by Sensors.service() as soon as a fix
     * with a valid date arrives, so the aid record simply stamps itself with
     * d.utcEpoch and there is one clock rather than two competing ones.
     * Self-paces to AID_SAVE_MS and refuses to store a poor-quality fix. */
    Logger.saveGPSStateToSD(d);

    /* ---- beacon + status LED (§5.2.v) ----------------------------------- *
     * set() is idempotent — it returns immediately when the state has not
     * changed — so calling it every pass is how the beacon stays in step with
     * `beaconOn` no matter what changed it: a ground command, the touchdown
     * transition, or a state restored from EEPROM after a brownout. service()
     * then runs the warble and the on/off pulse without blocking. */
    Beacon.set(beaconOn);
    Beacon.service();
    if (beaconOn) digitalWrite(PIN_LED, ((now / 250) & 1) ? HIGH : LOW);
    else          digitalWrite(PIN_LED, Radio.ready() && d.gnssFix ? HIGH : LOW);

    /* ---- bench status line ---------------------------------------------- */
    if (now - lastPrintMs >= 1000) {
        lastPrintMs = now;
        Serial.printf(
            "// t=%lus st=%u %s | P=%.1fPa T=%.1fC AGL=%.1fm VS=%+.1f | "
            "%.2fV %.0f%% | fix=%u sats=%u hdop=%.1f snr=%u drop=%lu ttff=%lums | "
            "a=%.2fg spin=%.0f | RF tx=%lu fail=%lu cmd=%lu rej=%lu rssi=%.0f len=%u | "
            "SD %s rec=%lu err=%lu | i2c f=%u r=%u\n",
            (unsigned long)missionS(), state,
            Radio.txEnabled() ? "TX" : "SILENT-RX",
            d.presPa, d.tempC, d.altM, d.vspeedMs,
            d.busV, d.socPct,
            d.fixQuality, d.sats, d.hdop, d.bestSnr, (unsigned long)d.fixDropouts,
            (unsigned long)d.ttffMs,
            d.accMagG, d.spinDps,
            (unsigned long)Radio.txCount(), (unsigned long)Radio.txFails(),
            (unsigned long)Radio.cmdCount(), (unsigned long)Radio.crcRejects(),
            Radio.lastRssi(), txlen,
            Logger.ok() ? Logger.binName() : "OFF",
            (unsigned long)Logger.records(), (unsigned long)Logger.writeErrors(),
            d.i2cFaults, d.i2cRecovers);

        if (!d.baroOk)
            Serial.println(F("// [WARN] barometer not responding — ALT_M/PRES_PA blank"));
        if (!d.imuOk)
            Serial.println(F("// [WARN] IMU not responding"));
    }
}
