# VRITRA — compliance audit and RF design record

Team 2026-INSPACe-CAN-7USAT-025 · Lovely Professional University · 2026-08-03

Cross-check of the firmware, the PDR, the Fusion model and the ground station
against **IN-SPACe.CAN.MR.CG.01** (mission requirements) and the **CAN-7USAT
frequency allocation plan**. Sources are quoted rather than paraphrased where
the exact wording decides a design question.

---

## 1. The RF plan is assigned, not chosen

Row **025** of the frequency allocation plan:

| Parameter | Assigned | Was in firmware | Was in PDR |
|---|---|---|---|
| 868 Main | **867.00 MHz** | 867.00 ✅ | 865 MHz ❌ |
| 868 Backup | **865.75 MHz** | 865.75 ✅ | — |
| Spreading factor | **SF10** | SF10 ✅ | SF9 ❌ |
| Sync word | **0x98** | 0x98 ✅ | "PANID 0025" |
| Bandwidth | **125 kHz** (mandatory) | 125 ✅ | 125 ✅ |
| Coding rate | **4/5** | 4/5 ✅ | 4/5 ✅ |
| TX power | **+10 dBm pad / ~+14 dBm flight** | **+22 dBm** ❌ | +22 dBm ❌ |
| XBee | Ch 23 (2465) / Ch 16 (2430), PAN 0x1018 | not fitted | not fitted |

Two corrections landed from this:

- **TX power was 8–12 dB over the assignment.** Now staged automatically by the
  flight computer: +10 dBm while `FS_BOOT`, +14 dBm from `FS_LAUNCH` onward, so
  it cannot be left wrong on the pad.
- **PDR §5.4 (865 MHz) and §5.4.3 (SF9) contradict the allocation** and need a
  CDR correction. The firmware follows the allocation.

Also newly implemented, from the same document:

- *"All CAN-7USATs should automatically switch off transmission 90 min after
  launch detection."* → `TX_AUTO_OFF_MS`. The clock starts at the
  `FS_BOOT → FS_LAUNCH` transition and is persisted, so a mid-flight reset
  cannot restart it. The **receiver stays armed** after cut-off so Mission
  Control can still reach the vehicle; an explicit `CMD_TX_ON` overrides it.
- *"must be able to switch EACH link ON and OFF by telecommand"* →
  `CMD_TX_ON` / `CMD_TX_OFF`, already present.

---

## 2. The encoding question, settled

Guideline **§6.3.i** is not ambiguous:

> "The telemetry data **shall be transmitted with ASCII comma-separated fields
> followed by a carriage return** in the following format:
> `<TEAM ID>,<TIME STAMPING>,<PACKET COUNT>,<ALTITUDE>,<PRESSURE>,<TEMP>,
> <VOLTAGE>,<GNSS TIME>,<GNSS LATITUDE>,<GNSS LONGITUDE>,<GNSS ALTITUDE>,
> <GNSS SATS>,<ACCELEROMETER DATA>,<GYRO SPIN RATE>,<FLIGHT SOFTWARE STATE>,
> <ANY OPTIONAL DATA>`"

That governs what goes over the air, not only what the ground station saves.
So:

- **Packed binary — not compliant.** Removed.
- **Base-36 ASCII CSV — adopted.** It satisfies all four literal requirements:
  ASCII text, comma-separated, carriage-return terminated, mandated field
  order. Each numeric field is a base-36 integer of `value × scale`.

**The interpretation risk, stated openly.** §6.3.iii gives a per-field
resolution table (0.1 m, 1 Pa, 0.01 V, 0.0001°). Its own column heading reads
*"The telemetry parameters **display format** with resolution"* — read as
describing the display, the gateway and GCS satisfy it exactly, rendering those
precise resolutions in the graded CSV. Read as describing the on-air bytes,
base-36 does not. This is a deliberate team decision taken with the trade in
view, not an oversight.

**The fallback is cheap.** The codec is isolated in `Base36.h`; reverting to
plain decimal touches two functions and costs ~200 ms of airtime per packet
(and pushes worst-case packets past 1 s). Worth one email to the organisers:
*"may telemetry field values be transmitted base-36 encoded, given the ground
station renders the mandated decimal resolutions in the graded CSV?"*

### Correction to my earlier analysis

I previously argued binary was necessary because ASCII "cost 1190 ms at SF10".
That number came from an arithmetic error — I used `4×8` where the LoRa payload
formula requires `4×SF`, i.e. `4×10 = 40`. The corrected form is:

```
n_payload = 8 + ceil( (8·PL − 4·SF + 28 + 16·CRC − 20·IH) / (4·(SF − 2·DE)) ) · (CR+4)
          = 8 + ceil( (8·PL + 4) / 40 ) · 5           [SF10, CRC on, explicit hdr]
Ts        = 2^10 / 125000 = 8.192 ms
preamble  = (8 + 4.25) × 8.192 = 100.4 ms
```

| Payload | Symbols | Airtime |
|---|---|---|
| 92 B | 103 | **944 ms** |
| **99 B** | **108** | **985 ms** ← ceiling for 1 Hz |
| 113 B | 123 | 1108 ms (0.90 Hz) |
| 12 B uplink | 23 | 288 ms |

The conclusion (ASCII) is unchanged, because §6.3.i mandates it regardless of
airtime — but the reasoning I gave for it was wrong and is corrected here.

### The 99-byte wall

At the assigned SF10, a 1 Hz downlink cannot exceed **99 bytes**. That single
number shapes the entire wire format:

```
<TEAM>,<TIME_S>,<PKT>,<ALT_M>,<PRES_PA>,<TEMP_C>,<BUS_V>,<GNSS_TIME>,
<LAT>,<LON>,<GPS_ALT>,<SATS>,<ACC_X>,<ACC_Y>,<ACC_Z>,<SPIN>,<STATE>,
<FLAGS>,F<n>,<value>\r
```

A real packet, decoded field by field:

```
025,F3,F3,41E,21HA,52,WX,20XC,FX4T7,1DY2AG,46C,9,3C,6E,R9,CK,2,1R,F7,-CK
     ^543  ^543 ^523.4  ^95230 Pa   ^26.740123     ^9 sats      ^state 2
```

`FX4T7` = 15·36⁴ + 33·36³ + 4·36² + 29·36 + 7 = 26 740 123 → 26.740123°.

Measured against plain decimal, over the full physical range of all 36 fields:

| | Decimal | Base-36 |
|---|---|---|
| Typical packet | 89 B → 903 ms | **73 B → 780 ms** |
| Worst case | 112 B → 1108 ms ❌ | **90 B → 944 ms** ✅ |

The worst case is what actually mattered: with decimal, packets exceeded 1 s
exactly when accelerometer and spin fields hit full deflection — i.e. during
ejection shock. Base-36 keeps every packet under a second unconditionally, and
lets the uplink window open every 3rd cycle instead of every 5th.

Fields 0–15 are the mandated set in the mandated order at or above the §6.3.iii
resolutions. `<FLAGS>` onward is `<ANY OPTIONAL DATA>`, which §6.3.iii.B
explicitly permits. One rotating optional value is appended, and only when the
core leaves room — graded data never competes with non-graded data for airtime.
The rotation covers 19 fields, so the GCS panels fill in over 19 s.

### Codec verification

`Base36.h` is byte-identical in `vritra_fc/` and `vritra_gs/`, and both sketches
print `b36SchemaSum()` at boot — **currently `0x007C`. If the two printed values
ever differ, the copies have drifted and every decoded field is silently wrong.
Check that before trusting any telemetry.**

The codec was exercised across every field's full physical range (~120 000
points) plus edge cases. All pass:

- round-trip is **exact**, guaranteed by `dp == log10(div)` for all 36 entries
- scaling uses `double` + `llround`, not `float` — a 32-bit float stops
  representing consecutive integers above 16 777 216, so `lroundf(lon * 1e6f)`
  would have quantised longitude into ~10 m steps near 83°E: a silent,
  plausible-looking position error
- `0` encodes as `"0"`, never empty; negatives carry a leading `-`
- malformed tokens (`""`, `"-"`, `"1.5"`, `"12$"`, `"+5"`, overflow) are
  **rejected whole** and yield an empty CSV cell rather than a fabricated number
- INT32_MIN negation and overflow are handled via int64 intermediates
- GNSS fields are sent **empty until first fix** — encoding no-fix as 0,0 would
  plot the CanSat in the Gulf of Guinea on the map and in the graded CSV

Everything else is on the SD card at 20 Hz, which §6.3.iii.C blesses directly:
*"Only the transmitted telemetry is graded, however, the backup data can be
used when completing the Post Flight Review."*

### One judgement call, stated plainly

The full identifier `2026-INSPACe-CAN-7USAT-025` is **26 of the 99 bytes** — a
quarter of the entire 1 Hz budget spent re-sending a constant. The link carries
`025` and the gateway expands it before the CSV the judges examine (§6.3.ii), so
the graded artefact is byte-identical. **If the jury wants the full string over
the air, set `TEAM_ID_AIR` to `TEAM_ID_FULL` in `Config.h`** — the packet then
costs 1108 ms and the downlink drops to 0.90 Hz. That is the whole trade, and
it is a one-line change.

---

## 3. Range — in flight it is horizon-limited; on the ground it is mast-limited

### 3.1 The antenna

RadioMaster dual-band 2.4 GHz / Sub-G 900 MHz whip
([anubisrc](https://anubisrc.com/product/dual-band-2-4ghz-sub-g-900mhz-antenna/) ·
[RadioMaster](https://radiomasterrc.com/products/dual-band-2-4ghz-sub-g-900mhz-antenna)):

| | Value |
|---|---|
| Gain | **3.58 dBi @ 900 MHz**, 3.83 dBi @ 2.4 GHz |
| VSWR | 1.25 @ 900 MHz, 1.27 @ 2.4 GHz |
| Impedance | 50 Ω |
| Connector | RP-SMA (an IPEX1/u.FL version also exists) |
| Size / mass | 100 mm body, 80 mm coax, **15.78 g** |
| Pattern | omnidirectional, linear |

**+1.4 dB over the 2.2 dBi whip the PDR assumed.** Good, but three caveats:

- **The sub-GHz range is not published for this model.** RadioMaster's
  comparable single-band [Bandit T antenna](https://radiomasterrc.com/products/bandit-t-antenna)
  specifies **860–930 MHz**, which does cover 867. The ELRS "900 MHz" family is
  sold overwhelmingly into the 915 MHz US market, so the quoted VSWR 1.25 is
  almost certainly measured at 915, not 867. Even if it degrades to 2:1 at 867
  the mismatch loss is only 0.5 dB, so this is not a link-killer — but the
  3.58 dBi gain claim is unverified at our frequency. **Sweep it with a NanoVNA
  at 867 MHz before flight.** That is a ₹3 000 instrument and exactly what it
  is for.
- **It is single-feed, so it reaches one port.** The Core1121-XF brings out two
  separate u.FL ports on two separately matched RF chains — ANT1 = 2.4 GHz,
  ANT2 = sub-GHz. Connect **ANT2**. The 2.4 GHz half is then dead weight for a
  band the allocation plan assigns to XBee, not to your LoRa link.
- **15.78 g against an 80 g declared mass margin.** A single-band 868 MHz whip
  would be lighter, better matched at 867, and lose you nothing. Worth
  reconsidering — but the link closes either way, so this is your call.

### 3.2 Link budget

```
TX power (assigned, in flight)          +14.0 dBm
CanSat antenna                           +3.58 dBi
CanSat feed (u.FL→RP-SMA pigtail)        −0.6 dB
Ground Yagi (PDR §5.4.4)                +11.0 dBi
Ground feed (3 m RG58 @ 867 MHz)         −2.1 dB
                                        ──────────
System gain                             +25.88 dBm
LR1121 sensitivity, SF10 / BW125        −132 dBm
                                        ──────────
Total path-loss budget                   157.9 dB
Impairments: polarisation 3 · blockage 3 · fade 6   −12 dB
                                        ──────────
Usable path loss                         145.9 dB
```

### 3.3 In flight — not a constraint

At 1000 m deployment, a 2 km ground range is a 2.24 km slant path, essentially
free space:

```
FSPL(2.24 km) = 98.2 dB   →   RX = −72.3 dBm   →   47.7 dB margin after impairments
```

Solving for the budget limit gives **~540 km**, which is meaningless because the
radio horizon at 1000 m altitude is `4.12(√1000 + √3) ≈ 137 km`. **In flight the
link is horizon-limited, not budget-limited.** Even a worst-case 20 dB
polarisation null still leaves ~31 dB at 2 km. The 2 km requirement is met with
enormous room.

### 3.4 After landing — this is the real limit

Once the CanSat is on the ground, the two-ray ground-reflection regime takes
over and path loss grows at **40 dB/decade**, not 20. Breakpoint
`d_b = 4·h_t·h_r/λ`, λ = 0.3458 m:

| CanSat antenna height | GS mast | Breakpoint | **Max range** |
|---|---|---|---|
| 0.1 m (lying flat) | 3 m | 3.5 m | **1.37 km** ❌ |
| 0.3 m (upright) | 3 m | 10.4 m | **2.38 km** ✅ |
| 0.1 m (lying flat) | **6 m** | 6.9 m | **1.95 km** ~ |
| 0.3 m (upright) | **6 m** | 20.8 m | **3.35 km** ✅ |

Three things fall out of this, and they are the actionable findings:

1. **Post-landing range is set by ground-station mast height, not transmit
   power.** Going 3 m → 6 m buys ~40 % more range. Going +14 → +22 dBm would buy
   ~50 %, and you are not allowed to. Guideline §6.1.iii already tells you to
   elevate the antenna; this is the number behind it.
2. **How the CanSat lands matters as much as anything in the firmware.** Lying
   flat costs you nearly half your recovery range. An antenna that stays
   upright, or protrudes axially, is worth more than any RF tuning.
3. **Vegetation is not in this table.** At 867 MHz a vehicle lying in standing
   crop can lose another 10–20 dB. This is exactly why §5.2.v mandates the
   audio beacon — and why the beacon still isn't wired (§5.2 below).

### 3.5 Polarisation — the cheapest remaining win

A linear whip on a tumbling vehicle against a linear Yagi is the single largest
uncontrolled loss in the budget: 3 dB on average, unbounded in a cross-pol null.
**A circularly-polarised ground antenna** (crossed Yagi or helix, RHCP) caps it
at a flat 3 dB and removes the deep nulls entirely. Standard practice in
rocketry telemetry, and it costs nothing in the CanSat.

> **PDR correction needed:** §5.4.4 computes at 1 km, +22 dBm and 2.2 dBi, and
> reports 73.5 dB. Redo it at 2 km, +14 dBm, 3.58 dBi for CDR. The answer is
> still comfortable — but the submitted numbers no longer match either the
> assigned power or the fitted antenna.

### 3.6 Structure: aluminium plates are the risk, glass fibre is fine

At 867 MHz, **λ = 345.8 mm and λ/4 = 86.5 mm.**

- **Glass-fibre rods — good choice.** GFRP is effectively RF-transparent
  (ε_r ≈ 4.4, low loss at 900 MHz). Mounting the antenna against a GFRP rod
  shifts resonance by well under 1 %.
- **Aluminium plates — genuine problem.** Aluminium closer than λ/4 to the
  radiating element detunes it, becomes an uncontrolled ground plane, and nulls
  the pattern in the plane of the plate. Your vehicle is 150 mm across, so
  **86.5 mm of clearance cannot be achieved inside the stack.**

  The element must sit **outside the aluminium, along the long axis** — either
  protruding from an end cap or carried on a GFRP standoff clear of the plates.
  If aluminium ends up near it anyway, make it a *deliberate* ground plane
  (a plate perpendicular to the element, ≥ λ/4 radius = 87 mm) rather than a
  random scatterer. A 150 mm plate gives 75 mm of radius, which is close enough
  to work as one.

  This also interacts with §5.2.i's "all electronics enclosed and shielded"
  requirement — an aluminium enclosure is a Faraday cage, so the antenna cannot
  live inside it under any circumstances.

---

## 4. Other guideline items now handled in firmware

| Clause | Requirement | Status |
|---|---|---|
| §6.1.v | *"shall not transmit telemetry until commanded"* | **Fixed.** `TX_ENABLED_AT_BOOT = false`; boots into continuous receive. The old firmware booted transmitting. |
| §6.1.ix | *"mission clock/time stamp & system state shall be maintained"* after a reset | **Fixed.** Mission seconds checkpointed to EEPROM every 5 s and resumed; `TIME_S` no longer restarts from `millis()`. |
| §6.1.vi | GS can command gyro / baro / accel zeroing on the pad | `CMD_CALIBRATE`, stillness-gated so motion cannot be baked into the bias |
| §6.2.v | telemetry on/off by RF command | `CMD_TX_ON` / `CMD_TX_OFF`; TX_OFF leaves the receiver hot permanently |
| §5.4.vi | secondary descent control at **600 m ±10 m** | **Fixed.** `FSM_SEC_EJECT_AGL_M = 600` (was a guessed 400) |
| §5.1.xi | record to SD against telemetry loss | 20 Hz binary + 1 Hz CSV, 512 B block aligned, 2 s sync |
| §6.3.iii.A | `Flight_<TEAM_ID>.csv` | produced by `vritra_gcs.py`, unchanged |

---

## 5. Conflicts that need *your* decision

### 5.1 Battery: PDR and CAD say 3S, the bench says 2S

Fusion contains **three 21700 cells**; PDR §5.5.1 declares a 3S 40 A BMS
(11.1 V nominal, 12.6 V full). Guideline §5.3.iii allows metal-can Li-ion and
forbids LiPo — 21700 complies.

But your ADC log reads a steady **7.87 V**. On a 3S pack that is 2.62 V/cell,
below any BMS cut-off. **Whatever was on the bench was not the flight pack.**
Firmware and GCS are both set to 3S; confirm before flight. (I had briefly
changed the GCS to 2S on the strength of the bench reading alone — that was
wrong and is reverted.)

The divider maths itself is confirmed correct:
`1762/4095 × 3.3 × (122/22) = 7.873 V`.

### 5.2 Audio beacon — required, not wired

§5.2.v mandates **≥ 92 dB SPL**; PDR promises an SFM20 at ≥ 95 dB. No buzzer
pin exists in the hardware brief. `PIN_BEACON` is stubbed in `Config.h` and the
beacon flag is already plumbed through the FSM — uncomment it against a real
pin and it works. **Until then the recovery indication is an LED, which does
not satisfy §5.2.v.**

### 5.3 Fusion model: mass cannot be verified yet

Envelope is fine: bounding box **144.7 × 132.5 × 225.7 mm** against a limit of
150 mm diameter × 400 mm height. ✅

Mass reports **2949 g** against a 1000 g (±50 g) limit — but that figure is not
trustworthy, because most bodies still carry Fusion's default *Steel* material.
A 21700 cell is modelled at ~165 g against a real ~70 g; each "Servo Motor 35KG"
at ~265 g. **Assign real materials before the mass budget means anything.**
Your PDR claims ~920 g; that is the number to verify, and it cannot be verified
from the current model.

Two things worth a look while you are in there:

- **§5.2.i**: *"All electronics shall be enclosed and shielded from the
  environment. No electronics can be exposed except for sensors. There must be
  a structural enclosure."* The hex-lattice shell is open enough to see the
  electronics through. This is a real compliance risk, not a styling note.
- Colour is fluorescent magenta ✅ (§5.2 wants pink / red / orange).
- Two **35 kg·cm** servos on a ~1 kg vehicle is a lot of servo. Worth a
  torque-budget sanity check against the mass budget.

### 5.4 Sensors in CAD but not in firmware

INA226, SCD41, HDC3020, BMV080 and a XIAO ESP32-S3-Sense camera are all in the
model, and the GCS has schema slots for every one of them. Per your call they
are **left out of this firmware** — none of them appeared in your I²C scan, so
there is nothing to talk to yet. Their GCS fields stay empty (rendered "—"),
which is honest. Give me addresses once they're on the bus.

### 5.5 Team ID spelling

The guideline table gives `2026-IN-SPACe-CAN-7USAT-XXX` (hyphen inside
IN-SPACe). Every artefact you have — PDR cover, `vritra_gcs.py`, the CSV
filename — uses `2026-INSPACe-CAN-7USAT-025`. Kept consistent with your own
documents; worth one email to the organisers.

---

## 6. Files changed

| File | Change |
|---|---|
| `vritra_fc/Config.h` | assigned RF plan, staged TX power, 90-min cut-off, 600 m secondary, silent boot, 99-byte wall |
| `vritra_fc/Radio.*` | ASCII packets, runtime TX power, decimated uplink window |
| `vritra_fc/vritra_fc.ino` | ASCII builder, mission-clock persistence, power staging, auto cut-off |
| `vritra_gs/vritra_gs.ino` | ASCII parse, short-ID expansion, assigned RF plan |
| `vritra_gcs.py` | SF10 sensitivity (−132), assigned frequency constants, 3S limits restored, 32 GB card |

Still open from the sensor-test phase: AK09916 hard-iron calibration never run
to completion; DS3231 SQW pin dead on pin 2; NavIC L5 antenna gain unconfirmed;
`GYRO_WHEEL_PCT = 70` has no angular-momentum calculation behind it; GNSS under
real flight dynamics untested.
