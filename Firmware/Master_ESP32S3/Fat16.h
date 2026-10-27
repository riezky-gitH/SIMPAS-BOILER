#pragma once
#include <stdint.h>
#include <string.h>
// Read-only virtual FAT16. Data is supplied by an immutable, pinned CSV snapshot.
// Host never mounts or writes the LittleFS partition itself.
namespace UsbExport {
constexpr uint32_t SECTORS=131072,SECTOR_SIZE=512,FAT_SECTORS=128,ROOT_START=257,DATA_START=261,CLUSTER_BYTES=2048;
typedef int(*Reader)(uint32_t,uint8_t*,size_t);
inline void le16(uint8_t*p,uint16_t x){p[0]=x;p[1]=x>>8;}
inline void le32(uint8_t*p,uint32_t x){le16(p,x);le16(p+2,x>>16);}
inline uint8_t checksum(const char*s){uint8_t sum=0;for(int i=0;i<11;i++)sum=((sum&1)?128:0)+(sum>>1)+s[i];return sum;}
inline void lfn(uint8_t*out,uint8_t ordinal){
  const char*name="history_boiler.csv";const uint8_t positions[]={1,3,5,7,9,14,16,18,20,22,24,28,30};
  memset(out,0xFF,32);out[0]=ordinal;out[11]=0x0F;out[12]=0;out[13]=checksum("HISTOR~1CSV");le16(out+26,0);
  unsigned start=((ordinal&31)-1)*13,n=strlen(name);
  for(uint8_t i=0;i<13;i++){unsigned index=start+i;le16(out+positions[i],index<n?name[index]:(index==n?0:0xFFFF));}
}
inline bool sector(uint32_t lba,uint8_t*out,uint32_t fileSize,Reader read){
  memset(out,0,512);if(lba>=SECTORS)return false;
  if(lba==0){
    out[0]=0xEB;out[1]=0x3C;out[2]=0x90;memcpy(out+3,"SIMPAS  ",8);le16(out+11,512);out[13]=4;le16(out+14,1);out[16]=2;
    le16(out+17,64);le16(out+19,0);le32(out+32,SECTORS);out[21]=0xF8;le16(out+22,FAT_SECTORS);le16(out+24,32);le16(out+26,64);
    out[36]=0x80;out[38]=0x29;le32(out+39,0x53494D33);memcpy(out+43,"SIMPAS LOG ",11);memcpy(out+54,"FAT16   ",8);out[510]=0x55;out[511]=0xAA;
  }else if(lba<ROOT_START){
    uint32_t offset=((lba-1)%FAT_SECTORS)*256,clusters=(fileSize+CLUSTER_BYTES-1)/CLUSTER_BYTES;
    for(uint32_t i=0;i<256;i++){uint32_t entry=offset+i;uint16_t value=entry==0?0xFFF8:entry==1?0xFFFF:(entry>=2&&entry<clusters+2?(entry==clusters+1?0xFFFF:entry+1):0);le16(out+i*2,value);}
  }else if(lba==ROOT_START){
    memcpy(out,"SIMPAS LOG ",11);out[11]=8;lfn(out+32,0x42);lfn(out+64,1);
    uint8_t*entry=out+96;memcpy(entry,"HISTOR~1CSV",11);entry[11]=0x21;le16(entry+24,0x21);le16(entry+26,fileSize?2:0);le32(entry+28,fileSize);
  }else if(lba>=DATA_START){uint32_t position=(lba-DATA_START)*512;if(position<fileSize){size_t n=fileSize-position<512?fileSize-position:512;return read&&read(position,out,n)==int(n);}}
  return true;
}
}
