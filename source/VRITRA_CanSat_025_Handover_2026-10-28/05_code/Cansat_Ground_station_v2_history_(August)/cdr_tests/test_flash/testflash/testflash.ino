#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>

#define PIN_SDA 18
#define PIN_SCL 19
#define BMP_ADDR 0x47
#define PIN_FLASH_CS 5

#define CMD_WRITE_ENABLE 0x06
#define CMD_READ_STATUS 0x05
#define CMD_READ_DATA 0x03
#define CMD_PAGE_PROGRAM 0x02
#define CMD_SECTOR_ERASE 0x20
#define CMD_JEDEC_ID 0x9F
#define CMD_RELEASE_PWRDN 0xAB

#define BMP_CHIP_ID 0x01
#define BMP_DATA 0x1D
#define BMP_ODR_CFG 0x36
#define BMP_OSR_CFG 0x37
#define BMP_DSP_CONFIG 0x38
#define BMP_CMD 0x7E

#define SEALEVEL_PA 100340.0f

static SPISettings SPI_CFG(8000000, MSBFIRST, SPI_MODE0);

enum FlightState { STATE_SETUP, STATE_LOGGING, STATE_HALTED };
static FlightState currentState = STATE_SETUP;

static uint32_t logStartTime = 0;
static uint32_t lastPrint = 0;
static uint32_t lastUpdate = 0;

static uint8_t pageBuffer[256];
static uint16_t bufferIdx = 0;
static uint32_t flashAddr = 0;

static float padPa = 0.0f, padTc = 0.0f;
static bool padSet = false;
static uint32_t nSamples = 0;
static float minPa = 1e9f, maxPa = -1e9f, sumPa = 0.0f;

static float k_alt = 0.0f;
static float k_vel = 0.0f;
static float P[2][2] = {{1.0f, 0.0f}, {0.0f, 1.0f}};

const float Q_alt = 0.0001f;
const float Q_vel = 0.001f;
const float R_meas = 3.0f;

static void csLow() { digitalWrite(PIN_FLASH_CS, LOW); }
static void csHigh() { digitalWrite(PIN_FLASH_CS, HIGH); }

static void cmd1(uint8_t c) {
    SPI.beginTransaction(SPI_CFG); csLow();
    SPI.transfer(c);
    csHigh(); SPI.endTransaction();
}

static uint8_t status() {
    SPI.beginTransaction(SPI_CFG); csLow();
    SPI.transfer(CMD_READ_STATUS);
    const uint8_t s = SPI.transfer(0x00);
    csHigh(); SPI.endTransaction();
    return s;
}

static bool waitIdle(uint32_t limitMs) {
    const uint32_t t0 = millis();
    while (millis() - t0 < limitMs) {
        if ((status() & 0x01) == 0) return true;
    }
    return false;
}

static void readBytes(uint32_t addr, uint8_t* buf, uint16_t n) {
    SPI.beginTransaction(SPI_CFG); csLow();
    SPI.transfer(CMD_READ_DATA);
    SPI.transfer((uint8_t)(addr >> 16));
    SPI.transfer((uint8_t)(addr >> 8));
    SPI.transfer((uint8_t)addr);
    for (uint16_t i = 0; i < n; i++) buf[i] = SPI.transfer(0x00);
    csHigh(); SPI.endTransaction();
}

static bool eraseSector(uint32_t addr) {
    cmd1(CMD_WRITE_ENABLE);
    if ((status() & 0x02) == 0) return false;
    SPI.beginTransaction(SPI_CFG); csLow();
    SPI.transfer(CMD_SECTOR_ERASE);
    SPI.transfer((uint8_t)(addr >> 16));
    SPI.transfer((uint8_t)(addr >> 8));
    SPI.transfer((uint8_t)addr);
    csHigh(); SPI.endTransaction();
    return waitIdle(3000);
}

static bool writeBytes(uint32_t addr, const uint8_t* buf, uint16_t n) {
    cmd1(CMD_WRITE_ENABLE);
    if ((status() & 0x02) == 0) return false;
    SPI.beginTransaction(SPI_CFG); csLow();
    SPI.transfer(CMD_PAGE_PROGRAM);
    SPI.transfer((uint8_t)(addr >> 16));
    SPI.transfer((uint8_t)(addr >> 8));
    SPI.transfer((uint8_t)addr);
    for (uint16_t i = 0; i < n; i++) SPI.transfer(buf[i]);
    csHigh(); SPI.endTransaction();
    return waitIdle(1000);
}

/* Missing wr8 function restored */
static bool wr8(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(BMP_ADDR);
    Wire.write(reg); Wire.write(val);
    return Wire.endTransmission() == 0;
}

static bool rdN(uint8_t reg, uint8_t* buf, uint8_t n) {
    Wire.beginTransmission(BMP_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((int)BMP_ADDR, (int)n) != n) return false;
    for (int i = 0; i < n; i++) buf[i] = Wire.read();
    return true;
}
static bool rd8(uint8_t reg, uint8_t& out) { return rdN(reg, &out, 1); }

static bool baroRead(float& pa, float& tc) {
    uint8_t b[6];
    if (!rdN(BMP_DATA, b, 6)) return false;
    int32_t rt = (int32_t)(b[0] | (b[1] << 8) | (b[2] << 16)) << 8 >> 8;
    int32_t rp = (int32_t)(b[3] | (b[4] << 8) | (b[5] << 16)) << 8 >> 8;
    tc = (float)rt / 65536.0f;
    pa = (float)rp / 64.0f;
    return true;
}

static float altitudeASL(float pa) {
    return 44330.77f * (1.0f - powf(pa / SEALEVEL_PA, 0.190263f));
}

static float altitudeAGL(float pa) {
    if (!padSet || pa <= 0.0f || padPa <= 0.0f) return 0.0f;
    const float t0 = padTc + 273.15f;
    return (t0 / 0.0065f) * (1.0f - powf(pa / padPa, 0.190263f));
}

void resetKalman() {
    k_alt = 0.0f; k_vel = 0.0f;
    P[0][0] = 1.0f; P[0][1] = 0.0f;
    P[1][0] = 0.0f; P[1][1] = 1.0f;
    lastUpdate = micros();
}

void setup() {
    Serial.begin(115200);
    uint32_t t0 = millis();
    while (!Serial && millis() - t0 < 3000) { }

    pinMode(PIN_FLASH_CS, OUTPUT);
    csHigh();
    SPI.begin();
    cmd1(CMD_RELEASE_PWRDN);
    delay(5);

    Wire.begin();
    Wire.setClock(400000);

    Serial.println("SPI Flash");
    SPI.beginTransaction(SPI_CFG); csLow();
    SPI.transfer(CMD_JEDEC_ID);
    uint8_t mf = SPI.transfer(0x00);
    csHigh(); SPI.endTransaction();
    if (mf == 0xFF || mf == 0x00) {
        Serial.println("SPI Flash Error");
        return;
    }

    uint8_t id = 0;
    if (!rd8(BMP_CHIP_ID, id)) {
        Serial.println("BMP585 Error");
        return;
    }

    Serial.println("Reflashing");
    eraseSector(0x000000);

    /* Soft reset is 0xB6, not 0x86 */
    wr8(BMP_CMD, 0xB6); delay(10);
    wr8(BMP_DSP_CONFIG, 0x1B);
    wr8(BMP_OSR_CFG, 0xC7);
    wr8(BMP_ODR_CFG, 0x45);

    float pa, tc;
    for (int i = 0; i < 10; i++) { baroRead(pa, tc); delay(40); }

    if (baroRead(pa, tc)) {
        padPa = pa; padTc = tc; padSet = true;
        resetKalman();
        nSamples = 0; sumPa = 0.0f; minPa = 1e9f; maxPa = -1e9f;
        
        Serial.println("Reading BMP585");
        logStartTime = millis();
        lastPrint = logStartTime;
        currentState = STATE_LOGGING;
    }
}

void loop() {
    if (currentState == STATE_LOGGING) {
        float pa, tc;
        if (!baroRead(pa, tc)) return;

        nSamples++;
        sumPa += pa;
        if (pa < minPa) minPa = pa;
        if (pa > maxPa) maxPa = pa;

        uint32_t now = micros();
        float dt = (now - lastUpdate) / 1000000.0f;
        lastUpdate = now;
        if (dt > 1.0f) dt = 0.04f;

        k_alt += k_vel * dt;
        P[0][0] += dt * (P[1][0] + P[0][1]) + dt * dt * P[1][1] + Q_alt;
        P[0][1] += dt * P[1][1];
        P[1][0] += dt * P[1][1];
        P[1][1] += Q_vel;

        float raw_agl = altitudeAGL(pa);
        float y = raw_agl - k_alt;
        float S = P[0][0] + R_meas;
        float K0 = P[0][0] / S;
        float K1 = P[1][0] / S;

        k_alt += K0 * y;
        k_vel += K1 * y;

        float P00_temp = P[0][0];
        float P01_temp = P[0][1];
        P[0][0] = (1.0f - K0) * P00_temp;
        P[0][1] = (1.0f - K0) * P01_temp;
        P[1][0] = -K1 * P00_temp + P[1][0];
        P[1][1] = -K1 * P01_temp + P[1][1];

        if (k_alt < 0.0f) {
            k_alt = 0.0f;
            if (k_vel < 0.0f) k_vel = 0.0f;
        }

        memcpy(&pageBuffer[bufferIdx], &k_alt, 4);
        bufferIdx += 4;

        if (millis() - lastPrint >= 2000) {
            lastPrint = millis();

            if (bufferIdx > 0) {
                writeBytes(flashAddr, pageBuffer, bufferIdx);
                flashAddr += 256;
                bufferIdx = 0;
            }

            const float mean = sumPa / (float)nSamples;
            const float spread = maxPa - minPa;

            Serial.printf("Pressure           %.1f Pa\n", pa);
            Serial.printf("Temperature        %.2f C\n", tc);
            Serial.printf("Abs Altitude (ASL) %.2f m\n", altitudeASL(pa));
            Serial.printf("Rel Height (AGL)   %.2f m\n", k_alt);
            Serial.printf("Mean Pressure      %.1f Pa over %lu samples\n", mean, (unsigned long)nSamples);
            Serial.printf("Spread             %.1f Pa, about %.2f m noise\n\n", spread, spread * 0.084f);

            nSamples = 0; sumPa = 0.0f; minPa = 1e9f; maxPa = -1e9f;
        }

        if (millis() - logStartTime >= 10000) {
            currentState = STATE_HALTED;
            Serial.println("Sensor Read fininshed(Simulated Crash)");
            Serial.println("Press r to reset using SPI Flash.\n");
        }

        delay(40);
    }
    else if (currentState == STATE_HALTED) {
        if (Serial.available()) {
            char c = (char)Serial.read();
            while (Serial.available()) Serial.read();

            if (c == 'f' || c == 'F') {
                Serial.println("Reading Flash");
                for (uint32_t page = 0; page < flashAddr; page += 256) {
                    uint8_t readBuf[256];
                    readBytes(page, readBuf, 256);

                    for (int i = 0; i < 200; i += 4) {
                        float saved_alt;
                        memcpy(&saved_alt, &readBuf[i], 4);
                        if (!isnan(saved_alt)) {
                             Serial.printf("Addr 0x%04X: %.2f m\n", page + i, saved_alt);
                        }
                    }
                }
                Serial.println("End Read");
            }
            else if (c == 'r' || c == 'R') {
                Serial.println("Erasing Sector 0");
                eraseSector(0x000000);
                resetKalman();
                flashAddr = 0;
                bufferIdx = 0;
                nSamples = 0; sumPa = 0.0f; minPa = 1e9f; maxPa = -1e9f;
                Serial.println("Logging Active 10s");
                logStartTime = millis();
                lastPrint = logStartTime;
                currentState = STATE_LOGGING;
            }
        }
    }
}