/* =============================================================================
 * DataLogger.h — SdFat / SDIO flight recorder for Teensy 4.1
 * =============================================================================
 * WHY BINARY, AND WHY 128 BYTES
 *   A CanSat's log has to survive a battery disconnect at chute snap-open. The
 *   two things that actually make that work are (a) never letting a partial
 *   write corrupt anything but the record it was in, and (b) never touching
 *   FAT metadata while airborne.
 *
 *   LogRecord is exactly 128 bytes, so four records fill one 512-byte SD block
 *   with no padding and no read-modify-write. Records are accumulated into a
 *   512-byte page and handed to the card whole — the card's own program unit
 *   is a block, so a block-aligned write is the only kind that is atomic from
 *   the flash controller's point of view.
 *
 *   Each record carries a magic and a CRC-16 over its own bytes. A torn write
 *   therefore costs one record, and a post-flight decoder can resynchronise by
 *   scanning for the magic instead of giving up on the whole file.
 *
 *   The file is pre-allocated contiguous at boot. Without that, every time the
 *   file grows the driver updates the FAT — an unbounded-latency write to a
 *   different part of the card, at exactly the wrong moment.
 *
 *   A 1 Hz plain-CSV mirror is written alongside for eyeballing and for
 *   feeding the GCS post-flight. It is cheap (about 200 B/s) and completely
 *   separate, so a corrupted text file can never take the binary log with it.
 * ========================================================================== */
#pragma once
#include <Arduino.h>
#include "Config.h"
#include "Sensors.h"

/* 128 bytes exactly. Do not reorder or resize without bumping the magic —
 * a decoder written against the old layout would read plausible garbage. */
struct __attribute__((packed)) LogRecord {
    uint32_t magic;        /*   0  0x564C4752 'VRLG'                        */
    uint32_t t_ms;         /*   4  millis() at sample                       */
    uint32_t pkt;          /*   8  downlink packet counter                  */
    uint32_t utc_epoch;    /*  12  DS3231, seconds since 2000-01-01         */
    float    pres_pa;      /*  16                                           */
    float    temp_c;       /*  20                                           */
    float    alt_agl_m;    /*  24                                           */
    float    vspeed_ms;    /*  28                                           */
    float    bus_v;        /*  32                                           */
    int32_t  lat_e7;       /*  36  degrees x 1e7 — 1.1 cm, exact in int32   */
    int32_t  lon_e7;       /*  40                                           */
    float    gps_alt_m;    /*  44                                           */
    float    gnd_speed_ms; /*  48                                           */
    uint32_t gnss_time;    /*  52  hhmmsscc                                 */
    float    hdop;         /*  56                                           */
    float    vdop;         /*  60                                           */
    float    geoid_m;      /*  64                                           */
    float    ax, ay, az;   /*  68  g                                        */
    float    gx, gy, gz;   /*  80  deg/s                                    */
    float    mx, my, mz;   /*  92  uT                                       */
    float    spin_dps;     /* 104                                           */
    uint16_t sats;         /* 108                                           */
    uint8_t  navic;        /* 110                                           */
    uint8_t  fix_q;        /* 111                                           */
    uint8_t  gnss_sys;     /* 112                                           */
    uint8_t  state;        /* 113                                           */
    uint8_t  flags;        /* 114                                           */
    uint8_t  sd_pct;       /* 115                                           */
    int16_t  rssi_x10;     /* 116  last uplink RSSI                         */
    uint16_t i2c_faults;   /* 118                                           */
    uint8_t  best_snr;     /* 120  best GNSS satellite SNR, dB-Hz           */
    uint8_t  rsv[5];       /* 121                                           */
    uint16_t crc16;        /* 126  over bytes 0..125                        */
};                         /* 128                                           */
static_assert(sizeof(LogRecord) == LOG_RECORD_BYTES,
              "LogRecord must be exactly 128 bytes for block alignment");

class DataLogger {
public:
    bool begin();                       /* returns false if no card         */
    bool ok() const { return ok_; }

    /* Append one sample. Buffers into a 512-byte page and only touches the
     * card when the page is full. Never blocks on anything else. */
    void logSample(const SensorData& d, uint8_t state, uint8_t flags,
                   uint32_t pkt, int16_t rssi_x10);

    /* Human-readable mirror at LOG_CSV_RATE_HZ (now the full 20 Hz sample
     * rate). Writes EVERY schema field, in vritra_gcs.py SCHEMA order, so the
     * file drops straight into the same analysis tools as the graded CSV.
     * Sensors that are not fitted on this build are written as "-", never as
     * 0 — "not present" and "measured zero" must stay distinguishable in the
     * post-flight record exactly as they do on the ground station. */
    void logCsv(const SensorData& d, uint8_t state, uint32_t pkt,
                uint8_t flags, uint32_t missionS);

    /* Mandated flush. Call every loop; it self-paces to LOG_SYNC_MS and
     * forces the card controller to commit to physical flash, which is what
     * guarantees survival of a power loss during deployment shock. */
    void service();

    /* ---- GNSS aiding ---------------------------------------------------- *
     * Last known position lives on the card, accurate UTC lives in the
     * DS3231's battery-backed registers. Fed back to the receiver at boot so a
     * cold start becomes a warm one. See the GNSS AIDING block in Config.h. */
    void saveGPSStateToSD(const SensorData& d);

    void     flushNow();
    uint8_t  usedPct()  const { return usedPct_; }
    uint32_t records()  const { return recCount_; }
    uint32_t writeErrors() const { return writeErr_; }
    const char* binName() const { return binName_; }

private:
    /* ---- staged, non-blocking aid injection ------------------------------
     * The old version fired all three commands back to back with delay(20)
     * between them, inside service(), which runs in the flight loop. This
     * walks the same commands out one per pass with millis() pacing instead —
     * the L89 drops commands that arrive faster than it can consume them, so
     * the pacing is required, but it must not be bought with delay(). */
    enum AidStage : uint8_t {
        AID_IDLE = 0,   /* nothing to do, or already done      */
        AID_READ,       /* waiting for Sensors.ready()          */
        AID_TIME,       /* send the time aid                    */
        AID_POS,        /* send the position aid                */
        AID_EASY,       /* re-assert orbit prediction           */
        AID_DONE
    };
    AidStage aidStage_ = AID_READ;
    uint32_t aidAt_    = 0;
    bool     aidHave_  = false;      /* a usable stored position was loaded  */
    double   aidLat_ = 0, aidLon_ = 0;
    float    aidAlt_ = 0;
    uint32_t aidSavedEpoch_ = 0;     /* utcEpoch when the file was written   */
    uint32_t lastAidSaveMs_ = 0;

    void aidService();
    bool aidLoad();                  /* read AID_FILE into aidLat_/aidLon_   */

    bool     ok_        = false;
    uint8_t  page_[LOG_BLOCK_BYTES];
    uint8_t  pageFill_  = 0;            /* records currently in the page    */
    uint32_t recCount_  = 0;
    uint32_t bytesOut_  = 0;
    uint32_t writeErr_  = 0;
    uint32_t lastSyncMs_ = 0;
    uint32_t lastCsvMs_  = 0;
    uint8_t  usedPct_   = 0;
    uint64_t capBytes_  = SD_CAPACITY_BYTES;   /* real capacity, read at boot */
    uint64_t baseUsed_  = 0;                   /* bytes in use before this run */
    char     binName_[24];
    char     csvName_[24];
};

extern DataLogger Logger;
