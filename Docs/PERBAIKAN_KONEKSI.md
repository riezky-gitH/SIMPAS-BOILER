# Perbaikan kestabilan dashboard dan UART

26 Oktober 2026 · ESP32-S3 N16R8 · Firmware/protokol v3

## Gejala dan penyebab di kode

Pengguna melaporkan WiFi tetap tersambung, sementara dashboard berulang kali menampilkan OFFLINE; koneksi master ke Nano juga kadang terputus.

Pemeriksaan menemukan beberapa pekerjaan yang saling menghambat:

- Server HTTP sinkron, USB/serial ekspor, TFT, dan UART dilayani dari loop yang sama. Pembacaan HTTP atau operasi file yang lambat dapat menahan UART selama beberapa detik.
- Browser menjalankan polling telemetri dan riwayat/event secara bersamaan. Riwayat tetap diambil saat halaman grafik tidak dibuka.
- Respons history dikirim per baris dalam ratusan potongan TCP kecil.
- Satu kegagalan HTTP langsung memaksa tampilan OFFLINE walaupun sampel terakhir masih baru.
- Polling UART berikutnya mengganti sequence polling sebelumnya tanpa menunggu balasan. Balasan yang terlambat dapat ditolak meskipun CRC benar.
- Parser mengukur batas 300 ms dari awal frame, bukan dari karakter terakhir. Frame yang masih terus diterima dapat dianggap kedaluwarsa.

Temuan ini berasal dari alur program. Pengaruh catu daya, kabel, level tegangan UART, dan interferensi belum diukur pada perangkat saat pekerjaan ini dilakukan.

## Perubahan

### Master

Loop kontrol menjadi pemilik tunggal state kedua Nano, keputusan perintah, Preferences, dan event. HTTP, USB, serial ekspor, dan penggambaran TFT dijalankan dalam task I/O tersendiri pada core 0.

HTTP dan TFT membaca salinan state. Perintah HTTP masuk melalui antrean ke loop kontrol, sehingga handler web tidak mengubah state UART secara bersamaan. Mutex salinan dilepas sebelum jaringan/file/SPI dijalankan.

- Buffer RX UART menjadi **2.048 byte per link**.
- Hanya satu polling aktif per Nano; balasan ditunggu sampai **700 ms**.
- Poll berikutnya dijadwalkan sekitar **100 ms setelah balasan valid** (dipercepat pada pembaruan layar). Perintah juga menunggu polling selesai agar tidak bertabrakan dengan transmisi SoftwareSerial Nano.
- Data node tetap dinyatakan putus jika tidak ada telemetri valid selama **3 detik**; timeout ini tidak diperpanjang untuk menyembunyikan masalah.
- History JSON dirakit sebagai satu respons dengan panjang diketahui. Pencarian berhenti setelah jendela waktu boot saat ini terlewati.
- Fallback tanpa PSRAM membatasi history grafik hingga 180 titik dan menjaga ruang SRAM untuk WiFi/task; PSRAM aktif mendukung hingga 720 titik.
- Dua framebuffer berindeks 4 bit memakai total 55.040 byte, dengan pembaruan area berubah. Fallback SRAM digunakan bila ruangnya cukup; tidak ada clear/gambar langsung berulang.
- Kegagalan router opsional menghentikan percobaan STA tanpa mematikan/membuat ulang AP. AP menggunakan channel 6 saat dimulai; sleep WiFi dimatikan.

### Nano

- Penerimaan UART dilayani sebelum dan setelah pekerjaan sensor/tombol.
- Pembaruan LCD menulis bagian teks yang berubah secara berurutan, sehingga transaksi I2C berkurang.
- Overflow buffer SoftwareSerial dicatat sebagai error dan parser disinkronkan ulang.
- Balasan perintah duplikat tetap memperbarui waktu kontak master.
- Batas frame 300 ms berlaku untuk jeda antar karakter, bukan keseluruhan durasi frame.

Format telemetry, CRC, baud **9600 8N1**, pinout, konfigurasi EEPROM, dan format CSV tetap kompatibel dengan v3.

### Dashboard

- Satu transaksi HTTP aktif per tab; antrean dilepas setelah seluruh body respons diterima, termasuk saat error.
- Telemetri angka live ditargetkan setiap **250 ms**, dengan satu permintaan aktif dan backoff saat gagal. Snapshot master diperbarui setiap 100 ms.
- History hanya diambil saat halaman Tren & riwayat dibuka; event hanya saat halaman Alarm & kejadian dibuka.
- Kegagalan sesaat memakai batas kesegaran sampel **4 detik**. Setelah batas terlampaui, angka menjadi --, relay UNKNOWN dan kontrol dinonaktifkan.
- Telemetri tetap diperbarui di sela transaksi ekspor CSV.

## Hasil pemeriksaan

- **19 regresi JavaScript lulus**, termasuk serialisasi permintaan, kegagalan antrean, halaman history tersembunyi, outage nyata melewati batas stale, serta transport HTTP lokal satu client.
- **6 uji Python lulus**, termasuk kesesuaian generated assets dan partisi.
- Master dan kedua Nano berhasil dikompilasi dengan profil yang didokumentasikan.
- Uji C++ ditambah untuk polling, rollover waktu, frame lambat, serta pemulihan setelah frame rusak. Syntax check C++ lulus; executable native belum dijalankan lokal dan tersedia pada workflow CI.
- Header dalam hasil build dibandingkan dengan sumber final. Binary dan manifest diperbarui di `audit/firmware_v3` workspace.

| Target | Flash aplikasi menurut compiler | SRAM global |
|---|---:|---:|
| Master | 1.208.655 byte | 111.520 byte |
| Nano 1 | 16.276 byte | 1.161 byte |
| Nano 2 | 16.276 byte | 1.161 byte |

**Belum diverifikasi:** soak test board fisik dan pengukuran jalur UART/catu daya. Pada saat pemeriksaan, board tidak tersedia melalui USB dan `192.168.4.1` tidak dapat diakses dari komputer ini. Tidak dilakukan upload atau erase oleh agen.

## Memasang dan memeriksa hasilnya

1. Upload ulang sketch **master, Nano 1, dan Nano 2** dari folder firmware masing-masing. Master memakai Flash 16MB, OPI PSRAM, USB-OTG TinyUSB dan CDC Enabled; Firmware MSC On Boot Disabled.
2. Hubungkan kembali WiFi master. Muat ulang penuh dashboard agar script lama tidak digunakan.
3. Buka Monitor selama 5–10 menit, lalu buka Tren & riwayat dan coba ekspor CSV. Pastikan koneksi kedua Nano tidak berubah menjadi OFFLINE saat halaman web sibuk.
4. Buka **Sistem**. Catat **Jeda UART maks**, **Poll timeout**, **CRC error**, dan **Putus koneksi** sebelum/sesudah pengujian. Nilai jeda adalah maksimum sejak boot, termasuk operasi yang sengaja dilakukan.
5. Jika gangguan tersisa, simpan Data diagnostik lengkap beserta kondisi pengujian. Timeout/CRC yang terus bertambah saat jeda pelayanan UART kecil memerlukan pemeriksaan jalur komunikasi, sambungan GND, dan kesesuaian level Nano TX ke ESP32 RX. Perubahan boot ID/uptime menunjukkan perangkat restart dan perlu ditelusuri terpisah.

Rincian penggunaan dan pemasangan: [GUIDEBOOK.md](GUIDEBOOK.md).
