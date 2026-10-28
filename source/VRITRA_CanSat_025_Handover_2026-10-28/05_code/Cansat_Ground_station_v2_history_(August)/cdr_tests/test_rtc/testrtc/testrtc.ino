#include <Arduino.h>
#include <Wire.h>
#include <EEPROM.h>

#define rtcAddr 0x68

uint8_t decToBcd(uint8_t val) {
    return (val / 10 * 16) + (val % 10);
}

uint8_t bcdToDec(uint8_t val) {
    return (val / 16 * 10) + (val % 16);
}

void setIstTime() {
    Wire.beginTransmission(rtcAddr);
    Wire.write(0x00); 
    Wire.write(decToBcd(0));   // Seconds
    Wire.write(decToBcd(28));  // Minutes
    Wire.write(decToBcd(17));  // Hours
    Wire.write(decToBcd(6));   // Day of week (Friday is 6)
    Wire.write(decToBcd(7));   // Date
    Wire.write(decToBcd(8));   // Month
    Wire.write(decToBcd(26));  // Year
    Wire.endTransmission();
}

void setup() {
    Serial.begin(115200);
    delay(3000);

    Wire.begin();
    Wire.setClock(400000);

    Serial.println("Booting RTC Module");

    /* Target minute we want to flash */
    uint8_t targetMinute = 28;
    
    /* Read the last configured minute from Teensy internal memory */
    uint8_t lastSetMinute = EEPROM.read(0);

    /* If they do not match, this is a fresh flash. Set the time. */
    if (lastSetMinute != targetMinute) {
        Serial.println("Setting IST time to August 7 2026 17 28 00");
        setIstTime();
        /* Save the flag so it never sets this time again */
        EEPROM.write(0, targetMinute);
    } else {
        Serial.println("RTC battery active Time preserved");
    }
    
    Serial.println();
}

void loop() {
    Wire.beginTransmission(rtcAddr);
    Wire.write(0x00);
    Wire.endTransmission();
    Wire.requestFrom(rtcAddr, 7);

    uint8_t seconds = bcdToDec(Wire.read() & 0x7F);
    uint8_t minutes = bcdToDec(Wire.read());
    uint8_t hours   = bcdToDec(Wire.read() & 0x3F);
    uint8_t day     = bcdToDec(Wire.read());
    uint8_t date    = bcdToDec(Wire.read());
    uint8_t month   = bcdToDec(Wire.read() & 0x7F);
    uint8_t year    = bcdToDec(Wire.read());

    Wire.beginTransmission(rtcAddr);
    Wire.write(0x11);
    Wire.endTransmission();
    Wire.requestFrom(rtcAddr, 2);

    int8_t tempMsb = Wire.read();
    uint8_t tempLsb = Wire.read();
    float temperature = tempMsb + ((tempLsb >> 6) * 0.25f);

    Serial.printf("Date 20%02d/%02d/%02d Time %02d:%02d:%02d Temp %.2f C\n",
                  year, month, date, hours, minutes, seconds, temperature);

    delay(1000);
}