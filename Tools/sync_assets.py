"""Generate Arduino-local headers and synchronize legacy workspace copies.
Run from any directory. No network access, commits, or firmware upload.
"""
from pathlib import Path
import argparse
import shutil

ROOT = Path(__file__).resolve().parents[1]
def copy_if_changed(source,target):
    if not target.exists() or source.read_bytes()!=target.read_bytes():shutil.copyfile(source,target)
def write_if_changed(target,text):
    if not target.exists() or target.read_text(encoding='utf-8')!=text:target.write_text(text,encoding='utf-8')

def generate():
    fw = ROOT / 'Firmware'
    for name in ('Master_ESP32S3', 'Slave1_Nano', 'Slave2_Nano'):
        for header in ('Protocol.h', 'Control.h', 'Button.h','Broadcast.h'):
            copy_if_changed(fw / 'Shared' / header, fw / name / header)
    copy_if_changed(fw / 'Shared/Fat16.h', fw / 'Master_ESP32S3/Fat16.h')
    copy_if_changed(fw / 'Shared/IndexedFrame.h', fw / 'Master_ESP32S3/IndexedFrame.h')
    copy_if_changed(fw / 'Shared/UsbSession.h', fw / 'Master_ESP32S3/UsbSession.h')
    copy_if_changed(fw / 'Shared/HistoryTiming.h', fw / 'Master_ESP32S3/HistoryTiming.h')
    copy_if_changed(fw / 'Shared/HistoryRecord.h', fw / 'Master_ESP32S3/HistoryRecord.h')
    copy_if_changed(fw / 'Shared/HistoryExport.h', fw / 'Master_ESP32S3/HistoryExport.h')
    copy_if_changed(fw / 'Shared/MasterScreen.h', fw / 'Master_ESP32S3/MasterScreen.h')
    for node in (1, 2):
        folder = fw / f'Slave{node}_Nano'
        copy_if_changed(fw / 'SlaveCommon/SlaveApp.h', folder / 'SlaveApp.h')
        copy_if_changed(fw / 'Shared/SlaveUi.h', folder / 'SlaveUi.h')
        copy_if_changed(fw / 'Shared/DemoSensors.h', folder / 'DemoSensors.h')
        write_if_changed(folder / f'Slave{node}_Nano.ino',f'// Generated local headers: python Tools/sync_assets.py\n#define SLAVE_ID {node}\n#include "SlaveApp.h"\n')
    html = (fw / 'WebDashboard/index.html').read_text(encoding='utf-8')
    write_if_changed(fw / 'Master_ESP32S3/dashboard_html.h',
        '// Generated from Firmware/WebDashboard/index.html.\n#pragma once\n#include <pgmspace.h>\n'
        'const char INDEX_HTML[] PROGMEM = R"SCADA_HTML(\n' + html + '\n)SCADA_HTML";\n')
    copy_if_changed(fw / 'WebDashboard/index.html', ROOT / 'Tools/dashboard_scada_boiler.html')

def legacy():
    workspace = ROOT.parent
    if not (workspace / 'GUIDEBOOK.md').exists():
        raise RuntimeError('Expected legacy WMLTA workspace not found')
    shutil.copytree(ROOT / 'Firmware', workspace / 'Firmware', dirs_exist_ok=True)
    shutil.copyfile(ROOT / 'Tools/dashboard_scada_boiler.html', workspace / 'dashboard_scada_boiler.html')
    shutil.copyfile(ROOT / 'Tools/dashboard_mqtt_bridge.py', workspace / 'dashboard_mqtt_bridge.py')
    shutil.copyfile(ROOT / 'Tools/logger_v3.py', workspace / 'logger_v3.py')
    shutil.copyfile(ROOT / 'Docs/GUIDEBOOK.md', workspace / 'GUIDEBOOK.md')

if __name__ == '__main__':
    p = argparse.ArgumentParser()
    p.add_argument('--legacy', action='store_true', help='Also synchronize existing WMLTA root copies')
    args = p.parse_args()
    generate()
    if args.legacy:
        legacy()
    print('Arduino headers and dashboard generated' + ('; legacy copies synchronized' if args.legacy else ''))
