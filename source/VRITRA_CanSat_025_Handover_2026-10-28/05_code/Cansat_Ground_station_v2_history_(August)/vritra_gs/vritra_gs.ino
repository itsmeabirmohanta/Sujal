/* =============================================================================
 * VRITRA GROUND STATION GATEWAY  ·  2026-INSPACe-CAN-7USAT-025
 * ESP32 + Semtech LR1121  ·  firmware 6.1.0
 * =============================================================================
 * Pairs with vritra_fc (Teensy 4.1).
 *
 * DOWNLINK
 *   The CanSat transmits plain ASCII comma-separated fields terminated by a
 *   carriage return, exactly as guideline §6.3.i mandates:
 *     <TEAM_ID>,<TIME_S>,<PKT>,<ALT_M>,<PRES_PA>,<TEMP_C>,<BUS_V>,<GNSS_TIME>,
 *     <LAT>,<LON>,<GPS_ALT>,<SATS>,<ACC_X>,<ACC_Y>,<ACC_Z>,<SPIN>,<STATE>,
 *     <FLAGS>,<ATT_ROLL>,<ATT_PITCH>,<opt>
 *   Positions 0..16 are the mandated set in the mandated order and are passed
 *   straight through. This gateway's remaining job is small and honest:
 *     - expand <FLAGS> into GCS fields 67 / 68 / 71
 *     - place the ONE rotating optional value at its schema index, choosing the
 *       slot as (PKT - 1) % B36_OPT_N — wire layout 2 carries no F<n> tag
 *     - derive UTC (75) from GNSS_TIME (6)
 *     - inject the three values only a receiver can know: RSSI, SNR, frequency
 *     - emit the full N_FIELDS-column positional CSV vritra_gcs.py parses
 *       (83 as of this revision: 81 mandated+optional, ATT_ROLL/ATT_PITCH at
 *       81-82 -- see the N_FIELDS comment below)
 *   Every field with no fitted sensor stays an EMPTY cell, so the GCS can still
 *   tell "the sensor reported zero" from "there is no sensor".
 *   ** vritra_gcs.py's SCHEMA must stay in step with N_FIELDS below. **
 *
 * UPLINK
 *   "CMD_XXX [arg]" on USB from the GCS becomes
 *     <TEAM_ID>,C,<id>,<arg>,<seq>
 *   transmitted 25 ms after each downlink so it lands inside the CanSat's RX
 *   window, and repeated UL_REPEATS times so one closed window cannot swallow
 *   it. The CanSat de-duplicates on <seq>, so a repeat never executes twice.
 *   With the CanSat silent (guideline §6.1.v boot state, or after CMD_TX_OFF)
 *   there is no downlink to key off, so repeats fall back to a free-running
 *   timer — that is the path that carries CMD_TX_ON to a silent vehicle, and
 *   the path used to start telemetry before launch per §6.1.ii.
 *
 * WIRING:
 *   NSS=5  SCK=18  MOSI=23  MISO=19  RST=14  BUSY=27  IRQ(DIO9)=26
 *   Antenna on ANT2 (LORA_ANT, sub-GHz). NOT ANT1 (2.4 GHz).
 * ========================================================================== */
#include <RadioLib.h>
#include <SPI.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

/* ---- ORGANISER-ASSIGNED RF PLAN, team 025 ------------------------------- *
 * Source: "CAN-7USAT - Frequency allocation plan for teams", row 025.
 * Must match vritra_fc/Config.h exactly. Never transmit on a frequency not
 * assigned to this team; switch to backup only on Mission Control's call. */
#define FREQ_MAIN_MHZ    867.00f     /* 868 Main   */
#define FREQ_BACKUP_MHZ  865.75f     /* 868 Backup */
#define LORA_SF          10
#define LORA_BW_KHZ      125.0f      /* mandatory  */
#define LORA_CR          5           /* 4/5        */
#define LORA_SYNC        0x98        /* 0x80 + team id */
#define LORA_PREAMBLE    8
#define TCXO_V           1.6f
#define TX_USE_HP_PA     true
/* The ground station is not on a pad and is the fixed end of the link, but it
 * shares the assigned channel, so it transmits at the same in-flight level
 * rather than full power. Antenna at this end is currently the RadioMaster
 * dual-band whip (3.58 dBi @ 900 MHz) on ANT2 — the PDR's 11 dBi Yagi is not
 * attached. Uplink budget is comfortable either way; see the link-budget
 * comment in vritra_fc/Config.h for the current numbers. */
/* Reduced to 10 dBm to prevent the 3.3V rail from sagging/browning out the
 * LR1121 during TX. 10 dBm is still plenty for a ground station uplink. */
#define TX_POWER_DBM     10

#define TEAM_ID_AIR "025"                          /* short form on the link  */
#define TEAM_ID_GCS "2026-INSPACe-CAN-7USAT-025"   /* MUST equal vritra_gcs.py
                                                      TEAM_ID exactly          */
/* 83 = len(SCHEMA) in vritra_gcs.py.
 * It MUST equal that length exactly or every emitted row is the wrong width
 * and the GCS parser silently misaligns — it maps columns positionally, by
 * enumerate(ALL_FIELDS), with no name lookup and no error on a short row.
 *
 * WAS 90, THEN 87, NOW 83. Two cuts, both of data that was duplicated or never
 * looked at:
 *   90 -> 87  AX/AY/AZ left SCHEMA — they were ACC_X/Y/Z in g, derived by this
 *             gateway and never transmitted. Every index above 22 shifted down
 *             by three.
 *   87 -> 83  ATT_YAW and QUAT_X/Y/Z left the WIRE and SCHEMA together (wire
 *             layout 3). 12 B of an 86 B packet carrying attitude the ground
 *             station displayed nowhere: no panel read ATT_YAW, and
 *             _update_instruments() discarded the quaternion outright with
 *             set_quaternion(None), rendering the 3-D model from its own
 *             derived Euler angles instead. Both blocks sat at the END of
 *             SCHEMA, so this cut truncated rather than shifting anything.
 *
 * ATT_ROLL and ATT_PITCH stayed at 81-82, and the reason is compliance rather
 * than use: guideline table row 13 defines <ACCELEROMETER DATA> as
 * "acceleration and roll & pitch parameters", which makes them mandated content
 * for the graded CSV. Yaw and quaternions are named nowhere in that table.
 * All four removed values are still recorded on the vehicle's SD card at 20 Hz.
 *
 * Silent misalignment is exactly the hazard this comment warns about, so it is
 * checked mechanically rather than by eye: the verifier reads SCHEMA out of
 * vritra_gcs.py and cross-checks N_FIELDS, GCS_ATT_MAP, B36_OPT_MAP and every
 * literal rec[] index in this file against it BY NAME. Re-run it after any
 * schema edit. */
#define N_FIELDS         83
#define GCS_ATT_ROLL     81
static const int GCS_ATT_MAP[2]  = { 81, 82 };
/* MUST equal B36_OPT_N in Base36.h and N_FRAMES in vritra_fc/Config.h.
 * Reduced 19 -> 16: AX/AY/AZ left the rotation because they duplicate the
 * mandated ACC_X/Y/Z. They have since left the schema altogether — the ground
 * station converts m/s^2 to g itself. */
#define N_FRAMES         16
#define DL_PACKET_MAX    140
#define UL_PACKET_MAX    48
#define UPLINK_DELAY_MS  25
#define UL_REPEATS       6
#define UL_BLIND_MS      1200UL

#define PIN_CS    5
#define PIN_IRQ   26
#define PIN_RST   14
#define PIN_BUSY  27
#define PIN_SCK   18
#define PIN_MOSI  23
#define PIN_MISO  19

/* ---- command ids — must match Config.h enum CmdId ------------------------ *
 * IDS 4-8 ARE RETIRED AND THE GAP IS INTENTIONAL. ARM, DISARM, FORCE_EJECT,
 * FORCE_SEC_EJ and BRAKE_DEPLOY are gone from both ends: this vehicle carries
 * no ejection charge, no secondary charge and no drag brake, and arming is now
 * implicit in CMD_TX_ON (guideline §6.1.v already makes that the last operator
 * action before flight).
 *
 * THE NUMBERS STAY RESERVED. The uplink carries the command as an integer, so
 * closing the gap would silently redefine every id above it — a gateway flashed
 * from one commit talking to a flight computer flashed from another would
 * execute the WRONG command rather than failing cleanly. CMD_BEACON_ON (9)
 * would arrive as CMD_FORCE_SEC_EJ. Never reuse 4-8; new commands take 19+.
 * This enum must stay identical to vritra_fc/Config.h enum CmdId. */
enum : uint8_t {
    CMD_NONE=0, CMD_TX_ON=1, CMD_TX_OFF=2, CMD_CALIBRATE=3,
    /* 4..8 RETIRED — do not reuse */
    CMD_BEACON_ON=9, CMD_BEACON_OFF=10, CMD_FAILOVER=11, CMD_RESET=12,
    CMD_GYRO_ON=13, CMD_GYRO_OFF=14, CMD_GYRO_SPD=15, CMD_MAG_CAL=16,
    CMD_GPS_RESET=17, CMD_NEW_MISSION=18,
};
struct CmdName { const char* s; uint8_t id; };
/* Must stay in step with COMMANDS in vritra_gcs.py — that dict is what the
 * operator's drop-down offers, this table is what the gateway will actually
 * accept off USB. A command present there but missing here is queued by the
 * GCS, silently ignored here, and never reaches the vehicle. */
static const CmdName CMD_TABLE[] = {
    {"TX_ON",CMD_TX_ON},{"TX_OFF",CMD_TX_OFF},{"CALIBRATE",CMD_CALIBRATE},
    {"BEACON_ON",CMD_BEACON_ON},{"BEACON_OFF",CMD_BEACON_OFF},
    {"FAILOVER",CMD_FAILOVER},{"RESET",CMD_RESET},{"GYRO_ON",CMD_GYRO_ON},
    {"GYRO_OFF",CMD_GYRO_OFF},{"GYRO_SPD",CMD_GYRO_SPD},{"MAG_CAL",CMD_MAG_CAL},
    {"GPS_RESET",CMD_GPS_RESET},{"NEW_MISSION",CMD_NEW_MISSION},
};

/* The scale table, the frame->schema map and the base-36 codec all live in
 * Base36.h, which is byte-identical to the copy in vritra_fc/. Both sketches
 * print b36SchemaSum() at boot; if the two numbers differ the copies have
 * drifted and every decoded field is silently wrong. */
#include "Base36.h"

/* ---- RF switch table (LR1121-internal DIOs, not MCU pins) ---------------- */
static const uint32_t SW_PINS[] = {
    RADIOLIB_LR11X0_DIO5, RADIOLIB_LR11X0_DIO6,
    RADIOLIB_NC, RADIOLIB_NC, RADIOLIB_NC
};
static const Module::RfSwitchMode_t SW_TABLE[] = {
    { LR11x0::MODE_STBY,  { LOW,  LOW  } },
    { LR11x0::MODE_RX,    { HIGH, LOW  } },
    { LR11x0::MODE_TX,    { HIGH, HIGH } },
    { LR11x0::MODE_TX_HP, { LOW,  HIGH } },
    { LR11x0::MODE_TX_HF, { LOW,  LOW  } },
    { LR11x0::MODE_GNSS,  { LOW,  LOW  } },
    { LR11x0::MODE_WIFI,  { LOW,  LOW  } },
    END_OF_MODE_TABLE,
};

LR1121 radio = new Module(PIN_CS, PIN_IRQ, PIN_RST, PIN_BUSY, SPI);

/* Only ONE IRQ action is ever registered on DIO9 (setPacketReceivedAction).
 * A second registration (setPacketSentAction) was tried and rejected: the
 * LR11x0 driver attaches a single interrupt vector to the pin, and stacking a
 * second attachInterrupt() on top of it corrupts that vector — the symptom
 * was a Guru Meditation "IllegalInstruction" panic with PC landing inside the
 * tiny ISR body, right after the first successful TX. TX completion is
 * instead detected by polling PIN_IRQ directly (same signal RadioLib's own
 * blocking transmit() polls), with delay(1) in the loop so the ESP32
 * interrupt watchdog stays fed. */
static volatile bool pktFlag  = false;
static void IRAM_ATTR onPacket()  { pktFlag = true; }

static bool     useBackup = false;
static uint32_t rxCount = 0, rejected = 0, missed = 0;
static uint32_t lastPkt = 0;
static bool     havePkt = false;

/* Persistent N_FIELDS-wide record. An empty string means "never received",
 * which the GCS renders as an em-dash. */
static String rec[N_FIELDS];

/* pending uplink */
static uint8_t  ulCmd = CMD_NONE;
static int16_t  ulArg = 0;
static uint8_t  ulSeq = 0;
static uint8_t  ulLeft = 0;
static uint32_t ulNextBlindMs = 0;

static String usbBuf;

/* RadioLib returns bare integers; naming them at the point of failure saves
 * looking up TypeDef.h while a range test waits. Mirrors rfErrName() in the
 * flight computer's Radio.cpp — keep the two in step. */
static const char* rfErrName(int16_t c) {
    switch (c) {
    case RADIOLIB_ERR_NONE:                 return "NONE";
    case RADIOLIB_ERR_CHIP_NOT_FOUND:       return "CHIP_NOT_FOUND (no plausible reply on MISO)";
    case RADIOLIB_ERR_SPI_CMD_TIMEOUT:      return "SPI_CMD_TIMEOUT (BUSY never released)";
    case RADIOLIB_ERR_SPI_CMD_INVALID:      return "SPI_CMD_INVALID (chip rejected the opcode)";
    case RADIOLIB_ERR_SPI_CMD_FAILED:       return "SPI_CMD_FAILED (status byte was not OK)";
    case RADIOLIB_ERR_INVALID_FREQUENCY:    return "INVALID_FREQUENCY";
    case RADIOLIB_ERR_INVALID_OUTPUT_POWER: return "INVALID_OUTPUT_POWER";
    case RADIOLIB_ERR_INVALID_BANDWIDTH:    return "INVALID_BANDWIDTH";
    case RADIOLIB_ERR_INVALID_SPREADING_FACTOR: return "INVALID_SPREADING_FACTOR";
    case -20:                               return "WRONG_MODEM (SPI glitch or chip reset)";
    default:                                return "see RadioLib TypeDef.h";
    }
}

/* ---- radio init ---------------------------------------------------------- */
static bool radioInit() {
    /* ESP32 takes its SPI pins as explicit arguments and routes them through
     * the IO mux, so the Teensy-side failure that cost this project a day
     * CANNOT happen here: there is no default MISO pin to be left behind. If
     * this board reports -707, the cause is the RF switch table, TCXO_V, or a
     * genuinely dead module — not pin routing. (Flight side: Radio.cpp fact 5.) */
    SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);
    radio.setRfSwitchTable(SW_PINS, SW_TABLE);
    float f = useBackup ? FREQ_BACKUP_MHZ : FREQ_MAIN_MHZ;
    int16_t st = radio.begin(f, LORA_BW_KHZ, LORA_SF, LORA_CR,
                             LORA_SYNC, TX_POWER_DBM, LORA_PREAMBLE, TCXO_V);
    if (st != RADIOLIB_ERR_NONE) {
        Serial.printf("// [RF] begin FAILED code=%d — %s\n", st, rfErrName(st));
        return false;
    }
    radio.setRfSwitchTable(SW_PINS, SW_TABLE);   /* begin() cleared it */
    radio.setCRC(2);
    radio.setRegulatorDCDC();
    radio.setSyncWord(LORA_SYNC);
    radio.setOutputPower(TX_POWER_DBM, TX_USE_HP_PA);   /* HP PA — mandatory */

    /* IMAGE CALIBRATION AT 863-870 MHz, and BOOSTED RX GAIN.
     * The LR1121 calibrates image rejection at 915 MHz during POR and the
     * default band is 902-928 MHz; the User Manual (2.1.3) requires a
     * re-calibration for any frequency change > 10 MHz. We run at 867.00 MHz,
     * which is 35-61 MHz away. With a TCXO fitted the POR calibrations are
     * skipped entirely and the host must re-launch them.
     * The ground station matters MORE than the vehicle here: it is the end of
     * the link doing the hard work of hearing a +14 dBm transmitter 2 km away,
     * so every dB of its sensitivity is range. Datasheet Table 3-8 quotes all
     * sensitivity figures with RxBoosted = 1, which is not the default.
     * Keep this in step with hwInit() in ../vritra_fc/Radio.cpp. */
    int16_t cal = radio.calibrateImageRejection(863.0f, 870.0f);
    if (cal != RADIOLIB_ERR_NONE)
        Serial.printf("// [RF] WARNING image calibration failed code=%d — %s\n",
                      cal, rfErrName(cal));
    int16_t rxb = radio.setRxBoostedGainMode(true);
    if (rxb != RADIOLIB_ERR_NONE)
        Serial.printf("// [RF] WARNING RxBoosted enable failed code=%d\n", rxb);

    LR11x0VersionInfo_t vi;
    if (radio.getVersionInfo(&vi) == RADIOLIB_ERR_NONE)
        Serial.printf("// [RF] HW=0x%02X DEV=0x%02X FW=%d.%d  %.2f MHz SF%d BW%.0fk "
                      "sync 0x%02X +%d dBm HP\n",
                      vi.hardware, vi.device, vi.fwMajor, vi.fwMinor,
                      f, LORA_SF, LORA_BW_KHZ, LORA_SYNC, TX_POWER_DBM);
    radio.setPacketReceivedAction(onPacket);
    return true;
}

/* ---- USB -> queued command ---------------------------------------------- */
static void pollUsb() {
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c != '\r' && c != '\n') {
            if (usbBuf.length() < 64) usbBuf += c;
            continue;
        }
        if (!usbBuf.length()) continue;
        String line = usbBuf; usbBuf = ""; line.trim();

        if (line == "FAILOVER_LOCAL") {
            useBackup = !useBackup;
            radioInit();
            radio.startReceive();
            Serial.printf("// [UP] local failover -> %.2f MHz\n",
                          useBackup ? FREQ_BACKUP_MHZ : FREQ_MAIN_MHZ);
            continue;
        }
        if (!line.startsWith("CMD_")) continue;

        String body = line.substring(4);
        int16_t arg = 0;
        int sp = body.indexOf(' ');
        if (sp < 0) sp = body.indexOf(',');
        if (sp > 0) { arg = (int16_t)body.substring(sp + 1).toInt(); body = body.substring(0, sp); }

        for (auto& m : CMD_TABLE) {
            if (body == m.s) {
                ulCmd  = m.id;
                ulArg  = arg;
                ulSeq++;                        /* new command -> new sequence */
                ulLeft = UL_REPEATS;
                ulNextBlindMs = millis();
                Serial.printf("// [UP] queued %s arg=%d seq=%u (x%u)\n",
                              m.s, arg, ulSeq, UL_REPEATS);
                Serial.flush();
                break;
            }
        }
    }
}

static void sendUplink() {
    if (!ulLeft || ulCmd == CMD_NONE) return;
    char b[UL_PACKET_MAX];
    int n = snprintf(b, sizeof(b), "%s,C,%u,%d,%u\n",
                     TEAM_ID_AIR, ulCmd, ulArg, ulSeq);
    if (n <= 0 || n >= (int)sizeof(b)) { ulLeft = 0; ulCmd = CMD_NONE; return; }

    /* No manual standby() here. startTransmit() already transitions the
     * LR11x0 out of RX and into TX internally — an extra standby() call was
     * tried and rejected: it left the radio in a state where every TX after
     * the first failed with code=-1 (generic SPI/status failure). The
     * confirmed-working ping-test sketches never call standby() either; they
     * just call transmit()/startTransmit() directly on top of whatever state
     * the radio was already in, and that is reliable.
     *
     * Non-blocking TX: startTransmit() loads the FIFO and fires the PA, then
     * returns immediately. Completion is detected by polling PIN_IRQ (DIO9)
     * directly rather than a second attachInterrupt() — see the note by
     * pktFlag's declaration for why a second ISR is not used here. delay(1)
     * inside the wait loop yields to FreeRTOS every millisecond, which is
     * what prevents the ESP32 interrupt watchdog from firing (it trips when
     * the FreeRTOS tick is starved for > ~300 ms, and SF10 airtime at
     * 125 kHz BW is ~289 ms). */
    int16_t st = radio.startTransmit((uint8_t*)b, (size_t)n);
    if (st != RADIOLIB_ERR_NONE) {
        Serial.printf("// [UP] startTransmit FAILED code=%d (%s)\n",
                      st, rfErrName(st));
        /* The chip's internal state is unknown. Force it back through a
         * clean standby → startReceive sequence. Without this, every
         * subsequent startTransmit() also returns -1 and the entire
         * retry window is lost. */
        radio.standby();
        delay(2);
        radio.clearIrqFlags(0xFFFFFFFFUL);
        radio.startReceive();
    } else {
        uint32_t t0 = millis();
        while (digitalRead(PIN_IRQ) == LOW) {
            delay(1);
            if (millis() - t0 > 2000) { st = RADIOLIB_ERR_TX_TIMEOUT; break; }
        }
        if (st == RADIOLIB_ERR_NONE) {
            st = radio.finishTransmit();
            if (st != RADIOLIB_ERR_NONE)
                Serial.printf("// [UP] finishTransmit FAILED code=%d (%s)\n",
                              st, rfErrName(st));
        } else {
            Serial.printf("// [UP] TX timeout after %lu ms (PIN_IRQ never asserted)\n",
                          (unsigned long)(millis() - t0));
        }
    }

    /* onPacket() is still attached to the physical DIO9 pin (via
     * setPacketReceivedAction in radioInit). DIO9 also pulses on TX-done,
     * which fires that same pin-change ISR and sets pktFlag = true even
     * though nothing was received — clear it so the next loop() pass does
     * not misread our own TX completion as an incoming downlink. */
    pktFlag = false;

    /* HARDWARE NOTE — root cause of the intermittent "startReceive FAILED
     * code=-20" seen after TX, on some boots but not others:
     * The HP PA pulls ~300 mA during transmit. On USB power the 3.3V rail
     * sags under that load and has NOT fully recovered by the time
     * startReceive() issues its SPI write a few ms later — that write is
     * heavier than the simple status read finishTransmit() just did, so it
     * lands mid-sag and comes back corrupted. This is a power-supply
     * problem, not a logic bug: it will keep recurring, at a rate that
     * depends on cable resistance and USB port quality, until a 220-470 uF
     * low-ESR electrolytic (+ a 100 nF ceramic right at the pins) is
     * soldered across the LR1121's 3.3V/GND. The retry logic below makes
     * the firmware resilient to it, not immune. */
    delay(30);   /* let the rail settle before the heavier SET_RX write */

    int16_t rxst = radio.startReceive();
    if (rxst != RADIOLIB_ERR_NONE) {
        /* One cheap retry first — usually enough once the rail has had a
         * little longer. Only fall back to a full radioInit() (slow: it
         * re-runs SPI.begin(), image calibration, etc., and would eat into
         * the next uplink's timing budget) if the retry also fails. */
        delay(20);
        radio.standby(); /* Force state machine reset just in case */
        rxst = radio.startReceive();
        if (rxst != RADIOLIB_ERR_NONE) {
            Serial.printf("// [UP] startReceive FAILED code=%d (%s) — re-initing radio\n",
                          rxst, rfErrName(rxst));
            radioInit();
            radio.startReceive();
        }
    }

    ulLeft--;
    ulNextBlindMs = millis() + UL_BLIND_MS;
    Serial.printf("// [UP] tx cmd=%u seq=%u left=%u %s\n",
                  ulCmd, ulSeq, ulLeft, st == RADIOLIB_ERR_NONE ? "OK" : "FAIL");

    if (!ulLeft) {
        /* THE GATEWAY FOLLOWS THE VEHICLE ACROSS A FAILOVER, AUTOMATICALLY.
         *
         * CMD_FAILOVER moves the CanSat from 867.00 to 865.75 MHz. Until now
         * the gateway stayed where it was and the operator had to remember to
         * type FAILOVER_LOCAL as a separate step. Forget it — or reboot the
         * gateway before typing it — and the two ends are on different
         * channels with no way back: every command that could repair the
         * situation, CMD_FAILOVER included, has to travel over the link that
         * no longer exists.
         *
         * Switching here, after the last repeat has actually gone out, closes
         * that. The vehicle has already heard the command by this point (or it
         * never will), so there is nothing left for us to say on the old
         * channel. Both frequencies are assigned to team 025 by the allocation
         * plan, so this never puts either end outside our allocation.
         *
         * The flight computer independently reverts to main if it hears
         * nothing on backup within FAILOVER_PROOF_MS, so a failover that fails
         * at either end converges back to a working link rather than silence.
         * The two mechanisms are deliberately independent: this one makes the
         * normal case work, that one makes the abnormal case survivable. */
        if (ulCmd == CMD_FAILOVER) {
            useBackup = !useBackup;
            Serial.printf("// [UP] following CanSat to %.2f MHz\n",
                          useBackup ? FREQ_BACKUP_MHZ : FREQ_MAIN_MHZ);
            radioInit();
            radio.startReceive();
        }
        ulCmd = CMD_NONE;
    }
}

/* ---- ASCII downlink -> N_FIELDS-wide record ------------------------------ */
static bool ingest(char* line, float rssi, float snr) {
    /* Trim the mandated carriage return / newline. */
    int len = (int)strlen(line);
    while (len > 0 && (line[len-1] == '\r' || line[len-1] == '\n' ||
                       line[len-1] == ' ')) line[--len] = '\0';
    /* MINIMUM LENGTH IS A NOISE FILTER, NOT A VALIDITY TEST — and it was set
     * too high. It used to be 40, which REJECTS A LEGAL PACKET: a vehicle whose
     * barometer, IMU, magnetometer and GNSS are all down still transmits the
     * mandated frame, but with every absent field an empty cell it comes to
     * 39 B and was thrown away as too short. That is exactly the case where the
     * operator most needs what does still arrive — state, bus voltage, packet
     * count and the FLAGS byte that says WHICH subsystem died.
     *
     * The real structural gate is `nf < 18` below (team id + the 16 mandated
     * fields + FLAGS), which no legal packet can fail and no fragment can pass.
     * This length check only needs to be cheap enough to discard obvious noise
     * before the split loop runs. Under wire layout 3 the always-written part is
     * "025" plus the 19 separators that precede fields 1..19 = 22 B, and six of
     * those fields (TIME_S, PKT, BUS_V, SATS, STATE, FLAGS) can never be empty,
     * so the true floor is 28 B. 20 sits under that with room for a future
     * layout change and still rejects a stray byte or a truncated preamble.
     *
     * KEEP THIS BELOW THE FLOOR WHENEVER THE LAYOUT CHANGES. It was 40 under
     * layout 2, which already rejected a legal 39 B all-sensors-dead packet —
     * exactly the case where the operator most needs whatever still arrives. */
    if (len < 20) return false;

    /* Split. Guideline §6.ii's NETID/PANID requirement exists so another
     * team's traffic never enters our pipeline — check the id first. */
    char* f[32];
    int nf = 0;
    f[nf++] = line;
    for (char* p = line; *p && nf < 32; p++)
        if (*p == ',') { *p = '\0'; f[nf++] = p + 1; }

    /* The link carries the short team number to save a quarter of the 99-byte
     * airtime budget; the full identifier is restored below, before the CSV
     * that the judges examine (§6.3.ii). Both spellings are accepted so the
     * gateway keeps working if the flight side is switched to the long form. */
    if (nf < 18) return false;
    if (strcmp(f[0], TEAM_ID_AIR) != 0 && strcmp(f[0], TEAM_ID_GCS) != 0)
        return false;

    /* Wire positions 1..16 are the mandated fields in the GCS's own schema
     * order; position 17 is the status FLAGS byte. Every one is a base-36
     * integer of (value x scale) and is decoded straight back to the plain
     * decimal text the GCS expects, at the decimal places the scale defines.
     * A malformed or empty token yields an EMPTY cell, never a fabricated
     * number — that is what keeps "reported zero" separable from "no fix". */
    char cell[28];
    for (int i = 0; i < 16; i++) {
        b36Field(f[1 + i], B36_CORE[i], cell, sizeof(cell));
        rec[i] = String(cell);
    }

    /* ---- AX / AY / AZ ARE GONE FROM THE SCHEMA ENTIRELY -------------------
     * This gateway used to synthesise schema 20/21/22 (AX/AY/AZ, in g) here by
     * dividing schema 11/12/13 (ACC_X/Y/Z, m/s^2) by 9.80665. That removed the
     * duplication from the WIRE — they had been costing three rotation slots —
     * but left it in place one layer up: two schema columns, two CSV columns
     * and two sets of limits, all describing one measurement.
     *
     * The ground station now converts to g itself, once, in Physics, and
     * publishes AX/AY/AZ as DERIVED values. That is where a pure unit change
     * belongs — it needs no information this gateway has and nothing the
     * vehicle sends. Schema drops 90 -> 87 fields and every index above 22
     * shifts down by three; see the verifier note below.
     *
     * DO NOT reintroduce a rec[] write here for AX/AY/AZ. There is no slot
     * for it any more, and adding one would write into GX. */

    /* ---- UTC (75) is DERIVED from GNSS_TIME (6) whenever there is a fix ---
     * Same duplication as AX/AY/AZ above, one level less obvious because the
     * two carry the same instant in different encodings rather than the same
     * quantity in different units:
     *
     *   core  6  GNSS_TIME  hhmmss, in EVERY packet, straight off the GGA
     *   opt  F15 UTC        seconds since midnight, once every N_FRAMES
     *
     * and the flight computer keeps the DS3231 disciplined from GNSS, so once
     * there is a fix the two ARE the same clock. Rebuilding UTC here costs
     * nothing and moves it from a 16 s refresh to every single packet.
     *
     * F15 is deliberately still decoded below and still wins: before first fix
     * GNSS_TIME is empty and the RTC is the only clock on the vehicle, which is
     * exactly when an operator wants to see that the DS3231 survived the power
     * cycle. After a fix both paths produce the same string, so the later
     * overwrite is a no-op. Deriving does NOT free the rotation slot — it makes
     * the slot redundant only while the fix holds. */
    if (rec[6].length()) {
        long v = rec[6].toInt();                 /* hhmmss */
        if (v >= 0 && v <= 235959) {
            const int hh = (int)(v / 10000);
            const int mm = (int)((v / 100) % 100);
            const int ss = (int)(v % 100);
            if (hh < 24 && mm < 60 && ss < 60) {
                char u[12];
                snprintf(u, sizeof(u), "%02d:%02d:%02d", hh, mm, ss);
                rec[75] = String(u);                       /* UTC */
            }
        }
    }

    /* Packet-count continuity, for a real PER rather than a guessed one.
     * Read from the decoded value so a corrupt token cannot inflate `missed`. */
    if (rec[1].length()) {
        uint32_t pkt = (uint32_t)rec[1].toInt();
        if (havePkt && pkt > lastPkt + 1) missed += (pkt - lastPkt - 1);
        lastPkt = pkt; havePkt = true;
    }

    /* <FLAGS> -> the three status bits that ride in every packet. */
    int32_t flg;
    if (nf > 17 && b36dec(f[17], flg)) {
        rec[67] = (flg & 0x10) ? "1" : "0";     /* MCU_ACTIVE */
        rec[68] = (flg & 0x08) ? "1" : "0";     /* WDOG_OK    */
        rec[71] = (flg & 0x04) ? "1" : "0";     /* BEACON_ON  */
    }

    /* The always-on raw gyro triple was REMOVED from the wire: core+gyro+the
     * attitude block came to 112 B, breaching the 99 B SF10 wall. GX/GY/GZ
     * still arrive via the rotating optional slot, and SPIN_RATE is in the
     * mandated core every packet.
     * See buildPacket() in ../vritra_fc/vritra_fc.ino. */
    /* ATTITUDE — wire positions 18,19 (ROLL, PITCH in degrees).
     * Produced on the flight computer by atan2 on the accelerometer, anchored
     * to gravity so it cannot drift. See Sensors.cpp for the rationale.
     * Scale is B36_ATT (div 10, dp 1). This used to be written as B36_OPT[5],
     * borrowing the GX row from the rotation table because it happened to have
     * the same divisor — a landmine, since that table is a rotation SCHEDULE
     * and reordering it silently re-scales attitude at both ends. It has now
     * been reordered (GX moved 5 -> 2), so the borrowed index would be reading
     * MX. Named constant, no coupling.
     *
     * WIRE LAYOUT 3 REMOVED ATT_YAW (was position 20) AND QUAT_X/Y/Z (21,22,23)
     * — 12 B per packet of attitude the ground station never displayed. Roll
     * and pitch stay because guideline row 13 makes them mandated content. */
    for (int i = 0; i < 2; i++) {
        if (nf <= 18 + i) break;
        b36Field(f[18 + i], B36_ATT, cell, sizeof(cell));
        rec[GCS_ATT_MAP[i]] = String(cell);
    }

    /* One rotating optional field, present only when the mandated core AND the
     * attitude block left room for it. Its absence is normal, not an error: on
     * a full-deflection packet the graded core plus attitude is sent and this
     * is the only thing dropped.
     * NOTE the index. Wire layout after FLAGS is:
     *   18,19 = ATT_ROLL, ATT_PITCH (comma always reserved)
     *   20    = the rotating value itself
     *
     * WIRE LAYOUT 2 — THE ",F<n>" TAG IS GONE. It used to sit at position 24
     * with the value at 25, costing 3-4 B of every packet to name a slot the
     * receiver could always have computed: the flight computer advances the
     * rotation and the packet counter off the same transmit count, so
     *     frame = (PKT - 1) % B36_OPT_N
     * exactly, and PKT is already decoded above into rec[1].
     *
     * This needs NO receiver state and survives packet loss completely — each
     * packet names its own frame through its own PKT, so a gap in the stream
     * costs the fields in the missed packets and nothing more. The old tag
     * parse was equally stateless, so nothing is given up.
     *
     * rec[1] is used rather than f[2] deliberately: it is the value AFTER
     * b36dec succeeded, so a corrupt counter yields an empty string here and
     * the optional field is skipped rather than being written into a slot
     * chosen by a garbage index. */
    if (nf > 20 && rec[1].length()) {
        const uint32_t pktNo = (uint32_t)rec[1].toInt();
        const int frame = (pktNo >= 1)
                        ? (int)((pktNo - 1) % (uint32_t)B36_OPT_N) : -1;
        if (frame >= 0 && frame < B36_OPT_N) {
            if (frame == B36_OPT_UTC_FRAME) {
                /* UTC travels as seconds since midnight (4 base-36 chars
                 * instead of 8) and is rebuilt into the HH:MM:SS string the
                 * GCS shows for SCHEMA field 75 (UTC). This is the one frame
                 * whose value is a clock, so it cannot go through
                 * B36_OPT_MAP/b36Field like the other fifteen.
                 * Once a fix exists this is redundant with the GNSS_TIME
                 * derivation above and simply rewrites the same string; before
                 * first fix it is the only source of UTC. */
                int32_t s;
                if (b36dec(f[20], s) && s >= 0 && s < 86400) {
                    char u[12];
                    snprintf(u, sizeof(u), "%02ld:%02ld:%02ld",
                             (long)(s / 3600), (long)((s / 60) % 60), (long)(s % 60));
                    rec[75] = String(u);                   /* UTC */
                }
            } else {
                b36Field(f[20], B36_OPT[frame], cell, sizeof(cell));
                rec[B36_OPT_MAP[frame]] = String(cell);
            }
        }
    }

    /* Receiver-domain values the CanSat cannot know.
     * SCHEMA positions: 58 RSSI, 59 SNR, 60 FREQ_MHZ. */
    rec[58] = String(rssi, 1);
    rec[59] = String(snr, 1);
    rec[60] = useBackup ? String(FREQ_BACKUP_MHZ, 2) : String(FREQ_MAIN_MHZ, 2);
    return true;
}

static void emitCsv() {
    String out;
    out.reserve(460);
    out = TEAM_ID_GCS;                     /* GCS strips column 0 by name */
    for (int i = 0; i < N_FIELDS; i++) { out += ','; out += rec[i]; }
    Serial.println(out);
}

/* ========================================================================== */
void setup() {
    /* The LR1121 HP PA draws ~300 mA during TX ramp-up. On USB power this
     * droops the 3.3 V rail below the ESP32 BOD threshold (~2.45 V) and
     * triggers a spurious brownout reset. The real fix is a 220 µF cap on the
     * 3.3 V rail; this suppresses the false-positive in software until hardware
     * is added. */
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

    Serial.begin(115200);
    Serial.println();
    Serial.println(F("// [VRITRA-GS] " TEAM_ID_GCS "  gateway 6.2.0"));
    /* The flight computer prints the same number. If they differ, the two
     * copies of Base36.h have drifted and every decoded field is silently
     * wrong — check this before trusting a single byte of telemetry. */
    Serial.printf("// [WIRE] base-36, schema sum 0x%04X\n", b36SchemaSum());
    for (int i = 0; i < N_FIELDS; i++) rec[i] = "";
    while (!radioInit()) { Serial.println(F("// [RF] retry 2 s...")); delay(2000); }
    int16_t st = radio.startReceive();
    Serial.println(st == RADIOLIB_ERR_NONE ? F("// [RX] listening")
                                           : F("// [RX] startReceive FAILED"));
    Serial.println(F("// [RX] CanSat boots SILENT per guideline 6.1.v — "
                     "send CMD_TX_ON to start telemetry"));
}

void loop() {
    pollUsb();

    if (pktFlag) {
        pktFlag = false;
        char buf[DL_PACKET_MAX + 1];
        int len = radio.getPacketLength();
        /* sendUplink() already re-arms RX at its own end (both on the
         * success and the recovery path). Tracking whether it ran here
         * avoids calling startReceive() a second time back-to-back for no
         * reason on every packet that triggers an uplink reply. Every OTHER
         * exit from this block (bad length, read error, rejected packet, no
         * pending uplink) still falls through to the startReceive() below. */
        bool uplinkSent = false;

        if (len > 0 && len <= DL_PACKET_MAX &&
            radio.readData((uint8_t*)buf, (size_t)len) == RADIOLIB_ERR_NONE) {
            buf[len] = '\0';

            float rssi = radio.getRSSI();
            float snr  = radio.getSNR();

            if (ingest(buf, rssi, snr)) {
                rxCount++;
                emitCsv();
                float per = (rxCount + missed)
                          ? (100.0f * missed / (float)(rxCount + missed)) : 0.0f;
                Serial.printf("// [RX] #%lu %dB RSSI %.1f SNR %.1f | "
                              "missed=%lu PER=%.1f%% rej=%lu\n",
                              (unsigned long)rxCount, len, rssi, snr,
                              (unsigned long)missed, per,
                              (unsigned long)rejected);

                /* Land the uplink inside the CanSat's post-transmit window. */
                if (ulLeft) {
                    delay(UPLINK_DELAY_MS);
                    sendUplink();
                    uplinkSent = true;
                }
            } else {
                rejected++;
                Serial.printf("// [RX] rejected %dB RSSI %.1f SNR %.1f (rej=%lu)\n",
                              len, rssi, snr, (unsigned long)rejected);
            }
        }
        if (!uplinkSent) radio.startReceive();
    }

    /* Blind repeat path. With the CanSat silent — its mandated boot state, or
     * after CMD_TX_OFF — there is no downlink to key off, so this is the only
     * way CMD_TX_ON reaches it. The CanSat's receiver is continuously armed in
     * that mode, so these always land. */
    if (ulLeft && (int32_t)(millis() - ulNextBlindMs) >= 0) {
        sendUplink();
    }
}
