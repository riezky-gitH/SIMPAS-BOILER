"""Capture the firmware's pinned CSV snapshot over USB CDC. No fixed duration assumed."""
import argparse
from pathlib import Path
import time
from validate_history import validate

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--port',required=True);p.add_argument('--out',type=Path,default=Path('history_boiler.csv'));p.add_argument('--timeout',type=float,default=600);args=p.parse_args()
    try:import serial
    except ImportError:raise SystemExit('Install USB capture dependency: python -m pip install "pyserial>=3.5,<4"')
    s=serial.Serial();s.port=args.port;s.baudrate=115200;s.timeout=1;s.dtr=False;s.rts=False;s.open()
    partial=args.out.with_suffix(args.out.suffix+'.partial');started=False;complete=False;expected=None;count=0;deadline=time.monotonic()+args.timeout
    try:
        time.sleep(1);s.reset_input_buffer();s.write(b'DUMP\n')
        with partial.open('wb') as out:
            while time.monotonic()<deadline:
                line=s.readline()
                if line.startswith(b'# DUMP BEGIN '):started=True;expected=int(line.split()[3]);continue
                if line.startswith(b'# DUMP END'):complete=True;break
                if started and line.strip() and not line.startswith(b'#'):
                    out.write(line);count+=len(line)
        if not complete or expected!=count:raise RuntimeError(f'Incomplete export: received {count}, expected {expected}; partial file preserved')
        records=validate(partial)
        partial.replace(args.out);print(f'Saved {count} bytes, {records} validated records, to {args.out}')
    finally:
        if not complete:s.write(b'STOP\n')
        s.close()

if __name__=='__main__':main()
