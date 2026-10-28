# Open questions and evidence required

Generated from the [canonical engineering report](VRITRA_ENGINEERING_REPORT.md). All hardware results remain pending.

## 15. Open questions only the team can close

1. Which **ram-air** flight article is packed:7-cell0.40 m² or11-cell0.71 m²? The clarification confirmed two parachutes,not the ram-air area/cell count.
2. Is950 m petri opening explicitly on **descent**,and how soon can release confirmation arrive? Does “above boot” mean the pad boot point even after a reset?
3. What is the measured bay-command-to-physical-opening latency,and is the drogue retained without fouling the ram-air canopy?
4. Are B26CLM the actual two servos,and what are each servo's safe pulse endpoints,line travel,working tension and loaded current?
5. What are the exact ESC,Hall sensor and adjustable buck models,ESC protocol/arm/stop behavior,magnet count and qualified rotor speed?
6. Is the GNSS J7 backup cell physically fitted and maintaining the correct voltage? Is any external antenna connected,changing the internal antenna selection?
7. Are there independent release/deployment sensors or rail measurements omitted from the current files? No such connections are assumed here.
8. Which hardware tests have actually run since the brief's “compiled,not run” status? Provide raw traces/results before marking any matrix row passed.
9. Has the organiser accepted the accelerometer CSV expansion, empty invalid fields and short command alias, and specified the92 dB measurement setup?
10. Can the team demonstrate stable1–3 m/s secondary descent and mechanical wheel stabilization with this flight article? Simulation and requested RPM do not close those requirements.
