# Layar TFT dan respons pembacaan sensor

26 Oktober 2026 · Master ESP32-S3 N16R8

## Tampilan

TFT menggunakan abu-abu terang, panel putih, teks gelap, header marun `#800020`, dan footer abu-abu gelap seperti dashboard. Warna hijau/merah/amber tetap membedakan kondisi normal, alarm dan peringatan. Orientasi tetap rotasi 3; QR tetap hitam-putih dengan quiet zone.

## Perbaikan kedip

Sebelumnya, fallback tanpa framebuffer membersihkan layar fisik lalu menggambar teks lagi setiap pembaruan. Mode framebuffer RGB565 juga mengirim seluruh layar walaupun sebagian besar gambar tidak berubah.

Renderer baru memakai dua framebuffer berindeks 4 bit: **27.520 byte per frame**, total **55.040 byte**. Buffer memakai PSRAM jika tersedia, dengan fallback SRAM jika masih memiliki ruang yang cukup untuk jaringan dan task. Data RGB565 hanya dibentuk untuk transfer SPI yang diperlukan.

- Layar digambar lengkap di RAM terlebih dahulu.
- Frame dibandingkan secara persis per area **16×8 piksel**; tidak menggunakan checksum yang dapat berkolisi.
- Area berubah yang bersebelahan digabungkan, lalu ditransfer ke TFT. Frame sama menghasilkan **nol transfer SPI**.
- Penghapusan angka lama dan penggambaran angka baru terjadi dalam buffer sebelum transfer; layar fisik tidak dikosongkan pada pembaruan rutin.
- Sudut bawah layar yang hanya setinggi 4 piksel ditangani terpisah agar tidak melewati buffer.
- Perintah serial `TFT` menginisialisasi ulang panel dan memaksa satu pengiriman frame lengkap.
- Jika kedua buffer benar-benar gagal dialokasikan, layar menunjukkan pesan kekurangan memori satu kali; firmware tidak kembali ke siklus clear/gambar yang berkedip.

Pembaruan TFT tetap maksimal 4 frame/detik pada task I/O. Loop UART/kontrol tetap terpisah dari transfer SPI.

## Respons dashboard

| Tahap | Konfigurasi terbaru |
|---|---|
| Sampling sensor Nano | Tetap sekitar 250 ms |
| Jeda polling setelah balasan Nano valid | 100 ms, sebelumnya 600 ms |
| Publikasi snapshot master | 100 ms, sebelumnya 500 ms |
| Polling angka live dashboard | Target 250 ms, sebelumnya 1 detik |
| Batas data dianggap stale | Tetap 4 detik |

Satu permintaan UART aktif per Nano dan satu transaksi HTTP aktif per tab tetap dipertahankan. Jadwal polling browser memperhitungkan waktu permintaan sebelumnya; permintaan lambat tidak menghasilkan tumpukan polling baru. Saat terjadi kegagalan, retry memakai backoff hingga 4 detik dan kembali cepat setelah respons valid.

Target angka live mendekati 4 pembaruan/detik pada koneksi yang baik. Laju aktual dibatasi durasi frame UART 9600 bps, sampling Nano, dan jaringan. Grafik history tetap menggunakan rekaman RAM per detik; pencatatan flash menggunakan jadwal 5 detik ditambah catatan langsung saat threshold/alarm/relay baru aktif.

## Verifikasi

- **21 regresi JavaScript lulus**, termasuk jadwal pembaruan ≤250 ms, nilai sensor baru, satu transaksi aktif, outage/backoff, stale dan kontrol offline.
- **6 uji Python lulus** untuk aset firmware/dashboard dan logger.
- Uji renderer native lulus: frame sama, area berubah, penghapusan piksel, penggabungan area, clipping, palet dan guard bytes.
- Uji native Safety/CRC/framing/FAT16 juga berhasil dijalankan lokal.
- Build ESP32-S3 menggunakan profil Flash 16MB, OPI PSRAM, USB-OTG + CDC. Flash aplikasi 1.209.803 byte; SRAM global 116.648 byte. Log build: audit/master_tft_fast_build.log. Batas verifikasi fisik dicatat dalam manifest artefak workspace.

Pratinjau `audit/tft_scada_preview.png` menggunakan font bitmap dan palet firmware dengan angka simulasi. Pratinjau tersebut bukan foto layar perangkat. Pengukuran kedip dan latensi fisik setelah upload belum dilakukan.

## Menerapkan

Upload ulang sketch master beserta header lokalnya, dengan **Flash 16MB / OPI PSRAM**. Perubahan ini tidak memerlukan firmware Nano baru jika kedua Nano sudah memakai perbaikan koneksi v3 sebelumnya. Setelah upload, muat ulang penuh dashboard dengan Ctrl+F5.

## Penyederhanaan operasi — 27 Oktober 2026

Tema, orientasi, ukuran angka dan susunan dua kotak boiler dipertahankan. Header/footer mendapat margin 16 piksel. Lima halaman adalah SIMPAS → Riwayat → WiFi → Panduan → Batas Alarm; halaman Alarm dan Sistem khusus TFT dihapus. Memori bebas ditampilkan di Riwayat.

Border/badge WASPADA kuning terang muncul dalam jarak 0,5 bar atau 5 °C di bawah threshold. Penyebab alarm aktif menggunakan merah terang dan memiliki prioritas lebih tinggi. Trip tertahan tanpa penyebab aktif tidak menampilkan badge/border TRIP; baris relay tetap menjelaskan TRIP tanpa pop-up. Peringatan tidak menyalakan relay/buzzer. Border ditampilkan stabil melalui framebuffer yang sama, tanpa animasi berkedip dan tanpa perpindahan halaman otomatis. Preview audit/master_operator_1.png dirender dari operasi gambar C++ produksi dengan font Adafruit yang sama dan nilai sensor simulasi; bukan foto board.
