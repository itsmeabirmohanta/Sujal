from pathlib import Path
import requests, json, hashlib, concurrent.futures, sys
sys.path.insert(0,str(Path(__file__).parent/'deps'))
import pymupdf
sources={
'gnss_hardware':'https://quectel.com/content/uploads/2024/02/Quectel_L89_R2.0_Hardware-Design_V1.2.pdf',
'icm_v14':'https://invensense.tdk.com/wp-content/uploads/2021/07/DS-000189-ICM-20948-v1.4.pdf',
'rtc_ds':'https://www.analog.com/media/en/technical-documentation/data-sheets/DS3231.pdf',
'lsm_an':'https://www.st.com/resource/en/application_note/an5473-lsm6dso32-alwayson-3d-accelerometer-and-3d-gyroscope-stmicroelectronics.pdf',
'semtech_driver':'https://raw.githubusercontent.com/Lora-net/SWDR001/master/README.md',
'st_fifo_example':'https://raw.githubusercontent.com/STMicroelectronics/STMems_Standard_C_drivers/master/lsm6dso32_STdC/examples/lsm6dso32_fifo.c',
'st_selftest_example':'https://raw.githubusercontent.com/STMicroelectronics/STMems_Standard_C_drivers/master/lsm6dso32_STdC/examples/lsm6dso32_self_test.c',
'icm_driver_header':'https://raw.githubusercontent.com/sparkfun/SparkFun_ICM-20948_ArduinoLibrary/main/src/util/ICM_20948_REGISTERS.h',
'radiolib_properties':'https://raw.githubusercontent.com/jgromes/RadioLib/7.7.1/library.properties',
'arduino33_uart':'https://raw.githubusercontent.com/espressif/arduino-esp32/3.3.0/cores/esp32/HardwareSerial.h',
'hdc_ti_note':'https://www.ti.com/lit/an/sboa631/sboa631.pdf',
}
def fetch(k,u):
 row={'id':k,'url':u}
 try:
  r=requests.get(u,timeout=25);row.update(status=r.status_code,final_url=r.url)
  if r.ok:
   ext='.pdf' if r.content.startswith(b'%PDF') else '.txt';p=Path('research/sources')/(k+ext);p.write_bytes(r.content)
   row.update(file=p.as_posix(),bytes=len(r.content),sha256=hashlib.sha256(r.content).hexdigest())
   if ext=='.pdf':
    doc=pymupdf.open(p);p.with_suffix('.txt').write_text('\n'.join(f'\n--- PDF PAGE {i+1} ---\n'+pg.get_text(sort=True) for i,pg in enumerate(doc)),encoding='utf-8')
 except Exception as e: row['error']=str(e)
 return row
with concurrent.futures.ThreadPoolExecutor(max_workers=5) as ex: rows=list(ex.map(lambda x:fetch(*x),sources.items()))
Path('research/additional_manifest.json').write_text(json.dumps(rows,indent=2),encoding='utf-8')
print('\n'.join(f"{r['id']}: {r.get('status',r.get('error'))}" for r in rows))
