from pathlib import Path
import requests, json, hashlib, concurrent.futures
S={
'gnss_hardware':'https://quectel.com/content/uploads/2024/02/Quectel_L89_R2.0_Hardware-Design_V1.2.pdf',
'gnss_protocol':'https://quectel.com/content/uploads/2024/02/Quectel_L89_R2.0_GNSS_Protocol_Specification_V1.0.pdf',
'bmp585_ds':'https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmp585-ds003.pdf',
'bmp5_defs':'https://raw.githubusercontent.com/boschsensortec/BMP5_SensorAPI/master/bmp5_defs.h',
'bmp5_driver':'https://raw.githubusercontent.com/boschsensortec/BMP5_SensorAPI/master/bmp5.c',
'icm_ds':'https://product.tdk.com/system/files/dam/doc/product/sensor/mortion-inertial/imu/data_sheet/ds-000189-icm-20948-v1.5.pdf',
'lsm_ds':'https://www.st.com/resource/en/datasheet/lsm6dso32.pdf',
'lsm_driver':'https://raw.githubusercontent.com/STMicroelectronics/lsm6dso32-pid/master/lsm6dso32_reg.c',
'lsm_header':'https://raw.githubusercontent.com/STMicroelectronics/lsm6dso32-pid/master/lsm6dso32_reg.h',
'ina_ds':'https://www.ti.com/lit/ds/symlink/ina260.pdf',
'hdc_ds':'https://www.ti.com/lit/ds/symlink/hdc3020.pdf',
'rtc_ds':'https://www.analog.com/media/en/technical-documentation/data-sheets/ds3231.pdf',
'lr1121_ds':'https://files.waveshare.com/wiki/Core1121/LR1121_H2_DS_v2_0.pdf',
'radio_lr11_header':'https://raw.githubusercontent.com/jgromes/RadioLib/7.7.1/src/modules/LR11x0/LR11x0.h',
'radio_lr11_cpp':'https://raw.githubusercontent.com/jgromes/RadioLib/7.7.1/src/modules/LR11x0/LR11x0.cpp',
'radio_lr1121_cpp':'https://raw.githubusercontent.com/jgromes/RadioLib/7.7.1/src/modules/LR11x0/LR1121.cpp',
'core1121':'https://www.waveshare.com/wiki/Core1121-XF',
'p4board':'https://www.waveshare.com/wiki/ESP32-P4-Pico',
'agfrc':'https://www.agf-rc.com/agfrc-coreless-mini-servo-b26clm-p1669135.html',
'towerpro':'https://towerpro.com.tw/product/mg90s-3/',
'grove':'https://wiki.seeedstudio.com/Grove-Buzzer/',
'7semi_l89':'https://7semi.com/7semi-l89ha-breakout-multi-gnss-engines-for-gps-irnss-glonass-beidou-galileo-and-qzss/',
'p4_uart':'https://docs.espressif.com/projects/esp-idf/en/stable/esp32p4/api-reference/peripherals/uart.html',
'p4_ledc':'https://docs.espressif.com/projects/esp-idf/en/stable/esp32p4/api-reference/peripherals/ledc.html',
'p4_mcpwm':'https://docs.espressif.com/projects/esp-idf/en/stable/esp32p4/api-reference/peripherals/mcpwm.html',
'p4_freertos':'https://docs.espressif.com/projects/esp-idf/en/stable/esp32p4/api-reference/system/freertos_idf.html',
'teensy_serial':'https://www.pjrc.com/teensy/td_uart.html',
'teensy_wire':'https://www.pjrc.com/teensy/td_libs_Wire.html',
'teensy_timer':'https://www.pjrc.com/teensy/td_timing_IntervalTimer.html',
'teensy_sd':'https://www.pjrc.com/teensy/td_libs_SD.html',
'teensy_eeprom':'https://www.pjrc.com/teensy/td_libs_EEPROM.html',
'nasa_wind':'https://ntrs.nasa.gov/citations/20200011546',
'parafoil_paper':'https://arxiv.org/pdf/2105.00715',
'gnss_forum':'https://forums.quectel.com/t/problem-with-pstm-commands-for-l89ha-gnss-module/48222',
'hdc_driver':'https://raw.githubusercontent.com/adafruit/Adafruit_HDC302x/main/Adafruit_HDC302x.cpp',
'ino_ina':'https://raw.githubusercontent.com/adafruit/Adafruit_INA260/master/Adafruit_INA260.cpp',
'w25_ds':'https://www.winbond.com/resource-files/w25q64jv%20spi%20%20%20revc%2006032016%20kms.pdf',
'sparkfun_icm':'https://github.com/sparkfun/SparkFun_ICM-20948_ArduinoLibrary',
}
def fetch(k,u):
    row={'id':k,'url':u}
    try:
        r=requests.get(u,timeout=35);row.update(status=r.status_code,final_url=r.url)
        if r.ok:
            ext='.pdf' if r.content.startswith(b'%PDF') else ('.h' if u.endswith('.h') else '.cpp' if u.endswith('.cpp') else '.c' if u.endswith('.c') else '.html')
            p=Path('research/sources')/(k+ext);p.write_bytes(r.content)
            row.update(file=p.as_posix(),bytes=len(r.content),sha256=hashlib.sha256(r.content).hexdigest())
    except Exception as e: row['error']=str(e)
    return row
Path('research/sources').mkdir(parents=True,exist_ok=True)
with concurrent.futures.ThreadPoolExecutor(max_workers=6) as ex:
    rows=list(ex.map(lambda kv:fetch(*kv),S.items()))
Path('research/source_manifest.json').write_text(json.dumps(rows,indent=2),encoding='utf-8')
print('\n'.join(f"{r['id']}: {r.get('status',r.get('error'))} {r.get('bytes',0)}" for r in rows))
