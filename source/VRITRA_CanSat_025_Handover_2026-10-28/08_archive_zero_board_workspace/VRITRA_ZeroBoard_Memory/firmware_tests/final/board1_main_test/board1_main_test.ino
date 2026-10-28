/*
 * VRITRA BOARD 1 (MAIN, Teensy 4.1) — FULL BENCH TEST   (final etched PCB, 2026-10-27)
 *
 * At power-up every part is checked once and a PASS / FAIL table is printed.
 * After that, type a key in the Serial Monitor (115200 baud):
 *
 *   h  help                       a  live sensor data every second (default)
 *   i  I2C scan                   n  raw GNSS NMEA lines on/off
 *   f  flash write/read test      s  SD card write/read test
 *   r  radio test on/off (sends a short packet every 2 s, listens in between)
 *   b  beacon beep                c  camera ping (Serial1)      p  PARACHUTE ping (Serial3)
 *   x  stop everything (radio off, live off)
 *
 * WHAT IS CONNECTED WHERE (read from the etched BOARD1_MAIN.kicad_pcb)
 *   I2C  pin 18 SDA / 19 SCL : BMP585 #1 0x47 · ICM-20948 0x69 (+ AK09916 compass 0x0C) · DS3231 0x68
 *                              (+ EEPROM 0x57) · INA260 on the POWER board through J2 (0x40..0x4F)
 *   SPI1 pin 26 MOSI / 27 SCK / 39 MISO : Core1121 LR1121 (CS 29, BUSY 24, RESET 25, IRQ/DIO9 30)
 *                                          W25Q64 flash (CS 9)
 *   GNSS L89HA  Serial2: pin 7 RX <- GNSS TX, pin 8 TX -> GNSS RX, pin 6 RESET (LOW = reset), 9600 baud
 *   Camera      Serial1: pin 1 TX (R11 1k) -> J5.3, pin 0 RX <- J5.4   (cable to BACKUP board J2)
 *   Heartbeat   pin 5 (R3 1k) -> J5.1 -> BACKUP pin 4
 *   PARACHUTE   Serial3: pin 14 TX (R4 1k) -> J4.2,  pin 15 RX <- J4.1
 *   Battery     pin 20 (A6) <- 100k / 22k divider from J3 (VBAT)
 *   Beacon      MAX98357A on I2S2: pin 2 DIN, 3 LRC, 4 BCLK, pin 37 SD (HIGH = on)
 *   LEDs        pin 31 green (OK), pin 32 red (fault)
 *   SD card     the Teensy 4.1's own slot
 *
 * RADIO — IN-SPACe assignment 19-10-2026, team 025 (checked against the PDF):
 *   866.50 MHz main (865.25 backup), SF9, BW 125 kHz, CR 4/5, sync 0x99, +10 dBm on the pad.
 *   The radio NEVER transmits unless you press 'r'. Antenna on ANT2 (LORA_ANT), never ANT1.
 *   Your ground gateway (vritra_gs) will print these test packets as
 *   "// [RX] rejected 24B RSSI -40.0 SNR 10.0" — that line IS the proof the link works
 *   (the gateway only decodes real flight frames). Commands you send from the GCS
 *   (e.g. CMD_TX_ON) are printed here as [RADIO] heard: 2026-...,C,1,0,seq.
 *
 * Libraries: RadioLib (7.x, already installed). Wire, SPI, SD, Audio come with Teensyduino.
 */
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <Audio.h>
#include <RadioLib.h>

// ---------- pins (etched Board 1) ----------
const uint8_t LORA_CS = 29, LORA_IRQ = 30, LORA_RST = 25, LORA_BUSY = 24;
const uint8_t SPI1_MOSI = 26, SPI1_SCK = 27, SPI1_MISO = 39;
const uint8_t FLASH_CS = 9;
const uint8_t GNSS_RST = 6, HEARTBEAT = 5, AMP_SD = 37, LED_OK = 31, LED_FLT = 32, VBAT_PIN = 20;

// ---------- settings ----------
const uint32_t GNSS_BAUD = 9600;       // L89HA default (same as the flight firmware)
const uint32_t LINK_BAUD = 115200;     // camera + PARACHUTE links
const float    VBAT_RATIO = (100.0f + 22.0f) / 22.0f;

// radio: team 025 assignment — do not change
const float   FREQ_MAIN_MHZ = 866.50f;
const float   BW_KHZ = 125.0f;
const uint8_t LORA_SF = 9, LORA_CR = 5, LORA_SYNC = 0x99;
const int8_t  TX_DBM_PAD = 10;
const uint16_t PREAMBLE = 8;
const float   TCXO_V = 1.6f;           // Core1121 TCXO (proven in vritra_fc)
const uint32_t RADIO_PERIOD_MS = 2000;

// ---------- I2C addresses ----------
const uint8_t BMP = 0x47, ICM = 0x69, MAG = 0x0C, RTC = 0x68, EEP = 0x57;

// ---------- radio (same RF switch table as vritra_fc Radio.cpp — do not change) ----------
static const uint32_t SW_PINS[] = { RADIOLIB_LR11X0_DIO5, RADIOLIB_LR11X0_DIO6, RADIOLIB_NC, RADIOLIB_NC, RADIOLIB_NC };
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
LR1121 radio = new Module(LORA_CS, LORA_IRQ, LORA_RST, LORA_BUSY, SPI1);
volatile bool radioIrq = false;
void onRadioIrq() { radioIrq = true; }

// ---------- beacon tone: sine -> I2S2 (pins 2/3/4) -> MAX98357A ----------
AudioSynthWaveformSine tone1;
AudioOutputI2S2        i2s2;
AudioConnection        c1(tone1, 0, i2s2, 0), c2(tone1, 0, i2s2, 1);

// ---------- results ----------
bool bmpOk, icmOk, magOk, rtcOk, flashOk, sdOk, radioOk;
uint8_t inaAddr = 0;                   // 0 = no INA260 found
float padPa = 0;                       // pressure at boot = 0 m

// ---------- GNSS (parsed from GGA) ----------
struct Gnss { bool fix; uint8_t sats; float hdop, alt; double lat, lon; char utc[11]; uint32_t lines, bad; } gps = {};
bool showNmea = false;

// ---------- modes ----------
bool live = true, radioOn = false;

// ===================================================================== I2C helpers
bool wr(uint8_t a, uint8_t r, uint8_t v) {
  Wire.beginTransmission(a); Wire.write(r); Wire.write(v);
  return Wire.endTransmission() == 0;
}
bool rd(uint8_t a, uint8_t r, uint8_t *b, uint8_t n) {
  Wire.beginTransmission(a); Wire.write(r);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(a, n) != n) return false;
  for (uint8_t i = 0; i < n; i++) b[i] = Wire.read();
  return true;
}
bool rd16(uint8_t a, uint8_t r, uint16_t &v) {          // INA260 registers are 16-bit, MSB first
  uint8_t b[2];
  if (!rd(a, r, b, 2)) return false;
  v = (uint16_t)b[0] << 8 | b[1];
  return true;
}
uint8_t bcd(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }
bool present(uint8_t a) { Wire.beginTransmission(a); return Wire.endTransmission() == 0; }

// ===================================================================== I2C scan
const char *i2cName(uint8_t a) {
  if (a == BMP) return "BMP585 #1";
  if (a == ICM) return "ICM-20948";
  if (a == MAG) return "AK09916 compass (inside ICM-20948)";
  if (a == RTC) return "DS3231 RTC";
  if (a == EEP) return "AT24C32 EEPROM (on the RTC module)";
  if (a >= 0x40 && a <= 0x4F) return "INA260? (POWER board)";
  return "unknown";
}
void i2cScan() {
  Serial.println("\n[I2C] scan pins 18/19:");
  int n = 0;
  for (uint8_t a = 1; a < 127; a++)
    if (present(a)) { Serial.printf("   0x%02X  %s\n", a, i2cName(a)); n++; }
  if (!n) Serial.println("   nothing answers -> check 3V3, GND, pins 18/19");
}

// ===================================================================== BMP585
bool bmpStart() {
  uint8_t id = 0;
  wr(BMP, 0x7E, 0xB6); delay(10);                          // soft reset
  if (!rd(BMP, 0x01, &id, 1) || id != 0x51) { Serial.printf("   BMP585 CHIP_ID 0x%02X (want 0x51)\n", id); return false; }
  wr(BMP, 0x37, 0x80 | (0x14 << 2));                       // standby, 25 Hz
  wr(BMP, 0x36, 0x40 | (4 << 3) | 1);                      // pressure on, P x16, T x2
  delay(5);
  wr(BMP, 0x37, 0x80 | (0x14 << 2) | 1);                   // continuous
  delay(100);
  return true;
}
bool bmpRead(float &tc, float &pa) {
  uint8_t b[6];
  if (!rd(BMP, 0x1D, b, 6)) return false;
  int32_t rt = (int32_t)((uint32_t)b[2] << 16 | (uint32_t)b[1] << 8 | b[0]);
  if (rt & 0x800000) rt |= 0xFF000000;
  uint32_t rp = (uint32_t)b[5] << 16 | (uint32_t)b[4] << 8 | b[3];
  tc = rt / 65536.0f; pa = rp / 64.0f;
  return pa > 30000 && pa < 110000;                        // physically possible
}

// ===================================================================== ICM-20948 + AK09916
bool icmStart() {
  uint8_t who = 0;
  wr(ICM, 0x7F, 0x00);                                     // bank 0
  if (!rd(ICM, 0x00, &who, 1) || who != 0xEA) { Serial.printf("   ICM-20948 WHO_AM_I 0x%02X (want 0xEA)\n", who); return false; }
  wr(ICM, 0x06, 0x80); delay(50);                          // reset
  wr(ICM, 0x7F, 0x00);
  wr(ICM, 0x06, 0x01);                                     // wake, best clock
  wr(ICM, 0x07, 0x00);                                     // accel + gyro on (default +-2 g, +-250 dps)
  delay(50);
  return true;
}
bool magStart() {                                          // compass sits behind the ICM's I2C bypass
  uint8_t uc = 0, wia = 0;
  rd(ICM, 0x03, &uc, 1);
  wr(ICM, 0x03, uc & ~0x20);                               // USER_CTRL: I2C master off
  wr(ICM, 0x0F, 0x02);                                     // INT_PIN_CFG: BYPASS_EN
  delay(10);
  if (!rd(MAG, 0x01, &wia, 1) || wia != 0x09) { Serial.printf("   AK09916 WIA2 0x%02X (want 0x09)\n", wia); return false; }
  wr(MAG, 0x32, 0x01); delay(10);                          // soft reset
  wr(MAG, 0x31, 0x08);                                     // continuous 100 Hz
  return true;
}

// ===================================================================== DS3231
bool rtcStart() {
  uint8_t t[7];
  if (!rd(RTC, 0x00, t, 7)) return false;
  uint8_t st = 0; rd(RTC, 0x0F, &st, 1);
  Serial.printf("   RTC time 20%02u-%02u-%02u %02u:%02u:%02u%s\n", bcd(t[6]), bcd(t[5] & 0x1F), bcd(t[4]),
                bcd(t[2] & 0x3F), bcd(t[1]), bcd(t[0] & 0x7F),
                (st & 0x80) ? "  <- OSF set: clock stopped once (coin cell flat / never set)" : "");
  return true;
}

// ===================================================================== INA260 (battery monitor on POWER board)
uint8_t inaFind() {
  for (uint8_t a = 0x40; a <= 0x4F; a++) {
    uint16_t mid = 0, did = 0;
    if (a == BMP || !present(a)) continue;
    if (rd16(a, 0xFE, mid) && rd16(a, 0xFF, did) && mid == 0x5449 && (did >> 4) == 0x227) return a;   // "TI", INA260
  }
  return 0;
}
// rough resting voltage -> charge for one Li-ion cell (replace with your own measured curve later)
float socFromCell(float v) {
  const float V[] = {3.00f, 3.30f, 3.40f, 3.50f, 3.60f, 3.70f, 3.80f, 3.90f, 4.00f, 4.10f, 4.20f};
  const float P[] = {0, 6, 12, 20, 32, 45, 58, 70, 82, 92, 100};
  if (v <= V[0]) return 0;
  for (int i = 1; i < 11; i++)
    if (v <= V[i]) return P[i - 1] + (P[i] - P[i - 1]) * (v - V[i - 1]) / (V[i] - V[i - 1]);
  return 100;
}
void inaPrint() {
  uint16_t rv, ri, rp;
  if (!inaAddr || !rd16(inaAddr, 0x02, rv) || !rd16(inaAddr, 0x01, ri) || !rd16(inaAddr, 0x03, rp)) {
    Serial.println("INA260   not found");
    return;
  }
  float v = rv * 0.00125f, ma = (int16_t)ri * 1.25f, w = rp * 0.010f;
  const char *st = ma > 20 ? "DISCHARGING" : ma < -20 ? "CHARGING" : "idle";
  Serial.printf("INA260   0x%02X  %.3f V  %+.0f mA  %.2f W  %s  ~%.0f %% (%.3f V/cell, rough curve)\n",
                inaAddr, v, ma, w, st, socFromCell(v / 3), v / 3);
}

// ===================================================================== SPI1: W25Q64 flash
SPISettings spiSet(4000000, MSBFIRST, SPI_MODE0);
uint8_t flashCmd(uint8_t c) {
  SPI1.beginTransaction(spiSet); digitalWrite(FLASH_CS, LOW);
  uint8_t r = SPI1.transfer(c);
  digitalWrite(FLASH_CS, HIGH); SPI1.endTransaction();
  return r;
}
uint8_t flashStatus() {
  SPI1.beginTransaction(spiSet); digitalWrite(FLASH_CS, LOW);
  SPI1.transfer(0x05); uint8_t s = SPI1.transfer(0);
  digitalWrite(FLASH_CS, HIGH); SPI1.endTransaction();
  return s;
}
bool flashWait(uint32_t ms) {
  uint32_t t0 = millis();
  while (flashStatus() & 1) if (millis() - t0 > ms) return false;
  return true;
}
void flashAddr(uint8_t cmd, uint32_t a) { SPI1.transfer(cmd); SPI1.transfer(a >> 16); SPI1.transfer(a >> 8); SPI1.transfer(a); }
bool flashId() {
  SPI1.beginTransaction(spiSet); digitalWrite(FLASH_CS, LOW);
  SPI1.transfer(0x9F);
  uint8_t m = SPI1.transfer(0), t = SPI1.transfer(0), c = SPI1.transfer(0);
  digitalWrite(FLASH_CS, HIGH); SPI1.endTransaction();
  Serial.printf("   W25Q64 JEDEC %02X %02X %02X (want EF 40 17)\n", m, t, c);
  return m == 0xEF && c == 0x17;
}
void flashTest() {                                         // uses the LAST 4 kB sector only
  const uint32_t A = 0x7FF000;
  char out[32], in[32] = {};
  snprintf(out, sizeof out, "VRITRA B1 %lu", (unsigned long)millis());
  Serial.printf("[FLASH] erase sector 0x%06lX, write \"%s\", read back...\n", (unsigned long)A, out);
  flashCmd(0x06);                                          // write enable
  SPI1.beginTransaction(spiSet); digitalWrite(FLASH_CS, LOW); flashAddr(0x20, A); digitalWrite(FLASH_CS, HIGH); SPI1.endTransaction();
  if (!flashWait(500)) { Serial.println("[FLASH] FAIL: erase never finished"); return; }
  flashCmd(0x06);
  SPI1.beginTransaction(spiSet); digitalWrite(FLASH_CS, LOW); flashAddr(0x02, A);
  for (size_t i = 0; i < sizeof out; i++) SPI1.transfer(out[i]);
  digitalWrite(FLASH_CS, HIGH); SPI1.endTransaction();
  if (!flashWait(10)) { Serial.println("[FLASH] FAIL: write never finished"); return; }
  SPI1.beginTransaction(spiSet); digitalWrite(FLASH_CS, LOW); flashAddr(0x03, A);
  for (size_t i = 0; i < sizeof in; i++) in[i] = SPI1.transfer(0);
  digitalWrite(FLASH_CS, HIGH); SPI1.endTransaction();
  in[sizeof in - 1] = 0;
  Serial.printf("[FLASH] read \"%s\" -> %s\n", in, strcmp(in, out) == 0 ? "PASS" : "FAIL");
}

// ===================================================================== SD card (Teensy 4.1 slot)
void sdTest() {
  if (!sdOk) { sdOk = SD.begin(BUILTIN_SDCARD); if (!sdOk) { Serial.println("[SD] FAIL: no card / not FAT32"); return; } }
  char out[40];
  snprintf(out, sizeof out, "VRITRA B1 SD test %lu", (unsigned long)millis());
  SD.remove("B1TEST.TXT");
  File f = SD.open("B1TEST.TXT", FILE_WRITE);
  if (!f) { Serial.println("[SD] FAIL: cannot create B1TEST.TXT"); return; }
  f.println(out); f.close();
  f = SD.open("B1TEST.TXT");
  String in = f ? f.readStringUntil('\n') : "";
  if (f) f.close();
  in.trim();
  Serial.printf("[SD] wrote \"%s\", read \"%s\" -> %s   card %.1f GB\n", out, in.c_str(),
                in == out ? "PASS" : "FAIL", SD.totalSize() / 1e9);
}

// ===================================================================== radio
const char *rfErr(int16_t e) {
  switch (e) {
    case RADIOLIB_ERR_NONE:              return "OK";
    case RADIOLIB_ERR_CHIP_NOT_FOUND:    return "CHIP_NOT_FOUND (check MISO pin 39, CS 29, 3V3)";
    case RADIOLIB_ERR_SPI_CMD_TIMEOUT:   return "SPI_CMD_TIMEOUT (BUSY pin 24 never went low)";
    case RADIOLIB_ERR_SPI_CMD_FAILED:    return "SPI_CMD_FAILED";
    case RADIOLIB_ERR_CRC_MISMATCH:      return "CRC error (packet damaged)";
    default:                             return "see RadioLib TypeDef.h";
  }
}
bool radioStart() {
  SPI1.setMISO(SPI1_MISO); SPI1.setMOSI(SPI1_MOSI); SPI1.setSCK(SPI1_SCK);
  SPI1.begin();
  radio.setRfSwitchTable(SW_PINS, SW_TABLE);
  int16_t e = radio.begin(FREQ_MAIN_MHZ, BW_KHZ, LORA_SF, LORA_CR, LORA_SYNC, TX_DBM_PAD, PREAMBLE, TCXO_V);
  if (e != RADIOLIB_ERR_NONE) { Serial.printf("   LR1121 begin code %d %s\n", e, rfErr(e)); return false; }
  radio.setRfSwitchTable(SW_PINS, SW_TABLE);              // begin() clears it
  radio.setCRC(2);
  radio.setRegulatorDCDC();
  radio.setSyncWord(LORA_SYNC);
  radio.setOutputPower(TX_DBM_PAD, true);                  // true = high-power PA path (proven)
  radio.calibrateImageRejection(863.0f, 870.0f);           // our band, not the chip's default 915 MHz
  radio.setRxBoostedGainMode(true);
  LR11x0VersionInfo_t vi;
  if (radio.getVersionInfo(&vi) == RADIOLIB_ERR_NONE)
    Serial.printf("   LR1121 HW 0x%02X DEV 0x%02X FW %d.%d  %.2f MHz SF%u BW125 CR4/%u sync 0x%02X %+d dBm\n",
                  vi.hardware, vi.device, vi.fwMajor, vi.fwMinor, FREQ_MAIN_MHZ, LORA_SF, LORA_CR, LORA_SYNC, TX_DBM_PAD);
  radio.setIrqAction(onRadioIrq);
  radio.standby();                                         // silent until 'r'
  return true;
}
void radioTask() {                                         // send every 2 s, listen in between
  static enum { IDLE, SENDING, LISTEN } st = IDLE;
  static uint32_t tSend = 0, n = 0;
  uint32_t now = millis();
  if (!radioOn) { if (st != IDLE) { radio.standby(); st = IDLE; } return; }

  if (st == SENDING) {
    if (radioIrq || now - tSend > 1500) {
      bool ok = radioIrq;
      radioIrq = false;
      radio.finishTransmit();
      Serial.printf("[RADIO] packet %lu %s\n", (unsigned long)n, ok ? "sent" : "TIMEOUT (IRQ pin 30 never rose)");
      radio.startReceive();
      st = LISTEN;
    }
    return;
  }
  if (st == LISTEN && radioIrq) {
    radioIrq = false;
    uint8_t buf[256];
    size_t len = radio.getPacketLength();
    if (len > sizeof buf - 1) len = sizeof buf - 1;
    int16_t e = radio.readData(buf, len);
    buf[len] = 0;
    if (e == RADIOLIB_ERR_NONE)
      Serial.printf("[RADIO] heard %uB RSSI %.1f dBm SNR %.1f dB: %s\n", (unsigned)len, radio.getRSSI(), radio.getSNR(), (char *)buf);
    else
      Serial.printf("[RADIO] receive error %d %s\n", e, rfErr(e));
    radio.startReceive();
  }
  if (now - tSend >= RADIO_PERIOD_MS) {
    char pkt[48];
    snprintf(pkt, sizeof pkt, "025,B1TEST,%lu,%lu", (unsigned long)++n, (unsigned long)(now / 1000));
    radioIrq = false;
    radio.standby();
    int16_t e = radio.startTransmit(pkt);
    tSend = now;
    if (e != RADIOLIB_ERR_NONE) { Serial.printf("[RADIO] startTransmit %d %s\n", e, rfErr(e)); radio.startReceive(); st = LISTEN; return; }
    st = SENDING;
  }
}

// ===================================================================== GNSS (GGA parser)
bool nmeaChecksum(const char *s) {
  const char *star = strchr(s, '*');
  if (s[0] != '$' || !star) return false;
  uint8_t c = 0;
  for (const char *p = s + 1; p < star; p++) c ^= *p;
  return c == strtoul(star + 1, nullptr, 16);
}
double nmeaDeg(const char *f, char hemi) {                 // ddmm.mmmm -> degrees
  double v = atof(f);
  int d = (int)(v / 100);
  double deg = d + (v - d * 100) / 60.0;
  return (hemi == 'S' || hemi == 'W') ? -deg : deg;
}
void gnssLine(char *s) {
  gps.lines++;
  if (showNmea) { Serial.print("[NMEA] "); Serial.println(s); }
  if (!nmeaChecksum(s)) { gps.bad++; return; }
  if (strncmp(s + 3, "GGA", 3) != 0) return;
  char *f[16] = {}; int n = 0;                              // split on commas (empty fields kept)
  for (char *p = s; p && n < 16; n++) { f[n] = p; p = strchr(p, ','); if (p) *p++ = 0; }
  if (n < 10) return;
  strncpy(gps.utc, f[1], 10);
  gps.fix  = atoi(f[6]) > 0;
  gps.sats = atoi(f[7]);
  gps.hdop = atof(f[8]);
  gps.alt  = atof(f[9]);
  if (gps.fix) { gps.lat = nmeaDeg(f[2], f[3][0]); gps.lon = nmeaDeg(f[4], f[5][0]); }
}
void gnssPump() {
  static char line[100]; static uint8_t pos = 0;
  while (Serial2.available()) {
    char c = Serial2.read();
    if (c == '$') pos = 0;
    if (c == '\n' || c == '\r') { if (pos) { line[pos] = 0; gnssLine(line); } pos = 0; }
    else if (pos < sizeof line - 1) line[pos++] = c;
  }
}

// ===================================================================== small tests
void beep() {
  Serial.println("[BEACON] 2.8 kHz for 1 s (speaker on the MAX98357A screw terminal)");
  digitalWrite(AMP_SD, HIGH);
  tone1.frequency(2800); tone1.amplitude(0.8);
  delay(1000);
  tone1.amplitude(0);
  digitalWrite(AMP_SD, LOW);
}
float vbat() { return analogRead(VBAT_PIN) * 3.3f / 4095.0f * VBAT_RATIO; }

void liveData() {
  float tc, pa;
  if (bmpOk && bmpRead(tc, pa)) {
    if (padPa == 0) padPa = pa;
    float alt = 44330.0f * (1.0f - powf(pa / padPa, 0.1903f));
    Serial.printf("BMP585   %.2f C  %.1f Pa  alt %.2f m from boot (standard formula, not calibrated yet)\n", tc, pa, alt);
  } else Serial.println("BMP585   no data");

  uint8_t b[12];
  if (icmOk && rd(ICM, 0x2D, b, 12)) {
    int16_t v[6];
    for (int i = 0; i < 6; i++) v[i] = (int16_t)(b[2 * i] << 8 | b[2 * i + 1]);
    Serial.printf("ICM20948 raw A %6d %6d %6d G %6d %6d %6d | A g %6.3f %6.3f %6.3f  G dps %6.1f %6.1f %6.1f\n",
                  v[0], v[1], v[2], v[3], v[4], v[5], v[0] / 16384.0f, v[1] / 16384.0f, v[2] / 16384.0f,
                  v[3] / 131.0f, v[4] / 131.0f, v[5] / 131.0f);
  } else Serial.println("ICM20948 no data");

  uint8_t m[9];
  if (magOk && rd(MAG, 0x10, m, 9) && (m[0] & 1)) {        // ST1 .. ST2 (reading ST2 releases the next sample)
    int16_t x = m[2] << 8 | m[1], y = m[4] << 8 | m[3], z = m[6] << 8 | m[5];
    Serial.printf("COMPASS  raw %6d %6d %6d -> uT %6.1f %6.1f %6.1f%s\n", x, y, z, x * 0.15f, y * 0.15f, z * 0.15f,
                  (m[8] & 0x08) ? "  (overflow)" : "");
  } else Serial.println("COMPASS  no data");

  uint8_t t[7], tt[2];
  if (rtcOk && rd(RTC, 0x00, t, 7) && rd(RTC, 0x11, tt, 2))
    Serial.printf("RTC      20%02u-%02u-%02u %02u:%02u:%02u UTC  chip %.2f C\n", bcd(t[6]), bcd(t[5] & 0x1F), bcd(t[4]),
                  bcd(t[2] & 0x3F), bcd(t[1]), bcd(t[0] & 0x7F), (int8_t)tt[0] + (tt[1] >> 6) * 0.25f);
  inaPrint();
  Serial.printf("VBAT     %.2f V on pin 20 (divider)\n", vbat());
  Serial.printf("GNSS     %s sats %u hdop %.1f  UTC %s  lat %.6f lon %.6f alt %.1f m  (lines %lu, bad %lu)\n",
                gps.fix ? "FIX" : "no fix", gps.sats, gps.hdop, gps.utc, gps.lat, gps.lon, gps.alt,
                (unsigned long)gps.lines, (unsigned long)gps.bad);
  Serial.println();
}

void help() {
  Serial.println("\nKEYS: h help | a live data on/off | i I2C scan | n NMEA on/off | f flash test | s SD test");
  Serial.println("      r radio test on/off | b beep | c camera ping | p PARACHUTE ping | x stop all\n");
}
void result(const char *name, bool ok) { Serial.printf("  %-34s %s\n", name, ok ? "PASS" : "FAIL"); }

// ===================================================================== setup / loop
void setup() {
  Serial.begin(115200);
  uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 4000) {}
  Serial.println("\n=== VRITRA BOARD 1 (MAIN) full test ===");

  pinMode(LED_OK, OUTPUT); pinMode(LED_FLT, OUTPUT);
  digitalWrite(LED_OK, HIGH); digitalWrite(LED_FLT, HIGH);   // both on = LED check
  pinMode(HEARTBEAT, OUTPUT);
  pinMode(AMP_SD, OUTPUT); digitalWrite(AMP_SD, LOW);
  pinMode(GNSS_RST, OUTPUT); digitalWrite(GNSS_RST, HIGH);   // GNSS running
  pinMode(FLASH_CS, OUTPUT); digitalWrite(FLASH_CS, HIGH);
  pinMode(LORA_CS, OUTPUT); digitalWrite(LORA_CS, HIGH);
  analogReadResolution(12);
  AudioMemory(8);

  Wire.begin(); Wire.setClock(100000);                       // 100 kHz like the flight firmware
  Serial1.begin(LINK_BAUD);
  Serial2.begin(GNSS_BAUD);
  Serial3.begin(LINK_BAUD);

  i2cScan();
  Serial.println("\n[CHECKS]");
  bmpOk = bmpStart();
  icmOk = icmStart();
  magOk = icmOk && magStart();
  rtcOk = rtcStart();
  inaAddr = inaFind();
  radioOk = radioStart();                                    // also starts SPI1 (flash shares it)
  flashOk = flashId();
  sdOk = SD.begin(BUILTIN_SDCARD);

  Serial.println("\n[RESULT]");
  result("BMP585 #1 (0x47)", bmpOk);
  result("ICM-20948 (0x69)", icmOk);
  result("AK09916 compass (0x0C)", magOk);
  result("DS3231 RTC (0x68)", rtcOk);
  result("INA260 battery monitor", inaAddr != 0);
  result("Core1121 LR1121 radio (SPI1)", radioOk);
  result("W25Q64 flash (SPI1 CS 9)", flashOk);
  result("SD card (Teensy slot)", sdOk);
  Serial.printf("  %-34s %.2f V\n", "battery divider pin 20", vbat());
  bool allOk = bmpOk && icmOk && magOk && rtcOk && radioOk && flashOk && sdOk;
  digitalWrite(LED_FLT, allOk ? LOW : HIGH);                 // red stays on if anything failed
  Serial.println("  GNSS: watch the live lines below (fix needs open sky, 30-60 s cold)");
  help();
}

void loop() {
  static uint32_t tLive = 0;
  static bool hb = false;

  gnssPump();
  radioTask();
  while (Serial3.available()) { static String s; char c = Serial3.read(); if (c == '\n') { Serial.println("[PARACHUTE] " + s); s = ""; } else if (c != '\r') s += c; }
  while (Serial1.available()) { static String s; char c = Serial1.read(); if (c == '\n') { Serial.println("[CAMERA] " + s); s = ""; } else if (c != '\r') s += c; }

  while (Serial.available()) {
    char c = Serial.read();
    if (c == 'h') help();
    else if (c == 'a') { live = !live; Serial.printf("live data %s\n", live ? "ON" : "OFF"); }
    else if (c == 'i') i2cScan();
    else if (c == 'n') { showNmea = !showNmea; Serial.printf("NMEA lines %s\n", showNmea ? "ON" : "OFF"); }
    else if (c == 'f') flashTest();
    else if (c == 's') sdTest();
    else if (c == 'r') {
      if (!radioOk) Serial.println("[RADIO] not started (see FAIL above)");
      else { radioOn = !radioOn; Serial.printf("[RADIO] test %s\n", radioOn ? "ON: 866.50 MHz SF9 +10 dBm, antenna on ANT2!" : "OFF (silent)"); }
    }
    else if (c == 'b') beep();
    else if (c == 'c') { Serial1.println("TEST"); Serial.println("[CAMERA] sent TEST on pin 1; replies print as [CAMERA]"); }
    else if (c == 'p') { Serial3.println("B1 PING"); Serial.println("[PARACHUTE] sent B1 PING on pin 14; replies print as [PARACHUTE]"); }
    else if (c == 'x') { radioOn = false; live = false; Serial.println("radio off, live data off"); }
  }

  if (millis() - tLive < 1000) return;
  tLive = millis();
  hb = !hb;
  digitalWrite(HEARTBEAT, hb);                               // 0.5 Hz square wave to BACKUP pin 4
  digitalWrite(LED_OK, hb);
  if (live) liveData();
}
