/*
 * VRITRA  ·  2026-INSPACe-CAN-7USAT-025
 * ESP32 + Semtech LR1121  —  GROUND STATION RECEIVER / FRAME-REASSEMBLY GATEWAY
 * =====================================================================
 * The CanSat sends COMPACT frames to keep airtime low (competition §16):
 *   025,<FRAME>,<16 core fields>,<optional slice for FRAME>
 * where the 15 mandatory fields (schema 0..15) are in EVERY frame and the
 * optional data is split equally across FRAME = 0,1,2.
 *
 * This gateway keeps a persistent 84-field record, updates the core every frame
 * plus that frame's optional slice, injects the RF-domain values only the
 * receiver can know (RSSI, SNR, carrier freq), and forwards the FULL 84-field
 * plain-decimal positional CSV to the GCS — which therefore needs NO changes.
 *
 * It also relays uplink commands: the GCS writes "CMD_XXX\r\n" over USB; we
 * transmit "025,CMD,XXX" right after a downlink so it lands in the CanSat's
 * 600 ms RX window.
 *
 * SPI:  NSS=5  SCK=18  MOSI=23  MISO=19  RST=14  BUSY=27  IRQ=26
 * RF:   867.00 MHz · SF10 · BW125 kHz · CR4/5 · Sync 0x98 · Pre 8  (match TX!)
 */

#include <RadioLib.h>
#include <SPI.h>

// ─── Pin map ─────────────────────────────────────────────────────────────────
#define PIN_CS    5
#define PIN_IRQ   26
#define PIN_RST   14
#define PIN_BUSY  27
#define PIN_SCK   18
#define PIN_MOSI  23
#define PIN_MISO  19

// ─── RF parameters — keep identical to the Teensy TX ─────────────────────────
#define FREQ_MAIN   867.00f
#define FREQ_BACKUP 865.75f
#define LORA_SF     10
#define LORA_BW     125.0f
#define LORA_CR     5
#define LORA_SYNC   0x98
#define LORA_PRE    8
#define TX_POWER    22
#define TCXO_V      1.6f

#define TEAM_ID     "025"                          // short id on the RF link (saves airtime)
#define TEAM_ID_GCS "2026-INSPACe-CAN-7USAT-025"   // FULL id — MUST equal vritra_gcs.py TEAM_ID
                                                   // so the parser strips column 0. If this does
                                                   // not match, every field shifts by +1 and the
                                                   // whole dashboard reads garbage.
#define N_FIELDS    84            // full GCS SCHEMA length
#define N_CORE      16            // schema 0..15 sent in every frame
#define UPLINK_DELAY_MS  25       // land inside CanSat RX window

// Optional-slice → schema-index maps. MUST match vritra_tx.ino buildPacket().
static const uint8_t F0_MAP[] = {17, 18, 70, 71, 74};        // SOC,GND_SPEED,MCU,WDOG,BEACON
static const uint8_t F1_MAP[] = {23, 24, 25, 26, 27, 28};    // GX,GY,GZ,MX,MY,MZ
static const uint8_t F2_MAP[] = {77, 79, 80, 81, 82, 83};    // NAVIC,HDOP,VDOP,FIX,GEOID,GNSS_SYS

// ─── Base-36 decoder ─────────────────────────────────────────────────────────
// The Teensy encodes every numeric field as a base-36 integer (multiplied by a
// scale factor) to shrink each air frame by ~25 B. This gateway decodes every
// token back to a plain-decimal string before putting it in rec[], so the GCS
// still receives the unchanged 84-field plain-decimal CSV it already parses.
//
// Dc: denominator (1 = integer, else float) and decimal places after decode.
// Scale contract — MUST match vritra_tx.ino b36_* calls:
//   integer (div=1):            TIME_S PKT PRES_PA GNSS_TIME SATS STATE
//                               SOC_PCT MCU WDOG BEACON NAVIC FIX GNSS_SYS MX MY MZ
//   ×10  → ÷10 (dp=1):         ALT_M TEMP_C GPS_ALT_M ACC_X/Y/Z SPIN_RATE
//                               GND_SPEED GX GY GZ GEOID_SEP
//   ×100 → ÷100 (dp=2):        BUS_V HDOP VDOP
//   ×1e6 → ÷1e6 (dp=6):        LAT LON

struct Dc { uint32_t div; uint8_t dp; };

// Core fields: positions 0..15 in the compact frame → schema 0..15
static const Dc CORE_DC[16] = {
    {1,0},       // 0  TIME_S    integer
    {1,0},       // 1  PKT       integer
    {10,1},      // 2  ALT_M     ÷10, 1dp
    {1,0},       // 3  PRES_PA   integer Pa
    {10,1},      // 4  TEMP_C    ÷10, 1dp
    {100,2},     // 5  BUS_V     ÷100, 2dp
    {1,0},       // 6  GNSS_TIME integer
    {1000000,6}, // 7  LAT       ÷1e6, 6dp
    {1000000,6}, // 8  LON       ÷1e6, 6dp
    {10,1},      // 9  GPS_ALT_M ÷10, 1dp
    {1,0},       // 10 SATS      integer
    {10,1},      // 11 ACC_X     ÷10, 1dp
    {10,1},      // 12 ACC_Y     ÷10, 1dp
    {10,1},      // 13 ACC_Z     ÷10, 1dp
    {10,1},      // 14 SPIN_RATE ÷10, 1dp
    {1,0},       // 15 STATE     integer
};
// F0 slice: {17=SOC, 18=GND_SPEED, 70=MCU, 71=WDOG, 74=BEACON}
static const Dc F0_DC[] = {{1,0},{10,1},{1,0},{1,0},{1,0}};
// F1 slice: {23=GX, 24=GY, 25=GZ, 26=MX, 27=MY, 28=MZ}
static const Dc F1_DC[] = {{10,1},{10,1},{10,1},{1,0},{1,0},{1,0}};
// F2 slice: {77=NAVIC, 79=HDOP, 80=VDOP, 81=FIX, 82=GEOID_SEP, 83=GNSS_SYS}
static const Dc F2_DC[] = {{1,0},{100,2},{100,2},{1,0},{10,1},{1,0}};

// Decode one base-36 token to a plain-decimal string ready for rec[].
// Empty token → returns "" (GCS renders "—").
static String b36dec(const String& tok, Dc dc) {
    if (!tok.length()) return "";
    bool neg = (tok[0] == '-');
    int32_t raw = 0;
    for (int i = neg ? 1 : 0; i < (int)tok.length(); i++) {
        char c = tok[i];
        c = (c >= 'a' && c <= 'z') ? c - 32 : c;   // tolower safe
        raw = raw * 36 + ((c >= 'A') ? (c - 'A' + 10) : (c - '0'));
    }
    if (neg) raw = -raw;
    if (dc.div == 1) {
        char b[14]; snprintf(b, sizeof b, "%ld", (long)raw);
        return String(b);
    }
    char b[20];
    dtostrf((float)raw / (float)dc.div, 0, dc.dp, b);
    return String(b[0] == ' ' ? b + 1 : b);
}

// ─── RF switch table (match TX) ──────────────────────────────────────────────
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

LR1121 radio(new Module(PIN_CS, PIN_IRQ, PIN_RST, PIN_BUSY, SPI));

volatile bool pktFlag   = false;
bool          useBackup = false;
uint32_t      rxCount   = 0;
String        pendingCmd = "";     // most-recent queued command ("" = none)
String        usbBuf     = "";

// Persistent reassembled record. "" = never received → GCS renders "—".
String        rec[N_FIELDS];

void IRAM_ATTR onPacket() { pktFlag = true; }

// ─── Radio init ──────────────────────────────────────────────────────────────
static bool radioInit() {
    SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);
    radio.setRfSwitchTable(SW_PINS, SW_TABLE);
    float f = useBackup ? FREQ_BACKUP : FREQ_MAIN;
    int16_t st = radio.begin(f, LORA_BW, LORA_SF, LORA_CR,
                             LORA_SYNC, TX_POWER, LORA_PRE, TCXO_V);
    if (st != RADIOLIB_ERR_NONE) { Serial.printf("// [RF] begin FAILED code=%d\n", st); return false; }
    radio.setRfSwitchTable(SW_PINS, SW_TABLE);   // begin() clears it — re-apply
    radio.setCRC(2);
    radio.setRegulatorDCDC();
    radio.setSyncWord(LORA_SYNC);
    radio.setOutputPower(TX_POWER, true);        // HP PA
    LR11x0VersionInfo_t vi;
    if (radio.getVersionInfo(&vi) == RADIOLIB_ERR_NONE)
        Serial.printf("// [RF] HW=0x%02X DEV=0x%02X FW=%d.%d  %.2f MHz SF%d BW%.0fk OK\n",
            vi.hardware, vi.device, vi.fwMajor, vi.fwMinor, f, LORA_SF, LORA_BW);
    return true;
}

// ─── Poll USB serial for GCS commands ────────────────────────────────────────
static void pollUSB() {
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\r' || c == '\n') {
            if (usbBuf.length()) {
                String line = usbBuf; usbBuf = ""; line.trim();
                if (line.startsWith("CMD_")) {
                    pendingCmd = line.substring(4);
                    Serial.printf("// [UP] queued: %s\n", pendingCmd.c_str());
                } else if (line == "FAILOVER_LOCAL") {
                    useBackup = !useBackup; radioInit();
                    Serial.printf("// [UP] local failover -> %.2f MHz\n",
                                  useBackup ? FREQ_BACKUP : FREQ_MAIN);
                }
            }
        } else if (usbBuf.length() < 128) usbBuf += c;
    }
}

// ─── Fire queued uplink right after a downlink ───────────────────────────────
static void fireUplink() {
    if (!pendingCmd.length()) return;
    String pkt = String(TEAM_ID) + ",CMD," + pendingCmd;
    pendingCmd = "";
    delay(UPLINK_DELAY_MS);
    int16_t st = radio.transmit(pkt);
    Serial.printf("// [UP] tx \"%s\" %s\n", pkt.c_str(),
                  st == RADIOLIB_ERR_NONE ? "OK" : "FAIL");
}

// ─── Reassemble a compact frame into the persistent 84-field record ──────────
static bool ingestFrame(const String& raw) {
    // Split into tokens (a compact frame has at most ~24)
    static String tok[40];
    int nt = 0, start = 0;
    while (nt < 40) {
        int c = raw.indexOf(',', start);
        if (c < 0) { tok[nt++] = raw.substring(start); break; }
        tok[nt++] = raw.substring(start, c);
        start = c + 1;
    }
    if (nt < 2 || tok[0] != TEAM_ID) return false;
    if (tok[1] == "CMD") return false;

    int f = tok[1].toInt();
    if (nt < 2 + N_CORE) return false;              // core incomplete → drop

    // Decode base-36 core fields to plain-decimal strings for rec[]
    for (int i = 0; i < N_CORE; i++)
        rec[i] = b36dec(tok[2 + i], CORE_DC[i]);

    const uint8_t* map = nullptr;
    const Dc*      dct = nullptr;
    int mlen = 0;
    if      (f == 0) { map = F0_MAP; dct = F0_DC; mlen = sizeof(F0_MAP); }
    else if (f == 1) { map = F1_MAP; dct = F1_DC; mlen = sizeof(F1_MAP); }
    else if (f == 2) { map = F2_MAP; dct = F2_DC; mlen = sizeof(F2_MAP); }
    for (int k = 0; k < mlen; k++)
        if (2 + N_CORE + k < nt)
            rec[map[k]] = b36dec(tok[2 + N_CORE + k], dct[k]);
    return true;
}

// ─── Emit the full 84-field positional CSV the GCS expects ───────────────────
static void emitFull(float rssi, float snr) {
    rec[61] = String(rssi, 1);                        // RSSI  (receiver-measured)
    rec[62] = String(snr, 1);                         // SNR
    rec[63] = useBackup ? "865.75" : "867.00";        // FREQ_MHZ (receiver knows)
    String out; out.reserve(300);
    out = TEAM_ID_GCS;                                // FULL id so the GCS strips column 0
    for (int i = 0; i < N_FIELDS; i++) { out += ','; out += rec[i]; }
    Serial.println(out);                              // → GCS parses this line
}

// ─── setup ───────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(400);
    Serial.println(F("// [VRITRA-RX] 2026-INSPACe-CAN-7USAT-025  ESP32 gateway"));
    for (int i = 0; i < N_FIELDS; i++) rec[i] = "";
    while (!radioInit()) { Serial.println(F("// [RF] retry 2 s...")); delay(2000); }
    radio.setPacketReceivedAction(onPacket);
    int16_t st = radio.startReceive();
    Serial.println(st == RADIOLIB_ERR_NONE ? F("// [RX] listening...") : F("// [RX] startReceive FAILED"));
}

// ─── loop ────────────────────────────────────────────────────────────────────
void loop() {
    pollUSB();

    if (pktFlag) {
        pktFlag = false;
        String raw;
        int16_t st = radio.readData(raw);
        if (st == RADIOLIB_ERR_NONE && raw.length()) {
            float rssi = radio.getRSSI();
            float snr  = radio.getSNR();
            if (ingestFrame(raw)) {
                rxCount++;
                emitFull(rssi, snr);
                Serial.printf("// [RX] #%lu frame f=%c %dB RSSI %.1f SNR %.1f\n",
                              rxCount, raw.length() > 4 ? raw[4] : '?', raw.length(), rssi, snr);
                fireUplink();     // relay a queued command into the CanSat RX slot
            }
        } else {
            Serial.printf("// [RX] read err code=%d\n", st);
        }
        radio.startReceive();
    }
}
