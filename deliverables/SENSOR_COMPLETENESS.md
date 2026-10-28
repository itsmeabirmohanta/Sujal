# Device configuration completeness and unavailable evidence

Canonical boot values and read sequences are in report §§4–6. This page makes the requested datasheet/application-note/driver/self-test coverage explicit. “Unavailable” never means a compatible-looking command can be substituted. All priorities are A unless marked B/C in the report.

| Part | Controlling docs / driver | Power, self-test and recovery decision |
|---|---|---|
| L89HA | Quectel hardware v1.2 and protocol v1.0; 7Semi library; source register links | Scheduled RESET_N≥100 ms from hardware guide; MAIN-only ground configuration. No public verified AG3335M SDK or firmware-matched EPO/dynamic-model guide obtained. Warm retention and real sky/ACK tests precede optional assistance. |
| BMP585 | Bosch datasheet DS003; BMP5 SensorAPI definitions/source | Reset≥2 ms then NVM/identity/readback; DRDY burst and settling. No fabricated stimulated-test thresholds. Physical reference/ramp/noise test required. |
| ICM-20948/AK09916 | TDK DS-000189, SparkFun register driver | Scheduled reset100 ms D; identity/bank checks; ground stimulated self-test uses exact manufacturer limits, which are not independently fully verified here. Bypass mag path, DRDY/ST2 overflow, bus recovery and quarantine. |
| LSM6DSO32 | ST datasheet/AN5473, ST PID driver/self-test example | Poll reset completion with finite timeout, timestamp new data; self-test setup/limits in report §5.4. No INT pin needed. FIFO is B with tagged polling and overflow recovery. |
| LR1121 | Semtech datasheet/SWDR001; RadioLib7.7.1; supplied RF switch table | Check BUSY deadlines, boot/calibration/error returns, IRQ flags and CRC. No RF-ready claim from SPI success. Reset only with receive/TX state reconciled. |
| INA260 | [TI datasheet §§8.5–8.6](https://www.ti.com/lit/ds/symlink/ina260.pdf); Adafruit library | Report row restored: ID, reset,0x6527 averaging/conversion, status/read/scales.10 ms startup deadline D; no stimulated self-test. Validate against meter; current does not include bypass branches. |
| DS3231 | ADI datasheet/register map; [RTClib](https://github.com/adafruit/RTClib) as community implementation | Trust OSF/calendar, not compiled build date. No data FIFO/self-test. Do not read calendar until supply/bus is stable; a10 ms initial scheduler delay is D, not a guaranteed oscillator settling time. Compare UTC over pad wait; hung bus recovery cannot repair depleted backup power. |
| HDC3020 | TI datasheet and SBOA631 programming note; Adafruit driver | Identity+CRC, reset/heater off, auto mode; conversion and startup budgets in report. Discard first unsettled/CRC-failing sample. No assumed SHT4x interchangeability. |
| W25Q64 | Winbond suffix-specific datasheet; direct SPI commands | Actual suffix/voltage unknown. Read JEDEC/status; do not write until suffix-specific power-up/write timing is known. No FIFO; pre-erased append region, WIP timeout/readback/CRC. Maximum erase time intentionally unresolved. |
| XIAO S3 Sense | Seeed camera guide; esp32-camera SDK; supplied camera source | Actual camera/PSRAM identity and initialization ACK required. Storage/memory tests and a decoded saved frame are the self-test. Restart camera independently; no blocking MAIN waits. |
| Grove buzzer | Seeed Grove-Buzzer specification | Digital input has no registers/FIFO/SDK. Boot low; timed output test and acoustic measurement; software cannot remedy an under-rated sounder. |
| P4/Teensy | Espressif IDF5.5/Arduino3.3.x; PJRC core/API | Use SDK peripheral abstractions, watchdog/reset-reason and timing tests. No hard-coded peripheral register boot sequence; HAL timeout and stack/heap checks required. |
| B26CLM/MG90S | Manufacturer product pages; MCU PWM APIs | No readable actuator identity/status. Pulse endpoints, current, lost-signal and backdrive behavior unverified. Use measured safe positions; supply dips require electrical test.333 Hz only an optional qualified B26CLM setting. |
| EMAX/ESC/Hall | Motor brand known; ESC and Hall exact parts unidentified | Register/SDK/startup/current/rating claims cannot be made from “40 A”/“A3144-type”. Team must identify labels and test stopped/armed/loss-of-signal/edge counts. C autonomous tuning is not assumed. |

## Polling read contract

For every I²C/SPI transaction: record start, impose controller timeout, check complete transfer length/status, timestamp completion (or mapped sensor timestamp), convert units, attach sequence/validity, then publish. Ready flags are consumed once. A poll without new data cannot refresh sample time. Distinguish bus completion from sample freshness and signal integrity. No driver may silently provide cached good data as a new sample.

After recovery, reidentify, reapply and read back configuration, discard settling data, then re-enter valid state. A per-device retry counter and total deadline bound recovery. The report's9-clock bus recovery only works if lines can rise. No wired per-device power switch is assumed. Device absence remains visible in logs and CSV.

## Exact evidence gaps

The prompt's demand for *every* official document and a fully verified value for every hardware behavior cannot be honestly closed with the accessible documents and unidentified parts. HA-specific assistance commands, AG3335M channel allocation/SDK, fitted ESC/Hall/buck/flash suffix, servo current/loss-of-signal and full target-platform builds remain evidence-limited. They are mapped explicitly in `PROMPT_COVERAGE.csv` and are not counted as verified launch capability.
