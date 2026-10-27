"""Local UI fixture only. Simulated data is prominently labelled; no hardware access."""
import json
import math
from pathlib import Path
import time
import copy
from urllib.parse import urlsplit,parse_qs
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
ROOT=Path(__file__).resolve().parents[1]
START=time.monotonic()

def node(i):
    return dict(connected=True,state='NORMAL',quality_p='GOOD',quality_t='GOOD',pressure=2.35+i*.2,temp=118+i*7,
        src_p='SIM',src_t='SIM',relay='OFF',valve_feedback='NOT_INSTALLED',buzzer=False,causes=0,cause_text='NONE',
        muted=0,acknowledged=0,trip=False,local=False,remote=False,global_=False,age_ms=60,command_id=0,command_status='NONE',
        config=dict(pressure=4,temp=160,version=1,live=0,latch=1,zero_mv=500,span_mv=4000,vcc_mv=5000),packets=100,crc_errors=0,slave_errors=0,boot=i+1,raw_adc=450)
NODES=[node(0),node(1)]
for n in NODES:n['global']=n.pop('global_')

def snapshot(scene='normal'):
    data=dict(schema=3,boot=123,epoch=int(time.time()),uptime=int(time.monotonic()-START),ap_clients=1,purge_all=False,
        slave1=NODES[0],slave2=NODES[1],knob_pressure=4.5,knob_temp=170,free_heap=180000,free_psram=4500000,reset_reason=1,
        history=dict(mounted=True,ready=True,psram=True,capacity_records=87381,flash_bytes=520000,budget_bytes=9437184,
            pending_est_bytes=560,write_error=False,paused_by_snapshot=False,dropped=0,corrupt=0,rotated_segments=0,flushes=30,
            last_flush_age_ms=20000,usb_snapshot=False,usb_bytes=0,peaks=[dict(pressure=3.7,temp=145),dict(pressure=3.9,temp=147)],alarms=[]))
    data=copy.deepcopy(data)
    if scene in ('warning','mixed'):data['slave1'].update(pressure=3.7,state='WARNING')
    if scene in ('alarm','mixed'):data['slave2'].update(temp=165,state='ALARM',causes=2,cause_text='TEMP_HIGH',buzzer=True,trip=True,relay='ON')
    if scene=='trip':data['slave2'].update(state='ALARM',trip=True,relay='ON')
    if scene=='muted':data['slave2'].update(temp=165,state='ALARM',causes=2,cause_text='TEMP_HIGH',buzzer=False,muted=2,trip=True,relay='ON')
    return data

class Handler(BaseHTTPRequestHandler):
    def log_message(self,*_):pass
    def reply(self,data,status=200):
        body=json.dumps(data).encode();self.send_response(status);self.send_header('Content-Type','application/json');self.send_header('Content-Length',str(len(body)));self.end_headers();self.wfile.write(body)
    def do_GET(self):
        url=urlsplit(self.path);scene=parse_qs(url.query).get('scene',['normal'])[0]
        if scene not in ('normal','warning','alarm','mixed','trip','muted','offline'):scene='normal'
        if url.path=='/':
            html=(ROOT/'Firmware/WebDashboard/index.html').read_text(encoding='utf-8').replace('<body>', '<body class="preview-mode"><div class="preview-banner">PREVIEW / DATA SIMULASI — tanpa perangkat terhubung · token 123456</div>')
            html=html.replace("api('/api/data')",f"api('/api/data?scene={scene}')")
            body=html.encode();self.send_response(200);self.send_header('Content-Type','text/html; charset=utf-8');self.send_header('Content-Length',str(len(body)));self.end_headers();self.wfile.write(body)
        elif url.path=='/api/data':self.reply({'error':'Simulated offline'} if scene=='offline' else snapshot(scene),503 if scene=='offline' else 200)
        elif self.path.startswith('/api/history'):
            now=int(time.time());self.reply({'records':[[now-300+i,123,i,235+int(20*math.sin(i/30)),1180,3,1,255+int(20*math.sin(i/25)),1250,3,1]for i in range(300)]})
        elif self.path=='/api/events':self.reply({'events':[dict(id=1,epoch=int(time.time()),boot=123,uptime=1,node=0,type='PREVIEW_FIXTURE',detail='UI test only, no physical readings')]})
        else:self.reply({'error':'Unknown fixture path'},404)
    def do_POST(self):
        payload=self.rfile.read(int(self.headers.get('Content-Length','0')))
        if self.headers.get('Authorization')!='Bearer 123456':self.reply({'error':'Fixture operator token required'},401);return
        if self.path=='/api/command':
            d=json.loads(payload)
            for i,n in enumerate(NODES):
                if d['node'] in (0,i+1):n['command_id']+=1;n['command_status']='APPLIED'
            self.reply({'status':'QUEUED','id1':1,'id2':1},202)
        elif self.path=='/api/config':self.reply({'status':'QUEUED'},202)
        elif self.path in ('/api/time','/api/usb-refresh'):self.reply({'status':'APPLIED'})
        else:self.reply({'error':'Fixture export intentionally not connected to device storage'},503)

if __name__=='__main__':
    print('UI fixture preview: http://127.0.0.1:8765',flush=True)
    ThreadingHTTPServer(('127.0.0.1',8765),Handler).serve_forever()
