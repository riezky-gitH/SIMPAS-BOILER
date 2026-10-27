# Panduan Singkat Pengguna SIMPAS

**Master ESP32-S3 N16R8 · Dua slave boiler · Firmware v3**

Master menampilkan data kedua boiler dan menyediakan dashboard. Setiap slave membaca sensor serta menjalankan alarm dan kendali relay secara lokal.

## 1. Mulai menggunakan alat

1. Pastikan sambungan dan catu daya sudah diperiksa, lalu nyalakan master serta kedua slave.
2. Tunggu proses startup. Pada firmware ini, relay dapat ON selama pembacaan awal belum valid.
3. Periksa layar master: **B1** adalah Boiler 1, **B2** adalah Boiler 2. Pastikan data tekanan, suhu, dan status koneksi muncul.
4. Periksa sumber sensor: **SIM** berarti simulasi dari potensiometer; **REAL** berarti sensor asli. Mode LIVE memerlukan kedua sumber sensor REAL.

**P** menunjukkan tekanan dalam **bar**; **T** menunjukkan suhu dalam **°C**. Indikator relay menunjukkan output listrik, bukan bukti posisi katup fisik.

## 2. Tombol pada master

Tekan singkat berarti tekan lalu lepaskan. Tahan berarti sekitar **2 detik**.

| Tombol | Tekan singkat | Tahan |
|---|---|---|
| **PB1** | Ganti halaman | Kembali ke monitor; pada halaman Batas Alarm, pilih parameter |
| **PB2** | Matikan suara alarm kedua slave | Akui alarm (ACK); pada halaman Batas Alarm, simpan nilai usulan |
| **PB3** | Aktifkan purge kedua boiler | Ajukan pelepasan purge global jika sudah aktif sebelum tombol ditekan |

**PB3 adalah tombol purge.** Untuk melepas purge yang baru diaktifkan, lepaskan tombol terlebih dahulu, lalu tekan dan tahan kembali. Pelepasan hanya diterima jika kondisi aman.

Urutan halaman master: **SIMPAS → Riwayat → WiFi → Panduan → Batas Alarm**. SIMPAS adalah halaman utama. Border kuning **WASPADA** muncul mulai 0,5 bar atau 5 °C di bawah threshold; border merah menunjukkan alarm/trip. Master tidak berpindah halaman saat alarm. Sisa RAM dan PSRAM tersedia di **Riwayat**. Halaman Panduan berisi QR **“scan for guidebook”** menuju repository proyek.

## 3. Tombol pada setiap slave

| Tombol | Tekan singkat | Tahan ±2 detik |
|---|---|---|
| **PB1** | Aktifkan/lepas permintaan purge lokal | Reset trip jika kondisi aman |
| **PB2** | Matikan suara alarm slave tersebut | Akui alarm (ACK) |
| **PB3** | Ganti halaman LCD | Kembali ke monitor |

LCD menyediakan halaman monitor, alarm, relay, koneksi/ambang, dan petunjuk QR. Saat alarm baru berbunyi, halaman alarm muncul sementara. Setelah dibisukan dengan PB2 atau penyebab alarm pulih, LCD kembali ke halaman terakhir yang dipilih. Penanda sumber tekanan dan suhu sama-sama memakai `[S]` (simulasi) atau `[R]` (real). Halaman koneksi menampilkan MASTER dan TH; nomor versi V disembunyikan. Kontrol lokal slave tetap bekerja saat WiFi tidak tersedia.

**Demo otomatis:** pilih SIM pada kedua selector dan nonaktifkan LIVE. Setelah slave siap, tahan **PB2+PB3 bersamaan dua detik** lalu lepaskan keduanya. `DEMO AUTO ON` berarti nilai dummy naik/turun bertahap dan slave mengirim sendiri setiap sekitar satu detik. Tahan kombinasi yang sama untuk berhenti. Memilih REAL/LIVE atau restart menghentikan demo. Nilai tetap berlabel `[S]`; mode ini tidak disimpan. Trip yang sudah aktif tetap perlu RESET setelah kondisi aman; mematikan demo tidak membatalkan trip/purge. Lihat [panduan broadcast](DEMO_BROADCAST.md).

**LED aktuator:** LED slave mengikuti relay (menyala saat output katup ON), dengan pengecualian tes indikator dua detik. Mematikan suara alarm tidak mematikan aktuator. Jika **TRIP** tertahan, pastikan penyebab alarm hilang dan kondisi aman minimal dua detik, lalu tahan PB1 untuk reset. Jika **LOCAL**, tekan singkat PB1 untuk melepas permintaan lokal; jika **REMOTE**, pilih Lepas manual di dashboard; jika **GLOBAL**, lepas purge global dari master. Permintaan lain yang masih aktif dapat mempertahankan relay ON. Mengubah angka sensor atau threshold saja tidak mereset trip.

## 4. Membuka dashboard

1. Hubungkan HP/laptop ke WiFi **`SCADA_BOILER_PLTU`**.
2. Masukkan password **`00000000`**. Tetap terhubung meskipun HP memberi pesan “tanpa internet”.
3. Buka **http://192.168.4.1** di browser.
4. Untuk membaca data, cukup buka halaman **Monitor**.
5. Untuk menjalankan kontrol, buka halaman **WiFi** pada layar master, lalu masukkan kode kontrol **123456** ke menu **Akses operator** di dashboard dan pilih **Simpan akses**.
6. Setelah token operator tersimpan, jam master otomatis mengikuti jam HP/laptop. Pastikan jam perangkat benar. Tombol **Pengaturan → Sinkronkan jam** tetap tersedia.

| Menu dashboard | Kegunaan |
|---|---|
| **Monitor** | Tekanan, suhu, status relay, serta kontrol tiap boiler dan purge bersama |
| **Tren & riwayat** | Grafik 5/15/60 menit dan unduh riwayat CSV |
| **Alarm & kejadian** | Alarm aktif, insiden terakhir, dan log kejadian |
| **Pengaturan** | Batas alarm, mode sensor, dan sinkronisasi jam |
| **Sistem** | Koneksi dan kondisi memori/penyimpanan |

**Manual ON** mengaktifkan permintaan relay dari dashboard. **Lepas manual** hanya melepas permintaan tersebut; alarm, trip, atau purge lain dapat membuat relay tetap ON. Setelah mengirim perintah, tunggu hasil **APPLIED**. **QUEUED/SENT** berarti masih diproses; **BLOCKED/TIMEOUT** berarti tindakan belum berhasil dikonfirmasi.

Alamat **127.0.0.1:8765** adalah preview simulasi di laptop. Gunakan **192.168.4.1** untuk mengoperasikan board melalui WiFi master.

## 5. Saat alarm berbunyi

1. Baca boiler dan penyebab alarm di layar atau dashboard.
2. Tekan **PB2 singkat** atau **Silence** untuk mematikan suara. Alarm baru dapat membunyikan buzzer kembali.
3. Tahan **PB2** atau pilih **ACK** untuk mengakui alarm. Pada master, pindah dari halaman Batas Alarm sebelum menahan PB2.
4. Tangani penyebabnya. Silence dan ACK tidak menghilangkan penyebab alarm atau melepas trip.
5. Setelah sensor valid dan kondisi aman setidaknya **2 detik**, tahan **PB1 slave** atau pilih **Reset trip** di dashboard.

Jika muncul **RESET BLOCKED**, kondisi belum memenuhi syarat reset. **HELD BY PRIORITY** berarti output masih ditahan oleh alarm, trip, atau permintaan purge lainnya.

## 6. Mengubah batas alarm

**Melalui master:** buka **Batas Alarm**, tahan PB1 untuk memilih B1/B2 dan tekanan/suhu, putar **RV5**, lalu tahan PB2 untuk mengirim nilai usulan.

**Melalui dashboard:** buka **Pengaturan**, pilih unit, isi batas tekanan/suhu, lalu pilih **Terapkan pengaturan** dan konfirmasi. Pastikan nilai efektif berubah setelah slave mengonfirmasi. Gunakan batas yang sudah ditetapkan untuk pengujian/operasi alat.

## 7. Melihat dan mengambil riwayat

- **Layar master:** buka **Riwayat** untuk melihat tekanan/suhu puncak dan lima insiden terakhir.
- **Dashboard:** buka **Tren & riwayat → Unduh CSV** setelah memasukkan token operator.
- **USB tanpa WiFi:** sambungkan port USB native master ke laptop menggunakan kabel data. Salin **`history_boiler.csv`** dari drive history. Lakukan **Eject** setelah selesai; sambungkan ulang untuk snapshot terbaru. Jika drive belum terlihat, kirim `USB` melalui Serial Monitor dan tunggu beberapa detik.
- **Serial alternatif:** buka Serial Monitor **115200**, pilih **Newline**, lalu kirim **`DUMP`**. Simpan keluaran CSV di antara penanda BEGIN dan END. Kirim **`STOP`** untuk membatalkan.

Data disimpan tiap **5 detik**. Jika tekanan atau suhu mencapai threshold, atau alarm/relay baru aktif, pembacaan langsung dicatat meskipun belum 5 detik. Jadwal rutin tetap berjalan: contoh 12:00:00 → 12:00:02 (threshold) → 12:00:05. Penyimpanan permanen dilakukan berkala, sehingga sebagian rekaman terbaru dapat hilang jika listrik terputus. Cadangkan CSV secara rutin karena riwayat tertua dihapus saat penyimpanan penuh. Kolom **Waktu WIB** di CSV menampilkan tanggal dan jam (`YYYY-MM-DD HH:MM:SS`). **Belum sinkron** atau **U+…s** berarti belum ada acuan jam. Setelah listrik terputus, buka dashboard dan masukkan token kembali bila belum tersimpan; sinkronisasi tidak membutuhkan internet. Setelah sinkronisasi, Eject lalu perbarui snapshot USB dengan `USB` untuk menyalin CSV terbaru.

CSV hanya berisi tujuh kolom: **Waktu WIB**, **Tekanan Slave 1 (bar)**, **Suhu Slave 1 (C)**, **Tekanan Slave 2 (bar)**, **Suhu Slave 2 (C)**, **Status Slave 1**, dan **Status Slave 2**. Nilai langsung memakai bar dan °C, tanpa perlu dibagi lagi. Status ditulis NORMAL, WASPADA, atau ALARM beserta penyebabnya, misalnya **ALARM: Suhu tinggi; Gangguan sensor tekanan**. Saat koneksi/sensor tidak tersedia, sel pembacaan terkait dikosongkan; status dapat menampilkan TERPUTUS atau MENUNGGU SENSOR.

Status direkam saat pembacaan, sehingga perubahan threshold berikutnya tidak mengubah status history. Data lama yang belum merekam kondisi WASPADA diberi **STATUS LAMA TIDAK TERSEDIA** jika tidak ada informasi alarm/trip; penyebab alarm lama tetap diterjemahkan bila ada. Jika Excel menampilkan `####`, lebarkan kolom Waktu WIB.

## 8. Kendala umum

| Gejala | Tindakan |
|---|---|
| **OFFLINE**, **DEGRADED**, atau angka **--** | Periksa daya, sambungan, dan koneksi master/slave. Angka -- bukan nilai nol. |
| **FAULT / ERROR** | Periksa sensor yang ditandai dan sumber SIM/REAL. |
| Tombol dashboard tidak aktif | Periksa koneksi dan token operator; tunggu jika ekspor sedang berjalan. |
| Relay tetap ON setelah reset/lepas manual | Periksa alarm, trip, purge lokal, dan purge global. |
| Penyimpanan error atau CSV tidak tersedia | Minta pemeriksaan teknisi; jangan memformat jika ada riwayat penting. |
| Port USB muncul lalu hilang | Untuk upload, tahan BOOT, tekan-lepas RESET, lalu lepaskan BOOT dan pilih port baru. Konfigurasi USB diperiksa oleh teknisi. |

Panduan teknis lengkap: [GUIDEBOOK.md](GUIDEBOOK.md).

### Pembaruan demo dan token

Demo kini lebih cepat: pada uji simulasi dari nilai nol dengan ambang default, alarm pertama tercapai paling lambat 18 detik. Waktu sebenarnya mengikuti nilai awal, ambang dan keadaan purge/trip. Jika sebelumnya memakai token panjang, ganti menjadi **123456** pada Akses operator setelah upload master. Badge TRIP master dihapus; trip tetap bekerja dan dapat terlihat pada baris relay. Untuk modul relay dua channel, IN1/IN2 memakai D9 masing-masing slave, bukan D4 LED; baca [diagnosis relay](RELAY_DUA_CHANNEL.md).

### Notifikasi dashboard dan suara laptop

ALARM merah menandai penyebab aktif, WASPADA kuning pembacaan yang mendekati batas. Trip yang masih tertahan tidak membuat pop-up; keterangannya tetap di baris output relay. Klik **Audio AKTIFKAN** atau berinteraksi dengan dashboard untuk membuka izin audio browser. Saat **Audio ON**, alarm belum dibisukan menghasilkan suara dua nada berulang. Silence, kondisi pulih, putus data atau Audio OFF menghentikan suara; waspada tidak bersuara. Untuk revisi tampilan ini cukup upload master lalu refresh dashboard.
