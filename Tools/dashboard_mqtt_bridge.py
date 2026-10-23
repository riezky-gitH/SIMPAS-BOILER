# ==============================================================================
# SCADA BOILER PLTU - MQTT TO CSV & WEBSOCKET LOGGER BRIDGE
# Workshop Mikrokontroler Lanjut dan Teknik Antarmuka (WMLTA)
# ==============================================================================
import json
import time
import csv
import os
from datetime import datetime

try:
    import paho.mqtt.client as mqtt
except ImportError:
    print('Paho-MQTT belum terpasang. Jalankan: pip install paho-mqtt')

BROKER_HOST = 'localhost' # Atau alamat IP PC di jaringan WiFi lokal
BROKER_PORT = 1883
TOPIC_TELEMETRY = 'boiler/telemetry/#'
TOPIC_COMMAND = 'boiler/command'
CSV_FILENAME = 'telemetri_boiler_realtime.csv'

def init_csv():
    if not os.path.exists(CSV_FILENAME):
        with open(CSV_FILENAME, mode='w', newline='', encoding='utf-8') as f:
            writer = csv.writer(f)
            writer.writerow(['Timestamp', 'Node_ID', 'Tekanan_Bar', 'Suhu_C', 'Katup_Status', 'Buzzer_Status', 'Alarm_State'])
        print(f'[LOGGER] File {CSV_FILENAME} siap.')

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print(f'[MQTT] Berhasil terhubung ke broker {BROKER_HOST}:{BROKER_PORT}')
        client.subscribe(TOPIC_TELEMETRY)
        print(f'[MQTT] Berlangganan topik: {TOPIC_TELEMETRY}')
    else:
        print(f'[MQTT] Gagal terhubung, kode status: {rc}')

def on_message(client, userdata, msg):
    try:
        payload = msg.payload.decode('utf-8')
        data = json.loads(payload)
        now_str = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
        
        node_id = data.get('node', 'UNKNOWN')
        pressure = data.get('pressure', 0.0)
        temp = data.get('temp', 0.0)
        valve = data.get('valve', 'CLOSED')
        buzzer = data.get('buzzer', False)
        state = data.get('state', 'NORMAL')
        
        print(f'[{now_str}] [{node_id}] P={pressure:.2f} Bar | T={temp:.1f} C | Katup={valve} | State={state}')
        
        with open(CSV_FILENAME, mode='a', newline='', encoding='utf-8') as f:
            writer = csv.writer(f)
            writer.writerow([now_str, node_id, pressure, temp, valve, buzzer, state])
            
    except Exception as e:
        print(f'[ERROR] Gagal parsing pesan: {e} | Raw: {msg.payload}')

if __name__ == '__main__':
    init_csv()
    print('Starting MQTT Bridge... Buka dashboard_scada_boiler.html di browser Anda.')
