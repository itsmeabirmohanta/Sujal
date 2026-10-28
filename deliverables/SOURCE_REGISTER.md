# Source register and coverage

Review date: 28 October 2026, Asia/Calcutta. The report distinguishes verified documentation from design proposals and unverified hardware behavior.

## Authority and local evidence

1. Current team clarification: passive drogue immediately after drone release; ram-air secondary at600 ±10 m. This overrides the initial interpretation of the brief.
2. Current `01_PROJECT_BRIEF_AND_PARTS.md` plus current board BUILD_LIST/KiCad files control the fixed parts and pins. The older final-build memory contains superseded Teensy/P4 maps; do not choose its first map.
3. Official mission guidelines control requirements. Relevant complete pages were extracted; pages15–18 were rendered. Radio assignment pages1–2 were visually inspected, including team025 row18. The supplied July radio plan is superseded.
4. Current repository was inspected for drivers, telemetry packing, commands, mission state, logger, board tests, camera and GCS schema/parsing/simulation. Code line references refer to the supplied snapshot.
5. Earlier chats, CDR, design reports and engineering-memory files explain history and conflicts; an AI answer or simulation is not device documentation or a passed test.

## Archive coverage and limits

- All **812 supplied files** were inventoried with byte count and SHA256. **526 text/structured files** were extracted, including **16 PDFs** and both Word files. Extracted material totals385981 lines. Nested ZIP member lists were recorded.
- All supplied chat text was scanned (about13.3 million characters), with targeted review of relevant decisions and current hardware discussions. This is not a claim of human-style line-by-line reading of every historical chat token or visual inspection of every archive image.
- Current board net/build lists and source drivers were checked against the brief. Historical Gerbers, CAD/STEP/STL meshes and all nested archive variants were not independently rebuilt, rendered or electrically/mechanically qualified. Empty DRC reports do not verify the soldered article.
- The original ZIP and extracted source were not changed. No physical board, GNSS sky reception, RF transmission, actuator, acoustic output, power endurance or drop test was available.
- Full-source manifests and extracted pages remain under `research/` in the workspace; downloaded third-party documents are excluded from the shareable handoff ZIP.

## Public primary sources retrieved

HTTP failures below are recorded rather than silently substituted with another part. Some documents could be read through web extraction even when the local download failed.

| ID | Source | Retrieval | Local evidence |
|---|---|---|---|
|gnss_protocol|[Official/vendor source](https://quectel.com/content/uploads/2024/02/Quectel_L89_R2.0_GNSS_Protocol_Specification_V1.0.pdf)|200|research/sources/gnss_protocol.pdf|
|bmp585_ds|[Official/vendor source](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmp585-ds003.pdf)|200|research/sources/bmp585_ds.pdf|
|bmp5_defs|[Official/vendor source](https://raw.githubusercontent.com/boschsensortec/BMP5_SensorAPI/master/bmp5_defs.h)|200|research/sources/bmp5_defs.h|
|bmp5_driver|[Official/vendor source](https://raw.githubusercontent.com/boschsensortec/BMP5_SensorAPI/master/bmp5.c)|200|research/sources/bmp5_driver.c|
|icm_ds|[Official/vendor source](https://product.tdk.com/system/files/dam/doc/product/sensor/mortion-inertial/imu/data_sheet/ds-000189-icm-20948-v1.5.pdf)|403|not downloaded|
|lsm_ds|[Official/vendor source](https://www.st.com/resource/en/datasheet/lsm6dso32.pdf)|ERROR|not downloaded|
|lsm_driver|[Official/vendor source](https://raw.githubusercontent.com/STMicroelectronics/lsm6dso32-pid/master/lsm6dso32_reg.c)|200|research/sources/lsm_driver.c|
|lsm_header|[Official/vendor source](https://raw.githubusercontent.com/STMicroelectronics/lsm6dso32-pid/master/lsm6dso32_reg.h)|200|research/sources/lsm_header.h|
|ina_ds|[Official/vendor source](https://www.ti.com/lit/ds/symlink/ina260.pdf)|200|research/sources/ina_ds.pdf|
|hdc_ds|[Official/vendor source](https://www.ti.com/lit/ds/symlink/hdc3020.pdf)|200|research/sources/hdc_ds.pdf|
|rtc_ds|[Official/vendor source](https://www.analog.com/media/en/technical-documentation/data-sheets/ds3231.pdf)|ERROR|not downloaded|
|lr1121_ds|[Official/vendor source](https://files.waveshare.com/wiki/Core1121/LR1121_H2_DS_v2_0.pdf)|200|research/sources/lr1121_ds.pdf|
|radio_lr11_header|[Official/vendor source](https://raw.githubusercontent.com/jgromes/RadioLib/7.7.1/src/modules/LR11x0/LR11x0.h)|200|research/sources/radio_lr11_header.h|
|radio_lr11_cpp|[Official/vendor source](https://raw.githubusercontent.com/jgromes/RadioLib/7.7.1/src/modules/LR11x0/LR11x0.cpp)|200|research/sources/radio_lr11_cpp.cpp|
|radio_lr1121_cpp|[Official/vendor source](https://raw.githubusercontent.com/jgromes/RadioLib/7.7.1/src/modules/LR11x0/LR1121.cpp)|200|research/sources/radio_lr1121_cpp.cpp|
|core1121|[Official/vendor source](https://www.waveshare.com/wiki/Core1121-XF)|403|not downloaded|
|p4board|[Official/vendor source](https://www.waveshare.com/wiki/ESP32-P4-Pico)|403|not downloaded|
|agfrc|[Official/vendor source](https://www.agf-rc.com/agfrc-coreless-mini-servo-b26clm-p1669135.html)|200|research/sources/agfrc.html|
|towerpro|[Official/vendor source](https://towerpro.com.tw/product/mg90s-3/)|200|research/sources/towerpro.html|
|grove|[Official/vendor source](https://wiki.seeedstudio.com/Grove-Buzzer/)|200|research/sources/grove.html|
|7semi_l89|[Official/vendor source](https://7semi.com/7semi-l89ha-breakout-multi-gnss-engines-for-gps-irnss-glonass-beidou-galileo-and-qzss/)|200|research/sources/7semi_l89.html|
|p4_uart|[Official/vendor source](https://docs.espressif.com/projects/esp-idf/en/stable/esp32p4/api-reference/peripherals/uart.html)|200|research/sources/p4_uart.html|
|p4_ledc|[Official/vendor source](https://docs.espressif.com/projects/esp-idf/en/stable/esp32p4/api-reference/peripherals/ledc.html)|200|research/sources/p4_ledc.html|
|p4_mcpwm|[Official/vendor source](https://docs.espressif.com/projects/esp-idf/en/stable/esp32p4/api-reference/peripherals/mcpwm.html)|200|research/sources/p4_mcpwm.html|
|p4_freertos|[Official/vendor source](https://docs.espressif.com/projects/esp-idf/en/stable/esp32p4/api-reference/system/freertos_idf.html)|200|research/sources/p4_freertos.html|
|teensy_serial|[Official/vendor source](https://www.pjrc.com/teensy/td_uart.html)|200|research/sources/teensy_serial.html|
|teensy_wire|[Official/vendor source](https://www.pjrc.com/teensy/td_libs_Wire.html)|200|research/sources/teensy_wire.html|
|teensy_timer|[Official/vendor source](https://www.pjrc.com/teensy/td_timing_IntervalTimer.html)|200|research/sources/teensy_timer.html|
|teensy_sd|[Official/vendor source](https://www.pjrc.com/teensy/td_libs_SD.html)|404|not downloaded|
|teensy_eeprom|[Official/vendor source](https://www.pjrc.com/teensy/td_libs_EEPROM.html)|200|research/sources/teensy_eeprom.html|
|nasa_wind|[Official/vendor source](https://ntrs.nasa.gov/citations/20200011546)|200|research/sources/nasa_wind.html|
|parafoil_paper|[Official/vendor source](https://arxiv.org/pdf/2105.00715)|200|research/sources/parafoil_paper.pdf|
|gnss_forum|[Official/vendor source](https://forums.quectel.com/t/problem-with-pstm-commands-for-l89ha-gnss-module/48222)|200|research/sources/gnss_forum.html|
|hdc_driver|[Official/vendor source](https://raw.githubusercontent.com/adafruit/Adafruit_HDC302x/main/Adafruit_HDC302x.cpp)|200|research/sources/hdc_driver.cpp|
|ino_ina|[Official/vendor source](https://raw.githubusercontent.com/adafruit/Adafruit_INA260/master/Adafruit_INA260.cpp)|200|research/sources/ino_ina.cpp|
|w25_ds|[Official/vendor source](https://www.winbond.com/resource-files/w25q64jv%20spi%20%20%20revc%2006032016%20kms.pdf)|200|research/sources/w25_ds.pdf|
|sparkfun_icm|[Official/vendor source](https://github.com/sparkfun/SparkFun_ICM-20948_ArduinoLibrary)|200|research/sources/sparkfun_icm.html|
|gnss_hardware|[Official/vendor source](https://quectel.com/content/uploads/2024/02/Quectel_L89_R2.0_Hardware-Design_V1.2.pdf)|200|research/sources/gnss_hardware.pdf|
|icm_v14|[Official/vendor source](https://invensense.tdk.com/wp-content/uploads/2021/07/DS-000189-ICM-20948-v1.4.pdf)|404|not downloaded|
|rtc_ds|[Official/vendor source](https://www.analog.com/media/en/technical-documentation/data-sheets/DS3231.pdf)|ERROR|not downloaded|
|lsm_an|[Official/vendor source](https://www.st.com/resource/en/application_note/an5473-lsm6dso32-alwayson-3d-accelerometer-and-3d-gyroscope-stmicroelectronics.pdf)|ERROR|not downloaded|
|semtech_driver|[Official/vendor source](https://raw.githubusercontent.com/Lora-net/SWDR001/master/README.md)|200|research/sources/semtech_driver.txt|
|st_fifo_example|[Official/vendor source](https://raw.githubusercontent.com/STMicroelectronics/STMems_Standard_C_drivers/master/lsm6dso32_STdC/examples/lsm6dso32_fifo.c)|404|not downloaded|
|st_selftest_example|[Official/vendor source](https://raw.githubusercontent.com/STMicroelectronics/STMems_Standard_C_drivers/master/lsm6dso32_STdC/examples/lsm6dso32_self_test.c)|200|research/sources/st_selftest_example.txt|
|icm_driver_header|[Official/vendor source](https://raw.githubusercontent.com/sparkfun/SparkFun_ICM-20948_ArduinoLibrary/main/src/util/ICM_20948_REGISTERS.h)|200|research/sources/icm_driver_header.txt|
|radiolib_properties|[Official/vendor source](https://raw.githubusercontent.com/jgromes/RadioLib/7.7.1/library.properties)|200|research/sources/radiolib_properties.txt|
|arduino33_uart|[Official/vendor source](https://raw.githubusercontent.com/espressif/arduino-esp32/3.3.0/cores/esp32/HardwareSerial.h)|200|research/sources/arduino33_uart.txt|
|hdc_ti_note|[Official/vendor source](https://www.ti.com/lit/an/sboa631/sboa631.pdf)|200|research/sources/hdc_ti_note.pdf|

## Additional primary sources consulted through web tools

- [ST LSM6DSO32 datasheet](https://www.st.com/resource/en/datasheet/lsm6dso32.pdf), [AN5473](https://www.st.com/resource/en/application_note/an5473-lsm6dso32-alwayson-3d-accelerometer-and-3d-gyroscope-stmicroelectronics.pdf), [official driver](https://github.com/STMicroelectronics/lsm6dso32-pid). Local ST PDF retrieval timed out; the official driver and self-test source were downloaded.
- [TDK ICM-20948 DS-000189](https://product.tdk.com/system/files/dam/doc/product/sensor/mortion-inertial/imu/data_sheet/ds-000189-icm-20948-v1.5.pdf), with [SparkFun register reference](https://github.com/sparkfun/SparkFun_ICM-20948_ArduinoLibrary). TDK search excerpts and official register descriptions were available; complete local TDK PDF download was denied. Do not regard every timing/self-test limit as independently verified here.
- [ADI DS3231 datasheet](https://www.analog.com/media/en/technical-documentation/data-sheets/ds3231.pdf). Web text was available; local download timed out.
- [Quectel L89 product/document index](https://www.quectel.com/product/gnss-l89-r2/) lists HA protocol v1.1, hardware v1.4 and AGNSS notes, but those downloads require an account. The accessible v1.0 protocol and v1.2 hardware guide were used with explicit installed-firmware verification gates.
- [7Semi L89HA official library](https://github.com/7semi-solutions/7semi-L89HA-GNSS-Module-Arduino-Library), also supplied in the ZIP. It is a board-vendor library, not an Airoha register-level SDK.
- [P4 UART v5.5](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32p4/api-reference/peripherals/uart.html), [MCPWM v5.5](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32p4/api-reference/peripherals/mcpwm.html), [FreeRTOS v5.5](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32p4/api-reference/system/freertos_idf.html), [Arduino LEDC](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/ledc.html). Stable/latest pages can drift; verify against the locked core.
- [RadioLib LR1120 API at7.7.1](https://github.com/jgromes/RadioLib/blob/7.7.1/src/modules/LR11x0/LR1120.h) and [implementation](https://github.com/jgromes/RadioLib/blob/7.7.1/src/modules/LR11x0/LR1120.cpp); [Semtech calculator](https://www.semtech.com/design-support/lora-calculator).
- [NASA standard atmosphere](https://ntrs.nasa.gov/api/citations/19770009539/downloads/19770009539.pdf) for the declared atmosphere model; [NASA parafoil wind/aerodynamic estimation](https://ntrs.nasa.gov/citations/20200011546); [parafoil real-time guidance research](https://arxiv.org/abs/2105.00715).
- [ArduPilot parachute documentation](https://docs.ardupilot.org/plane/docs/common-parachute.html) is a deployment failsafe reference, not a demonstrated ram-air guidance mode. Fixed-wing automatic landing code cannot be copied as a qualified parafoil controller.

## Unclosed source/measurement questions

- No verified public AG3335M firmware SDK, complete HA-specific EPO injection transaction, firmware-matched dynamic model or geofence command was obtained. No LC29H/LC76G command was assumed interchangeable.
- Exact installed flash suffix, ESC, Hall sensor, steering buck, camera sensor and servo-programming configuration remain team identification tasks.
- No manufacturer guarantee was found for the fitted servos’ loss-of-signal holding, backdrive torque or stall current. These are measurements, not constants to invent.
- The canopy geometry conflict remains unresolved. Supplied CFD/FEA/Monte Carlo claims were not rerun or adopted as flight performance.
- Reference frame/log/journal codecs, scheduling helpers, scalar estimation, wind fit and planner blocks now have host checks. Complete board adapters, persistent storage writes, path constraints, device drivers and GUI integration remain outstanding; see IMPLEMENTATION_STATUS.md.

## Source hashes

The workspace JSON manifests contain SHA256 values for downloaded source files. The handoff has its own manifest so future edits can be distinguished from this review.
