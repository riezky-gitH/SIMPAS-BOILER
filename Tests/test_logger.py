import csv
import importlib.util
import json
from pathlib import Path
import sqlite3
import tempfile
import unittest
import threading
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
from urllib.request import Request,urlopen
from urllib.error import HTTPError

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('logger',ROOT/'Tools/logger_v3.py')
logger=importlib.util.module_from_spec(spec);spec.loader.exec_module(logger)
vspec=importlib.util.spec_from_file_location('validator',ROOT/'Tools/validate_history.py')
validator=importlib.util.module_from_spec(vspec);vspec.loader.exec_module(validator)

class LoggerTests(unittest.TestCase):
    def fixture(self):
        return dict(epoch=0,boot=42,uptime=10,slave1=dict(connected=True,pressure=2.5,temp=None,quality_p='GOOD',quality_t='FAULT',relay='ON',buzzer=True,state='FAULT',config={'version':4}),slave2=dict(connected=False,pressure=9,temp=150,relay='CLOSED',buzzer=False))
    def test_no_fabricated_reading(self):
        rows=list(logger.rows(self.fixture()));self.assertEqual(rows[0][5],2.5);self.assertIsNone(rows[0][6]);self.assertIsNone(rows[0][1]);self.assertIsNone(rows[1][5]);self.assertEqual(rows[1][9],'UNKNOWN');self.assertIsNone(rows[1][10]);self.assertEqual(rows[1][11],'OFFLINE')
    def test_csv_sqlite_and_rotation(self):
        with tempfile.TemporaryDirectory() as t:
            csv_path=Path(t)/'history.csv';db_path=Path(t)/'history.sqlite3';sink=logger.Sink(csv_path,db_path,rotate_mb=0.0001,keep=2)
            sink.write(self.fixture());sink.write(self.fixture());sink.close()
            self.assertTrue(csv_path.with_suffix('.csv.1').exists())
            with csv_path.open(newline='',encoding='utf-8') as f:
                rows=list(csv.reader(f));self.assertEqual(rows[0],logger.COLUMNS);self.assertEqual(len(rows),3)
            db=sqlite3.connect(db_path);self.assertEqual(db.execute('select count(*) from telemetry').fetchone()[0],4);db.close()
    def test_partition_fills_n16r8_without_overlap(self):
        parts=[]
        for row in csv.reader((ROOT/'Firmware/Master_ESP32S3/partitions.csv').read_text().splitlines()):
            if not row or row[0].startswith('#'):continue
            parts.append((row[0],int(row[3],0),int(row[4],0)))
        for a,b in zip(parts,parts[1:]):self.assertLessEqual(a[1]+a[2],b[1])
        self.assertEqual(parts[-1][2],10*1024*1024);self.assertEqual(parts[-1][1]+parts[-1][2],16*1024*1024)
    def test_generated_assets_match(self):
        fw=ROOT/'Firmware'
        for target in ['Master_ESP32S3','Slave1_Nano','Slave2_Nano']:
            for name in ['Protocol.h','Control.h','Button.h']:
                self.assertEqual((fw/'Shared'/name).read_bytes(),(fw/target/name).read_bytes())
        header=(fw/'Master_ESP32S3/dashboard_html.h').read_text(encoding='utf-8');html=(fw/'WebDashboard/index.html').read_text(encoding='utf-8')
        self.assertEqual(header.split('R"SCADA_HTML(\n')[1].split('\n)SCADA_HTML"')[0],html)
    def test_history_crc_detects_corruption(self):
        self.assertEqual(validator.crc16(b'123456789'),0x29b1)
        with tempfile.TemporaryDirectory() as t:
            p=Path(t)/'history.csv';payload=b'0,42,5,200,1000,32771,0,0,0,200,1000,32771,0,0,0'
            row=payload+b','+f'{validator.crc16(payload):04X}'.encode()+b'\n';p.write_bytes(validator.HEADER+row)
            self.assertEqual(validator.validate(p),1)
            p.write_bytes(validator.HEADER+row.replace(b'200',b'201',1))
            with self.assertRaises(ValueError):validator.validate(p)
    def test_history_readable_wib_and_legacy_validation(self):
        with tempfile.TemporaryDirectory() as t:
            p=Path(t)/'history.csv'
            for epoch,stamp in [(1704067200,b'2024-01-01 07:00:00'),(0,b'UNSYNCED')]:
                payload=str(epoch).encode()+b',42,5,200,1000,32771,0,0,0,200,1000,32771,0,0,0,'+stamp
                row=payload+b','+f'{validator.crc16(payload):04X}'.encode()+b'\n'
                p.write_bytes(validator.NEW_HEADER+row);self.assertEqual(validator.validate(p),1)
            payload=b'1704067200,42,5,200,1000,32771,0,0,0,200,1000,32771,0,0,0,2024-01-01 00:00:00'
            p.write_bytes(validator.NEW_HEADER+payload+b','+f'{validator.crc16(payload):04X}'.encode()+b'\n')
            with self.assertRaises(ValueError):validator.validate(p)
    def test_readable_history_headers_units_and_written_status(self):
        with tempfile.TemporaryDirectory() as t:
            p=Path(t)/'history.csv'
            rows=[['2026-10-27 12:00:00','2.61','80.7','2.16','52.5','NORMAL','WASPADA'],['Belum sinkron','','','3.50','180.0','TERPUTUS','ALARM: Suhu tinggi; Gangguan sensor tekanan']]
            def save():
                with p.open('w',encoding='utf-8',newline='') as f:
                    writer=csv.writer(f);writer.writerow(validator.READABLE_HEADER);writer.writerows(rows)
            save();self.assertEqual(validator.validate(p),2)
            rows[0][1]='261';save()
            with self.assertRaises(ValueError):validator.validate(p)
            rows[0][1]='2.61';rows[0][5]='4';save()
            with self.assertRaises(ValueError):validator.validate(p)
            rows[0][5]='NORMAL';rows[0].pop();save()
            with self.assertRaises(ValueError):validator.validate(p)
    def test_proxy_preserves_authentication_and_checks_origin(self):
        class Upstream(BaseHTTPRequestHandler):
            def log_message(self,*args):pass
            def do_POST(self):
                self.rfile.read(int(self.headers.get('Content-Length','0')))
                ok=self.headers.get('Authorization')=='Bearer fixture'
                body=json.dumps({'origin':self.headers.get('Origin')}).encode();self.send_response(202 if ok else 401);self.send_header('Content-Type','application/json');self.send_header('Content-Length',str(len(body)));self.end_headers();self.wfile.write(body)
        upstream=ThreadingHTTPServer(('127.0.0.1',0),Upstream);master=f'http://127.0.0.1:{upstream.server_port}'
        proxy=ThreadingHTTPServer(('127.0.0.1',0),logger.proxy_handler(master,ROOT/'Firmware/WebDashboard/index.html'));base=f'http://127.0.0.1:{proxy.server_port}'
        threads=[threading.Thread(target=s.serve_forever,daemon=True)for s in (upstream,proxy)]
        for t in threads:t.start()
        try:
            r=Request(base+'/api/command',data=b'{}',headers={'Authorization':'Bearer fixture','Origin':base},method='POST')
            with urlopen(r,timeout=3) as response:self.assertEqual(response.status,202);self.assertEqual(json.load(response)['origin'],master)
            with self.assertRaises(HTTPError) as error:urlopen(Request(base+'/api/command',data=b'{}',method='POST'),timeout=3)
            self.assertEqual(error.exception.code,401)
            error.exception.close()
            with self.assertRaises(HTTPError) as error:urlopen(Request(base+'/api/command',data=b'{}',headers={'Origin':'http://untrusted.example'},method='POST'),timeout=3)
            self.assertEqual(error.exception.code,403)
            error.exception.close()
        finally:
            for s in (proxy,upstream):s.shutdown();s.server_close()

if __name__=='__main__':unittest.main()
