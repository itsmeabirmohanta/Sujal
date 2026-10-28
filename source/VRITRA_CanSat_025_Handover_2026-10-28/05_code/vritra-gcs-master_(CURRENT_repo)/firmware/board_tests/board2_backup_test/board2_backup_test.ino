/*
 * VRITRA BOARD 2 (BACKUP + SENSOR, Teensy 4.1) — FULL BENCH TEST   (final etched PCB, 2026-10-27)
 *
 * At power-up every part is checked once and a PASS / FAIL table is printed.
 * Keys in the Serial Monitor (115200 baud):
 *   h help | a live data on/off | i I2C scan | n NMEA on/off | s SD card test
 *   l LSM6DSO32 diagnostic (you said it misbehaves — this shows why) | p PARACHUTE ping | x stop
 *
 * WHAT IS CONNECTED WHERE (read from kicad_single/BOARD2_BACKUP_SENSOR_1L, the board you built)
 *   I2C  pin 18 SDA / 19 SCL : BMP585 #2 0x47 · temp/humidity sensor on the M1L cable holes (HDC3020 0x44, or SHT4x 0x44)
 *                              · LSM6DSO32 0x6A (fitted, NOT used in flight — MAIN's ICM-20948 is the IMU)
 *   microSD module  SPI: pin 10 CS, 11 MOSI, 12 MISO, 13 SCK (module powered from 5 V)
 *   GNSS copy       pin 0 (Serial1 RX) <- J2.2 <- MAIN's L89 TX (listen only)
 *   Heartbeat in    pin 4 <- J2.1 <- MAIN pin 5 (0.5 Hz square wave while MAIN runs)
 *   PARACHUTE link  Serial3: pin 14 TX (R1 1k) -> J3.2,  pin 15 RX <- J3.1   (cable C11 to Board 4 J3)
 *                   Board 4 is now the ESP32-P4-Pico: with its test running it answers 'p' with "B4 PONG ..."
 *   LEDs            pin 36 green (OK), pin 37 red (fault)
 *   UART mux U10    NOT fitted (you have none). Pins 5/6/9 (mux select) and Serial8 34/35 are unused here.
 *                   2 short wires on the empty mux holes give MAIN a
 *                   direct line to the camera on J4: see firmware/vritra_cam/BOARD2_CAMERA_WIRES.png
 * Libraries: only Teensyduino's own (Wire, SPI, SD).
 */
#include <Wire.h>
#include <SPI.h>
#include <SD.h>

// ---------- pins (etched Board 2) ----------
const uint8_t SD_CS = 10, HB_IN = 4, LED_OK = 36, LED_FLT = 37;
const uint8_t MUX_A = 5, MUX_B = 6, MUX_C = 9;

// ---------- settings ----------
const uint32_t GNSS_BAUD = 9600, LINK_BAUD = 115200;
const uint8_t BMP = 0x47, LSM = 0x6A, TRH = 0x44;

bool bmpOk, sdOk, lsmOk;
enum { TRH_NONE, TRH_HDC3020, TRH_SHT4X } trhType = TRH_NONE;
float padPa = 0;
bool live = true, showNmea = false;
volatile uint32_t hbEdges = 0;
void hbIsr() { hbEdges++; }

struct Gnss { bool fix; uint8_t sats; float hdop, alt; double lat, lon; char utc[11]; uint32_t lines, bad; } gps = {};

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
bool present(uint8_t a) { Wire.beginTransmission(a); return Wire.endTransmission() == 0; }
uint8_t crc8(const uint8_t *d, int n) {                    // Sensirion / TI: poly 0x31, init 0xFF
  uint8_t c = 0xFF;
  for (int i = 0; i < n; i++) { c ^= d[i]; for (int b = 0; b < 8; b++) c = (c & 0x80) ? (c << 1) ^ 0x31 : c << 1; }
  return c;
}

void i2cScan() {
  Serial.println("\n[I2C] scan pins 18/19:");
  int n = 0;
  for (uint8_t a = 1; a < 127; a++) {
    if (!present(a)) continue;
    n++;
    Serial.printf("   0x%02X  %s\n", a, a == BMP ? "BMP585 #2" : a == LSM ? "LSM6DSO32 (not used in flight)" :
                  (a == 0x44 || a == 0x45) ? "temp/humidity (J5)" : "unknown");
  }
  if (!n) Serial.println("   nothing answers -> check 3V3_B on J1 pin 3, GND, pins 18/19");
}

// ===================================================================== BMP585 #2
bool bmpStart() {
  uint8_t id = 0;
  wr(BMP, 0x7E, 0xB6); delay(10);
  if (!rd(BMP, 0x01, &id, 1) || id != 0x51) { Serial.printf("   BMP585 CHIP_ID 0x%02X (want 0x51)\n", id); return false; }
  wr(BMP, 0x37, 0x80 | (0x14 << 2));
  wr(BMP, 0x36, 0x40 | (4 << 3) | 1);
  delay(5);
  wr(BMP, 0x37, 0x80 | (0x14 << 2) | 1);
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
  return pa > 30000 && pa < 110000;
}

// ===================================================================== temperature + humidity (J5)
bool cmd16(uint16_t c) {
  Wire.beginTransmission(TRH); Wire.write(c >> 8); Wire.write(c & 0xFF);
  return Wire.endTransmission() == 0;
}
bool read6(uint8_t *b) {
  if (Wire.requestFrom(TRH, (uint8_t)6) != 6) return false;
  for (int i = 0; i < 6; i++) b[i] = Wire.read();
  return crc8(b, 2) == b[2] && crc8(b + 3, 2) == b[5];
}
void trhDetect() {                                         // which chip is on J5?
  uint8_t b[6];
  if (!present(TRH)) { Serial.println("   nothing at 0x44 on J5"); return; }
  if (cmd16(0x3781)) {                                     // HDC3020: read manufacturer ID -> 0x3000 (TI)
    delay(2);
    if (Wire.requestFrom(TRH, (uint8_t)3) == 3) {
      for (int i = 0; i < 3; i++) b[i] = Wire.read();
      if (crc8(b, 2) == b[2] && b[0] == 0x30 && b[1] == 0x00) { trhType = TRH_HDC3020; Serial.println("   J5 = HDC3020"); return; }
    }
  }
  Wire.beginTransmission(TRH); Wire.write(0x89); Wire.endTransmission();   // SHT4x: serial number
  delay(2);
  if (read6(b)) { trhType = TRH_SHT4X; Serial.println("   J5 = SHT4x"); return; }
  Serial.println("   0x44 answers but is neither HDC3020 nor SHT4x — tell me the chip name");
}
bool trhRead(float &tc, float &rh) {
  uint8_t b[6];
  if (trhType == TRH_HDC3020) { if (!cmd16(0x2400)) return false; delay(15); }          // one-shot, low noise
  else if (trhType == TRH_SHT4X) { Wire.beginTransmission(TRH); Wire.write(0xFD); Wire.endTransmission(); delay(10); }
  else return false;
  if (!read6(b)) return false;
  uint16_t rt = b[0] << 8 | b[1], rr = b[3] << 8 | b[4];
  tc = -45.0f + 175.0f * rt / 65535.0f;
  rh = trhType == TRH_HDC3020 ? 100.0f * rr / 65535.0f : -6.0f + 125.0f * rr / 65535.0f;
  rh = constrain(rh, 0.0f, 100.0f);
  return true;
}

// ===================================================================== LSM6DSO32 (diagnostic only)
bool lsmStart() {
  uint8_t who = 0;
  if (!rd(LSM, 0x0F, &who, 1) || who != 0x6C) { Serial.printf("   LSM6DSO32 WHO_AM_I 0x%02X (want 0x6C)\n", who); return false; }
  wr(LSM, 0x12, 0x01); delay(20);                          // reset
  wr(LSM, 0x12, 0x44);                                     // block data update + auto increment
  wr(LSM, 0x10, 0x4C);                                     // accel 104 Hz +-16 g  (0.488 mg/LSB)
  wr(LSM, 0x11, 0x4C);                                     // gyro 104 Hz +-2000 dps (70 mdps/LSB)
  delay(50);
  return true;
}
void lsmDiag() {
  Serial.println("\n[LSM] 10 samples, keep the board still and flat (Z should be about +1 g, gyro near 0):");
  if (!lsmOk) lsmOk = lsmStart();
  if (!lsmOk) { Serial.println("[LSM] not answering -> check its VIN/GND, SCL/SDA, DO -> GND, CS -> 3V3"); return; }
  for (int k = 0; k < 10; k++) {
    uint8_t s = 0, b[12];
    rd(LSM, 0x1E, &s, 1);                                  // STATUS: bit0 accel ready, bit1 gyro ready
    if (!rd(LSM, 0x22, b, 12)) { Serial.println("[LSM] read failed (I2C error)"); delay(100); continue; }
    int16_t g[3], a[3];
    for (int i = 0; i < 3; i++) { g[i] = b[2 * i + 1] << 8 | b[2 * i]; a[i] = b[2 * i + 7] << 8 | b[2 * i + 6]; }
    float gx = a[0] * 0.000488f, gy = a[1] * 0.000488f, gz = a[2] * 0.000488f;
    Serial.printf("  status %02X  A g %6.3f %6.3f %6.3f (|a| %.3f)  G dps %7.1f %7.1f %7.1f\n", s, gx, gy, gz,
                  sqrtf(gx * gx + gy * gy + gz * gz), g[0] * 0.07f, g[1] * 0.07f, g[2] * 0.07f);
    delay(100);
  }
  Serial.println("[LSM] good part: |a| = 0.95..1.05 g, gyro within +-3 dps. Anything else = faulty part or joint.");
}

// ===================================================================== SD module (SPI pins 10-13)
void sdTest() {
  if (!sdOk) { sdOk = SD.begin(SD_CS); if (!sdOk) { Serial.println("[SD] FAIL: no card / not FAT32 / module not on 5 V"); return; } }
  char out[40];
  snprintf(out, sizeof out, "VRITRA B2 SD test %lu", (unsigned long)millis());
  SD.remove("B2TEST.TXT");
  File f = SD.open("B2TEST.TXT", FILE_WRITE);
  if (!f) { Serial.println("[SD] FAIL: cannot create B2TEST.TXT"); return; }
  f.println(out); f.close();
  f = SD.open("B2TEST.TXT");
  String in = f ? f.readStringUntil('\n') : "";
  if (f) f.close();
  in.trim();
  Serial.printf("[SD] wrote \"%s\", read \"%s\" -> %s\n", out, in.c_str(), in == out ? "PASS" : "FAIL");
}

// ===================================================================== GNSS copy on pin 0
bool nmeaChecksum(const char *s) {
  const char *star = strchr(s, '*');
  if (s[0] != '$' || !star) return false;
  uint8_t c = 0;
  for (const char *p = s + 1; p < star; p++) c ^= *p;
  return c == strtoul(star + 1, nullptr, 16);
}
double nmeaDeg(const char *f, char hemi) {
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
  char *f[16] = {}; int n = 0;
  for (char *p = s; p && n < 16; n++) { f[n] = p; p = strchr(p, ','); if (p) *p++ = 0; }
  if (n < 10) return;
  strncpy(gps.utc, f[1], 10);
  gps.fix = atoi(f[6]) > 0; gps.sats = atoi(f[7]); gps.hdop = atof(f[8]); gps.alt = atof(f[9]);
  if (gps.fix) { gps.lat = nmeaDeg(f[2], f[3][0]); gps.lon = nmeaDeg(f[4], f[5][0]); }
}
void gnssPump() {
  static char line[100]; static uint8_t pos = 0;
  while (Serial1.available()) {
    char c = Serial1.read();
    if (c == '$') pos = 0;
    if (c == '\n' || c == '\r') { if (pos) { line[pos] = 0; gnssLine(line); } pos = 0; }
    else if (pos < sizeof line - 1) line[pos++] = c;
  }
}

// ===================================================================== live
void liveData() {
  static uint32_t lastEdges = 0;
  float tc, pa, t2, rh;
  if (bmpOk && bmpRead(tc, pa)) {
    if (padPa == 0) padPa = pa;
    Serial.printf("BMP585#2 %.2f C  %.1f Pa  alt %.2f m from boot\n", tc, pa, 44330.0f * (1.0f - powf(pa / padPa, 0.1903f)));
  } else Serial.println("BMP585#2 no data");
  if (trhRead(t2, rh)) Serial.printf("T/RH     %.2f C  %.1f %%RH (%s)\n", t2, rh, trhType == TRH_HDC3020 ? "HDC3020" : "SHT4x");
  else Serial.println("T/RH     no data");
  uint32_t e = hbEdges, d = e - lastEdges;
  lastEdges = e;
  Serial.printf("HEARTBEAT from MAIN: %lu edges/s (%s)\n", (unsigned long)d, d ? "MAIN alive" : "none -> MAIN off / J2 cable");
  Serial.printf("GNSS copy %s sats %u hdop %.1f UTC %s lat %.6f lon %.6f (lines %lu, bad %lu)\n\n",
                gps.fix ? "FIX" : "no fix", gps.sats, gps.hdop, gps.utc, gps.lat, gps.lon,
                (unsigned long)gps.lines, (unsigned long)gps.bad);
}

void help() {
  Serial.println("\nKEYS: h help | a live on/off | i I2C scan | n NMEA on/off | s SD test | l LSM diagnostic | p PARACHUTE ping | x stop\n");
}
void result(const char *name, bool ok) { Serial.printf("  %-34s %s\n", name, ok ? "PASS" : "FAIL"); }

void setup() {
  Serial.begin(115200);
  uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 4000) {}
  Serial.println("\n=== VRITRA BOARD 2 (BACKUP + SENSOR) full test ===");

  pinMode(LED_OK, OUTPUT); pinMode(LED_FLT, OUTPUT);
  digitalWrite(LED_OK, HIGH); digitalWrite(LED_FLT, HIGH);
  pinMode(MUX_A, INPUT); pinMode(MUX_B, INPUT); pinMode(MUX_C, INPUT);   // mux not fitted: leave alone
  pinMode(HB_IN, INPUT_PULLDOWN);
  attachInterrupt(digitalPinToInterrupt(HB_IN), hbIsr, CHANGE);

  Wire.begin(); Wire.setClock(100000);
  Serial1.begin(GNSS_BAUD);                                 // GNSS copy (RX pin 0 only)
  Serial3.begin(LINK_BAUD);                                 // PARACHUTE link

  i2cScan();
  Serial.println("\n[CHECKS]");
  bmpOk = bmpStart();
  trhDetect();
  lsmOk = lsmStart();
  sdOk = SD.begin(SD_CS);

  Serial.println("\n[RESULT]");
  result("BMP585 #2 (0x47)", bmpOk);
  result("temp/humidity (M1L, 0x44)", trhType != TRH_NONE);
  result("microSD module (pins 10-13)", sdOk);
  Serial.printf("  %-34s %s\n", "LSM6DSO32 (0x6A, not used)", lsmOk ? "answers (press l to judge it)" : "no answer");
  digitalWrite(LED_FLT, (bmpOk && trhType != TRH_NONE && sdOk) ? LOW : HIGH);
  Serial.println("  Heartbeat + GNSS copy: watch the live lines (MAIN must be running)");
  help();
}

void loop() {
  static uint32_t tLive = 0;
  gnssPump();
  while (Serial3.available()) { static String s; char c = Serial3.read(); if (c == '\n') { Serial.println("[PARACHUTE] " + s); s = ""; } else if (c != '\r') s += c; }

  while (Serial.available()) {
    char c = Serial.read();
    if (c == 'h') help();
    else if (c == 'a') { live = !live; Serial.printf("live data %s\n", live ? "ON" : "OFF"); }
    else if (c == 'i') i2cScan();
    else if (c == 'n') { showNmea = !showNmea; Serial.printf("NMEA lines %s\n", showNmea ? "ON" : "OFF"); }
    else if (c == 's') sdTest();
    else if (c == 'l') lsmDiag();
    else if (c == 'p') { Serial3.println("B2 PING"); Serial.println("[PARACHUTE] sent B2 PING on pin 14; expect [PARACHUTE] B4 PONG B2 PING ..."); }
    else if (c == 'x') { live = false; Serial.println("live data off"); }
  }

  if (millis() - tLive < 1000) return;
  tLive = millis();
  digitalWrite(LED_OK, !digitalRead(LED_OK));
  if (live) liveData();
}
