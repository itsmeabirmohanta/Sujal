# Board 4 PARACHUTE — ESP32-P4-WIFI6 — every connection (same nets as the KiCad file)

Each line = one net: solder a trail or wire joining ALL the pads listed. Pin names as printed on the modules.
P4 = Waveshare ESP32-P4-WIFI6, header pin numbers as in its pinout (pin 1 = GPIO52, top-left, USB-C up).

- **GND**: U5.GND, J10.−, J11.−, J12.−, J13.−, J16.−, J14.−, J15.−, J2.GND, J5.GND, J3.GND, J1.GND, J1.GND, J6.GND, J8.GND, J8.GND, C1.-, C2.-, C3.-, C4.2, C5.2, U6.GND, U6.DO, U1.GND (P4 pin 3), U1.GND (P4 pin 38)
- **+8V_STEER**: J1.+8V, J1.+8V, C1.+, J10.+, J11.+
- **+5V_SERVO**: J6.+5VS, C2.+, J12.+, J13.+, J16.+
- **+5V_LOGIC**: J8.+5V, C3.+, C4.1, J15.+5V, U1.VSYS (P4 pin 39)
- **+3V3_B**: J8.+3V3, U5.VIN, U5.SDO, U5.CS, R6.2, C5.1, U6.VIN, U6.CS
- **SDA**: U5.SDI, U6.SDA, U1.GPIO7/SDA (P4 pin 17)
- **SCL**: U5.SCK, U6.SCL, U1.GPIO8/SCL (P4 pin 16)
- **GNSS_IN**: J5.GNSS, U1.GPIO5 (P4 pin 11)
- **STEER_L**: R1.1, U1.GPIO20 (P4 pin 35)
- **STEER_L_S**: R1.2, J10.S
- **STEER_R**: R2.1, U1.GPIO21 (P4 pin 34)
- **STEER_R_S**: R2.2, J11.S
- **BAY_LID**: R3.1, U1.GPIO22 (P4 pin 32)
- **BAY_LID_S**: R3.2, J12.S
- **PETRI**: R4.1, U1.GPIO23 (P4 pin 31)
- **PETRI_S**: R4.2, J13.S
- **SERVO_X**: R9.1, U1.GPIO33 (P4 pin 25)
- **SERVO_X_S**: R9.2, J16.S
- **ESC**: R5.1, U1.GPIO32 (P4 pin 26)
- **ESC_S**: R5.2, J14.S
- **RX2**: J2.RX2←A, U1.GPIO47 (P4 pin 22)
- **TX2**: R7.1, R8.1, U1.GPIO46 (P4 pin 24)
- **TX2_A**: R7.2, J2.TX2→A
- **TX2_B**: R8.2, J3.TX→B
- **RX3**: J3.RX3←B, U1.GPIO48 (P4 pin 21)
- **HALL**: J15.OUT, R6.1, U1.GPIO4 (P4 pin 12)

Leave open: P4 pins 36 (3V3 OUT), 37 (3V3_EN), 40 (VBUS), 30 (RUN), 19/20 (USB D−/D+), and every other unlisted P4 pin. LSM back row (SCX, SDX, CS, DO, GND) not soldered.
