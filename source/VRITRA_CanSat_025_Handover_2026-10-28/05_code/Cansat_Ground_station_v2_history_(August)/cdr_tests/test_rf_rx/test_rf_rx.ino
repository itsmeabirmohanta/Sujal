/*
 * test_rf_rx.ino
 * Team 2026 INSPACe CAN 7USAT 025
 *
 * Range test receiver and commander. Runs on the ESP32, the ground station
 * side. Works with test_rf_tx.ino on the Teensy 4.1.
 *
 * For every packet it prints the contents and the radio numbers, RSSI and SNR,
 * so you can read the link margin while walking a field. It also sends commands
 * up to the CanSat, start telemetry, stop, and calibrate.
 *
 * WHY COMMANDS SOMETIMES DID NOT TAKE BEFORE, and what is fixed here.
 *
 *  1. The serial monitor line ending. If the monitor is set to "No line
 *     ending" the text you type never arrives with a newline, so the old code
 *     kept collecting letters and never acted on them. It looked completely
 *     dead. This version also acts on a short pause in typing, so it works
 *     whichever line ending the monitor is set to. Newline is still fine too.
 *
 *  2. The listen window. While the CanSat is sending 1 Hz telemetry it can only
 *     spare a short window to listen, and the old flight timing opened that
 *     window one packet in three, so a command had to get lucky to land. The
 *     range test transmitter now opens a window after EVERY packet, and this
 *     sketch fires the command in the moment right after it hears a packet, so
 *     it drops straight into the open window. Before the CanSat is sending at
 *     all it listens continuously, so the start command lands at once.
 *
 *  3. Knowing it worked. The CanSat echoes the number of the last command it
 *     obeyed inside its telemetry. This sketch watches for that echo and tells
 *     you "the CanSat confirmed ...", then stops repeating. So you get a real
 *     acknowledgement, not just hope.
 *
 * The radio settings are handed to us by the organisers, frequency allocation
 * plan row 025. Do not change the frequency, spreading factor, bandwidth,
 * coding rate, sync word or power.
 */

#include <Arduino.h>
#include <RadioLib.h>
#include <SPI.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

#define TeamId "025"

/* radio settings from the frequency allocation plan, row 025 */
#define FreqMhz       867.00f
#define BwKhz         125.0f
#define Sf            10
#define Cr            5
#define SyncWord      0x98
#define PreambleLen   8
#define TcxoVolts     1.6f
#define UseHpPa       true
#define UplinkPower   10

/* pins on the ESP32 */
#define PinCs     5
#define PinIrq    26
#define PinRst    14
#define PinBusy   27
#define PinSck    18
#define PinMosi   23
#define PinMiso   19

/* uplink settings. More repeats and a shorter gap than before, so a command
 * keeps trying for a few seconds and lands the moment a window opens. */
#define UplinkMax      48
#define UplinkRepeats  10
#define UplinkGapMs    25UL      /* wait this long after a packet, then send   */
#define UplinkBlindMs  700UL     /* if no packets, just retry on this timer     */
#define SerialIdleMs   150UL     /* treat typed text as done after this pause   */

/* command numbers, same as the flight computer uses */
#define CmdNone       0
#define CmdTxOn       1
#define CmdTxOff      2
#define CmdCalibrate  3

/* The RF switch table uses pins inside the LR1121 chip, not pins on the
 * ESP32. Copied from vritra_gs.ino. Do not change these. */
static const uint32_t swPins[] = {
    RADIOLIB_LR11X0_DIO5, RADIOLIB_LR11X0_DIO6,
    RADIOLIB_NC, RADIOLIB_NC, RADIOLIB_NC
};
static const Module::RfSwitchMode_t swTable[] = {
    { LR11x0::MODE_STBY,  { LOW,  LOW  } },
    { LR11x0::MODE_RX,    { HIGH, LOW  } },
    { LR11x0::MODE_TX,    { HIGH, HIGH } },
    { LR11x0::MODE_TX_HP, { LOW,  HIGH } },
    { LR11x0::MODE_TX_HF, { LOW,  LOW  } },
    { LR11x0::MODE_GNSS,  { LOW,  LOW  } },
    { LR11x0::MODE_WIFI,  { LOW,  LOW  } },
    END_OF_MODE_TABLE,
};

static LR1121 radio = new Module(PinCs, PinIrq, PinRst, PinBusy, SPI);

/* Only one interrupt is ever attached. A second one for transmit done was
 * tried on this board and crashed the ESP32 straight after the first send,
 * so transmit is finished by watching the pin instead. */
static volatile bool packetFlag = false;
static void IRAM_ATTR onPacket() { packetFlag = true; }

static uint32_t gotCount  = 0;
static uint32_t lostCount = 0;
static uint32_t oddCount  = 0;
static uint32_t lastPkt   = 0;
static bool     havePkt   = false;

static float    bestRssi  = -999.0f;
static float    worstRssi =  999.0f;

static uint8_t  waitingCmd  = CmdNone;
static uint8_t  seqNumber   = 0;
static uint8_t  repeatsLeft = 0;
static uint32_t nextSendMs  = 0;

static String   inputLine;
static uint32_t lastKeyMs   = 0;

/* turn a radio error number into words */
static const char* errorText(int16_t code) {
    switch (code) {
    case RADIOLIB_ERR_NONE:                     return "no error";
    case RADIOLIB_ERR_CHIP_NOT_FOUND:           return "chip not found, check the wiring";
    case RADIOLIB_ERR_SPI_CMD_TIMEOUT:          return "the chip never released busy";
    case RADIOLIB_ERR_SPI_CMD_INVALID:          return "the chip rejected that command";
    case RADIOLIB_ERR_SPI_CMD_FAILED:           return "the chip did not report success";
    case RADIOLIB_ERR_INVALID_FREQUENCY:        return "bad frequency";
    case RADIOLIB_ERR_INVALID_OUTPUT_POWER:     return "bad output power";
    case RADIOLIB_ERR_INVALID_BANDWIDTH:        return "bad bandwidth";
    case RADIOLIB_ERR_INVALID_SPREADING_FACTOR: return "bad spreading factor";
    default:                                    return "see the RadioLib error list";
    }
}

static const char* commandName(uint8_t id) {
    switch (id) {
    case CmdTxOn:      return "start telemetry";
    case CmdTxOff:     return "stop telemetry";
    case CmdCalibrate: return "calibrate";
    default:           return "unknown";
    }
}

/* base 36 back to a number, understands a leading minus */
static long fromBase36(const char* s) {
    bool neg = false;
    if (*s == '-') { neg = true; s++; }
    long value = 0;
    while (*s) {
        char c = *s;
        int  d;
        if      (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'A' && c <= 'Z') d = c - 'A' + 10;
        else if (c >= 'a' && c <= 'z') d = c - 'a' + 10;
        else break;
        value = value * 36 + d;
        s++;
    }
    return neg ? -value : value;
}

static void showHelp() {
    Serial.println();
    Serial.println("Type one of these, then Enter:");
    Serial.println("  TX ON        tell the CanSat to start sending telemetry");
    Serial.println("  TX OFF       tell the CanSat to stop sending");
    Serial.println("  CALIBRATE    zero the barometer at the current spot");
    Serial.println("  STATUS       show the totals so far");
    Serial.println("  HELP         show this list");
    Serial.println();
    Serial.println("If a command has no effect, the CanSat may be out of range.");
    Serial.println("Watch for the line that says the CanSat confirmed it.");
    Serial.println();
}

static void showStatus() {
    uint32_t total = gotCount + lostCount;
    Serial.println();
    Serial.print("Packets received ");
    Serial.println(gotCount);
    Serial.print("Packets lost ");
    Serial.println(lostCount);
    Serial.print("Packets expected ");
    Serial.println(total);
    Serial.print("Loss ");
    if (total > 0) Serial.print((100.0f * (float)lostCount) / (float)total, 1);
    else           Serial.print(0.0f, 1);
    Serial.println(" percent");
    if (gotCount > 0) {
        Serial.print("Best RSSI ");
        Serial.print(bestRssi, 1);
        Serial.print(" dBm, worst ");
        Serial.print(worstRssi, 1);
        Serial.println(" dBm");
    }
    Serial.print("Packets from other teams ignored ");
    Serial.println(oddCount);
    if (waitingCmd != CmdNone) {
        Serial.print("Still trying to send ");
        Serial.print(commandName(waitingCmd));
        Serial.print(", ");
        Serial.print(repeatsLeft);
        Serial.println(" tries left");
    }
    Serial.println();
}

static bool startRadio() {
    SPI.begin(PinSck, PinMiso, PinMosi, PinCs);

    radio.setRfSwitchTable(swPins, swTable);
    int16_t st = radio.begin(FreqMhz, BwKhz, Sf, Cr, SyncWord,
                             UplinkPower, PreambleLen, TcxoVolts);
    if (st != RADIOLIB_ERR_NONE) {
        Serial.print("The radio did not start. ");
        Serial.println(errorText(st));
        return false;
    }
    radio.setRfSwitchTable(swPins, swTable);   /* begin() wipes it */
    radio.setCRC(2);
    radio.setRegulatorDCDC();
    radio.setSyncWord(SyncWord);
    radio.setOutputPower(UplinkPower, UseHpPa);
    if (radio.calibrateImageRejection(863.0f, 870.0f) != RADIOLIB_ERR_NONE)
        Serial.println("Image calibration did not finish cleanly");
    if (radio.setRxBoostedGainMode(true) != RADIOLIB_ERR_NONE)
        Serial.println("Could not turn on boosted receive gain");

    Serial.print("Radio ready on ");
    Serial.print(FreqMhz, 2);
    Serial.print(" MHz, SF");
    Serial.print(Sf);
    Serial.print(", ");
    Serial.print(BwKhz, 0);
    Serial.print(" kHz, CR 4 of ");
    Serial.println(Cr);

    radio.setPacketReceivedAction(onPacket);
    return true;
}

static void queueCommand(uint8_t id, const char* name) {
    waitingCmd  = id;
    seqNumber++;
    repeatsLeft = UplinkRepeats;
    nextSendMs  = millis();
    Serial.print("Sending ");
    Serial.print(name);
    Serial.println(" to the CanSat, will keep trying until it confirms");
}

static void sendUplink() {
    if (repeatsLeft == 0 || waitingCmd == CmdNone) return;

    char out[UplinkMax];
    int n = snprintf(out, sizeof(out), "%s,C,%u,0,%u\n",
                     TeamId, waitingCmd, seqNumber);
    if (n <= 0 || n >= (int)sizeof(out)) {
        repeatsLeft = 0;
        waitingCmd  = CmdNone;
        return;
    }

    /* do not call standby() first, it makes every send after the first fail */
    int16_t st = radio.startTransmit((uint8_t*)out, (size_t)n);
    if (st != RADIOLIB_ERR_NONE) {
        Serial.print("Sending that command failed. ");
        Serial.println(errorText(st));
        radio.standby();
        delay(2);
        radio.clearIrqFlags(0xFFFFFFFFUL);
        radio.startReceive();
    } else {
        uint32_t t0 = millis();
        while (digitalRead(PinIrq) == LOW) {
            delay(1);           /* the delay keeps the watchdog happy */
            if (millis() - t0 > 2000) { st = RADIOLIB_ERR_TX_TIMEOUT; break; }
        }
        if (st == RADIOLIB_ERR_NONE) radio.finishTransmit();
        else Serial.println("The command did not finish sending in time");
    }
    packetFlag = false;   /* the pin also pulses when a send finishes */

    /* the amplifier pulls a lot of current, give the supply a moment */
    delay(30);
    if (radio.startReceive() != RADIOLIB_ERR_NONE) {
        delay(20);
        radio.standby();
        if (radio.startReceive() != RADIOLIB_ERR_NONE) {
            startRadio();
            radio.startReceive();
        }
    }

    repeatsLeft--;
    nextSendMs = millis() + UplinkBlindMs;
    if (repeatsLeft == 0) {
        Serial.print("No confirmation yet for ");
        Serial.print(commandName(waitingCmd));
        Serial.println(". Send it again if the CanSat still has not reacted.");
        waitingCmd = CmdNone;
    }
}

static void readPacket() {
    char buf[160];
    int  len = radio.getPacketLength();
    if (len <= 0 || len >= (int)sizeof(buf)) { radio.startReceive(); return; }
    if (radio.readData((uint8_t*)buf, (size_t)len) != RADIOLIB_ERR_NONE) {
        radio.startReceive();
        return;
    }
    buf[len] = '\0';

    int   bytes = len;
    float rssi  = radio.getRSSI();
    float snr   = radio.getSNR();

    while (len > 0 && (buf[len-1] == '\r' || buf[len-1] == '\n')) buf[--len] = '\0';

    /* ignore anything that is not from our team */
    int idLen = strlen(TeamId);
    if (len <= idLen || strncmp(buf, TeamId, idLen) != 0) {
        oddCount++;
        Serial.println();
        Serial.println("Heard a packet that was not ours");
        Serial.print("  RSSI     "); Serial.print(rssi, 1); Serial.println(" dBm");
        Serial.print("  SNR      "); Serial.print(snr, 1);  Serial.println(" dB");
        radio.startReceive();
        return;
    }

    /* split on commas so we can read a few fields out of it */
    char work[160];
    strncpy(work, buf, sizeof(work) - 1);
    work[sizeof(work) - 1] = '\0';
    char* field[28];
    int   count = 0;
    field[count++] = work;
    for (char* p = work; *p && count < 28; p++)
        if (*p == ',') { *p = '\0'; field[count++] = p + 1; }

    /* field layout, matching the transmitter:
     * 0 team  1 time  2 packet  3 alt  4 pres  5 temp  6 volt  7 gnss time
     * 8 lat  9 lon  10 gps alt  11 sats  12 ax  13 ay  14 az  15 spin
     * 16 state  17 flags  18 F0 tag  19 power  20 last command */
    long number = (count > 2)  ? fromBase36(field[2])  : 0;
    long altT   = (count > 3)  ? fromBase36(field[3])  : 0;   /* tenths of a metre */
    long voltT  = (count > 6)  ? fromBase36(field[6])  : 0;   /* hundredths of a volt */
    long sats   = (count > 11) ? fromBase36(field[11]) : 0;
    long power  = (count > 19) ? fromBase36(field[19]) : 0;
    long echoed = (count > 20) ? fromBase36(field[20]) : 0;   /* last command obeyed */

    /* count anything that went missing since the last packet */
    uint32_t missed = 0;
    if (havePkt && (uint32_t)number > lastPkt + 1) missed = (uint32_t)number - lastPkt - 1;
    lostCount += missed;
    lastPkt = (uint32_t)number;
    havePkt = true;
    gotCount++;
    if (rssi > bestRssi)  bestRssi  = rssi;
    if (rssi < worstRssi) worstRssi = rssi;

    Serial.println();
    Serial.print("Packet ");
    Serial.println(number);
    Serial.print("  RSSI     "); Serial.print(rssi, 1); Serial.println(" dBm");
    Serial.print("  SNR      "); Serial.print(snr, 1);  Serial.println(" dB");
    Serial.print("  Bytes    "); Serial.println(bytes);
    Serial.print("  Height   "); Serial.print(altT / 10.0f, 1); Serial.println(" m");
    Serial.print("  Bus      "); Serial.print(voltT / 100.0f, 2); Serial.println(" V");
    Serial.print("  Sats     "); Serial.println(sats);
    Serial.print("  Power    "); Serial.print(power); Serial.println(" dBm");
    Serial.print("  Received "); Serial.println(gotCount);
    Serial.print("  Lost     "); Serial.println(lostCount);
    if (missed > 0) {
        Serial.print("  Missed "); Serial.print(missed);
        Serial.println(" just before this one");
    }

    /* Did the CanSat confirm the command we are trying to send? Its telemetry
     * carries the number of the last command it obeyed. */
    if (waitingCmd != CmdNone && echoed == (long)waitingCmd) {
        Serial.print("  The CanSat confirmed ");
        Serial.println(commandName(waitingCmd));
        repeatsLeft = 0;
        waitingCmd  = CmdNone;
    }

    /* If we still have a command to deliver, fire it now, right after hearing a
     * packet, so it lands in the window the CanSat opens straight after it
     * sends. This is the reliable moment to transmit. */
    if (repeatsLeft > 0) {
        delay(UplinkGapMs);
        sendUplink();
    } else {
        radio.startReceive();
    }
}

/* Act on a finished line. Called on Enter and also on a typing pause, so the
 * command works whatever the serial monitor line ending is set to. */
static void handleLine() {
    if (inputLine.length() == 0) return;
    String cmd = inputLine;
    inputLine = "";
    cmd.trim();
    cmd.toUpperCase();

    /* the ground control app sends the longer names, so accept both */
    if (cmd == "TX ON" || cmd == "CMD_TX_ON" || cmd == "ON") {
        queueCommand(CmdTxOn, "start telemetry");
    } else if (cmd == "TX OFF" || cmd == "CMD_TX_OFF" || cmd == "OFF") {
        queueCommand(CmdTxOff, "stop telemetry");
    } else if (cmd == "CALIBRATE" || cmd == "CMD_CALIBRATE" || cmd == "CAL") {
        queueCommand(CmdCalibrate, "calibrate");
    } else if (cmd == "STATUS") {
        showStatus();
    } else if (cmd == "HELP" || cmd == "?") {
        showHelp();
    } else {
        Serial.println("I did not understand that. Type HELP for the list.");
    }
}

static void readSerial() {
    while (Serial.available()) {
        char c = (char)Serial.read();
        lastKeyMs = millis();
        if (c == '\r' || c == '\n') { handleLine(); continue; }
        if (inputLine.length() < 64) inputLine += c;
    }
    /* No newline came, but typing has paused, so treat what we have as a line.
     * This is what makes the monitor's "No line ending" setting work. */
    if (inputLine.length() > 0 && (millis() - lastKeyMs) > SerialIdleMs)
        handleLine();
}

void setup() {
    /* the amplifier can dip the supply enough to trip the brownout reset,
     * so turn that off here. A bigger capacitor is the real fix. */
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

    Serial.begin(115200);
    Serial.println();
    Serial.println("Range test receiver, team 2026 INSPACe CAN 7USAT 025");
    Serial.println("Listens for the CanSat and sends it commands.");

    while (!startRadio()) {
        Serial.println("Trying again in two seconds");
        delay(2000);
    }

    /* let the supply settle before the amplifier is ever used */
    delay(250);

    if (radio.startReceive() == RADIOLIB_ERR_NONE)
        Serial.println("Listening now");
    else
        Serial.println("Could not start listening");

    showHelp();
}

void loop() {
    readSerial();

    if (packetFlag) {
        packetFlag = false;
        readPacket();
    }

    /* blind retry, for when the CanSat is silent and there are no packets to
     * ride in on, which is exactly the case when you send the start command */
    if (repeatsLeft > 0 && (int32_t)(millis() - nextSendMs) >= 0) sendUplink();
}
