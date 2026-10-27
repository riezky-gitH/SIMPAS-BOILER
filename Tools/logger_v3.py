"""SIMPAS v3 HTTP/MQTT logger and optional localhost dashboard proxy.

No package is required for HTTP mode. MQTT mode requires paho-mqtt 2.x.
The proxy forwards operator Authorization; monitoring never grants control.
"""
from __future__ import annotations
import argparse
import csv
import io
import json
import os
from pathlib import Path
import signal
import sqlite3
import threading
import time
from datetime import datetime, timezone
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen

COLUMNS = ['received_utc','epoch','boot','uptime','node','pressure_bar','temperature_c','quality_p',
           'quality_t','relay_command','buzzer','state','causes','muted','acknowledged','trip','config_version']

def rows(snapshot):
    """Keep unknowns unknown. Never invent zero readings or a closed valve."""
    received = datetime.now(timezone.utc).isoformat(timespec='seconds')
    for node in (1, 2):
        data = snapshot.get(f'slave{node}')
        if not isinstance(data, dict):
            continue
        connected = data.get('connected') is True
        yield [received, snapshot.get('epoch') or None, snapshot.get('boot'), snapshot.get('uptime'), node,
               data.get('pressure') if connected and data.get('quality_p') == 'GOOD' else None,
               data.get('temp') if connected and data.get('quality_t') == 'GOOD' else None,
               data.get('quality_p','UNKNOWN'), data.get('quality_t','UNKNOWN'),
               data.get('relay','UNKNOWN') if connected else 'UNKNOWN',
               data.get('buzzer') if connected else None, data.get('state','UNKNOWN') if connected else 'OFFLINE',
               data.get('causes'),data.get('muted'),data.get('acknowledged'),data.get('trip'),
               data.get('config',{}).get('version')]

class Sink:
    def __init__(self, csv_path, db_path, rotate_mb=20, keep=5):
        self.csv_path=Path(csv_path);self.csv_path.parent.mkdir(parents=True,exist_ok=True)
        self.db_path=Path(db_path);self.db_path.parent.mkdir(parents=True,exist_ok=True)
        self.limit=int(rotate_mb*1024*1024);self.keep=keep;self.lock=threading.Lock()
        self.db=sqlite3.connect(self.db_path,check_same_thread=False)
        self.db.execute('PRAGMA journal_mode=WAL')
        self.db.execute('CREATE TABLE IF NOT EXISTS telemetry ('+', '.join(c+' '+('REAL' if c in ('pressure_bar','temperature_c') else 'TEXT') for c in COLUMNS)+')')
        self.db.execute('CREATE INDEX IF NOT EXISTS telemetry_time ON telemetry(received_utc,node)');self.db.commit()
    def rotate(self):
        if self.csv_path.exists() and self.csv_path.stat().st_size>=self.limit:
            last=self.csv_path.with_suffix(f'.csv.{self.keep}')
            if last.exists():last.unlink()
            for i in range(self.keep-1,0,-1):
                a=self.csv_path.with_suffix(f'.csv.{i}')
                if a.exists():a.replace(self.csv_path.with_suffix(f'.csv.{i+1}'))
            self.csv_path.replace(self.csv_path.with_suffix('.csv.1'))
    def write(self,snapshot):
        batch=list(rows(snapshot))
        if not batch:return
        with self.lock:
            self.rotate();fresh=not self.csv_path.exists() or self.csv_path.stat().st_size==0
            with self.csv_path.open('a',newline='',encoding='utf-8') as f:
                writer=csv.writer(f)
                if fresh:writer.writerow(COLUMNS)
                writer.writerows(batch);f.flush()
            with self.db:self.db.executemany('INSERT INTO telemetry VALUES ('+','.join('?' for _ in COLUMNS)+')',batch)
    def close(self):
        with self.lock:self.db.close()

def proxy_handler(master, html):
    master=master.rstrip('/')
    class Proxy(BaseHTTPRequestHandler):
        def log_message(self,*_):pass
        def do_GET(self):
            if self.path=='/':
                data=html.read_bytes();self.send_response(200);self.send_header('Content-Type','text/html; charset=utf-8');self.send_header('Content-Length',str(len(data)));self.end_headers();self.wfile.write(data)
            elif self.path.startswith('/api/'):self.forward()
            else:self.send_error(404)
        def do_POST(self):
            if self.path.startswith('/api/'):self.forward()
            else:self.send_error(404)
        def forward(self):
            origin=self.headers.get('Origin')
            if origin and origin!='http://'+self.headers.get('Host',''):
                self.send_error(403,'Origin rejected');return
            length=int(self.headers.get('Content-Length','0'))
            if length>768:self.send_error(413);return
            data=self.rfile.read(length) if self.command=='POST' else None
            headers={'Origin':master}
            for key in ('Authorization','Content-Type'):
                if self.headers.get(key):headers[key]=self.headers[key]
            req=Request(master+self.path,data=data,method=self.command,headers=headers)
            try:response=urlopen(req,timeout=15)
            except HTTPError as exc:response=exc
            except (URLError,TimeoutError):self.send_error(503,'ESP32 unavailable');return
            with response:
                payload=response.read();self.send_response(response.status)
                for key in ('Content-Type','Content-Disposition'):
                    if response.headers.get(key):self.send_header(key,response.headers[key])
                self.send_header('Content-Length',str(len(payload)));self.send_header('Cache-Control','no-store');self.end_headers()
                try:self.wfile.write(payload)
                except (BrokenPipeError,ConnectionResetError):pass
    return Proxy

def main(argv=None):
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--mode',choices=['http','mqtt'],default='http')
    p.add_argument('--master',default='http://192.168.4.1')
    p.add_argument('--broker',default='localhost');p.add_argument('--port',type=int,default=1883)
    p.add_argument('--interval',type=float,default=1);p.add_argument('--csv',default='telemetri_boiler_realtime.csv')
    p.add_argument('--sqlite',default='telemetri_boiler.sqlite3');p.add_argument('--rotate-mb',type=float,default=20)
    p.add_argument('--keep',type=int,default=5);p.add_argument('--serve',type=int,help='Optional localhost dashboard port, e.g. 8080')
    args=p.parse_args(argv)
    if args.interval<0.5 or args.rotate_mb<=0 or args.keep<1:p.error('interval >=0.5, rotate-mb >0, keep >=1 required')
    sink=Sink(args.csv,args.sqlite,args.rotate_mb,args.keep);stop=threading.Event();web=None;client=None
    def halt(*_):stop.set()
    signal.signal(signal.SIGINT,halt);signal.signal(signal.SIGTERM,halt)
    if args.serve:
        web=ThreadingHTTPServer(('127.0.0.1',args.serve),proxy_handler(args.master,Path(__file__).with_name('dashboard_scada_boiler.html')))
        threading.Thread(target=web.serve_forever,daemon=True).start();print(f'Dashboard: http://127.0.0.1:{args.serve} (proxy to {args.master})')
    try:
        if args.mode=='http':
            while not stop.is_set():
                try:
                    with urlopen(args.master.rstrip('/')+'/api/data',timeout=3) as response:data=json.load(response)
                    sink.write(data)
                except (OSError,ValueError,TimeoutError) as e:print(f'[HTTP] {e}; retrying',flush=True)
                stop.wait(args.interval)
        else:
            try:import paho.mqtt.client as mqtt
            except ImportError:
                print('MQTT mode requires: python -m pip install "paho-mqtt>=2.1,<3"');return 2
            client=mqtt.Client(mqtt.CallbackAPIVersion.VERSION2,client_id=f'simpas-logger-{os.getpid()}')
            user=os.environ.get('SIMPAS_MQTT_USER');password=os.environ.get('SIMPAS_MQTT_PASSWORD')
            if user:client.username_pw_set(user,password)
            def on_connect(c,userdata,flags,reason,properties):
                if reason==0:c.subscribe('boiler/telemetry');print('[MQTT] subscribed boiler/telemetry',flush=True)
                else:print(f'[MQTT] connect rejected: {reason}',flush=True)
            def on_message(c,userdata,msg):
                try:sink.write(json.loads(msg.payload.decode('utf-8')))
                except (ValueError,UnicodeError,sqlite3.Error,OSError) as e:print(f'[MQTT] invalid/unwritable telemetry: {e}',flush=True)
            client.on_connect=on_connect;client.on_message=on_message;client.reconnect_delay_set(1,30)
            client.connect_async(args.broker,args.port,keepalive=30);client.loop_start();stop.wait()
    finally:
        if client:client.disconnect();client.loop_stop()
        if web:web.shutdown();web.server_close()
        sink.close()
    return 0

if __name__=='__main__':raise SystemExit(main())
