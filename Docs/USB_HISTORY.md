# Drive history USB

## Jika drive hanya muncul sesaat

Windows dapat mendeteksi antarmuka USB Mass Storage tetapi tidak menampilkan drive jika ESP32 belum menyediakan media. Periksa `STATUS` melalui USB Serial:

| Status | Arti |
|---|---|
| `FS=unmounted` | LittleFS belum berhasil dibuka. Drive history belum dapat disiapkan. |
| `FS=mounted history=recovering` | Filesystem terbaca; arsip sedang dipulihkan. Tunggu sampai siap. |
| `FS=mounted history=ready` | Penyimpanan siap. Kirim `USB` untuk membuat/memperbarui snapshot bila drive belum terlihat. |
| `PSRAM=0` | PSRAM belum aktif/terdeteksi; untuk N16R8 pilih OPI PSRAM saat compile lalu upload ulang. |

`history=ready` sendiri tidak membuktikan filesystem terpasang: buffer RAM juga bisa siap ketika `FS=unmounted`.

Untuk flash baru yang belum pernah berisi history, inisialisasi filesystem dilakukan satu kali melalui `FORMAT CONFIRM`, setelah menyetujui penghapusan isi partisi history. Firmware menolak format jika filesystem sudah mounted. Jangan mengulangi format untuk me-refresh drive yang sudah bekerja.

Drive berisi snapshot baca-saja `history_boiler.csv`. Setelah menyalin, lakukan Eject. Untuk data terbaru, Eject lalu kirim `USB` dan tunggu media muncul kembali; cabut-pasang kabel bila Windows masih menyimpan cache.

## Perbaikan firmware 26 Oktober 2026

CDC On Boot dapat memulai USB sebelum `setup()` memasang event handler aplikasi. Kode lama hanya menunggu `USB_STARTED`; jika event awal terlewat, media tidak dipublikasikan. Kegagalan membuat snapshot juga sebelumnya membatalkan permintaan tanpa retry.

Firmware baru:

- Membaca status host USB aktual saat inisialisasi dan secara berkala, sehingga tidak bergantung pada event awal.
- Menunggu filesystem dan pemulihan data siap sebelum menampilkan media.
- Mencoba ulang persiapan snapshot yang gagal sementara, dengan jeda satu detik.
- Menghormati Eject; media tidak muncul sendiri lagi sampai refresh atau koneksi baru.
- Memindahkan penanganan Eject/disconnect ke task I/O agar callback USB tidak menunggu filesystem.
- Melaporkan kondisi melalui `USB=...` pada `STATUS`, field API `history.usb_status`, dan baris USB history di halaman Sistem.
- Tetap tidak memformat penyimpanan secara otomatis.

Status USB meliputi `NO_HOST`, `FS_UNAVAILABLE`, `RECOVERING`, `WAITING`, `SNAPSHOT_RETRY`, `READY`, `EJECTED`, `INIT_ERROR`, dan `DISABLED` untuk build serial-only.

## Verifikasi

Uji native lifecycle USB lulus: event startup terlewat, filesystem/recovery belum siap, retry, Eject/refresh, reconnect, serta rollover waktu. Pengujian dashboard dan aset tetap lulus.

Pada board yang diperiksa, Windows mengenali **SIMPAS LOG (E:)**, virtual disk 16 MiB. File `E:\history_boiler.csv` berukuran 12.690 byte berhasil dibaca dan **201 baris data lolos pemeriksaan CRC** setelah filesystem siap dan snapshot diperbarui. Ini memverifikasi akses CSV perangkat pada firmware sebelumnya; otomatisasi mount baru masih memerlukan upload firmware hasil perbaikan.

Partisi LittleFS fisik tetap 10 MiB. Ukuran virtual disk USB bukan kapasitas filesystem internal. Riwayat USB adalah snapshot dan tidak bertambah selama sesi mount yang sama.

Build master hasil perbaikan berhasil: flash aplikasi 1.210.959 byte, SRAM global 116.672 byte. Log audit/master_usb_history_build.log; binary dan manifest diperbarui di audit/firmware_v3. Tidak dilakukan upload oleh agen.

## Waktu dan interval pembacaan

CSV baru memuat `waktu_wib`, misalnya `2026-10-26 12:00:02`. Setiap baris tetap memiliki `epoch`, `boot`, `uptime_s`, dan CRC. Jadwal rutin adalah 5 detik. Ketika nilai mencapai threshold (`P >= P_th` atau `T >= T_th`), alarm baru muncul, atau relay mulai aktif, master mencatat paket itu segera tanpa menunggu jadwal. Jika nilai turun lalu naik lagi, crossing baru juga dicatat, termasuk ketika trip masih tertahan. Grafik RAM tetap diperbarui per detik dan pembacaan keselamatan Nano tetap berjalan cepat.

Jam disinkronkan otomatis dari browser setelah token operator tersimpan; tombol **Sinkronkan jam** tetap tersedia. Pastikan jam HP/laptop benar. Format tanggal dan jam selalu WIB meskipun zona waktu laptop berbeda. Internet tidak diperlukan untuk sinkronisasi browser. RTC internal ESP32-S3 tidak mempertahankan waktu setelah daya terputus; tanpa RTC eksternal berbaterai, lakukan sinkronisasi setelah power-on. Rujukan: [Espressif System Time](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/api-reference/system/system_time.html).

Rekaman yang masih berada di RAM pada boot yang sama dapat memperoleh acuan waktu melalui selisih uptime setelah sinkronisasi. Baris yang sudah terlanjur permanen tanpa jam, terutama dari boot lama, tetap `UNSYNCED`; firmware tidak mengarang tanggal dari jam upload atau jam laptop saat file disalin. `uptime_s` tetap berguna untuk urutan relatifnya.

Pada boot pertama firmware ini, segmen format lama dikonversi satu per satu melalui file sementara. Firmware memeriksa CRC semua baris sebelum mengganti sumber secara atomik; kegagalan konversi mempertahankan sumber dan menampilkan error penyimpanan. Konversi memerlukan ruang tambahan untuk satu segmen, dan tanggal/jam menambah ukuran baris. Tidak dilakukan format otomatis.

Drive USB adalah snapshot. Jika jam baru disinkronkan setelah drive muncul, lakukan Eject dan kirim `USB` atau sambungkan ulang untuk mengambil snapshot baru. Jangan mengedit CSV di drive.

Verifikasi revisi waktu: uji native jadwal 5 detik/crossing independen/rollover/WIB/acuan boot/CSV lama dan CRC lulus; 24 uji dashboard dan 7 uji Python lulus. Salinan 1.300 rekaman dari USB board dikonversi memakai codec C++ produksi lalu diperiksa ulang dengan validator Python; semua CRC valid. Ini uji konversi pada host, bukan uji migrasi flash langsung pada board. Build master akhir berhasil: flash 1.214.515 byte, SRAM global 117.328 byte. Seluruh header hasil build cocok dengan sumber saat ini. Binary tersedia di audit/firmware_v3/master; log audit/master_history_timing_build.log. Firmware belum diunggah.

## CSV pengguna tujuh kolom — 27 Oktober 2026

Format ekspor terbaru menggantikan kolom teknis dengan:

`Waktu WIB,Tekanan Slave 1 (bar),Suhu Slave 1 (C),Tekanan Slave 2 (bar),Suhu Slave 2 (C),Status Slave 1,Status Slave 2`

Contoh: `2026-10-27 12:00:00,2.61,80.7,2.16,165.0,NORMAL,ALARM: Suhu tinggi`.

Pembacaan sudah dalam bar/°C. Status adalah NORMAL/WASPADA atau ALARM dengan penyebab lengkap. Beberapa sebab dipisahkan tanda `;`. TERPUTUS dan MENUNGGU SENSOR tidak disamarkan sebagai NORMAL. Kanal yang tidak valid dikosongkan. Waktu tanpa acuan ditulis Belum sinkron. Status warning direkam memakai bit metadata journal pada saat pembacaan, tidak dihitung ulang menggunakan threshold saat ekspor. Rekaman lama tanpa metadata status tetap dapat dibaca, tetapi status warning lamanya tidak ditebak. Alarm/trip lama tetap diterjemahkan dari bukti yang tersimpan.

CRC dan metadata tetap berada di journal internal. Ekspor melakukan konversi baca-saja; tidak menulis ulang arsip untuk menyembunyikan kolom dan tidak membuat duplikat CSV besar di flash. Indeks jarang dibangun saat pemulihan/flush dan dibekukan bersama snapshot. Cache 4 KiB menghindari pembacaan ulang journal pada setiap sektor USB/fragmen serial. Snapshot HTTP/USB/DUMP memakai proyeksi yang sama. Kegagalan baca/CRC diteruskan sebagai kegagalan I/O, bukan sektor kosong yang dianggap berhasil.

Volume FAT16 virtual sekarang 64 MiB agar teks penyebab alarm yang panjang dapat ditampung. Ini ukuran ruang alamat drive virtual, bukan penambahan kapasitas fisik; LittleFS tetap 10 MiB dan budget history 9 MiB. Tidak ada perubahan partisi atau format flash untuk revisi tampilan CSV ini. Eject/reconnect setelah upload agar host membaca geometri dan snapshot baru.

`Tools/validate_history.py` menerima format tujuh kolom dan memeriksa header, waktu, satuan nilai, serta status tertulis. Jika diberi journal teknis lama, alat masih memeriksa CRC. `Tools/dump_serial.py` memeriksa jumlah byte dan validasi sesuai format file. CSV pengguna tidak memiliki CRC yang dapat diperiksa ulang setelah diedit di Excel.

Validasi format tujuh kolom: enam suite C++ native, 24 pengujian JS, delapan pengujian Python lulus; 1.300 rekaman dari salinan arsip lama dapat diproyeksikan dan divalidasi tanpa mengubah sumber. Buffer parser ekspor menggunakan ruang kerja Store yang dilindungi mutex, bukan stack TinyUSB yang terbatas. Build master berhasil (flash 1.215.723 / SRAM global 134.272 byte); log audit/master_readable_history_build.log. Hanya master memerlukan update untuk perubahan ekspor ini. Belum dilakukan upload/uji format baru langsung pada board.
