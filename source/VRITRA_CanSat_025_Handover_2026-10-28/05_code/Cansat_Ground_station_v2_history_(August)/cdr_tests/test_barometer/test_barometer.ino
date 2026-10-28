/* =============================================================================
 * test_barometer_kalman_2sec.ino — BMP585 with 1D Kalman & 2-Second Display
 * ========================================================================== */
#include <Arduino.h>
#include <Wire.h>

#define PIN_SDA     18
#define PIN_SCL     19
#define BMP_ADDR    0x47        

/* ---------- registers (Source: Bosch BMP585 Datasheet) --------------------- */
#define BMP_CHIP_ID    0x01
#define BMP_DATA       0x1D      
#define BMP_ODR_CFG    0x36      
#define BMP_OSR_CFG    0x37      
#define BMP_DSP_CONFIG 0x38      
#define BMP_CMD        0x7E      

#define SEALEVEL_PA    100340.0f 

static float padPa = 0.0f, padTc = 0.0f;
static bool  padSet = false;


static uint32_t nSamples = 0;
static float minPa = 1e9f, maxPa = -1e9f, sumPa = 0.0f;
static uint32_t lastPrint = 0;


static float k_alt = 0.0f; 
static float k_vel = 0.0f; 
static float P[2][2] = {{1.0f, 0.0f}, {0.0f, 1.0f}}; 
static uint32_t lastUpdate = 0;


const float Q_alt = 0.0001f;  
const float Q_vel = 0.001f;   
const float R_meas = 3.0f;    

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
    k_alt = 0.0f;
    k_vel = 0.0f;
    P[0][0] = 1.0f; P[0][1] = 0.0f;
    P[1][0] = 0.0f; P[1][1] = 1.0f;
    lastUpdate = micros();
}

void setup() {
    Serial.begin(115200);
    uint32_t t0 = millis();
    while (!Serial && millis() - t0 < 3000) { }

    Wire.begin();
    Wire.setClock(400000); 

    Serial.println();
    Serial.println(F("  BMP585 with 1D Kalman (2-Second Output)"));
    Serial.println(F("  ------------------------------------------------"));

    uint8_t id = 0;
    if (!rd8(BMP_CHIP_ID, id)) return;

    wr8(BMP_CMD, 0xB6);  
    delay(10);        

    wr8(BMP_DSP_CONFIG, 0x1B);  
    wr8(BMP_OSR_CFG, 0xC7);     
    wr8(BMP_ODR_CFG, 0x45);     
    
    Serial.println(F("  Warming up IIR filter..."));
    float pa, tc;
    for (int i = 0; i < 10; i++) {
        baroRead(pa, tc);
        delay(40);
    }

    if (baroRead(pa, tc)) {
        padPa = pa; padTc = tc; padSet = true;
        resetKalman();
        
        
        nSamples = 0; sumPa = 0.0f; minPa = 1e9f; maxPa = -1e9f;
        lastPrint = millis();
        
        Serial.printf("  Pad reference locked at %.1f Pa and %.2f C\n", padPa, padTc);
        Serial.println(F("  ------------------------------------------------"));
    }
}

void loop() {
    if (Serial.available()) {
        while (Serial.available()) Serial.read();
        float pa, tc;
        if (baroRead(pa, tc)) {
            padPa = pa; padTc = tc;
            resetKalman();
            nSamples = 0; sumPa = 0.0f; minPa = 1e9f; maxPa = -1e9f;
            Serial.println(F("\n  Pad reference recaptured. Kalman & Stats reset.\n"));
        }
    }

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
        
        if (k_vel < 0.0f) {
            k_vel = 0.0f;
        }
    }

    
    if (millis() - lastPrint >= 2000) {
        lastPrint = millis();
        
        const float mean   = sumPa / (float)nSamples;
        const float spread = maxPa - minPa;
        
        Serial.printf("  Pressure           %9.1f Pa\n", pa);
        Serial.printf("  Temperature        %9.2f C\n", tc);
        Serial.printf("  Abs Altitude (ASL) %9.2f m (real world altitude)\n", altitudeASL(pa));
        
        
        Serial.printf("  Rel Height (AGL)   %9.2f m \n", k_alt);
        
        Serial.printf("  Mean Pressure      %9.1f Pa over %lu samples\n", mean, (unsigned long)nSamples);
        
    }
    
    
    delay(40); 
}