## VRITRA Ground Control Station v1.0.0

**Team 2026-INSPACe-CAN-7USAT-025 | Lovely Professional University**

### What's included
- Single-file standalone `.exe` — **no Python required** at runtime
- Pure-QPainter 3-D CanSat attitude view — **no OpenGL / GPU required**
- Live telemetry display over serial (LoRa gateway)
- 83-field CSV logging with Base-36 packet decoding
- Offline map tile support (run `fetch_map.py` to pre-cache your launch site)

### How to run
1. Download `VRITRA_GCS.exe`
2. Double-click — no install needed
3. Select the correct COM port and baud rate in the UI

### Optional: offline map tiles
```
python gcs/fetch_map.py "Phagwara, Punjab" --radius 10
```
Copy the generated `map_tiles/` folder next to `VRITRA_GCS.exe`.

### Build from source
```
cd gcs
build_exe.bat
```
