/*
 * VRITRA BOARD 4 (PARACHUTE, Waveshare ESP32-P4-Pico) — FULL BENCH TEST   (zero board, 2026-10-28)
 * Replaces the Teensy 4.0 version (the etched Board 4 shorted). Same circuit, new MCU pins.
 *
 * At power-up the sensors are checked once and a PASS / FAIL table is printed.
 * Then type a key in the Serial Monitor (115200 baud):
 *   h help   i I2C scan   l LSM6DSO32   b BMP585   g GNSS   u UART links (PING every 1 s)
 *   v all servo plugs in turn   1..5 one servo plug   s centre/stop servos
 *   p petri plate: OPEN, hold 10 s, CLOSE (same as the 950 m event in flight)
 *   e ESC test (ONLY with the wheel secured!)   k Hall / wheel RPM   x everything off
 * Board 4 also ANSWERS by itself: "B1 PING" from MAIN or "B2 PING" from BACKUP -> "B4 PONG ..." on TX2.
 *
 * WHAT IS CONNECTED WHERE (kicad_single/BOARD4_PARACHUTE_PICO, P4 header pin -> GPIO)
 *   I2C          pin 17 GPIO7 SDA, pin 16 GPIO8 SCL : BMP585 #3 (0x47) + LSM6DSO32 (0x6A)
 *                (the P4-Pico's own ES8311 audio codec also answers at 0x18 on this bus: that is normal)
 *   GNSS in      pin 9  GPIO14 (Serial1 RX)  <- MAIN J8 -> J5   (listen only)
 *   MAIN link    pin 2  GPIO19 (Serial2 RX)  <- J2 pin 2 (MAIN TX3)
 *                pin 1  GPIO54 (Serial2 TX)  -> 1k R7 -> J2 pin 1 (MAIN)  AND  1k R8 -> J3 pin 1 (BACKUP)
 *   BACKUP link  pin 6  GPIO16 (Serial3 RX)  <- J3 pin 2 (BACKUP TX3)
 *   Servos       pin 35 GPIO20 -> 330R R1 -> J10 B26CLM steer LEFT  (steering rail, max 8.4 V!)
 *                pin 34 GPIO21 -> 330R R2 -> J11 B26CLM steer RIGHT
 *                pin 32 GPIO22 -> 330R R3 -> J12 MG90S parachute bay (5 V servo rail)
 *                pin 31 GPIO23 -> 330R R4 -> J13 MG90S petri plate
 *                pin 26 GPIO32 -> 330R R9 -> J16 MG90S extra
 *   ESC signal   pin 25 GPIO33 -> 330R R5 -> J14   (Cyclone 45A BLHeli_S, 1000 us = stop)
 *   Hall sensor  pin 11 GPIO5  <- J15 OUT (R6 10k pull-up to 3V3_B)
 *   Power        J8: +5 V -> P4 pin 39 VSYS, +3V3_B -> sensors only (P4 pin 36 3V3 OUT stays unconnected)
 *                J6: 5 V servo rail · J1: steering rail (5.0-8.4 V for the B26CLM, NOT 9 V)
 *
 * ARDUINO SETTINGS: board "ESP32P4 Dev Module" (esp32 core 3.3.x). USB CDC On Boot: Disabled
 *   (the P4-Pico's USB-C goes through a CH343 USB-serial chip to UART0 = Serial).
 *   Chip Variant: esptool prints "Chip is ESP32-P4 (revision v1.x)" while uploading -> "Before v3.00";
 *   revision v3.x -> "v3.00 or newer". The wrong choice uploads but then reboots in a loop.
 */
#include <Wire.h>

// ----- pins (P4 GPIO numbers) -----
const int PIN_SDA = 7, PIN_SCL = 8;
const int PIN_GNSS_RX = 14;
const int PIN_MAIN_RX = 19, PIN_LINK_TX = 54;      // TX reaches MAIN and BACKUP
const int PIN_BACKUP_RX = 16;
const int PIN_ESC = 33, PIN_HALL = 5;

// ----- settings you may need to change -----
const uint32_t GNSS_BAUD = 9600;      // L89 GNSS output rate (same as MAIN firmware)
const uint32_t LINK_BAUD = 115200;    // MAIN / BACKUP UART link speed: must match their firmware
const uint8_t  MAGNETS   = 1;         // magnets on the gyro wheel (Hall pulses per turn)
const int      ESC_STOP_US = 1000, ESC_TEST_US = 1100;   // keep the test throttle LOW
// petri plate positions — set these to YOUR lid after watching the test
const int      PETRI_OPEN_US  = 2000;
const int      PETRI_CLOSE_US = 1000;
const uint32_t PETRI_OPEN_MS  = 10000; // stays open 10 s

// ----- servo PWM (LEDC hardware PWM, 50 Hz, 14 bit) -----
const uint32_t SERVO_HZ = 50, PERIOD_US = 1000000UL / SERVO_HZ;
const uint8_t  PWM_BITS = 14;

// ----- I2C sensors -----
const uint8_t LSM_ADDR = 0x6A;        // LSM6DSO32: DO -> GND gives 0x6A
const uint8_t BMP_ADDR = 0x47;        // BMP585:    SDO -> 3V3 gives 0x47
const uint8_t CODEC_ADDR = 0x18;      // ES8311 on the P4-Pico itself

// ----- servo plugs: GPIO, centre pulse, name -----
struct Plug { int pin; int centreUs; const char *name; };
const Plug SERVO_PLUG[] = {
  {20, 1520, "J10 B26CLM steer LEFT"},       // AGFRC B26CLM: centre 1520 us
  {21, 1520, "J11 B26CLM steer RIGHT"},
  {22, 1500, "J12 MG90S #1 parachute bay"},
  {23, 1500, "J13 MG90S #2 petri plate"},
  {32, 1500, "J16 MG90S #3 extra"},
};
const uint8_t NS = sizeof(SERVO_PLUG) / sizeof(SERVO_PLUG[0]);
const uint8_t PETRI = 3;              // index of J13 in SERVO_PLUG

HardwareSerial &gnss   = Serial1;
HardwareSerial &mainLk = Serial2;
HardwareSerial &backLk = Serial3;

enum Mode { NONE, LSM, BMP, GNSS, LINK, SERVO_AUTO, SERVO_ONE, ESC_TEST, HALL, PETRI_TEST };
Mode mode = NONE;
uint8_t oneServo = 0;
bool lsmOk = false, bmpOk = false, pwmOk = true;
float padPa = 0;                      // pressure at the first BMP reading = 0 m

volatile uint32_t hallPulses = 0;
void IRAM_ATTR hallIsr() { hallPulses = hallPulses + 1; }

// ---------- PWM ----------
void pulse(int pin, int us) {
  ledcWrite(pin, (uint32_t)us * ((1UL << PWM_BITS) - 1) / PERIOD_US);
}
bool pwmStart(int pin, int us) {
  if (!ledcAttach(pin, SERVO_HZ, PWM_BITS)) { Serial.printf("   PWM on GPIO%d FAILED\n", pin); return false; }
  pulse(pin, us);
  return true;
}
void servosCentre() {                 // centre (180° servo) / stop (360° servo)
  for (uint8_t i = 0; i < NS; i++) pulse(SERVO_PLUG[i].pin, SERVO_PLUG[i].centreUs);
}

// ---------- small I2C helpers ----------
bool writeReg(uint8_t a, uint8_t r, uint8_t v) {
  Wire.beginTransmission(a); Wire.write(r); Wire.write(v);
  return Wire.endTransmission() == 0;
}
bool readRegs(uint8_t a, uint8_t r, uint8_t *buf, uint8_t n) {
  Wire.beginTransmission(a); Wire.write(r);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(a, n) != n) return false;
  for (uint8_t i = 0; i < n; i++) buf[i] = Wire.read();
  return true;
}

void i2cScan() {
  Serial.println("\n[I2C] scanning GPIO7 SDA / GPIO8 SCL ...");
  int n = 0;
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  0x%02X %s\n", a, a == LSM_ADDR ? "LSM6DSO32" : a == BMP_ADDR ? "BMP585" :
                    a == CODEC_ADDR ? "ES8311 codec on the P4-Pico (normal)" : "(unknown)");
      n++;
    }
  }
  if (!n) Serial.println("  nothing -> check 3V3_B on J8 pin 3, GND, P4 pins 16/17");
}

// ---------- LSM6DSO32: accelerometer + gyro ----------
bool lsmStart() {
  uint8_t who = 0;
  if (!readRegs(LSM_ADDR, 0x0F, &who, 1) || who != 0x6C) {
    Serial.printf("   LSM6DSO32 WHO_AM_I 0x%02X (want 0x6C)\n", who);
    return false;
  }
  writeReg(LSM_ADDR, 0x12, 0x01); delay(20);  // reset
  writeReg(LSM_ADDR, 0x12, 0x44);             // safe 16-bit reads, auto address step
  writeReg(LSM_ADDR, 0x10, 0x4C);             // accel 104 Hz, +-16 g
  writeReg(LSM_ADDR, 0x11, 0x4C);             // gyro  104 Hz, +-2000 dps
  delay(50);
  return true;
}
void lsmPrint() {
  uint8_t b[14];
  if (!readRegs(LSM_ADDR, 0x20, b, 14)) { Serial.println("[LSM] read failed"); return; }
  int16_t t = b[1] << 8 | b[0], g[3], a[3];
  for (int i = 0; i < 3; i++) { g[i] = b[3 + 2 * i] << 8 | b[2 + 2 * i]; a[i] = b[9 + 2 * i] << 8 | b[8 + 2 * i]; }
  float ax = a[0] * 0.000488f, ay = a[1] * 0.000488f, az = a[2] * 0.000488f;
  Serial.printf("[LSM] A g %6.3f %6.3f %6.3f (|a| %.3f)  G dps %7.1f %7.1f %7.1f  T %.1f C\n",
                ax, ay, az, sqrtf(ax * ax + ay * ay + az * az),
                g[0] * 0.070f, g[1] * 0.070f, g[2] * 0.070f, 25.0f + t / 256.0f);
}

// ---------- BMP585: pressure + temperature ----------
bool bmpStart() {
  uint8_t id = 0;
  writeReg(BMP_ADDR, 0x7E, 0xB6); delay(10);  // reset
  if (!readRegs(BMP_ADDR, 0x01, &id, 1) || id != 0x51) {
    Serial.printf("   BMP585 CHIP_ID 0x%02X (want 0x51)\n", id);
    return false;
  }
  writeReg(BMP_ADDR, 0x37, 0x80 | (0x14 << 2));          // standby, 25 Hz
  writeReg(BMP_ADDR, 0x36, 0x40 | (4 << 3) | 1);         // pressure on, oversampling P x16, T x2
  delay(5);
  writeReg(BMP_ADDR, 0x37, 0x80 | (0x14 << 2) | 0x01);   // run continuously
  delay(100);
  return true;
}
void bmpPrint() {
  uint8_t b[6];
  if (!readRegs(BMP_ADDR, 0x1D, b, 6)) { Serial.println("[BMP] read failed"); return; }
  int32_t rt = (int32_t)((uint32_t)b[2] << 16 | (uint32_t)b[1] << 8 | b[0]);
  if (rt & 0x800000) rt |= 0xFF000000;
  uint32_t rp = (uint32_t)b[5] << 16 | (uint32_t)b[4] << 8 | b[3];
  float tc = rt / 65536.0f, pa = rp / 64.0f;
  if (pa < 30000 || pa > 110000) { Serial.printf("[BMP] impossible pressure %.1f Pa\n", pa); return; }
  if (padPa == 0) padPa = pa;
  float alt = 44330.0f * (1.0f - powf(pa / padPa, 0.1903f));
  Serial.printf("[BMP] T %.2f C  P %.1f Pa  alt %.2f m from boot (standard formula, not calibrated yet)\n", tc, pa, alt);
}

// ---------- UART links: print lines, answer pings ----------
void linkPump(HardwareSerial &s, const char *from, String &buf) {
  while (s.available()) {
    char c = s.read();
    if (c == '\r') continue;
    if (c != '\n') { if (buf.length() < 120) buf += c; continue; }
    if (mode == LINK || buf.indexOf("PING") >= 0) Serial.printf("[from %s] %s\n", from, buf.c_str());
    if (buf.startsWith("B1 PING") || buf.startsWith("B2 PING")) {
      mainLk.printf("B4 PONG %s (uptime %lu s)\n", buf.c_str(), (unsigned long)(millis() / 1000));
      Serial.printf("[LINK] answered %s with B4 PONG on GPIO54\n", buf.substring(0, 2).c_str());
    }
    buf = "";
  }
}

// ---------- everything off ----------
void allOff() {
  servosCentre();
  pulse(PIN_ESC, ESC_STOP_US);
  mode = NONE;
  Serial.println("everything off (servos centred, ESC 1000 us)");
}

void help() {
  Serial.println("\nKEYS: h help | i I2C scan | l LSM6DSO32 | b BMP585 | g GNSS | u UART links");
  Serial.println("      v all servos in turn | 1..5 one servo | s centre servos | p petri 10 s | e ESC test | k Hall RPM | x all off");
  for (uint8_t i = 0; i < NS; i++)
    Serial.printf("      %u = %s (GPIO%d, centre %d us)\n", i + 1, SERVO_PLUG[i].name, SERVO_PLUG[i].pin, SERVO_PLUG[i].centreUs);
}
void result(const char *name, bool ok) { Serial.printf("  %-34s %s\n", name, ok ? "PASS" : "FAIL"); }

void setup() {
  // servos and ESC get a safe pulse before anything else, so nothing twitches or arms at boot
  for (uint8_t i = 0; i < NS; i++) pwmOk &= pwmStart(SERVO_PLUG[i].pin, SERVO_PLUG[i].centreUs);
  pwmOk &= pwmStart(PIN_ESC, ESC_STOP_US);               // ESC sees "stop" from power-up, so it arms safely

  Serial.begin(115200);
  delay(1500);                                           // time to open the Serial Monitor
  Serial.println("\n=== VRITRA BOARD 4 (PARACHUTE, ESP32-P4-Pico) full test ===");

  Wire.begin(PIN_SDA, PIN_SCL, 100000);                  // 100 kHz like the other boards
  gnss.begin(GNSS_BAUD, SERIAL_8N1, PIN_GNSS_RX, -1);
  mainLk.begin(LINK_BAUD, SERIAL_8N1, PIN_MAIN_RX, PIN_LINK_TX);
  backLk.begin(LINK_BAUD, SERIAL_8N1, PIN_BACKUP_RX, -1);

  pinMode(PIN_HALL, INPUT);                              // R6 10k already pulls it up to 3V3
  attachInterrupt(digitalPinToInterrupt(PIN_HALL), hallIsr, FALLING);

  i2cScan();
  Serial.println("\n[CHECKS]");
  bmpOk = bmpStart();
  lsmOk = lsmStart();
  Serial.println("\n[RESULT]");
  result("BMP585 #3 (0x47)", bmpOk);
  result("LSM6DSO32 (0x6A)", lsmOk);
  result("servo + ESC PWM (6 LEDC outputs)", pwmOk);
  Serial.println("  GNSS / links / servos / Hall: use the keys below");
  help();
}

void loop() {
  static uint32_t tPrint = 0, tStep = 0, tLink = 0, tEsc = 0, hallLast = 0, tHall = 0;
  static uint8_t phase = 0, sel = 0;
  static String mainBuf, backBuf;
  uint32_t now = millis();

  // links are always read, so pings from MAIN / BACKUP are answered in every mode
  linkPump(mainLk, "MAIN", mainBuf);
  linkPump(backLk, "BACKUP", backBuf);

  // ----- keys -----
  while (Serial.available()) {
    char c = Serial.read();
    if (c == 'h') help();
    else if (c == 'i') i2cScan();
    else if (c == 'l') { lsmOk = lsmStart(); mode = LSM; Serial.println(lsmOk ? "[LSM] flat on the table: Z about +1 g" : "[LSM] not answering"); }
    else if (c == 'b') { bmpOk = bmpStart(); padPa = 0; mode = BMP; Serial.println(bmpOk ? "[BMP] OK" : "[BMP] not answering"); }
    else if (c == 'g') { mode = GNSS; Serial.println("[GNSS] printing NMEA from MAIN (GPIO14). Nothing = check J5 cable / MAIN on"); }
    else if (c == 'u') { mode = LINK; Serial.println("[LINK] sending 'B4 PING n' on GPIO54 every 1 s, printing what arrives from MAIN (GPIO19) and BACKUP (GPIO16)"); }
    else if (c == 'v') { servosCentre(); mode = SERVO_AUTO; sel = 0; phase = 0; tStep = 0; Serial.println("[SERVO] all plugs in turn"); }
    else if (c >= '1' && c < '1' + NS) { servosCentre(); oneServo = c - '1'; mode = SERVO_ONE; phase = 0; tStep = 0;
                                         Serial.printf("[SERVO] only %s\n", SERVO_PLUG[oneServo].name); }
    else if (c == 's') { servosCentre(); mode = NONE; Serial.println("[SERVO] centred"); }
    else if (c == 'e') { mode = ESC_TEST; tEsc = now; pulse(PIN_ESC, ESC_TEST_US);
                         Serial.printf("[ESC] %d us for 2 s, then stop. Wheel must be secured!\n", ESC_TEST_US); }
    else if (c == 'p') { servosCentre(); mode = PETRI_TEST; tStep = now; pulse(SERVO_PLUG[PETRI].pin, PETRI_OPEN_US);
                         Serial.printf("[PETRI] OPEN (%d us) for %lu s\n", PETRI_OPEN_US, (unsigned long)(PETRI_OPEN_MS / 1000)); }
    else if (c == 'k') { mode = HALL; hallLast = hallPulses; tHall = now; Serial.println("[HALL] spin the wheel / pass a magnet"); }
    else if (c == 'x') allOff();
  }

  // ----- running test -----
  switch (mode) {
    case LSM:  if (lsmOk && now - tPrint >= 200) { tPrint = now; lsmPrint(); } break;
    case BMP:  if (bmpOk && now - tPrint >= 200) { tPrint = now; bmpPrint(); } break;

    case GNSS:                                    // copy GNSS sentences to the screen
      while (gnss.available()) Serial.write(gnss.read());
      break;

    case LINK:                                    // UART links to MAIN and BACKUP
      if (now - tLink >= 1000) {
        tLink = now;
        static uint32_t n = 0;
        mainLk.printf("B4 PING %lu\n", (unsigned long)n++);   // reaches MAIN and BACKUP
      }
      break;

    case SERVO_AUTO:                              // each plug: centre+300, centre, centre-300, centre, next plug
    case SERVO_ONE:
      if (now - tStep >= 1200) {
        tStep = now;
        uint8_t i = (mode == SERVO_AUTO) ? sel : oneServo;
        const int c0 = SERVO_PLUG[i].centreUs;
        const int seq[4] = {c0 + 300, c0, c0 - 300, c0};
        pulse(SERVO_PLUG[i].pin, seq[phase]);
        Serial.printf("[SERVO] %s GPIO%d -> %d us\n", SERVO_PLUG[i].name, SERVO_PLUG[i].pin, seq[phase]);
        if (++phase == 4) { phase = 0; if (mode == SERVO_AUTO) sel = (sel + 1) % NS; }
      }
      break;

    case PETRI_TEST:                              // open, wait 10 s, close
      if (now - tStep >= PETRI_OPEN_MS) {
        pulse(SERVO_PLUG[PETRI].pin, PETRI_CLOSE_US);
        mode = NONE;
        Serial.printf("[PETRI] CLOSE (%d us)\n", PETRI_CLOSE_US);
      }
      break;

    case ESC_TEST:                                // short low-throttle pulse, then stop
      if (now - tEsc >= 2000) { pulse(PIN_ESC, ESC_STOP_US); mode = NONE; Serial.println("[ESC] stop (1000 us)"); }
      break;

    case HALL:                                    // count Hall pulses every second
      if (now - tHall >= 1000) {
        uint32_t p = hallPulses, d = p - hallLast;
        hallLast = p; tHall = now;
        Serial.printf("[HALL] GPIO5 = %s  pulses/s %lu  wheel %lu RPM\n", digitalRead(PIN_HALL) ? "HIGH" : "LOW",
                      (unsigned long)d, (unsigned long)(d * 60UL / MAGNETS));
      }
      break;

    default: break;
  }
}
