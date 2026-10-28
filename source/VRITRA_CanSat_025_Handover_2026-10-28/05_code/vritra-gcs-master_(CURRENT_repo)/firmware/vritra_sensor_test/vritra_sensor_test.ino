/* =============================================================================
 * vritra_sensor_test.ino — standalone hardware bring-up for Teensy 4.1
 * Tests each sensor independently. Upload this, open Serial Monitor at
 * 115200 baud, power cycle. Each test prints PASS or FAIL with a reason.
 * ============================================================================ */
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>

/* ---------- pin map (must match Config.h exactly) -------------------------  */
#define PIN_SDA        18
#define PIN_SCL        19
#define PIN_SPI1_MOSI  26
#define PIN_SPI1_SCK   27
#define PIN_SPI1_MISO  39
#define PIN_LORA_CS    29
#define PIN_LORA_IRQ   30
#define PIN_LORA_RST   25
#define PIN_LORA_BUSY  24
#define PIN_GNSS_RX     7   /* Serial2 */
#define PIN_GNSS_TX     8
#define PIN_GNSS_RST    6

/* ---------- I2C addresses ------------------------------------------------- */
#define BMP_ADDR    0x47    /* SDO pulled HIGH */
#define ICM_ADDR    0x69    /* AD0 pulled HIGH */
#define RTC_ADDR    0x68
#define AK_ADDR     0x0C    /* AK09916 via bypass */

/* ---------- helpers -------------------------------------------------------- */
static bool wr8(uint8_t dev, uint8_t reg, uint8_t val) {
    Wire.beginTransmission(dev);
    Wire.write(reg); Wire.write(val);
    return Wire.endTransmission() == 0;
}
static bool rd8(uint8_t dev, uint8_t reg, uint8_t& out) {
    Wire.beginTransmission(dev);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((int)dev, 1) != 1) return false;
    out = Wire.read();
    return true;
}
static bool rdN(uint8_t dev, uint8_t reg, uint8_t* buf, uint8_t n) {
    Wire.beginTransmission(dev);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((int)dev, (int)n) != n) return false;
    for (int i = 0; i < n; i++) buf[i] = Wire.read();
    return true;
}

/* ============================================================================
 * TEST 0 — I2C bus scan (finds every device, diagnoses shorts)
 * ========================================================================== */
static void testI2CScan() {
    Serial.println(F("\n=== TEST 0: I2C SCAN ==="));
    int found = 0;
    for (uint8_t a = 8; a <= 119; a++) {
        Wire.beginTransmission(a);
        if (Wire.endTransmission() == 0) {
            Serial.printf("  FOUND 0x%02X", a);
            if (a == BMP_ADDR) Serial.print(F(" <BMP585>"));
            if (a == ICM_ADDR) Serial.print(F(" <ICM-20948>"));
            if (a == RTC_ADDR) Serial.print(F(" <DS3231/RTC>"));
            if (a == AK_ADDR)  Serial.print(F(" <AK09916 mag>"));
            Serial.println();
            found++;
        }
    }
    if (found == 0)
        Serial.println(F("  FAIL — no devices at all. Check SDA/SCL wiring and pull-ups."));
    else
        Serial.printf("  %d device(s) found\n", found);
}

/* ============================================================================
 * TEST 1 — BMP585 (barometer)
 * ========================================================================== */
static void testBMP585() {
    Serial.println(F("\n=== TEST 1: BMP585 BAROMETER ==="));

    uint8_t id = 0;
    if (!rd8(BMP_ADDR, 0x01, id)) {          /* CHIP_ID register */
        Serial.println(F("  FAIL — no ACK at 0x47. Wiring or power problem."));
        return;
    }
    Serial.printf("  CHIP_ID = 0x%02X (expect 0x51 for BMP585)\n", id);
    if (id != 0x51) {
        Serial.println(F("  WARN — unexpected chip id, may still work"));
    }

    /* soft reset */
    wr8(BMP_ADDR, 0x7E, 0xB6);
    delay(5);

    /* enable continuous mode, 8x OSR, ODR 25 Hz */
    wr8(BMP_ADDR, 0x37, 0xC7);   /* OSR_P=8x, OSR_T=4x */
    wr8(BMP_ADDR, 0x36, 0x45);   /* power mode = Normal, ODR 25 Hz */
    delay(50);

    uint8_t buf[6];
    if (!rdN(BMP_ADDR, 0x20, buf, 6)) {
        Serial.println(F("  FAIL — cannot read pressure/temperature registers"));
        return;
    }
    int32_t rawP = (int32_t)(buf[0] | (buf[1] << 8) | (buf[2] << 16)) << 8 >> 8;
    int32_t rawT = (int32_t)(buf[3] | (buf[4] << 8) | (buf[5] << 16)) << 8 >> 8;

    /* BMP585 outputs are already in Pa (pressure) and 1/65536 °C (temperature) */
    float pa = (float)rawP / 64.0f;
    float tc = (float)rawT / 65536.0f;

    Serial.printf("  Pressure = %.1f Pa (%.4f kPa) — expect ~101325 Pa at sea level\n",
                  pa, pa / 1000.0f);
    Serial.printf("  Temp     = %.2f C\n", tc);

    if (pa > 80000.0f && pa < 120000.0f)
        Serial.println(F("  PASS"));
    else
        Serial.println(F("  WARN — pressure out of plausible range"));
}

/* ============================================================================
 * TEST 2 — ICM-20948 (IMU: accelerometer + gyroscope)
 * ========================================================================== */
static void testICM20948() {
    Serial.println(F("\n=== TEST 2: ICM-20948 IMU ==="));

    /* wake, select bank 0 */
    wr8(ICM_ADDR, 0x06, 0x01);   /* PWR_MGMT_1: DEVICE_RESET */
    delay(100);
    wr8(ICM_ADDR, 0x06, 0x01);   /* clk sel auto, wake up */
    delay(20);
    wr8(ICM_ADDR, 0x7F, 0x00);   /* USER_BANK = 0 */

    uint8_t who = 0;
    if (!rd8(ICM_ADDR, 0x00, who)) {
        Serial.println(F("  FAIL — no ACK at 0x69. Wiring or power problem."));
        return;
    }
    Serial.printf("  WHO_AM_I = 0x%02X (expect 0xEA)\n", who);
    if (who != 0xEA) {
        Serial.println(F("  FAIL — wrong chip id"));
        return;
    }

    /* configure: ACCEL ±16 g, GYRO ±2000 dps, DLPF on */
    wr8(ICM_ADDR, 0x7F, 0x20);   /* bank 2 */
    wr8(ICM_ADDR, 0x14, 0x1F);   /* ACCEL_CONFIG: ±16 g, DLPF on */
    wr8(ICM_ADDR, 0x01, 0x2F);   /* GYRO_CONFIG_1: ±2000 dps, DLPF 11.6 Hz */
    wr8(ICM_ADDR, 0x7F, 0x00);   /* back to bank 0 */
    delay(30);

    uint8_t raw[12];
    if (!rdN(ICM_ADDR, 0x2D, raw, 12)) {   /* ACCEL_XOUT_H .. GYRO_ZOUT_L */
        Serial.println(F("  FAIL — cannot read sensor registers"));
        return;
    }
    int16_t ax = (int16_t)((raw[0]<<8)|raw[1]);
    int16_t ay = (int16_t)((raw[2]<<8)|raw[3]);
    int16_t az = (int16_t)((raw[4]<<8)|raw[5]);
    int16_t gx = (int16_t)((raw[6]<<8)|raw[7]);
    int16_t gy = (int16_t)((raw[8]<<8)|raw[9]);
    int16_t gz = (int16_t)((raw[10]<<8)|raw[11]);

    float axg = ax / 2048.0f;  /* ±16 g → 2048 LSB/g */
    float ayg = ay / 2048.0f;
    float azg = az / 2048.0f;
    float accelMag = sqrtf(axg*axg + ayg*ayg + azg*azg);

    Serial.printf("  Accel  X=%.3f Y=%.3f Z=%.3f g  |a|=%.3f g\n",
                  axg, ayg, azg, accelMag);
    Serial.printf("  Gyro   X=%d Y=%d Z=%d raw\n", gx, gy, gz);

    if (accelMag > 0.7f && accelMag < 1.3f)
        Serial.println(F("  PASS — |a| near 1 g (flat, still)"));
    else
        Serial.println(F("  WARN — |a| not near 1 g. Is the board still and level?"));
}

/* ============================================================================
 * TEST 3 — AK09916 magnetometer (accessed through ICM bypass)
 * ========================================================================== */
static void testAK09916() {
    Serial.println(F("\n=== TEST 3: AK09916 MAGNETOMETER (via ICM bypass) ==="));

    /* Enable I2C bypass on ICM so we can talk directly to AK09916 */
    wr8(ICM_ADDR, 0x7F, 0x00);
    wr8(ICM_ADDR, 0x0F, 0x02);   /* INT_PIN_CFG: BYPASS_EN */
    delay(5);

    uint8_t wia = 0;
    if (!rd8(AK_ADDR, 0x01, wia)) {
        Serial.println(F("  FAIL — no ACK at 0x0C. ICM bypass may not be enabled."));
        wr8(ICM_ADDR, 0x0F, 0x00);
        return;
    }
    Serial.printf("  WIA2 = 0x%02X (expect 0x09 for AK09916)\n", wia);
    if (wia != 0x09) {
        Serial.println(F("  FAIL — wrong magnetometer id"));
        wr8(ICM_ADDR, 0x0F, 0x00);
        return;
    }

    /* single measurement */
    wr8(AK_ADDR, 0x31, 0x01);   /* CNTL2: single measurement */
    delay(15);

    uint8_t st1 = 0;
    rd8(AK_ADDR, 0x10, st1);
    if (!(st1 & 0x01)) {
        Serial.println(F("  WARN — data not ready yet, reading anyway"));
    }

    uint8_t mag[8];
    rdN(AK_ADDR, 0x11, mag, 8);   /* HXL..ST2 */
    int16_t mx = (int16_t)((mag[1]<<8)|mag[0]);
    int16_t my = (int16_t)((mag[3]<<8)|mag[2]);
    int16_t mz = (int16_t)((mag[5]<<8)|mag[4]);

    float mxuT = mx * 0.15f;
    float myuT = my * 0.15f;
    float mzuT = mz * 0.15f;
    float magMag = sqrtf(mxuT*mxuT + myuT*myuT + mzuT*mzuT);

    Serial.printf("  Mag  X=%.1f Y=%.1f Z=%.1f uT  |m|=%.1f uT\n",
                  mxuT, myuT, mzuT, magMag);
    if (magMag > 20.0f && magMag < 70.0f)
        Serial.println(F("  PASS — field strength plausible for Earth"));
    else
        Serial.println(F("  WARN — field unusual. Hard-iron distortion or not calibrated?"));

    wr8(ICM_ADDR, 0x0F, 0x00);   /* disable bypass */
}

/* ============================================================================
 * TEST 4 — DS3231 RTC
 * ========================================================================== */
static void testDS3231() {
    Serial.println(F("\n=== TEST 4: DS3231 RTC ==="));

    uint8_t sec = 0, min_ = 0, hr = 0, day = 0, mon = 0, yr = 0;
    if (!rd8(RTC_ADDR, 0x00, sec)) {
        Serial.println(F("  FAIL — no ACK at 0x68. Wiring or power problem."));
        return;
    }
    rd8(RTC_ADDR, 0x01, min_);
    rd8(RTC_ADDR, 0x02, hr);
    rd8(RTC_ADDR, 0x04, day);
    rd8(RTC_ADDR, 0x05, mon);
    rd8(RTC_ADDR, 0x06, yr);

    /* BCD decode */
    auto bcd = [](uint8_t v) -> uint8_t { return (v >> 4) * 10 + (v & 0x0F); };
    Serial.printf("  Time: 20%02u-%02u-%02u  %02u:%02u:%02u\n",
                  bcd(yr), bcd(mon & 0x1F), bcd(day),
                  bcd(hr & 0x3F), bcd(min_), bcd(sec & 0x7F));

    uint8_t tmp[2];
    rdN(RTC_ADDR, 0x11, tmp, 2);
    float rtcTemp = (int8_t)tmp[0] + (float)(tmp[1] >> 6) * 0.25f;
    Serial.printf("  RTC temp: %.2f C\n", rtcTemp);

    if (rtcTemp > 10.0f && rtcTemp < 50.0f)
        Serial.println(F("  PASS"));
    else
        Serial.println(F("  WARN — temperature out of expected range"));
}

/* ============================================================================
 * TEST 5 — L89HA GNSS (Serial2 passthrough — just print raw NMEA for 5 s)
 * ========================================================================== */
static void testGNSS() {
    Serial.println(F("\n=== TEST 5: L89HA GNSS (Serial2 — raw NMEA 5 s) ==="));
    Serial.println(F("  Listening on Serial2 RX=7. You should see $G... sentences."));
    Serial.println(F("  If nothing appears: check TX->RX wiring and that RST is HIGH."));

    /* release reset */
    pinMode(PIN_GNSS_RST, OUTPUT);
    digitalWrite(PIN_GNSS_RST, HIGH);
    Serial2.begin(9600, SERIAL_8N1);
    delay(200);

    uint32_t end = millis() + 5000;
    int lines = 0;
    char buf[128];
    int bi = 0;
    while (millis() < end) {
        while (Serial2.available()) {
            char c = (char)Serial2.read();
            if (c == '\n' || bi >= 126) {
                buf[bi] = '\0';
                if (bi > 3) { Serial.printf("  %s\n", buf); lines++; }
                bi = 0;
            } else if (c != '\r') {
                buf[bi++] = c;
            }
        }
    }
    if (lines == 0)
        Serial.println(F("  FAIL — no NMEA data received in 5 s"));
    else
        Serial.printf("  PASS — %d sentences received\n", lines);
}

/* ============================================================================
 * TEST 6 — LR1121 LoRa (SPI1)
 * ========================================================================== */
static uint8_t lrSpiCmd(uint8_t opc, uint8_t* out, uint8_t nOut) {
    /* LR1121 SPI: send opcode (2 bytes: 0x00 | opc), read status + nOut bytes */
    SPI1.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
    digitalWrite(PIN_LORA_CS, LOW);
    SPI1.transfer(0x00);
    SPI1.transfer(opc);
    uint8_t status = SPI1.transfer(0x00);   /* first byte after opcode is status */
    for (uint8_t i = 0; i < nOut; i++) out[i] = SPI1.transfer(0x00);
    digitalWrite(PIN_LORA_CS, HIGH);
    SPI1.endTransaction();
    return status;
}

static void testLR1121() {
    Serial.println(F("\n=== TEST 6: LR1121 LoRa (SPI1) ==="));

    SPI1.setMISO(PIN_SPI1_MISO);
    SPI1.setMOSI(PIN_SPI1_MOSI);
    SPI1.setSCK(PIN_SPI1_SCK);
    SPI1.begin();

    pinMode(PIN_LORA_CS,   OUTPUT); digitalWrite(PIN_LORA_CS,   HIGH);
    pinMode(PIN_LORA_RST,  OUTPUT); digitalWrite(PIN_LORA_RST,  HIGH);
    pinMode(PIN_LORA_BUSY, INPUT);
    pinMode(PIN_LORA_IRQ,  INPUT);

    /* hardware reset */
    digitalWrite(PIN_LORA_RST, LOW);  delay(10);
    digitalWrite(PIN_LORA_RST, HIGH); delay(20);

    /* wait for BUSY to go LOW (chip ready) */
    uint32_t t0 = millis();
    while (digitalRead(PIN_LORA_BUSY) && millis() - t0 < 500) {}
    bool busyOk = !digitalRead(PIN_LORA_BUSY);
    Serial.printf("  BUSY after reset: %s (expect LOW)\n", busyOk ? "LOW  GOOD" : "HIGH FAIL — stuck busy");
    if (!busyOk) {
        Serial.println(F("  FAIL — BUSY pin stuck HIGH. Check wiring to pin 24."));
        return;
    }

    /* read MISO during idle — should not be all-0 or all-FF */
    uint8_t misoIdle = 0;
    SPI1.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
    digitalWrite(PIN_LORA_CS, LOW);
    misoIdle = SPI1.transfer(0x00);
    digitalWrite(PIN_LORA_CS, HIGH);
    SPI1.endTransaction();
    delay(2);
    Serial.printf("  MISO idle byte: 0x%02X\n", misoIdle);
    if (misoIdle == 0x00)
        Serial.println(F("  WARN — MISO stuck LOW (line shorted or MISO not connected to pin 39)"));
    else if (misoIdle == 0xFF)
        Serial.println(F("  WARN — MISO stuck HIGH (MISO not pulled down, possibly not connected)"));

    /* GetVersion command (opcode 0x0101 on LR11x0) */
    /* LR11x0 protocol: 16-bit opcode MSB first, then NOP bytes for response */
    SPI1.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
    digitalWrite(PIN_LORA_CS, LOW);
    SPI1.transfer(0x01);   /* opcode MSB */
    SPI1.transfer(0x01);   /* opcode LSB: GetVersion */
    uint8_t stat = SPI1.transfer(0x00);  /* status */
    uint8_t hw   = SPI1.transfer(0x00);
    uint8_t dev  = SPI1.transfer(0x00);
    uint8_t fwMaj = SPI1.transfer(0x00);
    uint8_t fwMin = SPI1.transfer(0x00);
    digitalWrite(PIN_LORA_CS, HIGH);
    SPI1.endTransaction();

    Serial.printf("  GetVersion: status=0x%02X HW=0x%02X DEV=0x%02X FW=%d.%d\n",
                  stat, hw, dev, fwMaj, fwMin);

    if (hw == 0x22 && (dev == 0x01 || dev == 0x03 || dev == 0x02))
        Serial.println(F("  PASS — LR1121 hardware confirmed"));
    else if (hw == 0 && dev == 0)
        Serial.println(F("  FAIL — all zeros. MISO not connected to pin 39, or chip unpowered."));
    else if (hw == 0xFF && dev == 0xFF)
        Serial.println(F("  FAIL — all ones. MISO floating or CS not working."));
    else
        Serial.printf("  WARN — unexpected ids (HW=0x%02X DEV=0x%02X). May still work.\n", hw, dev);
}

/* ============================================================================
 * SETUP / LOOP
 * ========================================================================== */
void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000) {}

    Serial.println(F("\n========================================"));
    Serial.println(F("  VRITRA sensor bring-up test"));
    Serial.println(F("  Teensy 4.1 — all sensors checked"));
    Serial.println(F("========================================"));

    Wire.begin();
    Wire.setClock(100000);   /* 100 kHz — safe for bring-up without pull-ups */

    testI2CScan();
    testBMP585();
    testICM20948();
    testAK09916();
    testDS3231();
    testGNSS();
    testLR1121();

    Serial.println(F("\n=== ALL TESTS DONE ==="));
    Serial.println(F("Fix any FAILs above before uploading the flight firmware."));
}

void loop() {}
