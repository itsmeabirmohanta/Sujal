/*
 * VRITRA BOARD 4 (PARACHUTE, Teensy 4.0) — FULL BENCH TEST
 * Tests everything on Board 4, one item at a time, from the Serial Monitor.
 *
 * WHAT IS CONNECTED WHERE (Board 4 design)
 *   I2C  (Wire)  pin 18 SDA, pin 19 SCL : BMP585 #3 (0x47) + LSM6DSO32 (0x6A)
 *   GNSS         pin 0  (Serial1 RX)    : NMEA from MAIN board J8 -> J5   (listen only)
 *   MAIN link    pin 7  (Serial2 RX)    : data from MAIN board J4 -> J2 pin 2
 *                pin 8  (Serial2 TX)    : data to MAIN (J2 pin 1, via R7 1k) AND to BACKUP (J3 pin 1, via R8 1k)
 *   BACKUP link  pin 15 (Serial3 RX)    : data from BACKUP board J3 -> J3 pin 2
 *   Servos (330 R each) pin 2 J10 steer L (8 V) · pin 3 J11 steer R (8 V)
 *                       pin 4 J12 bay MG90S · pin 5 J13 petri MG90S · pin 10 J16 extra MG90S (5 V)
 *   ESC signal   pin 6  (J14, 330 R)    : Cyclone 45A BLHeli_S, 1000 us = stop
 *   Hall sensor  pin 9  (J15 OUT, 10k pull-up to 3V3) : gyro-wheel speed
 *   Power        J8 = 5 V + 3V3 logic (sensors get 3V3 ONLY from J8 pin 3)
 *                J6 = 5 V servo · J1 = 8 V steering servos
 *
 * KEYS:  h help   i I2C scan   l LSM6DSO32   b BMP585   g GNSS   u UART links
 *        v servo test (all plugs in turn)    1..5 one servo plug    s stop servos
 *        e ESC test (ONLY with the wheel secured!)   k Hall / wheel RPM   x everything off
 */
#include <Wire.h>
#include <Servo.h>

// ----- settings you may need to change -----
const uint32_t GNSS_BAUD = 9600;      // L89 GNSS output rate (same as MAIN firmware)
const uint32_t LINK_BAUD = 115200;    // MAIN / BACKUP UART link speed: must match their firmware
const uint8_t  MAGNETS   = 1;         // magnets on the gyro wheel (Hall pulses per turn)
const int      ESC_TEST_US = 1100;    // ESC test throttle (1000 = stop, keep LOW for the test)

// ----- I2C sensors -----
const uint8_t LSM_ADDR = 0x6A;        // LSM6DSO32: DO -> GND gives 0x6A
const uint8_t BMP_ADDR = 0x47;        // BMP585:    SDO -> 3V3 gives 0x47

// ----- servo plugs: pin, name -----
struct Plug { uint8_t pin; const char *name; };
const Plug SERVO_PLUG[] = {
  {2,  "J10 A20CLM steer LEFT (8 V)"},
  {3,  "J11 A20CLM steer RIGHT (8 V)"},
  {4,  "J12 MG90S #1 parachute bay (5 V)"},
  {5,  "J13 MG90S #2 petri plate (5 V)"},
  {10, "J16 MG90S #3 extra (5 V)"},
};
const uint8_t NS = sizeof(SERVO_PLUG) / sizeof(SERVO_PLUG[0]);
const uint8_t ESC_PIN = 6;            // J14 ESC signal
const uint8_t HALL_PIN = 9;           // J15 Hall OUT

Servo servo[NS], esc;

// what is running
enum Mode { NONE, LSM, BMP, GNSS, LINK, SERVO_AUTO, SERVO_ONE, ESC_TEST, HALL };
Mode mode = NONE;
uint8_t oneServo = 0;

// Hall pulse counter (interrupt)
volatile uint32_t hallPulses = 0;
void hallIsr() { hallPulses++; }

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

// ---------- I2C scan: shows which sensors answer ----------
void i2cScan() {
  Serial.println("\n[I2C] scanning pin 18 SDA / 19 SCL ...");
  int n = 0;
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  0x%02X %s\n", a, a == LSM_ADDR ? "LSM6DSO32" : a == BMP_ADDR ? "BMP585" : "(unknown)");
      n++;
    }
  }
  if (!n) Serial.println("  nothing -> check 3V3 on J8 pin 3, GND, pins 18/19");
}

// ---------- LSM6DSO32: accelerometer + gyro ----------
bool lsmStart() {
  uint8_t who = 0;
  if (!readRegs(LSM_ADDR, 0x0F, &who, 1) || who != 0x6C) {
    Serial.printf("[LSM] not found (WHO_AM_I 0x%02X, want 0x6C)\n", who);
    return false;
  }
  writeReg(LSM_ADDR, 0x12, 0x01); delay(20);  // reset
  writeReg(LSM_ADDR, 0x12, 0x44);             // safe 16-bit reads, auto address step
  writeReg(LSM_ADDR, 0x10, 0x4C);             // accel 104 Hz, +-16 g
  writeReg(LSM_ADDR, 0x11, 0x4C);             // gyro  104 Hz, +-2000 dps
  delay(50);
  Serial.println("[LSM] OK  (flat on the table: Z about +1 g)");
  return true;
}
void lsmPrint() {
  uint8_t b[14];
  if (!readRegs(LSM_ADDR, 0x20, b, 14)) { Serial.println("[LSM] read failed"); return; }
  int16_t t = b[1] << 8 | b[0], g[3], a[3];
  for (int i = 0; i < 3; i++) { g[i] = b[3 + 2 * i] << 8 | b[2 + 2 * i]; a[i] = b[9 + 2 * i] << 8 | b[8 + 2 * i]; }
  Serial.printf("[LSM] raw A %6d %6d %6d G %6d %6d %6d | A g %6.3f %6.3f %6.3f  G dps %7.1f %7.1f %7.1f  T %.1f C\n",
                a[0], a[1], a[2], g[0], g[1], g[2],
                a[0] * 0.000488f, a[1] * 0.000488f, a[2] * 0.000488f,
                g[0] * 0.070f, g[1] * 0.070f, g[2] * 0.070f, 25.0f + t / 256.0f);
}

// ---------- BMP585: pressure + temperature ----------
bool bmpStart() {
  uint8_t id = 0;
  writeReg(BMP_ADDR, 0x7E, 0xB6); delay(10);  // reset
  if (!readRegs(BMP_ADDR, 0x01, &id, 1) || id != 0x51) {
    Serial.printf("[BMP] not found (CHIP_ID 0x%02X, want 0x51)\n", id);
    return false;
  }
  writeReg(BMP_ADDR, 0x37, 0x80 | (0x14 << 2));          // standby, 25 Hz
  writeReg(BMP_ADDR, 0x36, 0x40 | (4 << 3) | 1);         // pressure on, oversampling P x16, T x2
  delay(5);
  writeReg(BMP_ADDR, 0x37, 0x80 | (0x14 << 2) | 0x01);   // run continuously
  delay(60);
  Serial.println("[BMP] OK");
  return true;
}
void bmpPrint() {
  uint8_t b[6];
  if (!readRegs(BMP_ADDR, 0x1D, b, 6)) { Serial.println("[BMP] read failed"); return; }
  int32_t rt = (int32_t)((uint32_t)b[2] << 16 | (uint32_t)b[1] << 8 | b[0]);
  if (rt & 0x800000) rt |= 0xFF000000;
  uint32_t rp = (uint32_t)b[5] << 16 | (uint32_t)b[4] << 8 | b[3];
  float tc = rt / 65536.0f, pa = rp / 64.0f;
  float alt = 44330.0f * (1.0f - powf(pa / 101325.0f, 0.1903f));
  Serial.printf("[BMP] raw T %ld P %lu | T %.2f C  P %.1f Pa  alt %.2f m\n", (long)rt, (unsigned long)rp, tc, pa, alt);
}

// ---------- servos ----------
void servosStop() {                    // 1500 us = centre (180° servo) / stop (360° servo)
  for (uint8_t i = 0; i < NS; i++) servo[i].writeMicroseconds(1500);
}

// ---------- everything off ----------
void allOff() {
  servosStop();
  esc.writeMicroseconds(1000);         // ESC stop
  mode = NONE;
  Serial.println("everything off (servos 1500 us, ESC 1000 us)");
}

void help() {
  Serial.println("\nKEYS: h help | i I2C scan | l LSM6DSO32 | b BMP585 | g GNSS | u UART links");
  Serial.println("      v all servos in turn | 1..5 one servo | s stop servos | e ESC test | k Hall RPM | x all off");
  for (uint8_t i = 0; i < NS; i++) Serial.printf("      %u = %s (pin %u)\n", i + 1, SERVO_PLUG[i].name, SERVO_PLUG[i].pin);
}

void setup() {
  Serial.begin(115200);
  uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 3000) {}
  Serial.println("\n=== VRITRA BOARD 4 full test ===");

  Wire.begin();                         // pins 18 / 19
  Wire.setClock(100000);
  Serial1.begin(GNSS_BAUD);             // GNSS in on pin 0
  Serial2.begin(LINK_BAUD);             // MAIN link: RX pin 7, TX pin 8 (TX also reaches BACKUP)
  Serial3.begin(LINK_BAUD);             // BACKUP link: RX pin 15

  for (uint8_t i = 0; i < NS; i++) servo[i].attach(SERVO_PLUG[i].pin, 1000, 2000);
  servosStop();
  esc.attach(ESC_PIN, 1000, 2000);
  esc.writeMicroseconds(1000);          // ESC sees "stop" from power-up, so it arms safely

  pinMode(HALL_PIN, INPUT);             // R6 10k already pulls it up to 3V3
  attachInterrupt(digitalPinToInterrupt(HALL_PIN), hallIsr, FALLING);

  i2cScan();
  help();
}

void loop() {
  static uint32_t tPrint = 0, tStep = 0, tLink = 0, tEsc = 0, hallLast = 0, tHall = 0;
  static uint8_t phase = 0, sel = 0;
  static bool lsmOk = false, bmpOk = false;
  uint32_t now = millis();

  // ----- keys -----
  while (Serial.available()) {
    char c = Serial.read();
    if (c == 'h') help();
    else if (c == 'i') i2cScan();
    else if (c == 'l') { lsmOk = lsmStart(); mode = LSM; }
    else if (c == 'b') { bmpOk = bmpStart(); mode = BMP; }
    else if (c == 'g') { mode = GNSS; Serial.println("[GNSS] printing NMEA from MAIN (pin 0). Nothing = check J5 cable / MAIN on"); }
    else if (c == 'u') { mode = LINK; Serial.println("[LINK] sending 'B4 PING n' on pin 8, printing what arrives on pin 7 (MAIN) and pin 15 (BACKUP)"); }
    else if (c == 'v') { servosStop(); mode = SERVO_AUTO; sel = 0; phase = 0; tStep = 0; Serial.println("[SERVO] all plugs in turn"); }
    else if (c >= '1' && c < '1' + NS) { servosStop(); oneServo = c - '1'; mode = SERVO_ONE; phase = 0; tStep = 0;
                                         Serial.printf("[SERVO] only %s\n", SERVO_PLUG[oneServo].name); }
    else if (c == 's') { servosStop(); mode = NONE; Serial.println("[SERVO] stopped"); }
    else if (c == 'e') { mode = ESC_TEST; tEsc = now; esc.writeMicroseconds(ESC_TEST_US);
                         Serial.printf("[ESC] %d us for 2 s, then stop. Wheel must be secured!\n", ESC_TEST_US); }
    else if (c == 'k') { mode = HALL; hallLast = hallPulses; tHall = now; Serial.println("[HALL] spin the wheel / pass a magnet"); }
    else if (c == 'x') allOff();
  }

  // ----- running test -----
  switch (mode) {
    case LSM:  if (lsmOk && now - tPrint >= 200) { tPrint = now; lsmPrint(); } break;
    case BMP:  if (bmpOk && now - tPrint >= 200) { tPrint = now; bmpPrint(); } break;

    case GNSS:                                    // copy GNSS sentences to the screen
      while (Serial1.available()) Serial.write(Serial1.read());
      break;

    case LINK:                                    // UART links to MAIN and BACKUP
      if (now - tLink >= 1000) {
        tLink = now;
        static uint32_t n = 0;
        Serial2.printf("B4 PING %lu\n", (unsigned long)n++);   // goes to MAIN and BACKUP
      }
      while (Serial2.available()) { Serial.print("[from MAIN]   "); Serial.write(Serial2.read()); Serial.println(); }
      while (Serial3.available()) { Serial.print("[from BACKUP] "); Serial.write(Serial3.read()); Serial.println(); }
      break;

    case SERVO_AUTO:                              // each plug: 1800 us, 1500, 1200, 1500, then next plug
    case SERVO_ONE:
      if (now - tStep >= 1200) {
        tStep = now;
        uint8_t i = (mode == SERVO_AUTO) ? sel : oneServo;
        const int seq[4] = {1800, 1500, 1200, 1500};
        servo[i].writeMicroseconds(seq[phase]);
        Serial.printf("[SERVO] %s pin %u -> %d us\n", SERVO_PLUG[i].name, SERVO_PLUG[i].pin, seq[phase]);
        if (++phase == 4) { phase = 0; if (mode == SERVO_AUTO) sel = (sel + 1) % NS; }
      }
      break;

    case ESC_TEST:                                // short low-throttle pulse, then stop
      if (now - tEsc >= 2000) { esc.writeMicroseconds(1000); mode = NONE; Serial.println("[ESC] stop (1000 us)"); }
      break;

    case HALL:                                    // count Hall pulses every second
      if (now - tHall >= 1000) {
        uint32_t p = hallPulses, d = p - hallLast;
        hallLast = p; tHall = now;
        Serial.printf("[HALL] pin 9 = %s  pulses/s %lu  wheel %lu RPM\n", digitalRead(HALL_PIN) ? "HIGH" : "LOW",
                      (unsigned long)d, (unsigned long)(d * 60UL / MAGNETS));
      }
      break;

    default: break;
  }
}
