#include <Arduino.h>
#include <Wire.h>

#define icmAddr 0x69
#define icmWhoAmI 0x00
#define icmPwrMgmt1 0x06
#define icmAccelXout 0x2D
#define icmBankSel 0x7F
#define icmB2AccCfg 0x14

#define accLsbPerG 2048.0f

float homeRoll = 0.0f;
float homePitch = 0.0f;
float homeAx = 0.0f;
float homeAy = 0.0f;
float homeAz = 0.0f;

int currentStage = 0; 

bool wr8(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(icmAddr);
    Wire.write(reg);
    Wire.write(val);
    return Wire.endTransmission() == 0;
}

bool rdN(uint8_t reg, uint8_t* buf, uint8_t n) {
    Wire.beginTransmission(icmAddr);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((int)icmAddr, (int)n) != n) return false;
    for (int i = 0; i < n; i++) buf[i] = Wire.read();
    return true;
}

bool rd8(uint8_t reg, uint8_t& out) {
    return rdN(reg, &out, 1);
}

bool getAccel(float& ax, float& ay, float& az) {
    uint8_t r[6];
    if (!rdN(icmAccelXout, r, 6)) return false;
    ax = (int16_t)((r[0] << 8) | r[1]) / accLsbPerG;
    ay = (int16_t)((r[2] << 8) | r[3]) / accLsbPerG;
    az = (int16_t)((r[4] << 8) | r[5]) / accLsbPerG;
    return true;
}

void getAngles(float ax, float ay, float az, float& roll, float& pitch) {
    roll = atan2(ay, az) * 180.0 / PI;
    pitch = atan2(-ax, sqrt(ay * ay + az * az)) * 180.0 / PI;
}

float getDeviation(float current, float home) {
    float diff = current - home;
    while (diff > 180.0) diff -= 360.0;
    while (diff < -180.0) diff += 360.0;
    return diff;
}

void takeSamplesAndDeviations() {
    for (int i = 1; i <= 5; i++) {
        float ax, ay, az;
        getAccel(ax, ay, az);
        
        float roll, pitch;
        getAngles(ax, ay, az, roll, pitch);

        float devRoll = getDeviation(roll, homeRoll);
        float devPitch = getDeviation(pitch, homePitch);

        Serial.printf("Sample %d: ax=%.2fg ay=%.2fg az=%.2fg roll=%.2fdeg pitch=%.2fdeg dev roll=%.2fdeg dev pitch=%.2fdeg\n", 
                      i, ax, ay, az, roll, pitch, devRoll, devPitch);
        delay(200);
    }
}

void setup() {
    Serial.begin(115200);
    delay(3000);

    Wire.begin();
    Wire.setClock(400000);

    Serial.println("Booting IMU");

    wr8(icmPwrMgmt1, 0x80); 
    delay(100);
    wr8(icmPwrMgmt1, 0x01); 
    delay(20);
    wr8(icmBankSel, 0x00);

    uint8_t who = 0;
    if (!rd8(icmWhoAmI, who) || who != 0xEA) {
        Serial.println("IMU not found");
        while (1);
    }

    wr8(icmBankSel, 0x20);
    wr8(icmB2AccCfg, 0x1F);
    wr8(icmBankSel, 0x00);
    delay(50);

    float sumAx = 0, sumAy = 0, sumAz = 0;
    for (int i = 0; i < 50; i++) {
        float tx, ty, tz;
        getAccel(tx, ty, tz);
        sumAx += tx; 
        sumAy += ty; 
        sumAz += tz;
        delay(10);
    }
    
    homeAx = sumAx / 50.0;
    homeAy = sumAy / 50.0;
    homeAz = sumAz / 50.0;
    getAngles(homeAx, homeAy, homeAz, homeRoll, homePitch);

    Serial.println();
    Serial.println("Home position");
    takeSamplesAndDeviations();
    
    Serial.println();
    Serial.println("Imu Tilt Left");
    currentStage = 1;
}

void loop() {
    if (Serial.available()) {
        while (Serial.available()) Serial.read(); 

        if (currentStage == 1) {
            Serial.println();
            Serial.println("Left 90 Tilt Reading");
            takeSamplesAndDeviations();
            Serial.println();
            Serial.println("Tilt IMU Right 90 degrees");
            currentStage = 2;
        } 
        else if (currentStage == 2) {
            Serial.println();
            Serial.println("Right 90 Tilt Reading");
            takeSamplesAndDeviations();
            Serial.println();
            Serial.println("Tilt IMU Front 90 degrees");
            currentStage = 3;
        } 
        else if (currentStage == 3) {
            Serial.println();
            Serial.println("Front 90 Tilt Reading");
            takeSamplesAndDeviations();
            Serial.println();
            Serial.println("Tilt IMU Back 90 degrees");
            currentStage = 4;
        } 
        else if (currentStage == 4) {
            Serial.println();
            Serial.println("Back 90 Tilt Reading");
            takeSamplesAndDeviations();
            Serial.println();
            Serial.println("Test Complete");
            currentStage = 5;
        }
    }
}