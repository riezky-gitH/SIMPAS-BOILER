# BUKU PANDUAN LENGKAP (PROJECT GUIDEBOOK)
## SISTEM MONITORING TEKANAN & SUHU DENGAN KONTROL SOLENOID OTOMATIS PADA BOILER PLTU
**Mata Kuliah:** Workshop Mikrokontroler Lanjut dan Teknik Antarmuka (WMLTA) - Semester 5  
**Arsitektur:** Distributed Control System (DCS) / SCADA Miniatur Industri  
**Hardware Utama:** ESP32-S3 DevKitC-1 N16R8 (Master - 16MB Flash, 8MB OPI PSRAM) & 2x Arduino Nano ATmega328P (Slaves)  

---

## DAFTAR ISI
1. [BAB 1: Arsitektur & Prinsip Kerja Sistem](#bab-1-arsitektur--prinsip-kerja-sistem)
2. [BAB 2: Spesifikasi & Pengkabelan Perangkat Keras (Hardware Wiring)](#bab-2-spesifikasi--pengkabelan-perangkat-keras-hardware-wiring)
3. [BAB 3: Protokol Komunikasi & Antarmuka Data](#bab-3-protokol-komunikasi--antarmuka-data)
4. [BAB 4: Panduan Instalasi Software & Flashing Firmware](#bab-4-panduan-instalasi-software--flashing-firmware)
5. [BAB 5: Panduan Operasional HMI & Web SCADA Dashboard](#bab-5-panduan-operasional-hmi--web-scada-dashboard)
6. [BAB 6: Prosedur Pengujian, Skenario Demo, & Commissioning](#bab-6-prosedur-pengujian-skenario-demo--commissioning)
7. [BAB 7: Matriks Diagnosis Masalah & Troubleshooting](#bab-7-matriks-diagnosis-masalah--troubleshooting)

---

## BAB 1: Arsitektur & Prinsip Kerja Sistem

### 1.1 Latar Belakang & Deskripsi Sistem
Pada instalasi Pembangkit Listrik Tenaga Uap (PLTU), boiler bekerja di bawah tekanan tinggi (*high pressure*) dan suhu ekstrem (*high temperature*). Kegagalan mitigasi lonjakan tekanan (*overpressure*) atau suhu (*overtemperature*) dapat mengakibatkan ledakan ruang bakar (*boiler explosion*).

Proyek ini merekayasa sistem kendali terdistribusi (*Distributed Control System / DCS*) dan akuisisi data (*SCADA*) 3-tingkat untuk memantau dua unit bejana boiler (**Boiler Chamber 1** dan **Boiler Chamber 2**) secara otonom, terintegrasi, dan memiliki keandalan tinggi (*high availability*).

```
+----------------------------------------------------------------------------------+
|                             LEVEL 3: SUPERVISORY                                 |
|  Web SCADA Dashboard (Laptop/Smartphone via WiFi AP 192.168.4.1 & MQTT Broker)    |
+----------------------------------------------------------------------------------+
                                        ▲ HTTP JSON / WebSocket / MQTT
                                        ▼
+----------------------------------------------------------------------------------+
|                        LEVEL 2: CONCENTRATOR & LOCAL HMI                         |
|                    ESP32-S3 DevKitC-1 N8R2 (Master Controller)                   |
|   - Layar TFT 1.47" ST7789 (SPI 40MHz)      - Potensio RV5 Setting Setpoint      |
|   - Tombol PB1..PB3 Navigasi & Safety       - Dual Hardware UART Concentrator    |
+----------------------------------------------------------------------------------+
                 ▲ UART1 (115200/9600)                     ▲ UART2 (115200/9600)
                 ▼                                         ▼
+----------------------------------+      +----------------------------------------+
|        LEVEL 1: SLAVE 1          |      |            LEVEL 1: SLAVE 2            |
|     Arduino Nano (Chamber 1)     |      |        Arduino Nano (Chamber 2)        |
| - Sensor Suhu MAX6675 (K-Type)   |      | - Sensor Suhu MAX6675 (K-Type)         |
| - Transmitter Tekanan (0-10 Bar) |      | - Transmitter Tekanan (0-10 Bar)       |
| - Relay Katup Solenoid (Purge)   |      | - Relay Katup Solenoid (Purge)         |
| - Buzzer & LED Aktuator          |      | - Buzzer & LED Aktuator                |
| - LCD 16x2 I2C & Tombol Lokal    |      | - LCD 16x2 I2C & Tombol Lokal          |
+----------------------------------+      +----------------------------------------+
                 │                                         │
+----------------------------------+      +----------------------------------------+
|   LEVEL 0: SENSOR & AKTUATOR     |      |       LEVEL 0: SENSOR & AKTUATOR       |
| Sensor MAX6675, Transmitter,     |      | Sensor MAX6675, Transmitter,           |
| Solenoid Valve, Modul Relay 5V   |      | Solenoid Valve, Modul Relay 5V         |
+----------------------------------+      +----------------------------------------+
```

### 1.2 Filosofi Kendali Otonom (Fail-Safe & Anti-Tabrakan Fitur)
1. **Otonomi Tingkat Slave (Decentralized Safety)**:
   Setiap Arduino Nano mengambil keputusan mitigasi secara independen pada *loop* kecepatan tinggi (250 ms). Jika kabel komunikasi serial ke ESP32 terputus, Nano tetap dapat membuka katup purge dan membunyikan alarm sendiri secara lokal.
2. **Prioritas Aktuator Katup**:
   $$\text{Kondisi Katup Buka} = \text{Alarm Otomatis} \lor \text{Manual Purge}$$
   - **ALARM (Tertinggi)**: Jika tekanan atau suhu melebihi ambang batas, katup **wajib membuka** dan tidak dapat dipaksa tutup secara manual selama bahaya masih berlangsung.
   - **MANUAL PURGE**: Operator dapat membuka katup untuk perawatan atau uji coba. Katup menutup kembali saat manual dinonaktifkan.
   - **NORMAL (Terendah)**: Katup tertutup rapat.
3. **Mekanisme Histeresis (Anti-Hunting/Chattering)**:
   Untuk mencegah katup dan relay membuka-menutup cepat di sekitar garis batas ambang (*flickering*):
   - **Ambang Batas Tekanan**: Reset alarm jika $P < (P_{\text{threshold}} - 0.20\text{ Bar})$.
   - **Ambang Batas Suhu**: Reset alarm jika $T < (T_{\text{threshold}} - 3.0^\circ\text{C})$.
4. **Latching Alarm & Smart Mute**:
   - Buzzer berbunyi kontinu saat terjadi ALARM.
   - Penekanan tombol Mute (**PB2 Slave** atau **PB2 Master** atau tombol Web) mematikan suara buzzer untuk insiden tersebut.
   - Katup tetap membuka demi keselamatan.
   - Jika kondisi bahaya baru muncul setelah alarm sebelumnya normal, status *mute* otomatis di-reset sehingga buzzer berbunyi kembali.
5. **Fail-Safe Sensor MAX6675 Putus (Open-Circuit Detection)**:
   Jika kabel termokopel putus atau terlepas, bit D2 pembacaan MAX6675 akan bernilai `1` (*open-circuit*). Slave Nano secara otomatis mendeteksi kegagalan ini, menetapkan pembacaan ke $999.9^\circ\text{C}$ (*Fail-Safe Danger*), mengaktifkan katup pengaman, dan membunyikan buzzer.

---

## BAB 2: Spesifikasi & Pengkabelan Perangkat Keras (Hardware Wiring)

### 2.1 Bill of Materials (Daftar Komponen Utama)

| No | Modul / Komponen | Jumlah | Fungsi / Deskripsi |
|---|---|:---:|---|
| 1 | **ESP32-S3 DevKitC-1 N8R2** | 1 | Master Controller, Web SCADA Server, Dual UART, SPI ST7789 |
| 2 | **Arduino Nano ATmega328P** | 2 | Slave 1 & Slave 2 Controller |
| 3 | **Modul Layar TFT 1.47" ST7789 SPI** | 1 | Display Master (320x172 Landscape, 262k RGB) |
| 4 | **Layar LCD 16x2 + I2C Backpack (PCF8574)** | 2 | Display Lokal Slave 1 & 2 (Alamat I2C: `0x27`) |
| 5 | **Modul Termokopel MAX6675 + K-Type** | 2 | Sensor Suhu Ruang Boiler ($0 - 1024^\circ\text{C}$) |
| 6 | **Pressure Transmitter 0-10 Bar (G1/4")** | 2 | Sensor Tekanan Industri (Output analog: $0.5\text{V} - 4.5\text{V}$) |
| 7 | **Modul Relay 5V 1-Channel (Active-LOW)** | 2 | Penggerak Katup Solenoid Purge Boiler |
| 8 | **Katup Solenoid Air/Uap 12V/220V** | 2 | Aktuator Pembuang Tekanan Lebih (*Pressure Relief*) |
| 9 | **Buzzer Aktif 5V + Driver NPN (2N2222/BC547)** | 2 | Alarm Suara Kontinu Slave |
| 10 | **LED Indikator 5mm Merah** | 2 | Indikator Fisik Status Katup Solenoid |
| 11 | **Potensiometer 10k Rotary (RV1, RV2, RV5)** | 5 | Simulator Tekanan, Simulator Suhu, & Setpoint Master |
| 12 | **Saklar Geser SPDT** | 4 | Selector Mode Real Sensor vs Potensio Simulator |
| 13 | **Push Button Momentary Panel** | 9 | Tombol Operasional (3 Master, 3 Slave 1, 3 Slave 2) |
| 14 | **Resistor Pembagi Tegangan (1kΩ & 2.2kΩ)** | 2 pasang | Level Shifter 5V (Nano TX) ke 3.3V (ESP32-S3 RX) |

---

### 2.2 Pemetaan Pin Master (ESP32-S3 DevKitC-1 N8R2)

| Label Sinyal KiCad | Pin GPIO ESP32-S3 | Mode I/O | Keterangan & Proteksi Level Tegangan |
| :--- | :---: | :---: | :--- |
| **VR_Setpoint (RV5)** | **GPIO 04** | Analog IN | Wiper Potensiometer Setting Batas Bahaya ($0 - 3.3\text{V}$, ADC1_CH3) |
| **PB1_Master (SW11)** | **GPIO 05** | Digital IN | Tombol Ganti Halaman Layar TFT (`INPUT_PULLUP`) |
| **PB2_Master (SW12)** | **GPIO 06** | Digital IN | Tombol Global Mute / Silence Alarm Seluruh Slave (`INPUT_PULLUP`) |
| **PB3_Master (SW13)** | **GPIO 07** | Digital IN | Tombol Toggle Emergency All-Purge (`INPUT_PULLUP`) |
| **Slave2TX_ESP** | **GPIO 08** | Digital IN | Serial2 RX dari Slave 2 (Lewat Resistor Divider: R7=1k / R8=2.2k) |
| **TFT_DC** | **GPIO 09** | Digital OUT | Data / Command Control Layar TFT ST7789 |
| **TFT_CS** | **GPIO 10** | Digital OUT | SPI Chip Select Layar TFT ST7789 |
| **TFT_MOSI (SDA)** | **GPIO 11** | SPI MOSI | Hardware SPI Master Out (40 MHz) |
| **TFT_SCK (SCL)** | **GPIO 12** | SPI SCK | Hardware SPI Clock (40 MHz) |
| **TFT_RST** | **GPIO 14** | Digital OUT | Hardware Reset Layar TFT ST7789 |
| **Slave1TX_ESP** | **GPIO 16** | Digital IN | Serial1 RX dari Slave 1 (Lewat Resistor Divider: R5=1k / R6=2.2k) |
| **ESPTX_Slave1** | **GPIO 17** | Digital OUT | Serial1 TX langsung ke Pin D2 Arduino Nano 1 ($3.3\text{V} \rightarrow 5\text{V}$ aman) |
| **ESPTX_Slave2** | **GPIO 18** | Digital OUT | Serial2 TX langsung ke Pin D2 Arduino Nano 2 ($3.3\text{V} \rightarrow 5\text{V}$ aman) |
| **Catu Daya 5V & 3.3V** | **5V0 & 3V3** | Power | Input 5V dari Regulator PCB; 3.3V untuk VCC TFT & Potensio RV5 |
| **GND** | **GND** | Ground | Wajib menyatu (Common Ground) dengan ground seluruh board |

---

### 2.3 Pemetaan Pin Slave 1 & Slave 2 (Arduino Nano ATmega328P)
*(Konfigurasi pin hardware identik untuk Slave 1 dan Slave 2)*

| Komponen / Sinyal | Pin Arduino Nano | Tipe I/O | Keterangan & Karakteristik Fisik |
| :--- | :---: | :---: | :--- |
| **SoftSerial RX** | **Pin D2** | Digital IN | Menerima data dari ESP32 Master (TX ESP32 $3.3\text{V}$) |
| **SoftSerial TX** | **Pin D3** | Digital OUT | Mengirim telemetri ke ESP32 (Lewat pembagi tegangan 1k/2.2k) |
| **LED Indikator Katup** | **Pin D4** | Digital OUT | Menyala merah terang saat Katup BUKA, padam saat TUTUP |
| **Driver Buzzer** | **Pin D5** | Digital OUT | Mengontrol basis transistor NPN (Bunyi kontinu saat ALARM) |
| **PB1_Slave (SW1/SW8)** | **Pin D6** | Digital IN | Tombol Manual Purge Solenoid Valve (`INPUT_PULLUP`) |
| **PB2_Slave (SW2/SW9)** | **Pin D7** | Digital IN | Tombol Mute Alarm Suara Buzzer (`INPUT_PULLUP`) |
| **PB3_Slave (SW3/SW10)**| **Pin D8** | Digital IN | Tombol Navigasi Halaman LCD 16x2 (`INPUT_PULLUP`) |
| **Relay Solenoid Valve** | **Pin D9** | Digital OUT | Modul Relay Active-LOW (LOW = ON / Katup Buka, HIGH = OFF / Tutup) |
| **MAX6675 CS** | **Pin D10** | Digital OUT | Chip Select SPI Modul MAX6675 |
| **SPDT 1 (Selector Suhu)** | **Pin D11** | Digital IN | LOW = Sensor Suhu Real (MAX6675), HIGH = Potensio VR2 (Sim) |
| **MAX6675 SO (MISO)** | **Pin D12** | Digital IN | Serial Data Out dari Modul MAX6675 |
| **MAX6675 SCK (Clock)** | **Pin A0** | Digital OUT | Clock SPI MAX6675 (**Dipindah ke A0 karena pin D13 fisik rusak**) |
| **VR2 (Potensio Suhu)** | **Pin A1** | Analog IN | Simulasi Suhu ($0 - 5.0\text{V} \rightarrow 0 - 200.0^\circ\text{C}$) |
| **VR1 (Potensio Tekanan)**| **Pin A2** | Analog IN | Simulasi Tekanan ($0 - 5.0\text{V} \rightarrow 0 - 10.00\text{ Bar}$) |
| **SPDT 2 (Selector Tekanan)**| **Pin A3** | Digital IN | LOW = Transmitter Real (A6), HIGH = Potensio VR1 (Sim) |
| **LCD 16x2 SDA** | **Pin A4** | I2C Data | Jalur data I2C ke backpack PCF8574 |
| **LCD 16x2 SCL** | **Pin A5** | I2C Clock | Jalur clock I2C ke backpack PCF8574 |
| **Pressure Real Transmitter**| **Pin A6** | Analog IN | Input ADC Khusus Nano ($0.5\text{V} - 4.5\text{V} \rightarrow 0 - 10.00\text{ Bar}$) |

---

### 2.4 Rangkaian Proteksi Khusus

#### A. Pembagi Tegangan UART (5V ke 3.3V)
Pin GPIO ESP32-S3 tidak toleran terhadap tegangan 5V (*5V non-tolerant*). Jalur transmisi data dari Arduino Nano (TX = 5V) menuju ESP32-S3 (RX = 3.3V) wajib melewati rangkaian *resistor voltage divider*:
```
Nano TX (D3, 5V) ───[ 1.0 kΩ ]───┬───> Ke ESP32-S3 RX (GPIO 16 / GPIO 8)
                                 │
                             [ 2.2 kΩ ]
                                 │
                                GND
```
Tegangan input ESP32:
$$V_{\text{ESP32}} = 5\text{V} \times \left(\frac{2.2\text{k}\Omega}{1.0\text{k}\Omega + 2.2\text{k}\Omega}\right) = 3.43\text{V} \quad (\text{Sangat aman bagi ESP32-S3})$$

Sebaliknya, sinyal dari ESP32-S3 TX (3.3V) ke Arduino Nano RX (D2) dapat langsung dihubungkan karena logika HIGH $3.3\text{V}$ melampaui tegangan ambang batas logika HIGH ATmega328P ($V_{IH} = 0.6 \times V_{CC} = 3.0\text{V}$).

#### B. Pengkabelan Modul Relay (Active-LOW) & Anti-Glitch
1. Hubungkan pin sinyal `IN` Modul Relay ke pin **D9** Arduino Nano, `VCC` ke **5V**, dan `GND` ke **GND**.
2. Di firmware Slave, status pin diinisialisasi ke `HIGH` sebelum dijadikan `OUTPUT` untuk mencegah relay berbunyi klik sesaat (*boot glitch*) saat mikro pertama kali dinyalakan:
   ```cpp
   digitalWrite(PIN_RELAY_VALVE, HIGH); // RELAY_OFF
   pinMode(PIN_RELAY_VALVE, OUTPUT);
   ```
3. Hubungkan terminal Solenoid Valve:
   - Satu kabel Solenoid ke catu daya katup (+12V atau Line AC).
   - Satu kabel Solenoid ke terminal **NO (Normally Open)** Relay.
   - Terminal **COM (Common)** Relay ke ground / netral sumber catu daya katup.

---

## BAB 3: Protokol Komunikasi & Antarmuka Data

### 3.1 Protokol Serial Dual-UART (Master $\leftrightarrow$ Slaves)
Komunikasi serial berjalan dengan *baudrate* **9600 bps** (SoftwareSerial pada Nano dan Hardware Serial1 / Serial2 pada ESP32-S3).

```
                      Paket Telemetri Tiap 500ms
  Slave Nano ─────────────────────────────────────────► ESP32-S3 Master
              #SLVx,Pressure,Temp,SrcP,SrcT,Valve,Buzzer,State*\n

                      Paket Heartbeat Tiap 1000ms
  ESP32-S3 Master ─────────────────────────────────────► Slave Nano
                           #CMD0,SET,Pth,Tth*\n

                      Paket Perintah Kontrol (Burst 3x)
  ESP32-S3 Master ─────────────────────────────────────► Slave Nano
                        #CMDx,PURGE,1* atau #CMDx,MUTE,1*
```

#### A. Format Telemetri (Slave $\rightarrow$ Master)
Dikirim setiap **500 ms** secara otomatis oleh masing-masing Slave:
```text
#SLV<id>,<pressure>,<temperature>,<src_p>,<src_t>,<valve>,<buzzer>,<state>*
```
- **`<id>`**: `1` (Boiler 1) atau `2` (Boiler 2).
- **`<pressure>`**: Nilai tekanan dalam satuan Bar (contoh: `3.45`).
- **`<temperature>`**: Nilai suhu dalam derajat Celsius (contoh: `142.5`).
- **`<src_p>`**: `REAL` (sensor transmitter) atau `SIM` (potensio simulator VR1).
- **`<src_t>`**: `REAL` (sensor termokopel MAX6675) atau `SIM` (potensio simulator VR2).
- **`<valve>`**: `OPEN` (katup buang terbuka) atau `CLOSED` (katup tertutup).
- **`<buzzer>`**: `1` (alarm suara menyala) atau `0` (mati / di-mute).
- **`<state>`**: `NORMAL`, `WARNING`, atau `ALARM`.

*Contoh Paket:*
```text
#SLV1,4.20,165.2,REAL,REAL,OPEN,1,ALARM*
```

#### B. Format Perintah (Master $\rightarrow$ Slave)
- **Sinkronisasi Ambang Batas (Heartbeat berkala 1 detik)**:
  `#CMD0,SET,4.0,160*` (Target ID 0 = Broadcast ke kedua slave).
- **Buka Katup Tertentu (Manual Purge On)**:
  `#CMD1,PURGE,1*` (Untuk Slave 1) atau `#CMD2,PURGE,1*` (Untuk Slave 2).
- **Tutup Katup Tertentu (Manual Purge Off)**:
  `#CMD1,PURGE,0*` atau `#CMD2,PURGE,0*`.
- **Mute Suara Buzzer**:
  `#CMD0,MUTE,1*` (Mute semua slave) atau `#CMD1,MUTE,1*`.

#### C. Fitur Idempotent Command Queue (Burst 3x)
Karena *SoftwareSerial* pada Arduino Nano bersifat setengah-dupleks (*half-duplex*) dan dapat melewatkan karakter saat sedang memproses transmisi telemetri, ESP32 Master dilengkapi mekanisme antrean pengiriman ulang:
- Setiap perintah penting (`PURGE` dan `MUTE`) otomatis dikirimkan sebanyak **3 kali** berturut-turut pada interval $0\text{ ms}$, $150\text{ ms}$, dan $300\text{ ms}$.

---

### 3.2 REST API Server ESP32-S3
Web Server ESP32 berjalan di Port 80 dan menyediakan antarmuka API berbasis JSON untuk integrasi aplikasi pihak ketiga:

| Endpoint | Metode | Parameter | Deskripsi Fungsi |
|---|:---:|---|---|
| `/` | `GET` | - | Mengembalikan halaman antarmuka Web SCADA Dashboard |
| `/api/data` | `GET` | - | Mengembalikan data telemetri kedua boiler, setpoint, client AP, dan uptime dalam format JSON |
| `/api/command` | `GET` | `node=0..2`, `action=...` | Mengirim perintah kendali (`action`: `PURGE,1`, `PURGE,0`, `MUTE,1`) |

*Contoh Respons `/api/data`:*
```json
{
  "setpoint_p": 4.00,
  "setpoint_t": 160.0,
  "purge_all": false,
  "ap_clients": 1,
  "uptime": 1284,
  "slave1": {
    "pressure": 3.42,
    "temp": 145.2,
    "src_p": "REAL",
    "src_t": "REAL",
    "valve": "CLOSED",
    "buzzer": false,
    "state": "NORMAL",
    "connected": true
  },
  "slave2": {
    "pressure": 4.15,
    "temp": 162.8,
    "src_p": "SIM",
    "src_t": "SIM",
    "valve": "OPEN",
    "buzzer": true,
    "state": "ALARM",
    "connected": true
  }
}
```

---

## BAB 4: Panduan Instalasi Software & Flashing Firmware

### 4.1 Persiapan Software & Library Arduino IDE
Gunakan **Arduino IDE 2.x** (versi 2.3.2 atau lebih baru direkomendasikan):

1. **Board Manager ESP32**:
   Buka **File ➔ Preferences**, tambahkan URL Board Manager:
   ```text
   https://espressif.github.io/arduino-esp32/package_esp32_index.json
   ```
   Buka **Tools ➔ Board ➔ Boards Manager...**, cari `esp32` oleh Espressif Systems dan install versi **3.0.x atau 3.3.x**.
2. **Library yang Wajib Diinstall** (via **Tools ➔ Manage Libraries...**):
   - **`LiquidCrystal_I2C`** oleh Frank de Brabander (untuk LCD 16x2 Slave).
   - **`Adafruit GFX Library`** oleh Adafruit (untuk grafis TFT Master).
   - **`Adafruit ST7789 and ST7735 Library`** oleh Adafruit (untuk driver layar 1.47" ST7789).
   - **`PubSubClient`** oleh Nick O'Leary (untuk protokol MQTT Master).
   *(Catatan: Library `Servo.h` sudah tidak diperlukan lagi karena katup digerakkan menggunakan modul relay digital).*

---

### 4.2 Langkah Upload ke Arduino Nano (Slave 1 & Slave 2)

#### A. Upload Slave 1 (Boiler Chamber 1):
1. Buka file `Firmware/Slave1_Nano/Slave1_Nano.ino`.
2. Hubungkan Arduino Nano 1 ke laptop menggunakan kabel Mini-USB.
3. Atur konfigurasi menu **Tools**:
   - **Board**: `Arduino Nano`
   - **Processor**: `ATmega328P (Old Bootloader)` *(Jika gagal upload, ubah ke `ATmega328P`)*
   - **Port**: Sesuaikan COM Port Nano Anda (misal `COM3`).
4. Klik tombol **Upload**.

#### B. Upload Slave 2 (Boiler Chamber 2):
1. Buka file `Firmware/Slave2_Nano/Slave2_Nano.ino`.
2. Hubungkan Arduino Nano 2 ke laptop.
3. Atur konfigurasi yang sama, lalu klik **Upload**.

---

### 4.3 Langkah Upload ke ESP32-S3 Master

1. Buka file `Firmware/Master_ESP32S3/Master_ESP32S3.ino`.
2. Hubungkan kabel USB Type-C ke port **USB (Native USB / CDC)** pada ESP32-S3 DevKitC-1.
3. Atur menu **Tools** pada Arduino IDE dengan konfigurasi wajib berikut:
   - **Board**: `ESP32S3 Dev Module`
   - **USB CDC On Boot**: **`Enabled`** *(Kritis: agar Serial Monitor & komunikasi native USB aktif)*
   - **CPU Frequency**: `240MHz (WiFi)`
   - **Flash Mode**: `QIO 80MHz`
   - **Flash Size**: `16MB (128Mb)` *(Sesuai modul fisik N16R8)*
   - **Partition Scheme**: `16M Flash (3MB APP/9.9MB FATFS)` atau `16M Flash (3MB APP/9.9MB SPIFFS)`
   - **PSRAM**: `OPI PSRAM` *(Wajib diaktifkan untuk chip Octal PSRAM 8MB pada N16R8)*
   - **Upload Speed**: `921600`
   - **Port**: Sesuaikan COM Port ESP32-S3.
4. Klik tombol **Upload**.
   *(Tips: Jika ESP32-S3 tidak merespons download mode, tahan tombol **BOOT**, tekan tombol **RESET** sekali, lalu lepaskan tombol BOOT).*

---

## BAB 5: Panduan Operasional HMI & Web SCADA Dashboard

### 5.1 Antarmuka Layar LCD 16x2 Slave
Tekan tombol **PB3 pada Slave** untuk berpindah mode tampilan LCD:

```
[ MODE 0: MONITORING UTAMA ]      [ MODE 1: STATUS AKTUATOR ]       [ MODE 2: STATUS LINK ]
+----------------+               +----------------+                +----------------+
|B1 P:3.42B [R] N|               |B1 V:CLSD BZ:OFF|                |B1 LINK: ONLINE |
|   T:145.2C[R]  |               |SP:4.0B  TH:160C|                |MASTER: ACTIVE  |
+----------------+               +----------------+                +----------------+
```
- **`[R]`**: Data berasal dari sensor fisik asli (**R**EAL).
- **`[S]`**: Data berasal dari simulator potensiometer (**S**IM).
- **`N` / `W` / `A`**: Status **N**ormal, **W**arning, atau **A**larm.
- **`V:CLSD` / `V:OPEN`**: Posisi katup solenoid tertutup atau terbuka.
- **`BZ:ON` / `BZ:OFF`**: Status buzzer alarm berbunyi atau diam.

---

### 5.2 Antarmuka Layar TFT 1.47" ST7789 Master
Layar TFT Master menyajikan visualisasi data modern 320x172 landscape bebas kedip (*zero-flicker differential rendering*). Tekan tombol **PB1 Master** untuk berpindah halaman:

#### Halaman 1: Dual Overview (Visualisasi Berdampingan)
Menampilkan kartu status Boiler 1 dan Boiler 2 secara simultan:
- Kotak Kartu Boiler 1 (Kiri) & Boiler 2 (Kanan).
- Nilai Tekanan (Cyan), Suhu (Amber), Status Sumber Sensor (`R`/`S`).
- Lencana Status Dinamis: `NORM` (Hijau), `WARN` (Kuning), `ALARM` (Merah Berkedip), `DISC` (Abu-abu / Terputus).
- Status Katup Solenoid (`VALVE: CLOSED` hijau atau `VALVE: OPEN` merah).
- Baris Footer: Menampilkan nilai Setpoint saat ini dan status perintah darurat.

#### Halaman 2: Detail Progress Bar & Aktuator
Menyajikan bilah proporsional (*analog progress bar*) tekanan dan suhu terhadap batas ambang batas bahaya (*threshold*).

#### Halaman 3: Status Jaringan & Sistem
Menampilkan status SSID Hotspot WiFi AP, alamat IP Master (`192.168.4.1`), jumlah perangkat klien yang terhubung, dan waktu operasi (*uptime*).

---

### 5.3 Fungsi Tombol Fisik Master & Slave

```
+-----------------------------------------------------------------------------------+
| TOMBOL MASTER (ESP32-S3):                                                         |
| - PB1 (GPIO 05) : Mengganti halaman tampilan Layar TFT Master (Hal 1 -> 2 -> 3)    |
| - PB2 (GPIO 06) : Global Alarm Silence / Mematikan suara buzzer di seluruh Slave  |
| - PB3 (GPIO 07) : Emergency All-Purge Toggle (Buka/Tutup semua katup sekaligus)   |
+-----------------------------------------------------------------------------------+
| TOMBOL SLAVE (ARDUINO NANO 1 & 2):                                                |
| - PB1 (Pin D6)  : Manual Purge Katup Lokal (Toggle buka/tutup katup solenoid)     |
| - PB2 (Pin D7)  : Local Alarm Mute (Mematikan suara buzzer pada slave tersebut)   |
| - PB3 (Pin D8)  : Mengganti halaman mode LCD 16x2 lokal (Mode 0 -> 1 -> 2)       |
+-----------------------------------------------------------------------------------+
```

---

### 5.4 Akses Web SCADA Dashboard Standalone

Sistem ini bersifat **100% Standalone**, tidak membutuhkan router eksternal atau koneksi internet.

```
                    +---------------------------+
                    |    ESP32-S3 Master        |
                    | Access Point:             |
                    | SSID: SCADA_BOILER_PLTU   |
                    | Pass: admin1234           |
                    | IP  : 192.168.4.1         |
                    +-------------▲-------------+
                                  │ WiFi 2.4 GHz
            ┌─────────────────────┴─────────────────────┐
            │                                           │
+-----------▼-------------+                 +-----------▼-------------+
|    Laptop / PC Lab      |                 |    Smartphone Tablet    |
| Buka: http://192.168.4.1|                 | Buka: http://192.168.4.1|
+-------------------------+                 +-------------------------+
```

1. Nyalakan alat. Tunggu 3 detik hingga modul ESP32 selesai melakukan inisialisasi.
2. Pada laptop atau smartphone, sambungkan koneksi WiFi ke jaringan:
   - **SSID**: `SCADA_BOILER_PLTU`
   - **Password**: `admin1234`
3. Buka browser (Google Chrome, Microsoft Edge, atau Safari) dan masukkan URL:
   ```text
   http://192.168.4.1
   ```
4. **Fitur Web SCADA Interaktif**:
   - **Industrial Gauges**: Radial gauge SVG interaktif untuk tekanan (0-10 Bar) dan termometer suhu (0-200°C) dengan zona warna dinamis.
   - **Real-Time Strip Chart**: Grafik tren berjalan 60 FPS untuk melihat dinamika kenaikan tekanan dan suhu.
   - **Quick Action Control Bar**: Tombol *Manual Purge B1*, *Manual Purge B2*, *Silence Buzzer*, dan *Emergency All-Purge*.
   - **Live SCADA Event Logger**: Pencatat log riwayat insiden alarm otomatis beserta stempel waktu operasional (*system uptime*).

---

## BAB 6: Prosedur Pengujian, Skenario Demo, & Commissioning

Gunakan skenario pengujian bertahap berikut untuk demonstrasi praktikum atau pengujian laboratorium:

### Skenario 1: Verifikasi Booting & Dual-UART Link Handshake
1. Berikan catu daya 5V ke terminal PCB.
2. Amati layar LCD 16x2 pada kedua Slave:
   - Menampilkan `PLTU BOILER SLV1` / `PLTU BOILER SLV2` selama 1 detik, lalu masuk ke halaman monitoring.
3. Amati layar TFT 1.47" Master:
   - Kartu Boiler 1 dan Boiler 2 akan beralih dari status `DISC` menjadi `NORM` warna hijau dalam waktu kurang dari 2 detik setelah menerima telemetri serial pertama.

### Skenario 2: Uji Simulator Sensor Potensiometer
1. Geser saklar **SPDT 1 ke posisi SIM** dan **SPDT 2 ke posisi SIM** pada Slave 1.
2. Pada LCD Slave 1, label sensor akan berubah menjadi `P:...B [S]` dan `T:...C [S]`.
3. Putar potensiometer **VR1** (Tekanan):
   - Nilai tekanan pada LCD Slave 1, layar TFT Master, dan Web SCADA berubah secara responsif dari $0.00\text{ Bar}$ hingga $10.00\text{ Bar}$.
4. Putar potensiometer **VR2** (Suhu):
   - Nilai suhu berubah dari $0.0^\circ\text{C}$ hingga $200.0^\circ\text{C}$.

### Skenario 3: Uji Pengaturan Ambang Batas Master (Potensio RV5)
1. Putar potensiometer **RV5** pada board Master ESP32-S3.
2. Amati nilai setpoint pada baris status footer layar TFT Master:
   - Setpoint tekanan berubah dalam rentang $1.0\text{ Bar} - 8.0\text{ Bar}$.
   - Setpoint suhu berubah dalam rentang $100^\circ\text{C} - 200^\circ\text{C}$.
3. Buka LCD Slave halaman 2 (tekan PB3):
   - Nilai `SP:` pada Slave otomatis tersinkronisasi dalam waktu kurang dari 1 detik via perintah `$CMD0,SET,...`.

### Skenario 4: Uji Alarm Overpressure / Overtemperature & Auto-Purge
1. Atur setpoint master ke `4.0 Bar` dan `160 C`.
2. Putar potensio simulator VR1 hingga tekanan mencapai `4.5 Bar`:
   - **Relay Solenoid (D9)** seketika aktif berbunyi *klik*, kontak NO menutup.
   - **LED Merah D4** menyala terang menandakan katup membuka.
   - **Buzzer D5** berbunyi nyaring secara kontinu.
   - Layar TFT Master berubah menjadi warna merah terang dengan lencana `ALARM`.
   - Web SCADA menampilkan banner alarm darurat dan mencatat insiden ke dalam tabel log.

### Skenario 5: Uji Fitur Alarm Mute & Retriggering
1. Dalam kondisi ALARM di Skenario 4, tekan tombol **PB2 pada Slave 1** (atau **PB2 Master** atau tombol *Mute* di web):
   - **Buzzer langsung berhenti bersuara** (*Silence*).
   - **Relay katup solenoid tetap membuka** dan LED D4 tetap menyala (mengutamakan keselamatan boiler).
2. Putar VR1 turun hingga tekanan berada di bawah ambang histeresis ($< 3.80\text{ Bar}$):
   - Katup solenoid otomatis menutup, LED D4 padam, status kembali `NORM`.
3. Naikkan kembali tekanan di atas ambang batas ($> 4.0\text{ Bar}$):
   - Buzzer **kembali berbunyi otomatis** (membuktikan status mute otomatis di-reset untuk insiden baru).

### Skenario 6: Uji Emergency All-Purge
1. Dalam kondisi normal, tekan tombol **PB3 pada Master** (atau tombol *Emergency All-Purge* di Web SCADA):
   - Kedua modul relay pada Slave 1 dan Slave 2 seketika aktif membuka kedua katup solenoid.
   - Label tombol pada layar TFT Master berubah menjadi `TUTUP KATUP`.
2. Tekan kembali PB3 Master:
   - Kedua katup menutup kembali secara aman.

### Skenario 7: Uji Proteksi Kegagalan Sensor (MAX6675 Open-Circuit Failsafe)
1. Pindahkan saklar SPDT 1 ke posisi **REAL**.
2. Lepaskan salah satu kabel probe termokopel Tipe-K dari terminal screw MAX6675:
   - Slave Nano seketika mendeteksi bit *open-circuit*.
   - Nilai suhu pada LCD berubah menjadi `999.9C` dengan peringatan error.
   - Relay katup pembuang otomatis membuka dan buzzer berbunyi demi mencegah ledakan akibat hilangnya sinyal pemantauan suhu.

### Skenario 8: Uji Watchdog Link Timeout (Kabel Serial Terputus)
1. Cabut kabel jalur TX atau RX antara Slave 1 dan Master:
   - Setelah 3 detik tanpa paket serial, Master ESP32 secara cerdas menandai status Boiler 1 sebagai **`OFFLINE`** / **`DISC`** (bukan menampilkan data usang/beku).
   - Slave 1 pada halaman LCD mode 3 menampilkan `MASTER: --` namun tetap menjalankan kontrol logika keselamatan secara otonom.
2. Pasang kembali kabel serial:
   - Koneksi pulih kembali (*auto-reconnect*) dalam tempo 500 ms tanpa perlu me-reset board.

---

## BAB 7: Matriks Diagnosis Masalah & Troubleshooting

| Gejala Masalah | Kemungkinan Penyebab Utama | Solusi Tindakan Perbaikan |
|---|---|---|
| **Relay katup menyala terus tanpa henti** | Modul relay adalah tipe *Active-LOW*, namun sebelumnya dipicu oleh sinyal servo PWM. | Pastikan firmware Slave sudah menggunakan versi terbaru (library `Servo.h` dibuang, menggunakan `digitalWrite(PIN_RELAY_VALVE, RELAY_OFF)` saat inisialisasi). Jika menggunakan modul *Active-HIGH*, ubah `#define RELAY_ON HIGH` dan `#define RELAY_OFF LOW`. |
| **Layar LCD 16x2 hanya menampilkan kotak hitam atau blank** | 1. Kontras LCD belum diatur.<br>2. Alamat I2C salah (biasanya `0x27` atau `0x3F`). | 1. Putar potensiometer trimpot biru di belakang backpack I2C menggunakan obeng kecil.<br>2. Jalankan sketch I2C Scanner untuk memastikan alamat modul PCF8574. |
| **Suhu MAX6675 bernilai 0.0°C atau 999.9°C** | 1. Pin SCK masih tertancap di D13 yang rusak.<br>2. Polaritas kabel termokopel (+/-) terbalik.<br>3. Probe tidak terpasang kencang. | 1. Pastikan pin SCK MAX6675 terhubung ke pin **A0** Arduino Nano.<br>2. Tukar kabel merah (+) dan biru/kuning (-) termokopel.<br>3. Kencangkan baut terminal screw. |
| **Master menampilkan status `DISC` / `OFFLINE` terus** | 1. Jalur RX dan TX terbalik.<br>2. Ground (GND) antara Nano dan ESP32 tidak tersambung.<br>3. Nilai resistor divider salah. | 1. Pastikan Nano TX (D3) $\rightarrow$ ESP32 RX (GPIO 16/8) via divider, dan ESP32 TX (GPIO 17/18) $\rightarrow$ Nano RX (D2).<br>2. Sambungkan seluruh pin GND menjadi satu (*common ground*). |
| **Web Dashboard tidak dapat diakses (IP 192.168.4.1 tidak terbuka)** | 1. Laptop/HP belum terhubung ke WiFi `SCADA_BOILER_PLTU`.<br>2. IP bentrok dengan adapter jaringan lain. | 1. Hubungkan ke SSID `SCADA_BOILER_PLTU` (password: `admin1234`).<br>2. Matikan sementara mobile data / koneksi Ethernet lain pada laptop saat pengujian. |
| **Layar TFT Master 1.47" berwarna gelap / tidak menyala** | 1. Pin RST / CS / DC tidak sesuai definisi.<br>2. Tegangan catu daya TFT kurang dari 3.3V. | Periksa sambungan: DC ke GPIO 09, CS ke GPIO 10, MOSI ke GPIO 11, SCK ke GPIO 12, RST ke GPIO 14, VCC ke pin 3V3, GND ke GND. |
| **WiFi ESP32 putus-nyambung terus menerus** | Flag `ENABLE_STA_ROUTER` aktif (`true`) tetapi router WiFi luar tidak ditemukan, sehingga ESP32 terus scanning channel. | Buka `Master_ESP32S3.ino`, pastikan `const bool ENABLE_STA_ROUTER = false;` saat beroperasi mandiri (*standalone hotspot*). |

---

## BAB 8: Penutup & Standar Pemeliharaan

Sistem SCADA Boiler PLTU ini telah dirancang memenuhi kriteria rancang bangun teknik antarmuka industri modern:
- **Modularitas Tinggi**: Pemisahan tegas antara tugas akuisisi sensor (Slave) dan konsentrasi data serta penyajian grafis (Master).
- **Keamanan Berlapis (*Defense-in-Depth*)**: Kontrol darurat dapat diinisiasi dari level tombol fisik lokal lapangan, tombol master panel kontrol, maupun remote Web SCADA.
- **Efisiensi Memori**: Bebas dari alokasi memori dinamis (*String heap fragmentation*) pada mikrokontroler Slave untuk menjamin stabilitas operasi jangka panjang (*zero crash*).

*Dokumentasi ini disusun sebagai acuan teknis komprehensif mata kuliah Workshop Mikrokontroler Lanjut dan Teknik Antarmuka (WMLTA).*
