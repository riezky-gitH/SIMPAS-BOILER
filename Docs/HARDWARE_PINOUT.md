# DOKUMENTASI LENGKAP FIRMWARE & SISTEM SCADA BOILER PLTU
**Mata Kuliah:** Workshop Mikrokontroler Lanjut dan Teknik Antarmuka (WMLTA) - Semester 5  
**Proyek:** Sistem Monitoring Tekanan dan Suhu dengan Kontrol Solenoid Otomatis Pada Boiler PLTU  

---

## 1. Struktur Folder Firmware
```text
d:\Kuliah\Semester 5\WMLTA\Firmware/
├── Master_ESP32S3/
│   └── Master_ESP32S3.ino    # Firmware ESP32-S3 (Dual UART, TFT ST7789, WiFi AP, Web SCADA, MQTT)
├── Slave1_Nano/
│   └── Slave1_Nano.ino       # Firmware Arduino Nano Boiler 1 (MAX6675, Pressure Transmitter, LCD 16x2, Servo, Buzzer)
├── Slave2_Nano/
│   └── Slave2_Nano.ino       # Firmware Arduino Nano Boiler 2 (Sama persis dengan Slave 1, Node ID = 2)
├── WebDashboard/
│   └── index.html            # Web Dashboard SCADA Standalone
└── README.md                 # Panduan instalasi, pinout, dan pengujian
```

---

## 2. Pemetaan Pin Hardware (100% Sesuai Skematik KiCad)

### A. Master (ESP32-S3 DevKitC-1 N8R2)
| Komponen / Sinyal | Pin ESP32-S3 | Keterangan & Proteksi Level Tegangan |
| :--- | :--- | :--- |
| **VR_Setpoint (RV5)** | **GPIO 04** | Potensio Setting Ambang Batas (0 - 3.3V, ADC1_CH3) |
| **PB1_Master (SW11)** | **GPIO 05** | Tombol Ganti Halaman Layar TFT (`INPUT_PULLUP`) |
| **PB2_Master (SW12)** | **GPIO 06** | Tombol Global Mute / Silence Alarm (`INPUT_PULLUP`) |
| **PB3_Master (SW13)** | **GPIO 07** | Tombol Emergency All-Purge (`INPUT_PULLUP`) |
| **Slave2TX_ESP** | **GPIO 08** | UART2 RX dari Slave 2 (Lewat pembagi tegangan R7=1k / R8=2.2k) |
| **TFT_DC** | **GPIO 09** | Layar TFT 1.47" ST7789 SPI Data/Command (Landscape 320x172) |
| **TFT_CS** | **GPIO 10** | Layar TFT 1.47" ST7789 SPI Chip Select |
| **TFT_MOSI** | **GPIO 11** | Layar TFT 1.47" ST7789 SPI MOSI (SDA) |
| **TFT_SCK** | **GPIO 12** | Layar TFT 1.47" ST7789 SPI Clock (SCL) |
| **TFT_RST** | **GPIO 14** | Layar TFT 1.47" ST7789 SPI Reset |
| **Slave1TX_ESP** | **GPIO 16** | UART1 RX dari Slave 1 (Lewat pembagi tegangan R5=1k / R6=2.2k) |
| **ESPTX_Slave1** | **GPIO 17** | UART1 TX ke Pin D2 Nano 1 (Langsung 3.3V ke 5V aman) |
| **ESPTX_Slave2** | **GPIO 18** | UART2 TX ke Pin D2 Nano 2 (Langsung 3.3V ke 5V aman) |
| **Catu Daya 5V & 3.3V** | **5V0 & 3V3** | Input 5V dari terminal J9, 3.3V untuk layar & potensio RV5 |

---

### B. Slave 1 & Slave 2 (Arduino Nano ATmega328P)
| Komponen / Sinyal | Pin Arduino Nano | Keterangan & Fungsi |
| :--- | :--- | :--- |
| **SoftSerial RX** | **Pin D2** | Terima data dari ESP32 Master (TX ESP32) |
| **SoftSerial TX** | **Pin D3** | Kirim data ke ESP32 Master (ke divider 1k/2.2k) |
| **LED Indikator** | **Pin D4** | LED Merah Indikator Aktuator (Menyala saat katup BUKA, Mati saat TUTUP) |
| **Buzzer Alarm** | **Pin D5** | Bunyi KONTINU saat ALARM (Tekanan/Suhu melebihi batas, dimatikan via PB2) |
| **PB1_Slave (SW1/SW8)** | **Pin D6** | Tombol Manual Purge Solenoid Valve (`INPUT_PULLUP`) |
| **PB2_Slave (SW2/SW9)** | **Pin D7** | Tombol Mematikan Suara Buzzer / Mute Alarm (`INPUT_PULLUP`) |
| **PB3_Slave (SW3/SW10)**| **Pin D8** | Tombol Ganti Mode Tampilan LCD 16x2 (`INPUT_PULLUP`) |
| **Relay Solenoid** | **Pin D9** | Driver Relay Solenoid Valve (Active-LOW: LOW=ON/Buka, HIGH=OFF/Tutup) |
| **MAX6675 CS** | **Pin D10**| Chip Select Termokopel Suhu Tipe-K |
| **SPDT 1 (Suhu)** | **Pin D11**| LOW = Sensor Asli (MAX6675), HIGH = Simulasi Potensio VR2 |
| **MAX6675 SO** | **Pin D12**| MISO Data Out Termokopel |
| **SPDT 2 (Tekanan)** | **Pin A3** | LOW = Sensor Asli Transmitter, HIGH = Simulasi Potensio VR1 |
| **MAX6675 SCK**| **Pin A0** | Clock SPI MAX6675 (DIPINDAH ke A0 karena pin D13 fisik rusak) |
| **VR2 (Potensio Suhu)** | **Pin A1** | Simulasi Suhu (0 - 5.0V = 0 - 200 °C) |
| **VR1 (Potensio Tekanan)**| **Pin A2** | Simulasi Tekanan (0 - 5.0V = 0 - 10 Bar) |
| **Pressure Real (Transmitter)**| **Pin A6** | Sensor Transmitter Tekanan (0.5V - 4.5V = 0 - 10 Bar, dipindah dari A0 ke A6) |
| **LCD 16x2 SDA** | **Pin A4** | I2C Data LCD (PCF8574 Backpack) |
| **LCD 16x2 SCL** | **Pin A5** | I2C Clock LCD (PCF8574 Backpack) |
| **Input 5V** | **Pin 27 (+5V)**| Input catu daya 5V langsung (Pin 30/VIN dikosongkan) |

---

## 3. Daftar Library Arduino yang Diperlukan

Buka **Arduino IDE ➔ Tools ➔ Manage Libraries...**, lalu install library berikut:

### Untuk Arduino Nano (Slave 1 & Slave 2):
1. **LiquidCrystal_I2C** (oleh Frank de Brabander)
2. *Library bawaan Arduino (tidak perlu install):* `SoftwareSerial`, `Wire`. *(Tidak lagi butuh `Servo.h`)*

### Untuk ESP32-S3 (Master):
1. **Adafruit GFX Library** (oleh Adafruit)
2. **Adafruit ST7789 and ST7735 Library** (oleh Adafruit)
3. **PubSubClient** (oleh Nick O'Leary) — untuk protokol MQTT
4. *Library bawaan ESP32 core:* `WiFi`, `WebServer`, `SPI`.

---

## 4. Pengaturan Konfigurasi Upload Board di Arduino IDE

### A. Saat Upload ke Arduino Nano:
* **Board:** `Arduino Nano`
* **Processor:** `ATmega328P (Old Bootloader)` *(atau `ATmega328P` tergantung klon chip Nano)*
* **Port:** Sesuaikan COM Port Nano Anda.

### B. Saat Upload ke ESP32-S3:
* **Board:** `ESP32S3 Dev Module`
* **USB CDC On Boot:** `Enabled`
* **CPU Frequency:** `240MHz (WiFi)`
* **Flash Mode:** `QIO 80MHz`
* **Flash Size:** `16MB (128Mb)` *(Sesuai modul N16R8)*
* **Partition Scheme:** `16M Flash (3MB APP/9.9MB FATFS)` atau `16MB (3MB APP/9.9MB SPIFFS)`
* **PSRAM:** `OPI PSRAM` *(Wajib untuk chip Octal PSRAM 8MB pada N16R8)*
* **Upload Speed:** `921600`
* **Port:** Sesuaikan COM Port ESP32 Anda.

---

## 5. Cara Menjalankan & Membuka Web Dashboard SCADA

Sistem ini didesain **100% mandiri (Standalone)** tanpa wajib menyalakan laptop/PC terus-menerus:

1. **Akses Langsung via WiFi ESP32 (Paling Praktis):**
   * Nyalakan alat. ESP32 Master akan memancarkan WiFi Hotspot sendiri:
     * **SSID:** `SCADA_BOILER_PLTU`
     * **Password:** `admin1234`
   * Hubungkan HP atau Laptop Anda ke WiFi tersebut.
   * Buka browser (Chrome / Edge) dan ketik alamat IP:
     ```text
     http://192.168.4.1
     ```
   * Web Dashboard SCADA interaktif langsung tampil secara *real-time*! Anda bisa memantau grafik, nilai tekanan/suhu, dan menekan tombol *Manual Purge* atau *Emergency All-Purge*.

2. **Akses via Jaringan Lab & MQTT (Opsional):**
   * Jika ingin mencatat data ke file CSV di PC, jalankan script Python yang sudah tersedia:
     ```bash
     python d:\Kuliah\Semester 5\WMLTA\dashboard_mqtt_bridge.py
     ```
   * Buka file `dashboard_scada_boiler.html` di browser Anda.

---

## 6. Prosedur Pengujian Tanpa Sensor Asli (Fitur SPDT Simulasi)

Jika Anda belum memasang sensor fisik (Pressure Transmitter & MAX6675):
1. Geser saklar **SPDT 1 ke posisi SIM** ➔ Putar potensio **VR2** untuk mengubah nilai suhu (0 - 200 °C).
2. Geser saklar **SPDT 2 ke posisi SIM** ➔ Putar potensio **VR1** untuk mengubah nilai tekanan (0 - 10 Bar).
3. Putar **Potensio RV5 di Master** untuk mengatur ambang batas (*setpoint threshold*).
4. Amati saat tekanan/suhu melebihi ambang batas:
   * Katup servo di Slave otomatis membuka 90 derajat (*Purge*).
   * **LED Indikator Aktuator (D4) langsung MENYALA** menandakan katup terbuka.
   * **Buzzer berbunyi KONTINU (terus-menerus)** tanpa jeda.
   * Layar LCD Slave menampilkan `ALARM!` dan status `V:ALRM`.
   * Layar TFT Master berubah warna menjadi **Merah (ALARM)**.
5. Tekan tombol **PB2 di Slave (atau PB2 di Master)** untuk **mematikan suara buzzer** (*Mute*).
6. Tekan tombol **PB1 di Slave** untuk uji coba buka/tutup katup manual (*Manual Purge*). Saat dibuka, LED D4 ikut menyala.
7. Putar kembali potensio simulator ke bawah ambang batas:
   * Katup servo otomatis menutup kembali ke 0°.
   * LED D4 otomatis MATI.
   * Status sistem kembali ke `NORM`.

---

## 7. Aturan Prioritas & Perilaku Sistem (v2 - Anti Tabrakan Fitur)

| Situasi | Katup | Buzzer | Catatan |
|---|---|---|---|
| Normal | Tutup | Mati | - |
| Manual Purge (PB1 slave / web / PB3 master) | Buka | Mati | Tutup lagi dengan PB1 / web "Tutup Katup" / PB3 |
| Alarm (P >= TH atau T >= TH) | **Buka otomatis** | **Bunyi terus** | Mute: PB2 slave / PB2 master / tombol web |
| Alarm + Manual Purge | Buka | Bunyi | Prioritas: ALARM > MANUAL > TUTUP. Katup baru menutup jika alarm reda **dan** manual dimatikan |
| Alarm reda (P < TH-0.2 Bar & T < TH-3 C) | Tutup (jika tidak manual) | Mati, status mute di-reset | Alarm berikutnya akan berbunyi lagi |
| Sensor MAX6675 putus (mode REAL) | Buka | Bunyi | Fail-safe, LCD menampilkan `T:ERR` |
| Slave tidak mengirim data > 3 dtk | - | - | Master/web menampilkan OFFLINE (bukan data basi) |
| Master tidak terdengar > 5 dtk | Slave tetap otonom | Tetap | LCD slave halaman 3 menampilkan `MASTER:--` |

**Tombol Master:** PB1 = ganti halaman, PB2 = mute semua alarm, PB3 = **toggle** buka/tutup semua katup (label di layar berubah `Purge` <-> `Tutup`).

**Kesepakatan komunikasi:** Master mengirim setpoint tiap 1 detik (heartbeat). Perintah PURGE/MUTE dikirim 3x agar tidak hilang saat Nano sedang mengirim telemetri. Semua paket divalidasi (jumlah field, rentang nilai, ID slave).
