# Demo sensor dan broadcast otomatis

Fitur ini membuat **slave mengirim telemetri tanpa menunggu polling master**. Data tekanan/suhu adalah simulasi, berubah naik dan turun secara bertahap; pengendalian alarm, relay, LED, dan trip tetap menggunakan logika proteksi yang sama.

## Mengaktifkan

1. Gunakan firmware revisi broadcast pada **master dan kedua Nano**. Firmware master lama belum mengenali frame `B`.
2. Pada slave yang akan didemokan, pilih **SIM untuk tekanan dan suhu**; mode konfigurasi **LIVE harus nonaktif**.
3. Setelah startup siap, **tekan PB2 dan PB3 bersamaan selama dua detik**, lalu lepaskan keduanya. LCD menampilkan `DEMO AUTO ON`; halaman koneksi menunjukkan `MASTER:OK AUTO` ketika link sehat.
4. Ulangi pada slave kedua bila diperlukan. Masing-masing slave memiliki urutan dummy sendiri.
5. Untuk berhenti, tahan kombinasi yang sama dua detik. Memilih REAL pada salah satu selector, mengaktifkan LIVE, atau restart slave juga menghentikannya. Mode ini tidak disimpan di EEPROM.

Kombinasi menahan PB2/PB3 mengonsumsi aksi singkat/panjang kedua tombol sampai keduanya dilepas. PB1 tetap berfungsi untuk purge/reset. Kombinasi harus ditekan bersamaan; bila salah satu dilepas sebelum dua detik, toggle dibatalkan.

**Sesudah alarm:** mematikan demo tidak menghapus trip atau permintaan purge. Pada konfigurasi default, trip tetap aktif setelah nilai normal. Bisukan dengan PB2 bila diperlukan, tunggu kondisi aman dua detik, lalu lakukan RESET seperti panduan. LED D4 mengikuti output aktuator, termasuk trip yang masih terkunci.

## Perilaku nilai

- Nilai dimulai dari pembacaan saat aktivasi, kemudian menuju target acak dengan pembatas laju; tidak melompat langsung ke angka acak baru.
- Enam fase menyajikan kondisi normal, mendekati ambang, tekanan tinggi, pemulihan, suhu tinggi, dan pendinginan. Durasi fase bervariasi. Kedua node tidak dipaksa serempak.
- Profil presentasi dipercepat: pembaruan model setiap 250 ms, laju normal dibatasi **0,40 bar/detik** dan **6 °C/detik**. Saat output pelepasan aktif, penurunan dibatasi **0,80 bar/detik** dan **12 °C/detik**. Nilai tetap bergerak bertahap menuju target, bukan melompat langsung.
- Fase normal/pemulihan sekitar 6–8 detik, fase tekanan tinggi 12–16 detik, dan fase suhu tinggi 28–36 detik. Pada uji native 80 kombinasi seed/node dari nilai awal nol dan ambang default 4 bar/160 °C, alarm pertama tercapai paling lambat **18 detik**. Ini hasil simulasi, bukan janji waktu fisik; ambang, nilai awal, dan purge/trip yang aktif memengaruhi jalur demo.
- Rentang model 0–10 bar dan 0–350 °C. Relai aktif mengarahkan target menuju tekanan/suhu yang lebih rendah. Trip latch dapat mempertahankan pendinginan sampai operator melakukan reset.
- Status sumber tetap `[S]`. Flag UART `AUTO_DEMO` bit 14 dan JSON `demo_auto` menandai generator otomatis. Nilai ini **bukan pengukuran proses nyata**; model ditujukan untuk demonstrasi, bukan perhitungan termodinamika boiler.

## Komunikasi

UART tetap 9600 baud, 8N1, dua link point-to-point. Balasan perintah memakai frame `T` dengan sequence perintah. Broadcast memakai **frame `B`, 24 field yang sama, sequence 0**, result 0. Broadcast dikirim nominal setiap satu detik; penyebab alarm baru dan penghentian demo diberitakan lebih awal saat jalur aman.

Frame `B` **tidak menyelesaikan perintah yang menunggu ACK**. Master tetap mengirim keepalive `POLL` sekitar tiga detik untuk mengonfirmasi jalur balik dan memperbarui status MASTER di Nano. Pengiriman B tidak memerlukan polling tersebut.

SoftwareSerial Nano tidak bisa menerima dan mengirim serentak. Saat node otomatis dan online, master memulai command dalam 150 ms sesudah frame lengkap; slave menunggu setidaknya 400 ms dari akhir TX dan 40 ms tanpa byte RX sebelum broadcast. Timer broadcast dihitung dari awal TX agar periode tidak ikut memanjang oleh durasi frame. Saat node offline, master mencoba pemulihan link kembali. Ini pengaturan waktu perangkat lunak; ketahanan tabrakan dan jitter tetap perlu diuji pada perangkat fisik.

Session 0 hanya diizinkan pada frame B bootstrap sebelum slave mengenal sesi master. Balasan T tetap harus cocok sesi dan sequence. Boot ID dan sample sequence digunakan untuk menolak B duplikat/lama. Master memisahkan waktu frame di kabel dari waktu telemetri yang diterima untuk pemulihan nonce sesi tanpa menganggap data lama segar.

API berotorisasi juga menerima `{"node":1,"action":"DEMO,1"}` atau `DEMO,0`. HTTP 202 berarti masuk antrean; keberhasilan mengikuti hasil `T` dari slave. Tidak ada tombol dashboard baru untuk mode ini; gunakan kombinasi tombol slave.

## Verifikasi revisi

Tujuh suite C++ portable, regresi dashboard dan Python, serta kompilasi tiga board diperiksa. Uji dummy satu jam mencakup batas laju, rentang, perbedaan node, warning, alarm tekanan dan suhu, rollover, gate SIM, isolasi B/T, dan gesture tombol. Untuk membuktikan seluruh fase secara otomatis, harness mengatur `latch=0`; **default firmware tetap `latch=1`**. Fitur baru belum diunggah atau dinyatakan lulus uji hardware.
