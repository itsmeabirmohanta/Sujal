"""Offline calculations only. No hardware, serial ports, or flight data output."""
from pathlib import Path
import json, math

HERE=Path(__file__).resolve().parent
OUT=HERE.parent
def toa_ms(n):
    sf=9;bw=125000;cr_extra=1;crc=1;ih=0;de=0;preamble=8
    symbols=8+max(math.ceil((8*n-4*sf+28+16*crc-20*ih)/(4*(sf-2*de)))*(cr_extra+4),0)
    return (preamble+4.25+symbols)*2**sf/bw*1000
def sentence(body):
    checksum=0
    for b in body.encode('ascii'): checksum^=b
    return f'${body}*{checksum:02X}'

commands=[
 ('PQTMVERNO','PQTMVERNO response','Identify; no PAIR001 assumed'),
 ('PAIR067','PAIR001,067,0','Query constellation profile'),
 ('PAIR050,1000','PAIR001,050,0','Configure at 1 Hz'),
 ('PAIR066,1,1,1,1,1,1','PAIR001,066,0','All-six trial, require ACK and fix test'),
 ('PAIR070,0','PAIR001,070,0','Disable static hold'),
 ('PAIR072,10','PAIR001,072,0','10 degree elevation mask trial'),
 ('PAIR410,1','PAIR001,410,0','SBAS search'),
 ('PAIR400,2','PAIR001,400,0','SBAS correction source'),
 *[(f'PAIR062,{i},{rate}','PAIR001,062,0',name) for i,rate,name in [
 (0,1,'GGA'),(1,0,'disable GLL'),(2,1,'GSA'),(3,5,'sparse GSV'),
 (4,1,'RMC'),(5,0,'disable VTG'),(6,0,'disable ZDA'),(7,0,'disable GRS'),(8,1,'GST if supported')]],
 ('PAIR864,0,0,115200','PAIR001,864,0','Save/reboot before baud takes effect; collect ACK at prior rate'),
 ('PAIR513','PAIR001,513,0','Save changed settings at 1 Hz'),
 ('PAIR050,200','PAIR001,050,0','5 Hz trial; verify measured epoch cadence'),
 ('PAIR003','PAIR001,003,0','Stop GNSS before high-rate save'),
 ('PAIR513','PAIR001,513,0','Save high-rate settings while GNSS stopped'),
 ('PAIR002','PAIR001,002,0','Restart GNSS; verify measured rate'),
]
text=['GNSS GROUND CONFIGURATION TRANSACTION CHECKLIST - NOT AN AUTOMATIC SCRIPT',
      'Every line ends with CR LF. Send one command, wait for ACK, read back, then proceed.',
      'Confirm installed firmware compatibility. Never reconfigure the shared receiver in flight.','']
for command,ack,why in commands:
    expected=sentence(ack) if ack.startswith('PAIR001') else ack
    text.extend([sentence(command),f'  expect: {expected}',f'  purpose: {why}'])
text.extend(['','QUERY/RECOVERY COMMANDS - DELIBERATE GROUND USE ONLY'])
for body in ['PAIR051','PAIR063,-1','PAIR067','PAIR071','PAIR073','PAIR401','PAIR411','PAIR865,0,0',
             'PAIR004','PAIR005','PAIR006','PAIR007','PAIR511','PAIR490,1','PAIR074,1','PAIR075','PAIR391,1']:
    text.append(sentence(body))
(HERE/'gnss_commands.txt').write_text('\n'.join(text)+'\n',encoding='utf-8')
assert sentence('PAIR864,0,0,115200')=='$PAIR864,0,0,115200*1B'
assert sentence('PQTMVERNO')=='$PQTMVERNO*58'
assert sentence('PAIR513')=='$PAIR513*3D'
assert abs(toa_ms(200)-1004.544)<1e-8
assert toa_ms(138)+toa_ms(28)+40<=1000
assert toa_ms(139)+toa_ms(28)+40>1000
assert toa_ms(130)+toa_ms(32)+40<=1000
assert len('025,C,19,-2147483648,FF,12345678')<=32  # uplink has no terminator
sample=['2026-INSPACe-CAN-7USAT-025','10000','10000','1100.0','101325','-20.0','12.60',
        '86399','26.717535','84.301543','1100.0','35','-156.9','-156.9','-156.9','60000','6']
length=len(','.join(sample)+'\r')
assert length==130
result={
 'status':'Calculated design budgets; NOT measured flight performance',
 'lora_assumptions':{'sf':9,'bw_hz':125000,'cr':'4/5','preamble_symbols':8,'header':'explicit','crc':True,'ldro':False},
 'toa_ms':{str(n):toa_ms(n) for n in [24,28,32,40,120,130,138,140,160,180,190,197,200,255]},
 'maximum_payload_bytes_for_1s_without_uplink':max(n for n in range(256) if toa_ms(n)<=1000),
 'maximum_payload_bytes_with_28B_uplink_and_40ms_guards':max(n for n in range(256) if toa_ms(n)+toa_ms(28)+40<=1000),
 'one_hz_schedule_ms':toa_ms(130)+toa_ms(32)+40,
 'one_hz_schedule_downlink_bytes':130,
 'one_hz_schedule_uplink_bytes':32,
 'illustrative_core_csv_bytes':length,
 'pressure_m_per_pa_at_293_15K_100000Pa':287.05*293.15/9.80665/100000,
 'noise_m_rms_for_0_21Pa':.21*287.05*293.15/9.80665/100000,
 'dynamic_pressure_pa_at_8mps_density1_2':.5*1.2*8**2,
 'dynamic_pressure_pa_at_20mps_density1_2':.5*1.2*20**2,
 'uart_96B_ms_at115200':96*10/115200*1000,
 'uart_200B_ms_at115200':200*10/115200*1000,
 'end_to_end_budget_ms_excluding_receiver_internal_latency':200+200*10/115200*1000+1+50+20,
 'complete_epoch_to_pwm_budget_ms':1+50+20,
 'flash_8MiB_96B_at20Hz_minutes':8*1024**2/(96*20)/60,
 'flash_8MiB_96B_at200Hz_minutes':8*1024**2/(96*200)/60,
 'spool_travel_15mm_60deg_mm':15*math.pi/3,
 'wheel_10000rpm_hall_hz_1ppr':10000/60,
 'wheel_10000rpm_degrees_per_s':10000*6,
 'capacitor_470uF_2A_5ms_ideal_droop_V':2*.005/470e-6,
}
(OUT/'calculations.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
print('PASS: independent checksum examples, airtime boundary, and illustrative CSV length')
print(f'1Hz schedule: {result["one_hz_schedule_ms"]:.3f} ms')
