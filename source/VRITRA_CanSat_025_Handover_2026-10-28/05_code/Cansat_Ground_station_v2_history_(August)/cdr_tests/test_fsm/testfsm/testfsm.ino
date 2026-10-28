#include <Arduino.h>
#include <EEPROM.h>

/* ---------- states, as telemetered ----------------------------------------- */
enum : uint8_t { ST_BOOT = 0, ST_LAUNCH, ST_EJECT, ST_SEC_EJECT, ST_TOUCHDOWN };

static const char* stateName(uint8_t s) {
    switch (s) {
        case ST_BOOT:       return "boot";
        case ST_LAUNCH:     return "launch detected";
        case ST_EJECT:      return "apogee, primary parachute";
        case ST_SEC_EJECT:  return "secondary mechanism";
        case ST_TOUCHDOWN:  return "touchdown";
        default:            return "unknown";
    }
}

/* ---------- thresholds ----------------------------------------------------- */
#define SEC_EJECT_AGL_M     600.0f      /* fixed by the guideline, +-10 m */
#define LAUNCH_ALT_M         30.0f
#define LAUNCH_VS_MS          5.0f
#define LAUNCH_SAMPLES          4
#define APOGEE_DROP_M         5.0f
#define LAND_ALT_M           30.0f
#define LAND_VS_MS            1.0f
#define LAND_SAMPLES           20

/* ---------- persisted state ------------------------------------------------ */
#define EE_MAGIC_ADDR   300
#define EE_STATE_ADDR   304
#define EE_APOGEE_ADDR  308
#define EE_MAGIC        0x56464D31UL    /* 'VFM1' */

static uint8_t state = ST_BOOT;
static bool    armed = false;
static float   apogeeM = 0.0f;
static int     riseCount = 0, stillCount = 0;

/* ---------- transition audit ----------------------------------------------- */
static bool sawLaunch = false, sawApogee = false, sawSecond = false, sawLand = false;
static float secondAtM = 0.0f;

static void rule() { Serial.println(F("")); }

static void persist() {
    uint32_t magic = EE_MAGIC;
    EEPROM.put(EE_MAGIC_ADDR, magic);
    EEPROM.put(EE_STATE_ADDR, state);
    EEPROM.put(EE_APOGEE_ADDR, apogeeM);
}

static void resetRun() {
    state = ST_BOOT; apogeeM = 0; riseCount = 0; stillCount = 0;
    sawLaunch = sawApogee = sawSecond = sawLand = false;
    secondAtM = 0;
    persist();
}

/* One step of the state machine. Everything it needs is passed in, so the
 * caller decides whether the numbers came from a sensor or from a profile. */
static void step(float altM, float vsMs, float tSec) {
    if (altM > apogeeM) apogeeM = altM;
    const uint8_t was = state;

    switch (state) {
    case ST_BOOT:
        /* Arming is a deliberate act. Guideline 6.1.v makes the transmit
         * command the last thing before flight, so that command arms too. */
        if (armed && altM > LAUNCH_ALT_M && vsMs > LAUNCH_VS_MS) {
            if (++riseCount >= LAUNCH_SAMPLES) state = ST_LAUNCH;
        } else riseCount = 0;
        break;

    case ST_LAUNCH:
        /* Apogee is a fall away from the highest point seen, not a sign change
         * on vertical speed — noise crosses zero many times near the top. */
        if (vsMs < 0.0f && altM < apogeeM - APOGEE_DROP_M) state = ST_EJECT;
        break;

    case ST_EJECT:
        if (altM < SEC_EJECT_AGL_M) state = ST_SEC_EJECT;
        break;

    case ST_SEC_EJECT:
        if (altM < LAND_ALT_M && fabsf(vsMs) < LAND_VS_MS) {
            if (++stillCount >= LAND_SAMPLES) state = ST_TOUCHDOWN;
        } else stillCount = 0;
        break;

    default: break;
    }

    if (state != was) {
        Serial.printf("  t %6.1f s   height %7.1f m   rate %+7.1f m/s   -> %s\n",
                      tSec, altM, vsMs, stateName(state));
        if (state == ST_LAUNCH)    sawLaunch = true;
        if (state == ST_EJECT)     sawApogee = true;
        if (state == ST_SEC_EJECT) { sawSecond = true; secondAtM = altM; }
        if (state == ST_TOUCHDOWN) sawLand = true;
        persist();
    }
}

/* ---------- the injected profile ------------------------------------------- */
/* Ten samples a second, which is the rate the flight firmware runs its state
 * machine at. Up at sixty metres per second to a thousand, then down under the
 * primary at twenty, and under the secondary at two. */
static void runProfile() {
    Serial.println();
    Serial.println(F("  Injected flight profile"));
    rule();
    resetRun();
    armed = true;
    Serial.println(F("  Armed. This is what the transmit command does in flight."));

    float alt = 0.0f, prev = 0.0f;
    const float dt = 0.1f;
    float t = 0.0f;

    /* climb: 0 to 1000 m at 60 m/s, about 16.7 s */
    for (int k = 0; k < 170; k++) {
        prev = alt;
        alt = min(1000.0f, 60.0f * t);
        step(alt, (alt - prev) / dt, t);
        t += dt;
    }
    /* coast over the top for a second so apogee is a real turn, not a corner */
    for (int k = 0; k < 20; k++) {
        prev = alt;
        alt = 1000.0f - 0.5f * 9.81f * powf(k * dt, 2.0f) * 0.15f;
        step(alt, (alt - prev) / dt, t);
        t += dt;
    }
    /* primary descent at 20 m/s down to 600 m */
    while (alt > 600.0f) {
        prev = alt;
        alt -= 20.0f * dt;
        step(alt, (alt - prev) / dt, t);
        t += dt;
    }
    /* secondary descent at 2 m/s to the ground */
    while (alt > 0.0f) {
        prev = alt;
        alt = max(0.0f, alt - 2.0f * dt);
        step(alt, (alt - prev) / dt, t);
        t += dt;
    }
    /* sit still so touchdown can latch */
    for (int k = 0; k < 40; k++) { step(0.0f, 0.0f, t); t += dt; }

    rule();
    Serial.printf("  Highest point reached %.1f m\n", apogeeM);
    Serial.printf("  Secondary fired at    %.1f m\n", secondAtM);
    Serial.println();
    Serial.println(sawLaunch ? F("  PASS  launch detected on a sustained climb")
                             : F("  FAIL  launch never detected"));
    Serial.println(sawApogee ? F("  PASS  apogee detected and primary Parachute deployed")
                             : F("  FAIL  apogee never detected"));
    Serial.println(fabsf(secondAtM - SEC_EJECT_AGL_M) <= 10.0f && sawSecond
                   ? F("  PASS  secondary Parachute deployed")
                   : F("  FAIL  secondary outside the required window"));
    Serial.println(sawLand ? F("  PASS  Landed Properly")
                           : F("  FAIL  touchdown never latched"));
    rule();
}

static void help() {
    Serial.println();
    Serial.println(F("  p          run the whole injected flight profile"));
    Serial.println(F("  a          arm, which the transmit command does in flight"));
    Serial.println(F("  h <m> <v>  inject one sample by hand, height and rate"));
    Serial.println(F("  z          reset to boot and clear the stored state"));
    Serial.println(F("  s          show the current state"));
    rule();
}

void setup() {
    Serial.begin(115200);
    uint32_t t0 = millis();
    while (!Serial && millis() - t0 < 3000) { }

    Serial.println();
    Serial.println(F("  Flight state machine test — injected values"));
    rule();

    uint32_t magic = 0;
    EEPROM.get(EE_MAGIC_ADDR, magic);
    if (magic == EE_MAGIC) {
        EEPROM.get(EE_STATE_ADDR, state);
        EEPROM.get(EE_APOGEE_ADDR, apogeeM);
        if (state > ST_TOUCHDOWN) state = ST_BOOT;
        Serial.printf("", state, stateName(state));
        Serial.printf("  Highest point of cansat  %.1f m\n", apogeeM);
        Serial.println(F(""));
    } else {
        Serial.println(F(" ."));
        resetRun();
    }
    Serial.println();
   
    help();
}

void loop() {
    if (!Serial.available()) return;
    char buf[32];
    uint8_t n = 0;
    const uint32_t t0 = millis();
    while (millis() - t0 < 200 && n < sizeof(buf) - 1) {
        if (!Serial.available()) continue;
        const char c = (char)Serial.read();
        if (c == '\n' || c == '\r') break;
        buf[n++] = c;
    }
    buf[n] = '\0';
    if (n == 0) return;

    switch (buf[0]) {
    case 'p': case 'P': runProfile(); break;
    case 'a': case 'A':
        armed = true;
        Serial.println(F("\n  Armed."));
        break;
    case 'z': case 'Z':
        resetRun();
        armed = false;
        Serial.println(F("\n  Reset to boot, stored state cleared."));
        break;
    case 's': case 'S':
        Serial.printf("\n  State %u, %s. Highest point %.1f m. %s\n",
                      state, stateName(state), apogeeM,
                      armed ? "Armed." : "Not armed.");
        break;
    case 'h': case 'H': {
        float m = 0, v = 0;
        if (sscanf(buf + 1, "%f %f", &m, &v) == 2) {
            Serial.printf("\n  Injecting height %.1f m, rate %+.1f m/s\n", m, v);
            step(m, v, millis() / 1000.0f);
            Serial.printf("  State is now %s\n", stateName(state));
        } else {
            Serial.println(F("\n  Use  h <height> <rate>, for example  h 700 -18"));
        }
        break;
    }
    default: help(); break;
    }
}
