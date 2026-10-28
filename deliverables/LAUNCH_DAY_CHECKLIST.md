# Launch-day checklist

Generated from the [canonical engineering report](VRITRA_ENGINEERING_REPORT.md). All hardware results remain pending.

## 14. Launch-day checklist

- [ ] Record actual canopy/mass/rigging and secondary mechanism; confirm **drogue immediately, ram-air600±10 m**.
- [ ] Inspect fasteners,battery retention,connectors,antenna clearances and correct physical board revisions.
- [ ] Measure battery and steering/logic rails under a brief representative load;steering never exceeds8.4 V.
- [ ] Verify safe pulse positions with unloaded mechanisms,then pack/secure them;no automatic sweep on boot.
- [ ] Boot the exact frozen MAIN/BACKUP/P4/camera/GS/GCS versions;record build IDs and reset reasons.
- [ ] Confirm RF is silent before command and only authorised slot/channel/power will be used;camera Wi-Fi/BT remains off.
- [ ] Confirm L89 real coherent fix,actual update rate,quality/age and all three listener counters;no overflows.
- [ ] Capture pad p0/ambientT and stationary calibration;compare three barometers;lock the datum against in-flight changes.
- [ ] Verify DS3231 trustworthy time,mission epoch,event journal,new-mission status and non-resettable90-minute deadline.
- [ ] Set target26.717535N,84.301543E or the authorised adjustment;read **P4's applied target/revision** back.
- [ ] Confirm active release-confirmation procedure and its latency relative to950 m;do not confuse ARM with RELEASE.
- [ ] Confirm SD/flash capacity and write health;GCS judges'filename/header/plots/raw receive log ready.
- [ ] Start telemetry by command;verify actual RF CSV at≥1Hz and ACK channel;TX_OFF test completed earlier.
- [ ] Verify cameraREC acknowledgment and storage;beacon acoustic test and wheel/Hall guarded checks already passed.
- [ ] Arm mission only after readiness flags and physical mechanism checks;record operator actions.
- [ ] During lift/release:watch state,AGL,release latch,petri deadline,600 mbay event,GNSSage and supply/fault indicators.
- [ ] After landing: stop the wheel, activate the beacon, obtain recovery clearance, shut down RF and copy the received CSV to USB immediately.
- [ ] Preserve original logs and cards;record actual landing point,target revision,wind and anomalies before tuning anything.

