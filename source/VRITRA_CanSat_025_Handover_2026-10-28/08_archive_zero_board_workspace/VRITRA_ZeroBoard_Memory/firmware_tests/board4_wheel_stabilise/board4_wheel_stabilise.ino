/*
 * VRITRA BOARD 4 — boot zeroing + gyro-wheel stabilisation (Teensy 4.0)
 *
 * WHAT IT DOES
 *  1. Boot: ESC gets 1000 us (stop) so it arms. Keep the CanSat STILL for ~3 s:
 *     the LSM6DSO32 gyro bias is measured and the present attitude is stored as
 *     roll = pitch = yaw = 0.
 *  2. Then roll / pitch / yaw are reported relative to that boot attitude.
 *  3. After you type 'm' (motor enable) the gyro wheel spins up to a base speed and
 *     a PD controller changes the wheel speed to push yaw (rotation about the wheel
 *     axis) back to 0: speeding the wheel up twists the CanSat one way, slowing it
 *     down twists it the other way (reaction torque).
 *
 * PHYSICS LIMIT: one wheel can only correct rotation AROUND ITS OWN AXIS (WHEEL_AXIS).
 * Tilt about the other two axes is not corrected actively; the spinning wheel only
 * resists it (gyroscopic stiffness).
 *
 * CONNECTIONS (Board 4)
 *  LSM6DSO32 0x6A on pins 18 SDA / 19 SCL (3.3 V from J8 pin 3)
 *  ESC signal  pin 6 -> R5 330 R -> J14 S  (Cyclone 45A BLHeli_S, normal one-direction mode)
 *  Hall sensor pin 9 <- J15 OUT (10k pull-up)  -> wheel RPM (display only)
 *
 * KEYS: m = motor enable / disable    z = zero again (CanSat must be still)
 *       x = STOP motor now            + / - = raise / lower wheel base speed
 *       r = reverse correction sign (if the wheel makes the spin WORSE)
 *
 * FIRST TEST: wheel secured, CanSat on a turntable or hanging on a string, props/
 * parachute off, people clear. The motor only starts after 'm'.
 */
#include <Wire.h>
#include <Servo.h>

// ---------- settings ----------
const uint8_t LSM_ADDR   = 0x6A;      // LSM6DSO32 (DO -> GND)
const uint8_t ESC_PIN    = 6;         // J14 S via R5
const uint8_t HALL_PIN   = 9;         // J15 OUT
const uint8_t WHEEL_AXIS = 2;         // LSM axis parallel to the wheel shaft: 0 = X, 1 = Y, 2 = Z
const uint8_t MAGNETS    = 1;         // Hall pulses per wheel turn

const int ESC_STOP_US = 1000;         // BLHeli_S stop / arm
const int ESC_MIN_US  = 1150;         // lowest running throttle (wheel never stops while enabled)
const int ESC_MAX_US  = 1700;         // safety ceiling for tests (raise only after testing)
int       escBaseUs   = 1400;         // base wheel speed, change with + / -

float KP = 4.0f;                      // us per degree of yaw error
float KD = 1.5f;                      // us per deg/s of yaw rate
int   corrSign = +1;                  // flip with 'r' if correction acts the wrong way
const float TILT_CUTOFF_DEG = 70.0f;  // motor stops if the CanSat tips over this far

// LSM6DSO32 scaling (ST datasheet): +-16 g = 0.488 mg/LSB, +-2000 dps = 70 mdps/LSB
const float ACC_G_PER_LSB    = 0.000488f;
const float GYR_DPS_PER_LSB  = 0.070f;
const float LOOP_HZ          = 200.0f;
const float COMP_ALPHA       = 0.98f; // complementary filter: 98 % gyro, 2 % accelerometer

Servo esc;

// ---------- state ----------
float gyroBias[3] = {0, 0, 0};        // deg/s, measured at boot
float roll0 = 0, pitch0 = 0;          // boot attitude from the accelerometer
float roll = 0, pitch = 0, yaw = 0;   // attitude relative to boot, degrees
float rate[3] = {0, 0, 0};            // bias-corrected rates, deg/s
bool  motorOn = false, sensorOk = false;
int   escUs = ESC_STOP_US;
volatile uint32_t hallPulses = 0;
void hallIsr() { hallPulses++; }

// ---------- I2C ----------
bool writeReg(uint8_t r, uint8_t v) {
  Wire.beginTransmission(LSM_ADDR); Wire.write(r); Wire.write(v);
  return Wire.endTransmission() == 0;
}
bool readRegs(uint8_t r, uint8_t *buf, uint8_t n) {
  Wire.beginTransmission(LSM_ADDR); Wire.write(r);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(LSM_ADDR, n) != n) return false;
  for (uint8_t i = 0; i < n; i++) buf[i] = Wire.read();
  return true;
}

// ---------- LSM6DSO32 ----------
bool lsmStart() {
  uint8_t who = 0;
  if (!readRegs(0x0F, &who, 1) || who != 0x6C) { Serial.printf("LSM6DSO32 not found (0x%02X)\n", who); return false; }
  writeReg(0x12, 0x01); delay(20);    // software reset
  writeReg(0x12, 0x44);               // BDU (consistent 16-bit reads) + address auto-increment
  writeReg(0x10, 0x6C);               // accel 416 Hz, +-16 g
  writeReg(0x11, 0x6C);               // gyro  416 Hz, +-2000 dps
  delay(50);
  return true;
}

// read accel (g) and gyro (deg/s, raw, bias NOT removed)
bool lsmRead(float acc[3], float gyr[3]) {
  uint8_t b[12];
  if (!readRegs(0x22, b, 12)) return false;   // 0x22..0x27 gyro XYZ, 0x28..0x2D accel XYZ
  for (int i = 0; i < 3; i++) {
    gyr[i] = (int16_t)(b[1 + 2 * i] << 8 | b[2 * i]) * GYR_DPS_PER_LSB;
    acc[i] = (int16_t)(b[7 + 2 * i] << 8 | b[6 + 2 * i]) * ACC_G_PER_LSB;
  }
  return true;
}

float accRoll(const float a[3])  { return atan2f(a[1], a[2]) * 57.2958f; }
float accPitch(const float a[3]) { return atan2f(-a[0], sqrtf(a[1] * a[1] + a[2] * a[2])) * 57.2958f; }

// ---------- boot zeroing: CanSat must be still ----------
bool calibrate() {
  Serial.println("ZEROING: keep the CanSat still ...");
  const int N = 600;                  // 600 samples at ~200 Hz = 3 s
  double gs[3] = {0, 0, 0}, gsq[3] = {0, 0, 0}, as[3] = {0, 0, 0};
  for (int k = 0; k < N; k++) {
    float a[3], g[3];
    if (!lsmRead(a, g)) { Serial.println("read error during zeroing"); return false; }
    for (int i = 0; i < 3; i++) { gs[i] += g[i]; gsq[i] += g[i] * g[i]; as[i] += a[i]; }
    delay(5);
  }
  float a[3];
  for (int i = 0; i < 3; i++) {
    gyroBias[i] = gs[i] / N;
    float var = gsq[i] / N - gyroBias[i] * gyroBias[i];
    a[i] = as[i] / N;
    if (var > 0.5f * 0.5f) {          // gyro moved more than ~0.5 deg/s rms -> not still
      Serial.printf("moved during zeroing (axis %d noise %.2f dps) -> try again\n", i, sqrtf(var));
      return false;
    }
  }
  float gmag = sqrtf(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
  roll0 = accRoll(a); pitch0 = accPitch(a);
  roll = pitch = yaw = 0;
  Serial.printf("gyro bias %.3f %.3f %.3f deg/s (datasheet typical +-1)  |g| %.3f g (expect ~1.000)\n",
                gyroBias[0], gyroBias[1], gyroBias[2], gmag);
  Serial.println("ZERO SET: roll = pitch = yaw = 0");
  return true;
}

// ---------- motor ----------
void setEsc(int us) { escUs = us; esc.writeMicroseconds(us); }
void motorStop(const char *why) {
  motorOn = false;
  setEsc(ESC_STOP_US);
  Serial.printf("MOTOR STOP (%s)\n", why);
}

void setup() {
  Serial.begin(115200);
  esc.attach(ESC_PIN, 1000, 2000);
  setEsc(ESC_STOP_US);                // ESC must see stop from power-up to arm
  uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 3000) {}
  Serial.println("\n=== VRITRA Board 4: zero + gyro-wheel stabilisation ===");
  Wire.begin();
  Wire.setClock(400000);
  pinMode(HALL_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(HALL_PIN), hallIsr, FALLING);
  sensorOk = lsmStart();
  delay(2000);                        // ESC arming beeps happen here
  while (sensorOk && !calibrate()) delay(1000);
  Serial.println("keys: m motor on/off | z zero again | x STOP | + / - base speed | r flip correction");
}

void loop() {
  static uint32_t tLoop = micros(), tPrint = 0, tRpm = 0, hallLast = 0, rpm = 0;
  static float yawErrPrev = 0;

  // ----- keys -----
  while (Serial.available()) {
    char c = Serial.read();
    if (c == 'm') {
      if (motorOn) motorStop("key m");
      else if (sensorOk) { motorOn = true; Serial.println("MOTOR ENABLED: wheel ramps to base speed"); }
    } else if (c == 'x') motorStop("key x");
    else if (c == 'z') { motorStop("re-zero"); while (!calibrate()) delay(1000); }
    else if (c == '+') { escBaseUs = min(escBaseUs + 25, ESC_MAX_US - 100); Serial.printf("base %d us\n", escBaseUs); }
    else if (c == '-') { escBaseUs = max(escBaseUs - 25, ESC_MIN_US + 100); Serial.printf("base %d us\n", escBaseUs); }
    else if (c == 'r') { corrSign = -corrSign; Serial.printf("correction sign %+d\n", corrSign); }
  }

  // ----- fixed-rate control loop -----
  uint32_t nowUs = micros();
  if (nowUs - tLoop < (uint32_t)(1e6f / LOOP_HZ)) return;
  float dt = (nowUs - tLoop) * 1e-6f;
  tLoop = nowUs;

  float a[3], g[3];
  if (!sensorOk || !lsmRead(a, g)) {
    if (motorOn) motorStop("sensor read failed");
    return;
  }
  for (int i = 0; i < 3; i++) rate[i] = g[i] - gyroBias[i];

  // attitude relative to boot: gyro integration, slowly pulled to the accelerometer for roll/pitch
  float accR = accRoll(a) - roll0, accP = accPitch(a) - pitch0;
  roll  = COMP_ALPHA * (roll  + rate[0] * dt) + (1 - COMP_ALPHA) * accR;
  pitch = COMP_ALPHA * (pitch + rate[1] * dt) + (1 - COMP_ALPHA) * accP;
  yaw  += rate[WHEEL_AXIS] * dt;                  // yaw = rotation about the wheel axis (no absolute reference)
  if (yaw > 180) yaw -= 360;
  if (yaw < -180) yaw += 360;

  // ----- wheel control -----
  if (motorOn) {
    if (fabsf(roll) > TILT_CUTOFF_DEG || fabsf(pitch) > TILT_CUTOFF_DEG) {
      motorStop("tipped over");
    } else {
      float yawErr = yaw;                          // target = 0 (boot heading)
      float u = corrSign * (KP * yawErr + KD * rate[WHEEL_AXIS]);
      int target = constrain((int)(escBaseUs + u), ESC_MIN_US, ESC_MAX_US);
      // limit how fast the throttle may change (protects ESC, battery, structure)
      int stepMax = 4;                             // us per loop = 800 us/s
      int next = escUs < ESC_MIN_US ? ESC_MIN_US : escUs;
      next += constrain(target - next, -stepMax, stepMax);
      setEsc(next);
      yawErrPrev = yawErr;
    }
  }

  // ----- wheel RPM from the Hall sensor (once per second) -----
  if (millis() - tRpm >= 1000) {
    uint32_t p = hallPulses;
    rpm = (p - hallLast) * 60UL / MAGNETS;
    hallLast = p; tRpm = millis();
  }

  // ----- report 10 Hz -----
  if (millis() - tPrint >= 100) {
    tPrint = millis();
    Serial.printf("roll %7.2f  pitch %7.2f  yaw %7.2f deg | rate %7.2f %7.2f %7.2f dps | ESC %4d us %s | wheel %5lu rpm\n",
                  roll, pitch, yaw, rate[0], rate[1], rate[2], escUs, motorOn ? "ON " : "off", (unsigned long)rpm);
  }
  (void)yawErrPrev;
}
