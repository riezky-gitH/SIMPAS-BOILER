# Implementasi firmware v3

Target: ESP32-S3 N16R8 (flash 16 MiB, OPI PSRAM 8 MiB), dua Arduino Nano ATmega328P, TFT ST7789 320×172, dan dashboard lokal. Panduan operasi lengkap ada di GUIDEBOOK.md.

## Firmware dan proteksi

Master menangani dua link UART point-to-point dengan framing CRC16, nomor sesi/urutan, retry dan pelacakan perintah melalui telemetry. Slave menjalankan proteksi lokal tanpa bergantung pada WiFi/master. Penyebab alarm tekanan/suhu tinggi, sensor fault dan SIM_IN_LIVE dipisahkan; mute dan acknowledgment berlaku per penyebab. Trip latch default aktif. Reset latch membutuhkan kondisi aman selama dua detik; permintaan purge lokal, remote dan global digabungkan.

Konfigurasi, global purge dan trip disimpan di EEPROM dua slot dengan CRC dan commit marker. WDT Nano default dimatikan karena bootloader lama memerlukan verifikasi tersendiri; opsi Optiboot disediakan. I2C memiliki timeout. MAX6675 memakai clock A0 agar tidak tergantung pin D13 yang bermasalah pada rancangan sebelumnya.

## Antarmuka dan dashboard

Master memiliki tujuh halaman: overview, alarm/perintah, history/peak hold, jaringan/token, QR guidebook, draft setpoint RV5 dan diagnostik. QR menuju https://github.com/riezky-gitH/SIMPAS-BOILER dengan tulisan “scan for guidebook”. Slave memiliki lima halaman LCD. Perilaku tombol, termasuk perbedaan tekan singkat/tahan dan reset aman, dijelaskan dalam guidebook.

Dashboard membedakan stale, offline dan sensor fault dari angka yang valid, menampilkan relay UNKNOWN saat link hilang, memiliki grafik history dan status perintah, serta draft setpoint per node. Kontrol HTTP/MQTT memerlukan kode operator singkat 123456; password AP `00000000` diterapkan, termasuk mengganti nilai lama di NVS saat boot. Monitoring dapat dibaca tanpa token. Secrets lokal tidak dilacak Git. MQTT opsional dijalankan dalam task agar koneksi broker tidak memblokir kontrol.

## History dan akses offline

Ring buffer PSRAM menampung data live per detik. Persistensi memakai interval tetap lima detik, ditambah pencatatan langsung saat threshold, alarm baru, atau aktivasi relay terdeteksi dari paket telemetry. Event tidak menggeser deadline periodik. LittleFS 10 MiB memiliki anggaran journal 9 MiB dan segmen 128 KiB, dengan flush 4 KiB atau 60 detik. CSV memiliki CRC per baris; startup memulihkan data di background dan memperbaiki ekor baris yang terpotong. Kegagalan mount tidak otomatis memformat data. Pembacaan live dimulai setelah recovery siap.

Snapshot ekspor tersedia melalui HTTP, USB MSC virtual FAT16 read-only dan serial DUMP. Snapshot mematok segmen sampai selesai/eject; penghapusan segmen ditahan jika masih digunakan. Mount native USB dan kebijakan host perlu diuji di board. Pending buffer dapat hilang saat listrik mati. Dengan perkiraan 100 byte/baris, anggaran 9 MiB sekitar 5,5 hari pada interval lima detik, berkurang oleh catatan kejadian tambahan; durasi bukan jaminan dan bukan klaim 2–4 minggu.

Logger PC mendukung CSV/SQLite, rotasi file dan validasi history. Source dashboard tunggal dan shared firmware headers disalin oleh Tools/sync_assets.py; salinan root workspace disinkronkan dengan --legacy. Tools/build_firmware.py hanya compile, tanpa upload.

## Verifikasi awal

Build ketiga board berhasil pada 25 Oktober 2026 sebelum koreksi diagnostik TFT:

| Target | Flash menurut compiler | Global SRAM menurut compiler |
|---|---:|---:|
| Master N16R8 | 1.185.711 / 6.225.920 byte | 110.256 / 327.680 byte |
| Nano 1 | 16.080 / 30.720 byte | 1.161 / 2.048 byte |
| Nano 2 | 16.080 / 30.720 byte | 1.161 / 2.048 byte |

Lulus: 10 regresi JavaScript produksi, 6 uji Python (logger, CSV/SQLite/rotasi, generated assets, CRC history, partisi dan otorisasi/origin proxy), syntax check master dan decoding QR ukuran TFT 123×123. Browser fixture diperiksa untuk monitoring/tren/konfigurasi; server yang dihentikan menghasilkan OFFLINE, angka --, relay UNKNOWN dan kualitas STALE. Screenshot disimpan di folder audit workspace; fixture tidak di-embed ke firmware.

Header build dibandingkan dengan sumber setelah menghapus directive #line. Binari partisi diperiksa: app0 pada 0x10000 berukuran 0x5f0000, LittleFS pada 0x600000 berukuran 0xa00000, batas akhir 16 MiB. Artefak, log dan manifest SHA-256 berada di audit/firmware_v3 di luar repository sumber. Rebuild diagnostik TFT juga berhasil: flash 1.187.183 byte, global SRAM 110.256 byte. Log berada di audit/master_tft_fix_build.log; artefak master dan manifest diperbarui.

Tests/core_tests.cpp dan workflow CI portable disediakan; compiler C++ host tidak tersedia saat audit awal, sehingga CI tidak diklaim sudah berjalan. Profil alternatif --serial-only dan --nano-watchdog belum dikompilasi dalam hasil default di atas. Belum ada push atau upload oleh agen, uji relay fisik, USB MSC di board, power-cut/recovery, endurance atau pengukuran latensi hardware. Perubahan divider/buffer, safety valve, feedback aktuator, bootloader dan RS-485 tetap memerlukan pekerjaan fisik.

## Temuan TFT pada perangkat

Pembacaan COM20 menemukan bootloop sebelum aplikasi: `partition 2 invalid - offset 0x10000 size 0x5f0000 exceeds flash chip size 0x400000`, diikuti `Failed to verify partition table` dan `load partition table error`. Boot image yang terpasang menggunakan batas flash 4 MiB sementara tabel partisi membutuhkan 16 MiB. Log tersebut menjelaskan backlight menyala tetapi aplikasi belum menggambar; ia tidak mengukur kapasitas fisik flash. Bukti disimpan pada audit/master_com20_boot_failure.txt.

Penyelesaian bootloop adalah compile/upload ulang dengan Flash Size 16MB, QIO flash, OPI PSRAM dan tabel partisi yang benar, termasuk bootloader dan partition table. Menghapus history tidak diperlukan untuk masalah ini.

Firmware juga diperbaiki: layar menggambar tahap boot langsung sebelum Preferences/history/WiFi/USB; alokasi framebuffer memiliki fallback PSRAM → internal RAM → gambar langsung melalui SPI; orientasi 3 (180 derajat dari tampilan sebelumnya), transfer 20 MHz, dan perintah serial STATUS/TFT ditambahkan. Syntax check dan enam regresi Python lulus setelah perubahan. Pemulihan tampilan fisik baru dapat dinyatakan setelah firmware berhasil boot dan layar diamati.


## Penyederhanaan tampilan master

Atas permintaan pengguna, TFT diputar 180 derajat (rotasi 1 menjadi 3), termasuk layar boot dan perintah inisialisasi ulang TFT. Judul/menu dan petunjuk tombol dipersingkat; monitoring tetap menampilkan tekanan, suhu, status, sumber SIM/REAL, relay dan trip. Angka teknis UART/perintah dibuang dari halaman operasi, sementara statistik komunikasi tetap tersedia di Sistem. Alarm offline tidak menampilkan status sensor lama. QR dan tulisan scan for guidebook tetap tersedia.

Password AP ditetapkan menjadi `00000000`. Nilai lama di NVS diganti saat boot hanya bila berbeda, sehingga tidak memerlukan erase flash dan tidak menulis NVS pada setiap boot. SSID tetap SCADA_BOILER_PLTU dan kode kontrol operator 123456 (revisi presentasi terbaru).

Build penyederhanaan UI berhasil: flash 1.185.811 byte, global SRAM 110.256 byte, profil flash 16 MB/OPI PSRAM. Log: audit/master_ui_simple_build.log. Binary master diperbarui, tanpa upload oleh agen.


## Diagnosis port USB

Windows mendeteksi SIMPAS History ReadOnly sebagai USB Mass Storage tanpa port COM. Log build pengguna sebelumnya memiliki CDC On Boot Disabled. Ditambahkan guard compile untuk mewajibkan CDC On Boot Enabled dan Firmware MSC On Boot Disabled; aplikasi tetap menyediakan MSC history sendiri. Ini hanya validasi konfigurasi, tidak mengubah perilaku pada profil build yang benar. Recovery memerlukan bootloader manual dan upload dengan CDC Enabled; tidak dilakukan upload/erase oleh agen saat diagnosis.


## Dashboard SCADA abu-abu / marun

Desain mengikuti palet referensi pengguna dengan aksen #800020 dan #65031c serta abu-abu. Navigasi kiri berubah menjadi bar atas pada layar sempit, panel boiler menjadi satu kolom pada ponsel, dan konten dibatasi lebarnya pada ultrawide. Diagram boiler adalah SVG lokal; tidak ada animasi level/flow yang mengklaim sensor tidak tersedia. Teks operasi disingkat dan informasi teknis lengkap ditempatkan pada halaman Sistem.

Diuji di browser dengan fixture yang diberi label simulasi pada viewport 320, 390, 768, 1024, 1440, 1920 dan 2560 px. Tidak ditemukan overflow horizontal pada halaman monitor; formulir dan tabel kejadian juga diperiksa pada ponsel. Alur token/perintah fixture berhasil. Server dimatikan untuk membuktikan OFFLINE, nilai --, relay UNKNOWN, dan semua kontrol disabled. Tiga regresi baru memeriksa ringkasan stale, global control saat offline, dan indikator relay stale; total 13 regresi JavaScript dan 6 uji Python lulus.

Build master berhasil dengan dashboard tertanam: flash 1.200.099 byte, SRAM global 110.256 byte, profil N16R8 16MB/OPI PSRAM dengan CDC aktif. Log audit/master_scada_redesign_build.log. Header/salinan root dan binary diperbarui. Belum ada upload firmware oleh agen. Screenshot desktop, mobile dan offline berada di audit/dashboard_scada_*.png. Preview http://127.0.0.1:8765 menggunakan data simulasi, bukan data perangkat.


## Perbaikan kestabilan 26 Oktober 2026

HTTP/USB/serial ekspor/TFT dipisahkan dari loop kontrol; state node dimiliki loop dan dibagikan lewat snapshot, sedangkan mutasi dari HTTP memakai antrean. Polling stop-and-wait, RX 2 KiB, parser inter-byte, pembaruan LCD dan polling browser diperbaiki. Rincian dan batas pengujian ada di PERBAIKAN_KONEKSI.md. Build terbaru: master flash 1.208.655 / SRAM 111.520 byte; kedua Nano 16.276 / 1.161 byte. 19 regresi JS dan 6 uji Python lulus. Syntax uji C++ lulus, eksekusi native disediakan di CI; perangkat belum diuji/upload oleh agen. Log build: audit/master_connection_fix_build.log, slave1_connection_fix_build.log dan slave2_connection_fix_build.log.


## TFT marun/abu-abu dan angka live cepat

Renderer TFT diganti menjadi dua frame 4 bit (55.040 byte total) dengan transfer hanya area 16×8 yang berubah. Layar tidak dibersihkan secara fisik pada render rutin. Header marun, panel putih, teks gelap mengikuti dashboard. Poll gap setelah balasan valid 100 ms, snapshot master 100 ms, polling browser target 250 ms dengan antrean tunggal dan backoff. Pencatatan history tetap seperti sebelumnya. Rincian: LAYAR_TFT_DAN_RESPONS.md. 21 regresi JS, 6 uji Python, uji renderer native dan uji Safety/CRC/framing/FAT16 native lulus; eksekusi C++ native yang tertunda pada laporan sebelumnya kini telah dilakukan. Belum ada upload/penilaian kedip fisik oleh agen.


## Drive USB history

Diagnosis perangkat menunjukkan FS awalnya unmounted; kemudian FS mounted/ready dan refresh USB berhasil menampilkan SIMPAS LOG (E:). File CSV 12.690 byte dibaca langsung dan 201 rekaman lolos CRC. Firmware diperbaiki agar membaca keadaan host aktual, mengatasi event USB_STARTED yang terjadi sebelum handler terpasang, menunggu FS/recovery, retry snapshot, dan mempertahankan perilaku Eject. Tambahan status USB di Serial/API/Sistem. Uji lifecycle native lulus; pengujian perangkat yang disebut di atas memakai firmware sebelum patch otomatisasi mount baru. Rincian: USB_HISTORY.md.

## Revisi waktu history dan pencatatan kejadian

CSV menambahkan waktu WIB yang terbaca untuk setiap rekaman. Browser operator menyinkronkan jam tanpa internet saat epoch belum ada atau drift lebih dari 3 detik, dengan backoff 30 detik bila gagal. Jadwal flash kini tetap 5 detik; threshold crossing, sebab alarm baru, dan relay rising edge dicatat segera pada penerimaan telemetry. Event tidak mengubah jadwal rutin. Acuan uptime hanya diterapkan pada rekaman RAM dengan boot ID yang sama. Waktu boot lama yang tidak diketahui tetap UNSYNCED. Konversi file lama memeriksa CRC saat membaca dan saat membaca ulang file sementara, sebelum rename atomik.

Validasi: native historian lulus; 24 JS dan 7 Python lulus; 1.300 baris CSV fisik diuji melalui konversi host dan CRC independen. Build N16R8 lulus: 1.214.515 byte flash / 117.328 byte SRAM global. Tidak dilakukan upload atau uji migrasi filesystem langsung di board pada revisi ini.

## Layar operator — 27 Oktober 2026

Master memakai lima halaman: SIMPAS, Riwayat, WiFi, Panduan, Batas Alarm. Header/footer dipadatkan dengan margin horizontal 16 piksel. Panel boiler, ukuran angka, tema marun/abu-abu, rotasi, QR dan renderer dirty-tile tetap digunakan. Halaman khusus Alarm dan Sistem pada TFT dihapus; RAM/PSRAM bebas ditampilkan di Riwayat. Alarm hanya mengubah border dan badge unit terkait, tanpa mengubah halaman. WASPADA kuning terang aktif pada kanal valid dalam jarak 0,5 bar atau 5 °C di bawah threshold; alarm/trip merah mendapat prioritas.

Slave memisahkan halaman operator dari halaman alarm sementara. Alarm baru dapat muncul sementara; MUTE atau pulihnya penyebab mengembalikan halaman terakhir. Navigasi manual selama alarm dihormati. Label sumber P dan T sama-sama memakai [S]/[R]; nomor versi V disembunyikan dari MASTER/TH, tetapi tetap digunakan protokol.

Pengguna mengonfirmasi LED tidak lagi terus menyala. Perubahan percobaan pada pelepasan trip dibatalkan; arbitrasi relay, purge, trip, polaritas pin dan tombol kembali persis ke perilaku sebelumnya. Perubahan bersama Control.h hanya definisi WARNING, yang tidak mengaktifkan relay/buzzer.

Validasi: 24 uji JS, 7 Python, serta 5 suite native C++ lulus. Uji baru mencakup batas WASPADA, prioritas alarm/trip, output yang tidak berubah karena warning, permintaan purge independen, reset eksplisit, navigasi pulih dan alarm baru, lima halaman, margin teks, serta operasi gambar produksi. Preview menggunakan nilai simulasi, bukan foto TFT. Build semua board lulus: master flash 1.212.811 / SRAM global 117.328 byte; masing-masing Nano flash 16.304 / SRAM global 1.179 byte. Tidak dilakukan upload.

## Ekspor history untuk pengguna — 27 Oktober 2026

USB, unduh dashboard dan Serial DUMP kini menyajikan tujuh kolom: waktu WIB, tekanan/suhu kedua slave dalam satuan bar/°C, dan status tertulis masing-masing slave. Kode flag, cause mask, epoch, boot, uptime, mute/ACK serta CRC tetap tersedia dalam journal internal untuk recovery/integritas tetapi tidak menjadi kolom pengguna. Penyebab alarm diterjemahkan lengkap; beberapa penyebab digabung dengan titik koma. Kanal invalid dikosongkan, koneksi hilang ditulis TERPUTUS, waktu kosong Belum sinkron.

Status historis direkam di bit cadangan journal (13 = status diketahui, 12 = WASPADA pada saat sampling). Ini tidak mengubah flag UART atau struktur konfigurasi slave. Rekaman lama yang tidak memiliki bukti status warning tidak dihitung ulang menggunakan threshold terbaru; diberi keterangan status lama tidak tersedia kecuali alarm/trip dapat dibuktikan dari cause/flag lama.

Proyeksi baca-saja memakai indeks jarang per segmen, dibangun saat recovery dan flush. Akuisisi snapshot membekukan segmen dan indeks, tanpa scan ulang seluruh history atau duplikat file besar. Cache 4 KiB digunakan untuk USB/HTTP/serial. Ruang kerja parser dialokasikan di Store di bawah mutex files agar tidak membebani stack TinyUSB 4 KiB; analisis compiler menurunkan frame projectSlice dari 1.344 ke 112 byte. Geometri virtual FAT16 64 MiB menampung pemekaran teks status; partisi fisik LittleFS tetap 10 MiB, budget 9 MiB. Error baca/CRC diteruskan ke I/O USB.

Validasi: 24 JS, 8 Python dan 6 suite native C++ lulus. Uji ekspor mencakup satuan, tujuh kolom, semua cause mask, data invalid, status lama, indeks/akses acak lintas baris/checkpoint, cache, tail terpotong, CRC rusak dan FAT16. Salinan arsip 1.300 rekaman lama berhasil diproyeksikan dan divalidasi tanpa mengubah sumber. Build master N16R8 lulus: 1.215.723 byte flash dan 134.272 byte SRAM global. Header hasil build cocok dengan sumber final. Firmware belum diunggah; akses fisik format CSV baru pada board belum diuji.

## Demo broadcast otomatis

Revisi 27 Oktober 2026 menambahkan generator dummy bertahap dan frame B spontan setiap sekitar satu detik. Aktivasi PB2+PB3 slave selama dua detik mensyaratkan kedua selector SIM dan LIVE nonaktif. Balasan T tetap khusus korelasi command; B memakai sequence 0 dan flag AUTO_DEMO bit 14. Master memiliki jendela TX 150 ms sesudah frame lengkap dan keepalive tiga detik; Nano menahan B selama guard TX/RX. Proteksi dan default trip latch tidak diubah. Lihat [DEMO_BROADCAST.md](DEMO_BROADCAST.md).

Build revisi: master 1.216.111 byte flash dan 134.288 byte global RAM; masing-masing Nano 19.796 byte flash dan 1.287 byte global RAM. Tujuh suite C++ portable termasuk simulasi dummy satu jam, 24 uji JS dan delapan uji Python digunakan untuk regresi. Angka compiler bukan pengukuran heap runtime. Fitur broadcast belum diunggah atau diuji pada perangkat fisik.

## Revisi kecepatan demo dan notifikasi

Profil demo dipercepat menjadi maksimal 0,40 bar/detik dan 6 °C/detik normal, 0,80 bar/detik dan 12 °C/detik saat relief, dengan fase singkat. Uji 80 kombinasi node/seed dari nol pada ambang default mencapai alarm pertama paling lambat 18 detik. Border/badge TRIP tanpa causes di master dihapus; trip dan arbitrasi output tetap bekerja. Kode operator tetap 123456 menggantikan token lama di NVS saat berbeda. Pemeriksaan kode relay REAL/SIM tidak menemukan gate yang mencegah REAL mengaktifkan D9; diagnosis LED modul tanpa klik masih memerlukan rating/tegangan catu perangkat.

Build final profil presentasi: master 1.215.943 byte flash / 134.264 byte global RAM; masing-masing Nano 19.832 / 1.287 byte. Semua header build cocok sumber. 24 JS dan 8 Python lulus; tiga suite C++ yang terkait perubahan (core, operator UI, demo broadcast) diuji ulang. Kode baru belum diunggah.

## Dashboard dengan notifikasi dan desktop satu viewport

Revisi UI menambahkan pemberitahuan per boiler untuk ALARM merah dan WASPADA kuning, tanpa pop-up/count/suara untuk trip tertahan tanpa causes. Header BOILER STATION dan footer Politeknik Elektronika Negeri Surabaya. Sirene Web Audio dua nada menunggu gesture browser, lalu mengikuti alarm yang belum dibisukan; OFF, Silence, pulih dan offline menghentikan voice. Monitor desktop lolos pemeriksaan viewport 1024×600, 1280×720, 1366×768, 1536×864, 1920×1080, 2560×1080 dengan Guidebook dan kontrol bawah terlihat. Mobile 390 px tidak overflow horizontal. Detail: DASHBOARD_ALARM_DAN_LAYOUT.md.

31 regresi JS dan 8 Python lulus; master berhasil dibangun dengan 1.224.839 byte flash dan 134.264 byte global RAM. Header build sesuai sumber final; tidak ada upload/uji board pada revisi ini. Firmware Nano tidak diubah untuk revisi dashboard.
