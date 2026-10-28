/*
 * VRITRA BOARD 1 (MAIN, Teensy 4.1) — basic raw test of everything on the board
 * Runs each check once at boot, then prints raw sensor data every second and
 * copies GNSS / link traffic to the Serial Monitor.
 *
 * CONNECTIONS (Board 1)
 *  I2C  pin 18 SDA / 19 SCL : BMP585 0x47 · ICM-20948 0x69 · DS3231 RTC 0x68 (+EEPROM 0x57)
 *                             + INA260 0x40/0x41/0x45 on the POWER board through J2
 *  SPI1 pin 26 MOSI / 27 SCK / 39 MISO : Core1121 LoRa (CS 29, BUSY 24, RESET 25, IRQ 30)
 *                                         W25Q64 flash (CS 9)
 *  GNSS L89  Serial2: pin 7 RX (GNSS TX) / pin 8 TX (GNSS RX) · pin 6 GNSS RESET (low = reset)
 *  Camera    Serial1: pin 1 TX (via R11) / pin 0 RX -> J5 -> BACKUP mux -> XIAO
 *  Heartbeat pin 5 -> R3 -> J5 pin 1 (to BACKUP)
 *  PARACHUTE Serial3: pin 14 TX (via R4) / pin 15 RX -> J4
 *  Battery   pin 20 (A6) <- 100k / 22k divider from J3 (VBAT)
 *  Beacon    MAX98357A I2S2: pin 2 DIN, 3 LRC, 4 BCLK, pin 37 SD (high = on)
 *  LEDs      pin 31 green (OK), pin 32 red (fault)
 * Only Teensyduino built-in libraries: Wire, SPI, Audio.
 */
#include <Wire.h>
#include <SPI.h>
#include <Audio.h>

// ---------- pins ----------
const uint8_t LORA_CS = 29, LORA_BUSY = 24, LORA_RST = 25, LORA_IRQ = 30;
const uint8_t FLASH_CS = 9;
const uint8_t GNSS_RST = 6, HEARTBEAT = 5, AMP_SD = 37, LED_OK = 31, LED_FLT = 32, VBAT_PIN = 20;
const uint32_t GNSS_BAUD = 9600, LINK_BAUD = 115200;

// ---------- I2C addresses ----------
const uint8_t BMP = 0x47, ICM = 0x69, RTC = 0x68, EEP = 0x57;

// ---------- beacon tone: sine -> I2S2 (pins 2/3/4) -> MAX98357A ----------
AudioSynthWaveformSine tone1;
AudioOutputI2S2        i2s2;
AudioConnection        c1(tone1, 0, i2s2, 0), c2(tone1, 0, i2s2, 1);

bool bmpOk = false, icmOk = false, rtcOk = false;

// ---------- I2C helpers ----------
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
uint8_t bcd(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }

// ---------- 1. I2C scan ----------
void i2cScan() {
  Serial.println("\n[1] I2C scan (pins 18/19)");
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      const char *n = a == BMP ? "BMP585" : a == ICM ? "ICM-20948" : a == RTC ? "DS3231 RTC" : a == EEP ? "AT24C32 EEPROM (RTC module)" :
                      (a == 0x40 || a == 0x41 || a == 0x45) ? "INA260 (POWER board)" : "?";
      Serial.printf("    0x%02X %s\n", a, n);
    }
  }
}

// ---------- 2. BMP585 ----------
bool bmpStart() {
  uint8_t id = 0;
  wr(BMP, 0x7E, 0xB6); delay(10);                         // reset
  if (!rd(BMP, 0x01, &id, 1) || id != 0x51) { Serial.printf("[2] BMP585 FAIL (id 0x%02X, want 0x51)\n", id); return false; }
  wr(BMP, 0x37, 0x80 | (0x14 << 2));                      // standby, 25 Hz
  wr(BMP, 0x36, 0x40 | (4 << 3) | 1);                     // pressure on, P x16, T x2
  delay(5);
  wr(BMP, 0x37, 0x80 | (0x14 << 2) | 1);                  // continuous
  Serial.println("[2] BMP585 OK (id 0x51)");
  return true;
}

// ---------- 3. ICM-20948 ----------
bool icmStart() {
  uint8_t who = 0;
  wr(ICM, 0x7F, 0x00);                                    // register bank 0
  if (!rd(ICM, 0x00, &who, 1) || who != 0xEA) { Serial.printf("[3] ICM-20948 FAIL (who 0x%02X, want 0xEA)\n", who); return false; }
  wr(ICM, 0x06, 0x80); delay(50);                         // reset
  wr(ICM, 0x7F, 0x00);
  wr(ICM, 0x06, 0x01);                                    // wake, auto clock
  wr(ICM, 0x07, 0x00);                                    // accel + gyro on
  delay(50);
  Serial.println("[3] ICM-20948 OK (who 0xEA)  default +-2 g, +-250 dps");
  return true;
}

// ---------- 4. DS3231 RTC ----------
bool rtcStart() {
  uint8_t t[7];
  if (!rd(RTC, 0x00, t, 7)) { Serial.println("[4] DS3231 FAIL (no answer at 0x68)"); return false; }
  Serial.printf("[4] DS3231 OK  20%02u-%02u-%02u %02u:%02u:%02u\n", bcd(t[6]), bcd(t[5] & 0x1F), bcd(t[4]),
                bcd(t[2] & 0x3F), bcd(t[1]), bcd(t[0] & 0x7F));
  uint8_t e = 0;
  Wire.beginTransmission(EEP); Wire.write(0); Wire.write(0);
  bool eok = Wire.endTransmission(false) == 0 && Wire.requestFrom(EEP, (uint8_t)1) == 1;
  if (eok) e = Wire.read();
  Serial.printf("    RTC EEPROM 0x57 %s (byte0 = 0x%02X)\n", eok ? "OK" : "not found", e);
  return true;
}

// ---------- 5. SPI1: LoRa Core1121 + W25Q64 flash ----------
void spiTests() {
  pinMode(LORA_CS, OUTPUT);  digitalWrite(LORA_CS, HIGH);
  pinMode(FLASH_CS, OUTPUT); digitalWrite(FLASH_CS, HIGH);
  pinMode(LORA_BUSY, INPUT);
  pinMode(LORA_IRQ, INPUT);
  pinMode(LORA_RST, OUTPUT);
  SPI1.setMISO(39);                                       // MISO1 is on pin 39 on this board
  SPI1.begin();
  SPISettings s(2000000, MSBFIRST, SPI_MODE0);

  // W25Q64: JEDEC ID should be EF 40 17
  SPI1.beginTransaction(s);
  digitalWrite(FLASH_CS, LOW);
  SPI1.transfer(0x9F);
  uint8_t m = SPI1.transfer(0), t = SPI1.transfer(0), c = SPI1.transfer(0);
  digitalWrite(FLASH_CS, HIGH);
  SPI1.endTransaction();
  Serial.printf("[5] W25Q64 JEDEC %02X %02X %02X %s\n", m, t, c, (m == 0xEF && c == 0x17) ? "OK" : "FAIL (want EF 40 17)");

  // Core1121 (LR1121): reset, wait BUSY low, GetVersion (0x0101)
  digitalWrite(LORA_RST, LOW); delay(2); digitalWrite(LORA_RST, HIGH);
  uint32_t t0 = millis();
  while (digitalRead(LORA_BUSY) && millis() - t0 < 500) {}
  if (digitalRead(LORA_BUSY)) { Serial.println("[5] Core1121 FAIL: BUSY (pin 24) stays high"); return; }
  SPI1.beginTransaction(s);
  digitalWrite(LORA_CS, LOW); SPI1.transfer(0x01); SPI1.transfer(0x01); digitalWrite(LORA_CS, HIGH);
  SPI1.endTransaction();
  t0 = millis();
  while (digitalRead(LORA_BUSY) && millis() - t0 < 100) {}
  uint8_t v[5];
  SPI1.beginTransaction(s);
  digitalWrite(LORA_CS, LOW);
  for (int i = 0; i < 5; i++) v[i] = SPI1.transfer(0);
  digitalWrite(LORA_CS, HIGH);
  SPI1.endTransaction();
  Serial.printf("[5] Core1121 version: stat %02X hw %02X type %02X fw %02X.%02X  (type 03 = LR1121)\n",
                v[0], v[1], v[2], v[3], v[4]);
}

// ---------- 6. beacon ----------
void beep() {
  Serial.println("[6] beacon: 2 kHz tone for 1 s (speaker on the MAX98357A)");
  digitalWrite(AMP_SD, HIGH);                             // amplifier on
  tone1.frequency(2000);
  tone1.amplitude(0.8);
  delay(1000);
  tone1.amplitude(0);
  digitalWrite(AMP_SD, LOW);                              // amplifier off
}

// ---------- 7. battery ----------
float vbat() {
  int raw = analogRead(VBAT_PIN);                         // 12-bit
  return raw * 3.3f / 4095.0f * (100.0f + 22.0f) / 22.0f;
}

void setup() {
  Serial.begin(115200);
  uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 3000) {}
  Serial.println("\n=== VRITRA BOARD 1 (MAIN) basic test ===");

  pinMode(LED_OK, OUTPUT); pinMode(LED_FLT, OUTPUT);
  digitalWrite(LED_OK, HIGH); digitalWrite(LED_FLT, HIGH); delay(500);   // both LEDs on = LED test
  digitalWrite(LED_FLT, LOW);
  pinMode(HEARTBEAT, OUTPUT);
  pinMode(AMP_SD, OUTPUT); digitalWrite(AMP_SD, LOW);
  pinMode(GNSS_RST, OUTPUT); digitalWrite(GNSS_RST, HIGH);              // GNSS running (low = reset)
  analogReadResolution(12);
  AudioMemory(8);

  Wire.begin(); Wire.setClock(100000);
  Serial1.begin(LINK_BAUD);      // camera link
  Serial2.begin(GNSS_BAUD);      // GNSS
  Serial3.begin(LINK_BAUD);      // PARACHUTE link

  i2cScan();
  bmpOk = bmpStart();
  icmOk = icmStart();
  rtcOk = rtcStart();
  spiTests();
  beep();
  Serial.printf("[7] battery %.2f V (pin 20)\n", vbat());
  Serial.println("[8] camera link: sending \"TEST\" on Serial1 (pin 1); replies are printed as [CAM]");
  Serial1.println("TEST");
  Serial.println("\nlive data every second; GNSS lines as [GNSS], PARACHUTE as [B4], camera as [CAM]\n");
}

void loop() {
  static uint32_t tLive = 0;
  static bool hb = false;

  // copy incoming serial traffic
  while (Serial2.available()) { static String g; char c = Serial2.read(); if (c == '\n') { Serial.print("[GNSS] "); Serial.println(g); g = ""; } else if (c != '\r') g += c; }
  while (Serial3.available()) { Serial.print("[B4] ");  Serial.write(Serial3.read()); Serial.println(); }
  while (Serial1.available()) { Serial.print("[CAM] "); Serial.write(Serial1.read()); Serial.println(); }

  if (millis() - tLive < 1000) return;
  tLive = millis();

  hb = !hb;
  digitalWrite(HEARTBEAT, hb);               // heartbeat to BACKUP: 0.5 Hz square wave
  digitalWrite(LED_OK, hb);                  // green LED blinks with it
  Serial3.println("B1 PING");                // to PARACHUTE

  if (bmpOk) {
    uint8_t b[6];
    if (rd(BMP, 0x1D, b, 6)) {
      int32_t rt = (int32_t)((uint32_t)b[2] << 16 | (uint32_t)b[1] << 8 | b[0]);
      if (rt & 0x800000) rt |= 0xFF000000;
      uint32_t rp = (uint32_t)b[5] << 16 | (uint32_t)b[4] << 8 | b[3];
      Serial.printf("BMP raw T %ld P %lu -> %.2f C %.1f Pa\n", (long)rt, (unsigned long)rp, rt / 65536.0f, rp / 64.0f);
    }
  }
  if (icmOk) {
    uint8_t b[12];
    if (rd(ICM, 0x2D, b, 12)) {                // accel X,Y,Z then gyro X,Y,Z (big-endian)
      int16_t v[6];
      for (int i = 0; i < 6; i++) v[i] = (int16_t)(b[2 * i] << 8 | b[2 * i + 1]);
      Serial.printf("ICM raw A %6d %6d %6d  G %6d %6d %6d -> A g %.2f %.2f %.2f  G dps %.1f %.1f %.1f\n",
                    v[0], v[1], v[2], v[3], v[4], v[5], v[0] / 16384.0f, v[1] / 16384.0f, v[2] / 16384.0f,
                    v[3] / 131.0f, v[4] / 131.0f, v[5] / 131.0f);
    }
  }
  if (rtcOk) {
    uint8_t t[7], tt[2];
    if (rd(RTC, 0x00, t, 7) && rd(RTC, 0x11, tt, 2))
      Serial.printf("RTC %02u:%02u:%02u  temp %.2f C\n", bcd(t[2] & 0x3F), bcd(t[1]), bcd(t[0] & 0x7F),
                    (int8_t)tt[0] + (tt[1] >> 6) * 0.25f);
  }
  Serial.printf("VBAT %.2f V | LoRa BUSY %d IRQ %d\n\n", vbat(), digitalRead(LORA_BUSY), digitalRead(LORA_IRQ));
}
