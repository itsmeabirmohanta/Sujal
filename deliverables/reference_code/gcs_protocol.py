"""Flight protocol building blocks. No simulator, replay or serial-send code."""
from dataclasses import dataclass
import csv, math, re

TEAM='2026-INSPACe-CAN-7USAT-025'
FIELDS=('TEAM_ID','TIME_S','PKT','ALT_M','PRES_PA','TEMP_C','BUS_V','GNSS_TIME','LAT','LON','GPS_ALT_M','SATS','ACC_X','ACC_Y','ACC_Z','SPIN_RATE','STATE')
INTEGER={'PKT','SATS','STATE','GNSS_TIME'}
GNSS={'GNSS_TIME','LAT','LON','GPS_ALT_M','SATS'}

@dataclass(frozen=True)
class LivePacket:
    cells:tuple
    received_monotonic_ns:int
    transport:str

def parse_core(line:bytes,received_monotonic_ns:int,transport:str)->LivePacket:
    if transport!='LIVE_SERIAL' or received_monotonic_ns<0:raise ValueError('physical live serial transport required')
    if not line.endswith(b'\r') or b'\n' in line or len(line)>255:raise ValueError('expected one CR-terminated RF core packet')
    cells=line[:-1].decode('ascii').split(',')
    if len(cells)!=len(FIELDS) or cells[0]!=TEAM:raise ValueError('wrong team or core schema')
    for name,cell in zip(FIELDS[1:],cells[1:]):
        if not cell:continue
        if not re.fullmatch(r'-?\d+(?:\.\d+)?',cell):raise ValueError('non-decimal data')
        value=float(cell)
        if not math.isfinite(value):raise ValueError('non-finite data')
        if name in INTEGER and not value.is_integer():raise ValueError('integer field required')
        if name=='STATE' and not 0<=value<=7:raise ValueError('state outside0..7')
        if name=='LAT' and not -90<=value<=90:raise ValueError('latitude')
        if name=='LON' and not -180<=value<=180:raise ValueError('longitude')
        if name=='GNSS_TIME' and not 0<=value<86400:raise ValueError('UTC seconds')
        if name in {'PKT','SATS'} and value<0:raise ValueError('negative count')
    if bool(cells[FIELDS.index('LAT')])!=bool(cells[FIELDS.index('LON')]):raise ValueError('partial coordinate pair')
    return LivePacket(tuple(cells),received_monotonic_ns,transport)

class FlightCsvWriter:
    def __init__(self,file):
        self.writer=csv.writer(file,lineterminator='\n');self.writer.writerow(FIELDS)
    def append(self,packet:LivePacket):
        if packet.transport!='LIVE_SERIAL':raise ValueError('test/replay cannot write the flight CSV')
        self.writer.writerow(packet.cells) # preserve blanks; never fill previous values

def compact_command(command:int,argument:int,transaction:int,session:int)->bytes:
    if command not in {1,2,3,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24}:raise ValueError('unknown/retired command')
    if not -2147483648<=argument<=2147483647 or not 0<=transaction<=255 or not 1<=session<=0xffffffff:raise ValueError('wire range')
    wire=f'025,C,{command},{argument},{transaction:02X},{session:08X}'.encode('ascii')
    if len(wire)>32:raise ValueError('airtime budget exceeded')
    return wire # RF length terminates uplink; NO CR

def target_commands(lat:float,lon:float,transaction:int,session:int):
    if not math.isfinite(lat) or not math.isfinite(lon) or not -90<=lat<=90 or not -180<=lon<=180:raise ValueError('target range')
    return (compact_command(19,round(lat*1e7),transaction,session),
            compact_command(20,round(lon*1e7),transaction,session),compact_command(21,0,transaction,session))

def current_marker(packet:LivePacket,now_ns:int,max_receive_age_ns:int):
    if packet.transport!='LIVE_SERIAL' or now_ns<packet.received_monotonic_ns or now_ns-packet.received_monotonic_ns>max_receive_age_ns:return None
    lat,lon=(packet.cells[FIELDS.index(k)] for k in ['LAT','LON'])
    return None if not lat or not lon else (float(lat),float(lon))
