# Firmware v3 — N16R8

Baca `Docs/GUIDEBOOK.md` dari root repository SIMPAS-BOILER untuk konfigurasi, pinout, partisi, dan pengoperasian.

Sumber bersama: `Shared/` dan `SlaveCommon/`. Jalankan `python Tools/sync_assets.py` dari root repository setelah mengubah sumber tersebut. Header lokal di folder sketch dihasilkan agar ketiga sketch dapat dibuka langsung dalam Arduino IDE.

Master: ESP32-S3 N16R8, flash 16 MiB QIO, OPI PSRAM 8 MiB, USB-OTG TinyUSB + CDC, firmware MSC On Boot OFF. File `partitions.csv` memberi LittleFS 10 MiB.

Nano: firmware protokol v3 pada kedua node. Sketch pengujian UART lama bukan firmware ini. Build helper tidak melakukan upload.
