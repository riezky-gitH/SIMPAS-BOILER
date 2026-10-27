"""Convert the supplied, decoded QR modules into a lossless TFT bitmap header."""
import argparse
from pathlib import Path
import cv2
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
EXPECTED = 'https://github.com/riezky-gitH/SIMPAS-BOILER'

def main(image):
    im = cv2.imread(str(image))
    url, _, modules = cv2.QRCodeDetector().detectAndDecode(im)
    if url != EXPECTED or modules is None:
        raise ValueError(f'QR destination mismatch: {url!r}')
    size = modules.shape[0]
    # OpenCV's rectified grid changed three cells in this axis-aligned input.
    # Sample centers directly from the original pixel grid instead, then decode
    # the reconstructed TFT-sized image before accepting the generated asset.
    gray=cv2.cvtColor(im,cv2.COLOR_BGR2GRAY)
    ys,xs=np.where(gray<128)
    left,top,right,bottom=int(xs.min()),int(ys.min()),int(xs.max()),int(ys.max())
    if abs((right-left)-(bottom-top))>1:
        raise ValueError('Expected an axis-aligned square QR image')
    modules=np.array([[0 if gray[int(top+(y+.5)*(bottom-top+1)/size),int(left+(x+.5)*(right-left+1)/size)]<128 else 255 for x in range(size)]for y in range(size)],dtype=np.uint8)
    test=cv2.resize(np.pad(modules,4,constant_values=255),None,fx=3,fy=3,interpolation=cv2.INTER_NEAREST)
    if cv2.QRCodeDetector().detectAndDecode(test)[0]!=EXPECTED:
        raise ValueError('Generated 3x TFT QR did not decode; keeping existing header')
    rows = []
    for row in modules:
        for start in range(0, size, 8):
            value = 0
            for bit in range(8):
                if start + bit < size and row[start + bit] == 0:
                    value |= 0x80 >> bit
            rows.append(value)
    out = ROOT / 'Firmware/Master_ESP32S3/GuideQr.h'
    out.write_text('// Generated from the supplied QR; module-exact, no image resampling.\n'
        '#pragma once\n#include <Arduino.h>\n'
        f'constexpr int GUIDE_QR_SIZE={size};\n'
        f'constexpr const char* GUIDE_URL="{url}";\n'
        'const uint8_t GUIDE_QR[] PROGMEM={\n' +
        ',\n'.join(','.join(f'0x{x:02X}' for x in rows[i:i+16]) for i in range(0,len(rows),16)) + '\n};\n', encoding='utf-8')
    print(f'QR verified: {url}, {size}x{size} modules, {len(rows)} bytes')

if __name__ == '__main__':
    p=argparse.ArgumentParser();p.add_argument('image',type=Path);main(p.parse_args().image)
