/* =============================================================================
 * Radio.h — LR1121 half-duplex transceiver, fully non-blocking
 * =============================================================================
 * STATE MACHINE
 *
 *            +-------------------- txEnabled == false --------------------+
 *            |                                                            v
 *   RS_IDLE --(TX slot due)--> RS_TXING --(sent IRQ)--> RS_RX_WINDOW --> RS_IDLE
 *      ^                          |                          |
 *      |                     (timeout)                  (cmd IRQ)
 *      |                          v                          |
 *      +----------------------- RS_FAULT <-------------------+
 *
 *                            RS_RX_CONT   (txEnabled == false)
 *                            continuous receive, forever, never standby
 *
 * THE TX_OFF CONTRACT — this is the bug the previous firmware had.
 *   "Transmission OFF" must silence the downlink and leave the receiver hot.
 *   The old code called listenForCommand() (a 500 ms window) and then
 *   radio.standby(). Between windows the chip was deaf, so "Transmission ON"
 *   was routinely missed and the CanSat looked bricked.
 *   Here, txEnabled == false drops the radio into RS_RX_CONT: startReceive()
 *   is armed once, re-armed after every packet, and standby()/sleep() are
 *   never called. The chip stays powered and listening 100 % of the time.
 * ========================================================================== */
#pragma once
#include <Arduino.h>
#include "Config.h"

class RadioLink {
public:
    void begin();
    void service();                       /* call every loop — never blocks  */

    bool ready()      const { return state_ != RS_FAULT && state_ != RS_INIT; }
    bool txEnabled()  const { return txEnabled_; }
    void setTxEnabled(bool en);

    /* Hand a fully built ASCII downlink line to the radio. It is transmitted at
     * the next TX slot. A queued packet that has not gone out by the time the
     * next one arrives is overwritten — stale telemetry is worse than none. */
    void queueDownlink(const char* pkt, uint16_t len);

    /* Pop one decoded uplink command. Returns false if none pending. */
    bool takeCommand(uint8_t& id, int16_t& arg);

    void  useBackupFreq(bool b);
    bool  onBackupFreq() const { return backup_; }
    float lastRssi()     const { return rssi_; }
    float lastSnr()      const { return snr_;  }
    uint32_t txCount()   const { return txCount_;  }
    uint32_t txFails()   const { return txFails_;  }
    uint32_t cmdCount()  const { return cmdCount_; }
    uint32_t crcRejects()const { return crcRej_;   }
    uint32_t lastTxMs()  const { return lastTxMs_; }

    /* Slot pacing. The flight computer slows the cadence on the pad so
     * commanding is responsive, and runs 1 Hz once airborne. */
    void setPeriod(uint32_t ms) { periodMs_ = ms; }

    /* How often to open an uplink window, in transmit cycles. 1 = every cycle
     * (used on the pad); RX_EVERY_N_CYCLES in flight, where a 400 ms window on
     * every cycle would not fit alongside a 944 ms downlink. */
    void setRxEveryN(uint8_t n) { rxEveryN_ = n ? n : 1; }

    /* Assigned TX power is staged: +10 dBm on pad, +14 dBm in flight
     * (frequency allocation plan). Safe to call every loop — it only touches
     * the radio when the value actually changes. */
    void setTxPower(int8_t dbm);
    int8_t txPower() const { return txPower_; }

    static uint16_t crc16(const uint8_t* p, uint8_t n);

private:
    enum State : uint8_t {
        RS_INIT = 0, RS_IDLE, RS_TXING, RS_RX_WINDOW, RS_RX_CONT, RS_FAULT
    };

    bool hwInit();
    void enterRxContinuous();
    void handleRxPacket();
    void applyTxPower();          /* writes powerDirty_ through when idle */

    State    state_      = RS_INIT;
    /* Guideline §6.1.v — the vehicle must not transmit until commanded. */
    bool     txEnabled_  = TX_ENABLED_AT_BOOT;
    bool     backup_     = false;
    bool     havePkt_    = false;
    uint8_t  pkt_[DL_PACKET_MAX];
    uint16_t pktLen_     = 0;

    int8_t   txPower_    = TX_POWER_PAD_DBM;   /* pad level until commanded up */
    /* Set when txPower_ has changed but the chip has not been written yet
     * because the radio was mid-transmit. Cleared only once setOutputPower()
     * has actually been issued. See setTxPower() in Radio.cpp — without this
     * the +10 -> +14 dBm flight step was being lost permanently. */
    bool     powerDirty_ = true;
    uint8_t  rxEveryN_   = 1;
    uint8_t  rxSkip_     = 0;
    uint32_t periodMs_   = TX_PERIOD_MS;
    uint32_t nextTxMs_   = 0;
    uint32_t stateAt_    = 0;
    uint32_t lastTxMs_   = 0;
    uint32_t retryAt_    = 0;

    /* Backup-frequency dead-man state. backupProven_ latches on the first valid
     * uplink heard after a switch; until then the vehicle is prepared to undo
     * the switch on its own. See useBackupFreq() and FAILOVER_PROOF_MS. */
    uint32_t backupAt_     = 0;
    bool     backupProven_ = false;

    float    rssi_ = 0, snr_ = 0;
    uint32_t txCount_ = 0, txFails_ = 0, cmdCount_ = 0, crcRej_ = 0;
    bool     rxContLogged_ = false;

    /* single-slot command mailbox */
    volatile bool cmdPending_ = false;
    uint8_t  cmdId_  = 0;
    int16_t  cmdArg_ = 0;
    uint8_t  lastCmdSeq_ = 0xFF;
};

extern RadioLink Radio;
