# Pemeriksaan modul relay dua channel

## Jalur program dan PCB

Firmware memakai satu keluaran relay per Nano. `outputs()` pada `Firmware/SlaveCommon/SlaveApp.h` menulis **D9 LOW saat safety.relay true**, dan **D9 HIGH saat false**. `setup()` menyetel D9 sebagai OUTPUT. D4 adalah LED indikator dan D5 buzzer. Sinyal D9 bukan servo PWM walaupun label CAD lama masih memakai nama Servo.

Jalur relay tidak dibatasi mode SIM. Pembacaan REAL yang mencapai ambang, sensor fault, trip, dan permintaan purge mengaktifkan keluaran melalui arbitrasi yang sama. Uji portable mencakup alarm tekanan/suhu REAL dalam LIVE, mute tanpa menutup relay, trip reset, dan purge remote.

Untuk satu modul yang dipakai bersama oleh kedua slave:

| Terminal modul | Sambungan |
|---|---|
| IN1 | D9 Nano slave 1; CAD J2 pad 1 |
| IN2 | D9 Nano slave 2; CAD J5 pad 1 |
| GND | Ground bersama modul dan kedua Nano |
| VCC | Catu sesuai rating modul/koil, bukan sinyal GPIO |

CAD J2/J5 memiliki pad 2 net 5V dan pad 3 GND. Nomor pad tidak sama dengan urutan kiri/kanan dari semua arah pandang. Pengguna mengonfirmasi label **SRD-05VDC**, catu step-down **12V ke 5V**, dan LED modul berubah mengikuti perintah Nano. Rating koil dan target catu cocok secara nominal; tegangan di terminal modul saat ON, ground bersama, dan jalur daya koil belum terukur. Default program ditujukan bagi modul **trigger LOW**; bila modul ternyata trigger HIGH, polaritas harus dicocokkan secara eksplisit, bukan ditebak dari warna LED.

## Membedakan indikator dan kontak relay

**Tes indikator** di dashboard hanya menjalankan LED/buzzer dua detik, sengaja tanpa relay. Untuk memeriksa permintaan relay gunakan **Manual ON** atau tekan PB1 pendek pada slave. Uji awal dengan beban kontak dilepas. Baca halaman RELAY pada LCD dan amati LED D4; ketika keluaran ON diharapkan D9 berada dekat GND terhadap GND Nano.

| Pengamatan | Pemeriksaan berikutnya |
|---|---|
| LCD RELAY:OFF | Pastikan aksi Manual ON diterapkan, bukan hanya Tes indikator |
| LCD RELAY:ON, D4 menyala, LED modul tidak merespons | Cek IN dari D9, ground, polaritas trigger, dan kontinuitas konektor |
| LED modul menyala tetapi tidak ada klik | Ukur VCC–GND saat diperintah ON, cocokkan rating koil, periksa catu/driver/modul |
| Relay klik tetapi beban tidak bekerja | Periksa COM/NO/NC dan catu beban; itu terpisah dari pin IN |

LED modul yang merespons merupakan petunjuk bahwa input berubah, bukan bukti bahwa koil telah mendapat tegangan cukup. Tidak ada pembacaan feedback koil atau kontak pada firmware, sehingga dashboard tidak dapat memastikan relay mekanik benar-benar bergerak. Menambah delay atau mengirim PWM tidak memperbaiki catu koil yang tidak cocok.

## Lanjutan untuk LED yang sudah mengikuti command

1. Dengan beban kontak dilepas, ukur **VCC–GND langsung di modul** saat Manual ON, bukan hanya di keluaran step-down tanpa beban. Target untuk modul yang disebut pengguna adalah 5V.
2. Pastikan ground step-down, modul, dan kedua Nano tersambung. Periksa soket/kabel menuju modul, bukan hanya label tegangan adaptor.
3. Jika ada jumper **VCC/JD-VCC** pada modul, cocokkan posisi jumper dengan petunjuk modul untuk catu koil. Bila hanya ada empat pin dan tidak ada jumper, langkah ini tidak berlaku.
4. Pada modul yang sudah dipastikan trigger LOW, lepaskan kabel IN dari Nano lalu uji satu channel dengan IN ke GND modul, tetap tanpa beban kontak. Jika catu benar dan uji input langsung tetap tidak menghasilkan klik, driver/koil/modul perlu diperiksa; mempercepat program atau mengganti token tidak menyelesaikannya.
5. Bandingkan channel kedua dengan kondisi catu dan metode yang sama. Kontak mekanik yang tidak berubah harus diperiksa terpisah dari LED indikator.

Setelah pengujian, lepaskan Manual ON/purge yang diaktifkan. Trip perlu RESET setelah penyebab pulih dan aman dua detik; sumber local/remote/global lain yang masih aktif dapat mempertahankan keluaran. Pemeriksaan kode sudah dilakukan, tetapi sebab fisik relay tidak berbunyi belum dapat dinyatakan selesai tanpa pengukuran atau pemeriksaan modul.

Referensi rating koil: [datasheet Songle SRD](https://naylampmechatronics.com/img/cms/000263/SRD-05VDC-SL-C-Datasheet.pdf). Datasheet komponen relay tidak menentukan seluruh wiring/polaritas PCB modul dua channel yang belum diidentifikasi.
