# Ground-station team handoff

Read report §10.3–10.4 alongside the supplied `gcs/vritra_gcs.py` and `firmware/vritra_gs/vritra_gs.ino`. Their existing legacy parser/schema and command tables were inspected in this audit. The reference `gcs_protocol.py` is an offline-tested new protocol helper, not a replacement GUI or gateway image.

| Existing behavior | Required behavior | Acceptance |
|---|---|---|
| Base36/83-field gateway expansion, optional values retained | Transparent decimal core CSV with original sample validity; separate typed diagnostics | RF capture and PC file agree; no old numeric data becomes current |
| TX_ON also arms old FSM | TX_ON controls radio only; explicit readiness/mission arm path | Telemetry request cannot release or steer mechanisms |
| int16 command argument, repeated bursts | Signed int32e7 target coordinates; session/transaction deduplication; stage/commit | Partial/wrong-session/out-of-area updates rejected; P4 ACK/revision visible |
| Main map position | Current valid marker plus age, track, target and separately styled prediction | Unplug serial: marker expires; no substitute/default coordinates |
| Generic status fields | Guidance mode/owner/lease, board ages, GNSS quality, command and applied brake | Missing board is shown as stale/fault, not connected |
| No validated wind fit display | N/E wind, uncertainty/residual/coverage/age or UNKNOWN | Never display zero wind because no fit exists |
| Simulation/replay in main application | Separate test entrypoint/output directory; flight transport physical serial only | Automated dependency/entrypoint check plus unplug/reconnect test |
| Existing required plots/CSV | Preserve SI telemetry plots and `Flight_2026-INSPACe-CAN-7USAT-025.csv` | Header, CR receive parsing, field order and missing values checked |

GUI target workflow: edit authorized coordinates → validate geographic/approved-area bounds → show pending transaction → transmit19/20/21 in available windows → await P4 applied target/revision → show ACTIVE. Timeout remains PENDING/FAILED, never optimistically active. Freeze changes below the qualified final-height gate. Keep UI responsive while waiting; no retry burst that monopolizes receive airtime.

Predicted landing marker must carry model age/validity and label “prediction”. Initial constant-velocity projection is a diagnostic, not an achieved precision result. Do not append large wind/track/ACK payloads to every core packet: the SF9 budget is already nearly full. Select lower-rate diagnostics only through a schedule proved to preserve core telemetry and command access, or transmit extra details over the local USB log when available.

Follow the original CSV order. Confirm accelerometer expansion/invalid blanks with the judge before final parser freeze. Keep a raw RF/USB receive log separate from the judges' CSV so corrupt/partial frames and reconnects can be audited. Host helper tests cover parser rejection, marker expiry, blank preservation and target byte bounds; real serial, GUI, RF and platform integration remain pending.
