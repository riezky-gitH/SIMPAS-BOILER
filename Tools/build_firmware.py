"""Reproducible local build. Never uploads. Install pinned dependencies first (guidebook)."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys
ROOT=Path(__file__).resolve().parents[1]
MASTER_FQBN='esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,FlashMode=qio,USBMode=default,CDCOnBoot=cdc,MSCOnBoot=default,DFUOnBoot=default'
NANO_FQBN='arduino:avr:nano:cpu=atmega328old'
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--cli');p.add_argument('--target',choices=['all','master','slave1','slave2'],default='all');p.add_argument('--output',type=Path,default=ROOT/'build');p.add_argument('--serial-only',action='store_true');p.add_argument('--nano-watchdog',action='store_true',help='Requires tested Optiboot bootloader');args=p.parse_args()
    cli=args.cli or shutil.which('arduino-cli') or r'C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe'
    subprocess.run([sys.executable,str(ROOT/'Tools/sync_assets.py')],check=True)
    args.output.mkdir(parents=True,exist_ok=True)
    profiles=[('slave1','Slave1_Nano',NANO_FQBN),('slave2','Slave2_Nano',NANO_FQBN),('master','Master_ESP32S3',MASTER_FQBN)]
    results=[]
    for target,folder,fqbn in profiles:
        if args.target not in ('all',target):continue
        if target!='master' and args.nano_watchdog:fqbn='arduino:avr:nano:cpu=atmega328'
        # Copy artifacts ourselves: core 3.3.5's Windows post-export hook assumes
        # sketch/build/<fqbn>, which old Arduino CLI --output-dir does not create.
        cmd=[cli,'compile','--fqbn',fqbn,'--build-cache-path',str(args.output/'cache'),'--build-path',str(args.output/target)]
        if target=='master':cmd+=['--build-property','upload.maximum_size=6225920']
        if target=='master' and args.serial_only:cmd+=['--build-property','compiler.cpp.extra_flags=-DENABLE_USB_MSC=0']
        if target!='master' and args.nano_watchdog:cmd+=['--build-property','compiler.cpp.extra_flags=-DENABLE_AVR_WATCHDOG=1']
        cmd.append(str(ROOT/'Firmware'/folder))
        with (args.output/(target+'.log')).open('w',encoding='utf-8') as log:
            process=subprocess.run(cmd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,encoding='utf-8',errors='replace');log.write(process.stdout);print(process.stdout)
        results.append({'target':target,'fqbn':fqbn,'exit_code':process.returncode})
        if process.returncode:break
        release=args.output/'release'/target;release.mkdir(parents=True,exist_ok=True)
        for pattern in ('*.bin','*.hex','*.elf','partitions.csv'):
            for artifact in (args.output/target).glob(pattern):shutil.copyfile(artifact,release/artifact.name)
    (args.output/'results.json').write_text(json.dumps(results,indent=2),encoding='utf-8')
    return 1 if any(r['exit_code'] for r in results) else 0
if __name__=='__main__':raise SystemExit(main())
