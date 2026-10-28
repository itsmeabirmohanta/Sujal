/*
 * VRITRA Board 4 (PARACHUTE, Teensy 4.0) — bench test
 *   - I2C scan of Wire (pins 18 SDA / 19 SCL)
 *   - LSM6DSO32 @0x6A: raw accel / gyro / temperature + scaled values
 *   - BMP585    @0x47: raw temperature / pressure + °C / Pa / altitude
 *   - one servo sweeping continuously (default J12 = MG90S #1, Teensy pin 4)
 * Serial monitor (any baud). Keys: 's' = stop/start servo, 'c' = centre servo,
 *                                  '2'..'5' / 'x' = choose servo pin 2,3,4,5 / 10 (see table below).
 * Only Teensyduino built-in libraries are used (Wire, Servo).
 *
 * Board 4 servo plugs (− + S):  J10 STEER L = pin 2 (8 V)   J11 STEER R = pin 3 (8 V)
 *                               J12 BAY     = pin 4 (5 V)   J13 PETRI   = pin 5 (5 V)   J16 EXTRA = pin 10 (5 V)
 */
#include <Wire.h>
#include <Servo.h>

// ---------------- pins / addresses (Board 4 design) ----------------
static uint8_t servoPin = 4;           // J12 MG90S #1 (5 V servo rail from J6)
const uint8_t LSM_ADDR  = 0x6A;        // LSM6DSO32, DO -> GND
const uint8_t BMP_ADDR  = 0x47;        // BMP585, SDO -> 3V3

// servo sweep (MG90S safe range 1000..2000 us; widen slowly if you need more travel)
const int SERVO_MIN_US = 1000, SERVO_MAX_US = 2000, SERVO_STEP_US = 10, SERVO_STEP_MS = 15;

// ---------------- LSM6DSO32 registers ----------------
const uint8_t LSM_WHO_AM_I = 0x0F, LSM_WHO_VALUE = 0x6C;
const uint8_t LSM_CTRL1_XL = 0x10, LSM_CTRL2_G = 0x11, LSM_CTRL3_C = 0x12;
const uint8_t LSM_STATUS = 0x1E, LSM_OUT_TEMP_L = 0x20;      // 0x20..0x2D: temp, gyro XYZ, accel XYZ
const float   LSM_ACC_MG_PER_LSB  = 0.488f;   // +-16 g
const float   LSM_GYR_MDPS_PER_LSB = 70.0f;   // +-2000 dps

// ---------------- BMP585 registers ----------------
const uint8_t BMP_CHIP_ID = 0x01, BMP_CHIP_VALUE = 0x51;
const uint8_t BMP_TEMP_XLSB = 0x1D, BMP_STATUS = 0x28;
const uint8_t BMP_OSR_CONFIG = 0x36, BMP_ODR_CONFIG = 0x37, BMP_CMD = 0x7E;

Servo servo;
bool servoRunning = true, lsmOk = false, bmpOk = false;
int servoUs = 1500, servoDir = +1;
uint32_t lastStep = 0, lastPrint = 0;

// ---------------- I2C helpers ----------------
bool writeReg(uint8_t addr, uint8_t reg, uint8_t val) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

bool readRegs(uint8_t addr, uint8_t reg, uint8_t *buf, uint8_t n) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;          // repeated start
  if (Wire.requestFrom(addr, n) != n) return false;
  for (uint8_t i = 0; i < n; i++) buf[i] = Wire.read();
  return true;
}

void i2cScan() {
  Serial.println("I2C scan (Wire, SDA 18 / SCL 19):");
  int found = 0;
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  found 0x%02X%s\n", a, a == LSM_ADDR ? "  <- LSM6DSO32" : a == BMP_ADDR ? "  <- BMP585" : "");
      found++;
    }
  }
  if (!found) Serial.println("  nothing found -> check 3V3 on J8 pin 3, GND, SDA/SCL trails");
}

// ---------------- LSM6DSO32 ----------------
bool lsmBegin() {
  uint8_t who = 0;
  if (!readRegs(LSM_ADDR, LSM_WHO_AM_I, &who, 1)) { Serial.println("LSM6DSO32: no answer at 0x6A"); return false; }
  Serial.printf("LSM6DSO32 WHO_AM_I = 0x%02X (expect 0x6C)\n", who);
  if (who != LSM_WHO_VALUE) return false;
  writeReg(LSM_ADDR, LSM_CTRL3_C, 0x01);                       // software reset
  delay(20);
  bool ok = writeReg(LSM_ADDR, LSM_CTRL3_C, 0x44)              // BDU + register auto-increment
         && writeReg(LSM_ADDR, LSM_CTRL1_XL, 0x4C)             // accel 104 Hz, +-16 g
         && writeReg(LSM_ADDR, LSM_CTRL2_G, 0x4C);             // gyro  104 Hz, +-2000 dps
  delay(50);
  return ok;
}

bool lsmRead(int16_t &t, int16_t g[3], int16_t a[3]) {
  uint8_t b[14];
  if (!readRegs(LSM_ADDR, LSM_OUT_TEMP_L, b, 14)) return false;
  t = (int16_t)(b[1] << 8 | b[0]);
  for (int i = 0; i < 3; i++) {
    g[i] = (int16_t)(b[3 + 2 * i] << 8 | b[2 + 2 * i]);
    a[i] = (int16_t)(b[9 + 2 * i] << 8 | b[8 + 2 * i]);
  }
  return true;
}

// ---------------- BMP585 ----------------
bool bmpBegin() {
  uint8_t id = 0;
  writeReg(BMP_ADDR, BMP_CMD, 0xB6);                           // soft reset
  delay(10);
  if (!readRegs(BMP_ADDR, BMP_CHIP_ID, &id, 1)) { Serial.println("BMP585: no answer at 0x47"); return false; }
  Serial.printf("BMP585 CHIP_ID = 0x%02X (expect 0x51)\n", id);
  if (id != BMP_CHIP_VALUE) return false;
  uint8_t st = 0;
  readRegs(BMP_ADDR, BMP_STATUS, &st, 1);
  Serial.printf("BMP585 STATUS = 0x%02X (bit1 = NVM ready)\n", st);
  bool ok = writeReg(BMP_ADDR, BMP_ODR_CONFIG, 0x80 | (0x14 << 2) | 0x00)   // standby, 25 Hz, deep-standby off
         && writeReg(BMP_ADDR, BMP_OSR_CONFIG, 0x40 | (4 << 3) | 1);         // pressure on, OSR_P x16, OSR_T x2
  delay(5);
  ok = ok && writeReg(BMP_ADDR, BMP_ODR_CONFIG, 0x80 | (0x14 << 2) | 0x01); // normal mode
  delay(60);
  return ok;
}

bool bmpRead(int32_t &rawT, uint32_t &rawP) {
  uint8_t b[6];
  if (!readRegs(BMP_ADDR, BMP_TEMP_XLSB, b, 6)) return false;
  rawT = (int32_t)((uint32_t)b[2] << 16 | (uint32_t)b[1] << 8 | b[0]);
  if (rawT & 0x800000) rawT |= 0xFF000000;                     // sign-extend 24 bit
  rawP = (uint32_t)b[5] << 16 | (uint32_t)b[4] << 8 | b[3];
  return true;
}

// ---------------- servo ----------------
void attachServo(uint8_t pin) {
  servo.detach();
  servoPin = pin;
  servo.attach(servoPin, 500, 2500);
  servo.writeMicroseconds(servoUs);
  Serial.printf("servo on pin %u\n", servoPin);
}

void handleKeys() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == 's') { servoRunning = !servoRunning; Serial.println(servoRunning ? "servo: sweeping" : "servo: stopped"); }
    else if (c == 'c') { servoRunning = false; servoUs = 1500; servo.writeMicroseconds(servoUs); Serial.println("servo: centre 1500 us"); }
    else if (c >= '2' && c <= '5') attachServo(c - '0');
    else if (c == 'x') attachServo(10);
  }
}

void setup() {
  Serial.begin(115200);
  uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 3000) {}
  Serial.println("\nVRITRA Board 4 test — Teensy 4.0");
  Wire.begin();
  Wire.setClock(100000);                                       // 100 kHz, same as the flight firmware
  i2cScan();
  lsmOk = lsmBegin();
  bmpOk = bmpBegin();
  Serial.printf("LSM6DSO32 %s, BMP585 %s\n", lsmOk ? "OK" : "FAIL", bmpOk ? "OK" : "FAIL");
  attachServo(servoPin);
  Serial.println("keys: s stop/start  c centre  2/3/4/5/x servo pin 2/3/4/5/10\n");
}

void loop() {
  handleKeys();
  uint32_t now = millis();

  if (servoRunning && now - lastStep >= SERVO_STEP_MS) {      // continuous sweep, never blocks the sensors
    lastStep = now;
    servoUs += servoDir * SERVO_STEP_US;
    if (servoUs >= SERVO_MAX_US) { servoUs = SERVO_MAX_US; servoDir = -1; }
    if (servoUs <= SERVO_MIN_US) { servoUs = SERVO_MIN_US; servoDir = +1; }
    servo.writeMicroseconds(servoUs);
  }

  if (now - lastPrint >= 100) {                                // 10 Hz print
    lastPrint = now;
    Serial.printf("t=%7lu ms  servo %u us | ", now, servoUs);
    int16_t t, g[3], a[3];
    if (lsmOk && lsmRead(t, g, a)) {
      Serial.printf("LSM raw A %6d %6d %6d  G %6d %6d %6d  T %6d | ", a[0], a[1], a[2], g[0], g[1], g[2], t);
      Serial.printf("A[g] %6.3f %6.3f %6.3f  G[dps] %7.2f %7.2f %7.2f  T %5.2f C | ",
                    a[0] * LSM_ACC_MG_PER_LSB / 1000.0f, a[1] * LSM_ACC_MG_PER_LSB / 1000.0f,
                    a[2] * LSM_ACC_MG_PER_LSB / 1000.0f, g[0] * LSM_GYR_MDPS_PER_LSB / 1000.0f,
                    g[1] * LSM_GYR_MDPS_PER_LSB / 1000.0f, g[2] * LSM_GYR_MDPS_PER_LSB / 1000.0f,
                    25.0f + t / 256.0f);
    } else {
      Serial.print("LSM -- | ");
    }
    int32_t rt; uint32_t rp;
    if (bmpOk && bmpRead(rt, rp)) {
      float tc = rt / 65536.0f, pa = rp / 64.0f;
      float alt = 44330.0f * (1.0f - powf(pa / 101325.0f, 0.1903f));
      Serial.printf("BMP raw T %ld P %lu  T %5.2f C  P %9.2f Pa  alt %7.2f m", (long)rt, (unsigned long)rp, tc, pa, alt);
    } else {
      Serial.print("BMP --");
    }
    Serial.println();
  }
}
