/* =============================================================================
 * DataLogger.cpp
 * =============================================================================
 * SdFat on the Teensy 4.1's built-in SDIO controller. FIFO_SDIO is the
 * non-DMA path: slightly lower peak throughput than DMA_SDIO but it does not
 * fight the SPI1 DMA channels the LR1121 uses, and at 2.5 kB/s we are three
 * orders of magnitude below the card's limit anyway. Determinism beats peak
 * bandwidth for a flight recorder.
 * ========================================================================== */
#include "DataLogger.h"
#include "Radio.h"
#include "SdFat.h"

DataLogger Logger;

static SdFs  sd;
static FsFile fbin;
static FsFile fcsv;

/* ============================================================================
 * FULL-SCHEMA CSV MIRROR
 * ============================================================================
 * Columns 0..82 are vritra_gcs.py's SCHEMA, in SCHEMA order, so this file is
 * interchangeable with the graded CSV for any analysis tool. Columns from 83 on
 * are flight-computer diagnostics that have no schema slot; they are APPENDED,
 * never inserted, so a positional reader of the first 83 is unaffected.
 *
 * WAS 0..89 / 90+, THEN 0..86 / 87+. AX/AY/AZ left SCHEMA (they were ACC_X/Y/Z
 * in g) and every column above 22 shifted down by three; then ATT_YAW and
 * QUAT_X/Y/Z left the WIRE at layout 3 and so had to leave the schema block
 * here too — they were APPENDED to the diagnostics tail rather than deleted,
 * because the reason they left the link (airtime) does not apply to a 32 GB
 * card. verify_schema.py compares this header against vritra_gcs.py's SCHEMA
 * name by name — run it after any edit here rather than counting commas.
 *
 * NOT-FITTED SENSORS ARE WRITTEN AS "-", NOT 0. This is the same rule the
 * ground station follows for the em-dash: "the sensor is not there" and "the
 * sensor measured zero" are different facts, and a flight record that conflates
 * them is worse than one that omits the column. The full prototype-to-final
 * sensor set is present as columns either way, so the layout does not change
 * when a sensor is finally fitted — only the "-" becomes a number, and no
 * analysis script has to be rewritten.
 *
 * Header must match the emitter in logCsv() field for field. Broken across
 * lines at schema group boundaries to make that checkable by eye.
 *
 * ATT_YAW and the quaternion left the WIRE at layout 3, NOT this file. Bytes
 * are free on a 32 GB card and they are a genuine record of what the vehicle's
 * filters computed, so they moved out of the schema block and onto the end of
 * the diagnostics tail, because columns 0..82 must mirror vritra_gcs.py's
 * SCHEMA exactly and they are no longer in it.
 *
 * KEEP COMMENTS OUT OF THE CONCATENATION BELOW, except the short column-range
 * markers. verify_schema.py and csvcheck.py both locate this header with a
 * non-greedy match up to the first ';', so a single semicolon inside a comment
 * between two string literals silently truncates the header they check against
 * — the build stays correct and the safety net quietly stops covering the tail.
 */
#define CSV_ABSENT "-"
static const char CSV_HEADER[] =
    /*  0-15 mandated core          */ "TIME_S,PKT,ALT_M,PRES_PA,TEMP_C,BUS_V,GNSS_TIME,LAT,LON,GPS_ALT_M,SATS,ACC_X,ACC_Y,ACC_Z,SPIN_RATE,STATE,"
    /* 16-19 power / env            */ "BUS_I_MA,SOC_PCT,GND_SPEED,HUM_PCT,"
    /* 20-28 inertial               */ "GX,GY,GZ,MX,MY,MZ,AHX,AHY,AHZ,"
    /* 29-43 environmental          */ "PM1,PM25,PM10,LUX,UV,CO2,TVOC,ECO2,AQI,O3,HDC_T,HDC_RH,LGT_DIST,LGT_INT,LGT_FLAG,"
    /* 44-57 spectral (AS7343)      */ "S_405,S_425,S_450,S_475,S_515,S_550,S_555,S_600,S_640,S_690,S_745,S_855,S_CLR,S_FLK,"
    /* 58-60 rf                     */ "RSSI,SNR,FREQ_MHZ,"
    /* 61-66 servo / mech           */ "SERVO_V,SERVO_I_MA,SRV1_DEG,SRV2_DEG,SRV3_DEG,SRV4_DEG,"
    /* 67-73 system                 */ "MCU_ACTIVE,WDOG_OK,SD_PCT,FLASH_PCT,BEACON_ON,CAM1_OK,CAM2_OK,"
    /* 74-80 gnss                   */ "NAVIC_SATS,UTC,HDOP,VDOP,FIX_QUALITY,GEOID_SEP,GNSS_SYS,"
    /* 81-82 attitude (mandated)    */ "ATT_ROLL,ATT_PITCH,"
    /* 83+   FC diagnostics         */ "LOCAL_TIME,T_MS,VSPEED_MS,ALT_ASL_M,PAD_PRES_PA,PAD_TEMP_C,"
                                       "ACC_MAG_G,BEST_SNR,RTC_TEMP_C,I2C_FAULTS,I2C_RECOVERS,"
                                       "FIX_DROPOUTS,WORST_GAP_MS,TTFF_MS,AID_USED,RTC_SYNCED,"
    /* 99-102 off-wire attitude     */ "ATT_YAW,QUAT_X,QUAT_Y,QUAT_Z";

bool DataLogger::begin() {
    ok_ = false;
    pageFill_ = 0;
    memset(page_, 0, sizeof(page_));

    if (!sd.begin(SdioConfig(FIFO_SDIO))) {
        Serial.println(F("// [SD] card not found — logging disabled"));
        return false;
    }

    /* Find the next free run number so a re-boot on the pad never overwrites
     * the previous attempt's data. */
    uint16_t n = 0;
    for (n = 0; n < 1000; n++) {
        snprintf(binName_, sizeof(binName_), "FLT%03u.BIN", n);
        if (!sd.exists(binName_)) break;
    }
    snprintf(csvName_, sizeof(csvName_), "FLT%03u.CSV", n);

    if (!fbin.open(binName_, O_RDWR | O_CREAT | O_TRUNC)) {
        Serial.println(F("// [SD] cannot create binary log"));
        return false;
    }
    /* Contiguous pre-allocation. Without this the FAT is rewritten every time
     * the file crosses a cluster boundary — an unbounded-latency write to a
     * different region of the card, which is exactly what must not happen
     * during descent. */
    if (!fbin.preAllocate(LOG_PREALLOC_BYTES))
        Serial.println(F("// [SD] preAllocate failed — log is non-contiguous"));

    if (!fcsv.open(csvName_, O_RDWR | O_CREAT | O_TRUNC)) {
        Serial.println(F("// [SD] cannot create CSV mirror"));
    } else {
        fcsv.println(CSV_HEADER);
        fcsv.sync();
    }

    /* Capacity is read once. freeClusterCount() walks the FAT and can take
     * hundreds of milliseconds — acceptable at boot, never in flight. */
    capBytes_ = (uint64_t)sd.card()->sectorCount() * 512ULL;
    if (!capBytes_) capBytes_ = SD_CAPACITY_BYTES;
    uint64_t freeBytes = (uint64_t)sd.freeClusterCount() *
                         (uint64_t)sd.bytesPerCluster();
    baseUsed_ = (capBytes_ > freeBytes) ? (capBytes_ - freeBytes) : 0;
    usedPct_  = (uint8_t)((baseUsed_ * 100ULL) / capBytes_);
    Serial.printf("// [SD] %s + %s  %.1f GB card, %u%% used\n",
                  binName_, csvName_, (double)capBytes_ / 1e9, usedPct_);

    lastSyncMs_ = millis();
    lastCsvMs_  = millis();
    ok_ = true;
    return true;
}

void DataLogger::logSample(const SensorData& d, uint8_t state, uint8_t flags,
                           uint32_t pkt, int16_t rssi_x10) {
    if (!ok_) return;

    LogRecord r;
    memset(&r, 0, sizeof(r));
    r.magic        = 0x564C4752UL;               /* 'VRLG' */
    r.t_ms         = millis();
    r.pkt          = pkt;
    r.utc_epoch    = d.utcEpoch;
    r.pres_pa      = d.presPa;
    r.temp_c       = d.tempC;
    r.alt_agl_m    = d.altM;
    r.vspeed_ms    = d.vspeedMs;
    r.bus_v        = d.busV;
    r.lat_e7       = (int32_t)lround(d.lat * 1e7);
    r.lon_e7       = (int32_t)lround(d.lon * 1e7);
    r.gps_alt_m    = d.gpsAltM;
    r.gnd_speed_ms = d.gndSpeedMs;
    r.gnss_time    = d.gnssTime;
    r.hdop         = d.hdop;
    r.vdop         = d.vdop;
    r.geoid_m      = d.geoidSepM;
    r.ax = d.ax; r.ay = d.ay; r.az = d.az;
    r.gx = d.gx; r.gy = d.gy; r.gz = d.gz;
    r.mx = d.mx; r.my = d.my; r.mz = d.mz;
    r.spin_dps     = d.spinDps;
    r.sats         = d.sats;
    r.navic        = d.navicSats;
    r.fix_q        = d.fixQuality;
    r.gnss_sys     = d.gnssSysMask;
    r.state        = state;
    r.flags        = flags;
    r.sd_pct       = usedPct_;
    r.rssi_x10     = rssi_x10;
    r.i2c_faults   = d.i2cFaults;
    r.best_snr     = d.bestSnr;
    r.crc16        = RadioLink::crc16((const uint8_t*)&r, sizeof(r) - 2);

    memcpy(page_ + (size_t)pageFill_ * LOG_RECORD_BYTES, &r, sizeof(r));
    recCount_++;

    if (++pageFill_ >= LOG_RECS_PER_BLOCK) {
        /* One full 512-byte block, block-aligned. This is the only call that
         * ever touches the card in the flight path. */
        if (fbin.write(page_, LOG_BLOCK_BYTES) != (int)LOG_BLOCK_BYTES)
            writeErr_++;
        else
            bytesOut_ += LOG_BLOCK_BYTES;
        pageFill_ = 0;
        memset(page_, 0, sizeof(page_));
    }
}

void DataLogger::logCsv(const SensorData& d, uint8_t state, uint32_t pkt,
                        uint8_t flags, uint32_t missionS) {
    if (!ok_ || !fcsv) return;

    /* One line is built in a local buffer and written once. At 20 Hz this is
     * ~28 writes/s into SdFat's 512 B buffer, which is nothing — but a stream
     * of small printf() calls would be, so the formatting is batched. */
    char b[900];
    int n = 0;
    const int cap = (int)sizeof(b);
    #define CSVP(...)  do { if (n < cap) n += snprintf(b + n, (size_t)(cap - n), __VA_ARGS__); } while (0)

    /* --- 0..15  mandated core, same units as the graded downlink --------- */
    CSVP("%lu,%lu,", (unsigned long)missionS, (unsigned long)pkt);
    if (d.baroOk) CSVP("%.2f,%.1f,%.2f,", d.altM, d.presPa, d.tempC);
    else          CSVP(CSV_ABSENT "," CSV_ABSENT "," CSV_ABSENT ",");
    CSVP("%.3f,", d.busV);
    /* GNSS_TIME IS hhmmss, NOT hhmmsscc — THIS COLUMN IS SHARED WITH THE GRADED
     * CSV AND THE TWO MUST CARRY THE SAME NUMBER.
     * d.gnssTime is stored internally as hhmmsscc (GGA's hhmmss.ss x 100), but
     * the downlink sends hhmmss (B36_CORE[6], div 1) and vritra_gcs.py writes
     * that column as %.2f. Printing the raw internal value here made column 6
     * of this file exactly 100x the same column of the ground station's file,
     * so the two records disagreed about the time of every single sample while
     * looking equally plausible. Divided and printed at 2 dp the column now has
     * the same units and scale in both files; this one keeps the centiseconds
     * the wire drops, so it reads hhmmss.cc against the downlink's hhmmss.00 —
     * a difference in resolution, which is honest, rather than in scale, which
     * was a lie. (The L89 outputs whole seconds at 1 Hz, so in practice cc is
     * 00 and the two columns match character for character.) */
    if (d.gnssFix) CSVP("%.2f,%.7f,%.7f,%.1f,",
                        (double)d.gnssTime / 100.0, d.lat, d.lon, (double)d.gpsAltM);
    else           CSVP(CSV_ABSENT "," CSV_ABSENT "," CSV_ABSENT "," CSV_ABSENT ",");
    CSVP("%u,", d.sats);
    /* ACC_* is the mandated accelerometer field, in m/s^2 per §6.3.iii, and it
     * is now the ONLY accelerometer column. AX/AY/AZ used to follow at 20/21/22
     * carrying the same three numbers in g; they have been removed from SCHEMA
     * and the ground station derives them for display. Writing a value twice in
     * one row is how a log grows a column that disagrees with itself. */
    if (d.imuOk) CSVP("%.4f,%.4f,%.4f,%.2f,",
                      d.ax * 9.80665f, d.ay * 9.80665f, d.az * 9.80665f, d.spinDps);
    else         CSVP(CSV_ABSENT "," CSV_ABSENT "," CSV_ABSENT "," CSV_ABSENT ",");
    CSVP("%u,", state);

    /* --- 16..19  BUS_I_MA(INA226) SOC GND_SPEED HUM_PCT ------------------ */
    CSVP(CSV_ABSENT ",%.1f,%.2f," CSV_ABSENT ",", d.socPct, (double)d.gndSpeedMs);

    /* --- 20..28  inertial. AHX/AHY/AHZ are the LSM6DSO32, not fitted. ---- */
    if (d.imuOk) CSVP("%.2f,%.2f,%.2f,", d.gx, d.gy, d.gz);
    else         CSVP(CSV_ABSENT "," CSV_ABSENT "," CSV_ABSENT ",");
    if (d.magOk) CSVP("%.1f,%.1f,%.1f,", d.mx, d.my, d.mz);
    else         CSVP(CSV_ABSENT "," CSV_ABSENT "," CSV_ABSENT ",");
    CSVP(CSV_ABSENT "," CSV_ABSENT "," CSV_ABSENT ",");        /* AHX AHY AHZ */

    /* --- 29..43  environmental — none of these are fitted on the prototype */
    CSVP(CSV_ABSENT "," CSV_ABSENT "," CSV_ABSENT ","          /* PM1 PM25 PM10 */
         CSV_ABSENT "," CSV_ABSENT ","                          /* LUX UV        */
         CSV_ABSENT "," CSV_ABSENT "," CSV_ABSENT "," CSV_ABSENT "," /* CO2 TVOC ECO2 AQI */
         CSV_ABSENT ","                                         /* O3            */
         CSV_ABSENT "," CSV_ABSENT ","                          /* HDC_T HDC_RH  */
         CSV_ABSENT "," CSV_ABSENT "," CSV_ABSENT ",");         /* LGT_*         */

    /* --- 44..57  AS7343 spectral, 14 channels, not fitted ---------------- */
    for (int i = 0; i < 14; i++) CSVP(CSV_ABSENT ",");

    /* --- 58..60  rf ------------------------------------------------------ */
    CSVP("%.1f,%.1f,%.2f,", (double)Radio.lastRssi(), (double)Radio.lastSnr(),
         Radio.onBackupFreq() ? (double)FREQ_BACKUP_MHZ : (double)FREQ_MAIN_MHZ);

    /* --- 61..66  servo rail + four servos, not fitted -------------------- */
    for (int i = 0; i < 6; i++) CSVP(CSV_ABSENT ",");

    /* --- 67..73  system. FLASH_PCT/CAM* have no hardware. ---------------- */
    CSVP("%u,%u,%u," CSV_ABSENT ",%u," CSV_ABSENT "," CSV_ABSENT ",",
         (flags & FLAG_MCU_BACKUP) ? 1u : 0u,
         (flags & FLAG_WDOG_OK)    ? 1u : 0u,
         usedPct_,
         (flags & FLAG_BEACON)     ? 1u : 0u);

    /* --- 74..80  gnss ---------------------------------------------------- */
    CSVP("%u,", d.navicSats);
    if (d.rtcOk) CSVP("%02u:%02u:%02u,", d.utcHour, d.utcMin, d.utcSec);
    else         CSVP(CSV_ABSENT ",");
    if (d.gnssFix) CSVP("%.2f,%.2f,%u,%.1f,",
                        (double)d.hdop, (double)d.vdop, d.fixQuality, (double)d.geoidSepM);
    else           CSVP(CSV_ABSENT "," CSV_ABSENT ",%u," CSV_ABSENT ",", d.fixQuality);
    CSVP("%u,", d.gnssSysMask);

    /* --- 81..82  attitude, the two the guideline mandates ---------------- */
    if (d.imuOk) CSVP("%.2f,%.2f,", d.rollDeg, d.pitchDeg);
    else         CSVP(CSV_ABSENT "," CSV_ABSENT ",");

    /* --- 83+  flight-computer diagnostics, appended ---------------------- */
    uint16_t ly; uint8_t lmo, ld, lh, lmi, ls;
    Sensors.localTime(ly, lmo, ld, lh, lmi, ls);
    if (d.rtcOk) CSVP("%04u-%02u-%02uT%02u:%02u:%02u,", ly, lmo, ld, lh, lmi, ls);
    else         CSVP(CSV_ABSENT ",");
    CSVP("%lu,", (unsigned long)millis());
    if (d.baroOk) CSVP("%.2f,%.2f,%.1f,%.1f,",
                       d.vspeedMs, d.altAslM, d.padPresPa, d.padTempC);
    else          CSVP(CSV_ABSENT "," CSV_ABSENT "," CSV_ABSENT "," CSV_ABSENT ",");
    if (d.imuOk) CSVP("%.4f,", d.accMagG); else CSVP(CSV_ABSENT ",");
    CSVP("%u,", d.bestSnr);
    if (d.rtcOk) CSVP("%.2f,", d.rtcTempC); else CSVP(CSV_ABSENT ",");
    CSVP("%u,%u,%lu,%lu,%lu,%u,%u,",
         d.i2cFaults, d.i2cRecovers,
         (unsigned long)d.fixDropouts, (unsigned long)d.worstGapMs,
         (unsigned long)d.ttffMs,
         d.aidInjected ? 1u : 0u, d.rtcSynced ? 1u : 0u);

    /* ATT_YAW + quaternion — off the wire since layout 3, still recorded here.
     * These close the row, so this is the only CSVP that ends in \n. */
    if (d.imuOk)     CSVP("%.2f,", d.yawDeg); else CSVP(CSV_ABSENT ",");
    if (d.attQuatOk) CSVP("%.4f,%.4f,%.4f\n", d.qx, d.qy, d.qz);
    else             CSVP(CSV_ABSENT "," CSV_ABSENT "," CSV_ABSENT "\n");
    #undef CSVP

    if (n > 0) fcsv.write(b, (size_t)(n < cap ? n : cap - 1));
}

void DataLogger::service() {
    if (!ok_) return;
    /* Walks the aid-injection state machine. It waits for Sensors.ready() by
     * itself — doing this in setup() was a no-op because the GNSS UART is not
     * open until the staged init reaches IS_GNSS_PORT. */
    aidService();

    uint32_t now = millis();
    if (now - lastSyncMs_ < LOG_SYNC_MS) return;
    lastSyncMs_ = now;
    flushNow();
}

void DataLogger::flushNow() {
    if (!ok_) return;
    /* sync() forces the directory entry and the card's own write cache out to
     * physical flash. Without it, everything since the last sync lives only in
     * the card's volatile buffer and is lost on a battery disconnect —
     * which is precisely the parachute-shock failure mode. */
    fbin.sync();
    if (fcsv) fcsv.sync();

    /* Track utilisation from the boot-time baseline plus bytes actually
     * written, rather than re-walking the FAT — freeClusterCount() would stall
     * the flight loop for hundreds of milliseconds. */
    uint64_t used = baseUsed_ + (uint64_t)bytesOut_;
    uint64_t p = (used * 100ULL) / capBytes_;
    usedPct_ = (uint8_t)(p > 100 ? 100 : p);
}

/* ============================================================================
 * GNSS AIDING  —  making the second and every later lock fast
 * ============================================================================
 * WHY THIS EXISTS. The L89 is assumed to have no backup supply on V_BCKP, so
 * every power-down wipes its ephemeris, its almanac, its last position AND its
 * internal clock. Left to itself it therefore performs a full cold start every
 * single time: it does not know the date, so it cannot predict which satellites
 * are above the horizon, and it has to sweep the entire code/Doppler space
 * blind. That is the 30-60 second wait.
 *
 * WHAT WE CAN GIVE IT BACK, AND FROM WHERE:
 *   time      the DS3231, which keeps running on its own coin cell at ±2 ppm.
 *             Synced from GNSS on the first fix of the previous flight, so it
 *             is accurate to about a second. See Sensors.cpp.
 *   position  this file on the SD card, written while the last fix was held.
 *   orbits    EASY, the receiver's own on-chip orbit prediction. Enabled in
 *             the GNSS config block and re-asserted here. It needs receiver
 *             retention to survive a power cycle, so on this hardware it
 *             probably contributes nothing — it is re-asserted anyway because
 *             it costs one command and is free if the assumption is wrong.
 *
 * Time plus position is a WARM start: roughly 15-30 s instead of 30-60 s. A hot
 * start (1-5 s) additionally needs valid ephemeris, which is only ~4 hours old
 * before it expires and cannot survive a power cycle without backup power. Warm
 * is the honest ceiling for this hardware and is what this implements.
 *
 * ON THE COMMANDS. $PMTK740 (time aid) and $PMTK741 (position + time aid) are
 * the MediaTek aiding pair, and this receiver already demonstrably accepts a
 * PMTK subset. Whether it honours these two specifically is NOT assumed — it is
 * MEASURED: SensorData::ttffMs latches the time to first fix on every boot and
 * prints whether the attempt was aided. Compare an aided boot against an
 * unaided one and the answer is in the log rather than in a comment. If they
 * turn out to be ignored, the DS3231 sync is still worth having on its own,
 * because it is what puts a correct UTC in the flight record and in the CSV
 * without waiting for a fix.
 *
 * SAFETY: A STALE POSITION IS WORSE THAN NO POSITION. A receiver aided with
 * where it was two weeks and 400 km ago will search the wrong Dopplers and take
 * LONGER than an honest cold start. AID_MAX_AGE_S is the guard, and it is
 * checked against the RTC — which is why the aid file stores the epoch it was
 * written at rather than just a position. */

/* Aid record: a single line, version-tagged so a future format change is
 * detectable rather than silently misparsed.
 *   VRITRAAID,1,<lat>,<lon>,<alt_m>,<utc_epoch>,<sats>,<hdop> */
#define AID_TAG      "VRITRAAID"
#define AID_VERSION  1

bool DataLogger::aidLoad() {
    if (!sd.exists(AID_FILE)) {
        Serial.println(F("// [AID] no stored position — this boot is a cold start"));
        return false;
    }
    FsFile f = sd.open(AID_FILE, O_READ);
    if (!f) return false;
    char line[128];
    int n = f.fgets(line, sizeof(line));
    f.close();
    if (n <= 0) return false;

    char tag[16] = {0};
    int  ver = 0, sats = 0;
    double la = 0, lo = 0;
    float  al = 0, hd = 0;
    unsigned long ep = 0;
    if (sscanf(line, "%15[^,],%d,%lf,%lf,%f,%lu,%d,%f",
               tag, &ver, &la, &lo, &al, &ep, &sats, &hd) != 8) {
        Serial.println(F("// [AID] stored position unparsable — ignoring"));
        return false;
    }
    if (strcmp(tag, AID_TAG) != 0 || ver != AID_VERSION) {
        Serial.printf("// [AID] stored position is format '%s' v%d, expected "
                      "'%s' v%d — ignoring\n", tag, ver, AID_TAG, AID_VERSION);
        return false;
    }

    /* AGE CHECK. Only possible because the RTC survived the power cycle too —
     * without a trustworthy clock there is no way to know whether the stored
     * position is minutes or months old, and an unbounded-age position is
     * exactly the input that makes aiding harmful. */
    const SensorData& d = Sensors.data();
    if (Sensors.rtcValid() && d.utcEpoch > ep) {
        uint32_t age = d.utcEpoch - ep;
        if (age > AID_MAX_AGE_S) {
            Serial.printf("// [AID] stored position is %lu days old (limit %lu) "
                          "— ignoring, aiding with a stale position is slower "
                          "than a clean cold start\n",
                          (unsigned long)(age / 86400UL),
                          (unsigned long)(AID_MAX_AGE_S / 86400UL));
            return false;
        }
        Serial.printf("// [AID] stored position %.5f,%.5f alt %.0f m, %lu h old\n",
                      la, lo, (double)al, (unsigned long)(age / 3600UL));
    } else {
        Serial.printf("// [AID] stored position %.5f,%.5f alt %.0f m, age UNKNOWN "
                      "(no trustworthy RTC) — using it anyway\n", la, lo, (double)al);
    }

    aidLat_ = la; aidLon_ = lo; aidAlt_ = al; aidSavedEpoch_ = ep;
    return true;
}

/* One command per pass, paced. Mirrors the pacing in Sensors.cpp's IS_GNSS_CFG
 * stage: the L89 silently drops commands that arrive faster than it consumes
 * them, so the gap is required — it just must not be bought with delay(). */
void DataLogger::aidService() {
    const uint32_t now = millis();
    switch (aidStage_) {

    case AID_READ:
        /* Wait for Sensors to finish its staged init. Before that the GNSS
         * UART is not open and every command would go nowhere — which is
         * exactly why the original version of this could not live in setup(). */
        if (!Sensors.ready()) return;
        aidHave_ = aidLoad();
        aidStage_ = AID_TIME;
        aidAt_ = now;
        return;

    case AID_TIME: {
        if (now - aidAt_ < 120) return;      /* let the config block finish  */
        aidAt_ = now;
        aidStage_ = aidHave_ ? AID_POS : AID_EASY;
        if (!Sensors.rtcValid()) {
            Serial.println(F("// [AID] RTC not trustworthy — no time aid sent"));
            return;
        }
        const SensorData& d = Sensors.data();
        char cmd[64];
        snprintf(cmd, sizeof(cmd), "PMTK740,%04u,%02u,%02u,%02u,%02u,%02u",
                 d.utcYear, d.utcMon, d.utcDay, d.utcHour, d.utcMin, d.utcSec);
        Sensors.sendPMTKCommand(String(cmd));
        Serial.printf("// [AID] time aid  %04u-%02u-%02u %02u:%02u:%02u UTC\n",
                      d.utcYear, d.utcMon, d.utcDay, d.utcHour, d.utcMin, d.utcSec);
        return; }

    case AID_POS: {
        if (now - aidAt_ < 120) return;
        aidAt_ = now;
        aidStage_ = AID_EASY;
        const SensorData& d = Sensors.data();
        /* PMTK741 wants position AND time together. With no trustworthy clock
         * the receiver cannot use the position either, so this is skipped
         * rather than sent with a fabricated timestamp. */
        if (!Sensors.rtcValid()) return;
        char cmd[112];
        snprintf(cmd, sizeof(cmd),
                 "PMTK741,%.6f,%.6f,%.1f,%04u,%02u,%02u,%02u,%02u,%02u",
                 aidLat_, aidLon_, (double)aidAlt_,
                 d.utcYear, d.utcMon, d.utcDay, d.utcHour, d.utcMin, d.utcSec);
        Sensors.sendPMTKCommand(String(cmd));
        Serial.printf("// [AID] position aid %.5f,%.5f %.0f m — expect a WARM "
                      "start (~15-30 s) instead of cold (~30-60 s)\n",
                      aidLat_, aidLon_, (double)aidAlt_);
        return; }

    case AID_EASY: {
        if (now - aidAt_ < 120) return;
        aidAt_ = now;
        aidStage_ = AID_DONE;
        Sensors.sendPMTKCommand("PMTK869,1,1");   /* re-assert orbit prediction */
        /* Recorded so the FIRST FIX line can say whether it was aided — the
         * only way to tell whether any of this actually helped.
         *
         * AND-ed with rtcValid() on purpose: the position aid command carries
         * a timestamp, so AID_POS declines to send it without a trustworthy
         * clock. Reporting "aided" off aidHave_ alone would claim credit for a
         * command that was never transmitted, and the TTFF comparison it feeds
         * would be measuring the wrong thing. */
        const bool reallyAided = aidHave_ && Sensors.rtcValid();
        Sensors.markAidInjected(reallyAided);
        Serial.printf("// [AID] injection complete (%s). Watch the "
                      "[GNSS] FIRST FIX line for the measured TTFF.\n",
                      reallyAided ? "time + position"
                                  : (Sensors.rtcValid() ? "time only — no stored position"
                                                        : "NOTHING — no trustworthy clock"));
        return; }

    case AID_IDLE:
    case AID_DONE:
    default:
        return;
    }
}

/* Refresh the stored position while a good fix is held.
 *
 * QUALITY-GATED. Only a fix with enough satellites and a low enough HDOP is
 * worth storing — aiding the next boot with a poor position wastes the whole
 * mechanism. And written ATOMICALLY: to a temp file, then renamed over the old
 * one. The previous version did sd.remove() then sd.open(O_CREAT), which leaves
 * a window where the file does not exist at all. A power cut inside that window
 * destroys the aid data — during descent, which is precisely when an
 * unscheduled power loss is most likely and when the stored position matters
 * most for recovery. */
void DataLogger::saveGPSStateToSD(const SensorData& d) {
    if (!ok_) return;
    if (millis() - lastAidSaveMs_ < AID_SAVE_MS) return;
    if (!d.gnssFix || d.sats < AID_MIN_SATS) return;
    if (d.hdop <= 0.0f || d.hdop > AID_MAX_HDOP) return;
    if (!Sensors.rtcValid()) return;      /* no clock -> cannot stamp an age */
    lastAidSaveMs_ = millis();

    const char* tmp = "GPSAID.TMP";
    FsFile f = sd.open(tmp, O_WRITE | O_CREAT | O_TRUNC);
    if (!f) return;
    char line[128];
    int n = snprintf(line, sizeof(line), "%s,%d,%.6f,%.6f,%.1f,%lu,%u,%.2f\n",
                     AID_TAG, AID_VERSION, d.lat, d.lon, (double)d.gpsAltM,
                     (unsigned long)d.utcEpoch, d.sats, (double)d.hdop);
    bool wrote = (n > 0) && (f.write(line, (size_t)n) == n);
    f.sync();
    f.close();
    if (!wrote) { sd.remove(tmp); return; }

    /* Rename over the old file. On FAT this updates one directory entry, which
     * is as close to atomic as this filesystem offers — far better than the
     * remove-then-create window it replaces. */
    if (sd.exists(AID_FILE)) sd.remove(AID_FILE);
    sd.rename(tmp, AID_FILE);
}
