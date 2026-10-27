# SIMPAS-BOILER — Guidebook firmware v3

Revisi 27 Oktober 2026 · WMLTA semester 5 · **ESP32-S3 N16R8, 16 MiB flash + 8 MiB OPI PSRAM**, dua Arduino Nano ATmega328P.

Repository: https://github.com/riezky-gitH/SIMPAS-BOILER

Panduan ini menggantikan panduan v2. Firmware ketiga board harus diperbarui bersama karena protokol UART v3 tidak kompatibel dengan paket `#SLV`/`#CMD` lama. Ini prototipe pembelajaran kontrol terdistribusi; posisi katup belum mempunyai feedback dan sistem bukan safety controller boiler industri yang tersertifikasi.

## 1. Arsitektur dan keputusan kontrol

- **Slave:** sampling sensor, kualitas data, alarm per penyebab, trip latch, arbitrasi permintaan, relay, buzzer, dan LCD. Kontrol tetap berjalan ketika master/jaringan mati.
- **Master:** dua link UART, command tracker, TFT, WiFi AP, dashboard, historian, ekspor USB/serial, MQTT opsional.
- **Dashboard:** membaca snapshot, menyajikan data stale/unknown, dan mengajukan perintah operator. Browser tidak mengambil keputusan proteksi.

Output demo adalah:

`relay_on = startup_unknown OR active_causes OR trip_latched OR local_purge OR remote_purge OR global_purge`

Sumber purge berbeda dan tidak saling menghapus. `PURGE,0` hanya melepas permintaan remote. Tombol lokal hanya mengubah permintaan lokal. Pelepasan global hanya diizinkan setelah sensor valid dan tidak ada penyebab aktif minimal dua detik. Trip yang sudah tertahan tetap perlu RESET tersendiri. Flag purge lokal/remote/global tidak ikut dihapus oleh reset trip.

Penyebab alarm memakai bit mask:

| Bit | Nilai | Makna |
|---|---:|---|
| 0 | 1 | PRESSURE_HIGH |
| 1 | 2 | TEMP_HIGH |
| 2 | 4 | PRESSURE_FAULT |
| 3 | 8 | TEMP_FAULT |
| 4 | 16 | SIM_IN_LIVE |

Tekanan tinggi masuk pada P ≥ ambang, pulih pada P ≤ ambang − 0,20 bar. Suhu tinggi masuk pada T ≥ ambang, pulih pada T ≤ ambang − 3 °C. Penyebab baru membunyikan ulang buzzer walaupun penyebab lain sebelumnya di-silence. Silence dan ACK adalah dua keadaan berbeda. Trip latch default ON; tidak hilang hanya karena nilai kembali aman.

Mode default DEMO memperbolehkan sumber SIM/REAL campuran dengan label yang terlihat. Mode LIVE menolak konfigurasi awal jika sumber masih SIM dan memicu SIM_IN_LIVE bila selector berubah ke SIM kemudian. Selector pullup bukan detektor kerusakan kabel yang lengkap.

## 2. Pin dan batas hardware

### Master

| Sinyal | GPIO |
|---|---:|
| RV5 / draft ambang | 4 |
| PB1 / PB2 / PB3 | 5 / 6 / 7 |
| UART slave 1 RX / TX | 16 / 17 |
| UART slave 2 RX / TX | 8 / 18 |
| TFT DC / CS / MOSI / SCK / RESET | 9 / 10 / 11 / 12 / 14 |
| Native USB D− / D+ | GPIO19 / GPIO20, melalui port USB native board |

### Kedua slave

| Sinyal | Pin |
|---|---|
| RX dari master / TX ke master | D2 / D3 |
| LED aktuator HIGH / buzzer HIGH / relay LOW saat aktif | D4 / D5 / D9 |
| PB1 / PB2 / PB3 | D6 / D7 / D8 |
| MAX6675 CS / SO / SCK | D10 / D12 / A0 |
| Selector suhu / tekanan | D11 / A3 |
| Pot simulasi suhu / tekanan | A1 / A2 |
| Transmitter tekanan real | A6 |
| LCD I2C SDA / SCL | A4 / A5 |

MAX6675 dibaca setiap 250 ms. Fault D2 dan pola tidak valid tertentu dipisahkan dari angka suhu; tidak memakai 999.9 sebagai error sentinel. Ini tidak dapat mendeteksi semua jenis kerusakan, misalnya bus yang terus terbaca nol. Transmitter diasumsikan 0,5–4,5 V untuk 0–10 bar, referensi ADC 5 V; nilai di bawah 0,35 V atau di atas 4,65 V ditandai fault. Pastikan spesifikasi sensor aktual sebelum mengubah kalibrasi.

Nano TX adalah 5 V sedangkan ESP32 RX bukan 5 V tolerant. Divider lama 1k/2,2k menghasilkan sekitar 3,44 V nominal; evaluasi toleransi dan margin terhadap domain GPIO, atau gunakan buffer yang sesuai. Perubahan firmware tidak memperbaiki rangkaian listrik secara fisik. Verifikasi proteksi beban induktif, rating relay/solenoid, catu daya, dan keadaan output saat reset/power-loss. Relay ON bukan bukti katup fisik membuka.

Folder `uji_uart_potensiometer` di workspace adalah tes lama dengan RX D10/TX D11 dan pot A0. Jangan memakai pin/protokol tes lama untuk firmware ini. CAD asli menyimpan nama footprint board terdahulu; pinout dan board fisik harus diverifikasi saat commissioning.

### Jika TFT backlight menyala tetapi gambar hitam

Pada perangkat yang diperiksa melalui COM20, master berulang kali reset dengan pesan `partition 2 invalid ... size 0x5f0000 exceeds flash chip size 0x400000`, diikuti `load partition table error`. Ini berarti ukuran flash pada image boot dikonfigurasi 4 MiB, sementara tabel partisi aplikasi memerlukan 16 MiB. Program belum mencapai `setup()` sehingga TFT belum mendapat gambar. Log ini tidak mengukur kapasitas fisik modul.

Untuk N16R8, pilih **ESP32S3 Dev Module**, **Flash Size 16MB**, **Flash Mode QIO 80MHz**, **PSRAM OPI**, **USB Mode USB-OTG (TinyUSB)** dan **USB CDC On Boot Enabled**. Buka sketch master pada folder yang sama dengan `partitions.csv`, lalu compile dan upload ulang melalui Arduino IDE agar bootloader, partition table dan aplikasi diperbarui bersama. Mengganti menu Flash Size tanpa upload ulang tidak memperbaiki boot image. Tidak perlu menghapus seluruh flash atau memformat history untuk masalah ini. Alternatif build CLI: `python Tools/build_firmware.py --target master` menggunakan profil 16 MB secara eksplisit; perintah tersebut hanya compile.

Firmware master kini menggambar tahap boot langsung ke TFT sebelum Preferences/history/WiFi/USB, memakai orientasi 3 (diputar 180 derajat dari tampilan sebelumnya), serta clock transfer 20 MHz. TFT memakai dua framebuffer berindeks 4 bit dan hanya mengirim area berubah. Jika PSRAM tidak tersedia, buffer dapat memakai SRAM bila ruangnya cukup. Kegagalan alokasi menampilkan pesan memori satu kali, tanpa siklus clear/gambar. `STATUS` di serial aplikasi menampilkan memori/FS dan `TFT` menginisialisasi ulang layar. Layar boot ini baru dapat muncul setelah bootloader berhasil menjalankan aplikasi; ia tidak dapat mengatasi kesalahan partisi sebelum aplikasi dimulai.

### Jika COM muncul sebentar lalu hanya disk history yang terlihat

Pada pemeriksaan Windows, perangkat terdeteksi sebagai `SIMPAS History ReadOnly USB Device` tanpa port COM. Log build sebelumnya menunjukkan `ARDUINO_USB_CDC_ON_BOOT=0`. Dengan konfigurasi itu, aplikasi mengaktifkan USB MSC tanpa antarmuka serial CDC; port ROM yang semula muncul dapat hilang ketika aplikasi mulai. Ini bukan bukti bahwa chip rusak atau history harus dihapus.

Untuk recovery, tahan BOOT, tekan-lepas RESET/EN, lalu lepaskan BOOT. Pilih port baru yang muncul di Arduino IDE. Aktifkan USB CDC On Boot, pilih USB-OTG (TinyUSB), nonaktifkan USB Firmware MSC On Boot dan USB DFU On Boot, serta tetap gunakan flash 16MB dan OPI PSRAM. Upload ulang melalui UART0 / Hardware CDC; reset setelah upload jika diperlukan. Pilih kembali port aplikasi yang baru muncul jika nomor COM berubah. Disk history dibuat sendiri oleh aplikasi, sehingga menu USB Firmware MSC On Boot harus Disabled. CDC dan disk history aplikasi dapat aktif bersamaan.

Header OfflineUsb.h kini menolak compile bila CDC On Boot dimatikan atau Firmware MSC On Boot diaktifkan, agar konfigurasi yang menghilangkan serial aplikasi tidak terulang.

## 3. Dependensi dan build

Versi yang dipakai untuk verifikasi:

- Core `esp32:esp32` **3.3.5**; core `arduino:avr` **1.8.6**.
- Adafruit GFX 1.12.4, Adafruit BusIO 1.17.4, Adafruit ST7735/ST7789 1.11.0.
- ArduinoJson 7.4.2, PubSubClient 2.8, LiquidCrystal I2C 1.1.2.
- WiFi, WebServer, Preferences, FS/LittleFS, USB dan Wire dari core terkait.

Pengaturan ESP32-S3 Dev Module:

| Menu | Nilai |
|---|---|
| Flash Size | **16MB** |
| Flash Mode | **QIO 80MHz** untuk WROOM-1 N16R8 |
| PSRAM | **OPI PSRAM** |
| USB Mode | **USB-OTG (TinyUSB)** |
| USB CDC On Boot | **Enabled** |
| USB Firmware MSC On Boot | **Disabled**, aplikasi membuat disk history sendiri |
| USB DFU On Boot | Disabled |
| CPU | 240 MHz |
| Partisi | `partitions.csv` dalam folder sketch, bukan default 4 MB |

OPI PSRAM tidak berarti flash harus dipilih OPI. Cocokkan marking modul fisik. Lihat [dokumentasi Espressif](https://docs.espressif.com/projects/arduino-esp32/en/latest/troubleshooting.html).

Partisi custom:

| Partisi | Offset | Ukuran |
|---|---:|---:|
| NVS | 0x9000 | 0x5000 |
| OTA metadata (reserved) | 0xE000 | 0x2000 |
| app0 | 0x10000 | 0x5F0000 |
| LittleFS, label `spiffs` | 0x600000 | **0xA00000 = 10 MiB** |

Layout berakhir tepat di 16 MiB. Hanya ada satu slot aplikasi; OTA update tidak disediakan. Mengganti layout dari firmware lama memerlukan penulisan tabel partisi baru dan dapat membuat data lama tidak dapat diakses. Unduh arsip yang diperlukan lebih dahulu.

Dari root repository:

```powershell
python Tools/sync_assets.py
python Tools/build_firmware.py --target all
node Tests/dashboard.test.cjs
python -m unittest discover -s Tests -p "test_*.py" -v
```

Build script tidak mengunggah firmware. Output dan log berada di `build/`; lihat `build/results.json`. Buka `.ino` pada masing-masing folder sketch untuk upload melalui Arduino IDE, dengan seluruh header lokal dan `partitions.csv` tetap berada di folder tersebut. Jangan hanya menyalin file `.ino` tunggal.

Header lokal dihasilkan dari `Firmware/Shared` dan `Firmware/SlaveCommon`; ubah sumber kanonik lalu jalankan `sync_assets.py`. `Firmware/WebDashboard/index.html` adalah satu sumber dashboard. Header PROGMEM dan HTML Tools dihasilkan otomatis. Untuk menyamakan salinan workspace lama: `python Tools/sync_assets.py --legacy`.

Nano default memakai pilihan ATmega328P Old Bootloader untuk kompatibilitas board yang sebelumnya digunakan. Watchdog AVR default OFF karena sejumlah bootloader Nano lama tidak pulih dengan benar dari watchdog reset. Sesudah memasang dan menguji Optiboot, gunakan `python Tools/build_firmware.py --nano-watchdog`; profil itu memakai CPU atmega328 dan watchdog 2 detik. Ini memerlukan pekerjaan bootloader fisik. ESP32 memakai loop watchdog, dan I2C slave dibatasi timeout 25 ms.

## 4. Startup, WiFi, dan hak operator

1. Nyalakan alat. Slave memasuki STARTUP/UNKNOWN; pada firmware demo relay diminta ON sampai pembacaan awal valid. Pullup/keadaan relay sebelum aplikasi berjalan tetap harus dipastikan secara hardware.
2. Master memancarkan SSID `SCADA_BOILER_PLTU`. Password AP adalah `00000000`, sesuai konfigurasi yang diminta. Password lama di NVS diperbarui saat boot.
3. Tekan PB1 sampai halaman **WiFi** untuk melihat password AP dan token operator. Atau gunakan USB Serial 115200, Newline, ketik `TOKEN`.
4. Hubungkan laptop/HP ke AP dan buka `http://192.168.4.1`.
5. Monitoring dapat dibaca tanpa token. Untuk kontrol, masukkan kode **123456** ke dashboard. Kode singkat ini menggantikan token acak lama otomatis saat boot revisi terbaru, tanpa menghapus history. Ganti nilai lama pada Akses operator dan Simpan akses. Token hanya disimpan dalam sessionStorage browser. Server memverifikasi tiap aksi.
6. Setelah token operator tersimpan, dashboard otomatis menyinkronkan jam dari HP/laptop bila jam master kosong atau meleset lebih dari 3 detik. Tombol Sinkronkan jam tetap tersedia. Tampilan selalu WIB. Tanpa acuan jam, CSV untuk pengguna menulis **Belum sinkron**. Journal internal tetap menyimpan epoch 0 serta boot ID + uptime.

Konfigurasi efektif berasal dari EEPROM slave. Master menyimpan referensi setelah ACK, tetapi tidak menimpa slave secara otomatis saat reboot. Perbedaan referensi ditandai `config_mismatch` di API. Perubahan ambang dilakukan sebagai draft, ditinjau, dikirim, dan baru dianggap efektif setelah laporan slave. Rentang P 0,5–10 bar dan T 50–300 °C; default 4 bar/160 °C, independen untuk setiap node.

## 5. Push button dan halaman display

### Slave LCD 16×2

| Tombol | Tekan pendek | Tahan ±2 detik |
|---|---|---|
| PB1 | Toggle permintaan purge lokal | Reset trip jika aman ≥2 detik |
| PB2 | Silence penyebab alarm aktif | Acknowledge penyebab alarm aktif |
| PB3 | Ganti halaman | Kembali ke monitoring |

Halaman: monitoring → alarm/mute/ACK → relay dan sumber permintaan → link/ambang efektif → petunjuk guidebook. LCD menampilkan notifikasi RESET BLOCKED atau HELD BY PRIORITY saat aksi tidak dapat menutup output. Alarm baru membuka halaman alarm sekali.

LCD karakter 16×2 tidak dapat menampilkan QR 33×33 modul. Halaman guidebook slave menulis `GUIDEBOOK ONLINE` / `SCAN QR MASTER`.

### Master TFT 320×172

Tampilan memakai palet abu-abu/marun yang sama dengan dashboard. Renderer 4 bit membandingkan dua frame dan mengirim hanya area yang berubah, sehingga tidak membersihkan layar fisik setiap pembaruan. Angka live dashboard ditargetkan diperbarui setiap 250 ms, snapshot master 100 ms, dengan laju aktual mengikuti sampling/UART/jaringan. Lihat [LAYAR_TFT_DAN_RESPONS.md](LAYAR_TFT_DAN_RESPONS.md).

| Tombol | Fungsi |
|---|---|
| PB1 pendek | Ganti lima halaman |
| PB1 tahan | Home; khusus halaman draft: pilih B1-P/B1-T/B2-P/B2-T |
| PB2 pendek | Silence semua slave |
| PB2 tahan | ACK semua; khusus halaman draft: APPLY parameter terpilih |
| PB3 tekan | Aktifkan global purge bila belum aktif; bukan toggle tutup |
| PB3 tahan, bila global sudah aktif sebelum ditekan | Ajukan pelepasan global purge, diperiksa tiap slave |

Lima halaman TFT: SIMPAS (monitor/home), Riwayat, WiFi, Panduan (QR), dan Batas Alarm (RV5). Halaman Alarm dan Sistem di TFT dihapus. Header/footer memiliki margin horizontal 16 piksel. Sisa RAM/PSRAM dipindahkan ke Riwayat. Alarm tidak memindahkan halaman master; kotak masing-masing boiler menampilkan border/badge kuning terang WASPADA atau merah ALARM/GANGGUAN hanya ketika penyebab alarm masih aktif. Trip yang tertahan tanpa penyebab aktif tidak membuat border/badge merah; informasi `Relay ON / TRIP` tetap ada sebagai penjelasan output. WASPADA aktif pada rentang P_th−0,5 bar ≤ P < P_th atau T_th−5 °C ≤ T < T_th, selama kanal valid dan tidak ada penyebab alarm dengan prioritas lebih tinggi. Tampilan diputar 180 derajat dengan rotasi 3. Teks diringkas; umur paket, nomor perintah, cause mask mentah dan petunjuk USB dipindahkan dari layar operasi. Halaman WiFi menampilkan password `00000000`, IP dan token kontrol. Footer tetap menunjukkan purge/alarm/link. Bila awal penekanan PB3 mengaktifkan purge baru, menahannya tidak langsung melepas purge itu kembali.

Pada halaman draft, RV5 hanya mengubah nilai usulan untuk satu parameter. Tahan PB2 secara sengaja untuk APPLY; ambang lain tidak ikut bergerak. Knob P 1–8 bar dan T 50–300 °C; rentang penuh P sampai 10 bar tersedia melalui dashboard.

Halaman QR menampilkan gambar yang berasal dari modul QR lampiran, dengan quiet zone empat modul, skala 3, dan tulisan **`scan for guidebook`**. Ukuran QR total 123×123 piksel; tujuan: repository di atas. Halaman stats menampilkan P-max/T-max dua node dan lima insiden alarm terakhir; `U+...s` berarti jam belum tersinkron.

### Navigasi alarm dan indikator slave

PB1 slave tetap untuk purge lokal/reset, PB2 untuk silence/ACK, PB3 untuk halaman/home. Halaman yang dipilih operator disimpan terpisah dari overlay alarm. Penyebab alarm baru yang belum dibisukan dapat memunculkan halaman alarm, lalu silence atau pulihnya penyebab mengembalikan halaman terakhir. Navigasi manual selama alarm tetap dihormati; penyebab baru dapat memberi overlay lagi. Tidak ada pemaksaan halaman terus-menerus selama penyebab yang sama bertahan.

LED D4 mewakili output aktuator, menyala HIGH saat relay D9 aktif LOW (atau selama TEST dua detik); bukan indikator buzzer. Sensor normal tidak otomatis menutup output bila trip atau purge masih aktif. Kedua sumber pembacaan ditulis `[S]`/`[R]`. Nomor versi konfigurasi tetap dipakai protokol tetapi tidak ditampilkan pada halaman MASTER/TH.

### Demo broadcast otomatis pada slave

Pilih kedua sumber SIM dan LIVE nonaktif, lalu tahan **PB2+PB3 bersamaan dua detik** setelah startup siap. Notifikasi `DEMO AUTO ON` menandakan P/T dummy bertahap dan pengiriman B otomatis. Tahan kembali untuk berhenti. REAL/LIVE atau restart menghentikan mode; tidak disimpan di EEPROM. Penanda `[S]` tetap terlihat. Mematikan demo tidak menghapus trip/purge; lakukan RESET ketika aman. Detail: [DEMO_BROADCAST.md](DEMO_BROADCAST.md).

## 6. Historian lokal dua tingkat

### Tier 1 — PSRAM

- Ring buffer dialokasikan **3 MiB dari PSRAM 8 MiB**, bukan menghabiskan seluruh PSRAM sehingga masih ada ruang untuk framebuffer, jaringan, dan ekspor.
- Rekaman RAM setiap 1 detik untuk grafik. Kapasitas aktual bergantung `sizeof(Record)`; halaman Riwayat TFT menampilkan sisa PSRAM dan RAM; diagnostik lengkap tetap di dashboard Sistem.
- Tanpa PSRAM, buffer kecil fallback tersedia dan dashboard menampilkan peringatan; fitur tidak diam-diam mengklaim kapasitas N16R8.
- Ring menyimpan juga record yang masih menunggu flush. Bila tertahan terlalu lama sampai tertimpa, penghitung `dropped` bertambah.

### Tier 2 — LittleFS

- Partisi fisik **10 MiB**. Budget journal history **9 MiB**, sisanya untuk event, metadata, dan ruang kerja filesystem.
- Persist rutin setiap **5 detik** pada semua kondisi. Saat tekanan/suhu mencapai threshold, penyebab alarm baru muncul, atau relay mulai aktif, catat segera saat paket sensor diterima. Catatan tambahan tidak menggeser jadwal 5 detik. Kondisi tinggi yang menetap tidak membanjiri log per paket; re-crossing tetap dicatat.
- Worker flash terpisah melakukan flush saat sekitar **4 KiB data CSV** terkumpul atau **60 detik** berlalu. WiFi tidak diperlukan.
- Kegagalan tulis memberi jeda retry sekitar lima detik agar worker tidak terus membebani flash yang bermasalah.
- File aktif bernama `/history_boiler.csv`, dengan arsip `/h00000001.csv` dan seterusnya. Segmen sekitar 128 KiB; saat budget penuh, segmen tertua yang tidak dipin dihapus.
- Ekspor USB, dashboard dan Serial DUMP menyajikan **`history_boiler.csv`** dengan tujuh kolom yang dapat dibaca: Waktu WIB, tekanan/suhu kedua slave dalam bar/°C, dan dua status tertulis. Journal internal tetap menggunakan segmen dengan CRC; field teknis tidak ditampilkan pada CSV pengguna.
- Tiap baris journal internal memiliki CRC16; boot memulihkan tail file yang belum lengkap dan mengabaikan baris rusak saat pemulihan grafik. Counter corrupt tetap terlihat.
- Event disimpan di `/events.csv` dan satu arsip sebelumnya, masing-masing dibatasi sekitar 128 KiB. Lima alarm terakhir disimpan dengan checksum dan penggantian file sementara.
- Gagal mount **tidak otomatis memformat** agar data lama tidak terhapus.

**Inisialisasi flash baru:** melalui USB Serial ketik `FORMAT CONFIRM`. Perintah ini secara eksplisit menghapus filesystem dan hanya diperbolehkan ketika filesystem belum ter-mount. Jangan menjalankannya untuk memulihkan data penting. Jika mount gagal pada alat yang sebelumnya berisi data, investigasi/backup lebih dahulu. Setelah berhasil, firmware menjadwalkan snapshot USB; reconnect bila OS belum mendeteksinya.

**Mati listrik:** PSRAM hilang. Data yang sudah di-flush tetap berada di flash; data pending sejak flush terakhir dapat hilang. Setelah write error, pending dapat mencakup lebih dari satu menit. Flush berkala tidak sama dengan transaksi yang dijamin bertahan pada setiap instruksi CPU saat listrik terputus. Gunakan backup supply bila kehilangan satu menit data tidak dapat diterima.

### Kapasitas sebenarnya

Estimasi: `hari = budget_byte / ukuran_rata_rata_baris × interval_detik / 86400`.

Dengan budget 9 MiB dan baris 100 byte: interval 5 detik memberi perkiraan **5,5 hari** sebelum penghapusan data tertua. Kejadian tambahan memperpendek retensi. Angka berubah sesuai panjang nilai, boot ID, dan kejadian. Dashboard/API menghitung estimasi dari ukuran baris aktual. Journal internal memiliki `waktu_wib` sebelum CRC; CSV ekspor menyajikan tujuh kolom pengguna tanpa field teknis. Alat validasi menerima CSV terbaca maupun journal lama/baru; validasi CSV terbaca memeriksa skema/nilai, bukan CRC yang tidak disertakan dalam ekspor. Segmen lama dikonversi melalui file sementara dan penggantian atomik setelah seluruh baris lolos CRC. Waktu lama tanpa acuan boot tetap `UNSYNCED`. Klaim 2–4 minggu atau ribuan jam tidak dapat dijamin untuk CSV lengkap dua node pada interval ini. Untuk mencapai beberapa minggu dengan interval tetap, gunakan media lebih besar atau format penyimpanan terkompresi yang dirancang terpisah.

## 7. Tiga akses offline

### A. TFT tanpa laptop

Tekan PB1 ke halaman Local History / Peak Hold. Tersedia peak dua node dan lima insiden terakhir, termasuk jam yang valid atau boot/uptime. Kontrol lokal slave tetap bekerja ketika WiFi master mati.

### B. Native USB mass storage, baca-saja

Drive hanya mendapat media ketika filesystem mounted dan recovery siap. `history=ready` tanpa `FS=mounted` belum cukup. Firmware memeriksa status host USB aktual agar event awal CDC yang terlewat tetap dapat dipulihkan, dan melakukan retry snapshot yang gagal sementara. `STATUS` melaporkan `USB=...`; halaman Sistem dashboard juga menampilkan kondisi USB history. Baca [USB_HISTORY.md](USB_HISTORY.md) untuk diagnosis dan hasil uji perangkat.

Gunakan **port USB native** ESP32-S3 dan kabel data; pilih USB-OTG TinyUSB seperti tabel build. Perangkat menampilkan drive FAT16 virtual berisi `history_boiler.csv`. Salin ke laptop/Excel. Drive ini **bukan** raw LittleFS dan tidak menerima write dari OS.

Saat host mount, master mem-pin snapshot segmen yang sudah tersimpan. Logging baru memakai file aktif baru. Host tidak melihat file berubah di tengah copy. Snapshot tidak bertambah otomatis; eject dan reconnect, atau ketik `USB` di serial setelah eject untuk menjadwalkan pembaruan.

Jika snapshot dipertahankan lama dan flash hampir penuh, rotasi tidak menghapus segmen yang sedang diekspor. Flush dapat tertahan; pending tetap di RAM, dan `paused_by_snapshot`/`dropped` terlihat. Eject drive untuk melepas pin dan melanjutkan rotasi. Ini batas yang disengaja, bukan penghapusan arsip di bawah host.

Jangan mengaktifkan menu **USB Firmware MSC On Boot**; itu bukan disk history aplikasi ini. Hub USB, kabel, driver OS, mount/eject dan pembacaan penuh masih perlu diuji di perangkat nyata.

### C. USB CDC Serial

Serial Monitor 115200, Newline:

| Perintah | Fungsi |
|---|---|
| HELP | Daftar perintah |
| TOKEN | Token operator dan password AP |
| DUMP | Stream snapshot CSV dengan penanda BEGIN/END |
| STOP | Batalkan dump dan lepas pin |
| FLUSH | Flush batch pending yang dibatasi |
| USB | Jadwalkan pembaruan snapshot disk |
| WIFI OFF / WIFI ON | Uji offline / aktifkan kembali AP |
| FORMAT CONFIRM | Inisialisasi filesystem yang belum mount; menghapus data |

Alternatif penangkapan otomatis:

```powershell
python -m pip install "pyserial>=3.5,<4"
python Tools/dump_serial.py --port COM7 --out history_boiler.csv --timeout 1800
```

Script menyimpan `.partial` sampai ukuran BEGIN cocok dan END diterima; file tidak dianggap lengkap jika koneksi terputus. Durasi bergantung ukuran arsip dan throughput host. Pada UART 115200 8N1, batas teoritis hanya sekitar 11,5 kB/s; CDC native dapat lebih cepat, tetapi dump 10 MiB dalam 3 detik bukan jaminan. UART/kontrol dijalankan pada loop terpisah dari task I/O yang menangani ekspor.

Capture otomatis memverifikasi jumlah byte dan skema/nilai CSV sebelum mengganti `.partial` menjadi arsip akhir. Untuk file yang disalin dari USB/dashboard, jalankan `python Tools/validate_history.py history_boiler.csv` untuk pemeriksaan format. CRC journal internal tetap diperiksa oleh firmware saat pembacaan; CRC tidak disertakan sebagai kolom ekspor pengguna.

PB2+PB3 tidak dijadikan shortcut DUMP karena PB3 adalah kendali purge. Gunakan `DUMP` agar pengambilan data tidak berbenturan dengan fungsi tombol. Bila kebijakan IT hanya mengizinkan CDC, buat firmware master dengan `--serial-only`; disk MSC tidak diiklankan. Sesuaikan dengan izin IT yang berlaku.

## 8. Dashboard dan API

**Kestabilan koneksi:** HTTP, USB, serial ekspor dan TFT berjalan di task I/O terpisah dari loop UART/kontrol. Browser membatasi satu transaksi HTTP aktif, polling telemetri dengan target 250 ms, dan history/event hanya diminta saat halamannya dibuka. Satu kegagalan HTTP tidak langsung mengubah status; data menjadi OFFLINE setelah sampel terakhir berumur 4 detik. Halaman Sistem menampilkan jeda UART maksimum, timeout polling dan jumlah putus koneksi. Baca [PERBAIKAN_KONEKSI.md](PERBAIKAN_KONEKSI.md) untuk perubahan, hasil uji dan pemeriksaan perangkat setelah upload.

Dashboard memakai layout SCADA abu-abu/marun, diagram boiler sederhana, angka proses besar, ringkasan koneksi/alarm/puncak, dan navigasi Monitor, Tren & riwayat, Alarm & kejadian, Pengaturan, Sistem. Menu berpindah ke atas pada tablet/ponsel dan panel menjadi satu kolom di layar kecil. Token operator dimasukkan melalui Akses operator di kanan atas. Data diagnostik lengkap dan kalibrasi tersedia dalam panel yang dapat dibuka.

Angka proses dan ringkasan disembunyikan saat data master tidak segar; semua tombol perintah, termasuk purge global, dinonaktifkan. Diagram menunjukkan output relay, tanpa menyatakan posisi katup atau level air yang tidak diukur. Grafik B1 marun dan B2 abu-abu; pola garis juga berbeda. Aset, ikon SVG dan grafik canvas tersedia lokal tanpa CDN atau font eksternal.

Dashboard menyediakan monitoring, grafik P/T 5/15/60 menit dari ring RAM, alarm per penyebab, event master, draft ambang/kalibrasi, sinkronisasi jam, diagnostik, dan ekspor CSV. Riwayat lebih lama tersedia melalui CSV flash; ring RAM tidak diklaim menyimpan seluruh periode flash. Grafik memakai gap untuk node/channel yang invalid. Suara browser memakai alarm dua nada dan mengikuti penyebab aktif serta status buzzer slave; Silence, pemulihan, offline dan Audio OFF menghentikannya. Browser membuka audio setelah interaksi pertama atau tombol Audio AKTIFKAN. Pilihan OFF disimpan selama sesi. Waspada hanya memberi notifikasi kuning tanpa suara.

| Endpoint | Metode | Otorisasi | Fungsi |
|---|---|---|---|
| `/` | GET | AP client | Dashboard embedded |
| `/api/data` | GET | AP client | Snapshot dua node, umur/kualitas, command status, storage stats |
| `/api/history?seconds=300` | GET | AP client | Maksimal 720 titik RAM (180 tanpa PSRAM), batas 60–3600 s |
| `/api/events` | GET | AP client | 128 event boot saat ini |
| `/api/events.csv` | GET | Bearer token | Arsip event flash yang tersedia |
| `/api/command` | POST | Bearer token | `node`: 0–2, `action` whitelist |
| `/api/config` | POST | Bearer token | Draft P/T independen dan kalibrasi untuk node online |
| `/api/time` | POST | Bearer token | Sinkronisasi epoch operator |
| `/api/export/start` | POST | Bearer token | Buat/pin tiket ekspor |
| `/api/export/chunk?id=...&offset=...` | GET | Bearer token | Chunk maksimal 32 KiB |
| `/api/export/end` | POST | Bearer token | Lepaskan tiket ekspor |
| `/api/history.csv` | GET | Bearer token | Ekspor langsung untuk klien yang mendukung stream panjang |
| `/api/usb-refresh` | POST | Bearer token | Refresh snapshot disk |

Contoh body command: `{"node":1,"action":"PURGE,1"}`. Aksi: `PURGE,1`, `PURGE,0`, `GLOBAL,1`, `GLOBAL,0`, `MUTE`, `ACK`, `RESET`, `TEST`, `DEMO,1`, `DEMO,0`. Respons HTTP 202 hanya berarti QUEUED di master. Lihat hasil node: SENT, APPLIED, BLOCKED, INVALID, TIMEOUT; hasil polling setelah ACK hilang dapat ditandai RECONCILED/OBSERVED_RELEASE. Jangan mengartikan 202 sebagai katup sudah bergerak.

TEST hanya menguji LED/buzzer selama dua detik di DEMO ketika kedua sumber SIM dan tidak ada alarm/trip. Relay diuji melalui manual purge secara sengaja. Ambang dikirim atomik bersama versi, mode, trip latch, dan parameter kalibrasi. Command menggunakan rate limit; Origin browser diperiksa dan GET tidak mengubah output.

Contoh config: `{"node":1,"pressure":4.0,"temp":160.0,"live":0,"latch":1,"zero_mv":500,"span_mv":4000,"vcc_mv":5000}`.

Ekspor dashboard menggunakan potongan 32 KiB, sehingga master dapat melayani kontrol di antara request. Tiket yang ditinggalkan kedaluwarsa setelah 120 detik tanpa aktivitas. Download langsung yang panjang masih dapat menunda request HTTP lain; gunakan tombol ekspor dashboard untuk penggunaan interaktif.

## 9. Protokol UART v3

9600 8N1, point-to-point. Master menunggu balasan polling maksimal 700 ms, lalu menjadwalkan polling berikutnya sekitar 100 ms setelah balasan valid. Hanya satu polling aktif per link; fase awal kedua node berbeda. Pada mode biasa, slave membalas permintaan. Mode demo broadcast mengirim frame `B` secara otomatis nominal tiap satu detik tanpa menunggu polling; master tetap melakukan keepalive sekitar tiga detik. Pengiriman master saat node otomatis online dibatasi 150 ms setelah frame lengkap, sedangkan slave menunggu 400 ms sejak akhir TX dan 40 ms tanpa byte RX. Frame B sequence 0 tidak pernah dianggap ACK command. Lihat [DEMO_BROADCAST.md](DEMO_BROADCAST.md) untuk aktivasi dan batas pengujiannya.

Frame: `#<payload>*<CRC16 4 hex uppercase>\n`, CRC-16/CCITT-FALSE (polynomial 0x1021, initial 0xFFFF), hanya atas payload. Vektor `123456789` menghasilkan `29B1`. Buffer maksimum 176 byte; frame parsial timeout setelah 300 ms tanpa karakter baru dan karakter `#` baru memulai ulang parser.

Command payload: `C,node,session,sequence,operation[,args]`.

SET args: `P_cbar,T_dC,version,live,latch,zero_mV,span_mV,vcc_mV,2`. Angka terakhir adalah versi skema konfigurasi. Semua field divalidasi sebelum penerapan.

Telemetry/ACK payload:

`T,id,session,sequence,slave_boot,sample_sequence,P_cbar,T_dC,flags,causes,muted,ack,config_version,THP_cbar,THT_dC,state,result,raw_adc,rx_errors,zero_mV,span_mV,vcc_mV,live,latch`

Flag bits: 0 P_SIM, 1 T_SIM, 2 P_BAD, 3 T_BAD, 4 RELAY_ON, 5 LOCAL, 6 REMOTE, 7 GLOBAL, 8 TRIP, 9 BUZZER, 10 TEST, 11 STARTUP, 14 AUTO_DEMO. Mask flag UART yang diizinkan adalah 20479; bit 12/13/15 bukan flag UART. Journal CSV internal memakai bit 15 untuk koneksi valid, bit 13 untuk status yang direkam, dan bit 12 untuk WASPADA yang direkam. Bit tambahan ini tidak dikirim melalui UART dan tidak muncul sebagai kolom CSV pengguna. Nilai −1 adalah angka tidak valid; kualitas tetap harus dilihat dari flag dan koneksi. State 0 NORMAL, 1 WARNING, 2 ALARM, 3 FAULT, 4 STARTUP. Result 0 POLLED, 1 APPLIED, 2 BLOCKED, 3 INVALID, 4 BUSY.

Command ID tidak berubah saat retry. Maksimal tiga upaya dengan jarak sekitar 700 ms. Deduplikasi mencegah penerapan berulang; sequence lama pada sesi yang sama ditolak. Balasan T diterima bila ID/sesi/sequence serta seluruh field cocok. Frame B memakai skema 24 field yang sama, sequence 0, sesi yang cocok atau sesi 0 saat bootstrap, serta boot/sample sequence untuk menolak duplikat; frame B tidak menyelesaikan command yang pending. Tidak ada ACK fisik posisi katup; ACK ini hasil logika pengendali.

## 10. MQTT dan logger PC opsional

Copy `Secrets.example.h` menjadi `Secrets.h` dan isi router/broker privat. File Secrets diabaikan Git. Default tidak mengaktifkan router/MQTT. Jika koneksi router gagal setelah startup, percobaan STA dihentikan tanpa mematikan/membuat ulang AP. Koneksi MQTT berjalan di task terpisah agar timeout broker tidak memblokir loop kontrol/HMI.

- Full snapshot: `boiler/telemetry`, schema sama dengan HTTP.
- Availability/LWT retained: `boiler/availability` online/offline.
- Command: `boiler/command`, JSON `{"token":"...","node":1,"action":"MUTE"}`. Hasil terlihat pada telemetri node.
- Broker adalah layanan terpisah; script Python bukan broker. MQTT ini untuk LAN privat. Jangan mengekspos HTTP/MQTT plaintext ini ke internet tanpa rancangan transport/akses tambahan.

Logger HTTP tanpa dependency tambahan:

```powershell
python Tools/dashboard_mqtt_bridge.py --mode http --master http://192.168.4.1 --serve 8080
```

Buka `http://127.0.0.1:8080` untuk dashboard proxy. Membuka HTML langsung dengan `file://` tidak menyediakan API. Proxy tidak menyuntikkan hak operator; token tetap diverifikasi master. Data masuk ke CSV rotasi dan SQLite; default CSV 20 MiB × lima arsip, SQLite memerlukan pengelolaan kapasitas PC oleh operator.

MQTT:

```powershell
python -m pip install "paho-mqtt>=2.1,<3"
python Tools/dashboard_mqtt_bridge.py --mode mqtt --broker 192.168.1.10
```

Kredensial logger opsional melalui environment `SIMPAS_MQTT_USER` dan `SIMPAS_MQTT_PASSWORD`. MQTT-only logger dapat bekerja tanpa HTTP, tetapi dashboard proxy tetap memerlukan jalur HTTP ke master.

## 11. Commissioning dan batas verifikasi

Lakukan di mode SIM tanpa tekanan/suhu berbahaya:

1. Pastikan compile ketiga board dan profil/partisi cocok; upload ketiganya.
2. Verifikasi pin, polaritas relay, suplai, tegangan UART, dan kedua arah komunikasi.
3. Uji P/T normal, warning, alarm, hysteresis, fault sensor, dan perpindahan SIM/REAL.
4. Mute alarm tekanan lalu naikkan suhu: penyebab baru harus membunyikan buzzer kembali.
5. Aktifkan purge global lalu tekan PB1 lokal: global tidak boleh terhapus.
6. Lepaskan manual remote saat alarm/trip aktif: output tetap sesuai prioritas dan status menjelaskan penahanan.
7. Reset trip saat bahaya dan saat aman ≥2 s: harus ditolak/diterima sesuai kondisi.
8. Cabut UART, reboot satu node, kirim frame CRC salah/duplikat: data stale, retry, dan timeout harus terlihat.
9. Sinkronkan jam, rekam normal/kritis, unduh CSV, cek tujuh kolom/nilai/status (CRC tetap di journal internal), flush lalu matikan daya dan cek pemulihan.
10. Matikan WiFi dengan `WIFI OFF`; cek stats TFT, USB disk dan DUMP. Uji file penuh, pin snapshot lama, eject, dan kelanjutan flush.
11. Uji kompatibilitas native USB, partial reads, driver Windows, serta transfer besar di laptop yang akan dipakai.

Build dan pengujian script tidak membuktikan tindakan katup, wiring, endurance flash, atau waktu respons fisik. Penggantian divider, feedback aktuator, RS-485, proteksi catu daya, dan safety valve independen membutuhkan pekerjaan hardware. Tidak ada perubahan CAD/PCB yang otomatis diterapkan oleh firmware ini.

Lihat `IMPLEMENTATION_V3.md` untuk matriks perubahan dan hasil verifikasi yang tersedia. Tidak ada firmware yang otomatis diunggah oleh tool build.

## Revisi presentasi dan relay 27 Oktober 2026

Demo dipercepat: laju normal maksimal 0,40 bar/detik dan 6 °C/detik dengan fase lebih singkat; broadcast tetap sekitar satu detik. Dalam uji native 80 seed/node dari nol pada threshold default, alarm pertama paling lambat 18 detik. Proteksi sensor REAL dan trip latch tidak diubah. Kode kontrol tetap 123456; token acak lama diganti saat boot.

Modul relay dua channel: IN1 ke D9 slave 1, IN2 ke D9 slave 2, ground bersama dan catu sesuai rating modul. CAD lama menamai konektor itu Servo SG90 (J2/J5 pad 1). Tes indikator bukan tes relay. Lihat [RELAY_DUA_CHANNEL.md](RELAY_DUA_CHANNEL.md) untuk membedakan input, koil dan kontak.

## Dashboard alarm dan layout desktop

Notifikasi merah ALARM dan kuning WASPADA mengikuti kondisi proses setiap boiler. Trip yang tertahan tanpa causes tidak membuat pop-up, hitungan alarm atau suara; status TRIP tetap dapat dibaca di baris relay. Header BOILER STATION; footer Politeknik Elektronika Negeri Surabaya. Monitor desktop diuji muat satu viewport 1024×600 hingga 2560×1080, termasuk Guidebook dan kontrol bawah. Halaman panjang memakai scroll pada panel, sedangkan ponsel tetap memakai alur scroll responsif. Detail dan cara aktivasi audio: [DASHBOARD_ALARM_DAN_LAYOUT.md](DASHBOARD_ALARM_DAN_LAYOUT.md).
