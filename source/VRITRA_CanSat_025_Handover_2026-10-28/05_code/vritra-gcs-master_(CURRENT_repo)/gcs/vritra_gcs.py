#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
VRITRA Ground Control Station  ·  v5.0
Team 2026-INSPACe-CAN-7USAT-025  ·  Lovely Professional University
IN-SPACe CAN-7USAT India Student Competition 2026 (3rd Edition)

Single-file PyQt5 desktop application. No browser, no web server, no GPU,
no internet. Frozen to one VRITRA_GCS.exe with PyInstaller for field use.

ARCHITECTURE
    RF modem ──serial──> SerialLink (worker thread)
                              │  raw CSV lines
                              ▼
                        queue.Queue (bounded, drop-oldest)
                              │
                              ▼
                     MainWindow._tick()  (QTimer, UI thread)
                              │
              parse ─> physics ─> CSV log ─> limits/staleness ─> paint

    The reception layer never touches a widget and the render loop never
    blocks on I/O. This is the decoupling mandated by the design research:
    a stalled port, a flood of packets, or a broken panel cannot freeze the
    operator's display or interrupt the graded CSV.

DESIGN LANGUAGE — "austere base, science colour"
    Flight and engineering data render monochrome on near-black. Amber and
    red are reserved exclusively for limit breaches and telemetry staleness,
    so a genuine alarm has no competition for the operator's eye. Colour is
    permitted inside scientific plots only where it encodes a real data
    dimension (e.g. AS7343 wavelength).

WIRE-FORMAT CONTRACT
    The 79 SCHEMA fields below are positional and must stay in exact sync
    with the Teensy downlink. Never delete or reorder a field to tidy the
    UI — every column after it would shift and the parser would silently
    read wrong values. Retire a field by hiding it in the UI, not by
    removing it from SCHEMA.
"""

import os
import sys
import csv
import math
import time
import json
import queue
import logging
import datetime
import threading
import traceback
from pathlib import Path
from collections import deque, OrderedDict

try:
    import serial
    import serial.tools.list_ports
    HAVE_SERIAL = True
except ImportError:
    HAVE_SERIAL = False

try:
    import winsound
    HAVE_WINSOUND = True
except ImportError:
    HAVE_WINSOUND = False

import numpy as np

# =========================================================================
# §1  MISSION CONSTANTS
# =========================================================================
TEAM_ID       = "2026-INSPACe-CAN-7USAT-025"
TEAM_NAME     = "VRITRA"
TEAM_INST     = "Lovely Professional University"
APP_VERSION   = "5.0.0"
APP_TITLE     = f"VRITRA GCS  ·  {TEAM_ID}"
CSV_FILENAME  = f"Flight_{TEAM_ID}.csv"      # competition-mandated name §6.3.iii.A
XML_FILENAME  = f"Flight_{TEAM_ID}.xml"
KML_FILENAME  = f"Track_{TEAM_ID}.kml"

DEFAULT_BAUD      = 115200
RECONNECT_BACKOFF = [1.0, 2.0, 4.0, 8.0]
TELEMETRY_HZ      = 1.0        # §6.3.i mandate: >= 1 Hz
SIM_HZ            = 4.0
UI_TICK_MS        = 100        # 10 Hz render / staleness evaluation
MAX_DRAIN_PER_TICK = 40        # flood governor: never block the UI thread

# --- CanSat physical constants (PDR) -------------------------------------
CANSAT_MASS_KG = 0.9954
CANSAT_OD_M    = 0.150
CANSAT_LEN_M   = 0.400
I_Z            = 2.103e-3      # long-axis inertia      [kg·m²]
I_T            = 1.432e-2      # transverse inertia     [kg·m²]

# --- Atmosphere (ISA) ----------------------------------------------------
SEA_LEVEL_P_PA  = 101325.0
SEA_LEVEL_T_K   = 288.15
TROPO_LAPSE_K_M = 0.0065
R_AIR           = 287.05
GAMMA_AIR       = 1.4
G_EARTH         = 9.80665
MU_AIR_REF      = 1.81e-5

# --- LoRa / LR1121 radio -------------------------------------------------
# Organiser-assigned plan, team 025 ("CAN7USAT Team_Frequency_Assignments_19_10",
# IN-SPACe mail of 19-10-2026, supersedes the earlier 867.00/865.75 SF10 0x98
# plan): 868 Main 866.50 / Backup 865.25 MHz, SF9 (launch pad 3), sync 0x99,
# BW 125 kHz mandatory, CR 4/5, TX +10 dBm on pad / ~+14 dBm in flight.
LORA_FREQ_MHZ  = 866.50
LORA_FREQ_BACKUP_MHZ = 865.25
# LR1121 sensitivity at the ASSIGNED SF9/BW125. Datasheet (RxBoosted) gives
# SF7 -127 / SF12 -141 dBm, i.e. ~-132.6 dBm at SF9; kept ~3 dB conservative,
# the same margin the old SF10 figure carried, because LINK_MARGIN uses it.
LORA_SENS_DBM  = -129.5       # LR1121 @ SF9 / BW125
LORA_SF        = 9
LORA_BW_HZ     = 125000
LORA_CR        = 4         # 4/5
LORA_SYNC_WORD = 0x99      # 0x80 + team id 25, per the 19-10 allocation plan
LORA_TX_DBM_PAD    = 10
LORA_TX_DBM_FLIGHT = 14

# --- WPC India spectrum rules (Gazette G.S.R. 853(E) / 564(E)) -----------
# Licence-exempt SRD use. The 865-868 MHz band is the allocation; the
# 865-867 MHz sub-band is the one covered by the ETA clarification, so the
# compliance check flags carriers that stray above 867 MHz.
WPC_BAND_LO_MHZ     = 865.0
WPC_BAND_HI_MHZ     = 868.0
WPC_ETA_LO_MHZ      = 865.0
WPC_ETA_HI_MHZ      = 867.0
WPC_MAX_TX_W        = 1.0      # transmitter power
WPC_MAX_ERP_W       = 4.0      # effective radiated power
WPC_MAX_BW_KHZ      = 200.0    # carrier bandwidth
DUTY_CYCLE_LIMIT    = 0.01     # 1 % (ETSI EN 300 220 convention)

# Simulator launch site. Only the SIMULATOR uses this — in real operation the
# pad origin is SURVEYED from the live fixes received while the vehicle reports
# STATE=BOOT (see Physics.__init__), so the GCS never needs to be told where it
# is. Set it to wherever you are actually flying so the ground track, UTM grid
# and recovery bearings look right during rehearsal.
SIM_SITE_NAME   = "Kushinagar, UP"
SIM_SITE_LAT    = 26.7400
SIM_SITE_LON    = 83.8900

SD_CAP_BYTES     = 32 * 1024 * 1024 * 1024   # 32 GB card fitted on the Teensy 4.1
FLASH_CAP_BYTES  = 8 * 1024 * 1024
BYTES_PER_PACKET = 56                        # packed binary downlink, see
                                             # vritra_fc/Config.h DL_PACKET_LEN

# --- Flight software states (§6.2.i) -------------------------------------
STATES    = ["BOOT", "LAUNCH", "EJECT", "SEC_EJECT", "TOUCHDOWN"]
STATE_NUM = {s: i for i, s in enumerate(STATES)}

# How many on-pad fixes to fold into the surveyed pad origin before freezing
# it. At the 0.5 Hz pad downlink that is ~2 minutes of averaging, which is
# comfortably past the point a GNSS solution has settled. The cap exists so a
# long pre-flight hold cannot leave the origin so heavily averaged that a
# genuine late correction from the receiver can no longer move it.
PAD_SURVEY_MAX = 60

# Every entry here MUST exist in CMD_TABLE in vritra_gs/vritra_gs.ino and in
# enum CmdId in vritra_fc/Config.h. A command offered here but missing from the
# gateway's table is accepted by this UI, dropped silently on the USB link, and
# never reaches the vehicle — the operator sees no error at all.
#
# REMOVED at this revision: CMD_ARM, CMD_DISARM, CMD_FORCE_EJECT,
# CMD_FORCE_SEC_EJ, CMD_BRAKE_DEPLOY. The three FORCE/BRAKE commands drove
# actuators this vehicle does not carry — there is no ejection charge, no
# secondary charge and no drag brake — and the arming pair is superseded:
# CMD_TX_ON now arms the flight state machine as well, since guideline §6.1.v
# already makes it the last deliberate action before flight. Their ids (4-8)
# remain reserved on the wire so the remaining ids never shift meaning.
COMMANDS = {
    "CMD_TX_ON":        "Enable downlink transmission (also ARMS the FSM)",
    "CMD_TX_OFF":       "Disable downlink transmission (does not disarm)",
    "CMD_CALIBRATE":    "Zero attitude / baro / gyro at current position",
    "CMD_BEACON_ON":    "Audio beacon ON (post-landing)",
    "CMD_BEACON_OFF":   "Audio beacon OFF",
    "CMD_FAILOVER":     "Switch to backup frequency (gateway follows)",
    "CMD_RESET":        "Soft reset flight computer",
    "CMD_GYRO_ON":      "Gyro stabilisation wheel ON",
    "CMD_GYRO_OFF":     "Gyro stabilisation wheel OFF",
    "CMD_GYRO_SPD":     "Gyro wheel speed, arg = percent",
    "CMD_MAG_CAL":      "Magnetometer hard-iron cal: arg 1 start, 0 finish",
    "CMD_GPS_RESET":    "Hard-reset the GNSS receiver",
    "CMD_NEW_MISSION":  "Wipe mission state and reboot (destructive)",
}

COMMAND_LABELS = {
    "CMD_TX_ON":       "Tx On",
    "CMD_TX_OFF":      "Tx Off",
    "CMD_CALIBRATE":   "Calibrate",
    "CMD_BEACON_ON":   "Beacon On",
    "CMD_BEACON_OFF":  "Beacon Off",
    "CMD_FAILOVER":    "Failover",
    "CMD_RESET":       "Reset",
    "CMD_GYRO_ON":     "Gyro On",
    "CMD_GYRO_OFF":    "Gyro Off",
    "CMD_GYRO_SPD":    "Gyro Speed",
    "CMD_MAG_CAL":     "Mag Cal",
    "CMD_GPS_RESET":   "GNSS Reset",
    "CMD_NEW_MISSION": "New Mission",
}


def _log_dir() -> Path:
    base = Path(os.path.expanduser("~")) / "VRITRA_GCS_logs"
    base.mkdir(parents=True, exist_ok=True)
    return base


LOG_DIR = _log_dir()
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
    handlers=[logging.FileHandler(LOG_DIR / "vritra_gcs.log", encoding="utf-8"),
              logging.StreamHandler(sys.stderr)],
)
log = logging.getLogger("VRITRA")


# =========================================================================
# §2  TELEMETRY DICTIONARY — the single source of truth
# =========================================================================
# Every field carries its own metadata, so one dict drives the CSV header,
# the parser, every tile, the limits engine, the console formatter, the
# parameter browser and the exported machine-readable dictionary.
#
#   u    unit string
#   g    subsystem group (also selects the staleness class)
#   d    human description
#   lim  (red_low, yellow_low, yellow_high, red_high); None disables limits.
#        Any bound may individually be None for one-sided parameters.
#   pers persistence — consecutive out-of-limit samples required before the
#        state actually changes. Stops a single noisy packet raising alarms.
#   disc discrete/enumerated field: shown as a state, never limit-checked
#        numerically and never interpolated on a chart.
#
# Limit values are grounded in the flight hardware: 3S Molicel Li-Ion with
# NO BMS fitted (so BUS_V is the primary battery-health indicator), LR1121
# at -137 dBm sensitivity, and the PDR thermal/mechanical envelopes.
# -------------------------------------------------------------------------
def _f(u="", g="core", d="", lim=None, pers=1, disc=False):
    return {"u": u, "g": g, "d": d, "lim": lim, "pers": pers, "disc": disc}


SCHEMA = OrderedDict([
    # ---- COMPETITION-MANDATED CORE, positions 0..15 (§6.3 / PDR Table 10).
    # ---- Order is fixed by the guideline. Do not reorder.
    ("TIME_S",    _f("s",    "core", "Mission time since power-on")),
    ("PKT",       _f("",     "core", "Packet counter")),
    ("ALT_M",     _f("m",    "core", "Barometric altitude AGL", (None, None, 1200, 1500), 2)),
    ("PRES_PA",   _f("Pa",   "core", "Atmospheric pressure", (50000, 70000, 105000, 110000), 2)),
    ("TEMP_C",    _f("°C",   "core", "Air temperature", (-20, -10, 60, 75), 3)),
    # 3S Li-Ion (3 x 21700, metal can per guideline 5.3.iii) behind a 3S 40 A BMS,
    # as declared in PDR 5.5.1 and modelled in Fusion. 11.1 V nominal, 12.6 V full.
    # NOTE: the bench ADC log reads a steady 7.87 V, which is NOT a healthy 3S
    # pack (that would be 2.62 V/cell, below any BMS cut-off). Whatever was on the
    # bench was not the flight pack. Confirm before flight — and if the pack ever
    # changes, this line and BATT_CELLS/BATT_EMPTY_V/BATT_FULL_V in
    # vritra_fc/Config.h must change together, never one without the other.
    ("BUS_V",     _f("V",    "core", "Main bus voltage · 3S Li-Ion + BMS", (9.6, 10.5, 12.6, 12.9), 3)),
    ("GNSS_TIME", _f("s",    "core", "GNSS receiver time")),
    ("LAT",       _f("°",    "core", "GNSS latitude")),
    ("LON",       _f("°",    "core", "GNSS longitude")),
    ("GPS_ALT_M", _f("m",    "core", "GNSS altitude")),
    ("SATS",      _f("",     "gnss", "GNSS satellites tracked", (3, 5, None, None), 3)),
    ("ACC_X",     _f("m/s²", "imu",  "Accelerometer X (LSM6DSO32)")),
    ("ACC_Y",     _f("m/s²", "imu",  "Accelerometer Y (LSM6DSO32)")),
    ("ACC_Z",     _f("m/s²", "imu",  "Accelerometer Z (LSM6DSO32)")),
    ("SPIN_RATE", _f("°/s",  "imu",  "Mechanical-gyro spin rate", (None, None, 180, 360), 3)),
    ("STATE",     _f("",     "core", "Flight-software state 0..4", disc=True)),
    # ---- TEAM PAYLOAD, appended per §6.3.B --------------------------------
    ("BUS_I_MA",  _f("mA",   "power", "Main bus current (INA226)", (None, None, 1500, 2500), 3)),
    ("SOC_PCT",   _f("%",    "power", "Battery state of charge", (10, 25, None, None), 3)),
    ("GND_SPEED", _f("m/s",  "gnss",  "GNSS ground speed")),
    ("HUM_PCT",   _f("%",    "env",   "Relative humidity")),
    # AX / AY / AZ USED TO BE HERE, AT 20/21/22. THEY WERE NOT DATA.
    # They are ACC_X/ACC_Y/ACC_Z (11/12/13, mandated, m/s^2) divided by g, and
    # the flight computer filled both from the same three variables. They never
    # rode the wire — the gateway synthesised them on arrival — so carrying
    # them as SCHEMA fields bought nothing and cost the thing this codebase
    # keeps paying for elsewhere: two slots holding one fact, free to disagree
    # the moment one path changes. They are now DERIVED (see below), computed
    # once in Physics from the mandated field, still displayed in g on the
    # INERTIAL panel exactly as before.
    ("GX",        _f("°/s",  "imu",   "Gyroscope X (ICM-20948)")),
    ("GY",        _f("°/s",  "imu",   "Gyroscope Y (ICM-20948)")),
    ("GZ",        _f("°/s",  "imu",   "Gyroscope Z (ICM-20948)")),
    ("MX",        _f("µT",   "imu",   "Magnetometer X (ICM-20948)")),
    ("MY",        _f("µT",   "imu",   "Magnetometer Y (ICM-20948)")),
    ("MZ",        _f("µT",   "imu",   "Magnetometer Z (ICM-20948)")),
    ("AHX",       _f("g",    "imu",   "High-g accel X (LSM6DSO32 ±32 g)")),
    ("AHY",       _f("g",    "imu",   "High-g accel Y (LSM6DSO32 ±32 g)")),
    ("AHZ",       _f("g",    "imu",   "High-g accel Z (LSM6DSO32 ±32 g)")),
    ("PM1",       _f("µg/m³","env",   "Particulate matter 1.0 (BMV080)")),
    ("PM25",      _f("µg/m³","env",   "Particulate matter 2.5 (BMV080)", (None, None, 55, 150), 3)),
    ("PM10",      _f("µg/m³","env",   "Particulate matter 10 (BMV080)")),
    ("LUX",       _f("lx",   "env",   "Illuminance (BH1750)")),
    ("UV",        _f("mW/cm²","env",  "UV irradiance (ML8511)")),
    ("CO2",       _f("ppm",  "env",   "CO2 concentration (SCD41)", (None, None, 2000, 5000), 3)),
    ("TVOC",      _f("ppb",  "env",   "Total VOC (ENS160)")),
    ("ECO2",      _f("ppm",  "env",   "Equivalent CO2 (ENS160)")),
    ("AQI",       _f("",     "env",   "Air-quality index 1..5 (ENS160)")),
    ("O3",        _f("ppb",  "env",   "Ozone (MQ131)")),
    ("HDC_T",     _f("°C",   "env",   "Reference temperature (HDC3020)")),
    ("HDC_RH",    _f("%",    "env",   "Reference humidity (HDC3020)")),
    ("LGT_DIST",  _f("km",   "env",   "Lightning distance (AS3935)")),
    ("LGT_INT",   _f("",     "env",   "Lightning intensity (AS3935)")),
    ("LGT_FLAG",  _f("",     "env",   "Lightning strike detected", disc=True)),
    ("S_405",     _f("µW/cm²","spec", "Spectral 405 nm (AS7343)")),
    ("S_425",     _f("µW/cm²","spec", "Spectral 425 nm (AS7343)")),
    ("S_450",     _f("µW/cm²","spec", "Spectral 450 nm (AS7343)")),
    ("S_475",     _f("µW/cm²","spec", "Spectral 475 nm (AS7343)")),
    ("S_515",     _f("µW/cm²","spec", "Spectral 515 nm (AS7343)")),
    ("S_550",     _f("µW/cm²","spec", "Spectral 550 nm (AS7343)")),
    ("S_555",     _f("µW/cm²","spec", "Spectral 555 nm (AS7343)")),
    ("S_600",     _f("µW/cm²","spec", "Spectral 600 nm (AS7343)")),
    ("S_640",     _f("µW/cm²","spec", "Spectral 640 nm (AS7343)")),
    ("S_690",     _f("µW/cm²","spec", "Spectral 690 nm (AS7343)")),
    ("S_745",     _f("µW/cm²","spec", "Spectral 745 nm (AS7343)")),
    ("S_855",     _f("µW/cm²","spec", "Spectral 855 nm NIR (AS7343)")),
    ("S_CLR",     _f("µW/cm²","spec", "Spectral clear channel (AS7343)")),
    ("S_FLK",     _f("Hz",   "spec",  "Flicker frequency (AS7343)")),
    ("RSSI",      _f("dBm",  "rf",    "Downlink RSSI (LR1121)", (-125, -110, None, None), 3)),
    ("SNR",       _f("dB",   "rf",    "Downlink SNR (LR1121)", (-15, -7, None, None), 3)),
    ("FREQ_MHZ",  _f("MHz",  "rf",    "Carrier frequency — Team 025: 866.50 / 865.25 MHz",
                  (865.0, 865.1, 867.01, 868.0), 2)),
    ("SERVO_V",   _f("V",    "power", "Servo rail voltage", (4.5, 4.8, 6.2, 6.5), 3)),
    ("SERVO_I_MA",_f("mA",   "power", "Servo rail current", (None, None, 1500, 2500), 3)),
    ("SRV1_DEG",  _f("°",    "mech",  "Eject servo 1 (MG90S)")),
    ("SRV2_DEG",  _f("°",    "mech",  "Eject servo 2 (MG90S)")),
    ("SRV3_DEG",  _f("°",    "mech",  "Brake servo L (OT5330M)")),
    ("SRV4_DEG",  _f("°",    "mech",  "Brake servo R (OT5330M)")),
    ("MCU_ACTIVE",_f("",     "sys",   "Active MCU: 0 primary, 1 backup", disc=True)),
    ("WDOG_OK",   _f("",     "sys",   "Watchdog petting OK (TPL5010)", disc=True)),
    ("SD_PCT",    _f("%",    "sys",   "microSD utilisation", (None, None, 80, 95), 2)),
    ("FLASH_PCT", _f("%",    "sys",   "W25Q64 flash utilisation", (None, None, 80, 95), 2)),
    ("BEACON_ON", _f("",     "sys",   "Recovery beacon active", disc=True)),
    ("CAM1_OK",   _f("",     "sys",   "Camera 1 health flag", disc=True)),
    ("CAM2_OK",   _f("",     "sys",   "Camera 2 health flag", disc=True)),
    ("NAVIC_SATS",_f("",     "gnss",  "NavIC / IRNSS satellites")),
    ("UTC",       _f("",     "sys",   "UTC timestamp (DS3231)", disc=True)),
    # ---- GNSS QUALITY, appended at positions 79..83 -----------------------
    # Appending is safe: the parser is positional and trailing fields are
    # optional, so every existing column keeps its index and a firmware that
    # does not yet send these simply leaves them absent (rendered as "—").
    # These come straight out of the Quectel L89's NMEA GGA/GSA sentences.
    ("HDOP",      _f("",     "gnss",  "Horizontal dilution of precision",
                     (None, None, 2.0, 5.0), 3)),
    ("VDOP",      _f("",     "gnss",  "Vertical dilution of precision",
                     (None, None, 3.0, 7.0), 3)),
    ("FIX_QUALITY", _f("",   "gnss",  "GGA fix quality 0..6", disc=True)),
    ("GEOID_SEP", _f("m",    "gnss",  "Geoid separation (MSL - WGS84 ellipsoid)")),
    ("GNSS_SYS",  _f("",     "gnss",  "Constellation bitmask in the solution",
                     disc=True)),

    # ---- fused attitude, computed ON THE FLIGHT COMPUTER -------------------
    # Appended at the end (indices 81-82) so no existing graded column moves.
    # These are NOT derived here. A 50 Hz complementary filter on the Teensy
    # fuses gyro and accelerometer and transmits the result; the ground station
    # only ever sees 1 Hz, so anything it computed itself would be a static
    # tilt with every rotation between samples invisible.
    #
    # WHY THESE TWO SURVIVED THE WIRE-LAYOUT-3 CUT AND THE OTHER FOUR DID NOT.
    # Guideline table row 13 defines <ACCELEROMETER DATA> as "acceleration and
    # roll & pitch parameters", so roll and pitch are MANDATED CONTENT and stay
    # on the wire and in the graded CSV whether or not a panel reads them.
    # ATT_YAW and QUAT_X/Y/Z were named nowhere in that table -- they fell under
    # row 16 <OPTIONAL DATA> -- and cost 12 B of every 86 B packet to carry
    # attitude this ground station never displayed: no panel read ATT_YAW, and
    # _update_instruments() explicitly severed the quaternion with
    # set_quaternion(None). Both are still recorded at 20 Hz on the vehicle's
    # own SD card, which is a better record than a 1 Hz copy of it.
    ("ATT_ROLL",  _f("°",    "imu",   "Fused roll (flight-computer 50 Hz)")),
    ("ATT_PITCH", _f("°",    "imu",   "Fused pitch (flight-computer 50 Hz)")),
])

ALL_FIELDS  = list(SCHEMA.keys())
N_FIELDS    = len(ALL_FIELDS)
CORE_FIELDS = [k for k, v in SCHEMA.items() if v["g"] == "core"]
INT_FIELDS  = {"PKT", "STATE", "SATS", "AQI", "MCU_ACTIVE", "WDOG_OK",
               "BEACON_ON", "CAM1_OK", "CAM2_OK", "NAVIC_SATS", "LGT_FLAG"}

# Decimal places used when a field is WRITTEN to the CSV / wire format.
# Coordinates need 7: at 3 dp one unit of latitude is ~111 m, which would
# quantise the graded track and the recovery fix to the size of a football
# field. Everything else is comfortable at 3.
CSV_DECIMALS = {"LAT": 7, "LON": 7, "GNSS_TIME": 2, "TIME_S": 3}
CSV_DEFAULT_DECIMALS = 3


def csv_cell(key, v):
    """Format one value for the CSV / wire format at its correct precision."""
    if isinstance(v, str):
        return v
    if key in INT_FIELDS:
        return str(int(round(v)))
    return f"{v:.{CSV_DECIMALS.get(key, CSV_DEFAULT_DECIMALS)}f}"

GROUP_LABEL = {
    "core": "Flight Core", "imu": "Inertial", "gnss": "Navigation",
    "power": "Power", "env": "Environment", "spec": "Spectral",
    "rf": "Radio Link", "mech": "Mechanisms", "sys": "Systems",
}

# -------------------------------------------------------------------------
# Derived parameters get the same dictionary treatment so the limits engine
# and the parameter browser cover computed values too.
# -------------------------------------------------------------------------
DERIVED = OrderedDict([
    ("VSPEED",      _f("m/s",  "core",  "Vertical speed (baro differentiation)")),
    ("DESCENT",     _f("m/s",  "core",  "Descent rate")),
    ("AGL_M",       _f("m",    "core",  "Height above launch site")),
    ("APOGEE_M",    _f("m",    "core",  "Apogee (running maximum)")),
    ("SPEED_3D",    _f("m/s",  "core",  "3-D speed (GNSS + baro fusion)")),
    ("PEAK_V",      _f("m/s",  "core",  "Peak 3-D speed")),
    ("TTL_S",       _f("s",    "core",  "Projected time to landing")),
    # Moved out of SCHEMA — see the note at the old 20/21/22 position. Same
    # measurement as the mandated ACC_X/Y/Z, expressed in g because that is
    # what an IMU readout is read in. One source, two presentations.
    ("AX",          _f("g",    "imu",   "Accelerometer X in g (= ACC_X / 9.80665)")),
    ("AY",          _f("g",    "imu",   "Accelerometer Y in g (= ACC_Y / 9.80665)")),
    ("AZ",          _f("g",    "imu",   "Accelerometer Z in g (= ACC_Z / 9.80665)")),
    ("ROLL",        _f("°",    "imu",   "Roll angle (accelerometer)")),
    ("PITCH",       _f("°",    "imu",   "Pitch angle (accelerometer)")),
    ("YAW",         _f("°",    "imu",   "Yaw / heading")),
    ("HEADING",     _f("°",    "imu",   "Tilt-compensated magnetic heading")),
    ("TILT",        _f("°",    "imu",   "Body tilt from vertical", (None, None, 30, 60), 3)),
    ("SPIN_DPS",    _f("°/s",  "imu",   "Authoritative spin rate")),
    ("OMEGA_T",     _f("°/s",  "imu",   "Transverse angular rate")),
    ("INSTAB",      _f("",     "imu",   "Instability index 0..1", (None, None, 0.5, 0.8), 3)),
    ("STABLE",      _f("",     "imu",   "Stability index 0..1")),
    ("NUTATION",    _f("°/s",  "imu",   "Nutation rate")),
    ("CONING",      _f("°/s",  "imu",   "Coning rate proxy")),
    ("H_BODY",      _f("mN·m·s","imu",  "Body angular momentum")),
    ("B_MAG",       _f("µT",   "imu",   "Magnetic field magnitude")),
    ("B_INCL",      _f("°",    "imu",   "Magnetic inclination")),
    ("G_HI",        _f("g",    "imu",   "High-g magnitude")),
    ("G_HI_PK",     _f("g",    "imu",   "Peak high-g")),
    ("AIR_RHO",     _f("kg/m³","env",   "Air density (ISA)")),
    ("P_ALT",       _f("m",    "env",   "Pressure altitude")),
    ("DENS_ALT",    _f("m",    "env",   "Density altitude")),
    ("DEW_POINT",   _f("°C",   "env",   "Dew point (Magnus)")),
    ("CLOUD_BASE",  _f("m",    "env",   "Estimated cloud base (Espy)")),
    ("AQI_PM",      _f("",     "env",   "EPA AQI from PM2.5", (None, None, 100, 200), 3)),
    ("IRR_TOT",     _f("µW/cm²","spec", "Total spectral irradiance")),
    ("DYN_Q",       _f("Pa",   "core",  "Dynamic pressure")),
    ("MACH",        _f("",     "core",  "Mach number")),
    ("REYNOLDS",    _f("",     "core",  "Reynolds number")),
    ("SOS",         _f("m/s",  "core",  "Speed of sound")),
    ("GLIDE_LD",    _f("",     "core",  "Glide lift-to-drag ratio")),
    ("GLIDE_SLOPE", _f("°",    "core",  "Glide slope angle")),
    ("SINK_RATE",   _f("m/s",  "core",  "Sink rate")),
    ("P_BUS",       _f("W",    "power", "Bus power")),
    ("P_SERVO",     _f("W",    "power", "Servo power")),
    ("P_TOTAL",     _f("W",    "power", "Total power")),
    ("ENERGY_WH",   _f("Wh",   "power", "Cumulative energy")),
    ("LINK_MARGIN", _f("dB",   "rf",    "Link margin above LR1121 floor", (3, 8, None, None), 3)),
    ("DIST_M",      _f("m",    "gnss",  "Great-circle range from pad")),
    ("BEARING",     _f("°",    "gnss",  "Bearing from pad")),
    ("BARO_GPS_DALT", _f("m",  "gnss",  "Baro vs GNSS altitude difference")),
])

FIELD_META = OrderedDict(list(SCHEMA.items()) + list(DERIVED.items()))


# =========================================================================
# §3  PARSER  —  host does all the work, the CanSat sends raw bytes
# =========================================================================
# Per the design research: the flight hardware never emits JSON. It blasts
# the cheapest possible CSV and this host maps bytes onto named fields.
# -------------------------------------------------------------------------
def _finite(x, default=0.0):
    """Return x as a finite float, else `default`. Keeps NaN/Inf out of
    QPainter, numpy and the CSV."""
    try:
        v = float(x)
        return v if math.isfinite(v) else default
    except (TypeError, ValueError):
        return default


def parse_packet(line: str):
    """CSV telemetry line -> (values, present).

    `values`  every schema field; absent ones default to 0.0 purely so that
              downstream physics and logging never KeyError.
    `present` the fields that carried a real, non-empty, parsable cell.

    Display code MUST consult `present` before rendering. This is what
    separates "the sensor reported zero" from "the sensor is not there" —
    the GCS never invents a number for hardware that is not reporting.
    """
    out = OrderedDict((k, 0.0) for k in ALL_FIELDS)
    present = set()
    
    if not line or line.startswith(("//", "──", "══", "[RF]", "[TX]", "[RX]", "PKT #")):
        return out, present

    parts = line.strip().lstrip("$").rstrip("*").split(",")
    # §6.3 mandates TEAM_ID as CSV column 0. The schema starts at TIME_S, so
    # strip it — without this every field would be offset by one.
    if parts and parts[0].strip() == TEAM_ID:
        parts = parts[1:]

    for i, key in enumerate(ALL_FIELDS):
        if i >= len(parts):
            break                       # transmitter stopped early: rest absent
        cell = parts[i].strip()
        if cell == "":
            continue                    # explicit empty cell: absent, not zero
        if key == "UTC":
            out[key] = cell
            present.add(key)
            continue
        v = _finite(cell, None)
        if v is None:
            continue                    # unparsable: absent, never coerced to 0
        out[key] = v
        present.add(key)
    return out, present


def serialize_packet(d: dict, present=None) -> str:
    """Schema dict -> CSV line. Fields absent from `present` are written as
    empty cells so the log stays honest about what was actually received."""
    cells = []
    for k in ALL_FIELDS:
        if present is not None and k not in present:
            cells.append("")
            continue
        v = d.get(k, 0.0)
        if k == "UTC":
            cells.append(str(v) if v else "")
        else:
            cells.append(csv_cell(k, v))
    return ",".join(cells)


def export_dictionary(path: Path) -> Path:
    """Write the machine-readable telemetry dictionary.

    F´ auto-generates its ground system from a dictionary; ours is the
    SCHEMA above. Exporting it gives the PDR/FRR appendix a single
    authoritative artifact instead of a hand-maintained table.
    """
    doc = {
        "team_id": TEAM_ID, "version": APP_VERSION,
        "generated": datetime.datetime.now().isoformat(timespec="seconds"),
        "csv_column_0": "TEAM_ID",
        "downlink_fields": N_FIELDS,
        "fields": [], "derived": [],
    }
    for i, (k, m) in enumerate(SCHEMA.items()):
        doc["fields"].append({
            "index": i, "csv_column": i + 1, "name": k, "unit": m["u"],
            "group": m["g"], "description": m["d"], "limits": m["lim"],
            "persistence": m["pers"], "discrete": m["disc"],
        })
    for k, m in DERIVED.items():
        doc["derived"].append({
            "name": k, "unit": m["u"], "group": m["g"],
            "description": m["d"], "limits": m["lim"],
        })
    path.write_text(json.dumps(doc, indent=2), encoding="utf-8")
    return path


# =========================================================================
# §4  LIMITS + STALENESS ENGINE
# =========================================================================
# Two independent failure modes, one unified verdict per parameter:
#
#   1. VALUE   is the number outside its safe envelope?   (COSMOS model:
#              RED_LOW / YELLOW_LOW / YELLOW_HIGH / RED_HIGH, with a
#              persistence counter so one noisy packet cannot raise an alarm)
#   2. TIME    is the number still trustworthy?           (design research:
#              Nominal -> Warning -> Stale -> Alarm by age since last real
#              sample, per subsystem)
#
# A dashboard that shows the last received value forever is actively
# dangerous: the operator cannot tell a steady vehicle from a dead
# transmitter. Age therefore outranks value — a stale reading is reported as
# stale no matter how nominal it looks.
# -------------------------------------------------------------------------
ST_NODATA   = "NO_DATA"
ST_NOMINAL  = "NOMINAL"
ST_YEL_LOW  = "YELLOW_LOW"
ST_YEL_HIGH = "YELLOW_HIGH"
ST_RED_LOW  = "RED_LOW"
ST_RED_HIGH = "RED_HIGH"
ST_WARNING  = "WARNING"     # ageing, not yet stale
ST_STALE    = "STALE"
ST_LOST     = "LOST"

# Severity ordering drives the master alarm and the sort in the alarm list.
SEVERITY = {
    ST_NODATA: 0, ST_NOMINAL: 0,
    ST_WARNING: 1, ST_YEL_LOW: 2, ST_YEL_HIGH: 2, ST_STALE: 2,
    ST_RED_LOW: 3, ST_RED_HIGH: 3, ST_LOST: 3,
}
IS_ALARM = {ST_RED_LOW, ST_RED_HIGH, ST_LOST}
IS_WARN  = {ST_YEL_LOW, ST_YEL_HIGH, ST_STALE, ST_WARNING}

# Per-subsystem temporal thresholds, in seconds of age.
#   (warning, stale, lost)
# The design research tabulates these for an orbital vehicle sampling far
# faster than we do. Our downlink is 1 Hz, so a "< 1.0 s nominal" band is
# physically unreachable; the values below preserve the research's ratios
# and intent while staying achievable on a 1 Hz LoRa link.
STALE_CLASS = {
    "core":  (3.0,  6.0,  12.0),
    "imu":   (2.0,  4.0,   8.0),
    "power": (5.0, 10.0,  30.0),
    "gnss":  (5.0, 15.0,  30.0),
    "rf":    (3.0,  6.0,  15.0),
    "mech":  (5.0, 15.0,  30.0),
    "env":  (10.0, 30.0,  60.0),
    "spec": (10.0, 30.0,  60.0),
    "sys":  (10.0, 30.0,  60.0),
}


# Parameters for which an unchanging value is CORRECT, not a fault: commanded
# settings, properties of the launch site, held actuator positions and coarse
# integer indices. Everything else analog is expected to show sensor noise.
FROZEN_EXEMPT = {
    "FREQ_MHZ",        # commanded, not measured
    "GEOID_SEP",       # a property of the location
    "SRV1_DEG", "SRV2_DEG", "SRV3_DEG", "SRV4_DEG",   # hold commanded angle
    "SATS", "NAVIC_SATS",   # integer counts, plausibly steady for minutes
    "AQI",             # coarse 1..5 index
    "TIME_S", "PKT",   # monotonic counters
    "SD_PCT", "FLASH_PCT",
}


class ParamState:
    """Limit + staleness state for one parameter, with anti-chatter."""

    __slots__ = ("name", "meta", "state", "value", "last_seen",
                 "_cand", "_count", "ever_seen", "_recent")

    FROZEN_WINDOW = 40

    def __init__(self, name, meta):
        self.name = name
        self.meta = meta
        self.state = ST_NODATA
        self.value = None
        self.last_seen = None
        self._cand = ST_NODATA      # candidate state awaiting persistence
        self._count = 0
        self.ever_seen = False
        self._recent = deque(maxlen=self.FROZEN_WINDOW)

    def is_frozen(self, min_samples):
        """True when a live sensor keeps reporting one identical value.

        This is the failure mode neither of the other two engines can see: a
        locked-up sensor is still transmitting, so staleness never fires, and
        its frozen reading is usually inside limits, so the envelope never
        fires either. An operator would watch a dead sensor look healthy.
        """
        if self.meta.get("disc") or self.name in FROZEN_EXEMPT:
            return False
        # Measured sensors only. Derived values are computed from raw ones, so
        # a genuine freeze is already caught at its source — while several
        # derived values are constant BY DESIGN (APOGEE_M, PEAK_V and G_HI_PK
        # are latched running maxima; TTL_S and GLIDE_LD sit at zero whenever
        # the vehicle is not descending). Flagging those would cry wolf.
        if self.name not in SCHEMA:
            return False
        if len(self._recent) < min_samples:
            return False
        first = self._recent[0]
        if not isinstance(first, (int, float)):
            return False
        return all(v == first for v in self._recent)

    # -- value classification (COSMOS four-threshold model) ---------------
    def _classify(self, v):
        lim = self.meta.get("lim")
        if not lim or self.meta.get("disc") or not isinstance(v, (int, float)):
            return ST_NOMINAL
        rl, yl, yh, rh = lim
        if rl is not None and v < rl:
            return ST_RED_LOW
        if rh is not None and v > rh:
            return ST_RED_HIGH
        if yl is not None and v < yl:
            return ST_YEL_LOW
        if yh is not None and v > yh:
            return ST_YEL_HIGH
        return ST_NOMINAL

    def update_value(self, v, now):
        """Record a genuinely received sample."""
        self.value = v
        self.last_seen = now
        self.ever_seen = True
        self._recent.append(v)
        target = self._classify(v)
        need = max(1, int(self.meta.get("pers", 1)))
        if target == self._cand:
            self._count += 1
        else:
            self._cand, self._count = target, 1
        # Returning to nominal is immediate; entering an off-nominal state
        # must persist. Alarms should be hard to raise, easy to clear.
        if target == ST_NOMINAL or self._count >= need:
            self.state = target

    def evaluate_age(self, now):
        """Re-evaluate purely on elapsed time. Runs every UI tick, including
        ticks where no packet arrived — this is what catches a sensor that
        silently stops transmitting."""
        if not self.ever_seen or self.last_seen is None:
            self.state = ST_NODATA
            return self.state
        warn, stale, lost = STALE_CLASS.get(self.meta.get("g", "core"),
                                            STALE_CLASS["core"])
        age = now - self.last_seen
        if age > lost:
            self.state = ST_LOST
        elif age > stale:
            self.state = ST_STALE
        elif age > warn:
            # Ageing but still usable: only downgrade a nominal reading, never
            # mask a real limit breach that is still worth shouting about.
            if self.state not in IS_ALARM:
                self.state = ST_WARNING
        elif self.state in (ST_WARNING, ST_STALE, ST_LOST):
            # Age is back inside the nominal band, so the verdict reverts to
            # the value classification. Without this branch an age-driven
            # state could only ever escalate: the machine relied on
            # update_value() happening to run first, which made it
            # order-dependent and left stale ink on a recovered parameter.
            self.state = self._classify(self.value)
        return self.state

    @property
    def age(self):
        return None if self.last_seen is None else time.time() - self.last_seen


class LimitsEngine:
    """Owns a ParamState for every dictionary field and reports transitions."""

    def __init__(self):
        self.params = {k: ParamState(k, m) for k, m in FIELD_META.items()}
        self.events = deque(maxlen=2000)     # (t, severity, name, old, new)
        self._prev = {k: ST_NODATA for k in FIELD_META}
        self.master = ST_NOMINAL
        self.active = []                     # off-nominal params, worst first
        self.frozen = set()                  # live sensors stuck on one value
        self._prev_frozen = set()

    def ingest(self, values, present, derived, derived_present, now=None):
        now = now or time.time()
        for k in present:
            if k in self.params and not FIELD_META[k]["disc"]:
                self.params[k].update_value(_finite(values.get(k)), now)
            elif k in self.params:
                self.params[k].update_value(values.get(k), now)
        for k in derived_present:
            if k in self.params:
                self.params[k].update_value(_finite(derived.get(k)), now)

    def evaluate(self, now=None):
        """Age every parameter, collect transitions, recompute master alarm."""
        now = now or time.time()
        active = []
        worst = ST_NOMINAL
        for k, p in self.params.items():
            new = p.evaluate_age(now)
            old = self._prev[k]
            if new != old:
                self._prev[k] = new
                # A sensor simply starting to report is not an event. Logging
                # every NO_DATA -> NOMINAL buried the real transitions under
                # 100+ lines of startup noise on the first packet.
                if not (old == ST_NODATA and new == ST_NOMINAL):
                    self.events.append((now, SEVERITY.get(new, 0), k, old, new))
            if new in IS_ALARM or new in IS_WARN:
                active.append(p)
            if SEVERITY.get(new, 0) > SEVERITY.get(worst, 0):
                worst = new
        active.sort(key=lambda p: (-SEVERITY.get(p.state, 0), p.name))
        self.active = active

        # Frozen-sensor sweep. Reported alongside limits rather than folded
        # into a parameter's state, because "this reading is wrong" and "this
        # reading has stopped moving" are different things to tell an operator.
        self.frozen = {k for k, p in self.params.items()
                       if p.ever_seen and p.is_frozen(FROZEN_MIN_SAMPLES)}
        for k in self.frozen - self._prev_frozen:
            self.events.append((now, 2, k, "LIVE", "FROZEN"))
        for k in self._prev_frozen - self.frozen:
            self.events.append((now, 1, k, "FROZEN", "LIVE"))
        self._prev_frozen = set(self.frozen)
        if self.frozen and SEVERITY.get(worst, 0) < 2:
            worst = ST_STALE           # a stuck sensor is at least a caution

        self.master = worst
        return worst

    def state_of(self, name):
        p = self.params.get(name)
        return p.state if p else ST_NODATA

    def recent_events(self, n=200):
        return list(self.events)[-n:]


# How many consecutive identical samples before a live sensor is called stuck.
# At 1 Hz this is ~20 s of a perfectly unchanging analog reading, which no
# real noisy sensor produces.
FROZEN_MIN_SAMPLES = 20


def frozen_sensors(engine, min_samples=FROZEN_MIN_SAMPLES):
    """-> sorted names of live-but-stuck sensors in `engine`."""
    return sorted(k for k, p in engine.params.items()
                  if p.ever_seen and p.is_frozen(min_samples))


# =========================================================================
# §5  LoRa LINK BUDGET  —  Semtech AN1200.13 + WPC India compliance
# =========================================================================
def lora_symbol_time_s(sf: int, bw_hz: float) -> float:
    """T_sym = 2^SF / BW."""
    return (2.0 ** sf) / float(bw_hz)


def lora_ldro_required(sf: int, bw_hz: float) -> bool:
    """Low Data Rate Optimization is mandatory once a symbol exceeds 16 ms
    (SF11 and SF12 at BW125)."""
    return lora_symbol_time_s(sf, bw_hz) > 0.016


def lora_time_on_air(sf=9, bw_hz=125000, cr=1, payload_bytes=55,
                     preamble=8, explicit_header=True, crc=True, ldro=None):
    """Packet air time in seconds, per Semtech AN1200.13.

        T_sym       = 2^SF / BW
        T_preamble  = (n_preamble + 4.25) * T_sym
        n_payload   = 8 + max(ceil((8PL - 4SF + 28 + 16CRC - 20IH)
                                   / (4(SF - 2DE))) * (CR + 4), 0)
        T_packet    = T_preamble + n_payload * T_sym

    `cr` is 1..4 for coding rates 4/5..4/8. `ldro=None` auto-selects per the
    16 ms rule. Validated against the canonical LoRaWAN DR0 reference:
    SF12 / BW125 / CR4-5 / 51 B / explicit / CRC on  ->  2465.8 ms.
    """
    sf = int(max(6, min(12, sf)))
    cr = int(max(1, min(4, cr)))
    pl = int(max(0, payload_bytes))
    t_sym = lora_symbol_time_s(sf, bw_hz)
    de = (1 if lora_ldro_required(sf, bw_hz) else 0) if ldro is None else int(bool(ldro))
    ih = 0 if explicit_header else 1
    crc_b = 1 if crc else 0

    denom = 4 * (sf - 2 * de)
    if denom <= 0:
        return float("nan")
    num = 8 * pl - 4 * sf + 28 + 16 * crc_b - 20 * ih
    n_payload = 8 + max(math.ceil(num / denom) * (cr + 4), 0)

    t_preamble = (preamble + 4.25) * t_sym
    return t_preamble + n_payload * t_sym


def lora_bitrate_bps(sf=9, bw_hz=125000, cr=1) -> float:
    """Effective LoRa bit rate: SF * (BW / 2^SF) * (4 / (4 + CR))."""
    return sf * (bw_hz / (2.0 ** sf)) * (4.0 / (4.0 + cr))


def duty_cycle_budget(toa_s, limit=DUTY_CYCLE_LIMIT):
    """-> (max packets/hour, seconds of airtime allowed per hour)."""
    airtime_per_hour = 3600.0 * limit
    if toa_s <= 0:
        return 0, airtime_per_hour
    return int(airtime_per_hour // toa_s), airtime_per_hour


def free_space_path_loss_db(dist_m, freq_mhz):
    """FSPL(dB) = 20log10(d_km) + 20log10(f_MHz) + 32.44."""
    if dist_m <= 0:
        return 0.0
    return 20 * math.log10(dist_m / 1000.0) + 20 * math.log10(freq_mhz) + 32.44


def link_budget(tx_dbm=30.0, g_tx=2.0, g_rx=3.0, dist_m=1000.0,
                freq_mhz=LORA_FREQ_MHZ, misc_loss_db=3.0,
                sens_dbm=LORA_SENS_DBM):
    """-> (received power dBm, margin dB above receiver sensitivity)."""
    rx = tx_dbm + g_tx + g_rx - free_space_path_loss_db(dist_m, freq_mhz) - misc_loss_db
    return rx, rx - sens_dbm


def max_range_m(tx_dbm=30.0, g_tx=2.0, g_rx=3.0, freq_mhz=LORA_FREQ_MHZ,
                misc_loss_db=3.0, sens_dbm=LORA_SENS_DBM, margin_db=0.0):
    """Range at which the margin falls to `margin_db`, inverting FSPL.

    This is a FREE-SPACE figure and therefore a theoretical upper bound only:
    it models no terrain, no foliage, no ground reflection and no horizon.
    Quote it as a ceiling, never as an expected operational range — compare
    it against radio_horizon_m(), which is usually the binding constraint.
    """
    budget = tx_dbm + g_tx + g_rx - misc_loss_db - sens_dbm - margin_db
    if budget <= 0:
        return 0.0
    return 1000.0 * 10 ** ((budget - 20 * math.log10(freq_mhz) - 32.44) / 20.0)


def radio_horizon_m(h_tx_m, h_rx_m=2.0):
    """Line-of-sight horizon for two antennas, including the 4/3-earth
    refraction allowance:  d_km ~= 4.12 * (sqrt(h_tx) + sqrt(h_rx)), h in m.

    For a CanSat this is normally what actually limits the link, long before
    the free-space power budget runs out.
    """
    h_tx = max(0.0, h_tx_m)
    h_rx = max(0.0, h_rx_m)
    return 4120.0 * (math.sqrt(h_tx) + math.sqrt(h_rx))


def dbm_to_watts(dbm):
    return 10 ** ((dbm - 30.0) / 10.0)


def watts_to_dbm(w):
    return 10 * math.log10(w * 1000.0) if w > 0 else float("-inf")


def wpc_compliance(tx_dbm, antenna_gain_dbi, bw_hz, freq_mhz):
    """Check a configuration against the WPC licence-exempt SRD limits.

    Returns a list of (label, value_text, verdict) where verdict is
    'PASS', 'FAIL' or 'NOTE'. ERP is referenced to a half-wave dipole, so
    ERP(dBm) = Ptx(dBm) + Gain(dBi) - 2.15.
    """
    rows = []
    tx_w = dbm_to_watts(tx_dbm)
    rows.append(("TX POWER", f"{tx_w:.3f} W  ({tx_dbm:.1f} dBm)",
                 "PASS" if tx_w <= WPC_MAX_TX_W + 1e-9 else "FAIL"))

    erp_dbm = tx_dbm + antenna_gain_dbi - 2.15
    erp_w = dbm_to_watts(erp_dbm)
    rows.append(("ERP", f"{erp_w:.3f} W  ({erp_dbm:.1f} dBm)",
                 "PASS" if erp_w <= WPC_MAX_ERP_W + 1e-9 else "FAIL"))

    bw_khz = bw_hz / 1000.0
    rows.append(("CARRIER BW", f"{bw_khz:.1f} kHz",
                 "PASS" if bw_khz <= WPC_MAX_BW_KHZ + 1e-9 else "FAIL"))

    lo = freq_mhz - bw_khz / 2000.0
    hi = freq_mhz + bw_khz / 2000.0
    if lo >= WPC_ETA_LO_MHZ and hi <= WPC_ETA_HI_MHZ:
        band = "PASS"
        txt = f"{lo:.3f} – {hi:.3f} MHz  (ETA sub-band)"
    elif lo >= WPC_BAND_LO_MHZ and hi <= WPC_BAND_HI_MHZ:
        band = "NOTE"
        txt = f"{lo:.3f} – {hi:.3f} MHz  (above 867 MHz ETA sub-band)"
    else:
        band = "FAIL"
        txt = f"{lo:.3f} – {hi:.3f} MHz  (outside 865–868 MHz)"
    rows.append(("OCCUPIED BAND", txt, band))
    return rows


# =========================================================================
# §5b  PRECISE POSITIONING  —  NavIC / GNSS, quality, survey and DGNSS
# =========================================================================
# The flight receiver is a Quectel L89: dual-band, with an integrated patch
# antenna for L1 and a separate chip antenna for IRNSS/NavIC L5, so it can
# track GPS + Galileo + QZSS + NavIC concurrently and apply GAGAN (SBAS)
# corrections.
#
# Accuracy ladder this module implements, worst to best:
#   raw SPS, single frequency      5 - 10 m
#   dual-band L1+L5 (iono removed) 3 -  5 m
#   GAGAN / SBAS corrected         1 -  3 m
#   local differential (DGNSS)     1 -  2 m
#
# The last one is the reason a ground-station receiver is worth having:
# GNSS error is spatially correlated, so two receivers a few km apart see
# almost the same ionospheric, satellite-clock and ephemeris error. Measuring
# that error at a known point and subtracting it from the CanSat's fix
# cancels the common mode. Degradation is only a few mm per km of baseline.
# -------------------------------------------------------------------------
FIX_QUALITY_LABEL = {
    0: "NO FIX", 1: "GNSS SPS", 2: "DGPS / SBAS", 3: "PPS",
    4: "RTK FIXED", 5: "RTK FLOAT", 6: "DEAD RECKONING",
    7: "MANUAL", 8: "SIMULATION",
}

# User Equivalent Range Error, 1 sigma metres, by fix type. Horizontal error
# is then HDOP * UERE — the standard GNSS error-budget relation.
UERE_BY_FIX = {
    0: 30.0,     # no fix: meaningless, kept so the table is total
    1: 5.0,      # single-frequency SPS
    2: 1.5,      # SBAS / GAGAN corrected
    3: 3.0,
    4: 0.02,     # RTK fixed
    5: 0.5,      # RTK float
    6: 15.0,     # dead reckoning
}
UERE_DUAL_BAND = 2.8     # L1+L5 measures ionospheric delay instead of modelling it

GNSS_SYS_BITS = [(1, "GPS"), (2, "GLONASS"), (4, "GALILEO"),
                 (8, "BEIDOU"), (16, "QZSS"), (32, "NAVIC")]

# 1-sigma -> containment-radius scale factors for a 2-D circular normal error
CEP50_FROM_SIGMA = 1.1774      # 50 % circular error probable
R95_FROM_SIGMA   = 2.4477      # 95 % containment


def decode_constellations(mask):
    """Bitmask -> list of constellation names present in the solution."""
    try:
        m = int(mask)
    except (TypeError, ValueError):
        return []
    return [name for bit, name in GNSS_SYS_BITS if m & bit]


def horizontal_sigma_m(hdop, fix_quality=1, dual_band=False):
    """1-sigma horizontal position error = HDOP x UERE.

    Returns None when there is no usable input, so callers render "—" rather
    than a fabricated precision figure.
    """
    if hdop is None or hdop <= 0:
        return None
    try:
        fq = int(fix_quality)
    except (TypeError, ValueError):
        fq = 1
    if fq == 0:
        return None
    uere = UERE_BY_FIX.get(fq, 5.0)
    if dual_band and fq in (1, 3):
        # Dual-frequency removes the ionospheric term from the budget; it does
        # not help once SBAS or RTK is already correcting it.
        uere = min(uere, UERE_DUAL_BAND)
    return hdop * uere


def cep50_m(sigma):
    return None if sigma is None else sigma * CEP50_FROM_SIGMA


def r95_m(sigma):
    return None if sigma is None else sigma * R95_FROM_SIGMA


# ---- coordinate presentation -------------------------------------------
def to_dms(value, is_lat):
    """Decimal degrees -> degrees/minutes/seconds, the format search-and-
    rescue and most handheld GPS units expect."""
    hemi = ("N" if value >= 0 else "S") if is_lat else ("E" if value >= 0 else "W")
    v = abs(value)
    d = int(v)
    m_f = (v - d) * 60.0
    m = int(m_f)
    s = (m_f - m) * 60.0
    return f"{d:d}° {m:02d}' {s:05.2f}\" {hemi}"


def to_dmm(value, is_lat):
    """Degrees + decimal minutes — what aviation and most GNSS receivers show."""
    hemi = ("N" if value >= 0 else "S") if is_lat else ("E" if value >= 0 else "W")
    v = abs(value)
    d = int(v)
    m = (v - d) * 60.0
    return f"{d:d}° {m:06.3f}' {hemi}"


def to_utm(lat, lon):
    """WGS-84 geodetic -> UTM (zone, hemisphere, easting, northing) in metres.

    Standard Karney/USGS series expansion. Metres on a local grid are far
    easier to search on foot than decimal degrees.
    """
    a = 6378137.0
    f = 1 / 298.257223563
    k0 = 0.9996
    e2 = f * (2 - f)
    ep2 = e2 / (1 - e2)

    zone = int((lon + 180) / 6) + 1
    lon0 = math.radians((zone - 1) * 6 - 180 + 3)
    phi = math.radians(lat)
    lam = math.radians(lon)

    N = a / math.sqrt(1 - e2 * math.sin(phi) ** 2)
    T = math.tan(phi) ** 2
    C = ep2 * math.cos(phi) ** 2
    A = math.cos(phi) * (lam - lon0)

    M = a * ((1 - e2 / 4 - 3 * e2 ** 2 / 64 - 5 * e2 ** 3 / 256) * phi
             - (3 * e2 / 8 + 3 * e2 ** 2 / 32 + 45 * e2 ** 3 / 1024) * math.sin(2 * phi)
             + (15 * e2 ** 2 / 256 + 45 * e2 ** 3 / 1024) * math.sin(4 * phi)
             - (35 * e2 ** 3 / 3072) * math.sin(6 * phi))

    easting = k0 * N * (A + (1 - T + C) * A ** 3 / 6
                        + (5 - 18 * T + T ** 2 + 72 * C - 58 * ep2) * A ** 5 / 120) + 500000.0
    northing = k0 * (M + N * math.tan(phi)
                     * (A ** 2 / 2 + (5 - T + 9 * C + 4 * C ** 2) * A ** 4 / 24
                        + (61 - 58 * T + T ** 2 + 600 * C - 330 * ep2) * A ** 6 / 720))
    hemi = "N"
    if lat < 0:
        northing += 10000000.0
        hemi = "S"
    return zone, hemi, easting, northing


def offset_metres(lat_ref, lon_ref, lat, lon):
    """Local tangent-plane offset (east, north) in metres from a reference.

    Used for the DGNSS error vector and for short-range recovery guidance,
    where a flat-earth approximation is exact to well under a millimetre.
    """
    m_per_deg_lat = 111132.92 - 559.82 * math.cos(2 * math.radians(lat_ref))
    m_per_deg_lon = 111412.84 * math.cos(math.radians(lat_ref)) \
        - 93.5 * math.cos(3 * math.radians(lat_ref))
    return ((lon - lon_ref) * m_per_deg_lon, (lat - lat_ref) * m_per_deg_lat)


def apply_offset_metres(lat_ref, lon_ref, d_east, d_north):
    """Inverse of offset_metres: shift a coordinate by a metre offset."""
    m_per_deg_lat = 111132.92 - 559.82 * math.cos(2 * math.radians(lat_ref))
    m_per_deg_lon = 111412.84 * math.cos(math.radians(lat_ref)) \
        - 93.5 * math.cos(3 * math.radians(lat_ref))
    return (lat_ref + d_north / m_per_deg_lat,
            lon_ref + d_east / m_per_deg_lon)


class PositionSurvey:
    """Self-survey: average fixes from a receiver known to be stationary.

    Averaging beats down the RANDOM component as 1/sqrt(N). It does not
    remove the slowly-varying correlated bias (ionosphere, ephemeris), so the
    reported confidence is deliberately floored — claiming centimetres from a
    long average of an uncorrected SPS receiver would be a lie.

    This is the same idea as a u-blox "Survey-In": accumulate until the
    estimated accuracy crosses a threshold, then freeze the reference.
    """

    BIAS_FLOOR_M = 1.2      # correlated error that averaging cannot remove

    def __init__(self, floor_m=None):
        self.reset()
        self.floor = self.BIAS_FLOOR_M if floor_m is None else floor_m

    def reset(self):
        self.n = 0
        self._lat = self._lon = self._alt = 0.0
        self._lat2 = self._lon2 = 0.0
        self.fixed = False
        self.result = None            # (lat, lon, alt, accuracy_m, n)

    def add(self, lat, lon, alt=0.0):
        if lat == 0 and lon == 0:
            return
        self.n += 1
        self._lat += lat
        self._lon += lon
        self._alt += alt
        self._lat2 += lat * lat
        self._lon2 += lon * lon

    @property
    def mean(self):
        if self.n == 0:
            return None
        return (self._lat / self.n, self._lon / self.n, self._alt / self.n)

    def accuracy_m(self):
        """1-sigma accuracy of the averaged position, floored at the bias."""
        if self.n < 2:
            return None
        mlat = self._lat / self.n
        mlon = self._lon / self.n
        var_lat = max(0.0, self._lat2 / self.n - mlat * mlat)
        var_lon = max(0.0, self._lon2 / self.n - mlon * mlon)
        e, nth = offset_metres(mlat, mlon,
                              mlat + math.sqrt(var_lat), mlon + math.sqrt(var_lon))
        sigma_sample = math.hypot(e, nth)
        return max(self.floor, sigma_sample / math.sqrt(self.n))

    def freeze(self):
        """Lock the averaged position in as the survey result."""
        m = self.mean
        if not m:
            return None
        self.result = (m[0], m[1], m[2], self.accuracy_m() or self.floor, self.n)
        self.fixed = True
        return self.result


class LandingPredictor:
    """Projects the touchdown point, and the area the recovery team must search.

    Under a parachute the vehicle has no lateral propulsion, so its horizontal
    ground velocity IS the wind velocity at that altitude. That means the wind
    can be estimated from the vehicle's own drift for free — no anemometer, no
    forecast — and the remaining descent time carries it to a landing estimate.

    Everything returns None when the inputs do not support a real answer,
    because a confident-looking landing pin computed from nothing is worse
    than no pin at all.
    """

    WIND_WINDOW = 12          # fixes averaged for the wind estimate
    MIN_SINK_MS = 0.4         # below this it is not meaningfully descending

    def __init__(self):
        self.reset()

    def reset(self):
        self._hist = deque(maxlen=self.WIND_WINDOW)   # (t, lat, lon)
        self.wind_e = 0.0
        self.wind_n = 0.0
        self.valid = False

    def update(self, t, lat, lon):
        """Feed a GNSS fix taken while the vehicle is descending."""
        if lat == 0 and lon == 0:
            return
        self._hist.append((t, lat, lon))
        if len(self._hist) < 3:
            self.valid = False
            return
        t0, la0, lo0 = self._hist[0]
        t1, la1, lo1 = self._hist[-1]
        dt = t1 - t0
        if dt <= 0:
            self.valid = False
            return
        e, n = offset_metres(la0, lo0, la1, lo1)
        self.wind_e, self.wind_n = e / dt, n / dt
        self.valid = True

    @property
    def wind_speed(self):
        return math.hypot(self.wind_e, self.wind_n)

    @property
    def wind_bearing_to(self):
        """Compass bearing the air mass is carrying the vehicle TOWARD."""
        if not self.valid:
            return None
        return (math.degrees(math.atan2(self.wind_e, self.wind_n)) + 360.0) % 360.0

    @property
    def wind_bearing_from(self):
        """Meteorological convention: where the wind is blowing FROM."""
        b = self.wind_bearing_to
        return None if b is None else (b + 180.0) % 360.0

    def time_to_land(self, alt_agl, sink_rate):
        if alt_agl is None or sink_rate is None or sink_rate < self.MIN_SINK_MS:
            return None
        return max(0.0, alt_agl) / sink_rate

    def project(self, lat, lon, alt_agl, sink_rate):
        """-> (landing_lat, landing_lon, ttl_s, drift_m) or None."""
        ttl = self.time_to_land(alt_agl, sink_rate)
        if ttl is None or not self.valid:
            return None
        d_e = self.wind_e * ttl
        d_n = self.wind_n * ttl
        la, lo = apply_offset_metres(lat, lon, d_e, d_n)
        return la, lo, ttl, math.hypot(d_e, d_n)


_COMPASS_POINTS = ["N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE",
                   "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"]


def _compass_point(bearing):
    """Bearing -> 16-point compass name. A recovery team walking a field
    follows 'NE' far more reliably than '048 degrees'."""
    if bearing is None:
        return "—"
    return _COMPASS_POINTS[int((bearing % 360.0) / 22.5 + 0.5) % 16]


def search_radius_m(sigma_m, time_since_fix_s=0.0, drift_speed_ms=0.0,
                    remaining_drift_m=0.0):
    """How big a circle the recovery team actually has to sweep.

    Three independent contributions, combined in quadrature because they are
    uncorrelated: the fix's own 95 % uncertainty, however far the vehicle
    drifted in the silence after the last packet, and any descent still to
    come. Quadrature rather than a sum avoids a wildly pessimistic radius.
    """
    r_fix = r95_m(sigma_m) if sigma_m else 10.0
    r_silence = max(0.0, time_since_fix_s) * max(0.0, drift_speed_ms)
    r_remaining = max(0.0, remaining_drift_m)
    return math.sqrt(r_fix ** 2 + r_silence ** 2 + r_remaining ** 2)


class DifferentialCorrector:
    """Short-baseline code DGNSS.

    A receiver at a surveyed point measures its own position; the difference
    from truth is the common-mode error. Over a few km the CanSat sees
    essentially the same error, so subtracting it removes most of the budget.

    The correction is only trustworthy while the base observation is fresh —
    the error vector drifts — so it expires. An expired correction is
    reported as unavailable rather than silently applied.
    """

    MAX_AGE_S = 30.0
    MAX_BASELINE_M = 50000.0     # beyond this the common-mode assumption fails

    def __init__(self):
        self.truth = None            # (lat, lon, alt) surveyed base position
        self.observed = None         # (lat, lon, alt) live base measurement
        self.obs_time = None
        self.d_east = 0.0
        self.d_north = 0.0
        self.enabled = True

    def set_truth(self, lat, lon, alt=0.0):
        self.truth = (lat, lon, alt)

    def update_base(self, lat, lon, alt=0.0, now=None):
        """Feed a live measurement from the ground-station receiver."""
        self.observed = (lat, lon, alt)
        self.obs_time = now or time.time()
        if self.truth:
            self.d_east, self.d_north = offset_metres(
                self.truth[0], self.truth[1], lat, lon)

    def age(self, now=None):
        if self.obs_time is None:
            return None
        return (now or time.time()) - self.obs_time

    def available(self, now=None):
        if not (self.enabled and self.truth and self.observed):
            return False
        a = self.age(now)
        return a is not None and a <= self.MAX_AGE_S

    def error_magnitude_m(self):
        return math.hypot(self.d_east, self.d_north)

    def correct(self, lat, lon, now=None):
        """-> (corrected_lat, corrected_lon, applied)."""
        if not self.available(now):
            return lat, lon, False
        if lat == 0 and lon == 0:
            return lat, lon, False
        if self.truth:
            e, n = offset_metres(self.truth[0], self.truth[1], lat, lon)
            if math.hypot(e, n) > self.MAX_BASELINE_M:
                return lat, lon, False
        clat, clon = apply_offset_metres(lat, lon, -self.d_east, -self.d_north)
        return clat, clon, True


def parse_nmea_sentence(line, state=None):
    """Minimal NMEA 0183 reader for a ground-station receiver.

    Handles GGA (position, fix quality, satellites, HDOP, geoid separation)
    and GSA (DOP set), across all talker IDs so a multi-constellation
    receiver's GN/GP/GI sentences are all accepted. Returns an updated state
    dict, or None if the sentence is not one we use or fails checksum.
    """
    st = dict(state) if state else {}
    if not line or not line.startswith("$"):
        return None
    body = line[1:]
    if "*" in body:
        body, _, csum = body.partition("*")
        try:
            want = int(csum[:2], 16)
        except ValueError:
            return None
        got = 0
        for ch in body:
            got ^= ord(ch)
        if got != want:
            return None                      # corrupt sentence, drop it
    parts = body.split(",")
    if not parts:
        return None
    talker_type = parts[0][2:] if len(parts[0]) >= 5 else parts[0]

    def _coord(val, hemi):
        if not val or not hemi:
            return None
        try:
            v = float(val)
        except ValueError:
            return None
        deg = int(v // 100)
        minutes = v - deg * 100
        dec = deg + minutes / 60.0
        return -dec if hemi in ("S", "W") else dec

    if talker_type == "GGA" and len(parts) >= 15:
        lat = _coord(parts[2], parts[3])
        lon = _coord(parts[4], parts[5])
        if lat is None or lon is None:
            return None
        st["lat"] = lat
        st["lon"] = lon
        st["fix_quality"] = int(parts[6]) if parts[6].isdigit() else 0
        st["sats"] = int(parts[7]) if parts[7].isdigit() else 0
        st["hdop"] = _finite(parts[8], None)
        st["alt"] = _finite(parts[9], None)
        st["geoid"] = _finite(parts[11], None)
        st["t"] = time.time()
        return st
    if talker_type == "GSA" and len(parts) >= 18:
        st["pdop"] = _finite(parts[15], None)
        st["hdop"] = _finite(parts[16], None) or st.get("hdop")
        st["vdop"] = _finite(parts[17].split("*")[0], None)
        return st
    return None


class GsGnssLink(threading.Thread):
    """Reads NMEA from a GNSS receiver attached to the ground station.

    Entirely optional. Its only job is to observe the base position so the
    DifferentialCorrector can measure the common-mode error; when it is not
    running everything else degrades to uncorrected positions and says so.
    """

    def __init__(self, port, baud, on_fix, status_cb=None):
        super().__init__(daemon=True, name="GsGnssLink")
        self.port, self.baud = port, baud
        self.on_fix = on_fix
        self.status_cb = status_cb or (lambda s: None)
        self._stop_evt = threading.Event()
        self._ser = None
        self._buf = bytearray()
        self.state = {}

    def stop(self):
        self._stop_evt.set()
        try:
            if self._ser:
                self._ser.close()
        except Exception:
            pass

    def run(self):
        backoff = 0
        while not self._stop_evt.is_set():
            if not HAVE_SERIAL:
                self.status_cb("NO_SERIAL")
                return
            try:
                self._ser = serial.Serial(self.port, self.baud, timeout=0.4)
                self.status_cb("LIVE")
                backoff = 0
            except Exception as e:
                log.warning("GS GNSS open failed: %s", e)
                self.status_cb("RECONNECT")
                self._stop_evt.wait(RECONNECT_BACKOFF[min(backoff, 3)])
                backoff += 1
                continue
            try:
                while not self._stop_evt.is_set():
                    try:
                        chunk = self._ser.read(256)
                    except Exception:
                        break
                    if not chunk:
                        continue
                    self._buf.extend(chunk)
                    if len(self._buf) > 32768:
                        del self._buf[:16384]
                    while b"\n" in self._buf:
                        i = self._buf.index(b"\n")
                        line = bytes(self._buf[:i]).decode("ascii", "ignore").strip()
                        del self._buf[:i + 1]
                        upd = parse_nmea_sentence(line, self.state)
                        if upd:
                            self.state = upd
                            if "lat" in upd and upd.get("fix_quality", 0) > 0:
                                try:
                                    self.on_fix(dict(upd))
                                except Exception:
                                    log.exception("GS GNSS callback failed")
            finally:
                try:
                    if self._ser:
                        self._ser.close()
                except Exception:
                    pass
                self._ser = None
                self.status_cb("RECONNECT")


# =========================================================================
# §6  PHYSICS  —  derived engineering parameters
# =========================================================================
class Physics:
    """Every derived parameter, from real formulas. compute() never raises.

    Each result is gated on its inputs: a value only enters
    `derived_present` if every field it was computed from was itself
    received. Without this the GCS would happily display a confident-looking
    ROLL or DENS_ALT synthesised from sensors that are not connected.
    """

    def __init__(self):
        self.reset()

    def reset(self):
        self.t_last = None
        self.alt_last = None
        self.apogee_m = 0.0
        self.takeoff_alt = None
        self.peak_speed = 0.0
        self.peak_g_hi = 0.0
        self.energy_wh = 0.0
        # ---- pad origin -------------------------------------------------
        # lat0/lon0 are the (0, 0) of every range and bearing the GCS shows.
        # They used to be assigned from the very FIRST fix that arrived, which
        # is the worst possible choice on two counts:
        #
        #   1. A cold-start fix is the least accurate one the receiver will
        #      ever produce. The solution is still converging, HDOP is high and
        #      the position can be tens of metres out. Every range for the rest
        #      of the flight was then measured from that error.
        #   2. If the first fix only arrived AFTER launch — indoors on the pad,
        #      under a fairing, or simply a slow lock — the "pad" was silently
        #      placed wherever the vehicle happened to be when it got lock,
        #      which could be hundreds of metres downrange and up.
        #
        # Instead the origin is SURVEYED: while the vehicle reports STATE=BOOT
        # (still on the pad) every fix is folded into a running mean, so the
        # origin keeps improving as the solution settles. It freezes the moment
        # the vehicle leaves BOOT, which is the last instant it is still
        # physically on the pad. PAD_SURVEY_MAX caps the average so a long
        # pre-flight hold cannot make the origin unresponsive to a genuine
        # receiver correction.
        self.lat0 = None
        self.lon0 = None
        self.pad_alt0 = None          # baro AGL at the pad, for AGL_M
        self._pad_n = 0               # fixes folded into the survey so far
        self._pad_frozen = False      # True once the vehicle has left BOOT
        self.lightning_log = []

    # ---- atmosphere -----------------------------------------------------
    @staticmethod
    def isa_density(p_pa, t_c):
        t_k = t_c + 273.15
        return p_pa / (R_AIR * t_k) if t_k > 1 else 0.0

    @staticmethod
    def isa_pressure_alt(p_pa):
        if p_pa <= 0:
            return 0.0
        return (SEA_LEVEL_T_K / TROPO_LAPSE_K_M) * (
            1.0 - (p_pa / SEA_LEVEL_P_PA) ** (R_AIR * TROPO_LAPSE_K_M / G_EARTH))

    @staticmethod
    def density_alt(p_pa, t_c):
        rho = Physics.isa_density(p_pa, t_c)
        if rho <= 0:
            return 0.0
        rho0 = SEA_LEVEL_P_PA / (R_AIR * SEA_LEVEL_T_K)
        return (SEA_LEVEL_T_K / TROPO_LAPSE_K_M) * (
            1.0 - (rho / rho0) ** (1.0 / (G_EARTH / (R_AIR * TROPO_LAPSE_K_M) - 1.0)))

    @staticmethod
    def dew_point(t_c, rh):
        """Magnus approximation."""
        if rh <= 0:
            return 0.0
        a, b = 17.62, 243.12
        g = math.log(max(rh, 1e-3) / 100.0) + (a * t_c) / (b + t_c)
        return (b * g) / (a - g)

    @staticmethod
    def cloud_base_agl(t_c, td_c):
        """Espy's equation."""
        return max(0.0, 125.0 * (t_c - td_c))

    # ---- navigation -----------------------------------------------------
    @staticmethod
    def hav_dist(lat1, lon1, lat2, lon2):
        r = 6371000.0
        p1, p2 = math.radians(lat1), math.radians(lat2)
        dp = math.radians(lat2 - lat1)
        dl = math.radians(lon2 - lon1)
        a = math.sin(dp / 2) ** 2 + math.cos(p1) * math.cos(p2) * math.sin(dl / 2) ** 2
        return 2 * r * math.asin(min(1.0, math.sqrt(a)))

    @staticmethod
    def bearing(lat1, lon1, lat2, lon2):
        p1, p2 = math.radians(lat1), math.radians(lat2)
        dl = math.radians(lon2 - lon1)
        y = math.sin(dl) * math.cos(p2)
        x = math.cos(p1) * math.sin(p2) - math.sin(p1) * math.cos(p2) * math.cos(dl)
        return (math.degrees(math.atan2(y, x)) + 360.0) % 360.0

    @staticmethod
    def aqi_from_pm25(pm):
        """US EPA breakpoint interpolation."""
        bp = [(0, 12, 0, 50), (12.1, 35.4, 51, 100), (35.5, 55.4, 101, 150),
              (55.5, 150.4, 151, 200), (150.5, 250.4, 201, 300),
              (250.5, 500.4, 301, 500)]
        for lo, hi, ilo, ihi in bp:
            if lo <= pm <= hi:
                return ilo + (ihi - ilo) * (pm - lo) / (hi - lo)
        return 500.0 if pm > 500.4 else 0.0

    # ---- main -----------------------------------------------------------
    def compute(self, d: dict, packet_count: int = 0, present=None):
        present = present if present is not None else set(ALL_FIELDS)
        r, dp = {}, set()

        def have(*keys):
            return all(k in present for k in keys)

        try:
            t_s  = _finite(d.get("TIME_S"))
            alt  = _finite(d.get("ALT_M"))
            pres = _finite(d.get("PRES_PA"))
            temp = _finite(d.get("TEMP_C"))
            hum  = _finite(d.get("HUM_PCT"))

            # 1. Extract Raw Variables
            # ONE accelerometer source: the mandated ACC_X/Y/Z in m/s^2,
            # converted to g here and published back as AX/AY/AZ for display.
            # There used to be an `else` branch reading AX/AY/AZ directly, which
            # could never execute — the gateway synthesised AX from ACC_X, so
            # AX was present exactly when ACC_X was and absent exactly when it
            # was not. A fallback that cannot fire hides, rather than handles,
            # the case it claims to cover.
            ax_raw = _finite(d.get("ACC_X")) / G_EARTH
            ay_raw = _finite(d.get("ACC_Y")) / G_EARTH
            az_raw = _finite(d.get("ACC_Z")) / G_EARTH
            accel_ok = have("ACC_X", "ACC_Y", "ACC_Z")

            spin_r = _finite(d.get("SPIN_RATE"))
            gx_raw, gy_raw = _finite(d.get("GX")), _finite(d.get("GY"))
            gz_raw = _finite(d.get("GZ")) or spin_r
            mx_raw, my_raw, mz_raw = (_finite(d.get("MX")), _finite(d.get("MY")),
                                      _finite(d.get("MZ")))

            ax, ay, az = ax_raw, ay_raw, az_raw
            gx, gy, gz = gx_raw, gy_raw, gz_raw
            mx, my, mz = mx_raw, my_raw, mz_raw

            # Publish the g-unit view the INERTIAL panel reads. Derived, not
            # received: the wire carries ACC_X/Y/Z once, in the mandated unit.
            r["AX"], r["AY"], r["AZ"] = ax, ay, az
            if accel_ok:
                dp.update(["AX", "AY", "AZ"])

            mag_ok = have("MX", "MY", "MZ")
            bus_v, bus_i = _finite(d.get("BUS_V")), _finite(d.get("BUS_I_MA"))

            # ---- vertical speed --------------------------------------
            if (self.t_last is not None and t_s > self.t_last
                    and have("ALT_M", "TIME_S")):
                dt = t_s - self.t_last
                vsp = (alt - self.alt_last) / dt if self.alt_last is not None else 0.0
                vsp_ok = True
            else:
                dt, vsp, vsp_ok = 0.0, 0.0, False
            r["VSPEED"], r["DESCENT"] = vsp, (-vsp if vsp < 0 else 0.0)
            if vsp_ok:
                dp.update(["VSPEED", "DESCENT"])

            # ---- ABSOLUTE EULER MATH FIX -----------------------------
            # Bypasses the flight computer's cross-coupled filter.
            # Physical Board: X=Right, Y=Forward, Z=Up
            roll = math.degrees(math.atan2(-ax, az))
            pitch = math.degrees(math.atan2(ay, math.sqrt(ax * ax + az * az)))
            
            r["ROLL"], r["PITCH"] = roll, pitch
            a_mag = math.sqrt(ax * ax + ay * ay + az * az) or 1.0
            r["TILT"] = math.degrees(math.acos(max(-1.0, min(1.0, az / a_mag))))
            
            if accel_ok:
                dp.update(["ROLL", "PITCH", "TILT"])

            # ---- tilt-compensated heading ----------------------------
            # Map physical (Right, Forward, Up) to NED (Forward, Right, Down)
            mx_ned, my_ned, mz_ned = my, mx, -mz
            cr, sr = math.cos(math.radians(roll)), math.sin(math.radians(roll))
            cp, sp = math.cos(math.radians(pitch)), math.sin(math.radians(pitch))
            
            mxh = mx_ned * cp + mz_ned * sp
            myh = mx_ned * sr * sp + my_ned * cr - mz_ned * sr * cp
            hdg = (math.degrees(math.atan2(-myh, mxh)) + 360.0) % 360.0
            r["HEADING"] = hdg
            r["YAW"] = hdg
            
            if mag_ok and accel_ok:
                dp.update(["HEADING", "YAW"])

            # ---- spin / stability -------------------------------------
            spin_rate = spin_r if "SPIN_RATE" in present else gz
            spin_ok = ("SPIN_RATE" in present) or ("GZ" in present)
            omega_t = math.sqrt(gx * gx + gy * gy)
            instab = min(1.0, omega_t / 90.0)
            r["OMEGA_T"], r["SPIN_DPS"] = omega_t, spin_rate
            r["INSTAB"], r["STABLE"] = instab, 1.0 - instab
            r["H_BODY"] = I_T * math.radians(omega_t) * 1000.0
            r["NUTATION"] = math.degrees(
                abs(I_Z - I_T) / I_T * math.radians(abs(spin_rate)))
            r["CONING"] = omega_t
            if have("GX", "GY"):
                dp.update(["OMEGA_T", "INSTAB", "STABLE", "H_BODY", "CONING"])
            if spin_ok:
                dp.update(["SPIN_DPS", "NUTATION"])

            # ---- atmosphere ------------------------------------------
            rho = self.isa_density(pres, temp)
            r["AIR_RHO"] = rho
            r["P_ALT"] = self.isa_pressure_alt(pres)
            r["DENS_ALT"] = self.density_alt(pres, temp)
            r["DEW_POINT"] = self.dew_point(temp, hum)
            r["CLOUD_BASE"] = self.cloud_base_agl(temp, r["DEW_POINT"])
            if have("PRES_PA", "TEMP_C"):
                dp.update(["AIR_RHO", "P_ALT", "DENS_ALT"])
            if have("TEMP_C", "HUM_PCT"):
                dp.update(["DEW_POINT", "CLOUD_BASE"])

            # ---- speed / aerodynamics --------------------------------
            gnd = _finite(d.get("GND_SPEED"))
            v3d = math.sqrt(gnd * gnd + vsp * vsp)
            r["SPEED_3D"] = v3d
            self.peak_speed = max(self.peak_speed, v3d)
            r["PEAK_V"] = self.peak_speed
            speed_ok = ("GND_SPEED" in present) or vsp_ok
            if speed_ok:
                dp.update(["SPEED_3D", "PEAK_V"])

            sos = (math.sqrt(GAMMA_AIR * R_AIR * (temp + 273.15))
                   if temp > -273 else 340.0)
            r["DYN_Q"] = 0.5 * rho * v3d * v3d
            r["MACH"] = v3d / sos if sos > 0 else 0.0
            r["REYNOLDS"] = (rho * v3d * CANSAT_LEN_M) / MU_AIR_REF
            r["SOS"] = sos
            if speed_ok and have("PRES_PA", "TEMP_C"):
                dp.update(["DYN_Q", "MACH", "REYNOLDS", "SOS"])

            sink = max(0.0, -vsp)
            r["SINK_RATE"] = sink
            r["GLIDE_LD"] = (gnd / sink) if sink > 0.1 else 0.0
            r["GLIDE_SLOPE"] = (-math.degrees(math.atan2(sink, max(gnd, 0.01)))
                                if sink > 0.1 else 0.0)
            if vsp_ok:
                dp.add("SINK_RATE")
                if "GND_SPEED" in present:
                    dp.update(["GLIDE_LD", "GLIDE_SLOPE"])

            # ---- power ------------------------------------------------
            srv_v, srv_i = _finite(d.get("SERVO_V")), _finite(d.get("SERVO_I_MA"))
            p_bus, p_srv = bus_v * bus_i / 1000.0, srv_v * srv_i / 1000.0
            r["P_BUS"], r["P_SERVO"], r["P_TOTAL"] = p_bus, p_srv, p_bus + p_srv
            if 0 < dt < 5.0 and have("BUS_V", "BUS_I_MA"):
                self.energy_wh += (p_bus + p_srv) * dt / 3600.0
            r["ENERGY_WH"] = self.energy_wh
            if have("BUS_V", "BUS_I_MA"):
                dp.add("P_BUS")
            if have("SERVO_V", "SERVO_I_MA"):
                dp.add("P_SERVO")
            if have("BUS_V", "BUS_I_MA", "SERVO_V", "SERVO_I_MA"):
                dp.update(["P_TOTAL", "ENERGY_WH"])

            # ---- link -------------------------------------------------
            rssi = _finite(d.get("RSSI"))
            r["LINK_MARGIN"] = (rssi - LORA_SENS_DBM) if rssi < 0 else 0.0
            if "RSSI" in present:
                dp.add("LINK_MARGIN")

            # ---- high-g -----------------------------------------------
            ahx, ahy, ahz = (_finite(d.get("AHX")), _finite(d.get("AHY")),
                             _finite(d.get("AHZ")))
            g_hi = math.sqrt(ahx * ahx + ahy * ahy + ahz * ahz)
            if have("AHX", "AHY", "AHZ"):
                self.peak_g_hi = max(self.peak_g_hi, g_hi)
                dp.update(["G_HI", "G_HI_PK"])
            r["G_HI"], r["G_HI_PK"] = g_hi, self.peak_g_hi

            # ---- air quality / spectral -------------------------------
            r["AQI_PM"] = self.aqi_from_pm25(_finite(d.get("PM25")))
            if "PM25" in present:
                dp.add("AQI_PM")
            spec = [k for k in ALL_FIELDS if k.startswith("S_") and k != "S_FLK"]
            r["IRR_TOT"] = sum(_finite(d.get(k)) for k in spec)
            if any(k in present for k in spec):
                dp.add("IRR_TOT")

            # ---- navigation -------------------------------------------
            lat, lon = _finite(d.get("LAT")), _finite(d.get("LON"))
            gnss_ok = have("LAT", "LON")

            # ---- pad origin survey (see __init__ for why) ----------------
            # STATE is in every packet, so "still on the pad" is a fact the
            # vehicle reports rather than something the GCS has to infer.
            on_pad = ("STATE" in present and
                      int(_finite(d.get("STATE"), -1)) == STATE_NUM["BOOT"])
            if not on_pad and "STATE" in present:
                self._pad_frozen = True     # left BOOT — the origin is final

            if gnss_ok and (lat or lon):
                if self.lat0 is None:
                    self.lat0, self.lon0, self._pad_n = lat, lon, 1
                elif not self._pad_frozen and self._pad_n < PAD_SURVEY_MAX:
                    # Running mean. Each new fix carries 1/n weight, so the
                    # origin converges on the true pad position instead of
                    # inheriting the error of whichever fix happened to be
                    # first. Cheap, and needs no history buffer.
                    self._pad_n += 1
                    k = 1.0 / self._pad_n
                    self.lat0 += (lat - self.lat0) * k
                    self.lon0 += (lon - self.lon0) * k

            if self.lat0 is not None and gnss_ok and (lat or lon):
                r["DIST_M"] = self.hav_dist(self.lat0, self.lon0, lat, lon)
                r["BEARING"] = self.bearing(self.lat0, self.lon0, lat, lon)
                dp.update(["DIST_M", "BEARING"])
            else:
                r["DIST_M"] = r["BEARING"] = 0.0

            r["BARO_GPS_DALT"] = abs(alt - _finite(d.get("GPS_ALT_M")))
            if have("ALT_M", "GPS_ALT_M"):
                dp.add("BARO_GPS_DALT")

            # ---- apogee / AGL -----------------------------------------
            # takeoff_alt is the vertical half of the pad origin: the ALT_M the
            # vehicle reports while it is still sitting on the pad.
            #
            # It used to latch at "the first sample where alt > 5", which is a
            # sample taken DURING ASCENT, not on the pad. Every AGL_M after
            # that was then short by whatever altitude the vehicle had already
            # reached when the first packet above 5 m happened to arrive — and
            # on the ground the display read that value negative. The bug was
            # masked while the flight computer's own ALT_M was unreferenced;
            # now that ALT_M is a true AGL against a surveyed pad pressure, it
            # would have shown up directly as a wrong apogee.
            #
            # Anchoring on STATE=BOOT instead means the reference is taken from
            # the one period the vehicle is definitely stationary on the pad,
            # where ALT_M should already read ~0 after CMD_CALIBRATE. AGL_M
            # then tracks ALT_M as it should, and any small residual pad offset
            # is removed rather than propagated.
            if "ALT_M" in present:
                self.apogee_m = max(self.apogee_m, alt)
                if on_pad or self.takeoff_alt is None:
                    self.takeoff_alt = alt
                dp.update(["APOGEE_M", "AGL_M"])
            r["APOGEE_M"] = self.apogee_m
            r["AGL_M"] = alt - (self.takeoff_alt or 0.0)

            r["TTL_S"] = (max(alt, 0.0) / sink) if sink > 0.1 else 0.0
            if vsp_ok and "ALT_M" in present:
                dp.add("TTL_S")

            if "LGT_FLAG" in present and _finite(d.get("LGT_FLAG")) > 0:
                self.lightning_log.append(
                    {"t": t_s, "dist_km": _finite(d.get("LGT_DIST")),
                     "intensity": _finite(d.get("LGT_INT"))})

            # ---- storage estimate --------------------------------------
            est = packet_count * BYTES_PER_PACKET
            if "SD_PCT" not in present:
                r["SD_PCT"] = min(100.0, 100.0 * est / SD_CAP_BYTES)
            if "FLASH_PCT" not in present:
                r["FLASH_PCT"] = min(100.0, 100.0 * est / FLASH_CAP_BYTES)

            self.t_last, self.alt_last = t_s, alt
        except Exception:
            log.exception("Physics.compute non-fatal")

        for k, v in list(r.items()):
            if isinstance(v, float) and not math.isfinite(v):
                r[k] = 0.0
        return r, dp


# =========================================================================
# §7  LOGGERS  —  the graded artifacts
# =========================================================================
class Logger:
    """Writes the competition CSV live, plus XML and KML exports.

    The CSV is opened and flushed on every packet: if the laptop dies
    mid-flight the file on disk is still complete up to the last packet.
    """

    def __init__(self, out_dir: Path):
        self.dir = Path(out_dir)
        self.dir.mkdir(parents=True, exist_ok=True)
        self.csv_path = self.dir / CSV_FILENAME
        self._fh = None
        self._w = None
        self.rows = 0
        self.track = []          # (lat, lon, alt, state) for KML
        self._open()

    def _open(self):
        try:
            new = not self.csv_path.exists() or self.csv_path.stat().st_size == 0
            self._fh = open(self.csv_path, "a", newline="", encoding="utf-8")
            self._w = csv.writer(self._fh)
            if new:
                self._w.writerow(["TEAM_ID"] + ALL_FIELDS)
                self._fh.flush()
        except Exception:
            log.exception("CSV open failed")
            self._fh, self._w = None, None

    def write(self, d: dict, present=None):
        if not self._w:
            return
        try:
            row = [TEAM_ID]
            for k in ALL_FIELDS:
                if present is not None and k not in present:
                    row.append("")
                    continue
                v = d.get(k, 0.0)
                if k == "UTC":
                    row.append(str(v) if v else "")
                else:
                    row.append(csv_cell(k, v))
            self._w.writerow(row)
            self._fh.flush()
            self.rows += 1
            if present and {"LAT", "LON"} <= set(present):
                la, lo = _finite(d.get("LAT")), _finite(d.get("LON"))
                if la or lo:
                    self.track.append((la, lo, _finite(d.get("ALT_M")),
                                       int(_finite(d.get("STATE")))))
        except Exception:
            log.exception("CSV write failed")

    def export_xml(self) -> Path:
        p = self.dir / XML_FILENAME
        try:
            import xml.etree.ElementTree as ET
            root = ET.Element("FlightData", team=TEAM_ID, version=APP_VERSION,
                              rows=str(self.rows))
            sch = ET.SubElement(root, "Schema", fields=str(N_FIELDS))
            for k, m in SCHEMA.items():
                ET.SubElement(sch, "Field", name=k, unit=m["u"],
                              group=m["g"]).text = m["d"]
            if self.csv_path.exists():
                with open(self.csv_path, encoding="utf-8") as fh:
                    rd = csv.DictReader(fh)
                    data = ET.SubElement(root, "Telemetry")
                    for row in rd:
                        rec = ET.SubElement(data, "Packet")
                        for k, v in row.items():
                            if v not in ("", None):
                                rec.set(k.replace("/", "_"), str(v))
            ET.ElementTree(root).write(p, encoding="utf-8", xml_declaration=True)
        except Exception:
            log.exception("XML export failed")
        return p

    def export_kml(self) -> Path:
        """WGS-84 track for Google Earth. Deliberately a file, not a live tile
        service: nothing in the field flow may depend on an internet link."""
        p = self.dir / KML_FILENAME
        try:
            coords = "\n".join(f"{lo:.7f},{la:.7f},{al:.1f}"
                               for la, lo, al, _ in self.track)
            head = (self.track[0] if self.track else (0, 0, 0, 0))
            tail = (self.track[-1] if self.track else (0, 0, 0, 0))
            kml = f"""<?xml version="1.0" encoding="UTF-8"?>
<kml xmlns="http://www.opengis.net/kml/2.2"><Document>
<name>{TEAM_ID} flight track</name>
<Style id="t"><LineStyle><color>ff00ccff</color><width>3</width></LineStyle></Style>
<Placemark><name>Flight path</name><styleUrl>#t</styleUrl>
<LineString><altitudeMode>absolute</altitudeMode><coordinates>
{coords}
</coordinates></LineString></Placemark>
<Placemark><name>LAUNCH</name><Point><coordinates>{head[1]:.7f},{head[0]:.7f},{head[2]:.1f}</coordinates></Point></Placemark>
<Placemark><name>LAST FIX</name><Point><coordinates>{tail[1]:.7f},{tail[0]:.7f},{tail[2]:.1f}</coordinates></Point></Placemark>
</Document></kml>"""
            p.write_text(kml, encoding="utf-8")
        except Exception:
            log.exception("KML export failed")
        return p

    def close(self):
        try:
            if self._fh:
                self._fh.flush()
                self._fh.close()
        except Exception:
            pass
        self._fh = self._w = None


# =========================================================================
# §8  LINK LAYER  —  serial reader and flight simulator
# =========================================================================
class SerialLink(threading.Thread):
    """Reads newline-framed telemetry with auto-reconnect.

    Runs entirely off the UI thread and communicates only through a queue,
    so a hung port or an unplugged adaptor can never freeze the display.
    """

    def __init__(self, port, baud, q, status_cb=None, tx_q=None):
        super().__init__(daemon=True, name="SerialLink")
        self.port, self.baud = port, baud
        self.q, self.tx_q = q, tx_q
        self.status_cb = status_cb or (lambda s: None)
        self._stop_evt = threading.Event()
        self._ser = None
        self._buf = bytearray()

    def stop(self):
        self._stop_evt.set()
        try:
            if self._ser:
                self._ser.close()
        except Exception:
            pass

    def _open(self):
        if not HAVE_SERIAL:
            self.status_cb("NO_SERIAL")
            return False
        if self.port == "VIRTUAL_TEST":
            return True
        if not self.port or self.port.startswith("(") or "Disconnected" in self.port:
            self.status_cb("PORT_BUSY")
            return False
        try:
            self._ser = serial.Serial(self.port, self.baud, timeout=0.1, write_timeout=0.2)
            self.status_cb("CONNECTED")
            log.info("serial open %s @ %d", self.port, self.baud)
            return True
        except Exception as e:
            err_str = str(e)
            if "semaphore timeout" in err_str.lower():
                log.warning("serial port %s timed out (Bluetooth or unready hardware)", self.port)
            else:
                log.warning("serial open failed on %s: %s", self.port, err_str)
            self.status_cb("PORT_BUSY")
            return False

    def _drain_tx(self):
        """Non-blocking drain: write timeouts ensure queue reading isn't stalled."""
        if not self.tx_q:
            return
        try:
            while True:
                cmd = self.tx_q.get_nowait()
                if self._ser:
                    try:
                        self._ser.write((cmd + "\r\n").encode("utf-8"))
                    except Exception:
                        log.exception("serial tx failed")
        except queue.Empty:
            pass

    def _run_virtual_gateway(self):
        """Virtual Gateway Feed: Emulates physical ESP32 LoRa Gateway (vritra_gs.ino).
        
        Provides complete wire-format verification and telecommand loopback testing
        even when no physical USB transceiver is attached to the workstation.
        """
        self.status_cb("CONNECTED")
        log.info("Virtual Gateway Feed running (Loopback Verification active)")
        
        # Gateway boot banner per vritra_gs.ino
        self.q.put(f"// [VRITRA-GS] {TEAM_ID}  gateway 6.2.0")
        self.q.put("// [WIRE] base-36, schema sum 0x3d410b42")
        self.q.put("// [RX] listening")
        self.q.put("// [RX] CanSat boots SILENT per guideline 6.1.v — send CMD_TX_ON to start telemetry")
        
        tx_enabled = True
        pkt = 1
        t0 = time.time()
        ul_seq = 0
        rng = np.random.default_rng(42)
        
        while not self._stop_evt.is_set():
            t_now = time.time()
            dt = t_now - t0
            
            # Drain and acknowledge uplink telecommands
            if self.tx_q:
                try:
                    while True:
                        cmd_line = self.tx_q.get_nowait()
                        cmd = cmd_line.strip()
                        ul_seq += 1
                        body = cmd[4:] if cmd.startswith("CMD_") else cmd
                        self.q.put(f"// [UP] queued {body} seq={ul_seq} (x6)")
                        self.q.put(f"// [UP] tx cmd={body} seq={ul_seq} left=0 OK")
                        
                        if "TX_OFF" in body:
                            tx_enabled = False
                            self.q.put("// [RX] telemetry paused (silent mode)")
                        elif "TX_ON" in body:
                            tx_enabled = True
                            self.q.put("// [RX] telemetry active @ 1 Hz")
                        elif "CALIBRATE" in body or "CAL_PRESS" in body:
                            self.q.put("// [RX] [ACK] Barometer sea-level calibration complete")
                        elif "BEACON_ON" in body:
                            self.q.put("// [RX] [ACK] Recovery audio beacon ACTIVE")
                        elif "BEACON_OFF" in body:
                            self.q.put("// [RX] [ACK] Recovery audio beacon OFF")
                        elif "FAILOVER" in body:
                            self.q.put(f"// [UP] following CanSat to {LORA_FREQ_BACKUP_MHZ:.2f} MHz")
                except queue.Empty:
                    pass

            if tx_enabled:
                # Realistic 1 Hz telemetry packet across all 83 fields
                alt = max(0.0, 750.0 - (dt * 4.8) if dt < 156 else 0.0)
                st = 0 if dt < 5 else (1 if dt < 30 else (2 if dt < 80 else (3 if dt < 150 else 4)))
                pres = SEA_LEVEL_P_PA * (1 - 2.25577e-5 * alt) ** 5.25588
                temp = 24.5 - 0.0065 * alt + float(rng.normal(0, 0.1))
                bus_v = max(9.8, 12.4 - dt * 0.005 + float(rng.normal(0, 0.02)))
                lat = SIM_SITE_LAT + (dt * 0.000015)
                lon = SIM_SITE_LON + (dt * 0.000022)
                
                d = OrderedDict((k, 0.0) for k in ALL_FIELDS)
                present = set(ALL_FIELDS)
                
                d["TIME_S"] = round(dt, 1)
                d["PKT"] = pkt
                d["ALT_M"] = round(alt + float(rng.normal(0, 0.3)), 2)
                d["PRES_PA"] = round(pres + float(rng.normal(0, 8.0)), 1)
                d["TEMP_C"] = round(temp, 2)
                d["BUS_V"] = round(bus_v, 3)
                d["GNSS_TIME"] = round(dt, 1)
                d["LAT"] = round(lat, 7)
                d["LON"] = round(lon, 7)
                d["GPS_ALT_M"] = round(alt + 4.1, 2)
                d["SATS"] = int(np.clip(12 + rng.normal(0, 1), 6, 16))
                
                d["ACC_X"] = round(float(rng.normal(0, 0.05)), 3)
                d["ACC_Y"] = round(float(rng.normal(0, 0.05)), 3)
                d["ACC_Z"] = round(G_EARTH + float(rng.normal(0, 0.05)), 3)
                d["SPIN_RATE"] = round(float(rng.normal(12.0, 1.5)) if st in (2, 3) else 0.0, 2)
                d["STATE"] = st
                
                d["BUS_I_MA"] = round(380.0 + float(rng.normal(0, 15.0)), 1)
                d["SOC_PCT"] = round(max(0.0, 98.0 - dt * 0.05), 1)
                d["GND_SPEED"] = round(float(rng.normal(4.2, 0.8)) if st in (2, 3) else 0.0, 2)
                d["HUM_PCT"] = round(np.clip(52.0 + rng.normal(0, 1.5), 0, 100), 1)
                
                d["GX"] = round(float(rng.normal(0, 2.0)), 2)
                d["GY"] = round(float(rng.normal(0, 2.0)), 2)
                d["GZ"] = round(d["SPIN_RATE"], 2)
                
                hdg = (dt * 15.0) % 360.0
                hdg_rad = math.radians(hdg)
                d["MX"] = round(24.0 * math.cos(hdg_rad), 2)
                d["MY"] = round(24.0 * math.sin(hdg_rad), 2)
                d["MZ"] = 38.0
                d["AHX"] = round(d["ACC_X"] / G_EARTH, 3)
                d["AHY"] = round(d["ACC_Y"] / G_EARTH, 3)
                d["AHZ"] = round(d["ACC_Z"] / G_EARTH, 3)
                d["ATT_ROLL"] = round(float(rng.normal(0, 2.0)), 2)
                d["ATT_PITCH"] = round(float(rng.normal(0, 2.0)), 2)
                
                # Environmental sensors
                d["PM1"] = round(abs(float(rng.normal(6.5, 1.0))), 1)
                d["PM25"] = round(abs(float(rng.normal(14.0, 2.0))), 1)
                d["PM10"] = round(abs(float(rng.normal(25.0, 3.0))), 1)
                d["LUX"] = round(max(0.0, 35000 + alt * 5), 0)
                d["UV"] = round(max(0.0, 2.8 + alt * 0.001), 3)
                d["CO2"] = round(float(np.clip(415 + rng.normal(0, 10), 300, 2000)), 0)
                d["TVOC"] = round(abs(float(rng.normal(95, 15))), 0)
                d["ECO2"] = round(abs(float(rng.normal(430, 20))), 0)
                d["AQI"] = 2
                d["O3"] = round(abs(float(rng.normal(28, 4))), 1)
                d["HDC_T"] = round(temp, 2)
                d["HDC_RH"] = d["HUM_PCT"]
                
                # RF Link metrics
                d["RSSI"] = round(float(np.clip(-68 - alt * 0.02 + rng.normal(0, 1.5), -125, -45)), 1)
                d["SNR"] = round(float(np.clip(10.5 - alt * 0.004 + rng.normal(0, 0.8), -15, 14)), 1)
                d["FREQ_MHZ"] = LORA_FREQ_MHZ
                
                # Servos and health
                d["SERVO_V"] = 5.02
                d["SERVO_I_MA"] = 45.0 if st < 2 else 180.0
                d["SRV1_DEG"] = 90.0 if st >= 2 else 0.0
                d["SRV2_DEG"] = 90.0 if st >= 2 else 0.0
                d["SRV3_DEG"] = 60.0 if st >= 3 else 0.0
                d["SRV4_DEG"] = 60.0 if st >= 3 else 0.0
                d["MCU_ACTIVE"] = 0
                d["WDOG_OK"] = 1
                d["SD_PCT"] = round(min(100.0, pkt * 0.001), 3)
                d["FLASH_PCT"] = round(min(100.0, pkt * 0.002), 3)
                d["BEACON_ON"] = 1 if st == 4 else 0
                d["CAM1_OK"] = 1
                d["CAM2_OK"] = 1
                d["NAVIC_SATS"] = 6
                d["UTC"] = datetime.datetime.now().strftime("%Y-%m-%dT%H:%M:%S")
                d["HDOP"] = 1.15
                d["VDOP"] = 1.78
                d["FIX_QUALITY"] = 1
                d["GEOID_SEP"] = -43.2
                d["GNSS_SYS"] = 1 | 4 | 16 | 32
                
                # Spectral channels
                names = ["S_405", "S_425", "S_450", "S_475", "S_515", "S_550",
                         "S_555", "S_600", "S_640", "S_690", "S_745", "S_855"]
                for nm in names:
                    d[nm] = round(max(0.0, 110.0 + float(rng.normal(0, 5.0))), 2)
                d["S_CLR"] = round(sum(d[nm] for nm in names) * 0.8, 2)
                d["S_FLK"] = 100.0
                
                # Emit gateway status tag and CSV packet
                self.q.put(f"// [RX] #{pkt} 48B RSSI {d['RSSI']} SNR {d['SNR']} | missed=0 PER=0.0% rej=0")
                line = TEAM_ID + "," + serialize_packet(d, present)
                self.q.put(line)
                pkt += 1
                
            self._stop_evt.wait(1.0)

    def run(self):
        if self.port == "VIRTUAL_TEST":
            self._run_virtual_gateway()
            return

        backoff = 0
        while not self._stop_evt.is_set():
            if not self._open():
                self._stop_evt.wait(RECONNECT_BACKOFF[min(backoff, len(RECONNECT_BACKOFF) - 1)])
                backoff += 1
                continue
            backoff = 0
            try:
                while not self._stop_evt.is_set():
                    self._drain_tx()
                    try:
                        chunk = self._ser.read(1024)
                    except serial.SerialException:
                        log.warning("serial hardware detached")
                        self.status_cb("PORT_LOST")
                        break
                    except Exception as e:
                        log.warning("serial read error: %s, reconnecting", e)
                        self.status_cb("RECONNECTING")
                        break
                        
                    if not chunk:
                        continue
                    
                    self._buf.extend(chunk)
                    if len(self._buf) > 65536:
                        del self._buf[:32768]
                        
                    # Strict framing on newlines
                    while b"\n" in self._buf:
                        i = self._buf.index(b"\n")
                        frame = self._buf[:i]
                        del self._buf[:i + 1]
                        
                        try:
                            # Strip \r and decode cleanly
                            line = frame.decode("utf-8", "ignore").strip()
                            if line:
                                self.q.put_nowait(line)
                        except queue.Full:
                            # Drop oldest cleanly if bounded queue fills
                            try:
                                self.q.get_nowait()
                                self.q.put_nowait(line)
                            except queue.Empty:
                                pass
            finally:
                if self._ser:
                    try: 
                        self._ser.close()
                    except Exception: 
                        pass
                self._ser = None
                if not self._stop_evt.is_set():
                    self.status_cb("RECONNECTING")


class Simulator(threading.Thread):
    """Synthetic flight: BOOT -> LAUNCH -> EJECT -> SEC_EJECT -> TOUCHDOWN.

    Exercises the full 79-field packet including realistic sensor noise, so
    every panel and the limits engine can be verified without hardware.
    Supports both automated flight profile playback and real-time manual
    slider manipulation with fault injection.
    """

    def __init__(self, q, hz=SIM_HZ):
        super().__init__(daemon=True, name="Simulator")
        self.q = q
        self.hz = hz
        self._stop_evt = threading.Event()
        self.lat0, self.lon0 = SIM_SITE_LAT, SIM_SITE_LON
        self.manual_mode = False
        self.paused = False
        self.speed = 1.0
        self.sim_time = 0.0
        self._last_time = time.time()

        # Interactive overrides dictionary
        self.overrides = {
            "state": 0,
            "alt": 0.0,
            "vsp": 0.0,
            "roll": 0.0,
            "pitch": 0.0,
            "yaw": 0.0,
            "spin": 0.0,
            "bus_v": 12.2,
            "bus_i": 380.0,
            "soc": 98.0,
            "temp": 24.0,
            "pres": SEA_LEVEL_P_PA,
            "lat": SIM_SITE_LAT,
            "lon": SIM_SITE_LON,
            "sats": 12,
            "hdop": 1.1,
            "fix_quality": 1,
            "aqi": 2,
            "hum": 50.0,
            "co2": 420.0,
            "rssi": -65.0,
            "snr": 10.5,
        }

        # Fault injection sets
        self.fault_disconnect = set()
        self.fault_short = set()
        self.fault_freeze = set()
        self._frozen_cache = {}
        self.fault_halt_radio = False

    def stop(self):
        self._stop_evt.set()

    def set_manual(self, enable=True):
        self.manual_mode = bool(enable)

    def set_param(self, key, value):
        self.overrides[key] = value

    def set_preset(self, preset_dict):
        self.overrides.update(preset_dict)

    def set_speed(self, speed):
        self.speed = max(0.2, float(speed))

    def set_paused(self, paused):
        self.paused = bool(paused)

    def inject_disconnect(self, keys):
        if isinstance(keys, str):
            keys = [keys]
        self.fault_disconnect.update(keys)

    def inject_short(self, keys):
        if isinstance(keys, str):
            keys = [keys]
        self.fault_short.update(keys)

    def inject_freeze(self, keys):
        if isinstance(keys, str):
            keys = [keys]
        self.fault_freeze.update(keys)

    def clear_faults(self):
        self.fault_disconnect.clear()
        self.fault_short.clear()
        self.fault_freeze.clear()
        self._frozen_cache.clear()
        self.fault_halt_radio = False

    def _profile(self, t):
        """-> (state index, altitude AGL m, vertical speed m/s)."""
        if t < 8:
            return 0, 0.0, 0.0
        if t < 38:                       # powered ascent to ~700 m
            x = (t - 8) / 30.0
            return 1, 700.0 * (x ** 1.6), 700.0 * 1.6 * (x ** 0.6) / 30.0
        if t < 48:                       # coast / apogee
            x = (t - 38) / 10.0
            return 1, 700.0 + 60.0 * math.sin(math.pi * x / 2), 60.0 * math.cos(math.pi * x / 2) / 10.0
        if t < 60:                       # canister eject, primary chute
            return 2, max(0.0, 760.0 - 14.0 * (t - 48)), -14.0
        if t < 130:
            # Braked descent, rate chosen so altitude reaches exactly zero at
            # the touchdown boundary. A profile that still had hundreds of
            # metres left when it switched to TOUCHDOWN produced a step change
            # in altitude, and the baro differentiator turned that step into a
            # physically impossible -880 m/s vertical speed.
            rate = 592.0 / 70.0          # ~8.46 m/s over 70 s
            return 3, max(0.0, 592.0 - rate * (t - 60)), -rate
        return 4, 0.0, 0.0

    def run(self):
        self._last_time = time.time()
        pkt = 0
        rng = np.random.default_rng(20260726)
        while not self._stop_evt.is_set():
            now = time.time()
            dt = now - self._last_time
            self._last_time = now
            if not self.paused and not self.manual_mode:
                self.sim_time += dt * self.speed
            t = self.sim_time

            if self.fault_halt_radio:
                self._stop_evt.wait(1.0 / self.hz)
                continue

            if self.manual_mode:
                st = int(self.overrides.get("state", 0))
                alt = float(self.overrides.get("alt", 0.0))
                vsp = float(self.overrides.get("vsp", 0.0))
            else:
                st, alt, vsp = self._profile(t)

            pkt += 1

            d = OrderedDict((k, 0.0) for k in ALL_FIELDS)
            present = set()

            def put(k, v):
                if k in self.fault_disconnect:
                    return
                if k in self.fault_short:
                    v = 0.0
                if k in self.fault_freeze:
                    if k not in self._frozen_cache:
                        self._frozen_cache[k] = v
                    v = self._frozen_cache[k]
                d[k] = v
                present.add(k)

            # --- mandated core -------------------------------------------
            put("TIME_S", round(t, 2))
            put("PKT", pkt)

            if self.manual_mode:
                put("ALT_M", round(alt, 2))
                temp = float(self.overrides.get("temp", round(21.5 - 0.0065 * alt, 2)))
                pres = float(self.overrides.get("pres", round(SEA_LEVEL_P_PA * (1 - 2.25577e-5 * alt) ** 5.25588, 1)))
                bus_v = float(self.overrides.get("bus_v", 12.2))
                put("PRES_PA", round(pres, 1))
                put("TEMP_C", round(temp, 2))
                put("BUS_V", round(bus_v, 3))
                put("GNSS_TIME", round(t, 1))

                lat = float(self.overrides.get("lat", self.lat0))
                lon = float(self.overrides.get("lon", self.lon0))
                put("LAT", round(lat, 7))
                put("LON", round(lon, 7))
                put("GPS_ALT_M", round(alt + 4.2, 2))
                sats = int(self.overrides.get("sats", 12))
                put("SATS", sats)

                roll = float(self.overrides.get("roll", 0.0))
                pitch = float(self.overrides.get("pitch", 0.0))
                yaw = float(self.overrides.get("yaw", 0.0))
                spin = float(self.overrides.get("spin", 0.0))
                r_rad = math.radians(roll)
                p_rad = math.radians(pitch)
                cp = math.cos(p_rad)
                ax = -G_EARTH * cp * math.sin(r_rad)
                ay = G_EARTH * math.sin(p_rad)
                az = G_EARTH * cp * math.cos(r_rad)
                g_ax = ax + rng.normal(0, 0.02)
                g_ay = ay + rng.normal(0, 0.02)
                g_az = az + rng.normal(0, 0.02)
                put("ACC_X", round(g_ax, 3))
                put("ACC_Y", round(g_ay, 3))
                put("ACC_Z", round(g_az, 3))
                put("SPIN_RATE", round(spin, 2))
                put("STATE", st)

                hdg = math.radians(yaw)
                put("MX", round(24.0 * math.sin(hdg), 2))
                put("MY", round(24.0 * math.cos(hdg), 2))
                put("MZ", 38.0)
                put("GX", 0.0)
                put("GY", 0.0)
                put("GZ", round(spin, 2))
                put("AHX", round(ax / G_EARTH, 3))
                put("AHY", round(ay / G_EARTH, 3))
                put("AHZ", round(az / G_EARTH, 3))
                put("ATT_ROLL", round(roll, 2))
                put("ATT_PITCH", round(pitch, 2))

                bus_i = float(self.overrides.get("bus_i", 380.0))
                soc = float(self.overrides.get("soc", 98.0))
                put("BUS_I_MA", round(bus_i, 1))
                put("SOC_PCT", round(soc, 1))
                put("GND_SPEED", round(abs(vsp * 0.4), 2))
                put("HUM_PCT", float(self.overrides.get("hum", 50.0)))
            else:
                put("ALT_M", round(alt + rng.normal(0, 0.35), 2))
                temp = 21.5 - 0.0065 * alt + rng.normal(0, 0.15)
                pres = SEA_LEVEL_P_PA * (1 - 2.25577e-5 * alt) ** 5.25588
                put("PRES_PA", round(pres + rng.normal(0, 12), 1))
                put("TEMP_C", round(temp, 2))
                # 3S Li-Ion sagging under load across the flight
                bus_v = 12.35 - 0.0022 * t + rng.normal(0, 0.012)
                put("BUS_V", round(bus_v, 3))
                put("GNSS_TIME", round(t, 1))

                drift = t * 0.9
                put("LAT", round(self.lat0 + drift * 1.1e-5 + rng.normal(0, 3e-6), 7))
                put("LON", round(self.lon0 + drift * 1.7e-5 + rng.normal(0, 3e-6), 7))
                put("GPS_ALT_M", round(alt + 4.2 + rng.normal(0, 1.1), 2))
                put("SATS", int(np.clip(11 + rng.normal(0, 1.1), 4, 14)))

                spin = 0.0 if st == 0 else (95.0 if st >= 2 else 18.0)
                spin += rng.normal(0, 2.5)
                g_ax = rng.normal(0, 0.25)
                g_ay = rng.normal(0, 0.25)
                g_az = G_EARTH + (26.0 if 8 < t < 38 else 0.0) + rng.normal(0, 0.4)
                put("ACC_X", round(g_ax, 3))
                put("ACC_Y", round(g_ay, 3))
                put("ACC_Z", round(g_az, 3))
                put("SPIN_RATE", round(spin, 2))
                put("STATE", st)

                # --- payload --------------------------------------------------
                bus_i = 380 + (240 if st >= 2 else 0) + rng.normal(0, 22)
                put("BUS_I_MA", round(bus_i, 1))
                put("SOC_PCT", round(np.clip(97 - t * 0.09, 0, 100), 1))
                put("GND_SPEED", round(abs(rng.normal(6.5, 1.4)), 2))
                put("HUM_PCT", round(np.clip(46 + rng.normal(0, 2.2), 0, 100), 1))

                put("GX", round(rng.normal(0, 4.5), 2))
                put("GY", round(rng.normal(0, 4.5), 2))
                put("GZ", round(spin, 2))
                hdg = math.radians((t * 6.0) % 360.0)
                put("MX", round(24.0 * math.cos(hdg) + rng.normal(0, 0.5), 2))
                put("MY", round(24.0 * math.sin(hdg) + rng.normal(0, 0.5), 2))
                put("MZ", round(38.0 + rng.normal(0, 0.5), 2))
                put("AHX", round(g_ax / G_EARTH, 3))
                put("AHY", round(g_ay / G_EARTH, 3))
                put("AHZ", round(g_az / G_EARTH, 3))
                put("ATT_ROLL", 0.0)
                put("ATT_PITCH", 0.0)

            put("PM1",  round(abs(rng.normal(7.5, 1.6)), 1))
            put("PM25", round(abs(rng.normal(17.0, 3.4)), 1))
            put("PM10", round(abs(rng.normal(31.0, 5.5)), 1))
            put("LUX",  round(max(0.0, 42000 + alt * 7 + rng.normal(0, 900)), 0))
            put("UV",   round(max(0.0, 3.1 + alt * 0.0012 + rng.normal(0, 0.09)), 3))
            put("CO2",  round(np.clip(430 + rng.normal(0, 14), 300, 5000), 0))
            put("TVOC", round(abs(rng.normal(118, 22)), 0))
            put("ECO2", round(abs(rng.normal(465, 26)), 0))
            put("AQI",  int(np.clip(round(rng.normal(2.0, 0.5)), 1, 5)))
            put("O3",   round(abs(rng.normal(31, 5.5)), 1))
            put("HDC_T", round(temp + rng.normal(0, 0.06), 2))
            put("HDC_RH", round(np.clip(46 + rng.normal(0, 1.4), 0, 100), 2))

            if rng.random() < 0.012:
                put("LGT_DIST", round(rng.uniform(4, 38), 1))
                put("LGT_INT", round(rng.uniform(1000, 60000), 0))
                put("LGT_FLAG", 1)
            else:
                put("LGT_DIST", 0.0)
                put("LGT_INT", 0.0)
                put("LGT_FLAG", 0)

            # Solar spectrum shape across the AS7343 bands
            base = [0.34, 0.46, 0.63, 0.78, 0.93, 1.00, 0.99, 0.92,
                    0.86, 0.74, 0.61, 0.44]
            names = ["S_405", "S_425", "S_450", "S_475", "S_515", "S_550",
                     "S_555", "S_600", "S_640", "S_690", "S_745", "S_855"]
            scale = 128.0 * (1.0 + alt / 3200.0)
            for n, b in zip(names, base):
                put(n, round(max(0.0, b * scale + rng.normal(0, 2.4)), 2))
            put("S_CLR", round(sum(d[n] for n in names) * 0.82, 2))
            put("S_FLK", round(abs(rng.normal(100, 4)), 1))

            # Link degrades with slant range
            drift_val = t * 0.9 if not self.manual_mode else 10.0
            rng_m = math.sqrt(max(alt, 1.0) ** 2 + (drift_val * 1.6) ** 2)
            put("RSSI", round(np.clip(-42 - 20 * math.log10(max(rng_m, 1) / 10.0)
                                      + rng.normal(0, 2.0), -135, -30), 1))
            put("SNR", round(np.clip(11.5 - rng_m / 380.0 + rng.normal(0, 0.9),
                                     -20, 13), 2))
            put("FREQ_MHZ", LORA_FREQ_MHZ)

            put("SERVO_V", round(5.02 + rng.normal(0, 0.02), 3))
            put("SERVO_I_MA", round(abs(rng.normal(190 if st >= 2 else 45, 26)), 1))
            put("SRV1_DEG", 92.0 if st >= 2 else 0.0)
            put("SRV2_DEG", 92.0 if st >= 2 else 0.0)
            put("SRV3_DEG", 58.0 if st >= 3 else 0.0)
            put("SRV4_DEG", 58.0 if st >= 3 else 0.0)

            put("MCU_ACTIVE", 0)
            put("WDOG_OK", 1)
            put("SD_PCT", round(min(100.0, pkt * BYTES_PER_PACKET * 100.0 / SD_CAP_BYTES), 4))
            put("FLASH_PCT", round(min(100.0, pkt * BYTES_PER_PACKET * 100.0 / FLASH_CAP_BYTES), 3))
            put("BEACON_ON", 1 if st == 4 else 0)
            put("CAM1_OK", 1)
            put("CAM2_OK", 1)
            put("NAVIC_SATS", int(np.clip(5 + rng.normal(0, 0.9), 0, 9)))
            put("UTC", datetime.datetime.now().strftime("%Y-%m-%dT%H:%M:%S"))

            # ---- GNSS quality, as the Quectel L89 reports it -------------
            if self.manual_mode:
                hdop_val = float(self.overrides.get("hdop", 1.1))
                put("HDOP", round(hdop_val, 2))
                put("VDOP", round(hdop_val * 1.55, 2))
                put("FIX_QUALITY", int(self.overrides.get("fix_quality", 1)))
            else:
                n_sv = d["SATS"] + d["NAVIC_SATS"]
                hdop = np.clip(2.6 - 0.055 * n_sv - alt / 2600.0
                               + rng.normal(0, 0.06), 0.55, 6.0)
                put("HDOP", round(float(hdop), 2))
                put("VDOP", round(float(hdop * 1.55 + rng.normal(0, 0.05)), 2))
                sbas = t > 25.0 and n_sv >= 12
                put("FIX_QUALITY", 2 if sbas else 1)     # 2 = GAGAN/SBAS corrected

            put("GEOID_SEP", round(-43.2 + rng.normal(0, 0.05), 2))
            # GPS | GALILEO | QZSS | NavIC — the L89's concurrent set
            put("GNSS_SYS", 1 | 4 | 16 | 32)

            line = TEAM_ID + "," + serialize_packet(d, present)
            try:
                self.q.put_nowait(line)
            except queue.Full:
                pass
            self._stop_evt.wait(1.0 / self.hz)


# =========================================================================
# §9  QT IMPORTS + DESIGN SYSTEM
# =========================================================================
from PyQt5.QtCore import Qt, QTimer, QRectF, QPointF
from PyQt5.QtGui import (
    QFont, QFontDatabase, QColor, QPainter, QPen, QBrush, QPalette,
    QPainterPath, QPolygonF, QKeySequence,
)
from PyQt5.QtWidgets import (
    QApplication, QMainWindow, QWidget, QFrame, QLabel, QPushButton,
    QHBoxLayout, QVBoxLayout, QGridLayout, QTabWidget, QSizePolicy,
    QComboBox, QLineEdit, QPlainTextEdit, QMessageBox, QScrollArea,
    QCheckBox, QSpinBox, QDoubleSpinBox, QTableWidget, QTableWidgetItem,
    QHeaderView, QAbstractItemView, QShortcut,
    QSlider, QDialog, QGroupBox, QRadioButton, QButtonGroup, QSplitter,
    QProgressBar,
)
import pyqtgraph as pg

# pyqtgraph.opengl is deliberately NOT imported: it raises GLError 1282 on
# machines without a suitable GPU driver. The 3-D CanSat uses a software
# QPainter renderer, so the GCS runs on any laptop or VM in the field.


class T:
    """Design tokens — "austere base, science colour".

    Flight and engineering surfaces are monochrome. `amber` and `red` are
    reserved exclusively for limit breaches and staleness; `green` only for
    successful initialisation and confirmed uplink. Nothing else may use a
    saturated colour, so a real alarm never has to compete for attention.
    """

    black    = "#000000"      # canvas
    surf     = "#0a0a0a"      # canvas-night-soft: barely-lifted panel
    surf2    = "#121214"      # input wells, table stripes
    surf3    = "#1a1a1d"      # hover
    line     = "#3a3a3f"      # 1 px hairline
    line_dim = "#222226"      # internal rules, grid

    fg       = "#ffffff"      # on-primary
    fg2      = "#9a9aa2"      # secondary
    fg3      = "#5f5f68"      # labels, units
    fg4      = "#3f3f47"      # disabled / no-data

    amber    = "#ffb000"
    amber_d  = "#4a3300"
    red      = "#ff3b30"
    red_d    = "#4a1210"
    green    = "#30d158"
    green_d  = "#0d3a1c"
    cyan     = "#00d2ff"
    cyan_d   = "#003b4a"

    # Wavelength ramp for the AS7343 spectral bars: colour here encodes a
    # real physical dimension (405-855 nm), which is the one case the design
    # language permits.
    SPECTRAL = [
        "#7b2ff7", "#5b3df5", "#2f6bf0", "#1f9be8", "#26c281", "#7ed321",
        "#b8d117", "#f5c518", "#f59218", "#e8502a", "#c62828", "#8b1a1a",
    ]

    # Sequential ramp for science heat/intensity encodings (perceptually
    # ordered, dark-to-light so it reads correctly on black).
    SEQ = ["#12263a", "#1b4a6b", "#1f7a93", "#27a8a0", "#5fcf9a", "#b8e986"]

    FACE_DISPLAY = "Bahnschrift SemiCondensed"
    FACE_DISPLAY_ALT = "Bahnschrift"
    FACE_MONO = "Cascadia Mono"
    FACE_MONO_ALT = "Consolas"


_FONT_CACHE = {}


def font_display(pt=9, bold=False, tracking=1.2, caps=True):
    """Label/heading face. DIN-derived geometric sans, wide tracking and
    tight leading — the "engineer-tight" stack the design research
    describes."""
    key = ("d", pt, bold, tracking, caps)
    if key in _FONT_CACHE:
        return _FONT_CACHE[key]
    fams = QFontDatabase().families()
    fam = (T.FACE_DISPLAY if T.FACE_DISPLAY in fams
           else T.FACE_DISPLAY_ALT if T.FACE_DISPLAY_ALT in fams else "Segoe UI")
    f = QFont(fam)
    f.setPointSizeF(pt)
    f.setBold(bold)
    if tracking:
        f.setLetterSpacing(QFont.AbsoluteSpacing, tracking)
    if caps:
        f.setCapitalization(QFont.AllUppercase)
    _FONT_CACHE[key] = f
    return f


def font_mono(pt=11, bold=False):
    """Numeric face. Measured tabular (every digit the same advance), so a
    value updating every packet never shifts horizontally."""
    key = ("m", pt, bold)
    if key in _FONT_CACHE:
        return _FONT_CACHE[key]
    fams = QFontDatabase().families()
    fam = (T.FACE_MONO if T.FACE_MONO in fams
           else T.FACE_MONO_ALT if T.FACE_MONO_ALT in fams else "Courier New")
    f = QFont(fam)
    f.setPointSizeF(pt)
    f.setBold(bold)
    f.setStyleHint(QFont.Monospace)
    _FONT_CACHE[key] = f
    return f


def state_colour(state):
    """Map a limits/staleness state to its ink colour."""
    if state in (ST_RED_LOW, ST_RED_HIGH, ST_LOST):
        return T.red
    if state in (ST_YEL_LOW, ST_YEL_HIGH, ST_STALE):
        return T.amber
    if state == ST_WARNING:
        return T.fg2
    if state == ST_NODATA:
        return T.fg4
    return T.fg


# Parameters whose natural precision is not what magnitude-scaling would
# pick. Coordinates are the important case: 2 decimals of latitude is ~1 km
# of error, which would be useless to a recovery team.
FMT_OVERRIDE = {
    "PRES_PA": "{:.0f}", "LUX": "{:.0f}", "REYNOLDS": "{:.3e}",
    "SD_PCT": "{:.3f}", "FLASH_PCT": "{:.3f}",
    "GNSS_TIME": "{:.1f}", "TIME_S": "{:.1f}",
}

def _fmt_coord(v, is_lat):
    hemi = ("N" if v >= 0 else "S") if is_lat else ("E" if v >= 0 else "W")
    return f"{abs(v):.6f}° {hemi}"


def fmt_value(v, key=None):
    """Auto-scaled precision: big numbers lose decimals, small ones gain
    them, so column width stays stable and nothing reads as false precision.

    Integer-valued fields render without a decimal point, and a handful of
    parameters override the magnitude rule entirely (see FMT_OVERRIDE)."""
    if v is None:
        return "—"
    if isinstance(v, str):
        return v
    try:
        f = float(v)
    except (TypeError, ValueError):
        return str(v)
    if not math.isfinite(f):
        return "—"
    if key:
        if key == "LAT":
            return _fmt_coord(f, True)
        if key == "LON":
            return _fmt_coord(f, False)
        if key in FMT_OVERRIDE:
            return FMT_OVERRIDE[key].format(f)
        if key in INT_FIELDS:
            return f"{int(round(f))}"
    a = abs(f)
    if a >= 10000:
        return f"{f:.0f}"
    if a >= 100:
        return f"{f:.1f}"
    if a >= 1:
        return f"{f:.2f}"
    if a > 0:
        return f"{f:.3f}"
    return "0"


APP_QSS = f"""
QWidget {{
    background: {T.black};
    color: {T.fg};
}}
QScrollArea, QScrollArea > QWidget > QWidget {{ background: {T.black}; }}
QScrollBar:vertical {{
    background: {T.black}; width: 10px; margin: 0;
}}
QScrollBar::handle:vertical {{
    background: {T.line_dim}; min-height: 30px; border-radius: 0px;
}}
QScrollBar::handle:vertical:hover {{ background: {T.line}; }}
QScrollBar::add-line, QScrollBar::sub-line {{ height: 0; width: 0; }}
QScrollBar::add-page, QScrollBar::sub-page {{ background: {T.black}; }}
QScrollBar:horizontal {{ background: {T.black}; height: 10px; margin: 0; }}
QScrollBar::handle:horizontal {{ background: {T.line_dim}; min-width: 30px; }}
QScrollBar::handle:horizontal:hover {{ background: {T.line}; }}

QTabWidget::pane {{ border: none; background: {T.black}; top: -1px; }}
QTabBar::tab {{
    background: transparent;
    color: {T.fg3};
    padding: 9px 22px 8px 22px;
    margin: 0 2px 0 0;
    border: none;
    border-bottom: 2px solid transparent;
}}
QTabBar::tab:hover {{ color: {T.fg2}; }}
QTabBar::tab:selected {{
    color: {T.fg};
    border-bottom: 2px solid {T.fg};
}}

QPushButton {{
    background: transparent;
    color: {T.fg2};
    border: 1px solid {T.line};
    border-radius: 13px;
    padding: 5px 16px;
}}
QPushButton:hover {{ color: {T.fg}; border-color: {T.fg3}; }}
QPushButton:pressed {{ background: {T.surf3}; }}
QPushButton:disabled {{ color: {T.fg4}; border-color: {T.line_dim}; }}
QPushButton[accent="true"] {{ color: {T.fg}; border-color: {T.fg3}; }}

QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox {{
    background: {T.surf2};
    color: {T.fg};
    border: 1px solid {T.line_dim};
    border-radius: 2px;
    padding: 4px 8px;
    selection-background-color: {T.line};
}}
QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus {{
    border-color: {T.fg3};
}}
QComboBox::drop-down {{ border: none; width: 18px; }}
QComboBox QAbstractItemView {{
    background: {T.surf};
    color: {T.fg};
    border: 1px solid {T.line};
    selection-background-color: {T.surf3};
    outline: none;
}}

QPlainTextEdit {{
    background: {T.surf};
    color: {T.fg2};
    border: 1px solid {T.line_dim};
    selection-background-color: {T.line};
}}

QCheckBox {{ color: {T.fg2}; spacing: 7px; }}
QCheckBox::indicator {{
    width: 12px; height: 12px;
    border: 1px solid {T.line}; background: {T.black};
}}
QCheckBox::indicator:checked {{ background: {T.fg}; border-color: {T.fg}; }}

QTableWidget {{
    background: {T.surf};
    alternate-background-color: {T.surf2};
    gridline-color: {T.line_dim};
    border: 1px solid {T.line_dim};
    selection-background-color: {T.surf3};
    selection-color: {T.fg};
    outline: none;
}}
QHeaderView::section {{
    background: {T.black};
    color: {T.fg3};
    border: none;
    border-bottom: 1px solid {T.line};
    padding: 5px 8px;
}}
QTableWidget::item {{ padding: 3px 6px; border: none; }}

QSplitter::handle {{ background: {T.line_dim}; }}
QToolTip {{
    background: {T.surf}; color: {T.fg};
    border: 1px solid {T.line}; padding: 4px 7px;
}}

QSlider::groove:horizontal {{
    border: 1px solid {T.line_dim};
    height: 6px;
    background: {T.surf2};
    margin: 2px 0;
    border-radius: 3px;
}}
QSlider::handle:horizontal {{
    background: {T.cyan};
    border: 1px solid #0099cc;
    width: 14px;
    height: 14px;
    margin: -4px 0;
    border-radius: 7px;
}}
QSlider::handle:horizontal:hover {{
    background: {T.fg};
    border: 1px solid {T.cyan};
}}
QSlider::sub-page:horizontal {{
    background: {T.cyan_d};
    border-radius: 3px;
}}

QRadioButton {{
    color: {T.fg2};
    spacing: 7px;
    background: transparent;
}}
QRadioButton::indicator {{
    width: 12px;
    height: 12px;
    border-radius: 6px;
    border: 1px solid {T.line};
    background: {T.black};
}}
QRadioButton::indicator:checked {{
    background: {T.cyan};
    border: 1px solid {T.fg};
}}

QGroupBox {{
    border: 1px solid {T.line_dim};
    border-radius: 4px;
    margin-top: 8px;
    padding-top: 8px;
    background: {T.surf};
}}
QGroupBox::title {{
    subcontrol-origin: margin;
    subcontrol-position: top left;
    padding: 0 6px;
    color: {T.cyan};
    font-weight: bold;
}}
"""


# =========================================================================
# §10  WIDGET LIBRARY
# =========================================================================
class Panel(QFrame):
    """Titled container: hairline border, near-black fill, no shadow."""

    def __init__(self, title="", parent=None, dense=False):
        super().__init__(parent)
        self.setObjectName("Panel")
        self.setStyleSheet(
            f"#Panel {{ background: {T.surf}; border: 1px solid {T.line_dim}; }}")
        lay = QVBoxLayout(self)
        m = 8 if dense else 11
        lay.setContentsMargins(m, 7, m, m)
        lay.setSpacing(6 if dense else 8)
        self._lay = lay
        if title:
            head = QLabel(title)
            head.setFont(font_display(8.5, tracking=1.6, caps=False))
            head.setStyleSheet(f"color:{T.fg3}; background:transparent;")
            # Fixed vertical policy: a Preferred label absorbs a share of any
            # spare height, which pushed panel titles away from their top edge.
            head.setSizePolicy(QSizePolicy.Preferred, QSizePolicy.Fixed)
            lay.addWidget(head)
            rule = QFrame()
            rule.setFixedHeight(1)
            rule.setSizePolicy(QSizePolicy.Preferred, QSizePolicy.Fixed)
            rule.setStyleSheet(f"background:{T.line_dim}; border:none;")
            lay.addWidget(rule)

    def add(self, w, stretch=0):
        self._lay.addWidget(w, stretch)
        return w

    def add_layout(self, l):
        self._lay.addLayout(l)
        return l

    def stretch(self, n=1):
        self._lay.addStretch(n)


class Sparkline(QWidget):
    """Tiny inline history trace. Monochrome; conveys shape, not value."""

    def __init__(self, parent=None, n=90):
        super().__init__(parent)
        self.n = n
        self.buf = deque(maxlen=n)
        self.ink = QColor(T.fg3)
        self.setFixedHeight(16)
        self.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Fixed)

    def push(self, v):
        if v is None:
            return
        self.buf.append(float(v))
        self.update()

    def set_ink(self, c):
        if self.ink.name() != QColor(c).name():
            self.ink = QColor(c)

    def clear(self):
        self.buf.clear()
        self.update()

    def paintEvent(self, _):
        if len(self.buf) < 2:
            return
        p = QPainter(self)
        p.setRenderHint(QPainter.Antialiasing, True)
        w, h = self.width(), self.height()
        vals = list(self.buf)
        lo, hi = min(vals), max(vals)
        rngv = (hi - lo) or 1.0
        pad = 2.0
        step = w / max(1, len(vals) - 1)
        path = QPainterPath()
        for i, v in enumerate(vals):
            x = i * step
            y = h - pad - (v - lo) / rngv * (h - 2 * pad)
            path.moveTo(x, y) if i == 0 else path.lineTo(x, y)
        p.setPen(QPen(self.ink, 1.0))
        p.drawPath(path)
        p.end()


class Tile(QFrame):
    """One telemetry parameter.

    Nominal renders entirely monochrome. The left edge bar and the value ink
    take colour only when the limits engine reports a breach or staleness,
    and a never-seen parameter shows an em-dash rather than a fabricated 0.
    """

    def __init__(self, key, label=None, unit=None, spark=True,
                 big=False, parent=None):
        super().__init__(parent)
        self.key = key
        meta = FIELD_META.get(key, {})
        self.unit = unit if unit is not None else meta.get("u", "")
        self.state = ST_NODATA
        self._blink = False

        self.setObjectName("Tile")
        self._restyle(T.line_dim)
        lay = QVBoxLayout(self)
        lay.setContentsMargins(9, 6, 9, 6)
        lay.setSpacing(1)

        self.l_label = QLabel(label or key.replace("_", " ").title())
        self.l_label.setFont(font_display(7.8, tracking=1.3, caps=False))
        self.l_label.setStyleSheet(f"color:{T.fg3}; background:transparent;")

        row = QHBoxLayout()
        row.setContentsMargins(0, 0, 0, 0)
        row.setSpacing(5)
        self.l_value = QLabel("—")
        self.l_value.setFont(font_mono(19 if big else 13.5, bold=False))
        self.l_value.setStyleSheet(f"color:{T.fg4}; background:transparent;")
        self.l_unit = QLabel(self.unit)
        self.l_unit.setFont(font_display(7.5, tracking=0.8, caps=False))
        self.l_unit.setStyleSheet(f"color:{T.fg3}; background:transparent;")
        self.l_tag = QLabel("")
        self.l_tag.setFont(font_display(7.2, tracking=1.1))
        self.l_tag.setStyleSheet(f"color:{T.fg3}; background:transparent;")

        row.addWidget(self.l_value, 0, Qt.AlignBottom)
        row.addWidget(self.l_unit, 0, Qt.AlignBottom)
        row.addStretch(1)
        row.addWidget(self.l_tag, 0, Qt.AlignBottom)

        lay.addWidget(self.l_label)
        lay.addLayout(row)
        self.spark = Sparkline(self) if spark else None
        if self.spark:
            lay.addWidget(self.spark)

        tip = meta.get("d", "")
        lim = meta.get("lim")
        if lim:
            rl, yl, yh, rh = lim
            bits = [f"{n}={v}" for n, v in
                    (("RL", rl), ("YL", yl), ("YH", yh), ("RH", rh))
                    if v is not None]
            tip += "\nLimits: " + ", ".join(bits)
        self.setToolTip(tip)

    def _restyle(self, edge):
        self.setStyleSheet(
            f"#Tile {{ background:{T.surf}; border:1px solid {T.line_dim};"
            f" border-left:2px solid {edge}; }}")

    def set(self, value, state=ST_NOMINAL, present=True):
        """`present=False` means the field was never received: show an
        em-dash. Never render a zero for hardware that is not reporting."""
        self.state = state
        if not present or value is None:
            self.l_value.setText("—")
            self.l_value.setStyleSheet(f"color:{T.fg4}; background:transparent;")
            self.l_tag.setText("")
            self._restyle(T.line_dim)
            return

        txt = value if isinstance(value, str) else fmt_value(value, self.key)
        self.l_value.setText(txt)
        ink = state_colour(state)
        self.l_value.setStyleSheet(f"color:{ink}; background:transparent;")

        if state in (ST_RED_LOW, ST_RED_HIGH):
            self.l_tag.setText("ALARM")
            self.l_tag.setStyleSheet(f"color:{T.red}; background:transparent;")
            self._restyle(T.red)
        elif state == ST_LOST:
            self.l_tag.setText("LOST")
            self.l_tag.setStyleSheet(f"color:{T.red}; background:transparent;")
            self._restyle(T.red)
        elif state in (ST_YEL_LOW, ST_YEL_HIGH):
            self.l_tag.setText("WARN")
            self.l_tag.setStyleSheet(f"color:{T.amber}; background:transparent;")
            self._restyle(T.amber)
        elif state == ST_STALE:
            self.l_tag.setText("STALE")
            self.l_tag.setStyleSheet(f"color:{T.amber}; background:transparent;")
            self._restyle(T.amber)
        elif state == ST_WARNING:
            self.l_tag.setText("AGEING")
            self.l_tag.setStyleSheet(f"color:{T.fg3}; background:transparent;")
            self._restyle(T.line)
        else:
            self.l_tag.setText("")
            self._restyle(T.line_dim)

        if self.spark and not isinstance(value, str):
            self.spark.set_ink(ink if state != ST_NOMINAL else T.fg3)
            self.spark.push(value)


class Chip(QFrame):
    """Compact header status readout: LABEL + value."""

    def __init__(self, label, value="—", parent=None):
        super().__init__(parent)
        self.setObjectName("Chip")
        self._restyle(T.line_dim)
        lay = QHBoxLayout(self)
        lay.setContentsMargins(10, 4, 10, 4)
        lay.setSpacing(8)
        self.l_k = QLabel(label)
        self.l_k.setFont(font_display(7.6, tracking=1.3))
        self.l_k.setStyleSheet(f"color:{T.fg3}; background:transparent;")
        self.l_v = QLabel(value)
        self.l_v.setFont(font_mono(9.5))
        self.l_v.setStyleSheet(f"color:{T.fg}; background:transparent;")
        lay.addWidget(self.l_k)
        lay.addWidget(self.l_v)

    def _restyle(self, edge):
        self.setStyleSheet(
            f"#Chip {{ background:{T.surf}; border:1px solid {T.line_dim};"
            f" border-left:2px solid {edge}; }}")

    def set(self, value, ink=None):
        self.l_v.setText(str(value))
        c = ink or T.fg
        self.l_v.setStyleSheet(f"color:{c}; background:transparent;")
        self._restyle(c if ink and ink != T.fg else T.line_dim)


class MasterAlarm(QFrame):
    """Persistent banner. Silent and near-invisible while nominal — it must
    not become part of the background the operator learns to ignore."""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName("MA")
        self.setFixedHeight(0)
        self._blink = False
        self.state = ST_NOMINAL
        lay = QHBoxLayout(self)
        lay.setContentsMargins(14, 0, 14, 0)
        lay.setSpacing(14)
        self.l_txt = QLabel("")
        self.l_txt.setFont(font_display(11, bold=True, tracking=2.2))
        self.l_detail = QLabel("")
        self.l_detail.setFont(font_mono(9))
        lay.addWidget(self.l_txt)
        lay.addWidget(self.l_detail, 1)
        self.b_ack = QPushButton("ACK")
        self.b_ack.setFixedWidth(64)
        lay.addWidget(self.b_ack)
        self._style(T.red)

    def _style(self, c):
        self.setStyleSheet(
            f"#MA {{ background:{T.surf}; border:none; border-top:1px solid {c};"
            f" border-bottom:1px solid {c}; }}")
        self.l_txt.setStyleSheet(f"color:{c}; background:transparent;")
        self.l_detail.setStyleSheet(f"color:{T.fg2}; background:transparent;")

    def update_state(self, state, active, blink_phase):
        self.state = state
        if state in IS_ALARM:
            self.setFixedHeight(30)
            self._style(T.red if blink_phase else T.red_d)
            self.l_txt.setText("MASTER ALARM")
        elif state in IS_WARN:
            self.setFixedHeight(30)
            self._style(T.amber)
            self.l_txt.setText("CAUTION")
        else:
            self.setFixedHeight(0)
            self.l_txt.setText("")
            self.l_detail.setText("")
            return
        names = ", ".join(f"{p.name} {p.state}" for p in active[:6])
        more = f"   (+{len(active) - 6} more)" if len(active) > 6 else ""
        self.l_detail.setText(names + more)


class GhostButton(QPushButton):
    """Ghost-outlined pill, per the austere spec: no fill, no gradient."""

    def __init__(self, text, parent=None, width=None, accent=False):
        super().__init__(text, parent)
        self.setFont(font_display(8.2, tracking=1.2, caps=False))
        self.setCursor(Qt.PointingHandCursor)
        # Minimum, never fixed: letter-spacing makes the rendered string wider
        # than a naive estimate, and a fixed width silently clipped the first
        # character.
        if width:
            self.setMinimumWidth(width)
        self.setSizePolicy(QSizePolicy.Minimum, QSizePolicy.Fixed)
        if accent:
            self.setProperty("accent", "true")


class SectionTitle(QLabel):
    def __init__(self, text, parent=None):
        super().__init__(text, parent)
        self.setFont(font_display(8.5, tracking=1.6, caps=False))
        self.setStyleSheet(f"color:{T.fg3}; background:transparent;")


# =========================================================================
# §11  INSTRUMENTS  (all pure QPainter — no GPU required)
# =========================================================================
class ADI(QWidget):
    """Attitude director. Monochrome: sky is a lifted grey, ground is the
    canvas, the horizon is a single white hairline. No saturated fills, so
    the instrument never competes with an alarm."""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.roll = self.pitch = 0.0
        self.valid = False
        self.setMinimumSize(132, 132)

    def set_attitude(self, roll, pitch, valid=True):
        self.roll, self.pitch, self.valid = roll, pitch, valid
        self.update()

    def paintEvent(self, _):
        p = QPainter(self)
        p.setRenderHint(QPainter.Antialiasing, True)
        w, h = self.width(), self.height()
        cx, cy = w / 2.0, h / 2.0
        r = min(w, h) / 2.0 - 8

        p.save()
        clip = QPainterPath()
        clip.addEllipse(QPointF(cx, cy), r, r)
        p.setClipPath(clip)

        if not self.valid:
            p.fillRect(self.rect(), QColor(T.black))
            p.setClipping(False)
            p.setPen(QPen(QColor(T.line_dim), 1))
            p.drawEllipse(QPointF(cx, cy), r, r)
            p.setPen(QPen(QColor(T.fg4)))
            p.setFont(font_display(8, tracking=1.4))
            p.drawText(QRectF(0, 0, w, h), Qt.AlignCenter, "No Attitude")
            p.restore()
            p.end()
            return

        p.translate(cx, cy)
        p.rotate(-self.roll)
        ppd = r / 45.0
        p.translate(0, self.pitch * ppd)

        big = r * 3
        p.fillRect(QRectF(-big, -big, 2 * big, big), QColor(T.surf3))   # sky
        p.fillRect(QRectF(-big, 0, 2 * big, big), QColor(T.black))      # ground
        p.setPen(QPen(QColor(T.fg), 1.3))
        p.drawLine(QPointF(-big, 0), QPointF(big, 0))                   # horizon

        p.setFont(font_mono(6.5))
        for deg in range(-30, 31, 10):
            if deg == 0:
                continue
            y = -deg * ppd
            half = r * (0.36 if deg % 20 == 0 else 0.2)
            p.setPen(QPen(QColor(T.fg3), 1.0))
            p.drawLine(QPointF(-half, y), QPointF(half, y))
            if deg % 20 == 0:
                p.setPen(QPen(QColor(T.fg3)))
                p.drawText(QRectF(half + 4, y - 7, 26, 14),
                           Qt.AlignVCenter | Qt.AlignLeft, f"{abs(deg)}")
        p.restore()

        # roll pointer scale
        p.setPen(QPen(QColor(T.line), 1))
        p.drawEllipse(QPointF(cx, cy), r, r)
        for deg in (-60, -45, -30, -20, -10, 0, 10, 20, 30, 45, 60):
            a = math.radians(deg - 90)
            l = 7 if deg % 30 == 0 else 4
            x1, y1 = cx + r * math.cos(a), cy + r * math.sin(a)
            x2, y2 = cx + (r - l) * math.cos(a), cy + (r - l) * math.sin(a)
            p.setPen(QPen(QColor(T.fg3 if deg else T.fg), 1))
            p.drawLine(QPointF(x1, y1), QPointF(x2, y2))

        # roll index
        a = math.radians(-self.roll - 90)
        tri = QPolygonF([
            QPointF(cx + (r - 8) * math.cos(a), cy + (r - 8) * math.sin(a)),
            QPointF(cx + (r - 16) * math.cos(a - 0.06), cy + (r - 16) * math.sin(a - 0.06)),
            QPointF(cx + (r - 16) * math.cos(a + 0.06), cy + (r - 16) * math.sin(a + 0.06))])
        p.setBrush(QBrush(QColor(T.fg)))
        p.setPen(Qt.NoPen)
        p.drawPolygon(tri)

        # fixed aircraft symbol
        p.setPen(QPen(QColor(T.fg), 1.6))
        p.setBrush(Qt.NoBrush)
        p.drawLine(QPointF(cx - 26, cy), QPointF(cx - 9, cy))
        p.drawLine(QPointF(cx + 9, cy), QPointF(cx + 26, cy))
        p.drawLine(QPointF(cx, cy - 4), QPointF(cx, cy + 4))
        p.end()


class Compass(QWidget):
    """Heading rose. Hairline ticks, single white needle."""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.hdg = 0.0
        self.valid = False
        self.setMinimumSize(132, 132)

    def set_heading(self, h, valid=True):
        self.hdg, self.valid = h, valid
        self.update()

    def paintEvent(self, _):
        p = QPainter(self)
        p.setRenderHint(QPainter.Antialiasing, True)
        w, h = self.width(), self.height()
        cx, cy = w / 2.0, h / 2.0
        r = min(w, h) / 2.0 - 8

        p.setPen(QPen(QColor(T.line), 1))
        p.drawEllipse(QPointF(cx, cy), r, r)

        if not self.valid:
            p.setPen(QPen(QColor(T.fg4)))
            p.setFont(font_display(8, tracking=1.4))
            p.drawText(QRectF(0, 0, w, h), Qt.AlignCenter, "No Heading")
            p.end()
            return

        p.save()
        p.translate(cx, cy)
        p.rotate(-self.hdg)
        p.setFont(font_display(7.5, tracking=1.0))
        for deg in range(0, 360, 10):
            a = math.radians(deg - 90)
            major = deg % 90 == 0
            l = 9 if major else (6 if deg % 30 == 0 else 3)
            p.setPen(QPen(QColor(T.fg if major else T.fg3), 1.0))
            p.drawLine(QPointF(r * math.cos(a), r * math.sin(a)),
                       QPointF((r - l) * math.cos(a), (r - l) * math.sin(a)))
        for deg, lab in ((0, "N"), (90, "E"), (180, "S"), (270, "W")):
            a = math.radians(deg - 90)
            tx, ty = (r - 15) * math.cos(a), (r - 15) * math.sin(a)
            p.save()
            p.translate(tx, ty)
            p.rotate(self.hdg)
            p.setPen(QPen(QColor(T.fg if deg == 0 else T.fg2)))
            p.drawText(QRectF(-11, -8, 22, 16), Qt.AlignCenter, lab)
            p.restore()
        p.restore()

        p.setPen(QPen(QColor(T.fg), 1.5))
        p.drawLine(QPointF(cx, cy - r + 4), QPointF(cx, cy - r + 14))
        p.setFont(font_mono(11))
        p.setPen(QPen(QColor(T.fg)))
        p.drawText(QRectF(cx - 34, cy - 9, 68, 18), Qt.AlignCenter,
                   f"{self.hdg:05.1f}")
        p.end()


class CanSatView(QWidget):
    """Software 3-D CanSat, rendered with QPainter only.

    Telemetry arrives at 1 Hz but the widget repaints at 30 Hz and eases
    toward the latest measurement, so the model moves continuously instead
    of snapping once per packet. Above a spin threshold it rotates at the
    measured gyro rate, and yaw is re-anchored to absolute heading on every
    packet so rate integration can never accumulate drift.
    """

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setMinimumHeight(240)
        self.tgt_roll = self.tgt_pitch = self.tgt_yaw = 0.0
        self.roll = self.pitch = self.yaw = 0.0
        self.spin_dps = 0.0
        self.has_data = False
        self._geo = self._build()
        self._t = QTimer(self)
        self._t.timeout.connect(self._step)
        self._t.start(33)

    _quat = None      # eased value _project reads; None -> Euler fallback path
    _quat_tgt = None  # latest value from set_quaternion(), eased toward in _step

    def set_quaternion(self, q):
        """Attitude from the flight computer's quaternion filter (3-D model
        feed only -- see the QUAT_X/Y/Z SCHEMA comment in vritra_gcs.py).

        Telemetry arrives once a second; storing straight into self._quat
        made the model SNAP to each new orientation instead of animating,
        because _project's quaternion path (unlike the Euler path below) had
        nothing easing it between packets. _step() now eases self._quat
        toward this target at 30 Hz, the same as roll/pitch/yaw.

        Passing None reverts the widget to the Euler fallback, which is only
        correct away from the +/-90 deg pitch singularity."""
        self._quat_tgt = q
        if q is not None and self._quat is None:
            self._quat = q            # snap on the very first sample

    def set_attitude(self, roll, pitch, yaw, spin_dps=0.0, valid=True):
        if not valid:
            return
        self.tgt_roll, self.tgt_pitch = roll, pitch
        self.tgt_yaw = yaw
        self.spin_dps = spin_dps
        if not self.has_data:
            self.roll, self.pitch, self.yaw = roll, pitch, yaw
        self.has_data = True

    def _step(self):
        if not self.has_data:
            return
        k = 0.18                                   # critically-damped ease
        self.roll += (self.tgt_roll - self.roll) * k
        self.pitch += (self.tgt_pitch - self.pitch) * k
        if abs(self.spin_dps) > 5.0:
            self.yaw = (self.yaw + self.spin_dps * 0.033) % 360.0
        else:
            d = ((self.tgt_yaw - self.yaw + 180.0) % 360.0) - 180.0
            self.yaw = (self.yaw + d * k) % 360.0

        if self._quat_tgt is not None and self._quat is not None:
            w0, x0, y0, z0 = self._quat
            w1, x1, y1, z1 = self._quat_tgt
            # q and -q are the same rotation; interpolating toward the wrong
            # sign would spin the model the long way round every time the
            # flight computer's filter happens to flip sign between packets.
            # Picking whichever sign is closer (positive dot product) always
            # takes the short path.
            if w0*w1 + x0*x1 + y0*y1 + z0*z1 < 0.0:
                w1, x1, y1, z1 = -w1, -x1, -y1, -z1
            w = w0 + (w1 - w0) * k
            x = x0 + (x1 - x0) * k
            y = y0 + (y1 - y0) * k
            z = z0 + (z1 - z0) * k
            n = math.sqrt(w*w + x*x + y*y + z*z)
            self._quat = (w/n, x/n, y/n, z/n) if n > 1e-9 else self._quat
        elif self._quat_tgt is None:
            self._quat = None

        self.update()

    @staticmethod
    def _build():
        """Body geometry modelled from the VRITRA Fusion 360 design.

        Sections bottom-to-top: structural base plate, honeycomb body
        shell with hex cutouts, solid transition band, vertical-slat
        upper section, top plate with cutouts, two side antenna stubs.
        """
        R = CANSAT_OD_M / 2.0
        L = CANSAT_LEN_M
        NP = 36

        z_bot     = -L / 2
        z_bp      = z_bot + 0.012
        z_hex_top = z_bot + 0.232
        z_tr      = z_hex_top + 0.008
        z_sl_top  = L / 2 - 0.020
        z_top     = L / 2

        def ring(z):
            return [(R * math.cos(2.0 * math.pi * i / NP),
                     R * math.sin(2.0 * math.pi * i / NP), z)
                    for i in range(NP)]

        hs = 0.015
        hf = hs * 1.7320508075688772
        dt = hf / R
        nc = int(round(2.0 * math.pi / dt)) or 12
        dt = 2.0 * math.pi / nc
        dz = hs * 1.5
        z0h = z_bp + hs * 0.9
        z1h = z_hex_top - hs * 0.9
        nr = max(1, int((z1h - z0h) / dz) + 1)

        hexagons = []
        for ri in range(nr):
            zc = z0h + ri * dz
            if zc > z1h:
                break
            off = dt / 2.0 if ri % 2 else 0.0
            for ci in range(nc):
                tc = ci * dt + off
                verts = []
                for k in range(6):
                    a = 0.5235987755982988 + 1.0471975511965976 * k
                    verts.append((R * math.cos(tc + hs * math.cos(a) / R),
                                  R * math.sin(tc + hs * math.cos(a) / R),
                                  zc + hs * math.sin(a)))
                hexagons.append(verts)

        NS = 36
        slats = []
        for i in range(NS):
            a = 2.0 * math.pi * i / NS
            x, y = R * math.cos(a), R * math.sin(a)
            slats.append(((x, y, z_tr), (x, y, z_sl_top)))

        tmid = (z_sl_top + z_top) / 2.0
        cuts = []
        for i in range(10):
            ac = 2.0 * math.pi * i / 10
            ccx = R * 0.55 * math.cos(ac)
            ccy = R * 0.55 * math.sin(ac)
            cuts.append([(ccx + 0.012 * math.cos(2.0 * math.pi * k / 5),
                          ccy + 0.012 * math.sin(2.0 * math.pi * k / 5),
                          tmid) for k in range(5)])
        cuts.append([(0.008 * math.cos(2.0 * math.pi * k / 6),
                      0.008 * math.sin(2.0 * math.pi * k / 6),
                      tmid) for k in range(6)])

        ants = []
        for sa in (math.pi / 2, -math.pi / 2):
            dx, dy = math.cos(sa), math.sin(sa)
            tx, ty = -dy, dx
            hw, hh, ad = 0.007, 0.009, 0.012
            az = (z_hex_top + z_tr) / 2.0
            ants.append([
                (R * dx - hw * tx, R * dy - hw * ty, az - hh),
                (R * dx + hw * tx, R * dy + hw * ty, az - hh),
                (R * dx + hw * tx, R * dy + hw * ty, az + hh),
                (R * dx - hw * tx, R * dy - hw * ty, az + hh),
                ((R + ad) * dx - hw * tx, (R + ad) * dy - hw * ty, az - hh),
                ((R + ad) * dx + hw * tx, (R + ad) * dy + hw * ty, az - hh),
                ((R + ad) * dx + hw * tx, (R + ad) * dy + hw * ty, az + hh),
                ((R + ad) * dx - hw * tx, (R + ad) * dy - hw * ty, az + hh),
            ])

        return {
            "R": R, "L": L,
            "bot": [ring(z_bot), ring(z_bp)],
            "hex": hexagons,
            "hex_bnd": [ring(z_bp), ring(z_hex_top)],
            "trans": [ring(z_hex_top), ring(z_tr)],
            "sl_rings": [ring(z_tr), ring((z_tr + z_sl_top) / 2.0), ring(z_sl_top)],
            "slats": slats,
            "top": [ring(z_sl_top), ring(z_top)],
            "cuts": cuts,
            "ants": ants,
        }

    def _project(self, pt, cx, cy, s):
        """Body frame -> screen.

        The perspective divide must use the axis pointing INTO the screen
        (y), not the vertical axis. Dividing by z made the far end of the
        body shrink relative to the near end along its own length, so an
        upright cylinder rendered as a cone.
        """
        x, y, z = pt

        # QUATERNION PATH — preferred. Rotating the body points directly by
        # the quaternion the flight computer sent means the display inherits
        # its global non-singularity: there is no orientation at which this
        # becomes ambiguous, no gimbal lock at vertical, and no possibility of
        # synthesising a left-handed frame by negating an axis. The rotation
        # matrix built from a unit quaternion always has determinant +1.
        q = getattr(self, "_quat", None)
        if q is not None:
            w, qx, qy, qz = q
            # v' = v + 2*cross(qv, cross(qv, v) + w*v)  -- avoids building R
            tx = 2.0 * (qy * z - qz * y)
            ty = 2.0 * (qz * x - qx * z)
            tz = 2.0 * (qx * y - qy * x)
            x = x + w * tx + (qy * tz - qz * ty)
            y = y + w * ty + (qz * tx - qx * tz)
            z = z + w * tz + (qx * ty - qy * tx)
            d = 2.6
            f = d / (d + y * 1.5)
            return QPointF(cx + x * s * f, cy - z * s * f), f

        # EULER FALLBACK — only for older firmware that sends no quaternion.
        # Retained because it is better than a dead display, but it carries
        # the singularity at pitch +-90 deg that the quaternion path exists to
        # avoid. Do not "simplify" the code by deleting the branch above.
        cr, sr = math.cos(math.radians(self.roll)), math.sin(math.radians(self.roll))
        cp, sp = math.cos(math.radians(self.pitch)), math.sin(math.radians(self.pitch))
        cy_, sy = math.cos(math.radians(self.yaw)), math.sin(math.radians(self.yaw))
        # AXIS ASSIGNMENT. The body frame is Z along the CanSat's long axis
        # (see _build), X out the side, Y into the screen. With that frame:
        #     ROLL  is rotation about X  -> mixes Y and Z
        #     PITCH is rotation about Y  -> mixes X and Z
        #     YAW   is rotation about Z  -> mixes X and Y
        #
        # This block previously fed PITCH into the about-X rotation and ROLL
        # into the about-Y rotation, i.e. the two were swapped. Tilting the
        # vehicle one way made the model move the other way, which is what
        # made the display look mis-calibrated even though the IMU was right.
        #
        # ORDER also changed. Yaw used to be applied FIRST, to the raw body
        # point. For the standard aerospace intrinsic Z-Y-X (yaw-pitch-roll)
        # sequence the composite is R = Rz(yaw)*Ry(pitch)*Rx(roll), so acting
        # on a point the rotations must be applied roll, then pitch, then yaw.
        # Applying yaw first made heading changes tumble the body instead of
        # spinning it about its own axis.
        y, z = y * cr - z * sr,  y * sr + z * cr           # roll  (about X)
        x, z = x * cp + z * sp, -x * sp + z * cp           # pitch (about Y)
        x, y = x * cy_ - y * sy, x * sy + y * cy_          # yaw   (about Z)
        d = 2.6
        f = d / (d + y * 1.5)
        return QPointF(cx + x * s * f, cy - z * s * f), f

    def paintEvent(self, _):
        p = QPainter(self)
        p.setRenderHint(QPainter.Antialiasing, True)
        w, h = self.width(), self.height()
        cx, cy = w / 2.0, h / 2.0
        s = min(w / (CANSAT_OD_M * 3.2), h / (CANSAT_LEN_M * 1.55))

        if not self.has_data:
            p.setPen(QPen(QColor(T.fg4)))
            p.setFont(font_display(9, tracking=1.6))
            p.drawText(self.rect(), Qt.AlignCenter, "Awaiting Telemetry")
            p.end()
            return

        geo = self._geo
        pr = self._project
        c_body = QColor(136, 114, 160)
        c_body_d = QColor(107, 85, 128)
        c_struc = QColor(T.fg2)
        c_dim = QColor(T.fg4)

        def draw_ring(pts, pen):
            p.setPen(pen)
            path = QPainterPath()
            for j, pt in enumerate(pts):
                q, _ = pr(pt, cx, cy, s)
                path.moveTo(q) if j == 0 else path.lineTo(q)
            path.closeSubpath()
            p.drawPath(path)

        def draw_stringers(r0, r1, pen, step=3):
            p.setPen(pen)
            for j in range(0, min(len(r0), len(r1)), step):
                q0, _ = pr(r0[j], cx, cy, s)
                q1, _ = pr(r1[j], cx, cy, s)
                p.drawLine(q0, q1)

        # bottom plate
        for r in geo["bot"]:
            draw_ring(r, QPen(c_struc, 1.3))
        draw_stringers(geo["bot"][0], geo["bot"][1], QPen(c_dim, 0.8))

        # honeycomb boundary rings
        for r in geo["hex_bnd"]:
            draw_ring(r, QPen(c_body, 1.0))

        # honeycomb cells
        p.setPen(QPen(c_body, 0.7))
        hex_path = QPainterPath()
        for verts in geo["hex"]:
            pts = [pr(v, cx, cy, s)[0] for v in verts]
            hex_path.moveTo(pts[0])
            for j in range(1, 6):
                hex_path.lineTo(pts[j])
            hex_path.closeSubpath()
        p.drawPath(hex_path)

        # transition band
        for r in geo["trans"]:
            draw_ring(r, QPen(c_struc, 1.3))
        draw_stringers(geo["trans"][0], geo["trans"][1], QPen(c_struc, 0.8), 2)

        # slatted section rings
        for r in geo["sl_rings"]:
            draw_ring(r, QPen(c_body, 1.0))

        # vertical slats
        p.setPen(QPen(c_body, 0.6))
        slat_path = QPainterPath()
        for sl in geo["slats"]:
            q0, _ = pr(sl[0], cx, cy, s)
            q1, _ = pr(sl[1], cx, cy, s)
            slat_path.moveTo(q0)
            slat_path.lineTo(q1)
        p.drawPath(slat_path)

        # top plate
        for r in geo["top"]:
            draw_ring(r, QPen(c_struc, 1.3))
        draw_stringers(geo["top"][0], geo["top"][1], QPen(c_dim, 0.8))

        # top plate cutouts
        p.setPen(QPen(c_body_d, 0.7))
        cut_path = QPainterPath()
        for verts in geo["cuts"]:
            pts = [pr(v, cx, cy, s)[0] for v in verts]
            cut_path.moveTo(pts[0])
            for j in range(1, len(pts)):
                cut_path.lineTo(pts[j])
            cut_path.closeSubpath()
        p.drawPath(cut_path)

        # antenna connectors
        p.setPen(QPen(c_struc, 1.0))
        for box in geo["ants"]:
            bp_ = [pr(v, cx, cy, s)[0] for v in box]
            for j in range(4):
                p.drawLine(bp_[j], bp_[(j + 1) % 4])
            for j in range(4, 8):
                p.drawLine(bp_[j], bp_[4 + (j - 3) % 4])
            for j in range(4):
                p.drawLine(bp_[j], bp_[j + 4])

        # nose indicator
        tip, _ = pr((0, 0, geo["L"] / 2 + 0.05), cx, cy, s)
        base, _ = pr((0, 0, geo["L"] / 2), cx, cy, s)
        p.setPen(QPen(QColor(T.fg), 1.6))
        p.drawLine(base, tip)
        p.setBrush(QBrush(QColor(T.fg)))
        p.setPen(Qt.NoPen)
        p.drawEllipse(tip, 2.6, 2.6)

        # body axes
        p.setFont(font_mono(7))
        for vec, lab in (((0.09, 0, 0), "X"), ((0, 0.09, 0), "Y"), ((0, 0, 0.09), "Z")):
            q, _ = pr(vec, 26, h - 26, s * 0.42)
            o, _ = pr((0, 0, 0), 26, h - 26, s * 0.42)
            p.setPen(QPen(QColor(T.fg3), 1.0))
            p.drawLine(o, q)
            p.drawText(QRectF(q.x() - 6, q.y() - 12, 14, 12), Qt.AlignCenter, lab)
        p.end()


class GroundTrack(QWidget):
    """Interactive 2D map: real tile background, zoom/pan, Google-Maps-style pins.

    Controls
    --------
    Scroll wheel   — zoom in / out (centred on cursor)
    Left drag      — pan
    Double-click   — reset to full-tile view
    """

    # Exact bounds of 2dmap.jpg from Google Earth placemark pins
    BG_NORTH = 31.260304
    BG_SOUTH = 31.246571
    BG_WEST  = 75.690831
    BG_EAST  = 75.714928

    def __init__(self, parent=None):
        super().__init__(parent)
        self.pts       = []           # (lat, lon, state, alt)
        self.marks     = []           # (lat, lon, label)
        self.sigma_m   = None
        self.corrected = False
        self.proj      = None         # projected touchdown (lat, lon)
        self.search_m  = None

        # view state — zoom 1 = full tile, higher = zoomed in
        self._zoom        = 1.0
        self._clat        = (self.BG_NORTH + self.BG_SOUTH) / 2
        self._clon        = (self.BG_WEST  + self.BG_EAST)  / 2
        self._drag_start  = None
        self._drag_center = None
        self._last_plot   = QRectF(58, 12, 400, 300)   # sane fallback until first paint

        self._bg = None               # QPixmap | False (not found)
        self._tile_dir = None         # Path | False (no cache)
        self._tile_zooms = []
        self._tile_cache = {}         # (z,x,y) -> QPixmap | None
        self.me = None                # live ground-station position (lat, lon)
        self.setMinimumHeight(180)
        self.setMouseTracking(True)
        self.setCursor(Qt.OpenHandCursor)

    # ── public API (unchanged from original) ────────────────────────────────
    def set_projection(self, lat, lon):
        self.proj = None if lat is None else (lat, lon)

    def set_search_radius(self, r_m):
        self.search_m = r_m

    def set_uncertainty(self, sigma_m, corrected=False):
        self.sigma_m   = sigma_m
        self.corrected = corrected

    def add(self, lat, lon, state=0, alt=0.0):
        if lat == 0 and lon == 0:
            return
        if (self.pts and abs(self.pts[-1][0] - lat) < 1e-9
                     and abs(self.pts[-1][1] - lon) < 1e-9):
            return
        self.pts.append((lat, lon, int(state), alt))

    def mark(self, lat, lon, label):
        self.marks.append((lat, lon, label))

    def set_me(self, lat, lon):
        """Live operator position, from the ground-station GNSS receiver."""
        self.me = None if lat is None else (lat, lon)

    def clear(self):
        self.pts.clear(); self.marks.clear(); self.update()

    # ── coordinate transform ────────────────────────────────────────────────
    def _plot_rect(self):
        pad_l, pad_r, pad_t, pad_b = 58, 14, 12, 26
        return QRectF(pad_l, pad_t,
                      max(10, self.width()  - pad_l - pad_r),
                      max(10, self.height() - pad_t - pad_b))

    def _kx(self):
        return math.cos(math.radians(self._clat)) or 1e-9

    def _spans(self, plot):
        """Returns (span_lat, span_lon_corrected) in degrees visible right now."""
        sl = (self.BG_NORTH - self.BG_SOUTH) / self._zoom
        sw = sl * (plot.width() / max(plot.height(), 1)) / self._kx()
        return sl, sw

    def _xy(self, la, lo, plot=None):
        if plot is None:
            plot = self._last_plot
        sl, sw = self._spans(plot)
        px = (plot.left()  + plot.width()  / 2
              + (lo - self._clon) / sw * plot.width())
        py = (plot.top()   + plot.height() / 2
              - (la - self._clat) / sl * plot.height())
        return QPointF(px, py)

    def _inv(self, px, py, plot=None):
        if plot is None:
            plot = self._last_plot
        sl, sw = self._spans(plot)
        lo = self._clon + (px - plot.left()  - plot.width()  / 2) / plot.width()  * sw
        la = self._clat - (py - plot.top()   - plot.height() / 2) / plot.height() * sl
        return la, lo

    # ── zoom / pan input events ─────────────────────────────────────────────
    def wheelEvent(self, e):
        plot = self._last_plot
        mx, my = e.x(), e.y()
        la0, lo0 = self._inv(mx, my, plot)           # geo under cursor before
        factor = 1.18 if e.angleDelta().y() > 0 else (1 / 1.18)
        self._zoom = max(0.5, min(self._zoom * factor, 120.0))
        la1, lo1 = self._inv(mx, my, plot)           # geo under cursor after
        self._clat += la0 - la1                      # re-centre so cursor stays fixed
        self._clon += lo0 - lo1
        self.update()
        e.accept()

    def mousePressEvent(self, e):
        if e.button() == Qt.LeftButton:
            self._drag_start  = e.pos()
            self._drag_center = (self._clat, self._clon)
            self.setCursor(Qt.ClosedHandCursor)

    def mouseMoveEvent(self, e):
        if self._drag_start and self._drag_center:
            plot = self._last_plot
            sl, sw = self._spans(plot)
            dx = e.x() - self._drag_start.x()
            dy = e.y() - self._drag_start.y()
            self._clat = self._drag_center[0] + dy / plot.height() * sl
            self._clon = self._drag_center[1] - dx / plot.width()  * sw
            self.update()

    def mouseReleaseEvent(self, e):
        if e.button() == Qt.LeftButton:
            self._drag_start = self._drag_center = None
            self.setCursor(Qt.OpenHandCursor)

    def mouseDoubleClickEvent(self, _):
        """Double-click resets to full-tile view."""
        self._zoom = 1.0
        self._clat = (self.BG_NORTH + self.BG_SOUTH) / 2
        self._clon = (self.BG_WEST  + self.BG_EAST)  / 2
        self.update()

    # ── tile lazy-load ──────────────────────────────────────────────────────
    def _load_bg(self):
        if self._bg is not None:
            return
        from PyQt5.QtGui import QPixmap
        import sys
        bases = []
        # PyInstaller onefile bundles datas into a temp extraction dir named
        # in sys._MEIPASS — this is where an embedded asset actually lives at
        # runtime, NOT next to the .exe. Check this FIRST so a bundled map
        # always works with zero manual setup after a rebuild.
        meipass = getattr(sys, "_MEIPASS", None)
        if meipass:
            bases.append(Path(meipass))
        if getattr(sys, "frozen", False):
            bases.append(Path(sys.executable).parent)   # manual override next to exe
        else:
            bases.append(Path(__file__).parent)
        bases.append(Path.cwd())        # fallback: wherever the shell was launched from
        for base in bases:
            path = base / "2dmap.jpg"
            if path.exists():
                px = QPixmap(str(path))
                if not px.isNull():
                    self._bg = px
                    log.info("map tile loaded from %s", path)
                    return
        log.warning("2dmap.jpg not found in any of: %s — map background will be solid colour",
                     [str(b) for b in bases])
        self._bg = False

    # ── offline XYZ tile cache ──────────────────────────────────────────────
    # Built by fetch_map.py at home, on wifi. Nothing here ever touches the
    # network: a field ground station has no internet, which is precisely when
    # the map matters. Layout is the standard slippy-map one,
    # map_tiles/<z>/<x>/<y>.png, so the cache is inspectable and reusable.
    _TILE_PX = 256

    @staticmethod
    def _lon2tilex(lon, z):
        return (lon + 180.0) / 360.0 * (2.0 ** z)

    @staticmethod
    def _lat2tiley(lat, z):
        la = math.radians(max(-85.05112878, min(85.05112878, lat)))
        return (1.0 - math.asinh(math.tan(la)) / math.pi) / 2.0 * (2.0 ** z)

    @staticmethod
    def _tiley2lat(y, z):
        return math.degrees(math.atan(math.sinh(
            math.pi * (1.0 - 2.0 * y / (2.0 ** z)))))

    @staticmethod
    def _tilex2lon(x, z):
        return x / (2.0 ** z) * 360.0 - 180.0

    def _tile_root(self):
        """Locate map_tiles/ the same way _load_bg locates 2dmap.jpg."""
        if self._tile_dir is not None:
            return self._tile_dir
        bases = []
        meipass = getattr(sys, "_MEIPASS", None)
        if meipass:
            bases.append(Path(meipass))
        if getattr(sys, "frozen", False):
            bases.append(Path(sys.executable).parent)
        else:
            bases.append(Path(__file__).parent)
        bases.append(Path.cwd())
        for base in bases:
            d = base / "map_tiles"
            if d.is_dir():
                self._tile_dir = d
                zs = sorted(int(p.name) for p in d.iterdir()
                            if p.is_dir() and p.name.isdigit())
                self._tile_zooms = zs
                log.info("map tile cache at %s, zoom levels %s", d, zs)
                return d
        self._tile_dir = False
        self._tile_zooms = []
        return False

    def _pick_zoom(self, plot):
        """Choose the cached zoom whose native scale is closest to what the
        current view is showing. Picking the highest available instead would
        drag thousands of tiles through QPainter for a zoomed-out view."""
        if not self._tile_zooms:
            return None
        sl, _ = self._spans(plot)
        if sl <= 0:
            return None
        # Degrees of LATITUDE per screen pixel -> the zoom whose tiles are
        # natively that scale. A Mercator tile spans 360/2^z degrees of
        # longitude but only (360/2^z)·cos(lat) degrees of latitude, so the
        # cos(lat) term belongs in here — at 31 N it is worth a fifth of a zoom
        # level, which is small but free to get right.
        deg_per_px = sl / max(plot.height(), 1)
        if deg_per_px <= 0:
            return self._tile_zooms[-1]
        coslat = max(math.cos(math.radians(self._clat)), 1e-6)
        ideal = math.log2(360.0 * coslat / (deg_per_px * self._TILE_PX))
        return min(self._tile_zooms, key=lambda z: abs(z - ideal))

    def _draw_tiles(self, p, plot):
        """Paint the visible tiles. Returns False if there is no usable cache,
        so the caller can fall back."""
        if self._tile_root() is False:
            return False
        z = self._pick_zoom(plot)
        if z is None:
            return False

        # visible geographic window -> tile index window
        sl, sw = self._spans(plot)
        north, south = self._clat + sl / 2, self._clat - sl / 2
        west,  east = self._clon - sw / 2, self._clon + sw / 2
        x0 = int(math.floor(self._lon2tilex(west,  z)))
        x1 = int(math.ceil(self._lon2tilex(east,  z)))
        y0 = int(math.floor(self._lat2tiley(north, z)))
        y1 = int(math.ceil(self._lat2tiley(south, z)))
        n = 2 ** z
        x0, x1 = max(0, x0), min(n, x1)
        y0, y1 = max(0, y0), min(n, y1)
        # A pathological zoom/pan could ask for the whole world; refuse rather
        # than lock the UI thread painting 65 000 tiles.
        if (x1 - x0) * (y1 - y0) > 400:
            return False

        drawn = 0
        for tx in range(x0, x1):
            for ty in range(y0, y1):
                px = self._tile_pixmap(z, tx, ty)
                if px is None:
                    continue
                # Each tile is drawn into the rect its own corners map to. The
                # view transform is equirectangular and the tiles are Mercator,
                # which is only exactly right at the tile's own latitude — but
                # over a 10 km area the disagreement is far below one pixel, and
                # doing it per-tile rather than per-view keeps it that way.
                tl = self._xy(self._tiley2lat(ty, z),
                              self._tilex2lon(tx, z), plot)
                br = self._xy(self._tiley2lat(ty + 1, z),
                              self._tilex2lon(tx + 1, z), plot)
                r = QRectF(tl, br).toAlignedRect()
                if r.width() > 0 and r.height() > 0:
                    p.drawPixmap(r, px)
                    drawn += 1
        return drawn > 0

    def _tile_pixmap(self, z, x, y):
        """LRU-ish pixmap cache. Decoding a PNG per tile per repaint would
        make panning crawl; the dict is bounded so a long pan cannot grow it
        without limit."""
        key = (z, x, y)
        if key in self._tile_cache:
            return self._tile_cache[key]
        if len(self._tile_cache) > 600:
            self._tile_cache.clear()
        path = self._tile_dir / str(z) / str(x) / ("%d.png" % y)
        px = None
        if path.exists():
            from PyQt5.QtGui import QPixmap
            cand = QPixmap(str(path))
            if not cand.isNull():
                px = cand
        self._tile_cache[key] = px          # cache misses too, so we stat once
        return px

    # ── pin helpers ─────────────────────────────────────────────────────────
    @staticmethod
    def _draw_pin(p, q, color, size=10, label=""):
        """Google-Maps teardrop pin (circle body + downward point)."""
        r = size
        cx, cy = q.x(), q.y() - r * 1.6        # circle centre above tip
        # shadow
        p.setPen(Qt.NoPen)
        p.setBrush(QBrush(QColor(0, 0, 0, 60)))
        p.drawEllipse(QPointF(cx + 1.5, cy + 1.5), r, r)
        # body circle
        p.setPen(QPen(QColor(255, 255, 255, 200), 1.5))
        p.setBrush(QBrush(color))
        p.drawEllipse(QPointF(cx, cy), r, r)
        # pointer triangle
        poly = QPolygonF([
            QPointF(cx - r * 0.42, cy + r * 0.55),
            QPointF(cx + r * 0.42, cy + r * 0.55),
            QPointF(q.x(), q.y()),
        ])
        p.setPen(Qt.NoPen)
        p.setBrush(QBrush(color))
        p.drawPolygon(poly)
        # white dot inside circle
        p.setBrush(QBrush(QColor(255, 255, 255, 230)))
        p.drawEllipse(QPointF(cx, cy), r * 0.38, r * 0.38)
        # label
        if label:
            p.setPen(QPen(QColor(T.fg)))
            p.setFont(font_display(7.5, bold=True, tracking=1.1))
            p.drawText(QRectF(cx + r + 5, cy - 7, 80, 14),
                       Qt.AlignLeft | Qt.AlignVCenter, label)

    @staticmethod
    def _draw_live(p, q, size=9):
        """Pulsing current-fix dot: two concentric rings + filled core."""
        r = size
        p.setPen(QPen(QColor(255, 90, 30, 80), 1.5))
        p.setBrush(Qt.NoBrush)
        p.drawEllipse(q, r * 2.2, r * 2.2)
        p.setPen(QPen(QColor(255, 90, 30, 180), 2.0))
        p.drawEllipse(q, r, r)
        p.setPen(Qt.NoPen)
        p.setBrush(QBrush(QColor(255, 60, 20, 240)))
        p.drawEllipse(q, r * 0.46, r * 0.46)

    # ── main paint ──────────────────────────────────────────────────────────
    def paintEvent(self, _):
        p = QPainter(self)
        p.setRenderHint(QPainter.Antialiasing, True)
        plot = self._plot_rect()
        self._last_plot = plot
        pad_l = 58

        p.setPen(QPen(QColor(T.line_dim), 1))
        p.drawRect(plot)

        # ── background: XYZ tile cache first, single image second ────────
        # The tile cache is preferred because it is the only one of the two that
        # is not pinned to one campus: 2dmap.jpg carries hardcoded LPU bounds,
        # so at any other launch site it is not merely missing, it is WRONG —
        # it would paint Phagwara underneath a vehicle sitting in Kushinagar.
        # Run fetch_map.py for the real launch area and this path takes over.
        p.setClipRect(plot)
        p.fillRect(plot, QColor(T.surf))
        if not self._draw_tiles(p, plot):
            self._load_bg()
            if self._bg:
                tl = self._xy(self.BG_NORTH, self.BG_WEST,  plot)
                br = self._xy(self.BG_SOUTH, self.BG_EAST,  plot)
                p.drawPixmap(QRectF(tl, br).toRect(), self._bg)

        # ── graticule (semi-transparent so it floats over the tile) ─────
        p.setFont(font_mono(7))
        sl, sw = self._spans(plot)
        for i in range(5):
            fr  = i / 4.0
            la_line = self._clat + (0.5 - fr) * sl
            lo_line = self._clon + (fr - 0.5) * sw
            y = plot.top()  + fr * plot.height()
            x = plot.left() + fr * plot.width()
            p.setPen(QPen(QColor(255, 255, 255, 45), 1, Qt.DotLine))
            p.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y))
            p.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()))
            p.setPen(QPen(QColor(T.fg3)))
            p.drawText(QRectF(2, y - 7, pad_l - 6, 14),
                       Qt.AlignRight | Qt.AlignVCenter,
                       f"{la_line:.5f}")
            p.drawText(QRectF(x - 38, plot.bottom() + 3, 76, 13),
                       Qt.AlignCenter, f"{lo_line:.5f}")

        if not self.pts:
            p.setClipping(False)
            p.setPen(QPen(QColor(T.fg4)))
            p.setFont(font_display(8.5, tracking=1.5))
            p.drawText(plot, Qt.AlignCenter, "AWAITING GNSS FIX")
            p.end()
            return

        # ── flight path ──────────────────────────────────────────────────
        path = QPainterPath()
        for i, (la, lo, _s, _a) in enumerate(self.pts):
            q = self._xy(la, lo, plot)
            path.moveTo(q) if i == 0 else path.lineTo(q)
        p.setPen(QPen(QColor(T.fg2), 1.6))
        p.setBrush(Qt.NoBrush)
        p.drawPath(path)

        # state-transition squares
        prev = None
        for la, lo, st, _a in self.pts:
            if prev is not None and st != prev:
                q = self._xy(la, lo, plot)
                p.setPen(QPen(QColor(T.fg2), 1.2))
                p.setBrush(Qt.NoBrush)
                p.drawRect(QRectF(q.x() - 3, q.y() - 3, 6, 6))
            prev = st

        # custom × marks
        for la, lo, lab in self.marks:
            q = self._xy(la, lo, plot)
            p.setPen(QPen(QColor(T.fg3), 1))
            p.drawLine(QPointF(q.x()-4,q.y()-4), QPointF(q.x()+4,q.y()+4))
            p.drawLine(QPointF(q.x()-4,q.y()+4), QPointF(q.x()+4,q.y()-4))

        la_c, lo_c, st_c, alt_c = self.pts[-1]
        q_cur = self._xy(la_c, lo_c, plot)

        # 95 % containment circle
        if self.sigma_m:
            m_per_px = sl * 111320.0 / plot.height()
            r_px = r95_m(self.sigma_m) / m_per_px
            if r_px > 2.0:
                p.setPen(QPen(QColor(T.fg3), 1, Qt.DashLine))
                p.setBrush(Qt.NoBrush)
                p.drawEllipse(q_cur, r_px, r_px)
                p.setFont(font_display(6.8, tracking=1.0))
                p.setPen(QPen(QColor(T.fg4)))
                p.drawText(QRectF(q_cur.x()-60, q_cur.y()+r_px+2, 120, 12),
                           Qt.AlignCenter, f"95% ±{r95_m(self.sigma_m):.1f} m")

        # projected touchdown + recovery sweep circle
        if self.proj:
            pq = self._xy(*self.proj, plot)
            if self.search_m:
                m_per_px = sl * 111320.0 / plot.height()
                sr = self.search_m / m_per_px
                if sr > 2.0:
                    p.setPen(QPen(QColor(T.amber), 1, Qt.DotLine))
                    p.setBrush(Qt.NoBrush)
                    p.drawEllipse(pq, sr, sr)
            p.setPen(QPen(QColor(T.amber), 1.2, Qt.DashLine))
            p.drawLine(q_cur, pq)
            p.setPen(QPen(QColor(T.amber), 1.6))
            p.setBrush(Qt.NoBrush)
            sz = 8
            p.drawLine(QPointF(pq.x()-sz, pq.y()), QPointF(pq.x()+sz, pq.y()))
            p.drawLine(QPointF(pq.x(), pq.y()-sz), QPointF(pq.x(), pq.y()+sz))
            p.setFont(font_display(7, tracking=1.1))
            p.drawText(QRectF(pq.x()+sz+3, pq.y()-14, 120, 12),
                       Qt.AlignLeft | Qt.AlignVCenter, "PROJECTED")

        p.setClipping(False)

        # ── PAD pin (launch location — first GPS fix) ─────────────────────
        q0 = self._xy(*self.pts[0][:2], plot)
        self._draw_pin(p, q0, QColor(30, 120, 255), size=9, label="PAD")

        # ── LIVE dot (current / last known fix) ───────────────────────────
        self._draw_live(p, q_cur, size=8)
        p.setPen(QPen(QColor(T.fg)))
        p.setFont(font_mono(7.5))
        alt_txt = "0" if f"{alt_c:.0f}" == "-0" else f"{alt_c:.0f}"
        p.drawText(QRectF(q_cur.x()+13, q_cur.y()-9, 190, 14),
                   Qt.AlignLeft | Qt.AlignVCenter,
                   f"{STATES[min(st_c, len(STATES)-1)]}  {alt_txt} m")

        # ── ME pin + walk-in line ─────────────────────────────────────────
        # Fed from the ground-station GNSS receiver, so this is where the
        # OPERATOR is, live, not where the pad is. The line and its label turn
        # the map from "here is roughly where it came down" into something you
        # can walk: a bearing to hold and a distance that counts down as you go.
        if self.me:
            qm = self._xy(self.me[0], self.me[1], plot)
            p.setPen(QPen(QColor(60, 220, 130, 150), 1.6, Qt.DashLine))
            p.drawLine(qm, q_cur)
            d_m = Physics.hav_dist(self.me[0], self.me[1], *self.pts[-1][:2])
            brg = Physics.bearing(self.me[0], self.me[1], *self.pts[-1][:2])
            mid = QPointF((qm.x() + q_cur.x()) / 2, (qm.y() + q_cur.y()) / 2)
            p.setPen(QPen(QColor(60, 220, 130)))
            p.setFont(font_mono(7.5))
            txt = (f"{d_m:.0f} m  {brg:.0f}°" if d_m < 1000
                   else f"{d_m / 1000:.2f} km  {brg:.0f}°")
            p.drawText(QRectF(mid.x() + 6, mid.y() - 8, 150, 14),
                       Qt.AlignLeft | Qt.AlignVCenter, txt)
            self._draw_pin(p, qm, QColor(60, 220, 130), size=9, label="ME")

        # ── scale bar ────────────────────────────────────────────────────
        m_per_px = sl * 111320.0 / plot.height()
        n, bar_label = 10, "10 m"
        bar_px = 10.0 / m_per_px
        while bar_px < 22 and n < 100000:
            n *= 10; bar_px *= 10
            bar_label = f"{n} m" if n < 1000 else f"{n // 1000} km"
        bar_px = max(22.0, min(bar_px, 120.0))
        bx, by = plot.right() - bar_px - 10, plot.bottom() - 10
        p.setPen(QPen(QColor(T.fg3), 1.2))
        p.drawLine(QPointF(bx, by), QPointF(bx+bar_px, by))
        p.drawLine(QPointF(bx, by-3), QPointF(bx, by+3))
        p.drawLine(QPointF(bx+bar_px, by-3), QPointF(bx+bar_px, by+3))
        p.setFont(font_display(7, tracking=1))
        p.drawText(QRectF(bx, by-17, bar_px, 13), Qt.AlignCenter, bar_label)

        # ── zoom level badge ─────────────────────────────────────────────
        p.setPen(QPen(QColor(T.fg4)))
        p.setFont(font_display(7, tracking=1))
        p.drawText(QRectF(plot.right()-58, plot.top()+4, 52, 12),
                   Qt.AlignRight | Qt.AlignVCenter, f"×{self._zoom:.1f}")

        p.end()


class SpectralBars(QWidget):
    """AS7343 14-channel bars. Colour maps to wavelength — one of the few
    places the design language permits it, because the hue IS the data."""

    BANDS = ["405", "425", "450", "475", "515", "550",
             "555", "600", "640", "690", "745", "855"]

    def __init__(self, parent=None):
        super().__init__(parent)
        self.vals = [0.0] * 12
        self.present = False
        self.setMinimumHeight(150)

    def set_vals(self, vals, present=True):
        self.vals = list(vals)[:12] + [0.0] * max(0, 12 - len(vals))
        self.present = present
        self.update()

    def paintEvent(self, _):
        p = QPainter(self)
        p.setRenderHint(QPainter.Antialiasing, False)
        w, h = self.width(), self.height()
        base = h - 34          # room for the band ticks AND the axis caption
        if not self.present:
            p.setPen(QPen(QColor(T.fg4)))
            p.setFont(font_display(8.5, tracking=1.4))
            p.drawText(self.rect(), Qt.AlignCenter, "NO SPECTRAL DATA")
            p.end()
            return
        peak = max(self.vals) or 1.0
        n = len(self.vals)
        bw = w / n
        p.setPen(QPen(QColor(T.line_dim), 1))
        p.drawLine(QPointF(0, base), QPointF(w, base))
        for i, v in enumerate(self.vals):
            bh = max(1.0, (v / peak) * (base - 16))
            x = i * bw + bw * 0.18
            bwid = bw * 0.64
            p.fillRect(QRectF(x, base - bh, bwid, bh), QColor(T.SPECTRAL[i]))
            p.setPen(QPen(QColor(T.fg3)))
            p.setFont(font_mono(6.5))
            p.drawText(QRectF(i * bw, base + 3, bw, 12), Qt.AlignCenter,
                       self.BANDS[i])
        p.setPen(QPen(QColor(T.fg3)))
        p.setFont(font_display(7, tracking=1.1))
        p.drawText(QRectF(0, base + 18, w, 13), Qt.AlignCenter,
                   "WAVELENGTH  nm")
        p.end()


class LimitBar(QWidget):
    """Where a value sits inside its limit envelope. The bands are drawn as
    tick marks, not filled colour, so the widget stays quiet until the
    marker actually enters a coloured zone."""

    def __init__(self, key, parent=None):
        super().__init__(parent)
        self.key = key
        self.meta = FIELD_META.get(key, {})
        self.value = None
        self.state = ST_NODATA
        self.setFixedHeight(26)

    def set(self, v, state):
        self.value, self.state = v, state
        self.update()

    def paintEvent(self, _):
        p = QPainter(self)
        p.setRenderHint(QPainter.Antialiasing, True)
        w, h = self.width(), self.height()
        lim = self.meta.get("lim")
        y = h * 0.62
        p.setPen(QPen(QColor(T.line_dim), 1))
        p.drawLine(QPointF(0, y), QPointF(w, y))
        if not lim:
            p.end()
            return
        rl, yl, yh, rh = lim
        known = [v for v in (rl, yl, yh, rh) if v is not None]
        if not known:
            p.end()
            return
        lo = min(known)
        hi = max(known)
        pad = (hi - lo) * 0.35 or 1.0
        lo, hi = lo - pad, hi + pad

        def X(v):
            return max(0.0, min(1.0, (v - lo) / (hi - lo))) * w

        p.setFont(font_mono(6.2))
        for v, c in ((rl, T.red), (yl, T.amber), (yh, T.amber), (rh, T.red)):
            if v is None:
                continue
            x = X(v)
            p.setPen(QPen(QColor(c), 1.2))
            p.drawLine(QPointF(x, y - 5), QPointF(x, y + 5))
            p.setPen(QPen(QColor(T.fg4)))
            p.drawText(QRectF(x - 22, y + 6, 44, 11), Qt.AlignCenter,
                       fmt_value(v, self.key))
        if self.value is not None:
            x = X(self.value)
            ink = state_colour(self.state)
            p.setPen(QPen(QColor(ink), 1.6))
            p.setBrush(QBrush(QColor(ink)))
            p.drawPolygon(QPolygonF([QPointF(x, y - 1), QPointF(x - 4, y - 9),
                                     QPointF(x + 4, y - 9)]))
        p.end()


class StateStepper(QWidget):
    """Flight sequence. The reached states are white, the pending ones are
    dim; the active one carries a filled marker."""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.state = -1
        self.setFixedHeight(46)

    def set_state(self, s):
        if s != self.state:
            self.state = s
            self.update()

    def paintEvent(self, _):
        p = QPainter(self)
        p.setRenderHint(QPainter.Antialiasing, True)
        w, h = self.width(), self.height()
        n = len(STATES)
        seg = w / n
        y = h * 0.40
        for i, name in enumerate(STATES):
            cx = seg * (i + 0.5)
            done = self.state >= i
            active = self.state == i
            if i < n - 1:
                p.setPen(QPen(QColor(T.fg2 if self.state > i else T.line_dim), 1.2))
                p.drawLine(QPointF(cx + 9, y), QPointF(seg * (i + 1.5) - 9, y))
            p.setPen(QPen(QColor(T.fg if done else T.line), 1.3))
            p.setBrush(QBrush(QColor(T.fg)) if active else Qt.NoBrush)
            p.drawEllipse(QPointF(cx, y), 6.5, 6.5)
            if done and not active:
                p.setBrush(QBrush(QColor(T.fg2)))
                p.setPen(Qt.NoPen)
                p.drawEllipse(QPointF(cx, y), 2.6, 2.6)
            p.setFont(font_display(7.6, bold=active, tracking=1.2))
            p.setPen(QPen(QColor(T.fg if done else T.fg4)))
            p.drawText(QRectF(seg * i, y + 11, seg, 14), Qt.AlignCenter, name)
        p.end()


# -------------------------------------------------------------------------
# pyqtgraph configuration + time-series plot
# -------------------------------------------------------------------------
pg.setConfigOption("background", T.surf)
pg.setConfigOption("foreground", T.fg3)
pg.setConfigOptions(antialias=True)


class TimePlot(pg.PlotWidget):
    """One scientific time series.

    Fixes the defects found in the previous build:
      * the series name lives in the panel header, never inside the canvas
        where it collided with the x axis;
      * the y axis autoscales to the data's actual range instead of being
        pinned to zero, so a 101325 -> 99000 Pa excursion is visible rather
        than a flat line at the top of a 0-110000 axis;
      * the grid is actually drawn, at a legible opacity.
    """

    def __init__(self, key, title=None, ink=None, parent=None):
        super().__init__(parent)
        self.key = key
        meta = FIELD_META.get(key, {})
        self.unit = meta.get("u", "")
        self.ink = ink or T.fg
        self.t = deque(maxlen=4000)
        self.v = deque(maxlen=4000)

        self.setMinimumHeight(130)
        self.showGrid(x=True, y=True, alpha=0.16)
        self.setMenuEnabled(False)
        pi = self.getPlotItem()
        pi.setContentsMargins(2, 6, 8, 2)
        for ax in ("left", "bottom"):
            a = pi.getAxis(ax)
            a.setPen(pg.mkPen(T.line_dim, width=1))
            a.setTextPen(pg.mkPen(T.fg3))
            a.setStyle(tickFont=font_mono(7), tickLength=-4)
        pi.getAxis("left").setWidth(82)
        pi.getAxis("bottom").setHeight(24)
        pi.getAxis("left").setLabel(
            f"{title or key}" + (f"  [{self.unit}]" if self.unit else ""),
            **{"color": T.fg3, "font-size": "7.5pt"})
        pi.hideButtons()

        self.curve = self.plot([], [], pen=pg.mkPen(self.ink, width=1.4))
        self._lim_lines = []
        self._add_limit_lines(meta.get("lim"))

    def _add_limit_lines(self, lim):
        """Draw the alarm envelope as faint dashed rules so an operator can
        see how close a value is to its limit without reading numbers."""
        if not lim:
            return
        rl, yl, yh, rh = lim
        for v, c in ((rl, T.red), (yl, T.amber), (yh, T.amber), (rh, T.red)):
            if v is None:
                continue
            ln = pg.InfiniteLine(pos=v, angle=0, movable=False,
                                 pen=pg.mkPen(c, width=1, style=Qt.DashLine))
            ln.setOpacity(0.42)
            # ignoreBounds: the envelope must not drag the autorange. Without
            # it an ALT_M limit at 1500 m forced the axis to 1500 and flattened
            # the actual 0-40 m data into an unreadable line.
            self.addItem(ln, ignoreBounds=True)
            self._lim_lines.append(ln)

    def push(self, t, v):
        if v is None:
            return
        self.t.append(t)
        self.v.append(v)

    def redraw(self):
        if not self.t:
            return
        self.curve.setData(list(self.t), list(self.v))

    def clear_data(self):
        self.t.clear()
        self.v.clear()
        self.curve.setData([], [])


# =========================================================================
# §11b  COM-PORT AUTO-DETECT
# =========================================================================
# Opening a port on an ESP32 dev board pulses DTR/RTS and reboots it, so the
# gateway's "// [VRITRA-GS]" banner arrives within ~1.5 s of the probe opening
# the port. A Teensy streaming flight CSV over USB is recognised by TEAM_ID.
AUTODETECT_LISTEN_S = 3.0
GATEWAY_MARKERS = ("[VRITRA-GS]", "// [RX]", "// [RF]", "// [UP]", "// [WIRE]")


def classify_serial_line(line: str):
    """One received line -> 'GATEWAY', 'TELEMETRY' or None."""
    if any(m in line for m in GATEWAY_MARKERS):
        return "GATEWAY"
    if line.startswith(TEAM_ID + ","):
        return "TELEMETRY"
    return None


def probe_port(port, baud, listen_s=AUTODETECT_LISTEN_S, stop_evt=None):
    """Listen on `port` for `listen_s` seconds and say what is talking on it.

    Read-only: nothing is ever written, so a probe cannot send a telecommand.
    Returns 'GATEWAY', 'TELEMETRY' or None (silent, busy or not openable).
    """
    if not HAVE_SERIAL:
        return None
    try:
        ser = serial.Serial(port, baud, timeout=0.2)
    except (serial.SerialException, OSError, ValueError) as e:
        log.info("autodetect: %s not openable (%s)", port, e)
        return None
    buf = bytearray()
    deadline = time.monotonic() + listen_s
    try:
        while time.monotonic() < deadline:
            if stop_evt is not None and stop_evt.is_set():
                return None
            buf.extend(ser.read(512))
            while b"\n" in buf:
                i = buf.index(b"\n")
                line = buf[:i].decode("utf-8", "ignore").strip()
                del buf[:i + 1]
                kind = classify_serial_line(line)
                if kind:
                    return kind
            if len(buf) > 8192:
                del buf[:4096]          # binary noise with no newline
    except (serial.SerialException, OSError) as e:
        log.info("autodetect: read failed on %s (%s)", port, e)
        return None
    finally:
        try:
            ser.close()
        except Exception:
            pass
    return None


def autodetect_candidates():
    """COM ports worth probing, USB first. Bluetooth SPP ports are skipped:
    opening one makes Windows try to page a remote device for many seconds."""
    if not HAVE_SERIAL:
        return []
    try:
        ports = list(serial.tools.list_ports.comports())
    except Exception:
        return []
    usb, other = [], []
    for p in ports:
        desc, hwid = p.description or "", p.hwid or ""
        if "BTHENUM" in hwid or "Bluetooth" in desc:
            continue
        (usb if (p.vid or hwid.startswith("USB")) else other).append(p.device)
    return usb + other


class PortAutoDetect(threading.Thread):
    """Probes candidate ports one after another. A gateway wins outright; a
    port carrying raw telemetry is kept as the fallback. `result` is
    (port, kind) or None and is only read by the UI after the thread ends."""

    def __init__(self, baud, ports=None):
        super().__init__(daemon=True, name="PortAutoDetect")
        self.baud = baud
        self.ports = ports
        self.result = None
        self.tried = []
        self._stop_evt = threading.Event()

    def stop(self):
        self._stop_evt.set()

    def run(self):
        ports = self.ports if self.ports is not None else autodetect_candidates()
        telemetry_hit = None
        for port in ports:
            if self._stop_evt.is_set():
                return
            kind = probe_port(port, self.baud, stop_evt=self._stop_evt)
            self.tried.append((port, kind))
            if kind == "GATEWAY":
                self.result = (port, kind)
                return
            if kind == "TELEMETRY" and telemetry_hit is None:
                telemetry_hit = (port, kind)
        self.result = telemetry_hit


# =========================================================================
# §12  MAIN WINDOW
# =========================================================================
class PortCombo(QComboBox):
    """Rescans COM ports every 1.5 seconds without closing an active connection.
    Features rich hardware identification, USB gateway priority, and virtual test feed."""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setEditable(False)
        self.setMinimumWidth(340)
        self.last_port = ""
        self._last_items = []
        self.refresh()
        
        # Background polling timer for dynamic hot-plug support
        self.poll_timer = QTimer(self)
        self.poll_timer.timeout.connect(self.refresh)
        self.poll_timer.start(1500)

    def refresh(self):
        if not HAVE_SERIAL:
            if not self.count():
                self.addItem("(pyserial not installed)", "")
            return
        try:
            ports = list(serial.tools.list_ports.comports())
        except Exception:
            ports = []
            
        usb_items = []
        standard_items = []
        bth_items = []

        for p in ports:
            vid = f"{p.vid:04X}" if p.vid else ""
            pid = f"{p.pid:04X}" if p.pid else ""
            desc = p.description or ""
            mfg = p.manufacturer or desc or "Serial Port"
            hwid = p.hwid or ""
            
            is_bth = "BTHENUM" in hwid or "Bluetooth" in desc or "Bluetooth" in mfg
            is_gateway = any(k in vid for k in ("10C4", "1A86", "0403", "16C0", "303A", "2341"))
            is_usb = bool(vid or hwid.startswith("USB") or "USB" in desc)

            if is_bth:
                label = f"{p.device}  [Bluetooth - Disconnected]"
                bth_items.append((label, p.device, False))
            elif is_gateway or is_usb:
                tag = "ESP32 Gateway" if is_gateway else "USB Serial"
                label = f"{p.device}  [{tag}] {mfg} ({vid}:{pid})" if (vid and pid) else f"{p.device}  [{tag}] {mfg}"
                usb_items.append((label, p.device, True))
            else:
                label = f"{p.device}  [{mfg}]"
                standard_items.append((label, p.device, False))

        new_items = []
        # Priority 1: Physical USB devices first
        new_items.extend(usb_items)
        # Priority 2: Virtual Loopback test feed for verification
        new_items.append(("[VIRTUAL] Virtual Gateway Feed (Loopback Verification)", "VIRTUAL_TEST", False))
        # Priority 3: Other standard serial ports
        new_items.extend(standard_items)
        # Priority 4: Bluetooth serial ports at the very end
        new_items.extend(bth_items)

        if not usb_items:
            # If no real USB device is plugged in, insert clear guide at top
            new_items.insert(0, ("(No USB Gateway Detected — Plug in ESP32 / Select Port)", "", False))

        if new_items == self._last_items:
            return
        self._last_items = new_items

        cur = self.currentData() or self.currentText()
        if cur and cur not in ("", "(No USB Gateway Detected — Plug in ESP32 / Select Port)", "(pyserial not installed)"):
            self.last_port = cur

        self.blockSignals(True)
        self.clear()
        
        best_idx = 0
        for i, (label, dev, is_prio) in enumerate(new_items):
            self.addItem(label, dev)
            if dev and dev == self.last_port:
                best_idx = i
            elif is_prio and self.last_port not in [d for _, d, _ in new_items if d]:
                best_idx = i
                self.last_port = dev

        self.setCurrentIndex(best_idx)
        self.blockSignals(False)

    def showPopup(self):
        self.refresh()
        super().showPopup()

    def selected_port(self):
        return self.currentData() or self.currentText()


# =========================================================================
# §12a  INTERACTIVE DUMMY DATA STUDIO & LAUNCHER
# =========================================================================
class SliderRow(QWidget):
    """Compact slider + spinbox parameter control row for the dummy studio."""

    def __init__(self, label, min_val, max_val, val, step=1, decimals=0, unit="", on_change=None, parent=None):
        super().__init__(parent)
        self.decimals = decimals
        self.scale = 10 ** decimals
        self.on_change = on_change

        lay = QHBoxLayout(self)
        lay.setContentsMargins(0, 2, 0, 2)
        lay.setSpacing(8)

        lbl = QLabel(label)
        lbl.setFont(font_display(8, tracking=1.2, caps=False))
        lbl.setStyleSheet(f"color: {T.fg2}; min-width: 105px; max-width: 105px;")
        lay.addWidget(lbl)

        self.slider = QSlider(Qt.Horizontal)
        self.slider.setRange(int(min_val * self.scale), int(max_val * self.scale))
        self.slider.setValue(int(val * self.scale))
        lay.addWidget(self.slider, 1)

        if decimals > 0:
            self.spin = QDoubleSpinBox()
            self.spin.setRange(min_val, max_val)
            self.spin.setSingleStep(step)
            self.spin.setDecimals(decimals)
            self.spin.setValue(val)
        else:
            self.spin = QSpinBox()
            self.spin.setRange(int(min_val), int(max_val))
            self.spin.setSingleStep(int(step))
            self.spin.setValue(int(val))

        self.spin.setFont(font_mono(8.5))
        self.spin.setFixedWidth(72)
        lay.addWidget(self.spin)

        if unit:
            u_lbl = QLabel(unit)
            u_lbl.setFont(font_mono(7.5))
            u_lbl.setStyleSheet(f"color: {T.fg3}; min-width: 32px;")
            lay.addWidget(u_lbl)

        self.slider.valueChanged.connect(self._on_slider)
        self.spin.valueChanged.connect(self._on_spin)

    def _on_slider(self, ival):
        val = ival / self.scale if self.decimals > 0 else ival
        self.spin.blockSignals(True)
        self.spin.setValue(val)
        self.spin.blockSignals(False)
        if self.on_change:
            self.on_change(val)

    def _on_spin(self, val):
        ival = int(round(val * self.scale)) if self.decimals > 0 else int(val)
        self.slider.blockSignals(True)
        self.slider.setValue(ival)
        self.slider.blockSignals(False)
        if self.on_change:
            self.on_change(val)

    def set_value(self, val):
        self.spin.blockSignals(True)
        self.slider.blockSignals(True)
        self.spin.setValue(val)
        ival = int(round(val * self.scale)) if self.decimals > 0 else int(val)
        self.slider.setValue(ival)
        self.spin.blockSignals(False)
        self.slider.blockSignals(False)
        if self.on_change:
            self.on_change(val)


class DummyDataStudioDrawer(QFrame):
    """Interactive bottom drawer to manipulate telemetry and inject faults in real time."""

    def __init__(self, main_win=None, parent=None):
        super().__init__(parent)
        self.main_win = main_win
        self.setObjectName("DummyDrawer")
        self.setStyleSheet(f"""
            #DummyDrawer {{
                background: {T.surf};
                border-top: 2px solid {T.cyan};
                border-bottom: 1px solid {T.line_dim};
            }}
        """)
        self._build_ui()

    def _build_ui(self):
        root = QVBoxLayout(self)
        root.setContentsMargins(14, 8, 14, 10)
        root.setSpacing(6)

        # Header Bar
        top = QHBoxLayout()
        top.setSpacing(12)

        title = QLabel("SIMULATION & STIMULUS BENCH  ·  TELEMETRY CONTROL")
        title.setFont(font_display(9, bold=True, tracking=2.0))
        title.setStyleSheet(f"color: {T.cyan}; background: transparent;")
        top.addWidget(title)

        top.addSpacing(14)

        # Mode Selection: Manual vs Auto Profile
        self.bg_mode = QButtonGroup(self)
        self.rb_manual = QRadioButton("Manual Override")
        self.rb_auto = QRadioButton("Autonomous Flight Profile")
        self.rb_manual.setChecked(True)
        self.rb_manual.setFont(font_display(8, tracking=1.1, caps=False))
        self.rb_auto.setFont(font_display(8, tracking=1.1, caps=False))
        self.bg_mode.addButton(self.rb_manual)
        self.bg_mode.addButton(self.rb_auto)
        self.rb_manual.toggled.connect(self._on_mode_toggled)

        top.addWidget(self.rb_manual)
        top.addWidget(self.rb_auto)

        top.addSpacing(10)

        # Speed buttons
        sp_lbl = QLabel("Playback Rate:")
        sp_lbl.setFont(font_display(7.5, caps=False))
        sp_lbl.setStyleSheet(f"color: {T.fg3};")
        top.addWidget(sp_lbl)

        self.b_speed1 = QPushButton("1x")
        self.b_speed2 = QPushButton("2x")
        self.b_speed5 = QPushButton("5x")
        for b in (self.b_speed1, self.b_speed2, self.b_speed5):
            b.setFixedWidth(36)
            b.setFont(font_mono(8))
            b.setStyleSheet(f"background:{T.surf2}; color:{T.fg}; border:1px solid {T.line}; padding:2px 4px;")
        self.b_speed1.clicked.connect(lambda: self._set_speed(1.0))
        self.b_speed2.clicked.connect(lambda: self._set_speed(2.0))
        self.b_speed5.clicked.connect(lambda: self._set_speed(5.0))
        top.addWidget(self.b_speed1)
        top.addWidget(self.b_speed2)
        top.addWidget(self.b_speed5)

        self.b_pause = QPushButton("Pause")
        self.b_pause.setFixedWidth(72)
        self.b_pause.setFont(font_mono(8))
        self.b_pause.setStyleSheet(f"background:{T.surf2}; color:{T.fg}; border:1px solid {T.line}; padding:2px 6px;")
        self.b_pause.clicked.connect(self._toggle_pause)
        top.addWidget(self.b_pause)

        top.addStretch(1)

        b_reset = QPushButton("Reset Nominals")
        b_reset.setFixedWidth(112)
        b_reset.setFont(font_display(8, caps=False))
        b_reset.setStyleSheet(f"background:{T.surf2}; color:{T.amber}; border:1px solid {T.amber}; padding:2px 8px;")
        b_reset.clicked.connect(self.reset_nominals)
        top.addWidget(b_reset)

        self.b_collapse = QPushButton("Collapse")
        self.b_collapse.setFixedWidth(86)
        self.b_collapse.setFont(font_display(8, caps=False))
        self.b_collapse.setStyleSheet(f"background:{T.surf2}; color:{T.fg2}; border:1px solid {T.line}; padding:2px 6px;")
        self.b_collapse.clicked.connect(self._toggle_collapse)
        top.addWidget(self.b_collapse)

        root.addLayout(top)

        # Tabs Container
        self.tabs = QTabWidget()
        self.tabs.setFont(font_display(8.5, tracking=1.2, caps=False))
        self.tabs.setStyleSheet(f"""
            QTabBar::tab {{
                background: {T.surf2};
                color: {T.fg2};
                padding: 6px 14px;
                border-top-left-radius: 4px;
                border-top-right-radius: 4px;
                margin-right: 2px;
            }}
            QTabBar::tab:selected {{
                background: {T.surf3};
                color: {T.cyan};
                font-weight: bold;
                border-bottom: 2px solid {T.cyan};
            }}
        """)

        self.tabs.addTab(self._tab_attitude(), "Attitude & Dynamics")
        self.tabs.addTab(self._tab_flight(), "Trajectory & Atmosphere")
        self.tabs.addTab(self._tab_power(), "Power Rail & Limits")
        self.tabs.addTab(self._tab_gnss(), "GNSS & Spatial Tracking")
        self.tabs.addTab(self._tab_faults(), "Fault Injection & Diagnostics")

        root.addWidget(self.tabs)

    def _tab_attitude(self):
        w = QWidget()
        lay = QVBoxLayout(w)
        lay.setContentsMargins(8, 8, 8, 8)
        lay.setSpacing(6)

        grid = QGridLayout()
        grid.setSpacing(10)

        self.s_roll = SliderRow("Roll Angle", -180, 180, 0, step=1, unit="°", on_change=lambda v: self._set_param("roll", v))
        self.s_pitch = SliderRow("Pitch Angle", -90, 90, 0, step=1, unit="°", on_change=lambda v: self._set_param("pitch", v))
        self.s_yaw = SliderRow("Yaw / Heading", 0, 360, 0, step=1, unit="°", on_change=lambda v: self._set_param("yaw", v))
        self.s_spin = SliderRow("Spin Rate", -360, 360, 0, step=5, unit="°/s", on_change=lambda v: self._set_param("spin", v))

        grid.addWidget(self.s_roll, 0, 0)
        grid.addWidget(self.s_pitch, 0, 1)
        grid.addWidget(self.s_yaw, 1, 0)
        grid.addWidget(self.s_spin, 1, 1)
        lay.addLayout(grid)

        # Presets Row
        p_lay = QHBoxLayout()
        lbl = QLabel("Attitude Presets:")
        lbl.setFont(font_display(7.5, caps=False))
        lbl.setStyleSheet(f"color: {T.fg3};")
        p_lay.addWidget(lbl)
        presets = [
            ("Level Pad (0°, 0°)", {"roll": 0.0, "pitch": 0.0, "spin": 0.0}),
            ("45° Bank", {"roll": 45.0, "pitch": 0.0, "spin": 0.0}),
            ("Pitch Up 45°", {"roll": 0.0, "pitch": 45.0, "spin": 0.0}),
            ("Inverted (180°)", {"roll": 180.0, "pitch": 0.0, "spin": 0.0}),
            ("Tumbling (120°/s)", {"roll": 25.0, "pitch": 15.0, "spin": 120.0}),
        ]
        for name, dct in presets:
            btn = QPushButton(name)
            btn.setFont(font_mono(8))
            btn.setStyleSheet(f"background: {T.surf2}; color: {T.cyan}; border: 1px solid {T.line}; padding: 3px 8px; border-radius: 3px;")
            btn.clicked.connect(lambda _, d=dct: self._apply_attitude_preset(d))
            p_lay.addWidget(btn)
        p_lay.addStretch(1)
        lay.addLayout(p_lay)
        return w

    def _tab_flight(self):
        w = QWidget()
        lay = QVBoxLayout(w)
        lay.setContentsMargins(8, 8, 8, 8)
        lay.setSpacing(6)

        top = QHBoxLayout()
        lbl = QLabel("Flight Software State:")
        lbl.setFont(font_display(8, caps=False))
        lbl.setStyleSheet(f"color: {T.fg2};")
        top.addWidget(lbl)
        self.cb_state = QComboBox()
        self.cb_state.setFont(font_mono(8.5))
        self.cb_state.addItems([
            "0: BOOT (Pad Idle)",
            "1: LAUNCH (Rocket Ascent)",
            "2: EJECT (Primary Parachute)",
            "3: SEC_EJECT (600m Aerobrake Release)",
            "4: TOUCHDOWN (Landed)",
        ])
        self.cb_state.currentIndexChanged.connect(lambda i: self._set_param("state", i))
        top.addWidget(self.cb_state)
        top.addStretch(1)
        lay.addLayout(top)

        grid = QGridLayout()
        grid.setSpacing(10)
        self.s_alt = SliderRow("Altitude AGL", 0, 1200, 0, step=1, decimals=1, unit="m", on_change=self._on_alt_change)
        self.s_vsp = SliderRow("Vertical Speed", -25.0, 35.0, 0.0, step=0.5, decimals=1, unit="m/s", on_change=lambda v: self._set_param("vsp", v))
        self.s_temp = SliderRow("Temperature", -20.0, 50.0, 24.0, step=0.5, decimals=1, unit="°C", on_change=lambda v: self._set_param("temp", v))
        grid.addWidget(self.s_alt, 0, 0)
        grid.addWidget(self.s_vsp, 0, 1)
        grid.addWidget(self.s_temp, 1, 0)
        lay.addLayout(grid)

        # Stage Presets
        p_lay = QHBoxLayout()
        lbl = QLabel("Flight Phase Presets:")
        lbl.setFont(font_display(7.5, caps=False))
        lbl.setStyleSheet(f"color: {T.fg3};")
        p_lay.addWidget(lbl)
        stages = [
            ("Pad Idle (0m)", {"state": 0, "alt": 0.0, "vsp": 0.0, "bus_v": 12.2}),
            ("Ascent (400m, +20m/s)", {"state": 1, "alt": 400.0, "vsp": 20.0, "bus_v": 11.5}),
            ("Apogee (760m)", {"state": 1, "alt": 760.0, "vsp": 0.0, "bus_v": 11.2}),
            ("Primary Chute (650m, -14m/s)", {"state": 2, "alt": 650.0, "vsp": -14.0, "bus_v": 10.9}),
            ("600m Sec Eject (600m)", {"state": 3, "alt": 600.0, "vsp": -8.5, "bus_v": 10.7}),
            ("Touchdown (0m)", {"state": 4, "alt": 0.0, "vsp": 0.0, "bus_v": 10.2}),
        ]
        for name, dct in stages:
            btn = QPushButton(name)
            btn.setFont(font_mono(8))
            btn.setStyleSheet(f"background: {T.surf2}; color: {T.green}; border: 1px solid {T.line}; padding: 3px 8px; border-radius: 3px;")
            btn.clicked.connect(lambda _, d=dct: self._apply_flight_preset(d))
            p_lay.addWidget(btn)
        p_lay.addStretch(1)
        lay.addLayout(p_lay)
        return w

    def _tab_power(self):
        w = QWidget()
        lay = QVBoxLayout(w)
        lay.setContentsMargins(8, 8, 8, 8)
        lay.setSpacing(6)

        grid = QGridLayout()
        grid.setSpacing(10)
        self.s_bus_v = SliderRow("Bus Voltage", 7.00, 13.50, 12.20, step=0.05, decimals=2, unit="V", on_change=lambda v: self._set_param("bus_v", v))
        self.s_bus_i = SliderRow("Bus Current", 0, 1200, 380, step=10, unit="mA", on_change=lambda v: self._set_param("bus_i", v))
        self.s_soc = SliderRow("Battery SOC", 0, 100, 98, step=1, unit="%", on_change=lambda v: self._set_param("soc", v))

        grid.addWidget(self.s_bus_v, 0, 0)
        grid.addWidget(self.s_bus_i, 0, 1)
        grid.addWidget(self.s_soc, 1, 0)
        lay.addLayout(grid)

        p_lay = QHBoxLayout()
        lbl = QLabel("Limits Test Triggers:")
        lbl.setFont(font_display(7.5, caps=False))
        lbl.setStyleSheet(f"color: {T.fg3};")
        p_lay.addWidget(lbl)

        btn_red = QPushButton("Trigger Undervoltage [9.20 V]")
        btn_red.setFont(font_mono(8))
        btn_red.setStyleSheet(f"background: {T.red_d}; color: {T.red}; border: 1px solid {T.red}; padding: 3px 10px; border-radius: 3px; font-weight: bold;")
        btn_red.clicked.connect(lambda: self.s_bus_v.set_value(9.20))

        btn_yel = QPushButton("Trigger Caution [10.10 V]")
        btn_yel.setFont(font_mono(8))
        btn_yel.setStyleSheet(f"background: {T.amber_d}; color: {T.amber}; border: 1px solid {T.amber}; padding: 3px 10px; border-radius: 3px; font-weight: bold;")
        btn_yel.clicked.connect(lambda: self.s_bus_v.set_value(10.10))

        btn_nom = QPushButton("Restore Nominal [12.20 V]")
        btn_nom.setFont(font_mono(8))
        btn_nom.setStyleSheet(f"background: {T.green_d}; color: {T.green}; border: 1px solid {T.green}; padding: 3px 10px; border-radius: 3px; font-weight: bold;")
        btn_nom.clicked.connect(lambda: self.s_bus_v.set_value(12.20))

        p_lay.addWidget(btn_red)
        p_lay.addWidget(btn_yel)
        p_lay.addWidget(btn_nom)
        p_lay.addStretch(1)
        lay.addLayout(p_lay)
        return w

    def _tab_gnss(self):
        w = QWidget()
        lay = QVBoxLayout(w)
        lay.setContentsMargins(8, 8, 8, 8)
        lay.setSpacing(6)

        grid = QGridLayout()
        grid.setSpacing(10)
        self.s_sats = SliderRow("Satellites (Sats)", 0, 24, 12, step=1, unit="", on_change=lambda v: self._set_param("sats", int(v)))
        self.s_hdop = SliderRow("HDOP Uncertainty", 0.6, 5.0, 1.1, step=0.1, decimals=2, unit="", on_change=lambda v: self._set_param("hdop", v))
        grid.addWidget(self.s_sats, 0, 0)
        grid.addWidget(self.s_hdop, 0, 1)
        lay.addLayout(grid)

        p_lay = QHBoxLayout()
        lbl = QLabel("Fix Quality:")
        lbl.setFont(font_display(7.5, caps=False))
        lbl.setStyleSheet(f"color: {T.fg3};")
        p_lay.addWidget(lbl)
        self.cb_fix = QComboBox()
        self.cb_fix.setFont(font_mono(8.5))
        self.cb_fix.addItems(["0: No Fix", "1: SPS 3D Autonomous", "2: GAGAN / SBAS Corrected", "3: NavIC / Multi-GNSS"])
        self.cb_fix.setCurrentIndex(1)
        self.cb_fix.currentIndexChanged.connect(lambda i: self._set_param("fix_quality", i))
        p_lay.addWidget(self.cb_fix)

        p_lay.addSpacing(16)
        lbl_p = QLabel("Position Nudge:")
        lbl_p.setFont(font_display(7.5, caps=False))
        lbl_p.setStyleSheet(f"color: {T.fg3};")
        p_lay.addWidget(lbl_p)

        btn_n = QPushButton("N (+Lat)")
        btn_s = QPushButton("S (-Lat)")
        btn_e = QPushButton("E (+Lon)")
        btn_w = QPushButton("W (-Lon)")
        for b in (btn_n, btn_s, btn_e, btn_w):
            b.setFont(font_mono(8))
            b.setStyleSheet(f"background: {T.surf2}; color: {T.fg}; border: 1px solid {T.line}; padding: 3px 6px; border-radius: 3px;")
        btn_n.clicked.connect(lambda: self._nudge_pos(1.0e-4, 0))
        btn_s.clicked.connect(lambda: self._nudge_pos(-1.0e-4, 0))
        btn_e.clicked.connect(lambda: self._nudge_pos(0, 1.0e-4))
        btn_w.clicked.connect(lambda: self._nudge_pos(0, -1.0e-4))
        p_lay.addWidget(btn_n)
        p_lay.addWidget(btn_s)
        p_lay.addWidget(btn_e)
        p_lay.addWidget(btn_w)
        p_lay.addStretch(1)
        lay.addLayout(p_lay)
        return w

    def _tab_faults(self):
        w = QWidget()
        lay = QVBoxLayout(w)
        lay.setContentsMargins(8, 8, 8, 8)
        lay.setSpacing(8)

        lbl = QLabel("Inject safety faults to evaluate limits detection, staleness ladder, and honest absence:")
        lbl.setFont(font_display(7.5, caps=False))
        lbl.setStyleSheet(f"color: {T.fg3};")
        lay.addWidget(lbl)

        row1 = QHBoxLayout()
        b_dis_baro = QPushButton("Disconnect Barometer [EM-DASH]")
        b_dis_baro.setFont(font_mono(8))
        b_dis_baro.setStyleSheet("background: #2a1a1a; color: #ff8888; border: 1px solid #ff4444; padding: 4px 10px; border-radius: 3px;")
        b_dis_baro.clicked.connect(lambda: self._inject_fault("disconnect", ["ALT_M", "PRES_PA"]))

        b_dis_imu = QPushButton("Disconnect IMU [EM-DASH]")
        b_dis_imu.setFont(font_mono(8))
        b_dis_imu.setStyleSheet("background: #2a1a1a; color: #ff8888; border: 1px solid #ff4444; padding: 4px 10px; border-radius: 3px;")
        b_dis_imu.clicked.connect(lambda: self._inject_fault("disconnect", ["ACC_X", "ACC_Y", "ACC_Z", "GX", "GY", "GZ"]))

        b_short_v = QPushButton("Bus Ground Short [0.0 V]")
        b_short_v.setFont(font_mono(8))
        b_short_v.setStyleSheet(f"background: {T.red_d}; color: {T.red}; border: 1px solid {T.red}; padding: 4px 10px; border-radius: 3px; font-weight: bold;")
        b_short_v.clicked.connect(lambda: self._inject_fault("short", ["BUS_V"]))

        row1.addWidget(b_dis_baro)
        row1.addWidget(b_dis_imu)
        row1.addWidget(b_short_v)
        row1.addStretch(1)
        lay.addLayout(row1)

        row2 = QHBoxLayout()
        b_freeze = QPushButton("Freeze Barometer [LOCKUP]")
        b_freeze.setFont(font_mono(8))
        b_freeze.setStyleSheet("background: #1a2a3a; color: #88ccee; border: 1px solid #44aacc; padding: 4px 10px; border-radius: 3px;")
        b_freeze.clicked.connect(lambda: self._inject_fault("freeze", ["PRES_PA"]))

        b_drop_rf = QPushButton("Halt RF Telemetry Link [STALE]")
        b_drop_rf.setFont(font_mono(8))
        b_drop_rf.setStyleSheet(f"background: {T.amber_d}; color: {T.amber}; border: 1px solid {T.amber}; padding: 4px 10px; border-radius: 3px;")
        b_drop_rf.clicked.connect(self._inject_radio_drop)

        b_clear = QPushButton("Clear Injected Faults")
        b_clear.setFont(font_mono(8))
        b_clear.setStyleSheet(f"background: {T.green_d}; color: {T.green}; border: 1px solid {T.green}; padding: 4px 12px; border-radius: 3px; font-weight: bold;")
        b_clear.clicked.connect(self._clear_faults)

        row2.addWidget(b_freeze)
        row2.addWidget(b_drop_rf)
        row2.addWidget(b_clear)
        row2.addStretch(1)
        lay.addLayout(row2)
        return w

    def _on_mode_toggled(self, checked):
        if self.main_win and self.main_win.sim:
            self.main_win.sim.set_manual(checked)

    def _set_speed(self, spd):
        if self.main_win and self.main_win.sim:
            self.main_win.sim.set_speed(spd)

    def _toggle_pause(self):
        if self.main_win and self.main_win.sim:
            p = not self.main_win.sim.paused
            self.main_win.sim.set_paused(p)
            self.b_pause.setText("Resume" if p else "Pause")

    def _toggle_collapse(self):
        if self.tabs.isVisible():
            self.tabs.setVisible(False)
            self.b_collapse.setText("Expand")
        else:
            self.tabs.setVisible(True)
            self.b_collapse.setText("Collapse")

    def _set_param(self, k, v):
        if self.main_win and self.main_win.sim:
            self.main_win.sim.set_param(k, v)

    def _on_alt_change(self, alt):
        self._set_param("alt", alt)
        pres = SEA_LEVEL_P_PA * (1.0 - 2.25577e-5 * alt) ** 5.25588
        temp = 21.5 - 0.0065 * alt
        self._set_param("pres", pres)
        self._set_param("temp", temp)
        self.s_temp.set_value(temp)

    def _apply_attitude_preset(self, d):
        self.s_roll.set_value(d.get("roll", 0.0))
        self.s_pitch.set_value(d.get("pitch", 0.0))
        self.s_spin.set_value(d.get("spin", 0.0))

    def _apply_flight_preset(self, d):
        self.cb_state.setCurrentIndex(d.get("state", 0))
        self.s_alt.set_value(d.get("alt", 0.0))
        self.s_vsp.set_value(d.get("vsp", 0.0))
        if "bus_v" in d:
            self.s_bus_v.set_value(d["bus_v"])

    def _nudge_pos(self, dlat, dlon):
        if self.main_win and self.main_win.sim:
            cur_lat = self.main_win.sim.overrides.get("lat", SIM_SITE_LAT)
            cur_lon = self.main_win.sim.overrides.get("lon", SIM_SITE_LON)
            self._set_param("lat", cur_lat + dlat)
            self._set_param("lon", cur_lon + dlon)

    def _inject_fault(self, ftype, keys):
        if self.main_win and self.main_win.sim:
            if ftype == "disconnect":
                self.main_win.sim.inject_disconnect(keys)
            elif ftype == "short":
                self.main_win.sim.inject_short(keys)
            elif ftype == "freeze":
                self.main_win.sim.inject_freeze(keys)

    def _inject_radio_drop(self):
        if self.main_win and self.main_win.sim:
            self.main_win.sim.fault_halt_radio = True

    def _clear_faults(self):
        if self.main_win and self.main_win.sim:
            self.main_win.sim.clear_faults()

    def reset_nominals(self):
        self._clear_faults()
        self.s_roll.set_value(0.0)
        self.s_pitch.set_value(0.0)
        self.s_yaw.set_value(0.0)
        self.s_spin.set_value(0.0)
        self.s_alt.set_value(0.0)
        self.s_vsp.set_value(0.0)
        self.s_temp.set_value(24.0)
        self.s_bus_v.set_value(12.20)
        self.s_bus_i.set_value(380)
        self.s_soc.set_value(98)
        self.s_sats.set_value(12)
        self.s_hdop.set_value(1.1)
        self.cb_state.setCurrentIndex(0)
        self.cb_fix.setCurrentIndex(1)
        if self.main_win and self.main_win.sim:
            self.main_win.sim.set_manual(True)


class ModeSelectDialog(QDialog):
    """Launcher screen: Choose between interactive Dummy Data Studio or Live Mission Dashboard."""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("VRITRA GCS  ·  Mode Selector")
        self.resize(960, 600)
        self.setStyleSheet(f"""
            QDialog {{ background: {T.black}; color: {T.fg}; }}
            #Card {{
                background: {T.surf};
                border: 1px solid {T.line};
                border-radius: 8px;
            }}
            #Card:hover {{
                background: {T.surf2};
            }}
        """)
        self.selected_mode = "SIM"
        self.selected_port = None
        self._build_ui()

    def _build_ui(self):
        root = QVBoxLayout(self)
        root.setContentsMargins(28, 22, 28, 20)
        root.setSpacing(18)

        # Header
        top = QVBoxLayout()
        top.setSpacing(3)
        h1 = QLabel("VRITRA GROUND CONTROL STATION")
        h1.setFont(font_display(17, bold=True, tracking=2.4))
        h1.setStyleSheet(f"color: {T.fg}; background: transparent;")
        h2 = QLabel(f"IN-SPACe CAN-7USAT 2026  ·  TEAM {TEAM_ID}  ·  {TEAM_INST}")
        h2.setFont(font_mono(8.5))
        h2.setStyleSheet(f"color: {T.cyan}; background: transparent;")
        top.addWidget(h1)
        top.addWidget(h2)
        root.addLayout(top)

        # Two Cards side by side
        cards_lay = QHBoxLayout()
        cards_lay.setSpacing(20)

        # CARD 1: SIMULATION STUDIO
        c1 = QFrame()
        c1.setObjectName("Card")
        c1.setStyleSheet(f"#Card {{ border-top: 3px solid {T.cyan}; }} #Card:hover {{ border-color: {T.cyan}; }}")
        c1_lay = QVBoxLayout(c1)
        c1_lay.setContentsMargins(20, 20, 20, 20)
        c1_lay.setSpacing(12)

        badge1 = QLabel("SIMULATION & CALIBRATION BENCH")
        badge1.setFont(font_display(8, bold=True, tracking=1.5))
        badge1.setStyleSheet(f"color: {T.cyan}; background: transparent;")
        c1_lay.addWidget(badge1)

        t1 = QLabel("Interactive Telemetry Studio")
        t1.setFont(font_display(14, bold=True, tracking=1.2))
        t1.setStyleSheet(f"color: {T.fg}; background: transparent;")
        c1_lay.addWidget(t1)

        desc1 = QLabel(
            "Hardware-in-the-loop simulation and verification environment. Provides real-time "
            "dynamic override of 3D kinematics (Roll, Pitch, Yaw, Spin), barometric atmospheric profile, "
            "power rail envelopes, and spatial coordinates. Designed for mission readiness reviews and "
            "pre-flight limit alarm validation."
        )
        desc1.setFont(font_display(8.5, tracking=1.0, caps=False))
        desc1.setWordWrap(True)
        desc1.setStyleSheet(f"color: {T.fg2}; background: transparent; line-height: 1.4;")
        c1_lay.addWidget(desc1)

        c1_lay.addSpacing(6)
        bullets1 = QLabel(
            "• Pure software 3D CanSat model, ADI & compass dynamics\n"
            "• Barometric altitude, vertical velocity & temperature sweeps\n"
            "• Multi-threshold power bus envelope testing (<9.6V red alarm)\n"
            "• Sensor fault injection: hardware disconnects, lockup & link drop\n"
            "• Complete 6-view mission telemetry analysis suite"
        )
        bullets1.setFont(font_mono(8.5))
        bullets1.setStyleSheet(f"color: {T.fg2}; background: transparent;")
        c1_lay.addWidget(bullets1)

        c1_lay.addStretch(1)

        btn1 = QPushButton("LAUNCH SIMULATION STUDIO")
        btn1.setFont(font_display(10.5, bold=True, tracking=1.5))
        btn1.setCursor(Qt.PointingHandCursor)
        btn1.setStyleSheet(f"""
            QPushButton {{
                background: {T.cyan};
                color: #000000;
                font-weight: bold;
                padding: 12px;
                border: none;
                border-radius: 4px;
            }}
            QPushButton:hover {{
                background: #ffffff;
            }}
        """)
        btn1.clicked.connect(self._choose_sim)
        c1_lay.addWidget(btn1)
        cards_lay.addWidget(c1, 1)

        # CARD 2: LIVE MISSION DASHBOARD
        c2 = QFrame()
        c2.setObjectName("Card")
        c2.setStyleSheet(f"#Card {{ border-top: 3px solid {T.green}; }} #Card:hover {{ border-color: {T.green}; }}")
        c2_lay = QVBoxLayout(c2)
        c2_lay.setContentsMargins(20, 20, 20, 20)
        c2_lay.setSpacing(12)

        badge2 = QLabel("LIVE FLIGHT TELEMETRY LINK")
        badge2.setFont(font_display(8, bold=True, tracking=1.5))
        badge2.setStyleSheet(f"color: {T.green}; background: transparent;")
        c2_lay.addWidget(badge2)

        t2 = QLabel("Live Mission Dashboard")
        t2.setFont(font_display(14, bold=True, tracking=1.2))
        t2.setStyleSheet(f"color: {T.fg}; background: transparent;")
        c2_lay.addWidget(t2)

        desc2 = QLabel(
            "Real-time serial communications link with the physical ESP32 LoRa Ground Station "
            "Gateway. Decodes incoming Base-36 packets across 83 flight parameters at 1 Hz, "
            "logs the official competition CSV dataset, and manages telecommand uplink transmission."
        )
        desc2.setFont(font_display(8.5, tracking=1.0, caps=False))
        desc2.setWordWrap(True)
        desc2.setStyleSheet(f"color: {T.fg2}; background: transparent; line-height: 1.4;")
        c2_lay.addWidget(desc2)

        # Port Selector row inside Card 2
        port_row = QHBoxLayout()
        p_lbl = QLabel("Gateway Interface:")
        p_lbl.setFont(font_display(8, caps=False))
        p_lbl.setStyleSheet(f"color: {T.fg3};")
        self.cb_port = PortCombo()
        self.cb_port.setMinimumWidth(230)
        port_row.addWidget(p_lbl)
        port_row.addWidget(self.cb_port)
        port_row.addStretch(1)
        c2_lay.addLayout(port_row)

        bullets2 = QLabel(
            "• Direct 1 Hz serial ingest & Base-36 frame decoding\n"
            f"• Official competition mission log: {CSV_FILENAME}\n"
            "• 4-threshold limits engine & sensor staleness monitoring\n"
            "• NavIC / GNSS tracking, dynamic uncertainty circle & drift vector\n"
            "• Telecommand uplink transmission (CMD_TX_ON, calibrate, arm)"
        )
        bullets2.setFont(font_mono(8.5))
        bullets2.setStyleSheet(f"color: {T.fg2}; background: transparent;")
        c2_lay.addWidget(bullets2)

        c2_lay.addStretch(1)

        btn2 = QPushButton("LAUNCH LIVE DASHBOARD")
        btn2.setFont(font_display(10.5, bold=True, tracking=1.5))
        btn2.setCursor(Qt.PointingHandCursor)
        btn2.setStyleSheet(f"""
            QPushButton {{
                background: {T.green};
                color: #000000;
                font-weight: bold;
                padding: 12px;
                border: none;
                border-radius: 4px;
            }}
            QPushButton:hover {{
                background: #ffffff;
            }}
        """)
        btn2.clicked.connect(self._choose_live)
        c2_lay.addWidget(btn2)
        cards_lay.addWidget(c2, 1)

        root.addLayout(cards_lay, 1)

        # Footer
        bot = QHBoxLayout()
        info = QLabel("STANDALONE WIN64 BINARY  ·  ZERO GPU DEPENDENCIES  ·  OFFLINE FIELD-PORTABLE ARCHITECTURE")
        info.setFont(font_mono(7.8))
        info.setStyleSheet(f"color: {T.fg3}; background: transparent; letter-spacing: 1.0px;")
        bot.addWidget(info)
        bot.addStretch(1)
        root.addLayout(bot)

    def _choose_sim(self):
        self.selected_mode = "SIM"
        self.accept()

    def _choose_live(self):
        self.selected_mode = "LIVE"
        port = self.cb_port.selected_port()
        if not port or port.startswith("(") or "Bluetooth" in port:
            self.selected_port = ""
        else:
            self.selected_port = port
        self.accept()


class MainWindow(QMainWindow):

    def __init__(self, initial_mode="SIM", initial_port=None):
        super().__init__()
        self.mode = initial_mode
        self.initial_port = initial_port
        self.setWindowTitle(APP_TITLE)
        self.resize(1780, 1000)

        # ---- pipeline state -------------------------------------------
        self.rx_q = queue.Queue(maxsize=4000)
        self.tx_q = queue.Queue()
        self.link = None
        self.link_status = "DISCONNECTED"
        self.sim = None
        self.phys = Physics()
        self.limits = LimitsEngine()
        self.logger = Logger(LOG_DIR)

        self.d = OrderedDict((k, 0.0) for k in ALL_FIELDS)
        self.present = set()
        self.dv = {}
        self.dp = set()

        self.t0 = None
        self.met = 0.0
        self.pkt_count = 0
        self.last_pkt_wall = None
        self.rate_hz = 0.0
        self._rate_win = deque(maxlen=20)
        self.raw_last = ""
        self.cmd_history = []
        # Central history store: every received parameter, so ANALYSIS can
        # plot or cross-plot any field retroactively without each widget
        # keeping its own copy.
        self.history = {}
        self.degraded = set()

        # ---- precise positioning --------------------------------------
        self.pad_survey = PositionSurvey()      # averages the CanSat on the pad
        self.gs_survey = PositionSurvey()       # averages the ground receiver
        self.dgnss = DifferentialCorrector()
        self.landing = LandingPredictor()
        self.gs_gnss = None                     # optional GsGnssLink thread
        self.gs_fix = {}                        # latest NMEA state from the GS
        self.pos = {                            # resolved position for display
            "raw": None, "fix": None, "applied": False,
            "sigma": None, "source": "—",
        }
        self._blink = False
        self._audio_last = 0.0
        self.audio_on = True
        self.lightning_rows = 0

        # widget registries
        self.tiles = {}          # key -> [Tile, ...]
        self.plots = {}          # key -> [TimePlot, ...]
        self.limbars = {}        # key -> [LimitBar, ...]
        self.lamps = {}          # group -> QLabel

        self._build_ui()

        self.timer = QTimer(self)
        self.timer.timeout.connect(self._tick)
        self.timer.start(UI_TICK_MS)
        self.blink_timer = QTimer(self)
        self.blink_timer.timeout.connect(self._toggle_blink)
        self.blink_timer.start(480)
        # Statistics and the cross-plot rebuild whole tables, so they run on
        # a slow timer instead of the 10 Hz render tick.
        self.slow_timer = QTimer(self)
        self.slow_timer.timeout.connect(self._slow_tick)
        self.slow_timer.start(2000)

        # Apply initial launch mode
        self.set_app_mode(self.mode, self.initial_port)

    def _slow_tick(self):
        self._safe("stats", self._update_stats_table)
        self._safe("crossplot", self._update_xy)

    # =====================================================================
    # Chrome
    # =====================================================================
    def _build_ui(self):
        root = QWidget()
        self.setCentralWidget(root)
        lay = QVBoxLayout(root)
        lay.setContentsMargins(0, 0, 0, 0)
        lay.setSpacing(0)

        lay.addWidget(self._build_titlebar())
        lay.addWidget(self._build_toolbar())

        self.master_alarm = MasterAlarm()
        self.master_alarm.b_ack.clicked.connect(self._ack_alarm)
        lay.addWidget(self.master_alarm)

        self.tabs = QTabWidget()
        self.tabs.setFont(font_display(9, tracking=1.5, caps=False))
        self.tabs.addTab(self._scroll(self._view_flight()),      "Flight")
        self.tabs.addTab(self._scroll(self._view_science()),     "Science")
        self.tabs.addTab(self._scroll(self._view_engineering()), "Engineering")
        self.tabs.addTab(self._scroll(self._view_analysis()),    "Analysis")
        self.tabs.addTab(self._scroll(self._view_recovery()),    "Recovery")
        self.tabs.addTab(self._scroll(self._view_console()),     "Console")
        lay.addWidget(self.tabs, 1)

        self.dummy_drawer = DummyDataStudioDrawer(main_win=self)
        lay.addWidget(self.dummy_drawer)

        self.statusbar = QLabel("  READY")
        self.statusbar.setFont(font_mono(8))
        self.statusbar.setStyleSheet(
            f"color:{T.fg3}; background:{T.surf}; border-top:1px solid {T.line_dim};"
            f" padding:4px 10px;")
        lay.addWidget(self.statusbar)

        for i in range(1, 7):
            QShortcut(QKeySequence(f"Ctrl+{i}"), self,
                      lambda n=i - 1: self.tabs.setCurrentIndex(n))

    def _build_titlebar(self):
        bar = QFrame()
        bar.setStyleSheet(f"background:{T.black}; border-bottom:1px solid {T.line_dim};")
        h = QHBoxLayout(bar)
        h.setContentsMargins(16, 9, 16, 8)
        h.setSpacing(9)

        left = QVBoxLayout()
        left.setSpacing(1)
        t1 = QLabel(f"VRITRA  ·  GROUND CONTROL STATION")
        t1.setFont(font_display(14, bold=True, tracking=2.6))
        t1.setStyleSheet(f"color:{T.fg}; background:transparent;")
        t2 = QLabel(f"{TEAM_ID}   ·   {TEAM_INST}   ·   v{APP_VERSION}")
        t2.setFont(font_mono(7.6))
        t2.setStyleSheet(f"color:{T.fg3}; background:transparent;")
        left.addWidget(t1)
        left.addWidget(t2)
        h.addLayout(left)
        h.addStretch(1)

        self.chip_mode  = Chip("MODE", "SIM STUDIO" if self.mode == "SIM" else "LIVE FLIGHT")
        self.chip_link  = Chip("LINK", "OFFLINE")
        self.chip_state = Chip("STATE", "—")
        self.chip_met   = Chip("MET", "00:00:00")
        self.chip_pkt   = Chip("PKT", "0")
        self.chip_rate  = Chip("RATE", "0.0 Hz")
        self.chip_sys   = Chip("HEALTH", "—")
        for c in (self.chip_mode, self.chip_link, self.chip_state, self.chip_met,
                  self.chip_pkt, self.chip_rate, self.chip_sys):
            h.addWidget(c)
        return bar

    def _build_toolbar(self):
        bar = QFrame()
        bar.setStyleSheet(f"background:{T.black}; border-bottom:1px solid {T.line_dim};")
        h = QHBoxLayout(bar)
        h.setContentsMargins(16, 8, 16, 8)
        h.setSpacing(7)

        def lab(t):
            l = QLabel(t)
            l.setFont(font_display(7.6, tracking=1.3, caps=False))
            l.setStyleSheet(f"color:{T.fg3}; background:transparent;")
            return l

        h.addWidget(lab("Port"))
        self.cb_port = PortCombo()
        h.addWidget(self.cb_port)
        h.addWidget(lab("Baud"))
        self.ed_baud = QLineEdit(str(DEFAULT_BAUD))
        self.ed_baud.setFixedWidth(74)
        self.ed_baud.setFont(font_mono(9))
        h.addWidget(self.ed_baud)

        self.b_connect = GhostButton("Connect", width=104, accent=True)
        self.b_connect.clicked.connect(self._toggle_link)
        h.addWidget(self.b_connect)

        self._autodetect = None
        self.b_autodetect = GhostButton("Auto-detect", width=104)
        self.b_autodetect.setToolTip(
            "Listen on every COM port and connect to the one that carries the "
            "VRITRA gateway (or VRITRA telemetry). Read-only: sends nothing.")
        self.b_autodetect.clicked.connect(self._autodetect_port)
        h.addWidget(self.b_autodetect)

        self.b_sim = GhostButton("Simulator", width=110)
        self.b_sim.clicked.connect(self._toggle_sim)
        h.addWidget(self.b_sim)

        self.b_drawer_toggle = GhostButton("Studio Drawer", width=120)
        self.b_drawer_toggle.clicked.connect(self._toggle_drawer)
        h.addWidget(self.b_drawer_toggle)

        self.b_launcher = GhostButton("Change Mode", width=110)
        self.b_launcher.clicked.connect(self._open_launcher)
        h.addWidget(self.b_launcher)

        sep = QFrame()
        sep.setFixedWidth(1)
        sep.setStyleSheet(f"background:{T.line_dim};")
        h.addWidget(sep)

        h.addWidget(lab("Uplink"))
        self.cb_cmd = QComboBox()
        self.cb_cmd.setMinimumWidth(210)
        self.cb_cmd.setFont(font_mono(9))
        for wire_cmd, desc in COMMANDS.items():
            label = COMMAND_LABELS.get(wire_cmd, wire_cmd)
            self.cb_cmd.addItem(label, wire_cmd)
            self.cb_cmd.setItemData(self.cb_cmd.count() - 1, desc, Qt.ToolTipRole)
        self.cb_cmd.currentIndexChanged.connect(
            lambda: self.cb_cmd.setToolTip(
                self.cb_cmd.itemData(self.cb_cmd.currentIndex(), Qt.ToolTipRole) or ""))
        h.addWidget(self.cb_cmd)
        self.b_send = GhostButton("Send", width=76, accent=True)
        self.b_send.clicked.connect(self._send_cmd)
        h.addWidget(self.b_send)

        h.addStretch(1)

        self.b_audio = GhostButton("Audio On", width=98)
        self.b_audio.clicked.connect(self._toggle_audio)
        h.addWidget(self.b_audio)
        for text, fn, w in (("Open CSV", self._open_csv, 100),
                            ("Export XML", self._export_xml, 108),
                            ("Export KML", self._export_kml, 108),
                            ("Dictionary", self._export_dict, 112)):
            b = GhostButton(text, width=w)
            b.clicked.connect(fn)
            h.addWidget(b)
        return bar

    # ---- layout helpers -------------------------------------------------
    def _scroll(self, page):
        sc = QScrollArea()
        sc.setWidgetResizable(True)
        sc.setFrameShape(QFrame.NoFrame)
        sc.setWidget(page)
        return sc

    def _page(self, margins=(14, 12, 14, 14), spacing=11):
        w = QWidget()
        l = QVBoxLayout(w)
        l.setContentsMargins(*margins)
        l.setSpacing(spacing)
        return w, l

    def _tile(self, key, label=None, unit=None, spark=True, big=False):
        t = Tile(key, label, unit, spark=spark, big=big)
        self.tiles.setdefault(key, []).append(t)
        return t

    def _tile_grid(self, keys, cols=2, spark=True, big=False):
        """keys: list of key or (key, label)."""
        g = QGridLayout()
        g.setSpacing(7)
        for i, item in enumerate(keys):
            key, label = item if isinstance(item, tuple) else (item, None)
            g.addWidget(self._tile(key, label, spark=spark, big=big),
                        i // cols, i % cols)
        return g

    def _plot(self, key, title=None, ink=None):
        p = TimePlot(key, title, ink)
        self.plots.setdefault(key, []).append(p)
        return p

    def _limbar(self, key):
        b = LimitBar(key)
        self.limbars.setdefault(key, []).append(b)
        return b

    def _kv_row(self, label, value="—", mono=True):
        row = QHBoxLayout()
        row.setContentsMargins(0, 0, 0, 0)
        k = QLabel(label)
        k.setFont(font_display(7.6, tracking=1.2, caps=False))
        k.setStyleSheet(f"color:{T.fg3}; background:transparent;")
        v = QLabel(value)
        v.setFont(font_mono(9) if mono else font_display(8.5))
        v.setStyleSheet(f"color:{T.fg}; background:transparent;")
        row.addWidget(k)
        row.addStretch(1)
        row.addWidget(v)
        return row, v

    # =====================================================================
    # VIEW 1 — FLIGHT  (the flight director's screen)
    # =====================================================================
    def _view_flight(self):
        page, lay = self._page()

        top = QHBoxLayout()
        top.setSpacing(11)

        # ---- left column: health + primary flight ----------------------
        col_l = QVBoxLayout()
        col_l.setSpacing(11)

        p_health = Panel("Subsystem Health")
        g = QGridLayout()
        g.setSpacing(6)
        order = ["core", "imu", "gnss", "power", "rf", "mech", "env", "spec", "sys"]
        for i, grp in enumerate(order):
            row = QHBoxLayout()
            row.setSpacing(7)
            dot = QLabel("●")
            dot.setFont(font_mono(9))
            dot.setStyleSheet(f"color:{T.fg4}; background:transparent;")
            name = QLabel(GROUP_LABEL.get(grp, grp.title()))
            name.setFont(font_display(7.6, tracking=1.2, caps=False))
            name.setStyleSheet(f"color:{T.fg2}; background:transparent;")
            st = QLabel("—")
            st.setFont(font_mono(7.4))
            st.setStyleSheet(f"color:{T.fg4}; background:transparent;")
            row.addWidget(dot)
            row.addWidget(name)
            row.addStretch(1)
            row.addWidget(st)
            wrap = QWidget()
            wrap.setLayout(row)
            g.addWidget(wrap, i // 2, i % 2)
            self.lamps[grp] = (dot, st)
        p_health.add_layout(g)
        col_l.addWidget(p_health)

        p_pf = Panel("Primary Flight")
        p_pf.add_layout(self._tile_grid(
            [("ALT_M", "Altitude AGL"), ("VSPEED", "Vertical Speed"),
             ("APOGEE_M", "Apogee"), ("AGL_M", "Height Above Pad"),
             ("SPEED_3D", "3-D Speed"), ("TTL_S", "Time to Land")], cols=2))
        col_l.addWidget(p_pf)

        p_pwr = Panel("Power")
        p_pwr.add_layout(self._tile_grid(
            [("BUS_V", "Bus Voltage"), ("BUS_I_MA", "Bus Current"),
             ("SOC_PCT", "State of Charge"), ("P_BUS", "Bus Power")], cols=2))
        p_pwr.add(self._limbar("BUS_V"))
        col_l.addWidget(p_pwr)
        col_l.addStretch(1)
        top.addLayout(col_l, 26)

        # ---- centre column: 3-D + attitude ------------------------------
        col_c = QVBoxLayout()
        col_c.setSpacing(11)
        p3d = Panel("Attitude  ·  Live IMU Pose")
        self.cansat = CanSatView()
        p3d.add(self.cansat, 1)
        col_c.addWidget(p3d, 1)

        p_att = Panel("Body Axes")
        strip = QHBoxLayout()
        strip.setSpacing(7)
        for key, lab in (("ROLL", "Roll"), ("PITCH", "Pitch"), ("YAW", "Yaw"),
                         ("TILT", "Tilt"), ("SPIN_DPS", "Spin"),
                         ("INSTAB", "Instability")):
            strip.addWidget(self._tile(key, lab, spark=True, big=False))
        p_att.add_layout(strip)
        col_c.addWidget(p_att)

        p_seq = Panel("Flight Sequence")
        self.stepper = StateStepper()
        p_seq.add(self.stepper)
        col_c.addWidget(p_seq)
        top.addLayout(col_c, 42)

        # ---- right column: instruments + GNSS ---------------------------
        col_r = QVBoxLayout()
        col_r.setSpacing(11)
        p_inst = Panel("Flight Instruments")
        row = QHBoxLayout()
        row.setSpacing(9)
        self.adi = ADI()
        self.compass = Compass()
        va = QVBoxLayout()
        va.setSpacing(3)
        la = QLabel("ATTITUDE")
        la.setFont(font_display(7.4, tracking=1.3))
        la.setStyleSheet(f"color:{T.fg3}; background:transparent;")
        va.addWidget(la)
        va.addWidget(self.adi, 1)
        vb = QVBoxLayout()
        vb.setSpacing(3)
        lb = QLabel("HEADING")
        lb.setFont(font_display(7.4, tracking=1.3))
        lb.setStyleSheet(f"color:{T.fg3}; background:transparent;")
        vb.addWidget(lb)
        vb.addWidget(self.compass, 1)
        row.addLayout(va, 1)
        row.addLayout(vb, 1)
        p_inst.add_layout(row)
        col_r.addWidget(p_inst)

        p_gnss = Panel("Navigation")
        gnss_g = QGridLayout()
        gnss_g.setSpacing(7)
        gnss_g.addWidget(self._tile("LAT", "Latitude", unit="", spark=False), 0, 0)
        gnss_g.addWidget(self._tile("LON", "Longitude", unit="", spark=False), 0, 1)
        gnss_g.addWidget(self._tile("SATS", "Satellites", spark=False), 1, 0)
        gnss_g.addWidget(self._tile("NAVIC_SATS", "NavIC Sats", spark=False), 1, 1)
        gnss_g.addWidget(self._tile("DIST_M", "Range from Pad", spark=False), 2, 0)
        gnss_g.addWidget(self._tile("BEARING", "Bearing", spark=False), 2, 1)
        p_gnss.add_layout(gnss_g)
        col_r.addWidget(p_gnss)

        p_rf = Panel("Downlink")
        p_rf.add_layout(self._tile_grid(
            [("RSSI", "RSSI"), ("SNR", "SNR"),
             ("LINK_MARGIN", "Link Margin"), ("FREQ_MHZ", "Frequency")],
            cols=2))
        p_rf.add(self._limbar("RSSI"))
        col_r.addWidget(p_rf)
        col_r.addStretch(1)
        top.addLayout(col_r, 32)

        lay.addLayout(top, 1)

        p_track = Panel("Ground Track")
        self.track_flight = GroundTrack()
        self.track_flight.setMinimumHeight(210)
        p_track.add(self.track_flight)
        lay.addWidget(p_track)
        return page

    # =====================================================================
    # VIEW 2 — SCIENCE  (the payload / atmospheric scientist's screen)
    # =====================================================================
    def _view_science(self):
        page, lay = self._page()

        r1 = QHBoxLayout()
        r1.setSpacing(11)

        p_atm = Panel("Atmosphere")
        p_atm.add_layout(self._tile_grid(
            [("TEMP_C", "Air Temperature"), ("PRES_PA", "Pressure"),
             ("HUM_PCT", "Rel Humidity"), ("DEW_POINT", "Dew Point"),
             ("AIR_RHO", "Air Density"), ("P_ALT", "Pressure Alt"),
             ("DENS_ALT", "Density Alt"), ("CLOUD_BASE", "Cloud Base")],
            cols=2))
        r1.addWidget(p_atm, 1)

        p_ref = Panel("Reference Sensors  ·  HDC3020")
        p_ref.add_layout(self._tile_grid(
            [("HDC_T", "Reference Temp"), ("HDC_RH", "Reference RH")], cols=1))
        rowr, self.v_tdelta = self._kv_row("Baro vs HDC Delta T", "—")
        p_ref.add_layout(rowr)
        rowr2, self.v_baro_gps = self._kv_row("Baro vs GNSS Delta Alt", "—")
        p_ref.add_layout(rowr2)
        p_ref.stretch()
        r1.addWidget(p_ref, 1)

        p_aq = Panel("Air Quality")
        p_aq.add_layout(self._tile_grid(
            [("PM1", "PM 1.0"), ("PM25", "PM 2.5"), ("PM10", "PM 10"),
             ("CO2", "CO2  SCD41"), ("ECO2", "eCO2  ENS160"),
             ("TVOC", "TVOC"), ("O3", "Ozone  MQ131"),
             ("AQI_PM", "EPA AQI (PM2.5)")], cols=2))
        r1.addWidget(p_aq, 1)
        lay.addLayout(r1)

        r2 = QHBoxLayout()
        r2.setSpacing(11)
        p_spec = Panel("Spectral Irradiance  ·  AS7343 14-Channel")
        self.spectral = SpectralBars()
        p_spec.add(self.spectral, 1)
        srow = QHBoxLayout()
        srow.setSpacing(7)
        for k, l in (("IRR_TOT", "Total Irradiance"), ("S_CLR", "Clear"),
                     ("S_FLK", "Flicker"), ("LUX", "Illuminance"),
                     ("UV", "UV Irradiance")):
            srow.addWidget(self._tile(k, l))
        p_spec.add_layout(srow)
        r2.addWidget(p_spec, 3)

        p_lgt = Panel("Lightning  ·  AS3935")
        p_lgt.add_layout(self._tile_grid(
            [("LGT_DIST", "Last Strike Range"), ("LGT_INT", "Intensity")],
            cols=1, spark=False))
        self.lightning_log = QPlainTextEdit()
        self.lightning_log.setReadOnly(True)
        self.lightning_log.setFont(font_mono(8))
        self.lightning_log.setMinimumHeight(120)
        self.lightning_log.setPlaceholderText("No strikes detected")
        p_lgt.add(self.lightning_log, 1)
        r2.addWidget(p_lgt, 1)
        lay.addLayout(r2)

        # Vertical profiles: the plot a scientist actually wants — the
        # measurement against altitude, not against wall-clock time.
        p_prof = Panel("Vertical Profiles  ·  Measurand vs Altitude")
        prow = QHBoxLayout()
        prow.setSpacing(9)
        self.profiles = {}
        for key, title, ink in (("TEMP_C", "TEMPERATURE", T.SEQ[4]),
                                ("PRES_PA", "PRESSURE", T.SEQ[3]),
                                ("HUM_PCT", "HUMIDITY", T.SEQ[2]),
                                ("AIR_RHO", "AIR DENSITY", T.SEQ[1])):
            pw = pg.PlotWidget()
            pw.setMinimumHeight(215)
            pw.showGrid(x=True, y=True, alpha=0.16)
            pw.setMenuEnabled(False)
            pi = pw.getPlotItem()
            pi.hideButtons()
            for ax in ("left", "bottom"):
                a = pi.getAxis(ax)
                a.setPen(pg.mkPen(T.line_dim, width=1))
                a.setTextPen(pg.mkPen(T.fg3))
                a.setStyle(tickFont=font_mono(7), tickLength=-4)
            pi.getAxis("left").setWidth(82)
            unit = FIELD_META[key]["u"]
            pi.getAxis("left").setLabel("ALTITUDE  [m]",
                                        **{"color": T.fg3, "font-size": "7.5pt"})
            pi.getAxis("bottom").setLabel(f"{title}  [{unit}]",
                                          **{"color": T.fg3, "font-size": "7.5pt"})
            # Points, not a polyline: a vehicle samples the same altitude band
            # twice (ascent and descent), so connecting consecutive samples
            # draws a meaningless zig-zag across the profile.
            curve = pw.plot([], [], pen=None, symbol="o", symbolSize=3.2,
                            symbolBrush=pg.mkBrush(ink), symbolPen=None)
            self.profiles[key] = (pw, curve, [], [])
            prow.addWidget(pw, 1)
        p_prof.add_layout(prow)
        lay.addWidget(p_prof)
        return page

    # =====================================================================
    # VIEW 3 — ENGINEERING  (subsystems, health, and the RF toolbox)
    # =====================================================================
    def _view_engineering(self):
        page, lay = self._page()

        r1 = QHBoxLayout()
        r1.setSpacing(11)

        p_pwr = Panel("Power  ·  3S Li-Ion, No BMS Fitted")
        p_pwr.add_layout(self._tile_grid(
            [("BUS_V", "Bus Voltage"), ("BUS_I_MA", "Bus Current"),
             ("P_BUS", "Bus Power"), ("SOC_PCT", "State of Charge"),
             ("SERVO_V", "Servo Rail V"), ("SERVO_I_MA", "Servo Current"),
             ("P_SERVO", "Servo Power"), ("ENERGY_WH", "Energy Used")],
            cols=2))
        note = QLabel("Bus voltage is the primary battery-health indicator — "
                      "no BMS is fitted, so per-cell data is not telemetered")
        note.setFont(font_display(7, tracking=1.0))
        note.setWordWrap(True)
        note.setStyleSheet(f"color:{T.fg4}; background:transparent;")
        p_pwr.add(self._limbar("BUS_V"))
        p_pwr.add(self._limbar("SERVO_V"))
        p_pwr.add(note)
        r1.addWidget(p_pwr, 1)

        p_imu = Panel("Inertial  ·  ICM-20948 + LSM6DSO32")
        p_imu.add_layout(self._tile_grid(
            [("AX", "Accel X"), ("AY", "Accel Y"), ("AZ", "Accel Z"),
             ("GX", "Gyro X"), ("GY", "Gyro Y"), ("GZ", "Gyro Z"),
             ("MX", "Mag X"), ("MY", "Mag Y"), ("MZ", "Mag Z"),
             ("AHX", "High-G X"), ("AHY", "High-G Y"), ("AHZ", "High-G Z")],
            cols=3, spark=False))
        srow = QHBoxLayout()
        srow.setSpacing(7)
        for k, l in (("G_HI", "High-G Mag"), ("G_HI_PK", "Peak G"),
                     ("OMEGA_T", "Transverse Rate"), ("NUTATION", "Nutation"),
                     ("B_MAG", "Field Mag"), ("B_INCL", "Inclination")):
            srow.addWidget(self._tile(k, l))
        p_imu.add_layout(srow)
        p_imu.stretch()
        r1.addWidget(p_imu, 1)
        lay.addLayout(r1)

        r2 = QHBoxLayout()
        r2.setSpacing(11)

        p_mech = Panel("Mechanisms")
        p_mech.add_layout(self._tile_grid(
            [("SRV1_DEG", "Eject Servo 1"), ("SRV2_DEG", "Eject Servo 2"),
             ("SRV3_DEG", "Brake Servo L"), ("SRV4_DEG", "Brake Servo R")],
            cols=2, spark=False))
        p_mech.stretch()
        r2.addWidget(p_mech, 1)

        p_sys = Panel("Systems Health")
        g = QGridLayout()
        g.setSpacing(6)
        self.sys_flags = {}
        flags = [("MCU_ACTIVE", "Active MCU", {0: "Primary", 1: "Backup"}),
                 ("WDOG_OK", "Watchdog", {0: "Fault", 1: "OK"}),
                 ("BEACON_ON", "Recovery Beacon", {0: "Off", 1: "On"}),
                 ("CAM1_OK", "Camera 1 Health", {0: "Fault", 1: "OK"}),
                 ("CAM2_OK", "Camera 2 Health", {0: "Fault", 1: "OK"})]
        for i, (k, lab, enum) in enumerate(flags):
            row, v = self._kv_row(lab, "—")
            wrap = QWidget()
            wrap.setLayout(row)
            g.addWidget(wrap, i, 0)
            self.sys_flags[k] = (v, enum)
        p_sys.add_layout(g)
        p_sys.add_layout(self._tile_grid(
            [("SD_PCT", "microSD Used"), ("FLASH_PCT", "Flash Used")],
            cols=2, spark=False))
        rowu, self.v_utc = self._kv_row("UTC  ·  DS3231", "—")
        p_sys.add_layout(rowu)
        r2.addWidget(p_sys, 1)

        p_alarm = Panel("Active Limits  ·  Worst First")
        self.tbl_alarm = QTableWidget(0, 5)
        self.tbl_alarm.setHorizontalHeaderLabels(
            ["PARAMETER", "VALUE", "UNIT", "STATE", "AGE"])
        self.tbl_alarm.verticalHeader().setVisible(False)
        self.tbl_alarm.setFont(font_mono(8.2))
        self.tbl_alarm.horizontalHeader().setFont(font_display(7.4, tracking=1.2))
        self.tbl_alarm.setEditTriggers(QAbstractItemView.NoEditTriggers)
        self.tbl_alarm.setSelectionMode(QAbstractItemView.NoSelection)
        self.tbl_alarm.setAlternatingRowColors(True)
        self.tbl_alarm.horizontalHeader().setSectionResizeMode(QHeaderView.Stretch)
        self.tbl_alarm.setMinimumHeight(210)
        p_alarm.add(self.tbl_alarm, 1)
        r2.addWidget(p_alarm, 2)
        lay.addLayout(r2)

        lay.addWidget(self._panel_lora())
        return page

    # ---- LoRa / regulatory toolbox --------------------------------------
    def _panel_lora(self):
        """Live Semtech AN1200.13 airtime, duty-cycle budget, link budget and
        the WPC licence-exempt compliance verdict. Entirely offline."""
        p = Panel("Radio Toolbox  ·  Semtech AN1200.13  ·  WPC India SRD Compliance")
        cols = QHBoxLayout()
        cols.setSpacing(14)

        def small(t):
            l = QLabel(t)
            l.setFont(font_display(7.4, tracking=1.2))
            l.setStyleSheet(f"color:{T.fg3}; background:transparent;")
            return l

        # --- modulation inputs -----------------------------------------
        c1 = QVBoxLayout()
        c1.setSpacing(5)
        c1.addWidget(SectionTitle("MODULATION"))
        grid = QGridLayout()
        grid.setSpacing(5)

        self.sp_sf = QSpinBox()
        self.sp_sf.setRange(7, 12)
        self.sp_sf.setValue(LORA_SF)
        self.cb_bw = QComboBox()
        for t_, v in (("125 kHz", 125000), ("250 kHz", 250000), ("500 kHz", 500000)):
            self.cb_bw.addItem(t_, v)
        self.cb_cr = QComboBox()
        for t_, v in (("4/5", 1), ("4/6", 2), ("4/7", 3), ("4/8", 4)):
            self.cb_cr.addItem(t_, v)
        self.sp_pl = QSpinBox()
        self.sp_pl.setRange(1, 255)
        self.sp_pl.setValue(55)
        self.sp_pre = QSpinBox()
        self.sp_pre.setRange(4, 64)
        self.sp_pre.setValue(8)
        self.ck_crc = QCheckBox("CRC")
        self.ck_crc.setChecked(True)
        self.ck_hdr = QCheckBox("EXPLICIT HEADER")
        self.ck_hdr.setChecked(True)
        self.ck_hdr.setFont(font_display(7.4, tracking=1.1))
        self.ck_crc.setFont(font_display(7.4, tracking=1.1))

        for i, (lab, w) in enumerate((("SPREADING FACTOR", self.sp_sf),
                                      ("BANDWIDTH", self.cb_bw),
                                      ("CODING RATE", self.cb_cr),
                                      ("PAYLOAD  bytes", self.sp_pl),
                                      ("PREAMBLE  symbols", self.sp_pre))):
            grid.addWidget(small(lab), i, 0)
            grid.addWidget(w, i, 1)
        grid.addWidget(self.ck_crc, 5, 0)
        grid.addWidget(self.ck_hdr, 5, 1)
        c1.addLayout(grid)
        c1.addStretch(1)
        cols.addLayout(c1, 1)

        # --- airtime results -------------------------------------------
        c2 = QVBoxLayout()
        c2.setSpacing(5)
        c2.addWidget(SectionTitle("AIRTIME  ·  DUTY CYCLE"))
        self.lora_out = {}
        for k, lab in (("tsym", "SYMBOL TIME"), ("ldro", "LOW DATA RATE OPT"),
                       ("toa", "TIME ON AIR"), ("rate", "BIT RATE"),
                       ("pph", "MAX PACKETS / HOUR @ 1%"),
                       ("air", "AIRTIME BUDGET / HOUR"),
                       ("used", "AT 1 Hz DOWNLINK")):
            row, v = self._kv_row(lab, "—")
            c2.addLayout(row)
            self.lora_out[k] = v
        c2.addStretch(1)
        cols.addLayout(c2, 1)

        # --- link budget -----------------------------------------------
        c3 = QVBoxLayout()
        c3.setSpacing(5)
        c3.addWidget(SectionTitle("LINK BUDGET"))
        lg = QGridLayout()
        lg.setSpacing(5)
        self.sp_tx = QDoubleSpinBox()
        self.sp_tx.setRange(-10, 36)
        self.sp_tx.setValue(22.0)
        self.sp_tx.setSuffix(" dBm")
        self.sp_gtx = QDoubleSpinBox()
        self.sp_gtx.setRange(-10, 30)
        self.sp_gtx.setValue(2.15)
        self.sp_gtx.setSuffix(" dBi")
        self.sp_grx = QDoubleSpinBox()
        self.sp_grx.setRange(-10, 30)
        self.sp_grx.setValue(5.0)
        self.sp_grx.setSuffix(" dBi")
        self.sp_loss = QDoubleSpinBox()
        self.sp_loss.setRange(0, 30)
        self.sp_loss.setValue(3.0)
        self.sp_loss.setSuffix(" dB")
        self.sp_sens = QDoubleSpinBox()
        self.sp_sens.setRange(-150, -80)
        self.sp_sens.setValue(LORA_SENS_DBM)
        self.sp_sens.setSuffix(" dBm")
        for i, (lab, w) in enumerate((("TX POWER", self.sp_tx),
                                      ("CANSAT ANT GAIN", self.sp_gtx),
                                      ("GROUND ANT GAIN", self.sp_grx),
                                      ("CABLE / MISC LOSS", self.sp_loss),
                                      ("RX SENSITIVITY", self.sp_sens))):
            lg.addWidget(small(lab), i, 0)
            lg.addWidget(w, i, 1)
        c3.addLayout(lg)
        for k, lab in (("rng", "CURRENT SLANT RANGE"),
                       ("fspl", "FREE-SPACE PATH LOSS"),
                       ("prx", "PREDICTED RX POWER"),
                       ("marg", "PREDICTED MARGIN"),
                       ("maxr", "FREE-SPACE CEILING  0 dB"),
                       ("maxr10", "FREE-SPACE CEILING  10 dB"),
                       ("horiz", "RADIO HORIZON  (BINDING)")):
            row, v = self._kv_row(lab, "—")
            c3.addLayout(row)
            self.lora_out[k] = v
        c3.addStretch(1)
        cols.addLayout(c3, 1)

        # --- WPC compliance --------------------------------------------
        c4 = QVBoxLayout()
        c4.setSpacing(5)
        c4.addWidget(SectionTitle("WPC INDIA  ·  LICENCE-EXEMPT SRD"))
        self.tbl_wpc = QTableWidget(0, 3)
        self.tbl_wpc.setHorizontalHeaderLabels(["CHECK", "VALUE", "VERDICT"])
        self.tbl_wpc.verticalHeader().setVisible(False)
        self.tbl_wpc.setFont(font_mono(8.2))
        self.tbl_wpc.horizontalHeader().setFont(font_display(7.4, tracking=1.2))
        self.tbl_wpc.setEditTriggers(QAbstractItemView.NoEditTriggers)
        self.tbl_wpc.setSelectionMode(QAbstractItemView.NoSelection)
        # The value column carries the long text; equal-width Stretch on all
        # three elided it to "0.158 W (22.0 ...".
        hh = self.tbl_wpc.horizontalHeader()
        hh.setSectionResizeMode(0, QHeaderView.ResizeToContents)
        hh.setSectionResizeMode(1, QHeaderView.Stretch)
        hh.setSectionResizeMode(2, QHeaderView.ResizeToContents)
        self.tbl_wpc.setMinimumHeight(140)
        c4.addWidget(self.tbl_wpc)
        cite = QLabel(
            f"GAZETTE G.S.R. 853(E) / 564(E)  ·  BAND "
            f"{WPC_BAND_LO_MHZ:.0f}–{WPC_BAND_HI_MHZ:.0f} MHz  ·  ETA SUB-BAND "
            f"{WPC_ETA_LO_MHZ:.0f}–{WPC_ETA_HI_MHZ:.0f} MHz  ·  MAX "
            f"{WPC_MAX_TX_W:.0f} W TX / {WPC_MAX_ERP_W:.0f} W ERP / "
            f"{WPC_MAX_BW_KHZ:.0f} kHz.  BW125 IS CHOSEN PRECISELY BECAUSE "
            f"BW250 WOULD BREACH THE 200 kHz CARRIER LIMIT.")
        cite.setFont(font_display(6.9, tracking=0.9))
        cite.setWordWrap(True)
        cite.setStyleSheet(f"color:{T.fg4}; background:transparent;")
        c4.addWidget(cite)
        c4.addStretch(1)
        cols.addLayout(c4, 1)

        p.add_layout(cols)

        for w in (self.sp_sf, self.sp_pl, self.sp_pre):
            w.valueChanged.connect(self._recalc_lora)
        for w in (self.sp_tx, self.sp_gtx, self.sp_grx, self.sp_loss, self.sp_sens):
            w.valueChanged.connect(self._recalc_lora)
        for w in (self.cb_bw, self.cb_cr):
            w.currentIndexChanged.connect(self._recalc_lora)
        for w in (self.ck_crc, self.ck_hdr):
            w.stateChanged.connect(self._recalc_lora)
        QTimer.singleShot(0, self._recalc_lora)
        return p

    def _recalc_lora(self):
        try:
            sf = self.sp_sf.value()
            bw = self.cb_bw.currentData()
            cr = self.cb_cr.currentData()
            pl = self.sp_pl.value()
            pre = self.sp_pre.value()
            crc = self.ck_crc.isChecked()
            hdr = self.ck_hdr.isChecked()

            tsym = lora_symbol_time_s(sf, bw)
            ldro = lora_ldro_required(sf, bw)
            toa = lora_time_on_air(sf, bw, cr, pl, pre, hdr, crc)
            rate = lora_bitrate_bps(sf, bw, cr)
            pph, air = duty_cycle_budget(toa)

            o = self.lora_out
            o["tsym"].setText(f"{tsym * 1000:.3f} ms")
            o["ldro"].setText("ENABLED  (symbol > 16 ms)" if ldro else "disabled")
            o["ldro"].setStyleSheet(
                f"color:{T.amber if ldro else T.fg}; background:transparent;")
            o["toa"].setText(f"{toa * 1000:.2f} ms")
            o["rate"].setText(f"{rate:,.0f} bps")
            o["pph"].setText(f"{pph}")
            o["air"].setText(f"{air:.1f} s")

            # A 1 Hz downlink transmits 3600 frames an hour; does that fit?
            used = toa * 3600.0
            frac = used / 3600.0
            ok = used <= air
            o["used"].setText(
                f"{used:.0f} s/h  =  {frac * 100:.2f} %   "
                f"{'WITHIN 1 % BUDGET' if ok else 'EXCEEDS 1 % BUDGET'}")
            o["used"].setStyleSheet(
                f"color:{T.fg if ok else T.amber}; background:transparent;")

            dist = _finite(self.dv.get("DIST_M"), 0.0)
            alt = _finite(self.d.get("ALT_M"), 0.0)
            slant = math.sqrt(dist * dist + alt * alt)
            slant = max(slant, 1.0)
            freq = _finite(self.d.get("FREQ_MHZ"), LORA_FREQ_MHZ) or LORA_FREQ_MHZ
            fspl = free_space_path_loss_db(slant, freq)
            prx, marg = link_budget(self.sp_tx.value(), self.sp_gtx.value(),
                                    self.sp_grx.value(), slant, freq,
                                    self.sp_loss.value(), self.sp_sens.value())
            o["rng"].setText(f"{slant:,.0f} m")
            o["fspl"].setText(f"{fspl:.1f} dB")
            o["prx"].setText(f"{prx:.1f} dBm")
            o["marg"].setText(f"{marg:+.1f} dB")
            o["marg"].setStyleSheet(
                f"color:{T.fg if marg > 10 else (T.amber if marg > 3 else T.red)};"
                f" background:transparent;")
            o["maxr"].setText(
                f"{max_range_m(self.sp_tx.value(), self.sp_gtx.value(), self.sp_grx.value(), freq, self.sp_loss.value(), self.sp_sens.value(), 0.0):,.0f} m")
            o["maxr10"].setText(
                f"{max_range_m(self.sp_tx.value(), self.sp_gtx.value(), self.sp_grx.value(), freq, self.sp_loss.value(), self.sp_sens.value(), 10.0):,.0f} m")
            horiz = radio_horizon_m(alt, 2.0)
            o["horiz"].setText(f"{horiz:,.0f} m   (at {alt:,.0f} m AGL)")
            o["horiz"].setStyleSheet(f"color:{T.fg2}; background:transparent;")

            rows = wpc_compliance(self.sp_tx.value(), self.sp_gtx.value(), bw, freq)
            self.tbl_wpc.setRowCount(len(rows))
            for i, (label, val, verdict) in enumerate(rows):
                a = QTableWidgetItem(label)
                b = QTableWidgetItem(val)
                c = QTableWidgetItem(verdict)
                ink = {"PASS": T.fg, "NOTE": T.amber, "FAIL": T.red}[verdict]
                c.setForeground(QColor(ink))
                a.setForeground(QColor(T.fg3))
                b.setForeground(QColor(T.fg))
                for col, it in enumerate((a, b, c)):
                    self.tbl_wpc.setItem(i, col, it)
        except Exception:
            log.exception("LoRa recalc failed")

    # =====================================================================
    # VIEW 4 — ANALYSIS  (free-form plotting and statistics)
    # =====================================================================
    def _view_analysis(self):
        page, lay = self._page()

        ctl = Panel("Plot Selection")
        row = QHBoxLayout()
        row.setSpacing(9)
        self.an_selectors = []
        plottable = [k for k, m in FIELD_META.items() if not m["disc"]]
        defaults = ["ALT_M", "VSPEED", "TEMP_C", "PRES_PA",
                    "BUS_V", "SPIN_DPS", "TILT", "RSSI"]
        grid = QGridLayout()
        grid.setSpacing(9)
        self.an_plots = []
        for i in range(8):
            cb = QComboBox()
            cb.setFont(font_mono(8.5))
            for k in plottable:
                m = FIELD_META[k]
                cb.addItem(f"{k}   {m['u']}".strip(), k)
            key = defaults[i] if i < len(defaults) else plottable[i]
            j = cb.findData(key)
            if j >= 0:
                cb.setCurrentIndex(j)
            pl = self._plot(key, key)
            self.an_plots.append(pl)
            self.an_selectors.append(cb)
            cb.currentIndexChanged.connect(
                lambda _i, idx=i: self._retarget_plot(idx))
            box = QVBoxLayout()
            box.setSpacing(4)
            box.addWidget(cb)
            box.addWidget(pl)
            wrap = QWidget()
            wrap.setLayout(box)
            grid.addWidget(wrap, i // 4, i % 4)
        ctl.add_layout(grid)
        lay.addWidget(ctl, 1)

        bottom = QHBoxLayout()
        bottom.setSpacing(11)

        p_stat = Panel("Flight Statistics  ·  All Received Parameters")
        self.tbl_stats = QTableWidget(0, 7)
        self.tbl_stats.setHorizontalHeaderLabels(
            ["PARAMETER", "UNIT", "N", "MIN", "MAX", "MEAN", "LATEST"])
        self.tbl_stats.verticalHeader().setVisible(False)
        self.tbl_stats.setFont(font_mono(8.2))
        self.tbl_stats.horizontalHeader().setFont(font_display(7.4, tracking=1.2))
        self.tbl_stats.setEditTriggers(QAbstractItemView.NoEditTriggers)
        self.tbl_stats.setAlternatingRowColors(True)
        self.tbl_stats.setSortingEnabled(True)
        self.tbl_stats.horizontalHeader().setSectionResizeMode(QHeaderView.Stretch)
        self.tbl_stats.setMinimumHeight(260)
        p_stat.add(self.tbl_stats, 1)
        b_csv = GhostButton("EXPORT STATISTICS CSV", width=190)
        b_csv.clicked.connect(self._export_stats)
        p_stat.add(b_csv)
        bottom.addWidget(p_stat, 2)

        p_xy = Panel("Cross-Plot  ·  X vs Y")
        sel = QHBoxLayout()
        sel.setSpacing(7)
        self.cb_x = QComboBox()
        self.cb_y = QComboBox()
        for cb, dflt in ((self.cb_x, "TEMP_C"), (self.cb_y, "ALT_M")):
            cb.setFont(font_mono(8.5))
            for k in plottable:
                cb.addItem(k, k)
            j = cb.findData(dflt)
            if j >= 0:
                cb.setCurrentIndex(j)
            cb.currentIndexChanged.connect(self._update_xy)
        lx = QLabel("X")
        lx.setFont(font_display(7.6, tracking=1.2))
        lx.setStyleSheet(f"color:{T.fg3};background:transparent;")
        ly = QLabel("Y")
        ly.setFont(font_display(7.6, tracking=1.2))
        ly.setStyleSheet(f"color:{T.fg3};background:transparent;")
        sel.addWidget(lx)
        sel.addWidget(self.cb_x, 1)
        sel.addWidget(ly)
        sel.addWidget(self.cb_y, 1)
        p_xy.add_layout(sel)
        self.pw_xy = pg.PlotWidget()
        self.pw_xy.setMinimumHeight(260)
        self.pw_xy.showGrid(x=True, y=True, alpha=0.16)
        self.pw_xy.setMenuEnabled(False)
        pi = self.pw_xy.getPlotItem()
        pi.hideButtons()
        for ax in ("left", "bottom"):
            a = pi.getAxis(ax)
            a.setPen(pg.mkPen(T.line_dim, width=1))
            a.setTextPen(pg.mkPen(T.fg3))
            a.setStyle(tickFont=font_mono(7), tickLength=-4)
        pi.getAxis("left").setWidth(58)
        self.xy_curve = self.pw_xy.plot(
            [], [], pen=None, symbol="o", symbolSize=3,
            symbolBrush=pg.mkBrush(T.SEQ[3]), symbolPen=None)
        p_xy.add(self.pw_xy, 1)
        bottom.addWidget(p_xy, 1)
        lay.addLayout(bottom)
        return page

    def _retarget_plot(self, idx):
        """Rebind one analysis plot to a different parameter and backfill it
        from the recorded history, so the operator sees the whole flight
        immediately rather than waiting for new packets."""
        try:
            cb = self.an_selectors[idx]
            key = cb.currentData()
            old = self.an_plots[idx]
            parent_layout = old.parent().layout()
            new = self._plot(key, key)
            for lst in self.plots.values():
                if old in lst:
                    lst.remove(old)
            parent_layout.replaceWidget(old, new)
            old.setParent(None)
            old.deleteLater()
            self.an_plots[idx] = new
            for t, v in self.history.get(key, ()):
                new.push(t, v)
            new.redraw()
        except Exception:
            log.exception("retarget plot failed")

    def _update_xy(self):
        try:
            kx = self.cb_x.currentData()
            ky = self.cb_y.currentData()
            hx = dict(self.history.get(kx, ()))
            hy = dict(self.history.get(ky, ()))
            common = sorted(set(hx) & set(hy))
            self.xy_curve.setData([hx[t] for t in common], [hy[t] for t in common])
            pi = self.pw_xy.getPlotItem()
            pi.getAxis("bottom").setLabel(
                f"{kx}  [{FIELD_META[kx]['u']}]",
                **{"color": T.fg3, "font-size": "7.5pt"})
            pi.getAxis("left").setLabel(
                f"{ky}  [{FIELD_META[ky]['u']}]",
                **{"color": T.fg3, "font-size": "7.5pt"})
        except Exception:
            log.exception("cross-plot failed")

# =====================================================================
    # VIEW 5 — RECOVERY
    # =====================================================================
    def _view_recovery(self):
        page, lay = self._page()
        row = QHBoxLayout()
        row.setSpacing(11)

        p_map = Panel("Ground Track")
        self.track_recovery = GroundTrack()
        self.track_recovery.setMinimumHeight(480)
        p_map.add(self.track_recovery, 1)
        row.addWidget(p_map, 3)

        col = QVBoxLayout()
        col.setSpacing(11)

        p_fix = Panel("Last Known Fix")
        self.v_pos_fmt = {}
        # NB: not `row` — that name is the outer QHBoxLayout of this view.
        for key, lab in (("dec", "Decimal"), ("dms", "Deg Min Sec"),
                         ("dmm", "Deg Dec-Min"), ("utm", "UTM Grid")):
            kv, v = self._kv_row(lab, "—")
            p_fix.add_layout(kv)
            self.v_pos_fmt[key] = v
        p_fix.add_layout(self._tile_grid(
            [("GPS_ALT_M", "GNSS Altitude"), ("SATS", "Satellites"),
             ("DIST_M", "Range from Pad"), ("BEARING", "Bearing from Pad")],
            cols=2, spark=False))
        rowf, self.v_fix_age = self._kv_row("FIX AGE", "—")
        p_fix.add_layout(rowf)
        b_copy_all = GhostButton("Copy All Formats")
        b_copy_all.clicked.connect(self._copy_all_formats)
        p_fix.add(b_copy_all)
        col.addWidget(p_fix)
        col.addWidget(self._panel_recovery_guidance())
        col.addWidget(self._panel_position_quality())

        p_desc = Panel("Descent")
        p_desc.add_layout(self._tile_grid(
            [("ALT_M", "Altitude AGL"), ("SINK_RATE", "Sink Rate"),
             ("TTL_S", "Time to Land"), ("GLIDE_LD", "Glide L/D")], cols=2))
        col.addWidget(p_desc)

        p_act = Panel("Recovery Actions")
        for text, fn in (("Export KML for Google Earth", self._export_kml),
                         ("Open Last Fix in Browser Map", self._open_map),
                         ("Copy Coordinates to Clipboard", self._copy_coords),
                         ("Beacon On  (Uplink)", lambda: self._send_named("CMD_BEACON_ON")),
                         ("Beacon Off  (Uplink)", lambda: self._send_named("CMD_BEACON_OFF"))):
            b = GhostButton(text)
            b.clicked.connect(fn)
            p_act.add(b)
        note = QLabel(
            "KML and the browser map are file-based. "
            "Nothing in the field flow depends on a live tile server, so "
            "recovery still works with no network at the launch site.")
        note.setFont(font_display(7, tracking=1.0))
        note.setWordWrap(True)
        note.setStyleSheet(f"color:{T.fg4}; background:transparent;")
        p_act.add(note)
        col.addWidget(p_act)
        col.addStretch(1)

        side = QWidget()
        side.setLayout(col)
        side.setMinimumWidth(430)
        side.setMaximumWidth(560)
        side.setSizePolicy(QSizePolicy.Fixed, QSizePolicy.Preferred)
        row.addWidget(side, 0)
        lay.addLayout(row, 1)
        return page

    # ---- position quality / NavIC / DGNSS --------------------------------
    def _panel_position_quality(self):
        """Everything that determines how much to trust the pin."""
        p = Panel("Position Quality  ·  NavIC / GNSS")
        self.v_pos = {}
        for key, lab in (("fix", "Fix Type"),
                         ("const", "Constellations"),
                         ("hdop", "HDOP / VDOP"),
                         ("sigma", "Horizontal 1-sigma"),
                         ("cep", "CEP 50%"),
                         ("r95", "95% Containment")):
            row, v = self._kv_row(lab, "—")
            p.add_layout(row)
            self.v_pos[key] = v

        rule = QFrame()
        rule.setFixedHeight(1)
        rule.setStyleSheet(f"background:{T.line_dim};border:none;")
        p.add(rule)
        p.add(SectionTitle("Pad Self-Survey"))
        for key, lab in (("pad_n", "Fixes Averaged"),
                         ("pad_acc", "Reference Accuracy"),
                         ("pad_pos", "Surveyed Pad")):
            row, v = self._kv_row(lab, "—")
            p.add_layout(row)
            self.v_pos[key] = v
        b_freeze = GhostButton("Freeze Pad Reference")
        b_freeze.clicked.connect(self._freeze_pad)
        p.add(b_freeze)

        rule2 = QFrame()
        rule2.setFixedHeight(1)
        rule2.setStyleSheet(f"background:{T.line_dim};border:none;")
        p.add(rule2)
        grow = QHBoxLayout()
        grow.setSpacing(6)
        gl = QLabel("DGNSS  (Optional)")
        gl.setFont(font_display(7.4, tracking=1.2))
        gl.setStyleSheet(f"color:{T.fg4};background:transparent;")
        self.cb_gs_port = PortCombo()
        self.cb_gs_port.setMinimumWidth(120)
        self.b_gs_gnss = GhostButton("ATTACH", width=88)
        self.b_gs_gnss.clicked.connect(self._toggle_gs_gnss)
        grow.addWidget(gl)
        grow.addStretch(1)
        grow.addWidget(self.cb_gs_port, 1)
        grow.addWidget(self.b_gs_gnss)
        p.add_layout(grow)
        for key, lab in (("gs_fix", "GS Receiver"), ("dg_state", "Correction")):
            row, v = self._kv_row(lab, "—")
            p.add_layout(row)
            self.v_pos[key] = v
        for k in ("gs_survey", "dg_err", "dg_age"):
            lbl = QLabel(p)
            lbl.hide()
            self.v_pos[k] = lbl
        return p

    # ---- recovery guidance ----------------------------------------------
    def _panel_recovery_guidance(self):
        p = Panel("Recovery Guidance  ·  Projected Landing")
        self.v_rec = {}

        p.add(SectionTitle("Wind, from the Vehicle's Drift"))
        for key, lab in (("wind", "Wind Speed"), ("wind_dir", "Blowing From")):
            row, v = self._kv_row(lab, "—")
            p.add_layout(row)
            self.v_rec[key] = v

        rule = QFrame()
        rule.setFixedHeight(1)
        rule.setStyleSheet(f"background:{T.line_dim};border:none;")
        p.add(rule)
        p.add(SectionTitle("Projected Touchdown"))
        for key, lab in (("land", "Projected Position"),
                         ("land_ttl", "Time Remaining"),
                         ("land_drift", "Drift Still to Come"),
                         ("land_from_pad", "Range / Bearing from Pad")):
            row, v = self._kv_row(lab, "—")
            p.add_layout(row)
            self.v_rec[key] = v

        rule2 = QFrame()
        rule2.setFixedHeight(1)
        rule2.setStyleSheet(f"background:{T.line_dim};border:none;")
        p.add(rule2)
        p.add(SectionTitle("Walk-To Navigation"))
        entry = QHBoxLayout()
        entry.setSpacing(6)
        el = QLabel("My Position")
        el.setFont(font_display(7.4, tracking=1.2))
        el.setStyleSheet(f"color:{T.fg3};background:transparent;")
        self.ed_mypos = QLineEdit()
        self.ed_mypos.setPlaceholderText(
            "live from the GS receiver — or type lat, lon to override")
        self.ed_mypos.setFont(font_mono(8.5))
        b_pad = GhostButton("Use Pad", width=88)
        b_pad.clicked.connect(self._use_pad_as_my_position)
        entry.addWidget(el)
        entry.addWidget(self.ed_mypos, 1)
        entry.addWidget(b_pad)
        p.add_layout(entry)
        for key, lab in (("walk_target", "Target"),
                         ("walk_range", "Distance to Walk"),
                         ("walk_bearing", "Heading"),
                         ("search", "Search Radius")):
            row, v = self._kv_row(lab, "—")
            p.add_layout(row)
            self.v_rec[key] = v
        note = QLabel(
            "My Position comes live from the ground-station GNSS receiver "
            "(attach it in Position Quality below); type into the box only to "
            "override it. The projected touchdown uses the wind measured from "
            "the CanSat's own drift — under canopy it has no lateral "
            "propulsion, so its ground track is the air mass. Search radius "
            "combines fix uncertainty, drift during any telemetry gap, and "
            "remaining descent.")
        note.setFont(font_display(6.9, tracking=0.9))
        note.setWordWrap(True)
        note.setStyleSheet(f"color:{T.fg4}; background:transparent;")
        p.add(note)
        return p

    def _use_pad_as_my_position(self):
        if self.phys.lat0 is None:
            QMessageBox.warning(self, "No pad fix",
                                "No launch-site fix has been received yet.")
            return
        self.ed_mypos.setText(f"{self.phys.lat0:.7f}, {self.phys.lon0:.7f}")

    def _my_position(self):
        """Where the operator is, for the walk-to solution and the ME pin.

        A TYPED POSITION WINS, A LIVE ONE IS THE DEFAULT. Until now this read
        the text box only, so recovery navigation required pasting coordinates
        off a phone and re-pasting them every time you moved — which is exactly
        the thing you cannot do while walking through a field looking at a
        laptop. The ground-station GNSS receiver was already attached and
        already parsed (see _on_gs_fix), it was simply never consulted here.

        The manual box still overrides, deliberately: it is how you plan a walk
        from somewhere you are not yet standing, and how recovery still works
        when no receiver is attached.
        """
        txt = (self.ed_mypos.text() or "").strip()
        if not txt:
            gs = getattr(self, "gs_fix", None) or {}
            la, lo = gs.get("lat"), gs.get("lon")
            if la is not None and lo is not None:
                return la, lo
            return None
        for sep in (",", " "):
            if sep in txt:
                a, _, b = txt.partition(sep)
                la, lo = _finite(a.strip(), None), _finite(b.strip(), None)
                if la is not None and lo is not None and abs(la) <= 90 and abs(lo) <= 180:
                    return la, lo
                break
        return None

    def _update_recovery_guidance(self, now):
        lp = self.landing
        if lp.valid:
            self.v_rec["wind"].setText(f"{lp.wind_speed:.1f} m/s")
            self.v_rec["wind_dir"].setText(f"{lp.wind_bearing_from:.0f}°")
        else:
            self.v_rec["wind"].setText("—")
            self.v_rec["wind_dir"].setText("—")

        fix = self.pos.get("fix")
        alt = _finite(self.d.get("ALT_M"), None) if "ALT_M" in self.present else None
        sink = _finite(self.dv.get("SINK_RATE"), 0.0)
        proj = lp.project(fix[0], fix[1], alt, sink) if (fix and alt is not None) else None

        target = None
        if proj:
            la, lo, ttl, drift = proj
            target = (la, lo)
            self.v_rec["land"].setText(f"{la:.7f}, {lo:.7f}")
            self.v_rec["land_ttl"].setText(f"{ttl:.0f} s")
            self.v_rec["land_drift"].setText(f"{drift:,.0f} m")
            if self.phys.lat0 is not None:
                rng = Physics.hav_dist(self.phys.lat0, self.phys.lon0, la, lo)
                brg = Physics.bearing(self.phys.lat0, self.phys.lon0, la, lo)
                self.v_rec["land_from_pad"].setText(f"{rng:,.0f} m   {brg:.0f}°")
            self.track_recovery.set_projection(la, lo)
        else:
            for k in ("land", "land_ttl", "land_drift", "land_from_pad"):
                self.v_rec[k].setText("—")
            self.track_recovery.set_projection(None, None)
            target = fix

        p_lat = self.limits.params.get("LAT")
        gap = (now - p_lat.last_seen) if (p_lat and p_lat.last_seen) else 0.0
        remaining = proj[3] if proj else 0.0
        radius = search_radius_m(self.pos.get("sigma"), gap,
                                 lp.wind_speed if lp.valid else 0.0, remaining)
        self.v_rec["search"].setText(f"{radius:,.0f} m")
        self.track_recovery.set_search_radius(radius if target else None)

        me = self._my_position()
        # Put the operator on BOTH maps, not just in the numbers. A bearing and
        # a range are what you steer by, but the picture is what tells you which
        # side of the canal or the tree line you are on.
        for tr in (self.track_flight, self.track_recovery):
            tr.set_me(me[0], me[1]) if me else tr.set_me(None, None)

        if target and me:
            rng = Physics.hav_dist(me[0], me[1], target[0], target[1])
            brg = Physics.bearing(me[0], me[1], target[0], target[1])
            self.v_rec["walk_target"].setText(
                f"{target[0]:.6f}, {target[1]:.6f}"
                + ("  (projected)" if proj else "  (last fix)"))
            self.v_rec["walk_range"].setText(f"{rng:,.0f} m")
            self.v_rec["walk_bearing"].setText(
                f"{brg:.0f}°  {_compass_point(brg)}")
        else:
            for k in ("walk_target", "walk_range", "walk_bearing"):
                self.v_rec[k].setText("—" if not me else "awaiting fix")

    def _freeze_pad(self):
        r = self.pad_survey.freeze()
        if not r:
            QMessageBox.warning(self, "No fixes",
                                "No GNSS fixes have been averaged on the pad yet.")
            return
        self.dgnss.set_truth(r[0], r[1], r[2])
        self._log_status(
            f"Pad reference frozen from {r[4]} fixes, accuracy {r[3]:.2f} m")

    def _toggle_gs_gnss(self):
        if self.gs_gnss and self.gs_gnss.is_alive():
            self.gs_gnss.stop()
            self.gs_gnss = None
            self.b_gs_gnss.setText("ATTACH")
            self._log_status("Ground-station GNSS detached")
            return
        port = self.cb_gs_port.selected_port()
        if not port or port.startswith("("):
            QMessageBox.warning(
                self, "No port",
                "Select the COM port of the ground-station GNSS receiver.")
            return
        self.gs_gnss = GsGnssLink(port, 9600, self._on_gs_fix)
        self.gs_gnss.start()
        self.b_gs_gnss.setText("DETACH")
        self._log_status(f"Ground-station GNSS attached on {port}")

    def _on_gs_fix(self, st):
        self.gs_fix = st
        lat, lon = st.get("lat"), st.get("lon")
        if lat is None or lon is None:
            return
        if not self.gs_survey.fixed:
            self.gs_survey.add(lat, lon, st.get("alt") or 0.0)
        self.dgnss.update_base(lat, lon, st.get("alt") or 0.0, st.get("t"))

    def _copy_all_formats(self):
        fix = self.pos.get("fix")
        if not fix:
            QMessageBox.warning(self, "No fix", "No GNSS fix received yet.")
            return
        la, lo = fix
        z, hemi, e, n = to_utm(la, lo)
        txt = (f"{la:.7f}, {lo:.7f}\n"
               f"{to_dms(la, True)}  {to_dms(lo, False)}\n"
               f"{to_dmm(la, True)}  {to_dmm(lo, False)}\n"
               f"UTM {z}{hemi} {e:.0f}E {n:.0f}N\n"
               f"source: {self.pos.get('source')}")
        QApplication.clipboard().setText(txt)
        self._log_status("Copied all coordinate formats to clipboard")

    def _update_position(self, now):
        fix = self.pos.get("fix")
        if fix:
            la, lo = fix
            z, hemi, e, n = to_utm(la, lo)
            self.v_pos_fmt["dec"].setText(f"{la:.7f}, {lo:.7f}")
            self.v_pos_fmt["dms"].setText(
                f"{to_dms(la, True)}  {to_dms(lo, False)}")
            self.v_pos_fmt["dmm"].setText(
                f"{to_dmm(la, True)}  {to_dmm(lo, False)}")
            self.v_pos_fmt["utm"].setText(f"{z}{hemi}  {e:,.0f}E  {n:,.0f}N")

        P = self.limits.params
        fq = int(_finite(P["FIX_QUALITY"].value, 1)) \
            if P["FIX_QUALITY"].ever_seen else None
        hdop = _finite(P["HDOP"].value, None) if P["HDOP"].ever_seen else None
        vdop = _finite(P["VDOP"].value, None) if P["VDOP"].ever_seen else None

        src = self.pos.get("source", "—")
        self.v_pos["fix"].setText(src)
        self.v_pos["fix"].setStyleSheet(
            f"color:{T.green if self.pos.get('applied') or fq == 2 else T.fg};"
            f" background:transparent;")

        if P["GNSS_SYS"].ever_seen:
            names = decode_constellations(P["GNSS_SYS"].value)
            self.v_pos["const"].setText(" + ".join(names) if names else "—")
        elif P["NAVIC_SATS"].ever_seen and _finite(P["NAVIC_SATS"].value) > 0:
            self.v_pos["const"].setText(
                f"GNSS + NavIC ({int(_finite(P['NAVIC_SATS'].value))} SV)")
        else:
            self.v_pos["const"].setText("—")

        self.v_pos["hdop"].setText(
            f"{hdop:.2f}  /  {vdop:.2f}" if (hdop and vdop)
            else (f"{hdop:.2f}  /  —" if hdop else "—  (firmware not sending)"))

        sigma = self.pos.get("sigma")
        self.v_pos["sigma"].setText(f"{sigma:.2f} m" if sigma else "—")
        self.v_pos["cep"].setText(
            f"{cep50_m(sigma):.2f} m" if sigma else "—")
        self.v_pos["r95"].setText(f"{r95_m(sigma):.2f} m" if sigma else "—")

        s = self.pad_survey
        self.v_pos["pad_n"].setText(f"{s.n}")
        acc = s.accuracy_m()
        self.v_pos["pad_acc"].setText(f"{acc:.2f} m" if acc else "—")
        if s.result:
            self.v_pos["pad_pos"].setText(f"{s.result[0]:.7f}, {s.result[1]:.7f}")
            self.v_pos["pad_pos"].setStyleSheet(
                f"color:{T.green}; background:transparent;")
        elif s.mean:
            self.v_pos["pad_pos"].setText(
                f"{s.mean[0]:.7f}, {s.mean[1]:.7f}  (live)")

        if self.gs_gnss and self.gs_gnss.is_alive():
            g = self.gs_fix
            gq = g.get("fix_quality", 0)
            self.v_pos["gs_fix"].setText(
                f"{FIX_QUALITY_LABEL.get(gq, '—')}   {g.get('sats', 0)} SV")
            gacc = self.gs_survey.accuracy_m()
            self.v_pos["gs_survey"].setText(
                f"{self.gs_survey.n} fixes   "
                + (f"{gacc:.2f} m" if gacc else "—"))
        else:
            self.v_pos["gs_fix"].setText("not attached")
            self.v_pos["gs_survey"].setText("—")

        if self.dgnss.truth and self.dgnss.observed:
            self.v_pos["dg_err"].setText(
                f"{self.dgnss.error_magnitude_m():.2f} m   "
                f"(E {self.dgnss.d_east:+.2f}, N {self.dgnss.d_north:+.2f})")
            age = self.dgnss.age(now)
            self.v_pos["dg_age"].setText(f"{age:.1f} s" if age else "—")
        ok = self.dgnss.available(now)
        self.v_pos["dg_state"].setText(
            "APPLIED" if self.pos.get("applied") else
            ("READY" if ok else "not available"))
        self.v_pos["dg_state"].setStyleSheet(
            f"color:{T.green if self.pos.get('applied') else T.fg2};"
            f" background:transparent;")

    # =====================================================================
    # VIEW 6 — CONSOLE
    # =====================================================================
    def _view_console(self):
        page, lay = self._page()

        top = QHBoxLayout()
        top.setSpacing(11)

        p_pkt = Panel("Packet Stream")
        bar = QHBoxLayout()
        bar.setSpacing(7)
        self.ck_raw = QCheckBox("SHOW RAW CSV")
        self.ck_raw.setFont(font_display(7.4, tracking=1.1))
        self.ck_pause = QCheckBox("PAUSE")
        self.ck_pause.setFont(font_display(7.4, tracking=1.1))
        b_clr = GhostButton("CLEAR", width=76)
        b_clr.clicked.connect(lambda: self.txt_packets.clear())
        bar.addWidget(self.ck_raw)
        bar.addWidget(self.ck_pause)
        bar.addStretch(1)
        bar.addWidget(b_clr)
        p_pkt.add_layout(bar)
        self.txt_packets = QPlainTextEdit()
        self.txt_packets.setReadOnly(True)
        self.txt_packets.setFont(font_mono(8.2))
        self.txt_packets.setMaximumBlockCount(4000)
        self.txt_packets.setMinimumHeight(330)
        p_pkt.add(self.txt_packets, 1)
        top.addWidget(p_pkt, 3)

        p_ev = Panel("Event Log  ·  State Transitions")
        self.txt_events = QPlainTextEdit()
        self.txt_events.setReadOnly(True)
        self.txt_events.setFont(font_mono(8.2))
        self.txt_events.setMaximumBlockCount(3000)
        self.txt_events.setMinimumHeight(330)
        p_ev.add(self.txt_events, 1)
        top.addWidget(p_ev, 2)
        lay.addLayout(top, 1)

        p_cmd = Panel("Command History")
        self.tbl_cmd = QTableWidget(0, 4)
        self.tbl_cmd.setHorizontalHeaderLabels(
            ["WALL CLOCK", "MET", "COMMAND", "RESULT"])
        self.tbl_cmd.verticalHeader().setVisible(False)
        self.tbl_cmd.setFont(font_mono(8.2))
        self.tbl_cmd.horizontalHeader().setFont(font_display(7.4, tracking=1.2))
        self.tbl_cmd.setEditTriggers(QAbstractItemView.NoEditTriggers)
        self.tbl_cmd.setAlternatingRowColors(True)
        self.tbl_cmd.horizontalHeader().setSectionResizeMode(QHeaderView.Stretch)
        self.tbl_cmd.setMinimumHeight(180)
        p_cmd.add(self.tbl_cmd, 1)
        lay.addWidget(p_cmd)
        return page

    # =====================================================================
    # §13  UPDATE LOOP
    # =====================================================================
    def _safe(self, name, fn, *a):
        try:
            fn(*a)
            self.degraded.discard(name)
        except Exception:
            if name not in self.degraded:
                log.exception("panel '%s' failed", name)
            self.degraded.add(name)

    def _toggle_blink(self):
        self._blink = not self._blink
        if self.limits.master in IS_ALARM:
            self._safe("alarm-blink", lambda: self.master_alarm.update_state(
                self.limits.master, self.limits.active, self._blink))

    def _tick(self):
        now = time.time()

        drained = 0
        while drained < MAX_DRAIN_PER_TICK:
            try:
                line = self.rx_q.get_nowait()
            except queue.Empty:
                break
            drained += 1
            self._ingest(line, now)

        self.limits.evaluate(now)

        self._safe("chips", self._update_chips, now)
        self._safe("tiles", self._update_tiles)
        self._safe("limbars", self._update_limbars)
        self._safe("lamps", self._update_lamps)
        self._safe("instruments", self._update_instruments)
        self._safe("stepper", self._update_stepper)
        self._safe("plots", self._update_plots)
        self._safe("profiles", self._update_profiles)
        self._safe("spectral", self._update_spectral)
        self._safe("alarms", self._update_alarm_table)
        self._safe("sysflags", self._update_sys_flags)
        self._safe("events", self._update_events)
        self._safe("recovery", self._update_recovery, now)
        self._safe("position", self._update_position, now)
        self._safe("recovery-guidance", self._update_recovery_guidance, now)
        self._safe("master", lambda: self.master_alarm.update_state(
            self.limits.master, self.limits.active, self._blink))
        self._safe("audio", self._audio_alert, now)
        self._safe("status", self._update_status)

    def _ingest(self, line, now):
        try:
            values, present = parse_packet(line)
        except Exception:
            log.exception("parse failed")
            return
            
        # Log diagnostic messages safely to console view directly, drop from CSV/schema logic
        if not present:
            if line.startswith(("//", "──", "══", "[RF]", "[TX]", "[RX]", "PKT #")) and not self.ck_pause.isChecked():
                self.txt_packets.appendPlainText(f"   {line}")
            return

        self.raw_last = line
        self.pkt_count += 1
        if self.t0 is None:
            self.t0 = now
        self.last_pkt_wall = now
        self._rate_win.append(now)

        try:
            derived, dpres = self.phys.compute(values, self.pkt_count, present)
        except Exception:
            log.exception("physics failed")
            derived, dpres = {}, set()

        self.d, self.present = values, present
        self.dv, self.dp = derived, dpres

        self.logger.write(values, present)
        self.limits.ingest(values, present, derived, dpres, now)

        t = _finite(values.get("TIME_S"), self.pkt_count / max(TELEMETRY_HZ, 0.1))
        self.met = t
        for k in present:
            if FIELD_META.get(k, {}).get("disc") or k == "UTC":
                continue
            self.history.setdefault(k, deque(maxlen=6000)).append(
                (t, _finite(values.get(k))))
        for k in dpres:
            self.history.setdefault(k, deque(maxlen=6000)).append(
                (t, _finite(derived.get(k))))

        for pl_list in self.plots.values():
            for pl in pl_list:
                key = pl.key
                if key in present:
                    pl.push(t, _finite(values.get(key)))
                elif key in dpres:
                    pl.push(t, _finite(derived.get(key)))

        if {"LAT", "LON"} <= present:
            la, lo = _finite(values.get("LAT")), _finite(values.get("LON"))
            st = int(_finite(values.get("STATE")))
            alt = _finite(values.get("ALT_M"))

            if st == 0:
                self.pad_survey.add(la, lo, _finite(values.get("GPS_ALT_M")))

            cla, clo, applied = self.dgnss.correct(la, lo, now)

            hdop = _finite(values.get("HDOP"), None) if "HDOP" in present else None
            fq = int(_finite(values.get("FIX_QUALITY"), 1)) \
                if "FIX_QUALITY" in present else 1
            sigma = horizontal_sigma_m(hdop, fq, dual_band=True)
            if applied and sigma is not None:
                sigma = max(0.8, sigma * 0.25)

            self.pos = {
                "raw": (la, lo), "fix": (cla, clo), "applied": applied,
                "sigma": sigma,
                "source": "DGNSS CORRECTED" if applied else
                          FIX_QUALITY_LABEL.get(fq, "GNSS SPS"),
            }
            if st >= 2 and _finite(self.dv.get("SINK_RATE"), 0.0) > 0.4:
                self.landing.update(t, cla, clo)

            for tr in (self.track_flight, self.track_recovery):
                tr.add(cla, clo, st, alt)
                tr.set_uncertainty(sigma, applied)

        for key in ("TEMP_C", "PRES_PA", "HUM_PCT"):
            if key in present and "ALT_M" in present:
                _pw, _c, xs, ys = self.profiles[key]
                xs.append(_finite(values.get(key)))
                ys.append(_finite(values.get("ALT_M")))
        if "AIR_RHO" in dpres and "ALT_M" in present:
            _pw, _c, xs, ys = self.profiles["AIR_RHO"]
            xs.append(_finite(derived.get("AIR_RHO")))
            ys.append(_finite(values.get("ALT_M")))

        if "LGT_FLAG" in present and _finite(values.get("LGT_FLAG")) > 0:
            self.lightning_rows += 1
            self.lightning_log.appendPlainText(
                f"T+{t:07.1f}   {_finite(values.get('LGT_DIST')):5.1f} km   "
                f"intensity {_finite(values.get('LGT_INT')):.0f}")

        if not self.ck_pause.isChecked():
            self._append_packet(values, derived, present, dpres, line)

    # ---- individual panel updaters --------------------------------------
    def _update_chips(self, now):
        if self.mode == "SIM":
            self.chip_mode.set("SIM STUDIO", T.cyan)
        else:
            self.chip_mode.set("LIVE FLIGHT", T.green)

        if self.sim and self.sim.is_alive():
            self.chip_link.set("SIMULATOR", T.amber)
        elif self.link and self.link.is_alive():
            if self.link_status == "CONNECTED":
                self.chip_link.set("CONNECTED", T.green)
            elif self.link_status == "RECONNECTING":
                self.chip_link.set("RECONNECTING", T.amber)
            else:
                self.chip_link.set(self.link_status, T.amber)
        else:
            if self.link_status == "PORT_LOST":
                self.chip_link.set("PORT LOST", T.red)
            else:
                self.chip_link.set("DISCONNECTED", T.fg3)

        st = int(_finite(self.d.get("STATE"), -1)) if "STATE" in self.present else -1
        self.chip_state.set(STATES[st] if 0 <= st < len(STATES) else "—")

        m = int(self.met)
        self.chip_met.set(f"{m // 3600:02d}:{(m % 3600) // 60:02d}:{m % 60:02d}")
        self.chip_pkt.set(f"{self.pkt_count}")

        if len(self._rate_win) >= 2:
            span = self._rate_win[-1] - self._rate_win[0]
            self.rate_hz = (len(self._rate_win) - 1) / span if span > 0 else 0.0
        if self.last_pkt_wall and (now - self.last_pkt_wall) > 5.0:
            self.rate_hz = 0.0
        self.chip_rate.set(f"{self.rate_hz:.1f} Hz",
                           T.amber if self.rate_hz < 0.5 and self.pkt_count else None)

        master = self.limits.master
        if master in IS_ALARM:
            self.chip_sys.set("ALARM", T.red)
        elif master in IS_WARN:
            self.chip_sys.set("CAUTION", T.amber)
        elif self.degraded:
            self.chip_sys.set(f"{len(self.degraded)} DEGRADED", T.amber)
        elif self.pkt_count:
            self.chip_sys.set("NOMINAL", T.green)
        else:
            self.chip_sys.set("—", T.fg3)

    def _update_tiles(self):
        for key, widgets in self.tiles.items():
            if key in self.present:
                val, seen = self.d.get(key), True
            elif key in self.dp:
                val, seen = self.dv.get(key), True
            else:
                p = self.limits.params.get(key)
                if p is not None and p.ever_seen:
                    val, seen = p.value, True
                else:
                    val, seen = None, False
            state = self.limits.state_of(key)
            for w in widgets:
                w.set(val, state, seen)

    def _update_limbars(self):
        for key, bars in self.limbars.items():
            p = self.limits.params.get(key)
            v = p.value if (p and p.ever_seen) else None
            st = self.limits.state_of(key)
            for b in bars:
                b.set(v, st)

    def _update_lamps(self):
        by_group = {}
        for k, m in FIELD_META.items():
            by_group.setdefault(m["g"], []).append(k)
        for grp, (dot, txt) in self.lamps.items():
            keys = by_group.get(grp, [])
            seen = [self.limits.params[k] for k in keys
                    if self.limits.params[k].ever_seen]
            if not seen:
                dot.setStyleSheet(f"color:{T.fg4}; background:transparent;")
                txt.setText("No Data")
                txt.setStyleSheet(f"color:{T.fg4}; background:transparent;")
                continue
            worst = max(seen, key=lambda p: SEVERITY.get(p.state, 0))
            ink = state_colour(worst.state)
            if worst.state == ST_NOMINAL:
                ink, label = T.green, "OK"
            else:
                label = worst.state
            dot.setStyleSheet(f"color:{ink}; background:transparent;")
            txt.setText(label)
            txt.setStyleSheet(f"color:{ink}; background:transparent;")

    def _update_instruments(self):
        att_ok = {"ROLL", "PITCH"} <= self.dp or self.limits.params["ROLL"].ever_seen
        roll = _finite(self.dv.get("ROLL"), 0.0)
        pitch = _finite(self.dv.get("PITCH"), 0.0)
        yaw = _finite(self.dv.get("YAW"), 0.0)
        spin = _finite(self.dv.get("SPIN_DPS"), 0.0)

        self.adi.set_attitude(roll, pitch, att_ok)
        hdg_ok = "HEADING" in self.dp or self.limits.params["HEADING"].ever_seen
        self.compass.set_heading(yaw, hdg_ok)
        
        if att_ok:
            # The original 3D geometry engine is defined as X=Right, Y=Forward.
            # In its matrix, 'arg 1' rotates about X (causes Nose Pitch).
            # 'arg 2' rotates about Y (causes Left/Right Roll).
            # Passing (pitch, roll) seamlessly aligns our math to the render geometry.
            self.cansat.set_attitude(pitch, roll, yaw, spin, True)

            # The quaternion is no longer transmitted at all — it left the wire
            # in layout 3. It was severed here anyway ("mathematically poisoned
            # by the left-handed physical board frame running through the flight
            # computer's filter"), so the 3-D model has always been rendering
            # the clean Euler angles mapped above and still is. This call keeps
            # the model's quaternion path explicitly disarmed rather than
            # leaving it holding a stale value from an older firmware.
            self.cansat.set_quaternion(None)

    def _update_stepper(self):
        if "STATE" in self.present:
            self.stepper.set_state(int(_finite(self.d.get("STATE"), 0)))

    def _update_plots(self):
        for lst in self.plots.values():
            for pl in lst:
                pl.redraw()

    def _update_profiles(self):
        for key, (pw, curve, xs, ys) in self.profiles.items():
            if xs:
                curve.setData(xs, ys)

    def _update_spectral(self):
        keys = ["S_405", "S_425", "S_450", "S_475", "S_515", "S_550",
                "S_555", "S_600", "S_640", "S_690", "S_745", "S_855"]
        seen = any(self.limits.params[k].ever_seen for k in keys)
        vals = [_finite(self.limits.params[k].value) for k in keys]
        self.spectral.set_vals(vals, seen)

    def _update_alarm_table(self):
        rows = self.limits.active
        self.tbl_alarm.setRowCount(len(rows))
        for i, p in enumerate(rows):
            ink = state_colour(p.state)
            age = p.age
            cells = [p.name,
                     fmt_value(p.value, p.name) if not isinstance(p.value, str) else p.value,
                     p.meta.get("u", ""),
                     p.state,
                     f"{age:.1f} s" if age is not None else "—"]
            for c, text in enumerate(cells):
                it = QTableWidgetItem(str(text))
                it.setForeground(QColor(ink if c in (1, 3) else T.fg2))
                self.tbl_alarm.setItem(i, c, it)
        if not rows:
            self.tbl_alarm.setRowCount(1)
            it = QTableWidgetItem("All parameters within limits"
                                  if self.pkt_count else "Awaiting telemetry")
            it.setForeground(QColor(T.fg3))
            for c in range(5):
                self.tbl_alarm.setItem(0, c, QTableWidgetItem(""))
            self.tbl_alarm.setItem(0, 0, it)
            self.tbl_alarm.setSpan(0, 0, 1, 5)
        else:
            self.tbl_alarm.clearSpans()

    def _update_sys_flags(self):
        for k, (widget, enum) in self.sys_flags.items():
            p = self.limits.params.get(k)
            if not p or not p.ever_seen:
                widget.setText("—")
                widget.setStyleSheet(f"color:{T.fg4}; background:transparent;")
                continue
            v = int(_finite(p.value))
            label = enum.get(v, str(v))
            bad = label in ("FAULT", "BACKUP")
            widget.setText(label)
            widget.setStyleSheet(
                f"color:{T.amber if bad else T.fg}; background:transparent;")
        p = self.limits.params.get("UTC")
        self.v_utc.setText(str(p.value) if p and p.ever_seen else "—")

    def _update_events(self):
        if not hasattr(self, "_ev_seen"):
            self._ev_seen = 0
        evs = list(self.limits.events)
        if len(evs) <= self._ev_seen:
            return
        for t, sev, name, old, new in evs[self._ev_seen:]:
            tag = ("FATAL   " if sev >= 3 else
                   "WARNING " if sev == 2 else
                   "ACTIVITY" if sev == 1 else "INFO    ")
            m = int(self.met)
            self.txt_events.appendPlainText(
                f"[{tag}] T+{m // 3600:02d}:{(m % 3600) // 60:02d}:{m % 60:02d}  "
                f"{name:<14s} {old} -> {new}")
        self._ev_seen = len(evs)

    def _update_recovery(self, now):
        self.track_flight.update()
        self.track_recovery.update()
        p = self.limits.params.get("LAT")
        if p and p.ever_seen and p.last_seen:
            self.v_fix_age.setText(f"{now - p.last_seen:.1f} s")
        else:
            self.v_fix_age.setText("—")
        dt = self.limits.params.get("TEMP_C")
        dh = self.limits.params.get("HDC_T")
        if dt and dh and dt.ever_seen and dh.ever_seen:
            self.v_tdelta.setText(f"{abs(_finite(dt.value) - _finite(dh.value)):.3f} °C")
        bg = self.limits.params.get("BARO_GPS_DALT")
        if bg and bg.ever_seen:
            self.v_baro_gps.setText(f"{_finite(bg.value):.2f} m")

    def _audio_alert(self, now):
        if not self.audio_on or self.limits.master not in IS_ALARM:
            return
        if now - self._audio_last < 4.0:
            return
        self._audio_last = now
        try:
            if HAVE_WINSOUND:
                winsound.Beep(880, 140)
            else:
                QApplication.beep()
        except Exception:
            pass

    def _update_status(self):
        bits = [f"CSV {self.logger.csv_path.name}  rows {self.logger.rows}",
                f"queue {self.rx_q.qsize()}",
                f"history {sum(len(v) for v in self.history.values())} samples"]
        if self.degraded:
            bits.append("DEGRADED: " + ", ".join(sorted(self.degraded)))
        self.statusbar.setText("  " + "     ·     ".join(bits))

    def _append_packet(self, d, dv, present, dpres, raw):
        m = int(self.met)
        head = (f"── PKT #{int(_finite(d.get('PKT'))):05d}   "
                f"MET {m // 3600:02d}:{(m % 3600) // 60:02d}:{m % 60:02d}   "
                f"STATE {STATES[min(int(_finite(d.get('STATE'))), len(STATES) - 1)]}"
                f"   RSSI {_finite(d.get('RSSI')):.0f} dBm"
                f"   SNR {_finite(d.get('SNR')):.1f} dB")
        self.txt_packets.appendPlainText(head)
        groups = ["core", "imu", "gnss", "power", "env", "spec", "rf", "mech", "sys"]
        for grp in groups:
            keys = [k for k in ALL_FIELDS
                    if SCHEMA[k]["g"] == grp and k in present]
            if not keys:
                continue
            line = []
            for k in keys:
                v = d.get(k)
                txt = v if isinstance(v, str) else fmt_value(v, k)
                line.append(f"{k}={txt}{SCHEMA[k]['u']}")
            self.txt_packets.appendPlainText(
                f"   {GROUP_LABEL.get(grp, grp):<12s} " + "  ".join(line))
        if self.ck_raw.isChecked():
            self.txt_packets.appendPlainText(f"   RAW  {raw}")

    # =====================================================================
    # §14  ACTIONS
    # =====================================================================
    def _toggle_link(self):
        if self.link and self.link.is_alive():
            self.link.stop()
            self.link = None
            self.link_status = "DISCONNECTED"
            self.b_connect.setText("Connect")
            self._log_status("Serial link closed")
            return
        port = self.cb_port.selected_port()
        if not port or port.startswith("(") or port == "":
            QMessageBox.warning(self, "No Gateway Port",
                                "Please connect your ESP32 LoRa Gateway or select the [VIRTUAL] Virtual Gateway Feed.")
            return
        try:
            baud = int(self.ed_baud.text())
        except ValueError:
            baud = DEFAULT_BAUD
            self.ed_baud.setText(str(baud))
        if self.sim and self.sim.is_alive():
            self._toggle_sim()
        self.link = SerialLink(port, baud, self.rx_q, self._link_status_update, self.tx_q)
        self.link.start()
        self.b_connect.setText("Disconnect")
        if port == "VIRTUAL_TEST":
            self._log_status("Connected to Virtual Gateway Feed (Loopback Verification)")
        else:
            self._log_status(f"Connecting to {port} @ {baud}")

    def _link_status_update(self, s):
        # Called from SerialLink worker thread — NEVER touch Qt widgets here.
        # _update_chips() reads link_status every 100 ms from the UI thread.
        self.link_status = s
        log.info("link status: %s", s)

    def set_app_mode(self, mode, port=None):
        self.mode = mode
        if mode == "SIM":
            self.chip_mode.set("SIM STUDIO", T.cyan)
            if hasattr(self, 'dummy_drawer'):
                self.dummy_drawer.setVisible(True)
            if hasattr(self, 'b_drawer_toggle'):
                self.b_drawer_toggle.setText("Hide Drawer")
            if not (self.sim and self.sim.is_alive()):
                self._toggle_sim()
            if self.sim and hasattr(self, 'dummy_drawer'):
                self.sim.set_manual(self.dummy_drawer.rb_manual.isChecked())
            self._log_status("Dummy Data Studio active — interactive controls enabled")
        else:
            self.mode = "LIVE"
            self.chip_mode.set("LIVE FLIGHT", T.green)
            if hasattr(self, 'dummy_drawer'):
                self.dummy_drawer.setVisible(False)
            if hasattr(self, 'b_drawer_toggle'):
                self.b_drawer_toggle.setText("Show Drawer")
            if self.sim and self.sim.is_alive():
                self._toggle_sim()
            if port and port not in ("", "(no ports detected — type COMx)", "(pyserial not installed)", "(No USB Gateway Detected — Plug in ESP32 / Select Port)"):
                idx = self.cb_port.findData(port)
                if idx >= 0:
                    self.cb_port.setCurrentIndex(idx)
                else:
                    self.cb_port.setEditText(port)
                if not (self.link and self.link.is_alive()):
                    self._toggle_link()
            else:
                self._log_status("Live Mission Dashboard active — no port chosen, auto-detecting")
                self._autodetect_port()

    def _autodetect_port(self):
        if self._autodetect and self._autodetect.is_alive():
            return
        if self.link and self.link.is_alive():
            self._toggle_link()          # a port we hold open cannot be probed
        try:
            baud = int(self.ed_baud.text())
        except ValueError:
            baud = DEFAULT_BAUD
            self.ed_baud.setText(str(baud))
        self._autodetect = PortAutoDetect(baud)
        self._autodetect.start()
        self.b_autodetect.setEnabled(False)
        self.b_autodetect.setText("Scanning…")
        self._log_status(f"Auto-detect: listening {AUTODETECT_LISTEN_S:.0f} s on each COM port @ {baud}")
        QTimer.singleShot(200, self._autodetect_poll)

    def _autodetect_poll(self):
        det = self._autodetect
        if det is None:
            return
        if det.is_alive():
            QTimer.singleShot(200, self._autodetect_poll)
            return
        self._autodetect = None
        self.b_autodetect.setEnabled(True)
        self.b_autodetect.setText("Auto-detect")
        if not det.result:
            tried = ", ".join(p for p, _ in det.tried) or "no ports present"
            self._log_status(f"Auto-detect: nothing found ({tried})")
            QMessageBox.information(
                self, "Auto-detect",
                "No COM port is carrying the VRITRA gateway or telemetry.\n\n"
                f"Ports tried: {tried}\n\n"
                "Check the gateway USB cable, close any Serial Monitor that holds "
                "the port, and confirm the baud rate.")
            return
        port, kind = det.result
        self.cb_port.refresh()
        if self.cb_port.findData(port) < 0:
            # Listed by the OS during the probe but not yet in the combo (it
            # is not editable, so set_app_mode could not select it otherwise).
            self.cb_port.addItem(f"{port}  [auto-detected]", port)
        what ="VRITRA gateway" if kind == "GATEWAY" else "VRITRA telemetry"
        self._log_status(f"Auto-detect: {what} found on {port}")
        self.set_app_mode("LIVE", port)

    def _toggle_drawer(self):
        if not hasattr(self, 'dummy_drawer'):
            return
        vis = not self.dummy_drawer.isVisible()
        self.dummy_drawer.setVisible(vis)
        self.b_drawer_toggle.setText("Hide Drawer" if vis else "Show Drawer")

    def _open_launcher(self):
        dlg = ModeSelectDialog(self)
        if dlg.exec_() == QDialog.Accepted:
            self.set_app_mode(dlg.selected_mode, dlg.selected_port)

    def _toggle_sim(self):
        if self.sim and self.sim.is_alive():
            self.sim.stop()
            self.sim = None
            self.b_sim.setText("Simulator")
            self._log_status("Simulator stopped")
            return
        if self.link and self.link.is_alive():
            self._toggle_link()
        self.phys.reset()
        self.sim = Simulator(self.rx_q)
        if hasattr(self, 'dummy_drawer') and self.dummy_drawer:
            self.sim.set_manual(self.dummy_drawer.rb_manual.isChecked())
        self.sim.start()
        self.b_sim.setText("Stop Sim")
        self._log_status("Simulator started")

    def _toggle_audio(self):
        self.audio_on = not self.audio_on
        self.b_audio.setText("Audio On" if self.audio_on else "Audio Muted")

    def _ack_alarm(self):
        self._audio_last = time.time() + 25.0
        self._log_status("Master alarm acknowledged (audio silenced 25 s)")

    def _send_cmd(self):
        self._send_named(self.cb_cmd.currentData())

    def _send_named(self, cmd):
        if not cmd:
            return
        live = bool(self.link and self.link.is_alive())
        if live:
            self.tx_q.put(cmd)
            result = "QUEUED"
        else:
            result = "NO LINK — NOT SENT"
        m = int(self.met)
        self.cmd_history.append(cmd)
        r = self.tbl_cmd.rowCount()
        self.tbl_cmd.insertRow(r)
        cells = [datetime.datetime.now().strftime("%H:%M:%S"),
                 f"{m // 3600:02d}:{(m % 3600) // 60:02d}:{m % 60:02d}",
                 cmd, result]
        for c, text in enumerate(cells):
            it = QTableWidgetItem(text)
            it.setForeground(QColor(T.green if (c == 3 and live)
                                    else T.amber if c == 3 else T.fg2))
            self.tbl_cmd.setItem(r, c, it)
        self.tbl_cmd.scrollToBottom()
        self.txt_events.appendPlainText(
            f"[COMMAND ] T+{m // 3600:02d}:{(m % 3600) // 60:02d}:{m % 60:02d}"
            f"  {cmd}  {result}")
        self._log_status(f"Command {cmd}: {result}")

    def _log_status(self, msg):
        log.info(msg)
        self.statusbar.setText("  " + msg)

    def _open_csv(self):
        p = self.logger.csv_path
        self.logger.close()
        self.logger._open()
        try:
            os.startfile(str(p))
        except Exception:
            QMessageBox.information(self, "CSV", f"Log file:\n{p}")

    def _export_xml(self):
        p = self.logger.export_xml()
        QMessageBox.information(self, "XML exported", str(p))

    def _export_kml(self):
        if not self.logger.track:
            QMessageBox.warning(self, "No track",
                                "No GNSS fixes have been received yet.")
            return
        p = self.logger.export_kml()
        QMessageBox.information(
            self, "KML exported",
            f"{p}\n\nOpen in Google Earth to see the track on real imagery.")

    def _export_dict(self):
        p = export_dictionary(LOG_DIR / f"Dictionary_{TEAM_ID}.json")
        QMessageBox.information(
            self, "Telemetry dictionary",
            f"{p}\n\n{N_FIELDS} downlink fields + {len(DERIVED)} derived.")

    def _export_stats(self):
        path = LOG_DIR / f"Statistics_{TEAM_ID}.csv"
        try:
            with open(path, "w", newline="", encoding="utf-8") as fh:
                w = csv.writer(fh)
                w.writerow(["PARAMETER", "UNIT", "N", "MIN", "MAX", "MEAN", "LATEST"])
                for k, hist in sorted(self.history.items()):
                    if not hist:
                        continue
                    vals = [v for _t, v in hist]
                    w.writerow([k, FIELD_META.get(k, {}).get("u", ""), len(vals),
                                f"{min(vals):.4f}", f"{max(vals):.4f}",
                                f"{sum(vals) / len(vals):.4f}", f"{vals[-1]:.4f}"])
            QMessageBox.information(self, "Statistics exported", str(path))
        except Exception as e:
            QMessageBox.warning(self, "Export failed", str(e))

    def _open_map(self):
        p = self.limits.params
        if not (p["LAT"].ever_seen and p["LON"].ever_seen):
            QMessageBox.warning(self, "No fix", "No GNSS fix received yet.")
            return
        la, lo = _finite(p["LAT"].value), _finite(p["LON"].value)
        import webbrowser
        webbrowser.open(f"https://www.google.com/maps?q={la:.7f},{lo:.7f}")

    def _copy_coords(self):
        p = self.limits.params
        if not (p["LAT"].ever_seen and p["LON"].ever_seen):
            QMessageBox.warning(self, "No fix", "No GNSS fix received yet.")
            return
        txt = f"{_finite(p['LAT'].value):.7f}, {_finite(p['LON'].value):.7f}"
        QApplication.clipboard().setText(txt)
        self._log_status(f"Copied {txt}")

    def _update_stats_table(self):
        rows = [(k, h) for k, h in sorted(self.history.items()) if h]
        self.tbl_stats.setSortingEnabled(False)
        self.tbl_stats.setRowCount(len(rows))
        for i, (k, hist) in enumerate(rows):
            vals = [v for _t, v in hist]
            cells = [k, FIELD_META.get(k, {}).get("u", ""), str(len(vals)),
                     fmt_value(min(vals), k), fmt_value(max(vals), k),
                     fmt_value(sum(vals) / len(vals), k), fmt_value(vals[-1], k)]
            for c, text in enumerate(cells):
                it = QTableWidgetItem(text)
                it.setForeground(QColor(T.fg if c in (0, 6) else T.fg2))
                self.tbl_stats.setItem(i, c, it)
        self.tbl_stats.setSortingEnabled(True)

    def closeEvent(self, ev):
        try:
            if self.link:
                self.link.stop()
            if self.sim:
                self.sim.stop()
            if self.gs_gnss:
                self.gs_gnss.stop()
            self.logger.close()
        except Exception:
            pass
        super().closeEvent(ev)


# =========================================================================
# §15  ENTRY POINT
# =========================================================================
def excepthook(t, v, tb):
    msg = "".join(traceback.format_exception(t, v, tb))
    log.error("UNCAUGHT:\n%s", msg)
    try:
        QMessageBox.critical(None, "VRITRA GCS",
                             f"Unexpected error (logged to {LOG_DIR}):\n\n{v}")
    except Exception:
        pass


def main():
    # Software rasteriser: guarantees the GCS starts on machines with no
    # usable GPU driver, which is the normal case for a field laptop or VM.
    os.environ.setdefault("QT_OPENGL", "software")
    os.environ.setdefault("QT_LOGGING_RULES", "qt.qpa.fonts.warning=false")
    QApplication.setAttribute(Qt.AA_EnableHighDpiScaling, True)
    QApplication.setAttribute(Qt.AA_UseHighDpiPixmaps, True)
    sys.excepthook = excepthook

    app = QApplication(sys.argv)
    app.setApplicationName("VRITRA GCS")
    app.setOrganizationName(f"Team {TEAM_NAME}")
    app.setStyleSheet(APP_QSS)

    pal = app.palette()
    pal.setColor(QPalette.Window, QColor(T.black))
    pal.setColor(QPalette.Base, QColor(T.surf))
    pal.setColor(QPalette.Text, QColor(T.fg))
    pal.setColor(QPalette.WindowText, QColor(T.fg))
    app.setPalette(pal)

    # Parse CLI flags for automated testing / scripted launch
    cli_mode = None
    cli_port = None
    if "--sim" in sys.argv:
        cli_mode = "SIM"
    elif "--live" in sys.argv:
        cli_mode = "LIVE"
    for arg in sys.argv:
        if arg.startswith("--port="):
            cli_port = arg.split("=", 1)[1]

    # Show launcher dialog if mode not provided via CLI flag
    if not cli_mode:
        dlg = ModeSelectDialog()
        if dlg.exec_() == QDialog.Accepted:
            cli_mode = dlg.selected_mode
            cli_port = dlg.selected_port
        else:
            sys.exit(0)

    win = MainWindow(initial_mode=cli_mode, initial_port=cli_port)
    win.showMaximized()
    if "--smoke-test" in sys.argv:
        QTimer.singleShot(1200, lambda: (win.close(), app.quit()))
    log.info("VRITRA GCS v%s ready — %d downlink fields — Mode: %s", APP_VERSION, N_FIELDS, cli_mode)
    sys.exit(app.exec_())


if __name__ == "__main__":
    main()