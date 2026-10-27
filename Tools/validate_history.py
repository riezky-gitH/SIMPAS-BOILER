"""Validate readable history exports or CRC-protected internal journals."""
import argparse
from pathlib import Path
import re
import csv
from datetime import datetime
from decimal import Decimal,InvalidOperation

HEADER=b'epoch,boot,uptime_s,p1_cbar,t1_dC,f1,c1,m1,a1,p2_cbar,t2_dC,f2,c2,m2,a2,crc16\n'
NEW_HEADER=HEADER.replace(b',crc16',b',waktu_wib,crc16')

READABLE_HEADER=['Waktu WIB','Tekanan Slave 1 (bar)','Suhu Slave 1 (C)','Tekanan Slave 2 (bar)','Suhu Slave 2 (C)','Status Slave 1','Status Slave 2']
ALARM_REASONS={'Tekanan tinggi','Suhu tinggi','Gangguan sensor tekanan','Gangguan sensor suhu','Sumber simulasi digunakan saat mode LIVE','Trip tertahan'}

def is_readable(path):
    with Path(path).open(encoding='utf-8-sig',newline='') as f:return next(csv.reader(f),None)==READABLE_HEADER

def validate_readable(path):
    count=0
    with Path(path).open(encoding='utf-8-sig',newline='') as f:
        rows=csv.reader(f)
        if next(rows,None)!=READABLE_HEADER:raise ValueError('Wrong readable history header')
        for number,row in enumerate(rows,2):
            if len(row)!=7:raise ValueError(f'Expected seven columns on row {number}')
            if row[0]!='Belum sinkron':
                try:datetime.strptime(row[0],'%Y-%m-%d %H:%M:%S')
                except ValueError:raise ValueError(f'Invalid WIB time on row {number}')
            for i in (1,2,3,4):
                if not row[i]:continue
                try:value=Decimal(row[i])
                except InvalidOperation:raise ValueError(f'Invalid sensor value on row {number}')
                if not value.is_finite() or not 0<=value<=(10 if i in (1,3) else 1024):raise ValueError(f'Sensor value outside real units on row {number}')
            for value in row[5:]:
                if value in ('NORMAL','WASPADA','TERPUTUS','MENUNGGU SENSOR','STATUS LAMA TIDAK TERSEDIA'):continue
                if not value.startswith('ALARM: ') or any(reason not in ALARM_REASONS for reason in value[7:].split('; ')):raise ValueError(f'Invalid written slave status on row {number}')
            count+=1
    return count

def crc16(data):
    crc=0xffff
    for b in data:
        crc^=b<<8
        for _ in range(8):crc=((crc<<1)^0x1021 if crc&0x8000 else crc<<1)&0xffff
    return crc
def validate(path):
    if is_readable(path):return validate_readable(path)
    count=0
    with Path(path).open('rb') as f:
        header=f.readline().replace(b'\r\n',b'\n')
        if header not in (HEADER,NEW_HEADER):raise ValueError('Wrong history schema/header')
        for number,line in enumerate(f,2):
            line=line.rstrip(b'\r\n')
            if not line:raise ValueError(f'Empty/truncated row {number}')
            try:payload,tag=line.rsplit(b',',1)
            except ValueError:raise ValueError(f'Missing CRC on row {number}')
            if not re.fullmatch(b'[0-9A-F]{4}',tag) or int(tag,16)!=crc16(payload):raise ValueError(f'CRC mismatch on row {number}')
            values=payload.split(b',')
            if len(values)!=(16 if header==NEW_HEADER else 15) or any(not re.fullmatch(b'-?[0-9]+',v) for v in values[:15]):raise ValueError(f'Invalid fields on row {number}')
            if header==NEW_HEADER:
                from datetime import datetime,timezone,timedelta
                epoch=int(values[0]);expected=datetime.fromtimestamp(epoch,timezone(timedelta(hours=7))).strftime('%Y-%m-%d %H:%M:%S').encode() if epoch else b'UNSYNCED'
                if values[15]!=expected:raise ValueError(f'Invalid WIB timestamp on row {number}')
            count+=1
    return count
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('csv',type=Path);args=p.parse_args();count=validate(args.csv);print(f'{count} valid readable history records (seven columns)' if is_readable(args.csv) else f'{count} valid CRC-checked journal records')
