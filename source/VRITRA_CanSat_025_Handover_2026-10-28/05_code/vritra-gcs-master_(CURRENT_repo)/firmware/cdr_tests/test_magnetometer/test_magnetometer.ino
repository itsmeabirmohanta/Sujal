/* =============================================================================
 * test_magnetometer.ino — AK09916 magnetometer (inside the ICM-20948, 7Semi)
 * Team 2026-INSPACe-CAN-7USAT-025 · Teensy 4.1
 * =============================================================================
 * Standalone. Nothing from vritra_fc/ is included.
 * Serial Monitor at 115200. Prints this sensor's own parameters only.
 *
 * THE MAGNETOMETER IS A SEPARATE DIE ON ITS OWN I2C ADDRESS, sitting behind
 * the ICM-20948's auxiliary bus. It does not answer at 0x0C until the ICM is
 * told to get out of the way: disable the ICM's own I2C master, then set the
 * bypass bit, and only then does 0x0C appear on the main bus. Scanning the bus
 * before that and concluding the magnetometer is dead is the standard trap.
 *
 * ALWAYS READ THROUGH ST2. The AK09916 latches its data and only releases the
 * next measurement when the status 2 register is read. Read six bytes instead
 * of eight and the values freeze after the first sample, which looks like a
 * working sensor that has stopped moving.
 * ========================================================================== */
#include <Arduino.h>
#include <Wire.h>

#define PIN_SDA     18
#define PIN_SCL     19
#define ICM_ADDR    0x69
#define AK_ADDR     0x0C

/* ---------- ICM registers needed to open the bypass ------------------------ */
#define ICM_USER_CTRL   0x03
#define ICM_PWR_MGMT_1  0x06
#define ICM_INT_PIN_CFG 0x0F
#define ICM_BANK_SEL    0x7F

/* ---------- AK09916 registers ---------------------------------------------- */
#define AK_WIA2     0x01        /* identity, 0x09          */
#define AK_ST1      0x10        /* bit 0 data ready        */
#define AK_HXL      0x11        /* 6 data bytes then ST2   */
#define AK_CNTL2    0x31        /* mode                    */
#define AK_CNTL3    0x32        /* soft reset              */
#define MAG_UT_PER_LSB  0.15f

/* Hard iron offsets, filled by the calibration routine. */
static float ox = 0, oy = 0, oz = 0;
static bool  calibrated = false;

static bool wrDev(uint8_t dev, uint8_t reg, uint8_t val) {
    Wire.beginTransmission(dev);
    Wire.write(reg); Wire.write(val);
    return Wire.endTransmission() == 0;
}
static bool rdDev(uint8_t dev, uint8_t reg, uint8_t* buf, uint8_t n) {
    Wire.beginTransmission(dev);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((int)dev, (int)n) != n) return false;
    for (int i = 0; i < n; i++) buf[i] = Wire.read();
    return true;
}

static bool magRead(float& mx, float& my, float& mz, bool& overflow) {
    uint8_t st1;
    if (!rdDev(AK_ADDR, AK_ST1, &st1, 1)) return false;
    uint8_t r[8];                                   /* through ST2, see header */
    if (!rdDev(AK_ADDR, AK_HXL, r, 8)) return false;
    overflow = (r[7] & 0x08) != 0;                  /* ST2 magnetic overflow   */
    /* little endian on this part, unlike the ICM's own big endian output */
    mx = (int16_t)((r[1] << 8) | r[0]) * MAG_UT_PER_LSB - (calibrated ? ox : 0.0f);
    my = (int16_t)((r[3] << 8) | r[2]) * MAG_UT_PER_LSB - (calibrated ? oy : 0.0f);
    mz = (int16_t)((r[5] << 8) | r[4]) * MAG_UT_PER_LSB - (calibrated ? oz : 0.0f);
    return true;
}

static void calibrateHardIron() {
    Serial.println(F("  Turn the board slowly through every orientation"));
    Serial.println(F("  for twenty seconds — figure of eight, all axes."));
    float lox = 1e9f, hix = -1e9f, loy = 1e9f, hiy = -1e9f, loz = 1e9f, hiz = -1e9f;
    calibrated = false;
    uint32_t t0 = millis();
    while (millis() - t0 < 20000) {
        float mx, my, mz; bool ov;
        if (magRead(mx, my, mz, ov) && !ov) {
            lox = min(lox, mx); hix = max(hix, mx);
            loy = min(loy, my); hiy = max(hiy, my);
            loz = min(loz, mz); hiz = max(hiz, mz);
        }
        if ((millis() - t0) % 2000 < 30) Serial.print(F("."));
        delay(20);
    }
    Serial.println();
    ox = (lox + hix) / 2.0f;
    oy = (loy + hiy) / 2.0f;
    oz = (loz + hiz) / 2.0f;
    calibrated = true;
    Serial.printf("  Hard iron offset X %+.1f  Y %+.1f  Z %+.1f uT\n", ox, oy, oz);
    Serial.printf("  Swing X %.1f  Y %.1f  Z %.1f uT\n",
                  hix - lox, hiy - loy, hiz - loz);
}

void setup() {
    Serial.begin(115200);
    uint32_t t0 = millis();
    while (!Serial && millis() - t0 < 3000) { }

    Wire.begin();
    Wire.setClock(100000);

    Serial.println();
    Serial.println(F("  Magnetometer test — AK09916 behind the ICM-20948"));
    Serial.println(F("  ------------------------------------------------"));

    /* wake the host part first, it gates the bus */
    wrDev(ICM_ADDR, ICM_PWR_MGMT_1, 0x80); delay(100);
    wrDev(ICM_ADDR, ICM_PWR_MGMT_1, 0x01); delay(20);
    wrDev(ICM_ADDR, ICM_BANK_SEL, 0x00);

    Serial.println(F("  Opening the bypass so the magnetometer reaches the main bus"));
    wrDev(ICM_ADDR, ICM_USER_CTRL, 0x00);       /* drop the ICM I2C master */
    wrDev(ICM_ADDR, ICM_INT_PIN_CFG, 0x02);     /* bypass enable           */
    delay(10);

    uint8_t id = 0;
    if (!rdDev(AK_ADDR, AK_WIA2, &id, 1)) {
        Serial.println(F("  FAIL  no answer at address 0x0C"));
        Serial.println(F("        the bypass did not open, or the part is not fitted"));
        return;
    }
    Serial.printf("  Chip identity 0x%02X, expected 0x09\n", id);
    if (id != 0x09) { Serial.println(F("  FAIL  wrong identity")); return; }

    wrDev(AK_ADDR, AK_CNTL3, 0x01); delay(10);  /* soft reset                */
    wrDev(AK_ADDR, AK_CNTL2, 0x08);             /* continuous mode 4, 100 Hz */
    delay(20);
    Serial.println(F("  Continuous mode at 100 samples per second"));
    Serial.println();
    Serial.println(F("  Send any character to run the hard iron calibration."));
    Serial.println(F("  ------------------------------------------------"));
}

void loop() {
    if (Serial.available()) {
        while (Serial.available()) Serial.read();
        Serial.println();
        calibrateHardIron();
        Serial.println();
    }

    float mx, my, mz; bool overflow = false;
    if (!magRead(mx, my, mz, overflow)) {
        Serial.println(F("  Read failed"));
        delay(500);
        return;
    }

    const float mag = sqrtf(mx * mx + my * my + mz * mz);
    /* Flat-board heading. It is only correct while the board is level — tilt
     * compensation needs the accelerometer and belongs in the flight code. */
    float hdg = degrees(atan2f(-my, mx));
    if (hdg < 0) hdg += 360.0f;
    /* Dip angle, the field's tilt below horizontal. Around 40 deg in Punjab. */
    const float horiz = sqrtf(mx * mx + my * my);
    const float dip = degrees(atan2f(mz, horiz > 0 ? horiz : 1e-6f));

    Serial.printf("  Field X      %+8.1f uT\n", mx);
    Serial.printf("  Field Y      %+8.1f uT\n", my);
    Serial.printf("  Field Z      %+8.1f uT\n", mz);
    Serial.printf("  Field total  %8.1f uT\n", mag);
    Serial.printf("  Heading      %8.1f deg   level board only\n", hdg);
    Serial.printf("  Dip angle    %+8.1f deg\n", dip);
    Serial.printf("  Calibration  %s\n", calibrated ? "hard iron removed" : "raw, not calibrated");

    if (overflow)
        Serial.println(F("  WARN  magnetic overflow, something strong is very close"));
    else if (mag > 25.0f && mag < 65.0f)
        Serial.println(F("  PASS  field strength is plausible for the Earth"));
    else
        Serial.println(F("  WARN  field unusual — nearby magnet, motor or steel bench"));

    Serial.println();
    delay(1000);
}
