# SIMPAS-BOILER
## Sistem Monitoring & Proteksi Automatis Solenoid Boiler

<p align="center">
  <img src="https://img.shields.io/badge/Platform-ESP32--S3-blue?style=for-the-badge&logo=espressif" alt="ESP32-S3"/>
  <img src="https://img.shields.io/badge/Platform-Arduino%20Nano-teal?style=for-the-badge&logo=arduino" alt="Arduino Nano"/>
  <img src="https://img.shields.io/badge/Architecture-DCS%20%2F%20SCADA-orange?style=for-the-badge" alt="DCS/SCADA"/>
  <img src="https://img.shields.io/badge/License-MIT-green?style=for-the-badge" alt="MIT License"/>
</p>

Sistem kendali terdistribusi (*Distributed Control System / DCS*) dan akuisisi data (*SCADA*) untuk monitoring tekanan & suhu dengan kontrol solenoid otomatis pada **Boiler PLTU** (Pembangkit Listrik Tenaga Uap).

> **Mata Kuliah:** Workshop Mikrokontroler Lanjut dan Teknik Antarmuka (WMLTA) — Semester 5

---

## Arsitektur Sistem

```
+-----------------------------------------------------------------------------+
|                    TIER 3: WEB SCADA DASHBOARD                              |
|  Browser Laptop / Smartphone via WiFi AP (http://192.168.4.1)               |
+------------------------------------+----------------------------------------+
                                     |  WiFi Hotspot (802.11 b/g/n)
+------------------------------------v----------------------------------------+
|                TIER 2: EDGE GATEWAY & CONCENTRATOR                          |
|                ESP32-S3 DevKitC-1 (N16R8)                                   |
|  - Layar TFT 1.47" ST7789 SPI (40 MHz, Zero-Flicker)                       |
|  - Dual Hardware UART Hub                                                   |
|  - WiFi AP Standalone + MQTT Gateway                                        |
|  - Local Historian (16 MB Flash / 8 MB OPI PSRAM)                           |
+----------------+-------------------------------+----------------------------+
                 | UART1 (via R-divider)          | UART2 (via R-divider)
+----------------v--------------+  +--------------v--------------------------+
|   TIER 1: SLAVE 1             |  |   TIER 1: SLAVE 2                      |
|   Arduino Nano (Chamber 1)    |  |   Arduino Nano (Chamber 2)             |
| - MAX6675 Thermocouple K-Type |  | - MAX6675 Thermocouple K-Type          |
| - Pressure Transmitter 0-10 B |  | - Pressure Transmitter 0-10 Bar        |
| - Relay Solenoid Valve (A-LOW)|  | - Relay Solenoid Valve (Active-LOW)     |
| - Buzzer + LED + LCD 16x2 I2C |  | - Buzzer + LED + LCD 16x2 I2C          |
+-------------------------------+  +-----------------------------------------+
```

### Prinsip Desain Utama
- **Autonomous Safety**: Setiap Slave Arduino Nano mengambil keputusan keselamatan secara independen (250 ms loop). Jika kabel UART ke Master putus, Slave tetap membuka katup purge dan membunyikan alarm secara mandiri.
- **Prioritas Aktuator**: `ALARM (otomatis) > MANUAL PURGE > TUTUP`. Katup tidak dapat dipaksa tutup selama kondisi bahaya masih berlangsung.
- **Hysteresis Anti-Chattering**: Tekanan reset di `Pth - 0.2 Bar`, suhu reset di `Tth - 3°C`.
- **Fail-Safe Sensor Putus**: Termokopel terlepas → pembacaan otomatis `999.9°C` → katup terbuka + alarm menyala.

---

## Struktur Repository

```
SIMPAS-BOILER/
├── Firmware/
│   ├── Master_ESP32S3/
│   │   ├── Master_ESP32S3.ino       # Firmware Master (Dual UART, TFT, WiFi AP, Web SCADA, MQTT)
│   │   └── dashboard_html.h         # Web Dashboard HTML (embedded PROGMEM, ~58 KB)
│   ├── Slave1_Nano/
│   │   └── Slave1_Nano.ino          # Firmware Slave Boiler Chamber 1
│   ├── Slave2_Nano/
│   │   └── Slave2_Nano.ino          # Firmware Slave Boiler Chamber 2
│   └── WebDashboard/
│       └── index.html               # Web Dashboard SCADA (standalone browser version)
├── Docs/
│   ├── GUIDEBOOK.md                 # Buku panduan lengkap proyek (7 BAB)
│   └── HARDWARE_PINOUT.md           # Pemetaan pin, library, & konfigurasi upload
├── Tools/
│   ├── dashboard_mqtt_bridge.py     # Python MQTT-to-CSV data logger bridge
│   └── dashboard_scada_boiler.html  # Dashboard SCADA standalone (buka langsung di browser)
├── .gitignore
├── LICENSE
└── README.md                        # Dokumen ini
```

---

## Spesifikasi Hardware

| Komponen | Jumlah | Fungsi |
|---|:---:|---|
| ESP32-S3 DevKitC-1 **N16R8** | 1 | Master Controller (16 MB Flash, 8 MB OPI PSRAM) |
| Arduino Nano ATmega328P | 2 | Slave Controller Boiler 1 & 2 |
| TFT 1.47" ST7789 SPI | 1 | Layar Master (320×172 Landscape) |
| LCD 16x2 I2C (PCF8574) | 2 | Layar Lokal Slave |
| MAX6675 + Thermocouple K-Type | 2 | Sensor Suhu (0 – 1024°C) |
| Pressure Transmitter 0-10 Bar | 2 | Sensor Tekanan (0.5V – 4.5V analog) |
| Modul Relay 5V Active-LOW | 2 | Penggerak Katup Solenoid Purge |
| Buzzer Aktif 5V + Driver NPN | 2 | Alarm Suara Kontinu |
| Push Button Momentary | 9 | Tombol Operasional (3 Master + 3×2 Slave) |

---

## Pemetaan Pin Utama

### Master (ESP32-S3)
| Sinyal | GPIO | Keterangan |
|---|:---:|---|
| VR Setpoint | 4 | Potensio threshold (ADC) |
| PB1 / PB2 / PB3 | 5 / 6 / 7 | Halaman TFT / Mute All / Emergency Purge |
| TFT (DC/CS/MOSI/SCK/RST) | 9/10/11/12/14 | SPI 40 MHz |
| UART1 (RX/TX → Slave 1) | 16 / 17 | Via voltage divider 1k/2.2k |
| UART2 (RX/TX → Slave 2) | 8 / 18 | Via voltage divider 1k/2.2k |

### Slave (Arduino Nano) — Identik untuk kedua Slave
| Sinyal | Pin | Keterangan |
|---|:---:|---|
| SoftSerial RX / TX | D2 / D3 | Komunikasi ke Master |
| LED / Buzzer | D4 / D5 | Indikator & alarm |
| PB1 / PB2 / PB3 | D6 / D7 / D8 | Purge / Mute / LCD Page |
| Relay Solenoid | D9 | Active-LOW (LOW=ON, HIGH=OFF) |
| MAX6675 (CS/SO/SCK) | D10/D12/**A0** | SCK dipindah ke A0 (D13 rusak) |
| Pressure Transmitter | **A6** | ADC-only pin (dipindah dari A0) |

> Detail lengkap: [`Docs/HARDWARE_PINOUT.md`](Docs/HARDWARE_PINOUT.md)

---

## Instalasi & Upload Firmware

### Prasyarat
- **Arduino IDE 2.x** (≥ 2.3.2)
- **ESP32 Board Manager** ≥ 3.0.x ([Panduan install](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html))
- **Library Arduino:**
  - `LiquidCrystal_I2C` — Frank de Brabander
  - `Adafruit GFX Library` — Adafruit
  - `Adafruit ST7735 and ST7789 Library` — Adafruit
  - `PubSubClient` — Nick O'Leary

### Upload ke Arduino Nano (Slave)
```
Board       : Arduino Nano
Processor   : ATmega328P (Old Bootloader)
Port        : COMx
```
1. Buka `Firmware/Slave1_Nano/Slave1_Nano.ino` → Upload ke Nano Boiler 1
2. Buka `Firmware/Slave2_Nano/Slave2_Nano.ino` → Upload ke Nano Boiler 2

### Upload ke ESP32-S3 (Master)
```
Board             : ESP32S3 Dev Module
USB CDC On Boot   : Enabled
Flash Size        : 16MB (128Mb)
Partition Scheme  : 16M Flash (3MB APP / 9.9MB FATFS)
PSRAM             : OPI PSRAM
Upload Speed      : 921600
```
Buka `Firmware/Master_ESP32S3/Master_ESP32S3.ino` → Upload

---

## Akses Web SCADA Dashboard

Sistem bersifat **100% standalone** — tidak butuh internet atau router eksternal.

1. Nyalakan alat (catu daya 5V).
2. Hubungkan laptop/smartphone ke WiFi:
   - **SSID:** `SCADA_BOILER_PLTU`
   - **Password:** `admin1234`
3. Buka browser → ketik:
   ```
   http://192.168.4.1
   ```

### Fitur Dashboard
- ⚡ **Industrial SVG Gauges** — Radial gauge tekanan & suhu real-time
- 📈 **Live Strip Chart** — Grafik tren 60 FPS dengan canvas rendering
- 🎛️ **Quick Action Bar** — Manual Purge, Mute Buzzer, Emergency All-Purge
- 📋 **SCADA Event Logger** — Log otomatis alarm dengan timestamp

---

## REST API

| Endpoint | Method | Deskripsi |
|---|:---:|---|
| `/` | GET | Web Dashboard SCADA |
| `/api/data` | GET | JSON telemetri kedua boiler + setpoint + uptime |
| `/api/command?node=0&action=PURGE,1` | GET | Kirim perintah ke slave (node: 0=all, 1, 2) |

**Action whitelist:** `PURGE,1` · `PURGE,0` · `MUTE,1`

---

## Protokol Komunikasi Serial

### Telemetri (Slave → Master, setiap 500 ms)
```
#SLV<id>,<pressure>,<temp>,<src_p>,<src_t>,<valve>,<buzzer>,<state>*
```
Contoh: `#SLV1,4.20,165.2,REAL,REAL,OPEN,1,ALARM*`

### Perintah (Master → Slave)
```
#CMD<target>,SET,<p_threshold>,<t_threshold>*     ← Heartbeat 1 dtk
#CMD<target>,PURGE,<0|1>*                         ← Buka/tutup katup
#CMD<target>,MUTE,1*                              ← Mute buzzer
```
Perintah PURGE/MUTE dikirim burst 3× (interval 150 ms) untuk keandalan SoftwareSerial.

---

## Dokumentasi Lengkap

📖 **[Buku Panduan Lengkap (7 BAB)](Docs/GUIDEBOOK.md)** — Arsitektur DCS, wiring, protokol, instalasi, operasional HMI, 8 skenario pengujian, dan troubleshooting.

---

## Lisensi

Proyek ini dilisensikan di bawah [MIT License](LICENSE).

---

<p align="center">
  <b>SIMPAS-BOILER</b> — Sistem Monitoring & Proteksi Automatis Solenoid Boiler<br/>
  Workshop Mikrokontroler Lanjut dan Teknik Antarmuka (WMLTA) — Semester 5
</p>
