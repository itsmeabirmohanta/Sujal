/*
 * VRITRA CAMERA — Seeed XIAO ESP32S3 Sense, onboard camera + onboard microSD   (2026-10-27)
 *
 * The camera does NOTHING until MAIN (Teensy 4.1, Board 1) tells it to:
 *   SNAP  -> takes ONE photo, saves /SNAPnnn.JPG, replies "CAM SNAP OK /SNAP003.JPG 24512B"   (bench test)
 *   REC   -> starts video and NEVER stops until power is removed (sent by MAIN at launch)
 *   STAT  -> replies state, file, frames, real fps, real kB/s, free card space
 *   PING / TEST -> replies "CAM OK ..."
 * The same commands also work from the XIAO's own USB port (Serial Monitor, 115200), for bench work.
 *
 * WIRING (Board 2 camera plug J4, through the 2 wires on the empty mux pads)
 *   J4.1 5V -> XIAO 5V   J4.2 GND -> XIAO GND
 *   J4.3 (from MAIN TX, pin 1) -> XIAO D7 = GPIO44 (RX)
 *   J4.4 (to MAIN RX, pin 0)   <- XIAO D6 = GPIO43 (TX)
 *   Do NOT use XIAO D8/D9/D10 (microSD) or the user LED (GPIO21 = microSD chip select).
 *
 * ARDUINO IDE SETTINGS (Tools menu) — the camera will NOT start without PSRAM:
 *   Board: "XIAO_ESP32S3"   ·   PSRAM: "OPI PSRAM"   ·   USB CDC On Boot: "Enabled"
 *
 * RADIO: Wi-Fi and Bluetooth are never started (WiFi.h / BLE are not even included), so this board
 *        cannot transmit on 2.4 GHz — required by the frequency rules.
 *
 * VIDEO FILES
 *   /VIDnnn_kk.MJPG  = plain JPEG frames back to back, a NEW file every SEGMENT_S seconds,
 *                      flushed every second -> a power cut at landing loses at most ~1 s.
 *   /VIDnnn.CSV      = one line per frame: segment, frame, ms since REC, bytes (to match telemetry time).
 *   Play / convert on the PC:   ffmpeg -framerate 15 -i VID001_00.MJPG -c copy VID001_00.avi
 *   (VLC also opens .MJPG files directly.)
 *
 * MCU current is lowest while waiting: the camera is switched off until SNAP / REC.
 */
#include "esp_camera.h"
#include <FS.h>
#include <SD.h>
#include <SPI.h>

// ---------- settings you may change ----------
const framesize_t FRAME_SIZE   = FRAMESIZE_VGA;   // 640x480. SVGA = 800x600, HD = 1280x720 (bigger files, fewer fps)
const int         JPEG_QUALITY = 12;              // 10 = best .. 63 = worst. 12 is a good balance
const uint32_t    SEGMENT_S    = 60;              // new video file every 60 s
const uint32_t    AUTO_REC_MIN = 0;               // 0 = only MAIN starts video. e.g. 180 = also start by itself 3 h after power-on
const uint32_t    LINK_BAUD    = 115200;          // must match MAIN Serial1

// ---------- XIAO ESP32S3 Sense pins (from the ESP32 core's camera_pins.h) ----------
#define PIN_XCLK 10
#define PIN_SIOD 40
#define PIN_SIOC 39
#define PIN_D7 48
#define PIN_D6 11
#define PIN_D5 12
#define PIN_D4 14
#define PIN_D3 16
#define PIN_D2 18
#define PIN_D1 17
#define PIN_D0 15
#define PIN_VSYNC 38
#define PIN_HREF 47
#define PIN_PCLK 13
#define SD_CS 21
#define SD_SCK 7
#define SD_MISO 8
#define SD_MOSI 9
#define LINK_RX 44                                // D7, from MAIN
#define LINK_TX 43                                // D6, to MAIN (the chip's boot text also comes out here at power-on)

HardwareSerial &Link = Serial0;                   // UART0 on GPIO43/44 (USB "Serial" is separate)

enum State { IDLE, RECORDING };
State state = IDLE;
bool camOn = false, sdOk = false;
char sensorName[12] = "none";
uint16_t vidNum = 0, segNum = 0;
File vidFile, csvFile;
uint32_t recStartMs = 0, segStartMs = 0, lastFlushMs = 0;
uint32_t frames = 0, bytesTotal = 0, statFrames = 0, statBytes = 0, statMs = 0;
float fpsNow = 0, kbpsNow = 0;

// ---------- replies go to MAIN and to USB ----------
void say(const char *fmt, ...) {
  char buf[160];
  va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof buf, fmt, ap); va_end(ap);
  Link.println(buf);
  Serial.println(buf);
}

// ---------- camera ----------
bool camStart() {
  if (camOn) return true;
  if (!psramFound()) { say("CAM ERR no PSRAM - set Tools > PSRAM > OPI PSRAM"); return false; }
  camera_config_t c = {};
  c.pin_d0 = PIN_D0; c.pin_d1 = PIN_D1; c.pin_d2 = PIN_D2; c.pin_d3 = PIN_D3;
  c.pin_d4 = PIN_D4; c.pin_d5 = PIN_D5; c.pin_d6 = PIN_D6; c.pin_d7 = PIN_D7;
  c.pin_xclk = PIN_XCLK; c.pin_pclk = PIN_PCLK; c.pin_vsync = PIN_VSYNC; c.pin_href = PIN_HREF;
  c.pin_sccb_sda = PIN_SIOD; c.pin_sccb_scl = PIN_SIOC;
  c.pin_pwdn = -1; c.pin_reset = -1;
  c.xclk_freq_hz = 20000000;
  c.ledc_timer = LEDC_TIMER_0; c.ledc_channel = LEDC_CHANNEL_0;
  c.pixel_format = PIXFORMAT_JPEG;
  c.frame_size = FRAME_SIZE;
  c.jpeg_quality = JPEG_QUALITY;
  c.fb_count = 2;
  c.fb_location = CAMERA_FB_IN_PSRAM;
  c.grab_mode = CAMERA_GRAB_LATEST;               // always the newest frame, never a stale one
  esp_err_t e = esp_camera_init(&c);
  if (e != ESP_OK) { say("CAM ERR camera init 0x%x - check the camera ribbon is clicked in", e); return false; }
  sensor_t *s = esp_camera_sensor_get();
  uint16_t pid = s ? s->id.PID : 0;
  snprintf(sensorName, sizeof sensorName, "%s", pid == OV2640_PID ? "OV2640" : pid == OV3660_PID ? "OV3660" :
           pid == OV5640_PID ? "OV5640" : "unknown");
  for (int i = 0; i < 4; i++) {                    // first frames after power-up are dark: let exposure settle
    camera_fb_t *fb = esp_camera_fb_get();
    if (fb) esp_camera_fb_return(fb);
  }
  camOn = true;
  return true;
}
void camStop() {                                   // saves power while waiting on the pad
  if (camOn) { esp_camera_deinit(); camOn = false; }
}

// ---------- SD card ----------
bool sdStart() {
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS, SPI, 20000000)) return false;
  return SD.cardType() != CARD_NONE;
}
uint16_t nextFree(const char *fmt) {               // first unused number for SNAP / VID files
  char name[24];
  for (uint16_t n = 0; n < 1000; n++) { snprintf(name, sizeof name, fmt, n); if (!SD.exists(name)) return n; }
  return 999;
}
float freeGB() { return (SD.totalBytes() - SD.usedBytes()) / 1e9f; }

// ---------- SNAP: one photo ----------
void snap() {
  if (state == RECORDING) { say("CAM SNAP refused: recording"); return; }
  if (!sdOk) { say("CAM ERR no SD card"); return; }
  if (!camStart()) return;
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) { say("CAM ERR no frame"); camStop(); return; }
  char name[24];
  snprintf(name, sizeof name, "/SNAP%03u.JPG", nextFree("/SNAP%03u.JPG"));
  File f = SD.open(name, FILE_WRITE);
  size_t w = f ? f.write(fb->buf, fb->len) : 0;
  if (f) f.close();
  if (w == fb->len) say("CAM SNAP OK %s %uB %ux%u", name, (unsigned)fb->len, fb->width, fb->height);
  else say("CAM ERR SD write failed (%u of %u)", (unsigned)w, (unsigned)fb->len);
  esp_camera_fb_return(fb);
  camStop();
}

// ---------- REC: video until power off ----------
bool openSegment() {
  char name[24];
  if (vidFile) vidFile.close();
  snprintf(name, sizeof name, "/VID%03u_%02u.MJPG", vidNum, segNum);
  vidFile = SD.open(name, FILE_WRITE);
  segStartMs = millis();
  if (!vidFile) { say("CAM ERR cannot open %s", name); return false; }
  return true;
}
void startRec() {
  if (state == RECORDING) { say("CAM REC already running VID%03u", vidNum); return; }
  if (!sdOk) { say("CAM ERR no SD card"); return; }
  if (!camStart()) return;
  vidNum = nextFree("/VID%03u.CSV");
  segNum = 0;
  char name[24];
  snprintf(name, sizeof name, "/VID%03u.CSV", vidNum);
  csvFile = SD.open(name, FILE_WRITE);
  if (csvFile) csvFile.println("segment,frame,ms_since_rec,bytes");
  if (!openSegment()) return;
  recStartMs = statMs = lastFlushMs = millis();
  frames = bytesTotal = statFrames = statBytes = 0;
  state = RECORDING;
  say("CAM REC OK VID%03u %s", vidNum, sensorName);
}
void recordFrame() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) return;
  uint32_t now = millis();
  size_t w = vidFile.write(fb->buf, fb->len);
  if (csvFile) csvFile.printf("%u,%lu,%lu,%u\n", segNum, (unsigned long)frames, (unsigned long)(now - recStartMs), (unsigned)fb->len);
  esp_camera_fb_return(fb);
  if (w == 0) { say("CAM ERR SD write failed (card full or removed?)"); delay(200); return; }
  frames++; bytesTotal += w; statFrames++; statBytes += w;

  if (now - lastFlushMs >= 1000) {                 // make the last second safe against power loss
    vidFile.flush(); if (csvFile) csvFile.flush();
    fpsNow = statFrames * 1000.0f / (now - statMs);
    kbpsNow = statBytes / 1.024f / (now - statMs);
    statFrames = statBytes = 0; statMs = lastFlushMs = now;
  }
  if (now - segStartMs >= SEGMENT_S * 1000UL) { segNum++; openSegment(); }
}

void status() {
  if (state == RECORDING)
    say("CAM STAT REC VID%03u_%02u frames %lu %.1f fps %.0f kB/s %.1f MB total, SD free %.1f GB, %s",
        vidNum, segNum, (unsigned long)frames, fpsNow, kbpsNow, bytesTotal / 1e6f, freeGB(), sensorName);
  else
    say("CAM STAT IDLE SD %s free %.1f GB, sensor %s, up %lu s", sdOk ? "OK" : "MISSING", sdOk ? freeGB() : 0.0f,
        sensorName, (unsigned long)(millis() / 1000));
}

// ---------- commands (one per line) ----------
void command(String s) {
  s.trim(); s.toUpperCase();
  if (s.length() == 0) return;
  if (s == "SNAP") snap();
  else if (s == "REC") startRec();
  else if (s == "STAT") status();
  else if (s == "PING" || s == "TEST") say("CAM OK %s SD %s %s", sensorName, sdOk ? "OK" : "MISSING", state == RECORDING ? "REC" : "IDLE");
  else say("CAM ? unknown command '%s' (SNAP REC STAT PING)", s.c_str());
}
void readLine(Stream &in, String &buf) {
  while (in.available()) {
    char c = in.read();
    if (c == '\n' || c == '\r') { command(buf); buf = ""; }
    else if (buf.length() < 40) buf += c;          // ignore noise, never grow without limit
  }
}

void setup() {
  Serial.begin(115200);                            // USB
  Link.begin(LINK_BAUD, SERIAL_8N1, LINK_RX, LINK_TX);
  delay(300);
  sdOk = sdStart();
  bool camOk = camStart();                         // self-test once, then switch the camera off
  camStop();
  say("CAM READY %s SD %s free %.1f GB  (%s q%d, Wi-Fi/BT off)", camOk ? sensorName : "CAMERA-FAIL",
      sdOk ? "OK" : "MISSING", sdOk ? freeGB() : 0.0f, FRAME_SIZE == FRAMESIZE_VGA ? "VGA" : "custom", JPEG_QUALITY);
}

void loop() {
  static String fromMain, fromUsb;
  static bool autoTried = false;
  readLine(Link, fromMain);
  readLine(Serial, fromUsb);
  if (state == RECORDING) recordFrame();
  else if (AUTO_REC_MIN && !autoTried && millis() > AUTO_REC_MIN * 60000UL) {
    autoTried = true;                              // try once, never spam
    say("CAM auto-start after %lu min", (unsigned long)AUTO_REC_MIN);
    startRec();
  }
}
