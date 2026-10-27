"""Fast syntax check using an existing ESP32 compilation database, without upload.
Useful with Arduino CLI versions whose library discovery is very slow.
"""
import argparse
import json
from pathlib import Path
import subprocess

ROOT=Path(__file__).resolve().parents[1]

def main():
    p=argparse.ArgumentParser();p.add_argument('database',type=Path);args=p.parse_args()
    entries=json.loads(args.database.read_text(encoding='utf-8'))
    entry=next(x for x in entries if '.ino.cpp' in x['file'])
    original=entry['arguments'];command=[];skip=False
    for x in original:
        if skip:skip=False;continue
        if x=='-o':skip=True;continue
        if x in ('-MMD','-c') or x==entry['file']:continue
        if x.startswith('-DARDUINO_USB_MODE='):x='-DARDUINO_USB_MODE=0'
        if x.startswith('-DARDUINO_USB_CDC_ON_BOOT='):x='-DARDUINO_USB_CDC_ON_BOOT=1'
        x=x.replace('/qio_qspi/','/qio_opi/').replace('\\qio_qspi\\','\\qio_opi\\')
        command.append(x)
    packages=Path.home()/'AppData/Local/Arduino15/packages/esp32/hardware/esp32/3.3.5'
    # Prefer exact core path already present in the database (including other users).
    core_arg=next(x[2:] for x in original if x.startswith('-I') and x.endswith('cores\\esp32'))
    packages=Path(core_arg).parents[1]
    libs=Path.home()/'Documents/Arduino/libraries'
    for name in ('Adafruit_GFX_Library','Adafruit_ST7735_and_ST7789_Library','Adafruit_BusIO','ArduinoJson','PubSubClient'):
        d=libs/name
        command.append('-I'+str(d/'src' if (d/'src').is_dir() else d))
    for name in ('LittleFS','FS','USB','SPI','WiFi','Network','WebServer','Hash','Preferences','Wire'):
        command.append('-I'+str(packages/'libraries'/name/'src'))
    command += ['-DBOARD_HAS_PSRAM','-fsyntax-only','-x','c++','-I'+str(ROOT/'Firmware/Master_ESP32S3'),str(ROOT/'Firmware/Master_ESP32S3/Master_ESP32S3.ino')]
    return subprocess.run(command,cwd=entry['directory']).returncode

if __name__=='__main__':raise SystemExit(main())
