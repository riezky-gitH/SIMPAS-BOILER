# Notifikasi proses dan layout dashboard

Revisi 27 Oktober 2026 mempertahankan tema SCADA abu-abu/marun. Header menjadi **BOILER STATION** dan footer kanan menjadi **Politeknik Elektronika Negeri Surabaya**.

## Alarm waspada dan trip

- **ALARM merah** tampil untuk penyebab aktif, termasuk gangguan sensor/mode. Kartu boiler memiliki border merah dan pemberitahuan singkat per unit.
- **WASPADA kuning** aktif saat kanal yang valid berada pada Pth−0,5 bar ≤ P < Pth atau Tth−5 °C ≤ T < Tth. Kartu dan pemberitahuan memakai kuning, tanpa suara alarm.
- **TRIP yang tertahan tanpa penyebab aktif tidak membuat pop-up, border merah, hitungan alarm, atau suara.** Status proses mengikuti pembacaan saat itu; baris output relay tetap dapat menampilkan `ON · TRIP` untuk menjelaskan mengapa output belum OFF. Reset trip dan proteksi lokal tetap bekerja.
- Silence menghentikan suara tetapi tidak menyembunyikan penyebab alarm yang belum pulih. Alarm node lain tetap terlihat jika satu node terputus.
- Pemberitahuan berlaku pada semua halaman, tidak memaksa navigasi, dan tidak membangun ulang elemennya setiap sampel yang nilainya berubah. Data yang tidak segar menghapus pemberitahuan proses lama.

## Suara laptop

Audio memakai Web Audio lokal, bukan file eksternal/CDN. Pola dua nada 740/980 Hz bergantian selama sekitar 0,84 detik, berulang sekitar setiap 1,6 detik selama ada alarm yang belum dibisukan. Suara tidak mengirim perintah ke Nano dan tidak menggantikan buzzer slave.

Browser biasanya memerlukan interaksi pengguna sebelum audio boleh dimainkan. Secara default dashboard menunggu interaksi pertama (klik/ketik) atau tombol **Audio AKTIFKAN**. Setelah konteks audio berjalan, tombol menunjukkan **Audio ON**. Klik untuk **Audio OFF** bila perlu; pilihan OFF disimpan untuk sesi browser dan tidak dinyalakan kembali oleh perubahan sensor.

Silence pada node, pemulihan penyebab, data master yang stale, node terputus, atau Audio OFF langsung menghentikan voice yang sedang dijadwalkan. Penyebab baru yang belum dibisukan tetap dapat menghasilkan suara. Volume sebenarnya mengikuti perangkat output, volume OS, dan pengaturan browser.

Referensi perilaku browser: [MDN Autoplay](https://developer.mozilla.org/en-US/docs/Web/Media/Guides/Autoplay) dan [OscillatorNode](https://developer.mozilla.org/en-US/docs/Web/API/OscillatorNode).

## Rasio layar

Pada desktop lebar minimal 1000 px, shell mengisi viewport. Tinggi diagram boiler menyesuaikan ruang yang tersedia; angka, kontrol, Guidebook, dan footer dipertahankan. Halaman monitor normal dan alarm/waspada campuran diuji pada **1024×600, 1280×720, 1366×768, 1536×864, 1920×1080, dan 2560×1080** tanpa scroll halaman atau kartu yang terpotong.

Panel sekunder yang panjang, seperti detail diagnostik atau kalibrasi yang dibuka, dapat menggulir di dalam area kontennya; sidebar dan footer tetap terlihat. Pada ponsel/tablet tata letak tetap mengalir dan dapat digulir. Untuk jendela desktop yang sangat pendek (di bawah 560 px), scroll diizinkan supaya kontrol tidak disembunyikan.

## Menerapkan revisi

Sumber UI adalah `Firmware/WebDashboard/index.html`. Jalankan `python Tools/sync_assets.py` sebelum build agar `dashboard_html.h` sesuai. Perubahan dashboard ini memerlukan **upload master ESP32 saja**; tidak ada perubahan firmware Nano pada revisi UI ini. Refresh dashboard setelah upload. History tidak diformat.

Preview `http://127.0.0.1:8765/` merupakan simulasi komputer. Fixture menyediakan `?scene=mixed`, `?scene=trip`, `?scene=muted`, `?scene=warning`, dan `?scene=offline` untuk QA tanpa perangkat. Hasil browser dan build tidak dinyatakan sebagai uji hardware baru.

Verifikasi: **31 regresi JavaScript** dan **8 pengujian Python** lulus. Pengujian audio memeriksa urutan nada, gating interaksi, mute, offline, trip saja, penyebab baru dan penghentian voice. Pengujian browser mengonfirmasi tombol audio dapat berpindah AKTIFKAN → ON → OFF tanpa error konsol.
