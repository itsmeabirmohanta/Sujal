/* =============================================================================
 * test_flash.ino — 7Semi serial NOR flash (W25Q family) on the SPI0 bus
 * Team 2026-INSPACe-CAN-7USAT-025 · Teensy 4.1
 * =============================================================================
 * Standalone. Nothing from vritra_fc/ is included.
 * Serial Monitor at 115200. Prints this part's own parameters only.
 *
 * WIRING. The radio owns SPI1 on pins 26, 27 and 39, so the flash goes on
 * SPI0, which nothing else uses because the card is on the SDIO socket.
 *
 *     VCC          3.3 V     never 5 V, these parts are 3.3 V only
 *     GND          GND
 *     CS           5         pin 10 was occupied; changed to 5
 *     DI  (MOSI)   11        fixed by hardware
 *     DO  (MISO)   12        fixed by hardware
 *     CLK          13        fixed by hardware
 *     WP           3.3 V     tie high (module board does this internally)
 *     HOLD         3.3 V     tie high (module board does this internally)
 *
 * TIE WP AND HOLD HIGH. Left floating, the part answers its identity happily
 * and then silently refuses every write, which reads exactly like a dead chip.
 * If the identity is right but nothing ever writes, check these two pins first.
 *
 * PIN 13 IS ALSO THE ON-BOARD LED. It will flicker during every access. That
 * is normal and harmless, and it is why the switch and indicator test uses an
 * external LED on pin 14 instead.
 * ========================================================================== */
#include <Arduino.h>
#include <SPI.h>

#define PIN_FLASH_CS  5

/* ---------- standard serial flash commands --------------------------------- */
#define CMD_WRITE_ENABLE    0x06
#define CMD_READ_STATUS     0x05
#define CMD_READ_DATA       0x03
#define CMD_PAGE_PROGRAM    0x02
#define CMD_SECTOR_ERASE    0x20    /* 4 kB */
#define CMD_JEDEC_ID        0x9F
#define CMD_RELEASE_PWRDN   0xAB

static SPISettings SPI_CFG(8000000, MSBFIRST, SPI_MODE0);
static bool present = false;
static uint32_t partBytes = 0;

static void csLow()  { digitalWrite(PIN_FLASH_CS, LOW);  }
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

static void jedec(uint8_t& mf, uint8_t& type, uint8_t& cap) {
    SPI.beginTransaction(SPI_CFG); csLow();
    SPI.transfer(CMD_JEDEC_ID);
    mf   = SPI.transfer(0x00);
    type = SPI.transfer(0x00);
    cap  = SPI.transfer(0x00);
    csHigh(); SPI.endTransaction();
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

static const char* makerName(uint8_t mf) {
    switch (mf) {
        case 0xEF: return "Winbond";
        case 0xC8: return "GigaDevice";
        case 0x20: return "Micron";
        case 0x1F: return "Adesto";
        case 0xC2: return "Macronix";
        default:   return "unrecognised";
    }
}

static void rule() { Serial.println(F("  ------------------------------------------------")); }

static void runVerify() {
    const uint32_t addr = 0x000000;
    uint8_t wbuf[256], rbuf[256];
    for (int i = 0; i < 256; i++) wbuf[i] = (uint8_t)(i * 7 + 13);

    Serial.println();
    Serial.println(F("  Erasing the first four kilobyte sector"));
    if (!eraseSector(addr)) {
        Serial.println(F("  FAIL  erase never completed"));
        Serial.println(F("        the write enable latch did not set — check WP and HOLD"));
        return;
    }
    readBytes(addr, rbuf, 256);
    bool blank = true;
    for (int i = 0; i < 256; i++) if (rbuf[i] != 0xFF) blank = false;
    Serial.println(blank ? F("  PASS  the sector erased to all ones")
                         : F("  FAIL  the sector did not erase"));
    if (!blank) return;

    Serial.println(F("  Writing one full page of 256 bytes"));
    if (!writeBytes(addr, wbuf, 256)) {
        Serial.println(F("  FAIL  the write never completed"));
        return;
    }
    readBytes(addr, rbuf, 256);
    int wrong = 0;
    for (int i = 0; i < 256; i++) if (rbuf[i] != wbuf[i]) wrong++;
    Serial.printf("  Compared 256 bytes, %d differ\n", wrong);
    Serial.println(wrong == 0 ? F("  PASS  what came back is exactly what went in")
                              : F("  FAIL  the data did not survive the round trip"));

    Serial.println(F("  Erasing again so the part is left clean"));
    eraseSector(addr);
    readBytes(addr, rbuf, 16);
    bool clean = true;
    for (int i = 0; i < 16; i++) if (rbuf[i] != 0xFF) clean = false;
    Serial.println(clean ? F("  PASS  sector left erased")
                         : F("  WARN  sector not fully erased"));
}

void setup() {
    Serial.begin(115200);
    uint32_t t0 = millis();
    while (!Serial && millis() - t0 < 3000) { }

    pinMode(PIN_FLASH_CS, OUTPUT);
    csHigh();
    SPI.begin();

    Serial.println();
    Serial.println(F("  Serial flash test — 7Semi W25Q on SPI0"));
    rule();
    Serial.printf("  Chip select %d, data in 11, data out 12, clock 13\n", PIN_FLASH_CS);
    Serial.println(F("  The on-board LED shares pin 13 and will flicker. Normal."));
    Serial.println();

    cmd1(CMD_RELEASE_PWRDN);
    delay(5);

    uint8_t mf, type, cap;
    jedec(mf, type, cap);
    Serial.printf("  Manufacturer   0x%02X  %s\n", mf, makerName(mf));
    Serial.printf("  Memory type    0x%02X\n", type);
    Serial.printf("  Capacity code  0x%02X\n", cap);

    if (mf == 0x00 || mf == 0xFF) {
        Serial.println(F("  FAIL  no answer on the bus"));
        Serial.println(F("        check chip select, the 3.3 V supply, and that data"));
        Serial.println(F("        out is on pin 12 rather than swapped with pin 11"));
        return;
    }

    if (cap >= 0x10 && cap <= 0x1A) {
        partBytes = 1UL << cap;
        Serial.printf("  Size           %lu KB, %lu Mbit\n",
                      (unsigned long)(partBytes / 1024UL),
                      (unsigned long)(partBytes / 131072UL));
    } else {
        Serial.println(F("  WARN  capacity code outside the usual range"));
    }
    Serial.printf("  Status         0x%02X\n", status());
    present = true;
    Serial.println(F("  PASS  the part answered its identity"));

    runVerify();

    rule();
    Serial.println(F("  Send  r  to run the erase, write and verify again."));
    Serial.println(F("  Send  d  to dump the first sixty four bytes."));
}

void loop() {
    if (!Serial.available()) return;
    const char c = (char)Serial.read();
    while (Serial.available()) Serial.read();
    if (!present) return;

    if (c == 'r' || c == 'R') {
        runVerify();
    } else if (c == 'd' || c == 'D') {
        uint8_t b[64];
        readBytes(0, b, 64);
        Serial.println(F("\n  First sixty four bytes"));
        for (int i = 0; i < 64; i++) {
            if ((i % 16) == 0) Serial.printf("\n  %04X  ", i);
            Serial.printf("%02X ", b[i]);
        }
        Serial.println("\n");
    }
}
