# Pinout hardware firmware v3

Master ESP32-S3 N16R8: 16 MiB flash, 8 MiB OPI PSRAM.

| Sinyal master | GPIO |
|---|---|
| RV5 / draft ambang | 4 |
| PB1 / PB2 / PB3 | 5 / 6 / 7 |
| UART1 RX / TX | 16 / 17 |
| UART2 RX / TX | 8 / 18 |
| TFT DC / CS / MOSI / SCK / RST | 9 / 10 / 11 / 12 / 14 |
| Native USB D− / D+ | 19 / 20 melalui konektor board |

| Sinyal slave, kedua Nano | Pin |
|---|---|
| UART RX / TX | D2 / D3 |
| LED aktuator HIGH / Buzzer HIGH / Relay LOW saat aktif | D4 / D5 / D9 |
| PB1 / PB2 / PB3 | D6 / D7 / D8 |
| MAX6675 CS / SO / SCK | D10 / D12 / A0 |
| Selector suhu / tekanan | D11 / A3 |
| SIM suhu / tekanan | A1 / A2 |
| Pressure transmitter | A6 |
| LCD SDA / SCL | A4 / A5 |

Seluruh tombol dan selector memakai INPUT_PULLUP. Selector LOW=REAL, HIGH=SIM. Output relay LOW=ON, HIGH=OFF. Common ground dan proteksi level UART diperlukan.

Untuk satu modul relay dua channel, **IN1 → D9 Nano slave 1**, **IN2 → D9 Nano slave 2**, dan **GND modul → GND kedua Nano**. VCC mengikuti rating modul yang benar. Jangan menghubungkan IN ke D4 karena itu jalur LED. Pada CAD lama konektor D9 masih bernama **Servo SG90**: **J2 slave 1** dan **J5 slave 2**, masing-masing pad 1 sinyal D9, pad 2 5V, pad 3 GND. Periksa nomor pad, bukan perkiraan urutan dari arah pandang konektor.

Program mengeluarkan LOW stabil saat relay ON dan HIGH saat OFF, bukan PWM servo; jalur ini sama pada SIM maupun REAL. Tombol dashboard **Tes indikator** memang tidak mengaktifkan relay; gunakan Manual ON atau PB1 pendek untuk meminta purge. LED modul yang menyala tanpa klik belum membuktikan kontak bergerak; periksa catu/rating koil, driver dan modul sesuai [diagnosis relay](RELAY_DUA_CHANNEL.md).

Divider 1k/2,2k pada CAD awal menghasilkan sekitar 3,44 V nominal. Verifikasi toleransi supply/resistor dan batas GPIO atau pasang buffer yang tepat. Tidak ada klaim sangat aman tanpa pengukuran. ESP32 RX tidak 5 V tolerant.

CAD/footprint awal masih dapat memuat nama modul N8R2. Firmware v3 tidak mengubah tembaga PCB atau menjamin rating relay, driver, proteksi beban induktif, maupun kondisi saat listrik mati. Cocokkan pin fisik board N16R8 dan wiring sebelum memberikan daya ke aktuator.

Sketch uji_uart_potensiometer lama memakai pin/protokol berbeda. Jangan mencampur sketch uji itu dengan firmware v3.

Lihat [guidebook](GUIDEBOOK.md) untuk fungsi tombol, profil native USB, dan commissioning.
