"""Verify generated TFT bitmap still decodes to the repository at actual 3x scale."""
from pathlib import Path
import re
import cv2
import numpy as np
ROOT=Path(__file__).resolve().parents[1]
s=(ROOT/'Firmware/Master_ESP32S3/GuideQr.h').read_text()
n=int(re.search(r'GUIDE_QR_SIZE=(\d+)',s)[1]);data=bytes(int(v,16) for v in re.findall(r'0x([0-9A-F]{2})',s))
image=np.full((n+8,n+8),255,dtype=np.uint8)
for y in range(n):
    for x in range(n):
        if data[y*((n+7)//8)+x//8]&(128>>(x%8)):image[y+4,x+4]=0
image=cv2.resize(image,None,fx=3,fy=3,interpolation=cv2.INTER_NEAREST)
url,_,_=cv2.QRCodeDetector().detectAndDecode(image)
assert url=='https://github.com/riezky-gitH/SIMPAS-BOILER',url
print(f'TFT QR decodes at {image.shape[1]}x{image.shape[0]}: {url}')
