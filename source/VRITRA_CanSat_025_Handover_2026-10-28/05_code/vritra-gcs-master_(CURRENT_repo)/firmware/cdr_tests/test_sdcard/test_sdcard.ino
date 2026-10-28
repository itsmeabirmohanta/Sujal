#include <Arduino.h>
#include <Wire.h>
#include <SdFat.h>

#define TeamIdFull "2026-INSPACe-CAN-7USAT-025"
#define rtcAddr 0x68

static const char* CsvHeader =
    "Team ID,Time S,Packet,Alt m,Pres Pa,Temp C,Bus V,GNSS Time,Lat,Lon,"
    "GPS Alt m,Sats,Acc X,Acc Y,Acc Z,Spin Rate,State";

static const char* TestFile  = "CDRTEST.CSV";
static const char* SpeedFile = "SPEED.BIN";

SdFs sd;
FsFile f;

static bool cardOk = false;

/* RTC BCD Converter */
uint8_t bcdToDec(uint8_t val) {
    return (val / 16 * 10) + (val % 16);
}

/* Fetches HH:MM:SS from DS3231 */
void getRtcTime(char* timeBuf) {
    Wire.beginTransmission(rtcAddr);
    Wire.write(0x00);
    if (Wire.endTransmission() != 0) {
        strcpy(timeBuf, "-");
        return;
    }
    Wire.requestFrom((int)rtcAddr, 3);
    if (Wire.available() >= 3) {
        uint8_t sec = bcdToDec(Wire.read() & 0x7F);
        uint8_t min = bcdToDec(Wire.read());
        uint8_t hr  = bcdToDec(Wire.read() & 0x3F);
        snprintf(timeBuf, 10, "%02d:%02d:%02d", hr, min, sec);
    } else {
        strcpy(timeBuf, "-");
    }
}

/* buildRow now takes the RTC string directly into the second column */
static int buildRow(char* out, int cap, const char* timeStr, uint32_t pkt,
                    float altM, float presPa, float tempC, float busV,
                    bool haveFix, uint32_t gnssTime, double lat, double lon,
                    float gpsAlt, uint8_t sats,
                    float axMs2, float ayMs2, float azMs2,
                    float spinDps, uint8_t state) {
    
    int n = snprintf(out, cap, "%s,%s,%lu,%.1f,%.0f,%.1f,%.2f,",
                     TeamIdFull, timeStr, (unsigned long)pkt,
                     altM, presPa, tempC, busV);
    
    if (haveFix) {
        n += snprintf(out + n, cap - n, "%lu,%.7f,%.7f,%.1f,%u,",
                      (unsigned long)gnssTime, lat, lon, gpsAlt, sats);
    } else {
        n += snprintf(out + n, cap - n, "-,-,-,-,%u,", sats);
    }
    
    n += snprintf(out + n, cap - n, "%.2f,%.2f,%.2f,%.1f,%u",
                  axMs2, ayMs2, azMs2, spinDps, state);
    return n;
}

static void showCard() {
    Serial.printf("Card type SDHC or SDXC\n");
    const uint64_t sectors = sd.card()->sectorCount();
    Serial.printf("Capacity %.2f GB\n", sectors * 512.0 / 1e9);
    Serial.printf("File system FAT%d\n", (int)sd.fatType());
    const uint64_t freeClusters = sd.freeClusterCount();
    const uint64_t bytesPerCluster = (uint64_t)sd.bytesPerCluster();
    Serial.printf("Free space %.2f GB\n", freeClusters * bytesPerCluster / 1e9);
    Serial.printf("Used %.1f %%\n",
                  100.0 - (freeClusters * bytesPerCluster * 100.0) / (sectors * 512.0));
}

static void testWriteReadBack() {
    Serial.println();
    Serial.println("Writing 200 rows with live RTC timestamps");
    sd.remove(TestFile);
    if (!f.open(TestFile, O_WRITE | O_CREAT | O_TRUNC)) {
        Serial.println("Error could not create the file");
        return;
    }
    f.println(CsvHeader);

    char row[240];
    char timeStr[10];

    for (uint32_t i = 1; i <= 200; i++) {
        /* Read exact real time for this specific row */
        getRtcTime(timeStr);

        const float alt   = 1000.0f - i * 4.0f;
        const float pres  = 101325.0f * powf(1.0f - (alt * 0.0065f) / 298.15f, 5.2559f);
        const bool  fixed = (i > 5); 
        
        buildRow(row, sizeof(row), timeStr, i, alt, pres, 24.5f, 11.85f,
                 fixed, 143052UL, 31.2503000, 75.7033000, alt + 5.0f,
                 fixed ? 9 : 0, 0.12f, -0.34f, -9.79f, 42.5f, 3);
        
        f.println(row);
        if ((i % 40) == 0) f.sync();
        
        /* Slowed down slightly so the RTC seconds visibly change during the test dump */
        delay(25); 
    }
    f.sync();
    const uint32_t wrote = f.fileSize();
    f.close();
    Serial.printf("Wrote %lu bytes\n", (unsigned long)wrote);

    Serial.println("Reading back raw CSV data for Excel:");
    Serial.println();
    
    if (!f.open(TestFile, O_READ)) {
        Serial.println("Error could not reopen the file");
        return;
    }
    
    int rows = 0;
    char rb[260];
    while (f.available()) {
        const int n = f.fgets(rb, sizeof(rb));
        if (n <= 0) break;
        rows++;
        
        /* Print first 10 rows to prove format */
        if (rows <= 10) { 
            Serial.print(rb); 
        }
    }
    f.close();
    Serial.println();
    Serial.printf("Read %d rows total\n", rows);
}

static void testSustainedRate() {
    Serial.println();
    Serial.println("Testing sustained write rate");
    sd.remove(SpeedFile);
    if (!f.open(SpeedFile, O_WRITE | O_CREAT | O_TRUNC)) {
        Serial.println("Error could not create speed file");
        return;
    }
    
    if (!f.preAllocate(2UL * 1024UL * 1024UL)) {
        Serial.println("Warning could not pre allocate");
    }

    uint8_t block[512];
    for (int i = 0; i < 512; i++) block[i] = (uint8_t)i;
    const int N = 2000;
    uint32_t worst = 0;
    const uint32_t t0 = micros();
    
    for (int i = 0; i < N; i++) {
        const uint32_t a = micros();
        f.write(block, sizeof(block));
        const uint32_t took = micros() - a;
        if (took > worst) worst = took;
    }
    
    f.sync();
    const uint32_t total = micros() - t0;
    f.close();
    sd.remove(SpeedFile);

    const float kbps = (N * 512.0f) / (total / 1e6f) / 1024.0f;
    Serial.printf("Wrote 1 MB in %.2f s at %.0f kB per second\n", total / 1e6f, kbps);
}

void setup() {
    Serial.begin(115200);
    uint32_t t0 = millis();
    while (!Serial && millis() - t0 < 3000) { }

    Wire.begin();
    Wire.setClock(400000);

    Serial.println();
    Serial.println("MicroSD on SDIO bus init");

    if (!sd.begin(SdioConfig(FIFO_SDIO))) {
        Serial.println("Error no card or format invalid");
        return;
    }
    cardOk = true;
    showCard();

    testWriteReadBack();
    testSustainedRate();

    Serial.println();
    Serial.println("Send d to list files");
    Serial.println("Send r to restart test");
}

void loop() {
    if (!Serial.available()) return;
    const char c = (char)Serial.read();
    while (Serial.available()) Serial.read();
    if (!cardOk) return;

    if (c == 'd' || c == 'D') {
        Serial.println("\nFiles on card");
        sd.ls(LS_DATE | LS_SIZE);
        Serial.println();
    } else if (c == 'r' || c == 'R') {
        testWriteReadBack();
    }
}