/* =============================================================================
 * Radio.cpp — LR1121 (Waveshare Core1121-XF) on Teensy 4.1 SPI1
 * =============================================================================
 * LOAD-BEARING FACTS. Every one of these cost real debugging time; none of
 * them should be "cleaned up" without a working reference to compare against.
 *
 *  1. setOutputPower(N, true). The second argument selects the PA path and it
 *     MUST be true. The Core1121-XF wires the HP path only (R3 = 0R, R4 = NC)
 *     and its antenna matching is built for HP. Omitting it routes through LP
 *     and costs ~30 dB — the difference between 2 km and 30 m. Diagnosed by an
 *     A/B test alternating HP and LP on successive packets and comparing
 *     received RSSI, not by inference.
 *
 *  2. setRfSwitchTable() is called BOTH before and after begin(). begin()
 *     clears the table. The version that called it only before failed
 *     reliably with -707.
 *
 *  2b. -707 IS NOT A DIAGNOSIS. RADIOLIB_ERR_SPI_CMD_FAILED means "the status
 *     byte the chip returned was not a success code" — it says nothing about
 *     why. On this hardware it has been produced by at least two unrelated
 *     faults, and they were debugged months apart:
 *       (a) the RF switch table cleared by begin(), as in 2 above; and
 *       (b) MISO listening on the wrong pin, as in 5 below — the Teensy read a
 *           floating line as 0xFF, and 0xFF decodes to a command-status field
 *           that is not "OK", so RadioLib correctly reported a failed command.
 *     Cause (b) is the nastier one because nothing is broken: the solder joint
 *     is good, the chip is alive and answering, and the MCU is simply not
 *     wired to the pad it is listening on. Chasing (a) when you have (b) can
 *     burn a day. diagnoseSpi() below exists to tell them apart in seconds.
 *
 *  3. TCXO voltage is 1.6 V. This was once "derived" from the schematic as
 *     0.0f (a crystal symbol was mistaken for no TCXO) and that was wrong.
 *     1.6f is the value proven on this hardware.
 *
 *  4. SW_PINS holds RADIOLIB_LR11X0_DIO5 / DIO6 — those are the LR1121's OWN
 *     internal DIOs driving the PE4259 antenna switch, configured over SPI.
 *     They are not MCU GPIO numbers and nothing is wired to them on the Teensy.
 *
 *  5. SPI1 MISO is remapped to pin 39, and stays there. The original reason
 *     was that SPI1's default MISO is pin 1, which was Serial1 TX — the GNSS
 *     line — and leaving it there corrupted every SPI read as CHIP_NOT_FOUND
 *     (-2). At harness rev 2026-08 the GNSS moved to Serial2 (pins 7/8), so
 *     pin 1 is now free and that collision is gone. The remap is retained
 *     because the harness is physically wired to 39. The obsolete rationale is
 *     not a licence to change the pin.
 *
 *  5b. Rev 2026-08 also moved CS 24->29, BUSY 31->24, RESET 30->25 and
 *     DIO9 32->30. CS and BUSY effectively swapped, and DIO9 took the old
 *     RESET pin, so a partially rewired harness drives two outputs together.
 *     If begin() returns -2, check the harness before the code.
 *
 *  6. The antenna belongs on ANT2 (LORA_ANT), the 867 MHz u.FL port. ANT1 is
 *     the 2.4 GHz port and sits right next to it on the board.
 *
 * FRAME INTEGRITY
 *   LoRa's own CRC is enabled (setCRC(2)), and every packet additionally
 *   carries a CRC-16/CCITT over its payload. In explicit-header mode a
 *   receiver at low SNR can misdecode the header itself and still report
 *   "CRC pass" on garbage; the application CRC closes that hole without the
 *   fragility of implicit header, where a single length mismatch silently
 *   kills the whole link with no diagnostic.
 * ========================================================================== */
#include "Radio.h"
#include <RadioLib.h>
#include <SPI.h>

RadioLink Radio;

static const uint32_t SW_PINS[] = {
    RADIOLIB_LR11X0_DIO5, RADIOLIB_LR11X0_DIO6,
    RADIOLIB_NC, RADIOLIB_NC, RADIOLIB_NC
};
static const Module::RfSwitchMode_t SW_TABLE[] = {
    { LR11x0::MODE_STBY,  { LOW,  LOW  } },
    { LR11x0::MODE_RX,    { HIGH, LOW  } },
    { LR11x0::MODE_TX,    { HIGH, HIGH } },
    { LR11x0::MODE_TX_HP, { LOW,  HIGH } },
    { LR11x0::MODE_TX_HF, { LOW,  LOW  } },
    { LR11x0::MODE_GNSS,  { LOW,  LOW  } },
    { LR11x0::MODE_WIFI,  { LOW,  LOW  } },
    END_OF_MODE_TABLE,
};

static LR1121 lr = new Module(PIN_LORA_CS, PIN_LORA_IRQ,
                              PIN_LORA_RST, PIN_LORA_BUSY, SPI1);

/* DIO9 fires for both "packet sent" and "packet received"; the state machine
 * decides which one it was. The ISR does nothing but raise a flag. */
static volatile bool irqFlag = false;
static void onDio9() { irqFlag = true; }

/* CRC-16/CCITT-FALSE: poly 0x1021, init 0xFFFF, no reflection, no final xor. */
uint16_t RadioLink::crc16(const uint8_t* p, uint8_t n) {
    uint16_t c = 0xFFFF;
    for (uint8_t i = 0; i < n; i++) {
        c ^= (uint16_t)p[i] << 8;
        for (uint8_t b = 0; b < 8; b++)
            c = (c & 0x8000) ? (uint16_t)((c << 1) ^ 0x1021) : (uint16_t)(c << 1);
    }
    return c;
}

/* RadioLib returns bare integers. Naming them at the point of failure is the
 * difference between "code=-707" and knowing which half of the problem space
 * to search. Codes are from RadioLib TypeDef.h. */
static const char* rfErrName(int16_t c) {
    switch (c) {
    case RADIOLIB_ERR_NONE:                 return "NONE";
    case RADIOLIB_ERR_CHIP_NOT_FOUND:       return "CHIP_NOT_FOUND (no plausible reply on MISO)";
    case RADIOLIB_ERR_SPI_CMD_TIMEOUT:      return "SPI_CMD_TIMEOUT (BUSY never released)";
    case RADIOLIB_ERR_SPI_CMD_INVALID:      return "SPI_CMD_INVALID (chip rejected the opcode)";
    case RADIOLIB_ERR_SPI_CMD_FAILED:       return "SPI_CMD_FAILED (status byte was not OK)";
    case RADIOLIB_ERR_INVALID_FREQUENCY:    return "INVALID_FREQUENCY";
    case RADIOLIB_ERR_INVALID_OUTPUT_POWER: return "INVALID_OUTPUT_POWER";
    case RADIOLIB_ERR_INVALID_BANDWIDTH:    return "INVALID_BANDWIDTH";
    case RADIOLIB_ERR_INVALID_SPREADING_FACTOR: return "INVALID_SPREADING_FACTOR";
    default:                                return "see RadioLib TypeDef.h";
    }
}

/* ---------------------------------------------------------------------------
 * diagnoseSpi() — runs ONLY after begin() has already failed.
 *
 * WHAT IT IS FOR. The -707 that cost this project a day was not a broken
 * radio: MISO was soldered to pin 39, but Teensy's SPI1 defaults its receive
 * channel to pin 1, so the MCU was listening to an unconnected pad. A floating
 * CMOS input idles high, so every read returned 0xFF, and 0xFF is not a valid
 * LR1121 status byte — hence a "command failed" that had nothing to do with
 * any command. SPI1.setMISO(PIN_SPI1_MISO) in hwInit() rewires the internal
 * multiplexer to the physical pad and is what fixed it.
 *
 * That fix is now permanent, so this routine is not there to repeat it. It is
 * there because the SAME SIGNATURE will reappear the moment anyone changes a
 * pin, reflows the board, or cracks that joint under 30 g — and next time the
 * person debugging it will not be whoever remembers 2026. The classification
 * below turns a day of guessing into one line of serial output:
 *
 *   all 0xFF  ->  MISO is not connected to the pin the MCU is listening on.
 *                 Either the setMISO() call was lost, or the joint is open.
 *   all 0x00  ->  MISO shorted low, or the module has no power.
 *   mixed     ->  the chip IS talking. Wiring is fine; look at the RF switch
 *                 table, the TCXO voltage, or the frequency/power arguments.
 *
 * The 2 ms reset pulse below is a busy-wait, which the project otherwise
 * forbids. It is allowed here on the explicit grounds that this function only
 * ever runs on a boot path that has already failed and is about to return
 * false — there is no cadence left to protect. */
static void diagnoseSpi() {
    pinMode(PIN_LORA_CS, OUTPUT);   digitalWrite(PIN_LORA_CS, HIGH);
    pinMode(PIN_LORA_RST, OUTPUT);
    pinMode(PIN_LORA_BUSY, INPUT);

    digitalWrite(PIN_LORA_RST, LOW);
    delayMicroseconds(2000);
    digitalWrite(PIN_LORA_RST, HIGH);

    uint32_t t0 = millis();
    while (digitalRead(PIN_LORA_BUSY) == HIGH && millis() - t0 < 50) { }
    const bool busyStuck = (digitalRead(PIN_LORA_BUSY) == HIGH);

    /* LR1121 GetVersion, opcode 0x0101: send the opcode, wait for BUSY to
     * fall, then clock out stat1 + four version bytes. */
    uint8_t rx[5] = { 0 };
    SPI1.beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));
    digitalWrite(PIN_LORA_CS, LOW);
    SPI1.transfer(0x01);
    SPI1.transfer(0x01);
    digitalWrite(PIN_LORA_CS, HIGH);
    t0 = millis();
    while (digitalRead(PIN_LORA_BUSY) == HIGH && millis() - t0 < 50) { }
    digitalWrite(PIN_LORA_CS, LOW);
    for (uint8_t i = 0; i < 5; i++) rx[i] = SPI1.transfer(0x00);
    digitalWrite(PIN_LORA_CS, HIGH);
    SPI1.endTransaction();

    bool allHigh = true, allLow = true;
    for (uint8_t i = 0; i < 5; i++) {
        if (rx[i] != 0xFF) allHigh = false;
        if (rx[i] != 0x00) allLow  = false;
    }

    Serial.printf("// [RF] diag: BUSY=%s  raw=%02X %02X %02X %02X %02X\n",
                  busyStuck ? "STUCK HIGH" : "ok",
                  rx[0], rx[1], rx[2], rx[3], rx[4]);
    Serial.printf("// [RF] diag: SPI1 MISO=%d MOSI=%d SCK=%d  CS=%d RST=%d BUSY=%d IRQ=%d\n",
                  PIN_SPI1_MISO, PIN_SPI1_MOSI, PIN_SPI1_SCK,
                  PIN_LORA_CS, PIN_LORA_RST, PIN_LORA_BUSY, PIN_LORA_IRQ);

    if (allHigh)
        Serial.println(F("// [RF] >> MISO READS ALL 1s — the MCU is listening to a floating pin.\n"
                         "// [RF]    This is the classic -707. SPI1's default MISO is pin 1;\n"
                         "// [RF]    the harness wires 39. Confirm SPI1.setMISO(39) still runs\n"
                         "// [RF]    BEFORE SPI1.begin(), then check the pin 39 joint."));
    else if (allLow)
        Serial.println(F("// [RF] >> MISO READS ALL 0s — line shorted low, or module unpowered.\n"
                         "// [RF]    Check 3V3 at the module and the MISO trace for a short."));
    else if (busyStuck)
        Serial.println(F("// [RF] >> BUSY never released. Chip is not booting: check RESET\n"
                         "// [RF]    polarity and that 3V3 is stable at the module."));
    else
        Serial.println(F("// [RF] >> The chip IS replying, so the SPI wiring is good.\n"
                         "// [RF]    Look at the RF switch table, TCXO_V (must be 1.6), or the\n"
                         "// [RF]    frequency/bandwidth/power arguments instead."));
}

bool RadioLink::hwInit() {
    /* THIS BLOCK IS LOAD-BEARING — see header fact 5 and diagnoseSpi() above.
     * setMISO() rewires Teensy's internal SPI1 receive multiplexer onto the
     * pad the harness actually uses. Without it the MCU listens to pin 1, reads
     * a floating 0xFF, and begin() fails with -707 while every solder joint on
     * the board is perfect. All three must be called BEFORE SPI1.begin();
     * afterwards the peripheral is already latched to the old pins. */
    SPI1.setMISO(PIN_SPI1_MISO);
    SPI1.setMOSI(PIN_SPI1_MOSI);
    SPI1.setSCK(PIN_SPI1_SCK);
    SPI1.begin();

    lr.setRfSwitchTable(SW_PINS, SW_TABLE);
    int16_t st = lr.begin(backup_ ? FREQ_BACKUP_MHZ : FREQ_MAIN_MHZ,
                          LORA_BW_KHZ, LORA_SF, LORA_CR,
                          LORA_SYNC, txPower_, LORA_PREAMBLE, TCXO_V);
    if (st != RADIOLIB_ERR_NONE) {
        Serial.printf("// [RF] begin FAILED code=%d — %s\n", st, rfErrName(st));
        diagnoseSpi();
        return false;
    }
    lr.setRfSwitchTable(SW_PINS, SW_TABLE);       /* begin() cleared it       */
    lr.setCRC(2);
    lr.setRegulatorDCDC();
    lr.setSyncWord(LORA_SYNC);
    /* The chip now genuinely holds txPower_, so the deferred-write flag can be
     * cleared. Any later change re-sets it and applyTxPower() carries it. */
    if (lr.setOutputPower(txPower_, TX_USE_HP_PA) == RADIOLIB_ERR_NONE)
        powerDirty_ = false;

    /* ---- IMAGE REJECTION CALIBRATION AT OUR ACTUAL BAND ------------------
     * THIS IS NOT OPTIONAL AND ITS ABSENCE IS SILENT.
     *
     * LR1121 User Manual 2.1.3: the POR sequence calibrates the RC
     * oscillators, PLL, ADC and mixer image rejection "at 915MHz", and the
     * default image calibration band is 902-928 MHz. It then states plainly:
     *   "Image calibration is necessary if there is a frequency change
     *    > 10MHz, or a temperature change > 10 C."
     *
     * We transmit at 867.00 MHz. That is 35-61 MHz below the calibrated band —
     * between three and six times the stated threshold. Until this call was
     * added, this radio had NEVER held a valid image calibration at its
     * operating frequency, on either board.
     *
     * It gets worse with a TCXO fitted, which is our case. The datasheet
     * (3.1, TCXO notes) says that when a TCXO replaces the 32 MHz crystal
     * "all start-up (POR) calibrations are skipped. The host processor should
     * program the TCXO configuration and re-launch the calibrations before
     * further usage of the chip." RadioLib's begin() programs the TCXO but
     * does not re-launch the calibrations for us.
     *
     * SYMPTOM THIS EXPLAINS. Degraded image rejection does not break the link,
     * it weakens it — the receiver is quietly several dB less sensitive than
     * the budget assumes, so acquisition is slow and marginal and the link
     * "takes minutes to come up" rather than failing outright. That is exactly
     * the behaviour observed on the bench.
     *
     * Range covers both assigned channels (867.00 main, 865.75 backup) with
     * margin. The chip quantises to 4 MHz steps internally; RadioLib handles
     * the conversion. Must run AFTER begin() has applied the TCXO setting. */
    int16_t cal = lr.calibrateImageRejection(863.0f, 870.0f);
    if (cal != RADIOLIB_ERR_NONE)
        Serial.printf("// [RF] WARNING image calibration failed code=%d — %s\n",
                      cal, rfErrName(cal));

    /* ---- BOOSTED RX GAIN -------------------------------------------------
     * Datasheet Table 3-8 quotes every sub-GHz LoRa sensitivity figure with
     * "RxBoosted = 1": BW125/SF7 = -127 dBm, BW125/SF12 = -141 dBm. Our
     * SF10 figure of -132 dBm in Config.h sits inside that interpolation
     * (~-135 dBm) only if boosted gain is actually enabled. It is not the
     * default, so without this call the link budget is quoting a sensitivity
     * the chip was never configured to deliver.
     * The cost is a small increase in RX current, which is irrelevant here:
     * the vehicle receives continuously at 9.2 mA and flies on 21700 cells. */
    int16_t rxb = lr.setRxBoostedGainMode(true);
    if (rxb != RADIOLIB_ERR_NONE)
        Serial.printf("// [RF] WARNING RxBoosted enable failed code=%d\n", rxb);

    LR11x0VersionInfo_t vi;
    if (lr.getVersionInfo(&vi) == RADIOLIB_ERR_NONE)
        Serial.printf("// [RF] HW=0x%02X DEV=0x%02X FW=%d.%d  %.2f MHz SF%d BW%.0fk "
                      "sync 0x%02X %+d dBm HP\n",
                      vi.hardware, vi.device, vi.fwMajor, vi.fwMinor,
                      backup_ ? FREQ_BACKUP_MHZ : FREQ_MAIN_MHZ,
                      LORA_SF, LORA_BW_KHZ, LORA_SYNC, txPower_);

    /* One handler for both edges. RadioLib's setPacketReceivedAction() and
     * setPacketSentAction() both resolve to setIrqAction() on LR11x0 — the
     * same pin, the same attachInterrupt. startTransmit()/startReceive()
     * decide which IRQ source is mapped to it, so a single flag plus the
     * state machine is enough to tell the two apart. */
    lr.setIrqAction(onDio9);
    irqFlag = false;
    return true;
}

void RadioLink::begin() {
    state_ = RS_INIT;
    stateAt_ = millis();
    nextTxMs_ = millis();
}

void RadioLink::setTxEnabled(bool en) {
    if (txEnabled_ == en) return;
    txEnabled_ = en;
    /* Before the chip is up there is nothing to command; RS_INIT will read
     * txEnabled_ and enter the right mode on its own. */
    if (state_ == RS_INIT || state_ == RS_FAULT) return;
    if (!en) {
        /* Go deaf-proof immediately: arm continuous receive on the spot rather
         * than waiting for the current slot to expire. */
        enterRxContinuous();
    } else {
        lr.standby();
        state_    = RS_IDLE;
        nextTxMs_ = millis();
        stateAt_  = millis();
    }
}

/* Assigned power is +10 dBm on the pad and ~+14 dBm in flight. Never retuned
 * mid-packet; deferred to the next idle moment instead.
 *
 * THE BUG THIS FIXES — IT COST 4 dB OF EVERY FLIGHT.
 *
 * The previous version was:
 *     if (dbm == txPower_) return;            // early-out
 *     txPower_ = dbm;                         // remember the new value
 *     if (state_ == RS_IDLE || RS_RX_CONT)    // only write the chip if idle
 *         lr.setOutputPower(...);
 *
 * Those three lines together lose the write permanently. loop() calls this
 * every pass, so on the FS_BOOT -> FS_LAUNCH transition the very next call asks
 * for +14 dBm. At that moment the radio is almost certainly in RS_TXING: a SF10
 * downlink occupies ~944 ms of a 1000 ms cycle, so the odds of catching it idle
 * are roughly one in twenty. txPower_ was therefore updated to 14 while the
 * chip stayed at 10 — and because txPower_ now equalled 14, EVERY subsequent
 * call hit the early-out and returned before it could ever retry.
 *
 * The vehicle flew the entire mission at +10 dBm believing it was at +14, and
 * said so on the console. 4 dB is a factor of 2.5 in received power and about
 * 37% of the link's range — the difference between comfortably making 2 km and
 * not. Nothing anywhere reported it, because the firmware's own view of the
 * setting was the value it had stored, not the value the chip held.
 *
 * The fix separates "what we want" from "what the chip has": powerDirty_ stays
 * set until setOutputPower() has actually been issued, and service() retries at
 * every idle opportunity. It is initialised true so the very first idle pass
 * asserts the pad power explicitly rather than trusting begin()'s argument. */
void RadioLink::setTxPower(int8_t dbm) {
    if (dbm != txPower_) {
        txPower_    = dbm;
        powerDirty_ = true;
    }
    applyTxPower();
}

/* Write the pending power to the chip if it is safe to do so. Called from
 * setTxPower() and from every idle pass of service(), so a change requested
 * mid-transmit is applied within one cycle instead of being dropped. */
void RadioLink::applyTxPower() {
    if (!powerDirty_) return;
    if (state_ != RS_IDLE && state_ != RS_RX_CONT) return;

    int16_t st = lr.setOutputPower(txPower_, TX_USE_HP_PA);
    if (st != RADIOLIB_ERR_NONE) {
        /* Leave powerDirty_ set so the next idle pass retries. A PA that
         * refused the setting must not be recorded as having accepted it. */
        Serial.printf("// [RF] setOutputPower(%+d dBm) FAILED code=%d — %s "
                      "(will retry)\n", txPower_, st, rfErrName(st));
        return;
    }
    powerDirty_ = false;
    Serial.printf("// [RF] TX power -> %+d dBm (confirmed on chip)\n", txPower_);
}

/* Both channels are assigned to team 025 by the frequency allocation plan
 * (867.00 Main, 865.75 Backup), so switching between them never transmits
 * outside our allocation. Mission Control still owns the decision.
 *
 * SWITCHING TO BACKUP ARMS A DEAD-MAN TIMER — see FAILOVER_PROOF_MS in
 * Config.h. The ground station does NOT follow automatically; the operator has
 * to move the gateway across separately with FAILOVER_LOCAL. If that never
 * happens, the two ends sit on different channels and every command capable of
 * fixing it — CMD_FAILOVER included — has to cross the link that no longer
 * exists. The vehicle is then unrecoverable by any means, with two perfectly
 * healthy radios. Requiring the vehicle to actually HEAR the ground station on
 * the new channel before it commits turns that dead end into a self-healing
 * 120-second detour. */
void RadioLink::useBackupFreq(bool b) {
    if (backup_ == b) return;
    backup_ = b;
    state_  = RS_INIT;
    stateAt_ = millis();
    backupAt_     = millis();
    backupProven_ = false;
    Serial.printf("// [RF] frequency -> %.2f MHz%s\n",
                  b ? FREQ_BACKUP_MHZ : FREQ_MAIN_MHZ,
                  b ? "  (reverts to main in 120 s unless an uplink is heard there)"
                    : "");
}

void RadioLink::queueDownlink(const char* pkt, uint16_t len) {
    if (!len || len > DL_PACKET_MAX) return;
    memcpy(pkt_, pkt, len);
    pktLen_  = len;
    havePkt_ = true;
}

bool RadioLink::takeCommand(uint8_t& id, int16_t& arg) {
    if (!cmdPending_) return false;
    noInterrupts();
    id  = cmdId_;
    arg = cmdArg_;
    cmdPending_ = false;
    interrupts();
    return true;
}

void RadioLink::enterRxContinuous() {
    irqFlag = false;
    int16_t st = lr.startReceive();
    if (st != RADIOLIB_ERR_NONE) { state_ = RS_FAULT; stateAt_ = millis(); return; }
    state_   = RS_RX_CONT;
    stateAt_ = millis();
    /* Confirm the radio is listening — visible on the Teensy serial monitor.
     * If this line never appears, hwInit() is failing (check code -707). */
    if (!rxContLogged_) { rxContLogged_ = true; Serial.println(F("// [RF] continuous RX armed — waiting for CMD_TX_ON")); }
}

/* Read whatever DIO9 just announced and, if it validates, drop it in the
 * command mailbox. Never blocks.
 *
 * Uplink is ASCII, matching the downlink's format family:
 *     <TEAM_ID>,C,<cmdId>,<arg>,<seq>
 * The team id is checked first, which is what guideline §6.ii's NETID/PANID
 * requirement is really for — another team's traffic on the same frequency is
 * discarded before it can be parsed as a command. */
void RadioLink::handleRxPacket() {
    char buf[UL_PACKET_MAX + 1];
    int len = lr.getPacketLength();
    if (len <= 0 || len > UL_PACKET_MAX) { lr.startReceive(); return; }

    if (lr.readData((uint8_t*)buf, (size_t)len) != RADIOLIB_ERR_NONE) {
        lr.startReceive();
        return;
    }
    buf[len] = '\0';
    rssi_ = lr.getRSSI();
    snr_  = lr.getSNR();
    /* Show every uplink on the console — even ones the team-ID filter will
     * reject.  This lets you confirm RF packets are arriving before checking
     * whether they parse correctly. */
    Serial.printf("// [UL] %dB RSSI %.1f SNR %.1f: %s\n", len, rssi_, snr_, buf);

    /* Trim the line terminator the gateway appends. */
    while (len > 0 && (buf[len - 1] == '\r' || buf[len - 1] == '\n' ||
                       buf[len - 1] == ' '))
        buf[--len] = '\0';

    /* Short team id, same as the downlink. A 26-character identifier would put
     * the command at 534 ms of airtime — longer than the 400 ms uplink window
     * the SF10 downlink leaves room for, so it could never be received. */
    const size_t idLen = strlen(TEAM_ID_AIR);
    if ((size_t)len > idLen + 4 &&
        strncmp(buf, TEAM_ID_AIR, idLen) == 0 &&
        buf[idLen] == ',' && buf[idLen + 1] == 'C' && buf[idLen + 2] == ',') {

        int id = 0, arg = 0, seq = -1;
        if (sscanf(buf + idLen + 3, "%d,%d,%d", &id, &arg, &seq) == 3 &&
            id > 0 && id < 256) {
            /* PROOF OF LIFE ON THIS CHANNEL. A well-formed command carrying our
             * team id means the ground station is definitely transmitting on
             * the frequency we are listening on, which is exactly what the
             * backup-frequency dead-man timer is waiting to establish. Latched
             * here rather than inside the de-duplication below on purpose: a
             * REPEAT of an already-executed command proves the channel just as
             * well as a new one, and after a failover the repeats are the most
             * likely thing to arrive first. */
            if (backup_ && !backupProven_) {
                backupProven_ = true;
                Serial.println(F("// [RF] backup frequency CONFIRMED — ground "
                                 "station heard here, auto-revert cancelled"));
            }
            /* The gateway repeats a command several times so it survives a
             * closed RX window. Execute it once. */
            if ((uint8_t)seq != lastCmdSeq_) {
                lastCmdSeq_ = (uint8_t)seq;
                cmdId_  = (uint8_t)id;
                cmdArg_ = (int16_t)arg;
                cmdPending_ = true;
                cmdCount_++;
            }
        } else crcRej_++;
    } else crcRej_++;           /* not ours, or malformed */

    lr.startReceive();          /* always re-arm — never fall out of receive */
}

void RadioLink::service() {
    const uint32_t now = millis();

    /* ---- backup-frequency dead-man ------------------------------------- *
     * Ran to time without hearing the ground station on the backup channel, so
     * the failover was never completed at the other end. Undo it. The vehicle
     * is the only party that can, because the operator's remaining commands
     * cannot reach it. A genuine failover is confirmed by the first command
     * that arrives and never gets here. */
    if (backup_ && !backupProven_ && (now - backupAt_) >= FAILOVER_PROOF_MS) {
        Serial.printf("// [RF] no uplink on backup for %lu s — REVERTING to "
                      "%.2f MHz (ground station never followed)\n",
                      (unsigned long)(FAILOVER_PROOF_MS / 1000UL),
                      FREQ_MAIN_MHZ);
        useBackupFreq(false);
        return;                 /* useBackupFreq forced RS_INIT — restart here */
    }

    switch (state_) {

    case RS_INIT:
        if (!hwInit()) {
            state_ = RS_FAULT;
            stateAt_ = now;
            retryAt_ = now + 1000;
            break;
        }
        if (txEnabled_) { lr.standby(); state_ = RS_IDLE; nextTxMs_ = now; }
        else            { enterRxContinuous(); }
        stateAt_ = now;
        break;

    case RS_FAULT:
        /* Retry once a second. Nothing else in the flight computer waits on
         * the radio: sensors keep sampling, the SD card keeps logging, and the
         * downlink resumes the instant the part answers again. */
        if ((int32_t)(now - retryAt_) >= 0) { state_ = RS_INIT; stateAt_ = now; }
        break;

    case RS_IDLE:
        /* Safe point to push a deferred power change through — the PA is not
         * keyed and no packet is in flight. This is what makes the pad->flight
         * step reliable rather than a one-in-twenty coincidence. */
        applyTxPower();
        if (!txEnabled_) { enterRxContinuous(); break; }
        if ((int32_t)(now - nextTxMs_) < 0) break;
        if (!havePkt_) { nextTxMs_ = now + 20; break; }   /* wait for a packet */
        irqFlag = false;
        if (lr.startTransmit(pkt_, pktLen_) != RADIOLIB_ERR_NONE) {
            txFails_++;
            state_ = RS_FAULT; retryAt_ = now + 500; stateAt_ = now;
            break;
        }
        havePkt_ = false;
        state_   = RS_TXING;
        stateAt_ = now;
        nextTxMs_ = now + periodMs_;   /* slot anchored to TX start, not end */
        break;

    case RS_TXING:
        if (irqFlag) {
            irqFlag = false;
            lr.finishTransmit();
            txCount_++;
            lastTxMs_ = now;
            /* Open the uplink window on every rxEveryN_-th cycle. The gateway
             * starts its command 25 ms after it finishes receiving, so the
             * preamble lands inside RX_WINDOW_MS; RX_OVERRUN_MS then lets an
             * in-progress packet finish rather than truncating a command.
             * At SF10 a 400 ms window does not fit alongside a 944 ms downlink
             * on every cycle, which is why this is decimated in flight. */
            if (++rxSkip_ >= rxEveryN_) {
                rxSkip_ = 0;
                state_ = (lr.startReceive() == RADIOLIB_ERR_NONE)
                       ? RS_RX_WINDOW : RS_IDLE;
            } else {
                lr.standby();
                state_ = RS_IDLE;
            }
            stateAt_ = now;
        } else if (now - stateAt_ > TX_TIMEOUT_MS) {
            /* The sent-IRQ never came. Something is wrong at the chip level —
             * a full re-init is the only reliable recovery. */
            txFails_++;
            state_ = RS_FAULT; retryAt_ = now + 200; stateAt_ = now;
        }
        break;

    case RS_RX_WINDOW:
        if (irqFlag) {
            irqFlag = false;
            handleRxPacket();
            lr.standby();
            state_ = RS_IDLE;
            stateAt_ = now;
        } else if (now - stateAt_ > RX_WINDOW_MS + RX_OVERRUN_MS) {
            /* Nothing arrived, and any packet that had started would have
             * finished by now. Give up and go back to the transmit cadence. */
            lr.standby();
            state_ = RS_IDLE;
            stateAt_ = now;
        }
        /* The window deliberately runs its full RX_WINDOW_MS even though the
         * next TX slot is already due — at SF10 a 944 ms downlink leaves no
         * gap, so closing early on the slot boundary would mean never hearing
         * a command at all. This is the cycle that runs long by design; the
         * cadence catches up on the following ones. */
        break;

    case RS_RX_CONT:
        /* TX_OFF. The chip stays in receive permanently — no standby, no
         * sleep, no power-down. */
        applyTxPower();          /* also a safe point: the PA is not keyed */
        if (txEnabled_) { lr.standby(); state_ = RS_IDLE; nextTxMs_ = now; stateAt_ = now; break; }
        if (irqFlag) {
            irqFlag = false;
            handleRxPacket();
            stateAt_ = now;
        } else if (now - stateAt_ > 10000UL) {
            /* Defensive re-arm. If the receiver ever falls out of RX because
             * of an SPI glitch there is no IRQ to tell us, so a silent link
             * would look identical to a quiet one. Ten seconds is long enough
             * that it cannot chop an inbound packet the gateway is repeating. */
            enterRxContinuous();
        }
        break;
    }
}
